#pragma once

#include <cstdint>
#include "Beat.h"
#include "hardware/AnalogInputs.h"

class Aleatory {
    public:
        void init(AnalogInputs* analogInputs);
        Beat::Hit generateHit(uint8_t channelNum, uint16_t position);

    private:
        AnalogInputs* _analogInputs;
};