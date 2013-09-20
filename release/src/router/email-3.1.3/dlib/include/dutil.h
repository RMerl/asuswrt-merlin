#ifndef __DUTIL_H
#define __DUTIL_H 1

#include "dstrbuf.h"
#include "dvector.h"

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);

size_t strstrip(char *str, char *items);
char *substr(const char *str, int start, size_t len);

int nextprime(int n);
int strfind(const char *str, char ch);

dvector explode(const char *str, const char *delim);
dstrbuf *implode(dvector vec, char delim);

size_t filesize(const char *file);
void chomp(char *str);

void __xfree(void *ptr);
#define xfree(ptr) __xfree(ptr); ptr = NULL

#endif

