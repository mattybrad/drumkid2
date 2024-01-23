#include <cstdint>
#include "constants.h"

// Borrowing some useful Arduino macros (again)
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

class Beat {
    public:
        // should be static, but don't know how to do that!
        int tupletMap[8][8] = {
            {0,1,2,3,4,5,6,7},
            {0,1,2,3,4,5,6,7},
            {0,1,2,4,5,6,7,7},
            {0,1,2,4,5,6,7,7},
            {0,2,4,5,7,7,7,7},
            {0,2,4,5,7,7,7,7},
            {0,1,2,3,4,6,7,7},
            {0,1,2,3,4,6,7,7}
        };
        uint64_t beatData[NUM_SAMPLES];
        bool getHit(uint8_t sample, uint16_t step, int tuplet) {
            bool isHit = false;
            if(step % 4 == 0)
            {
                int reducedStep = step >> 2;
                int thisQuarterNote = 8 * (reducedStep / 8);
                int adjustedStep = thisQuarterNote + tupletMap[tuplet][reducedStep % 8];
                isHit = bitRead(beatData[sample], adjustedStep);
            }
            return isHit;
        }
        Beat() {
            for(int i=0; i<NUM_SAMPLES; i++) {
                beatData[i] = 0;
            }
        }
};

static constexpr int tupletMap[7][8] = {
    {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 1, 2, 4, 5, 6, 7, 7},
    {0, 1, 2, 4, 5, 6, 7, 7},
    {0, 1, 2, 4, 5, 7, 7, 7},
    {0, 1, 2, 4, 5, 7, 7, 7},
    {0, 1, 2, 3, 4, 6, 7, 7}};