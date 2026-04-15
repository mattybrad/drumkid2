#pragma once

#include <cstdint>

#define MAX_PROGRAM_SIZE (1024*1024) // 1MB for program, rest of flash for user data and samples
#define FLASH_SIZE (4*1024*1024)
#define FLASH_PAGE_SIZE 256
#define FLASH_SECTOR_SIZE 4096
#define MAX_KITS 8
#define MAX_CHANNELS 16
static_assert(MAX_CHANNELS <= 16, "MAX_CHANNELS must not exceed 16"); // need to rewrite metadata format if more than 16 channels required, would require more than one page of flash (actually need to rewrite metadata format anyway now we're using multiple kits)
#define MAX_FOLDER_NAME_LENGTH 32
#define MAX_FOLDERS 64

/*

Memory info:
Pico 2 has 512KB of SRAM, 4MB of flash memory (1024 sectors x 4096 bytes)
One sector contains 16 flash pages (256 bytes per page)
Allowing 1MB of flash for code, need a way of checking size on compile
3MB for data storage

Sector usage:

From    To      Description
0       255     Program (1MB max, probably overkill, V1 drumkid.bin was ~286kB)
256     319     Settings, updated on a 64-sector cycle to reduce wear
320     383     User beats (1024 bytes per beat, theoretical max 256 beats)
384     384     Audio metadata
385     1023    Audio sample data

Audio metadata format (1 page per kit, up to 16 kits in theory)
For each kit:
    Kit name (32 bytes)
    Kit start sector (2 bytes)
    Total kit size in sectors (2 bytes)
    Number of samples (1 byte)
    Check number (1 byte, used to check whether flash is valid)
    For each sample in kit (up to 16 samples in theory):
        Sample address (page number) in flash (4 bytes)
        Sample length in bytes (4 bytes)
        Sample sample rate (4 bytes)

*/

#define SECTOR_SETTINGS 256
#define SECTOR_AUDIO_METADATA 384
#define SECTOR_AUDIO_DATA_START 385

// byte positions/offsets within settings page
// all settings are currently 32-bit (4-byte) values
#define SETTINGS_CLOCK_MODE 0
#define SETTINGS_TEMPO 4
#define SETTINGS_TIME_SIGNATURE 8
#define SETTINGS_TUPLET 12
#define SETTINGS_BEAT_NUM 16
#define SETTINGS_KIT_NUM 20
#define SETTINGS_CHECK_NUM 252
#define VALUE_SETTINGS_CHECK_NUM 12345678

// default settings values
#define DEFAULT_CLOCK_MODE 0
#define DEFAULT_TEMPO 120
#define DEFAULT_TIME_SIGNATURE 4
#define DEFAULT_TUPLET 0
#define DEFAULT_BEAT_NUM 0
#define DEFAULT_KIT_NUM 0

// byte positions/offsets within audio metadata page
// (TODO rename these constants for consistency)
#define PAGE_ADDRESS_KIT_NAME 0
#define PAGE_ADDRESS_KIT_START_SECTOR 32
#define PAGE_ADDRESS_KIT_SIZE 34
#define PAGE_ADDRESS_NUM_SAMPLES 36
#define PAGE_ADDRESS_CHECK_NUM 37
#define PAGE_ADDRESS_SAMPLE_INFO_START 38
#define PAGE_OFFSET_SAMPLE_ADDRESS 0
#define PAGE_OFFSET_SAMPLE_LENGTH 4
#define PAGE_OFFSET_SAMPLE_RATE 8

#define BUTTON_PLAY 1
#define BUTTON_TAP 2
#define BUTTON_LIVE 3
#define BUTTON_STEP 4
#define BUTTON_A 5
#define BUTTON_SAVE 6
#define BUTTON_BEAT 7
#define BUTTON_TEMPO 8
#define BUTTON_TUPLET 9
#define BUTTON_B 10
#define BUTTON_MENU 11
#define BUTTON_KIT 12
#define BUTTON_TIME_SIGNATURE 13
#define BUTTON_CLEAR 14
#define BUTTON_BACK 15
#define BUTTON_INC 16
#define BUTTON_DEC 17
#define BUTTON_YES 18
#define BUTTON_NO 19
#define BUTTON_POWER_OFF 20