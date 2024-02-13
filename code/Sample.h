#include "constants.h"
#include <stdio.h>

class Sample {
    private:
        float fadeOut = 1.0;
    public:
        static int LERP_BITS;
        static int16_t sampleData[MAX_SAMPLE_STORAGE];
        static int pitch;
        bool playing = false;
        bool waiting = false;
        float velocity = 1.0;
        float nextVelocity = 1.0;
        int16_t value = 0;
        int position = 0;
        int positionAccurate = 0;
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
                    positionAccurate = Sample::pitch >= 0 ? 0 : length << LERP_BITS;
                    position = Sample::pitch >= 0 ? 0 : length;
                    playing = true;
                    waiting = false;
                } else if(playing && delaySamples < 250) {
                    doFade = true;
                    fadeOut = delaySamples / 250.0; // to do: remove floats from here
                }
            }
            if(playing) {

                // lerp, not available natively because of old C++ version...
                int y1 = sampleData[position + startPosition];
                int y2 = sampleData[position + 1 + startPosition];
                value = y1 + ((y2-y1) * (positionAccurate - (position << LERP_BITS))) / (1<<LERP_BITS);
                value *= velocity; // temp, should be int not float velocity

                //if(doFade) value *= fadeOut; // temporarily disable fade out while figuring out reverse
                positionAccurate += Sample::pitch;
                position = positionAccurate >> LERP_BITS;
                if(Sample::pitch > 0) {
                    // playing forwards
                    if (position >= length)
                    {
                        position = length;
                        positionAccurate = length << LERP_BITS;
                        playing = false;
                        value = 0;
                    }
                } else {
                    // playing reverse
                    if (position <= 0)
                    {
                        position = 0;
                        positionAccurate = 0;
                        playing = false;
                        value = 0;
                    }
                }
            }
        }
};

int Sample::LERP_BITS = 10;
int16_t Sample::sampleData[MAX_SAMPLE_STORAGE];
int Sample::pitch = 1<<LERP_BITS;