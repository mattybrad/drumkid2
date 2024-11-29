# Drumkid Eurorack manual

## Introduction

Drumkid is a drum machine module which uses random numbers to create unpredictable rhythms. Essentially, you start with a standard drum beat and then use Drumkid’s controls to adjust how much randomness to add.

You start with a pre-programmed rhythm using four samples, then manipulate this rhythm using the knobs and control voltage (CV) inputs to add hits to (or remove hits from) the original rhythm.

This manual consists of a tutorial to guide you through the basic workflow of the module, followed by a detailed description of each parameter or feature. I have also included some less focused ramblings about the ideas behind the module, and how to hack/modify the code or even the hardware.

## Tutorial (NOT FINISHED, CHECK CONTENT)

The easiest way to start with Drumkid is to play a simple beat without any random additions, then gradually add random elements to get a feel for how the module's algorithm works.

1. Connect a patch lead to “out 1” so you can hear the output
2. Set the controls as follows: (image of chance 50, zoom 50, range 0, vel 100, plus other defaults)
3. Press “Play”
4. Select a preset beat by pressing “beat”, then using the +/- buttons
5. Slowly decrease the "chance" control to zero and notice how the beat is gradually silenced as the probability of each hit happening is reduced
6. Slowly increase the “chance” control back to 50% and beyond. As you increase past 50%, notice how more and more random hits are added to the beat, until at 100% you hear every sample being triggered over and over (100% probability). 
7. Set the “chance” back to around 3 o'clock, so you can hear that the full original beat plus some random hits, and now try adjusting the “zoom” control. As you increase the zoom, the beat will become busier as hits are triggered at smaller note values (8th-notes, 16th-notes, 32nd-notes, etc). These first two controls, chance and zoom, are the heart of the machine, and I would recommend playing around with them for a while to get a feel of how they affect the beat.
7. Slowly reduce the “velocity” control from +100% to 0% (in the middle) - the extra random hits will become quieter. If you set velocity to be negative, it will quieten or mute hits from the original beat.
8. Set “velocity” to +50%, and “range” to 100% (in the middle). The hits should now have a full range of volumes, from quiet to loud. Try playing with these controls to adjust the relative volume of the random hits.

## Parameters (knobs)

### Chance
Determines the chance/probability of a hit occurring on a given step. At 0%, no hits will be generated. At 50%, all hits from the programmed rhythm will be heard (but nothing else). At 100%, every possible hit will be generated.

### Zoom
Determines which steps of the rhythm are eligible to have extra random hits. At 0%, no extra hits will be generated. As you increase the zoom control, you introduce the possibility of hits on the first beat of the bar (whole notes), then half notes, quarter notes, eighth notes, etc, all the way until 128th notes. Note that these values will be different if the tuplet mode is anything other than "straight" (e.g. triplet).

### Cluster
Similar to chance, 

### Crop

### Crush

### Pitch

### Velocity (vel)

### Velocity (vel) range

### Magnet

### Swing

### Drop rand

### Drop all

## Inputs and outputs

### Clock in
Receives a clock input (a series of short pulses) to allow the module to be synchronised to an external signal. More details in the "synchronisation" section.

### Clock out
Outputs an adjustable clock signal. More details in the "synchronisation" section.

### Kick/snare/hat/other out
Each of the four samples has its own trigger output. This outputs a short pulse every time a given sample is played by the module. Note that if you only want to use the trigger output and not generate any audio for a given sample channel, you can disable individual sample audio outputs in the settings menu.

### Chance in
CV input for the "chance" parameter. The CV signal is added to the potentiometer value.

### Pitch in
CV input for the 

### Velocity in

### Zoom in

### Out 1

### Out 2


## SD card slot


## Display and indicators lights

### Main display

### Error

### Pulse

### Clock in

### Clock out

## Settings (buttons)

### Play

### Tap

### Live (edit)

### Step (edit)

### A/B

### Save

### Beat

### Tempo

### Tuplet

### Menu

### Kit

### T.sig (time signature)

### Clear

### Back


## Synchronisation (clock)


## Updating firmware


## Thoughts about Drumkid


## Hacking