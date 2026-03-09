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
#include "freertos/stream_buffer.h"

#include <string.h>
#include <stdbool.h>

static const char *TAG = "bt_hfp";

/* SCO outgoing audio buffer: ~4KB for PCM 8kHz 16-bit mono (~250ms) */
#define SCO_OUT_BUF_SIZE    4096
static StreamBufferHandle_t s_sco_out_buf = NULL;

#define NVS_NAMESPACE   "meshcom"
#define NVS_KEY_BTADDR  "intercom_addr"
#define BT_DEVICE_NAME  "MeshCom"

static esp_bd_addr_t s_intercom_addr;
static bool s_has_saved_addr = false;
static bool s_connected = false;
static bool s_sco_open = false;
static TimerHandle_t s_scan_timer = NULL;

/* Best device found during scan */
static esp_bd_addr_t s_scan_best_addr;
static int8_t        s_scan_best_rssi = -127;
static bool          s_scan_found = false;

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

/* HFP HF service UUID (the intercom side) */
#define UUID_HFP_HF 0x111E

static bool device_has_hfp(esp_bt_gap_cb_param_t *param)
{
    /* Walk EIR looking for 0x111E (HFP Hands-Free) */
    uint8_t *eir = param->disc_res.prop[0].val;   /* simplified */
    (void)eir;
    /* Full EIR parsing omitted for brevity — in practice use
     * esp_bt_gap_resolve_eir_data() to find UUID16 list.
     * For PoC: accept any device found during scan (user puts
     * only intercom into pairing mode). */
    return true;
}

static void scan_timer_cb(TimerHandle_t timer);

static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {

    case ESP_BT_GAP_DISC_RES_EVT: {
        /* A device was found during inquiry scan */
        int8_t rssi = -127;
        for (int i = 0; i < param->disc_res.num_prop; i++) {
            if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_RSSI) {
                rssi = *(int8_t *)param->disc_res.prop[i].val;
            }
        }
        ESP_LOGI(TAG, "Found device RSSI=%d", rssi);
        /* Keep the strongest signal device */
        if (rssi > s_scan_best_rssi) {
            s_scan_best_rssi = rssi;
            memcpy(s_scan_best_addr, param->disc_res.bda, sizeof(esp_bd_addr_t));
            s_scan_found = true;
        }
        break;
    }

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            if (s_scan_found) {
                ESP_LOGI(TAG, "Scan done, connecting to best device (RSSI=%d)", s_scan_best_rssi);
                save_bt_addr(s_scan_best_addr);
                esp_hf_ag_slc_connect(s_scan_best_addr);
                ui_set_bt_status("Connecting...");
            } else {
                ESP_LOGW(TAG, "Scan done, no device found");
                ui_set_bt_status("Not found");
                ui_set_state(UI_STATE_IDLE);
            }
        }
        break;

    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "BT auth complete");
            save_bt_addr(param->auth_cmpl.bda);
        }
        break;

    default:
        break;
    }
}

static void scan_timer_cb(TimerHandle_t timer)
{
    /* Stop scan if still running after timeout */
    esp_bt_gap_cancel_discovery();
    ui_set_state(UI_STATE_IDLE);
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
    if (!s_sco_out_buf) {
        memset(buf, 0, len);
        return len;
    }

    size_t read = xStreamBufferReceive(s_sco_out_buf, buf, len, 0);
    /* Fill remaining with zeros (comfort noise) if underrun */
    if (read < len) {
        memset(buf + read, 0, len - read);
    }
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

    /* Create SCO outgoing audio buffer */
    s_sco_out_buf = xStreamBufferCreate(SCO_OUT_BUF_SIZE, 1);
    if (!s_sco_out_buf) {
        ESP_LOGE(TAG, "Failed to create SCO output buffer");
        return ESP_ERR_NO_MEM;
    }

    /* Create scan timeout timer (one-shot, 30s) */
    s_scan_timer = xTimerCreate("scan_to", pdMS_TO_TICKS(30000), pdFALSE, NULL, scan_timer_cb);

    /* Load saved intercom address */
    load_bt_addr();

    if (s_has_saved_addr) {
        ESP_LOGI(TAG, "Saved intercom found, attempting reconnect");
        ui_set_bt_status("Reconnecting...");
        bt_hfp_reconnect();
    } else {
        ESP_LOGI(TAG, "No saved intercom — hold BTN_B 3s to scan for intercom");
        ui_set_bt_status("No intercom");
    }

    return ESP_OK;
}

esp_err_t bt_hfp_start_scan(void)
{
    /* Reset scan state */
    s_scan_found = false;
    s_scan_best_rssi = -127;

    ESP_LOGI(TAG, "Starting BT scan for intercom (30s)...");
    ui_set_bt_status("Scanning...");
    ui_set_state(UI_STATE_BT_SCAN);

    /* Inquiry: General inquiry access code, 30s (=30*1.28 = ~38s max),
     * unlimited responses */
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 24, 0);

    /* Safety timeout in case discovery never fires STOPPED */
    xTimerStart(s_scan_timer, 0);
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
    if (!s_sco_out_buf) return ESP_ERR_INVALID_STATE;

    /* Non-blocking write to stream buffer.
     * If buffer is full, data is silently dropped — better to lose
     * audio than to block the mesh receive path. */
    size_t written = xStreamBufferSend(s_sco_out_buf, pcm, len, 0);
    if (written < len) {
        ESP_LOGD(TAG, "SCO buffer full, dropped %d bytes", (int)(len - written));
    }
    return ESP_OK;
}

bool bt_hfp_is_connected(void)
{
    return s_connected;
}
