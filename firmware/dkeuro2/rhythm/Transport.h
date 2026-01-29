#pragma once
#include <cstdint>

class Transport {
    public:
        void init();
        void pulseIn();
        void update();
        int64_t getPositionFP(); // Q32.32
        void setBPM(float bpm);
    private:
        int64_t pulseInCount = 0; // total pulses received
        int64_t positionFP = 0; // Q32.32, pulses
        int64_t lastPositionFP = 0; // Q32.32, pulses
        int64_t rateFP = 0;     // Q32.32, pulses per microsecond
        int64_t lastUpdateTime = 0; // microseconds
        int64_t lastPulseTime = 0;   // microseconds
        int64_t nextPulseTimeEstimate = 0; // microseconds
        bool firstPulseReceived = false;
        bool secondPulseReceived = false;
};