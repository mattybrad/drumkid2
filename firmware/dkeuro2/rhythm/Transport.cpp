#include "Transport.h"
#include "pico/time.h"
#include <stdio.h>

void Transport::init() {
    
}

void Transport::toggleStartStop() {
    if(_clockMode == MODE_CLOCK_INTERNAL) {
        _running = !_running;
        if(_running) {
            _positionQ16 = 0;
            _startTimeUs = time_us_64();
        }
    }
}

bool Transport::isRunning() {
    return _running;
}

uint32_t Transport::getBpmQ16() {
    if(_rateUsPerQuarterNote == 0) return 0; // prevent division by zero
    uint64_t bpmQ16_16 = (60000000ULL << 16) / _rateUsPerQuarterNote;
    return (uint32_t)bpmQ16_16;
}

void Transport::update() {
    
}

void Transport::pulseIn() {
    if(_clockMode != MODE_CLOCK_EXTERNAL) {
        // ignore pulses if not in external clock mode
        return;
    }

    
    uint64_t nowUs = time_us_64();
    
    if(!_firstPulseReceived) {
        // this is first pulse
        _firstPulseReceived = true;
        _running = true;
        _positionQ16 = 0;
        printf("First pulse received, starting transport\n");
        return;
    }

    if(!_secondPulseReceived) {
        _secondPulseReceived = true;
    }
    
    _positionQ16 += (1 << 16) / PPQN; // advance position by one pulse (in Q16.16)
    //printf("Pulse, pos (QN): %d \n", _positionQ16 >> 16);
}

void Transport::setBpmQ16(uint32_t bpmQ16) {
    if (_running) {
        _positionQ16 = getPositionAtTimeQ16(time_us_64()); // capture position at current rate
        _startTimeUs = time_us_64();                      // restart the clock from here
    }
    _rateUsPerQuarterNote = (uint32_t)((60000000ULL << 16) / bpmQ16);
}

uint32_t Transport::getPositionAtTimeQ16(uint64_t timeUs) {
    if(_clockMode == MODE_CLOCK_INTERNAL) {
        uint64_t elapsedUs = timeUs - _startTimeUs;
        uint64_t newPositionQ16 = _positionQ16 + (elapsedUs << 16) / _rateUsPerQuarterNote;
        return (uint32_t)newPositionQ16;
    } else if(_clockMode == MODE_CLOCK_EXTERNAL) {
        // not done yet
        return _positionQ16;
    }
    return 0; // default return value if clock mode is invalid
}

// void Transport::setClockMode(uint32_t mode) {
//     _clockMode = mode;
// }