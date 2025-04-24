/**
 * @file device_state.h
 * @brief Device state tracking functionality for OSCMix
 */

#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h> // Add this for INFINITY
#include "device.h"

/* Device state representation for inputs */
struct inputstate
{
    float gain;
    bool phantom;
    bool hiz;
    bool mute;
    int reflevel;
};

/* Device state representation for outputs */
struct outputstate
{
    float volume;
    bool mute;
    int reflevel;
};

/* Complete device state structure */
struct devicestate
{
    struct inputstate *inputs;
    struct outputstate *outputs;
    float *mixervolume; /* 2D array: [input][output] */
    float *mixerpan;    /* 2D array: [input][output] */
    int samplerate;
    char clocksource[32];
    int buffersize;
};

/* Device state initialization and cleanup */
int initDeviceState(const struct device *dev);
void cleanupDeviceState(void);
struct devicestate *getDeviceState(void);

/* Current device accessor */
const struct device *getDevice(void);

/* Input state getters/setters */
float getInputGain(int index);
void setInputGain(int index, float gain);
bool getInputPhantom(int index);
void setInputPhantom(int index, bool enabled);
const char *getInputRefLevel(int index);
void setInputRefLevel(int index, int level);
bool getInputHiZ(int index);
void setInputHiZ(int index, bool enabled);
bool getInputMute(int index);
void setInputMute(int index, bool muted);

/* Output state getters/setters */
float getOutputVolume(int index);
void setOutputVolume(int index, float volume);
bool getOutputMute(int index);
void setOutputMute(int index, bool muted);
const char *getOutputRefLevel(int index);
void setOutputRefLevel(int index, int level);

/* Mixer state getters/setters */
float getMixerVolume(int input, int output);
void setMixerVolume(int input, int output, float volume);
float getMixerPan(int input, int output);
void setMixerPan(int input, int output, float pan);

/* System state getters/setters */
int getSampleRate(void);
void setSampleRate(int rate);
const char *getClockSource(void);
void setClockSource(const char *source);
int getBufferSize(void);
void setBufferSize(int size);

/* Save current device state to a file */
int saveDeviceState(void);

/**
 * @brief Initialize device state tracking
 * @param device Pointer to the device structure
 * @return 0 on success, non-zero on failure
 */
int device_state_init(const struct device *device);

/**
 * @brief Get or set the refreshing state
 * @param value If >= 0, set the state to this value
 * @return Current refreshing state
 */
bool refreshing_state(int value);

/**
 * @brief Get the current device
 * @return Pointer to the current device structure
 */
const struct device *getDevice(void);

/**
 * @brief Dump the current device state
 */
void dumpDeviceState(void);

/**
 * @brief Export device configuration to a file
 * @return 0 on success, non-zero on failure
 */
int dumpConfig(void);

/**
 * @brief Structure for mixer settings
 */
struct mix_state
{
    signed char pan;
    short vol;
};

/**
 * @brief Structure for input channel settings
 */
struct input_state
{
    bool stereo;
    bool mute;
    float width;
};

/**
 * @brief Structure for output channel settings
 */
struct output_state
{
    bool stereo;
    struct mix_state *mix;
};

/**
 * @brief Structure for DURec file information
 */
struct durecfile_state
{
    short reg[6];
    char name[9];
    unsigned long samplerate;
    unsigned channels;
    unsigned length;
};

/**
 * @brief Get DSP state information
 * @param vers Pointer to store DSP version
 * @param load Pointer to store DSP load value
 */
void get_dsp_state(int *vers, int *load);

/**
 * @brief Set DSP state information
 * @param vers DSP version to set (-1 to leave unchanged)
 * @param load DSP load value to set (-1 to leave unchanged)
 */
void set_dsp_state(int vers, int load);

/**
 * @brief Get input channel state
 * @param index Input channel index (0-based)
 * @return Pointer to input channel state
 */
struct input_state *get_input_state(int index);

/**
 * @brief Get playback channel state
 * @param index Playback channel index (0-based)
 * @return Pointer to playback channel state
 */
struct input_state *get_playback_state(int index);

/**
 * @brief Get output channel state
 * @param index Output channel index (0-based)
 * @return Pointer to output channel state
 */
struct output_state *get_output_state(int index);

/**
 * @brief Get durec state information
 * @param status Pointer to store status value
 * @param position Pointer to store position value
 * @param time Pointer to store time value
 * @param usberrors Pointer to store USB errors count
 * @param usbload Pointer to store USB load value
 * @param totalspace Pointer to store total space value
 * @param freespace Pointer to store free space value
 * @param file Pointer to store current file index
 * @param recordtime Pointer to store record time value
 * @param index Pointer to store index value
 * @param next Pointer to store next value
 * @param playmode Pointer to store playmode value
 */
void get_durec_state(int *status, int *position, int *time,
                     int *usberrors, int *usbload,
                     float *totalspace, float *freespace,
                     int *file, int *recordtime, int *index,
                     int *next, int *playmode);

/**
 * @brief Set durec state information
 * @param status Status value to set (-1 to leave unchanged)
 * @param position Position value to set (-1 to leave unchanged)
 * @param time Time value to set (-1 to leave unchanged)
 * @param usberrors USB errors count to set (-1 to leave unchanged)
 * @param usbload USB load value to set (-1 to leave unchanged)
 * @param totalspace Total space value to set (<0 to leave unchanged)
 * @param freespace Free space value to set (<0 to leave unchanged)
 * @param file Current file index to set (-1 to leave unchanged)
 * @param recordtime Record time value to set (-1 to leave unchanged)
 * @param index Index value to set (-1 to leave unchanged)
 * @param next Next value to set (-1 to leave unchanged)
 * @param playmode Playmode value to set (-1 to leave unchanged)
 */
void set_durec_state(int status, int position, int time,
                     int usberrors, int usbload,
                     float totalspace, float freespace,
                     int file, int recordtime, int index,
                     int next, int playmode);

/**
 * @brief Get durec files information
 * @param fileslen Pointer to store number of files
 * @return Pointer to array of durecfile structures
 */
struct durecfile_state *get_durec_files(size_t *fileslen);

/**
 * @brief Set durec files length
 * @param fileslen New file count
 * @return 0 on success, non-zero on failure
 */
int set_durec_files_length(size_t fileslen);

/**
 * @brief Clean up device state resources
 */
void device_state_cleanup(void);

#endif /* DEVICE_STATE_H */
