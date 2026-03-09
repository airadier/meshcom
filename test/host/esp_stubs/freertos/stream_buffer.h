/**
 * ESP-IDF stub: freertos/stream_buffer.h
 * Minimal mock of FreeRTOS StreamBuffer for host testing.
 * Uses a simple circular buffer with static storage.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Opaque handle */
typedef struct StreamBufferDef_t *StreamBufferHandle_t;

/* Static storage for mock stream buffers (support up to 4 instances) */
#define MOCK_STREAM_BUF_MAX_SIZE 8192
#define MOCK_STREAM_BUF_COUNT   4

typedef struct StreamBufferDef_t {
    uint8_t buf[MOCK_STREAM_BUF_MAX_SIZE];
    size_t capacity;
    size_t head;  /* write position */
    size_t tail;  /* read position */
    size_t count; /* bytes stored */
    int    used;
} StreamBufferDef_t;

static StreamBufferDef_t s_mock_stream_bufs[MOCK_STREAM_BUF_COUNT];

static inline StreamBufferHandle_t xStreamBufferCreate(size_t xBufferSizeBytes,
                                                        size_t xTriggerLevelBytes)
{
    (void)xTriggerLevelBytes;
    for (int i = 0; i < MOCK_STREAM_BUF_COUNT; i++) {
        if (!s_mock_stream_bufs[i].used) {
            StreamBufferDef_t *sb = &s_mock_stream_bufs[i];
            sb->used = 1;
            sb->capacity = (xBufferSizeBytes > MOCK_STREAM_BUF_MAX_SIZE)
                           ? MOCK_STREAM_BUF_MAX_SIZE : xBufferSizeBytes;
            sb->head = 0;
            sb->tail = 0;
            sb->count = 0;
            memset(sb->buf, 0, sb->capacity);
            return sb;
        }
    }
    return NULL;
}

static inline void vStreamBufferDelete(StreamBufferHandle_t xStreamBuffer)
{
    if (xStreamBuffer) {
        xStreamBuffer->used = 0;
    }
}

static inline size_t xStreamBufferSend(StreamBufferHandle_t xStreamBuffer,
                                        const void *pvTxData,
                                        size_t xDataLengthBytes,
                                        uint32_t xTicksToWait)
{
    (void)xTicksToWait;
    if (!xStreamBuffer) return 0;

    size_t available = xStreamBuffer->capacity - xStreamBuffer->count;
    size_t to_write = (xDataLengthBytes < available) ? xDataLengthBytes : available;
    const uint8_t *src = (const uint8_t *)pvTxData;

    for (size_t i = 0; i < to_write; i++) {
        xStreamBuffer->buf[xStreamBuffer->head] = src[i];
        xStreamBuffer->head = (xStreamBuffer->head + 1) % xStreamBuffer->capacity;
    }
    xStreamBuffer->count += to_write;
    return to_write;
}

static inline size_t xStreamBufferReceive(StreamBufferHandle_t xStreamBuffer,
                                           void *pvRxData,
                                           size_t xBufferLengthBytes,
                                           uint32_t xTicksToWait)
{
    (void)xTicksToWait;
    if (!xStreamBuffer) return 0;

    size_t to_read = (xBufferLengthBytes < xStreamBuffer->count)
                     ? xBufferLengthBytes : xStreamBuffer->count;
    uint8_t *dst = (uint8_t *)pvRxData;

    for (size_t i = 0; i < to_read; i++) {
        dst[i] = xStreamBuffer->buf[xStreamBuffer->tail];
        xStreamBuffer->tail = (xStreamBuffer->tail + 1) % xStreamBuffer->capacity;
    }
    xStreamBuffer->count -= to_read;
    return to_read;
}

static inline size_t xStreamBufferBytesAvailable(StreamBufferHandle_t xStreamBuffer)
{
    if (!xStreamBuffer) return 0;
    return xStreamBuffer->count;
}

static inline size_t xStreamBufferSpacesAvailable(StreamBufferHandle_t xStreamBuffer)
{
    if (!xStreamBuffer) return 0;
    return xStreamBuffer->capacity - xStreamBuffer->count;
}

static inline void mock_stream_buffers_reset(void)
{
    memset(s_mock_stream_bufs, 0, sizeof(s_mock_stream_bufs));
}
