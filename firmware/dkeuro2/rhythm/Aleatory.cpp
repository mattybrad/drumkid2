#include "Aleatory.h"
#include "Config.h"
#include <cstdlib>
#include <stdio.h>

void Aleatory::init(AnalogInputs* analogInputs) {
    // todo: random seed using analog input noise, flash contents, timings, etc
    _analogInputs = analogInputs;    
}

Beat::Hit Aleatory::generateHit(uint8_t channelNum, uint32_t positionFP) {
    Beat::Hit hit;
    hit.positionFP = positionFP;
    hit.channel = channelNum;
    uint8_t tempZoomArray[6] = {16, 15, 14, 13, 12, 11};
    uint8_t zoomLevel = _analogInputs->getInputValue(POT_ZOOM) / 683; // 0-5

    uint32_t quantizedPositionFP = (positionFP >> 11) << 11;
    uint32_t zoomPositionFP = (positionFP >> tempZoomArray[zoomLevel]) << tempZoomArray[zoomLevel];

    if(quantizedPositionFP != _lastPositionQuantizedFP && zoomPositionFP == quantizedPositionFP) {
        if(rand() % 4095 < _analogInputs->getInputValue(POT_CHANCE)) {
            int32_t velRange = _analogInputs->getInputValue(POT_VELOCITY_RANGE);
            int32_t vel = _analogInputs->getInputValue(POT_VELOCITY) + (rand() % (velRange * 2 + 1) - velRange); // currently max range is actually 2 x 4095, not sure if this is what we want
            if(vel < 0) vel = 0;
            if(vel > 4095) vel = 4095;
            hit.velocity = vel >> 4;
            //printf("V%d ", hit.velocity);
        } else {
            hit.velocity = 0;
        }
    } else {
        hit.velocity = 0;
    }   

    return hit;
}

void Aleatory::finishGeneratingHits(uint32_t positionFP) {
    _lastPositionQuantizedFP = (positionFP >> 11) << 11;
}