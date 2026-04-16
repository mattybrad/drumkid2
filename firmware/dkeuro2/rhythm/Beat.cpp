#include "Beat.h"

void Beat::init() {
    uint16_t p = 0;
    hits[p++] = {0, 0, 127};
    hits[p++] = {0, 2, 127};
    hits[p++] = {12, 2, 127};
    hits[p++] = {24, 1, 127};
    hits[p++] = {24, 2, 127};
    hits[p++] = {36, 2, 127};
    hits[p++] = {48, 0, 127};
    hits[p++] = {48, 2, 127};
    hits[p++] = {60, 2, 127};
    hits[p++] = {72, 3, 127};
    hits[p++] = {72, 2, 127};
    hits[p++] = {78, 2, 127};
    hits[p++] = {84, 2, 127};
    hits[p++] = {90, 2, 127};
    numHits = p;
}

bool Beat::hitAvailable(uint16_t position) {
    bool available = false;
    if(hits[_nextHitIndex].position == position) {
        available = true;
        _thisHitIndex = _nextHitIndex;
        _nextHitIndex = (_nextHitIndex + 1) % numHits;
    }
    return available;
}

Beat::Hit Beat::getNextHit() {
    return hits[_thisHitIndex];
}