/*
CoreAudio, or how to actually use the worst documented audio API in history.

Recently I started working on a CoreAudio plugin for audacious, to replace the
old one which was removed in Audacious 3.2, since the mac port was abandoned
due to the fact that Gtk+ is horrible on mac. Instead of updating the old
CoreAudio plugin which was very limited and consisted of bad code ported from
the XMMS days, I decided to start from scratch, using the simple SDL output
plugin as a model.

The API itself is okay, but the documentation is misleading. For example, they
encourage you to set up an Audio Unit Graph for simple audio playback with
audio control. At least on OS X, this isn't really necessary (thankfully).
Since this isn't necessary, I will show you how to do it the easy way, since
everyone seems to like to over-complicate things.

These examples assume you have a 'pump thread' which is pumping audio to a
circular buffer. Implementing that is not covered here, but isn't really very
hard to do.
*/

/*
This is the API we will be implementing, it is pretty straight-forward. There
are 6 functions which are provided:

init: start up the audio system
cleanup: shut down the audio system
set_volume: set the volume
open_audio: set up a stream for playback
close_audio: shuts down the playback code
pause_audio: pauses the playback code

The API will handle format conversion between your playback code's preferred
format type and CoreAudio's native format -- float32, non-interleaved linear
pcm. We will do this using a lookup table based on the enum...
*/

/* CoreAudio utility functions, public domain.
   http://kaniini.dereferenced.org/2014/08/31/CoreAudio-sucks.html */

#include "coreaudio_example.h"

namespace coreaudio_example {

struct CoreAudioFormatDescriptionMap {
    enum format_type type;
    int bits_per_sample;
    int bytes_per_sample;
    unsigned int flags;
};
static struct CoreAudioFormatDescriptionMap format_map[] = {
    {FMT_S16_LE, 16, sizeof (int16_t), kAudioFormatFlagIsSignedInteger},
    {FMT_S16_BE, 16, sizeof (int16_t), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian},
    {FMT_S32_LE, 32, sizeof (int32_t), kAudioFormatFlagIsSignedInteger},
    {FMT_S32_BE, 32, sizeof (int32_t), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian},
    {FMT_FLOAT,  32, sizeof (float),   kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved},
};

/*
This is our lookup table which handles looking up the format specification
attributes. There are a few members in each entry which are interesting:

- type: this is our format type (i.e. FMT_S16_LE) which we will be looking up
- bits_per_sample: this is the number of bits of data that should be included
  in a sample. CoreAudio takes the lower bits of data, not the higher bits of
  data. So if you were to implement 24-bit PCM support, you would pack the data
  into a series of 32-bit integers and tell CoreAudio to use 24 bits of each
  sample for PCM data.
- bytes_per_sample: this is the size in bytes of the type which contains each
  sample.
- flags: This is a bitfield of many flags, the ones mentioned here are:
- kAudioFormatIsSignedInteger: The audio format is a signed integer. If this is
  not set, CoreAudio assumes it's unsigned.
- kAudioFormatIsBigEndian: The audio format is in big-endian notation. If this
  is not set, CoreAudio assumes it's little-endian.
- kAudioFormatIsFloat: The audio format is in 32-bit floating point format.

We now continue with actually initializing our output unit so we can use it.
*/

static AudioComponent output_comp;
static AudioComponentInstance output_instance;

bool init (void)
{
    /* open the default audio device */
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    output_comp = AudioComponentFindNext (nullptr, & desc);
    if (! output_comp)
    {
        fprintf (stderr, "Failed to open default audio device.\n");
        return false;
    }

    if (AudioComponentInstanceNew (output_comp, & output_instance))
    {
        fprintf (stderr, "Failed to open default audio device.\n");
        return false;
    }

    return true;
}

void cleanup (void)
{
    AudioComponentInstanceDispose (output_instance);
}

/*
The init () and cleanup () routines handle bringing up CoreAudio in the app.
This gives you an output unit you can send data to using a callback. Now we
should actually set up the unit for playback...
*/

bool open_audio (enum format_type format, int rate, int chan, AURenderCallbackStruct * callback)
{
    struct CoreAudioFormatDescriptionMap * m = nullptr;

    for (struct CoreAudioFormatDescriptionMap it : format_map)
    {
        if (it.type == format)
        {
            m = & it;
            break;
        }
    }

    if (! m)
    {
        fprintf (stderr, "The requested audio format %d is unsupported.\n", format);
        return false;
    }

    if (AudioUnitInitialize (output_instance))
    {
        fprintf (stderr, "Unable to initialize audio unit instance\n");
        return false;
    }

    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = rate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = m->flags;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mChannelsPerFrame = chan;
    streamFormat.mBitsPerChannel = m->bits_per_sample;
    streamFormat.mBytesPerPacket = chan * m->bytes_per_sample;
    streamFormat.mBytesPerFrame = chan * m->bytes_per_sample;

    printf ("Stream format:\n");
    printf (" Channels: %d\n", streamFormat.mChannelsPerFrame);
    printf (" Sample rate: %f\n", streamFormat.mSampleRate);
    printf (" Bits per channel: %d\n", streamFormat.mBitsPerChannel);
    printf (" Bytes per frame: %d\n", streamFormat.mBytesPerFrame);

    if (AudioUnitSetProperty (output_instance, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof(streamFormat)))
    {
        fprintf (stderr, "Failed to set audio unit input property.\n");
        return false;
    }

    if (AudioUnitSetProperty (output_instance, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, callback, sizeof (AURenderCallbackStruct)))
    {
        fprintf (stderr, "Unable to attach an IOProc to the selected audio unit.\n");
        return false;
    }

    if (AudioOutputUnitStart (output_instance))
    {
        printf ("Unable to start audio unit.\n");
        return false;
    }

    return true;
}

void close_audio (void)
{
    AudioOutputUnitStop (output_instance);
}

/*
At this point you should have full playback with callback to your callback and
shutdown. Now to implement volume control...
*/

/* value is 0..100, the actual applied volume is based on a natual decibel scale. */
void set_volume (int value)
{
    float factor = (value == 0) ? 0.0 : powf (10, (float) VOLUME_RANGE * (value - 100) / 100 / 20);

    /* lots of pain concerning controlling application volume can be avoided
     * with this one neat trick... */
    AudioUnitSetParameter (output_instance, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, factor, 0);
}

/*
Two things here:

Apple says you should set up an AUGraph for doing something as simple as controlling output volume. I say that is unnecessary. There is a lot of misinformation here as well, that kHALOutputParam_Volume sets the system volume; it doesn't. It sets the individual output volume on the sound server, coreaudiod.

The reason for the powf () and the scary math is to give a logarithmic scale for tapering the volume down, similar to what would be observed in an actual stereo system. If you don't want this, just do float factor = value / 100.0.

Now to handle pausing...
*/

void pause_audio (bool paused)
{
    if (paused)
        AudioOutputUnitStop (output_instance);
    else
    {
        if (AudioOutputUnitStart (output_instance))
        {
            fprintf (stderr, "Unable to restart audio unit after pausing.\n");
            close_audio ();
        }
    }
}

} /* namespace coreaudio_example */

/*
That is all there is to the actual CoreAudio lowlevel operations. The rest is
up to the reader. In the callback function you just copy your buffer data to
ioData->mBuffers[0].mData. The amount of data requested is at
ioData->mBuffers[0].mDataByteSize.

Hopefully this documentation is useful to somebody else, I mostly wrote it for
my own reference. Usually I get to just deal with FMOD which is much more
pleasant (ha ha).
*/
