#include "constants.h"

class Sample {
    private:
        float fadeOut = 1.0;
    public:
        static int16_t sampleData[MAX_SAMPLE_STORAGE];
        bool playing = false;
        bool waiting = false;
        float velocity = 1.0;
        float nextVelocity = 1.0;
        int16_t value = 0;
        int position = 0;
        int delaySamples = 0;
        uint length = 0;
        uint startPosition = 0;
        bool output1 = true;
        bool output2 = true;
        void update() {
            bool doFade = false;
            if (delaySamples > 0)
            {
                delaySamples--;
                if (delaySamples == 0)
                {
                    velocity = nextVelocity;
                    position = 0;
                    playing = true;
                    waiting = false;
                } else if(playing && delaySamples < 250) {
                    doFade = true;
                    fadeOut = delaySamples / 250.0;
                }
            }
            if(playing) {

                value = velocity * sampleData[position + startPosition]; // combining ints and floats but i think it's fine..?
                if(doFade) value *= fadeOut;
                position ++;
                if(position >= length) {
                    playing = false;
                    value = 0;
                }
            }
        }
};

int16_t Sample::sampleData[MAX_SAMPLE_STORAGE];