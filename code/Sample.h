#include "constants.h"
#include <stdio.h>

class Sample {
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
            int64_t fadeTime;
            int64_t fadeStart;
            int64_t fadeEnd;
        };
        Hit queuedHits[HIT_QUEUE_SIZE];
        uint8_t hitQueueIndex = 0;

        bool playing = false;
        bool waiting = false;
        int velocity = 255;
        int nextVelocity = 255;
        int32_t value = 0;
        int position = 0;
        int positionAccurate = 0;
        int delaySamples = 0;
        uint length = 0;
        uint startPosition = 0;
        bool output1 = true;
        bool output2 = true;

        int64_t nextHitTime = INT64_MAX;
        uint8_t nextHitIndex = 0;
        uint8_t currentHitIndex = 0;
        bool update(int64_t time) {
            bool didTrigger = false;
            if(nextHitTime <= time) {
                if(queuedHits[nextHitIndex].waiting) {
                    didTrigger = true;
                    queuedHits[nextHitIndex].waiting = false;
                    velocity = queuedHits[nextHitIndex].velocity;
                    currentHitIndex = nextHitIndex;
                    nextHitIndex = (nextHitIndex + 1) % HIT_QUEUE_SIZE;
                    if(queuedHits[nextHitIndex].waiting) {
                        nextHitTime = queuedHits[nextHitIndex].time;
                    } else {
                        nextHitTime = INT64_MAX;
                    }
                    positionAccurate = Sample::pitch >= 0 ? 0 : length << LERP_BITS;
                    position = Sample::pitch >= 0 ? 0 : length;
                    playing = true;
                }
            }
            if(playing) {
                int y1 = sampleData[position + startPosition];
                int y2 = sampleData[position + 1 + startPosition]; // might have a fencepost error thingy here
                value = y1 + ((y2 - y1) * (positionAccurate - (position << LERP_BITS))) / (1 << LERP_BITS);
                value = (value * velocity) >> 12;

                // handle fade out if required
                if(queuedHits[currentHitIndex].fadeTime > 0) {
                    if(time > queuedHits[currentHitIndex].fadeStart) {
                        int64_t deltaT = queuedHits[currentHitIndex].fadeEnd - time;
                        int64_t fadeVel = (deltaT << 12) / queuedHits[currentHitIndex].fadeTime;
                        value = (value * fadeVel) >> 12;
                    }
                }

                positionAccurate += Sample::pitch;
                position = positionAccurate >> LERP_BITS;
                if(pitch > 0) {
                    // playing forwards
                    if (position >= length)
                    {
                        playing = false;
                        value = 0;
                        position = length;
                        positionAccurate = length << LERP_BITS;
                    }
                } else {
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
            return didTrigger;
        }
        void queueHit(int64_t hitTime, int16_t hitStep, int16_t hitVelocity) {

            /*if(pitch>0) {
                queuedHits[hitQueueIndex].time = hitTime;
            } else {
                queuedHits[hitQueueIndex].time = hitTime + (static_cast<int32_t>(length) << LERP_BITS) / pitch;
            }*/
            queuedHits[hitQueueIndex].time = hitTime;
            queuedHits[hitQueueIndex].step = hitStep;
            queuedHits[hitQueueIndex].velocity = hitVelocity;
            queuedHits[hitQueueIndex].waiting = true;
            queuedHits[hitQueueIndex].fadeTime = 0;
            queuedHits[hitQueueIndex].fadeStart = INT64_MAX;
            queuedHits[hitQueueIndex].fadeEnd = INT64_MAX;

            // calculate fade start/end times
            uint8_t prevIndex = (hitQueueIndex + HIT_QUEUE_SIZE - 1) % HIT_QUEUE_SIZE;
            int64_t deltaT = queuedHits[hitQueueIndex].time - queuedHits[prevIndex].time;
            if (deltaT < (static_cast<uint32_t>(length) << LERP_BITS) / pitch)
            {
                // fade out required (really these times should be recalculated at the start of each buffer window in case of pitch changes)
                int16_t fadeTime = std::min((int)deltaT, FADE_OUT);
                queuedHits[prevIndex].fadeTime = fadeTime;
                queuedHits[prevIndex].fadeStart = hitTime - fadeTime;
                queuedHits[prevIndex].fadeEnd = hitTime;
            }

            // increment index for next time
            hitQueueIndex = (hitQueueIndex + 1) % HIT_QUEUE_SIZE;

            // this can be done more efficiently, but here's a first attempt:
            nextHitTime = INT64_MAX;
            for(int i=0; i<HIT_QUEUE_SIZE; i++) {
                if(queuedHits[i].waiting) {
                    if(queuedHits[i].time < nextHitTime) {
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