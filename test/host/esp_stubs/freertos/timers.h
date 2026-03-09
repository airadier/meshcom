/** ESP-IDF stub: freertos/timers.h */
#pragma once
#include "freertos/FreeRTOS.h"

static inline TimerHandle_t xTimerCreate(const char *name, TickType_t period,
                                          BaseType_t reload, void *id,
                                          TimerCallbackFunction_t cb)
{
    (void)name; (void)period; (void)reload; (void)id; (void)cb;
    return (TimerHandle_t)1;
}

static inline BaseType_t xTimerStart(TimerHandle_t t, TickType_t wait)
{
    (void)t; (void)wait;
    return pdTRUE;
}

static inline BaseType_t xTimerStop(TimerHandle_t t, TickType_t wait)
{
    (void)t; (void)wait;
    return pdTRUE;
}
