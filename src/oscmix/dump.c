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
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

#include "oscmix.h"
#include "device.h"
#include "device_state.h"
#include "dump.h"
#include "platform.h"

extern int dflag; /* Debug flag from main */

/* Forward declarations for internal functions */
static void ensureDirectoryExists(const char *path);
static char *getFormattedDateTime(void);
static char *getConfigDirectory(void);

/**
 * @brief Dumps the current device state to the console
 *
 * This function is called when an OSC message with address "/dump" is received.
 * It prints a detailed report of the current device state to stdout.
 */
void dump(void)
{
    const struct device *dev = getDevice();
    int i;

    printf("OSCMix Internal State Dump:\n");

    /* Device information */
    printf("Device: %s (v%d)\n", dev->name, dev->version);
    printf("Flags: 0x%08x\n", dev->flags);
    if (dev->flags & DEVICE_DUREC)
        printf("  - Direct USB Recording supported\n");
    if (dev->flags & DEVICE_ROOMEQ)
        printf("  - Room EQ supported\n");

    printf("\nInputs:\n");
    for (i = 0; i < dev->inputslen; i++)
    {
        printf("  Input %d \"%s\":\n", i + 1, dev->inputs[i].name);
        printf("    flags: 0x%x\n", dev->inputs[i].flags);

        /* Print input-specific parameters based on flags */
        if (dev->inputs[i].flags & INPUT_GAIN)
        {
            printf("    gain: %.1f dB\n", getInputGain(i));
        }
        if (dev->inputs[i].flags & INPUT_48V)
        {
            printf("    phantom: %s\n", getInputPhantom(i) ? "on" : "off");
        }
        if (dev->inputs[i].flags & INPUT_REFLEVEL)
        {
            printf("    reflevel: %s\n", getInputRefLevel(i));
        }
        if (dev->inputs[i].flags & INPUT_HIZ)
        {
            printf("    hi-z: %s\n", getInputHiZ(i) ? "on" : "off");
        }

        printf("    mute: %s\n", getInputMute(i) ? "on" : "off");
    }

    printf("\nOutputs:\n");
    for (i = 0; i < dev->outputslen; i++)
    {
        printf("  Output %d \"%s\":\n", i + 1, dev->outputs[i].name);
        printf("    volume: %.1f dB\n", getOutputVolume(i));
        printf("    mute: %s\n", getOutputMute(i) ? "on" : "off");

        if (dev->outputs[i].flags & OUTPUT_REFLEVEL)
        {
            printf("    reflevel: %s\n", getOutputRefLevel(i));
        }
    }

    printf("\nMixer:\n");
    /* Print mixer settings */
    for (i = 0; i < dev->outputslen; i++)
    {
        int j;
        printf("  Output %d \"%s\" sources:\n", i + 1, dev->outputs[i].name);

        for (j = 0; j < dev->inputslen; j++)
        {
            float vol = getMixerVolume(j, i);
            float pan = getMixerPan(j, i);

            /* Skip printing if route is muted or at -inf */
            if (vol <= -90.0f)
            {
                printf("    Input %d: -inf dB (muted)\n", j + 1);
            }
            else
            {
                char pan_str[8] = "C";

                /* Format the pan value */
                if (pan < -0.01f)
                {
                    snprintf(pan_str, sizeof(pan_str), "L%.0f", -pan * 100.0f);
                }
                else if (pan > 0.01f)
                {
                    snprintf(pan_str, sizeof(pan_str), "R%.0f", pan * 100.0f);
                }

                printf("    Input %d: %.1f dB, pan: %s\n", j + 1, vol, pan_str);
            }
        }
    }

    printf("\nSystem:\n");
    printf("  sample_rate: %d\n", getSampleRate());
    printf("  clock_source: %s\n", getClockSource());
    printf("  buffer_size: %d\n", getBufferSize());
}

/**
 * @brief Dumps the device state to a JSON file
 *
 * This function creates a JSON representation of the device state and saves it
 * to a file in the application's configuration directory. The file is named
 * using the device name and current date/time.
 *
 * @return 0 on success, non-zero on failure
 */
int dumpState(void)
{
    const struct device *dev = getDevice();
    char *configDir = getConfigDirectory();
    char *dateTime = getFormattedDateTime();
    char filePath[512];
    FILE *fp;
    int i, j;

    if (!configDir || !dateTime)
    {
        if (configDir)
            free(configDir);
        if (dateTime)
            free(dateTime);
        return -1;
    }

    /* Ensure directory exists */
    char deviceConfigDir[512];
    snprintf(deviceConfigDir, sizeof(deviceConfigDir), "%s/device_config", configDir);
    ensureDirectoryExists(deviceConfigDir);

    /* Create file path with sanitized device name */
    char sanitizedName[64];
    size_t nameLen = strlen(dev->name);

    for (i = 0, j = 0; i < nameLen && j < sizeof(sanitizedName) - 1; i++)
    {
        char c = dev->name[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9'))
        {
            sanitizedName[j++] = c;
        }
        else if (c == ' ' || c == '.' || c == '-')
        {
            sanitizedName[j++] = '_';
        }
    }
    sanitizedName[j] = '\0';

    snprintf(filePath, sizeof(filePath), "%s/audio-device_%s_date-time_%s.json",
             deviceConfigDir, sanitizedName, dateTime);

    if (dflag)
    {
        printf("Saving device state to: %s\n", filePath);
    }

    fp = fopen(filePath, "w");
    if (!fp)
    {
        fprintf(stderr, "Error opening file for writing: %s\n", filePath);
        free(configDir);
        free(dateTime);
        return -1;
    }

    /* Write JSON format device state */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"device\": {\n");
    fprintf(fp, "    \"name\": \"%s\",\n", dev->name);
    fprintf(fp, "    \"id\": \"%s\",\n", dev->id);
    fprintf(fp, "    \"version\": %d,\n", dev->version);
    fprintf(fp, "    \"flags\": %d,\n", dev->flags);
    fprintf(fp, "    \"timestamp\": \"%s\"\n", dateTime);
    fprintf(fp, "  },\n");

    /* Inputs */
    fprintf(fp, "  \"inputs\": [\n");
    for (i = 0; i < dev->inputslen; i++)
    {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"index\": %d,\n", i);
        fprintf(fp, "      \"name\": \"%s\",\n", dev->inputs[i].name);
        fprintf(fp, "      \"flags\": %d,\n", dev->inputs[i].flags);

        if (dev->inputs[i].flags & INPUT_GAIN)
        {
            fprintf(fp, "      \"gain\": %.1f,\n", getInputGain(i));
        }
        if (dev->inputs[i].flags & INPUT_48V)
        {
            fprintf(fp, "      \"phantom\": %s,\n", getInputPhantom(i) ? "true" : "false");
        }
        if (dev->inputs[i].flags & INPUT_REFLEVEL)
        {
            fprintf(fp, "      \"reflevel\": \"%s\",\n", getInputRefLevel(i));
        }
        if (dev->inputs[i].flags & INPUT_HIZ)
        {
            fprintf(fp, "      \"hiz\": %s,\n", getInputHiZ(i) ? "true" : "false");
        }

        fprintf(fp, "      \"mute\": %s\n", getInputMute(i) ? "true" : "false");
        fprintf(fp, "    }%s\n", (i < dev->inputslen - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    /* Outputs */
    fprintf(fp, "  \"outputs\": [\n");
    for (i = 0; i < dev->outputslen; i++)
    {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"index\": %d,\n", i);
        fprintf(fp, "      \"name\": \"%s\",\n", dev->outputs[i].name);
        fprintf(fp, "      \"volume\": %.1f,\n", getOutputVolume(i));
        fprintf(fp, "      \"mute\": %s,\n", getOutputMute(i) ? "true" : "false");

        if (dev->outputs[i].flags & OUTPUT_REFLEVEL)
        {
            fprintf(fp, "      \"reflevel\": \"%s\"\n", getOutputRefLevel(i));
        }
        else
        {
            fprintf(fp, "      \"flags\": %d\n", dev->outputs[i].flags);
        }

        fprintf(fp, "    }%s\n", (i < dev->outputslen - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    /* Mixer */
    fprintf(fp, "  \"mixer\": [\n");
    for (i = 0; i < dev->outputslen; i++)
    {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"output\": %d,\n", i);
        fprintf(fp, "      \"sources\": [\n");

        for (j = 0; j < dev->inputslen; j++)
        {
            float vol = getMixerVolume(j, i);
            float pan = getMixerPan(j, i);

            fprintf(fp, "        {\n");
            fprintf(fp, "          \"input\": %d,\n", j);
            fprintf(fp, "          \"volume\": %.1f,\n", vol);
            fprintf(fp, "          \"pan\": %.2f\n", pan);
            fprintf(fp, "        }%s\n", (j < dev->inputslen - 1) ? "," : "");
        }

        fprintf(fp, "      ]\n");
        fprintf(fp, "    }%s\n", (i < dev->outputslen - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    /* System */
    fprintf(fp, "  \"system\": {\n");
    fprintf(fp, "    \"sample_rate\": %d,\n", getSampleRate());
    fprintf(fp, "    \"clock_source\": \"%s\",\n", getClockSource());
    fprintf(fp, "    \"buffer_size\": %d\n", getBufferSize());
    fprintf(fp, "  }\n");

    fprintf(fp, "}\n");

    fclose(fp);
    free(configDir);
    free(dateTime);

    return 0;
}

/**
 * @brief Dumps the current device state to a file
 * Convenience function that calls dumpState with the current device and state
 *
 * @return 0 on success, non-zero on failure
 */
int dumpConfig(void)
{
    const struct device *dev = getDevice();
    struct devicestate *state = getDeviceState();

    if (!dev || !state)
    {
        fprintf(stderr, "No active device or device state\n");
        return -1;
    }

    // Get application home directory
    char app_home[256];
    if (platform_get_app_data_dir(app_home, sizeof(app_home)) != 0)
    {
        fprintf(stderr, "Failed to get application data directory\n");
        return -1;
    }

    // Create the device_config subdirectory path
    char config_dir[512];
    snprintf(config_dir, sizeof(config_dir), "%s%cOSCMix%cdevice_config",
             app_home, PLATFORM_PATH_SEPARATOR, PLATFORM_PATH_SEPARATOR);

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
    char safe_name[64] = {0};
    const char *device_name = dev->name;
    size_t name_len = strlen(device_name);
    size_t j = 0;

    for (size_t i = 0; i < name_len && j < sizeof(safe_name) - 1; i++)
    {
        char c = device_name[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-')
        {
            safe_name[j++] = c;
        }
        else if (c == ' ' || c == '.' || c == ',')
        {
            safe_name[j++] = '_';
        }
    }

    // Create full filepath
    char filename[768];
    snprintf(filename, sizeof(filename), "%s%caudio-device_%s_date-time_%s.json",
             config_dir, PLATFORM_PATH_SEPARATOR, safe_name, date_str);

    // Rest of your file writing code...

    return 0;
}

/**
 * @brief Ensures that a directory exists, creating it if necessary
 *
 * @param path The directory path to check/create
 */
static void ensureDirectoryExists(const char *path)
{
    struct stat st;

    /* Check if directory already exists */
    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
        {
            return; /* Directory exists */
        }
    }

    /* Create directory */
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
    {
        fprintf(stderr, "Error creating directory: %s (errno: %d)\n", path, errno);
    }
}

/**
 * @brief Gets the formatted date-time string for the filename
 *
 * @return A malloc'd string with the date-time (caller must free)
 */
static char *getFormattedDateTime(void)
{
    time_t now;
    struct tm *timeinfo;
    char *buffer = malloc(20); /* YYYY-MM-DD_HH-MM-SS\0 */

    if (!buffer)
        return NULL;

    time(&now);
    timeinfo = localtime(&now);

    strftime(buffer, 20, "%Y-%m-%d_%H-%M-%S", timeinfo);
    return buffer;
}

/**
 * @brief Gets the application configuration directory
 *
 * @return A malloc'd string with the config directory path (caller must free)
 */
static char *getConfigDirectory(void)
{
    char *configDir = NULL;

#ifdef _WIN32
    /* Windows: %APPDATA%\oscmix */
    const char *appdata = getenv("APPDATA");
    if (appdata)
    {
        size_t len = strlen(appdata) + 8; /* appdata + '\oscmix\0' */
        configDir = malloc(len);
        if (configDir)
        {
            snprintf(configDir, len, "%s\\oscmix", appdata);
            ensureDirectoryExists(configDir);
        }
    }
#else
    /* Unix/Linux/Mac: $HOME/.config/oscmix */
    const char *home = getenv("HOME");
    if (home)
    {
        size_t len = strlen(home) + 16; /* home + '/.config/oscmix\0' */
        configDir = malloc(len);
        if (configDir)
        {
            snprintf(configDir, len, "%s/.config/oscmix", home);
            ensureDirectoryExists(configDir);
        }
    }
#endif

    /* Fallback to current directory if we can't get the config directory */
    if (!configDir)
    {
        configDir = malloc(8); /* "./oscmix\0" */
        if (configDir)
        {
            strcpy(configDir, "./oscmix");
            ensureDirectoryExists(configDir);
        }
    }

    return configDir;
}

/* Helper function prototypes for devices */
float getInputGain(int index);
bool getInputPhantom(int index);
const char *getInputRefLevel(int index);
bool getInputHiZ(int index);
bool getInputMute(int index);
float getOutputVolume(int index);
bool getOutputMute(int index);
const char *getOutputRefLevel(int index);
float getMixerVolume(int input, int output);
float getMixerPan(int input, int output);
int getSampleRate(void);
const char *getClockSource(void);
int getBufferSize(void);
const struct device *getDevice(void);

/**
 * Dumps the current device state to a JSON file in the device_config directory
 * @param dev Pointer to the device structure
 * @param state Pointer to the current device state
 * @return 0 on success, -1 on failure
 */
int dumpState(const struct device *dev, const struct devicestate *state)
{
    // Implementation as in device_ffucxii.c
    // ...
}
