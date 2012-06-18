/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1999, 2000, 2001, 2003 by Ralf Baechle
 * Copyright (C) 1999, 2000, 2001 Silicon Graphics, Inc.
 */
#ifndef _ASM_UNALIGNED_H
#define _ASM_UNALIGNED_H

#include <linux/types.h>

/*
 * get_unaligned - get value from possibly mis-aligned location
 * @ptr: pointer to value
 *
 * This macro should be used for accessing values larger in size than
 * single bytes at locations that are expected to be improperly aligned,
 * e.g. retrieving a u16 value from a location not u16-aligned.
 *
 * Note that unaligned accesses can be very expensive on some architectures.
 */
#define get_unaligned(ptr) \
	((__typeof__(*(ptr)))__get_unaligned((ptr), sizeof(*(ptr))))

/*
 * put_unaligned - put value to a possibly mis-aligned location
 * @val: value to place
 * @ptr: pointer to location
 *
 * This macro should be used for placing values larger in size than
 * single bytes at locations that are expected to be improperly aligned,
 * e.g. writing a u16 value to a location not u16-aligned.
 *
 * Note that unaligned accesses can be very expensive on some architectures.
 */
#define put_unaligned(x,ptr) \
	__put_unaligned((__u64)(x), (ptr), sizeof(*(ptr)))

/*
 * This is a silly but good way to make sure that
 * the get/put functions are indeed always optimized,
 * and that we use the correct sizes.
 */
extern void bad_unaligned_access_length(void);

/*
 * EGCS 1.1 knows about arbitrary unaligned loads.  Define some
 * packed structures to talk about such things with.
 */

struct __una_u64 { __u64 x __attribute__((packed)); };
struct __una_u32 { __u32 x __attribute__((packed)); };
struct __una_u16 { __u16 x __attribute__((packed)); };

/*
 * Elemental unaligned loads
 */

static inline __u64 __uldq(const __u64 * r11)
{
	const struct __una_u64 *ptr = (const struct __una_u64 *) r11;
	return ptr->x;
}

static inline __u32 __uldl(const __u32 * r11)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *) r11;
	return ptr->x;
}

static inline __u16 __uldw(const __u16 * r11)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *) r11;
	return ptr->x;
}

/*
 * Elemental unaligned stores
 */

static inline void __ustq(__u64 r5, __u64 * r11)
{
	struct __una_u64 *ptr = (struct __una_u64 *) r11;
	ptr->x = r5;
}

static inline void __ustl(__u32 r5, __u32 * r11)
{
	struct __una_u32 *ptr = (struct __una_u32 *) r11;
	ptr->x = r5;
}

static inline void __ustw(__u16 r5, __u16 * r11)
{
	struct __una_u16 *ptr = (struct __una_u16 *) r11;
	ptr->x = r5;
}

static inline __u64 __get_unaligned(const void *ptr, size_t size)
{
	__u64 val;

	switch (size) {
	case 1:
		val = *(const __u8 *)ptr;
		break;
	case 2:
		val = __uldw((const __u16 *)ptr);
		break;
	case 4:
		val = __uldl((const __u32 *)ptr);
		break;
	case 8:
		val = __uldq((const __u64 *)ptr);
		break;
	default:
		bad_unaligned_access_length();
	}
	return val;
}

static inline void __put_unaligned(__u64 val, void *ptr, size_t size)
{
	switch (size) {
	      case 1:
		*(__u8 *)ptr = (val);
	        break;
	      case 2:
		__ustw(val, (__u16 *)ptr);
		break;
	      case 4:
		__ustl(val, (__u32 *)ptr);
		break;
	      case 8:
		__ustq(val, (__u64 *)ptr);
		break;
	      default:
	    	bad_unaligned_access_length();
	}
}

#endif /* _ASM_UNALIGNED_H */
