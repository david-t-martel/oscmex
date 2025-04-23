/**
 * @file oscmix.h
 * @brief Core interface for OSCMix - a bridge between OSC messages and RME audio devices
 *
 * OSCMix provides a two-way translation layer between Open Sound Control (OSC)
 * messages from a network and MIDI SysEx commands to control RME audio interfaces.
 * This header defines the main entry points and callback functions used by the application.
 */

#ifndef OSCMIX_H
#define OSCMIX_H

/**
 * @brief Initialize the OSCMix application
 *
 * This function initializes the OSCMix application, setting up the parameter tree,
 * internal state tracking, and preparing to communicate with the specified RME device.
 * It establishes the connection to the device and requests initial state.
 *
 * @param port  The MIDI device name or ID to connect to. For Windows, this can be
 *              a device name substring or numeric ID. For Unix systems, this is used
 *              to identify the proper MIDI connection.
 *
 * @return 0 on success, non-zero on failure
 */
int init(const char *port);

/**
 * @brief Process incoming SysEx messages from the RME device
 *
 * This function handles incoming MIDI SysEx messages from the RME device,
 * updating the internal state representation and potentially triggering
 * OSC messages to be sent as notifications of parameter changes.
 *
 * The function parses the SysEx message, extracts parameter updates, and
 * synchronizes the internal state with the device's state.
 *
 * @param buf      Pointer to the SysEx message data
 * @param len      Length of the SysEx message in bytes
 * @param payload  Buffer to store extracted parameter values (must be at least len/4 in size)
 */
void handlesysex(const unsigned char *buf, size_t len, uint32_t *payload);

/**
 * @brief Process incoming OSC messages from the network
 *
 * This function handles OSC messages received from the network, parses
 * the OSC address pattern and arguments, and dispatches the commands
 * to modify device parameters, get parameter values, or execute special
 * commands like refresh or dump.
 *
 * @param buf  Pointer to the OSC message data
 * @param len  Length of the OSC message in bytes
 *
 * @return 0 on success, non-zero on failure
 */
int handleosc(const unsigned char *buf, size_t len);

/**
 * @brief Perform periodic processing like level meter updates
 *
 * This function is called periodically (typically every 100ms) to handle
 * tasks like updating level meters and other time-based operations.
 *
 * @param levels  Whether to process and send level meter updates (true) or not (false)
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

/**
 * Dump device state to console
 */
void dump(void);

/**
 * Dump device state to a JSON file
 * @return 0 on success, non-zero on failure
 */
int dumpState(void);

/**
 * Exports device configuration to a JSON file
 * @return 0 on success, negative value on error
 */
int dumpConfig(void);

#endif /* OSCMIX_H */
