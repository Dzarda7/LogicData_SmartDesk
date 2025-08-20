#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 1 bit per millisecond
#define LOGICDATA_SAMPLE_RATE_US 1000
// Consider idle if no edges for this long
#define LOGICDATA_IDLE_TIME_US   ((uint32_t)1u << 16)  // ~65ms
// Capture ring size
#define LOGICDATA_TRACE_HISTORY_MAX 80
// Sentinel for long idle
#define LOGICDATA_BIG_IDLE ((uint32_t)(-1))

// Opaque context
typedef struct logicdata_ctx logicdata_ctx_t;

// Initialize and start capturing on rx_gpio
esp_err_t logicdata_init(logicdata_ctx_t **out_ctx, gpio_num_t rx_gpio);
// Stop and free
void logicdata_deinit(logicdata_ctx_t *ctx);

// Try to read a decoded 32-bit word (non-blocking)
bool logicdata_try_read_word(logicdata_ctx_t *ctx, uint32_t *out_word);

// Helpers
bool logicdata_is_valid(uint32_t msg);
bool logicdata_is_number(uint32_t msg);
uint8_t logicdata_get_number(uint32_t msg);

// Convenience: returns height in cm if a number frame is available (0 if none)
uint8_t logicdata_try_read_height_cm(logicdata_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
