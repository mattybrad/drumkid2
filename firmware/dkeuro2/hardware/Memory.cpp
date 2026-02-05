#include "Memory.h"
#include <algorithm>
#include <stdio.h>
#include "hardware/flash.h"
#include "hardware/sync.h"

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

void Memory::init() {
    _operationInProgress = false;
    for(size_t i = 0; i < sizeof(_pageReady); i++) {
        _pageReady[i] = 0;
    }
}

// void Memory::writeToFlash(uint16_t page, const uint8_t* data, size_t length) {
//     uint32_t address = page * FLASH_PAGE_SIZE;

//     // check address begins after program/code ends and is within flash size limits
//     if(address < MAX_PROGRAM_SIZE || address + length > FLASH_SIZE) {
//         printf("Error: Attempt to write to invalid flash address 0x%08X\n", address);
//         return;
//     }
    
//     // for each page we need to write, check if it's ready (erased) and if not, erase the sector first
// }

void Memory::writeToFlashPage(uint16_t page, const uint8_t* data)
{
    if(page * FLASH_PAGE_SIZE < MAX_PROGRAM_SIZE || page * FLASH_PAGE_SIZE >= FLASH_SIZE) {
        printf("Error: Attempt to write to invalid flash page %d (address 0x%08X)\n", page, page * FLASH_PAGE_SIZE);
        return;
    }

    _operationInProgress = true;
    if(!bitReadArray(_pageReady, page)) {
        // page not ready for writing, need to erase sector first
        uint16_t sector = (page * FLASH_PAGE_SIZE) / FLASH_SECTOR_SIZE;
        printf("Erasing flash sector %d\n", sector);
        uint32_t interrupts = save_and_disable_interrupts();
        flash_range_erase(sector * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
        restore_interrupts(interrupts);
        for(size_t i = 0; i < FLASH_SECTOR_SIZE; i++) {
            bitWriteArray(_pageReady, (sector * FLASH_SECTOR_SIZE + i) / FLASH_PAGE_SIZE, 1); // mark all pages in sector as ready
        }
    }

    printf("Writing to flash page %d (address 0x%08X)\n", page, page * FLASH_PAGE_SIZE);
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_program(page * FLASH_PAGE_SIZE, data, FLASH_PAGE_SIZE);
    restore_interrupts(interrupts);
    bitWriteArray(_pageReady, page, 0); // mark page as not ready until next erase
    _operationInProgress = false;
}