#include "ChannelManager.h"

void ChannelManager::init() {
    for(int i = 0; i < MAX_CHANNELS; i++) {
        channels[i].init();
    }
}

void ChannelManager::triggerChannel(uint8_t channelNum, uint8_t velocity) {
    if(channelNum < numChannels) {
        channels[channelNum].samplePosition = channels[channelNum].playbackSpeedFP >= 0 ? 0 : channels[channelNum].sampleLength - 1;
        channels[channelNum].samplePositionFP = (int64_t)channels[channelNum].samplePosition << 32;
        channels[channelNum].velocity = velocity;
    }
}