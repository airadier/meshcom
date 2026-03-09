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

    return UNITY_END();
}
