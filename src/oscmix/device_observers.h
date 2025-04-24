#ifndef DEVICE_OBSERVERS_H
#define DEVICE_OBSERVERS_H

#include <stdbool.h>

/* Observer callback function types */
typedef void (*dsp_state_changed_cb)(int version, int load, void *user_data);
typedef void (*durec_status_changed_cb)(int status, int position, void *user_data);
typedef void (*samplerate_changed_cb)(int samplerate, void *user_data);
typedef void (*input_changed_cb)(int index, float gain, bool phantom, bool hiz, bool mute, void *user_data);
typedef void (*output_changed_cb)(int index, float volume, bool mute, void *user_data);
typedef void (*mixer_changed_cb)(int input, int output, float volume, float pan, void *user_data);

/* Observer registration functions */
int register_dsp_observer(dsp_state_changed_cb callback, void *user_data);
int register_durec_observer(durec_status_changed_cb callback, void *user_data);
int register_samplerate_observer(samplerate_changed_cb callback, void *user_data);
int register_input_observer(input_changed_cb callback, void *user_data);
int register_output_observer(output_changed_cb callback, void *user_data);
int register_mixer_observer(mixer_changed_cb callback, void *user_data);

/* Observer deregistration functions */
int unregister_dsp_observer(dsp_state_changed_cb callback, void *user_data);
int unregister_durec_observer(durec_status_changed_cb callback, void *user_data);
int unregister_samplerate_observer(samplerate_changed_cb callback, void *user_data);
int unregister_input_observer(input_changed_cb callback, void *user_data);
int unregister_output_observer(output_changed_cb callback, void *user_data);
int unregister_mixer_observer(mixer_changed_cb callback, void *user_data);

/* Notification functions */
void notify_dsp_state_changed(int version, int load);
void notify_durec_status_changed(int status, int position);
void notify_samplerate_changed(int samplerate);
void notify_input_changed(int index, float gain, bool phantom, bool hiz, bool mute);
void notify_output_changed(int index, float volume, bool mute);
void notify_mixer_changed(int input, int output, float volume, float pan);

// Add these declarations

/**
 * @brief Get observer registration status
 *
 * @param dsp_active Pointer to store DSP observer status (can be NULL)
 * @param durec_active Pointer to store DURec observer status (can be NULL)
 * @param samplerate_active Pointer to store samplerate observer status (can be NULL)
 * @param input_active Pointer to store input observer status (can be NULL)
 * @param output_active Pointer to store output observer status (can be NULL)
 * @param mixer_active Pointer to store mixer observer status (can be NULL)
 * @return Number of active observers
 */
int get_observer_status(bool *dsp_active, bool *durec_active, bool *samplerate_active,
                        bool *input_active, bool *output_active, bool *mixer_active);

#endif /* DEVICE_OBSERVERS_H */
