#include "Transport.h"
#include "pico/time.h"
#include <stdio.h>

void Transport::init() {
    
}

void Transport::toggleStartStop() {
    _running = !_running;
    if(_running) {
        _positionFP = 0;
        _startTimeUs = time_us_64();
    }
}

void Transport::update() {
    
}

void Transport::pulseIn() {
    
}

void Transport::setBPM(float bpm) {
    if (_running) {
        _positionFP = getPositionAtTimeFP(time_us_64()); // capture position at current rate
        _startTimeUs = time_us_64();                      // restart the clock from here
    }
    _rateUsPerQuarterNote = (uint32_t)(60000000.0f / bpm);
}

uint32_t Transport::getPositionAtTimeFP(uint64_t timeUs) {
    if(!_running) {
        return _positionFP;
    }
    uint64_t elapsedUs = timeUs - _startTimeUs;
    uint64_t positionQ16_16 = (elapsedUs << 16) / _rateUsPerQuarterNote;
    return (uint32_t)positionQ16_16;
}

// void Transport::setClockMode(uint32_t mode) {
//     _clockMode = mode;
// }