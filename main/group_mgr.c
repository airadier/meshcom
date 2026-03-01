/**
 * MeshCom - Group Manager
 * AES-128-GCM encryption, NVS persistence for group key and id
 */
#include "group_mgr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/gcm.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "group_mgr";

#define NVS_NAMESPACE   "meshcom"
#define NVS_KEY_GKEY    "group_key"
#define NVS_KEY_GID     "group_id"

static uint8_t  s_group_key[GROUP_KEY_LEN];
static uint16_t s_group_id;
static bool     s_initialized = false;

/* ---- NVS helpers ---- */

static esp_err_t load_from_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    size_t len = GROUP_KEY_LEN;
    err = nvs_get_blob(h, NVS_KEY_GKEY, s_group_key, &len);
    if (err == ESP_OK) {
        err = nvs_get_u16(h, NVS_KEY_GID, &s_group_id);
    }
    nvs_close(h);
    return err;
}

static esp_err_t save_to_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(h, NVS_KEY_GKEY, s_group_key, GROUP_KEY_LEN);
    if (err == ESP_OK) {
        err = nvs_set_u16(h, NVS_KEY_GID, s_group_id);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

/* ---- Public API ---- */

esp_err_t group_mgr_init(void)
{
    esp_err_t err = load_from_nvs();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded group %04X from NVS", s_group_id);
    } else {
        ESP_LOGW(TAG, "No group in NVS, generating new one");
        err = group_mgr_new_group();
    }
    s_initialized = true;
    return err;
}

esp_err_t group_mgr_get_key(uint8_t *key_out)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    memcpy(key_out, s_group_key, GROUP_KEY_LEN);
    return ESP_OK;
}

esp_err_t group_mgr_get_id(uint16_t *id_out)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    *id_out = s_group_id;
    return ESP_OK;
}

esp_err_t group_mgr_new_group(void)
{
    esp_fill_random(s_group_key, GROUP_KEY_LEN);
    s_group_id = (uint16_t)(esp_random() & 0xFFFF);
    ESP_LOGI(TAG, "New group generated: %04X", s_group_id);
    return save_to_nvs();
}

esp_err_t group_mgr_save_key(const uint8_t *key, uint16_t group_id)
{
    memcpy(s_group_key, key, GROUP_KEY_LEN);
    s_group_id = group_id;
    ESP_LOGI(TAG, "Saved group %04X from pairing", s_group_id);
    return save_to_nvs();
}

/* ---- Encrypt / Decrypt ---- */

/*
 * Wire format:
 *   [group_id: 2][seq: 2][nonce: 8][ciphertext: N][tag: 16]
 *
 * GCM IV = nonce (8 bytes) zero-padded to 12 bytes
 * AAD    = group_id || seq (4 bytes)
 */

int group_mgr_encrypt(const uint8_t *plain, size_t len,
                       uint16_t seq, uint8_t *out, size_t out_size)
{
    size_t total = len + GROUP_OVERHEAD;
    if (out_size < total) return -1;

    /* Header */
    out[0] = (s_group_id >> 8) & 0xFF;
    out[1] = s_group_id & 0xFF;
    out[2] = (seq >> 8) & 0xFF;
    out[3] = seq & 0xFF;

    /* Random nonce (8 bytes) */
    uint8_t *nonce_ptr = out + 4;
    esp_fill_random(nonce_ptr, GROUP_NONCE_LEN);

    /* Build 12-byte IV: nonce + 4 zero bytes */
    uint8_t iv[12];
    memcpy(iv, nonce_ptr, GROUP_NONCE_LEN);
    memset(iv + GROUP_NONCE_LEN, 0, 4);

    /* AAD = first 4 bytes (group_id + seq) */
    uint8_t *ciphertext = out + GROUP_HEADER_LEN;
    uint8_t *tag = out + GROUP_HEADER_LEN + len;

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_group_key, 128);
    if (ret != 0) { mbedtls_gcm_free(&gcm); return -1; }

    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                     len, iv, 12,
                                     out, 4,        /* AAD */
                                     plain, ciphertext,
                                     GROUP_TAG_LEN, tag);
    mbedtls_gcm_free(&gcm);

    return (ret == 0) ? (int)total : -1;
}

int group_mgr_decrypt(const uint8_t *pkt, size_t pkt_len,
                       uint16_t *seq_out, uint8_t *out, size_t out_size)
{
    if (pkt_len < GROUP_OVERHEAD + 1) return -1;

    uint16_t gid = ((uint16_t)pkt[0] << 8) | pkt[1];
    if (gid != s_group_id) return -1;  /* wrong group */

    uint16_t seq = ((uint16_t)pkt[2] << 8) | pkt[3];
    if (seq_out) *seq_out = seq;

    size_t cipher_len = pkt_len - GROUP_OVERHEAD;
    if (out_size < cipher_len) return -1;

    const uint8_t *nonce_ptr = pkt + 4;
    uint8_t iv[12];
    memcpy(iv, nonce_ptr, GROUP_NONCE_LEN);
    memset(iv + GROUP_NONCE_LEN, 0, 4);

    const uint8_t *ciphertext = pkt + GROUP_HEADER_LEN;
    const uint8_t *tag = pkt + GROUP_HEADER_LEN + cipher_len;

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_group_key, 128);
    if (ret != 0) { mbedtls_gcm_free(&gcm); return -1; }

    ret = mbedtls_gcm_auth_decrypt(&gcm, cipher_len,
                                    iv, 12,
                                    pkt, 4,     /* AAD */
                                    tag, GROUP_TAG_LEN,
                                    ciphertext, out);
    mbedtls_gcm_free(&gcm);

    return (ret == 0) ? (int)cipher_len : -1;
}
