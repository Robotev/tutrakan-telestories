#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "keypad.h"
#include "mp3_player.h"
#include "status_led.h"

static const char *TAG = "main";

typedef struct {
    const char *code;
    int module_index; // 0-4
} code_entry_t;

// Per requirements.md's "Code To MP3 Relation" table.
static const code_entry_t CODE_TABLE[] = {
    {"120", 0},
    {"175", 1},
    {"177", 2},
    {"0900", 3},
    {"1990", 4},
};
#define NUM_CODES (sizeof(CODE_TABLE) / sizeof(CODE_TABLE[0]))

static int lookup_module(const char *code)
{
    for (size_t i = 0; i < NUM_CODES; i++) {
        if (strcmp(code, CODE_TABLE[i].code) == 0) {
            return CODE_TABLE[i].module_index;
        }
    }
    return -1;
}

#define WAIT_LED_ON_MS 250
#define WAIT_LED_OFF_MS 250
#define DIAL_LED_ON_MS 250
#define DIAL_LED_OFF_MS 250
#define RESULT_FAIL_MS 1500

#define VOLUME_100_PERCENT 30 // level 30 of the module's 0-30 range
#define VOLUME_80_PERCENT 24  // round(0.8 * 30)

#define TRACK0_PATH "/00000*MP3"
#define TRACK0_SEQ_INDEX 1
#define TRACK1_PATH "/00001*MP3"
#define TRACK1_SEQ_INDEX 2

#define LOOP_MS 10

typedef enum {
    STATE_WAITING,
    STATE_DIALING,
    STATE_RESULT_FAIL,
    STATE_PLAYING,
} state_t;

typedef enum {
    STAGE_TRACK0,
    STAGE_TRACK1,
} play_stage_t;

static void enter_waiting(state_t *state)
{
    *state = STATE_WAITING;
    keypad_resume();
    status_led_set_pattern(LED_COLOR_YELLOW, WAIT_LED_ON_MS, WAIT_LED_OFF_MS);
}

static void enter_dialing(state_t *state)
{
    *state = STATE_DIALING;
    status_led_set_pattern(LED_COLOR_BLUE, DIAL_LED_ON_MS, DIAL_LED_OFF_MS);
}

static void enter_fail(state_t *state, int64_t *fail_deadline_us)
{
    *state = STATE_RESULT_FAIL;
    status_led_set_immediate(LED_COLOR_RED, true);
    *fail_deadline_us = esp_timer_get_time() + RESULT_FAIL_MS * 1000;
}

static void start_story(int module_index, state_t *state, play_stage_t *stage)
{
    ESP_LOGI(TAG, "Valid code dialed - playing story on MP3 #%d", module_index + 1);
    keypad_suspend();
    status_led_set_immediate(LED_COLOR_GREEN, true);

    mp3_player_select(module_index);
    mp3_player_set_volume(VOLUME_100_PERCENT);
    mp3_player_play(TRACK0_PATH, TRACK0_SEQ_INDEX);

    *stage = STAGE_TRACK0;
    *state = STATE_PLAYING;
}

void app_main(void)
{
    keypad_init();
    mp3_player_init();
    status_led_init();

    state_t state;
    enter_waiting(&state);

    play_stage_t stage = STAGE_TRACK0;
    int64_t fail_deadline_us = 0;

    while (1) {
        int64_t now = esp_timer_get_time();
        keypad_event_t kev = keypad_poll();
        mp3_player_update();
        status_led_update();

        switch (state) {
        case STATE_WAITING:
            if (kev.key_down) {
                enter_dialing(&state);
            }
            break;

        case STATE_DIALING:
            if (kev.window_elapsed) {
                int module_index = lookup_module(kev.code);
                if (module_index >= 0) {
                    start_story(module_index, &state, &stage);
                } else {
                    ESP_LOGI(TAG, "Dialed \"%s\" - no matching code", kev.code);
                    enter_fail(&state, &fail_deadline_us);
                }
            }
            break;

        case STATE_RESULT_FAIL:
            // A fresh key press immediately starts a new dial, cutting the
            // red flash short - requirements.md only reserves the ignore
            // window for a *successful* dial through end of playback, so
            // the user isn't kept waiting out the 1500ms after a mistake.
            if (kev.key_down) {
                enter_dialing(&state);
            } else if (now >= fail_deadline_us) {
                enter_waiting(&state);
            }
            break;

        case STATE_PLAYING:
            if (mp3_player_last_command_failed()) {
                // Not addressed by requirements.md (which assumes the MP3
                // communication always eventually succeeds via its
                // retry/confirm logic); on exhausted retries, abandon this
                // story and return to waiting rather than hang forever.
                ESP_LOGE(TAG, "MP3 module communication failed after all retries - aborting playback");
                enter_waiting(&state);
            } else if (stage == STAGE_TRACK0 && mp3_player_busy_just_went_idle()) {
                mp3_player_set_volume(VOLUME_80_PERCENT);
                mp3_player_play(TRACK1_PATH, TRACK1_SEQ_INDEX);
                stage = STAGE_TRACK1;
            } else if (stage == STAGE_TRACK1 && mp3_player_busy_just_went_idle()) {
                ESP_LOGI(TAG, "Story playback finished");
                enter_waiting(&state);
            }
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_MS));
    }
}
