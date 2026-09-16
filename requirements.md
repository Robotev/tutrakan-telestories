### Introduction and Goal
This project is for an interactive installation. The installation consists of a stationary telephone device with a 3x4 matrix keypad and five additional telephone handsets. When a user dials a valid combination on the telephone keypad, a story is played on one of the five handsets.

### Hardware 
The MCU for this installation is an ESP32-S3-DevKitC-1 and five mp3 modules, all of them DY-SV8F. In order to be able to issue the necessary commands, the ESP32-S3 will control the mp3 modules over UART and will monitor their BUSY pins.  

#### Telephone Keypad
the buttons are arranged as follows:

| row No. | col 1 | col 2 | col 3 |
| ---- | ---- | ---- | ---- |
| row 1 | 1 | 2 | 3 |
| row 2 | 4 | 5 | 6 |
| row 3 | 7 | 8 | 9 |
| row 4 | * | 0 | # |

#### Arduino Pin Mapping
The project uses an ESP32-S3 DevKitC-1 v1.1 (8MB flash, no PSRAM) to read the keypad, drive five DY-SV8F MP3 player modules over UART (TX/RX per module), read each module's BUSY pin, and use the onboard RGB LED for status. Pins are chosen to avoid all four ESP32-S3 strapping pins (GPIO0, 3, 45, 46), and each MP3 module's TX/RX/BUSY trio is placed on physically adjacent header pins.

TX/RX in the table below are from the ESP32's perspective - each module's TX pin wires to the ESP32's RX GPIO, and each module's RX pin wires to the ESP32's TX GPIO (standard cross-wired UART).

| Group | Signal | GPIO | Header |
| ---- | ---- | ---- | ---- |
| MP3 #1 | TX | 37 | J3 |
| MP3 #1 | RX | 36 | J3 |
| MP3 #1 | BUSY | 35 | J3 |
| MP3 #2 | TX | 48 | J3 |
| MP3 #2 | RX | 47 | J3 |
| MP3 #2 | BUSY | 21 | J3 |
| MP3 #3 | TX | 4 | J1 |
| MP3 #3 | RX | 5 | J1 |
| MP3 #3 | BUSY | 6 | J1 |
| MP3 #4 | TX | 7 | J1 |
| MP3 #4 | RX | 15 | J1 |
| MP3 #4 | BUSY | 16 | J1 |
| MP3 #5 | TX | 17 | J1 |
| MP3 #5 | RX | 18 | J1 |
| MP3 #5 | BUSY | 8 | J1 |
| Keypad | Row 1 | 43 | J3 |
| Keypad | Row 2 | 44 | J3 |
| Keypad | Row 3 | 1 | J3 |
| Keypad | Row 4 | 2 | J3 |
| Keypad | Col 1 | 42 | J3 |
| Keypad | Col 2 | 41 | J3 |
| Keypad | Col 3 | 40 | J3 |
| Status LED | onboard WS2812 (fixed) | 38 | J3 |

22 pins used, 0 strapping pins used. Spare pins: GPIO9, 10, 11, 12, 13, 14 (J1, contiguous) and GPIO39 (J3).

All the keypad input pins are to be pulled-up in software, and a debounce of 20ms is to be used for all inputs, including the mp3 BUSY line.

### Software
At boot, the installation is in a button-press monitoring state. Once a valid combination has been dialed, the ESP32 determines which of the five MP3 players must play adudio, based on the "code to mp3 relation" table below. If the UART is assigned to pins do not control the selected MP3 module, the UART is to be deassigned from the current pins and assigned to the pins that control the correct MP3 player for the dialed combination. Once the UART pins are assigned to the correct MP3 module, the ESP32 issues a command to the module to boost volume to 100% (level 30 of the module's 0-30 range) and then issues a second command to play file 00000.mp3, then it monitors the BUSY line for that MP3 player to determine when the track will finish. Once finished, the ESP32 issues a command for the volume to be reduced to 80% (level 24) and another command for file 00001.mp3 to be played, then starts monitoring the BUSY line to determine when the file will finish. All five of the MP3 modules will have files named 00000.mp3 and 00001.mp3.

From the time of the correct dialing of a valid combination until the playback of file 00001.mp3 has finished, the ESP32 ignores all button presses on the keypad - once a valid combination has been dialed, the respective file will be played to the end, without the option to be interrupted or a file on another mp3 player to be played.

To determine the valid combination, the ESP will evaluate all keypresses that happened within 1000ms of each other - i.e. 1000ms after the last key has been pressed, it will evaluate whether the pressed keys buffer holds a valid combination. This includes a 1000ms window after the last digit of a valid combination in which no new keypresses are to be registered. This is to ensure that user is not pressing keys randomly in order to guess a combination.

The `*` and `#` keys are not used in any valid combination and are treated as ordinary buffer contents; since no valid code contains them, any buffer including a `*` or `#` press simply fails to match and results in an unsuccessful attempt. They have no special clear/cancel function.

Upon the end of the evaluation, if the combination is not correct, no action is taken other than with the status LED, and the keypress buffer is cleared so the user can immediately begin dialing again. If the combination is correct, the sequence described above is implemented.

Code To MP3 Relation table
| Code | MP3 Module |
| ---- | ---------- |
|  120 | MP3 #1 |
|  175 | MP3 #2 |
|  177 | MP3 #3 |
| 0900 | MP3 #4 |
| 1990 | MP3 #5 |

#### LED Behaviour
When the installation is in waiting mode, the onboard LED blinks yellow at 250ms ON / 250ms OFF periods. During dialing/keypressing, the LED blinks blue at 250ms ON and OFF interval. When dialing ends, the LED is red for 1500ms if the attempt was unsuccessful or green for 1500ms if the attempt was successful. On a successful attempt, the LED remains solid green for the entire duration of playback of 00000.mp3 and 00001.mp3, then returns to the yellow waiting-mode blink once playback has finished. The brightness of the LED in all modes is 15% of maximum.

#### MP3 Communication Protocol
The communication with the MP3 modules is to follow the rules set out for a companion project that is available at ~/Projects/tutrakan-storyteller. This includes retries and confirmation logic.

#### Test Project 
The folder ~/Projects/telestories-test contains a test project which was used to successfully test the keypad with an Arduino Nano R4.

### Build System
This project targets the ESP-IDF framework, consistent with the companion tutrakan-storyteller project whose MP3 retry/confirmation logic is being reused.

### Audio File Preparation
The raw recordings for the installation live in to-convert/, named to match the Code To MP3 Relation table above (120.wav, 175.wav, 177.mp3, 0900.wav, 1990-1/2/3.wav, plus ring.mp3, the shared ring tone). tools/build_audio.py (run as `python3 tools/build_audio.py`, requires ffmpeg/ffprobe) converts these into the exact 00000.mp3/00001.mp3 pairs each module's SD card needs, under audio_output/MP3-<module>_code<code>/:

- ring.mp3 becomes 00000.mp3 on all five modules: peak-normalized as loud as possible without clipping (-1.0 dBFS sample peak), independent of the stories' common loudness level.
- Each code's story becomes 00001.mp3: two-pass loudness-normalized to a common -16 LUFS integrated / -1.5 dBTP level, so all five stories play back equally loud regardless of how they were originally recorded.
- The three 1990-*.wav files are joined in name order first (500ms fade-out, 500ms silence, 500ms fade-in between each pair), then loudness-normalized as a single unit like the other stories.
- All output files are mono, 22050 Hz, 64 kbps CBR MP3, to ease load on the DY-SV8F modules.

Re-run the script any time a source recording in to-convert/ is replaced; it regenerates audio_output/ from scratch.
