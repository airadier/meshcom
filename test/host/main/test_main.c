/**
 * MeshCom Host Tests — entry point
 */
#include "unity.h"

/* test_group_mgr.c */
extern void test_group_mgr_encrypt_decrypt_roundtrip(void);
extern void test_group_mgr_decrypt_wrong_group(void);
extern void test_group_mgr_decrypt_tampered(void);
extern void test_group_mgr_new_group_changes_key(void);
extern void test_group_mgr_save_and_load_key(void);
extern void test_group_mgr_nvs_persistence_across_reinit(void);

/* test_audio_pipe.c */
extern void test_vad_silence_rejected(void);
extern void test_vad_loud_signal_accepted(void);
extern void test_vad_threshold_boundary(void);
extern void test_dedup_rejects_duplicate_seq(void);
extern void test_dedup_accepts_new_seq(void);
extern void test_dedup_window_wraps(void);
extern void test_vad_hold_short_silence_stays_active(void);
extern void test_vad_hold_long_silence_deactivates(void);

/* test_bt_hfp_buffer.c */
extern void test_stream_buffer_write_read(void);
extern void test_stream_buffer_overflow_drops(void);
extern void test_stream_buffer_underrun_returns_zeros(void);

/* test_fragmentation.c */
extern void test_frag_large_payload(void);
extern void test_frag_reassemble_correct(void);
extern void test_frag_reassemble_out_of_order(void);
extern void test_frag_missing_fragment(void);
extern void test_frag_too_large(void);
extern void test_frag_small_payload_no_frag(void);

int main(void)
{
    UNITY_BEGIN();

    /* group_mgr tests */
    RUN_TEST(test_group_mgr_encrypt_decrypt_roundtrip);
    RUN_TEST(test_group_mgr_decrypt_wrong_group);
    RUN_TEST(test_group_mgr_decrypt_tampered);
    RUN_TEST(test_group_mgr_new_group_changes_key);
    RUN_TEST(test_group_mgr_save_and_load_key);
    RUN_TEST(test_group_mgr_nvs_persistence_across_reinit);

    /* audio_pipe tests */
    RUN_TEST(test_vad_silence_rejected);
    RUN_TEST(test_vad_loud_signal_accepted);
    RUN_TEST(test_vad_threshold_boundary);
    RUN_TEST(test_dedup_rejects_duplicate_seq);
    RUN_TEST(test_dedup_accepts_new_seq);
    RUN_TEST(test_dedup_window_wraps);
    RUN_TEST(test_vad_hold_short_silence_stays_active);
    RUN_TEST(test_vad_hold_long_silence_deactivates);

    /* bt_hfp SCO buffer tests */
    RUN_TEST(test_stream_buffer_write_read);
    RUN_TEST(test_stream_buffer_overflow_drops);
    RUN_TEST(test_stream_buffer_underrun_returns_zeros);

    /* fragmentation tests */
    RUN_TEST(test_frag_large_payload);
    RUN_TEST(test_frag_reassemble_correct);
    RUN_TEST(test_frag_reassemble_out_of_order);
    RUN_TEST(test_frag_missing_fragment);
    RUN_TEST(test_frag_too_large);
    RUN_TEST(test_frag_small_payload_no_frag);

    return UNITY_END();
}
