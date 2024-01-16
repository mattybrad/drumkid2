#ifndef DK_H
#define DK_H

#include "constants.h"

// Borrowing some useful Arduino macros
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#define SAMPLES_PER_BUFFER 512 // 256 works well

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

// flash data storage
#define FLASH_DATA_ADDRESS (1024 * 1024)
#define FLASH_AUDIO_ADDRESS (FLASH_DATA_ADDRESS + FLASH_SECTOR_SIZE)
const uint8_t *flashData = (const uint8_t *)(XIP_BASE + FLASH_DATA_ADDRESS);
const uint8_t *flashAudio = (const uint8_t *)(XIP_BASE + FLASH_AUDIO_ADDRESS);
#define CHECK_NUM -123456789
#define DATA_CHECK 0
#define SAMPLE_START_POINTS 4
#define SAMPLE_LENGTHS 20
#define VAR_TEMPO 36

// Beat variables
float tempo = 120.0; // BPM
int samplesPerStep;  // slower tempos give higher values
float sampleRate = 44100.0;
bool beatPlaying = false;
int beatNum = 0;
Beat beats[8];
Sample samples[NUM_SAMPLES];

// temporary (ish?) LED variables (first 8 bits are the segments, next 4 are the character selects, final 4 are 3mm LEDs)
uint8_t sevenSegData[4] = {0b00000000, 0b00000000, 0b00000000, 0b00000000};
uint8_t singleLedData = 0b0000; // 4 x 3mm LEDs
uint16_t storedLedData[4] = {0, 0, 0, 0};
uint8_t sevenSegCharacters[10] = {
    0b11111100,
    0b01100000,
    0b11011010,
    0b11110010,
    0b01100110,
    0b10110110,
    0b10111110,
    0b11100000,
    0b11111110,
    0b11110110};

// timers and alarms
repeating_timer_t mainTimer;
repeating_timer_t schedulerTimer;
repeating_timer_t syncInTimer;  // temp, wait and see what other LEDs are used for, maybe make a class
repeating_timer_t syncOutTimer; // temp again probably

uint analogLoopNum = 0; // 0 to 7
uint analogPhase = 0;   // 0 or 1
uint analogReadings[16] = {0};

uint shiftRegOutLoopNum = 0; // 0 to 15
uint shiftRegOutPhase = 0;   // 0, 1, or 2
uint sevenSegCharNum = 0;

bool buttonStableStates[18] = {false}; // array of bools not good use of space, change
uint32_t microsSinceChange[18] = {0};  // milliseconds since state change
uint shiftRegInLoopNum = 0;            // 0 to 15
uint shiftRegInPhase = 0;              // 0 or 1

void initGpio();
void initBeats();
bool mainTimerLogic(repeating_timer_t *rt);
struct audio_buffer_pool *init_audio();
char getNthDigit(int x, int n);
void updateLedDisplay(int num);
void handleIncDec(bool isInc);
void displayTempo();
void displayBeat();
void displayTimeSignature();
void displayTuplet();
void handleButtonChange(int buttonNum, bool buttonState);
void updateShiftRegButtons();
void updateAnalog();
void loadSamplesFromSD();
void updateLeds();
void pulseGpio(uint gpioNum, uint16_t pulseLengthMicros);
void pulseLed(uint ledNum, uint16_t pulseLengthMicros);
void initSamplesFromFlash();
void writePageToFlash(const uint8_t *buffer, uint address);
void checkFlashData();
int32_t getIntFromBuffer(const uint8_t *buffer, uint position);
float getFloatFromBuffer(const uint8_t *buffer, uint position);
void updateSyncIn();
void doStep();
void print_buf(const uint8_t *buf, size_t len);

#endif