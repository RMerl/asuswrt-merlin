/* Portions Copyright 2001 Sun Microsystems (thockin@sun.com) */
/* Portions Copyright 2002 Intel (scott.feldman@intel.com) */
#ifndef ETHTOOL_UTIL_H__
#define ETHTOOL_UTIL_H__

#include <sys/types.h>
typedef unsigned long long u64;         /* hack, so we may include kernel's ethtool.h */
typedef __uint32_t u32;         /* ditto */
typedef __uint16_t u16;         /* ditto */
typedef __uint8_t u8;           /* ditto */
#include "ethtool-copy.h"
#endif
