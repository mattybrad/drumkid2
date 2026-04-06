#pragma once
#include <cstdint>
#include "Config.h"
#include "audio/Channel.h"

class ChannelManager {
    public:
        void init();
        Channel channels[MAX_CHANNELS];
        uint8_t numChannels = 0;

    private:

};