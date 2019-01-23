/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_ICMPV6_H
#define LIBIPSET_ICMPV6_H

#include <stdint.h>				/* uintxx_t */

#ifdef __cplusplus
extern "C" {
#endif

extern const char *id_to_icmpv6(uint8_t id);
extern const char *icmpv6_to_name(uint8_t type, uint8_t code);
extern int name_to_icmpv6(const char *str, uint16_t *typecode);

#ifdef __cplusplus
}
#endif

#endif /* LIBIPSET_ICMPV6_H */
