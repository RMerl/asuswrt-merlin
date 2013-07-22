/*
 * Implements the standard CRC-16:
 *   Width 16
 *   Poly  0x8005 (x^16 + x^15 + x^2 + 1)
 *   Init  0
 *
 * Copyright (c) 2005 Ben Gardner <bgardner@wabtec.com>
 *
 * This code was taken from the linux kernel. The license is GPL Version 2.
 */

#ifndef __CRC16_H__
#define __CRC16_H__

#include <stdlib.h>
#include <stdint.h>

extern uint16_t const crc16_table[256];

extern uint16_t crc16(uint16_t crc, const uint8_t *buffer, size_t len);

static inline uint16_t crc16_byte(uint16_t crc, const uint8_t data)
{
	return (crc >> 8) ^ crc16_table[(crc ^ data) & 0xff];
}

#endif /* __CRC16_H__ */
