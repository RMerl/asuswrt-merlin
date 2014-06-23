/*
 * Network configuration layer (Linux)
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: netconf_linux.h 358181 2012-09-21 13:59:23Z $
 */

#ifndef _netconf_linux_h_
#define _netconf_linux_h_
#include <linux/version.h>

/* Debug malloc() */
#ifdef DMALLOC
#include <dmalloc.h>
#endif /* DMALLOC */

/* iptables definitions */
#include <libiptc/libiptc.h>
#include <iptables.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <xtables.h>
#include <limits.h> /* INT_MAX in ip_tables.h */
#endif /* linux-2.6.36 */
#include <linux/netfilter_ipv4/ip_tables.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36) )
#include <linux/netfilter/nf_conntrack_common.h>
#include <net/netfilter/nf_nat.h>
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22) )
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#else
#include <linux/netfilter/nf_nat.h>
#include <linux/netfilter/nf_conntrack_common.h>
#endif
#define ETH_ALEN ETHER_ADDR_LEN
#include <linux/netfilter_ipv4/ipt_mac.h>
#include <linux/netfilter_ipv4/ipt_state.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
#include <linux/netfilter/xt_time.h>
#else
#include <linux/netfilter_ipv4/ipt_time.h>
#endif
#include <linux/netfilter_ipv4/ipt_TCPMSS.h>
#include <linux/netfilter_ipv4/ipt_LOG.h>
#include <linux/netfilter_ipv4/ip_autofw.h>

/* ipt_entry alignment attribute */
#define IPT_ALIGNED ((aligned (__alignof__ (struct ipt_entry))))

/* TCP flags */
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20

#endif /* _netconf_linux_h_ */
