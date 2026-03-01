/**
 * MeshCom - Group Pairing
 * Share/join group keys via ESP-NOW broadcast (plaintext for PoC)
 *
 * TODO: Replace plaintext key exchange with ECDH for security
 */
#include "pairing.h"
#include "group_mgr.h"
#include "espnow_comm.h"
#include "ui.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <string.h>

static const char *TAG = "pairing";

/* Pairing packet: [magic:4][group_id:2][key:16] = 22 bytes */
#define PAIRING_PKT_LEN     (4 + 2 + GROUP_KEY_LEN)
#define PAIRING_DURATION_MS  30000
#define PAIRING_TX_INTERVAL  500  /* ms between share broadcasts */

static bool s_active = false;
static bool s_sharing = false;
static TimerHandle_t s_timer = NULL;
static TaskHandle_t s_share_task = NULL;

/* ---- Timer callback ---- */

static void pairing_timeout_cb(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "Pairing timeout");
    s_active = false;
    s_sharing = false;
    ui_set_state(UI_STATE_IDLE);
}

/* ---- Share task ---- */

static void share_task(void *arg)
{
    uint8_t pkt[PAIRING_PKT_LEN];
    uint8_t key[GROUP_KEY_LEN];
    uint16_t gid;

    /* Build packet */
    pkt[0] = (PAIRING_MAGIC >> 24) & 0xFF;
    pkt[1] = (PAIRING_MAGIC >> 16) & 0xFF;
    pkt[2] = (PAIRING_MAGIC >> 8)  & 0xFF;
    pkt[3] = PAIRING_MAGIC & 0xFF;

    group_mgr_get_id(&gid);
    pkt[4] = (gid >> 8) & 0xFF;
    pkt[5] = gid & 0xFF;

    group_mgr_get_key(key);
    memcpy(pkt + 6, key, GROUP_KEY_LEN);

    /* Broadcast repeatedly until pairing ends */
    while (s_sharing) {
        espnow_broadcast(pkt, PAIRING_PKT_LEN);
        vTaskDelay(pdMS_TO_TICKS(PAIRING_TX_INTERVAL));
    }

    s_share_task = NULL;
    vTaskDelete(NULL);
}

/* ---- Public API ---- */

esp_err_t pairing_start_share(void)
{
    if (s_active) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Starting SHARE mode (30s)");
    s_active = true;
    s_sharing = true;
    ui_set_state(UI_STATE_SHARE);

    if (!s_timer) {
        s_timer = xTimerCreate("pair", pdMS_TO_TICKS(PAIRING_DURATION_MS),
                                pdFALSE, NULL, pairing_timeout_cb);
    }
    xTimerStart(s_timer, 0);

    xTaskCreate(share_task, "pair_share", 2048, NULL, 5, &s_share_task);
    return ESP_OK;
}

esp_err_t pairing_start_join(void)
{
    if (s_active) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Starting JOIN mode (30s)");
    s_active = true;
    s_sharing = false;
    ui_set_state(UI_STATE_JOIN);

    if (!s_timer) {
        s_timer = xTimerCreate("pair", pdMS_TO_TICKS(PAIRING_DURATION_MS),
                                pdFALSE, NULL, pairing_timeout_cb);
    }
    xTimerStart(s_timer, 0);
    return ESP_OK;
}

bool pairing_is_active(void)
{
    return s_active;
}

void pairing_handle_packet(const uint8_t *data, size_t len)
{
    if (len < PAIRING_PKT_LEN) return;
    if (!s_active || s_sharing) return;  /* Only accept in JOIN mode */

    /* Verify magic */
    uint32_t magic = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                     ((uint32_t)data[2] << 8)  | data[3];
    if (magic != PAIRING_MAGIC) return;

    uint16_t gid = ((uint16_t)data[4] << 8) | data[5];
    const uint8_t *key = data + 6;

    ESP_LOGI(TAG, "Received group key %04X — saving!", gid);
    group_mgr_save_key(key, gid);

    /* Stop pairing */
    s_active = false;
    if (s_timer) xTimerStop(s_timer, 0);
    ui_set_state(UI_STATE_IDLE);
    ui_flash_led(3);
}
