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

#include "device_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Device state variables
static const struct device *cur_device = NULL;
static struct input_state *inputs = NULL;
static struct input_state *playbacks = NULL;
static struct output_state *outputs = NULL;
static struct
{
    int status;
    int position;
    int time;
    int usberrors;
    int usbload;
    float totalspace;
    float freespace;
    struct durecfile_state *files;
    size_t fileslen;
    int file;
    int recordtime;
    int index;
    int next;
    int playmode;
} durec = {.index = -1};
static struct
{
    int vers;
    int load;
} dsp = {-1, -1};
static bool refreshing = false;

const struct device *getDevice(void)
{
    return cur_device;
}

bool refreshing_state(int value)
{
    if (value >= 0)
        refreshing = (value != 0);
    return refreshing;
}

void get_dsp_state(int *vers, int *load)
{
    if (vers)
        *vers = dsp.vers;
    if (load)
        *load = dsp.load;
}

void set_dsp_state(int vers, int load)
{
    if (vers >= 0)
        dsp.vers = vers;
    if (load >= 0)
        dsp.load = load;
}

struct input_state *get_input_state(int index)
{
    if (!inputs || index < 0 || index >= cur_device->inputslen)
        return NULL;
    return &inputs[index];
}

struct input_state *get_playback_state(int index)
{
    if (!playbacks || index < 0 || index >= cur_device->outputslen)
        return NULL;
    return &playbacks[index];
}

struct output_state *get_output_state(int index)
{
    if (!outputs || index < 0 || index >= cur_device->outputslen)
        return NULL;
    return &outputs[index];
}

void get_durec_state(int *status, int *position, int *time,
                     int *usberrors, int *usbload,
                     float *totalspace, float *freespace,
                     int *file, int *recordtime, int *index,
                     int *next, int *playmode)
{
    if (status)
        *status = durec.status;
    if (position)
        *position = durec.position;
    if (time)
        *time = durec.time;
    if (usberrors)
        *usberrors = durec.usberrors;
    if (usbload)
        *usbload = durec.usbload;
    if (totalspace)
        *totalspace = durec.totalspace;
    if (freespace)
        *freespace = durec.freespace;
    if (file)
        *file = durec.file;
    if (recordtime)
        *recordtime = durec.recordtime;
    if (index)
        *index = durec.index;
    if (next)
        *next = durec.next;
    if (playmode)
        *playmode = durec.playmode;
}

void set_durec_state(int status, int position, int time,
                     int usberrors, int usbload,
                     float totalspace, float freespace,
                     int file, int recordtime, int index,
                     int next, int playmode)
{
    if (status >= 0)
        durec.status = status;
    if (position >= 0)
        durec.position = position;
    if (time >= 0)
        durec.time = time;
    if (usberrors >= 0)
        durec.usberrors = usberrors;
    if (usbload >= 0)
        durec.usbload = usbload;
    if (totalspace >= 0)
        durec.totalspace = totalspace;
    if (freespace >= 0)
        durec.freespace = freespace;
    if (file >= 0)
        durec.file = file;
    if (recordtime >= 0)
        durec.recordtime = recordtime;
    if (index >= 0)
        durec.index = index;
    if (next >= 0)
        durec.next = next;
    if (playmode >= 0)
        durec.playmode = playmode;
}

struct durecfile_state *get_durec_files(size_t *fileslen)
{
    if (fileslen)
        *fileslen = durec.fileslen;
    return durec.files;
}

int set_durec_files_length(size_t fileslen)
{
    struct durecfile_state *new_files;

    if (fileslen == durec.fileslen)
        return 0;

    new_files = realloc(durec.files, fileslen * sizeof(struct durecfile_state));
    if (!new_files && fileslen > 0)
        return -1;

    durec.files = new_files;

    if (fileslen > durec.fileslen)
    {
        memset(durec.files + durec.fileslen, 0,
               (fileslen - durec.fileslen) * sizeof(struct durecfile_state));
    }

    durec.fileslen = fileslen;
    if (durec.index >= durec.fileslen)
        durec.index = -1;

    return 0;
}

int device_state_init(const struct device *device)
{
    int i, j;

    cur_device = device;
    if (!cur_device)
        return -1;

    // Allocate input, playback, and output arrays
    inputs = calloc(cur_device->inputslen, sizeof(struct input_state));
    playbacks = calloc(cur_device->outputslen, sizeof(struct input_state));
    outputs = calloc(cur_device->outputslen, sizeof(struct output_state));

    if (!inputs || !playbacks || !outputs)
    {
        device_state_cleanup();
        return -1;
    }

    // Initialize stereo state for playbacks
    for (i = 0; i < cur_device->outputslen; ++i)
    {
        struct output_state *out;

        playbacks[i].stereo = true;
        out = &outputs[i];

        // Allocate mix settings array for each output
        out->mix = calloc(cur_device->inputslen + cur_device->outputslen,
                          sizeof(struct mix_state));
        if (!out->mix)
        {
            device_state_cleanup();
            return -1;
        }

        // Initialize all mix volumes to -650 (equivalent to -65dB)
        for (j = 0; j < cur_device->inputslen + cur_device->outputslen; ++j)
            out->mix[j].vol = -650;
    }

    // Initialize DURec state
    durec.index = -1;
    dsp.vers = -1;
    dsp.load = -1;

    return 0;
}

void device_state_cleanup(void)
{
    int i;

    if (outputs)
    {
        for (i = 0; i < cur_device->outputslen; ++i)
        {
            free(outputs[i].mix);
        }
        free(outputs);
        outputs = NULL;
    }

    free(inputs);
    inputs = NULL;

    free(playbacks);
    playbacks = NULL;

    free(durec.files);
    durec.files = NULL;
    durec.fileslen = 0;
}

void dumpDeviceState(void)
{
    int i, j;

    printf("Device: %s\n", cur_device->name);
    printf("DSP: version=%d, load=%d\n", dsp.vers, dsp.load);

    printf("\nInputs:\n");
    for (i = 0; i < cur_device->inputslen; ++i)
    {
        printf("  Input %d: stereo=%d, mute=%d, width=%.2f\n",
               i + 1, inputs[i].stereo, inputs[i].mute, inputs[i].width);
    }

    printf("\nOutputs:\n");
    for (i = 0; i < cur_device->outputslen; ++i)
    {
        printf("  Output %d: stereo=%d\n", i + 1, outputs[i].stereo);

        // Print a few mix settings as an example
        printf("    Mix settings (sample):\n");
        for (j = 0; j < 3 && j < cur_device->inputslen; ++j)
        {
            printf("      Input %d: vol=%d, pan=%d\n",
                   j + 1, outputs[i].mix[j].vol, outputs[i].mix[j].pan);
        }
    }

    printf("\nDURec:\n");
    printf("  Status: %d, Position: %d, Time: %d\n",
           durec.status, durec.position, durec.time);
    printf("  USB: errors=%d, load=%d\n", durec.usberrors, durec.usbload);
    printf("  Space: total=%.1f GB, free=%.1f GB\n",
           durec.totalspace, durec.freespace);
    printf("  Files: count=%zu, current=%d\n", durec.fileslen, durec.file);

    if (durec.fileslen > 0 && durec.files)
    {
        printf("  File list (sample):\n");
        for (i = 0; i < 3 && i < durec.fileslen; ++i)
        {
            printf("    File %d: name='%s', sr=%lu, ch=%u, len=%u\n",
                   i, durec.files[i].name, durec.files[i].samplerate,
                   durec.files[i].channels, durec.files[i].length);
        }
    }
}

int dumpConfig(void)
{
    char filename[256];
    time_t now;
    struct tm *timeinfo;
    FILE *f;

    // Get current time for filename
    time(&now);
    timeinfo = localtime(&now);

// Create filename with date/time stamp
#ifdef _WIN32
    char *appdata = getenv("APPDATA");
    if (!appdata)
        appdata = ".";

    snprintf(filename, sizeof(filename),
             "%s\\OSCMix\\device_config\\audio-device_%s_date-time_%04d%02d%02d-%02d%02d%02d.json",
             appdata, cur_device->name,
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    // Create directories if needed
    char dirname[256];
    snprintf(dirname, sizeof(dirname), "%s\\OSCMix", appdata);
    mkdir(dirname);
    snprintf(dirname, sizeof(dirname), "%s\\OSCMix\\device_config", appdata);
    mkdir(dirname);
#else
    char *home = getenv("HOME");
    if (!home)
        home = ".";

    snprintf(filename, sizeof(filename),
             "%s/device_config/audio-device_%s_date-time_%04d%02d%02d-%02d%02d%02d.json",
             home, cur_device->name,
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    // Create directories if needed
    char dirname[256];
    snprintf(dirname, sizeof(dirname), "%s/device_config", home);
    mkdir(dirname, 0755);
#endif

    // Open file for writing
    f = fopen(filename, "w");
    if (!f)
    {
        perror("Failed to create config file");
        return -1;
    }

    // Write JSON header
    fprintf(f, "{\n");
    fprintf(f, "  \"device\": \"%s\",\n", cur_device->name);
    fprintf(f, "  \"timestamp\": \"%04d-%02d-%02d %02d:%02d:%02d\",\n",
            timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    // Write DSP state
    fprintf(f, "  \"dsp\": {\n");
    fprintf(f, "    \"version\": %d,\n", dsp.vers);
    fprintf(f, "    \"load\": %d\n", dsp.load);
    fprintf(f, "  },\n");

    // Write inputs state (just a few key parameters)
    fprintf(f, "  \"inputs\": [\n");
    for (int i = 0; i < cur_device->inputslen; ++i)
    {
        fprintf(f, "    {\n");
        fprintf(f, "      \"index\": %d,\n", i);
        fprintf(f, "      \"stereo\": %s,\n", inputs[i].stereo ? "true" : "false");
        fprintf(f, "      \"mute\": %s,\n", inputs[i].mute ? "true" : "false");
        fprintf(f, "      \"width\": %.2f\n", inputs[i].width);
        fprintf(f, "    }%s\n", (i < cur_device->inputslen - 1) ? "," : "");
    }
    fprintf(f, "  ],\n");

    // Write outputs state
    fprintf(f, "  \"outputs\": [\n");
    for (int i = 0; i < cur_device->outputslen; ++i)
    {
        fprintf(f, "    {\n");
        fprintf(f, "      \"index\": %d,\n", i);
        fprintf(f, "      \"stereo\": %s\n", outputs[i].stereo ? "true" : "false");
        fprintf(f, "    }%s\n", (i < cur_device->outputslen - 1) ? "," : "");
    }
    fprintf(f, "  ]\n");

    // Close JSON object
    fprintf(f, "}\n");

    fclose(f);
    printf("Configuration saved to: %s\n", filename);
    return 0;
}
