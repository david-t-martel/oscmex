/**
 * @file util.c
 * @brief Utility functions for the OSCMix application
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "platform.h"
#include "device.h"
#include "device_state.h"

/**
 * @brief Prints a fatal error message and exits the program
 *
 * @param msg The error message format string
 * @param ... Variable arguments for the format string
 */
void fatal(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);

	if (!msg)
	{
		perror(NULL);
	}
	else if (*msg && msg[strlen(msg) - 1] == ':')
	{
		fputc(' ', stderr);
		perror(NULL);
	}
	else
	{
		fputc('\n', stderr);
	}
	exit(1);
}

/**
 * @brief Creates the specified directory if it doesn't exist
 *
 * @param path The directory path to create
 * @return 0 on success, -1 on failure
 */
int ensure_directory_exists(const char *path)
{
	// Use platform abstraction for directory creation
	return platform_ensure_directory(path);
}

/**
 * @brief Get the application home directory
 *
 * @param buffer Buffer to store the path
 * @param size Size of the buffer
 * @return 0 on success, -1 on failure
 */
int get_app_home_directory(char *buffer, size_t size)
{
	// Use platform abstraction for getting app data directory
	return platform_get_app_data_dir(buffer, size);
}

/**
 * @brief Join multiple path components into a single path
 *
 * @param buffer Buffer to store the joined path
 * @param size Size of the buffer
 * @param components Array of path components
 * @param count Number of components
 * @return 0 on success, -1 on failure
 */
int path_join(char *buffer, size_t size, const char **components, int count)
{
	if (!buffer || size == 0 || !components || count <= 0)
		return -1;

	// Start with empty string
	buffer[0] = '\0';

	// For the first component
	if (count > 0 && components[0])
	{
		strncpy(buffer, components[0], size - 1);
		buffer[size - 1] = '\0';
	}

	// Join the rest of the components using platform_path_join
	for (int i = 1; i < count; i++)
	{
		if (components[i])
		{
			char temp[1024];
			strncpy(temp, buffer, sizeof(temp) - 1);
			temp[sizeof(temp) - 1] = '\0';

			if (platform_path_join(buffer, size, temp, components[i]) != 0)
			{
				return -1;
			}
		}
	}

	return 0;
}

/**
 * @brief Sanitize a filename by replacing invalid characters with underscores
 *
 * @param input The input filename
 * @param output The sanitized output filename
 * @param output_size Size of the output buffer
 * @return 0 on success, -1 on failure
 */
int sanitize_filename(const char *input, char *output, size_t output_size)
{
	// Use platform abstraction for creating valid filenames
	return platform_create_valid_filename(input, output, output_size);
}

/**
 * @brief Exports device configuration to a JSON file
 * @return 0 on success, negative value on error
 */
int dumpConfig(void)
{
	const struct device *cur_device = getDevice();

	if (!cur_device)
	{
		fprintf(stderr, "No device initialized\n");
		return -1;
	}

	// Get application config directory
	char config_dir[512];
	if (platform_get_device_config_dir(config_dir, sizeof(config_dir)) != 0)
	{
		fprintf(stderr, "Failed to get configuration directory\n");
		return -1;
	}

	// Ensure directory exists
	if (platform_ensure_directory(config_dir) != 0)
	{
		fprintf(stderr, "Failed to create directory: %s\n", config_dir);
		return -1;
	}

	// Generate timestamp for filename
	char date_str[32];
	if (platform_format_time(date_str, sizeof(date_str), "%Y%m%d-%H%M%S") != 0)
	{
		fprintf(stderr, "Failed to format current time\n");
		return -1;
	}

	// Sanitize device name for filename
	char safe_name[64];
	if (platform_create_valid_filename(cur_device->name, safe_name, sizeof(safe_name)) != 0)
	{
		fprintf(stderr, "Failed to create valid filename\n");
		return -1;
	}

	// Create full filepath
	char filename[768];
	char *parts[] = {config_dir, "audio-device_", safe_name, "_date-time_", date_str, ".json"};

	// Join parts to form the filename
	if (path_join(filename, sizeof(filename), (const char **)parts, 6) != 0)
	{
		fprintf(stderr, "Failed to create config filename\n");
		return -1;
	}

	// Open file using platform abstraction
	FILE *file = platform_fopen(filename, "w");
	if (!file)
	{
		fprintf(stderr, "Failed to open file for writing: %s\n", filename);
		return -1;
	}

	// Write device state as JSON - real implementation would have more fields
	fprintf(file, "{\n");
	fprintf(file, "  \"device\": {\n");
	fprintf(file, "    \"name\": \"%s\",\n", cur_device->name);
	fprintf(file, "    \"id\": \"%s\",\n", cur_device->id);
	fprintf(file, "    \"version\": %d,\n", cur_device->version);
	fprintf(file, "    \"flags\": %d,\n", cur_device->flags);
	fprintf(file, "    \"timestamp\": \"%s\"\n", date_str);
	fprintf(file, "  }\n");
	fprintf(file, "}\n");

	fclose(file);
	printf("Device configuration saved to: %s\n", filename);

	return 0;
}

/**
 * @brief Dumps the current device state to the console
 */
void dumpDeviceState(void)
{
	const struct device *current = getDevice();

	if (!current)
	{
		fprintf(stderr, "No device initialized\n");
		return;
	}

	printf("Device: %s (ID: %s, Version: %d)\n",
		   current->name, current->id, current->version);
	printf("Inputs: %d, Outputs: %d\n",
		   current->inputslen, current->outputslen);

	// Print more device state information if needed
	printf("Flags: 0x%X\n", current->flags);

	// List input and output names
	printf("\nInput channels:\n");
	for (int i = 0; i < current->inputslen; i++)
	{
		printf("  %2d: %s (flags: 0x%X)\n",
			   i + 1, current->inputs[i].name, current->inputs[i].flags);
	}

	printf("\nOutput channels:\n");
	for (int i = 0; i < current->outputslen; i++)
	{
		printf("  %2d: %s (flags: 0x%X)\n",
			   i + 1, current->outputs[i].name, current->outputs[i].flags);
	}

	// Save state to file
	printf("\nSaving state to file...\n");
	dumpConfig();
}
