/**
 * MeshCom - Audio Pipeline
 * Bridges HFP SCO ↔ ESP-NOW with encryption, VAD, and dedup
 */
#include "audio_pipe.h"
#include "group_mgr.h"
#include "espnow_comm.h"
#include "bt_hfp.h"
#include "ui.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "audio_pipe";

/* PCM: 8kHz 16-bit mono. Typical HFP SCO frame = 60 bytes (30 samples) */
#define PCM_SAMPLE_RATE     8000
#define PCM_BYTES_PER_SAMPLE 2

/* VAD: energy threshold (sum of abs(sample) over frame) */
#define VAD_THRESHOLD       500

/* Sequence number */
static uint16_t s_tx_seq = 0;

/* Anti-duplicate: circular buffer of last 32 received sequence numbers */
#define DEDUP_WINDOW        32
static uint16_t s_seen_seq[DEDUP_WINDOW];
static int s_seen_idx = 0;

/* Activity tracking */
static int64_t s_last_tx_us = 0;
static int64_t s_last_rx_us = 0;
#define ACTIVITY_TIMEOUT_US  500000  /* 500ms */

/* VAD hold time: keep transmitting for VAD_HOLD_MS after last voice frame */
static int64_t s_last_voice_us = 0;
#define VAD_HOLD_US  ((int64_t)VAD_HOLD_MS * 1000)

/* ---- VAD ---- */

static bool vad_detect(const uint8_t *pcm, size_t len)
{
    /* TODO: Replace with proper VAD (e.g., WebRTC VAD or energy + ZCR) */
    size_t n_samples = len / PCM_BYTES_PER_SAMPLE;
    if (n_samples == 0) return false;

    int32_t energy = 0;
    const int16_t *samples = (const int16_t *)pcm;
    for (size_t i = 0; i < n_samples; i++) {
        int16_t s = samples[i];
        energy += (s < 0) ? -s : s;
    }
    energy /= (int32_t)n_samples;

    return (energy > VAD_THRESHOLD);
}

/* ---- Dedup ---- */

static bool is_duplicate(uint16_t seq)
{
    for (int i = 0; i < DEDUP_WINDOW; i++) {
        if (s_seen_seq[i] == seq) return true;
    }
    return false;
}

static void record_seq(uint16_t seq)
{
    s_seen_seq[s_seen_idx] = seq;
    s_seen_idx = (s_seen_idx + 1) % DEDUP_WINDOW;
}

/* ---- Public API ---- */

esp_err_t audio_pipe_init(void)
{
    s_tx_seq = (uint16_t)(esp_random() & 0xFFFF);
    s_last_voice_us = 0;
    memset(s_seen_seq, 0xFF, sizeof(s_seen_seq)); /* 0xFFFF = invalid seq */
    ESP_LOGI(TAG, "Audio pipeline initialized (PCM %dHz, VAD threshold %d)",
             PCM_SAMPLE_RATE, VAD_THRESHOLD);
    return ESP_OK;
}

void audio_pipe_send(const uint8_t *pcm, size_t len)
{
    /* VAD gate with hold time */
    bool voice_now = vad_detect(pcm, len);
    if (voice_now) {
        s_last_voice_us = esp_timer_get_time();
    } else {
        /* Check hold time: keep active for VAD_HOLD_MS after last voice */
        int64_t elapsed = esp_timer_get_time() - s_last_voice_us;
        if (s_last_voice_us == 0 || elapsed > VAD_HOLD_US) {
            return; /* No voice and hold expired — drop frame */
        }
    }

    /* Encrypt */
    uint8_t pkt[250];
    int pkt_len = group_mgr_encrypt(pcm, len, s_tx_seq, pkt, sizeof(pkt));
    if (pkt_len < 0) {
        ESP_LOGE(TAG, "Encrypt failed");
        return;
    }

    /* Check ESP-NOW size limit */
    if (pkt_len > 250) {
        ESP_LOGW(TAG, "Encrypted packet too large: %d bytes (PCM %d + overhead %d)",
                 pkt_len, (int)len, (int)GROUP_OVERHEAD);
        /* TODO: Implement Opus compression to reduce payload size */
        return;
    }

    s_tx_seq++;
    espnow_broadcast(pkt, pkt_len);

    s_last_tx_us = esp_timer_get_time();
    ui_set_activity("TX");
}

void audio_pipe_receive(const uint8_t *packet, size_t len)
{
    uint16_t seq;
    uint8_t pcm[250];

    int pcm_len = group_mgr_decrypt(packet, len, &seq, pcm, sizeof(pcm));
    if (pcm_len < 0) {
        /* Wrong group or auth failure — silent discard */
        return;
    }

    /* Dedup */
    if (is_duplicate(seq)) return;
    record_seq(seq);

    /* Send to intercom */
    bt_hfp_send_audio(pcm, pcm_len);

    s_last_rx_us = esp_timer_get_time();
    ui_set_activity("RX");
}

bool audio_pipe_is_tx(void)
{
    return (esp_timer_get_time() - s_last_tx_us) < ACTIVITY_TIMEOUT_US;
}

bool audio_pipe_is_rx(void)
{
    return (esp_timer_get_time() - s_last_rx_us) < ACTIVITY_TIMEOUT_US;
}
