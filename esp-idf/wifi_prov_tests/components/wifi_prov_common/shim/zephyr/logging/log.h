/*
 * Zephyr logging shim for ESP-IDF — maps LOG_* to ESP_LOG*.
 */
#pragma once

#include "esp_log.h"

#define LOG_MODULE_DECLARE(name, level) static const char *TAG = #name
#define LOG_MODULE_REGISTER(name, level) static const char *TAG = #name

#define LOG_INF(...) ESP_LOGI(TAG, __VA_ARGS__)
#define LOG_ERR(...) ESP_LOGE(TAG, __VA_ARGS__)
#define LOG_WRN(...) ESP_LOGW(TAG, __VA_ARGS__)
#define LOG_DBG(...) ESP_LOGD(TAG, __VA_ARGS__)
