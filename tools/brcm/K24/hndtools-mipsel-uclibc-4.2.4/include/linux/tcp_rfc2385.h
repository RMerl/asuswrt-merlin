/* Copyright 2001 AYR Networks, Inc.
 *
 * Author: Rick Payne
 *
 * This is a free document; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation:
 *     http://www.gnu.org/copyleft/gpl.html
 */

/*
 * Structures and function calls used for the RFC2385 code
 *
 * This is #included in both kernel and userland code,
 * so use __KERNEL__ appropriately.
 */

#ifndef __LINUX__TCP_RFC2385_H__
#define __LINUX__TCP_RFC2385_H__

/* setsockopt Number */
#define TCP_RFC2385 13

/* Commands (used in the structure passed from userland) */
#define TCP_RFC2385_ADD 1
#define TCP_RFC2385_DEL 2

struct tcp_rfc2385_cmd {
	u_int8_t     command;    /* Command - Add/Delete */
	u_int32_t    address;    /* IPV4 address associated */
	u_int8_t     keylen;     /* MD5 Key len (do NOT assume 0 terminated ascii) */
	void         *key;       /* MD5 Key */
};

#ifdef __KERNEL__
struct tcp_rfc2385 *tcp_v4_md5_lookup (struct sock *, __u32);
int tcp_v4_md5_do_add (struct sock *, __u32, char *, __u8);
int tcp_v4_parse_md5_keys (struct sock *, char *, int);
int tcp_v4_calc_md5_hash (char *, struct tcp_rfc2385 *,
						  __u32, __u32,
						  struct tcphdr *, int, int);

struct tcp_rfc2385 {
	__u32   addr;      /* The IPv4 Address for this key */
	__u8    keylen;    /* The Key Length */
	__u8    *key;      /* The key itself - not null terminated */
};
#endif

#endif /* __LINUX__TCP_RFC2385_H__ */
