/**
 * MeshCom - Bluetooth HFP AG
 * Acts as a "phone" for the helmet intercom (HFP HF device)
 */
#include "bt_hfp.h"
#include "audio_pipe.h"
#include "ui.h"
#include "board.h"

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_ag_api.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <string.h>
#include <stdbool.h>

static const char *TAG = "bt_hfp";

#define NVS_NAMESPACE   "meshcom"
#define NVS_KEY_BTADDR  "intercom_addr"
#define BT_DEVICE_NAME  "MeshCom"

static esp_bd_addr_t s_intercom_addr;
static bool s_has_saved_addr = false;
static bool s_connected = false;
static bool s_sco_open = false;
static TimerHandle_t s_disco_timer = NULL;

/* ---- NVS helpers ---- */

static esp_err_t load_bt_addr(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    size_t len = sizeof(esp_bd_addr_t);
    err = nvs_get_blob(h, NVS_KEY_BTADDR, s_intercom_addr, &len);
    nvs_close(h);
    if (err == ESP_OK) s_has_saved_addr = true;
    return err;
}

static esp_err_t save_bt_addr(const esp_bd_addr_t addr)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(h, NVS_KEY_BTADDR, addr, sizeof(esp_bd_addr_t));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) {
        memcpy(s_intercom_addr, addr, sizeof(esp_bd_addr_t));
        s_has_saved_addr = true;
    }
    return err;
}

/* ---- GAP callback ---- */

static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "BT auth complete, saving address");
            save_bt_addr(param->auth_cmpl.bda);
        }
        break;
    default:
        break;
    }
}

/* ---- HFP AG incoming audio callback ---- */

static void hfp_incoming_data_cb(const uint8_t *buf, uint32_t len)
{
    /* PCM data from intercom via SCO → forward to mesh */
    audio_pipe_send(buf, len);
}

/* ---- HFP AG outgoing audio callback ---- */

static uint32_t hfp_outgoing_data_cb(uint8_t *buf, uint32_t len)
{
    /* TODO: Fill buf with PCM from mesh rx buffer
     * For now, return silence */
    memset(buf, 0, len);
    return len;
}

/* ---- HFP AG event callback ---- */

static void hfp_cb(esp_hf_cb_event_t event, esp_hf_cb_param_t *param)
{
    switch (event) {
    case ESP_HF_CONNECTION_STATE_EVT:
        if (param->conn_stat.state == ESP_HF_CONNECTION_STATE_CONNECTED ||
            param->conn_stat.state == ESP_HF_CONNECTION_STATE_SLC_CONNECTED) {
            s_connected = true;
            ESP_LOGI(TAG, "Intercom connected");
            ui_set_bt_status("Connected");
            save_bt_addr(param->conn_stat.remote_bda);
        } else if (param->conn_stat.state == ESP_HF_CONNECTION_STATE_DISCONNECTED) {
            s_connected = false;
            s_sco_open = false;
            ESP_LOGW(TAG, "Intercom disconnected");
            ui_set_bt_status("Lost");
        }
        break;

    case ESP_HF_AUDIO_STATE_EVT:
        if (param->audio_stat.state == ESP_HF_AUDIO_STATE_CONNECTED) {
            s_sco_open = true;
            ESP_LOGI(TAG, "SCO audio link open");
        } else if (param->audio_stat.state == ESP_HF_AUDIO_STATE_DISCONNECTED) {
            s_sco_open = false;
            ESP_LOGW(TAG, "SCO audio link closed");
        }
        break;

    default:
        ESP_LOGD(TAG, "HFP event %d", event);
        break;
    }
}

/* ---- Discoverable timer ---- */

static void disco_timer_cb(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "Discoverable timeout, hiding");
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    ui_set_state(UI_STATE_IDLE);
}

/* ---- Public API ---- */

esp_err_t bt_hfp_init(void)
{
    /* Release BLE memory since we only use classic */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    /* Set device name */
    esp_bt_dev_set_device_name(BT_DEVICE_NAME);

    /* GAP callback */
    esp_bt_gap_register_callback(gap_cb);

    /* SSP (Simple Secure Pairing) */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE; /* just works */
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(iocap));

    /* HFP AG */
    esp_hf_ag_register_callback(hfp_cb);
    esp_hf_ag_init();

    /* Register audio data callbacks */
    esp_hf_ag_register_data_callback(hfp_incoming_data_cb, hfp_outgoing_data_cb);

    /* Create discoverable timer (one-shot) */
    s_disco_timer = xTimerCreate("disco", pdMS_TO_TICKS(60000), pdFALSE, NULL, disco_timer_cb);

    /* Load saved intercom address */
    load_bt_addr();

    if (s_has_saved_addr) {
        ESP_LOGI(TAG, "Saved intercom found, attempting reconnect");
        ui_set_bt_status("Reconnecting...");
        bt_hfp_reconnect();
    } else {
        ESP_LOGI(TAG, "No saved intercom, entering discoverable mode");
        bt_hfp_start_discoverable(60);
    }

    return ESP_OK;
}

esp_err_t bt_hfp_start_discoverable(int duration_sec)
{
    ESP_LOGI(TAG, "Discoverable for %ds", duration_sec);
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    ui_set_bt_status("Pairing...");
    ui_set_state(UI_STATE_BT_DISCO);

    xTimerChangePeriod(s_disco_timer, pdMS_TO_TICKS(duration_sec * 1000), 0);
    xTimerStart(s_disco_timer, 0);
    return ESP_OK;
}

esp_err_t bt_hfp_reconnect(void)
{
    if (!s_has_saved_addr) {
        ESP_LOGW(TAG, "No saved intercom address");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "Connecting to saved intercom...");
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    esp_hf_ag_slc_connect(s_intercom_addr);
    return ESP_OK;
}

esp_err_t bt_hfp_send_audio(const uint8_t *pcm, size_t len)
{
    if (!s_sco_open) return ESP_ERR_INVALID_STATE;
    /* Audio is sent via the outgoing_data_cb pull model.
     * TODO: Buffer pcm data so outgoing_data_cb can read it.
     * For now this is a placeholder — the real implementation needs
     * a ring buffer that outgoing_data_cb pulls from. */
    (void)pcm;
    (void)len;
    return ESP_OK;
}

bool bt_hfp_is_connected(void)
{
    return s_connected;
}
