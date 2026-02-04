#include "CardReader.h"
#include <algorithm>

void CardReader::init() {
    
}

void CardReader::transferWavToFlash(const char* path) {
    sd_init_driver();
    sd_card_t *pSD = sd_get_by_num(0);
    if (!pSD->sd_test_com(pSD))
    {
        printf("SD card not detected!\n");
        //flagError(ERROR_SD_MISSING);
        return;
    }
    //clearError(ERROR_SD_MISSING);
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr)
    {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        //flagError(ERROR_SD_MOUNT);
        return;
    }

    FIL fil;
    char filename[255];
    snprintf(filename, sizeof(filename), "%s", path);
    fr = f_open(&fil, filename, FA_READ);
    if (FR_OK != fr)
    {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        //flagError(ERROR_SD_OPEN);
        return;
    }
    //clearError(ERROR_SD_OPEN);

    printf("card okay\n");

    // read WAV file header before reading data
    uint br; // bytes read
    bool foundDataChunk = false;
    char descriptorBuffer[4];
    uint8_t sizeBuffer[4];
    uint8_t sampleDataBuffer[FLASH_PAGE_SIZE];
    //uint8_t sampleDataBuffer2[FLASH_PAGE_SIZE]; // required for stereo samples
    //bool useSecondBuffer = false;
    bool isStereo = false;

    // first 3 4-byte sections should be "RIFF", file size, "WAVE"
    fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
    if(strncmp(descriptorBuffer, "RIFF", 4) != 0) {
        printf("Not a valid WAV file (no RIFF header)\n");
        f_close(&fil);
        return;
    }
    fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
    uint32_t fileSize;
    memcpy(&fileSize, descriptorBuffer, 4);
    printf("WAV file size: %d bytes\n", fileSize);
    fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
    if(strncmp(descriptorBuffer, "WAVE", 4) != 0) {
        printf("Not a valid WAV file (no WAVE header)\n");
        f_close(&fil);
        return;
    }
    printf("Valid WAV file detected\n");

    while (!foundDataChunk)
    {
        // after the first three required sections, WAV files have a series of chunks of the form: <chunk ID (4 bytes), chunk size (4 bytes), chunk data (chunk size bytes)>
        fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
        printf("descriptor=%s, ", descriptorBuffer);
        fr = f_read(&fil, sizeBuffer, sizeof sizeBuffer, &br);
        uint32_t chunkSize;
        memcpy(&chunkSize, sizeBuffer, 4);
        printf("size=%d bytes\n", chunkSize);
        uint brTotal = 0;
        for (;;)
        {
            uint bytesToRead = std::min(sizeof sampleDataBuffer, (uint)chunkSize);
            fr = f_read(&fil, sampleDataBuffer, bytesToRead, &br);

            if(strncmp(descriptorBuffer, "fmt ", 4) == 0) {
                printf("FORMAT SECTION\n");
                uint16_t fmtType;
                uint16_t fmtChannels;
                uint32_t fmtRate;
                uint16_t fmtBitsPerSample;
                memcpy(&fmtType, &sampleDataBuffer[0], 2);
                memcpy(&fmtChannels, &sampleDataBuffer[2], 2);
                memcpy(&fmtRate, &sampleDataBuffer[4], 4);
                memcpy(&fmtBitsPerSample, &sampleDataBuffer[14], 2);
                printf("type=%d, channels=%d, rate=%d, bitsPerSample=%d\n", fmtType, fmtChannels, fmtRate, fmtBitsPerSample);
                //sampleRates[n] = fmtRate;
                if(fmtChannels == 2) isStereo = true;
            }

            if (br == 0)
                break;

            if(strncmp(descriptorBuffer, "data", 4) == 0) {
                //printf("DATA SECTION\n");
                //printf("data chunk size: %d bytes\n", chunkSize);
                foundDataChunk = true;
            }

            // if (strncmp(descriptorBuffer, "data", 4) == 0)
            // {
            //     if (!foundDataChunk)
            //     {
            //         sampleStartPoints[n] = pageNum * FLASH_PAGE_SIZE;
            //         sampleLengths[n] = isStereo ? chunkSize>>1 : chunkSize;
            //         totalSize += sampleLengths[n];
            //         printf("sample %d, start on page %d, length %d\n", n, sampleStartPoints[n], sampleLengths[n]);
            //     }
            //     foundDataChunk = true;

            //     if (!dryRun)
            //     {
            //         if(!isStereo) {
            //             // mono
            //             writePageToFlash(sampleDataBuffer, FLASH_SECTOR_AUDIO_DATA_START * FLASH_SECTOR_SIZE + pageNum * FLASH_PAGE_SIZE);
            //             pageNum++;
            //         } else {
            //             // stereo
            //             if(useSecondBuffer) {
            //                 int16_t thisSampleLeft;
            //                 int16_t thisSampleRight;
            //                 for(int i=0; i<FLASH_PAGE_SIZE; i+=2) {
            //                     if(i < FLASH_PAGE_SIZE>>1) {
            //                         std::memcpy(&thisSampleLeft, &sampleDataBuffer[i*2], 2);
            //                         std::memcpy(&thisSampleRight, &sampleDataBuffer[i * 2 + 2], 2);
            //                         //sampleDataBuffer[i] = sampleDataBuffer[i*2];
            //                         //sampleDataBuffer[i+1] = sampleDataBuffer[i * 2 + 1];
            //                     } else {
            //                         std::memcpy(&thisSampleLeft, &sampleDataBuffer2[i * 2 - FLASH_PAGE_SIZE], 2);
            //                         std::memcpy(&thisSampleRight, &sampleDataBuffer2[i * 2 + 2 - FLASH_PAGE_SIZE], 2);
            //                         //sampleDataBuffer[i] = sampleDataBuffer2[i*2-FLASH_PAGE_SIZE];
            //                         //sampleDataBuffer[i+1] = sampleDataBuffer2[i * 2 + 1 - FLASH_PAGE_SIZE];
            //                     }
            //                     int16_t thisSampleMono = (thisSampleLeft>>1) + (thisSampleRight>>1);
            //                     std::memcpy(&sampleDataBuffer[i], &thisSampleMono, 2);
            //                 }
            //                 writePageToFlash(sampleDataBuffer, FLASH_SECTOR_AUDIO_DATA_START * FLASH_SECTOR_SIZE + pageNum * FLASH_PAGE_SIZE);
            //                 pageNum ++;
            //             }
            //             useSecondBuffer = !useSecondBuffer;
            //         }
            //     }

            //     //pageNum++;
            // }

            brTotal += br;
            if (brTotal == chunkSize)
            {
                break;
            }
        }
    }

    // printf("\n");
    fr = f_close(&fil);
    if (FR_OK != fr)
    {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
    f_unmount("");
}