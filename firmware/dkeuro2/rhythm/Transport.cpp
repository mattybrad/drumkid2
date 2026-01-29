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
    positionFP = (pulseInCount << 32) + ((int64_t)(now - nextPulseTimeEstimate) << 32) / period;
    uint64_t wholePulsePosition = positionFP >> 32;
    if(wholePulsePosition > (lastPositionFP >> 32)) {
        if(wholePulsePosition % 24 == 0) {
            //printf("Pulse at position %llu\n", wholePulsePosition);
            //tempTriggerPulse();
        }
        if(wholePulsePosition % 24 == 0 || wholePulsePosition % 24 == 6) {
            //tempTriggerGate();
        }
    }
}

void Transport::pulseIn() {
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
        return;
    }
}

int64_t Transport::getPositionFP() {
    return positionFP;
}