/***************************************************************************
 * Linux PPP over X - Generic PPP transport layer sockets
 * Linux PPP over Ethernet (PPPoE) Socket Implementation (RFC 2516) 
 *
 * This file supplies definitions required by the PPP over Ethernet driver
 * (pppox.c).  All version information wrt this file is located in pppox.c
 *
 * License:
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 */

#ifndef __LINUX_IF_PPPOX_H
#define __LINUX_IF_PPPOX_H


#include <asm/types.h>
#include <asm/byteorder.h>
#include <linux/version.h>
#include <linux/config.h>

#ifdef  __KERNEL__
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <asm/semaphore.h>
#include <linux/ppp_channel.h>
#endif /* __KERNEL__ */
#include <linux/if_pppol2tp.h>

/* For user-space programs to pick up these definitions
 * which they wouldn't get otherwise without defining __KERNEL__
 */
#ifndef AF_PPPOX
#define AF_PPPOX	24
#define PF_PPPOX	AF_PPPOX
#endif /* !(AF_PPPOX) */

/************************************************************************ 
 * PPPoE addressing definition 
 */ 
typedef __u16 sid_t; 
#if defined(CONFIG_PPPOE) || defined(CONFIG_PPPOE_MODULE)
struct pppoe_addr{ 
       sid_t           sid;                    /* Session identifier */ 
       unsigned char   remote[ETH_ALEN];       /* Remote address */ 
       char            dev[IFNAMSIZ];          /* Local device to use */ 
}; 
#endif

#if defined(CONFIG_PPTP) || defined(CONFIG_PPTP_MODULE)
struct pptp_addr {
       __u16		call_id;
       struct in_addr 	sin_addr;
};
#endif
 
/************************************************************************ 
 * Protocols supported by AF_PPPOX 
 */ 
#define PX_PROTO_OE    0 /* Currently just PPPoE */
#define PX_PROTO_OL2TP 1 /* Now L2TP also */
#define PX_PROTO_PPTP  2
#define PX_MAX_PROTO   3

/* The use of a union isn't viable because the size of this struct
 * must stay fixed over time -- applications use sizeof(struct
 * sockaddr_pppox) to fill it. Use protocol specific sockaddr types
 * instead.
 */
struct sockaddr_pppox {
       sa_family_t     sa_family;            /* address family, AF_PPPOX */
       unsigned int    sa_protocol;          /* protocol identifier */
       union{
#if defined(CONFIG_PPPOE) || defined(CONFIG_PPPOE_MODULE)
		struct pppoe_addr       pppoe;
#endif
#if defined(CONFIG_PPPOL2TP) || defined(CONFIG_PPPOL2TP_MODULE)
		struct pppol2tp_addr    pppol2tp;
#endif
#if defined(CONFIG_PPTP) || defined(CONFIG_PPTP_MODULE)
		struct pptp_addr	pptp;
#endif
       }sa_addr;
}__attribute__ ((packed)); /* deprecated */

#if defined(CONFIG_PPPOE) || defined(CONFIG_PPPOE_MODULE)
/* Must be binary-compatible with sockaddr_pppox for backwards compatabilty */
struct sockaddr_pppoe {
	sa_family_t     sa_family;	/* address family, AF_PPPOX */
	unsigned int    sa_protocol;    /* protocol identifier */
	struct pppoe_addr pppoe;
}__attribute__ ((packed));
#endif

#if defined(CONFIG_PPPOL2TP) || defined(CONFIG_PPPOL2TP_MODULE)
struct sockaddr_pppol2tp {
	sa_family_t     sa_family;      /* address family, AF_PPPOX */
	unsigned int    sa_protocol;    /* protocol identifier */
	struct pppol2tp_addr pppol2tp;
}__attribute__ ((packed));
#endif

/* Socket options */
#define PPTP_SO_TIMEOUT 1
#define PPTP_SO_WINDOW  2
#define PPTP_FLAG_PAUSE 0
#define PPTP_FLAG_PROC  1

/*********************************************************************
 *
 * ioctl interface for defining forwarding of connections
 *
 ********************************************************************/

#define PPPOEIOCSFWD	_IOW(0xB1 ,0, sizeof(struct sockaddr_pppox))
#define PPPOEIOCDFWD	_IO(0xB1 ,1)
/*#define PPPOEIOCGFWD	_IOWR(0xB1 ,2, sizeof(struct sockaddr_pppox))*/
#define PPPTPIOWFP	_IOWR(0xB1 ,2, sizeof(struct sockaddr_pppox))*/

/* Codes to identify message types */
#define PADI_CODE	0x09
#define PADO_CODE	0x07
#define PADR_CODE	0x19
#define PADS_CODE	0x65
#define PADT_CODE	0xa7
struct pppoe_tag {
	__u16 tag_type;
	__u16 tag_len;
	char tag_data[0];
} __attribute ((packed));

/* Tag identifiers */
#define PTT_EOL		__constant_htons(0x0000)
#define PTT_SRV_NAME	__constant_htons(0x0101)
#define PTT_AC_NAME	__constant_htons(0x0102)
#define PTT_HOST_UNIQ	__constant_htons(0x0103)
#define PTT_AC_COOKIE	__constant_htons(0x0104)
#define PTT_VENDOR 	__constant_htons(0x0105)
#define PTT_RELAY_SID	__constant_htons(0x0110)
#define PTT_SRV_ERR     __constant_htons(0x0201)
#define PTT_SYS_ERR  	__constant_htons(0x0202)
#define PTT_GEN_ERR  	__constant_htons(0x0203)

struct pppoe_hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 ver : 4;
	__u8 type : 4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8 type : 4;
	__u8 ver : 4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	__u8 code;
	__u16 sid;
	__u16 length;
	struct pppoe_tag tag[0];
} __attribute__ ((packed));

#ifdef __KERNEL__

struct pppox_proto {
	int (*create)(struct socket *sock);
	int (*ioctl)(struct socket *sock, unsigned int cmd,
		     unsigned long arg);
};

extern int register_pppox_proto(int proto_num, struct pppox_proto *pp);
extern void unregister_pppox_proto(int proto_num);
extern void pppox_unbind_sock(struct sock *sk);/* delete ppp-channel binding */
extern int pppox_channel_ioctl(struct ppp_channel *pc, unsigned int cmd,
			       unsigned long arg);
extern int pppox_ioctl(struct socket *sock, unsigned int cmd,
			       unsigned long arg);

/* PPPoX socket states */
enum {
    PPPOX_NONE		= 0,  /* initial state */
    PPPOX_CONNECTED	= 1,  /* connection established ==TCP_ESTABLISHED */
    PPPOX_BOUND		= 2,  /* bound to ppp device */
    PPPOX_RELAY		= 4,  /* forwarding is enabled */
    PPPOX_ZOMBIE	= 8,  /* dead, but still bound to ppp device */
    PPPOX_DEAD		= 16  /* dead, useless, please clean me up!*/
};

extern struct ppp_channel_ops pppoe_chan_ops;

extern int pppox_proto_init(struct net_proto *np);

#endif /* __KERNEL__ */

#endif /* !(__LINUX_IF_PPPOX_H) */
