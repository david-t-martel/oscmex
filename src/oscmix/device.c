#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "device.h"
#include "platform.h"

/* Global variable to track the current device */
const struct device *cur_device = NULL;

/* External references to device definitions */
extern const struct device ffucxii;
extern const struct device ff802;
extern const struct device ufxii;

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
    .id = "FFUCXII",
    .name = "Fireface UCX II",
    .version = 1,
    .flags = DEVICE_DUREC,
    .inputs = ffucxii_inputs,
    .inputslen = sizeof(ffucxii_inputs) / sizeof(ffucxii_inputs[0]),
    .outputs = ffucxii_outputs,
    .outputslen = sizeof(ffucxii_outputs) / sizeof(ffucxii_outputs[0])};

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
int device_init(void)
{
    /* For now, just use the first device in the list */
    cur_device = known_devices[0];

    if (!cur_device)
    {
        fprintf(stderr, "No device available\n");
        return -1;
    }

    /* Allocate memory for device state storage */
    int num_inputs = cur_device->inputslen;
    int num_outputs = cur_device->outputslen;

    volumes = calloc(num_inputs, sizeof(float *));
    pans = calloc(num_inputs, sizeof(float *));

    if (!volumes || !pans)
    {
        fprintf(stderr, "Memory allocation failed\n");
        device_cleanup();
        return -1;
    }

    for (int i = 0; i < num_inputs; i++)
    {
        volumes[i] = calloc(num_outputs, sizeof(float));
        pans[i] = calloc(num_outputs, sizeof(float));

        if (!volumes[i] || !pans[i])
        {
            fprintf(stderr, "Memory allocation failed\n");
            device_cleanup();
            return -1;
        }

        /* Initialize mixer values to defaults */
        for (int j = 0; j < num_outputs; j++)
        {
            volumes[i][j] = -90.0f; /* -inf dB (muted) */
            pans[i][j] = 0.0f;      /* Center pan */
        }
    }

    /* Allocate and initialize other state arrays */
    output_volumes = calloc(num_outputs, sizeof(float));
    output_mutes = calloc(num_outputs, sizeof(bool));
    output_reflevels = calloc(num_outputs, sizeof(int));

    input_mutes = calloc(num_inputs, sizeof(bool));
    input_gains = calloc(num_inputs, sizeof(float));
    input_phantoms = calloc(num_inputs, sizeof(bool));
    input_reflevels = calloc(num_inputs, sizeof(int));
    input_hizs = calloc(num_inputs, sizeof(bool));

    if (!output_volumes || !output_mutes || !output_reflevels ||
        !input_mutes || !input_gains || !input_phantoms ||
        !input_reflevels || !input_hizs)
    {
        fprintf(stderr, "Memory allocation failed\n");
        device_cleanup();
        return -1;
    }

    /* Set default values for system settings */
    sample_rate = 1;  /* 44.1 kHz */
    clock_source = 0; /* Internal */
    buffer_size = 3;  /* 256 samples */

    /* Initialize the devicestate structure */
    if (initDeviceState(cur_device) != 0)
    {
        fprintf(stderr, "Failed to initialize device state\n");
        device_cleanup();
        return -1;
    }

    return 0;
}

/**
 * @brief Clean up the device subsystem
 */
void device_cleanup(void)
{
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
