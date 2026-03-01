/**
 * MeshCom - ESP-NOW Communication
 * WiFi station mode (no AP), ESP-NOW broadcast
 */
#include "espnow_comm.h"
#include "audio_pipe.h"
#include "pairing.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"

#include <string.h>

static const char *TAG = "espnow";

#define ESPNOW_CHANNEL  1
#define ESPNOW_MAX_DATA 250

/* Broadcast MAC */
static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* ---- Callbacks ---- */

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len < 4) return;

    /* Check if it's a pairing packet */
    uint32_t magic = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                     ((uint32_t)data[2] << 8)  | data[3];
    if (magic == 0x4D435052) {  /* PAIRING_MAGIC "MCPR" */
        pairing_handle_packet(data, len);
        return;
    }

    /* Audio packet */
    audio_pipe_receive(data, len);
}

static void send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "ESP-NOW send failed");
    }
}

/* ---- Public API ---- */

esp_err_t espnow_comm_init(void)
{
    /* Initialize networking stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* WiFi init in station mode */
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Fix channel */
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    /* ESP-NOW init */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));

    /* Add broadcast peer */
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "ESP-NOW initialized on channel %d", ESPNOW_CHANNEL);
    return ESP_OK;
}

esp_err_t espnow_broadcast(const uint8_t *data, size_t len)
{
    if (len > ESPNOW_MAX_DATA) {
        ESP_LOGW(TAG, "Packet too large: %d > %d", (int)len, ESPNOW_MAX_DATA);
        return ESP_ERR_INVALID_SIZE;
    }
    return esp_now_send(s_broadcast_mac, data, len);
}
