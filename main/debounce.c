#include "debounce.h"

void debounced_input_init(debounced_input_t *d, int initial_level)
{
    d->stable_level = initial_level;
    d->candidate_level = initial_level;
    d->candidate_since = 0;
}

bool debounced_input_update(debounced_input_t *d, int raw_level, int64_t now, int64_t debounce_us,
                             int *out_level)
{
    if (raw_level != d->candidate_level) {
        d->candidate_level = raw_level;
        d->candidate_since = now;
        return false;
    }
    if (raw_level != d->stable_level && (now - d->candidate_since) >= debounce_us) {
        d->stable_level = raw_level;
        *out_level = raw_level;
        return true;
    }
    return false;
}
