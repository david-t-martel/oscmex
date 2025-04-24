/**
 * @file oscmix_midi.h
 * @brief MIDI SysEx handling and OSC response functions
 */

#ifndef OSCMIX_MIDI_H
#define OSCMIX_MIDI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "oscnode_tree.h"
#include "osc.h"

/**
 * @brief Sets a register value in the device
 *
 * @param reg The register address
 * @param val The value to set
 * @return 0 on success, non-zero on failure
 */
int setreg(unsigned reg, unsigned val);

/**
 * @brief Register OSC observers for device state changes
 *
 * This function sets up callbacks that send OSC messages
 * when device state changes occur.
 *
 * @return 0 on success, non-zero on failure
 */
int register_osc_observers(void);

/**
 * @brief Register essential OSC observers only
 *
 * @return 0 on success, non-zero on failure
 */
int register_essential_osc_observers(void);

/**
 * @brief Sets an audio level parameter value in the device
 *
 * @param reg The register address
 * @param level The audio level value
 * @return 0 on success, non-zero on failure
 */
int setlevel(int reg, float level);

/**
 * @brief Sets a dB value in the device
 *
 * @param reg The register address
 * @param db The dB value
 * @return 0 on success, non-zero on failure
 */
int setdb(int reg, float db);

/**
 * @brief Sets a pan value in the device
 *
 * @param reg The register address
 * @param pan The pan value
 * @return 0 on success, non-zero on failure
 */
int setpan(int reg, int pan);

/**
 * @brief Handles register values received from the device
 *
 * @param payload The buffer containing the register values
 * @param len The length of the buffer in 32-bit words
 */
void handleregs(uint_least32_t *payload, size_t len);

/**
 * @brief Handles audio level values received from the device
 *
 * @param subid The sub ID of the SysEx message
 * @param payload The buffer containing the audio level values
 * @param len The length of the buffer in 32-bit words
 */
void handlelevels(int subid, uint_least32_t *payload, size_t len);

/**
 * @brief Sends an OSC message with variable arguments
 *
 * @param addr The OSC address
 * @param type The OSC type tags
 * @param ... The OSC arguments
 */
void oscsend(const char *addr, const char *type, ...);

/**
 * @brief Sends an enumerated value as an OSC message
 *
 * @param addr The OSC address
 * @param val The enumerated value
 * @param names The array of enumeration names
 * @param nameslen The length of the array
 */
void oscsendenum(const char *addr, int val, const char *const names[], size_t nameslen);

/**
 * @brief Flushes the OSC message buffer
 */
void oscflush(void);

/**
 * @brief Gets the sample rate corresponding to a value
 *
 * @param val The value representing the sample rate
 * @return The sample rate in Hz
 */
long getsamplerate(int val);

/**
 * @brief Writes a SysEx message to the device
 *
 * @param subid The sub ID of the SysEx message
 * @param buf The buffer containing the SysEx message data
 * @param len The length of the buffer
 * @param sysexbuf The buffer to store the encoded SysEx message
 */
void writesysex(int subid, const unsigned char *buf, size_t len, unsigned char *sysexbuf);

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

/**
 * @brief Send complete device state via OSC
 *
 * This function sends the entire device state to OSC clients,
 * useful for GUI initialization or reconnection.
 */
void send_full_device_state(void);

/**
 * @brief Set the last error information
 *
 * @param code Error code
 * @param context Error context string
 * @param format Format string followed by arguments (printf style)
 * @param ... Variable arguments
 */
void set_last_error(int code, const char *context, const char *format, ...);

/**
 * @brief Get the last error information
 *
 * @param code Pointer to store error code (can be NULL)
 * @param context Buffer to store context string (can be NULL)
 * @param context_size Size of context buffer
 * @return Error message string
 */
const char *get_last_error(int *code, char *context, size_t context_size);

/**
 * @brief Initialize the MIDI interface
 *
 * @return 0 on success, non-zero on failure
 */
int oscmix_midi_init(void);

/**
 * @brief Close the MIDI interface
 */
void oscmix_midi_close(void);

/* Parameter update handler functions */
int newint(const struct oscnode *path[], const char *addr, int reg, int val);
int newfixed(const struct oscnode *path[], const char *addr, int reg, int val);
int newenum(const struct oscnode *path[], const char *addr, int reg, int val);
int newbool(const struct oscnode *path[], const char *addr, int reg, int val);
int newinputstereo(const struct oscnode *path[], const char *addr, int reg, int val);
int newoutputstereo(const struct oscnode *path[], const char *addr, int reg, int val);
int newinputgain(const struct oscnode *path[], const char *addr, int reg, int val);
int newinput48v_reflevel(const struct oscnode *path[], const char *addr, int reg, int val);
int newinputhiz(const struct oscnode *path[], const char *addr, int reg, int val);
int newmix(const struct oscnode *path[], const char *addr, int reg, int val);
int newsamplerate(const struct oscnode *path[], const char *addr, int reg, int val);
int newdspload(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecstatus(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurectime(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecusbstatus(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurectotalspace(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecfreespace(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecfileslen(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecfile(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecnext(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecrecordtime(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecplaymode(const struct oscnode *path[], const char *addr, int reg, int val);
int refreshdone(const struct oscnode *path[], const char *addr, int reg, int val);

#endif /* OSCMIX_MIDI_H */
