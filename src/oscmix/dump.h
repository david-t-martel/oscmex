/**
 * @file dump.h
 * @brief Functions for dumping OSCMix device state for debugging and configuration
 */

#ifndef DUMP_H
#define DUMP_H

#include "device.h"
#include "device_state.h"

/**
 * @brief Dumps the current device state to the console
 *
 * This function is called when an OSC message with address "/dump" is received.
 * It prints a detailed report of the current device state to stdout.
 */
void dump(void);

/**
 * @brief Dumps the device state to a JSON file
 *
 * @param dev Pointer to the device structure
 * @param state Pointer to the device state structure
 * @return 0 on success, non-zero on failure
 */
int dumpState(const struct device *dev, const struct devicestate *state);

/**
 * @brief Dumps the current device state to a file
 * Convenience function that calls dumpState with the current device and state
 *
 * @return 0 on success, non-zero on failure
 */
int dumpConfig(void);

/**
 * @brief Outputs the detailed device state to console
 */
void dumpDeviceState(void);

#endif /* DUMP_H */
