/**
 * ESP-IDF stub: nvs_mock.c
 * Single-instance in-memory NVS mock shared across all translation units.
 */
#include "nvs.h"
#include <string.h>

/* Simple storage: one blob (group_key) and one u16 (group_id) */
static uint8_t  _nvs_blob[16];
static size_t   _nvs_blob_len = 0;
static uint16_t _nvs_u16 = 0;
static int      _nvs_has_blob = 0;
static int      _nvs_has_u16 = 0;

void nvs_mock_reset(void)
{
    memset(_nvs_blob, 0, sizeof(_nvs_blob));
    _nvs_blob_len = 0;
    _nvs_u16 = 0;
    _nvs_has_blob = 0;
    _nvs_has_u16 = 0;
}

esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h)
{
    (void)ns; (void)mode;
    *h = 1;
    return ESP_OK;
}

void nvs_close(nvs_handle_t h) { (void)h; }

esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *len)
{
    (void)h; (void)key;
    if (!_nvs_has_blob) return ESP_ERR_NOT_FOUND;
    if (*len < _nvs_blob_len) return ESP_FAIL;
    memcpy(out, _nvs_blob, _nvs_blob_len);
    *len = _nvs_blob_len;
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *val, size_t len)
{
    (void)h; (void)key;
    if (len > sizeof(_nvs_blob)) return ESP_FAIL;
    memcpy(_nvs_blob, val, len);
    _nvs_blob_len = len;
    _nvs_has_blob = 1;
    return ESP_OK;
}

esp_err_t nvs_get_u16(nvs_handle_t h, const char *key, uint16_t *out)
{
    (void)h; (void)key;
    if (!_nvs_has_u16) return ESP_ERR_NOT_FOUND;
    *out = _nvs_u16;
    return ESP_OK;
}

esp_err_t nvs_set_u16(nvs_handle_t h, const char *key, uint16_t val)
{
    (void)h; (void)key;
    _nvs_u16 = val;
    _nvs_has_u16 = 1;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }
