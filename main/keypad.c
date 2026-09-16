#include "keypad.h"

#include <string.h>

#include "debounce.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#define NUM_ROWS 4
#define NUM_COLS 3

// Rows are inputs with software pull-ups (no hardware pull-ups on the
// keypad); columns are outputs, driven LOW one at a time to scan each
// row - same approach validated in telestories-test/keypad_test.
static const gpio_num_t ROW_GPIO[NUM_ROWS] = {GPIO_NUM_43, GPIO_NUM_44, GPIO_NUM_1, GPIO_NUM_2};
static const gpio_num_t COL_GPIO[NUM_COLS] = {GPIO_NUM_42, GPIO_NUM_41, GPIO_NUM_40};

static const char KEYS[NUM_ROWS][NUM_COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'},
};

#define KEY_DEBOUNCE_US 20000 // 20ms, per requirements.md
#define DIAL_WINDOW_US 1000000 // 1000ms, per requirements.md
#define COL_SETTLE_US 5 // let a freshly-driven column settle before sampling rows

static debounced_input_t s_cell[NUM_ROWS][NUM_COLS];
static bool s_suspended;

static char s_buffer[KEYPAD_MAX_CODE_LEN + 1];
static uint8_t s_buffer_len;
static bool s_pending; // true from the first key of a dial until its window elapses
static int64_t s_last_key_us;

static void configure_row(gpio_num_t pin)
{
    gpio_config_t conf = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&conf);
}

static void configure_col(gpio_num_t pin)
{
    gpio_config_t conf = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&conf);
    gpio_set_level(pin, 1);
}

static void reset_dial_buffer(void)
{
    s_buffer_len = 0;
    s_buffer[0] = 0;
    s_pending = false;
}

void keypad_init(void)
{
    for (int r = 0; r < NUM_ROWS; r++) {
        configure_row(ROW_GPIO[r]);
    }
    for (int c = 0; c < NUM_COLS; c++) {
        configure_col(COL_GPIO[c]);
    }
    for (int r = 0; r < NUM_ROWS; r++) {
        for (int c = 0; c < NUM_COLS; c++) {
            debounced_input_init(&s_cell[r][c], 1); // 1 = HIGH = not pressed
        }
    }
    reset_dial_buffer();
}

void keypad_suspend(void)
{
    s_suspended = true;
    reset_dial_buffer();
}

void keypad_resume(void)
{
    for (int c = 0; c < NUM_COLS; c++) {
        gpio_set_level(COL_GPIO[c], 0);
        esp_rom_delay_us(COL_SETTLE_US);
        for (int r = 0; r < NUM_ROWS; r++) {
            debounced_input_init(&s_cell[r][c], gpio_get_level(ROW_GPIO[r]));
        }
        gpio_set_level(COL_GPIO[c], 1);
    }
    reset_dial_buffer();
    s_suspended = false;
}

keypad_event_t keypad_poll(void)
{
    keypad_event_t ev = {0};
    if (s_suspended) {
        return ev;
    }

    int64_t now = esp_timer_get_time();

    for (int c = 0; c < NUM_COLS; c++) {
        gpio_set_level(COL_GPIO[c], 0);
        esp_rom_delay_us(COL_SETTLE_US);

        for (int r = 0; r < NUM_ROWS; r++) {
            int level;
            if (debounced_input_update(&s_cell[r][c], gpio_get_level(ROW_GPIO[r]), now, KEY_DEBOUNCE_US, &level) &&
                level == 0) {
                if (s_buffer_len < KEYPAD_MAX_CODE_LEN) {
                    s_buffer[s_buffer_len++] = KEYS[r][c];
                    s_buffer[s_buffer_len] = 0;
                }
                s_last_key_us = now;
                s_pending = true;
                ev.key_down = true;
            }
        }

        gpio_set_level(COL_GPIO[c], 1);
    }

    if (s_pending && (now - s_last_key_us) >= DIAL_WINDOW_US) {
        ev.window_elapsed = true;
        strcpy(ev.code, s_buffer);
        reset_dial_buffer();
    }

    return ev;
}
