/**
 * MeshCom - Mesh Walkie-Talkie for Motorcycle Groups
 *
 * Entry point: initializes NVS, group manager, Bluetooth HFP AG,
 * ESP-NOW communication, audio pipeline, and UI.
 *
 * Architecture:
 *   Intercom (HFP HF) <--BT SCO--> [ESP32 HFP AG]
 *                                      |
 *                                  audio_pipe
 *                                      |
 *                                  [ESP-NOW broadcast]
 *                                      |
 *                                  Other ESP32s in group
 */
#include "board.h"
#include "group_mgr.h"
#include "bt_hfp.h"
#include "espnow_comm.h"
#include "audio_pipe.h"
#include "pairing.h"
#include "ui.h"

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "meshcom";

void app_main(void)
{
    ESP_LOGI(TAG, "=== MeshCom v%s (%s) ===", MESHCOM_VERSION, BOARD_NAME);

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS erased, re-init");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Group manager (loads or generates AES key) */
    ESP_ERROR_CHECK(group_mgr_init());

    uint16_t gid;
    group_mgr_get_id(&gid);
    ESP_LOGI(TAG, "Group ID: %04X", gid);

    /* Audio pipeline (must be before BT and ESP-NOW) */
    ESP_ERROR_CHECK(audio_pipe_init());

    /* ESP-NOW (WiFi + mesh broadcast) */
    ESP_ERROR_CHECK(espnow_comm_init());

    /* Bluetooth HFP AG (connects to intercom) */
    ESP_ERROR_CHECK(bt_hfp_init());

    /* UI (buttons, LED, display) */
    ESP_ERROR_CHECK(ui_init());

    ESP_LOGI(TAG, "MeshCom ready!");
}
