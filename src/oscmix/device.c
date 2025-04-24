#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "device.h"
#include "platform.h"
#include "logging.h"

/* Global variable to track the current device */
const struct device *cur_device = NULL;

/* External references to device definitions */
extern const struct device ffucxii;
extern const struct device ff802;
extern const struct device ufxii;

/* Define sample rate count */
#define SAMPLE_RATE_COUNT 10

/* Enumeration names */
const char *const CLOCK_SOURCE_NAMES[] = {
    "Internal", "AES", "ADAT", "Sync In"};

const char *const BUFFER_SIZE_NAMES[] = {
    "32", "64", "128", "256", "512", "1024"};

const char *const DUREC_STATUS_NAMES[] = {
    "No Media", "FS Error", "Initializing", "Reinitializing",
    "Unknown", "Stopped", "Recording", "Unknown",
    "Unknown", "Unknown", "Playing", "Paused"};

const char *const DUREC_PLAYMODE_NAMES[] = {
    "Single", "Repeat", "Sequence", "Random"};

const char *const INPUT_REF_LEVEL_NAMES[] = {
    "-10 dBV", "+4 dBu", "HiZ", "LoGain"};

const char *const OUTPUT_REF_LEVEL_NAMES[] = {
    "-10 dBV", "+4 dBu", "HiGain", "LoGain"};

const char *const DITHER_NAMES[] = {
    "Off", "16 bit", "20 bit"};

const char *const LOG_LEVEL_NAMES[] = {
    "Error", "Warning", "Info", "Debug"};

/* Storage for device states - these variables should be initialized in device_init() */
float **volumes = NULL;       // 2D array: [input][output]
float **pans = NULL;          // 2D array: [input][output]
float *output_volumes = NULL; // Array of output volumes
bool *output_mutes = NULL;    // Array of output mutes
bool *input_mutes = NULL;     // Array of input mutes
float *input_gains = NULL;    // Array of input gains
bool *input_phantoms = NULL;  // Array of input phantom power states
int *input_reflevels = NULL;  // Array of input reference levels
bool *input_hizs = NULL;      // Array of input high impedance states
int *output_reflevels = NULL; // Array of output reference levels
int sample_rate = 0;          // Current sample rate index
int clock_source = 0;         // Current clock source index
int buffer_size = 0;          // Current buffer size index

/* Define shared constants */
const long SAMPLE_RATES[SAMPLE_RATE_COUNT] = {
    44100, 48000, 88200, 96000, 176400, 192000,
    352800, 384000, 705600, 768000};

/* RME Fireface UCX II definitions */
static const struct inputinfo ffucxii_inputs[] = {
    {"MIC 1", INPUT_GAIN | INPUT_48V | INPUT_REFLEVEL | INPUT_HIZ},
    {"MIC 2", INPUT_GAIN | INPUT_48V | INPUT_REFLEVEL | INPUT_HIZ},
    {"IN 3", INPUT_REFLEVEL},
    {"IN 4", INPUT_REFLEVEL},
    {"IN 5", 0},
    {"IN 6", 0},
    {"AN 1", 0},
    {"AN 2", 0},
    {"ADAT 1", 0},
    {"ADAT 2", 0},
    {"ADAT 3", 0},
    {"ADAT 4", 0},
    {"ADAT 5", 0},
    {"ADAT 6", 0},
    {"ADAT 7", 0},
    {"ADAT 8", 0},
    {"AES L", 0},
    {"AES R", 0}};

static const struct outputinfo ffucxii_outputs[] = {
    {"AN 1", OUTPUT_REFLEVEL},
    {"AN 2", OUTPUT_REFLEVEL},
    {"AN 3", OUTPUT_REFLEVEL},
    {"AN 4", OUTPUT_REFLEVEL},
    {"AN 5", OUTPUT_REFLEVEL},
    {"AN 6", OUTPUT_REFLEVEL},
    {"HP 1", 0},
    {"HP 2", 0},
    {"ADAT 1", 0},
    {"ADAT 2", 0},
    {"ADAT 3", 0},
    {"ADAT 4", 0},
    {"ADAT 5", 0},
    {"ADAT 6", 0},
    {"ADAT 7", 0},
    {"ADAT 8", 0},
    {"AES L", 0},
    {"AES R", 0}};

/* Device definitions */
static const struct device ffucxii = {
    .name = "Fireface UCX II",
    .inputslen = sizeof(ffucxii_inputs) / sizeof(ffucxii_inputs[0]),
    .outputslen = sizeof(ffucxii_outputs) / sizeof(ffucxii_outputs[0]),
    .playbacklen = 18, /* 18 playback channels */
    .mixerlen = 18,    /* 18 mixer channels */
    .flags = DEVICE_DUREC,
    .inputs = ffucxii_inputs,
    .outputs = ffucxii_outputs};

/* Forward declaration for device state cleanup function */
void cleanupDeviceState(void);

/* Known devices array */
static const struct device *known_devices[] = {
    &ffucxii,
    &ff802,
    &ufxii,
    NULL};

/**
 * @brief Initialize the device subsystem and detect connected devices
 *
 * @return 0 on success, non-zero on failure
 */
int device_init(const char *name)
{
    if (!name)
    {
        /* Default to first device if none specified */
        cur_device = &ffucxii;
        log_info("Initialized default device: %s", ffucxii.name);
        return 0;
    }

    if (strcmp(name, "ffucxii") == 0 ||
        strcmp(name, "ucx2") == 0 ||
        strcmp(name, "ucxii") == 0)
    {
        cur_device = &ffucxii;
        log_info("Initialized device: %s", ffucxii.name);
        return 0;
    }

    if (strcmp(name, "ff802") == 0 ||
        strcmp(name, "fireface802") == 0)
    {
        cur_device = &ff802;
        return 0;
    }

    if (strcmp(name, "ufxii") == 0 ||
        strcmp(name, "ufx2") == 0)
    {
        cur_device = &ufxii;
        return 0;
    }

    /* Unknown device */
    log_error("Unknown device name: %s", name);
    return -1;
}

/**
 * @brief Clean up the device subsystem
 */
void device_cleanup(void)
{
    log_info("Cleaning up device resources");

    if (volumes)
    {
        for (int i = 0; cur_device && i < cur_device->inputslen; i++)
        {
            free(volumes[i]);
        }
        free(volumes);
        volumes = NULL;
    }

    if (pans)
    {
        for (int i = 0; cur_device && i < cur_device->inputslen; i++)
        {
            free(pans[i]);
        }
        free(pans);
        pans = NULL;
    }

    free(output_volumes);
    free(output_mutes);
    free(output_reflevels);
    free(input_mutes);
    free(input_gains);
    free(input_phantoms);
    free(input_reflevels);
    free(input_hizs);

    output_volumes = NULL;
    output_mutes = NULL;
    output_reflevels = NULL;
    input_mutes = NULL;
    input_gains = NULL;
    input_phantoms = NULL;
    input_reflevels = NULL;
    input_hizs = NULL;

    /* Clean up the device state */
    cleanupDeviceState();

    cur_device = NULL;
    log_debug("Device cleanup complete");
}

/**
 * @brief Get a pointer to the current device
 *
 * @return Pointer to the current device structure
 */
const struct device *get_current_device(void)
{
    return cur_device;
}

/* To maintain compatibility with existing code */
const struct device *getDevice(void)
{
    return cur_device;
}

/* Forward declaration for device state functions */
int set_dsp_state(int version, int load);
int set_durec_state(int status, int position, int time, int usb_errors, int usb_load,
                    float total_space, float free_space, int file, int record_time,
                    int prev, int next, int playmode);

/* Placeholder for device state functions */
void cleanupDeviceState(void)
{
    /* Implementation to be provided */
}

int load_device_config(const char *device_id, struct devicestate *state)
{
    char app_home[256];
    char config_path[512];

    if (platform_get_app_data_dir(app_home, sizeof(app_home)) != 0)
    {
        return -1;
    }

    snprintf(config_path, sizeof(config_path), "%s%cOSCMix%cdevice_config%c%s.json",
             app_home, PLATFORM_PATH_SEPARATOR, PLATFORM_PATH_SEPARATOR,
             PLATFORM_PATH_SEPARATOR, device_id);

    // Rest of config loading code...
}

/**
 * @brief Update DSP status from register value
 * @param reg_value The raw register value
 * @return 0 on success, non-zero on failure
 */
int update_dsp_status(uint16_t reg_value)
{
    int load = reg_value & 0xff;
    int vers = reg_value >> 8;

    // Update internal state
    set_dsp_state(vers, load);

    return 0;
}

/**
 * @brief Set DSP state
 */
int set_dsp_state(int version, int load)
{
    /* Implementation to be provided */
    return 0;
}

/**
 * @brief Update DURec status from register value
 * @param reg_value The raw register value
 * @return 0 on success, non-zero on failure
 */
int update_durec_status(uint16_t reg_value)
{
    int status = reg_value & 0xf;
    int position = (reg_value >> 8) * 100 / 65;

    // Update internal state
    set_durec_state(status, position, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

    return 0;
}

/**
 * @brief Set DURec state
 */
int set_durec_state(int status, int position, int time, int usb_errors, int usb_load,
                    float total_space, float free_space, int file, int record_time,
                    int prev, int next, int playmode)
{
    /* Implementation to be provided */
    return 0;
}

/* Gets the DSP status (version and load) */
int get_dsp_state(int *version, int *load)
{
    // Add implementation
    if (version)
        *version = 123; // Example value
    if (load)
        *load = 25; // Example value (25%)
    return 0;
}

/* Gets the DURec status */
int get_durec_status(int *status, int *position)
{
    // Add implementation
    if (status)
        *status = 5; // Stopped
    if (position)
        *position = 0; // 0%
    return 0;
}

/* Gets the current sample rate */
int get_sample_rate(int *rate)
{
    // Add implementation
    if (rate)
        *rate = 48000; // 48 kHz
    return 0;
}

/* Gets input channel state */
int get_input_params(int channel, float *gain, bool *phantom, bool *hiz, bool *mute)
{
    // Implementation of getting input parameters
    if (gain)
        *gain = 0.75f; // 75%
    if (phantom)
        *phantom = true;
    if (hiz)
        *hiz = false;
    if (mute)
        *mute = false;
    return 0;
}

/* Gets output channel state */
int get_output_params(int channel, float *volume, bool *mute)
{
    // Implementation of getting output parameters
    if (volume)
        *volume = 0.8f; // 80%
    if (mute)
        *mute = false;
    return 0;
}

/* Gets mixer matrix state */
int get_mixer_state(int input, int output, float *volume, float *pan)
{
    // Add implementation
    if (volume)
        *volume = 0.5f; // 50%
    if (pan)
        *pan = 0.5f; // Center
    return 0;
}

// Return the full input state struct
struct input_state *get_input_state_struct(int index)
{
    static struct input_state state;
    // Fill in state with appropriate values
    state.gain = 0.75f;
    state.phantom = true;
    state.hiz = false;
    state.pad = false;
    state.mute = false;
    state.stereo = false;
    state.reflevel = 0;
    return &state;
}

// Return the full output state struct
struct output_state *get_output_state_struct(int index)
{
    static struct output_state state;
    // Fill in state with appropriate values
    state.volume = 0.8f;
    state.mute = false;
    state.stereo = true;
    state.reflevel = 0;
    return &state;
}

// Refreshing state function
int refreshing_state(int new_state)
{
    static int is_refreshing = 0;

    if (new_state >= 0)
        is_refreshing = new_state;

    return is_refreshing;
}

/**
 * @brief Update sample rate
 */
int update_sample_rate(int val)
{
    /* Implementation to be provided */
    return 0;
}

/**
 * @brief Update input parameter
 */
int update_input_parameter(int channel, int param, int val)
{
    /* Implementation to be provided */
    return 0;
}

/**
 * @brief Set DURec files length
 */
int set_durec_files_length(int length)
{
    /* Implementation to be provided */
    return 0;
}
