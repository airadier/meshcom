/**
 * MeshCom - Audio Pipeline
 * Bridge between HFP SCO audio and ESP-NOW mesh
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

/** Initialize audio pipeline */
esp_err_t audio_pipe_init(void);

/** Called by HFP when PCM arrives from intercom — encrypts and broadcasts */
void audio_pipe_send(const uint8_t *pcm, size_t len);

/** Called by ESP-NOW when a packet arrives — decrypts and sends to HFP */
void audio_pipe_receive(const uint8_t *packet, size_t len);

/** Returns true if currently transmitting */
bool audio_pipe_is_tx(void);

/** Returns true if currently receiving */
bool audio_pipe_is_rx(void);

/** VAD hold time in milliseconds — keeps VAD active after last voice frame */
#define VAD_HOLD_MS 250
