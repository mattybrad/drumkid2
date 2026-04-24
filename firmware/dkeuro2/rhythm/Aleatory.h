#pragma once

#include <cstdint>
#include "Beat.h"
#include "hardware/AnalogInputs.h"

class Aleatory {
    public:
        void init(AnalogInputs* analogInputs);
        Beat::Hit generateHit(uint8_t channelNum, uint32_t positionFP);

    private:
        AnalogInputs* _analogInputs;
};