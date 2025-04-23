#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <stdbool.h>
#include <math.h> // Add this for INFINITY
#include "device.h"

/* Device state representation for inputs */
struct inputstate
{
    float gain;
    bool phantom;
    bool hiz;
    bool mute;
    int reflevel;
};

/* Device state representation for outputs */
struct outputstate
{
    float volume;
    bool mute;
    int reflevel;
};

/* Complete device state structure */
struct devicestate
{
    struct inputstate *inputs;
    struct outputstate *outputs;
    float *mixervolume; /* 2D array: [input][output] */
    float *mixerpan;    /* 2D array: [input][output] */
    int samplerate;
    char clocksource[32];
    int buffersize;
};

/* Device state initialization and cleanup */
int initDeviceState(const struct device *dev);
void cleanupDeviceState(void);
struct devicestate *getDeviceState(void);

/* Current device accessor */
const struct device *getDevice(void);

/* Input state getters/setters */
float getInputGain(int index);
void setInputGain(int index, float gain);
bool getInputPhantom(int index);
void setInputPhantom(int index, bool enabled);
const char *getInputRefLevel(int index);
void setInputRefLevel(int index, int level);
bool getInputHiZ(int index);
void setInputHiZ(int index, bool enabled);
bool getInputMute(int index);
void setInputMute(int index, bool muted);

/* Output state getters/setters */
float getOutputVolume(int index);
void setOutputVolume(int index, float volume);
bool getOutputMute(int index);
void setOutputMute(int index, bool muted);
const char *getOutputRefLevel(int index);
void setOutputRefLevel(int index, int level);

/* Mixer state getters/setters */
float getMixerVolume(int input, int output);
void setMixerVolume(int input, int output, float volume);
float getMixerPan(int input, int output);
void setMixerPan(int input, int output, float pan);

/* System state getters/setters */
int getSampleRate(void);
void setSampleRate(int rate);
const char *getClockSource(void);
void setClockSource(const char *source);
int getBufferSize(void);
void setBufferSize(int size);

/* Save current device state to a file */
int saveDeviceState(void);

#endif /* DEVICE_STATE_H */
