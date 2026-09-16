#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int stable_level;
    int candidate_level;
    int64_t candidate_since;
} debounced_input_t;

void debounced_input_init(debounced_input_t *d, int initial_level);

// Returns true (with the new level in *out_level) once a raw reading has
// held steady for debounce_us and differs from the last accepted level.
bool debounced_input_update(debounced_input_t *d, int raw_level, int64_t now, int64_t debounce_us,
                             int *out_level);
