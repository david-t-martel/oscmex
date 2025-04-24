#ifndef OSC_H
#define OSC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Maximum number of arguments in an OSC message */
#define OSC_MAX_ARGS 32

/* OSC message structure */
struct oscmsg
{
	unsigned char *buf; /* Current position in the buffer */
	unsigned char *end; /* End of the buffer */
	const char *type;	/* Type tag string being processed */
	const char *err;	/* Error message or NULL */
	int argc;			/* Number of arguments when parsing */
	struct
	{
		char type; /* Argument type: i, f, s, or b */
		union
		{
			int i;	 /* integer */
			float f; /* float */
			char *s; /* string */
			struct
			{
				void *data;	 /* blob data */
				size_t size; /* blob size */
			} b;			 /* blob */
		} i;				 /* Argument value */
		size_t size;		 /* Size for blobs */
	} argv[OSC_MAX_ARGS];	 /* Array of arguments */
};

/**
 * @brief Initialize the OSC subsystem
 *
 * @param port UDP port number to listen on
 * @return 0 on success, non-zero on failure
 */
int osc_init(int port);

/**
 * @brief Close the OSC subsystem
 */
void osc_close(void);

/**
 * @brief Process incoming OSC messages
 *
 * @return Number of messages processed
 */
int osc_process(void);

/**
 * @brief Initialize an OSC message structure
 *
 * @param msg Pointer to the OSC message structure
 * @param buf Buffer to use for the message
 * @param size Size of the buffer
 * @return 0 on success, non-zero on failure
 */
int oscinit(struct oscmsg *msg, void *buf, size_t size);

/**
 * @brief Parse an OSC message from the buffer
 *
 * @param msg Pointer to the OSC message structure
 * @return 0 on success, non-zero on failure
 */
int oscparse(struct oscmsg *msg);

/**
 * @brief Check if the OSC message is complete
 *
 * @param msg Pointer to the OSC message structure
 * @return 0 if message is complete, non-zero otherwise
 */
int oscend(struct oscmsg *msg);

/**
 * @brief Get an integer from the OSC message
 *
 * @param msg Pointer to the OSC message structure
 * @return The integer value
 */
int_least32_t oscgetint(struct oscmsg *msg);

/**
 * @brief Get a string from the OSC message
 *
 * @param msg Pointer to the OSC message structure
 * @return The string value
 */
char *oscgetstr(struct oscmsg *msg);

/**
 * @brief Get a float from the OSC message
 *
 * @param msg Pointer to the OSC message structure
 * @return The float value
 */
float oscgetfloat(struct oscmsg *msg);

/**
 * @brief Add a string to the OSC message
 *
 * @param msg Pointer to the OSC message structure
 * @param str The string to add
 */
void oscputstr(struct oscmsg *msg, const char *str);

/**
 * @brief Add an integer to the OSC message
 *
 * @param msg Pointer to the OSC message structure
 * @param val The integer value to add
 */
void oscputint(struct oscmsg *msg, int_least32_t val);

/**
 * @brief Add a float to the OSC message
 *
 * @param msg Pointer to the OSC message structure
 * @param val The float value to add
 */
void oscputfloat(struct oscmsg *msg, float val);

#endif /* OSC_H */
