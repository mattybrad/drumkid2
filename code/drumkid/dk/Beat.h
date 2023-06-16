#include <cstdint>

// maybe not a great use of space to store so many bools? Temp...

class Beat {
    public:
        bool hits[3][32];
        void addHit(uint8_t channel, uint8_t step) {
            hits[channel][step] = true;
        }
        Beat() {
            for(int i=0; i<3; i++) {
                for(int j=0; j<32; j++) {
                    hits[i][j] = false;
                }
            }
        }
};