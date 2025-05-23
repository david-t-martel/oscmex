/**
 * @file oscmix.c
 * @brief Core implementation for OSCMix - a bridge between OSC messages and RME audio devices
 *
 * This module implements the main functionality of the OSCMix application, which
 * translates between OSC network messages and MIDI SysEx commands for RME audio interfaces.
 *
 * The implementation follows these key components:
 * 1. Parameter definitions and data structures for device state tracking
 * 2. OSC message parsing and dispatching
 * 3. Device parameter manipulation
 * 4. MIDI SysEx message handling
 * 5. OSC message generation for notifications/responses
 */

#define _XOPEN_SOURCE 700 /* for memccpy */
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"
#include "intpack.h"
#include "oscmix.h"
#include "osc.h"
#include "sysex.h"
#include "util.h"
#include "dump.h"

#define LEN(a) (sizeof(a) / sizeof *(a))
#define PI 3.14159265358979323846

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

/**
 * @brief Represents a mix configuration for an output channel
 *
 * Contains information about the pan and volume settings for
 * each output channel.
 */
struct mix
{
	signed char pan;
	short vol;
};

/**
 * @brief Represents an input channel configuration
 *
 * Contains information about the stereo, mute, and width settings
 * for each input channel.
 */
struct input
{
	bool stereo;
	bool mute;
	float width;
};

/**
 * @brief Represents an output channel configuration
 *
 * Contains information about the stereo setting and mix configuration
 * for each output channel.
 */
struct output
{
	bool stereo;
	struct mix *mix;
};

/**
 * @brief Represents a durec file configuration
 *
 * Contains information about the durec file, including register values,
 * name, sample rate, channels, and length.
 */
struct durecfile
{
	short reg[6];
	char name[9];
	unsigned long samplerate;
	unsigned channels;
	unsigned length;
};

/**
 * @brief Debug flag for controlling verbosity
 * Set by the -d command line option in main.c
 */
int dflag;
static const struct device *cur_device;
static struct input *inputs;
static struct input *playbacks;
static struct output *outputs;
static struct
{
	int status;
	int position;
	int time;
	int usberrors;
	int usbload;
	float totalspace;
	float freespace;
	struct durecfile *files;
	size_t fileslen;
	int file;
	int recordtime;
	int index;
	int next;
	int playmode;
} durec = {.index = -1};
static struct
{
	int vers;
	int load;
} dsp;
static bool refreshing;

static void oscsend(const char *addr, const char *type, ...);
static void oscflush(void);
static void oscsendenum(const char *addr, int val, const char *const names[], size_t nameslen);

/**
 * @brief Dumps the contents of a buffer to stdout
 *
 * @param name The name of the buffer
 * @param ptr The pointer to the buffer
 * @param len The length of the buffer
 */
static void
dump(const char *name, const void *ptr, size_t len)
{
	size_t i;
	const unsigned char *buf;

	buf = ptr;
	if (name)
		fputs(name, stdout);
	if (len > 0)
	{
		if (name)
			putchar('\t');
		printf("%.2x", buf[0]);
	}
	for (i = 1; i < len; ++i)
		printf(" %.2x", buf[i]);
	putchar('\n');
}

/**
 * @brief Writes a SysEx message to the device
 *
 * @param subid The sub ID of the SysEx message
 * @param buf The buffer containing the SysEx message data
 * @param len The length of the buffer
 * @param sysexbuf The buffer to store the encoded SysEx message
 */
static void
writesysex(int subid, const unsigned char *buf, size_t len, unsigned char *sysexbuf)
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
static int
setreg(unsigned reg, unsigned val)
{
	unsigned long regval;
	unsigned char buf[4], sysexbuf[7 + 5];
	unsigned par;

	if (dflag && reg != 0x3f00)
		fprintf(stderr, "setreg %#.4x %#.4x\n", reg, val);
	regval = (reg & 0x7fff) << 16 | (val & 0xffff);
	par = regval >> 16 ^ regval;
	par ^= par >> 8;
	par ^= par >> 4;
	par ^= par >> 2;
	par ^= par >> 1;
	regval |= (~par & 1) << 31;
	putle32(buf, regval);

	writesysex(0, buf, sizeof buf, sysexbuf);
	return 0;
}

/**
 * @brief Sets an integer parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setint(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	int_least32_t val;

	val = oscgetint(msg);
	if (oscend(msg) != 0)
		return -1;
	setreg(reg, val);
	return 0;
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
static int
newint(const struct oscnode *path[], const char *addr, int reg, int val)
{
	oscsend(addr, ",i", val);
	return 0;
}

/**
 * @brief Sets a fixed-point parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setfixed(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	const struct oscnode *node;
	float val;

	node = *path;
	val = oscgetfloat(msg);
	if (oscend(msg) != 0)
		return -1;
	setreg(reg, val / node->scale);
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
static int
newfixed(const struct oscnode *path[], const char *addr, int reg, int val)
{
	const struct oscnode *node;

	node = *path;
	oscsend(addr, ",f", val * node->scale);
	return 0;
}

/**
 * @brief Sets an enumerated parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setenum(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	const struct oscnode *node;
	const char *str;
	int val;

	node = *path;
	switch (*msg->type)
	{
	case 's':
		str = oscgetstr(msg);
		if (str)
		{
			for (val = 0; val < node->nameslen; ++val)
			{
				if (stricmp(str, node->names[val]) == 0)
					break;
			}
			if (val == node->nameslen)
				return -1;
		}
		break;
	default:
		val = oscgetint(msg);
		break;
	}
	if (oscend(msg) != 0)
		return -1;
	setreg(reg, val);
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
static int
newenum(const struct oscnode *path[], const char *addr, int reg, int val)
{
	const struct oscnode *node;

	node = *path;
	oscsendenum(addr, val, node->names, node->nameslen);
	return 0;
}

/**
 * @brief Sets a boolean parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setbool(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	bool val;

	val = oscgetint(msg);
	if (oscend(msg) != 0)
		return -1;
	setreg(reg, val);
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
static int
newbool(const struct oscnode *path[], const char *addr, int reg, int val)
{
	oscsend(addr, ",i", val != 0);
	return 0;
}

/**
 * @brief Sets an audio level parameter value in the device
 *
 * @param reg The register address
 * @param level The audio level value
 * @return 0 on success, non-zero on failure
 */
static int
setlevel(int reg, float level)
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
 * @brief Sets the audio levels for an output channel
 *
 * @param out The output channel
 * @param in The input channel
 * @param mix The mix configuration
 */
static void
setlevels(struct output *out, struct input *in, struct mix *mix)
{
	int reg;
	float level, theta;

	reg = 0x4000 | (out - outputs) << 6 | (in - inputs);
	level = in->mute ? 0 : mix->vol <= -650 ? 0
											: powf(10, mix->vol / 200.f);
	if (out->stereo)
	{
		theta = (mix->pan + 100) / 400.f * PI;
		setlevel(reg, level * cosf(theta));
		setlevel(reg + 0x40, level * sinf(theta));
	}
	else
	{
		setlevel(reg, level);
	}
}

/**
 * @brief Sets the mute state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setinputmute(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	struct input *in;
	struct output *out;
	struct mix *mix;
	int inidx, outidx;
	bool val;

	val = oscgetint(msg);
	if (oscend(msg) != 0)
		return -1;
	inidx = path[-1] - path[-2]->child;
	assert(inidx < cur_device->inputslen);
	/* mutex */
	in = &inputs[inidx];
	if (inidx % 2 == 1 && in[-1].stereo)
		--in, --inidx;
	setreg(reg, val);
	if (in->mute != val)
	{
		in->mute = val;
		if (in->stereo)
			in[1].mute = val;
		for (outidx = 0; outidx < cur_device->outputslen; ++outidx)
		{
			out = &outputs[outidx];
			mix = &out->mix[inidx];
			if (mix->vol > -650)
				setlevels(out, in, mix);
			if (in->stereo && (++mix)->vol > -650)
				setlevels(out, in + 1, mix);
			if (out->stereo)
			{
				assert(outidx % 2 == 0);
				++outidx;
			}
		}
	}
	return 0;
}

/**
 * @brief Sets the stereo state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setinputstereo(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	int idx;
	bool val;

	val = oscgetint(msg);
	if (oscend(msg) != 0)
		return -1;
	idx = (path[-1] - path[-2]->child) & -2;
	assert(idx < cur_device->inputslen);
	inputs[idx].stereo = val;
	inputs[idx + 1].stereo = val;
	setreg(idx << 6 | 2, val);
	setreg((idx + 1) << 6 | 2, val);
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
static int
newinputstereo(const struct oscnode *path[], const char *addr, int reg, int val)
{
	int idx;
	char addrbuf[256];

	idx = (path[-1] - path[-2]->child) & -2;
	assert(idx < cur_device->inputslen);
	inputs[idx].stereo = val;
	inputs[idx + 1].stereo = val;
	addr = addrbuf;
	snprintf(addrbuf, sizeof addrbuf, "/input/%d/stereo", idx + 1);
	oscsend(addr, ",i", val != 0);
	snprintf(addrbuf, sizeof addrbuf, "/input/%d/stereo", idx + 2);
	oscsend(addr, ",i", val != 0);
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
static int
newoutputstereo(const struct oscnode *path[], const char *addr, int reg, int val)
{
	int idx;
	char addrbuf[256];

	idx = (path[-1] - path[-2]->child) & -2;
	assert(idx < cur_device->outputslen);
	outputs[idx].stereo = val;
	outputs[idx + 1].stereo = val;
	addr = addrbuf;
	snprintf(addrbuf, sizeof addrbuf, "/output/%d/stereo", idx + 1);
	oscsend(addr, ",i", val != 0);
	snprintf(addrbuf, sizeof addrbuf, "/output/%d/stereo", idx + 2);
	oscsend(addr, ",i", val != 0);
	return 0;
}

/**
 * @brief Sets the name for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setinputname(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	const char *name;
	char namebuf[12];
	int i, ch, val;

	ch = path[-1] - path[-2]->child;
	if (ch >= 20)
		return -1;
	name = oscgetstr(msg);
	if (oscend(msg) != 0)
		return -1;
	strncpy(namebuf, name, sizeof namebuf);
	namebuf[sizeof namebuf - 1] = '\0';
	reg = 0x3200 + ch * 8;
	for (i = 0; i < sizeof namebuf; i += 2, ++reg)
	{
		val = getle16(namebuf + i);
		setreg(reg, val);
	}
	return 0;
}

/**
 * @brief Sets the gain for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setinputgain(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	float val;
	bool mic;

	val = oscgetfloat(msg);
	if (oscend(msg) != 0)
		return -1;
	mic = (path[-1] - path[-2]->child) <= 1;
	if (val < 0 || val > 75 || (!mic && val > 24))
		return -1;
	setreg(reg, val * 10);
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
static int
newinputgain(const struct oscnode *path[], const char *addr, int reg, int val)
{
	oscsend(addr, ",f", val / 10.0);
	return 0;
}

/**
 * @brief Sets the 48V phantom power state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setinput48v(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	int idx;

	idx = path[-1] - path[-2]->child;
	assert(idx < cur_device->inputslen);
	if (cur_device->inputs[idx].flags & INPUT_48V)
		return setbool(path, reg, msg);
	return -1;
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
static int
newinput48v_reflevel(const struct oscnode *path[], const char *addr, int reg, int val)
{
	static const char *const names[] = {"+7dBu", "+13dBu", "+19dBu"};
	int idx;
	const struct inputinfo *info;

	idx = path[-1] - path[-2]->child;
	assert(idx < cur_device->inputslen);
	info = &cur_device->inputs[idx];
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
 * @brief Sets the Hi-Z state for an input channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setinputhiz(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	int idx;

	idx = path[-1] - path[-2]->child;
	assert(idx < cur_device->inputslen);
	if (cur_device->inputs[idx].flags & INPUT_HIZ)
		return setbool(path, reg, msg);
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
static int
newinputhiz(const struct oscnode *path[], const char *addr, int reg, int val)
{
	int idx;

	idx = path[-1] - path[-2]->child;
	assert(idx < cur_device->inputslen);
	if (cur_device->inputs[idx].flags & INPUT_HIZ)
		return newbool(path, addr, reg, val);
	return -1;
}

/**
 * @brief Sets the loopback state for an output channel
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setoutputloopback(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	bool val;
	unsigned char buf[4], sysexbuf[7 + 5];
	int idx;

	val = oscgetint(msg);
	if (oscend(msg) != 0)
		return -1;
	idx = path[-1] - path[-2]->child;
	if (val)
		idx |= 0x80;
	putle32(buf, idx);
	writesysex(3, buf, sizeof buf, sysexbuf);
	return 0;
}

/**
 * @brief Sets the EQD record state for the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
seteqdrecord(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	bool val;
	unsigned char buf[4], sysexbuf[7 + 5];

	val = oscgetint(msg);
	if (oscend(msg) != 0)
		return -1;
	putle32(buf, val);
	writesysex(4, buf, sizeof buf, sysexbuf);
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
static int
newdspload(const struct oscnode *path[], const char *addr, int reg, int val)
{
	if (dsp.load != (val & 0xff))
	{
		dsp.load = val & 0xff;
		oscsend("/hardware/dspload", ",i", dsp.load);
	}
	if (dsp.vers != val >> 8)
	{
		dsp.vers = val >> 8;
		oscsend("/hardware/dspvers", ",i", dsp.vers);
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
static int
newdspavail(const struct oscnode *path[], const char *addr, int reg, int val)
{
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
static int
newdspactive(const struct oscnode *path[], const char *addr, int reg, int val)
{
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
static int
newarcencoder(const struct oscnode *path[], const char *addr, int reg, int val)
{
	return 0;
}

/**
 * @brief Sets a dB value in the device
 *
 * @param reg The register address
 * @param db The dB value
 * @return 0 on success, non-zero on failure
 */
static int
setdb(int reg, float db)
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
static int
setpan(int reg, int pan)
{
	int val;

	val = (pan & 0x7fff) | 0x8000;
	return setreg(reg, val);
}

/**
 * @brief Sets a mix parameter value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setmix(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	int outidx, inidx, pan;
	float vol, level, theta, width;
	struct output *out;
	struct input *in;

	outidx = path[-2] - path[-3]->child;
	assert(outidx < cur_device->outputslen);
	out = &outputs[outidx];

	inidx = path[0] - path[-1]->child;
	if (reg & 0x20)
	{
		assert(inidx < cur_device->outputslen);
		in = &playbacks[inidx];
	}
	else
	{
		assert(inidx < cur_device->inputslen);
		in = &inputs[inidx];
	}

	vol = oscgetfloat(msg);
	if (vol <= -65)
		vol = -INFINITY;
	printf("setmix %d %d %f\n", (reg >> 6) & 0x3f, reg & 0x3f, vol);

	pan = 0;
	width = 1;
	if (*msg->type)
	{
		pan = oscgetint(msg);
		if (*msg->type && in->stereo && out->stereo)
			width = oscgetfloat(msg);
	}
	if (oscend(msg) != 0)
		return -1;

	level = pow(10, vol / 20);
	if (in->stereo)
	{
		float level0, level1, level00, level10, level01, level11;

		level0 = (100 - (pan > 0 ? pan : 0)) / 200.f * level;
		level1 = (100 + (pan < 0 ? pan : 0)) / 200.f * level;
		if (out->stereo)
		{
			level00 = level0 * (1 + width);
			level10 = level0 * (1 - width);
			level01 = level1 * (1 - width);
			level11 = level1 * (1 + width);
			setlevel(reg + 0x2000, level00);
			setlevel(reg + 0x2001, level10);
			setlevel(reg + 0x2040, level01);
			setlevel(reg + 0x2041, level11);

			level00 = level00 * level00;
			level0 = level00 + level01 * level01;
			/*
			L0 = level0^2 * (1 + width)^2 + level1^2 * (1 - width)^2
			L1 = level0^2 * (1 - width)^2 + level1^2 * (1 + width)^2
			*/
			setdb(reg, 10 * log10(level0));
			setpan(reg, acos(2 * level00 / level0 - 1) * 200 / PI - 100);

			level10 = level10 * level10;
			level1 = level10 + level11 * level11;
			setdb(reg + 1, 10 * log10(level1));
			setpan(reg + 1, acos(2 * level10 / level1 - 1) * 200 / PI - 100);
		}
		else
		{
			setlevel(reg + 0x2000, level0);
			setlevel(reg + 0x2001, level1);
			setdb(reg, 20 * log10(level0));
			setpan(reg, 0);
			setdb(reg + 1, 20 * log10(level1));
			setpan(reg + 1, 0);
		}
	}
	else
	{
		if (out->stereo)
		{
			theta = (pan + 100) * PI / 400;
			setlevel(reg + 0x2000, level * cos(theta));
			setlevel(reg + 0x2040, level * sin(theta));
		}
		else
		{
			setlevel(reg + 0x2000, level);
		}
		setdb(reg, vol);
		setpan(reg, pan);
	}
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
static int
newmix(const struct oscnode *path[], const char *addr, int reg, int val)
{
	struct output *out;
	struct input *in;
	struct mix *mix;
	int outidx, inidx;
	bool newpan;
	char addrbuf[256];
	float vol;
	int pan;

	outidx = (reg & 0xfff) >> 6;
	inidx = reg & 0x3f;
	if (outidx >= cur_device->outputslen || inidx >= cur_device->inputslen)
		return -1;
	out = &outputs[outidx];
	in = &inputs[inidx];
	mix = &out->mix[inidx];
	newpan = val & 0x8000;
	val = ((val & 0x7fff) ^ 0x4000) - 0x4000;
	if (newpan)
		mix->pan = val;
	else
		mix->vol = val;
	if (outidx & 1 && out[-1].stereo)
		--out, --outidx;
	if (inidx & 1 && in[-1].stereo)
		--in, --inidx;
	mix = &out->mix[inidx];
	if (in->stereo)
	{
		float level0, level1, scale;

		level0 = mix[0].vol <= -650 ? 0 : powf(10, mix[0].vol / 200.f);
		level1 = mix[1].vol <= -650 ? 0 : powf(10, mix[1].vol / 200.f);
		if (out->stereo)
		{
			// scale = sqrtf(2.f / (1 + in->width * in->width));
			scale = 1;
		}
		else
		{
			scale = 2;
		}
		level0 *= scale;
		level1 *= scale;
		if (level0 == 0 && level1 == 0)
		{
			vol = -INFINITY;
			pan = 0;
		}
		else if (level0 >= level1)
		{
			vol = 20 * log10f(level0);
			pan = 100 * (level1 / level0 - 1);
		}
		else
		{
			vol = 20 * log10f(level1);
			pan = -100 * (level0 / level1 - 1);
		}
	}
	else
	{
		vol = mix->vol <= -650 ? -65.f : mix->vol / 10.f;
		pan = mix->pan;
	}
	snprintf(addrbuf, sizeof addrbuf, "/mix/%d/input/%d", outidx + 1, inidx + 1);
	oscsend(addrbuf, ",f", vol);
	snprintf(addrbuf, sizeof addrbuf, "/mix/%d/input/%d/pan", outidx + 1, inidx + 1);
	oscsend(addrbuf, ",i", pan);
	return 0;
}

/**
 * @brief Gets the sample rate corresponding to a value
 *
 * @param val The value representing the sample rate
 * @return The sample rate in Hz
 */
static long
getsamplerate(int val)
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
	return val > 0 && val < LEN(samplerate) ? samplerate[val] : 0;
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
static int
newsamplerate(const struct oscnode *path[], const char *addr, int reg, int val)
{
	uint_least32_t rate;

	rate = getsamplerate(val);
	if (rate != 0)
		oscsend(addr, ",i", rate);
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
static int
newdynlevel(const struct oscnode *path[], const char *unused, int reg, int val)
{
	/*
	char addr[256];
	int ch;

	ch = (reg - 0x3180) * 2;
	if (ch < 20) {
		snprintf(addr, sizeof addr, "/input/%d/dynamics/level", ch + 1);
		oscsend(addr, ",i", val >> 8 & 0xff);
		snprintf(addr, sizeof addr, "/input/%d/dynamics/level", ch + 2);
		oscsend(addr, ",i", val & 0xff);
	} else if (ch < 40) {
		ch -= 20;
		snprintf(addr, sizeof addr, "/output/%d/dynamics/level", ch + 1);
		oscsend(addr, ",i", val >> 8 & 0xff);
		snprintf(addr, sizeof addr, "/output/%d/dynamics/level", ch + 2);
		oscsend(addr, ",i", val & 0xff);
	}
	*/
	return 0;
}

/**
 * @brief Sends a new durec status value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecstatus(const struct oscnode *path[], const char *addr, int reg, int val)
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
	int status;
	int position;

	status = val & 0xf;
	if (status != durec.status)
	{
		durec.status = status;
		oscsendenum("/durec/status", val & 0xf, names, LEN(names));
	}
	position = (val >> 8) * 100 / 65;
	if (position != durec.position)
	{
		durec.position = position;
		oscsend("/durec/position", ",i", (val >> 8) * 100 / 65);
	}
	return 0;
}

/**
 * @brief Sends a new durec time value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurectime(const struct oscnode *path[], const char *addr, int reg, int val)
{
	if (val != durec.time)
	{
		durec.time = val;
		oscsend(addr, ",i", val);
	}
	return 0;
}

/**
 * @brief Sends a new durec USB status value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecusbstatus(const struct oscnode *path[], const char *addr, int reg, int val)
{
	int usbload, usberrors;

	usbload = val >> 8;
	if (usbload != durec.usbload)
	{
		durec.usbload = usbload;
		oscsend("/durec/usbload", ",i", val >> 8);
	}
	usberrors = val & 0xff;
	if (usberrors != durec.usberrors)
	{
		durec.usberrors = usberrors;
		oscsend("/durec/usberrors", ",i", val & 0xff);
	}
	return 0;
}

/**
 * @brief Sends a new durec total space value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurectotalspace(const struct oscnode *path[], const char *addr, int reg, int val)
{
	float totalspace;

	totalspace = val / 16.f;
	if (totalspace != durec.totalspace)
	{
		durec.totalspace = totalspace;
		oscsend(addr, ",f", totalspace);
	}
	return 0;
}

/**
 * @brief Sends a new durec free space value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecfreespace(const struct oscnode *path[], const char *addr, int reg, int val)
{
	float freespace;

	freespace = val / 16.f;
	if (freespace != durec.freespace)
	{
		durec.freespace = freespace;
		oscsend(addr, ",f", freespace);
	}
	return 0;
}

/**
 * @brief Sends a new durec files length value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecfileslen(const struct oscnode *path[], const char *addr, int reg, int val)
{
	if (val < 0 || val == durec.fileslen)
		return 0;
	durec.files = realloc(durec.files, val * sizeof *durec.files);
	if (!durec.files)
		fatal(NULL);
	if (val > durec.fileslen)
		memset(durec.files + durec.fileslen, 0, (val - durec.fileslen) * sizeof *durec.files);
	durec.fileslen = val;
	if (durec.index >= durec.fileslen)
		durec.index = -1;
	oscsend(addr, ",i", val);
	return 0;
}

/**
 * @brief Sets the durec file value in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setdurecfile(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	int val;

	val = oscgetint(msg);
	if (oscend(msg) != 0)
		return -1;
	setreg(0x3e9c, val | 0x8000);
	return 0;
}

/**
 * @brief Sends a new durec file value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecfile(const struct oscnode *path[], const char *addr, int reg, int val)
{
	if (val != durec.file)
	{
		durec.file = val;
		oscsend(addr, ",i", val);
	}
	return 0;
}

/**
 * @brief Sends a new durec next value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecnext(const struct oscnode *path[], const char *addr, int reg, int val)
{
	static const char *const names[] = {
		"Single",
		"UFX Single",
		"Continuous",
		"Single Next",
		"Repeat Single",
		"Repeat All",
	};
	int next, playmode;

	next = ((val & 0xfff) ^ 0x800) - 0x800;
	if (next != durec.next)
	{
		durec.next = next;
		oscsend(addr, ",i", ((val & 0xfff) ^ 0x800) - 0x800);
	}
	playmode = val >> 12;
	if (playmode != durec.playmode)
	{
		durec.playmode = playmode;
		oscsendenum("/durec/playmode", val >> 12, names, LEN(names));
	}
	return 0;
}

/**
 * @brief Sends a new durec record time value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecrecordtime(const struct oscnode *path[], const char *addr, int reg, int val)
{
	if (val != durec.recordtime)
	{
		durec.recordtime = val;
		oscsend(addr, ",i", val);
	}
	return 0;
}

/**
 * @brief Sends a new durec index value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecindex(const struct oscnode *path[], const char *addr, int reg, int val)
{
	if (val + 1 > durec.fileslen)
		newdurecfileslen(NULL, "/durec/numfiles", -1, val + 1);
	durec.index = val;
	return 0;
}

/**
 * @brief Sends a new durec name value as an OSC message
 *
 * @param path The OSC address path
 * @param addr The OSC address
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecname(const struct oscnode *path[], const char *addr, int reg, int val)
{
	struct durecfile *f;
	char *pos, old[2];

	if (durec.index == -1)
		return 0;
	assert(durec.index < durec.fileslen);
	f = &durec.files[durec.index];
	reg -= 0x358b;
	assert(reg < sizeof f->name / 2);
	pos = f->name + reg * 2;
	memcpy(old, pos, sizeof old);
	putle16(pos, val);
	if (memcmp(old, pos, sizeof old) != 0)
		oscsend("/durec/name", ",is", durec.index, f->name);
	return 0;
}

/**
 * @brief Sends a new durec info value as an OSC message
 *
 * @param path The OSC address path
 * @param unused Unused parameter
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdurecinfo(const struct oscnode *path[], const char *unused, int reg, int val)
{
	struct durecfile *f;
	unsigned long samplerate;
	int channels;

	if (durec.index == -1)
		return 0;
	f = &durec.files[durec.index];
	samplerate = getsamplerate(val & 0xff);
	if (samplerate != f->samplerate)
	{
		f->samplerate = samplerate;
		oscsend("/durec/samplerate", ",ii", durec.index, samplerate);
	}
	channels = val >> 8;
	if (channels != f->channels)
	{
		f->channels = channels;
		oscsend("/durec/channels", ",ii", durec.index, channels);
	}
	return 0;
}

/**
 * @brief Sends a new durec length value as an OSC message
 *
 * @param path The OSC address path
 * @param unused Unused parameter
 * @param reg The register address
 * @param val The value to send
 * @return 0 on success, non-zero on failure
 */
static int
newdureclength(const struct oscnode *path[], const char *unused, int reg, int val)
{
	struct durecfile *f;

	if (durec.index == -1)
		return 0;
	f = &durec.files[durec.index];
	if (val != f->length)
	{
		f->length = val;
		oscsend("/durec/length", ",ii", durec.index, val);
	}
	return 0;
}

/**
 * @brief Sets the durec stop state in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setdurecstop(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	if (oscend(msg) != 0)
		return -1;
	setreg(0x3e9a, 0x8120);
	return 0;
}

/**
 * @brief Sets the durec play state in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setdurecplay(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	if (oscend(msg) != 0)
		return -1;
	setreg(0x3e9a, 0x8123);
	return 0;
}

/**
 * @brief Sets the durec record state in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setdurecrecord(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	if (oscend(msg) != 0)
		return -1;
	setreg(0x3e9a, 0x8122);
	return 0;
}

/**
 * @brief Sets the durec delete state in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setdurecdelete(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	int val;

	val = oscgetint(msg);
	if (oscend(msg) != 0)
		return -1;
	setreg(0x3e9b, 0x8000 | val);
	return 0;
}

/**
 * @brief Sets the refresh state in the device
 *
 * @param path The OSC address path
 * @param reg The register address
 * @param msg The OSC message containing the value
 * @return 0 on success, non-zero on failure
 */
static int
setrefresh(const struct oscnode *path[], int reg, struct oscmsg *msg)
{
	struct input *pb;
	char addr[256];
	int i;

	dsp.vers = -1;
	dsp.load = -1;
	setreg(0x3e04, 0x67cd);
	refreshing = true;
	/* FIXME: needs lock */
	for (i = 0; i < cur_device->outputslen; ++i)
	{
		pb = &playbacks[i];
		snprintf(addr, sizeof addr, "/playback/%d/stereo", i + 1);
		oscsend(addr, ",i", pb->stereo);
	}
	oscflush();
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
static int
refreshdone(const struct oscnode *path[], const char *addr, int reg, int val)
{
	refreshing = false;
	if (dflag)
		fprintf(stderr, "refresh done\n");
	return 0;
}

static const struct oscnode lowcuttree[] = {
	{"freq", 1, .set = setint, .new = newint, .min = 20, .max = 500},
	{"slope", 2, .set = setint, .new = newint},
	{0},
};

static const struct oscnode eqtree[] = {
	{"band1type", 1, .set = setenum, .new = newenum, .names = (const char *const[]){
														 "Peak",
														 "Low Shelf",
														 "High Pass",
														 "Low Pass",
													 },
	 .nameslen = 4},
	{"band1gain", 2, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -200, .max = 200},
	{"band1freq", 3, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band1q", 4, .set = setfixed, .new = newfixed, .scale = 0.1, .min = 4, .max = 99},
	{"band2gain", 5, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -200, .max = 200},
	{"band2freq", 6, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band2q", 7, .set = setfixed, .new = newfixed, .scale = 0.1, .min = 4, .max = 99},
	{"band3type", 8, .set = setenum, .new = newenum, .names = (const char *const[]){
														 "Peak",
														 "High Shelf",
														 "Low Pass",
														 "High Pass",
													 },
	 .nameslen = 3},
	{"band3gain", 9, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -200, .max = 200},
	{"band3freq", 10, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band3q", 11, .set = setfixed, .new = newfixed, .scale = 0.1, .min = 4, .max = 99},
	{0},
};

static const struct oscnode dynamicstree[] = {
	{"gain", 1, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -300, .max = 300},
	{"attack", 2, .set = setint, .new = newint, .min = 0, .max = 200},
	{"release", 3, .set = setint, .new = newint, .min = 100, .max = 999},
	{"compthres", 4, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -600, .max = 0},
	{"compratio", 5, .set = setfixed, .new = newfixed, .scale = 0.1, .min = 10, .max = 100},
	{"expthres", 6, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -990, .max = 200},
	{"expratio", 7, .set = setfixed, .new = newfixed, .scale = 0.1, .min = 10, .max = 100},
	{0},
};

static const struct oscnode autoleveltree[] = {
	{"maxgain", 1, .set = setfixed, .new = newfixed, .scale = 0.1, .min = 0, .max = 180},
	{"headroom", 2, .set = setfixed, .new = newfixed, .scale = 0.1, .min = 30, .max = 120},
	{"risetime", 3, .set = setint, .new = newint, .min = 100, .max = 9900},
	{0},
};

static const struct oscnode inputtree[] = {
	{"mute", 0x00, .set = setinputmute, .new = newbool},
	{"fx", 0x01, .set = setfixed, .new = newfixed, .min = -650, .max = 0, .scale = 0.1},
	{"stereo", 0x02, .set = setinputstereo, .new = newinputstereo},
	{"record", 0x03, .set = setbool, .new = newbool},
	{"", 0x04}, /* ? */
	{"playchan", 0x05, .set = setint, .new = newint, .min = 1, .max = 60},
	{"msproc", 0x06, .set = setbool, .new = newbool},
	{"phase", 0x07, .set = setbool, .new = newbool},
	{"gain", 0x08, .set = setinputgain, .new = newinputgain},
	{"48v", 0x09, .set = setinput48v},
	{"reflevel", 0x09, .set = setint, .new = newinput48v_reflevel},
	{"autoset", 0x0a, .set = setbool, .new = newbool},
	{"hi-z", 0x0b, .set = setinputhiz, .new = newinputhiz},
	{"lowcut", 0x0c, .set = setbool, .new = newbool, .child = lowcuttree},
	{"eq", 0x0f, .set = setbool, .new = newbool, .child = eqtree},
	{"dynamics", 0x1b, .set = setbool, .new = newbool, .child = dynamicstree},
	{"autolevel", 0x23, .set = setbool, .new = newbool, .child = autoleveltree},
	{"name", -1, .set = setinputname},
	{0},
};

static const struct oscnode roomeqtree[] = {
	{"delay", 0x00, .set = setfixed, .new = newfixed, .min = 0, .max = 425, .scale = 0.001},
	{"enabled", 0x01, .set = setbool, .new = newbool},
	{"band1type", 0x02, .set = setenum, .new = newenum, .names = (const char *const[]){
															"Peak",
															"Low Shelf",
															"High Pass",
															"Low Pass",
														},
	 .nameslen = 4},
	{"band1gain", 0x03, .set = setfixed, .new = newfixed, .min = -200, .max = 200, .scale = 0.1},
	{"band1freq", 0x04, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band1q", 0x05, .set = setfixed, .new = newfixed, .min = 4, .max = 99, .scale = 0.1},
	{"band2gain", 0x06, .set = setfixed, .new = newfixed, .min = -200, .max = 200, .scale = 0.1},
	{"band2freq", 0x07, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band2q", 0x08, .set = setfixed, .new = newfixed, .min = 4, .max = 99, .scale = 0.1},
	{"band3gain", 0x09, .set = setfixed, .new = newfixed, .min = -200, .max = 200, .scale = 0.1},
	{"band3freq", 0x0a, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band3q", 0x0b, .set = setfixed, .new = newfixed, .min = 4, .max = 99, .scale = 0.1},
	{"band4gain", 0x0c, .set = setfixed, .new = newfixed, .min = -200, .max = 200, .scale = 0.1},
	{"band4freq", 0x0d, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band4q", 0x0e, .set = setfixed, .new = newfixed, .min = 4, .max = 99, .scale = 0.1},
	{"band5gain", 0x0f, .set = setfixed, .new = newfixed, .min = -200, .max = 200, .scale = 0.1},
	{"band5freq", 0x10, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band5q", 0x11, .set = setfixed, .new = newfixed, .min = 4, .max = 99, .scale = 0.1},
	{"band6gain", 0x12, .set = setfixed, .new = newfixed, .min = -200, .max = 200, .scale = 0.1},
	{"band6freq", 0x13, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band6q", 0x14, .set = setfixed, .new = newfixed, .min = 4, .max = 99, .scale = 0.1},
	{"band7gain", 0x15, .set = setfixed, .new = newfixed, .min = -200, .max = 200, .scale = 0.1},
	{"band7freq", 0x16, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band7q", 0x17, .set = setfixed, .new = newfixed, .min = 4, .max = 99, .scale = 0.1},
	{"band8type", 0x18, .set = setenum, .new = newenum, .names = (const char *const[]){
															"Peak",
															"High Shelf",
															"Low Pass",
															"High Pass",
														},
	 .nameslen = 4},
	{"band8gain", 0x19, .set = setfixed, .new = newfixed, .min = -200, .max = 200, .scale = 0.1},
	{"band8freq", 0x1a, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band8q", 0x1b, .set = setfixed, .new = newfixed, .min = 4, .max = 99, .scale = 0.1},
	{"band9type", 0x1c, .set = setenum, .new = newenum, .names = (const char *const[]){
															"Peak",
															"High Shelf",
															"Low Pass",
															"High Pass",
														},
	 .nameslen = 4},
	{"band9gain", 0x1d, .set = setfixed, .new = newfixed, .min = -200, .max = 200, .scale = 0.1},
	{"band9freq", 0x1e, .set = setint, .new = newint, .min = 20, .max = 20000},
	{"band9q", 0x1f, .set = setfixed, .new = newfixed, .min = 4, .max = 99, .scale = 0.1},
	{0},
};

static const struct oscnode outputtree[] = {
	{"volume", 0x00, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -65.0, .max = 6.0},
	{"balance", 0x01, .set = setint, .new = newint, .min = -100, .max = 100},
	{"mute", 0x02, .set = setbool, .new = newbool},
	{"fx", 0x03, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -65.0, .max = 0.0},
	{"stereo", 0x04, .set = setbool, .new = newoutputstereo},
	{"record", 0x05, .set = setbool, .new = newbool},
	{"", 0x06}, /* ? */
	{"playchan", 0x07, .set = setint, .new = newint},
	{"phase", 0x08, .set = setbool, .new = newbool},
	{"reflevel", 0x09, .set = setenum, .new = newenum, .names = (const char *const[]){
														   "+4dBu",
														   "+13dBu",
														   "+19dBu",
													   },
	 .nameslen = 3}, // TODO: phones
	{"crossfeed", 0x0a, .set = setint, .new = newint},
	{"volumecal", 0x0b, .set = setfixed, .new = newfixed, .min = -2400, .max = 300, .scale = 0.01},
	{"lowcut", 0x0c, .set = setbool, .new = newbool, .child = lowcuttree},
	{"eq", 0x0f, .set = setbool, .new = newbool, .child = eqtree},
	{"dynamics", 0x1b, .set = setbool, .new = newbool, .child = dynamicstree},
	{"autolevel", 0x23, .set = setbool, .new = newbool, .child = autoleveltree},
	{"roomeq", -1, .child = roomeqtree},
	{"loopback", -1, .set = setoutputloopback},
	{0},
};
static const struct oscnode outputroomeqtree[] = {
	{"roomeq", 0x00, .child = roomeqtree},
	{0},
};
static const struct oscnode mixtree[] = {
	{"input", 0, .child = (const struct oscnode[]){
					 {"1", 0x00, .set = setmix, .new = newmix},
					 {"2", 0x01, .set = setmix, .new = newmix},
					 {"3", 0x02, .set = setmix, .new = newmix},
					 {"4", 0x03, .set = setmix, .new = newmix},
					 {"5", 0x04, .set = setmix, .new = newmix},
					 {"6", 0x05, .set = setmix, .new = newmix},
					 {"7", 0x06, .set = setmix, .new = newmix},
					 {"8", 0x07, .set = setmix, .new = newmix},
					 {"9", 0x08, .set = setmix, .new = newmix},
					 {"10", 0x09, .set = setmix, .new = newmix},
					 {"11", 0x0a, .set = setmix, .new = newmix},
					 {"12", 0x0b, .set = setmix, .new = newmix},
					 {"13", 0x0c, .set = setmix, .new = newmix},
					 {"14", 0x0d, .set = setmix, .new = newmix},
					 {"15", 0x0e, .set = setmix, .new = newmix},
					 {"16", 0x0f, .set = setmix, .new = newmix},
					 {"17", 0x10, .set = setmix, .new = newmix},
					 {"18", 0x11, .set = setmix, .new = newmix},
					 {"19", 0x12, .set = setmix, .new = newmix},
					 {"20", 0x13, .set = setmix, .new = newmix},
					 {0},
				 }},
	{"playback", 0x20, .child = (const struct oscnode[]){
						   {"1", 0x00, .set = setmix, .new = newmix},
						   {"2", 0x01, .set = setmix, .new = newmix},
						   {"3", 0x02, .set = setmix, .new = newmix},
						   {"4", 0x03, .set = setmix, .new = newmix},
						   {"5", 0x04, .set = setmix, .new = newmix},
						   {"6", 0x05, .set = setmix, .new = newmix},
						   {"7", 0x06, .set = setmix, .new = newmix},
						   {"8", 0x07, .set = setmix, .new = newmix},
						   {"9", 0x08, .set = setmix, .new = newmix},
						   {"10", 0x09, .set = setmix, .new = newmix},
						   {"11", 0x0a, .set = setmix, .new = newmix},
						   {"12", 0x0b, .set = setmix, .new = newmix},
						   {"13", 0x0c, .set = setmix, .new = newmix},
						   {"14", 0x0d, .set = setmix, .new = newmix},
						   {"15", 0x0e, .set = setmix, .new = newmix},
						   {"16", 0x0f, .set = setmix, .new = newmix},
						   {"17", 0x10, .set = setmix, .new = newmix},
						   {"18", 0x11, .set = setmix, .new = newmix},
						   {"19", 0x12, .set = setmix, .new = newmix},
						   {"20", 0x13, .set = setmix, .new = newmix},
						   {0},
					   }},
	{0},
};
static const struct oscnode tree[] = {
	{"input", 0, .child = (const struct oscnode[]){
					 {"1", 0x000, .child = inputtree},
					 {"2", 0x040, .child = inputtree},
					 {"3", 0x080, .child = inputtree},
					 {"4", 0x0c0, .child = inputtree},
					 {"5", 0x100, .child = inputtree},
					 {"6", 0x140, .child = inputtree},
					 {"7", 0x180, .child = inputtree},
					 {"8", 0x1c0, .child = inputtree},
					 {"9", 0x200, .child = inputtree},
					 {"10", 0x240, .child = inputtree},
					 {"11", 0x280, .child = inputtree},
					 {"12", 0x2c0, .child = inputtree},
					 {"13", 0x300, .child = inputtree},
					 {"14", 0x340, .child = inputtree},
					 {"15", 0x380, .child = inputtree},
					 {"16", 0x3c0, .child = inputtree},
					 {"17", 0x400, .child = inputtree},
					 {"18", 0x440, .child = inputtree},
					 {"19", 0x480, .child = inputtree},
					 {"20", 0x4c0, .child = inputtree},
					 {0},
				 }},
	{"output", 0x0500, .child = (const struct oscnode[]){
						   {"1", 0x000, .child = outputtree},
						   {"2", 0x040, .child = outputtree},
						   {"3", 0x080, .child = outputtree},
						   {"4", 0x0c0, .child = outputtree},
						   {"5", 0x100, .child = outputtree},
						   {"6", 0x140, .child = outputtree},
						   {"7", 0x180, .child = outputtree},
						   {"8", 0x1c0, .child = outputtree},
						   {"9", 0x200, .child = outputtree},
						   {"10", 0x240, .child = outputtree},
						   {"11", 0x280, .child = outputtree},
						   {"12", 0x2c0, .child = outputtree},
						   {"13", 0x300, .child = outputtree},
						   {"14", 0x340, .child = outputtree},
						   {"15", 0x380, .child = outputtree},
						   {"16", 0x3c0, .child = outputtree},
						   {"17", 0x400, .child = outputtree},
						   {"18", 0x440, .child = outputtree},
						   {"19", 0x480, .child = outputtree},
						   {"20", 0x4c0, .child = outputtree},
						   {0},
					   }},
	{"mix", 0x2000, .child = (const struct oscnode[]){
						{"1", 0x000, .child = mixtree},
						{"2", 0x040, .child = mixtree},
						{"3", 0x080, .child = mixtree},
						{"4", 0x0c0, .child = mixtree},
						{"5", 0x100, .child = mixtree},
						{"6", 0x140, .child = mixtree},
						{"7", 0x180, .child = mixtree},
						{"8", 0x1c0, .child = mixtree},
						{"9", 0x200, .child = mixtree},
						{"10", 0x240, .child = mixtree},
						{"11", 0x280, .child = mixtree},
						{"12", 0x2c0, .child = mixtree},
						{"13", 0x300, .child = mixtree},
						{"14", 0x340, .child = mixtree},
						{"15", 0x380, .child = mixtree},
						{"16", 0x3c0, .child = mixtree},
						{"17", 0x400, .child = mixtree},
						{"18", 0x440, .child = mixtree},
						{"19", 0x480, .child = mixtree},
						{"20", 0x4c0, .child = mixtree},
						{0},
					}},
	{"", 0x2fc0, .new = refreshdone},
	{"reverb", 0x3000, .set = setbool, .new = newbool, .child = (const struct oscnode[]){
														   {"type", 0x01, .set = setenum, .new = newenum, .names = (const char *const[]){
																											  "Small Room",
																											  "Medium Room",
																											  "Large Room",
																											  "Walls",
																											  "Shorty",
																											  "Attack",
																											  "Swagger",
																											  "Old School",
																											  "Echoistic",
																											  "8plus9",
																											  "Grand Wide",
																											  "Thicker",
																											  "Envelope",
																											  "Gated",
																											  "Space",
																										  },
															.nameslen = 15},
														   {"predelay", 0x02, .set = setint, .new = newint},
														   {"lowcut", 0x03, .set = setint, .new = newint},
														   {"roomscale", 0x04, .set = setfixed, .new = newfixed, .scale = 0.01},
														   {"attack", 0x05, .set = setint, .new = newint},
														   {"hold", 0x06, .set = setint, .new = newint},
														   {"release", 0x07, .set = setint, .new = newint},
														   {"highcut", 0x08, .set = setint, .new = newint},
														   {"time", 0x09, .set = setfixed, .new = newfixed, .scale = 0.1},
														   {"highdamp", 0x0a, .set = setint, .new = newint},
														   {"smooth", 0x0b, .set = setint, .new = newint},
														   {"volume", 0x0c, .set = setfixed, .new = newfixed, .scale = 0.1},
														   {"width", 0x0d, .set = setfixed, .new = newfixed, .scale = 0.01},
														   {0},
													   }},
	{"echo", 0x3014, .set = setbool, .new = newbool, .child = (const struct oscnode[]){
														 {"type", 0x01, .set = setenum, .new = newenum, .names = (const char *const[]){
																											"Stereo Echo",
																											"Stereo Cross",
																											"Pong Echo",
																										},
														  .nameslen = 3},
														 {"delay", 0x02, .set = setfixed, .new = newfixed, .scale = 0.001, .min = 0, .max = 2000},
														 {"feedback", 0x03, .set = setint, .new = newint},
														 {"highcut", 0x04, .set = setenum, .new = newenum, .names = (const char *const[]){
																											   "Off",
																											   "16kHz",
																											   "12kHz",
																											   "8kHz",
																											   "4kHz",
																											   "2kHz",
																										   },
														  .nameslen = 6},
														 {"volume", 0x05, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -650, .max = 60},
														 {"width", 0x06, .set = setfixed, .new = newfixed, .scale = 0.01},
														 {0},
													 }},
	{"controlroom", 0x3050, .child = (const struct oscnode[]){
								{"mainout", 0, .set = setenum, .new = newenum, .names = (const char *const[]){
																				   "1/2",
																				   "3/4",
																				   "5/6",
																				   "7/8",
																				   "9/10",
																				   "11/12",
																				   "13/14",
																				   "15/16",
																				   "17/18",
																				   "19/20",
																			   },
								 .nameslen = 10},
								{"mainmono", 1, .set = setbool, .new = newbool},
								{"", 2}, /* phones source? */
								{"muteenable", 3, .set = setbool, .new = newbool},
								{"dimreduction", 4, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -650, .max = 0},
								{"dim", 5, .set = setbool, .new = newbool},
								{"recallvolume", 6, .set = setfixed, .new = newfixed, .scale = 0.1, .min = -650, .max = 0},
								{0},
							}},
	{"clock", 0x3060, .child = (const struct oscnode[]){
						  {"source", 4, .set = setenum, .new = newenum, .names = (const char *const[]){
																			"Internal",
																			"Word Clock",
																			"SPDIF",
																			"AES",
																			"Optical",
																		},
						   .nameslen = 5},
						  {"samplerate", 5, .new = newsamplerate},
						  {"wckout", 6, .set = setbool, .new = newbool},
						  {"wcksingle", 7, .set = setbool, .new = newbool},
						  {"wckterm", 8, .set = setbool, .new = newbool},
						  {0},
					  }},
	{"hardware", 0x3070, .child = (const struct oscnode[]){
							 {"opticalout", 8, .set = setenum, .new = newenum, .names = (const char *const[]){
																				   "ADAT",
																				   "SPDIF",
																			   },
							  .nameslen = 2},
							 {"spdifout", 9, .set = setenum, .new = newenum, .names = (const char *const[]){
																				 "Consumer",
																				 "Professional",
																			 },
							  .nameslen = 2},
							 {"ccmode", 10},
							 {"ccmix", 11, .set = setenum, .new = newenum, .names = (const char *const[]){
																			   "TotalMix App",
																			   "6ch + phones",
																			   "8ch",
																			   "20ch",
																		   },
							  .nameslen = 4},
							 {"standalonemidi", 12, .set = setbool, .new = newbool},
							 {"standalonearc", 13, .set = setenum, .new = newenum, .names = (const char *const[]){
																					   "Volume",
																					   "1s Op",
																					   "Normal",
																				   },
							  .nameslen = 3},
							 {"lockkeys", 14, .set = setenum, .new = newenum, .names = (const char *const[]){
																				  "Off",
																				  "Keys",
																				  "All",
																			  },
							  .nameslen = 3},
							 {"remapkeys", 15, .set = setbool, .new = newbool},
							 {"", 16, .new = newdspload},
							 {"", 17, .new = newdspavail},
							 {"", 18, .new = newdspactive},
							 {"", 19, .new = newarcencoder},

							 {"eqdrecord", -1, .set = seteqdrecord},
							 {0},
						 }},
	{"input", 0x3180, .child = (const struct oscnode[]){
						  {"", 0, .new = newdynlevel},
						  {"", 1, .new = newdynlevel},
						  {"", 2, .new = newdynlevel},
						  {"", 3, .new = newdynlevel},
						  {"", 4, .new = newdynlevel},
						  {"", 5, .new = newdynlevel},
						  {"", 6, .new = newdynlevel},
						  {"", 7, .new = newdynlevel},
						  {"", 8, .new = newdynlevel},
						  {"", 9, .new = newdynlevel},
						  {"", 10, .new = newdynlevel},
						  {"", 11, .new = newdynlevel},
						  {"", 12, .new = newdynlevel},
						  {"", 13, .new = newdynlevel},
						  {"", 14, .new = newdynlevel},
						  {"", 15, .new = newdynlevel},
						  {"", 16, .new = newdynlevel},
						  {"", 17, .new = newdynlevel},
						  {"", 18, .new = newdynlevel},
						  {"", 19, .new = newdynlevel},
						  {0},
					  }},
	{"durec", 0x3580, .child = (const struct oscnode[]){
						  {"status", 0, .new = newdurecstatus},
						  {"time", 1, .new = newdurectime},
						  {"", 2}, /* ? */
						  {"usbload", 3, .new = newdurecusbstatus},
						  {"totalspace", 4, .new = newdurectotalspace},
						  {"freespace", 5, .new = newdurecfreespace},
						  {"numfiles", 6, .new = newdurecfileslen},
						  {"file", 7, .new = newdurecfile, .set = setdurecfile},
						  {"next", 8, .new = newdurecnext},
						  {"recordtime", 9, .new = newdurecrecordtime},
						  {"", 10, .new = newdurecindex},
						  {"", 11, .new = newdurecname},
						  {"", 12, .new = newdurecname},
						  {"", 13, .new = newdurecname},
						  {"", 14, .new = newdurecname},
						  {"", 15, .new = newdurecinfo},
						  {"", 16, .new = newdureclength},

						  {"stop", -1, .set = setdurecstop},
						  {"play", -1, .set = setdurecplay},
						  {"record", -1, .set = setdurecrecord},
						  {"delete", -1, .set = setdurecdelete},
						  {0},
					  }},
	{"output", 0x35d0, .child = (const struct oscnode[]){
						   {"1", 0x000, .child = outputroomeqtree},
						   {"2", 0x020, .child = outputroomeqtree},
						   {"3", 0x040, .child = outputroomeqtree},
						   {"4", 0x060, .child = outputroomeqtree},
						   {"5", 0x080, .child = outputroomeqtree},
						   {"6", 0x0a0, .child = outputroomeqtree},
						   {"7", 0x0c0, .child = outputroomeqtree},
						   {"8", 0x0e0, .child = outputroomeqtree},
						   {"9", 0x100, .child = outputroomeqtree},
						   {"10", 0x120, .child = outputroomeqtree},
						   {"11", 0x140, .child = outputroomeqtree},
						   {"12", 0x160, .child = outputroomeqtree},
						   {"13", 0x180, .child = outputroomeqtree},
						   {"14", 0x1a0, .child = outputroomeqtree},
						   {"15", 0x1c0, .child = outputroomeqtree},
						   {"16", 0x1e0, .child = outputroomeqtree},
						   {"17", 0x200, .child = outputroomeqtree},
						   {"18", 0x220, .child = outputroomeqtree},
						   {"19", 0x240, .child = outputroomeqtree},
						   {"20", 0x260, .child = outputroomeqtree},
						   {0},
					   }},
	/* write-only */
	{"refresh", -1, .set = setrefresh},
	{0},
};

/**
 * @brief Matches a pattern with a string
 *
 * @param pat The pattern to match
 * @param str The string to match against
 * @return The next character in the pattern if matched, NULL otherwise
 */
static const char *
match(const char *pat, const char *str)
{
	for (;;)
	{
		if (*pat == '/' || *pat == '\0')
			return *str == '\0' ? pat : NULL;
		if (*pat != *str)
			return NULL;
		++pat;
		++str;
	}
}

/**
 * @brief Handles an OSC message
 *
 * @param buf The buffer containing the OSC message
 * @param len The length of the buffer
 * @return 0 on success, non-zero on failure
 */
void handleosc(const void *buf, size_t len)
{
	const char *addr, *next;
	const struct oscnode *path[8], *node;
	size_t pathlen;
	struct oscmsg msg;
	int reg;

	if (len % 4 != 0)
		return;
	msg.err = NULL;
	msg.buf = (unsigned char *)buf;
	msg.end = (unsigned char *)buf + len;
	msg.type = "ss";

	addr = oscgetstr(&msg);
	msg.type = oscgetstr(&msg);
	if (msg.err)
	{
		fprintf(stderr, "invalid osc message: %s\n", msg.err);
		return;
	}
	++msg.type;

	reg = 0;
	pathlen = 0;
	for (node = tree; node->name;)
	{
		next = match(addr + 1, node->name);
		if (next)
		{
			assert(pathlen < LEN(path));
			path[pathlen++] = node;
			reg += node->reg;
			if (*next)
			{
				node = node->child;
				addr = next;
			}
			else
			{
				if (node->set)
				{
					node->set(path + pathlen - 1, reg, &msg);
					if (msg.err)
						fprintf(stderr, "%s: %s\n", addr, msg.err);
				}
				break;
			}
		}
		else
		{
			++node;
		}
	}

	// Check for special commands
	if (strcmp(addr, "/dump") == 0)
	{
		// Call the correct function
		dumpDeviceState();
		return;
	}
	else if (strcmp(addr, "/dump/save") == 0)
	{
		// New command to save config to file
		int result = dumpConfig();
		if (result == 0)
		{
			// Send acknowledgment via OSC
			oscsend("/dump/save", ",s", "Configuration saved successfully");
		}
		else
		{
			// Send error via OSC
			oscsend("/error", ",s", "Failed to save configuration");
		}
		return;
	}
}

static unsigned char oscbuf[8192];
static struct oscmsg oscmsg;

/**
 * @brief Sends an OSC message
 *
 * @param addr The OSC address
 * @param type The OSC type tags
 * @param ... The OSC arguments
 */
static void
oscsend(const char *addr, const char *type, ...)
{
	unsigned char *len;
	va_list ap;

	_Static_assert(sizeof(float) == sizeof(uint32_t), "unsupported float type");
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
	oscmsg.type = ++type;
	va_start(ap, type);
	for (; *type; ++type)
	{
		switch (*type)
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
static void
oscsendenum(const char *addr, int val, const char *const names[], size_t nameslen)
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
static void
oscflush(void)
{
	if (oscmsg.buf)
	{
		writeosc(oscbuf, oscmsg.buf - oscbuf);
		oscmsg.buf = NULL;
	}
}

/**
 * @brief Handles register values received from the device
 *
 * @param payload The buffer containing the register values
 * @param len The length of the buffer
 */
static void
handleregs(uint_least32_t *payload, size_t len)
{
	size_t i;
	int reg, val, off;
	const struct oscnode *node;
	char addr[256], *addrend;
	const struct oscnode *path[8];
	size_t pathlen;

	for (i = 0; i < len; ++i)
	{
		reg = payload[i] >> 16 & 0x7fff;
		val = (long)((payload[i] & 0xffff) ^ 0x8000) - 0x8000;
		addrend = addr;
		off = 0;
		node = tree;
		pathlen = 0;
		while (node->name)
		{
			if (reg >= off + node[1].reg && node[1].name && node[1].reg != -1)
			{
				++node;
				continue;
			}
			*addrend++ = '/';
			addrend = memccpy(addrend, node->name, '\0', addr + sizeof addr - addrend);
			assert(addrend);
			--addrend;
			assert(pathlen < LEN(path));
			path[pathlen++] = node;
			if (reg == off + node->reg && node->new)
			{
				node->new(path + pathlen - 1, addr, reg, val);
			}
			else if (node->child)
			{
				off += node->reg;
				node = node->child;
				continue;
			}
			else if (dflag && reg != off + node->reg)
			{
				switch (reg)
				{
				case 0x3180:
				case 0x3380:
					break;
				default:
					fprintf(stderr, "[%.4x]=%.4hx (%.4x %s)\n", reg, (short)val, off + node->reg, addr);
					break;
				}
			}
			break;
		}
	}
}

/**
 * @brief Handles audio level values received from the device
 *
 * @param subid The sub ID of the SysEx message
 * @param payload The buffer containing the audio level values
 * @param len The length of the buffer
 */
static void
handlelevels(int subid, uint_least32_t *payload, size_t len)
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
		assert(0);
	}
	for (i = 0; i < len; ++i)
	{
		rms = *payload++;
		rms |= (uint_least64_t)*payload++ << 32;
		peak = *payload++;
		if (type)
		{
			peakdb = 20 * log10((peak >> 4) / 0x1p23);
			rmsdb = 10 * log10(rms / 0x1p54);
			snprintf(addr, sizeof addr, "/%s/%d/level", type, (int)i + 1);
			if (peakfx)
			{
				peakfxdb = 20 * log10((peakfx[i] >> 4) / 0x1p23);
				rmsfxdb = 10 * log10(rmsfx[i] / 0x1p54);
				oscsend(addr, ",ffffi", peakdb, rmsdb, peakfxdb, rmsfxdb, peak & peakfx[i] & 1);
			}
			else
			{
				oscsend(addr, ",ffi", peakdb, rmsdb, peak & 1);
			}
		}
		else
		{
			*peakfx++ = peak;
			*rmsfx++ = rms;
		}
	}
}

/**
 * @brief Handles a SysEx message received from the device
 *
 * @param buf The buffer containing the SysEx message
 * @param len The length of the buffer
 * @param payload The buffer to store the decoded payload
 */
void handlesysex(const unsigned char *buf, size_t len, uint_least32_t *payload)
{
	struct sysex sysex;
	int ret;
	size_t i;
	uint_least32_t *pos;

	ret = sysexdec(&sysex, buf, len, SYSEX_MFRID | SYSEX_DEVID | SYSEX_SUBID);
	if (ret != 0 || sysex.mfrid != 0x200d || sysex.devid != 0x10 || sysex.datalen % 5 != 0)
	{
		if (ret == 0)
			fprintf(stderr, "ignoring unknown sysex packet (mfr=%x devid=%x datalen=%zu)\n", sysex.mfrid, sysex.devid, sysex.datalen);
		else
			fprintf(stderr, "ignoring unknown sysex packet\n");
		return;
	}
	pos = payload;
	for (i = 0; i < sysex.datalen; i += 5)
		*pos++ = getle32_7bit(sysex.data + i);
	switch (sysex.subid)
	{
	case 0:
		handleregs(payload, pos - payload);
		fflush(stdout);
		fflush(stderr);
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		handlelevels(sysex.subid, payload, pos - payload);
		break;
	default:
		fprintf(stderr, "ignoring unknown sysex sub ID\n");
	}
	oscflush();
}

/**
 * @brief Handles a timer event
 *
 * @param levels Whether to request audio levels
 */
void handletimer(bool levels)
{
	static int serial;
	unsigned char buf[7];

	if (levels && !refreshing)
	{
		/* XXX: ~60 times per second levels, ~30 times per second serial */
		writesysex(2, NULL, 0, buf);
	}

	setreg(0x3f00, serial);
	serial = (serial + 1) & 0xf;
}

/**
 * @brief Initializes the OSCMix application
 *
 * @param port The device identifier string
 * @return 0 on success, non-zero on failure
 */
int init(const char *port)
{
	extern const struct device ffucxii;
	static const struct device *devices[] = {
		&ffucxii,
	};
	int i, j;
	size_t namelen;

	for (i = 0; i < LEN(devices); ++i)
	{
		cur_device = devices[i];
		if (strcmp(port, cur_device->id) == 0)
			break;
		namelen = strlen(cur_device->name);
		if (strncmp(port, cur_device->name, namelen) == 0)
		{
			if (!port[namelen] || (port[namelen] == ' ' && port[namelen + 1] == '('))
				break;
		}
	}
	if (i == LEN(devices))
	{
		fprintf(stderr, "unsupported device '%s'", port);
		return -1;
	}

	inputs = calloc(cur_device->inputslen, sizeof *inputs);
	playbacks = calloc(cur_device->outputslen, sizeof *playbacks);
	outputs = calloc(cur_device->outputslen, sizeof *outputs);
	if (!inputs || !playbacks || !outputs)
	{
		perror(NULL);
		return -1;
	}
	for (i = 0; i < cur_device->outputslen; ++i)
	{
		struct output *out;

		playbacks[i].stereo = true;
		out = &outputs[i];
		out->mix = calloc(cur_device->inputslen + cur_device->outputslen, sizeof *out->mix);
		if (!out->mix)
		{
			perror(NULL);
			return -1;
		}
		for (j = 0; j < cur_device->inputslen + cur_device->outputslen; ++j)
			out->mix[j].vol = -650;
	}
	return 0;
}
