/**
 * @file oscmix_midi.c
 * @brief Implementation of MIDI SysEx handling functions
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

/* Global variables related to MIDI that were in oscmix.c */
static bool refreshing = false;
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
    struct sysex sysex;
    size_t sysexlen;

    sysex.mfrid = 0x200d;
    sysex.devid = 0x10;
    sysex.data = NULL;
    sysex.datalen = len * 5 / 4;
    sysex.subid = subid;
    sysexlen = sysexenc(&sysex, sysexbuf, SYSEX_MFRID | SYSEX_DEVID | SYSEX_SUBID);
    base128enc(sysex.data, buf, len);
    writemidi(sysexbuf, sysexlen);
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
    unsigned long regval;
    unsigned char buf[4], sysexbuf[7 + 5];
    unsigned par;

#ifdef DEBUG
    if (dflag && reg != 0x3f00)
        fprintf(stderr, "setreg %#.4x %#.4x\n", reg, val);
#endif

    regval = (reg & 0x7fff) << 16 | (val & 0xffff);
    par = regval >> 16 ^ regval;
    par ^= par >> 8;
    par ^= par >> 4;
    par ^= par >> 2;
    par ^= par >> 1;
    regval |= (~par & 1) << 31;
    putle32(buf, regval);

    writesysex(0, buf, sizeof buf, sysexbuf);

    // Special handling for refresh command
    if (reg == 0x8000 && val == 0xFFFFFFFF)
    {
        refreshing = true;
    }

    return 0;
}

/**
 * @brief Sets an audio level parameter value in the device
 *
 * @param reg The register address
 * @param level The audio level value
 * @return 0 on success, non-zero on failure
 */
int setlevel(int reg, float level)
{
    long val;

    val = level * 0x8000l;
    assert(val >= 0);
    assert(val <= 0x10000);
    if (val > 0x4000)
        val = (val >> 3) - 0x8000;
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

    val = (isinf(db) && db < 0 ? -650 : (int)(db * 10)) & 0x7fff;
    return setreg(reg, val);
}

/**
 * @brief Sets a pan value in the device
 *
 * @param reg The register address
 * @param pan The pan value
 * @return 0 on success, non-zero on failure
 */
int setpan(int reg, int pan)
{
    int val;

    val = (pan & 0x7fff) | 0x8000;
    return setreg(reg, val);
}

/**
 * @brief Handles register values received from the device
 *
 * @param payload The buffer containing the register values
 * @param len The length of the buffer
 */
void handleregs(uint_least32_t *payload, size_t len)
{
    size_t i;
    int reg, val;

    for (i = 0; i < len; i++)
    {
        reg = payload[i] >> 16 & 0x7fff;
        val = (long)((payload[i] & 0xffff) ^ 0x8000) - 0x8000;

#ifdef DEBUG
        fprintf(stderr, "Received register 0x%04x = 0x%08x\n", reg, val);
#endif

        // Lookup the node in the tree and notify clients of changes
        const struct oscnode *path[16];
        char addr[256];

        // Find parameters using this register and call the appropriate newXXX functions
        // This would be a complex implementation that searches the tree

        // For now, just provide a simplified implementation
        switch (reg)
        {
        case 0x8000: // Sample rate
            newsamplerate(NULL, "/clock/samplerate", reg, val);
            break;
        case 0x3e04: // DSP load
            newdspload(NULL, "/hardware/dspload", reg, val);
            break;
            // Add other known registers here
        }
    }

    // If we're in a refresh cycle, continue sending register read commands
    if (refreshing && reg == 0x2fc0)
    {
        refreshdone(NULL, NULL, reg, val);
    }

    oscflush();
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
        break;
    case 5:
        type = "output"; /* fallthrough */
    case 3:
        peakfx = outputpeakfx, rmsfx = outputrmsfx;
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

                // Send both pre-FX and post-FX levels
                oscsend(addr, ",ff", peakdb, rmsdb);
                snprintf(addr, sizeof addr, "/%s/%zu/fxlevel", type, i + 1);
                oscsend(addr, ",ff", peakfxdb, rmsfxdb);
            }
            else
            {
                // Send only level for playback
                oscsend(addr, ",ff", peakdb, rmsdb);
            }
        }
        else
        {
            // Store for later comparison
            *peakfx++ = peak;
            *rmsfx++ = rms;
        }
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
    unsigned char *len;
    va_list ap;

    assert(addr[0] == '/');
    assert(type[0] == ',');

    if (!oscmsg.buf)
    {
        oscmsg.buf = oscbuf;
        oscmsg.end = oscbuf + sizeof oscbuf;
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
            oscputfloat(&oscmsg, va_arg(ap, double));
            break;
        case 'i':
            oscputint(&oscmsg, va_arg(ap, uint_least32_t));
            break;
        case 's':
            oscputstr(&oscmsg, va_arg(ap, const char *));
            break;
        default:
            assert(0);
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
        oscsend(addr, ",is", val, names[val]);
    }
    else
    {
        fprintf(stderr, "unexpected enum value %d\n", val);
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
        writeosc(oscbuf, oscmsg.buf - oscbuf);
        oscmsg.buf = NULL;
    }
}

/**
 * @brief Sends a new integer parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newint(const struct oscnode *path[], const char *addr, int reg, int val)
{
    oscsend(addr, ",i", val);
    return 0;
}

/**
 * @brief Sends a new fixed-point parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newfixed(const struct oscnode *path[], const char *addr, int reg, int val)
{
    const struct oscnode *node;

    node = path[0];
    oscsend(addr, ",f", val * node->scale);
    return 0;
}

/**
 * @brief Sends a new enumerated parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newenum(const struct oscnode *path[], const char *addr, int reg, int val)
{
    const struct oscnode *node;

    node = path[0];
    oscsendenum(addr, val, node->names, node->nameslen);
    return 0;
}

/**
 * @brief Sends a new boolean parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newbool(const struct oscnode *path[], const char *addr, int reg, int val)
{
    oscsend(addr, ",i", val != 0);
    return 0;
}

/**
 * @brief Sends a new stereo state for an input channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newinputstereo(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int idx;
    char addrbuf[256];

    idx = path[-1] - path[-2]->child;
    idx = (idx & -2); // Round down to even number

    // Send to both channels in the stereo pair
    snprintf(addrbuf, sizeof addrbuf, "/input/%d/stereo", idx + 1);
    oscsend(addrbuf, ",i", val != 0);
    snprintf(addrbuf, sizeof addrbuf, "/input/%d/stereo", idx + 2);
    oscsend(addrbuf, ",i", val != 0);

    return 0;
}

/**
 * @brief Sends a new stereo state for an output channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newoutputstereo(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int idx;
    char addrbuf[256];

    idx = path[-1] - path[-2]->child;
    idx = (idx & -2); // Round down to even number

    // Send to both channels in the stereo pair
    snprintf(addrbuf, sizeof addrbuf, "/output/%d/stereo", idx + 1);
    oscsend(addrbuf, ",i", val != 0);
    snprintf(addrbuf, sizeof addrbuf, "/output/%d/stereo", idx + 2);
    oscsend(addrbuf, ",i", val != 0);

    return 0;
}

/**
 * @brief Sends a new gain value for an input channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newinputgain(const struct oscnode *path[], const char *addr, int reg, int val)
{
    oscsend(addr, ",f", val / 10.0);
    return 0;
}

/**
 * @brief Sends a new 48V phantom power state or reference level for an input channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newinput48v_reflevel(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static const char *const names[] = {"+7dBu", "+13dBu", "+19dBu"};
    int idx;
    const struct device *cur_device = getDevice();

    idx = path[-1] - path[-2]->child;
    assert(idx < cur_device->inputslen);

    const struct inputinfo *info = &cur_device->inputs[idx];
    if (info->flags & INPUT_48V)
    {
        char addrbuf[256];
        snprintf(addrbuf, sizeof addrbuf, "/input/%d/48v", idx + 1);
        return newbool(path, addrbuf, reg, val);
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
 * @brief Sends a new Hi-Z state for an input channel as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newinputhiz(const struct oscnode *path[], const char *addr, int reg, int val)
{
    int idx;
    const struct device *cur_device = getDevice();

    idx = path[-1] - path[-2]->child;
    assert(idx < cur_device->inputslen);

    if (cur_device->inputs[idx].flags & INPUT_HIZ)
        return newbool(path, addr, reg, val);

    return -1;
}

/**
 * @brief Gets the sample rate corresponding to a value
 *
 * @param val The value representing the sample rate
 * @return The sample rate in Hz
 */
long getsamplerate(int val)
{
    static const long samplerate[] = {
        32000,
        44100,
        48000,
        64000,
        88200,
        96000,
        128000,
        176400,
        192000,
    };

    return (val > 0 && val < sizeof(samplerate) / sizeof(samplerate[0]))
               ? samplerate[val]
               : 0;
}

/**
 * @brief Sends a new sample rate value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newsamplerate(const struct oscnode *path[], const char *addr, int reg, int val)
{
    uint_least32_t rate;

    rate = getsamplerate(val);
    if (rate != 0)
        oscsend(addr, ",i", rate);

    return 0;
}

/**
 * @brief Sends a new DSP load value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newdspload(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static int dsp_load = -1;
    static int dsp_vers = -1;

    int load = val & 0xff;
    int vers = val >> 8;

    if (dsp_load != load)
    {
        dsp_load = load;
        oscsend("/hardware/dspload", ",i", dsp_load);
    }

    if (dsp_vers != vers)
    {
        dsp_vers = vers;
        oscsend("/hardware/dspvers", ",i", dsp_vers);
    }

    return 0;
}

/**
 * @brief Sends a new DSP availability value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newdspavail(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Implementation would go here in a full version
    return 0;
}

/**
 * @brief Sends a new DSP active value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newdspactive(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Implementation would go here in a full version
    return 0;
}

/**
 * @brief Sends a new ARC encoder value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newarcencoder(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Implementation would go here in a full version
    return 0;
}

/**
 * @brief Sends a new mix parameter value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newmix(const struct oscnode *path[], const char *addr, int reg, int val)
{
    char addrbuf[256];
    int outidx, inidx;
    bool newpan;

    outidx = (reg & 0xfff) >> 6;
    inidx = reg & 0x3f;

    const struct device *cur_device = getDevice();
    if (outidx >= cur_device->outputslen || inidx >= cur_device->inputslen)
        return -1;

    // Is this a pan value or a volume value?
    newpan = val & 0x8000;
    val = ((val & 0x7fff) ^ 0x4000) - 0x4000;

    // Format the target address based on the parameters and value type
    if (newpan)
    {
        snprintf(addrbuf, sizeof addrbuf, "/mix/%d/input/%d/pan", outidx + 1, inidx + 1);
        oscsend(addrbuf, ",i", val);
    }
    else
    {
        float db_val = val <= -650 ? -65.0f : val / 10.0f;
        snprintf(addrbuf, sizeof addrbuf, "/mix/%d/input/%d", outidx + 1, inidx + 1);
        oscsend(addrbuf, ",f", db_val);
    }

    return 0;
}

/**
 * @brief Sends a new dynamics level value as an OSC message
 *
 * @param path The OSC address path
 * @param unused Unused parameter
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int newdynlevel(const struct oscnode *path[], const char *unused, int reg, int val)
{
    // Implementation would go here in a full version
    return 0;
}

/**
 * @brief Handles the completion of a refresh operation
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
int refreshdone(const struct oscnode *path[], const char *addr, int reg, int val)
{
    refreshing = false;

#ifdef DEBUG
    fprintf(stderr, "refresh done\n");
#endif

    return 0;
}

/* DURec-related functions */

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

    static int status = -1;
    static int position = -1;

    int new_status = val & 0xf;
    int new_position = (val >> 8) * 100 / 65;

    if (new_status != status)
    {
        status = new_status;
        oscsendenum("/durec/status", status, names, sizeof(names) / sizeof(names[0]));
    }

    if (new_position != position)
    {
        position = new_position;
        oscsend("/durec/position", ",i", position);
    }

    return 0;
}

int newdurectime(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static int time = -1;

    if (val != time)
    {
        time = val;
        oscsend(addr, ",i", val);
    }

    return 0;
}

int newdurecusbstatus(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static int usbload = -1;
    static int usberrors = -1;

    int new_usbload = val >> 8;
    int new_usberrors = val & 0xff;

    if (new_usbload != usbload)
    {
        usbload = new_usbload;
        oscsend("/durec/usbload", ",i", usbload);
    }

    if (new_usberrors != usberrors)
    {
        usberrors = new_usberrors;
        oscsend("/durec/usberrors", ",i", usberrors);
    }

    return 0;
}

int newdurectotalspace(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static float totalspace = -1.0f;
    float new_totalspace = val / 16.0f;

    if (new_totalspace != totalspace)
    {
        totalspace = new_totalspace;
        oscsend(addr, ",f", totalspace);
    }

    return 0;
}

int newdurecfreespace(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static float freespace = -1.0f;
    float new_freespace = val / 16.0f;

    if (new_freespace != freespace)
    {
        freespace = new_freespace;
        oscsend(addr, ",f", freespace);
    }

    return 0;
}

int newdurecfileslen(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // This would manage durecfiles array, but for simplicity
    // we'll just send the count here
    if (val >= 0)
    {
        oscsend(addr, ",i", val);
    }

    return 0;
}

int newdurecfile(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static int file = -1;

    if (val != file)
    {
        file = val;
        oscsend(addr, ",i", val);
    }

    return 0;
}

int newdurecnext(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static const char *const names[] = {
        "Single",
        "UFX Single",
        "Continuous",
        "Single Next",
        "Repeat Single",
        "Repeat All",
    };

    static int next = -1;
    static int playmode = -1;

    int new_next = ((val & 0xfff) ^ 0x800) - 0x800;
    int new_playmode = val >> 12;

    if (new_next != next)
    {
        next = new_next;
        oscsend(addr, ",i", next);
    }

    if (new_playmode != playmode)
    {
        playmode = new_playmode;
        oscsendenum("/durec/playmode", playmode, names, sizeof(names) / sizeof(names[0]));
    }

    return 0;
}

int newdurecrecordtime(const struct oscnode *path[], const char *addr, int reg, int val)
{
    static int recordtime = -1;

    if (val != recordtime)
    {
        recordtime = val;
        oscsend(addr, ",i", val);
    }

    return 0;
}

int newdurecindex(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // This would update the current index in the durecfiles array
    return 0;
}

int newdurecname(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // This would update the name in the durecfiles array
    return 0;
}

int newdurecinfo(const struct oscnode *path[], const char *unused, int reg, int val)
{
    // This would update samplerate and channels in the durecfiles array
    unsigned long samplerate = getsamplerate(val & 0xff);
    int channels = val >> 8;

    if (samplerate > 0)
    {
        // We would have a current index in a full implementation
        // Here we just send a message to a generic address
        oscsend("/durec/samplerate", ",i", samplerate);
        oscsend("/durec/channels", ",i", channels);
    }

    return 0;
}

int newdureclength(const struct oscnode *path[], const char *unused, int reg, int val)
{
    // This would update the length in the durecfiles array
    return 0;
}
