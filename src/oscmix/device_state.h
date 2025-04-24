/**
 * @file device_state.h
 * @brief Device state tracking functionality for OSCMix
 */

#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <stdbool.h>
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
 * @brief Get input channel volume
 * @param channel The input channel index (0-based)
 * @return Volume in dB
 */
float getInputVolume(int channel);

/**
 * @brief Get input channel mute state
 * @param channel The input channel index (0-based)
 * @return true if muted, false otherwise
 */
bool getInputMute(int channel);

/**
 * @brief Get input channel gain
 * @param channel The input channel index (0-based)
 * @return Gain in dB
 */
float getInputGain(int channel);

/**
 * @brief Get input phantom power state
 * @param channel The input channel index (0-based)
 * @return true if phantom power is on, false otherwise
 */
bool getInputPhantom(int channel);

/**
 * @brief Get output channel volume
 * @param channel The output channel index (0-based)
 * @return Volume in dB
 */
float getOutputVolume(int channel);

/**
 * @brief Get output channel mute state
 * @param channel The output channel index (0-based)
 * @return true if muted, false otherwise
 */
bool getOutputMute(int channel);

/**
 * @brief Get output channel reference level
 * @param channel The output channel index (0-based)
 * @return Reference level string
 */
const char *getOutputRefLevel(int channel);

/**
 * @brief Get mixer volume level
 * @param inputChannel The input channel index (0-based)
 * @param outputChannel The output channel index (0-based)
 * @return Volume in dB
 */
float getMixerVolume(int inputChannel, int outputChannel);

/**
 * @brief Get mixer pan value
 * @param inputChannel The input channel index (0-based)
 * @param outputChannel The output channel index (0-based)
 * @return Pan value from -100 (left) to 100 (right)
 */
float getMixerPan(int inputChannel, int outputChannel);

/**
 * @brief Dump the current device state to a file
 * @return 0 on success, non-zero on failure
 */
int dumpDeviceState(void);

/**
 * @brief Dump device configuration to a file
 * @return 0 on success, non-zero on failure
 */
int dumpConfig(void);

#endif /* DEVICE_STATE_H */
