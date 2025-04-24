#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stddef.h>
#include "platform.h"
#include "device.h"
#include "device_state.h"

/* Error reporting functions */
void fatal(const char *msg, ...);

/* File and directory utilities */
int ensure_directory_exists(const char *path);
int get_app_home_directory(char *buffer, size_t size);
int path_join(char *buffer, size_t size, const char **components, int count);
int sanitize_filename(const char *input, char *output, size_t output_size);

/* Configuration management */
int dumpConfig(void);

/**
 * Dumps the current device state to the console
 * Displays information about the device, inputs, outputs and includes
 * saving the configuration to a file.
 */
void dumpDeviceState(void);

#endif /* UTIL_H */
