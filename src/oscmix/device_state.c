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
#include "platform.h"
#include "oscmix.h"
#include "dump.h"             // Include dump.h for dumping functions
#include "device_observers.h" // Include the device observers header
#include "logging.h"

/* External variables from device.c */
extern const struct device *cur_device;
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
static bool refreshing = false;

// Device state variables
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

/* Initialize the device state structure */
int device_state_init(const struct device *device)
{
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
    for (int i = 0; i < cur_device->outputslen; ++i)
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
        for (int j = 0; j < cur_device->inputslen + cur_device->outputslen; ++j)
            out->mix[j].vol = -650;
    }

    return 0;
}

/* Free allocated memory for device state */
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

/* Get a pointer to the device state for external use */
struct devicestate *getDeviceState(void)
{
    return &device_state;
}

/* Get the current device structure */
const struct device *getDevice(void)
{
    return cur_device;
}

/* Get/Set refreshing state */
bool refreshing_state(int value)
{
    if (value >= 0)
        refreshing = (value != 0);
    return refreshing;
}

/* Dump the current device state to the console */
void dumpDeviceState(void)
{
    if (!cur_device)
    {
        fprintf(stderr, "No device initialized\n");
        return;
    }

    printf("Device: %s (ID: %s, Version: %d)\n",
           cur_device->name, cur_device->id, cur_device->version);

    // Print inputs
    printf("\nInputs:\n");
    for (int i = 0; i < cur_device->inputslen; i++)
    {
        printf("  Input %d: %s (flags: 0x%x)\n",
               i + 1, cur_device->inputs[i].name, cur_device->inputs[i].flags);

        // Print input parameters based on device flags
        if (cur_device->inputs[i].flags & INPUT_GAIN)
        {
            printf("    Gain: %.1f dB\n", input_gains[i]);
        }
        if (cur_device->inputs[i].flags & INPUT_48V)
        {
            printf("    Phantom Power: %s\n", input_phantoms[i] ? "On" : "Off");
        }
        if (cur_device->inputs[i].flags & INPUT_HIZ)
        {
            printf("    Hi-Z: %s\n", input_hizs[i] ? "On" : "Off");
        }
        printf("    Mute: %s\n", input_mutes[i] ? "On" : "Off");
    }

    // Print outputs
    printf("\nOutputs:\n");
    for (int i = 0; i < cur_device->outputslen; i++)
    {
        printf("  Output %d: %s (flags: 0x%x)\n",
               i + 1, cur_device->outputs[i].name, cur_device->outputs[i].flags);
        printf("    Volume: %.1f dB\n", output_volumes[i]);
        printf("    Mute: %s\n", output_mutes[i] ? "On" : "Off");
    }

    // Print some mixer settings as examples
    printf("\nMixer (sample):\n");
    if (volumes && pans)
    {
        for (int i = 0; i < 2 && i < cur_device->outputslen; i++)
        {
            for (int j = 0; j < 2 && j < cur_device->inputslen; j++)
            {
                printf("  Input %d -> Output %d: %.1f dB, Pan: %.2f\n",
                       j + 1, i + 1, volumes[j][i], pans[j][i]);
            }
        }
    }

    // Print system settings
    printf("\nSystem:\n");
    printf("  Sample Rate: %d\n", getSampleRate());
    printf("  Clock Source: %s\n", getClockSource());
    printf("  Buffer Size: %d\n", getBufferSize());
}

/* Save device configuration to a file */
int dumpConfig(void)
{
    if (!cur_device)
    {
        fprintf(stderr, "No device initialized\n");
        return -1;
    }

    // Get application data directory
    char app_home[256];
    if (platform_get_app_data_dir(app_home, sizeof(app_home)) != 0)
    {
        fprintf(stderr, "Failed to get application data directory\n");
        return -1;
    }

    // Create the device_config directory path
    char config_dir[512];
    if (platform_path_join(config_dir, sizeof(config_dir), app_home,
                           "OSCMix" PLATFORM_PATH_SEPARATOR_STR "device_config") != 0)
    {
        fprintf(stderr, "Failed to create config directory path\n");
        return -1;
    }

    // Ensure directory exists
    if (platform_ensure_directory(config_dir) != 0)
    {
        fprintf(stderr, "Failed to create directory: %s\n", config_dir);
        return -1;
    }

    // Generate timestamp for filename
    char date_str[32];
    if (platform_format_time(date_str, sizeof(date_str), "%Y%m%d-%H%M%S") != 0)
    {
        fprintf(stderr, "Failed to format current time\n");
        return -1;
    }

    // Sanitize device name for filename
    char safe_name[64];
    if (platform_create_valid_filename(cur_device->name, safe_name, sizeof(safe_name)) != 0)
    {
        fprintf(stderr, "Failed to create valid filename\n");
        return -1;
    }

    // Create full filepath
    char filename[768];
    snprintf(filename, sizeof(filename), "%s%caudio-device_%s_date-time_%s.json",
             config_dir, PLATFORM_PATH_SEPARATOR, safe_name, date_str);

    // Open file using platform abstraction
    FILE *file = platform_fopen(filename, "w");
    if (!file)
    {
        fprintf(stderr, "Failed to open file for writing: %s\n", filename);
        return -1;
    }

    // Write JSON header
    fprintf(file, "{\n");
    fprintf(file, "  \"device\": \"%s\",\n", cur_device->name);
    fprintf(file, "  \"timestamp\": \"%s\",\n", date_str);

    // Write system settings
    fprintf(file, "  \"system\": {\n");
    fprintf(file, "    \"sample_rate\": %d,\n", getSampleRate());
    fprintf(file, "    \"clock_source\": \"%s\",\n", getClockSource());
    fprintf(file, "    \"buffer_size\": %d\n", getBufferSize());
    fprintf(file, "  },\n");

    // Write inputs state
    fprintf(file, "  \"inputs\": [\n");
    for (int i = 0; i < cur_device->inputslen; ++i)
    {
        fprintf(file, "    {\n");
        fprintf(file, "      \"index\": %d,\n", i);
        fprintf(file, "      \"name\": \"%s\",\n", cur_device->inputs[i].name);
        fprintf(file, "      \"flags\": %d,\n", cur_device->inputs[i].flags);

        if (cur_device->inputs[i].flags & INPUT_GAIN)
        {
            fprintf(file, "      \"gain\": %.1f,\n", input_gains[i]);
        }
        if (cur_device->inputs[i].flags & INPUT_48V)
        {
            fprintf(file, "      \"phantom\": %s,\n", input_phantoms[i] ? "true" : "false");
        }
        if (cur_device->inputs[i].flags & INPUT_HIZ)
        {
            fprintf(file, "      \"hiz\": %s,\n", input_hizs[i] ? "true" : "false");
        }

        fprintf(file, "      \"mute\": %s\n", input_mutes[i] ? "true" : "false");
        fprintf(file, "    }%s\n", (i < cur_device->inputslen - 1) ? "," : "");
    }
    fprintf(file, "  ],\n");

    // Write outputs state
    fprintf(file, "  \"outputs\": [\n");
    for (int i = 0; i < cur_device->outputslen; ++i)
    {
        fprintf(file, "    {\n");
        fprintf(file, "      \"index\": %d,\n", i);
        fprintf(file, "      \"name\": \"%s\",\n", cur_device->outputs[i].name);
        fprintf(file, "      \"volume\": %.1f,\n", output_volumes[i]);
        fprintf(file, "      \"mute\": %s\n", output_mutes[i] ? "true" : "false");
        fprintf(file, "    }%s\n", (i < cur_device->outputslen - 1) ? "," : "");
    }
    fprintf(file, "  ]\n");

    // Close JSON object
    fprintf(file, "}\n");

    fclose(file);
    printf("Configuration saved to: %s\n", filename);
    return 0;
}

/* Implementation of getter functions for device parameters */

int getSampleRate(void)
{
    static const int sample_rates[] = {
        32000, 44100, 48000, 64000, 88200, 96000, 128000, 176400, 192000};

    if (sample_rate < 0 || sample_rate >= (int)(sizeof(sample_rates) / sizeof(sample_rates[0])))
        return 0;

    return sample_rates[sample_rate];
}

const char *getClockSource(void)
{
    static const char *clock_sources[] = {
        "Internal", "ADAT", "SPDIF", "AES", "Word Clock", "TCO"};

    if (clock_source < 0 || clock_source >= (int)(sizeof(clock_sources) / sizeof(clock_sources[0])))
        return "Unknown";

    return clock_sources[clock_source];
}

int getBufferSize(void)
{
    static const int buffer_sizes[] = {
        32, 64, 128, 256, 512, 1024, 2048};

    if (buffer_size < 0 || buffer_size >= (int)(sizeof(buffer_sizes) / sizeof(buffer_sizes[0])))
        return 0;

    return buffer_sizes[buffer_size];
}

float getInputGain(int index)
{
    if (!cur_device || !input_gains || index < 0 || index >= cur_device->inputslen)
        return 0.0f;

    return input_gains[index];
}

bool getInputPhantom(int index)
{
    if (!cur_device || !input_phantoms || index < 0 || index >= cur_device->inputslen)
        return false;

    return input_phantoms[index];
}

const char *getInputRefLevel(int index)
{
    static const char *reflevels[] = {
        "Lo Gain", "+4 dBu", "-10 dBV", "Hi Gain"};

    if (!cur_device || !input_reflevels || index < 0 || index >= cur_device->inputslen)
        return "Unknown";

    int level = input_reflevels[index];
    if (level < 0 || level >= (int)(sizeof(reflevels) / sizeof(reflevels[0])))
        return "Invalid";

    return reflevels[level];
}

bool getInputHiZ(int index)
{
    if (!cur_device || !input_hizs || index < 0 || index >= cur_device->inputslen)
        return false;

    return input_hizs[index];
}

bool getInputMute(int index)
{
    if (!cur_device || !input_mutes || index < 0 || index >= cur_device->inputslen)
        return false;

    return input_mutes[index];
}

float getOutputVolume(int index)
{
    if (!cur_device || !output_volumes || index < 0 || index >= cur_device->outputslen)
        return 0.0f;

    return output_volumes[index];
}

bool getOutputMute(int index)
{
    if (!cur_device || !output_mutes || index < 0 || index >= cur_device->outputslen)
        return false;

    return output_mutes[index];
}

const char *getOutputRefLevel(int index)
{
    static const char *reflevels[] = {
        "Hi Gain", "+4 dBu", "-10 dBV", "Lo Gain"};

    if (!cur_device || !output_reflevels || index < 0 || index >= cur_device->outputslen)
        return "Unknown";

    int level = output_reflevels[index];
    if (level < 0 || level >= (int)(sizeof(reflevels) / sizeof(reflevels[0])))
        return "Invalid";

    return reflevels[level];
}

float getMixerVolume(int input, int output)
{
    if (!cur_device || !volumes ||
        input < 0 || input >= cur_device->inputslen ||
        output < 0 || output >= cur_device->outputslen)
        return -INFINITY;

    return volumes[input][output];
}

float getMixerPan(int input, int output)
{
    if (!cur_device || !pans ||
        input < 0 || input >= cur_device->inputslen ||
        output < 0 || output >= cur_device->outputslen)
        return 0.0f;

    return pans[input][output];
}

void get_dsp_state(int *vers, int *load)
{
    if (vers)
        *vers = dsp.vers;
    if (load)
        *load = dsp.load;
}

// Modify set_dsp_state to use observers
void set_dsp_state(int vers, int load)
{
    int changed = 0;

    if (vers >= 0 && vers != dsp.vers)
    {
        dsp.vers = vers;
        changed = 1;
    }

    if (load >= 0 && load != dsp.load)
    {
        dsp.load = load;
        changed = 1;
    }

    // Notify observers if anything changed and not in refresh mode
    if (changed && !refreshing_state(-1))
    {
        notify_dsp_state_changed(dsp.vers, dsp.load);
    }
}

// Similarly update other state change functions
void set_durec_state(int status, int position, int time, int usberrors, int usbload,
                     float totalspace, float freespace, int file, int recordtime,
                     int index, int next, int playmode)
{
    int changed = 0;

    if (status >= 0 && status != durec.status)
    {
        durec.status = status;
        changed = 1;
    }

    if (position >= 0 && position != durec.position)
    {
        durec.position = position;
        changed = 1;
    }

    // Set other parameters...

    // Notify observers if status or position changed and not in refresh mode
    if (changed && !refreshing_state(-1))
    {
        notify_durec_status_changed(durec.status, durec.position);
    }
}

// Add functions to explicitly update specific parameters with notifications

/**
 * @brief Update input parameters and notify observers
 *
 * @param index Input channel index
 * @param gain New gain value (-1 to leave unchanged)
 * @param phantom New phantom power state (-1 to leave unchanged)
 * @param hiz New high impedance state (-1 to leave unchanged)
 * @param mute New mute state (-1 to leave unchanged)
 * @return 0 on success, non-zero on failure
 */
int update_input(int index, float gain, int phantom, int hiz, int mute)
{
    if (!cur_device || !input_gains || index < 0 || index >= cur_device->inputslen)
        return -1;

    int changed = 0;

    // Update gain if provided
    if (gain >= 0.0f && gain != input_gains[index])
    {
        input_gains[index] = gain;
        changed = 1;
    }

    // Update phantom if provided
    if (phantom >= 0 && (bool)phantom != input_phantoms[index])
    {
        input_phantoms[index] = (bool)phantom;
        changed = 1;
    }

    // Update hiz if provided
    if (hiz >= 0 && (bool)hiz != input_hizs[index])
    {
        input_hizs[index] = (bool)hiz;
        changed = 1;
    }

    // Update mute if provided
    if (mute >= 0 && (bool)mute != input_mutes[index])
    {
        input_mutes[index] = (bool)mute;
        changed = 1;
    }

    // Notify observers if anything changed and not in refresh mode
    if (changed && !refreshing_state(-1))
    {
        notify_input_changed(index, input_gains[index], input_phantoms[index],
                             input_hizs[index], input_mutes[index]);
    }

    return 0;
}

/**
 * @brief Update output parameters and notify observers
 *
 * @param index Output channel index
 * @param volume New volume value (-1 to leave unchanged)
 * @param mute New mute state (-1 to leave unchanged)
 * @return 0 on success, non-zero on failure
 */
int update_output(int index, float volume, int mute)
{
    if (!cur_device || !output_volumes || index < 0 || index >= cur_device->outputslen)
        return -1;

    int changed = 0;

    // Update volume if provided
    if (volume >= 0.0f && volume != output_volumes[index])
    {
        output_volumes[index] = volume;
        changed = 1;
    }

    // Update mute if provided
    if (mute >= 0 && (bool)mute != output_mutes[index])
    {
        output_mutes[index] = (bool)mute;
        changed = 1;
    }

    // Notify observers if anything changed and not in refresh mode
    if (changed && !refreshing_state(-1))
    {
        notify_output_changed(index, output_volumes[index], output_mutes[index]);
    }

    return 0;
}

/**
 * @brief Update mixer parameters and notify observers
 *
 * @param input Input channel index
 * @param output Output channel index
 * @param volume New volume value (negative to leave unchanged)
 * @param pan New pan value (negative to leave unchanged)
 * @return 0 on success, non-zero on failure
 */
int update_mixer(int input, int output, float volume, float pan)
{
    if (!cur_device || !volumes || !pans ||
        input < 0 || input >= cur_device->inputslen ||
        output < 0 || output >= cur_device->outputslen)
        return -1;

    int changed = 0;

    // Update volume if provided
    if (volume > -INFINITY && volume != volumes[input][output])
    {
        volumes[input][output] = volume;
        changed = 1;
    }

    // Update pan if provided
    if (pan >= -100.0f && pan <= 100.0f && pan != pans[input][output])
    {
        pans[input][output] = pan;
        changed = 1;
    }

    // Notify observers if anything changed and not in refresh mode
    if (changed && !refreshing_state(-1))
    {
        notify_mixer_changed(input, output, volumes[input][output], pans[input][output]);
    }

    return 0;
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

struct durecfile_state *get_durec_files(size_t *fileslen)
{
    if (fileslen)
        *fileslen = durec.fileslen;
    return durec.files;
}
