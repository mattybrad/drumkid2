#include "Channel.h"

void Channel::init() {
    samplePosition = 0;
    samplePositionFP = 0;
    playbackSpeed = 
(int64_t)(3.14 * (1LL << 32));
}