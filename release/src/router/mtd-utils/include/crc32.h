/*
 * This code was taken from the linux kernel. The license is GPL Version 2.
 */

#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>

/* Return a 32-bit CRC of the contents of the buffer */
extern uint32_t mtd_crc32(uint32_t val, const void *ss, int len);

#endif /* __CRC32_H__ */
