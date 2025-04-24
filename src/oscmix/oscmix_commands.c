/**
 * @file oscmix_commands.c
 * @brief Command handlers for OSC messages
 */

#include "oscmix_commands.h"
#include "oscnode_tree.h"
#include "oscmix_midi.h"
#include "device.h"
#include "device_state.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>

/* Helper functions for OSC argument handling */

/**
 * @brief Get an integer from an OSC argument
 *
 * @param arg The OSC argument
 * @param out_value Pointer to store the integer value
 * @return 0 on success, -1 on type mismatch
 */
static int osc_get_int(const struct oscarg *arg, int *out_value)
{
    if (!arg || !out_value)
        return -1;

    switch (arg->type)
    {
    case 'i':
        *out_value = arg->i;
        break;
    case 'f':
        *out_value = (int)arg->f;
        break;
    case 'T':
        *out_value = 1;
        break;
    case 'F':
    case 'N':
        *out_value = 0;
        break;
    default:
        return -1;
    }

    return 0;
}

/**
 * @brief Get a float from an OSC argument
 *
 * @param arg The OSC argument
 * @param out_value Pointer to store the float value
 * @return 0 on success, -1 on type mismatch
 */
static int osc_get_float(const struct oscarg *arg, float *out_value)
{
    if (!arg || !out_value)
        return -1;

    switch (arg->type)
    {
    case 'i':
        *out_value = (float)arg->i;
        break;
    case 'f':
        *out_value = arg->f;
        break;
    case 'T':
        *out_value = 1.0f;
        break;
    case 'F':
    case 'N':
        *out_value = 0.0f;
        break;
    default:
        return -1;
    }

    return 0;
}

/**
 * @brief Get a boolean from an OSC argument
 *
 * @param arg The OSC argument
 * @param out_value Pointer to store the boolean value
 * @return 0 on success, -1 on type mismatch
 */
static int osc_get_bool(const struct oscarg *arg, bool *out_value)
{
    if (!arg || !out_value)
        return -1;

    switch (arg->type)
    {
    case 'i':
        *out_value = (arg->i != 0);
        break;
    case 'f':
        *out_value = (arg->f != 0.0f);
        break;
    case 'T':
        *out_value = true;
        break;
    case 'F':
        *out_value = false;
        break;
    case 'N':
        *out_value = false;
        break;
    default:
        return -1;
    }

    return 0;
}

/**
 * @brief Get a string from an OSC argument
 *
 * @param arg The OSC argument
 * @param out_value Pointer to store the string pointer
 * @return 0 on success, -1 on type mismatch
 */
static int osc_get_string(const struct oscarg *arg, const char **out_value)
{
    if (!arg || !out_value)
        return -1;

    if (arg->type == 's')
    {
        *out_value = arg->s;
        return 0;
    }

    return -1;
}

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

/**
 * @brief Set the name of an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the name string
 * @return 0 on success, non-zero on failure
 */
int setinputname(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    const char *name;
    char namebuf[12];
    int i, ch, val;

    ch = path[-1] - path[-2]->child;
    if (ch >= 20)
        return -1;

    name = msg->argv[0].s;
    if (msg->argc != 1 || msg->argv[0].type != 's')
        return -1;

    strncpy(namebuf, name, sizeof(namebuf));
    namebuf[sizeof(namebuf) - 1] = '\0';

    reg = 0x3200 + ch * 8;
    for (i = 0; i < sizeof(namebuf); i += 2, ++reg)
    {
        // Pack two bytes of name into one register
        val = (namebuf[i] & 0xFF) | ((namebuf[i + 1] & 0xFF) << 8);
        setreg(reg, val);
    }

    return 0;
}

/**
 * @brief Set the gain for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the gain value
 * @return 0 on success, non-zero on failure
 */
int setinputgain(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    float val;
    bool mic;

    if (msg->argc != 1 || (msg->argv[0].type != 'f' && msg->argv[0].type != 'i'))
        return -1;

    if (msg->argv[0].type == 'f')
        val = msg->argv[0].f;
    else
        val = (float)msg->argv[0].i;

    mic = (path[-1] - path[-2]->child) <= 1;
    if (val < 0 || val > 75 || (!mic && val > 24))
        return -1;

    setreg(reg, val * 10);
    return 0;
}

/**
 * @brief Set the 48V phantom power state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the state value
 * @return 0 on success, non-zero on failure
 */
int setinput48v(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int idx;
    const struct device *dev = getDevice();

    idx = path[-1] - path[-2]->child;
    assert(idx < dev->inputslen);

    if (dev->inputs[idx].flags & INPUT_48V)
        return setbool(path, reg, msg);

    return -1;
}

/**
 * @brief Set the Hi-Z state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the state value
 * @return 0 on success, non-zero on failure
 */
int setinputhiz(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int idx;
    const struct device *dev = getDevice();

    idx = path[-1] - path[-2]->child;
    assert(idx < dev->inputslen);

    if (dev->inputs[idx].flags & INPUT_HIZ)
        return setbool(path, reg, msg);

    return -1;
}

/**
 * @brief Set the stereo status for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the stereo state
 * @return 0 on success, non-zero on failure
 */
int setinputstereo(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int idx;
    bool val;

    if (msg->argc != 1 || (msg->argv[0].type != 'i' &&
                           msg->argv[0].type != 'f' &&
                           msg->argv[0].type != 'T' &&
                           msg->argv[0].type != 'F'))
        return -1;

    // Convert to boolean
    if (msg->argv[0].type == 'i')
        val = msg->argv[0].i != 0;
    else if (msg->argv[0].type == 'f')
        val = msg->argv[0].f != 0.0f;
    else if (msg->argv[0].type == 'T')
        val = 1;
    else
        val = 0;

    idx = (path[-1] - path[-2]->child) & -2;

    // Set stereo state for both channels of the stereo pair
    setreg(idx << 6 | 2, val);
    setreg((idx + 1) << 6 | 2, val);

    return 0;
}

/**
 * @brief Set the mute state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the mute state
 * @return 0 on success, non-zero on failure
 */
int setinputmute(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    bool val;

    if (msg->argc != 1 || (msg->argv[0].type != 'i' &&
                           msg->argv[0].type != 'f' &&
                           msg->argv[0].type != 'T' &&
                           msg->argv[0].type != 'F'))
        return -1;

    // Convert to boolean
    if (msg->argv[0].type == 'i')
        val = msg->argv[0].i != 0;
    else if (msg->argv[0].type == 'f')
        val = msg->argv[0].f != 0.0f;
    else if (msg->argv[0].type == 'T')
        val = 1;
    else
        val = 0;

    // Set mute state
    setreg(reg, val);

    return 0;
}

/**
 * @brief Set the loopback state for an output channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the loopback state
 * @return 0 on success, non-zero on failure
 */
int setoutputloopback(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    bool val;
    unsigned char buf[4];
    int idx;

    if (msg->argc != 1 || (msg->argv[0].type != 'i' &&
                           msg->argv[0].type != 'f' &&
                           msg->argv[0].type != 'T' &&
                           msg->argv[0].type != 'F'))
        return -1;

    // Convert to boolean
    if (msg->argv[0].type == 'i')
        val = msg->argv[0].i != 0;
    else if (msg->argv[0].type == 'f')
        val = msg->argv[0].f != 0.0f;
    else if (msg->argv[0].type == 'T')
        val = 1;
    else
        val = 0;

    idx = path[-1] - path[-2]->child;
    if (val)
        idx |= 0x80;

    // Use special format for loopback command
    // This will be sent via writesysex in the MIDI module
    reg = 0x80 | idx;
    setreg(reg, 0);

    return 0;
}

/**
 * @brief Set EQD record state
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the EQD state
 * @return 0 on success, non-zero on failure
 */
int seteqdrecord(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    bool val;

    if (msg->argc != 1 || (msg->argv[0].type != 'i' &&
                           msg->argv[0].type != 'f' &&
                           msg->argv[0].type != 'T' &&
                           msg->argv[0].type != 'F'))
        return -1;

    // Convert to boolean
    if (msg->argv[0].type == 'i')
        val = msg->argv[0].i != 0;
    else if (msg->argv[0].type == 'f')
        val = msg->argv[0].f != 0.0f;
    else if (msg->argv[0].type == 'T')
        val = 1;
    else
        val = 0;

    // This will be sent via a special SysEx command
    reg = 0x90 | (val ? 1 : 0);
    setreg(reg, 0);

    return 0;
}

/**
 * @brief Set the mix parameters for a channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the mix parameters
 * @return 0 on success, non-zero on failure
 */
int setmix(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    float vol, width = 1.0f;
    int pan = 0;

    // First argument must be volume (dB)
    if (msg->argc < 1 || (msg->argv[0].type != 'f' && msg->argv[0].type != 'i'))
        return -1;

    if (msg->argv[0].type == 'f')
        vol = msg->argv[0].f;
    else
        vol = (float)msg->argv[0].i;

    // If volume is <= -65dB, treat as -infinity
    if (vol <= -65)
        vol = -INFINITY;

    // Optional second argument is pan (-100..100)
    if (msg->argc > 1 && (msg->argv[1].type == 'i' || msg->argv[1].type == 'f'))
    {
        if (msg->argv[1].type == 'i')
            pan = msg->argv[1].i;
        else
            pan = (int)msg->argv[1].f;

        // Clamp pan value to valid range
        if (pan < -100)
            pan = -100;
        if (pan > 100)
            pan = 100;
    }

    // Optional third argument is width (0..2)
    if (msg->argc > 2 && (msg->argv[2].type == 'f' || msg->argv[2].type == 'i'))
    {
        if (msg->argv[2].type == 'f')
            width = msg->argv[2].f;
        else
            width = (float)msg->argv[2].i;

        // Clamp width value to valid range
        if (width < 0)
            width = 0;
        if (width > 2)
            width = 2;
    }

    // Calculate and set the mix values in the device
    // Convert from dB to linear
    float level = vol > -INFINITY ? powf(10.0f, vol / 20.0f) : 0.0f;

    // Calculate left-right balance based on pan and width
    float left, right;
    if (pan <= 0)
    {
        // Pan center to left
        left = level;
        right = level * (1.0f + pan / 100.0f);
    }
    else
    {
        // Pan center to right
        left = level * (1.0f - pan / 100.0f);
        right = level;
    }

    // Apply width (only meaningful for stereo channels)
    if (width != 1.0f)
    {
        // Center content is preserved, sides are adjusted
        float mono = (left + right) / 2.0f;
        float sides = (left - right) / 2.0f;
        sides *= width;
        left = mono + sides;
        right = mono - sides;
    }

    // Convert to device values and send to the mixer
    int db_val = vol > -INFINITY ? (int)(vol * 10.0f) : -650;
    int pan_val = pan;

    // First register: volume/level as fixed point
    setreg(reg, db_val);

    // Second register: pan as signed integer with flag bit
    setreg(reg + 1, pan_val | 0x8000); // Set high bit to indicate pan value

    return 0;
}

/**
 * @brief Command to handle durec stop
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecstop(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    (void)path; // Unused
    (void)msg;  // Unused

    setreg(0x3e9a, 0x8120);
    return 0;
}

/**
 * @brief Command to handle durec play
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecplay(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    (void)path; // Unused
    (void)msg;  // Unused

    setreg(0x3e9a, 0x8123);
    return 0;
}

/**
 * @brief Command to handle durec record
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecrecord(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    (void)path; // Unused
    (void)msg;  // Unused

    setreg(0x3e9a, 0x8122);
    return 0;
}

/**
 * @brief Command to handle durec delete
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecdelete(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int val;

    if (msg->argc != 1 || msg->argv[0].type != 'i')
        return -1;

    val = msg->argv[0].i;
    setreg(0x3e9b, 0x8000 | val);
    return 0;
}

/**
 * @brief Command to handle durec file selection
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecfile(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int val;

    if (msg->argc != 1 || msg->argv[0].type != 'i')
        return -1;

    val = msg->argv[0].i;
    setreg(0x3e9c, val | 0x8000);
    return 0;
}

/**
 * @brief Set the stereo status for an output channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the stereo state
 * @return 0 on success, non-zero on failure
 */
int setoutputstereo(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int idx;
    bool val;

    if (msg->argc != 1 || (msg->argv[0].type != 'i' &&
                           msg->argv[0].type != 'f' &&
                           msg->argv[0].type != 'T' &&
                           msg->argv[0].type != 'F'))
        return -1;

    // Convert to boolean
    if (msg->argv[0].type == 'i')
        val = msg->argv[0].i != 0;
    else if (msg->argv[0].type == 'f')
        val = msg->argv[0].f != 0.0f;
    else if (msg->argv[0].type == 'T')
        val = 1;
    else
        val = 0;

    idx = (path[-1] - path[-2]->child) & -2;

    // Set stereo state for both channels of the stereo pair
    setreg((idx + 0x40) << 6 | 2, val);
    setreg((idx + 0x41) << 6 | 2, val);

    return 0;
}

/**
 * @brief Set the playback mode for DURec
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecplaymode(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    static const char *const names[] = {"Single", "Repeat", "Sequence", "Random"};
    int val, i;
    const char *str;

    if (msg->argc != 1)
        return -1;

    // Process string input
    if (msg->argv[0].type == 's')
    {
        str = msg->argv[0].s;
        for (val = 0; val < sizeof(names) / sizeof(names[0]); val++)
        {
            if (strcasecmp(str, names[val]) == 0)
            {
                setreg(0x3e9e, val);
                return 0;
            }
        }
        return -1;
    }
    // Process numeric input
    else if (msg->argv[0].type == 'i')
    {
        val = msg->argv[0].i;
        if (val >= 0 && val < sizeof(names) / sizeof(names[0]))
        {
            setreg(0x3e9e, val);
            return 0;
        }
        return -1;
    }

    return -1;
}

/**
 * @brief Set the next file for DURec to play
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message
 * @return 0 on success, non-zero on failure
 */
int setdurecnext(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    int val;

    if (msg->argc != 1 || msg->argv[0].type != 'i')
        return -1;

    val = msg->argv[0].i;

    // The format for next file is different from the normal file selection
    // We use a special register to set the next file
    setreg(0x3e9d, val | 0x8000);

    return 0;
}

/**
 * @brief Set the name of an output channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the name string
 * @return 0 on success, non-zero on failure
 */
int setoutputname(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
    const char *name;
    char namebuf[12];
    int i, ch, val;

    ch = path[-1] - path[-2]->child;
    if (ch >= 20)
        return -1;

    name = msg->argv[0].s;
    if (msg->argc != 1 || msg->argv[0].type != 's')
        return -1;

    strncpy(namebuf, name, sizeof(namebuf));
    namebuf[sizeof(namebuf) - 1] = '\0';

    // Output names use a different register range than inputs
    reg = 0x3400 + ch * 8;
    for (i = 0; i < sizeof(namebuf); i += 2, ++reg)
    {
        // Pack two bytes of name into one register
        val = (namebuf[i] & 0xFF) | ((namebuf[i + 1] & 0xFF) << 8);
        setreg(reg, val);
    }

    return 0;
}
