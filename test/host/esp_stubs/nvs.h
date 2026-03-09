/**
 * ESP-IDF stub: nvs.h
 * Simple in-memory NVS mock — stores one blob and one u16 only.
 * Sufficient for group_mgr tests.
 *
 * Variables live in nvs_mock.c (single instance shared across all TUs).
 */
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

typedef int nvs_handle_t;

#define NVS_READONLY   0
#define NVS_READWRITE  1

/* Implemented in nvs_mock.c */
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h);
void      nvs_close(nvs_handle_t h);
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *len);
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *val, size_t len);
esp_err_t nvs_get_u16(nvs_handle_t h, const char *key, uint16_t *out);
esp_err_t nvs_set_u16(nvs_handle_t h, const char *key, uint16_t val);
esp_err_t nvs_commit(nvs_handle_t h);

/* Reset all NVS mock state (call from test setUp / mock_state_reset) */
void nvs_mock_reset(void);
