#pragma once
#include <cstdint>
#include "Config.h"

static const int32_t PPQN = 4;  // Pulses per quarter note

class Transport {
    public:
        void init();
        void pulseIn();
        void update();
        void toggleStartStop();
        bool isRunning();
        uint32_t getBpmQ16(); // Q16.16
        uint32_t getPositionAtTimeQ16(uint64_t timeUs); // Q16.16, quarter notes
        void setBpmQ16(uint32_t bpmQ16); // Q16.16
        uint32_t getClockMode();
        void setClockMode(uint32_t mode);
        uint32_t getNumResets();
        void handleTapTempoPulse();
        void incrementTimeSignatureNumerator();
        void decrementTimeSignatureNumerator();
        void incrementTimeSignatureDenominator();
        void decrementTimeSignatureDenominator();
        struct TimeSignature {
            uint8_t numerator;
            uint8_t denominator;
        };
        TimeSignature getTimeSignature();
    private:
        uint32_t _clockMode = MODE_CLOCK_INTERNAL;
        uint32_t _numResets = 0;

        // Internal clock state
        bool _runningInt = false; // in internal clock mode, whether transport is running or stopped
        uint32_t _rateUsPerQuarterNoteInt = 500000; // default 120 BPM, microseconds per quarter note
        uint64_t _startTimeUsInt = 0; // time when transport was started, in microseconds
        uint32_t _positionQ16Int = 0; // Q16.16, quarter notes, measured at last update or pulse in

        // Tap tempo variables
        uint64_t _lastTapTimeUs = 0;
        uint32_t _recentTapsUs[4] = {0}; // store last 4 tap intervals in microseconds

        // External clock state
        bool _runningExt = false; // in external clock mode, whether transport is running or stopped
        uint64_t _anchorTimeUsExt = 0; // time of last pulse, in microseconds
        uint32_t _anchorPositionQ16Ext = 0; // Q16.16, quarter notes, measured at last pulse in
        uint32_t _estimatedUsPerQuarterNoteExt = 500000; // default 120 BPM, microseconds per quarter note
        bool _firstPulseReceived = false;
        bool _secondPulseReceived = false;
        uint8_t _timeSignatureNumerator = 4;
        uint8_t _timeSignatureDenominator = 4;
};