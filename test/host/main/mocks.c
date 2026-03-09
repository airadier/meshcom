/**
 * MeshCom Host Tests — stubs for ESP-IDF hardware dependencies
 *
 * Provides minimal implementations so group_mgr.c and audio_pipe.c
 * can compile and run on the linux host target.
 */
#include "mocks.h"
#include "espnow_comm.h"
#include "bt_hfp.h"
#include "ui.h"
#include "nvs.h"
#include "esp_err.h"

#include <string.h>

/* ---- Unity setUp/tearDown ---- */
void setUp(void) {}
void tearDown(void) {}

/* ---- Mock state ---- */

mock_state_t g_mock = {0};

void mock_state_reset(void)
{
    memset(&g_mock, 0, sizeof(g_mock));
    nvs_mock_reset();
}

/* ---- ESP-NOW stubs ---- */

esp_err_t espnow_broadcast(const uint8_t *data, size_t len)
{
    g_mock.espnow_broadcast_calls++;
    g_mock.last_broadcast_data = data;
    g_mock.last_broadcast_len = len;
    return ESP_OK;
}

esp_err_t espnow_comm_init(void)
{
    return ESP_OK;
}

/* ---- BT HFP stubs ---- */

esp_err_t bt_hfp_send_audio(const uint8_t *pcm, size_t len)
{
    g_mock.bt_hfp_send_audio_calls++;
    return ESP_OK;
}

bool bt_hfp_is_connected(void)
{
    return true;
}

esp_err_t bt_hfp_init(void) { return ESP_OK; }
esp_err_t bt_hfp_start_discoverable(int duration_sec) { return ESP_OK; }
esp_err_t bt_hfp_reconnect(void) { return ESP_OK; }

/* ---- UI stubs ---- */

esp_err_t ui_init(void) { return ESP_OK; }
void ui_set_state(ui_state_t state) { (void)state; }
void ui_set_bt_status(const char *status) { (void)status; }
void ui_set_activity(const char *activity) { (void)activity; }
void ui_flash_led(int count) { (void)count; }
