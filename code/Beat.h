#include <cstdint>
#include "constants.h"

class Beat {
    public:
        struct Hit
        {
            uint8_t sample;
            uint16_t step;
            uint8_t velocity;
        };
        Hit hits[MAX_BEAT_HITS];
        uint8_t numHits = 0;
        void addHit(uint8_t sample, uint16_t step, uint8_t velocity)
        {
            if(numHits < MAX_BEAT_HITS) {
                hits[numHits].sample = sample;
                hits[numHits].step = step;
                hits[numHits].velocity = velocity;
            }
            numHits ++;
        }
        bool getHit(uint8_t sample, uint16_t step)
        {
            bool isHit = false;
            for(int i=0; i<numHits; i++) {
                if(hits[i].sample == sample && hits[i].step == step) {
                    isHit = true;
                }
            }
            return isHit;
        }
        Beat() {
            
        }
};