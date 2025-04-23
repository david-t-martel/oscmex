/**
 * @file device_state.c
 * @brief Implementation of functions for accessing device state
 *
 * This file provides access to the current state of the RME device,
 * implementing the functions declared in device_state.h.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // For INFINITY
#include "device.h"
#include "device_state.h"
#include "oscmix.h"

/* External variables from device.c */
extern const struct device *cur_device; // Changed from 'device' to match device.c
extern float **volumes;
extern float **pans;
extern float *output_volumes;
extern bool *output_mutes;
extern bool *input_mutes;
extern float *input_gains;
extern bool *input_phantoms;
extern int *input_reflevels;
extern bool *input_hizs;
extern int *output_reflevels;
extern int sample_rate;
extern int clock_source;
extern int buffer_size;

/* Global device state structure */
static struct devicestate device_state = {0};

/* Initialize the device state structure */
int initDeviceState(const struct device *dev)
{
    if (!dev)
    {
        return -1;
    }

    /* Cleanup any existing state */
    cleanupDeviceState();

    /* Allocate memory for input states */
    device_state.inputs = calloc(dev->inputslen, sizeof(struct inputstate));
    if (!device_state.inputs)
    {
        return -1;
    }

    /* Allocate memory for output states */
    device_state.outputs = calloc(dev->outputslen, sizeof(struct outputstate));
    if (!device_state.outputs)
    {
        free(device_state.inputs);
        device_state.inputs = NULL;
        return -1;
    }

    /* Allocate memory for mixer volumes and pans (input x output matrices) */
    device_state.mixervolume = calloc(dev->inputslen * dev->outputslen, sizeof(float));
    device_state.mixerpan = calloc(dev->inputslen * dev->outputslen, sizeof(float));

    if (!device_state.mixervolume || !device_state.mixerpan)
    {
        cleanupDeviceState();
        return -1;
    }

    /* Initialize with default values */
    for (int i = 0; i < dev->inputslen; i++)
    {
        device_state.inputs[i].gain = 0.0f;
        device_state.inputs[i].phantom = false;
        device_state.inputs[i].hiz = false;
        device_state.inputs[i].mute = false;
        device_state.inputs[i].reflevel = 0;
    }

    for (int i = 0; i < dev->outputslen; i++)
    {
        device_state.outputs[i].volume = 0.0f;
        device_state.outputs[i].mute = false;
        device_state.outputs[i].reflevel = 0;
    }

    /* Initialize mixer with default values */
    for (int i = 0; i < dev->inputslen; i++)
    {
        for (int j = 0; j < dev->outputslen; j++)
        {
            /* Default to -90dB (effectively muted) */
            device_state.mixervolume[i * dev->outputslen + j] = -90.0f;
            /* Default to center pan */
            device_state.mixerpan[i * dev->outputslen + j] = 0.0f;
        }
    }

    /* Set default system values */
    device_state.samplerate = 44100;
    strncpy(device_state.clocksource, "Internal", sizeof(device_state.clocksource) - 1);
    device_state.buffersize = 64;

    return 0;
}

/* Free allocated memory for device state */
void cleanupDeviceState(void)
{
    free(device_state.inputs);
    free(device_state.outputs);
    free(device_state.mixervolume);
    free(device_state.mixerpan);

    device_state.inputs = NULL;
    device_state.outputs = NULL;
    device_state.mixervolume = NULL;
    device_state.mixerpan = NULL;
}

/* Get a pointer to the device state for external use */
struct devicestate *getDeviceState(void)
{
    return &device_state;
}

/* Get the current device structure */
const struct device *getDevice(void)
{
    return cur_device; // Changed from 'device' to match device.c
}

/*
 * Input state getters/setters
 */

float getInputGain(int index)
{
    if (!cur_device || index < 0 || index >= cur_device->inputslen)
        return 0.0f;

    if (!(cur_device->inputs[index].flags & INPUT_GAIN))
        return 0.0f;

    return input_gains[index];
}

void setInputGain(int index, float gain)
{
    if (device_state.inputs && index >= 0 && index < cur_device->inputslen)
    {
        device_state.inputs[index].gain = gain;
    }
}

bool getInputPhantom(int index)
{
    if (!cur_device || index < 0 || index >= cur_device->inputslen)
        return false;

    if (!(cur_device->inputs[index].flags & INPUT_48V))
        return false;

    return input_phantoms[index];
}

void setInputPhantom(int index, bool enabled)
{
    if (device_state.inputs && index >= 0 && index < cur_device->inputslen)
    {
        device_state.inputs[index].phantom = enabled;
    }
}

const char *getInputRefLevel(int index)
{
    static const char *reflevels[] = {
        "Lo Gain", "+4 dBu", "-10 dBV", "Hi Gain"};

    if (!cur_device || index < 0 || index >= cur_device->inputslen)
        return "Unknown";

    if (!(cur_device->inputs[index].flags & INPUT_REFLEVEL))
        return "Not Applicable";

    int level = input_reflevels[index];
    if (level < 0 || level >= (int)(sizeof(reflevels) / sizeof(reflevels[0])))
        return "Invalid";

    return reflevels[level];
}

void setInputRefLevel(int index, int level)
{
    if (device_state.inputs && index >= 0 && index < cur_device->inputslen)
    {
        device_state.inputs[index].reflevel = level;
    }
}

bool getInputHiZ(int index)
{
    if (!cur_device || index < 0 || index >= cur_device->inputslen)
        return false;

    if (!(cur_device->inputs[index].flags & INPUT_HIZ))
        return false;

    return input_hizs[index];
}

void setInputHiZ(int index, bool enabled)
{
    if (device_state.inputs && index >= 0 && index < cur_device->inputslen)
    {
        device_state.inputs[index].hiz = enabled;
    }
}

bool getInputMute(int index)
{
    if (!cur_device || index < 0 || index >= cur_device->inputslen)
        return false;

    return input_mutes[index];
}

void setInputMute(int index, bool muted)
{
    if (device_state.inputs && index >= 0 && index < cur_device->inputslen)
    {
        device_state.inputs[index].mute = muted;
    }
}

/*
 * Output state getters/setters
 */

float getOutputVolume(int index)
{
    if (!cur_device || index < 0 || index >= cur_device->outputslen)
        return 0.0f;

    return output_volumes[index];
}

void setOutputVolume(int index, float volume)
{
    if (device_state.outputs && index >= 0 && index < cur_device->outputslen)
    {
        device_state.outputs[index].volume = volume;
    }
}

bool getOutputMute(int index)
{
    if (!cur_device || index < 0 || index >= cur_device->outputslen)
        return false;

    return output_mutes[index];
}

void setOutputMute(int index, bool muted)
{
    if (device_state.outputs && index >= 0 && index < cur_device->outputslen)
    {
        device_state.outputs[index].mute = muted;
    }
}

const char *getOutputRefLevel(int index)
{
    static const char *reflevels[] = {
        "Hi Gain", "+4 dBu", "-10 dBV", "Lo Gain"};

    if (!cur_device || index < 0 || index >= cur_device->outputslen)
        return "Unknown";

    if (!(cur_device->outputs[index].flags & OUTPUT_REFLEVEL))
        return "Not Applicable";

    int level = output_reflevels[index];
    if (level < 0 || level >= (int)(sizeof(reflevels) / sizeof(reflevels[0])))
        return "Invalid";

    return reflevels[level];
}

void setOutputRefLevel(int index, int level)
{
    if (device_state.outputs && index >= 0 && index < cur_device->outputslen)
    {
        device_state.outputs[index].reflevel = level;
    }
}

/*
 * Mixer state getters/setters
 */

float getMixerVolume(int input, int output)
{
    if (!cur_device || !volumes)
        return -INFINITY;

    if (input < 0 || input >= cur_device->inputslen ||
        output < 0 || output >= cur_device->outputslen)
        return -INFINITY;

    return volumes[input][output];
}

void setMixerVolume(int input, int output, float volume)
{
    if (device_state.mixervolume &&
        input >= 0 && input < cur_device->inputslen &&
        output >= 0 && output < cur_device->outputslen)
    {
        device_state.mixervolume[input * cur_device->outputslen + output] = volume;
    }
}

float getMixerPan(int input, int output)
{
    if (!cur_device || !pans)
        return 0.0f;

    if (input < 0 || input >= cur_device->inputslen ||
        output < 0 || output >= cur_device->outputslen)
        return 0.0f;

    return pans[input][output];
}

void setMixerPan(int input, int output, float pan)
{
    if (device_state.mixerpan &&
        input >= 0 && input < cur_device->inputslen &&
        output >= 0 && output < cur_device->outputslen)
    {
        device_state.mixerpan[input * cur_device->outputslen + output] = pan;
    }
}

/*
 * System state getters/setters
 */

int getSampleRate(void)
{
    static const int sample_rates[] = {
        32000, 44100, 48000, 64000, 88200, 96000, 128000, 176400, 192000};

    if (sample_rate < 0 || sample_rate >= (int)(sizeof(sample_rates) / sizeof(sample_rates[0])))
        return 0;

    return sample_rates[sample_rate];
}

void setSampleRate(int rate)
{
    device_state.samplerate = rate;
}

const char *getClockSource(void)
{
    static const char *clock_sources[] = {
        "Internal", "ADAT", "SPDIF", "AES", "Word Clock", "TCO"};

    if (clock_source < 0 || clock_source >= (int)(sizeof(clock_sources) / sizeof(clock_sources[0])))
        return "Unknown";

    return clock_sources[clock_source];
}

void setClockSource(const char *source)
{
    if (source)
    {
        strncpy(device_state.clocksource, source, sizeof(device_state.clocksource) - 1);
        device_state.clocksource[sizeof(device_state.clocksource) - 1] = '\0';
    }
}

int getBufferSize(void)
{
    static const int buffer_sizes[] = {
        32, 64, 128, 256, 512, 1024, 2048};

    if (buffer_size < 0 || buffer_size >= (int)(sizeof(buffer_sizes) / sizeof(buffer_sizes[0])))
        return 0;

    return buffer_sizes[buffer_size];
}

void setBufferSize(int size)
{
    device_state.buffersize = size;
}

/* Save current device state to a file */
int saveDeviceState(void)
{
    return dumpState(cur_device, &device_state);
}
