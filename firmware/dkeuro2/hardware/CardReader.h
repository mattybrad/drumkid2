#pragma once

#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include <cstdint>
#include "Config.h"
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
        void transferAudioFolderToFlash(const char* folderPath, uint8_t kitSlot, uint32_t flashSectorStart);
        SampleInfo parseWavFile(const char* path, bool writeToFlash = false, uint32_t flashPageStart = 0);
        void transferWavToFlash(const char* path, uint32_t flashPageStart);
        void scanSampleFolders();
        uint16_t getNumSampleFolders() const { return _numFolders; };
        const char* getSampleFolderName(uint16_t index) const { return (index < _numFolders) ? _folderNames[index] : nullptr; };
        int getKitSizeSectors(uint8_t kitIndex);
        
    private:
        Memory *_memory;
        char _folderNames[MAX_FOLDERS][MAX_FOLDER_NAME_LENGTH];
        uint16_t _numFolders = 0;
        // Private methods and members for card communication
};