/**
 * MeshCom Host Tests — group_mgr
 * AES-128-GCM encrypt/decrypt, key generation, NVS persistence
 */
#include "unity.h"
#include "group_mgr.h"
#include "mocks.h"
#include <string.h>
#include <stdbool.h>

/* ---- Helpers ---- */

static void init_fresh_group(void)
{
    mock_state_reset();
    /* group_mgr_init() loads from NVS or generates new.
     * On linux host with real NVS component it should work. */
    group_mgr_init();
}

/* ---- Tests ---- */

void test_group_mgr_encrypt_decrypt_roundtrip(void)
{
    init_fresh_group();

    uint8_t pcm[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                     0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    size_t pcm_len = sizeof(pcm);

    uint8_t pkt[256];
    int pkt_len = group_mgr_encrypt(pcm, pcm_len, 42, pkt, sizeof(pkt));
    TEST_ASSERT_GREATER_THAN(0, pkt_len);
    TEST_ASSERT_EQUAL_INT((int)(pcm_len + GROUP_OVERHEAD), pkt_len);

    /* Decrypt */
    uint8_t out[256];
    uint16_t seq_out = 0;
    int out_len = group_mgr_decrypt(pkt, pkt_len, &seq_out, out, sizeof(out));

    TEST_ASSERT_EQUAL_INT((int)pcm_len, out_len);
    TEST_ASSERT_EQUAL_UINT16(42, seq_out);
    TEST_ASSERT_EQUAL_MEMORY(pcm, out, pcm_len);
}

void test_group_mgr_decrypt_wrong_group(void)
{
    init_fresh_group();

    uint8_t pcm[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t pkt[256];
    int pkt_len = group_mgr_encrypt(pcm, sizeof(pcm), 1, pkt, sizeof(pkt));
    TEST_ASSERT_GREATER_THAN(0, pkt_len);

    /* Corrupt group_id in packet header */
    pkt[0] ^= 0xFF;
    pkt[1] ^= 0xFF;

    uint8_t out[256];
    int out_len = group_mgr_decrypt(pkt, pkt_len, NULL, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(-1, out_len);
}

void test_group_mgr_decrypt_tampered(void)
{
    init_fresh_group();

    uint8_t pcm[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t pkt[256];
    int pkt_len = group_mgr_encrypt(pcm, sizeof(pcm), 7, pkt, sizeof(pkt));
    TEST_ASSERT_GREATER_THAN(0, pkt_len);

    /* Tamper with ciphertext (byte after header) */
    pkt[GROUP_HEADER_LEN] ^= 0x01;

    uint8_t out[256];
    int out_len = group_mgr_decrypt(pkt, pkt_len, NULL, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(-1, out_len); /* GCM auth should fail */
}

void test_group_mgr_new_group_changes_key(void)
{
    init_fresh_group();

    uint8_t key1[GROUP_KEY_LEN], key2[GROUP_KEY_LEN];
    uint16_t id1, id2;

    group_mgr_get_key(key1);
    group_mgr_get_id(&id1);

    group_mgr_new_group();

    group_mgr_get_key(key2);
    group_mgr_get_id(&id2);

    /* Extremely unlikely that random key/id are identical */
    bool keys_differ = (memcmp(key1, key2, GROUP_KEY_LEN) != 0);
    bool ids_differ = (id1 != id2);
    TEST_ASSERT_TRUE(keys_differ || ids_differ);
}

void test_group_mgr_save_and_load_key(void)
{
    init_fresh_group();

    /* Save a known key */
    uint8_t known_key[GROUP_KEY_LEN] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
    };
    uint16_t known_id = 0x1234;

    TEST_ASSERT_EQUAL(ESP_OK, group_mgr_save_key(known_key, known_id));

    /* Read back */
    uint8_t read_key[GROUP_KEY_LEN];
    uint16_t read_id;
    group_mgr_get_key(read_key);
    group_mgr_get_id(&read_id);

    TEST_ASSERT_EQUAL_MEMORY(known_key, read_key, GROUP_KEY_LEN);
    TEST_ASSERT_EQUAL_UINT16(known_id, read_id);

    /* Encrypt with saved key, decrypt should work */
    uint8_t data[] = {0x42, 0x42};
    uint8_t pkt[64];
    int pkt_len = group_mgr_encrypt(data, sizeof(data), 99, pkt, sizeof(pkt));
    TEST_ASSERT_GREATER_THAN(0, pkt_len);

    uint8_t out[64];
    uint16_t seq;
    int out_len = group_mgr_decrypt(pkt, pkt_len, &seq, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(2, out_len);
    TEST_ASSERT_EQUAL_UINT16(99, seq);
    TEST_ASSERT_EQUAL_MEMORY(data, out, 2);
}
