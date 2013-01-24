#ifndef _ASM_GENERIC_UNALIGNED_H_
#define _ASM_GENERIC_UNALIGNED_H_

/*
 * For the benefit of those who are trying to port Linux to another
 * architecture, here are some C-language equivalents. 
 *
 * This is based almost entirely upon Richard Henderson's
 * asm-alpha/unaligned.h implementation.  Some comments were
 * taken from David Mosberger's asm-ia64/unaligned.h header.
 */

#include <linux/types.h>

struct __una_u64 { __u64 x; } __attribute__((packed));
struct __una_u32 { __u32 x; } __attribute__((packed));
struct __una_u16 { __u16 x; } __attribute__((packed));

/*
 * Elemental unaligned loads 
 */

static inline __u64 __get_unaligned_cpu64(const void *addr)
{
	const struct __una_u64 *ptr = (const struct __una_u64 *) addr;
	return ptr->x;
}

static inline __u32 __get_unaligned_cpu32(const void *addr)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *) addr;
	return ptr->x;
}

static inline __u16 __get_unaligned_cpu16(const void *addr)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *) addr;
	return ptr->x;
}

/*
 * Elemental unaligned stores 
 */

static inline void __put_unaligned_cpu64(__u64 val, void *addr)
{
	struct __una_u64 *ptr = (struct __una_u64 *) addr;
	ptr->x = val;
}

static inline void __put_unaligned_cpu32(__u32 val, void *addr)
{
	struct __una_u32 *ptr = (struct __una_u32 *) addr;
	ptr->x = val;
}

static inline void __put_unaligned_cpu16(__u16 val, void *addr)
{
	struct __una_u16 *ptr = (struct __una_u16 *) addr;
	ptr->x = val;
}

#include <asm/byteorder.h>

#if defined(__LITTLE_ENDIAN)
# include <linux/unaligned/le_struct.h>
# include <linux/unaligned/be_byteshift.h>
# define get_unaligned  __get_unaligned_le
# define put_unaligned  __put_unaligned_le
#elif defined(__BIG_ENDIAN)
# include <linux/unaligned/be_struct.h>
# include <linux/unaligned/le_byteshift.h>
# define get_unaligned  __get_unaligned_be
# define put_unaligned  __put_unaligned_be
#else
# error need to define endianess
#endif

/*
 * Cause a link-time error if we try an unaligned access other than
 * 1,2,4 or 8 bytes long
 */
extern void __bad_unaligned_access_size(void);

#define __get_unaligned_le(ptr) ((__force typeof(*(ptr)))({			\
	__builtin_choose_expr(sizeof(*(ptr)) == 1, *(ptr),			\
	__builtin_choose_expr(sizeof(*(ptr)) == 2, get_unaligned_le16((ptr)),	\
	__builtin_choose_expr(sizeof(*(ptr)) == 4, get_unaligned_le32((ptr)),	\
	__builtin_choose_expr(sizeof(*(ptr)) == 8, get_unaligned_le64((ptr)),	\
	__bad_unaligned_access_size()))));					\
	}))

#define __get_unaligned_be(ptr) ((__force typeof(*(ptr)))({			\
	__builtin_choose_expr(sizeof(*(ptr)) == 1, *(ptr),			\
	__builtin_choose_expr(sizeof(*(ptr)) == 2, get_unaligned_be16((ptr)),	\
	__builtin_choose_expr(sizeof(*(ptr)) == 4, get_unaligned_be32((ptr)),	\
	__builtin_choose_expr(sizeof(*(ptr)) == 8, get_unaligned_be64((ptr)),	\
	__bad_unaligned_access_size()))));					\
	}))

#define __put_unaligned_le(val, ptr) ({					\
	void *__gu_p = (ptr);						\
	switch (sizeof(*(ptr))) {					\
	case 1:								\
		*(u8 *)__gu_p = (__force u8)(val);			\
		break;							\
	case 2:								\
		put_unaligned_le16((__force u16)(val), __gu_p);		\
		break;							\
	case 4:								\
		put_unaligned_le32((__force u32)(val), __gu_p);		\
		break;							\
	case 8:								\
		put_unaligned_le64((__force u64)(val), __gu_p);		\
		break;							\
	default:							\
		__bad_unaligned_access_size();				\
		break;							\
	}								\
	(void)0; })

#define __put_unaligned_be(val, ptr) ({					\
	void *__gu_p = (ptr);						\
	switch (sizeof(*(ptr))) {					\
	case 1:								\
		*(u8 *)__gu_p = (__force u8)(val);			\
		break;							\
	case 2:								\
		put_unaligned_be16((__force u16)(val), __gu_p);		\
		break;							\
	case 4:								\
		put_unaligned_be32((__force u32)(val), __gu_p);		\
		break;							\
	case 8:								\
		put_unaligned_be64((__force u64)(val), __gu_p);		\
		break;							\
	default:							\
		__bad_unaligned_access_size();				\
		break;							\
	}								\
	(void)0; })

#endif /* _ASM_GENERIC_UNALIGNED_H */
