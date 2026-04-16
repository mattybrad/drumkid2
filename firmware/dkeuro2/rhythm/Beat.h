#pragma once

#include <cstdint>

class Beat {
    public:
        void init();
        struct Hit {
            uint16_t position;
            uint8_t channel; // which channel to trigger
            uint8_t velocity; // how hard to trigger (0-127)
        };
        Hit hits[64]; // array of hits within the beat
        uint8_t numHits; // number of hits currently in the beat
        bool hitAvailable(uint16_t position); // check if a hit is available at the given position
        Hit getNextHit();

    private:
        uint16_t _thisHitIndex = 0; // index of the hit currently being processed
        uint16_t _nextHitIndex = 0; // index of the next hit to check for availability
        
};