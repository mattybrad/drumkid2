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
    uint8_t tempZoomArray[6] = {24, 12, 6, 3, 2, 1};
    uint8_t zoomLevel = _analogInputs->getInputValue(POT_ZOOM) / 683; // 0-5
    if(position % tempZoomArray[zoomLevel] == 0) {
        if(rand() % 4095 > _analogInputs->getInputValue(POT_CHANCE)) {
            hit.velocity = _analogInputs->getInputValue(POT_VELOCITY) >> 5;
        } else {
            hit.velocity = 0;
        }
    } else {
        hit.velocity = 0;
    }
    return hit;
}