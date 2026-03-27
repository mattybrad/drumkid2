#include "Memory.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include "hardware/flash.h"
#include "hardware/sync.h"

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

    //printf("Writing to flash page %d (address 0x%08X)\n", page, page * FLASH_PAGE_SIZE);
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_program(page * FLASH_PAGE_SIZE, data, FLASH_PAGE_SIZE);
    restore_interrupts(interrupts);
    bitWriteArray(_pageReady, page, 0); // mark page as not ready until next erase
    _operationInProgress = false;
}

void Memory::backupSector(uint32_t sector) {
    if(sector * FLASH_SECTOR_SIZE >= FLASH_SIZE) {
        printf("Error: Attempt to backup invalid flash sector %d\n", sector);
        return;
    }
    memcpy(_sectorBuffer, (const void *)(XIP_BASE + sector * FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
    _backupAddress = sector;
}

void Memory::restoreSector(uint32_t sector) {
    if(sector * FLASH_SECTOR_SIZE >= FLASH_SIZE) {
        printf("Error: Attempt to restore invalid flash sector %d\n", sector);
        return;
    }
    if(sector != _backupAddress) {
        printf("Error: Attempt to restore sector %d which is not currently backed up (backup address: %d)\n", sector, _backupAddress);
        return;
    }
    for(size_t i = 0; i < FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE; i++) {
        if(bitReadArray(_pageReady, (sector * FLASH_SECTOR_SIZE) / FLASH_PAGE_SIZE + i)) {
            writeToFlashPage((sector * FLASH_SECTOR_SIZE) / FLASH_PAGE_SIZE + i, &_sectorBuffer[i * FLASH_PAGE_SIZE]); 
        }
    }
}

// just an AI function for now, tidy up later...
uint32_t Memory::readIntFromFlash(uint32_t address, size_t length) {
    if(address >= FLASH_SIZE || address + length > FLASH_SIZE) {
        printf("Error: Attempt to read from invalid flash address 0x%08X\n", address);
        return 0;
    }
    uint32_t result = 0;
    memcpy(&result, (const void *)(XIP_BASE + address), length);
    return result;
}
