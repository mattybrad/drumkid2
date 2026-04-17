#include "ChannelManager.h"

void ChannelManager::init() {
    for(int i = 0; i < MAX_CHANNELS; i++) {
        channels[i].init();
    }
}

void ChannelManager::triggerChannel(uint8_t channelNum, uint8_t velocity) {
    if(channelNum < numChannels) {
        channels[channelNum].samplePosition = 0;
        channels[channelNum].samplePositionFP = 0;
        channels[channelNum].velocity = velocity;
    }
}