/**
 * @file oscmix_commands.h
 * @brief Command handlers for OSC messages
 */

#ifndef OSCMIX_COMMANDS_H
#define OSCMIX_COMMANDS_H

#include "oscnode_tree.h"
#include "osc.h"

/**
 * @brief Set an integer parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setint(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set a fixed-point parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setfixed(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set an enumerated parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setenum(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set a boolean parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setbool(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Trigger a refresh of the device state
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing any parameters
 * @return 0 on success, non-zero on failure
 */
int setrefresh(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the name of an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the name string
 * @return 0 on success, non-zero on failure
 */
int setinputname(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the gain for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the gain value
 * @return 0 on success, non-zero on failure
 */
int setinputgain(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the 48V phantom power state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the state value
 * @return 0 on success, non-zero on failure
 */
int setinput48v(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the Hi-Z state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the state value
 * @return 0 on success, non-zero on failure
 */
int setinputhiz(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the stereo status for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the stereo state
 * @return 0 on success, non-zero on failure
 */
int setinputstereo(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the mute state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the mute state
 * @return 0 on success, non-zero on failure
 */
int setinputmute(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the loopback state for an output channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the loopback state
 * @return 0 on success, non-zero on failure
 */
int setoutputloopback(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set EQD record state
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the EQD state
 * @return 0 on success, non-zero on failure
 */
int seteqdrecord(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the mix parameters for a channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the mix parameters
 * @return 0 on success, non-zero on failure
 */
int setmix(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Command to handle durec stop
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecstop(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Command to handle durec play
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecplay(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Command to handle durec record
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecrecord(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Command to handle durec delete
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecdelete(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Command to handle durec file selection
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecfile(const struct oscnode *path[], int reg, struct oscmsg *msg);

#endif /* OSCMIX_COMMANDS_H */
