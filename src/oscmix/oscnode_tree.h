/**
 * @file oscnode_tree.h
 * @brief OSC node tree structure and functions for mapping OSC addresses to parameters
 */

#ifndef OSCNODE_TREE_H
#define OSCNODE_TREE_H

#include "osc.h"

/**
 * @brief Represents a node in the OSC address tree
 *
 * Each node corresponds to a parameter or a group of parameters
 * in the device, and contains information about how to set or get
 * the parameter value.
 */
struct oscnode
{
    const char *name;
    int reg;
    int (*set)(const struct oscnode *path[], int reg, struct oscmsg *msg);
    int (*new)(const struct oscnode *path[], const char *addr, int reg, int val);
    union
    {
        struct
        {
            const char *const *const names;
            size_t nameslen;
        };
        struct
        {
            short min;
            short max;
            float scale;
        };
    };
    const struct oscnode *child;
};

// Root node tree definition
extern const struct oscnode tree[];

/**
 * @brief Initialize the OSC node tree
 *
 * @return 0 on success, non-zero on failure
 */
int oscnode_tree_init(void);

/**
 * @brief Find a node in the OSC tree by path components
 *
 * @param components Array of path components
 * @param ncomponents Number of components
 * @param path Output array for the path of nodes
 * @param npath Maximum size of the path array
 * @return Number of nodes in the path, or -1 if not found
 */
int oscnode_find(const char **components, int ncomponents,
                 const struct oscnode *path[], int npath);

/**
 * @brief Match a pattern against a string
 *
 * This function supports basic pattern matching used in OSC address pattern matching.
 *
 * @param pat The pattern string (may contain wildcards)
 * @param str The string to match against
 * @return Pointer to the remainder of the pattern if matched, NULL otherwise
 */
const char *match(const char *pat, const char *str);

#endif /* OSCNODE_TREE_H */
