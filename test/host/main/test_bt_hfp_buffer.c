/**
 * MeshCom Host Tests — bt_hfp SCO stream buffer
 * Tests the StreamBuffer mock and the write/read/overflow/underrun behavior.
 */
#include "unity.h"
#include "freertos/stream_buffer.h"
#include <string.h>

/* ---- Tests ---- */

void test_stream_buffer_write_read(void)
{
    mock_stream_buffers_reset();
    StreamBufferHandle_t sb = xStreamBufferCreate(256, 1);
    TEST_ASSERT_NOT_NULL(sb);

    int16_t samples[30];
    for (int i = 0; i < 30; i++) samples[i] = (int16_t)(i * 100);

    size_t written = xStreamBufferSend(sb, samples, sizeof(samples), 0);
    TEST_ASSERT_EQUAL(sizeof(samples), written);

    int16_t out[30];
    memset(out, 0, sizeof(out));
    size_t read = xStreamBufferReceive(sb, out, sizeof(out), 0);
    TEST_ASSERT_EQUAL(sizeof(out), read);
    TEST_ASSERT_EQUAL_MEMORY(samples, out, sizeof(samples));

    vStreamBufferDelete(sb);
}

void test_stream_buffer_overflow_drops(void)
{
    mock_stream_buffers_reset();
    /* Small buffer: 64 bytes */
    StreamBufferHandle_t sb = xStreamBufferCreate(64, 1);
    TEST_ASSERT_NOT_NULL(sb);

    uint8_t data[100];
    memset(data, 0xAA, sizeof(data));

    /* Write 100 bytes to 64-byte buffer: should accept only 64 */
    size_t written = xStreamBufferSend(sb, data, sizeof(data), 0);
    TEST_ASSERT_EQUAL(64, written);

    /* Buffer should be full */
    TEST_ASSERT_EQUAL(0, xStreamBufferSpacesAvailable(sb));
    TEST_ASSERT_EQUAL(64, xStreamBufferBytesAvailable(sb));

    /* Additional write should return 0 (non-blocking, no crash) */
    written = xStreamBufferSend(sb, data, 10, 0);
    TEST_ASSERT_EQUAL(0, written);

    vStreamBufferDelete(sb);
}

void test_stream_buffer_underrun_returns_zeros(void)
{
    mock_stream_buffers_reset();
    StreamBufferHandle_t sb = xStreamBufferCreate(256, 1);
    TEST_ASSERT_NOT_NULL(sb);

    /* Write 10 bytes */
    uint8_t src[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    xStreamBufferSend(sb, src, sizeof(src), 0);

    /* Try to read 20 bytes — should get 10 actual, rest unmodified */
    uint8_t out[20];
    memset(out, 0xFF, sizeof(out));  /* fill with 0xFF to detect partial read */
    size_t read = xStreamBufferReceive(sb, out, sizeof(out), 0);
    TEST_ASSERT_EQUAL(10, read);  /* only 10 bytes available */

    /* First 10 bytes should match source */
    TEST_ASSERT_EQUAL_MEMORY(src, out, 10);
    /* Remaining bytes should still be 0xFF (not touched by receive) */
    for (int i = 10; i < 20; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, out[i]);
    }

    /* Now simulate what hfp_outgoing_data_cb does: read + zero-fill */
    /* Write 5 bytes, then do the "read + memset" pattern */
    uint8_t src2[5] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
    xStreamBufferSend(sb, src2, 5, 0);

    uint8_t buf[20];
    memset(buf, 0xEE, sizeof(buf));
    read = xStreamBufferReceive(sb, buf, sizeof(buf), 0);
    if (read < sizeof(buf)) {
        memset(buf + read, 0, sizeof(buf) - read);
    }
    TEST_ASSERT_EQUAL(5, read);
    TEST_ASSERT_EQUAL_MEMORY(src2, buf, 5);
    /* Rest should be comfort noise (zeros) */
    for (int i = 5; i < 20; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, buf[i]);
    }

    vStreamBufferDelete(sb);
}
