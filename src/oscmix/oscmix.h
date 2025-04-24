/**
 * @file oscmix.h
 * @brief Main interface file for OSCMix
 *
 * This header declares the main entry points for the OSCMix application
 * which provides a bridge between OSC messages and RME audio devices.
 */

#ifndef OSCMIX_H
#define OSCMIX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations for OSC message structure
struct oscmsg;
struct oscnode;
struct device;

/**
 * @brief Initialize the OSCMix application
 *
 * @param port The device name or identifier
 * @return 0 on success, non-zero on failure
 */
int init(const char *port);

/**
 * @brief Process incoming SysEx messages from the device
 *
 * @param buf The buffer containing the SysEx message
 * @param len The length of the buffer
 * @param payload Buffer to store the decoded payload
 */
void handlesysex(const unsigned char *buf, size_t len, uint_least32_t *payload);

/**
 * @brief Process incoming OSC messages from the network
 *
 * @param buf The buffer containing the OSC message
 * @param len The length of the buffer
 * @return 0 on success, non-zero on failure
 */
int handleosc(const void *buf, size_t len);

/**
 * @brief Handle periodic timer events
 *
 * @param levels Whether to request level updates
 */
void handletimer(bool levels);

/**
 * @brief Send MIDI data to the RME device
 *
 * Platform-specific implementation to send MIDI messages (particularly SysEx)
 * to the connected RME device. This function is implemented in the platform-specific
 * main implementation file (main.c).
 *
 * @param buf  Pointer to the MIDI data to send
 * @param len  Length of the MIDI data in bytes
 */
extern void writemidi(const void *buf, size_t len);

/**
 * @brief Send OSC data to the network
 *
 * Platform-specific implementation to send OSC messages over the network
 * to listening clients. This function is implemented in the platform-specific
 * main implementation file (main.c).
 *
 * @param buf  Pointer to the OSC data to send
 * @param len  Length of the OSC data in bytes
 */
extern void writeosc(const void *buf, size_t len);

/*
 * The following functions provide access to the specialized modules in OSCMix.
 * Each module is defined in its own header file with more detailed interfaces.
 */

/* ----------------- oscmix_midi.h functionality ----------------- */

/**
 * @brief Sets a register value in the device
 * @see oscmix_midi.h for full details
 */
int setreg(unsigned reg, unsigned val);

/**
 * @brief Sets an audio level parameter value in the device
 * @see oscmix_midi.h for full details
 */
int setlevel(int reg, float level);

/**
 * @brief Sets a dB value in the device
 * @see oscmix_midi.h for full details
 */
int setdb(int reg, float db);

/**
 * @brief Sets a pan value in the device
 * @see oscmix_midi.h for full details
 */
int setpan(int reg, int pan);

/**
 * @brief Handles register values received from the device
 * @see oscmix_midi.h for full details
 */
void handleregs(uint_least32_t *payload, size_t len);

/**
 * @brief Handles audio level values received from the device
 * @see oscmix_midi.h for full details
 */
void handlelevels(int subid, uint_least32_t *payload, size_t len);

/**
 * @brief Sends an OSC message with variable arguments
 * @see oscmix_midi.h for full details
 */
void oscsend(const char *addr, const char *type, ...);

/**
 * @brief Sends an enumerated value as an OSC message
 * @see oscmix_midi.h for full details
 */
void oscsendenum(const char *addr, int value);

/* ----------------- oscmix_commands.h functionality ----------------- */

/**
 * @brief Set a boolean parameter value in the device
 * @see oscmix_commands.h for full details
 */
int setbool(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set an integer parameter value in the device
 * @see oscmix_commands.h for full details
 */
int setint(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set a fixed-point parameter value in the device
 * @see oscmix_commands.h for full details
 */
int setfixed(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set an enumerated parameter value in the device
 * @see oscmix_commands.h for full details
 */
int setenum(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Trigger a refresh of the device state
 * @see oscmix_commands.h for full details
 */
int setrefresh(const struct oscnode *path[], int reg, struct oscmsg *msg);

/* ----------------- oscnode_tree.h functionality ----------------- */

/**
 * @brief Root node of the OSC address tree
 * @see oscnode_tree.h for full details
 */
extern const struct oscnode tree[];

/**
 * @brief Initialize the OSC node tree
 * @see oscnode_tree.h for full details
 */
int oscnode_tree_init(void);

/**
 * @brief Find a node in the OSC tree by path components
 * @see oscnode_tree.h for full details
 */
int oscnode_find(const char **components, int ncomponents,
                 const struct oscnode *path[], int npath);

/* ----------------- device_state.h functionality ----------------- */

/**
 * @brief Dump the current device state
 * @see device_state.h for full details
 */
void dumpDeviceState(void);

/**
 * @brief Export device configuration to a file
 * @see device_state.h for full details
 */
int dumpConfig(void);

/**
 * @brief Get the current device
 * @see device_state.h for full details
 */
const struct device *getDevice(void);

/**
 * @brief Set or get refreshing state
 * @see device_state.h for full details
 */
bool refreshing_state(int value);

#endif /* OSCMIX_H */
