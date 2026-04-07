#pragma once
#include <cstdint>
#include <cstddef>
#include "Config.h"

// Borrowing some useful Arduino macros, but updating from 1UL to 1ULL for 64-bit compatibility
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitReadArray(array, bitIndex) bitRead(array[(bitIndex) / 8], (bitIndex) % 8)
#define bitSet(value, bit) ((value) |= (1ULL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1ULL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))
#define bitWriteArray(array, bitIndex, bitValue) (bitValue ? bitSet(array[(bitIndex) / 8], (bitIndex) % 8) : bitClear(array[(bitIndex) / 8], (bitIndex) % 8))

class Memory {
    public:
        void init();
        //bool operationInProgress();
        //void writeToFlash(uint16_t page, const uint8_t* data, size_t length);
        void writeToFlashPage(uint16_t page, const uint8_t* data);
        uint32_t readIntFromFlash(uint32_t address, size_t length);
        void backupSector(uint32_t sector);
        void restoreSector(uint32_t sector);
        void moveSector(uint32_t sourceSector, uint32_t destSector);
    private:
        bool _operationInProgress;
        uint8_t _pageReady[(FLASH_SIZE) / FLASH_PAGE_SIZE / 8]; // bitfield to track which pages have been erased and are ready for writing
        uint8_t _sectorBuffer[FLASH_SECTOR_SIZE]; // buffer for one flash sector of data
        uint32_t _backupAddress; // address of flash sector currently backed up in buffer
};