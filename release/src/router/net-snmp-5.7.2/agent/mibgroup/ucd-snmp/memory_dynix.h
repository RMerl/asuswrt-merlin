/*
 *  memory quantity mib groups
 *
 */
#ifndef _MIBGROUP_MEMORY_DYNIX_H
#define _MIBGROUP_MEMORY_DYNIX_H

config_require(util_funcs/header_generic)

#include "mibdefs.h"

/*
 * from /usr/include/sys/mc_param.h 
 */
#define MMU_PAGESIZE 0x1000     /* 4096 bytes */

/*
 * Here's the correct way to convert sectors to KB
 * #define S2KB(size) (((size)*DEV_BSIZE)/1024)
 * Here's the quick way plus no fear of overflow
 */
#define S2KB(size)  ((size)/2)  /* sectors to KB */
#define S2MB(size)  (((size)+1023)/2048)        /* sectors to MB */
#define P2KB(size)  ((size)*(MMU_PAGESIZE/1024))        /* pages to KB */
#define P2MB(size)  ((P2KB(size)+511)/1024)     /* pages to MB */

void            init_memory_dynix(void);

#endif                          /* _MIBGROUP_MEMORY_DYNIX_H */
