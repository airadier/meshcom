/** ESP-IDF stub: esp_err.h */
#pragma once
#include <stdint.h>

typedef int esp_err_t;

#define ESP_OK                  0
#define ESP_FAIL                (-1)
#define ESP_ERR_NOT_FOUND       0x105
#define ESP_ERR_INVALID_STATE   0x103
#define ESP_ERR_INVALID_SIZE    0x104
#define ESP_ERR_NVS_NO_FREE_PAGES 0x1100
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1110

#define ESP_ERROR_CHECK(x) do { (void)(x); } while(0)
