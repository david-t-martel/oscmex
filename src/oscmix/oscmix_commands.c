/**
 * @file oscmix_commands.c
 * @brief Implementation of command handlers for OSC messages
 */

#include "oscmix_commands.h"
#include "oscnode_tree.h"
#include "oscmix_midi.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * @brief Set an integer parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setint(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int val = 0;
    int i, min, max;

    // Extract the parameter value from the OSC message
    if (msg->argc != 1 || (msg->argv[0].type != 'i' && msg->argv[0].type != 'f'))
        return -1;

    if (msg->argv[0].type == 'i')
        val = msg->argv[0].i;
    else
        val = (int)msg->argv[0].f;

    // Find the min/max range for this parameter
    for (i = 0; path[i]; i++)
    {
        if (path[i]->reg == reg && path[i]->set == setint)
        {
            min = path[i]->min;
            max = path[i]->max;

            // Clamp the value to the valid range
            if (val < min)
                val = min;
            if (val > max)
                val = max;

            // Send the register update to the device
            setreg(reg, val);
            return 0;
        }
    }

    return -1;
}

/**
 * @brief Set a fixed-point parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setfixed(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    float f;
    int val, i, min, max;
    float scale;

    // Extract the parameter value from the OSC message
    if (msg->argc != 1 || (msg->argv[0].type != 'f' && msg->argv[0].type != 'i'))
        return -1;

    if (msg->argv[0].type == 'f')
        f = msg->argv[0].f;
    else
        f = (float)msg->argv[0].i;

    // Find the scale and range for this parameter
    for (i = 0; path[i]; i++)
    {
        if (path[i]->reg == reg && path[i]->set == setfixed)
        {
            min = path[i]->min;
            max = path[i]->max;
            scale = path[i]->scale;

            // Convert float to fixed-point integer
            val = roundf(f * scale);

            // Clamp to valid range
            if (val < min)
                val = min;
            if (val > max)
                val = max;

            // Send the register update to the device
            setreg(reg, val);
            return 0;
        }
    }

    return -1;
}

/**
 * @brief Set an enumerated parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setenum(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int val, i;
    const char *str;

    // Extract the parameter value from the OSC message
    if (msg->argc != 1)
        return -1;

    // Handle both string and numeric enum values
    if (msg->argv[0].type == 's')
    {
        str = msg->argv[0].s;

        // Find the enum index matching the string
        for (i = 0; path[i]; i++)
        {
            if (path[i]->reg == reg && path[i]->set == setenum)
            {
                for (val = 0; val < path[i]->nameslen; val++)
                {
                    if (strcmp(str, path[i]->names[val]) == 0)
                    {
                        setreg(reg, val);
                        return 0;
                    }
                }
                // String not found in enum
                return -1;
            }
        }
    }
    else if (msg->argv[0].type == 'i')
    {
        val = msg->argv[0].i;

        // Find the valid range for this enum
        for (i = 0; path[i]; i++)
        {
            if (path[i]->reg == reg && path[i]->set == setenum)
            {
                if (val >= 0 && val < path[i]->nameslen)
                {
                    setreg(reg, val);
                    return 0;
                }
                // Value out of range
                return -1;
            }
        }
    }
    else
    {
        // Invalid type
        return -1;
    }

    return -1;
}

/**
 * @brief Set a boolean parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
int setbool(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int val;

    // Extract the parameter value from the OSC message
    if (msg->argc != 1 || (msg->argv[0].type != 'i' &&
                           msg->argv[0].type != 'f' &&
                           msg->argv[0].type != 'T' &&
                           msg->argv[0].type != 'F'))
        return -1;

    // Convert various types to boolean
    if (msg->argv[0].type == 'i')
        val = msg->argv[0].i != 0;
    else if (msg->argv[0].type == 'f')
        val = msg->argv[0].f != 0.0f;
    else if (msg->argv[0].type == 'T')
        val = 1;
    else if (msg->argv[0].type == 'F')
        val = 0;
    else
        return -1;

    // Send the register update to the device
    setreg(reg, val);
    return 0;
}

/**
 * @brief Trigger a refresh of the device state
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing any parameters
 * @return 0 on success, non-zero on failure
 */
int setrefresh(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    (void)path; // Unused
    (void)msg;  // Unused

    // Magic value to trigger a device refresh
    setreg(reg, 0xFFFFFFFF);

    return 0;
}
