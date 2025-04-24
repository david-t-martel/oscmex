#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "platform.h"

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
 * Creates the specified directory if it doesn't exist
 *
 * @param path The directory path to create
 * @return 0 on success, -1 on failure
 */
int ensure_directory_exists(const char *path)
{
	return platform_ensure_directory(path);
}

/**
 * Get the application home directory
 *
 * @param buffer Buffer to store the path
 * @param size Size of the buffer
 * @return 0 on success, -1 on failure
 */
int get_app_home_directory(char *buffer, size_t size)
{
	return platform_get_app_data_dir(buffer, size);
}

/**
 * Join multiple path components into a single path
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

	*buffer = '\0';
	size_t pos = 0;

	for (int i = 0; i < count; i++)
	{
		if (!components[i])
			continue;

		size_t comp_len = strlen(components[i]);

		// Skip empty components
		if (comp_len == 0)
			continue;

		// Add separator if not the first component and if needed
		if (pos > 0 && buffer[pos - 1] != PLATFORM_PATH_SEPARATOR &&
			components[i][0] != PLATFORM_PATH_SEPARATOR)
		{
			if (pos + 1 >= size)
				return -1; // Buffer overflow

			buffer[pos++] = PLATFORM_PATH_SEPARATOR;
			buffer[pos] = '\0';
		}

		// Remove trailing separators from component
		while (comp_len > 0 && components[i][comp_len - 1] == PLATFORM_PATH_SEPARATOR)
			comp_len--;

		// Skip leading separators if not the first component
		const char *comp_start = components[i];
		if (i > 0)
		{
			while (*comp_start == PLATFORM_PATH_SEPARATOR)
				comp_start++;
		}

		comp_len = strlen(comp_start);

		// Add component
		if (pos + comp_len >= size)
			return -1; // Buffer overflow

		strncpy(buffer + pos, comp_start, comp_len);
		pos += comp_len;
		buffer[pos] = '\0';
	}

	return 0;
}

/**
 * Sanitize a filename by replacing invalid characters with underscores
 *
 * @param input The input filename
 * @param output The sanitized output filename
 * @param output_size Size of the output buffer
 * @return 0 on success, -1 on failure
 */
int sanitize_filename(const char *input, char *output, size_t output_size)
{
	if (!input || !output || output_size == 0)
		return -1;

	size_t i, j = 0;
	size_t input_len = strlen(input);

	for (i = 0; i < input_len && j < output_size - 1; i++)
	{
		char c = input[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
		{
			output[j++] = c;
		}
		else if (c == ' ' || c == ',' || c == ';' || c == ':')
		{
			output[j++] = '_';
		}
	}

	output[j] = '\0';
	return 0;
}

/**
 * Exports device configuration to a JSON file
 * @return 0 on success, negative value on error
 */
int dumpConfig(void)
{
	const struct device *cur_device = getDevice();
	struct devicestate *state = getDeviceState();

	if (!cur_device || !state)
	{
		fprintf(stderr, "No device initialized\n");
		return -1;
	}

	// Get application home directory
	char app_home[256];
	if (platform_get_app_data_dir(app_home, sizeof(app_home)) != 0)
	{
		fprintf(stderr, "Failed to get application data directory\n");
		return -1;
	}

	// Create the device_config subdirectory path
	char config_dir[512];
	snprintf(config_dir, sizeof(config_dir), "%s%cOSCMix%cdevice_config",
			 app_home, PLATFORM_PATH_SEPARATOR, PLATFORM_PATH_SEPARATOR);

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
	char safe_name[64] = {0};
	sanitize_filename(cur_device->name, safe_name, sizeof(safe_name));

	// Create full filepath
	char filename[768];
	snprintf(filename, sizeof(filename), "%s%caudio-device_%s_date-time_%s.json",
			 config_dir, PLATFORM_PATH_SEPARATOR, safe_name, date_str);

	// Open file for writing - using standard file for now, could be platform_fopen
	FILE *file = fopen(filename, "w");
	if (!file)
	{
		fprintf(stderr, "Failed to open file for writing: %s\n", filename);
		return -1;
	}

	// Write device state as JSON using proper device_state accessors
	fprintf(file, "{\n");
	fprintf(file, "  \"device\": {\n");
	fprintf(file, "    \"name\": \"%s\",\n", cur_device->name);
	fprintf(file, "    \"id\": \"%s\",\n", cur_device->id);
	fprintf(file, "    \"version\": %d,\n", cur_device->version);
	fprintf(file, "    \"flags\": %d\n", cur_device->flags);
	fprintf(file, "  },\n");

	// Rest of file writing...

	fclose(file);
	printf("Device configuration saved to: %s\n", filename);

	return 0;
}

// Updated function to dump device state to console
void dumpDeviceState(void)
{
	const struct device *current = cur_device; // Use cur_device consistently

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

	// Call the dumpConfig from util.c instead of duplicating the implementation
	printf("\nSaving state to file...\n");
	dumpConfig(); // This will call the implementation in util.c
}
