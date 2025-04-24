/**
 * @file oscnode_tree.c
 * @brief Implementation of the OSC node tree structure and functions
 */

#include "oscnode_tree.h"
#include "oscmix_commands.h"
#include "oscmix_midi.h"
#include "device.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * @brief Match a pattern against a string
 *
 * This function supports basic pattern matching used in OSC address pattern matching.
 *
 * @param pat The pattern string (may contain wildcards)
 * @param str The string to match against
 * @return Pointer to the remainder of the pattern if matched, NULL otherwise
 */
const char *match(const char *pat, const char *str)
{
    const char *star = NULL;

restart:
    while (*pat == *str)
    {
        if (!*pat)
            return pat;
        pat++;
        str++;
    }

    if (*pat == '*')
    {
        star = ++pat;
        if (!*pat)
            return pat;
    }
    else if (!*str)
    {
        return NULL;
    }
    else if (star)
    {
        str++;
        pat = star;
        goto restart;
    }

    return NULL;
}

/* Forward declarations of setter functions */
int setint(const struct oscnode *path[], int reg, struct oscmsg *msg);
int setfixed(const struct oscnode *path[], int reg, struct oscmsg *msg);
int setenum(const struct oscnode *path[], int reg, struct oscmsg *msg);
int setbool(const struct oscnode *path[], int reg, struct oscmsg *msg);
int setrefresh(const struct oscnode *path[], int reg, struct oscmsg *msg);
int oscstatus(const struct oscnode *path[], const char *addr, struct oscmsg *msg);
int getlogs(const struct oscnode *path[], const char *addr, struct oscmsg *msg);
int getlasterror(const struct oscnode *path[], const char *addr, struct oscmsg *msg);

/* Forward declarations of getter functions */
int newint(const struct oscnode *path[], const char *addr, int reg, int val);
int newfixed(const struct oscnode *path[], const char *addr, int reg, int val);
int newenum(const struct oscnode *path[], const char *addr, int reg, int val);
int newbool(const struct oscnode *path[], const char *addr, int reg, int val);
int newstr(const struct oscnode *path[], const char *addr, int reg, const char *val);

/* OSC node tree definition */

/* System-level nodes */
static const struct oscnode refresh_nodes[] = {
    {"", 0x8000, setrefresh, NULL, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode samplerate_nodes[] = {
    {"44100", 0, NULL, newint, .data = {0}, NULL},
    {"48000", 0, NULL, newint, .data = {0}, NULL},
    {"88200", 0, NULL, newint, .data = {0}, NULL},
    {"96000", 0, NULL, newint, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode system_nodes[] = {
    {"samplerate", REG_SAMPLE_RATE, setint, newint, .data = {.names = {(const char *const[]){"44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz"}, 4}}, &samplerate_nodes[0]},
    {"clocksource", REG_CLOCK_SOURCE, setenum, newenum, .data = {.names = {CLOCK_SOURCE_NAMES, 4}}, NULL},
    {"buffersize", REG_BUFFER_SIZE, setenum, newenum, .data = {.names = {BUFFER_SIZE_NAMES, 6}}, NULL},
    {"phantompower", REG_PHANTOM_POWER, setbool, newbool, .data = {0}, NULL},
    {"mastervol", REG_MASTER_VOLUME, setfixed, newfixed, .data = {.range = {0, 65535, 65535.0f}}, NULL},
    {"mastermute", REG_MASTER_MUTE, setbool, newbool, .data = {0}, NULL},
    {"digitalgain", REG_DIGITAL_GAIN, setfixed, newfixed, .data = {.range = {0, 65535, 65535.0f}}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Input channel nodes */
static const struct oscnode input_channel_nodes[] = {
    {"gain", REG_INPUT_GAIN, setfixed, newfixed, .data = {.range = {0, 65535, 65535.0f}}, NULL},
    {"phantom", REG_INPUT_PHANTOM, setbool, newbool, .data = {0}, NULL},
    {"pad", REG_INPUT_PAD, setbool, newbool, .data = {0}, NULL},
    {"reflevel", REG_INPUT_REFLEVEL, setenum, newenum, .data = {.names = {INPUT_REF_LEVEL_NAMES, 4}}, NULL},
    {"mute", REG_INPUT_MUTE, setbool, newbool, .data = {0}, NULL},
    {"hiz", REG_INPUT_HIZ, setbool, newbool, .data = {0}, NULL},
    {"aeb", REG_INPUT_AEB, setbool, newbool, .data = {0}, NULL},
    {"locut", REG_INPUT_LOCUT, setbool, newbool, .data = {0}, NULL},
    {"ms", REG_INPUT_MS, setbool, newbool, .data = {0}, NULL},
    {"autoset", REG_INPUT_AUTOSET, setbool, newbool, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode input_nodes[] = {
    {"*", 0, NULL, NULL, .data = {0}, &input_channel_nodes[0]},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Output channel nodes */
static const struct oscnode output_channel_nodes[] = {
    {"volume", REG_OUTPUT_VOLUME, setfixed, newfixed, .data = {.range = {0, 65535, 65535.0f}}, NULL},
    {"mute", REG_OUTPUT_MUTE, setbool, newbool, .data = {0}, NULL},
    {"reflevel", REG_OUTPUT_REFLEVEL, setenum, newenum, .data = {.names = {OUTPUT_REF_LEVEL_NAMES, 4}}, NULL},
    {"dither", REG_OUTPUT_DITHER, setenum, newenum, .data = {.names = {DITHER_NAMES, 3}}, NULL},
    {"phase", REG_OUTPUT_PHASE, setbool, newbool, .data = {0}, NULL},
    {"mono", REG_OUTPUT_MONO, setbool, newbool, .data = {0}, NULL},
    {"loopback", REG_OUTPUT_LOOPBACK, setbool, newbool, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode output_nodes[] = {
    {"*", 0, NULL, NULL, .data = {0}, &output_channel_nodes[0]},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Playback (DAW) channel nodes */
static const struct oscnode playback_channel_nodes[] = {
    {"volume", REG_PLAYBACK_VOLUME, setfixed, newfixed, .data = {.range = {0, 65535, 65535.0f}}, NULL},
    {"mute", REG_PLAYBACK_MUTE, setbool, newbool, .data = {0}, NULL},
    {"phase", REG_PLAYBACK_PHASE, setbool, newbool, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode playback_nodes[] = {
    {"*", 0, NULL, NULL, .data = {0}, &playback_channel_nodes[0]},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Mixer matrix nodes */
static const struct oscnode mixer_channel_nodes[] = {
    {"volume", REG_MIXER_VOLUME, setfixed, newfixed, .data = {.range = {0, 65535, 65535.0f}}, NULL},
    {"pan", REG_MIXER_PAN, setfixed, newfixed, .data = {.range = {0, 65535, 65535.0f}}, NULL},
    {"mute", REG_MIXER_MUTE, setbool, newbool, .data = {0}, NULL},
    {"solo", REG_MIXER_SOLO, setbool, newbool, .data = {0}, NULL},
    {"phase", REG_MIXER_PHASE, setbool, newbool, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode mixer_dest_nodes[] = {
    {"*", 0, NULL, NULL, .data = {0}, &mixer_channel_nodes[0]},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode mixer_source_nodes[] = {
    {"*", 0, NULL, NULL, .data = {0}, &mixer_dest_nodes[0]},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode mixer_nodes[] = {
    {"input", 0, NULL, NULL, .data = {0}, &mixer_source_nodes[0]},
    {"playback", 0, NULL, NULL, .data = {0}, &mixer_source_nodes[0]},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* TotalMix nodes */
static const struct oscnode totalmix_snapshot_nodes[] = {
    {"load", REG_TOTALMIX_LOAD, setint, NULL, .data = {.range = {1, 8, 1.0f}}, NULL},
    {"save", REG_TOTALMIX_SAVE, setint, NULL, .data = {.range = {1, 8, 1.0f}}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode totalmix_nodes[] = {
    {"snapshot", 0, NULL, NULL, .data = {0}, &totalmix_snapshot_nodes[0]},
    {"clearall", REG_TOTALMIX_CLEARALL, setbool, NULL, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Hardware status nodes */
static const struct oscnode hardware_nodes[] = {
    {"dspload", REG_DSP_STATUS, NULL, newint, .data = {0}, NULL},
    {"dspversion", REG_DSP_VERSION, NULL, newint, .data = {0}, NULL},
    {"temperature", REG_TEMPERATURE, NULL, newint, .data = {0}, NULL},
    {"clockstatus", REG_CLOCK_STATUS, NULL, newenum, .data = {.names = {(const char *const[]){"Lock", "No Lock"}, 2}}, NULL},
    {"inpsignal", REG_INPUT_SIGNAL, NULL, newbool, .data = {0}, NULL},
    {"outsignal", REG_OUTPUT_SIGNAL, NULL, newbool, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* DURec status and control nodes */
static const struct oscnode durec_file_nodes[] = {
    {"select", REG_DUREC_FILE, setint, newint, .data = {.range = {0, 65535, 1.0f}}, NULL},
    {"name", 0, NULL, newstr, .data = {0}, NULL},
    {"length", 0, NULL, newint, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

static const struct oscnode durec_nodes[] = {
    {"status", REG_DUREC_STATUS, NULL, newenum, .data = {.names = {DUREC_STATUS_NAMES, 12}}, NULL},
    {"position", REG_DUREC_POSITION, NULL, newint, .data = {.range = {0, 100, 1.0f}}, NULL},
    {"time", REG_DUREC_TIME, NULL, newint, .data = {0}, NULL},
    {"totalspace", REG_DUREC_TOTAL_SPACE, NULL, newfixed, .data = {.range = {0, 65535, 1.0f}}, NULL},
    {"freespace", REG_DUREC_FREE_SPACE, NULL, newfixed, .data = {.range = {0, 65535, 1.0f}}, NULL},
    {"file", 0, NULL, NULL, .data = {0}, &durec_file_nodes[0]},
    {"record", REG_DUREC_RECORD, setbool, NULL, .data = {0}, NULL},
    {"stop", REG_DUREC_STOP, setbool, NULL, .data = {0}, NULL},
    {"play", REG_DUREC_PLAY, setbool, NULL, .data = {0}, NULL},
    {"pause", REG_DUREC_PAUSE, setbool, NULL, .data = {0}, NULL},
    {"next", REG_DUREC_NEXT, setint, newint, .data = {.range = {0, 1, 1.0f}}, NULL},
    {"prev", REG_DUREC_PREV, setbool, NULL, .data = {0}, NULL},
    {"playmode", REG_DUREC_PLAYMODE, setenum, newenum, .data = {.names = {DUREC_PLAYMODE_NAMES, 4}}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Log nodes */
static const struct oscnode log_nodes[] = {
    {"debug", REG_LOG_DEBUG, setbool, newbool, .data = {0}, NULL}, /* Enable/disable debug logging */
    {"level", REG_LOG_LEVEL, setenum, newenum, .data = {.names = {LOG_LEVEL_NAMES, 4}}, NULL},
    {"clear", REG_LOG_CLEAR, setbool, NULL, .data = {0}, NULL}, /* Clear log history */
    {"get", 0x9003, getlogs, NULL, .data = {0}, NULL},          /* Get recent logs */
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Error nodes */
static const struct oscnode error_nodes[] = {
    {"last", 0x9010, getlasterror, NULL, .data = {0}, NULL}, /* Get last error */
    {"count", 0x9011, NULL, newint, .data = {0}, NULL},      /* Get error count */
    {"clear", 0x9012, setbool, NULL, .data = {0}, NULL},     /* Clear error history */
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Version nodes */
static const struct oscnode version_nodes[] = {
    {"oscmix", 0, NULL, newstr, .data = {0}, NULL},   /* OSCMix version */
    {"protocol", 0, NULL, newstr, .data = {0}, NULL}, /* Protocol version */
    {"firmware", 0, NULL, newstr, .data = {0}, NULL}, /* Device firmware version */
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Status and Diagnostics nodes */
static const struct oscnode oscstatus_nodes[] = {
    {"", 0, oscstatus, NULL, .data = {0}, NULL},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

/* Root nodes */
static const struct oscnode root_nodes[] = {
    {"system", 0, NULL, NULL, .data = {0}, &system_nodes[0]},
    {"input", 0, NULL, NULL, .data = {0}, &input_nodes[0]},
    {"output", 0, NULL, NULL, .data = {0}, &output_nodes[0]},
    {"playback", 0, NULL, NULL, .data = {0}, &playback_nodes[0]},
    {"mixer", 0, NULL, NULL, .data = {0}, &mixer_nodes[0]},
    {"totalmix", 0, NULL, NULL, .data = {0}, &totalmix_nodes[0]},
    {"refresh", 0, NULL, NULL, .data = {0}, &refresh_nodes[0]},
    {"hardware", 0, NULL, NULL, .data = {0}, &hardware_nodes[0]}, /* Added hardware node */
    {"durec", 0, NULL, NULL, .data = {0}, &durec_nodes[0]},       /* Added durec node */
    {"logs", 0, NULL, NULL, .data = {0}, &log_nodes[0]},          /* Added logs node */
    {"errors", 0, NULL, NULL, .data = {0}, &error_nodes[0]},      /* Added errors node */
    {"version", 0, NULL, NULL, .data = {0}, &version_nodes[0]},   /* Added version node */
    {"oscstatus", 0, NULL, NULL, .data = {0}, &oscstatus_nodes[0]},
    {NULL, 0, NULL, NULL, .data = {0}, NULL}};

// Make the root node accessible via the header
const struct oscnode tree[] = {
    {"", 0, NULL, NULL, .data = {0}, &root_nodes[0]},
};

/**
 * @brief Initialize the OSC node tree structure
 *
 * Currently a placeholder as the tree is statically defined.
 *
 * @return 0 on success, non-zero on failure
 */
int oscnode_tree_init(void)
{
    // We might eventually add dynamic aspects to tree initialization
    return 0;
}

/**
 * @brief Internal function to find a node in the tree
 *
 * @param node Current node to search from
 * @param components Array of path components
 * @param ncomponents Number of components remaining
 * @param path Output array for the path
 * @param npath Size of the path array
 * @param depth Current depth in the search (internal state)
 * @return Number of nodes in the path, or -1 if not found
 */
static int find_node(const struct oscnode *node,
                     const char **components, int ncomponents,
                     const struct oscnode *path[], int npath,
                     int depth)
{
    if (depth >= npath)
        return -1;

    path[depth] = node;

    if (ncomponents == 0)
        return depth + 1;

    if (!node->child)
        return -1;

    for (const struct oscnode *child = node->child; child->name; child++)
    {
        if (match(components[0], child->name))
        {
            int ret = find_node(child, components + 1, ncomponents - 1,
                                path, npath, depth + 1);
            if (ret >= 0)
                return ret;
        }
    }

    return -1;
}

/**
 * @brief Find a node in the OSC tree by path components
 *
 * @param components Array of path components
 * @param ncomponents Number of components
 * @param path Output array for the node path
 * @param npath Size of the path array
 * @return Number of nodes in the path, or -1 if not found
 */
int oscnode_find(const char **components, int ncomponents,
                 const struct oscnode *path[], int npath)
{
    return find_node(&tree[0], components, ncomponents, path, npath, 0);
}

/**
 * @brief Handle the /oscstatus OSC message
 *
 * This function responds to /oscstatus requests by sending the
 * observer status and other diagnostic information.
 *
 * @param path The path of nodes that led to this handler
 * @param addr The full OSC address
 * @param msg The OSC message containing arguments
 * @return 0 on success, non-zero on failure
 */
int oscstatus(const struct oscnode *path[], const char *addr, struct oscmsg *msg)
{
    extern int get_observer_status(bool *dsp_active, bool *durec_active,
                                   bool *samplerate_active, bool *input_active,
                                   bool *output_active, bool *mixer_active);
    extern void send_full_device_state(void);
    extern void oscsend(const char *addr, const char *type, ...);
    extern void oscflush(void);

    bool dsp_active = false, durec_active = false, samplerate_active = false;
    bool input_active = false, output_active = false, mixer_active = false;

    // Get observer status
    int active_count = get_observer_status(&dsp_active, &durec_active, &samplerate_active,
                                           &input_active, &output_active, &mixer_active);

    // Send status response
    oscsend("/oscstatus/active", ",i", active_count);
    oscsend("/oscstatus/dsp", ",i", dsp_active ? 1 : 0);
    oscsend("/oscstatus/durec", ",i", durec_active ? 1 : 0);
    oscsend("/oscstatus/samplerate", ",i", samplerate_active ? 1 : 0);
    oscsend("/oscstatus/input", ",i", input_active ? 1 : 0);
    oscsend("/oscstatus/output", ",i", output_active ? 1 : 0);
    oscsend("/oscstatus/mixer", ",i", mixer_active ? 1 : 0);

    oscflush();

    // Send full device state if requested and observers are partially active
    if (msg->argc > 0 && msg->argv[0].type == 'i' && msg->argv[0].i > 0 && active_count > 0)
    {
        send_full_device_state();
    }

    return 0;
}

/**
 * @brief Handle the /logs/get OSC message
 *
 * This function retrieves recent log messages and sends them via OSC.
 *
 * @param path The path of nodes that led to this handler
 * @param addr The full OSC address
 * @param msg The OSC message containing arguments
 * @return 0 on success, non-zero on failure
 */
int getlogs(const struct oscnode *path[], const char *addr, struct oscmsg *msg)
{
    extern const char **log_get_recent(int max_messages);
    extern void oscsend(const char *addr, const char *type, ...);
    extern void oscflush(void);

    int max_logs = 10; // Default number of logs to return

    // Check if a count was provided in the message
    if (msg->argc > 0 && msg->argv[0].type == 'i')
    {
        max_logs = msg->argv[0].i;
        if (max_logs < 1)
            max_logs = 1;
        if (max_logs > 50)
            max_logs = 50; // Reasonable limit
    }

    // Get recent logs
    const char **logs = log_get_recent(max_logs);
    if (!logs)
    {
        oscsend("/logs/error", ",s", "Failed to retrieve logs");
        oscflush();
        return -1;
    }

    // Send logs as individual messages
    int count = 0;
    for (int i = 0; i < max_logs && logs[i]; i++)
    {
        char addr_buf[64];
        snprintf(addr_buf, sizeof(addr_buf), "/logs/entry/%d", i);
        oscsend(addr_buf, ",s", logs[i]);
        count++;
    }

    // Send the total count
    oscsend("/logs/count", ",i", count);
    oscflush();

    return 0;
}

/**
 * @brief Handle the /errors/last OSC message
 *
 * This function retrieves the last error and sends it via OSC.
 *
 * @param path The path of nodes that led to this handler
 * @param addr The full OSC address
 * @param msg The OSC message containing arguments
 * @return 0 on success, non-zero on failure
 */
int getlasterror(const struct oscnode *path[], const char *addr, struct oscmsg *msg)
{
    extern const char *get_last_error(int *code, char *context, size_t context_size);
    extern void oscsend(const char *addr, const char *type, ...);
    extern void oscflush(void);

    int error_code = 0;
    char context[128] = {0};
    const char *error_msg = get_last_error(&error_code, context, sizeof(context));

    if (!error_msg || !error_msg[0])
    {
        oscsend("/errors/last/code", ",i", 0);
        oscsend("/errors/last/context", ",s", "");
        oscsend("/errors/last/message", ",s", "No errors recorded");
    }
    else
    {
        oscsend("/errors/last/code", ",i", error_code);
        oscsend("/errors/last/context", ",s", context);
        oscsend("/errors/last/message", ",s", error_msg);
    }

    oscflush();
    return 0;
}

/* Additional string handling OSC setter/getter (will need implementation) */
int newstr(const struct oscnode *path[], const char *addr, int reg, const char *val)
{
    extern void oscsend(const char *addr, const char *type, ...);

    if (val)
    {
        oscsend(addr, ",s", val);
    }
    else
    {
        oscsend(addr, ",s", "");
    }

    return 0;
}
