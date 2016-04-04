#ifndef _SAFE_MEMCLEAR_H_
#define _SAFE_MEMCLEAR_H_

/* size_t */
#include <sys/types.h>

void safe_memclear(void *s, size_t n);

#endif
