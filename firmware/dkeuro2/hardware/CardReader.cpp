#include "CardReader.h"

void CardReader::init() {
    // temporary testing code
    char path[] = "samples/";
    int numSampleFolders = 0;
    printf("scan sample folders...\n");
    sd_init_driver();
    sd_card_t *pSD = sd_get_by_num(0);
    if (!pSD->sd_test_com(pSD))
    {
        printf("SD card not detected!\n");
        return;
    }
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr)
    {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    static FILINFO fno;
    int foundNum = -1;
    DIR dir;
    fr = f_opendir(&dir, path);
    if (FR_OK != fr)
    {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    for (;;)
    {
        fr = f_readdir(&dir, &fno); /* Read a directory item */
        if (fr != FR_OK || fno.fname[0] == 0)
            break; /* Break on error or end of dir */
        if (fno.fattrib & AM_DIR)
        { /* It is a directory */
            foundNum++;
            printf("Found sample folder: %s\n", fno.fname);
        }
    }
    numSampleFolders = foundNum + 1;

    f_closedir(&dir);
}