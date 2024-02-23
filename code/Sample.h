#include "constants.h"
#include <stdio.h>

class Sample {
    private:
        float fadeOut = 1.0;
    public:
        static int LERP_BITS;
        static int16_t sampleData[MAX_SAMPLE_STORAGE];
        static int pitch;

        // new stuff 16/2/24
        struct Hit {
            int64_t time;
            int16_t step;
            int16_t velocity;
            bool waiting;
        };
        Hit queuedHits[HIT_QUEUE_SIZE];
        uint8_t hitQueueIndex = 0;

        bool playing = false;
        bool waiting = false;
        int velocity = 255;
        int nextVelocity = 255;
        int16_t value = 0;
        int position = 0;
        int positionAccurate = 0;
        int delaySamples = 0;
        uint length = 0;
        uint startPosition = 0;
        bool output1 = true;
        bool output2 = true;

        int64_t nextHitTime = INT64_MAX;
        uint8_t nextHitIndex = 0;
        void update(int64_t time) {
            if(nextHitTime <= time) {
                if(queuedHits[nextHitIndex].waiting) {
                    queuedHits[nextHitIndex].waiting = false;
                    velocity = queuedHits[nextHitIndex].velocity;
                    nextHitIndex = (nextHitIndex + 1) % HIT_QUEUE_SIZE;
                    if(queuedHits[nextHitIndex].waiting) {
                        nextHitTime = queuedHits[nextHitIndex].time;
                    } else {
                        nextHitTime = INT64_MAX;
                    }
                    position = 0;
                    playing = true;
                    printf("HIT\n");
                }
            }
            if(playing) {
                value = sampleData[position + startPosition];
                position ++;
                if(position >= length) {
                    playing = false;
                    value = 0;
                }
            }
        }
        void updateOld() {
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
                    //fadeOut = delaySamples / 250.0; // to do: remove floats from here
                }
            }
            if(playing) {

                // lerp, not available natively because of old C++ version...
                int y1 = sampleData[position + startPosition];
                int y2 = sampleData[position + 1 + startPosition];
                value = y1 + ((y2-y1) * (positionAccurate - (position << LERP_BITS))) / (1<<LERP_BITS);
                //value *= velocity; // temp, should be int not float velocity
                value = (value * velocity) >> 12;

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
        void queueHit(int64_t hitTime, int16_t hitStep, int16_t hitVelocity) {
            printf("ATTEMPT QUEUE\n");

            queuedHits[hitQueueIndex].time = hitTime;
            queuedHits[hitQueueIndex].step = hitStep;
            queuedHits[hitQueueIndex].velocity = hitVelocity;
            queuedHits[hitQueueIndex].waiting = true;
            hitQueueIndex = (hitQueueIndex + 1) % HIT_QUEUE_SIZE;

            // this can be done more efficiently, but here's a first attempt:
            nextHitTime = INT64_MAX;
            for(int i=0; i<HIT_QUEUE_SIZE; i++) {
                if(queuedHits[i].waiting) {
                    if(queuedHits[i].time < nextHitTime) {
                        printf("queue!\n");
                        nextHitTime = queuedHits[i].time;
                        nextHitIndex = i;
                    }
                }
            }
        }
};

int Sample::LERP_BITS = 10;
int16_t Sample::sampleData[MAX_SAMPLE_STORAGE];
int Sample::pitch = 1<<LERP_BITS;