/*
 * Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "sha1.h"
#include "intpack.h"

#define F(B, C, D) ((((C) ^ (D)) & (B)) ^ (D))
#define G(B, C, D) ((B) ^ (C) ^ (D))
#define H(B, C, D) (((D) & (C)) | (((D) | (C)) & (B)))
#define I(B, C, D) G(B, C, D)

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define K1 ((uint32_t)0x5A827999)
#define K2 ((uint32_t)0x6ED9EBA1)
#define K3 ((uint32_t)0x8F1BBCDC)
#define K4 ((uint32_t)0xCA62C1D6)

static const uint32_t sha1_IV[5] = {
	0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

static void
sha1_round(const unsigned char *buf, uint32_t *val)
{
	uint32_t m[80];
	uint32_t a, b, c, d, e;
	int i;

	a = val[0];
	b = val[1];
	c = val[2];
	d = val[3];
	e = val[4];
	for (i = 0; i < 16; i++)
	{
		m[i] = getbe32(buf + i * 4);
	}
	for (i = 16; i < 80; i++)
	{
		uint32_t x = m[i - 3] ^ m[i - 8] ^ m[i - 14] ^ m[i - 16];
		m[i] = ROTL(x, 1);
	}

	for (i = 0; i < 20; i += 5)
	{
		e += ROTL(a, 5) + F(b, c, d) + K1 + m[i + 0];
		b = ROTL(b, 30);
		d += ROTL(e, 5) + F(a, b, c) + K1 + m[i + 1];
		a = ROTL(a, 30);
		c += ROTL(d, 5) + F(e, a, b) + K1 + m[i + 2];
		e = ROTL(e, 30);
		b += ROTL(c, 5) + F(d, e, a) + K1 + m[i + 3];
		d = ROTL(d, 30);
		a += ROTL(b, 5) + F(c, d, e) + K1 + m[i + 4];
		c = ROTL(c, 30);
	}
	for (i = 20; i < 40; i += 5)
	{
		e += ROTL(a, 5) + G(b, c, d) + K2 + m[i + 0];
		b = ROTL(b, 30);
		d += ROTL(e, 5) + G(a, b, c) + K2 + m[i + 1];
		a = ROTL(a, 30);
		c += ROTL(d, 5) + G(e, a, b) + K2 + m[i + 2];
		e = ROTL(e, 30);
		b += ROTL(c, 5) + G(d, e, a) + K2 + m[i + 3];
		d = ROTL(d, 30);
		a += ROTL(b, 5) + G(c, d, e) + K2 + m[i + 4];
		c = ROTL(c, 30);
	}
	for (i = 40; i < 60; i += 5)
	{
		e += ROTL(a, 5) + H(b, c, d) + K3 + m[i + 0];
		b = ROTL(b, 30);
		d += ROTL(e, 5) + H(a, b, c) + K3 + m[i + 1];
		a = ROTL(a, 30);
		c += ROTL(d, 5) + H(e, a, b) + K3 + m[i + 2];
		e = ROTL(e, 30);
		b += ROTL(c, 5) + H(d, e, a) + K3 + m[i + 3];
		d = ROTL(d, 30);
		a += ROTL(b, 5) + H(c, d, e) + K3 + m[i + 4];
		c = ROTL(c, 30);
	}
	for (i = 60; i < 80; i += 5)
	{
		e += ROTL(a, 5) + I(b, c, d) + K4 + m[i + 0];
		b = ROTL(b, 30);
		d += ROTL(e, 5) + I(a, b, c) + K4 + m[i + 1];
		a = ROTL(a, 30);
		c += ROTL(d, 5) + I(e, a, b) + K4 + m[i + 2];
		e = ROTL(e, 30);
		b += ROTL(c, 5) + I(d, e, a) + K4 + m[i + 3];
		d = ROTL(d, 30);
		a += ROTL(b, 5) + I(c, d, e) + K4 + m[i + 4];
		c = ROTL(c, 30);
	}

	val[0] += a;
	val[1] += b;
	val[2] += c;
	val[3] += d;
	val[4] += e;
}

void sha1_init(sha1_context *cc)
{
	memcpy(cc->val, sha1_IV, sizeof cc->val);
	cc->count = 0;
}

void sha1_update(sha1_context *cc, const void *data, size_t len)
{
	const unsigned char *buf;
	size_t ptr;

	buf = data;
	ptr = (size_t)cc->count & 63;
	while (len > 0)
	{
		size_t clen;

		clen = 64 - ptr;
		if (clen > len)
		{
			clen = len;
		}
		memcpy(cc->buf + ptr, buf, clen);
		ptr += clen;
		buf += clen;
		len -= clen;
		cc->count += (uint64_t)clen;
		if (ptr == 64)
		{
			sha1_round(cc->buf, cc->val);
			ptr = 0;
		}
	}
}

void sha1_out(const sha1_context *cc, void *dst)
{
	unsigned char buf[64];
	uint32_t val[5];
	size_t ptr;

	ptr = (size_t)cc->count & 63;
	memcpy(buf, cc->buf, ptr);
	memcpy(val, cc->val, sizeof val);
	buf[ptr++] = 0x80;
	if (ptr > 56)
	{
		memset(buf + ptr, 0, 64 - ptr);
		sha1_round(buf, val);
		memset(buf, 0, 56);
	}
	else
	{
		memset(buf + ptr, 0, 56 - ptr);
	}
	putbe64(buf + 56, cc->count << 3);
	sha1_round(buf, val);
	putbe32(dst, val[0]);
	putbe32((unsigned char *)dst + 4, val[1]);
	putbe32((unsigned char *)dst + 8, val[2]);
	putbe32((unsigned char *)dst + 12, val[3]);
	putbe32((unsigned char *)dst + 16, val[4]);
}

/*
 * Dumps the device configuration/state to a JSON file
 * Saves to "application_home/device_config/audio-device_{device_name}_{date_time}.json"
 *
 * @param device_name Name of the audio device
 * @param state_obj JSON-formatted string containing the device state
 * @return 0 on success, -1 on failure
 */
int dumpConfig(const char *device_name, const char *state_obj)
{
	if (!device_name || !state_obj)
	{
		return -1;
	}

	// Get home directory
	const char *home_dir = getenv("HOME");
	if (!home_dir)
	{
#ifdef _WIN32
		home_dir = getenv("USERPROFILE");
		if (!home_dir)
		{
			return -1;
		}
#else
		return -1;
#endif
	}

	// Create path for device_config directory
	char config_dir[1024];
	snprintf(config_dir, sizeof(config_dir), "%s/device_config", home_dir);

// Create directory if it doesn't exist
#ifdef _WIN32
	mkdir(config_dir);
#else
	mkdir(config_dir, 0755);
#endif

	// Format current date and time
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	char date_time[64];
	strftime(date_time, sizeof(date_time), "%Y-%m-%d_%H-%M-%S", tm_info);

	// Sanitize device name (remove spaces and special characters)
	char safe_device_name[256];
	size_t j = 0;
	for (size_t i = 0; i < strlen(device_name) && j < sizeof(safe_device_name) - 1; i++)
	{
		char c = device_name[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
		{
			safe_device_name[j++] = c;
		}
		else if (c == ' ')
		{
			safe_device_name[j++] = '_';
		}
	}
	safe_device_name[j] = '\0';

	// Create full file path
	char filepath[1280];
	snprintf(filepath, sizeof(filepath), "%s/audio-device_%s_date-time_%s.json",
			 config_dir, safe_device_name, date_time);

	// Write state to file
	FILE *fp = fopen(filepath, "w");
	if (!fp)
	{
		return -1;
	}

	fputs(state_obj, fp);
	fclose(fp);

	return 0;
}
