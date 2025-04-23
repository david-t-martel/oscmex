#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "platform.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

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
 * Dump device configuration to a JSON file
 *
 * @param device The device to dump configuration for
 * @return 0 on success, -1 on failure
 */
int dumpConfig(const Device *device)
{
	if (!device)
	{
		LOG_ERROR("Cannot dump config: device is NULL");
		return -1;
	}

	// Get application home directory
	char app_home[256];
	if (get_app_home_directory(app_home, sizeof(app_home)) != 0)
	{
		return -1;
	}

	// Create the device_config subdirectory
	char config_dir[300];
	snprintf(config_dir, sizeof(config_dir), "%s/device_config", app_home);
	if (ensure_directory_exists(config_dir) != 0)
	{
		return -1;
	}

	// Get current date and time
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	char date_str[20];
	strftime(date_str, sizeof(date_str), "%Y%m%d-%H%M%S", tm_info);

	// Sanitize device name for filename (replace non-alphanumeric with underscores)
	char safe_name[64];
	size_t name_len = strlen(device->name);
	size_t i;
	for (i = 0; i < name_len && i < sizeof(safe_name) - 1; i++)
	{
		char c = device->name[i];
		safe_name[i] = (isalnum(c) || c == '-') ? c : '_';
	}
	safe_name[i] = '\0';

	// Create filename
	char filename[400];
	snprintf(filename, sizeof(filename), "%s/audio-device_%s_date-time_%s.json",
			 config_dir, safe_name, date_str);

	// Open file for writing
	FILE *file = fopen(filename, "w");
	if (!file)
	{
		LOG_ERROR("Failed to open file for writing: %s", filename);
		return -1;
	}

	// Write device state as JSON
	fprintf(file, "{\n");
	fprintf(file, "  \"device\": {\n");
	fprintf(file, "    \"name\": \"%s\",\n", device->name);
	fprintf(file, "    \"id\": %d,\n", device->id);
	fprintf(file, "    \"type\": %d,\n", device->type);
	fprintf(file, "    \"parameters\": [\n");

	// Write parameters
	for (i = 0; i < device->nparams; i++)
	{
		fprintf(file, "      {\n");
		fprintf(file, "        \"index\": %d,\n", i);
		fprintf(file, "        \"id\": %d,\n", device->params[i].id);
		fprintf(file, "        \"value\": %d,\n", device->params[i].value);
		fprintf(file, "        \"min\": %d,\n", device->params[i].min);
		fprintf(file, "        \"max\": %d,\n", device->params[i].max);
		fprintf(file, "        \"name\": \"%s\"\n", device->params[i].name);
		fprintf(file, "      }%s\n", (i < device->nparams - 1) ? "," : "");
	}

	fprintf(file, "    ]\n");
	fprintf(file, "  }\n");
	fprintf(file, "}\n");

	fclose(file);
	LOG_INFO("Device configuration saved to %s", filename);

	return 0;
}
