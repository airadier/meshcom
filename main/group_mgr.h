/**
 * MeshCom - Group Manager
 * AES-128-GCM encryption and NVS group persistence
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define GROUP_KEY_LEN       16
#define GROUP_NONCE_LEN     8
#define GROUP_TAG_LEN       16
/* Packet: [group_id:2][seq:2][nonce:8][ciphertext:N][tag:16] */
#define GROUP_HEADER_LEN    (2 + 2 + GROUP_NONCE_LEN)
#define GROUP_OVERHEAD      (GROUP_HEADER_LEN + GROUP_TAG_LEN)

esp_err_t group_mgr_init(void);

/** Get current AES-128 key (16 bytes) */
esp_err_t group_mgr_get_key(uint8_t *key_out);

/** Get current group id */
esp_err_t group_mgr_get_id(uint16_t *id_out);

/** Generate a brand new random group (key + id), save to NVS */
esp_err_t group_mgr_new_group(void);

/** Save an externally received key + group_id to NVS */
esp_err_t group_mgr_save_key(const uint8_t *key, uint16_t group_id);

/**
 * Encrypt PCM payload into wire packet.
 * out must be at least (len + GROUP_OVERHEAD) bytes.
 * Returns total packet length written to out.
 */
int group_mgr_encrypt(const uint8_t *plain, size_t len,
                       uint16_t seq, uint8_t *out, size_t out_size);

/**
 * Decrypt wire packet.
 * out must be at least (pkt_len - GROUP_OVERHEAD) bytes.
 * Returns plaintext length, or -1 on failure (wrong group / auth fail).
 */
int group_mgr_decrypt(const uint8_t *pkt, size_t pkt_len,
                       uint16_t *seq_out, uint8_t *out, size_t out_size);
