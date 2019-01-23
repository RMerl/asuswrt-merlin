/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_ICMP_H
#define LIBIPSET_ICMP_H

#include <stdint.h>				/* uintxx_t */

#ifdef __cplusplus
extern "C" {
#endif

extern const char *id_to_icmp(uint8_t id);
extern const char *icmp_to_name(uint8_t type, uint8_t code);
extern int name_to_icmp(const char *str, uint16_t *typecode);

#ifdef __cplusplus
}
#endif

#endif /* LIBIPSET_ICMP_H */
