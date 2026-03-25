#include "KitManager.h"

void KitManager::init() {
    // populate kits array with metadata from flash
    
}

uint KitManager::getFreeSectors() {
    // for now just return a dummy value, will need to implement a way of tracking used/free sectors in flash to get an accurate number here
    return 256; // 1MB free (256 sectors) for audio data, starting from sector 386
}