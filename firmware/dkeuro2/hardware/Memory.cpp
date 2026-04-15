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

    // check settings page is valid
    uint32_t checkNumber = readSetting(SETTINGS_CHECK_NUM);
    if(checkNumber != VALUE_SETTINGS_CHECK_NUM) {
        printf("Warning: Settings page is invalid (check number: %u), resetting to default settings\n", checkNumber);

        uint8_t defaultSettings[FLASH_PAGE_SIZE] = {0};
        *(uint32_t*)(&defaultSettings[SETTINGS_CLOCK_MODE]) = DEFAULT_CLOCK_MODE;
        *(uint32_t*)(&defaultSettings[SETTINGS_TEMPO]) = DEFAULT_TEMPO;
        *(uint32_t*)(&defaultSettings[SETTINGS_TIME_SIGNATURE]) = DEFAULT_TIME_SIGNATURE;
        *(uint32_t*)(&defaultSettings[SETTINGS_TUPLET]) = DEFAULT_TUPLET;
        *(uint32_t*)(&defaultSettings[SETTINGS_BEAT_NUM]) = DEFAULT_BEAT_NUM;
        *(uint32_t*)(&defaultSettings[SETTINGS_KIT_NUM]) = DEFAULT_KIT_NUM;
        *(uint32_t*)(&defaultSettings[SETTINGS_CHECK_NUM]) = VALUE_SETTINGS_CHECK_NUM;

        writeToFlashPage(SECTOR_SETTINGS * (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE), defaultSettings);
    }
}

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

void Memory::moveSector(uint32_t sourceSector, uint32_t destSector) {
    if(sourceSector * FLASH_SECTOR_SIZE >= FLASH_SIZE || destSector * FLASH_SECTOR_SIZE >= FLASH_SIZE) {
        printf("Error: Attempt to move invalid flash sector (source: %d, dest: %d)\n", sourceSector, destSector);
        return;
    }
    if(sourceSector == destSector) {
        printf("Warning: Attempt to move sector %d to the same location, skipping\n", sourceSector);
        return;
    }
    printf("Moving flash sector from %d to %d\n", sourceSector, destSector);
    
    backupSector(sourceSector);
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(destSector * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    flash_range_program(destSector * FLASH_SECTOR_SIZE, _sectorBuffer, FLASH_SECTOR_SIZE);
    restore_interrupts(interrupts);
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

uint32_t Memory::readSetting(uint32_t settingOffset) {
    return readIntFromFlash(SECTOR_SETTINGS * FLASH_SECTOR_SIZE + settingOffset, 4);
}
