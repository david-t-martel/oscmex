/**
 * @file observer_functions.c
 * @brief Implementation of observer pattern for device state changes
 */

#include "observer_functions.h"
#include "platform.h"
#include "logging.h"
#include "device.h"
#include "device_state.h"
#include "osc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Observer state tracking */
static platform_mutex_t observers_mutex;
static bool observers_initialized = false;

/* Observer activation flags */
static bool dsp_observer_active = false;
static bool durec_observer_active = false;
static bool samplerate_observer_active = false;
static bool input_observer_active = false;
static bool output_observer_active = false;
static bool mixer_observer_active = false;

/**
 * @brief Register essential observers for device communication
 *
 * @return 0 on success, non-zero on failure
 */
int register_essential_osc_observers(void)
{
    int result;

    // Initialize the mutex if not done already
    if (!observers_initialized)
    {
        result = platform_mutex_init(&observers_mutex);
        if (result != 0)
        {
            log_error("Failed to create observers mutex: %s", platform_strerror(platform_errno()));
            return -1;
        }
        observers_initialized = true;
    }

    // Lock the mutex during initialization
    platform_mutex_lock(&observers_mutex);

    // Register essential observers (DSP status, sample rate)
    dsp_observer_active = true;
    samplerate_observer_active = true;

    platform_mutex_unlock(&observers_mutex);

    log_info("Essential OSC observers registered");
    return 0;
}

/**
 * @brief Register all state observers for comprehensive device monitoring
 *
 * @return 0 on success, non-zero on failure
 */
int register_osc_observers(void)
{
    int result;

    // Ensure essential observers are active
    if (!dsp_observer_active || !samplerate_observer_active)
    {
        result = register_essential_osc_observers();
        if (result != 0)
        {
            return result;
        }
    }

    // Lock the mutex during registration
    platform_mutex_lock(&observers_mutex);

    // Register all observers
    dsp_observer_active = true;
    durec_observer_active = true;
    samplerate_observer_active = true;
    input_observer_active = true;
    output_observer_active = true;
    mixer_observer_active = true;

    platform_mutex_unlock(&observers_mutex);

    log_info("All OSC observers registered");
    return 0;
}

/**
 * @brief Get the status of active observers
 *
 * @param dsp_active Whether DSP status observer is active
 * @param durec_active Whether DURec status observer is active
 * @param samplerate_active Whether sample rate observer is active
 * @param input_active Whether input parameters observer is active
 * @param output_active Whether output parameters observer is active
 * @param mixer_active Whether mixer parameters observer is active
 * @return Number of active observers
 */
int get_observer_status(bool *dsp_active, bool *durec_active,
                        bool *samplerate_active, bool *input_active,
                        bool *output_active, bool *mixer_active)
{
    int active_count = 0;

    platform_mutex_lock(&observers_mutex);

    if (dsp_active)
        *dsp_active = dsp_observer_active;
    if (durec_active)
        *durec_active = durec_observer_active;
    if (samplerate_active)
        *samplerate_active = samplerate_observer_active;
    if (input_active)
        *input_active = input_observer_active;
    if (output_active)
        *output_active = output_observer_active;
    if (mixer_active)
        *mixer_active = mixer_observer_active;

    // Count active observers
    if (dsp_observer_active)
        active_count++;
    if (durec_observer_active)
        active_count++;
    if (samplerate_observer_active)
        active_count++;
    if (input_observer_active)
        active_count++;
    if (output_observer_active)
        active_count++;
    if (mixer_observer_active)
        active_count++;

    platform_mutex_unlock(&observers_mutex);

    return active_count;
}

/**
 * @brief Send the full device state to OSC clients
 *
 * This function sends the current state of all device parameters
 * to connected OSC clients, useful after connection or refresh.
 */
void send_full_device_state(void)
{
    const struct device *dev = getDevice();
    if (!dev)
    {
        log_error("Cannot send device state: no active device");
        return;
    }

    log_info("Sending full device state");

    // Get current state
    int dsp_version, dsp_load;
    get_dsp_state(&dsp_version, &dsp_load);

    // Send system parameters
    oscsend("/system/samplerate", ",i", getSampleRate());
    oscsend("/system/clocksource", ",s", getClockSource());
    oscsend("/system/buffersize", ",i", getBufferSize());

    // Send hardware status
    if (dsp_version >= 0)
    {
        oscsend("/hardware/dspversion", ",i", dsp_version);
    }
    if (dsp_load >= 0)
    {
        oscsend("/hardware/dspload", ",i", dsp_load);
    }

    // Send input parameters
    for (int i = 0; i < dev->inputslen; i++)
    {
        char path[128];

        snprintf(path, sizeof(path), "/input/%d/gain", i + 1);
        oscsend(path, ",f", getInputGain(i));

        snprintf(path, sizeof(path), "/input/%d/phantom", i + 1);
        oscsend(path, ",i", getInputPhantom(i) ? 1 : 0);

        snprintf(path, sizeof(path), "/input/%d/reflevel", i + 1);
        oscsend(path, ",s", getInputRefLevel(i));

        if (dev->inputs[i].flags & INPUT_HIZ)
        {
            snprintf(path, sizeof(path), "/input/%d/hiz", i + 1);
            oscsend(path, ",i", getInputHiZ(i) ? 1 : 0);
        }

        snprintf(path, sizeof(path), "/input/%d/mute", i + 1);
        oscsend(path, ",i", getInputMute(i) ? 1 : 0);
    }

    // Send output parameters
    for (int i = 0; i < dev->outputslen; i++)
    {
        char path[128];

        snprintf(path, sizeof(path), "/output/%d/volume", i + 1);
        oscsend(path, ",f", getOutputVolume(i));

        snprintf(path, sizeof(path), "/output/%d/mute", i + 1);
        oscsend(path, ",i", getOutputMute(i) ? 1 : 0);

        snprintf(path, sizeof(path), "/output/%d/reflevel", i + 1);
        oscsend(path, ",s", getOutputRefLevel(i));
    }

    // Send mixer parameters (sample of first few channels)
    for (int i = 0; i < 2 && i < dev->outputslen; i++)
    {
        for (int j = 0; j < 2 && j < dev->inputslen; j++)
        {
            char path[128];

            // Inputs to outputs
            snprintf(path, sizeof(path), "/mixer/input/%d/output/%d/volume", j + 1, i + 1);
            oscsend(path, ",f", getMixerVolume(j, i));

            snprintf(path, sizeof(path), "/mixer/input/%d/output/%d/pan", j + 1, i + 1);
            oscsend(path, ",f", getMixerPan(j, i));
        }
    }

    // Send DURec status if active
    if (durec_observer_active)
    {
        int status, position;
        get_durec_state(&status, &position, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

        oscsend("/durec/status", ",i", status);
        oscsend("/durec/position", ",i", position);
    }

    // Flush all pending OSC messages
    oscflush();

    log_info("Full device state sent");
}

/**
 * @brief Notification for DSP state changes
 *
 * @param version DSP firmware version
 * @param load DSP processing load
 */
void notify_dsp_state_changed(int version, int load)
{
    bool is_active;

    platform_mutex_lock(&observers_mutex);
    is_active = dsp_observer_active;
    platform_mutex_unlock(&observers_mutex);

    if (!is_active)
    {
        return;
    }

    // Send notifications via OSC
    if (version >= 0)
    {
        oscsend("/hardware/dspversion", ",i", version);
    }

    if (load >= 0)
    {
        oscsend("/hardware/dspload", ",i", load);
    }

    oscflush();
}

/**
 * @brief Notification for DURec status changes
 *
 * @param status New DURec status
 * @param position New playback position
 */
void notify_durec_status_changed(int status, int position)
{
    bool is_active;

    platform_mutex_lock(&observers_mutex);
    is_active = durec_observer_active;
    platform_mutex_unlock(&observers_mutex);

    if (!is_active)
    {
        return;
    }

    // Send status updates via OSC
    if (status >= 0)
    {
        oscsend("/durec/status", ",i", status);
    }

    if (position >= 0)
    {
        oscsend("/durec/position", ",i", position);
    }

    oscflush();
}

/**
 * @brief Notification for sample rate changes
 *
 * @param sample_rate New sample rate in Hz
 */
void notify_sample_rate_changed(int sample_rate)
{
    bool is_active;

    platform_mutex_lock(&observers_mutex);
    is_active = samplerate_observer_active;
    platform_mutex_unlock(&observers_mutex);

    if (!is_active)
    {
        return;
    }

    oscsend("/system/samplerate", ",i", sample_rate);
    oscflush();
}

/**
 * @brief Notification for input parameter changes
 *
 * @param index Input channel index
 * @param gain New gain value
 * @param phantom New phantom power state
 * @param hiz New HiZ state
 * @param mute New mute state
 */
void notify_input_changed(int index, float gain, bool phantom, bool hiz, bool mute)
{
    bool is_active;
    const struct device *dev = getDevice();
    char path[128];

    if (!dev || index < 0 || index >= dev->inputslen)
    {
        return;
    }

    platform_mutex_lock(&observers_mutex);
    is_active = input_observer_active;
    platform_mutex_unlock(&observers_mutex);

    if (!is_active)
    {
        return;
    }

    // Send parameter updates via OSC
    snprintf(path, sizeof(path), "/input/%d/gain", index + 1);
    oscsend(path, ",f", gain);

    snprintf(path, sizeof(path), "/input/%d/phantom", index + 1);
    oscsend(path, ",i", phantom ? 1 : 0);

    if (dev->inputs[index].flags & INPUT_HIZ)
    {
        snprintf(path, sizeof(path), "/input/%d/hiz", index + 1);
        oscsend(path, ",i", hiz ? 1 : 0);
    }

    snprintf(path, sizeof(path), "/input/%d/mute", index + 1);
    oscsend(path, ",i", mute ? 1 : 0);

    oscflush();
}

/**
 * @brief Notification for output parameter changes
 *
 * @param index Output channel index
 * @param volume New volume value
 * @param mute New mute state
 */
void notify_output_changed(int index, float volume, bool mute)
{
    bool is_active;
    const struct device *dev = getDevice();
    char path[128];

    if (!dev || index < 0 || index >= dev->outputslen)
    {
        return;
    }

    platform_mutex_lock(&observers_mutex);
    is_active = output_observer_active;
    platform_mutex_unlock(&observers_mutex);

    if (!is_active)
    {
        return;
    }

    // Send parameter updates via OSC
    snprintf(path, sizeof(path), "/output/%d/volume", index + 1);
    oscsend(path, ",f", volume);

    snprintf(path, sizeof(path), "/output/%d/mute", index + 1);
    oscsend(path, ",i", mute ? 1 : 0);

    oscflush();
}

/**
 * @brief Notification for mixer parameter changes
 *
 * @param input Input channel index
 * @param output Output channel index
 * @param volume New volume value
 * @param pan New pan value
 */
void notify_mixer_changed(int input, int output, float volume, float pan)
{
    bool is_active;
    const struct device *dev = getDevice();
    char path[128];

    if (!dev || input < 0 || input >= dev->inputslen ||
        output < 0 || output >= dev->outputslen)
    {
        return;
    }

    platform_mutex_lock(&observers_mutex);
    is_active = mixer_observer_active;
    platform_mutex_unlock(&observers_mutex);

    if (!is_active)
    {
        return;
    }

    // Send parameter updates via OSC
    snprintf(path, sizeof(path), "/mixer/input/%d/output/%d/volume", input + 1, output + 1);
    oscsend(path, ",f", volume);

    snprintf(path, sizeof(path), "/mixer/input/%d/output/%d/pan", input + 1, output + 1);
    oscsend(path, ",f", pan);

    oscflush();
}

/**
 * @brief Cleanup observer resources
 */
void observer_functions_cleanup(void)
{
    if (observers_initialized)
    {
        platform_mutex_destroy(&observers_mutex);
        observers_initialized = false;

        // Reset all activation flags
        dsp_observer_active = false;
        durec_observer_active = false;
        samplerate_observer_active = false;
        input_observer_active = false;
        output_observer_active = false;
        mixer_observer_active = false;
    }
}
