#define _XOPEN_SOURCE 700
#include <assert.h>
#include <string.h>
#include "osc.h"
#include "intpack.h"

int oscinit(struct oscmsg *msg, void *buf, size_t size)
{
	if (!msg || !buf || size == 0)
		return -1;

	msg->buf = buf;
	msg->end = (unsigned char *)buf + size;
	msg->type = NULL;
	msg->err = NULL;
	msg->argc = 0;

	return 0;
}

int oscparse(struct oscmsg *msg)
{
	const char *addr;
	const char *types;
	unsigned char *pos;
	int i;

	if (!msg || !msg->buf || msg->buf >= msg->end)
		return -1;

	/* Get the OSC address pattern */
	addr = (const char *)msg->buf;
	pos = msg->buf + ((strlen(addr) + 4) & ~3); /* Align to 4 bytes */

	if (pos >= msg->end)
	{
		msg->err = "Buffer too small for address";
		return -1;
	}

	/* Get the type tag string */
	if (*pos == ',')
	{
		types = (const char *)pos;
		pos += ((strlen(types) + 4) & ~3); /* Align to 4 bytes */

		if (pos >= msg->end)
		{
			msg->err = "Buffer too small for type tags";
			return -1;
		}

		/* Parse arguments based on type tags */
		msg->argc = strlen(types) - 1;
		if (msg->argc > 32)
		{
			msg->err = "Too many arguments";
			return -1;
		}

		for (i = 0; i < msg->argc; i++)
		{
			msg->argv[i].type = types[i + 1];

			switch (types[i + 1])
			{
			case 'i': /* int32 */
				if (pos + 4 > msg->end)
				{
					msg->err = "Buffer too small for int32";
					return -1;
				}
				msg->argv[i].i = getbe32(pos);
				pos += 4;
				break;

			case 'f': /* float32 */
				if (pos + 4 > msg->end)
				{
					msg->err = "Buffer too small for float32";
					return -1;
				}
				{
					union
					{
						uint32_t u;
						float f;
					} u;
					u.u = getbe32(pos);
					msg->argv[i].f = u.f;
				}
				pos += 4;
				break;

			case 's': /* string */
				msg->argv[i].s = (const char *)pos;
				pos += ((strlen(msg->argv[i].s) + 4) & ~3); /* Align to 4 bytes */
				break;

			case 'b': /* blob */
				if (pos + 4 > msg->end)
				{
					msg->err = "Buffer too small for blob size";
					return -1;
				}
				msg->argv[i].b.size = getbe32(pos);
				pos += 4;

				if (pos + msg->argv[i].b.size > msg->end)
				{
					msg->err = "Buffer too small for blob data";
					return -1;
				}
				msg->argv[i].b.data = pos;
				pos += ((msg->argv[i].b.size + 3) & ~3); /* Align to 4 bytes */
				break;

			case 'T': /* True */
			case 'F': /* False */
			case 'N': /* Nil */
			case 'I': /* Infinitum */
				/* These types don't have data */
				break;

			default:
				msg->err = "Unknown type tag";
				return -1;
			}

			if (pos > msg->end)
			{
				msg->err = "Buffer overflow during argument parsing";
				return -1;
			}
		}

		/* Update the buffer position */
		msg->buf = pos;
	}
	else
	{
		/* No type tag string, treat as raw data */
		msg->argc = 0;
	}

	return 0;
}

int oscend(struct oscmsg *msg)
{
	if (!msg->err)
	{
		if (*msg->type != '\0')
			msg->err = "extra arguments";
		else if (msg->buf != msg->end)
			msg->err = "extra argument data";
		else
			return 0;
	}
	return -1;
}

int_least32_t
oscgetint(struct oscmsg *msg)
{
	int_least32_t val;

	switch (msg->type ? *msg->type : 'i')
	{
	case 'i':
		if (msg->end - msg->buf < 4)
		{
			msg->err = "missing argument data";
			return 0;
		}
		val = getbe32(msg->buf), msg->buf += 4;
		break;
	case 'T':
		val = 1;
		break;
	case 'F':
		val = 0;
		break;
	case '\0':
		msg->err = "not enough arguments";
		return 0;
	default:
		msg->err = "incorrect argument type";
		return 0;
	}
	if (msg->type)
		++msg->type;
	return val;
}

char *
oscgetstr(struct oscmsg *msg)
{
	char *val;
	size_t len;

	switch (msg->type ? *msg->type : 's')
	{
	case 's':
		val = (char *)msg->buf;
		len = strnlen(val, msg->end - msg->buf);
		if (len == msg->end - msg->buf)
		{
			msg->err = "string is not nul-terminated";
			return NULL;
		}
		assert((msg->end - msg->buf) % 4 == 0);
		msg->buf += (len + 4) & -4;
		break;
	case 'N':
		val = "";
		break;
	case '\0':
		msg->err = "not enough arguments";
		return NULL;
	default:
		msg->err = "incorrect argument type";
		return NULL;
	}
	if (msg->type)
		++msg->type;
	return val;
}

float oscgetfloat(struct oscmsg *msg)
{
	union
	{
		uint32_t i;
		float f;
	} val;

	switch (*msg->type)
	{
	case 'i':
		if (msg->end - msg->buf < 4)
		{
			msg->err = "missing argument data";
			return 0;
		}
		val.f = getbe32(msg->buf), msg->buf += 4;
		break;
	case 'f':
		if (msg->end - msg->buf < 4)
		{
			msg->err = "missing argument data";
			return 0;
		}
		val.i = getbe32(msg->buf), msg->buf += 4;
		break;
	case '\0':
		msg->err = "not enough arguments";
		return 0;
	default:
		msg->err = "incorrect argument type";
		return 0;
	}
	++msg->type;
	return val.f;
}

void oscputstr(struct oscmsg *msg, const char *str)
{
	unsigned char *pos;
	int pad;

	if (msg->type)
	{
		assert(*msg->type == 's');
		++msg->type;
	}
	pos = msg->buf;
	pos = memccpy(pos, str, '\0', msg->end - pos);
	if (!pos)
	{
		msg->err = "string too large";
		return;
	}
	pad = 3 - ((pos - msg->buf) + 3) % 4;
	if (pad > msg->end - pos)
	{
		msg->err = "string too large";
		return;
	}
	memset(pos, 0, pad), pos += pad;
	msg->buf = pos;
}

void oscputint(struct oscmsg *msg, int_least32_t val)
{
	unsigned char *pos;

	if (msg->type)
	{
		assert(*msg->type == 'i');
		++msg->type;
	}
	pos = msg->buf;
	if (msg->end - pos < 4)
	{
		msg->err = "buffer too small";
		return;
	}
	putbe32(pos, val);
	msg->buf = pos + 4;
}

void oscputfloat(struct oscmsg *msg, float val)
{
	unsigned char *pos;
	union
	{
		float f32;
		uint32_t u32;
	} u;

	if (msg->type)
	{
		assert(*msg->type == 'f');
		++msg->type;
	}
	pos = msg->buf;
	if (msg->end - pos < 4)
	{
		msg->err = "buffer too small";
		return;
	}
	u.f32 = val;
	putbe32(pos, u.u32);
	msg->buf = pos + 4;
}
