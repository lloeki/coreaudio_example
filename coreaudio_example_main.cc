#include "coreaudio_example.h"

static double gtheta = 0;

OSStatus tone(void *inRef, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData) {
    // Fixed amplitude is good enough for our purposes
    const double amplitude = 0.25;

    // Get the tone parameters out of the static var
    // could be stored in inRef
    double theta = gtheta;
    double theta_increment = 2.0 * M_PI * 440 / 44100;

    // This is a mono tone generator so we only need the first buffer
    const int channel = 0;
    Float32 *buffer = (Float32 *)ioData->mBuffers[channel].mData;

    // Generate the samples
    for (UInt32 frame = 0; frame < inNumberFrames; frame++) 
    {
        buffer[frame] = sin(theta) * amplitude;

        theta += theta_increment;
        if (theta > 2.0 * M_PI)
        {
            theta -= 2.0 * M_PI;
        }
    }

    printf("%f\n", theta);
    gtheta = theta;

    return noErr;
}

int main() {
    coreaudio_example::init();

    AURenderCallbackStruct callback;
    callback.inputProc = tone;
    callback.inputProcRefCon = nullptr; // could pass a struct to store state like theta

    bool ok = coreaudio_example::open_audio(coreaudio_example::FMT_FLOAT, 44100, 1, &callback);
    if (!ok) {
        coreaudio_example::cleanup();
        fprintf(stderr, "failed to open\n");
        return 1;
    }

    coreaudio_example::set_volume(100);

    // sleep 1s, rendering via callback happens in another thread
    usleep(1000000);

    coreaudio_example::close_audio();
    coreaudio_example::cleanup();
    return 0;
}
