#pragma once
#include <stdbool.h>
#include <stdint.h>

#define MP3_PLAYER_MODULE_COUNT 5

// Configures all five BUSY inputs and installs the single shared UART
// peripheral (its pins are left unassigned - call mp3_player_select()
// before commanding any module).
void mp3_player_init(void);

// Reassigns the shared UART's TX/RX to the given module's pins (0-4, per
// requirements.md's pin table) and starts tracking that module's BUSY
// line. Only one module is ever addressed at a time, since only one story
// plays at once; call this before mp3_player_set_volume()/_play() for a
// newly dialed module.
void mp3_player_select(int module_index);

void mp3_player_update(void); // call every main-loop tick

// Fire-and-forget, no confirm/retry (matches tutrakan-storyteller's
// volume-set convention). level is 0-30, the module's native range.
void mp3_player_set_volume(uint8_t level);

// Starts (or restarts) a play-and-confirm sequence for the given file on
// the currently selected module. seq_index is the expected QUERY_TRACK
// reply (the file's 1-based position in flash storage order - see the
// placeholder caveat in mp3_player.c).
void mp3_player_play(const char *path, uint16_t seq_index);

// True once, the first time the BUSY line is read after transitioning
// HIGH->LOW (i.e. a file has actually started playing). Clears on read.
bool mp3_player_busy_just_went_playing(void);

// True once, the first time the BUSY line is read after transitioning
// LOW->HIGH (i.e. a file finished playing on its own). Clears on read.
bool mp3_player_busy_just_went_idle(void);

// True once, after a PLAY command has failed to be confirmed after
// MAX_ATTEMPTS retries. Clears on read.
bool mp3_player_last_command_failed(void);
