#include "CardReader.h"
#include <algorithm>

void CardReader::init(Memory *memory) {
    _memory = memory;
}

bool CardReader::checkCardInserted() {
    sd_card_t *pSD = sd_get_by_num(0);
    if (!pSD->sd_test_com(pSD))
    {
        printf("SD card not detected!\n");
        //flagError(ERROR_SD_MISSING);
        return false;
    }
    //clearError(ERROR_SD_MISSING);
    return true;
}

bool CardReader::mountCard() {
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr)
    {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        //flagError(ERROR_SD_MOUNT);
        return false;
    }
    return true;
}

void CardReader::transferAudioFolderToFlash(const char* folderPath) {
    sd_init_driver();
    
    if(!checkCardInserted()) {
        return;
    }

    if(!mountCard()) {
        return;
    }

    bool exitFindLoop = false;
    int numSamples = 0;
    uint8_t audioMetadataPage[FLASH_PAGE_SIZE] = {0};
    uint32_t samplePositionTally = 0;
    for (int i = 1; i <= 16 && !exitFindLoop; i++) {
        char path[128];
        snprintf(path, sizeof(path), "samples/%s/%d.wav", folderPath, i);
        //printf("Checking for sample at path: %s\n", path);
        SampleInfo info = getSampleInfo(path);
        if(info.lengthBytes > 0) {
            printf("Sample %d: length=%d bytes, sample rate=%d\n", i, info.lengthBytes, info.sampleRate);
            uint sampleMetadataOffset = 1 + 15 + 32 + (i-1) * (4+4+4); // numSamples (1 byte) + reserved (15 bytes) + folder name (32 bytes) + sample info for each sample (12 bytes)
            memcpy(&audioMetadataPage[sampleMetadataOffset], &samplePositionTally, 4);
            memcpy(&audioMetadataPage[sampleMetadataOffset + 4], &info.lengthBytes, 4);
            memcpy(&audioMetadataPage[sampleMetadataOffset + 8], &info.sampleRate, 4);
            numSamples = i;
            samplePositionTally += info.lengthBytes;
        } else {
            exitFindLoop = true;
        }
    }
    audioMetadataPage[0] = numSamples;
    memcpy(&audioMetadataPage[16], folderPath, std::min(strlen(folderPath), (size_t)32)); // copy folder name into metadata
    _memory->writeToFlashPage(384 * FLASH_SECTOR_SIZE/FLASH_PAGE_SIZE, audioMetadataPage); // write metadata to flash page
    printf("Found %d samples in folder %s\n", numSamples, folderPath);

    f_unmount("");
}

CardReader::SampleInfo CardReader::getSampleInfo(const char* path) {
    SampleInfo info = {0,0,0};
    FIL fil;
    char filename[128];
    snprintf(filename, sizeof(filename), "%s", path);
    FRESULT fr = f_open(&fil, filename, FA_READ);
    
    // read WAV file header before reading data
    uint br; // bytes read
    char descriptorBuffer[4];
    uint8_t sizeBuffer[4];
    uint8_t sampleDataBuffer[FLASH_PAGE_SIZE];
    //uint8_t sampleDataBuffer2[FLASH_PAGE_SIZE]; // required for stereo samples
    //bool useSecondBuffer = false;
    bool isStereo = false;

    // first 3 4-byte sections should be "RIFF", file size, "WAVE"
    fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
    if(strncmp(descriptorBuffer, "RIFF", 4) != 0) {
        //printf("Not a valid WAV file (no RIFF header)\n");
        f_close(&fil);
        return info;
    }
    fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
    uint32_t fileSize;
    memcpy(&fileSize, descriptorBuffer, 4);
    //printf("WAV file size: %d bytes\n", fileSize);
    fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
    if(strncmp(descriptorBuffer, "WAVE", 4) != 0) {
        //printf("Not a valid WAV file (no WAVE header)\n");
        f_close(&fil);
        return info;
    }
    //printf("Valid WAV file detected\n");

    uint16_t fmtChannels;
    uint32_t fmtRate;
    uint16_t fmtBitsPerSample;
    bool isDataChunk = false;
    uint audioDataPageTally = 0; // IMPORTANT - this actually needs to be defined outside this function because it needs to be cumulative for all samples, but it's late and I'm going to bed
    while (!isDataChunk)
    {
        // after the first three required sections, WAV files have a series of chunks of the form: <chunk ID (4 bytes), chunk size (4 bytes), chunk data (chunk size bytes)>
        fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
        //printf("descriptor=%s, ", descriptorBuffer);
        fr = f_read(&fil, sizeBuffer, sizeof sizeBuffer, &br);
        uint32_t chunkSize;
        memcpy(&chunkSize, sizeBuffer, 4);
        //printf("chunk size=%d bytes\n", chunkSize);
        uint bytesToRead = std::min(sizeof sampleDataBuffer, (uint)chunkSize);
        uint brTotal = 0;
        bool isFormatChunk = strncmp(descriptorBuffer, "fmt ", 4) == 0;
        isDataChunk = strncmp(descriptorBuffer, "data", 4) == 0;
        if(isDataChunk) {
            info.lengthBytes = chunkSize;
        }
        for (;;)
        {
            fr = f_read(&fil, sampleDataBuffer, bytesToRead, &br);

            if(isFormatChunk) {
                //printf("FORMAT SECTION\n");
                memcpy(&fmtChannels, &sampleDataBuffer[2], 2);
                memcpy(&fmtRate, &sampleDataBuffer[4], 4);
                memcpy(&fmtBitsPerSample, &sampleDataBuffer[14], 2);
                //printf("channels=%d, rate=%d, bitsPerSample=%d\n", fmtChannels, fmtRate, fmtBitsPerSample);
                info.flashAddress = 123;
                info.sampleRate = fmtRate;
            }

            if (br == 0)
                break;

            if(isDataChunk) {
                printf(".");
                _memory->writeToFlashPage((384 * FLASH_SECTOR_SIZE) / FLASH_PAGE_SIZE + audioDataPageTally, sampleDataBuffer);
                audioDataPageTally++;
            }

            brTotal += br;
            if (brTotal >= chunkSize)
            {
                printf("\n");
                break;
            }
        }
    }
    return info;
}