
# Drumkid 2
Aleatoric drum machine, uses random numbers to generate/alter drumbeats. This project is the successor to the original Drumkid, which was released in 2020.

## Changes
The original Drumkid generated lo-fi audio using the Mozzi library, running on an Arduino Nano clone. It also had MIDI in/out and could be powered by 3xAA batteries or a USB cable. It was housed in a basic, semi-open case with the PCB acting as the front panel. Drumkid 2's specifications are still in flux, but the planned changes are listed below:
- Durable metal case
- Raspberry Pi Pico (or RP2040) used instead of Arduino Nano
- High-quality audio via DAC instead of 8-bit PWM
- No longer battery powered as standard
- Eurorack support at some point (CV control over params, maybe individual trigger outputs)