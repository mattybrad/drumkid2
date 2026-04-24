#include "Transport.h"
#include "pico/time.h"
#include <stdio.h>

void Transport::init() {
    pulseInCount = 0;
    positionFP = 0;
    lastPositionFP = 0;
    rateFP = 0;
    lastUpdateTime = 0;
    lastPulseTime = 0;
    nextPulseTimeEstimate = 0;
    firstPulseReceived = false;
    secondPulseReceived = false;
}

void Transport::update() {
    if(!secondPulseReceived) {
        return; // can't update until we have a rate
    }

    uint64_t now = time_us_64();
    int64_t period = nextPulseTimeEstimate - lastPulseTime;
    lastPositionFP = positionFP;
    
    // Convert pulse count to quarter notes: pulseCount / PPQN
    uint64_t quarterNoteCount = pulseInCount / PPQN;
    uint32_t quarterNoteFraction = ((pulseInCount % PPQN) << 16) / PPQN;
    
    // Add sub-pulse interpolation based on time
    int64_t timeFractionFP = ((int64_t)(now - nextPulseTimeEstimate) << 16) / (period * PPQN);
    
    positionFP = (quarterNoteCount << 16) + quarterNoteFraction + timeFractionFP;
    
    uint32_t wholeQuarterNotePosition = positionFP >> 16;
    if(wholeQuarterNotePosition > (lastPositionFP >> 16)) {
        //printf("Quarter note at position %u\n", wholeQuarterNotePosition);
        //tempTriggerPulse();
    }
}

void Transport::pulseIn() {
    //printf("Pulse in %llu\n", pulseInCount);
    uint64_t now = time_us_64();
    int64_t period = now - lastPulseTime;
    pulseInCount++;

    if(!firstPulseReceived) {
        lastPulseTime = now;
        firstPulseReceived = true;
        return;
    }

    nextPulseTimeEstimate = now + period;
    lastPulseTime = now;

    if(!secondPulseReceived) {
        secondPulseReceived = true;
        // Calculate rate as microseconds per quarter note (Q16.16)
        rateFP = ((int64_t)(period * PPQN)) << 16;  // period * PPQN = microseconds per quarter note
        return;
    }
    
    // Update rate calculation
    rateFP = ((int64_t)(period * PPQN)) << 16;
}

uint32_t Transport::getPositionFP() {
    return positionFP;
}

uint32_t Transport::getPositionAtTimeFP(uint64_t timeUs) {
    if(!secondPulseReceived) {
        return positionFP;
    }
    int64_t deltaTimeUs = (int64_t)(timeUs - lastPulseTime);
    int64_t deltaQuarterNotesFP = (deltaTimeUs << 16) / rateFP;  // deltaTime / (us per quarter note)
    return positionFP + deltaQuarterNotesFP;
}