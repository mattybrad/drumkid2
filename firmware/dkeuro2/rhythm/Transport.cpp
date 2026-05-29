#include "Transport.h"
#include "pico/time.h"
#include <stdio.h>

void Transport::init() {
    _pulseInCount = 0;
    _positionFP = 0;
    _lastPositionFP = 0;
    _rateFP = 0;
    _lastUpdateTime = 0;
    _lastPulseTime = 0;
    _nextPulseTimeEstimate = 0;
    _firstPulseReceived = false;
    _secondPulseReceived = false;
}

void Transport::toggleStartStop() {
    if(_clockMode == MODE_CLOCK_INTERNAL) {
        _runningInt = !_runningInt;
        if(_runningInt) {
            printf("Transport start\n");
            _pulseInCount = 0;
            _positionFP = 0;
            _lastPositionFP = 0;
            _rateFP = 500000UL << 16; // default to 120 BPM (500us per quarter note)
            _lastUpdateTime = 0;
            _lastPulseTime = 0;
            _nextPulseTimeEstimate = 0;
            _firstPulseReceived = true;
            _secondPulseReceived = true;
        } else {
            printf("Transport stop\n");
        }
    }
}

void Transport::update() {
    if(_clockMode == MODE_CLOCK_INTERNAL) {
        if(!_runningInt) {
            return; // don't update position if stopped
        }
        uint64_t now = time_us_64();
        int64_t deltaTimeUs = (int64_t)(now - _lastUpdateTime);
        _lastUpdateTime = now;
        int64_t deltaQuarterNotesFP = (deltaTimeUs << 16) / _rateFP;  // deltaTime / (us per quarter note)
        _positionFP += deltaQuarterNotesFP;
    } else if(_clockMode == MODE_CLOCK_EXTERNAL) {
        if(!_secondPulseReceived) {
            return; // can't update until we have a rate
        }

        uint64_t now = time_us_64();
        int64_t period = _nextPulseTimeEstimate - _lastPulseTime;
        _lastPositionFP = _positionFP;
        
        // Convert pulse count to quarter notes: pulseCount / PPQN
        uint64_t quarterNoteCount = _pulseInCount / PPQN;
        uint32_t quarterNoteFraction = ((_pulseInCount % PPQN) << 16) / PPQN;
        
        // Add sub-pulse interpolation based on time
        int64_t timeFractionFP = ((int64_t)(now - _nextPulseTimeEstimate) << 16) / (period * PPQN);
        
        _positionFP = (quarterNoteCount << 16) + quarterNoteFraction + timeFractionFP;
        
        uint32_t wholeQuarterNotePosition = _positionFP >> 16;
        if(wholeQuarterNotePosition > (_lastPositionFP >> 16)) {
            //printf("Quarter note at position %u\n", wholeQuarterNotePosition);
            //tempTriggerPulse();
        }
    }


}

void Transport::pulseIn() {
    if(_clockMode != MODE_CLOCK_EXTERNAL) {
        return; // ignore pulses if not in external clock mode
    }

    uint64_t now = time_us_64();
    int64_t period = now - _lastPulseTime;
    _pulseInCount++;

    if(!_firstPulseReceived) {
        _lastPulseTime = now;
        _firstPulseReceived = true;
        return;
    }

    _nextPulseTimeEstimate = now + period;
    _lastPulseTime = now;

    if(!_secondPulseReceived) {
        _secondPulseReceived = true;
        // Calculate rate as microseconds per quarter note (Q16.16)
        _rateFP = ((int64_t)(period * PPQN)) << 16;  // period * PPQN = microseconds per quarter note
        return;
    }
    
    // Update rate calculation
    _rateFP = ((int64_t)(period * PPQN)) << 16;
}

uint32_t Transport::getPositionFP() {
    return _positionFP;
}

uint32_t Transport::getPositionAtTimeFP(uint64_t timeUs) {
    if(!_secondPulseReceived) {
        return _positionFP;
    }
    int64_t deltaTimeUs = (int64_t)(timeUs - _lastPulseTime);
    int64_t deltaQuarterNotesFP = (deltaTimeUs << 16) / _rateFP;  // deltaTime / (us per quarter note)
    return _positionFP + deltaQuarterNotesFP;
}

void Transport::setClockMode(uint32_t mode) {
    _clockMode = mode;
}