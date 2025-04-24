#ifndef OSC_H
#define OSC_H

#include <stdint.h>

/* OSC argument types */
typedef enum oscargtype
{
	OSC_INT = 'i',
	OSC_FLOAT = 'f',
	OSC_STRING = 's',
	OSC_BLOB = 'b',
	OSC_TRUE = 'T',
	OSC_FALSE = 'F',
	OSC_NIL = 'N',
	OSC_INFINITUM = 'I'
} oscargtype;

/* OSC argument structure */
typedef struct oscarg
{
	oscargtype type;
	union
	{
		int32_t i;
		float f;
		const char *s;
		struct
		{
			uint32_t size;
			const void *data;
		} b;
	};
} oscarg;

/* OSC message structure */
struct oscmsg
{
	/* Raw buffer and parsing state */
	unsigned char *buf, *end;
	const char *type;
	const char *err;

	/* Parsed arguments */
	int argc;
	struct oscarg argv[32]; /* Support up to 32 arguments */
};

/* Initialize an OSC message from a buffer */
int oscinit(struct oscmsg *msg, void *buf, size_t size);

/* Parse an OSC message */
int oscparse(struct oscmsg *msg);

/* Finish parsing and check for errors */
int oscend(struct oscmsg *msg);

/* OSC getter functions for raw parsing */
int_least32_t oscgetint(struct oscmsg *msg);
char *oscgetstr(struct oscmsg *msg);
float oscgetfloat(struct oscmsg *msg);

/* OSC setter functions for message building */
void oscputstr(struct oscmsg *msg, const char *str);
void oscputint(struct oscmsg *msg, int_least32_t val);
void oscputfloat(struct oscmsg *msg, float val);

#endif
