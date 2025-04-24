/**
 * @file dump.c
 * @brief Functions for dumping OSCMix device state for debugging and configuration
 *
 * This file implements the dump functionality for OSCMix, allowing users to view
 * the current device state and save it to a file. The dump command can be triggered
 * via an OSC message to "/dump" for console output, or "/dump/save" to write to a file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "oscmix.h"
#include "device.h"
#include "device_state.h"
#include "dump.h"
#include "platform.h"
#include "util.h"

extern int dflag; /* Debug flag from main */

/**
 * @brief Dumps the current device state to the console
 *
 * This function is called when an OSC message with address "/dump" is received.
 * It prints a detailed report of the current device state to stdout.
 */
void dump(void)
{
    // Call the device state dumping function
    dumpDeviceState();
}

/**
 * @brief Outputs the detailed device state to console
 */
void dumpDeviceState(void)
{
    const struct device *cur_device = getDevice();
    int dsp_vers, dsp_load;

    if (!cur_device)
    {
        fprintf(stderr, "No device initialized\n");
        return;
    }

    // Get DSP state
    get_dsp_state(&dsp_vers, &dsp_load);

    printf("Device: %s (ID: %s, Version: %d)\n",
           cur_device->name, cur_device->id, cur_device->version);
    printf("DSP: version=%d, load=%d\n", dsp_vers, dsp_load);

    // Print inputs
    printf("\nInputs:\n");
    for (int i = 0; i < cur_device->inputslen; i++)
    {
        struct input_state *input = get_input_state(i);

        printf("  Input %d: %s (flags: 0x%x)\n",
               i + 1, cur_device->inputs[i].name, cur_device->inputs[i].flags);

        // Print input parameters based on device flags
        if (cur_device->inputs[i].flags & INPUT_GAIN)
        {
            printf("    Gain: %.1f dB\n", getInputGain(i));
        }
        if (cur_device->inputs[i].flags & INPUT_48V)
        {
            printf("    Phantom Power: %s\n", getInputPhantom(i) ? "On" : "Off");
        }
        if (cur_device->inputs[i].flags & INPUT_HIZ)
        {
            printf("    Hi-Z: %s\n", getInputHiZ(i) ? "On" : "Off");
        }

        printf("    Mute: %s\n", getInputMute(i) ? "On" : "Off");

        if (input)
        {
            printf("    Stereo: %s\n", input->stereo ? "On" : "Off");
            printf("    Width: %.2f\n", input->width);
        }
    }

    // Print outputs
    printf("\nOutputs:\n");
    for (int i = 0; i < cur_device->outputslen; i++)
    {
        struct output_state *output = get_output_state(i);

        printf("  Output %d: %s (flags: 0x%x)\n",
               i + 1, cur_device->outputs[i].name, cur_device->outputs[i].flags);
        printf("    Volume: %.1f dB\n", getOutputVolume(i));
        printf("    Mute: %s\n", getOutputMute(i) ? "On" : "Off");

        if (output)
        {
            printf("    Stereo: %s\n", output->stereo ? "On" : "Off");

            // Print a few mix settings as an example
            printf("    Mix settings (sample):\n");
            for (int j = 0; j < 3 && j < cur_device->inputslen; j++)
            {
                if (output->mix)
                {
                    printf("      Input %d: vol=%d, pan=%d\n",
                           j + 1, output->mix[j].vol, output->mix[j].pan);
                }
            }
        }
    }

    // Print DURec state
    int status, position, time, usbload, usberrors, file, recordtime, index, next, playmode;
    float totalspace, freespace;

    get_durec_state(&status, &position, &time, &usberrors, &usbload,
                    &totalspace, &freespace, &file, &recordtime, &index, &next, &playmode);

    printf("\nDURec:\n");
    printf("  Status: %d, Position: %d, Time: %d\n", status, position, time);
    printf("  USB: errors=%d, load=%d\n", usberrors, usbload);
    printf("  Space: total=%.1f GB, free=%.1f GB\n", totalspace, freespace);
    printf("  Files: current=%d\n", file);

    size_t fileslen;
    struct durecfile_state *files = get_durec_files(&fileslen);

    printf("  Files: count=%zu, current=%d\n", fileslen, file);

    if (fileslen > 0 && files)
    {
        printf("  File list (sample):\n");
        for (int i = 0; i < 3 && i < fileslen; i++)
        {
            printf("    File %d: name='%s', sr=%lu, ch=%u, len=%u\n",
                   i, files[i].name, files[i].samplerate,
                   files[i].channels, files[i].length);
        }
    }

    // Print system settings
    printf("\nSystem:\n");
    printf("  Sample Rate: %d\n", getSampleRate());
    printf("  Clock Source: %s\n", getClockSource());
    printf("  Buffer Size: %d\n", getBufferSize());
}

/**
 * @brief Dumps the current device state to a file
 *
 * @return 0 on success, non-zero on failure
 */
int dumpConfig(void)
{
    const struct device *cur_device = getDevice();
    int dsp_vers, dsp_load;

    if (!cur_device)
    {
        fprintf(stderr, "No active device\n");
        return -1;
    }

    // Get DSP state
    get_dsp_state(&dsp_vers, &dsp_load);

    // Get application configuration directory
    char config_dir[512];
    if (platform_get_device_config_dir(config_dir, sizeof(config_dir)) != 0)
    {
        fprintf(stderr, "Failed to get device configuration directory\n");
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

    if (dflag)
    {
        printf("Saving device state to: %s\n", filename);
    }

    // Open file using platform abstraction
    FILE *fp = platform_fopen(filename, "w");
    if (!fp)
    {
        fprintf(stderr, "Error opening file for writing: %s\n", filename);
        return -1;
    }

    /* Write JSON format device state */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"device\": {\n");
    fprintf(fp, "    \"name\": \"%s\",\n", cur_device->name);
    fprintf(fp, "    \"id\": \"%s\",\n", cur_device->id);
    fprintf(fp, "    \"version\": %d,\n", cur_device->version);
    fprintf(fp, "    \"timestamp\": \"%s\"\n", date_str);
    fprintf(fp, "  },\n");

    /* Write DSP state */
    fprintf(fp, "  \"dsp\": {\n");
    fprintf(fp, "    \"version\": %d,\n", dsp_vers);
    fprintf(fp, "    \"load\": %d\n", dsp_load);
    fprintf(fp, "  },\n");

    /* System state */
    fprintf(fp, "  \"system\": {\n");
    fprintf(fp, "    \"sample_rate\": %d,\n", getSampleRate());
    fprintf(fp, "    \"clock_source\": \"%s\",\n", getClockSource());
    fprintf(fp, "    \"buffer_size\": %d\n", getBufferSize());
    fprintf(fp, "  },\n");

    /* Inputs */
    fprintf(fp, "  \"inputs\": [\n");
    for (int i = 0; i < cur_device->inputslen; i++)
    {
        struct input_state *input = get_input_state(i);

        fprintf(fp, "    {\n");
        fprintf(fp, "      \"index\": %d,\n", i);
        fprintf(fp, "      \"name\": \"%s\",\n", cur_device->inputs[i].name);
        fprintf(fp, "      \"flags\": %d,\n", cur_device->inputs[i].flags);

        if (cur_device->inputs[i].flags & INPUT_GAIN)
        {
            fprintf(fp, "      \"gain\": %.1f,\n", getInputGain(i));
        }
        if (cur_device->inputs[i].flags & INPUT_48V)
        {
            fprintf(fp, "      \"phantom\": %s,\n", getInputPhantom(i) ? "true" : "false");
        }
        if (cur_device->inputs[i].flags & INPUT_HIZ)
        {
            fprintf(fp, "      \"hiz\": %s,\n", getInputHiZ(i) ? "true" : "false");
        }

        fprintf(fp, "      \"mute\": %s", getInputMute(i) ? "true" : "false");

        if (input)
        {
            fprintf(fp, ",\n      \"stereo\": %s,\n", input->stereo ? "true" : "false");
            fprintf(fp, "      \"width\": %.2f\n", input->width);
        }
        else
        {
            fprintf(fp, "\n");
        }

        fprintf(fp, "    }%s\n", (i < cur_device->inputslen - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    /* Outputs */
    fprintf(fp, "  \"outputs\": [\n");
    for (int i = 0; i < cur_device->outputslen; i++)
    {
        struct output_state *output = get_output_state(i);

        fprintf(fp, "    {\n");
        fprintf(fp, "      \"index\": %d,\n", i);
        fprintf(fp, "      \"name\": \"%s\",\n", cur_device->outputs[i].name);
        fprintf(fp, "      \"volume\": %.1f,\n", getOutputVolume(i));
        fprintf(fp, "      \"mute\": %s", getOutputMute(i) ? "true" : "false");

        if (output)
        {
            fprintf(fp, ",\n      \"stereo\": %s\n", output->stereo ? "true" : "false");
        }
        else
        {
            fprintf(fp, "\n");
        }

        fprintf(fp, "    }%s\n", (i < cur_device->outputslen - 1) ? "," : "");
    }
    fprintf(fp, "  ]\n");

    /* Close JSON object */
    fprintf(fp, "}\n");

    fclose(fp);
    printf("Device state saved to: %s\n", filename);
    return 0;
}

/* Helper function prototypes - these should be actually implemented in device_state.c */
float getInputGain(int index);
bool getInputPhantom(int index);
const char *getInputRefLevel(int index);
bool getInputHiZ(int index);
bool getInputMute(int index);
float getOutputVolume(int index);
bool getOutputMute(int index);
const char *getOutputRefLevel(int index);
int getSampleRate(void);
const char *getClockSource(void);
int getBufferSize(void);
