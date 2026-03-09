/** ESP-IDF stub: freertos/task.h */
#pragma once
#include "freertos/FreeRTOS.h"

static inline BaseType_t xTaskCreate(void (*fn)(void*), const char *name,
                                      uint32_t stack, void *arg,
                                      UBaseType_t prio, TaskHandle_t *handle)
{
    (void)fn; (void)name; (void)stack; (void)arg; (void)prio;
    if (handle) *handle = (TaskHandle_t)1;
    return pdTRUE;
}

static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }
static inline void vTaskDelete(TaskHandle_t h) { (void)h; }
