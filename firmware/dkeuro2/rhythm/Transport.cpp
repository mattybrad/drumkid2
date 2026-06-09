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
    return _running || (_clockMode == MODE_CLOCK_EXTERNAL && _firstPulseReceived);
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
        _lastPulseTimeUs = nowUs;
        _positionQ16 = 0;
        return;
    }

    int64_t _pulseIntervalUs = nowUs - _lastPulseTimeUs;
    _nextPulseTimeEstimateUs = nowUs + _pulseIntervalUs;
    _lastPulseTimeUs = nowUs;

    if(!_secondPulseReceived) {
        // this is second pulse, we can now calculate a rate
        _secondPulseReceived = true;
    }

    _rateUsPerQuarterNote = _pulseIntervalUs * PPQN;
    _startTimeUs = nowUs; // reset start time to now, so position will be 0 at this pulse

    _positionQ16 += (1 << 16) / PPQN;

    printf("Pulse in: interval=%lld us, rate=%u us/qn, position=%u qn\n", _pulseIntervalUs, _rateUsPerQuarterNote, _positionQ16 >> 16);
}

void Transport::setBpmQ16(uint32_t bpmQ16) {
    if (_running) {
        _positionQ16 = getPositionAtTimeQ16(time_us_64()); // capture position at current rate
        _startTimeUs = time_us_64();                      // restart the clock from here
    }
    _rateUsPerQuarterNote = (uint32_t)((60000000ULL << 16) / bpmQ16);
}

uint32_t Transport::getPositionAtTimeQ16(uint64_t timeUs) {
    uint64_t elapsedUs = timeUs - _startTimeUs;
    uint64_t newPositionQ16 = _positionQ16 + (elapsedUs << 16) / _rateUsPerQuarterNote;
    return (uint32_t)newPositionQ16;
}

// void Transport::setClockMode(uint32_t mode) {
//     _clockMode = mode;
// }