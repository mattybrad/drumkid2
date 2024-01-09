#include "constants.h"

class Sample {
    public:
        float speed = 1.0;
        float position = 0.0;
        float velocity = 1.0;
        uint delaySamples = 0;
        int length = MAX_SAMPLE_LENGTH;
        int value = 0;
        int16_t sampleData[MAX_SAMPLE_LENGTH];
        void update() {
            int intPosition = (int) position; // naiive function, no lerping, temporary
            if(delaySamples > 0) {
                delaySamples --;
            } else {
                if(intPosition < length) {
                    if(velocity == 1.0) value = sampleData[intPosition];
                    else value = sampleData[intPosition] * velocity;
                } else {
                    value = 0;
                }
                position += speed;
            }
        }
};