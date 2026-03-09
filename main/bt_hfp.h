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

/**
 * Scan for nearby Bluetooth HFP devices (intercom in pairing mode).
 * Connects automatically to the device with strongest RSSI.
 * Duration: ~30s. User must first put the intercom into pairing mode.
 */
esp_err_t bt_hfp_start_scan(void);

/** Force reconnection to saved intercom */
esp_err_t bt_hfp_reconnect(void);

/**
 * Send PCM audio to intercom via SCO link.
 * Writes to internal stream buffer (non-blocking, drops if full).
 * @param pcm   PCM samples (16-bit signed, 8kHz mono)
 * @param len   Length in bytes
 */
esp_err_t bt_hfp_send_audio(const uint8_t *pcm, size_t len);

/** Check if intercom is connected */
bool bt_hfp_is_connected(void);
