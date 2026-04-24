#include "Beat.h"
#include <stdio.h>

void Beat::init() {
    uint16_t p = 0;
    hits[p++] = {0, 0, 255};
    hits[p++] = {0, 2, 255};
    hits[p++] = {1<<14, 2, 255};
    hits[p++] = {2<<14, 2, 255};
    hits[p++] = {3<<14, 2, 255};
    hits[p++] = {1<<16, 1, 255};
    hits[p++] = {2<<16, 2, 255};
    hits[p++] = {3<<16, 3, 255};
    numHits = p;
    _lastProcessedPositionFP = 0;
    _nextHitIndex = 0;
    _thisHitIndex = 0;
}

bool Beat::hitAvailable(uint32_t positionFP) {
    if(positionFP < _lastProcessedPositionFP) {
        // looped back around, reset hit indices
        _nextHitIndex = 0;
    }
    _lastProcessedPositionFP = positionFP;

    bool available = false;
    if(_nextHitIndex < numHits && hits[_nextHitIndex].positionFP <= positionFP) {
        available = true;
        //printf("Transport pos %.4f, hit available at position %.4f on channel %d with velocity %d\n", positionFP / 65536.0f, hits[_nextHitIndex].positionFP / 65536.0f, hits[_nextHitIndex].channel, hits[_nextHitIndex].velocity);
        _thisHitIndex = _nextHitIndex;
        _nextHitIndex ++;
    }
    return available;
}

Beat::Hit Beat::getNextHit() {
    return hits[_thisHitIndex];
}