DK Eurorack prototype

Either single PCB or double-decker
	PCB 1 = user interface, display, shift reg's, muxes, SD slot
	PCB 2 = pico, power, DAC, maybe op-amps?
	Think we can just about get away with single PCB
PCB dimensions 120 wide x 108 high (now 140 x 108, increased from 24HP to 28HP)
16 buttons (2 shift reg's)
12 pots (2 muxes inc CV's)
5 trigger outputs?
1 trigger input
4 analog CV inputs
2 audio outputs
SD card, vertical? Yamaichi PJS008U-3000-0?
	Just using SPI not SDIO, doesn't need to be very fast(?)
Power, buck converter, more efficient
Display: TBC

PCB assembly notes, 11/10/23
- Dimensions appear correct, the boards line up with each other and fit within a Eurorack case.
- Some difficulty with getting pots and tactile buttons up to sufficient/correct height - should maybe be using Thonk 11mm standoffs rather than 10mm to match pots and sockets, but then hard to get buttons high enough (especially the 12mm/large ones)
- Somehow screwed up the 3.3V LDO again - no magic smoke but only 0.2V output instead of 3.3V, assumed SOT23 (as specified by JLCPCB) but component is actually SOT23R (only a small difference in footprint but AP7333 has versions for both SOT23 and SOT23R with incompatible pinouts). Just need to make sure I'm using SOT23R footprint and SOT23R pinout (1=GND, 2=OUT, 3=IN) next time. Or give up and use dirty 3.3V supply from Pico.
- For now I'm going to remove the AP7333 and run a jumper to the Pico 3.3V to power the DAC.
- SD card works
- Buttons seem to work
- Pots seem to work
- 3mm LEDs seem to work
- Euro audio out works
- Sync out works

Notes continued, 6/11/23
- 7-seg display works but struggling with flicker (code issue very likely)