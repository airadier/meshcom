/**
 * MeshCom Host Tests — ESP-NOW fragmentation/reassembly
 */
#include "unity.h"
#include "espnow_comm.h"
#include <string.h>

/* ---- Tests ---- */

void test_frag_large_payload(void)
{
    /* 500 bytes should produce 3 fragments (239+239+22) */
    uint8_t data[500];
    for (int i = 0; i < 500; i++) data[i] = (uint8_t)(i & 0xFF);

    uint8_t frags[ESPNOW_MAX_FRAGS][ESPNOW_MAX_DATA];
    size_t frag_lens[ESPNOW_MAX_FRAGS];

    int n = espnow_fragment(data, sizeof(data), frags, frag_lens, ESPNOW_MAX_FRAGS);
    TEST_ASSERT_EQUAL(3, n);

    /* Each fragment should fit in ESP-NOW frame */
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_LESS_OR_EQUAL(ESPNOW_MAX_DATA, frag_lens[i]);
        TEST_ASSERT_GREATER_THAN(1, frag_lens[i]); /* at least header + 1 byte */
    }

    /* Headers should have correct index/total */
    for (int i = 0; i < n; i++) {
        uint8_t idx = (frags[i][0] >> 4) & 0x0F;
        uint8_t total = frags[i][0] & 0x0F;
        TEST_ASSERT_EQUAL(i, idx);
        TEST_ASSERT_EQUAL(3, total);
    }
}

void test_frag_reassemble_correct(void)
{
    /* Fragment and reassemble 500 bytes */
    uint8_t data[500];
    for (int i = 0; i < 500; i++) data[i] = (uint8_t)(i & 0xFF);

    uint8_t frags[ESPNOW_MAX_FRAGS][ESPNOW_MAX_DATA];
    size_t frag_lens[ESPNOW_MAX_FRAGS];

    int n = espnow_fragment(data, sizeof(data), frags, frag_lens, ESPNOW_MAX_FRAGS);
    TEST_ASSERT_EQUAL(3, n);

    /* Build const pointer array for reassemble */
    const uint8_t *frag_ptrs[ESPNOW_MAX_FRAGS];
    for (int i = 0; i < n; i++) frag_ptrs[i] = frags[i];

    uint8_t out[1024];
    int out_len = espnow_reassemble(frag_ptrs, frag_lens, n, out, sizeof(out));
    TEST_ASSERT_EQUAL(500, out_len);
    TEST_ASSERT_EQUAL_MEMORY(data, out, 500);
}

void test_frag_reassemble_out_of_order(void)
{
    /* Fragment 500 bytes, reassemble in reverse order */
    uint8_t data[500];
    for (int i = 0; i < 500; i++) data[i] = (uint8_t)(i & 0xFF);

    uint8_t frags[ESPNOW_MAX_FRAGS][ESPNOW_MAX_DATA];
    size_t frag_lens[ESPNOW_MAX_FRAGS];

    int n = espnow_fragment(data, sizeof(data), frags, frag_lens, ESPNOW_MAX_FRAGS);
    TEST_ASSERT_EQUAL(3, n);

    /* Reverse order */
    const uint8_t *frag_ptrs[ESPNOW_MAX_FRAGS];
    size_t rev_lens[ESPNOW_MAX_FRAGS];
    for (int i = 0; i < n; i++) {
        frag_ptrs[i] = frags[n - 1 - i];
        rev_lens[i] = frag_lens[n - 1 - i];
    }

    uint8_t out[1024];
    int out_len = espnow_reassemble(frag_ptrs, rev_lens, n, out, sizeof(out));
    TEST_ASSERT_EQUAL(500, out_len);
    TEST_ASSERT_EQUAL_MEMORY(data, out, 500);
}

void test_frag_missing_fragment(void)
{
    /* Fragment 500 bytes (3 frags), then try to reassemble with only 2 */
    uint8_t data[500];
    for (int i = 0; i < 500; i++) data[i] = (uint8_t)(i & 0xFF);

    uint8_t frags[ESPNOW_MAX_FRAGS][ESPNOW_MAX_DATA];
    size_t frag_lens[ESPNOW_MAX_FRAGS];

    int n = espnow_fragment(data, sizeof(data), frags, frag_lens, ESPNOW_MAX_FRAGS);
    TEST_ASSERT_EQUAL(3, n);

    /* Only pass 2 of 3 fragments — total field says 3 but n_frags is 2 */
    const uint8_t *frag_ptrs[2] = { frags[0], frags[1] };
    size_t lens[2] = { frag_lens[0], frag_lens[1] };

    uint8_t out[1024];
    int out_len = espnow_reassemble(frag_ptrs, lens, 2, out, sizeof(out));
    TEST_ASSERT_EQUAL(-1, out_len); /* Should fail: missing fragment */
}

void test_frag_too_large(void)
{
    /* Payload that would need > 4 fragments should fail */
    uint8_t data[1000]; /* 1000 / 239 = 5 frags, > ESPNOW_MAX_FRAGS */
    memset(data, 0xAA, sizeof(data));

    uint8_t frags[ESPNOW_MAX_FRAGS][ESPNOW_MAX_DATA];
    size_t frag_lens[ESPNOW_MAX_FRAGS];

    int n = espnow_fragment(data, sizeof(data), frags, frag_lens, ESPNOW_MAX_FRAGS);
    TEST_ASSERT_EQUAL(-1, n);
}

void test_frag_small_payload_no_frag(void)
{
    /* Payload that fits in one frame should produce 1 fragment */
    uint8_t data[100];
    memset(data, 0xBB, sizeof(data));

    uint8_t frags[ESPNOW_MAX_FRAGS][ESPNOW_MAX_DATA];
    size_t frag_lens[ESPNOW_MAX_FRAGS];

    int n = espnow_fragment(data, sizeof(data), frags, frag_lens, ESPNOW_MAX_FRAGS);
    TEST_ASSERT_EQUAL(1, n);

    /* Payload should be data with 1-byte header prepended */
    TEST_ASSERT_EQUAL(101, frag_lens[0]);
    TEST_ASSERT_EQUAL_MEMORY(data, frags[0] + 1, 100);
}
