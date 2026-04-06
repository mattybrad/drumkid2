#include "audio/KitManager.h"
#include "hardware/flash.h"
#include "Config.h"
#include <cstring>

void KitManager::init(Memory* memory, ChannelManager* channelManager) {
    _memory = memory;
    _channelManager = channelManager;
    reloadMetaData();
}

void KitManager::reloadMetaData() {
    // check that audio metadata check numbers are correct
    bool checkPassed = true;
    for(int i=0; i<MAX_KITS && checkPassed; i++) {
        uint8_t checkNumRead = _memory->readIntFromFlash((SECTOR_AUDIO_METADATA * FLASH_SECTOR_SIZE + i * FLASH_PAGE_SIZE + PAGE_ADDRESS_CHECK_NUM), 1);
        if(checkNumRead != i) {
            printf("Failed check on kit %d\n", i);
            checkPassed = false;
        }
    }
    if(!checkPassed) {
        printf("Audio metadata check failed, formatting default metadata pages\n");
        for(int i=0; i<MAX_KITS; i++) {
            uint8_t initPage[FLASH_PAGE_SIZE] = {0};
            initPage[PAGE_ADDRESS_CHECK_NUM] = i; // write correct check number to allow detection on next init
            _memory->writeToFlashPage((SECTOR_AUDIO_METADATA * FLASH_SECTOR_SIZE) / FLASH_PAGE_SIZE + i, initPage);
        }
    } else {
        printf("Audio metadata check passed\n");
    }

    // initialise kits from flash metadata
    for(int i=0; i<MAX_KITS; i++) {
        uint32_t metadataAddress = SECTOR_AUDIO_METADATA * FLASH_SECTOR_SIZE + i * FLASH_PAGE_SIZE;
        uint8_t numSamples = _memory->readIntFromFlash(metadataAddress + PAGE_ADDRESS_NUM_SAMPLES, 1);
        kits[i].numSamples = numSamples;
        uint16_t startSector = _memory->readIntFromFlash(metadataAddress + PAGE_ADDRESS_KIT_START_SECTOR, 2);
        kits[i].startSector = startSector;
        uint16_t kitSizeSectors = _memory->readIntFromFlash(metadataAddress + PAGE_ADDRESS_KIT_SIZE, 2);
        kits[i].sizeSectors = kitSizeSectors;
        if(numSamples > 0) {
            char folderName[33];
            memcpy(folderName, (const void *)(XIP_BASE + metadataAddress + PAGE_ADDRESS_KIT_NAME), 32);
            folderName[32] = '\0';
            printf("Kit %d: %d samples, start sector %d, %d sectors, folder name: %s\n", i+1, numSamples, startSector, kitSizeSectors, folderName);
            memcpy(kits[i].name, folderName, 32);

            for(int j=0; j<numSamples; j++) {
                kits[i].samples[j].address = _memory->readIntFromFlash(metadataAddress + PAGE_ADDRESS_SAMPLE_INFO_START + j * (4+4+4) + PAGE_OFFSET_SAMPLE_ADDRESS, 4);
                kits[i].samples[j].lengthSamples = _memory->readIntFromFlash(metadataAddress + PAGE_ADDRESS_SAMPLE_INFO_START + j * (4+4+4) + PAGE_OFFSET_SAMPLE_LENGTH, 4);
                kits[i].samples[j].sampleRate = _memory->readIntFromFlash(metadataAddress + PAGE_ADDRESS_SAMPLE_INFO_START + j * (4+4+4) + PAGE_OFFSET_SAMPLE_RATE, 4);
                printf("  Sample %d: flash page %d, length %d samples, sample rate %d\n", j+1, kits[i].samples[j].address, kits[i].samples[j].lengthSamples, kits[i].samples[j].sampleRate);
            }
        } else {
            printf("Kit %d: empty\n", i+1);
        }
    }
}

void KitManager::initKit(uint8_t newKitNum) {
    if(newKitNum >= MAX_KITS) {
        printf("Invalid kit number\n");
        return;
    }
    kitNum = newKitNum; // set current kit number
    
    // some stuff could be tidied up here probably
    for(int i=0; i<MAX_CHANNELS; i++) {
        if(kits[kitNum].numSamples > i) {
            _channelManager->channels[i].sampleData = (const int16_t *)(XIP_BASE + (kits[kitNum].samples[i].address * FLASH_PAGE_SIZE));
            _channelManager->channels[i].sampleLength = kits[kitNum].samples[i].lengthSamples;
            _channelManager->channels[i].playbackSpeed = (int64_t)(kits[kitNum].samples[i].sampleRate * (1LL << 32) / 44100); // convert sample rate to Q32.32 format for playback speed
        }
    }
    _channelManager->numChannels = kits[kitNum].numSamples;
}

uint32_t KitManager::getFreeSectors(uint8_t kitSlot) {
    uint32_t usedSectorTally = 0;
    for(int i=0; i<MAX_KITS; i++) {
        if(i != kitSlot) {
            usedSectorTally += kits[i].sizeSectors;
        }
    }
    printf("Used sectors: %d\n", usedSectorTally);
    uint32_t freeSectors = (1023 - SECTOR_AUDIO_DATA_START) - usedSectorTally + 1;
    printf("Free sectors: %d\n", freeSectors);
    return freeSectors; // assuming 256 sectors available for audio data
}

void KitManager::createSpaceForKit(uint8_t kitSlot, uint32_t kitSizeSectors) {
    if(kitSlot >= MAX_KITS) {
        printf("Invalid kit slot\n");
        return;
    }
    printf("Creating space for kit in slot %d, size %d sectors\n", kitSlot, kitSizeSectors);
    uint32_t freeSectors = getFreeSectors(kitSlot);
    if(kitSizeSectors > freeSectors) {
        printf("Not enough free space to create space for kit\n");
        return;
    }

    // find start sector for new kit by tallying used sectors from existing kits
    uint32_t newKitStartSector = SECTOR_AUDIO_DATA_START;
    for(int i=0; i<kitSlot; i++) {
        if(kits[i].sizeSectors > 0) {
            newKitStartSector += kits[i].sizeSectors;
        }
    }
    printf("New kit will be written starting at sector %d\n", newKitStartSector);

    bool anyKitsAfter = false;
    uint32_t existingNextKitStartSector = 0;
    for(int i=kitSlot+1; i<MAX_KITS; i++) {
        if(kits[i].sizeSectors > 0) {
            anyKitsAfter = true;
            existingNextKitStartSector = kits[i].startSector;
            printf("Existing kit found after slot %d at sector %d\n", kitSlot, existingNextKitStartSector);
            break;
        }
    }

    if(!anyKitsAfter) {
        printf("No existing kits after slot %d, no need to move data\n", kitSlot);
        return;
    }

    if(newKitStartSector + kitSizeSectors == existingNextKitStartSector) {
        printf("New kit fits exactly in free space, no need to move data\n");
        return;
    }

    int32_t sectorShift = (newKitStartSector + kitSizeSectors) - existingNextKitStartSector;
    printf("Shifting existing kits starting from sector %d by %d sectors\n", existingNextKitStartSector, sectorShift);

}