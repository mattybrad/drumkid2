#ifndef DK_H
#define DK_H

// Borrowing some useful Arduino macros
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
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

// button numbers
#define BUTTON_INC 0
#define BUTTON_DEC 1
#define BUTTON_BEAT 17
#define BUTTON_START_STOP 16

// Beat variables
int step = 0;         // e.g. 0 to 31 for a 4/4 pattern of 8th-notes
int stepPosition = 0; // position within the step, measured in samples
float tempo = 120.0;  // BPM
int samplesPerStep;   // slower tempos give higher values
float sampleRate = 44100.0;
bool beatPlaying = false;
int beatNum = 0;
Beat beats[8];
Sample samples[3];

// temporary (ish?) LED variables (first 8 bits are the segments, next 4 are the character selects, final 4 are 3mm LEDs)
uint8_t sevenSegData[4] = {0b00000000, 0b00000000, 0b00000000, 0b00000000};
uint8_t singleLedData = 0b0010; // 4 x 3mm LEDs
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
repeating_timer_t syncInTimer; // temp, wait and see what other LEDs are used for, maybe make a class
repeating_timer_t syncOutTimer; // temp again probably

uint analogLoopNum = 0; // 0 to 7
uint analogPhase = 0;   // 0 or 1
uint analogReadings[16] = {0};

uint shiftRegOutLoopNum = 0; // 0 to 15
uint shiftRegOutPhase = 0;   // 0, 1, or 2
uint sevenSegCharNum = 0;

bool buttonStableStates[18] = {false}; // array of bools not good use of space, change
uint16_t microsSinceChange[18] = {0};                          // milliseconds since state change
uint shiftRegInLoopNum = 0; // 0 to 15
uint shiftRegInPhase = 0;   // 0 or 1

void initGpio();
void initBeats();
void initSamples();
bool mainTimerLogic(repeating_timer_t *rt);
struct audio_buffer_pool *init_audio();
char getNthDigit(int x, int n);
void updateLedDisplay(int num);
void handleButtonChange(int buttonNum, bool buttonState);
void updateShiftRegButtons();
void updateStandardButtons();
void updateAnalog();
void loadSamples();
void updateLeds();
void pulseGpio(uint8_t gpioNum, uint16_t pulseLengthMicros);

#endif