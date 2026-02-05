#pragma once

#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include <cstdint>
#include "Memory.h"

class CardReader {
    public:
        void init();
        void transferWavToFlash(const char* path);
        
    private:
        // Private methods and members for card communication
};