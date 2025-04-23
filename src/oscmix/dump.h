#ifndef DUMP_H
#define DUMP_H

#include "device.h"
#include "device_state.h"

/**
 * @brief Dumps the current device state to the console
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

#endif /* DUMP_H */
