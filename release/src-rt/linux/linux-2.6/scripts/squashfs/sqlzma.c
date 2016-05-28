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
 * $Id: sqlzma.c,v 1.3 2009/03/10 08:42:14 Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "LzmaDec.h"
#include "LzmaEnc.h"
#include "sqlzma.h"

static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

int LzmaUncompress(char *dst, unsigned long *dstlen, char *src, int srclen)
{
	int res;
	SizeT inSizePure;
	ELzmaStatus status;

	if (srclen < LZMA_PROPS_SIZE)
	{
		memcpy(dst, src, srclen);
		return srclen;
	}
	inSizePure = srclen - LZMA_PROPS_SIZE;
	res = LzmaDecode(dst, (SizeT *) dstlen, src + LZMA_PROPS_SIZE, &inSizePure,
	                 src, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &g_Alloc);
	srclen = inSizePure ;

	if ((res == SZ_OK) ||
		((res == SZ_ERROR_INPUT_EOF) && (srclen == inSizePure)))
		res = 0;
	if (res != SZ_OK)
		printf("LzmaUncompress: error (%d)\n", res);
	return res;
}


int LzmaCompress(char *in_data, int in_size, char *out_data, int out_size, unsigned long *total_out)
{
	CLzmaEncProps props;
	SizeT headerSize = LZMA_PROPS_SIZE;
	SizeT outProcess;
	int ret;

	LzmaEncProps_Init(&props);
	props.level = 5;
	props.dictSize = out_size;
	props.lc = 0;
	props.lp = 2;
	props.pb = 2;
	props.fb = 64;
	props.algo = 1;
	props.btMode = 1;
	props.numHashBytes = 2;

	outProcess = out_size - LZMA_PROPS_SIZE;
	ret = LzmaEncode(out_data+LZMA_PROPS_SIZE, &outProcess, in_data, in_size, &props, out_data, 
						&headerSize, 0, NULL, &g_Alloc, &g_Alloc);
	*total_out = outProcess + LZMA_PROPS_SIZE;
	return ret;
}
