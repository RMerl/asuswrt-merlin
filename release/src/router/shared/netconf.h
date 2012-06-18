/*
 * Network configuration layer
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: netconf.h 241398 2011-02-18 03:46:33Z stakita $
 */

#ifndef _netconf_h_
#define _netconf_h_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include <typedefs.h>
#include <proto/ethernet.h>

#include <bcmconfig.h>

/* Supported match states */
#define NETCONF_INVALID		0x01	/* Packet could not be classified */
#define NETCONF_ESTABLISHED	0x02	/* Packet is related to an existing connection */
#define NETCONF_RELATED		0x04	/* Packet is part of an established connection */
#define NETCONF_NEW		0x08	/* Packet is trying to establish a new connection */

/* Supported match flags */
#define	NETCONF_INV_SRCIP	0x01	/* Invert the sense of source IP address */
#define	NETCONF_INV_DSTIP	0x02	/* Invert the sense of destination IP address */
#define	NETCONF_INV_SRCPT	0x04	/* Invert the sense of source port range */
#define	NETCONF_INV_DSTPT	0x08	/* Invert the sense of destination port range */
#define NETCONF_INV_MAC		0x10	/* Invert the sense of source MAC address */
#define NETCONF_INV_IN		0x20	/* Invert the sense of inbound interface */
#define NETCONF_INV_OUT		0x40	/* Invert the sense of outbound interface */
#define NETCONF_INV_STATE	0x80	/* Invert the sense of state */
#define NETCONF_INV_DAYS	0x100	/* Invert the sense of day of the week */
#define NETCONF_INV_SECS	0x200	/* Invert the sense of time of day */

#define NETCONF_DISABLED	0x80000000 /* Entry is disabled */


/* Cone NAT, Otherwise Symmetric NAT */
/* Please remember, the value 0x800 is in continuation with
 * the NFC_IP_XXX codes defined in linux/linux/inxlude/linux/netfilter_ipv4.h.
 * So, we need to keep both NETCONF_CONE_NAT here and NFC_IP_CONE_NAT
 * in netfilter_ipv4.h in sync.
 */
#define NETCONF_CONE_NAT         0x0800

/* Match description */
typedef struct _netconf_match_t {
	int ipproto;			/* IP protocol (TCP/UDP) */
	struct {
		struct in_addr ipaddr;	/* Match by IP address */
		struct in_addr netmask;
		uint16 ports[2];	/* Match by TCP/UDP port range */
	} src, dst;
	struct ether_addr mac;		/* Match by source MAC address */
	struct {
		char name[IFNAMSIZ];	/* Match by interface name */
	} in, out;
	int state;			/* Match by packet state */
	int flags;			/* Match flags */
	uint days[2];			/* Match by day of the week (local time) (Sunday == 0) */
	uint secs[2];			/* Match by time of day (local time) (12:00 AM == 0) */
	struct _netconf_match_t *next, *prev;
} netconf_match_t;

#ifndef __CONFIG_IPV6__
#define netconf_valid_ipproto(ipproto) \
	((ipproto == 0) || (ipproto) == IPPROTO_TCP || (ipproto) == IPPROTO_UDP)
#else
#define netconf_valid_ipproto(ipproto) \
	((ipproto == 0) || (ipproto) == IPPROTO_TCP || (ipproto) == IPPROTO_UDP || \
	(ipproto) == IPPROTO_IPV6)
#endif /* __CONFIG_IPV6__ */

/* Supported firewall target types */
enum netconf_target {
	NETCONF_DROP,			/* Drop packet (filter) */
	NETCONF_ACCEPT,			/* Accept packet (filter) */
	NETCONF_LOG_DROP,		/* Log and drop packet (filter) */
	NETCONF_LOG_ACCEPT,		/* Log and accept packet (filter) */
	NETCONF_SNAT,			/* Source NAT (nat) */
	NETCONF_DNAT,			/* Destination NAT (nat) */
	NETCONF_MASQ,			/* IP masquerade (nat) */
	NETCONF_APP,			/* Application specific port forward (app) */
	NETCONF_TARGET_MAX
};

#define netconf_valid_filter(target) \
	((target) == NETCONF_DROP || (target) == NETCONF_ACCEPT || \
	 (target) == NETCONF_LOG_DROP || (target) == NETCONF_LOG_ACCEPT)

#define netconf_valid_nat(target) \
	((target) == NETCONF_SNAT || (target) == NETCONF_DNAT || (target) == NETCONF_MASQ)

#define netconf_valid_target(target) \
	((target) >= 0 && (target) < NETCONF_TARGET_MAX)

#define NETCONF_FW_COMMON \
	netconf_match_t match;		/* Match type */ \
	enum netconf_target target;	/* Target type */ \
	char desc[40];			/* String description */ \
	struct _netconf_fw_t *next, *prev \

/* Generic firewall entry description */
typedef struct _netconf_fw_t {
	NETCONF_FW_COMMON;
	char data[0];			/* Target specific */
} netconf_fw_t;

/* Supported filter directions */
enum netconf_dir {
	NETCONF_IN,			/* Packets destined for the firewall */
	NETCONF_FORWARD,		/* Packets routed through the firewall */
	NETCONF_OUT,			/* Packets generated by the firewall */
	NETCONF_DIR_MAX
};

#define netconf_valid_dir(dir) \
	((dir) >= 0 && (dir) < NETCONF_DIR_MAX)

/* Filter target firewall entry description */
typedef struct _netconf_filter_t {
	NETCONF_FW_COMMON;
	enum netconf_dir dir;		/* Direction to filter */
} netconf_filter_t;

#ifdef __CONFIG_URLFILTER__
/* URL filter entry description */
typedef struct _netconf_urlfilter_t {
	NETCONF_FW_COMMON;
	char url[256];
} netconf_urlfilter_t;
#endif /* __CONFIG_URLFILTER__ */

/* NAT target firewall entry description */
typedef struct _netconf_nat_t {
	NETCONF_FW_COMMON;
	unsigned int type;		/* Indicates Cone/Symmetric NAT */
	struct in_addr ipaddr;		/* Address to map packet to */
	uint16 ports[2];		/* Port(s) to map packet to (network order) */
} netconf_nat_t;

/* Application specific port forward description */
typedef struct _netconf_app_t {
	NETCONF_FW_COMMON;
	uint16 proto;		/* Related protocol */
	uint16 dport[2];	/* Related destination port(s) (network order) */
	uint16 to[2];		/* Port(s) to map related destination port to (network order) */
} netconf_app_t;

/* Generic doubly linked list processing macros */
#define netconf_list_init(head) ((head)->next = (head)->prev = (head))

#define netconf_list_empty(head) ((head)->next == (head))

#define netconf_list_add(new, head) do { \
	(head)->next->prev = (new); \
	(new)->next = (head)->next; \
	(new)->prev = (head); \
	(head)->next = (new); \
} while (0)

#define netconf_list_del(old) do { \
	(old)->next->prev = (old)->prev; \
	(old)->prev->next = (old)->next; \
} while (0)

#define netconf_list_for_each(pos, head) \
	for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

#define netconf_list_free(head) do { \
	typeof(head) pos, next; \
	for ((pos) = (head)->next; (pos) != (head); (pos) = next) { \
		next = pos->next; \
		netconf_list_del(pos); \
		free(pos); \
	} \
} while (0)

/* 
 * Functions that work on arrays take a pointer to the array byte
 * length. If the length is zero, the function will set the length to
 * the number of bytes that must be provided to fulfill the
 * request. If the length is non-zero, then the array will be
 * constructed until the buffer length is exhausted or the request is
 * fulfilled.
 */

/*
 * Add a firewall entry
 * @param	fw	firewall entry
 * @return	0 on success and errno on failure
 */
extern int netconf_add_fw(netconf_fw_t *fw);

/*
 * Delete a firewall entry
 * @param	fw	firewall entry
 * @return	0 on success and errno on failure
 */
extern int netconf_del_fw(netconf_fw_t *fw);

/*
 * Get a list of the current firewall entries
 * @param	fw_list		list of firewall entries
 * @return	0 on success and errno on failure
 */
extern int netconf_get_fw(netconf_fw_t *fw_list);

/*
 * See if a given firewall entry already exists
 * @param	nat	NAT entry to look for
 * @return	whether NAT entry exists
 */
extern int netconf_fw_exists(netconf_fw_t *fw);

/*
 * Reset the firewall to a sane state
 * @return 0 on success and errno on failure
 */
extern int netconf_reset_fw(void);

/*
 * Add a NAT entry or list of NAT entries
 * @param	nat_list	NAT entry or list of NAT entries
 * @return	0 on success and errno on failure
 */
extern int netconf_add_nat(netconf_nat_t *nat_list);

/*
 * Delete a NAT entry or list of NAT entries
 * @param	nat_list	NAT entry or list of NAT entries
 * @return	0 on success and errno on failure
 */
extern int netconf_del_nat(netconf_nat_t *nat_list);

/*
 * Get an array of the current NAT entries
 * @param	nat_array	array of NAT entries
 * @param	space		Pointer to size of nat_array in bytes
 * @return 0 on success and errno on failure
 */
extern int netconf_get_nat(netconf_nat_t *nat_array, int *space);

/*
 * Add a filter entry or list of filter entries
 * @param	filter_list	filter entry or list of filter entries
 * @return	0 on success and errno on failure
 */
extern int netconf_add_filter(netconf_filter_t *filter_list);

/*
 * Delete a filter entry or list of filter entries
 * @param	filter_list	filter entry or list of filter entries
 * @return	0 on success and errno on failure
 */
extern int netconf_del_filter(netconf_filter_t *filter_list);

/*
 * Get an array of the current filter entries
 * @param	filter_array	array of filter entries
 * @param	space		Pointer to size of filter_array in bytes
 * @return 0 on success and errno on failure
 */
extern int netconf_get_filter(netconf_filter_t *filter_array, int *space);

extern int netconf_clamp_mss_to_pmtu(void);


#endif /* _netconf_h_ */
