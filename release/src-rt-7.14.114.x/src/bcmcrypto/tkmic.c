/*
 *   tkmic.c - TKIP Message Integrity Check (MIC) functions
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: tkmic.c 241182 2011-02-17 21:50:03Z $
 */

#include <typedefs.h>
#include <bcmendian.h>
#include <bcmcrypto/tkmic.h>

/*
 * "Michael" Messge Integrity Check (MIC) algorithm
 */
static INLINE void
tkip_micblock(uint32 *left, uint32 *right)
{
	uint32 l = *left;
	uint32 r = *right;

	/*
	 * Per Henry, we replaced the ROTL with ROTR
	 */
	r ^= ROTR32(l, 15);
	l += r; /* mod 2^32 */
	r ^= XSWAP32(l);
	l += r; /* mod 2^32 */
	r ^= ROTR32(l, 29);
	l += r; /* mod 2^32 */
	r ^= ROTR32(l, 2);
	l += r; /* mod 2^32 */

	*left = l;
	*right = r;
}


/* compute mic across message */
/* buffer must already have terminator and padding appended */
/* buffer length (n) specified in bytes */
void
BCMROMFN(tkip_mic)(uint32 k0, uint32 k1, int n, uint8 *m, uint32 *left, uint32 *right)
{
	uint32 l = k0;
	uint32 r = k1;

	if (((uintptr)m & 3) == 0) {
		for (; n > 0; n -= 4) {
			l ^= ltoh32(*(uint *)m);
			m += 4;
			tkip_micblock(&l, &r);
		}
	} else {
		for (; n > 0; n -= 4) {
			l ^= ltoh32_ua(m);
			m += 4;
			tkip_micblock(&l, &r);
		}
	}
	*left = l;
	*right = r;
}

/* append the MIC terminator to the data buffer */
/* terminator is 0x5a followed by 4-7 bytes of 0 */
/* param 'o' is the current frag's offset in the frame */
/* returns length of message plus terminator in bytes */
int
BCMROMFN(tkip_mic_eom)(uint8 *m, uint n, uint o)
{
	uint8 *mend = m + n;
	uint t = n + o;
	mend[0] = 0x5a;
	mend[1] = 0;
	mend[2] = 0;
	mend[3] = 0;
	mend[4] = 0;
	mend += 5;
	o += n + 5;
	while (o++%4) {
		*mend++ = 0;
	}
	return (n+o-1-t);
}
