#pragma once

#include <cstdint>

#define MAX_PROGRAM_SIZE (1024*1024) // 1MB for program, rest of flash for user data and samples
#define FLASH_SIZE (4*1024*1024)
#define FLASH_PAGE_SIZE 256
#define FLASH_SECTOR_SIZE 4096
#define MAX_CHANNELS 16
static_assert(MAX_CHANNELS <= 16, "MAX_CHANNELS must not exceed 16"); // need to rewrite metadata format if more than 16 channels required, would require more than one page of flash
#define MAX_FOLDER_NAME_LENGTH 32

/*

Memory info:
Pico 2 has 512KB of SRAM, 4MB of flash memory (1024 sectors x 4096 bytes)
Allowing 1MB of flash for code, need a way of checking size on compile
3MB for data storage

Sector usage:

From    To      Description
0       255     Program (1MB max, probably overkill, drumkid.bin is 286kB today)
256     319     Settings, updated on a 64-sector cycle to reduce wear
320     383     User beats (1024 bytes per beat, theoretical max 256 beats)
384     384     Audio metadata
385     1023    Audio sample data

Audio metadata format (one page, assume max 16 samples for now):

Number of samples (1 byte)
Reserved (15 bytes)
Folder name (32 bytes)
Sample n flash address (4 bytes)
Sample n length in bytes (4 bytes)
Sample n sample rate (4 bytes)

*/

#define SECTOR_AUDIO_METADATA 384
#define SECTOR_AUDIO_DATA_START 385

// byte positions/offsets within audio metadata page
#define ADDRESS_AUDIO_METADATA_NUM_SAMPLES 0
#define ADDRESS_AUDIO_METADATA_FOLDER_NAME 16
#define ADDRESS_AUDIO_METADATA_SAMPLE_INFO_START 48
#define OFFSET_AUDIO_METADATA_PAGE_NUM 0
#define OFFSET_AUDIO_METADATA_LENGTH_SAMPLES 4
#define OFFSET_AUDIO_METADATA_SAMPLE_RATE 8