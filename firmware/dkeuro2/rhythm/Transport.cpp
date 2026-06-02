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

bool Transport::isRunning() {
    return _running;
}

uint32_t Transport::getBpmFP() {
    if(_rateUsPerQuarterNote == 0) return 0; // prevent division by zero
    uint64_t bpmQ16_16 = (60000000ULL << 16) / _rateUsPerQuarterNote;
    return (uint32_t)bpmQ16_16;
}

void Transport::update() {
    
}

void Transport::pulseIn() {
    
}

void Transport::setBpmFP(uint32_t bpmFP) {
    if (_running) {
        _positionFP = getPositionAtTimeFP(time_us_64()); // capture position at current rate
        _startTimeUs = time_us_64();                      // restart the clock from here
    }
    _rateUsPerQuarterNote = (uint32_t)((60000000ULL << 16) / bpmFP);
}

uint32_t Transport::getPositionAtTimeFP(uint64_t timeUs) {
    uint64_t elapsedUs = timeUs - _startTimeUs;
    uint64_t positionQ16_16 = _positionFP + (elapsedUs << 16) / _rateUsPerQuarterNote;
    return (uint32_t)positionQ16_16;
}

// void Transport::setClockMode(uint32_t mode) {
//     _clockMode = mode;
// }