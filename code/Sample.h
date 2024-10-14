#include "constants.h"
#include <stdio.h>

class Sample {
    public:
        static int16_t sampleData[MAX_SAMPLE_STORAGE];
        static int pitch;
        static uint crop;
        static int LERP_BITS;

        struct Hit
        {
            int64_t time;
            int16_t step;
            int16_t velocity;
            bool waiting;
        };

        int32_t value = 0;
        bool playing = false;
        int velocity = 4095; // 12-bit resolution (for now)
        int position = 0;
        int positionAccurate = 0; // position at sub-sample resolution to allow playing samples at different pitches
        bool output1 = true;
        bool output2 = true;
        uint length = 0;
        uint startPosition = 0;

        Hit queuedHits[HIT_QUEUE_SIZE];
        uint8_t hitQueueIndex = 0;
        int64_t nextHitTime = INT64_MAX;
        uint8_t nextHitIndex = 0;
        uint8_t currentHitIndex = 0;

        bool update(int64_t time) {
            bool didTrigger = false;
            if (nextHitTime <= time)
            {
                if (queuedHits[nextHitIndex].waiting)
                {
                    didTrigger = true;
                    queuedHits[nextHitIndex].waiting = false;
                    currentHitIndex = nextHitIndex;
                    velocity = queuedHits[nextHitIndex].velocity;
                    nextHitIndex = (nextHitIndex + 1) % HIT_QUEUE_SIZE;
                    if (queuedHits[nextHitIndex].waiting)
                    {
                        nextHitTime = queuedHits[nextHitIndex].time;
                    }
                    else
                    {
                        nextHitTime = INT64_MAX;
                    }
                    positionAccurate = Sample::pitch >= 0 ? 0 : std::min(length, Sample::crop) << LERP_BITS;
                    position = Sample::pitch >= 0 ? 0 : std::min(length, Sample::crop);
                    playing = true;
                }
            }
            if (playing)
            {
                int y1 = sampleData[position + startPosition];
                int y2 = sampleData[position + 1 + startPosition]; // might have a fencepost error thingy here
                value = y1 + ((y2 - y1) * (positionAccurate - (position << LERP_BITS))) / (1 << LERP_BITS);
                value = (value * velocity) >> 8;

                positionAccurate += Sample::pitch;
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
            queuedHits[hitQueueIndex].time = hitTime;
            queuedHits[hitQueueIndex].step = hitStep;
            queuedHits[hitQueueIndex].velocity = hitVelocity;
            queuedHits[hitQueueIndex].waiting = true;

            // increment index for next time
            hitQueueIndex = (hitQueueIndex + 1) % HIT_QUEUE_SIZE;

            // this can be done more efficiently, but here's a first attempt:
            nextHitTime = INT64_MAX;
            for (int i = 0; i < HIT_QUEUE_SIZE; i++)
            {
                if (queuedHits[i].waiting)
                {
                    if (queuedHits[i].time < nextHitTime)
                    {
                        nextHitTime = queuedHits[i].time;
                        nextHitIndex = i;
                    }
                }
            }
        }
};

int Sample::LERP_BITS = 10;
int16_t Sample::sampleData[MAX_SAMPLE_STORAGE];
int Sample::pitch = 1 << LERP_BITS;
uint Sample::crop = MAX_SAMPLE_STORAGE;