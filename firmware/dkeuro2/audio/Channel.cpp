#include "Channel.h"

void Channel::init() {
    samplePosition = 0;
    samplePositionFP = 0;
    playbackSpeedFP = 
(int64_t)(2.00 * (1LL << 32));
}