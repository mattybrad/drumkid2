#include "CardReader.h"
#include <algorithm>

void CardReader::init(Memory *memory) {
    _memory = memory;
    scanSampleFolders();
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

void CardReader::transferAudioFolderToFlash(const char* folderPath, uint8_t kitSlot, uint32_t flashSectorStart) {
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
    memcpy(&audioMetadataPage[PAGE_ADDRESS_CHECK_NUM], &kitSlot, 1); // write kit slot to check num for basic validation of metadata
    uint32_t samplePageNumberTally = flashSectorStart * FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE;
    uint32_t samplePageNumbers[MAX_CHANNELS] = {0}; // support up to 16 samples for now, can expand later if needed
    for (int i = 1; i <= MAX_CHANNELS && !exitFindLoop; i++) {
        char path[128];
        snprintf(path, sizeof(path), "samples/%s/%d.wav", folderPath, i);
        //printf("Checking for sample at path: %s\n", path);
        SampleInfo info = parseWavFile(path);
        if(info.lengthBytes > 0) {
            printf("Sample %d: length=%d bytes, sample rate=%d\n", i, info.lengthBytes, info.sampleRate);
            uint sampleMetadataOffset = PAGE_ADDRESS_SAMPLE_INFO_START + (i-1) * (4+4+4);
            samplePageNumbers[i-1] = samplePageNumberTally;
            memcpy(&audioMetadataPage[sampleMetadataOffset + PAGE_OFFSET_SAMPLE_ADDRESS], &samplePageNumberTally, 4);
            uint32_t lengthSamples = info.lengthBytes / 2;
            memcpy(&audioMetadataPage[sampleMetadataOffset + PAGE_OFFSET_SAMPLE_LENGTH], &lengthSamples, 4);
            memcpy(&audioMetadataPage[sampleMetadataOffset + PAGE_OFFSET_SAMPLE_RATE], &info.sampleRate, 4);
            numSamples = i;
            samplePageNumberTally += (info.lengthBytes / FLASH_PAGE_SIZE) + 1;
        } else {
            exitFindLoop = true;
        }
    }

    audioMetadataPage[PAGE_ADDRESS_NUM_SAMPLES] = numSamples;
    memcpy(&audioMetadataPage[PAGE_ADDRESS_KIT_NAME], folderPath, std::min(strlen(folderPath), (size_t)32)); // copy folder name into metadata
    _memory->writeToFlashPage(SECTOR_AUDIO_METADATA * FLASH_SECTOR_SIZE/FLASH_PAGE_SIZE + kitSlot, audioMetadataPage); // write metadata to flash page
    printf("Found %d samples in folder %s\n", numSamples, folderPath);

    printf("Transferring samples from folder %s to flash...\n", folderPath);
    for(int i = 1; i <= numSamples; i++) {
        char path[128];
        snprintf(path, sizeof(path), "samples/%s/%d.wav", folderPath, i);
        parseWavFile(path, true, samplePageNumbers[i-1]);
    }

    f_unmount("");
}

CardReader::SampleInfo CardReader::parseWavFile(const char* path, bool writeToFlash, uint32_t flashPageStart) {
    SampleInfo info = {0,0,0};
    FIL fil;
    char filename[128];
    snprintf(filename, sizeof(filename), "%s", path);
    FRESULT fr = f_open(&fil, filename, FA_READ);
    
    // read WAV file header before reading data
    uint br; // bytes read
    uint dataChunksProcessed = 0;
    char descriptorBuffer[4];
    uint8_t sizeBuffer[4];
    uint8_t inputBuffer[48]; // needs to handle mono/stereo, 16/24/32 bits, i.e. 2,3,4,6 or 8 bytes per sample frame, and also needs to be big enough for entire format chunk (sometimes 40 bytes?)
    uint8_t outputBuffer[FLASH_PAGE_SIZE];
    bool isStereo = false;
    uint32_t outputBufferPos = 0;
    uint32_t dataPagesWritten = 0;

    // first 3 4-byte sections should be "RIFF", file size, "WAVE"
    fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
    if(strncmp(descriptorBuffer, "RIFF", 4) != 0) {
        f_close(&fil);
        return info;
    }
    fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
    uint32_t fileSize;
    memcpy(&fileSize, descriptorBuffer, 4);
    fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
    if(strncmp(descriptorBuffer, "WAVE", 4) != 0) {
        f_close(&fil);
        return info;
    }

    uint16_t fmtChannels;
    uint32_t fmtRate;
    uint16_t fmtBitsPerSample;
    uint16_t fmtFormatType;
    bool isDataChunk = false;
    while (!isDataChunk)
    {
        // after the first three required sections, WAV files have a series of chunks of the form: <chunk ID (4 bytes), chunk size (4 bytes), chunk data (chunk size bytes)>
        fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
        fr = f_read(&fil, sizeBuffer, sizeof sizeBuffer, &br);
        uint32_t chunkSize;
        memcpy(&chunkSize, sizeBuffer, 4);
        uint bytesToRead = std::min(sizeof inputBuffer, (uint)chunkSize);
        uint brTotal = 0; // bytes read so far from this chunk
        bool isFormatChunk = strncmp(descriptorBuffer, "fmt ", 4) == 0;
        isDataChunk = strncmp(descriptorBuffer, "data", 4) == 0;
        if(isDataChunk) {
            info.lengthBytes = 2 * chunkSize / ((fmtBitsPerSample / 8) * fmtChannels);
        }
        for (;;)
        {
            fr = f_read(&fil, inputBuffer, bytesToRead, &br);

            if(isFormatChunk) {
                memcpy(&fmtFormatType, &inputBuffer[0], 2);

                memcpy(&fmtChannels, &inputBuffer[2], 2);
                memcpy(&fmtRate, &inputBuffer[4], 4);
                memcpy(&fmtBitsPerSample, &inputBuffer[14], 2);
                info.flashAddress = flashPageStart * FLASH_PAGE_SIZE;
                info.sampleRate = fmtRate;
            }

            if (br == 0)
                break;

            if(isDataChunk) {
                if(writeToFlash) {
                    uint8_t bytesPerSampleFrame = (fmtBitsPerSample / 8) * fmtChannels;
                    for(uint i=0; i<br; i+=bytesPerSampleFrame) {
                        int16_t thisSample = 0;
                        int32_t summedSample = 0;
                        for(uint j=0; j<fmtChannels; j++) {
                            if(fmtBitsPerSample == 16) {
                                // 16 bits, same as output, just copy source
                                memcpy(&thisSample, &inputBuffer[i + j * 2], 2);
                            } else if(fmtBitsPerSample == 24) {
                                // 24 bits, copy to 32-bit first then remove extra precision
                                int32_t thisSample24 = 0;
                                memcpy(&thisSample24, &inputBuffer[i + j * 3], 3);
                                thisSample = thisSample24 >> 8;
                            } else if(fmtBitsPerSample == 32) {
                                // 32-bit float, read float then convert to int
                                float thisSampleFloat = 0.0f;
                                memcpy(&thisSampleFloat, &inputBuffer[i + j * 4], 4);
                                thisSample = (int16_t)(thisSampleFloat * 32767.0f);
                            }
                            summedSample += thisSample;
                        }
                        summedSample /= fmtChannels; // summing to mono, could do this slightly better, think i'm currently losing one bit of resolution on stereo samples
                        outputBuffer[outputBufferPos] = summedSample & 0xFF;
                        outputBuffer[outputBufferPos + 1] = (summedSample >> 8) & 0xFF;
                        outputBufferPos += 2;
                        if(outputBufferPos >= FLASH_PAGE_SIZE) {
                            _memory->writeToFlashPage(flashPageStart + dataPagesWritten, outputBuffer);
                            dataPagesWritten++;
                            outputBufferPos = 0;
                        }
                    }
                }
            }
            brTotal += br;
            if (brTotal >= chunkSize)
            {
                // double check next line at some point, AI wanted the outputBufferPos check
                if(isDataChunk && writeToFlash && outputBufferPos > 0) {
                    // handling leftover bytes here
                    _memory->writeToFlashPage(flashPageStart + dataPagesWritten, outputBuffer);
                    dataPagesWritten++;
                }
                printf("\n");
                if(chunkSize % 2 != 0) {
                    // skip padding byte at end of chunk if chunk size is odd
                    f_lseek(&fil, fil.fptr + 1);
                }
                break;
            }
        }
    }
    return info;
}

void CardReader::scanSampleFolders() {
    sd_init_driver();
    
    if(!checkCardInserted()) {
        return;
    }

    if(!mountCard()) {
        return;
    }

    _numFolders = 0;
    // reset folder names
    for(int i=0; i<MAX_FOLDERS; i++) {
        _folderNames[i][0] = '\0';
    }
    char samplePath[] = "samples/";
    
    DIR dir;
    FRESULT fr = f_opendir(&dir, samplePath);
    if (FR_OK != fr)
    {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    printf("Scanning for sample folders...\n");

    for(;;)
    {
        FILINFO fno;
        fr = f_readdir(&dir, &fno);
        if (FR_OK != fr || fno.fname[0] == 0)
            break; // end of directory (or error?)
        if (fno.fattrib & AM_DIR)
        {
            printf("Found folder: %s\n", fno.fname);
            if(_numFolders < MAX_FOLDERS) {
                strncpy(_folderNames[_numFolders], fno.fname, MAX_FOLDER_NAME_LENGTH-1);
                _folderNames[_numFolders][MAX_FOLDER_NAME_LENGTH-1] = '\0'; // ensure null termination
                _numFolders++;
            } else {
                printf("Max folders reached, skipping folder: %s\n", fno.fname);
            }
        }
    }

    f_unmount("");

}

int CardReader::getKitSize(uint8_t kitIndex) {
    if(kitIndex < _numFolders) {
        const char* folderName = getSampleFolderName(kitIndex);
        // check cumulative sample size required for kit
        uint totalSampleSizeBytes = 0;

        // initial setup
        sd_init_driver();
        if(!checkCardInserted()) {
            return -1;
        }
        if(!mountCard()) {
            return -1;
        }

        bool exitFindLoop = false;
        for (int i = 1; i <= MAX_CHANNELS && !exitFindLoop; i++) {
            char path[128];
            snprintf(path, sizeof(path), "samples/%s/%d.wav", folderName, i);
            printf("Checking for sample at path: %s\n", path);
            SampleInfo info = parseWavFile(path);
            if(info.lengthBytes > 0) {
                totalSampleSizeBytes += info.lengthBytes;
            } else {
                printf("No sample found at path: %s\n", path);
                exitFindLoop = true;
            }
        }

        return totalSampleSizeBytes;
    } else {
        printf("Invalid kit index: %d\n", kitIndex);
        return -1;
    }
}