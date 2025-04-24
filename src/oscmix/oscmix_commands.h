/**
 * @file oscmix_commands.h
 * @brief Command handlers for OSC messages
 */

#ifndef OSCMIX_COMMANDS_H
#define OSCMIX_COMMANDS_H

#include <stddef.h>
#include "osc.h"
#include "oscnode_tree.h"

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
 * @brief Set the mute state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setinputmute(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the stereo state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setinputstereo(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the name for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setinputname(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the gain for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setinputgain(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the 48V phantom power state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setinput48v(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the Hi-Z state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setinputhiz(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the loopback state for an output channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setoutputloopback(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the EQD record state for the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int seteqdrecord(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set a mix parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setmix(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the durec file value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setdurecfile(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the durec stop state in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setdurecstop(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the durec play state in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setdurecplay(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the durec record state in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setdurecrecord(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Set the durec delete state in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setdurecdelete(const struct oscnode *path[], int reg, struct oscmsg *msg);

/**
 * @brief Trigger a refresh of the device state
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing any parameters
 * @return 0 on success, non-zero on failure
 */
int setrefresh(const struct oscnode *path[], int reg, struct oscmsg *msg);

#endif /* OSCMIX_COMMANDS_H */
