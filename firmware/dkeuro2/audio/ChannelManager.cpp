#include "ChannelManager.h"

void ChannelManager::init() {
    for(int i = 0; i < MAX_CHANNELS; i++) {
        channels[i].init();
    }
}