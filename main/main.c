#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "iot_button.h"
#include "button_gpio.h"

#include "logic_data.h"

#define LOGICDATA_RX_GPIO CONFIG_LOGICDATA_RX_GPIO
#define PIN_UP CONFIG_PIN_UP_GPIO
#define PIN_DOWN CONFIG_PIN_DOWN_GPIO
#define BUTTON_UP CONFIG_BUTTON_UP_GPIO
#define BUTTON_DOWN CONFIG_BUTTON_DOWN_GPIO

#define PIN_LEVEL_ASSERTED 1
#define PIN_LEVEL_DEASSERTED 0

#define BUTTON_LEVEL_ACTIVE 1

#define THRESHOLD_HEIGHT_DIFF 1

static const char *TAG = "LogicData";

static logicdata_ctx_t *ld = NULL;

static volatile uint8_t height = 0;

static volatile uint8_t low_height = 0;
static volatile uint8_t high_height = 0;

static volatile bool go_to_height_active = false;
static volatile uint8_t go_to_height = 0;

static volatile bool btn_up_pressed = false;
static volatile bool btn_down_pressed = false;

static void handle_height_preset_change(uint8_t height)
{
    if (low_height == 0) {
        low_height = height;
    } else if (high_height == 0) {
        high_height = height;
    } else {
        // calculate distance to low and high height
        uint8_t dist_to_low = (height > low_height) ? (height - low_height) : (low_height - height);
        uint8_t dist_to_high = (height > high_height) ? (height - high_height) : (high_height - height);
        if (dist_to_low <= dist_to_high) {
            low_height = height;
        } else {
            high_height = height;
        }
    }
}

static void IRAM_ATTR read_height_callback(void *arg)
{
    uint8_t h = logicdata_try_read_height_cm(ld);
    if (h != 0) {
        height = h;
        ESP_LOGI(TAG, "height: %u cm", (unsigned)h);
    }
}

// Button up callbacks
static void btn_up_press_down_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON1: PRESS_DOWN");
    btn_up_pressed = true;
    if (btn_up_pressed && btn_down_pressed) {
        handle_height_preset_change(height);
    }
}

static void btn_up_press_end_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON1: PRESS_END");
    btn_up_pressed = false;
}

static void btn_up_double_click_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON1: DOUBLE_CLICK");
    go_to_height_active = true;
    go_to_height = high_height;
}

static void btn_up_long_press_start_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON1: LONG_PRESS_START");
    gpio_set_level(PIN_UP, PIN_LEVEL_ASSERTED);
}

static void btn_up_long_press_up_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON1: LONG_PRESS_UP");
    gpio_set_level(PIN_UP, PIN_LEVEL_DEASSERTED);
}

// Button down callbacks
static void btn_down_press_down_cb(void *arg, void *usr_data)
{
    btn_down_pressed = true;
    ESP_LOGI(TAG, "BUTTON2: PRESS_DOWN");
    if (btn_up_pressed && btn_down_pressed) {
        handle_height_preset_change(height);
    }
}

static void btn_down_press_end_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON2: PRESS_END");
    btn_down_pressed = false;
}

static void btn_down_double_click_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON2: DOUBLE_CLICK");
    go_to_height_active = true;
    go_to_height = low_height;
}

static void btn_down_long_press_start_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON2: LONG_PRESS_START");
    gpio_set_level(PIN_DOWN, PIN_LEVEL_ASSERTED);
}

static void btn_down_long_press_up_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON2: LONG_PRESS_UP");
    gpio_set_level(PIN_DOWN, PIN_LEVEL_DEASSERTED);
}

void app_main(void)
{
    // button_config_t btn_cfg = {
    //     .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME,
    //     .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME,
    // };

    button_config_t btn_cfg = {0};

    const button_gpio_config_t btn_up_gpio_cfg = {
        .gpio_num = BUTTON_UP,
        .active_level = BUTTON_LEVEL_ACTIVE,
    };
    button_handle_t btn_up;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_up_gpio_cfg, &btn_up);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button up create failed: %d", (int)ret);
        return;
    }

    // Register callbacks for Button up
    ESP_ERROR_CHECK(iot_button_register_cb(btn_up, BUTTON_PRESS_DOWN, NULL, btn_up_press_down_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(btn_up, BUTTON_PRESS_END, NULL, btn_up_press_end_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(btn_up, BUTTON_DOUBLE_CLICK, NULL, btn_up_double_click_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(btn_up, BUTTON_LONG_PRESS_START, NULL, btn_up_long_press_start_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(btn_up, BUTTON_LONG_PRESS_UP, NULL, btn_up_long_press_up_cb, NULL));

    const button_gpio_config_t btn_down_gpio_cfg = {
        .gpio_num = BUTTON_DOWN,
        .active_level = BUTTON_LEVEL_ACTIVE,
    };

    button_handle_t btn_down;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_down_gpio_cfg, &btn_down);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button down create failed: %d", (int)ret);
        return;
    }

    // Register callbacks for Button down
    ESP_ERROR_CHECK(iot_button_register_cb(btn_down, BUTTON_PRESS_DOWN, NULL, btn_down_press_down_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(btn_down, BUTTON_PRESS_END, NULL, btn_down_press_end_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(btn_down, BUTTON_DOUBLE_CLICK, NULL, btn_down_double_click_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(btn_down, BUTTON_LONG_PRESS_START, NULL, btn_down_long_press_start_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(btn_down, BUTTON_LONG_PRESS_UP, NULL, btn_down_long_press_up_cb, NULL));

    ESP_LOGI(TAG, "Starting LogicData height reader...");

    esp_err_t err = logicdata_init(&ld, LOGICDATA_RX_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "logicdata_init failed: %d", (int)err);
        return;
    }

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << PIN_UP | 1ULL << PIN_DOWN;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(PIN_UP, PIN_LEVEL_DEASSERTED);
    gpio_set_level(PIN_DOWN, PIN_LEVEL_DEASSERTED);

    esp_timer_create_args_t timer_args = {
        .callback = read_height_callback,
        .name = "read_height_timer"
    };
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, CONFIG_HEIGHT_READ_INTERVAL * 1000); // Convert ms to microseconds

    // Table does not send height when it is not moving
    // so we need to trigger it by asserting and deasserting the pins
    // for CONFIG_INITIAL_MOVEMENT_DELAY ms
    gpio_set_level(PIN_UP, PIN_LEVEL_ASSERTED);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_INITIAL_MOVEMENT_DELAY));
    gpio_set_level(PIN_UP, PIN_LEVEL_DEASSERTED);

    while (1) {
        ESP_LOGI(TAG, "Main loop: go_to_height_active=%s, height=%u, go_to_height=%u, low_height=%u, high_height=%u",
                 go_to_height_active ? "true" : "false", height, go_to_height, low_height, high_height);

        if (go_to_height_active) {
            ESP_LOGI(TAG, "Go to height is ACTIVE - target: %u cm", go_to_height);

            if (height == 0 || low_height == 0 || high_height == 0) {
                ESP_LOGW(TAG, "Missing height data - height=%u, low_height=%u, high_height=%u - stopping movement",
                         height, low_height, high_height);
                gpio_set_level(PIN_UP, PIN_LEVEL_DEASSERTED);
                gpio_set_level(PIN_DOWN, PIN_LEVEL_DEASSERTED);
            } else if (height < go_to_height) {
                ESP_LOGI(TAG, "Moving UP: current=%u < target=%u", height, go_to_height);
                gpio_set_level(PIN_UP, PIN_LEVEL_ASSERTED);
                gpio_set_level(PIN_DOWN, PIN_LEVEL_DEASSERTED);
            } else if (height > go_to_height) {
                ESP_LOGI(TAG, "Moving DOWN: current=%u > target=%u", height, go_to_height);
                gpio_set_level(PIN_DOWN, PIN_LEVEL_ASSERTED);
                gpio_set_level(PIN_UP, PIN_LEVEL_DEASSERTED);
            } else {
                ESP_LOGI(TAG, "TARGET REACHED: current=%u == target=%u - stopping movement", height, go_to_height);
                gpio_set_level(PIN_UP, PIN_LEVEL_DEASSERTED);
                gpio_set_level(PIN_DOWN, PIN_LEVEL_DEASSERTED);
                go_to_height_active = false;
                ESP_LOGI(TAG, "Go to height DEACTIVATED");
            }
        } else {
            ESP_LOGD(TAG, "Go to height is INACTIVE - waiting for command");
        }

        ESP_LOGD(TAG, "Main loop sleeping for 100ms...");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_MAIN_LOOP_DELAY));
    }
}
