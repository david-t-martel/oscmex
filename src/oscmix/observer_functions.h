/**
 * @file observer_functions.h
 * @brief Observer pattern for device state changes
 */

#ifndef OBSERVER_FUNCTIONS_H
#define OBSERVER_FUNCTIONS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Register essential OSC observers
     *
     * These are core observers that should always be active
     * for basic communication with the device.
     *
     * @return 0 on success, non-zero on failure
     */
    int register_essential_osc_observers(void);

    /**
     * @brief Register all OSC observers
     *
     * This registers all observers to track changes in various
     * device parameters.
     *
     * @return 0 on success, non-zero on failure
     */
    int register_osc_observers(void);

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
                            bool *output_active, bool *mixer_active);

    /**
     * @brief Send the full device state to OSC clients
     *
     * This function sends the current state of all device parameters
     * to connected OSC clients.
     */
    void send_full_device_state(void);

    /**
     * @brief Notification for DSP state changes
     *
     * @param version DSP firmware version
     * @param load DSP processing load
     */
    void notify_dsp_state_changed(int version, int load);

    /**
     * @brief Notification for DURec status changes
     *
     * @param status New DURec status
     * @param position New playback position
     */
    void notify_durec_status_changed(int status, int position);

    /**
     * @brief Notification for sample rate changes
     *
     * @param sample_rate New sample rate in Hz
     */
    void notify_sample_rate_changed(int sample_rate);

    /**
     * @brief Notification for input parameter changes
     *
     * @param index Input channel index
     * @param gain New gain value
     * @param phantom New phantom power state
     * @param hiz New HiZ state
     * @param mute New mute state
     */
    void notify_input_changed(int index, float gain, bool phantom, bool hiz, bool mute);

    /**
     * @brief Notification for output parameter changes
     *
     * @param index Output channel index
     * @param volume New volume value
     * @param mute New mute state
     */
    void notify_output_changed(int index, float volume, bool mute);

    /**
     * @brief Notification for mixer parameter changes
     *
     * @param input Input channel index
     * @param output Output channel index
     * @param volume New volume value
     * @param pan New pan value
     */
    void notify_mixer_changed(int input, int output, float volume, float pan);

#ifdef __cplusplus
}
#endif

#endif /* OBSERVER_FUNCTIONS_H */
