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
#include "sysex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

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
	extern const struct device ffucxii;
	static const struct device *devices[] = {
		&ffucxii,
	};
	int i;
	size_t namelen;

	// Find matching device
	for (i = 0; i < sizeof(devices) / sizeof(devices[0]); ++i)
	{
		if (strcmp(port, devices[i]->id) == 0)
			break;
		namelen = strlen(devices[i]->name);
		if (strncmp(port, devices[i]->name, namelen) == 0)
		{
			if (!port[namelen] || (port[namelen] == ' ' && port[namelen + 1] == '('))
				break;
		}
	}

	if (i == sizeof(devices) / sizeof(devices[0]))
	{
		fprintf(stderr, "unsupported device '%s'\n", port);
		return -1;
	}

	// Initialize device state
	if (device_state_init(devices[i]) != 0)
	{
		fprintf(stderr, "Failed to initialize device state\n");
		return -1;
	}

	// Initialize the node tree
	if (oscnode_tree_init() != 0)
	{
		fprintf(stderr, "Failed to initialize OSC node tree\n");
		return -1;
	}

	// Request initial device state with a refresh command
	// This triggers a refresh to populate our state with current device values
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
	size_t payloadlen;

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

	// Decode the SysEx payload
	payloadlen = sysex_decode(buf + 5, len - 6, (unsigned char *)payload);

	if (payloadlen % 4 != 0)
	{
		fprintf(stderr, "Invalid SysEx payload length: %zu\n", payloadlen);
		return;
	}

	payloadlen /= 4;

	// Handle different message types based on the subid
	switch (buf[4])
	{
	case 0x01: // Register values
		handleregs(payload, payloadlen);
		break;
	case 0x02: // Input levels
		handlelevels(1, payload, payloadlen);
		break;
	case 0x03: // Output levels
		handlelevels(3, payload, payloadlen);
		break;
	case 0x04: // Playback levels
		handlelevels(2, payload, payloadlen);
		break;
	case 0x05: // Input FX levels
		handlelevels(4, payload, payloadlen);
		break;
	case 0x06: // Output FX levels
		handlelevels(5, payload, payloadlen);
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
	const char *addr, *next;
	const struct oscnode *path[8], *node;
	size_t pathlen;
	struct oscmsg msg;
	int reg;

	if (len % 4 != 0)
		return -1;

	msg.err = NULL;
	msg.buf = (unsigned char *)buf;
	msg.end = (unsigned char *)buf + len;
	msg.type = "ss";

	addr = oscgetstr(&msg);
	msg.type = oscgetstr(&msg);
	if (msg.err)
	{
		fprintf(stderr, "invalid osc message: %s\n", msg.err);
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
	else if (strcmp(addr, "/version") == 0)
	{
		// Send version information
		oscsend("/version", ",s", "OSCMix 1.0");
		return 0;
	}

	// Regular command processing
	msg.argc = 0;
	msg.argv = NULL;
	if (*msg.type)
	{
		// Parse arguments for this message
		const char *typestr = msg.type;
		int count = strlen(typestr);

		// Allocate argument array
		msg.argv = malloc(count * sizeof(struct oscarg));
		if (!msg.argv)
		{
			fprintf(stderr, "memory allocation failed\n");
			return -1;
		}

		// Parse arguments based on type tags
		for (int i = 0; i < count; i++)
		{
			msg.argv[msg.argc].type = *typestr++;

			switch (msg.argv[msg.argc].type)
			{
			case 'i':
				msg.argv[msg.argc].i = oscgetint(&msg);
				break;
			case 'f':
				msg.argv[msg.argc].f = oscgetfloat(&msg);
				break;
			case 's':
				msg.argv[msg.argc].s = oscgetstr(&msg);
				break;
			case 'T':
				break;
			case 'F':
				break;
			default:
				fprintf(stderr, "unsupported argument type: %c\n", msg.argv[msg.argc].type);
				free(msg.argv);
				return -1;
			}

			msg.argc++;
		}
	}

	// Find the node in the tree
	reg = 0;
	pathlen = 0;
	node = &tree[0];

	while (node)
	{
		if (*addr == '/' || *addr == '\0')
		{
			addr++;
			for (node = node->child; node && node->name; node++)
			{
				next = match(node->name, addr);
				if (next)
				{
					addr = next;
					path[pathlen++] = node;
					reg += node->reg;

					if (*addr == '\0')
					{
						// We found the target node
						if (msg.argc == 0 && node->new)
						{
							// This is a GET request
							node->new(path, addr, reg, 0);
						}
						else if (node->set)
						{
							// This is a SET request
							node->set(path, reg, &msg);
						}
						else
						{
							fprintf(stderr, "no handler for node: %s\n", addr);
						}

						if (msg.argv)
							free(msg.argv);
						return 0;
					}

					if (node->child)
						break;
				}
			}

			if (!node || !node->name)
			{
				fprintf(stderr, "unknown osc address: %s\n", addr);
				if (msg.argv)
					free(msg.argv);
				return -1;
			}
		}
		else
		{
			addr++;
		}
	}

	if (msg.argv)
		free(msg.argv);
	return 0;
}

/**
 * @brief Handle periodic timer events
 *
 * @param levels Whether to request level updates
 */
void handletimer(bool levels)
{
	// If refreshing in progress, don't request new updates
	if (refreshing_state(-1))
		return;

	// Request level updates if needed
	if (levels)
	{
		// Request input levels
		setreg(0x9000, 1);

		// Request output levels
		setreg(0x9001, 1);

		// Request playback levels
		setreg(0x9002, 1);

		// If the device supports input/output FX levels, request those too
		const struct device *device = getDevice();
		if (device->flags & DEVICE_FX_LEVELS)
		{
			// Request input FX levels
			setreg(0x9003, 1);

			// Request output FX levels
			setreg(0x9004, 1);
		}

		// If the device supports DURec, request status updates
		if (device->flags & DEVICE_DUREC)
		{
			// Request DURec status
			setreg(0x9010, 1);

			// Request DURec time
			setreg(0x9011, 1);

			// Request DURec USB status
			setreg(0x9012, 1);
		}
	}

	// Flush any pending OSC messages
	oscflush();
}
