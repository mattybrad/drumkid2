#include "constants.h"
#include <stdio.h>

class Sample {
    public:
        static int16_t sampleData[MAX_SAMPLE_STORAGE];
        static int pitch;
        static uint crop;
        static int LERP_BITS;

        int32_t value = 0;
        bool playing = false;
        int velocity = 0; // 12-bit resolution (for now)
        int position = 0;
        int positionAccurate = 0; // position at sub-sample resolution to allow playing samples at different pitches
        bool output1 = true;
        bool output2 = true;
        uint length = 0;
        uint startPosition = 0;
        uint sampleRate = 44100;
        uint sampleRateAdjustment = 0;

        int64_t nextHitTime = INT64_MAX;
        uint8_t nextHitIndex = 0;
        uint8_t currentHitIndex = 0;
        bool waiting = false;

        int tailVelocity = 0;
        int tailPosition = 0;
        int tailPositionAccurate = 0;

        bool update(int64_t time) {
            bool didTrigger = false;
            if (nextHitTime <= time)
            {
                if (waiting)
                {
                    didTrigger = true;
                    waiting = false;
                    nextHitTime = INT64_MAX;
                    positionAccurate = Sample::pitch >= 0 ? 0 : std::min(length, Sample::crop) << LERP_BITS;
                    position = Sample::pitch >= 0 ? 0 : std::min(length, Sample::crop);
                    playing = true;
                }
            }
            if (playing)
            {
                // could maybe add a special case for 100% pitch to speed things up

                int y1 = sampleData[position + startPosition];
                int y2 = sampleData[position + 1 + startPosition]; // might have a fencepost error thingy here, perhaps at very end of sample playback
                value = y1 + ((y2 - y1) * (positionAccurate - (position << LERP_BITS))) / (1 << LERP_BITS);
                value = (value * velocity) >> 8;

                if(tailVelocity > 0 && tailPosition < length) {
                    y1 = sampleData[tailPosition + startPosition];
                    y2 = sampleData[tailPosition + 1 + startPosition];
                    value += ((y1 + ((y2 - y1) * (tailPositionAccurate - (tailPosition << LERP_BITS))) / (1 << LERP_BITS)) * tailVelocity) >> 8;
                    tailVelocity -= 4;
                    if(tailVelocity < 0) tailVelocity = 0;

                    tailPositionAccurate += Sample::pitch >> sampleRateAdjustment; // might be more efficient to do the sample rate calculation when pitch is changed
                    tailPosition = tailPositionAccurate >> LERP_BITS;
                }

                positionAccurate += Sample::pitch >> sampleRateAdjustment; // might be more efficient to do the sample rate calculation when pitch is changed
                position = positionAccurate >> LERP_BITS;

                if (pitch > 0)
                {
                    // playing forwards
                    if (position >= length || position >= Sample::crop) // temp crop test
                    {
                        playing = false;
                        value = 0;
                        position = length;
                        positionAccurate = length << LERP_BITS;
                    }
                }
                else
                {
                    // playing backwards
                    if (position <= 0)
                    {
                        playing = false;
                        value = 0;
                        position = 0;
                        positionAccurate = 0;
                    }
                }
            }
            return true;
        }
        void queueHit(int64_t hitTime, int16_t hitStep, int16_t hitVelocity) {
            nextHitTime = hitTime;
            if (playing)
            {
                // retrigger
                tailPositionAccurate = positionAccurate;
                tailPosition = position;
                tailVelocity = velocity;
            }
            velocity = hitVelocity;
            waiting = true;
        }
};

int Sample::LERP_BITS = 10;
int16_t Sample::sampleData[MAX_SAMPLE_STORAGE];
int Sample::pitch = 1 << LERP_BITS;
uint Sample::crop = MAX_SAMPLE_STORAGE;