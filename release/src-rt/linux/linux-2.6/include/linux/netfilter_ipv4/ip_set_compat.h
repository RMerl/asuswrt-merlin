#ifndef _IP_SET_COMPAT_H
#define _IP_SET_COMPAT_H

#ifdef __KERNEL__
#include <linux/version.h>

/* Arrgh */
#ifdef MODULE
#define __MOD_INC(foo)		__MOD_INC_USE_COUNT(foo)
#define __MOD_DEC(foo)		__MOD_DEC_USE_COUNT(foo)
#else
#define __MOD_INC(foo)		1
#define __MOD_DEC(foo)
#endif

/* Backward compatibility */
#ifndef __nocast
#define __nocast
#endif
#ifndef __bitwise__
#define __bitwise__
#endif

/* Compatibility glue code */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
#define __KMEM_CACHE_T__	kmem_cache_t
#else
#define __KMEM_CACHE_T__	struct kmem_cache
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
#define ip_hdr(skb)		((skb)->nh.iph)
#define skb_mac_header(skb)	((skb)->mac.raw)
#define eth_hdr(skb)		((struct ethhdr *)skb_mac_header(skb))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
#include <linux/netfilter.h>
#define KMEM_CACHE_CREATE(name, size) \
	kmem_cache_create(name, size, 0, 0, NULL, NULL)
#else
#define KMEM_CACHE_CREATE(name, size) \
	kmem_cache_create(name, size, 0, 0, NULL)
#endif

#ifndef NIPQUAD
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]
#endif

#ifndef HIPQUAD
#if defined(__LITTLE_ENDIAN)
#define HIPQUAD(addr) \
	((unsigned char *)&addr)[3], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[0]
#elif defined(__BIG_ENDIAN)
#define HIPQUAD NIPQUAD
#else
#error "Please fix asm/byteorder.h"
#endif /* __LITTLE_ENDIAN */
#endif

#endif /* __KERNEL__ */
#endif /* _IP_SET_COMPAT_H */   
