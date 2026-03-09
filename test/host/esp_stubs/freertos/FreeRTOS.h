/** ESP-IDF stub: freertos/FreeRTOS.h */
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
typedef void* TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t);
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define pdFALSE  0
#define pdTRUE   1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY 0xFFFFFFFF
