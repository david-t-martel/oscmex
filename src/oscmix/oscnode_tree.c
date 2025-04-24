/**
 * @file oscnode_tree.c
 * @brief Implementation of the OSC node tree structure and functions
 */

#include "oscnode_tree.h"
#include "oscmix_commands.h"
#include "oscmix_midi.h"
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

/* Forward declarations of getter functions */
int newint(const struct oscnode *path[], const char *addr, int reg, int val);
int newfixed(const struct oscnode *path[], const char *addr, int reg, int val);
int newenum(const struct oscnode *path[], const char *addr, int reg, int val);
int newbool(const struct oscnode *path[], const char *addr, int reg, int val);

/* OSC node tree definition */

/* System-level nodes */
static const struct oscnode refresh_nodes[] = {
    {"", 0x8000, setrefresh, NULL, {}, NULL},
    {NULL}};

static const struct oscnode samplerate_nodes[] = {
    {"44100", 0, NULL, newint, {}, NULL},
    {"48000", 0, NULL, newint, {}, NULL},
    {"88200", 0, NULL, newint, {}, NULL},
    {"96000", 0, NULL, newint, {}, NULL},
    {NULL}};

static const struct oscnode system_nodes[] = {
    {"samplerate", 0x8000, setint, newint, {.names = (const char *const[]){"44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz"}, .nameslen = 4}, &samplerate_nodes[0]},
    {"clocksource", 0x8002, setenum, newenum, {.names = (const char *const[]){"Internal", "AES", "ADAT", "Sync In"}, .nameslen = 4}, NULL},
    {"buffersize", 0x8004, setenum, newenum, {.names = (const char *const[]){"32", "64", "128", "256", "512", "1024"}, .nameslen = 6}, NULL},
    {"phantompower", 0x8006, setbool, newbool, {}, NULL},
    {"mastervol", 0x8008, setfixed, newfixed, {.min = 0, .max = 65535, .scale = 65535.0f}, NULL},
    {"mastermute", 0x800A, setbool, newbool, {}, NULL},
    {"digitalgain", 0x800C, setfixed, newfixed, {.min = 0, .max = 65535, .scale = 65535.0f}, NULL},
    {NULL}};

/* Input channel nodes */
static const struct oscnode input_channel_nodes[] = {
    {"gain", 0x0100, setfixed, newfixed, {.min = 0, .max = 65535, .scale = 65535.0f}, NULL},
    {"phantom", 0x0102, setbool, newbool, {}, NULL},
    {"pad", 0x0104, setbool, newbool, {}, NULL},
    {"reflevel", 0x0106, setenum, newenum, {.names = (const char *const[]){"-10 dBV", "+4 dBu", "HiZ"}, .nameslen = 3}, NULL},
    {"mute", 0x0108, setbool, newbool, {}, NULL},
    {"hiz", 0x010A, setbool, newbool, {}, NULL},
    {"aeb", 0x010C, setbool, newbool, {}, NULL},
    {"locut", 0x010E, setbool, newbool, {}, NULL},
    {"ms", 0x0110, setbool, newbool, {}, NULL},
    {"autoset", 0x0112, setbool, newbool, {}, NULL},
    {NULL}};

static const struct oscnode input_nodes[] = {
    {"*", 0, NULL, NULL, {}, &input_channel_nodes[0]},
    {NULL}};

/* Output channel nodes */
static const struct oscnode output_channel_nodes[] = {
    {"volume", 0x0200, setfixed, newfixed, {.min = 0, .max = 65535, .scale = 65535.0f}, NULL},
    {"mute", 0x0202, setbool, newbool, {}, NULL},
    {"reflevel", 0x0204, setenum, newenum, {.names = (const char *const[]){"-10 dBV", "+4 dBu", "HiZ"}, .nameslen = 3}, NULL},
    {"dither", 0x0206, setenum, newenum, {.names = (const char *const[]){"Off", "16 bit", "20 bit"}, .nameslen = 3}, NULL},
    {"phase", 0x0208, setbool, newbool, {}, NULL},
    {"mono", 0x020A, setbool, newbool, {}, NULL},
    {"loopback", 0x020C, setbool, newbool, {}, NULL},
    {NULL}};

static const struct oscnode output_nodes[] = {
    {"*", 0, NULL, NULL, {}, &output_channel_nodes[0]},
    {NULL}};

/* Playback (DAW) channel nodes */
static const struct oscnode playback_channel_nodes[] = {
    {"volume", 0x0300, setfixed, newfixed, {.min = 0, .max = 65535, .scale = 65535.0f}, NULL},
    {"mute", 0x0302, setbool, newbool, {}, NULL},
    {"phase", 0x0304, setbool, newbool, {}, NULL},
    {NULL}};

static const struct oscnode playback_nodes[] = {
    {"*", 0, NULL, NULL, {}, &playback_channel_nodes[0]},
    {NULL}};

/* Mixer matrix nodes */
static const struct oscnode mixer_channel_nodes[] = {
    {"volume", 0x0400, setfixed, newfixed, {.min = 0, .max = 65535, .scale = 65535.0f}, NULL},
    {"pan", 0x0402, setfixed, newfixed, {.min = 0, .max = 65535, .scale = 65535.0f}, NULL},
    {"mute", 0x0404, setbool, newbool, {}, NULL},
    {"solo", 0x0406, setbool, newbool, {}, NULL},
    {"phase", 0x0408, setbool, newbool, {}, NULL},
    {NULL}};

static const struct oscnode mixer_dest_nodes[] = {
    {"*", 0, NULL, NULL, {}, &mixer_channel_nodes[0]},
    {NULL}};

static const struct oscnode mixer_source_nodes[] = {
    {"*", 0, NULL, NULL, {}, &mixer_dest_nodes[0]},
    {NULL}};

static const struct oscnode mixer_nodes[] = {
    {"input", 0, NULL, NULL, {}, &mixer_source_nodes[0]},
    {"playback", 0, NULL, NULL, {}, &mixer_source_nodes[0]},
    {NULL}};

/* TotalMix nodes */
static const struct oscnode totalmix_snapshot_nodes[] = {
    {"load", 0x7000, setint, NULL, {.min = 1, .max = 8, .scale = 1.0f}, NULL},
    {"save", 0x7002, setint, NULL, {.min = 1, .max = 8, .scale = 1.0f}, NULL},
    {NULL}};

static const struct oscnode totalmix_nodes[] = {
    {"snapshot", 0, NULL, NULL, {}, &totalmix_snapshot_nodes[0]},
    {"clearall", 0x7004, setbool, NULL, {}, NULL},
    {NULL}};

/* Root nodes */
static const struct oscnode root_nodes[] = {
    {"system", 0, NULL, NULL, {}, &system_nodes[0]},
    {"input", 0, NULL, NULL, {}, &input_nodes[0]},
    {"output", 0, NULL, NULL, {}, &output_nodes[0]},
    {"playback", 0, NULL, NULL, {}, &playback_nodes[0]},
    {"mixer", 0, NULL, NULL, {}, &mixer_nodes[0]},
    {"totalmix", 0, NULL, NULL, {}, &totalmix_nodes[0]},
    {"refresh", 0, NULL, NULL, {}, &refresh_nodes[0]},
    {NULL}};

// Make the root node accessible via the header
const struct oscnode tree[] = {
    {"", 0, NULL, NULL, {}, &root_nodes[0]},
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
