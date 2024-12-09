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
Similar to the "chance" parameter, but designed to create repeated hits or rolls. If a random hit was generated on the most recent eligible step, "cluster" is the probability of another hit being generated on this step (unless "chance" is higher). For instance, if chance is set quite low, only occasionally generating hits, and then cluster is set to 90%, you will get occasional but quite long series of repeated hits. If you set cluster to 100%, you will keep hearing repeated hits continually.

### Crop
This crops the sample to create a staccato effect. At 0%, the entire sample is heard. As you increase the crop control, the sample will become shorter and shorter until you only hear a tiny pop at 100%.

### Crush
This reduces the bit depth from 16 bits (CD quality) at 0% down to one bit at 100%, creating a harsh, crunchy, distorted sound.

### Pitch
This changes the playback speed of the samples. At 50% (12 o'clock), the playback speed is 100% (normal), and there is a central "dead zone" at this value to make it easier to lock the pitch here. Above 50%, the playback speed increases, resulting in a higher pitch. Below 50%, the playback speed decreases, resulting in a lower pitch. As you keep reducing the pitch control towards 0%, the sample playback is actually reversed, and then increases in pitch again, so that 0% results in fast, reversed sample playback.

### Velocity (vel)
This determines the velocity (volume) of the randomly generated hits. It is used in conjunction with "vel range", so if "vel range" is 0% and "vel" is "75%", all randomly generated hits will be at exactly 75%. However, if "vel range" is 50% and "vel" is 50%, the randomly generated hits will have random velocities in the range 25% to 75%.

### Velocity (vel) range
The range of velocities for randomly generated hits (see above).

### Magnet
This control affects whether hits are generated at completely random points within the bar, or whether they are more likely to occur on larger-value subdivisions, e.g. quarter notes. At 0%, no adjustment is made to the way random notes are generated, but as you increase the magnet control, the likelihood of a note being generated becomes proportional to the note's subdivision, so a quarter-note hit is more likely than an eighth-note hit, which in turn is more likely than a 16th-note hit, etc. In practice, this control creates order out of chaos, turning a messy set of random hits into something more rhythmic. Note that as "chance" approaches 100%, the efficacy of the "magnet" control is gradually reduced to zero - the control was designed this way so that the behaviours of "chance" and "magnet" were not fighting each other at high probabilities. This is perhaps the most subtle control on Drumkid - I would recommend keeping it at 0% in normal use, and increasing it when you want to create a groove from something that is otherwise too messy.

### Swing
Allows you to swing certain hits. At 50% (12 o'clock), hits are generated as normal, with a dead zone to make it easier to keep this setting locked. As you turn the knob to the left (towards 0%), any off-beat 8th-notes are increasingly swung, while turning the knob to the right (towards 100%) swings any off-beat 16th-notes. When either of these swing modes is in use, random hits of a lower subdivision than the swung hits (e.g. 32nd notes when in 16th-note mode) are muted. The swing control is not available when tuplet modes (e.g. triplet) are activated in the settings. If you are not getting the desired behaviour while adjusting the various control which generate random hits, check that swing is set to 50% (off).

### Drop all
This control drops (or mutes) certain channels. At 50% (12 o'clock), all hits are audible. As you turn the knob to the left, you first mute the snare, then also the hi-hat, then the tom, and finally the kick. Turning the knob to the right first mutes the kick, then the tom, then the snare, then finally the hi-hat. At 0% and 100%, all random hits are muted. This control essentially allows you to create "breakdown" sections in a performance by progressively (or suddenly) eliminating certain channels.

### Drop rand
Similar to the "drop all" control (see above), but only applies to randomly generated hits, not hits from the programmed beat.

## Inputs and outputs

### Clock in
Receives a clock input (a series of short pulses) to allow the module to be synchronised to an external signal. More details in the "synchronisation" section.

### Clock out
Outputs an adjustable clock signal. More details in the "synchronisation" section.

### Trigger outputs (kick/snare/hat/tom out)
Each of the four samples has its own trigger output. This outputs a short pulse every time a given sample is played by the module. Note that if you only want to use the trigger output and not generate any audio for a given sample channel, you can disable individual sample audio outputs in the settings menu.

### CV inputs (chance/vel/pitch/zoom in)
The four CV inputs all work in the same way: the CV signal is added to the relevant potentiometer value. A positive CV signal will increase the value of a given parameter, while a negative signal will reduce it. For instance, if you have the velocity knob set at 50% and a sine wave LFO signal (with a range of -5V to 5V) connected to the "vel in" CV input, the velocity parameter will swing from 0% to 100%. If the CV signal would cause the parameter to go beyond the 0% to 100% range, it is clamped, except for "pitch in" because it's fun to have some extra range and there's no real reason not to. CV signals beyond the range -5V to 5V are clamped.

### Out 1
Audio output at 44.1kHz. You can use the menu to adjust which channels are muted for each output, and whether the crush effect is active for each output.

### Out 2
Second audio output, see above.

## SD card slot
The micro SD card slot can be used to load samples. There should be a folder called "samples" on the root of the SD card, containing a number of subfolders. Each subfolder is a "kit" (set of four samples), and should contain four WAV files with the filenames "1.wav", "2.wav", "3.wav", and "4.wav". The files must be in WAV format. The files can be stereo, but will be converted to mono when loaded. The sample rate can be 44100Hz, 22050Hz, or 11025Hz (you can use other sample rates, but they will be played back at 44100Hz). The maximum combined audio data size for each kit is 200kB (after any conversion to mono). If you want to use longer samples (for cymbals etc), you can try reducing the sample rate of these files.

## Display and indicators lights

### Main display
The four-digit display is normally used to show which quarter-note of the rhythm is currently being played. Pressing buttons will cause the display to show other relevant information. You can always return to the main display by pressing "back".

### Error
The "error" light is lit when there has been some sort of issue, usually related to loading samples from the SD card. The main display will give an error code when something goes wrong (see error code section). The error light will remain lit until the error is no longer actively causing an issue.

### Pulse
This light blinks once per quarter note when a rhythm is playing.

### Clock in
This blinks every time a clock pulse is received.

### Clock out
This blinks every time a clock pulse is sent.

## Buttons

### Play
This button seems self explanatory but has different behaviours depending on whether you are using internal or external clock. When using internal clock, pressing "play" once starts the rhythm playing from the start of the bar, and pressing it again stops the beat. When using external clock, the beat will always continue to play whenever a clock signal is received, but pressing "play" will reset the rhythm to the start of the bar.

### Tap
Tapping this button repeatedly will set a new tempo (each tap is a quarter note). The first tap will reset the rhythm to the start of the bar, and subsequent taps will fine-tune the tempo by averaging the past few taps. This button only works in internal clock mode.

### Live (edit)
This button puts the module into "live edit" mode, where you can play a beat using the four buttons below the main display (+/-/yes/no). In this mode, all random hits are muted, and a metronome sound is played. When you trigger a sample using one of the four buttons, it is added to the current rhythm. If you make a mistake, you can clear the 

### Step (edit)

### A/B
These buttons currently not operational. They are reserved for future firmware updates or user modification (see "hacking" section).

### Save
Pressing this button during normal operation manually saves your current settings (tempo, tuplet mode, clock mode, etc). This may cause a brief glitch in the audio output - this is normal. Note that your settings are also saved automatically whenever the module detects an appropriate period of silence, but you can press this button to save manually for peace of mind. The module loads the most recent set of saved settings when it is powered on. This button functions differently when in live edit or beat edit modes, where it is used to save an edited beat - see the live/step edit sections of the manual for details.

### Beat

### Tempo

### Tuplet

### Menu

### Kit

### T.sig (time signature)

### Clear

### Back


## Synchronisation (clock)


## Error codes


## Updating firmware


## Thoughts about Drumkid


## Hacking