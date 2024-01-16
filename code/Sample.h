#include "constants.h"

class Sample {
    public:
        float velocity = 0.5;
        bool playing = false;
        int16_t value = 0;
        int position = 0;
        uint length = MAX_SAMPLE_LENGTH;
        int16_t sampleData[MAX_SAMPLE_LENGTH];
        void trigger() {
            position = 0;
            playing = true;
        }
        void update() {
            if(playing) {
                value = sampleData[position];
                position ++;
                if(position >= length) {
                    playing = false;
                    value = 0;
                }
            }
        }
};