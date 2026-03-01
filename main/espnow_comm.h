/**
 * MeshCom - ESP-NOW Communication
 * Broadcast audio packets over ESP-NOW
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/** Initialize WiFi (station, no connect) and ESP-NOW */
esp_err_t espnow_comm_init(void);

/** Broadcast data to all peers (max 250 bytes) */
esp_err_t espnow_broadcast(const uint8_t *data, size_t len);
