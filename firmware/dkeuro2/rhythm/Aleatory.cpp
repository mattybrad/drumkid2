#include "Aleatory.h"
#include "Config.h"
#include <cstdlib>

void Aleatory::init(AnalogInputs* analogInputs) {
    // todo: random seed using analog input noise, flash contents, timings, etc
    _analogInputs = analogInputs;    
}

Beat::Hit Aleatory::generateHit(uint8_t channelNum, uint16_t position) {
    Beat::Hit hit;
    hit.position = position;
    hit.channel = channelNum;
    if(position % 6 == 0) {
        if(rand() % 4095 > _analogInputs->getInputValue(POT_CHANCE)) {
            hit.velocity = 255;
        } else {
            hit.velocity = 0;
        }
    } else {
        hit.velocity = 0;
    }
    return hit;
}