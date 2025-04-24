/**
 * @file oscmix.c
 * @brief Main implementation file for OSCMix
 *
 * This file contains the main entry points and coordination logic for OSCMix,
 * delegating most functionality to the specialized modules.
 */

#include "oscmix.h"
#include "oscnode_tree.h"
#include "oscmix_commands.h"
#include "oscmix_midi.h"
#include "device.h"
#include "device_state.h"
#include "util.h"
#include "osc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Remaining global variables from the original oscmix.c */
static const struct device *cur_device = NULL;

/**
 * @brief Initialize the OSCMix application
 *
 * @param port The device name or identifier
 * @return 0 on success, non-zero on failure
 */
int init(const char *port)
{
	// Initialize the device
	cur_device = getDevice();
	if (!cur_device)
	{
		fprintf(stderr, "No device available\n");
		return -1;
	}

	// Initialize the node tree
	if (oscnode_tree_init() != 0)
	{
		fprintf(stderr, "Failed to initialize OSC node tree\n");
		return -1;
	}

	// Request initial device state
	// This would trigger a refresh to populate our state with current device values
	const struct oscnode *path[16];
	struct oscmsg msg = {0}; // Empty message

	// Find the refresh node in the tree
	const char *components[] = {"refresh"};
	int npath = oscnode_find(components, 1, path, 16);

	if (npath > 0)
	{
		setrefresh(path, 0x8000, &msg); // Magic register for refresh
	}

	return 0;
}

/**
 * @brief Process incoming SysEx messages from the device
 *
 * @param buf The buffer containing the SysEx message
 * @param len The length of the buffer
 * @param payload Buffer to store the decoded payload
 */
void handlesysex(const unsigned char *buf, size_t len, uint_least32_t *payload)
{
	// Check for valid SysEx message
	if (len < 5 || buf[0] != 0xF0)
	{
		fprintf(stderr, "Invalid SysEx message\n");
		return;
	}

	// Check manufacturer ID
	if (buf[1] != 0x00 || buf[2] != 0x20 || buf[3] != 0x0C)
	{
		fprintf(stderr, "Unknown manufacturer ID\n");
		return;
	}

	// Handle different message types
	switch (buf[4])
	{
	case 0x01: // Register values
		handleregs(payload, (len - 6) / 4);
		break;
	case 0x02: // Input levels
		handlelevels(1, payload, (len - 6) / 4);
		break;
	case 0x03: // Output levels
		handlelevels(2, payload, (len - 6) / 4);
		break;
	case 0x04: // Playback levels
		handlelevels(3, payload, (len - 6) / 4);
		break;
	default:
		fprintf(stderr, "Unknown message type: %02x\n", buf[4]);
	}
}

/**
 * @brief Process incoming OSC messages from the network
 *
 * @param buf The buffer containing the OSC message
 * @param len The length of the buffer
 * @return 0 on success, non-zero on failure
 */
int handleosc(const void *buf, size_t len)
{
	struct oscmsg msg;
	const struct oscnode *path[16];
	const char *components[16];
	int ncomponents, npath;
	const char *addr;

	// Parse the OSC message
	addr = oscmsg_parse(buf, len, &msg);
	if (!addr)
	{
		fprintf(stderr, "Failed to parse OSC message\n");
		return -1;
	}

	// Handle special commands
	if (strcmp(addr, "/dump") == 0)
	{
		dumpDeviceState();
		return 0;
	}
	else if (strcmp(addr, "/dump/save") == 0)
	{
		int result = dumpConfig();
		if (result == 0)
		{
			oscsend("/dump/save", ",s", "Configuration saved successfully");
		}
		else
		{
			oscsend("/dump/save", ",s", "Failed to save configuration");
		}
		return 0;
	}

	// Split the address into components
	ncomponents = 0;
	for (const char *p = addr; *p; p++)
	{
		if (*p == '/')
		{
			if (ncomponents < 16)
			{
				components[ncomponents++] = p + 1;
			}
		}
	}

	// Find the node in the tree
	npath = oscnode_find(components, ncomponents, path, 16);
	if (npath <= 0)
	{
		fprintf(stderr, "Unknown OSC address: %s\n", addr);
		return -1;
	}

	// Handle GET requests (no arguments)
	if (msg.argc == 0)
	{
		const struct oscnode *node = path[npath - 1];
		if (node->new)
		{
			// Call the appropriate new* function to send current value
			node->new(path, addr, node->reg, 0); // Placeholder value - real impl would get actual value
		}
		return 0;
	}

	// Handle SET requests (with arguments)
	const struct oscnode *node = path[npath - 1];
	if (node->set)
	{
		return node->set(path, node->reg, &msg);
	}

	return -1;
}

/**
 * @brief Handle periodic timer events
 *
 * @param levels Whether to request level updates
 */
void handletimer(bool levels)
{
	// Request level updates if needed
	if (levels)
	{
		// Request input levels
		setreg(0x9000, 1);

		// Request output levels
		setreg(0x9001, 1);

		// Request playback levels
		setreg(0x9002, 1);
	}
}
