/*
 *  Copyright (C) 2014 Steve Harris et al. (see AUTHORS)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  $Id$
 */

#ifndef LO_ENDIAN_H
#define LO_ENDIAN_H

#include <sys/types.h>
#include <stdint.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define lo_swap16(x) htons(x)

#define lo_swap32(x) htonl(x)

/* These macros come from the Linux kernel */

#ifndef lo_swap16
#define lo_swap16(x) \
({ \
    uint16_t __x = (x); \
    ((uint16_t)( \
	(((uint16_t)(__x) & (uint16_t)0x00ffU) << 8) | \
	(((uint16_t)(__x) & (uint16_t)0xff00U) >> 8) )); \
})
#warning USING UNOPTIMISED ENDIAN STUFF
#endif

#ifndef lo_swap32
#define lo_swap32(x) \
({ \
    uint32_t __x = (x); \
    ((uint32_t)( \
	(((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
	(((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
	(((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
	(((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) )); \
})
#endif

#if 0
#ifndef lo_swap64
#define lo_swap64(x) \
({ \
    uint64_t __x = (x); \
    ((uint64_t)( \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000000000ffULL) << 56) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0xff00000000000000ULL) >> 56) )); \
})
#endif
#else

typedef union {
    uint64_t all;
    struct {
	uint32_t a;
	uint32_t b;
    } part;
} lo_split64;

#ifdef _MSC_VER
#define LO_INLINE __inline
#else
#define LO_INLINE inline
#endif
static LO_INLINE uint64_t lo_swap64(uint64_t x)
{
    lo_split64 in, out;

    in.all = x;
    out.part.a = lo_swap32(in.part.b);
    out.part.b = lo_swap32(in.part.a);

    return out.all;
}
#undef LO_INLINE
#endif

/* When compiling for an Apple universal build, allow compile-time
 * macros to override autoconf endianness settings. */
#ifdef LO_BIGENDIAN
#undef LO_BIGENDIAN
#endif
#if LO_BIGENDIAN == 2
#ifdef __BIG_ENDIAN__
#define LO_BIGENDIAN 1
#else
#ifdef __LITTLE_ENDIAN__
#define LO_BIGENDIAN 0
#else
#error Unknown endianness during Apple universal build.
#endif
#endif
#else
#define LO_BIGENDIAN LO_BIGENDIAN
#endif

/* Host to OSC and OSC to Host conversion macros */
#if LO_BIGENDIAN
#define lo_htoo16(x) (x)
#define lo_htoo32(x) (x)
#define lo_htoo64(x) (x)
#define lo_otoh16(x) (x)
#define lo_otoh32(x) (x)
#define lo_otoh64(x) (x)
#else
#define lo_htoo16 lo_swap16
#define lo_htoo32 lo_swap32
#define lo_htoo64 lo_swap64
#define lo_otoh16 lo_swap16
#define lo_otoh32 lo_swap32
#define lo_otoh64 lo_swap64
#endif

#ifdef __cplusplus
}
#endif

#endif

/* vi:set ts=8 sts=4 sw=4: */
