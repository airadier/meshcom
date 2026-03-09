/** ESP-IDF stub: esp_timer.h
 *  Supports mock time for testing VAD hold time */
#pragma once
#include <stdint.h>

/* Mock time control */
extern int64_t mock_time_us;

static inline int64_t esp_timer_get_time(void)
{
    return mock_time_us;
}

static inline void mock_time_set(int64_t us)
{
    mock_time_us = us;
}

static inline void mock_time_advance(int64_t us)
{
    mock_time_us += us;
}
