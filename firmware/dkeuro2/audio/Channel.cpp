#include "Channel.h"

void Channel::init() {
    samplePosition = 0;
    samplePositionFP = 0;
    playbackSpeed = 
(int64_t)(1.03 * (1LL << 32));
}