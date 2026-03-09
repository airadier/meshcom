/**
 * MeshCom Host Tests — pairing packet truncation (T9)
 * Tests that truncated pairing packets are safely ignored.
 */
#include "unity.h"
#include "pairing.h"
#include "group_mgr.h"
#include "mocks.h"
#include <string.h>

/* Pairing packet format: [magic:4][group_id:2][key:16] = 22 bytes */
#define PAIRING_PKT_LEN  (4 + 2 + GROUP_KEY_LEN)

/** Build a valid pairing packet */
static void build_valid_pairing_pkt(uint8_t *pkt)
{
    /* Magic "MCPR" */
    pkt[0] = (PAIRING_MAGIC >> 24) & 0xFF;
    pkt[1] = (PAIRING_MAGIC >> 16) & 0xFF;
    pkt[2] = (PAIRING_MAGIC >> 8)  & 0xFF;
    pkt[3] = PAIRING_MAGIC & 0xFF;

    /* Group ID */
    pkt[4] = 0x12;
    pkt[5] = 0x34;

    /* Key (16 bytes) */
    for (int i = 0; i < GROUP_KEY_LEN; i++) {
        pkt[6 + i] = (uint8_t)(0xA0 + i);
    }
}

void test_pairing_truncated_packet_ignored(void)
{
    mock_state_reset();
    group_mgr_init();

    /* Build a valid pairing packet */
    uint8_t pkt[PAIRING_PKT_LEN];
    build_valid_pairing_pkt(pkt);

    /* Get current key to verify it doesn't change */
    uint8_t key_before[GROUP_KEY_LEN];
    uint16_t id_before;
    group_mgr_get_key(key_before);
    group_mgr_get_id(&id_before);

    /* Feed truncated packet (magic correct but sizeof < expected) */
    /* Must not crash, and key should not change */
    pairing_handle_packet(pkt, 10); /* only 10 bytes, need 22 */

    uint8_t key_after[GROUP_KEY_LEN];
    uint16_t id_after;
    group_mgr_get_key(key_after);
    group_mgr_get_id(&id_after);

    TEST_ASSERT_EQUAL_MEMORY(key_before, key_after, GROUP_KEY_LEN);
    TEST_ASSERT_EQUAL_UINT16(id_before, id_after);
}

void test_pairing_correct_size_truncated_key_ignored(void)
{
    mock_state_reset();
    group_mgr_init();

    /* Build a valid pairing packet but truncate key area */
    uint8_t pkt[PAIRING_PKT_LEN];
    build_valid_pairing_pkt(pkt);

    uint8_t key_before[GROUP_KEY_LEN];
    uint16_t id_before;
    group_mgr_get_key(key_before);
    group_mgr_get_id(&id_before);

    /* Pass with length 6+10 = 16 (magic + gid + only 10 of 16 key bytes) */
    pairing_handle_packet(pkt, 16); /* < 22, so should be ignored */

    uint8_t key_after[GROUP_KEY_LEN];
    uint16_t id_after;
    group_mgr_get_key(key_after);
    group_mgr_get_id(&id_after);

    TEST_ASSERT_EQUAL_MEMORY(key_before, key_after, GROUP_KEY_LEN);
    TEST_ASSERT_EQUAL_UINT16(id_before, id_after);
}

void test_pairing_zero_length_ignored(void)
{
    mock_state_reset();
    group_mgr_init();

    uint8_t key_before[GROUP_KEY_LEN];
    group_mgr_get_key(key_before);

    /* Zero-length packet should not crash */
    pairing_handle_packet(NULL, 0);

    uint8_t key_after[GROUP_KEY_LEN];
    group_mgr_get_key(key_after);
    TEST_ASSERT_EQUAL_MEMORY(key_before, key_after, GROUP_KEY_LEN);
}
