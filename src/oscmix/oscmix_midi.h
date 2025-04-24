/**
 * @file oscmix_midi.h
 * @brief MIDI SysEx handling functions
 */

#ifndef OSCMIX_MIDI_H
#define OSCMIX_MIDI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
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
 * @param len The length of the buffer in words
 */
void handleregs(uint_least32_t *payload, size_t len);

/**
 * @brief Handles audio level values received from the device
 *
 * @param subid The sub ID of the SysEx message
 * @param payload The buffer containing the audio level values
 * @param len The length of the buffer
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
 * @brief Sends a new integer parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newint(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new fixed-point parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newfixed(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new enumerated parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newenum(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new boolean parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newbool(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new stereo state for an input channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newinputstereo(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new stereo state for an output channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newoutputstereo(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new gain value for an input channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newinputgain(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new 48V phantom power state or reference level for an input channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newinput48v_reflevel(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new Hi-Z state for an input channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newinputhiz(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new DSP load value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newdspload(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new DSP availability value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newdspavail(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new DSP active value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newdspactive(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new ARC encoder value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newarcencoder(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new mix parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newmix(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new sample rate value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newsamplerate(const struct oscnode *path[], const char *addr, int reg, int val);

/**
 * @brief Sends a new dynamics level value as an OSC message
 *
 * @param path The OSC address path
 * @param unused Unused parameter
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newdynlevel(const struct oscnode *path[], const char *unused, int reg, int val);

/**
 * @brief Handles the completion of a refresh operation
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int refreshdone(const struct oscnode *path[], const char *addr, int reg, int val);

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

/* Functions for durec-related OSC messages */
int newdurecstatus(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurectime(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecusbstatus(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurectotalspace(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecfreespace(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecfileslen(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecfile(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecnext(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecrecordtime(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecindex(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecname(const struct oscnode *path[], const char *addr, int reg, int val);
int newdurecinfo(const struct oscnode *path[], const char *unused, int reg, int val);
int newdureclength(const struct oscnode *path[], const char *unused, int reg, int val);

#endif /* OSCMIX_MIDI_H */
