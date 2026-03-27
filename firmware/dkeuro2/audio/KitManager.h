#pragma once
#include "audio/Kit.h"
#include "audio/Channel.h"
#include "hardware/Memory.h"
#include "Config.h"
#include <cstdint>
#include <stdio.h>

class KitManager {
    public:
        void init(Memory* memory);
        uint8_t kitNum = 0;
        Kit kits[MAX_KITS];
        //void loadKitFromCard(uint16_t folderIndex, uint16_t kitSlot);
        uint32_t getFreeSectors();

    private:
        Memory* _memory;

};