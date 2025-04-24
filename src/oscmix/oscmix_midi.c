/**
 * @file oscmix_midi.c
 * @brief Implementation of MIDI SysEx handling functions
 */

#include "oscmix_midi.h"
#include "oscnode_tree.h"
#include "device.h"
#include "device_state.h"
#include "oscmix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>

/* Global variables related to MIDI that were in oscmix.c */
static bool refreshing = false;
static char oscbuf[1024];

/* External references to functions defined in main.c */
extern void writemidi(const void *buf, size_t len);
extern void writeosc(const void *buf, size_t len);

/**
 * @brief Sets a register value in the device
 *
 * @param reg The register address
 * @param val The value to set
 * @return 0 on success, non-zero on failure
 */
int setreg(unsigned reg, unsigned val)
{
    unsigned char buf[7];

    // Format the SysEx message
    buf[0] = 0xF0;              // Start of SysEx
    buf[1] = 0x00;              // Manufacturer ID (part 1)
    buf[2] = 0x20;              // Manufacturer ID (part 2)
    buf[3] = 0x0C;              // Manufacturer ID (part 3)
    buf[4] = 0x01;              // Command: set register
    buf[5] = (reg >> 8) & 0x7F; // High byte of register address
    buf[6] = reg & 0x7F;        // Low byte of register address

    // Add the value bytes (up to 4 bytes, 7 bits each)
    int i;
    for (i = 0; i < 4; i++)
    {
        buf[7 + i] = (val >> (i * 7)) & 0x7F;
    }

    buf[11] = 0xF7; // End of SysEx

    // Send the SysEx message to the device
    writemidi(buf, 12);

    // Special handling for refresh command
    if (reg == 0x8000 && val == 0xFFFFFFFF)
    {
        refreshing = true;
    }

    return 0;
}

/**
 * @brief Handles register values received from the device
 *
 * @param payload The buffer containing the register values
 * @param len The length of the buffer in words
 */
void handleregs(uint_least32_t *payload, size_t len)
{
    if (len < 2)
    {
        fprintf(stderr, "Invalid register payload length: %zu\n", len);
        return;
    }

    // The first word is the register address
    unsigned reg = payload[0];

    // The second word is the register value
    unsigned val = payload[1];

    // Update device state based on the register
    // This is a simplified example - actual implementation would be more complex
    fprintf(stderr, "Received register 0x%04x = 0x%08x\n", reg, val);

    // We would look up the OSC node for this register and call the appropriate
    // new* function to notify clients of the change
    // This would involve finding all parameters that map to this register
    const struct oscnode *path[16];
    char addr[256];

    // This is a placeholder for the actual implementation
    // which would iterate through the tree to find parameters using this register

    // If we're in a refresh cycle, continue sending register read commands
    if (refreshing)
    {
        // Request the next register
        // This would be implemented based on a list of registers to read
    }
}

/**
 * @brief Handles audio level values received from the device
 *
 * @param subid The sub ID of the SysEx message
 * @param payload The buffer containing the audio level values
 * @param len The length of the buffer
 */
void handlelevels(int subid, uint_least32_t *payload, size_t len)
{
    char addr[128];
    int i;

    // Determine the type of levels based on subid
    const char *type = NULL;
    switch (subid)
    {
    case 0x01:
        type = "input";
        break;
    case 0x02:
        type = "output";
        break;
    case 0x03:
        type = "playback";
        break;
    default:
        fprintf(stderr, "Unknown level type: %d\n", subid);
        return;
    }

    // Send level values as OSC messages
    for (i = 0; i < len; i++)
    {
        snprintf(addr, sizeof(addr), "/level/%s/%d", type, i);

        // Convert the raw level value to dB
        float db = -90.0f; // Default to -inf
        if (payload[i] > 0)
        {
            // Example conversion formula - actual implementation may vary
            db = 20 * log10f((float)payload[i] / 32767.0f);
        }

        oscsend(addr, ",f", db);
    }
}

/**
 * @brief Sends an OSC message with variable arguments
 *
 * @param addr The OSC address
 * @param type The OSC type tags
 * @param ... The OSC arguments
 */
void oscsend(const char *addr, const char *type, ...)
{
    va_list ap;
    size_t len;

    // Start variable argument processing
    va_start(ap, type);

    // Format the OSC message into oscbuf
    // This is simplified - actual implementation would need to properly
    // format the OSC message according to the OSC specification
    len = strlen(addr) + 1;
    strcpy(oscbuf, addr);

    // Add type tag string
    oscbuf[len++] = ',';
    strcpy(oscbuf + len, type);
    len += strlen(type) + 1;

    // Add arguments based on type tags
    for (const char *t = type; *t; t++)
    {
        switch (*t)
        {
        case 'i':
        {
            int i = va_arg(ap, int);
            memcpy(oscbuf + len, &i, sizeof(i));
            len += sizeof(i);
        }
        break;
        case 'f':
        {
            float f = (float)va_arg(ap, double);
            memcpy(oscbuf + len, &f, sizeof(f));
            len += sizeof(f);
        }
        break;
        case 's':
        {
            const char *s = va_arg(ap, const char *);
            size_t slen = strlen(s) + 1;
            strcpy(oscbuf + len, s);
            len += slen;
            // Pad to multiple of 4
            while (len % 4)
                oscbuf[len++] = '\0';
        }
        break;
            // Other OSC types could be added here
        }
    }

    // End variable argument processing
    va_end(ap);

    // Send the OSC message
    writeosc(oscbuf, len);
}

/* Add implementations for other functions (oscsendenum, newint, newfixed, newenum, newbool) */
