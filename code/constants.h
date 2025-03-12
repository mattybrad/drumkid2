#ifndef CONSTANTS_H
#define CONSTANTS_H

#define NUM_SAMPLES 4
#define MAX_SAMPLE_STORAGE 230000 // limited by RAM, 520kB total, ~60kB used by program, 460kB available for samples => 230,000 samples (16-bit)
#define QUARTER_NOTE_STEPS 3360
#define QUARTER_NOTE_STEPS_SEQUENCEABLE 8
#define MAX_TIME_SIGNATURE 8
#define NUM_TUPLET_MODES 4
#define HIT_QUEUE_SIZE 8
#define FADE_OUT 64 // must be smaller than schedule ahead time(?) to work properly
#define MAX_BEAT_HITS 128

#endif