#pragma once

#include <cstdint>

#define MAX_PROGRAM_SIZE (1024*1024) // 1MB for program, rest of flash for user data and samples
#define FLASH_SIZE (4*1024*1024)
#define FLASH_PAGE_SIZE 256
#define FLASH_SECTOR_SIZE 4096
#define MAX_CHANNELS 2