/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright 2002-2004 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * Added helpers for unaligned native int access
 */

/*
 * raid6int1.c
 *
 * 1-way unrolled portable integer math RAID-6 instruction set
 *
 * This file was postprocessed using unroll.pl and then ported to userspace
 */
#include <stdint.h>
#include <unistd.h>
#include "kerncompat.h"
#include "ctree.h"
#include "disk-io.h"
#include "volumes.h"
#include "utils.h"

/*
 * This is the C data type to use
 */

/* Change this from BITS_PER_LONG if there is something better... */
#if BITS_PER_LONG == 64
# define NBYTES(x) ((x) * 0x0101010101010101UL)
# define NSIZE  8
# define NSHIFT 3
typedef uint64_t unative_t;
#define put_unaligned_native(val,p)	put_unaligned_64((val),(p))
#define get_unaligned_native(p)		get_unaligned_64((p))
#else
# define NBYTES(x) ((x) * 0x01010101U)
# define NSIZE  4
# define NSHIFT 2
typedef uint32_t unative_t;
#define put_unaligned_native(val,p)	put_unaligned_32((val),(p))
#define get_unaligned_native(p)		get_unaligned_32((p))
#endif

/*
 * These sub-operations are separate inlines since they can sometimes be
 * specially optimized using architecture-specific hacks.
 */

/*
 * The SHLBYTE() operation shifts each byte left by 1, *not*
 * rolling over into the next byte
 */
static inline __attribute_const__ unative_t SHLBYTE(unative_t v)
{
	unative_t vv;

	vv = (v << 1) & NBYTES(0xfe);
	return vv;
}

/*
 * The MASK() operation returns 0xFF in any byte for which the high
 * bit is 1, 0x00 for any byte for which the high bit is 0.
 */
static inline __attribute_const__ unative_t MASK(unative_t v)
{
	unative_t vv;

	vv = v & NBYTES(0x80);
	vv = (vv << 1) - (vv >> 7); /* Overflow on the top bit is OK */
	return vv;
}


void raid6_gen_syndrome(int disks, size_t bytes, void **ptrs)
{
	uint8_t **dptr = (uint8_t **)ptrs;
	uint8_t *p, *q;
	int d, z, z0;

	unative_t wd0, wq0, wp0, w10, w20;

	z0 = disks - 3;		/* Highest data disk */
	p = dptr[z0+1];		/* XOR parity */
	q = dptr[z0+2];		/* RS syndrome */

	for ( d = 0 ; d < bytes ; d += NSIZE*1 ) {
		wq0 = wp0 = get_unaligned_native(&dptr[z0][d+0*NSIZE]);
		for ( z = z0-1 ; z >= 0 ; z-- ) {
			wd0 = get_unaligned_native(&dptr[z][d+0*NSIZE]);
			wp0 ^= wd0;
			w20 = MASK(wq0);
			w10 = SHLBYTE(wq0);
			w20 &= NBYTES(0x1d);
			w10 ^= w20;
			wq0 = w10 ^ wd0;
		}
		put_unaligned_native(wp0, &p[d+NSIZE*0]);
		put_unaligned_native(wq0, &q[d+NSIZE*0]);
	}
}

static void xor_range(char *dst, const char*src, size_t size)
{
	/* Move to DWORD aligned */
	while (size && ((unsigned long)dst & sizeof(unsigned long))) {
		*dst++ ^= *src++;
		size--;
	}

	/* DWORD aligned part */
	while (size >= sizeof(unsigned long)) {
		*(unsigned long *)dst ^= *(unsigned long *)src;
		src += sizeof(unsigned long);
		dst += sizeof(unsigned long);
		size -= sizeof(unsigned long);
	}
	/* Remaining */
	while (size) {
		*dst++ ^= *src++;
		size--;
	}
}

/*
 * Generate desired data/parity stripe for RAID5
 *
 * @nr_devs:	Total number of devices, including parity
 * @stripe_len:	Stripe length
 * @data:	Data, with special layout:
 * 		data[0]:	 Data stripe 0
 * 		data[nr_devs-2]: Last data stripe
 * 		data[nr_devs-1]: RAID5 parity
 * @dest:	To generate which data. should follow above data layout
 */
int raid5_gen_result(int nr_devs, size_t stripe_len, int dest, void **data)
{
	int i;
	char *buf = data[dest];

	/* Validation check */
	if (stripe_len <= 0 || stripe_len != BTRFS_STRIPE_LEN) {
		error("invalid parameter for %s", __func__);
		return -EINVAL;
	}

	if (dest >= nr_devs || nr_devs < 2) {
		error("invalid parameter for %s", __func__);
		return -EINVAL;
	}
	/* Shortcut for 2 devs RAID5, which is just RAID1 */
	if (nr_devs == 2) {
		memcpy(data[dest], data[1 - dest], stripe_len);
		return 0;
	}
	memset(buf, 0, stripe_len);
	for (i = 0; i < nr_devs; i++) {
		if (i == dest)
			continue;
		xor_range(buf, data[i], stripe_len);
	}
	return 0;
}
