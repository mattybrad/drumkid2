#ifndef DK_H
#define DK_H

#include "constants.h"

// Borrowing some useful Arduino macros, but updating from 1UL to 1ULL for 64-bit compatibility
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1ULL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1ULL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#define SAMPLES_PER_BUFFER 32 // 256 works well

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

// button numbers (shifted buttons are +16)
#define BUTTON_CANCEL 0
#define BUTTON_CONFIRM 1
#define BUTTON_DEC 2
#define BUTTON_INC 3
#define BUTTON_PPQN (5 + 16)
#define BUTTON_LOAD_SAMPLES (6 + 16)
#define BUTTON_SHIFT 7
#define BUTTON_TUPLET 8
#define BUTTON_MANUAL_TEMPO 9
#define BUTTON_TIME_SIGNATURE (BUTTON_MANUAL_TEMPO + 16)
#define BUTTON_BEAT 10
#define BUTTON_EDIT_BEAT (BUTTON_BEAT + 16)
#define BUTTON_SAVE (11 + 16)
#define BUTTON_OUTPUT (12 + 16)
#define BUTTON_TAP_TEMPO 14
#define BUTTON_CLOCK_MODE (14 + 16)
#define BUTTON_START_STOP 15
#define BUTTON_RANDOMISE (15 + 16)

// pot numbers
#define POT_CHANCE 0
#define POT_ZOOM 1
#define POT_RANGE 2
#define POT_MIDPOINT 3
#define POT_SWING 4
#define POT_SLOP 5
#define POT_CROP 6
#define POT_CHAIN 7
#define POT_DROP 8
#define POT_PITCH 9
#define POT_CRUSH 10
#define POT_VOLUME 11

// CV input numbers
#define CV_CHANCE 12
#define CV_ZOOM 15
#define CV_MIDPOINT 13
#define CV_TBC 14

// tuplet modes
#define TUPLET_STRAIGHT 0
#define TUPLET_TRIPLET 1
#define TUPLET_QUINTUPLET 2
#define TUPLET_SEPTUPLET 3

// analog smoothing etc
#define ANALOG_DEAD_ZONE_LOWER 100
#define ANALOG_DEAD_ZONE_UPPER (4095-ANALOG_DEAD_ZONE_LOWER)
#define ANALOG_EFFECTIVE_RANGE (ANALOG_DEAD_ZONE_UPPER - ANALOG_DEAD_ZONE_LOWER)

// flash data storage
#define FLASH_DATA_ADDRESS (1024 * 1024)
#define FLASH_USER_BEATS_ADDRESS (FLASH_DATA_ADDRESS + 8 * FLASH_SECTOR_SIZE)
#define FLASH_AUDIO_ADDRESS (FLASH_DATA_ADDRESS + 128 * FLASH_SECTOR_SIZE)
const uint8_t *flashData = (const uint8_t *)(XIP_BASE + FLASH_DATA_ADDRESS);
const uint8_t *flashUserBeats = (const uint8_t *)(XIP_BASE + FLASH_USER_BEATS_ADDRESS);
const uint8_t *flashAudio = (const uint8_t *)(XIP_BASE + FLASH_AUDIO_ADDRESS);
#define CHECK_NUM -123456789
#define DATA_CHECK 0
#define SAMPLE_START_POINTS 4
#define SAMPLE_LENGTHS (SAMPLE_START_POINTS + 8*4)

// Beat variables
#define NUM_BEATS 16
uint16_t tempo = 120; // BPM
uint64_t stepTime = 2646000 / (tempo * QUARTER_NOTE_STEPS); // microseconds 
int samplesPerStep;  // slower tempos give higher values
uint32_t SAMPLE_RATE = 44100;
bool beatPlaying = false;
int beatNum = 0;
Beat beats[NUM_BEATS]; // temp, define max number of beats
Sample samples[NUM_SAMPLES];
int editSample = 0;
int editStep = 0;

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
uint8_t sevenSegAlphaCharacters[26] = {
    0b11101110,
    0b00111110,
    0b10011100,
    0b01111010,
    0b10011110,
    0b10001110,
    0b11110110,
    0b01101110,
    0b01100000,
    0b01111000,
    0b01101110,
    0b00011100,
    0b11101100,
    0b00101010,
    0b00111010,
    0b11001110,
    0b11100110,
    0b00001010,
    0b10110110,
    0b00011110,
    0b01111100,
    0b00111000,
    0b01111100,
    0b01101110,
    0b01110110,
    0b11011010};
uint8_t sevenSegAsciiCharacters[256] = {
    0b00000000, // 0 NULL
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000, // 32 SPACE
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000010, // -
    0b00000000,
    0b00000000,
    0b11111100, // digit 0
    0b01100000,
    0b11011010,
    0b11110010,
    0b01100110,
    0b10110110,
    0b10111110,
    0b11100000,
    0b11111110,
    0b11110110, // digit 9
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000, // 64
    0b11101110, // A (a)
    0b00111110,
    0b10011100,
    0b01111010,
    0b10011110,
    0b10001110,
    0b11110110,
    0b01101110,
    0b01100000,
    0b01111000,
    0b01101110,
    0b00011100,
    0b11101100,
    0b00101010,
    0b00111010,
    0b11001110,
    0b11100110,
    0b00001010,
    0b10110110,
    0b00011110,
    0b01111100,
    0b00111000,
    0b01111100,
    0b01101110,
    0b01110110,
    0b11011010, // Z (z)
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00010000, // _
    0b00000000, // 96
    0b11101110, // a
    0b00111110,
    0b10011100,
    0b01111010,
    0b10011110,
    0b10001110,
    0b11110110,
    0b01101110,
    0b01100000,
    0b01111000,
    0b01101110,
    0b00011100,
    0b11101100,
    0b00101010,
    0b00111010,
    0b11001110,
    0b11100110,
    0b00001010,
    0b10110110,
    0b00011110,
    0b01111100,
    0b00111000,
    0b01111100,
    0b01101110,
    0b01110110,
    0b11011010, // z
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

// timers and alarms
repeating_timer_t mainTimer;
repeating_timer_t performanceCheckTimer;

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
bool performanceCheck(repeating_timer_t *rt);
bool mainTimerLogic(repeating_timer_t *rt);
struct audio_buffer_pool *init_audio();
char getNthDigit(int x, int n);
void updateLedDisplay(int num);
void updateLedDisplayAlpha(char* word);
void handleIncDec(bool isInc, bool isHold);
void handleYesNo(bool isYes);
void displayClockMode();
void displayTempo();
void displayBeat();
void displayEditBeat();
void displayTimeSignature();
void displayTuplet();
void handleButtonChange(int buttonNum, bool buttonState);
void updateShiftRegButtons();
void updateAnalog();
void getNthSampleFolder(int n);
void listSampleFolders();
void loadSamplesFromSD();
void updateLeds();
void pulseGpio(uint gpioNum, uint16_t pulseLengthMicros);
void pulseLed(uint ledNum, uint16_t pulseLengthMicros);
void initSamplesFromFlash();
void loadBeatsFromFlash();
void writeBeatsToFlash();
void writePageToFlash(const uint8_t *buffer, uint address);
void checkFlashData();
int32_t getIntFromBuffer(const uint8_t *buffer, uint position);
float getFloatFromBuffer(const uint8_t *buffer, uint position);
void updateSyncIn();
void updateTapTempo();
void doStep();
void print_buf(const uint8_t *buf, size_t len);
void initZoom();

#endif