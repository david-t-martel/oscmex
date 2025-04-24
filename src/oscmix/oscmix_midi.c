/**
 * @file oscmix_midi.c
 * @brief MIDI SysEx handling and OSC response functions
 */

#include "oscmix_midi.h"
#include "platform.h"
#include "device.h"
#include "device_state.h"
#include "logging.h"
#include "sysex.h"
#include "util.h"
#include "observer_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>

/* Global variables related to MIDI communication */
static char oscbuf[8192];
static struct oscmsg oscmsg;

/* Last error information */
static int last_error_code = 0;
static char last_error_message[256] = "";
static char last_error_context[64] = "";

/* Observer state tracking */
static bool dsp_observer_active = false;
static bool durec_observer_active = false;
static bool samplerate_observer_active = false;
static bool input_observer_active = false;
static bool output_observer_active = false;
static bool mixer_observer_active = false;

/**
 * @brief Set the last error information
 *
 * @param code Error code
 * @param context Error context string
 * @param format Format string followed by arguments (printf style)
 * @param ... Variable arguments
 */
void set_last_error(int code, const char *context, const char *format, ...)
{
    va_list args;

    last_error_code = code;

    if (context && context[0])
    {
        strncpy(last_error_context, context, sizeof(last_error_context) - 1);
        last_error_context[sizeof(last_error_context) - 1] = '\0';
    }
    else
    {
        last_error_context[0] = '\0';
    }

    if (format)
    {
        va_start(args, format);
        vsnprintf(last_error_message, sizeof(last_error_message), format, args);
        va_end(args);
    }
    else
    {
        last_error_message[0] = '\0';
    }

    // Log the error
    log_error("%s: %s (code %d)", last_error_context, last_error_message, last_error_code);

    // Send via OSC for GUI clients
    oscsend("/error", ",iss", last_error_code, last_error_context, last_error_message);
    oscflush();
}

/**
 * @brief Get the last error information
 *
 * @param code Pointer to store error code (can be NULL)
 * @param context Buffer to store context string (can be NULL)
 * @param context_size Size of context buffer
 * @return Error message string
 */
const char *get_last_error(int *code, char *context, size_t context_size)
{
    if (code)
    {
        *code = last_error_code;
    }

    if (context && context_size > 0)
    {
        strncpy(context, last_error_context, context_size - 1);
        context[context_size - 1] = '\0';
    }

    return last_error_message;
}

/**
 * @brief Initialize the MIDI interface
 *
 * @return 0 on success, non-zero on failure
 */
int oscmix_midi_init(void)
{
    // Initialize the platform MIDI subsystem
    int result = platform_midi_init();
    if (result != 0)
    {
        log_error("Failed to initialize MIDI subsystem: %s",
                  platform_strerror(platform_errno()));
        return result;
    }

    log_info("MIDI subsystem initialized");
    return 0;
}

/**
 * @brief Close the MIDI interface
 */
void oscmix_midi_close(void)
{
    // Clean up the platform MIDI subsystem
    platform_midi_cleanup();
    log_info("MIDI subsystem closed");
}

/**
 * @brief Get observer registration status
 *
 * @param dsp_active Pointer to store DSP observer status (can be NULL)
 * @param durec_active Pointer to store DURec observer status (can be NULL)
 * @param samplerate_active Pointer to store samplerate observer status (can be NULL)
 * @param input_active Pointer to store input observer status (can be NULL)
 * @param output_active Pointer to store output observer status (can be NULL)
 * @param mixer_active Pointer to store mixer observer status (can be NULL)
 * @return Number of active observers
 */
int get_observer_status(bool *dsp_active, bool *durec_active, bool *samplerate_active,
                        bool *input_active, bool *output_active, bool *mixer_active)
{
    if (dsp_active)
        *dsp_active = dsp_observer_active;
    if (durec_active)
        *durec_active = durec_observer_active;
    if (samplerate_active)
        *samplerate_active = samplerate_observer_active;
    if (input_active)
        *input_active = input_observer_active;
    if (output_active)
        *output_active = output_observer_active;
    if (mixer_active)
        *mixer_active = mixer_observer_active;

    int count = 0;
    if (dsp_observer_active)
        count++;
    if (durec_observer_active)
        count++;
    if (samplerate_observer_active)
        count++;
    if (input_observer_active)
        count++;
    if (output_observer_active)
        count++;
    if (mixer_observer_active)
        count++;

    return count;
}

/**
 * @brief Sets a register value in the device
 *
 * Sends a MIDI SysEx message to set a register in the device.
 *
 * @param reg The register address
 * @param val The value to set
 * @return 0 on success, non-zero on failure
 */
int setreg(unsigned reg, unsigned val)
{
    unsigned char buf[16], sysexbuf[32];
    unsigned char *p;

    // Log register update at debug level
    log_debug("Setting register 0x%04x to 0x%08x", reg, val);

    // Prepare register update message
    p = buf;
    p = putle16(p, reg); // Register address (16-bit)
    p = putle32(p, val); // Register value (32-bit)

    // Send the SysEx message to the device using platform function
    writesysex(0x41, buf, p - buf, sysexbuf);

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
    // Convert float level (0.0 to 1.0) to device value (0 to 65535)
    int val = (int)(level * 65535.0f);

    // Clamp value to valid range
    if (val < 0)
        val = 0;
    if (val > 65535)
        val = 65535;

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
    // Convert dB to internal device value
    // Values below -65dB are treated as -infinity
    int val;

    if (db <= -65.0f)
    {
        val = -650; // -infinity
    }
    else
    {
        // Device uses value in 0.1 dB steps
        val = (int)(db * 10.0f);

        // Clamp to valid range
        if (val < -650)
            val = -650;
        if (val > 60)
            val = 60; // +6dB maximum
    }

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
    // Clamp pan value to valid range (-100 to 100)
    if (pan < -100)
        pan = -100;
    if (pan > 100)
        pan = 100;

    // Set high bit to indicate this is a pan value
    return setreg(reg, pan | 0x8000);
}

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
    struct sysex sx;
    size_t sysexlen;

    // Prepare the SysEx message structure
    sx.mfrid = 0x000166; // RME manufacturer ID
    sx.devid = 0x00;     // Device ID (always 0)
    sx.subid = subid;    // Command ID
    sx.data = (unsigned char *)buf;
    sx.datalen = len;

    // Encode the SysEx message
    sysexlen = sysexenc(&sx, sysexbuf, SYSEX_MFRID | SYSEX_DEVID | SYSEX_SUBID);

    // Send the SysEx message using platform MIDI function
    if (platform_midi_send(sysexbuf, sysexlen) != 0)
    {
        log_error("Failed to send SysEx message (subid 0x%02x, length %zu)",
                  subid, sysexlen);
    }
    else
    {
        log_debug("Sent SysEx message: subid=0x%02x, length=%zu", subid, sysexlen);
    }
}

/**
 * @brief Handles SysEx messages from the device
 *
 * @param buf The buffer containing the SysEx message
 * @param len The length of the buffer
 * @param payload Buffer to store decoded payload
 * @return 0 on success, non-zero on failure
 */
int handlesysex(const unsigned char *buf, size_t len, uint_least32_t *payload)
{
    struct sysex sx;
    unsigned char sysexbuf[4096];
    size_t payloadlen;
    int ret;

    // First check that this is a valid SysEx message
    if (len < 2 || buf[0] != 0xf0 || buf[len - 1] != 0xf7)
    {
        log_warning("Invalid SysEx format (missing F0/F7)");
        return -1;
    }

    // Decode the SysEx message header
    ret = sysexdec(&sx, buf, len, SYSEX_MFRID | SYSEX_DEVID | SYSEX_SUBID);
    if (ret < 0)
    {
        log_warning("Failed to decode SysEx header");
        return -1;
    }

    // Check manufacturer ID (RME = 0x000166)
    if (sx.mfrid != 0x000166)
    {
        log_debug("Ignored SysEx from manufacturer ID: 0x%06X", sx.mfrid);
        return -1;
    }

    // Verify device ID
    if (sx.devid != 0x00)
    {
        log_debug("Ignored SysEx from device ID: 0x%02X", sx.devid);
        return -1;
    }

    // Base-128 decode the SysEx payload
    payloadlen = (sx.datalen * 7 + 7) / 8;
    if (payloadlen > sizeof(sysexbuf))
    {
        log_error("SysEx payload too large: %zu bytes", payloadlen);
        return -1;
    }

    ret = base128dec(sysexbuf, sx.data, sx.datalen);
    if (ret < 0)
    {
        log_error("Failed to decode SysEx payload");
        return -1;
    }

    // Process message based on subid
    switch (sx.subid)
    {
    case 0x42: // Register update
        // Convert payload to 32-bit words
        for (size_t i = 0; i < payloadlen; i += 6)
        {
            if (i + 6 <= payloadlen)
            {
                payload[i / 6] = getle32(sysexbuf + i);
            }
        }
        handleregs(payload, payloadlen / 6);
        break;

    case 0x43: // Level meters
    case 0x44: // Peak level meters
        // Process level meters
        handlelevels(sx.subid, payload, payloadlen / 4);
        break;

    case 0x45: // DSP status
        // Process DSP status update
        // Note 0x45 subid is not currently used
        break;

    default:
        // Unknown subid
        log_debug("Ignored SysEx with unknown subid: 0x%02X", sx.subid);
        break;
    }

    return 0;
}

/**
 * @brief Handle register values received from the device
 *
 * This function processes register updates from the device,
 * updating the internal state and sending OSC notifications.
 *
 * @param payload The buffer containing the register values
 * @param len The length of the buffer in 32-bit words
 */
void handleregs(uint_least32_t *payload, size_t len)
{
    unsigned reg, val;
    const struct oscnode *path[8];
    int npath, i, j;
    const struct device *dev;

    dev = getDevice();

    for (i = 0; i < len; ++i)
    {
        // Extract register address and value
        reg = payload[i] & 0xffff;
        val = payload[i] >> 16;

        log_debug("Register update: 0x%04x = 0x%04x", reg, val);

        // Special handling for the refresh complete register
        if (reg == 0xffff)
        {
            // This indicates a refresh operation is complete
            if (tree->child)
            {
                const char *components[] = {"refresh"};
                npath = oscnode_find(components, 1, path, sizeof(path) / sizeof(path[0]));
                if (npath > 0)
                {
                    refreshdone(path, "/refresh", 0, 0);
                }
            }
            continue;
        }

        // Search for the register in our OSC node tree
        for (j = 0;; ++j)
        {
            const struct paramreg *pr = &dev->paramregs[j];
            if (!pr->reg)
                break;

            if (pr->reg == reg && pr->path && pr->param >= 0)
            {
                // Found the register, now find its path
                npath = oscnode_find((const char **)pr->path, pr->pathlen,
                                     path, sizeof(path) / sizeof(path[0]));
                if (npath > 0)
                {
                    // Call the appropriate handler
                    if (path[npath - 1]->get)
                    {
                        char addr[128];

                        // Build the OSC address
                        addr[0] = '/';
                        addr[1] = '\0';

                        for (i = 0; i < npath; ++i)
                        {
                            if (i > 0)
                            {
                                strcat(addr, "/");
                            }
                            strcat(addr, path[i]->name);
                        }

                        path[npath - 1]->get(path, addr, pr->param, val);
                    }
                }
            }
        }
    }
}

/**
 * @brief Handle audio level values received from the device
 *
 * @param subid The sub ID of the SysEx message
 * @param payload The buffer containing the audio level values
 * @param len The length of the buffer in 32-bit words
 */
void handlelevels(int subid, uint_least32_t *payload, size_t len)
{
    // Process level meters from the device
    // Send as OSC messages to clients interested in meters

    // VU meters (subid 0x43) or peak meters (subid 0x44)
    const char *addr = (subid == 0x43) ? "/vu" : "/peak";

    // Send meter values as an OSC bundle
    // Each meter value is normalized to 0.0-1.0 range

    for (size_t i = 0; i < len && i < 32; i++)
    {
        float value = payload[i] / 65535.0f;
        char meter_addr[32];

        // Format the address for this meter channel
        snprintf(meter_addr, sizeof(meter_addr), "%s/%zu", addr, i + 1);

        // Send the value
        oscsend(meter_addr, ",f", value);
    }

    // Flush the OSC buffer to send all the meter values
    oscflush();
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

    // Initialize message with address and type tags
    osc_init_message(&oscmsg, addr, type);

    // Process variable arguments based on type tags
    va_start(ap, type);
    while (*type)
    {
        switch (*type++)
        {
        case ',':
            // Type tag separator, skip
            break;

        case 'i':
            // Add integer argument
            osc_add_int(&oscmsg, va_arg(ap, int));
            break;

        case 'f':
            // Add float argument
            osc_add_float(&oscmsg, (float)va_arg(ap, double));
            break;

        case 's':
            // Add string argument
            osc_add_string(&oscmsg, va_arg(ap, const char *));
            break;

        case 'b':
            // Add blob argument
            {
                const void *data = va_arg(ap, const void *);
                int bloblen = va_arg(ap, int);
                osc_add_blob(&oscmsg, data, bloblen);
            }
            break;

        case 'T':
            // Add true argument
            osc_add_true(&oscmsg);
            break;

        case 'F':
            // Add false argument
            osc_add_false(&oscmsg);
            break;

        case 'N':
            // Add nil argument
            osc_add_nil(&oscmsg);
            break;

        default:
            // Unknown type tag, ignore
            break;
        }
    }
    va_end(ap);

    // Encode the OSC message
    len = osc_encode(&oscmsg, oscbuf, sizeof(oscbuf));

    // Send the message to OSC clients
    if (len > 0)
    {
        extern void writeosc(const void *, size_t);
        writeosc(oscbuf, len);
    }
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
    // Check if value is in range
    if (val >= 0 && val < nameslen && names[val])
    {
        // Send both the string and integer representation
        oscsend(addr, ",si", names[val], val);
    }
    else
    {
        // Value out of range, just send the integer
        oscsend(addr, ",i", val);
    }
}

/**
 * @brief Flushes the OSC message buffer
 */
void oscflush(void)
{
    // Nothing to do here as messages are sent immediately
    // This function exists for compatibility with systems that might buffer OSC messages
}

/**
 * @brief Gets the sample rate corresponding to a value
 *
 * @param val The value representing the sample rate
 * @return The sample rate in Hz
 */
long getsamplerate(int val)
{
    // Convert device sample rate code to actual sample rate in Hz
    switch (val)
    {
    case 0:
        return 44100;
    case 1:
        return 48000;
    case 2:
        return 88200;
    case 3:
        return 96000;
    case 4:
        return 176400;
    case 5:
        return 192000;
    case 8:
        return 32000; // Some devices support this
    default:
        return 0; // Unknown
    }
}

/**
 * @brief Register OSC observers for device state changes
 *
 * This function sets up callbacks that send OSC messages
 * when device state changes occur.
 *
 * @return 0 on success, non-zero on failure
 */
int register_osc_observers(void)
{
    // Register for device state changes that we want to report via OSC

    // Request an initial refresh to get current device state
    log_info("Registering OSC observers...");

    // System state observers
    int status = 0;

    // DSP status observer
    status |= register_dsp_observer();
    if (status == 0)
    {
        dsp_observer_active = true;
        log_debug("DSP observer registered");
    }
    else
    {
        log_warning("Failed to register DSP observer");
    }

    // Sample rate observer
    status = register_samplerate_observer();
    if (status == 0)
    {
        samplerate_observer_active = true;
        log_debug("Sample rate observer registered");
    }
    else
    {
        log_warning("Failed to register sample rate observer");
    }

    // Input channel observers
    status = register_input_observers();
    if (status == 0)
    {
        input_observer_active = true;
        log_debug("Input observers registered");
    }
    else
    {
        log_warning("Failed to register input observers");
    }

    // Output channel observers
    status = register_output_observers();
    if (status == 0)
    {
        output_observer_active = true;
        log_debug("Output observers registered");
    }
    else
    {
        log_warning("Failed to register output observers");
    }

    // Mixer observers
    status = register_mixer_observers();
    if (status == 0)
    {
        mixer_observer_active = true;
        log_debug("Mixer observers registered");
    }
    else
    {
        log_warning("Failed to register mixer observers");
    }

    // DURec observers
    status = register_durec_observers();
    if (status == 0)
    {
        durec_observer_active = true;
        log_debug("DURec observers registered");
    }
    else
    {
        log_warning("Failed to register DURec observers");
    }

    // Check overall registration status
    int active_count = get_observer_status(NULL, NULL, NULL, NULL, NULL, NULL);
    if (active_count == 0)
    {
        log_error("No observers registered - OSC updates will not work");
        return -1;
    }
    else if (active_count < 6)
    {
        log_warning("Only %d/6 observers registered - some OSC updates may not work", active_count);
        return 0;
    }
    else
    {
        log_info("All OSC observers registered successfully");
        return 0;
    }
}

/**
 * @brief Register essential OSC observers only
 *
 * This function registers only the most critical observers, for
 * cases where the full observer set fails to register.
 *
 * @return 0 on success, non-zero on failure
 */
int register_essential_osc_observers(void)
{
    // Reset observer flags
    dsp_observer_active = false;
    durec_observer_active = false;
    samplerate_observer_active = false;
    input_observer_active = false;
    output_observer_active = false;
    mixer_observer_active = false;

    log_info("Registering essential OSC observers only...");

    // Try to register sample rate observer
    int status = register_samplerate_observer();
    if (status == 0)
    {
        samplerate_observer_active = true;
        log_debug("Sample rate observer registered");
    }

    // Try to register input observers
    status = register_input_observers();
    if (status == 0)
    {
        input_observer_active = true;
        log_debug("Input observers registered");
    }

    // Try to register output observers
    status = register_output_observers();
    if (status == 0)
    {
        output_observer_active = true;
        log_debug("Output observers registered");
    }

    // Check if we registered anything
    int active_count = get_observer_status(NULL, NULL, NULL, NULL, NULL, NULL);
    if (active_count == 0)
    {
        log_error("No essential observers registered - OSC updates will not work");
        return -1;
    }
    else
    {
        log_info("Registered %d essential observers", active_count);
        return 0;
    }
}

/**
 * @brief Send complete device state via OSC
 *
 * This function sends the entire device state to OSC clients,
 * useful for GUI initialization or reconnection.
 */
void send_full_device_state(void)
{
    const struct device *dev = getDevice();
    if (!dev)
    {
        log_error("Cannot send device state: no device available");
        return;
    }

    log_info("Sending full device state...");

    // System parameters
    oscsend("/system/samplerate", ",i", dev->samplerate);
    oscsend("/system/clocksource", ",i", dev->clocksource);
    oscsend("/system/buffersize", ",i", dev->buffersize);
    oscsend("/system/phantompower", ",i", dev->phantompower);

    // Input channels
    for (int i = 0; i < dev->inputslen; i++)
    {
        const struct inputinfo *input = &dev->inputs[i];
        struct input_state *state = get_input_state_struct(i);

        char path[64];
        snprintf(path, sizeof(path), "/input/%d", i + 1);

        if (state)
        {
            snprintf(path, sizeof(path), "/input/%d/gain", i + 1);
            oscsend(path, ",f", state->gain);

            if (input->flags & INPUT_48V)
            {
                snprintf(path, sizeof(path), "/input/%d/phantom", i + 1);
                oscsend(path, ",i", state->phantom);
            }

            if (input->flags & INPUT_HIZ)
            {
                snprintf(path, sizeof(path), "/input/%d/hiz", i + 1);
                oscsend(path, ",i", state->hiz);
            }

            snprintf(path, sizeof(path), "/input/%d/mute", i + 1);
            oscsend(path, ",i", state->mute);

            snprintf(path, sizeof(path), "/input/%d/stereo", i + 1);
            oscsend(path, ",i", state->stereo);

            if (input->flags & INPUT_REFLEVEL)
            {
                snprintf(path, sizeof(path), "/input/%d/reflevel", i + 1);
                oscsend(path, ",i", state->reflevel);
            }
        }
    }

    // Output channels
    for (int i = 0; i < dev->outputslen; i++)
    {
        const struct outputinfo *output = &dev->outputs[i];
        struct output_state *state = get_output_state_struct(i);

        char path[64];

        if (state)
        {
            snprintf(path, sizeof(path), "/output/%d/volume", i + 1);
            oscsend(path, ",f", state->volume);

            snprintf(path, sizeof(path), "/output/%d/mute", i + 1);
            oscsend(path, ",i", state->mute);

            snprintf(path, sizeof(path), "/output/%d/stereo", i + 1);
            oscsend(path, ",i", state->stereo);

            if (output->flags & OUTPUT_REFLEVEL)
            {
                snprintf(path, sizeof(path), "/output/%d/reflevel", i + 1);
                oscsend(path, ",i", state->reflevel);
            }

            if (output->flags & OUTPUT_DITHER)
            {
                snprintf(path, sizeof(path), "/output/%d/dither", i + 1);
                oscsend(path, ",i", state->dither);
            }
        }
    }

    // Send hardware status information
    oscsend("/hardware/dspload", ",i", dev->dspload);
    oscsend("/hardware/temperature", ",i", dev->temperature);
    oscsend("/hardware/clockstatus", ",i", dev->clockstatus);

    // Send all values in a single packet
    oscflush();

    log_info("Full device state sent");
}

/* Parameter update handler functions */

/**
 * @brief OSC handler for integer parameters
 */
int newint(const struct oscnode *path[], const char *addr, int reg, int val)
{
    oscsend(addr, ",i", val);
    return 0;
}

/**
 * @brief OSC handler for fixed-point parameters
 */
int newfixed(const struct oscnode *path[], const char *addr, int reg, int val)
{
    float scale = 1.0f;

    // Find the scale factor for this parameter
    for (int i = 0; path[i]; i++)
    {
        if (path[i]->reg == reg && path[i]->get == newfixed)
        {
            scale = path[i]->scale;
            break;
        }
    }

    // Convert to float and send
    float f = val / scale;
    oscsend(addr, ",f", f);
    return 0;
}

/**
 * @brief OSC handler for enumerated parameters
 */
int newenum(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Find the enum names for this parameter
    for (int i = 0; path[i]; i++)
    {
        if (path[i]->reg == reg && path[i]->get == newenum)
        {
            // Send both name and value
            if (val >= 0 && val < path[i]->nameslen && path[i]->names[val])
            {
                oscsend(addr, ",si", path[i]->names[val], val);
            }
            else
            {
                // Value out of range, just send integer
                oscsend(addr, ",i", val);
            }
            return 0;
        }
    }

    // Enum names not found, send just the value
    oscsend(addr, ",i", val);
    return 0;
}

/**
 * @brief OSC handler for boolean parameters
 */
int newbool(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Convert any non-zero value to 1
    oscsend(addr, ",i", val ? 1 : 0);
    return 0;
}

/**
 * @brief Callback for when sample rate changes
 */
int newsamplerate(const struct oscnode *path[], const char *addr, int reg, int val)
{
    const char *const sr_names[] = {"44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz",
                                    "176.4 kHz", "192 kHz", "Unknown", "Unknown", "32 kHz"};
    const int sr_values[] = {44100, 48000, 88200, 96000, 176400, 192000, 0, 0, 32000};
    const int num_rates = sizeof(sr_values) / sizeof(sr_values[0]);

    // Send the sample rate as both enum name and Hz value
    if (val >= 0 && val < num_rates)
    {
        // First send to the enum address (e.g., /system/samplerate)
        oscsendenum(addr, val, sr_names, num_rates);

        // Then send the exact Hz value to child node (e.g., /system/samplerate/44100)
        char hz_addr[128];
        snprintf(hz_addr, sizeof(hz_addr), "%s/%d", addr, sr_values[val]);
        oscsend(hz_addr, ",i", 1); // Send 1 to indicate this is the active rate
    }
    else
    {
        // Unknown value
        oscsend(addr, ",i", val);
    }

    return 0;
}

/**
 * @brief Called when device refresh is completed
 */
int refreshdone(const struct oscnode *path[], const char *addr, int reg, int val)
{
    // Notify that refresh is complete
    oscsend("/refresh/done", ",i", 1);

    // Update observers after refresh completes
    register_osc_observers();

    // Send the full device state to clients
    send_full_device_state();

    return 0;
}

/* Implementation of other parameter handlers specific to device types */
/* These will need to be implemented based on the specific device */
