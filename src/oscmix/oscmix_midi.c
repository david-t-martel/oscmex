/**
 * @file oscmix_midi.c
 * @brief MIDI SysEx handling and OSC response functions
 */

#include "oscmix_midi.h"
#include "oscnode_tree.h"
#include "device.h"
#include "device_state.h"
#include "oscmix.h"
#include "sysex.h"
#include "osc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>

/* Global variables related to MIDI communication */
static char oscbuf[8192];
static struct oscmsg oscmsg;

/* External references to functions defined in main.c */
extern void writemidi(const void *buf, size_t len);
extern void writeosc(const void *buf, size_t len);

/**
 * @brief Writes a SysEx message to the device
 *
 * @param subid The sub ID of the SysEx message
 * @param buf The buffer containing the SysEx message data
 * @param len The length of the buffer
 * @param sysexbuf The buffer to store the encoded SysEx message
 */
void writesysex(int subid, const unsigned char *buf, size_t len, unsigned char *sysexbuf)
{
    size_t sysexlen;

    sysexbuf[0] = 0xF0;
    sysexbuf[1] = 0x00;
    sysexbuf[2] = 0x20;
    sysexbuf[3] = 0x0C;
    sysexbuf[4] = subid;

    sysexlen = sysex_encode(buf, len, sysexbuf + 5);
    sysexbuf[sysexlen + 5] = 0xF7;

    writemidi(sysexbuf, sysexlen + 6);
}

/**
 * @brief Sets a register value in the device
 *
 * @param reg The register address
 * @param val The value to set
 * @return 0 on success, non-zero on failure
 */
int setreg(unsigned reg, unsigned val)
{
    static unsigned char buf[8];
    static unsigned char sysexbuf[16];

#ifdef DEBUG
    fprintf(stderr, "Setting register 0x%04x = 0x%04x\n", reg, val);
#endif

    // Handle special case commands (loopback, EQD, etc.)
    if (reg & 0x80)
    {
        buf[0] = reg;
        writesysex(0xf, buf, 1, sysexbuf);
        return 0;
    }

    // Encode register address and value
    buf[0] = reg >> 8;
    buf[1] = reg;
    buf[2] = val >> 8;
    buf[3] = val;

    // Send to device
    writesysex(1, buf, 4, sysexbuf);
    return 0;
}

/**
 * @brief Sets an audio level parameter value in the device
 *
 * @param reg The register address
 * @param level The audio level value (0.0 to 1.0)
 * @return 0 on success, non-zero on failure
 */
int setlevel(int reg, float level)
{
    int val;

    // Clamp level to valid range
    if (level < 0.0f)
        level = 0.0f;
    if (level > 1.0f)
        level = 1.0f;

    // Convert to integer and set register
    val = (int)(level * 65535.0f);
    return setreg(reg, val);
}

/**
 * @brief Sets a dB value in the device
 *
 * @param reg The register address
 * @param db The dB value
 * @return 0 on success, non-zero on failure
 */
int setdb(int reg, float db)
{
    int val;

    // If negative infinity, set to minimum value
    if (db <= -65.0f)
        val = -650;
    else
        val = (int)(db * 10.0f);

    return setreg(reg, val);
}

/**
 * @brief Sets a pan value in the device
 *
 * @param reg The register address
 * @param pan The pan value (-100 to 100)
 * @return 0 on success, non-zero on failure
 */
int setpan(int reg, int pan)
{
    // Clamp pan to valid range
    if (pan < -100)
        pan = -100;
    if (pan > 100)
        pan = 100;

    // Set the high bit to indicate this is a pan value
    return setreg(reg, pan | 0x8000);
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
    unsigned char *len;
    va_list ap;

    assert(addr[0] == '/');
    assert(type[0] == ',');

    if (!oscmsg.buf)
    {
        oscmsg.buf = (unsigned char *)oscbuf;
        oscmsg.end = (unsigned char *)oscbuf + sizeof oscbuf;
        oscmsg.type = NULL;
        oscputstr(&oscmsg, "#bundle");
        oscputint(&oscmsg, 0);
        oscputint(&oscmsg, 1);
    }

    len = oscmsg.buf;
    oscmsg.type = NULL;
    oscputint(&oscmsg, 0);
    oscputstr(&oscmsg, addr);
    oscputstr(&oscmsg, type);
    oscmsg.type = type + 1;

    va_start(ap, type);
    for (const char *t = type + 1; *t; t++)
    {
        switch (*t)
        {
        case 'f':
            oscputfloat(&oscmsg, va_arg(ap, double)); // Note: float is promoted to double in var args
            break;
        case 'i':
            oscputint(&oscmsg, va_arg(ap, int));
            break;
        case 's':
            oscputstr(&oscmsg, va_arg(ap, const char *));
            break;
        case 'T':
        case 'F':
            // No argument for boolean tags
            break;
        default:
            fprintf(stderr, "unsupported osc type: %c\n", *t);
        }
    }
    va_end(ap);
    putbe32(len, oscmsg.buf - len - 4);
}

/**
 * @brief Sends an enumerated value as an OSC message
 *
 * @param addr The OSC address
 * @param val The enumerated value
 * @param names The array of enumeration names
 * @param nameslen The length of the array
 */
void oscsendenum(const char *addr, int val, const char *const names[], size_t nameslen)
{
    if (val >= 0 && val < nameslen)
    {
        oscsend(addr, ",s", names[val]);
        oscsend(addr, ",i", val);
    }
    else
    {
        oscsend(addr, ",s", "unknown");
        oscsend(addr, ",i", val);
    }
}

/**
 * @brief Flushes the OSC message buffer
 */
void oscflush(void)
{
    if (oscmsg.buf)
    {
        size_t len = oscmsg.buf - (unsigned char *)oscbuf;
        if (len > 4)
        {
            writeosc(oscbuf, len);
        }
        oscmsg.buf = NULL;
    }
}

/**
 * @brief Gets the sample rate corresponding to a value
 *
 * @param val The value representing the sample rate
 * @return The sample rate in Hz
 */
long getsamplerate(int val)
{
    static const long samplerates[] = {
        44100, 48000, 88200, 96000, 176400, 192000,
        352800, 384000, 705600, 768000};

    return (val >= 0 && val < sizeof(samplerates) / sizeof(samplerates[0]))
               ? samplerates[val]
               : 0;
}

/**
 * @brief Handles register values received from the device
 *
 * @param payload The buffer containing the register values
 * @param len The length of the buffer in 32-bit words
 */
void handleregs(uint_least32_t *payload, size_t len)
{
    size_t i;
    unsigned reg, val;
    const struct oscnode *path[16];
    char addr[256];
    bool isRefreshing = refreshing_state(-1);

    for (i = 0; i < len; i++)
    {
        reg = payload[i] >> 16 & 0x7fff;
        val = payload[i] & 0xffff;

#ifdef DEBUG
        fprintf(stderr, "Received register 0x%04x = 0x%04x\n", reg, val);
#endif

        // Lookup the node in the tree and notify clients of changes
        const char *components[8];
        int comp_count = 0;

        // Find the register in the tree - this is a simplified approach
        // In a real implementation, we'd have a reverse mapping from reg to path
        switch (reg)
        {
        case 0x8000: // Sample rate
            components[0] = "system";
            components[1] = "samplerate";
            comp_count = 2;
            break;

        case 0x3e04: // DSP load
            components[0] = "hardware";
            components[1] = "dspload";
            comp_count = 2;
            break;

        case 0x3e90: // DURec status
            components[0] = "durec";
            components[1] = "status";
            comp_count = 2;
            break;

        case 0x3e91: // DURec time
            components[0] = "durec";
            components[1] = "time";
            comp_count = 2;
            break;

        default:
            // Try to find by other means
            comp_count = 0;

            // Check for input register range
            if (reg >= 0x0000 && reg < 0x0800)
            {
                int ch = reg >> 6;
                int subidx = reg & 0x3F;

                if (ch < 64)
                { // Input channel
                    components[0] = "input";
                    char chnum[8];
                    sprintf(chnum, "%d", ch + 1);
                    components[1] = strdup(chnum);

                    switch (subidx)
                    {
                    case 0: // Volume
                        components[2] = "volume";
                        comp_count = 3;
                        break;

                    case 1: // Pan
                        components[2] = "pan";
                        comp_count = 3;
                        break;

                    case 2: // Stereo
                        components[2] = "stereo";
                        comp_count = 3;
                        break;

                    case 3: // Mute
                        components[2] = "mute";
                        comp_count = 3;
                        break;

                    case 4: // 48V/Hi-Z/Ref Level
                        if (ch < 2)
                        {
                            components[2] = "48v";
                            comp_count = 3;
                        }
                        else if (ch < 4)
                        {
                            components[2] = "hiz";
                            comp_count = 3;
                        }
                        else
                        {
                            components[2] = "reflevel";
                            comp_count = 3;
                        }
                        break;
                    }
                }
            }

            // Check for output register range
            else if (reg >= 0x1000 && reg < 0x1800)
            {
                int ch = (reg >> 6) & 0x3F;
                int subidx = reg & 0x3F;

                components[0] = "output";
                char chnum[8];
                sprintf(chnum, "%d", ch + 1);
                components[1] = strdup(chnum);

                switch (subidx)
                {
                case 0: // Volume
                    components[2] = "volume";
                    comp_count = 3;
                    break;

                case 1: // Pan
                    components[2] = "pan";
                    comp_count = 3;
                    break;

                case 2: // Stereo
                    components[2] = "stereo";
                    comp_count = 3;
                    break;

                case 3: // Mute
                    components[2] = "mute";
                    comp_count = 3;
                    break;
                }
            }

            // Check for mixer register range
            else if (reg >= 0x2000 && reg < 0x3000)
            {
                int outch = (reg >> 6) & 0x3F;
                int inch = reg & 0x3F;

                components[0] = "mixer";
                char outnum[8], innum[8];
                sprintf(outnum, "%d", outch + 1);
                sprintf(innum, "%d", inch + 1);
                components[1] = strdup(outnum);
                components[2] = strdup(innum);

                // Check for volume or pan
                if (val & 0x8000)
                {
                    components[3] = "pan";
                }
                else
                {
                    components[3] = "volume";
                }
                comp_count = 4;
            }
            break;
        }

        if (comp_count > 0)
        {
            int npath = oscnode_find(components, comp_count, path, 16);
            if (npath > 0 && path[npath - 1]->new)
            {
                // Format the full address
                addr[0] = '\0';
                for (int j = 0; j < comp_count; j++)
                {
                    strcat(addr, "/");
                    strcat(addr, components[j]);
                }

                // Handle signed values
                int sval = ((int)val ^ 0x8000) - 0x8000;

                // Call the node's update function
                path[npath - 1]->new(path, addr, reg, sval);
            }

            // Free any dynamically allocated component strings
            for (int j = 1; j < comp_count; j++)
            {
                if (j == 1 || j == 2)
                {
                    char test[8];
                    sprintf(test, "%d", j);
                    if (components[j] && strncmp(components[j], test, strlen(test)) == 0)
                    {
                        free((void *)components[j]);
                    }
                }
            }
        }
    }

    // If we're in a refresh cycle, check if it's complete
    if (isRefreshing && reg == 0x2fc0)
    {
        // Refresh is complete, notify the system
        const char *refreshComponents[] = {"refresh", "done"};
        int npath = oscnode_find(refreshComponents, 2, path, 16);

        if (npath > 0 && path[npath - 1]->new)
        {
            path[npath - 1]->new(path, "/refresh/done", 0, 1);
        }

        // Reset refreshing state
        refreshing_state(0);
    }

    oscflush();
}

/**
 * @brief Handles audio level values received from the device
 *
 * @param subid The sub ID of the SysEx message
 * @param payload The buffer containing the audio level values
 * @param len The length of the buffer in 32-bit words
 */
void handlelevels(int subid, uint_least32_t *payload, size_t len)
{
    static uint_least32_t inputpeakfx[22], outputpeakfx[22];
    static uint_least64_t inputrmsfx[22], outputrmsfx[22];
    uint_least32_t peak, *peakfx;
    uint_least64_t rms, *rmsfx;
    float peakdb, peakfxdb, rmsdb, rmsfxdb;
    const char *type;
    char addr[128];
    size_t i;

    if (len % 3 != 0)
    {
        fprintf(stderr, "unexpected levels data\n");
        return;
    }

    len /= 3;
    type = NULL;
    peakfx = NULL;
    rmsfx = NULL;

    switch (subid)
    {
    case 4:
        type = "input"; /* fallthrough */
    case 1:
        peakfx = inputpeakfx, rmsfx = inputrmsfx;
        if (!type)
            type = "input";
        break;
    case 5:
        type = "output"; /* fallthrough */
    case 3:
        peakfx = outputpeakfx, rmsfx = outputrmsfx;
        if (!type)
            type = "output";
        break;
    case 2:
        type = "playback";
        break;
    default:
        fprintf(stderr, "Unknown level type: %d\n", subid);
        return;
    }

    for (i = 0; i < len; i++)
    {
        rms = *payload++;
        rms |= (uint_least64_t)*payload++ << 32;
        peak = *payload++;

        if (type)
        {
            peakdb = 20 * log10((peak >> 4) / 0x1p23);
            rmsdb = 10 * log10(rms / 0x1p54);
            snprintf(addr, sizeof addr, "/%s/%zu/level", type, i + 1);

            if (peakfx)
            {
                peakfxdb = 20 * log10((peakfx[i] >> 4) / 0x1p23);
                rmsfxdb = 10 * log10(rmsfx[i] / 0x1p54);
                oscsend(addr, ",ffff", peakdb, rmsdb, peakfxdb, rmsfxdb);
                peakfx[i] = peak;
                rmsfx[i] = rms;
            }
            else
            {
                oscsend(addr, ",ff", peakdb, rmsdb);
            }
        }
    }

    oscflush();
}

/**
 * @brief Updates an integer parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newint(const struct oscnode *path[], const char *addr, int reg, int val)
{
    oscsend(addr, ",i", val);
    return 0;
}

/**
 * @brief Updates a fixed-point parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newfixed(const struct oscnode *path[], const char *addr, int reg, int val)
{
    float scale = 1.0f;

    // Find the scale for this parameter
    for (int i = 0; path[i]; i++)
    {
        if (path[i]->reg == reg && path[i]->set == setfixed)
        {
            scale = path[i]->scale;
            break;
        }
    }

    float fval = val / scale;
    oscsend(addr, ",f", fval);
    return 0;
}

/**
 * @brief Updates an enumerated parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newenum(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Find the enum names for this parameter
    for (int i = 0; path[i]; i++)
    {
        if (path[i]->reg == reg && path[i]->set == setenum)
        {
            oscsendenum(addr, val, path[i]->names, path[i]->nameslen);
            return 0;
        }
    }

    // If we couldn't find the enum names, just send the numeric value
    oscsend(addr, ",i", val);
    return 0;
}

/**
 * @brief Updates a boolean parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newbool(const struct oscnode *path[], const char *addr, int reg, int val)
{
    oscsend(addr, val ? ",T" : ",F");
    oscsend(addr, ",i", val ? 1 : 0);
    return 0;
}

/**
 * @brief Updates an input stereo parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newinputstereo(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int idx = (path[-1] - path[-2]->child) & -2;
    struct input_state *input;

    // Update both channels of the stereo pair
    input = get_input_state(idx);
    if (input)
    {
        input->stereo = val != 0;
    }

    input = get_input_state(idx + 1);
    if (input)
    {
        input->stereo = val != 0;
    }

    return newbool(path, addr, reg, val);
}

/**
 * @brief Updates an output stereo parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newoutputstereo(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int idx = (path[-1] - path[-2]->child) & -2;
    struct output_state *output;

    // Update both channels of the stereo pair
    output = get_output_state(idx);
    if (output)
    {
        output->stereo = val != 0;
    }

    output = get_output_state(idx + 1);
    if (output)
    {
        output->stereo = val != 0;
    }

    return newbool(path, addr, reg, val);
}

/**
 * @brief Updates an input gain parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newinputgain(const struct oscnode *path[], const char *addr, int reg, int val)
{
    float gain = val / 10.0f;
    oscsend(addr, ",f", gain);
    return 0;
}

/**
 * @brief Updates an input 48V/reflevel parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newinput48v_reflevel(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static const char *const names[] = {"+7dBu", "+13dBu", "+19dBu"};
    int idx;
    const struct device *cur_device = getDevice();

    idx = path[-1] - path[-2]->child;
    if (idx >= cur_device->inputslen)
        return -1;

    const struct inputinfo *info = &cur_device->inputs[idx];
    if (info->flags & INPUT_48V)
    {
        return newbool(path, addr, reg, val);
    }
    else if (info->flags & INPUT_HIZ)
    {
        oscsendenum(addr, val & 0xf, names, 2);
        return 0;
    }
    else if (info->flags & INPUT_REFLEVEL)
    {
        oscsendenum(addr, val & 0xf, names + 1, 2);
        return 0;
    }

    return -1;
}

/**
 * @brief Updates an input Hi-Z parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newinputhiz(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int idx;
    const struct device *cur_device = getDevice();

    idx = path[-1] - path[-2]->child;
    if (idx >= cur_device->inputslen)
        return -1;

    if (cur_device->inputs[idx].flags & INPUT_HIZ)
    {
        return newbool(path, addr, reg, val);
    }

    return -1;
}

/**
 * @brief Updates a mix parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newmix(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int outidx, inidx;
    bool isPan;

    outidx = (reg & 0xfff) >> 6;
    inidx = reg & 0x3f;

    const struct device *cur_device = getDevice();
    if (outidx >= cur_device->outputslen || inidx >= cur_device->inputslen)
        return -1;

    // Is this a pan value or a volume value?
    isPan = val & 0x8000;
    val = ((val & 0x7fff) ^ 0x4000) - 0x4000;

    if (isPan)
    {
        // Pan value
        oscsend(addr, ",i", val);
    }
    else
    {
        // Volume value (in 0.1 dB units)
        oscsend(addr, ",f", val / 10.0f);
    }

    return 0;
}

/**
 * @brief Updates a sample rate parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newsamplerate(const struct oscnode *path[], const char *addr, int reg, int val)
{
    uint_least32_t rate;

    rate = getsamplerate(val);
    if (rate != 0)
    {
        oscsend(addr, ",i", rate);
    }
    else
    {
        oscsend(addr, ",i", 0);
    }

    return 0;
}

/**
 * @brief Updates a DSP load parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdspload(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int load, vers;

    load = val & 0xff;
    vers = val >> 8;

    // Update device state
    set_dsp_state(vers, load);

    // Send OSC messages
    oscsend(addr, ",i", load);

    char versaddr[256];
    snprintf(versaddr, sizeof versaddr, "/hardware/dspversion");
    oscsend(versaddr, ",i", vers);

    return 0;
}

/**
 * @brief Updates a DURec status parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurecstatus(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static const char *const names[] = {
        "No Media",
        "Filesystem Error",
        "Initializing",
        "Reinitializing",
        [5] = "Stopped",
        "Recording",
        [10] = "Playing",
        "Paused",
    };
    int status, position;

    status = val & 0xf;
    position = (val >> 8) * 100 / 65;

    // Update device state
    set_durec_state(status, position, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

    // Send status as both string and integer
    if (status >= 0 && status < sizeof(names) / sizeof(names[0]) && names[status])
        oscsend(addr, ",s", names[status]);
    else
        oscsend(addr, ",s", "Unknown");

    oscsend(addr, ",i", status);

    // Send position separately
    char posaddr[256];
    snprintf(posaddr, sizeof posaddr, "/durec/position");
    oscsend(posaddr, ",i", position);

    return 0;
}

/**
 * @brief Updates a DURec time parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurectime(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Update device state
    set_durec_state(-1, -1, val, -1, -1, -1, -1, -1, -1, -1, -1, -1);

    // Send OSC message
    oscsend(addr, ",i", val);
    return 0;
}

/**
 * @brief Updates a DURec USB status parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurecusbstatus(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int usbload, usberrors;

    usbload = val >> 8;
    usberrors = val & 0xff;

    // Update device state
    set_durec_state(-1, -1, -1, usberrors, usbload, -1, -1, -1, -1, -1, -1, -1);

    // Send OSC messages
    char loadaddr[256], erraddr[256];
    snprintf(loadaddr, sizeof loadaddr, "/durec/usbload");
    snprintf(erraddr, sizeof erraddr, "/durec/usberrors");

    oscsend(loadaddr, ",i", usbload);
    oscsend(erraddr, ",i", usberrors);

    return 0;
}

/**
 * @brief Updates a DURec total space parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurectotalspace(const struct oscnode *path[], const char *addr, int reg, int val)
{
    float totalspace = val / 16.0f;

    // Update device state
    set_durec_state(-1, -1, -1, -1, -1, totalspace, -1, -1, -1, -1, -1, -1);

    // Send OSC message
    oscsend(addr, ",f", totalspace);
    return 0;
}

/**
 * @brief Updates a DURec free space parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurecfreespace(const struct oscnode *path[], const char *addr, int reg, int val)
{
    float freespace = val / 16.0f;

    // Update device state
    set_durec_state(-1, -1, -1, -1, -1, -1, freespace, -1, -1, -1, -1, -1);

    // Send OSC message
    oscsend(addr, ",f", freespace);
    return 0;
}

/**
 * @brief Updates a DURec files length parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurecfileslen(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Update the file length in device state
    if (set_durec_files_length(val) != 0)
        return -1;

    // Send OSC message
    oscsend(addr, ",i", val);
    return 0;
}

/**
 * @brief Updates a DURec file parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurecfile(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Update device state
    set_durec_state(-1, -1, -1, -1, -1, -1, -1, val, -1, -1, -1, -1);

    // Send OSC message
    oscsend(addr, ",i", val);
    return 0;
}

/**
 * @brief Updates a DURec next parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurecnext(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int next, playmode;

    next = ((val & 0xfff) ^ 0x800) - 0x800;
    playmode = val >> 12;

    // Update device state
    set_durec_state(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, next, playmode);

    // Send OSC message for next file
    oscsend(addr, ",i", next);

    // Send OSC message for play mode
    char modeaddr[256];
    snprintf(modeaddr, sizeof modeaddr, "/durec/playmode");

    static const char *const playmode_names[] = {"Single", "Repeat", "Sequence", "Random"};
    if (playmode >= 0 && playmode < sizeof(playmode_names) / sizeof(playmode_names[0]))
        oscsend(modeaddr, ",s", playmode_names[playmode]);
    else
        oscsend(modeaddr, ",s", "Unknown");

    oscsend(modeaddr, ",i", playmode);

    return 0;
}

/**
 * @brief Updates a DURec play mode parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurecplaymode(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static const char *const names[] = {"Single", "Repeat", "Sequence", "Random"};

    // Update device state
    set_durec_state(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, val);

    // Send OSC message
    if (val >= 0 && val < sizeof(names) / sizeof(names[0]))
        oscsend(addr, ",s", names[val]);
    else
        oscsend(addr, ",s", "Unknown");

    oscsend(addr, ",i", val);

    return 0;
}

/**
 * @brief Updates a DURec record time parameter and sends OSC notification
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int newdurecrecordtime(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Update device state
    set_durec_state(-1, -1, -1, -1, -1, -1, -1, -1, val, -1, -1, -1);

    // Send OSC message
    oscsend(addr, ",i", val);
    return 0;
}

/**
 * @brief Handles completion of refresh operation
 *
 * @param path The OSC address path
 * @param addr The OSC address string
 * @param reg The register address
 * @param val The new value
 * @return 0 on success, non-zero on failure
 */
int refreshdone(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Reset refreshing state
    refreshing_state(0);

    // Send notification
    oscsend(addr, ",T");
    return 0;
}
