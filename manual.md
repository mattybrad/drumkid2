# Drumkid Eurorack manual

## Introduction

Drumkid is a drum machine module which uses random numbers to create unpredictable rhythms. Essentially, you start with a standard drum beat and then use Drumkid’s controls to adjust how much randomness to add.

You start with a pre-programmed rhythm using four samples, then manipulate this rhythm using the knobs and control voltage (CV) inputs to add hits to (or remove hits from) the original rhythm.

This manual consists of a tutorial to guide you through the basic workflow of the module, followed by a detailed description of each parameter or feature. I have also included some less focused ramblings about the ideas behind the module, and how to hack/modify the code or even the hardware.

## Tutorial (NOT FINISHED, CHECK CONTENT)

The easiest way to start with Drumkid is to play a simple beat without any random additions, then gradually add random elements to get a feel for how the module's algorithm works.

1. Connect a patch lead to “out 1” so you can hear the output
2. Set the controls as follows: (image of chance 50, zoom 50(?), range 0, vel 100, plus other defaults)
3. Press “start”
4. Select a preset beat by pressing “beat”, then using the +/- buttons
5. Slowly increase the “chance” control and notice how more and more hits are added to the beat, until at 100% you hear every sample being triggered over and over
6. Set the “chance” back to 50%, and now try adjusting the “zoom” control. As you increase the zoom, the beat will become busier as hits are triggered at smaller note values (8th-notes, 16th-notes, 32nd-notes, etc). These first two controls, chance and zoom, are the heart of the machine, and I would recommend playing around with them for a while to get a feel of how they affect the beat.
7. Slowly reduce the “velocity” control from +100% to 0% (in the middle) - the extra random hits will become quieter. If you set velocity to be negative, it will quieten or mute hits from the original beat.
8. Set “velocity” to +50%, and “range” to 100% (in the middle). 

## Inputs and outputs

### Clock in

### Clock out

### Kick/snare/hat/other out

### Chance in

### Pitch in

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

## Parameters (knobs)

### Chance

### Zoom

### Cluster

### Crop

### Crush

### Pitch

### Velocity (vel)

### Velocity (vel) range

### Magnet

### Swing

### Drop rand

### Drop all


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