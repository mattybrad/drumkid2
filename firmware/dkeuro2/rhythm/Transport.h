#pragma once
#include <cstdint>

static const int32_t PPQN = 24;  // Pulses per quarter note

class Transport {
    public:
        void init();
        void pulseIn();
        void update();
        uint32_t getPositionFP(); // Q16.16, quarter notes
        uint32_t getPositionAtTimeFP(uint64_t timeUs); // Q16.16, quarter notes
        void setBPM(float bpm);
    private:
        int64_t pulseInCount = 0; // total pulses received
        uint32_t positionFP = 0; // Q16.16, quarter notes
        uint32_t lastPositionFP = 0; // Q16.16, quarter notes
        int64_t rateFP = 0;     // Q16.16, microseconds per quarter note
        int64_t lastUpdateTime = 0; // microseconds
        int64_t lastPulseTime = 0;   // microseconds
        int64_t nextPulseTimeEstimate = 0; // microseconds
        bool firstPulseReceived = false;
        bool secondPulseReceived = false;
};