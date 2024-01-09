#include <cstdint>
#include "constants.h"

// maybe not a great use of space to store so many bools? Could improve by 8x presumably. Therefore temp...

class Beat {
    public:
        bool hits[NUM_SAMPLES][MAX_BEAT_STEPS];
        void addHit(uint8_t channel, uint8_t step) {
            hits[channel][step] = true;
        }
        Beat() {
            for(int i=0; i<NUM_SAMPLES; i++) {
                for(int j=0; j<MAX_BEAT_STEPS; j++) {
                    hits[i][j] = false;
                }
            }
        }
};