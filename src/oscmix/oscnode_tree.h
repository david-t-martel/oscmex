/**
 * @file oscnode_tree.h
 * @brief OSC node tree structure and functions for mapping OSC addresses to parameters
 */

#ifndef OSCNODE_TREE_H
#define OSCNODE_TREE_H

#include "osc.h"
#include <stddef.h> // For size_t

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
            const char *const *names;
            size_t nameslen;
        } names;
        struct
        {
            short min;
            short max;
            float scale;
        } range;
        int data[3]; /* Generic data for other node types */
    } data;
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
int oscstatus(const struct oscnode *path[], const char *addr, struct oscmsg *msg);

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
int getlogs(const struct oscnode *path[], const char *addr, struct oscmsg *msg);

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
int getlasterror(const struct oscnode *path[], const char *addr, struct oscmsg *msg);

/**
 * @brief OSC string handler for sending string values
 *
 * @param path The path of nodes that led to this handler
 * @param addr The full OSC address
 * @param reg The register address (unused for strings)
 * @param val The string value to send
 * @return 0 on success, non-zero on failure
 */
int newstr(const struct oscnode *path[], const char *addr, int reg, const char *val);

#endif /* OSCNODE_TREE_H */
