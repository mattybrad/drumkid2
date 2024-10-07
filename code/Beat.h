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
        void removeHit(uint8_t sample, uint16_t step)
        {
            for (int i = 0; i < numHits; i++)
            {
                if (hits[i].sample == sample && hits[i].step == step)
                {
                    for(int j=i; j<numHits; j++) {
                        // overwrite target hit by shifting other hits back by one
                        // should probably do this with memcpy or something
                        if(j+1<numHits) {
                            hits[j].sample = hits[j+1].sample;
                            hits[j].step = hits[j+1].step;
                            hits[j].velocity = hits[j+1].velocity;
                            hits[j].probability = hits[j+1].probability;
                            hits[j].group = hits[j+1].group;
                        } else {
                            hits[j].sample = 255;
                        }
                    }
                    numHits --;
                }
            }
        }
        Beat() {
            for(int i=0; i<MAX_BEAT_HITS; i++) {
                hits[i].sample = 255; // this helps identify which hits are not in use when loading beats from flash, where numHits is not explicitly set/known
            }
        }
};