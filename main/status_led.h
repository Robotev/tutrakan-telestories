#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LED_COLOR_OFF,
    LED_COLOR_YELLOW,
    LED_COLOR_BLUE,
    LED_COLOR_RED,
    LED_COLOR_GREEN,
} led_color_t;

void status_led_init(void);

// Sets the LED to blink at the given on/off durations, or solid if on_ms
// is 0 (off_ms is then ignored). Takes effect immediately (starts ON).
void status_led_set_pattern(led_color_t color, uint32_t on_ms, uint32_t off_ms);

// Bypasses the blink engine to set a fixed color immediately.
void status_led_set_immediate(led_color_t color, bool on);

// Advances any active blink pattern. No-op in immediate mode. Call every
// main-loop tick.
void status_led_update(void);
