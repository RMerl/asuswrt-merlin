#ifndef _I386_BYTEORDER_H
#define _I386_BYTEORDER_H

#include <asm/types.h>
#include <linux/compiler.h>

#ifdef __GNUC__

/* For avoiding bswap on i386 */
#ifdef __KERNEL__
#endif

static __inline__ __attribute_const__ __u32 ___arch__swab32(__u32 x)
{
#ifdef CONFIG_X86_BSWAP
	__asm__("bswap %0" : "=r" (x) : "0" (x));
#else
	__asm__("xchgb %b0,%h0\n\t"	/* swap lower bytes	*/
		"rorl $16,%0\n\t"	/* swap words		*/
		"xchgb %b0,%h0"		/* swap higher bytes	*/
		:"=q" (x)
		: "0" (x));
#endif
	return x;
}

static __inline__ __attribute_const__ __u64 ___arch__swab64(__u64 val)
{ 
	union { 
		struct { __u32 a,b; } s;
		__u64 u;
	} v;
	v.u = val;
#ifdef CONFIG_X86_BSWAP
	asm("bswapl %0 ; bswapl %1 ; xchgl %0,%1" 
	    : "=r" (v.s.a), "=r" (v.s.b) 
	    : "0" (v.s.a), "1" (v.s.b)); 
#else
   v.s.a = ___arch__swab32(v.s.a); 
	v.s.b = ___arch__swab32(v.s.b); 
	asm("xchgl %0,%1" : "=r" (v.s.a), "=r" (v.s.b) : "0" (v.s.a), "1" (v.s.b));
#endif
	return v.u;	
} 

/* Do not define swab16.  Gcc is smart enough to recognize "C" version and
   convert it into rotation or exhange.  */

#define __arch__swab64(x) ___arch__swab64(x)
#define __arch__swab32(x) ___arch__swab32(x)

#define __BYTEORDER_HAS_U64__

#endif /* __GNUC__ */

#include <linux/byteorder/little_endian.h>

#endif /* _I386_BYTEORDER_H */
