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
        uint8_t kitNum = 0;
        Kit kits[MAX_KITS];
        uint32_t getFreeSectors();

    private:
        Memory* _memory;
        ChannelManager* _channelManager;

};