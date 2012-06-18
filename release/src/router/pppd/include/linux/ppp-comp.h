/*
 * ppp-comp.h - Definitions for doing PPP packet compression.
 *
 * Copyright (c) 1984 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: ppp-comp.h,v 1.10 2002/12/06 09:49:15 paulus Exp $
 */

/*
 *  ==FILEVERSION 20020715==
 *
 *  NOTE TO MAINTAINERS:
 *     If you modify this file at all, please set the above date.
 *     ppp-comp.h is shipped with a PPP distribution as well as with the kernel;
 *     if everyone increases the FILEVERSION number above, then scripts
 *     can do the right thing when deciding whether to install a new ppp-comp.h
 *     file.  Don't change the format of that line otherwise, so the
 *     installation script can recognize it.
 */

#ifndef _NET_PPP_COMP_H
#define _NET_PPP_COMP_H

/*
 * The following symbols control whether we include code for
 * various compression methods.
 */

#ifndef DO_BSD_COMPRESS
#define DO_BSD_COMPRESS	1	/* by default, include BSD-Compress */
#endif
#ifndef DO_DEFLATE
#define DO_DEFLATE	1	/* by default, include Deflate */
#endif
#define DO_PREDICTOR_1	0
#define DO_PREDICTOR_2	0

/*
 * Structure giving methods for compression/decompression.
 */

struct compressor {
	int	compress_proto;	/* CCP compression protocol number */

	/* Allocate space for a compressor (transmit side) */
	void	*(*comp_alloc) (unsigned char *options, int opt_len);

	/* Free space used by a compressor */
	void	(*comp_free) (void *state);

	/* Initialize a compressor */
	int	(*comp_init) (void *state, unsigned char *options,
			      int opt_len, int unit, int opthdr, int debug);

	/* Reset a compressor */
	void	(*comp_reset) (void *state);

	/* Compress a packet */
	int     (*compress) (void *state, unsigned char *rptr,
			      unsigned char *obuf, int isize, int osize);

	/* Return compression statistics */
	void	(*comp_stat) (void *state, struct compstat *stats);

	/* Allocate space for a decompressor (receive side) */
	void	*(*decomp_alloc) (unsigned char *options, int opt_len);

	/* Free space used by a decompressor */
	void	(*decomp_free) (void *state);

	/* Initialize a decompressor */
	int	(*decomp_init) (void *state, unsigned char *options,
				int opt_len, int unit, int opthdr, int mru,
				int debug);

	/* Reset a decompressor */
	void	(*decomp_reset) (void *state);

	/* Decompress a packet. */
	int	(*decompress) (void *state, unsigned char *ibuf, int isize,
				unsigned char *obuf, int osize);

	/* Update state for an incompressible packet received */
	void	(*incomp) (void *state, unsigned char *ibuf, int icnt);

	/* Return decompression statistics */
	void	(*decomp_stat) (void *state, struct compstat *stats);
};

/*
 * The return value from decompress routine is the length of the
 * decompressed packet if successful, otherwise DECOMP_ERROR
 * or DECOMP_FATALERROR if an error occurred.
 * 
 * We need to make this distinction so that we can disable certain
 * useful functionality, namely sending a CCP reset-request as a result
 * of an error detected after decompression.  This is to avoid infringing
 * a patent held by Motorola.
 * Don't you just lurve software patents.
 */

#define DECOMP_ERROR		-1	/* error detected before decomp. */
#define DECOMP_FATALERROR	-2	/* error detected after decomp. */

/*
 * CCP codes.
 */

#define CCP_CONFREQ	1
#define CCP_CONFACK	2
#define CCP_TERMREQ	5
#define CCP_TERMACK	6
#define CCP_RESETREQ	14
#define CCP_RESETACK	15

/*
 * Max # bytes for a CCP option
 */

#define CCP_MAX_OPTION_LENGTH	32

/*
 * Parts of a CCP packet.
 */

#define CCP_CODE(dp)		((dp)[0])
#define CCP_ID(dp)		((dp)[1])
#define CCP_LENGTH(dp)		(((dp)[2] << 8) + (dp)[3])
#define CCP_HDRLEN		4

#define CCP_OPT_CODE(dp)	((dp)[0])
#define CCP_OPT_LENGTH(dp)	((dp)[1])
#define CCP_OPT_MINLEN		2

/*
 * Definitions for BSD-Compress.
 */

#define CI_BSD_COMPRESS		21	/* config. option for BSD-Compress */
#define CILEN_BSD_COMPRESS	3	/* length of config. option */

/* Macros for handling the 3rd byte of the BSD-Compress config option. */
#define BSD_NBITS(x)		((x) & 0x1F)	/* number of bits requested */
#define BSD_VERSION(x)		((x) >> 5)	/* version of option format */
#define BSD_CURRENT_VERSION	1		/* current version number */
#define BSD_MAKE_OPT(v, n)	(((v) << 5) | (n))

#define BSD_MIN_BITS		9	/* smallest code size supported */
#define BSD_MAX_BITS		15	/* largest code size supported */

/*
 * Definitions for Deflate.
 */

#define CI_DEFLATE		26	/* config option for Deflate */
#define CI_DEFLATE_DRAFT	24	/* value used in original draft RFC */
#define CILEN_DEFLATE		4	/* length of its config option */

#define DEFLATE_MIN_SIZE	8
#define DEFLATE_MAX_SIZE	15
#define DEFLATE_METHOD_VAL	8
#define DEFLATE_SIZE(x)		(((x) >> 4) + DEFLATE_MIN_SIZE)
#define DEFLATE_METHOD(x)	((x) & 0x0F)
#define DEFLATE_MAKE_OPT(w)	((((w) - DEFLATE_MIN_SIZE) << 4) \
				 + DEFLATE_METHOD_VAL)
#define DEFLATE_CHK_SEQUENCE	0

/*
 * Definitions for MPPE.
 */

#define CI_MPPE			18	/* config option for MPPE */
#define CILEN_MPPE		6	/* length of config option */

/* MPPE/MPPC definitions by J.D.*/
#define MPPE_STATELESS          MPPE_H_BIT	/* configuration bit H */
#define MPPE_40BIT              MPPE_L_BIT	/* configuration bit L */
#define MPPE_56BIT              MPPE_M_BIT	/* configuration bit M */
#define MPPE_128BIT             MPPE_S_BIT	/* configuration bit S */
#define MPPE_MPPC               MPPE_C_BIT	/* configuration bit C */

/*
 * Definitions for Stac LZS.
 */

#define CI_LZS			17	/* config option for Stac LZS */
#define CILEN_LZS		5	/* length of config option */

#define LZS_OVHD		4	/* max. LZS overhead */
#define LZS_HIST_LEN		2048	/* LZS history size */
#define LZS_MAX_CCOUNT		0x0FFF	/* max. coherency counter value */

#define LZS_MODE_NONE		0
#define LZS_MODE_LCB		1
#define LZS_MODE_CRC		2
#define LZS_MODE_SEQ		3
#define LZS_MODE_EXT		4

#define LZS_EXT_BIT_FLUSHED	0x80	/* bit A */
#define LZS_EXT_BIT_COMP	0x20	/* bit C */

/*
 * Definitions for other, as yet unsupported, compression methods.
 */

#define CI_PREDICTOR_1		1	/* config option for Predictor-1 */
#define CILEN_PREDICTOR_1	2	/* length of its config option */
#define CI_PREDICTOR_2		2	/* config option for Predictor-2 */
#define CILEN_PREDICTOR_2	2	/* length of its config option */

#endif /* _NET_PPP_COMP_H */
