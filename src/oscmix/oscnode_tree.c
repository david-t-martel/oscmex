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

/* This is where we would paste the entire OSC node tree declaration from oscmix.c */
/* For brevity, I'll include a small example of what this would look like */

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
    {NULL}};

/* More nodes would be defined here - inputs, outputs, mixer, etc. */

static const struct oscnode root_nodes[] = {
    {"system", 0, NULL, NULL, {}, &system_nodes[0]},
    /* Other top-level nodes would be here */
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
