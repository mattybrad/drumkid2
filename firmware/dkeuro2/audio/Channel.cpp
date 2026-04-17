#include "Channel.h"

void Channel::init() {
    samplePosition = 0;
    samplePositionFP = 0;
    playbackSpeedFP = 
(int64_t)(1.00 * (1LL << 32));
    velocity = 0;
}