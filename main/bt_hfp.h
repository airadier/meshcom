/**
 * MeshCom - Bluetooth HFP AG
 * Connects to helmet intercom as hands-free audio gateway
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

/** Initialize Bluetooth stack and HFP AG profile */
esp_err_t bt_hfp_init(void);

/** Enter discoverable mode for duration_sec seconds */
esp_err_t bt_hfp_start_discoverable(int duration_sec);

/** Force reconnection to saved intercom */
esp_err_t bt_hfp_reconnect(void);

/** Send PCM audio to intercom via SCO link */
esp_err_t bt_hfp_send_audio(const uint8_t *pcm, size_t len);

/** Check if intercom is connected */
bool bt_hfp_is_connected(void);
