/**
 * MeshCom - Group Pairing
 * Share/join group keys over ESP-NOW broadcast
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#define PAIRING_MAGIC       0x4D435052  /* "MCPR" */

/** Start sharing current group key via broadcast (30s) */
esp_err_t pairing_start_share(void);

/** Start listening for a group key broadcast (30s) */
esp_err_t pairing_start_join(void);

/** Stop any active pairing mode (share or join) */
void pairing_stop(void);

/** Check if pairing is currently active */
bool pairing_is_active(void);

/** Called by espnow_comm when a pairing packet is received */
void pairing_handle_packet(const uint8_t *data, size_t len);
