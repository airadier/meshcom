/**
 * MeshCom Host Tests — audio_pipe
 * VAD (energy threshold) and anti-duplicate (seq window) tests
 *
 * We test the internal logic by calling public functions and checking
 * side effects via mock counters.
 */
#include "unity.h"
#include "audio_pipe.h"
#include "group_mgr.h"
#include "mocks.h"
#include <string.h>
#include <math.h>

/* ---- Helpers ---- */

/** Generate PCM frame of silence (all zeros), 8kHz 16-bit mono */
static void gen_silence(int16_t *buf, size_t n_samples)
{
    memset(buf, 0, n_samples * sizeof(int16_t));
}

/** Generate PCM frame with a loud tone */
static void gen_loud(int16_t *buf, size_t n_samples, int16_t amplitude)
{
    for (size_t i = 0; i < n_samples; i++) {
        /* Simple square wave */
        buf[i] = (i % 2 == 0) ? amplitude : -amplitude;
    }
}

static void setup(void)
{
    mock_state_reset();
    group_mgr_init();
    audio_pipe_init();
}

/* ---- VAD tests ---- */

/*
 * VAD threshold in audio_pipe.c is 500 (average abs amplitude per sample).
 * We test by calling audio_pipe_send() and checking if espnow_broadcast
 * was called (voice detected) or not (silence/below threshold).
 */

void test_vad_silence_rejected(void)
{
    setup();

    int16_t silence[30]; /* 30 samples = 60 bytes, typical HFP frame */
    gen_silence(silence, 30);

    audio_pipe_send((uint8_t *)silence, sizeof(silence));

    /* Silence should NOT trigger broadcast */
    TEST_ASSERT_EQUAL_INT(0, g_mock.espnow_broadcast_calls);
}

void test_vad_loud_signal_accepted(void)
{
    setup();

    int16_t loud[30];
    gen_loud(loud, 30, 5000); /* well above threshold of 500 */

    audio_pipe_send((uint8_t *)loud, sizeof(loud));

    /* Loud signal SHOULD trigger broadcast */
    TEST_ASSERT_EQUAL_INT(1, g_mock.espnow_broadcast_calls);
}

void test_vad_threshold_boundary(void)
{
    setup();

    /* Generate signal with average abs amplitude just below threshold (499) */
    int16_t below[30];
    for (size_t i = 0; i < 30; i++) {
        below[i] = 499; /* avg abs = 499 < 500 */
    }

    audio_pipe_send((uint8_t *)below, sizeof(below));
    TEST_ASSERT_EQUAL_INT(0, g_mock.espnow_broadcast_calls);

    /* Generate signal with average abs amplitude at threshold (501) */
    int16_t above[30];
    for (size_t i = 0; i < 30; i++) {
        above[i] = 501;
    }

    audio_pipe_send((uint8_t *)above, sizeof(above));
    TEST_ASSERT_EQUAL_INT(1, g_mock.espnow_broadcast_calls);
}

/* ---- Anti-duplicate tests ---- */

/*
 * We test dedup by creating encrypted packets with known seq numbers,
 * feeding them to audio_pipe_receive(), and checking bt_hfp_send_audio calls.
 */

/** Helper: create an encrypted packet from PCM with given seq */
static int make_encrypted_packet(uint16_t seq, uint8_t *pkt, size_t pkt_size)
{
    uint8_t pcm[] = {0x00, 0x10, 0x00, 0x10}; /* minimal PCM */
    return group_mgr_encrypt(pcm, sizeof(pcm), seq, pkt, pkt_size);
}

void test_dedup_rejects_duplicate_seq(void)
{
    setup();

    uint8_t pkt[128];
    int pkt_len = make_encrypted_packet(100, pkt, sizeof(pkt));
    TEST_ASSERT_GREATER_THAN(0, pkt_len);

    /* First receive — should pass through */
    audio_pipe_receive(pkt, pkt_len);
    TEST_ASSERT_EQUAL_INT(1, g_mock.bt_hfp_send_audio_calls);

    /* Second receive of same seq — must be a fresh packet (new nonce)
     * but same seq number. Re-encrypt to get valid GCM tag. */
    int pkt_len2 = make_encrypted_packet(100, pkt, sizeof(pkt));
    TEST_ASSERT_GREATER_THAN(0, pkt_len2);

    audio_pipe_receive(pkt, pkt_len2);
    TEST_ASSERT_EQUAL_INT(1, g_mock.bt_hfp_send_audio_calls); /* still 1 = rejected */
}

void test_dedup_accepts_new_seq(void)
{
    setup();

    uint8_t pkt[128];

    for (uint16_t seq = 0; seq < 10; seq++) {
        int pkt_len = make_encrypted_packet(seq, pkt, sizeof(pkt));
        TEST_ASSERT_GREATER_THAN(0, pkt_len);
        audio_pipe_receive(pkt, pkt_len);
    }

    /* All 10 distinct seqs should pass through */
    TEST_ASSERT_EQUAL_INT(10, g_mock.bt_hfp_send_audio_calls);
}

void test_dedup_window_wraps(void)
{
    setup();

    uint8_t pkt[128];

    /* Fill the dedup window (32 entries) */
    for (uint16_t seq = 0; seq < 32; seq++) {
        int pkt_len = make_encrypted_packet(seq, pkt, sizeof(pkt));
        audio_pipe_receive(pkt, pkt_len);
    }
    TEST_ASSERT_EQUAL_INT(32, g_mock.bt_hfp_send_audio_calls);

    /* Now seq 0 should have been evicted from the window.
     * Send 32 more to fully rotate the window. */
    for (uint16_t seq = 32; seq < 64; seq++) {
        int pkt_len = make_encrypted_packet(seq, pkt, sizeof(pkt));
        audio_pipe_receive(pkt, pkt_len);
    }
    TEST_ASSERT_EQUAL_INT(64, g_mock.bt_hfp_send_audio_calls);

    /* Seq 0 should now be accepted again (evicted from window) */
    int pkt_len = make_encrypted_packet(0, pkt, sizeof(pkt));
    audio_pipe_receive(pkt, pkt_len);
    TEST_ASSERT_EQUAL_INT(65, g_mock.bt_hfp_send_audio_calls);
}
