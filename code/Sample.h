#include "constants.h"
#include <stdio.h>

class Sample {
    public:
        static int16_t sampleData[MAX_SAMPLE_STORAGE];

        struct Hit
        {
            int64_t time;
            int16_t step;
            int16_t velocity;
            bool waiting;
        };

        int32_t value = 0;
        bool playing = false;
        int position = 0;
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
                    nextHitIndex = (nextHitIndex + 1) % HIT_QUEUE_SIZE;
                    if (queuedHits[nextHitIndex].waiting)
                    {
                        nextHitTime = queuedHits[nextHitIndex].time;
                    }
                    else
                    {
                        nextHitTime = INT64_MAX;
                    }
                    position = 0;
                    playing = true;
                }
            }
            if (playing)
            {
                value = sampleData[position + startPosition];
                position++;
                if (position >= length)
                {
                    playing = false;
                    value = 0;
                    position = length;
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

int16_t Sample::sampleData[MAX_SAMPLE_STORAGE];