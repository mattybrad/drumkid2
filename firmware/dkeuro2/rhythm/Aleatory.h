#pragma once

#include <cstdint>
#include "Beat.h"
#include "hardware/AnalogInputs.h"

// For each sample frame update, run generateHit() for each channel, then run finishGeneratingHits() when done

// TODO: cluster, magnet, swing?, drop..?

class Aleatory {
    public:
        void init(AnalogInputs* analogInputs);
        Beat::Hit generateHit(uint8_t channelNum, uint32_t positionFP);
        void finishGeneratingHits(uint32_t positionFP);

    private:
        AnalogInputs* _analogInputs;
        uint32_t _lastPositionQuantizedFP = 0;
};