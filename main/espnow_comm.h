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

/**
 * Broadcast data to all peers.
 * If data > ESPNOW_FRAG_PAYLOAD (240B), automatically fragments
 * into up to 4 chunks with 1-byte frag header each.
 */
esp_err_t espnow_broadcast(const uint8_t *data, size_t len);

/* Fragmentation constants */
#define ESPNOW_MAX_DATA     250
#define ESPNOW_FRAG_PAYLOAD 240  /* 250 - 10B margin */
#define ESPNOW_MAX_FRAGS    4

/**
 * Fragment a large payload into chunks for ESP-NOW.
 * Each fragment: [header:1][payload:N]
 * Header byte: [frag_index:4 | frag_total:4] (0-based index)
 *
 * @param data      Input data
 * @param len       Input length
 * @param frags     Output array of fragment buffers (caller provides)
 * @param frag_lens Output array of fragment lengths
 * @param max_frags Maximum fragments (array size)
 * @return Number of fragments, or -1 on error
 */
int espnow_fragment(const uint8_t *data, size_t len,
                    uint8_t frags[][ESPNOW_MAX_DATA], size_t *frag_lens,
                    int max_frags);

/**
 * Reassemble fragments into original payload.
 * @param frags     Array of received fragments (with header byte)
 * @param frag_lens Array of fragment lengths
 * @param n_frags   Number of fragments received
 * @param out       Output buffer
 * @param out_size  Output buffer size
 * @return Reassembled length, or -1 on error (missing/invalid fragments)
 */
int espnow_reassemble(const uint8_t *frags[], const size_t *frag_lens,
                      int n_frags, uint8_t *out, size_t out_size);
