#include <cstdint>
#include "constants.h"

class Beat {
    public:
        struct Hit
        {
            uint8_t sample;
            uint16_t step;
            uint8_t velocity;
            uint8_t probability;
            uint8_t group;
        };
        Hit hits[MAX_BEAT_HITS];
        uint8_t numHits = 0;
        void addHit(uint8_t sample, uint16_t step, uint8_t velocity, uint8_t probability, uint8_t group)
        {
            if(numHits < MAX_BEAT_HITS) {
                hits[numHits].sample = sample;
                hits[numHits].step = step;
                hits[numHits].velocity = velocity;
                hits[numHits].probability = probability;
                hits[numHits].group = group;
            }
            numHits ++;
        }
        int getHit(uint8_t sample, uint16_t step)
        {
            for(int i=0; i<numHits; i++) {
                if(hits[i].sample == sample && hits[i].step == step) {
                    return i;
                }
            }
            return -1;
        }
        Beat() {
            for(int i=0; i<MAX_BEAT_HITS; i++) {
                hits[i].sample = 255; // this helps identify which hits are not in use when loading beats from flash, where numHits is not explicitly set/known
            }
        }
};