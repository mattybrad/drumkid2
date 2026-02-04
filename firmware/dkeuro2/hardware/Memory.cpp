#include "Memory.h"

/*

Memory info:
Pico 2 has 512KB of SRAM, 4MB of flash memory
Allowing 1MB of flash for code, need a way of checking size on compile
3MB for data storage


*/

void Memory::init() {
    
}

void Memory::writeToFlash(uint32_t address, const uint8_t* data, size_t length) {
    // Implementation for writing data to flash memory
}