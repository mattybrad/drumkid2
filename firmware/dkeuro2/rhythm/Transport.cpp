#include "Transport.h"
#include "pico/time.h"
#include <stdio.h>

void Transport::init() {
    
}

void Transport::toggleStartStop() {
    if(_clockMode == MODE_CLOCK_INTERNAL) {
        _runningInt = !_runningInt;
        if(_runningInt) {
            _positionQ16Int = 0;
            _numResets++;
            _startTimeUsInt = time_us_64();
        }
    }
}

bool Transport::isRunning() {
    return (_clockMode == MODE_CLOCK_INTERNAL) ? _runningInt : _runningExt;
}

uint32_t Transport::getNumResets() {
    return _numResets;
}

uint32_t Transport::getBpmQ16() {
    if(_clockMode == MODE_CLOCK_EXTERNAL) {
        if(!_firstPulseReceived) {
            return 0; // no pulses received yet, BPM is unknown
        }
        if(_estimatedUsPerQuarterNoteExt == 0) return 0; // prevent division by zero
        uint64_t bpmQ16_16 = (60000000ULL << 16) / _estimatedUsPerQuarterNoteExt;
        return (uint32_t)bpmQ16_16;
    } else if(_clockMode == MODE_CLOCK_INTERNAL) {
        if(_rateUsPerQuarterNoteInt == 0) return 0; // prevent division by zero
        uint64_t bpmQ16_16 = (60000000ULL << 16) / _rateUsPerQuarterNoteInt;
        return (uint32_t)bpmQ16_16;
    }
    return 0; // default return value if clock mode is invalid
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
        _runningExt = true;
        _anchorTimeUsExt = nowUs;
        _anchorPositionQ16Ext = 0;
        _estimatedUsPerQuarterNoteExt = 500000; // default to 120 BPM for now
        printf("First pulse received, starting transport\n");
        return;
    }

    if(!_secondPulseReceived) {
        _secondPulseReceived = true;
    }
    
    int64_t elapsedUs = nowUs - _anchorTimeUsExt;
    uint64_t newMeasuredUsPerQuarterNote = elapsedUs * PPQN;

    // this is not yet perfect - seems to take a while to smooth out

    // simple low-pass filter to smooth out BPM changes, with more weight on previous estimate to prevent erratic BPM changes due to jittery clock signals
    _estimatedUsPerQuarterNoteExt = (_estimatedUsPerQuarterNoteExt * 7 + newMeasuredUsPerQuarterNote) / 8; // feeds in new measurement with 1/8 weight
    _anchorTimeUsExt = nowUs;
    _anchorPositionQ16Ext += (1 << 16) / PPQN; // advance position by one pulse (in Q16.16)
}

void Transport::setBpmQ16(uint32_t bpmQ16) {
    if (_runningInt) {
        _positionQ16Int = getPositionAtTimeQ16(time_us_64()); // capture position at current rate
        _startTimeUsInt = time_us_64();                      // restart the clock from here
    }
    _rateUsPerQuarterNoteInt = (uint32_t)((60000000ULL << 16) / bpmQ16);
}

uint32_t Transport::getPositionAtTimeQ16(uint64_t timeUs) {
    if(_clockMode == MODE_CLOCK_INTERNAL) {
        uint64_t elapsedUs = timeUs - _startTimeUsInt;
        uint64_t newPositionQ16 = _positionQ16Int + (elapsedUs << 16) / _rateUsPerQuarterNoteInt;
        return (uint32_t)newPositionQ16;
    } else if(_clockMode == MODE_CLOCK_EXTERNAL) {
        if(!_firstPulseReceived) {
            return 0; // no pulses received yet, position is 0
        }
        int64_t elapsedUs = timeUs - _anchorTimeUsExt;
        uint64_t newPositionQ16 = _anchorPositionQ16Ext + (elapsedUs << 16) / _estimatedUsPerQuarterNoteExt;
        return (uint32_t)newPositionQ16;
    }
    return 0; // default return value if clock mode is invalid
}

void Transport::setClockMode(uint32_t mode) {
    if(mode == MODE_CLOCK_INTERNAL) {
        _runningInt = true;
        _positionQ16Int = 0;
        _startTimeUsInt = time_us_64();
    } else if(mode == MODE_CLOCK_EXTERNAL) {
        _firstPulseReceived = false;
        _secondPulseReceived = false;
        _runningExt = false;
        _estimatedUsPerQuarterNoteExt = 500000; // default to 120 BPM
        _anchorTimeUsExt = time_us_64();
        _anchorPositionQ16Ext = 0;
    }
    _numResets++;
    _clockMode = mode;
}

uint32_t Transport::getClockMode() {
    return _clockMode;
}

void Transport::handleTapTempoPulse() {
    if(_clockMode != MODE_CLOCK_INTERNAL) {
        // ignore tap tempo if not in internal clock mode
        return;
    }
    uint64_t nowUs = time_us_64();
    if(_lastTapTimeUs != 0) {
        uint32_t intervalUs = (uint32_t)(nowUs - _lastTapTimeUs);
        // shift previous taps and add new interval
        for(int i = 3; i > 0; i--) {
            _recentTapsUs[i] = _recentTapsUs[i-1];
        }
        _recentTapsUs[0] = intervalUs;

        // calculate average interval from recent taps
        uint32_t sumIntervals = 0;
        int validTaps = 0;
        for(int i = 0; i < 4; i++) {
            if(_recentTapsUs[i] > 0) {
                sumIntervals += _recentTapsUs[i];
                validTaps++;
            }
        }
        if(validTaps > 0) {
            uint32_t avgIntervalUs = sumIntervals / validTaps;
            uint32_t bpmQ16 = (60000000ULL << 16) / avgIntervalUs;
            setBpmQ16(bpmQ16);
            printf("Tap tempo set to %d BPM\n", bpmQ16 >> 16);
        }
    }
    _lastTapTimeUs = nowUs;
}

void Transport::incrementTimeSignatureNumerator() {
    _timeSignatureNumerator++;
    _timeSignatureNumerator = (_timeSignatureNumerator > 32) ? 32 : _timeSignatureNumerator; // limit to 32
}

void Transport::decrementTimeSignatureNumerator() {
    _timeSignatureNumerator--;
    _timeSignatureNumerator = (_timeSignatureNumerator < 1) ? 1 : _timeSignatureNumerator; // limit to 1
}

void Transport::incrementTimeSignatureDenominator() {
    if(_timeSignatureDenominator == 4) {
        _timeSignatureDenominator = 8;
    } else {
        _timeSignatureDenominator = 4;
    }
}

void Transport::decrementTimeSignatureDenominator() {
    incrementTimeSignatureDenominator(); // toggle between 4 and 8 for now
}

Transport::TimeSignature Transport::getTimeSignature() {
    TimeSignature ts;
    ts.numerator = _timeSignatureNumerator;
    ts.denominator = _timeSignatureDenominator;
    return ts;
}