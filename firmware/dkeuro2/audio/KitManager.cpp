#include "audio/KitManager.h"
#include "hardware/flash.h"
#include "Config.h"
#include <cstring>

void KitManager::init(Memory* memory) {
    _memory = memory;

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
        if(numSamples > 0) {
            char folderName[33];
            memcpy(folderName, (const void *)(XIP_BASE + metadataAddress + PAGE_ADDRESS_KIT_NAME), 32);
            folderName[32] = '\0';
            printf("Kit %d: %d samples, folder name: %s\n", i+1, numSamples, folderName);
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

// void KitManager::loadKitFromCard(uint16_t folderIndex, uint16_t kitSlot) {
//     printf("Loading kit from card - folder index: %d, kit slot: %d\n", folderIndex, kitSlot);
//     if(folderIndex >= _cardReader->getNumSampleFolders() || kitSlot >= MAX_KITS) {
//         printf("Invalid folder index or kit slot\n");
//         return;
//     }
//     _cardReader->transferAudioFolderToFlash(_cardReader->getSampleFolderName(folderIndex));
//     // after transferring, re-init to update kit metadata from flash
//     init(_memory);
// }

uint32_t KitManager::getFreeSectors() {
    // for now just return a dummy value, will need to implement a way of tracking used/free sectors in flash to get an accurate number here
    return 256; // 1MB free (256 sectors) for audio data, starting from sector 386
}