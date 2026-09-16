#pragma once
#include <stdbool.h>

// Longest valid code is 4 digits; a little headroom beyond that for a
// mis-dial that overruns a valid code's length still needs to be captured
// so it correctly fails to match (see requirements.md's evaluation rule -
// it evaluates whatever's in the buffer, however long).
#define KEYPAD_MAX_CODE_LEN 8

typedef struct {
    // True once, edge-triggered, on any newly (debounced) pressed key -
    // including the first key of a fresh dial and every key after it.
    bool key_down;
    // True once, 1000ms after the last key_down, per requirements.md's
    // evaluation window. `code` holds everything pressed since the last
    // window_elapsed (or since boot), valid only when this is true. The
    // internal buffer is cleared immediately after, so a new dial can
    // start right away.
    bool window_elapsed;
    char code[KEYPAD_MAX_CODE_LEN + 1];
} keypad_event_t;

void keypad_init(void);

// No-op (returns an all-false event) while suspended.
keypad_event_t keypad_poll(void);

// Ignores all input - used from the correct dialing of a valid combination
// until its story has finished playing, per requirements.md.
void keypad_suspend(void);

// Re-baselines every key against its current physical reading and clears
// any pending dial buffer, so a key held down across suspend isn't
// mistaken for a fresh press, and resumes normal scanning.
void keypad_resume(void);
