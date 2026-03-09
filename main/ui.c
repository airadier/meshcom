/**
 * MeshCom - User Interface
 * Buttons (with hold detection), LED patterns, display (M5StickC Plus 2)
 */
#include "ui.h"
#include "board.h"
#include "pairing.h"
#include "bt_hfp.h"
#include "group_mgr.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "ui";

/* ---- LED ---- */

static ui_state_t s_state = UI_STATE_IDLE;
static TimerHandle_t s_led_timer = NULL;
static bool s_led_on = false;
static int s_flash_count = 0;
#if HAS_DISPLAY
static bool s_display_dirty = true;
#endif

static void led_set(bool on)
{
    s_led_on = on;
#if LED_ACTIVE_LOW
    gpio_set_level(LED_GPIO, on ? 0 : 1);
#else
    gpio_set_level(LED_GPIO, on ? 1 : 0);
#endif
}

static void led_timer_cb(TimerHandle_t timer)
{
    if (s_flash_count > 0) {
        led_set(!s_led_on);
        s_flash_count--;
        if (s_flash_count == 0) {
            /* Restore state-based pattern */
            xTimerChangePeriod(s_led_timer,
                               (s_state == UI_STATE_SHARE) ? pdMS_TO_TICKS(500) :
                               (s_state == UI_STATE_JOIN)  ? pdMS_TO_TICKS(150) :
                               pdMS_TO_TICKS(1000), 0);
        }
        return;
    }

    switch (s_state) {
    case UI_STATE_IDLE:
        led_set(true); /* solid */
        break;
    case UI_STATE_SHARE:
    case UI_STATE_JOIN:
    case UI_STATE_BT_SCAN:
        led_set(!s_led_on); /* toggle */
        break;
    }
}

/* ---- Display (M5StickC Plus 2 only) ---- */

#if HAS_DISPLAY
/* Simple framebuffer approach via SPI.
 * For PoC we just log to serial; real display init requires ST7789 driver.
 * TODO: Implement ST7789V2 SPI driver for M5StickC Plus 2 display
 */
static char s_disp_bt[32] = "--";
static char s_disp_activity[8] = "--";

static void display_update(void)
{
    if (!s_display_dirty) return;
    s_display_dirty = false;

    uint16_t gid = 0;
    group_mgr_get_id(&gid);

    ESP_LOGI(TAG, "[DISP] MeshCom v%s | BT: %s | Group: %04X | %s",
             MESHCOM_VERSION, s_disp_bt, gid, s_disp_activity);
    /* TODO: Render to ST7789V2 display:
     *   Line 1: "MeshCom v0.1"
     *   Line 2: "BT: %s", s_disp_bt
     *   Line 3: "Group: %04X", gid
     *   Line 4: "%s", s_disp_activity
     */
}
#endif

/* ---- Buttons ---- */

typedef struct {
    int gpio;
    int64_t press_time;
    bool pressed;
} button_t;

static button_t s_btn_a = { .gpio = BTN_A_GPIO, .press_time = 0, .pressed = false };
#if BTN_B_GPIO >= 0
static button_t s_btn_b = { .gpio = BTN_B_GPIO, .press_time = 0, .pressed = false };
#endif

static void handle_button_a(int hold_ms)
{
#ifdef BOARD_DEVKIT
    if (hold_ms > 10000) {
        ESP_LOGI(TAG, "BTN: BT discoverable");
        bt_hfp_start_scan();
    } else if (hold_ms > 5000) {
        ESP_LOGI(TAG, "BTN: New group");
        group_mgr_new_group();
        ui_flash_led(3);
    } else if (hold_ms > 2000) {
        ESP_LOGI(TAG, "BTN: JOIN");
        pairing_start_join();
    } else {
        ESP_LOGI(TAG, "BTN: SHARE");
        pairing_start_share();
    }
#else
    /* M5StickC Plus 2: Button A */
    if (hold_ms > 10000) {
        ESP_LOGI(TAG, "BTN A: New group");
        group_mgr_new_group();
        ui_flash_led(3);
    } else if (hold_ms > 2000) {
        ESP_LOGI(TAG, "BTN A: JOIN");
        pairing_start_join();
    } else {
        ESP_LOGI(TAG, "BTN A: SHARE");
        pairing_start_share();
    }
#endif
}

#if BTN_B_GPIO >= 0
static void handle_button_b(int hold_ms)
{
    if (hold_ms > 3000) {
        ESP_LOGI(TAG, "BTN B: BT discoverable");
        bt_hfp_start_scan();
    } else {
        ESP_LOGI(TAG, "BTN B: BT reconnect");
        bt_hfp_reconnect();
    }
}
#endif

static void poll_button(button_t *btn)
{
    bool level = (gpio_get_level(btn->gpio) == 0); /* active low */

    if (level && !btn->pressed) {
        btn->pressed = true;
        btn->press_time = esp_timer_get_time();
    } else if (!level && btn->pressed) {
        btn->pressed = false;
        int hold_ms = (int)((esp_timer_get_time() - btn->press_time) / 1000);

        if (btn->gpio == BTN_A_GPIO) {
            handle_button_a(hold_ms);
        }
#if BTN_B_GPIO >= 0
        else if (btn->gpio == BTN_B_GPIO) {
            handle_button_b(hold_ms);
        }
#endif
    }
}

/* ---- UI Task ---- */

static void ui_task(void *arg)
{
    while (1) {
        poll_button(&s_btn_a);
#if BTN_B_GPIO >= 0
        poll_button(&s_btn_b);
#endif

#if HAS_DISPLAY
        display_update();
#endif

        vTaskDelay(pdMS_TO_TICKS(50)); /* 20Hz polling */
    }
}

/* ---- Public API ---- */

esp_err_t ui_init(void)
{
    /* LED */
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_cfg);
    led_set(true);

    /* Buttons */
    uint64_t btn_mask = (1ULL << BTN_A_GPIO);
#if BTN_B_GPIO >= 0
    btn_mask |= (1ULL << BTN_B_GPIO);
#endif
    gpio_config_t btn_cfg = {
        .pin_bit_mask = btn_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn_cfg);

    /* LED blink timer */
    s_led_timer = xTimerCreate("led", pdMS_TO_TICKS(1000), pdTRUE, NULL, led_timer_cb);
    xTimerStart(s_led_timer, 0);

    /* UI task for button polling + display */
    xTaskCreate(ui_task, "ui", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "UI initialized (%s)", BOARD_NAME);
    return ESP_OK;
}

void ui_set_state(ui_state_t state)
{
    s_state = state;
#if HAS_DISPLAY
    s_display_dirty = true;
#endif
    int period_ms;
    switch (state) {
    case UI_STATE_SHARE:    period_ms = 500; break;
    case UI_STATE_JOIN:     period_ms = 150; break;
    case UI_STATE_BT_SCAN: period_ms = 300; break;
    default:                period_ms = 1000; break;
    }
    xTimerChangePeriod(s_led_timer, pdMS_TO_TICKS(period_ms), 0);
}

void ui_set_bt_status(const char *status)
{
#if HAS_DISPLAY
    strncpy(s_disp_bt, status, sizeof(s_disp_bt) - 1);
    s_display_dirty = true;
#endif
    ESP_LOGI(TAG, "BT: %s", status);
}

void ui_set_activity(const char *activity)
{
#if HAS_DISPLAY
    strncpy(s_disp_activity, activity, sizeof(s_disp_activity) - 1);
    s_display_dirty = true;
#endif
}

void ui_flash_led(int count)
{
    s_flash_count = count * 2; /* on+off per flash */
    xTimerChangePeriod(s_led_timer, pdMS_TO_TICKS(100), 0);
}
