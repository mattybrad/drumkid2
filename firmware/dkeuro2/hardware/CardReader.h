#pragma once

#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include <cstdint>
#include "Memory.h"

class CardReader {
    public:
        FATFS fs;
        struct SampleInfo {
            uint32_t flashAddress;
            uint32_t lengthBytes;
            uint32_t sampleRate;
        };
        void init(Memory *memory);
        bool checkCardInserted();
        bool mountCard();
        void transferAudioFolderToFlash(const char* folderPath);
        SampleInfo getSampleInfo(const char* path);
        void transferWavToFlash(const char* path);
        
    private:
        Memory *_memory;
        // Private methods and members for card communication
};