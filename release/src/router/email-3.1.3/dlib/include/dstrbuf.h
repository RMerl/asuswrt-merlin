#ifndef __DSTRBUF_H
#define __DSTRBUF_H 
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include "dlib.h"

typedef struct __dstrbuf {
	char *str;
	size_t size;
	size_t len;
} dstrbuf;

dstrbuf *dsbNew(size_t size);
void dsbResize(dstrbuf *dsb, size_t newsize);
void dsbDestroy(dstrbuf *dsb);
void dsbClear(dstrbuf *dsb);

size_t dsbCopy(dstrbuf *dsb, const char *buf);
size_t dsbnCopy(dstrbuf *dsb, const char *buf, size_t size);

size_t dsbCat(dstrbuf *dest, const char *src);
size_t dsbnCat(dstrbuf *dest, const char *src, size_t size);
void dsbCatChar(dstrbuf *dest, const u_char ch);

int dsbPrintf(dstrbuf *dsb, const char *fmt, ...);

size_t dsbReadline(dstrbuf *dsb, FILE *file);
size_t dsbFread(dstrbuf *dsb, size_t bytes, FILE *file);
ssize_t dsbRead(dstrbuf *dsb, size_t size, int fd);

#define DSB_NEW dsbNew(100)

#endif

