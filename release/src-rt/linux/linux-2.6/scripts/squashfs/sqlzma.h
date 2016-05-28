/*
 * squashfs with lzma compression
 *
 * Copyright (C) 2008, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: sqlzma.h,v 1.3 2009/03/10 08:42:14 Exp $
 */
#ifndef __sqlzma_h__
#define __sqlzma_h__

#ifndef __KERNEL__
#include <stdlib.h>
#include <string.h>
#endif

/*
 * detect the compression method automatically by the first byte of compressed
 * data.
 */
#define is_lzma(c)	(c == 0x5d)

int LzmaUncompress(char *dst, unsigned long *dstlen, char *src, int srclen);
int LzmaCompress(char *in_data, int in_size, char *out_data, int out_size, unsigned long *total_out);

#endif
