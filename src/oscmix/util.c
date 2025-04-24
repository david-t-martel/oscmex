/**
 * @file util.c
 * @brief Utility functions for the OSCMix application
 */

#include "util.h"
#include "platform.h"
#include "logging.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* Report a fatal error and exit */
void fatal(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	fprintf(stderr, "Fatal error: ");
	vfprintf(stderr, msg, ap);
	fprintf(stderr, "\n");
	va_end(ap);

	exit(1);
}

/* Create directories if they don't exist */
int ensure_directory_exists(const char *path)
{
	// Use platform function instead of custom implementation
	return platform_ensure_directory(path);
}

/* Get the application home directory */
int get_app_home_directory(char *buffer, size_t size)
{
	// Use platform function instead of custom implementation
	return platform_get_app_data_dir(buffer, size);
}

/* Join path components */
int path_join(char *buffer, size_t size, const char **components, int count)
{
	if (!buffer || size == 0 || !components || count <= 0)
		return -1;

	// Copy the first component
	if (strlcpy(buffer, components[0], size) >= size)
		return -1;

	// Join with the remaining components
	for (int i = 1; i < count; i++)
	{
		if (platform_path_join(buffer, size, buffer, components[i]) != 0)
			return -1;
	}

	return 0;
}

/* Convert a string to a valid filename */
int sanitize_filename(const char *input, char *output, size_t output_size)
{
	// Use the platform function for filename sanitization
	return platform_create_valid_filename(input, output, output_size);
}

/* Dump configuration to a file */
int dumpConfig(void)
{
	const struct device *device = getDevice();
	if (!device)
	{
		log_error("No device available for configuration dump");
		return -1;
	}

	char app_dir[PLATFORM_MAX_PATH];
	char config_dir[PLATFORM_MAX_PATH];
	char timestamp[64];
	char safe_device_name[256];
	char filename[256];
	char filepath[PLATFORM_MAX_PATH];

	// Get application data directory
	if (platform_get_app_data_dir(app_dir, sizeof(app_dir)) != 0)
	{
		log_error("Failed to get application data directory");
		return -1;
	}

	// Create path for device_config directory
	if (platform_path_join(config_dir, sizeof(config_dir), app_dir, "OSCMix/device_config") != 0)
	{
		log_error("Failed to create config directory path");
		return -1;
	}

	// Create directory if it doesn't exist
	if (platform_ensure_directory(config_dir) != 0)
	{
		log_error("Failed to create config directory: %s", config_dir);
		return -1;
	}

	// Format current date and time
	if (platform_format_time(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S") != 0)
	{
		// Fallback to a basic timestamp
		time_t now = time(NULL);
		struct tm *tm_info = localtime(&now);
		strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);
	}

	// Sanitize device name
	if (platform_create_valid_filename(device->name, safe_device_name, sizeof(safe_device_name)) != 0)
	{
		// Fallback to simple name if function fails
		strncpy(safe_device_name, "device", sizeof(safe_device_name) - 1);
		safe_device_name[sizeof(safe_device_name) - 1] = '\0';
	}

	// Create filename
	snprintf(filename, sizeof(filename), "audio-device_%s_date-time_%s.json",
			 safe_device_name, timestamp);

	// Create full file path
	if (platform_path_join(filepath, sizeof(filepath), config_dir, filename) != 0)
	{
		log_error("Failed to create complete file path");
		return -1;
	}

	// Generate JSON configuration
	char *config_json = generateDeviceConfigJson(device);
	if (!config_json)
	{
		log_error("Failed to generate device configuration JSON");
		return -1;
	}

	// Write state to file using platform function
	FILE *fp = platform_fopen(filepath, "w");
	if (!fp)
	{
		log_error("Failed to open file for writing: %s", filepath);
		free(config_json);
		return -1;
	}

	fputs(config_json, fp);
	fclose(fp);
	free(config_json);

	log_info("Device configuration saved to: %s", filepath);
	return 0;
}

/* Display complete device state */
void dumpDeviceState(void)
{
	const struct device *device = getDevice();
	if (!device)
	{
		printf("No device available.\n");
		return;
	}

	printf("Device: %s\n", device->name);
	printf("Inputs: %d\n", device->inputslen);
	printf("Outputs: %d\n", device->outputslen);
	printf("Playback: %d\n", device->playbacklen);
	printf("Mixer: %d\n", device->mixerlen);

	printf("\nInput Channels:\n");
	for (int i = 0; i < device->inputslen; i++)
	{
		const struct inputinfo *input = &device->inputs[i];
		struct input_state *state = get_input_state_struct(i);

		printf("  %2d: %-20s [", i + 1, input->name);

		if (input->flags & INPUT_GAIN)
			printf(" Gain");
		if (input->flags & INPUT_48V)
			printf(" 48V");
		if (input->flags & INPUT_REFLEVEL)
			printf(" RefLevel");
		if (input->flags & INPUT_HIZ)
			printf(" HiZ");
		if (input->flags & INPUT_PAD)
			printf(" Pad");

		printf(" ]\n");

		if (state)
		{
			printf("      Gain: %.1f dB\n", state->gain);
			if (input->flags & INPUT_48V)
				printf("      Phantom: %s\n", state->phantom ? "On" : "Off");
			if (input->flags & INPUT_HIZ)
				printf("      HiZ: %s\n", state->hiz ? "On" : "Off");
			printf("      Mute: %s\n", state->mute ? "On" : "Off");
			printf("      Stereo: %s\n", state->stereo ? "Yes" : "No");
			if (input->flags & INPUT_REFLEVEL)
				printf("      Ref Level: %d\n", state->reflevel);
		}
	}

	printf("\nOutput Channels:\n");
	for (int i = 0; i < device->outputslen; i++)
	{
		const struct outputinfo *output = &device->outputs[i];
		struct output_state *state = get_output_state_struct(i);

		printf("  %2d: %-20s [", i + 1, output->name);

		if (output->flags & OUTPUT_VOLUME)
			printf(" Volume");
		if (output->flags & OUTPUT_MUTE)
			printf(" Mute");
		if (output->flags & OUTPUT_REFLEVEL)
			printf(" RefLevel");
		if (output->flags & OUTPUT_DITHER)
			printf(" Dither");

		printf(" ]\n");

		if (state)
		{
			printf("      Volume: %.1f dB\n", state->volume);
			printf("      Mute: %s\n", state->mute ? "On" : "Off");
			printf("      Stereo: %s\n", state->stereo ? "Yes" : "No");
			if (output->flags & OUTPUT_REFLEVEL)
				printf("      Ref Level: %d\n", state->reflevel);
		}
	}

	// Save configuration to file
	int result = dumpConfig();
	if (result == 0)
	{
		printf("\nConfiguration saved to file.\n");
	}
	else
	{
		printf("\nFailed to save configuration to file.\n");
	}
}
