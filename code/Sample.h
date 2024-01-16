#include "constants.h"

class Sample {
    public:
        float velocity = 1.0;
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
                value = velocity * sampleData[position]; // combining ints and floats but i think it's fine..?
                position ++;
                if(position >= length) {
                    playing = false;
                    value = 0;
                }
            }
        }
};