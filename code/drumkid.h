#ifndef DK_H
#define DK_H

#include "constants.h"

// Borrowing some useful Arduino macros, but updating from 1UL to 1ULL for 64-bit compatibility
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1ULL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1ULL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#define SAMPLES_PER_BUFFER 256 // was 32 but anything under 256 appears to cause timing issues

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
const uint8_t TRIGGER_OUT_PINS[4] = {15, 28, 22, 18};

// default sample names
#define KICK 0
#define SNARE 1
#define HAT 2
#define CLICK 3

// button numbers
#define BUTTON_START_STOP 1
#define BUTTON_TAP_TEMPO 2
#define BUTTON_LIVE_EDIT 3
#define BUTTON_STEP_EDIT 4
#define BUTTON_A 5
#define BUTTON_SAVE 6
#define BUTTON_BEAT 7
#define BUTTON_MANUAL_TEMPO 8
#define BUTTON_TUPLET 9
#define BUTTON_B 10
#define BUTTON_MENU 11
#define BUTTON_KIT 12
#define BUTTON_TIME_SIGNATURE 13
#define BUTTON_CLEAR 14
#define BUTTON_BACK 15
#define BUTTON_INC 16
#define BUTTON_DEC 17
#define BUTTON_CONFIRM 18
#define BUTTON_CANCEL 19
#define NO_ACTIVE_BUTTON -1
#define ERROR_DISPLAY -2
#define BOOTUP_VISUALS -3

// menu settings
#define NUM_MENU_SETTINGS 10
#define SETTING_CLOCK_MODE 0
#define SETTING_OUTPUT_1 1
#define SETTING_OUTPUT_2 2
#define SETTING_GLITCH_CHANNEL 3
#define SETTING_OUTPUT_PULSE_LENGTH 4
#define SETTING_OUTPUT_PPQN 5
#define SETTING_INPUT_PPQN 6
#define SETTING_PITCH_CURVE 7
#define SETTING_INPUT_QUANTIZE 8
#define SETTING_FACTORY_RESET 9

// non-menu settings
#define SETTING_BEAT 8
#define SETTING_TEMPO 9
#define SETTING_TUPLET 10
#define SETTING_TIME_SIG 11

#define GLITCH_CHANNEL_BOTH 0
#define GLITCH_CHANNEL_1 1
#define GLITCH_CHANNEL_2 2
#define GLITCH_CHANNEL_NONE 3

#define PITCH_CURVE_DEFAULT 0
#define PITCH_CURVE_LINEAR 1
#define PITCH_CURVE_FORWARDS 2

// pot numbers
#define POT_CHANCE 0
#define POT_ZOOM 1
#define POT_CLUSTER 2
#define POT_CROP 3
#define POT_VELOCITY 4
#define POT_RANGE 5
#define POT_MAGNET 6
#define POT_SWING 7
#define POT_CRUSH 8
#define POT_PITCH 9
#define POT_DROP_RANDOM 10
#define POT_DROP 11

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

// group statuses
#define GROUP_PENDING 0
#define GROUP_YES 1
#define GROUP_NO 2

// analog smoothing etc
#define ANALOG_DEAD_ZONE_LOWER 200
#define ANALOG_DEAD_ZONE_UPPER (4095 - ANALOG_DEAD_ZONE_LOWER)
#define ANALOG_EFFECTIVE_RANGE (ANALOG_DEAD_ZONE_UPPER - ANALOG_DEAD_ZONE_LOWER)

// flash data storage
#define FLASH_DATA_ADDRESS (1024 * 1024)
#define FLASH_USER_BEATS_ADDRESS (FLASH_DATA_ADDRESS + 64 * FLASH_SECTOR_SIZE)
#define FLASH_AUDIO_METADATA_ADDRESS (FLASH_DATA_ADDRESS + 127 * FLASH_SECTOR_SIZE)
#define FLASH_AUDIO_ADDRESS (FLASH_DATA_ADDRESS + 128 * FLASH_SECTOR_SIZE)
const uint8_t *flashData = (const uint8_t *)(XIP_BASE + FLASH_DATA_ADDRESS);
const uint8_t *flashUserBeats = (const uint8_t *)(XIP_BASE + FLASH_USER_BEATS_ADDRESS);
const uint8_t *flashAudio = (const uint8_t *)(XIP_BASE + FLASH_AUDIO_ADDRESS);
const uint8_t *flashAudioMetadata = (const uint8_t *)(XIP_BASE + FLASH_AUDIO_METADATA_ADDRESS);
#define CHECK_NUM 100
#define DATA_CHECK 0
#define SAMPLE_START_POINTS 4
#define SAMPLE_LENGTHS (SAMPLE_START_POINTS + 8 * 4)
#define SAMPLE_RATES (SAMPLE_LENGTHS + 8 * 4)

// Beat variables
#define NUM_BEATS 24
#define MASTER_PPQN 3360                                    // lowest common multiple of 32, 24, 15, and 28 (the highest PPQN values used for the various tuplet modes) - currently unused, really just an aspiration to use a less stupid timing system than the current horrible one
uint16_t tempo = 120;                                       // BPM
uint64_t stepTime = 2646000 / (tempo * QUARTER_NOTE_STEPS); // microseconds
int samplesPerStep;                                         // slower tempos give higher values
uint32_t SAMPLE_RATE = 44100;
bool beatPlaying = false;
int beatNum = 0;
int saveBeatLocation;
Beat beats[NUM_BEATS];
Beat beatBackup; // copy of current beat before editing, to allow easy revert if not saving new version
Sample samples[NUM_SAMPLES];
int editSample = 0;
int editStep = 0;

// SD card stuff
#define MAX_SAMPLE_FOLDERS 16
#define MAX_FOLDER_NAME_LENGTH 16
char folderNames[MAX_SAMPLE_FOLDERS][MAX_FOLDER_NAME_LENGTH];
char path[] = "samples/";
int numSampleFolders = 0;

// temporary (ish?) LED variables (first 8 bits are the segments, next 4 are the character selects, final 4 are 3mm LEDs)
#define LED_PULSE_LENGTH 10000
uint8_t sevenSegData[4] = {0b00000000, 0b00000000, 0b00000000, 0b00000000};
uint8_t singleLedData = 0b0000; // 4 x 3mm LEDs
uint16_t storedLedData[4] = {0, 0, 0, 0};
// timers and alarms
repeating_timer_t mainTimer;
repeating_timer_t performanceCheckTimer;

uint analogLoopNum = 0; // 0 to 7
uint analogPhase = 0;   // 0 or 1
uint analogReadings[16] = {0};

uint shiftRegOutLoopNum = 0; // 0 to 15
uint shiftRegOutPhase = 0;   // 0, 1, or 2
uint sevenSegCharNum = 0;

uint32_t buttonStableStates = 0;       // array of bools not good use of space, change if running out of space
uint16_t cyclesSinceChange[32] = {0};  // shift reg updates since state change
uint shiftRegInLoopNum = 0;            // 0 to 15
uint shiftRegInPhase = 0;              // 0 or 1

int outputPulseLength = 15;
int pitchCurve = PITCH_CURVE_DEFAULT;
const int NUM_QUANTIZE_VALUES = 4;
int quantizeValues[NUM_QUANTIZE_VALUES] = {4, 8, 16, 32};
int inputQuantizeIndex = 0;
uint inputQuantize = quantizeValues[inputQuantizeIndex];

void initGpio();
bool performanceCheck(repeating_timer_t *rt);
bool mainTimerLogic(repeating_timer_t *rt);
struct audio_buffer_pool *init_audio();
char getNthDigit(int x, int n);
void updateLedDisplayNumber(int num);
void updateLedDisplayAlpha(const char *word);
void handleIncDec(bool isInc, bool isHold);
void handleYesNo(bool isYes);
void displayClockMode();
void displayTempo();
void displayBeat();
void displayEditBeat();
void displayTimeSignature();
void displayTuplet();
void displayOutput(int outputNum);
void displaySettings();
void handleButtonChange(int buttonNum, bool buttonState);
void updateShiftRegButtons();
void updateAnalog();
void getNthSampleFolder(int n);
void scanSampleFolders();
void loadSamplesFromSD();
void updateLeds();
void pulseGpio(uint gpioNum, uint16_t delayMicros);
void pulseLed(uint ledNum, uint16_t pulseLengthMicros);
void loadSamplesFromFlash();
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
void displayPulse(int pulseStep, uint16_t delayMicros);
void revertBeat(int revertBeatNum);
void backupBeat(int backupBeatNum);
void showError(const char *msgx);
void initFlash(bool doFactoryReset);
void saveSettings();
void loadSettings();
bool checkSettingsChange();
void scheduleSaveSettings();
int64_t stepAlarmCallback(alarm_id_t id, void *user_data);
void setStepAlarm();
void applyDeadZones(int &param);
void saveAllBeats();
void loadDefaultBeats();

#endif