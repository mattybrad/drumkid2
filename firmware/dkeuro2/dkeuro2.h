#ifndef DK_H
#define DK_H

void updateTransport();
void clockInCallback(uint gpio, uint32_t events);
void handleButtonPress(int16_t buttonIndex);
void tempKitLoad(uint kitNum);

#endif