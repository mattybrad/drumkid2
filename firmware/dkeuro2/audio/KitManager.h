#pragma once
#include "audio/Kit.h"
#include "audio/ChannelManager.h"
#include "hardware/Memory.h"
#include "Config.h"
#include <cstdint>
#include <stdio.h>

class KitManager {
    public:
        void init(Memory* memory, ChannelManager* channelManager);
        void reloadMetaData();
        void initKit(uint8_t newKitNum);
        void createSpaceForKit(uint8_t kitSlot, uint32_t kitSizeBytes);
        uint8_t kitNum = 0;
        Kit kits[MAX_KITS];
        uint32_t getFreeSectors(uint8_t kitSlot);

    private:
        Memory* _memory;
        ChannelManager* _channelManager;

};