#include <stdlib.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "logic_data.h"

typedef uint16_t ld_index_t;

struct logicdata_ctx {
    gpio_num_t rx_gpio;
    volatile ld_index_t head;
    volatile ld_index_t tail;
    uint32_t trace[LOGICDATA_TRACE_HISTORY_MAX];
    volatile uint32_t prev_bit_us;
    volatile bool pin_idle;
    bool started;
    portMUX_TYPE spin;
};

static inline ld_index_t ld_next(ld_index_t x)
{
    return (x + 1) % LOGICDATA_TRACE_HISTORY_MAX;
}
static inline uint32_t ld_size(struct logicdata_ctx *s)
{
    return (LOGICDATA_TRACE_HISTORY_MAX + s->head - s->tail) % LOGICDATA_TRACE_HISTORY_MAX;
}

static inline void ld_push_isr(struct logicdata_ctx *s, uint32_t t)
{
    portENTER_CRITICAL_ISR(&s->spin);
    s->trace[s->head] = t;
    ld_index_t new_head = ld_next(s->head);
    if (new_head == s->tail) {
        s->tail = ld_next(s->tail);
    }
    s->head = new_head;
    portEXIT_CRITICAL_ISR(&s->spin);
}

static bool ld_peek(struct logicdata_ctx *s, ld_index_t index, uint32_t *out)
{
    bool ok = false;
    portENTER_CRITICAL(&s->spin);
    uint32_t sz = ld_size(s);
    if (index < sz) {
        ld_index_t pos = (s->tail + index) % LOGICDATA_TRACE_HISTORY_MAX;
        *out = s->trace[pos];
        ok = true;
    }
    portEXIT_CRITICAL(&s->spin);
    return ok;
}

static void IRAM_ATTR logicdata_gpio_isr(void *arg)
{
    struct logicdata_ctx *s = (struct logicdata_ctx *)arg;
    const uint32_t now = (uint32_t)esp_timer_get_time();
    const bool level = gpio_get_level(s->rx_gpio) != 0;

    uint32_t delta_since = now - s->prev_bit_us;
    if (delta_since >= LOGICDATA_IDLE_TIME_US) {
        s->pin_idle = true;
    }

    bool sync = (s->head & 1u) != 0u;  // expect HIGH on even steps
    if (level == sync) {
        uint32_t t = s->pin_idle ? LOGICDATA_BIG_IDLE : delta_since;
        ld_push_isr(s, t);
        s->pin_idle = false;
        s->prev_bit_us = now;
    }
}

esp_err_t logicdata_init(logicdata_ctx_t **out_ctx, gpio_num_t rx_gpio)
{
    if (!out_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    struct logicdata_ctx *ctx = (struct logicdata_ctx *)calloc(1, sizeof(*ctx));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }
    ctx->rx_gpio = rx_gpio;
    ctx->head = 0;
    ctx->tail = 0;
    ctx->prev_bit_us = (uint32_t)esp_timer_get_time();
    ctx->pin_idle = true;
    ctx->started = false;
    ctx->spin = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;

    gpio_config_t io;
    io.pin_bit_mask = (1ULL << ctx->rx_gpio);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_ANYEDGE;
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        free(ctx);
        return err;
    }

    esp_err_t isr_res = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (isr_res != ESP_OK && isr_res != ESP_ERR_INVALID_STATE) {
        free(ctx);
        return isr_res;
    }
    gpio_isr_handler_add(ctx->rx_gpio, logicdata_gpio_isr, ctx);

    ctx->started = true;
    *out_ctx = (logicdata_ctx_t *)ctx;
    return ESP_OK;
}

void logicdata_deinit(logicdata_ctx_t *ctx_)
{
    struct logicdata_ctx *ctx = (struct logicdata_ctx *)ctx_;
    if (!ctx) {
        return;
    }
    if (ctx->started) {
        gpio_isr_handler_remove(ctx->rx_gpio);
    }
    free(ctx);
}

static uint32_t ld_parity(uint32_t msg)
{
    unsigned par_count = 0;
    for (uint32_t mask = 2u; mask; mask <<= 2) {
        par_count += msg & mask;
    }
    msg |= par_count & 1u;
    return msg;
}

static bool ld_check_parity(uint32_t msg)
{
    return ld_parity(msg) == msg;
}

static uint8_t reverse_nibble(uint8_t in)
{
    uint8_t ret = 0;
    ret |= (in << 3) & 8u;
    ret |= (in << 1) & 4u;
    ret |= (in >> 1) & 2u;
    ret |= (in >> 3) & 1u;
    return ret;
}

static uint8_t reverse_byte(uint8_t in)
{
    return (uint8_t)((reverse_nibble(in) << 4) | reverse_nibble((uint8_t)(in >> 4)));
}

bool logicdata_is_valid(uint32_t msg)
{
    if ((msg & 0xFFF00000u) != 0x40600000u) {
        return false;
    }
    return ld_check_parity(msg);
}

bool logicdata_is_number(uint32_t msg)
{
    return logicdata_is_valid(msg) && ((msg & 0x000FFE00u) == 0x00000400u);
}

uint8_t logicdata_get_number(uint32_t msg)
{
    return logicdata_is_number(msg) ? reverse_byte((uint8_t)(msg >> 1)) : 0;
}

bool logicdata_try_read_word(logicdata_ctx_t *ctx_, uint32_t *out_word)
{
    if (!ctx_ || !out_word) {
        return false;
    }
    struct logicdata_ctx *ctx = (struct logicdata_ctx *)ctx_;

    ld_index_t fini;
    portENTER_CRITICAL(&ctx->spin);
    fini = ctx->tail;
    portEXIT_CRITICAL(&ctx->spin);

    bool level = (fini & 1u) == 0u;
    uint32_t t = 0;
    ld_index_t i = 0;

    // Find start-bit (idle-low followed by high-pulse shorter than 2-bits)
    for (; ld_peek(ctx, i, &t); i++) {
        if (!level && t > (uint32_t)40u * LOGICDATA_SAMPLE_RATE_US) {
            uint32_t t1;
            if (ld_peek(ctx, (ld_index_t)(i + 1), &t1) && t1 < LOGICDATA_SAMPLE_RATE_US * 2u) {
                break;
            }
        }
        level = !level;
    }

    uint32_t mask = 1u << 31;
    uint32_t acc = 0;
    uint32_t t_meas = LOGICDATA_SAMPLE_RATE_US / 2u;
    for (t = 0; mask; mask >>= 1) {
        if (t_meas < LOGICDATA_SAMPLE_RATE_US) {
            if (!ld_peek(ctx, ++i, &t)) {
                break;
            }
            level = !level;
            t_meas += t;
        }
        if (!level) {
            acc |= mask;
        }
        t_meas -= LOGICDATA_SAMPLE_RATE_US;
    }

    if (mask) {
        return false;
    }

    bool ok = true;
    portENTER_CRITICAL(&ctx->spin);
    if (fini == ctx->tail) {
        if (i > 0) {
            ctx->tail = (ctx->tail + (i - 1)) % LOGICDATA_TRACE_HISTORY_MAX;
        }
    } else {
        ok = false;
    }
    portEXIT_CRITICAL(&ctx->spin);

    if (!ok) {
        return false;
    }
    *out_word = acc;
    return true;
}

uint8_t logicdata_try_read_height_cm(logicdata_ctx_t *ctx_)
{
    uint32_t word = 0;
    if (logicdata_try_read_word(ctx_, &word) && logicdata_is_number(word)) {
        return logicdata_get_number(word);
    }
    return 0;
}
