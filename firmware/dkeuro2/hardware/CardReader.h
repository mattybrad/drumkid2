#pragma once

#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include <cstdint>

#define FLASH_SECTOR_SIZE (4096)
#define FLASH_PAGE_SIZE (256)

class CardReader {
    public:
        void init();
        void transferWavToFlash(const char* path);
        
    private:
        // Private methods and members for card communication
};