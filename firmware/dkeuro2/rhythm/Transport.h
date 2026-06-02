#pragma once
#include <cstdint>
#include "Config.h"

static const int32_t PPQN = 24;  // Pulses per quarter note

class Transport {
    public:
        void init();
        void pulseIn();
        void update();
        void toggleStartStop();
        bool isRunning();
        uint32_t getBpmFP(); // Q16.16
        uint32_t getPositionAtTimeFP(uint64_t timeUs); // Q16.16, quarter notes
        void setBpmFP(uint32_t bpmFP); // Q16.16
        // void setClockMode(uint32_t mode);
        // void setTimeSignature(uint32_t timeSignature);
        // void setTupletMode(uint32_t tuplet);
    private:
        uint32_t _clockMode = MODE_CLOCK_INTERNAL;
        bool _running = false; // in internal clock mode, whether transport is running or stopped
        uint32_t _rateUsPerQuarterNote = 500000; // default 120 BPM, microseconds per quarter note
        uint64_t _startTimeUs = 0; // time when transport was started, in microseconds
        uint32_t _positionFP = 0; // Q16.16, quarter notes, measured at last update or pulse in

        // int64_t _pulseInCount = 0; // total pulses received
        // uint32_t _positionFP = 0; // Q16.16, quarter notes
        // uint32_t _lastPositionFP = 0; // Q16.16, quarter notes
        // int64_t _rateFP = 0;     // Q32.32, microseconds per quarter note
        // int64_t _lastUpdateTime = 0; // microseconds
        // int64_t _lastPulseTime = 0;   // microseconds
        // int64_t _nextPulseTimeEstimate = 0; // microseconds
        // bool _firstPulseReceived = false;
        // bool _secondPulseReceived = false;
};