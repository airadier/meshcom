/**
 * MeshCom Host Tests — mock/stub declarations
 *
 * Provides stubs for ESP-IDF functions not available on linux target
 * (BT, ESP-NOW, GPIO, etc.) so group_mgr.c and audio_pipe.c can link.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* Mock NVS: simple in-memory key-value store */
void mock_nvs_reset(void);

/* Track calls to mocked functions */
typedef struct {
    int espnow_broadcast_calls;
    int bt_hfp_send_audio_calls;
    const uint8_t *last_broadcast_data;
    size_t last_broadcast_len;
} mock_state_t;

extern mock_state_t g_mock;
void mock_state_reset(void);
