#ifndef DK_H
#define DK_H

#include "constants.h"

// Borrowing some useful Arduino macros
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#define SAMPLES_PER_BUFFER 256 // 256 works well

// pins (updated for PCB 2.02)
#define MUX_ADDR_A 19
#define MUX_ADDR_B 20
#define MUX_ADDR_C 21
#define MUX_READ_POTS 26
#define MUX_READ_CV 27
#define LED_PIN 25
#define DATA_595 6
#define CLOCK_595 7
#define LATCH_595 8
#define LOAD_165 12
#define CLOCK_165 13
#define DATA_165 14
#define SYNC_IN 16
#define SYNC_OUT 17
#define BUTTON_PIN_START_STOP 0
#define BUTTON_PIN_TAP_TEMPO 1
const uint8_t TRIGGER_OUT_PINS[4] = {15, 28, 22, 18};

// button numbers
#define BUTTON_CANCEL 0
#define BUTTON_CONFIRM 1
#define BUTTON_DEC 2
#define BUTTON_INC 3

#define BUTTON_SHIFT 7

#define BUTTON_TUPLET 8
#define BUTTON_MANUAL_TEMPO 9
#define BUTTON_BEAT 10

#define BUTTON_TAP_TEMPO 14
#define BUTTON_START_STOP 15

// shifted button numbers..?
#define BUTTON_TIME_SIGNATURE 25
#define BUTTON_SD_TEMP 22

// pot numbers
#define POT_CHANCE 0
#define POT_ZOOM 1
#define POT_RANGE 2
#define POT_MIDPOINT 3
#define POT_SWING 4
#define POT_SLOP 5
#define POT_DROP 6
#define POT_PITCH 7
#define POT_CRUSH 8
#define POT_CROP 9
#define POT_TBC 10
#define POT_VOLUME 11

// CV input numbers
#define CV_CHANCE 12
#define CV_ZOOM 15
#define CV_MIDPOINT 13
#define CV_TBC 14

// tuplet modes
#define TUPLET_STRAIGHT 1
#define TUPLET_TRIPLET 3
#define TUPLET_QUINTUPLET 5
#define TUPLET_SEPTUPLET 7

void initGpio();
struct audio_buffer_pool *init_audio();
bool mainTimerLogic(repeating_timer_t *rt);
bool testTimerLogic(repeating_timer_t *rt);

#endif