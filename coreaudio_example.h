/* Header file for example code.
   We include CoreAudio and AudioUnit framework headers directly. */

#include <cstdio>
#include <cstdlib>
#include <pthread.h>

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

namespace coreaudio_example {

/* these format constants are based on the ones we use in audacious.
   S16_LE means signed 16-bit pcm, little-endian. */
enum format_type {
    FMT_S16_LE,
    FMT_S16_BE,
    FMT_S32_LE,
    FMT_S32_BE,
    FMT_FLOAT
};

bool init (void);
void cleanup (void);
void set_volume (int value);
bool open_audio (enum format_type format, int rate, int chan,
                 AURenderCallbackStruct * callback);
void close_audio (void);
void pause_audio (bool paused);

#define VOLUME_RANGE (40) /* decibels */

}
