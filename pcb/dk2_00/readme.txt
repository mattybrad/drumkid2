This is an early prototype PCB for Drumkid 2. It is almost a proof-of-concept rather than a proper attempt at a design. Features can be whittled down later.

Rough specs:
MCU: Raspberry Pi Pico
DAC: PCM510XA
Pots: up to 8
LEDs: up to 16
MIDI: in/out, 3.5mm
Sync: in/out, 3.5mm
Audio: out, not sure whether 3.5mm or 6.35mm (or both? inc headphone?)
Buttons: 6+
Power: USB? 
Data: SD card

Might try and include some more Eurorack things to speed up later development. For instance:
Eurorack power input, maybe with jumper to power device instead of USB
CV inputs using MCP6002 (basic part on JLC)
CV outputs using MCP4922? Into some sort of op-amp, an MCP6002 again?