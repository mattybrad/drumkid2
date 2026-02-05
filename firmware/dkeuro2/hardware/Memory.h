#pragma once
#include <cstdint>
#include <cstddef>

#define MAX_PROGRAM_SIZE (1024*1024) // 1MB for program, rest of flash for user data and samples
#define FLASH_SIZE (4*1024*1024)
#define FLASH_SECTOR_SIZE (4096)
#define FLASH_PAGE_SIZE (256)

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
        void writeToFlash(uint16_t page, const uint8_t* data, size_t length);
        void writeToFlashPage(uint16_t page, const uint8_t* data);
    private:
        bool _operationInProgress;
        uint8_t _pageReady[(FLASH_SIZE) / FLASH_PAGE_SIZE / 8]; // bitfield to track which pages have been erased and are ready for writing
        uint8_t _sectorBuffer[FLASH_SECTOR_SIZE]; // buffer for one flash sector of data
};