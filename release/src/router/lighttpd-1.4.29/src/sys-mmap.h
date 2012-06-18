#ifndef WIN32_MMAP_H
#define WIN32_MMAP_H

#ifdef __WIN32

#define MAP_FAILED -1
#define PROT_SHARED 0
#define MAP_SHARED 0
#define PROT_READ 0

#define mmap(a, b, c, d, e, f) (-1)
#define munmap(a, b) (-1)

#include <windows.h>

#else
#include <sys/mman.h>

#ifndef MAP_FAILED
#define MAP_FAILED -1
#endif
#endif

#endif
