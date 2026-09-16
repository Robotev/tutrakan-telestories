#include "status_led.h"

#include "driver/rmt_tx.h"
#include "esp_timer.h"
#include "led_strip_encoder.h"

// GPIO38, the ESP32-S3-DevKitC-1's onboard addressable RGB LED (WS2812),
// per requirements.md's pin table. Not user-wired - fixed by the board.
#define LED_STRIP_GPIO 38
#define LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz, 1 tick = 0.1us

// Applied to every color below before it goes out over the wire, so the
// COLOR_TABLE entries can stay full-scale/readable. Per requirements.md.
#define LED_BRIGHTNESS_PERCENT 15

typedef struct {
    uint8_t r, g, b;
} rgb_t;

static const rgb_t COLOR_OFF = {0, 0, 0};
static const rgb_t COLOR_TABLE[] = {
    [LED_COLOR_OFF] = {0, 0, 0},
    [LED_COLOR_YELLOW] = {255, 255, 0},
    [LED_COLOR_BLUE] = {0, 0, 255},
    [LED_COLOR_RED] = {255, 0, 0},
    [LED_COLOR_GREEN] = {0, 255, 0},
};

// Driven directly with the RMT driver (rather than the espressif/led_strip
// managed component) - see tutrakan-storyteller/main/status_led.c, whose
// approach this follows; that project's led_strip-managed-component
// backend never lit the LED despite reporting ESP_OK, while this raw
// driver (copied from IDF's own peripherals/rmt/led_strip example) was
// verified working there.
static rmt_channel_handle_t s_led_chan;
static rmt_encoder_handle_t s_led_encoder;

static bool s_blinking;
static rgb_t s_pattern_color;
static uint32_t s_on_ms, s_off_ms;
static bool s_phase_on;
static int64_t s_phase_start_us;

static bool s_last_applied_valid;
static rgb_t s_last_applied;

static uint8_t scale_brightness(uint8_t v)
{
    return (uint8_t)((uint32_t)v * LED_BRIGHTNESS_PERCENT / 100);
}

static void apply(rgb_t color)
{
    if (s_last_applied_valid && color.r == s_last_applied.r && color.g == s_last_applied.g &&
        color.b == s_last_applied.b) {
        return;
    }
    uint8_t grb[3] = {scale_brightness(color.g), scale_brightness(color.r),
                       scale_brightness(color.b)}; // WS2812 wire order
    rmt_transmit_config_t tx_conf = {.loop_count = 0};
    rmt_transmit(s_led_chan, s_led_encoder, grb, sizeof(grb), &tx_conf);
    rmt_tx_wait_all_done(s_led_chan, -1);
    s_last_applied = color;
    s_last_applied_valid = true;
}

void status_led_init(void)
{
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = LED_STRIP_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    rmt_new_tx_channel(&tx_chan_config, &s_led_chan);

    led_strip_encoder_config_t encoder_config = {
        .resolution = LED_STRIP_RESOLUTION_HZ,
    };
    rmt_new_led_strip_encoder(&encoder_config, &s_led_encoder);

    rmt_enable(s_led_chan);
    apply(COLOR_OFF);
}

void status_led_set_pattern(led_color_t color, uint32_t on_ms, uint32_t off_ms)
{
    s_pattern_color = COLOR_TABLE[color];
    s_on_ms = on_ms;
    s_off_ms = off_ms;
    s_blinking = (on_ms != 0);
    s_phase_on = true;
    s_phase_start_us = esp_timer_get_time();
    apply(s_pattern_color);
}

void status_led_set_immediate(led_color_t color, bool on)
{
    s_blinking = false;
    apply(on ? COLOR_TABLE[color] : COLOR_OFF);
}

void status_led_update(void)
{
    if (!s_blinking) {
        return;
    }
    int64_t now = esp_timer_get_time();
    uint32_t threshold_ms = s_phase_on ? s_on_ms : s_off_ms;
    if (now - s_phase_start_us >= (int64_t)threshold_ms * 1000) {
        s_phase_on = !s_phase_on;
        s_phase_start_us = now;
        apply(s_phase_on ? s_pattern_color : COLOR_OFF);
    }
}
