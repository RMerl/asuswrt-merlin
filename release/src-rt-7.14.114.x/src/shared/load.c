/*
 * Initialization and support routines for self-booting
 * compressed image.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: load.c 410377 2013-07-01 01:34:53Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <siutils.h>
#include <sbchipc.h>
#include <bcmnvram.h>
#ifdef NFLASH_SUPPORT
#include <nflash.h>
#endif

extern void cpu_inv_cache_all(void);

void c_main(unsigned long ra);

extern unsigned char text_start[], text_end[];
extern unsigned char data_start[], data_end[];
extern char bss_start[], bss_end[];

#define INBUFSIZ 4096		/* Buffer size */
#define WSIZE 0x8000    	/* window size--must be a power of two, and */
				/*  at least 32K for zip's deflate method */

static uchar *inbuf;		/* input buffer */
#if !defined(USE_LZMA)
static ulong insize;		/* valid bytes in inbuf */
static ulong inptr;		/* index of next byte to be processed in inbuf */
#endif /* USE_GZIP */

static uchar *outbuf;		/* output buffer */
static ulong bytes_out;		/* valid bytes in outbuf */

static uint32 *inbase;		/* input data from flash */

#if !defined(USE_LZMA)
static int
fill_inbuf(void)
{
	for (insize = 0; insize < INBUFSIZ; insize += sizeof(uint32), inbase++)
		*((uint32 *)&inbuf[insize]) = *inbase;
	inptr = 1;

	return inbuf[0];
}

/* Defines for gzip/bzip */
#define	malloc(size)	MALLOC(NULL, size)
#define	free(addr)	MFREE(NULL, addr, 0)

static void
error(char *x)
{
	printf("\n\n%s\n\n -- System halted", x);

	for (;;);
}
#endif /* USE_LZMA */

#if defined(USE_GZIP)
extern int _memsize;
/*
 * gzip declarations
 */

#define OF(args) args
#define STATIC static

#define memzero(s, n)	memset ((s), 0, (n))

typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;

#define get_byte()  (inptr < insize ? inbuf[inptr++] : fill_inbuf())

/* Diagnostic functions (stubbed out) */

#define Assert(cond, msg)
#define Trace(x)
#define Tracev(x)
#define Tracevv(x)
#define Tracec(c, x)
#define Tracecv(c, x)

static uchar *window;		/* Sliding window buffer */
static unsigned outcnt;		/* bytes in window buffer */

static void
gzip_mark(void **ptr)
{
	/* I'm not sure what the pourpose of this is, there are no malloc
	 * calls without free's in the gunzip code.
	 */
}

static void
gzip_release(void **ptr)
{
}

static void flush_window(void);

#include "gzip_inflate.c"

/* ===========================================================================
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
static void
flush_window(void)
{
	ulg c = crc;
	unsigned n;
	uch *in, *out, ch;

	in = window;
	out = &outbuf[bytes_out];
	for (n = 0; n < outcnt; n++) {
		ch = *out++ = *in++;
		c = crc_32_tab[((int)c ^ ch) & 0xff] ^ (c >> 8);
	}
	crc = c;
	bytes_out += (ulg)outcnt;
	outcnt = 0;
	putc('.');
}

#elif defined(USE_BZIP2)

#include "bzip2_inflate.c"

/*
 * bzip2 declarations
 */

void bz_internal_error(int i)
{
	char msg[128];

	sprintf(msg, "Bzip2 internal error: %d", i);
	error(msg);
}

static int
bunzip2(void)
{
	bz_stream bzstream;
	int ret = 0;

	bzstream.bzalloc = 0;
	bzstream.bzfree = 0;
	bzstream.opaque = 0;
	bzstream.avail_in = 0;

	if ((ret = BZ2_bzDecompressInit(&bzstream, 0, 1)) != BZ_OK)
		return ret;

	for (;;) {
		if (bzstream.avail_in == 0) {
			fill_inbuf();
			bzstream.next_in = inbuf;
			bzstream.avail_in = insize;
		}
		bzstream.next_out = &outbuf[bytes_out];
		bzstream.avail_out = WSIZE;
		if ((ret = BZ2_bzDecompress(&bzstream)) != BZ_OK)
			break;
		bytes_out = bzstream.total_out_lo32;
		putc('.');
	}

	if (ret == BZ_STREAM_END)
		ret = BZ2_bzDecompressEnd(&bzstream);

	if (ret == BZ_OK)
		ret = 0;

	return ret;
}
#elif defined(USE_LZMA)

#include "LzmaDec.c"
#define LZMA_HEADER_SIZE (LZMA_PROPS_SIZE + 8)

static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

extern int _memsize;
/*
 * call LZMA decoder to decompress a LZMA block
 */
static int
decompressLZMA(unsigned char *src, unsigned int srcLen, unsigned char *dest, unsigned int destLen)
{
	int res;
	SizeT inSizePure;
	ELzmaStatus status;
	SizeT outSize;

	if (srcLen < LZMA_HEADER_SIZE)
		return SZ_ERROR_INPUT_EOF;

	inSizePure = srcLen - LZMA_HEADER_SIZE;
	outSize = destLen;
	res = LzmaDecode(dest, &outSize, src + LZMA_HEADER_SIZE, &inSizePure,
	                 src, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &g_Alloc);
	srcLen = inSizePure + LZMA_HEADER_SIZE;

	if ((res == SZ_OK) ||
		((res == SZ_ERROR_INPUT_EOF) && (srcLen == inSizePure + LZMA_HEADER_SIZE)))
		res = 0;
	return res;
}
#else
extern int _memsize;

#endif /* defined(USE_GZIP) */

extern char input_data[], input_data_end[];
extern int input_len;

static void
load(si_t *sih)
{
	int ret = 0;
	uint32 image_len;
#ifdef	CONFIG_XIP
	int inoff;

	inoff = ((ulong)text_end - (ulong)text_start) + ((ulong)input_data - (ulong)data_start);
	if (sih->ccrev == 12)
		inbase = OSL_UNCACHED(SI_FLASH2 + inoff);
	else
		inbase = OSL_CACHED(SI_FLASH2 + inoff);
	image_len = input_len;
#else
#if defined(CFG_SHMOO)
	int inoff;
	int bootdev;

	inoff = (ulong)input_data - (ulong)text_start;
	bootdev = soc_boot_dev((void *)sih);
	if (bootdev == SOC_BOOTDEV_NANDFLASH)
		inbase = (uint32 *)(SI_NS_NANDFLASH + inoff);
	else
		inbase = (uint32 *)(SI_NS_NORFLASH + inoff);
	image_len = *(uint32 *)((ulong)inbase - 4);
#else
	inbase = (uint32 *)input_data;
	image_len = input_len;
#endif /* CFG_SHMOO */
#endif /* CONFIG_XIP */

	outbuf = (uchar *)LOADADDR;
	bytes_out = 0;
	inbuf = malloc(INBUFSIZ);	/* input buffer */

#if defined(USE_GZIP)
	window = malloc(WSIZE);
	printf("Decompressing...");
	makecrc();
	ret = gunzip();
#elif defined(USE_BZIP2)
	/* Small decompression algorithm uses up to 2300k of memory */
	printf("Decompressing...");
	ret = bunzip2();
#elif defined(USE_LZMA)
	printf("Decompressing...");
	bytes_out = (ulong)_memsize - (ulong)PHYSADDR(outbuf);
	ret = decompressLZMA((unsigned char *)inbase, image_len, outbuf, bytes_out);
#else
	printf("Copying...");
	while (bytes_out < image_len) {
		fill_inbuf();
		memcpy(&outbuf[bytes_out], inbuf, insize);
		bytes_out += insize;
	}
#endif /* defined(USE_GZIP) */
	if (ret) {
		printf("error %d\n", ret);
	} else
		printf("done\n");
}

static void
set_sflash_div(si_t *sih)
{
	uint idx = si_coreidx(sih);
	osl_t *osh = si_osh(sih);
	chipcregs_t *cc;
	struct nvram_header *nvh = NULL;
	uintptr flbase;
	uint32 fltype, off, clkdiv, bpclock, sflmaxclk, sfldiv;

	/* Check for sflash */
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);

#ifdef NFLASH_SUPPORT
	if ((sih->ccrev == 38) && ((sih->chipst & (1 << 4)) != 0))
		goto out;
#endif /* NFLASH_SUPPORT */
	fltype = sih->cccaps & CC_CAP_FLASH_MASK;
	if ((fltype != SFLASH_ST) && (fltype != SFLASH_AT))
		goto out;

	flbase = (uintptr)OSL_UNCACHED((void *)SI_FLASH2);
	off = FLASH_MIN;
	while (off <= 16 * 1024 * 1024) {
		nvh = (struct nvram_header *)(flbase + off - MAX_NVRAM_SPACE);
		if (R_REG(osh, &nvh->magic) == NVRAM_MAGIC)
			break;
		off +=  DEF_NVRAM_SPACE;
		nvh = NULL;
	};

	if (nvh == NULL) {
		nvh = (struct nvram_header *)(flbase + 1024);
		if (R_REG(osh, &nvh->magic) != NVRAM_MAGIC) {
			goto out;
		}
	}

	sflmaxclk = R_REG(osh, &nvh->crc_ver_init) >> 16;
	if ((sflmaxclk == 0xffff) || (sflmaxclk == 0x0419))
		goto out;

	sflmaxclk &= 0xf;
	if (sflmaxclk == 0)
		goto out;

	bpclock = si_clock(sih);
	sflmaxclk *= 10000000;
	for (sfldiv = 2; sfldiv < 16; sfldiv += 2) {
		if ((bpclock / sfldiv) < sflmaxclk)
			break;
	}
	if (sfldiv > 14)
		sfldiv = 14;

	clkdiv = R_REG(osh, &cc->clkdiv);
	if (((clkdiv & CLKD_SFLASH) >> CLKD_SFLASH_SHIFT) != sfldiv) {
		clkdiv = (clkdiv & ~CLKD_SFLASH) | (sfldiv << CLKD_SFLASH_SHIFT);
		W_REG(osh, &cc->clkdiv, clkdiv);
	}

out:
	si_setcoreidx(sih, idx);
	return;
}

void
c_main(unsigned long ra)
{
	si_t *sih;

	BCMDBG_TRACE(0x4c4400);

#ifndef CFG_UNCACHED
	/* Discover cache configuration and if not already on,
	 * initialize and turn them on.
	 */
#ifndef CFG_SHMOO
	caches_on();
#endif
#endif /* CFG_UNCACHED */

	BCMDBG_TRACE(0x4c4401);

	/* Basic initialization */
	sih = (si_t *)osl_init();

	BCMDBG_TRACE(0x4c4402);

	/* Only do this for 4716, we need to reuse the
	 * space in the nvram header for TREF on 5357.
	 */
	if ((CHIPID(sih->chip) == BCM4716_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4748_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM47162_CHIP_ID))
		set_sflash_div(sih);

	BCMDBG_TRACE(0x4c4403);

	/* Load binary */
	load(sih);

	BCMDBG_TRACE(0x4c4404);

	/* Flush all caches */
	blast_dcache();
	blast_icache();

	BCMDBG_TRACE(0x4c4405);

	/* Jump to load address */
	((void (*)(void))LOADADDR)();
}
