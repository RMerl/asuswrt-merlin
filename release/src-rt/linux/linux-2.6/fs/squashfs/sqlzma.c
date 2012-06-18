#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include "LzmaDec.h"
#include "LzmaDec.c"
#include "sqlzma.h"

static void *SzAlloc(void *p, size_t size) { p = p; return vmalloc(size); }
static void SzFree(void *p, void *address) { p = p; vfree(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

int LzmaUncompress(void *dst, int *dstlen, void *src, int srclen)
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
	res = LzmaDecode(dst, dstlen, src + LZMA_PROPS_SIZE, &inSizePure,
	                 src, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &g_Alloc);
	srclen = inSizePure ;

	if ((res == SZ_OK) ||
		((res == SZ_ERROR_INPUT_EOF) && (srclen == inSizePure)))
		res = 0;
	return res;
}
