#include <cstdint>
#include "constants.h"

// Borrowing some useful Arduino macros (again)
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

class Beat {
    public:
        static const int tupletMap[NUM_TUPLET_MODES][QUARTER_NOTE_STEPS_SEQUENCEABLE];
        uint64_t beatData[NUM_SAMPLES];
        bool getHit(uint8_t sample, uint16_t step, int tuplet) {
            bool isHit = false;
            if(step % 4 == 0)
            {
                int reducedStep = step >> 2;
                int thisQuarterNote = 8 * (reducedStep / 8);
                int adjustedStep = thisQuarterNote + Beat::tupletMap[tuplet][reducedStep % 8];
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

// defines which step a hit refers to in different triplet modes
// e.g. there are 8 available steps (0-7) per quarter note; in straight mode (0th array), this is a direct mapping, but in triplet mode (1st array), element 3 of the array has the value 4 (not 3), so instead of returning hit 3, we should return hit 4 (because it will line up better with the way the beat was intended..?)
const int Beat::tupletMap[NUM_TUPLET_MODES][QUARTER_NOTE_STEPS_SEQUENCEABLE] = {
    {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 1, 2, 4, 5, 6, 7, 7},
    {0, 2, 4, 5, 7, 7, 7, 7},
    {0, 1, 2, 3, 4, 6, 7, 7}};