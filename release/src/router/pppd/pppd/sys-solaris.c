/*
 * System-dependent procedures for pppd under Solaris 2.
 *
 * Parts re-written by Adi Masputra <adi.masputra@sun.com>, based on 
 * the original sys-svr4.c
 *
 * Copyright (c) 2000 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  
 *
 * SUN MAKES NO REPRESENTATION OR WARRANTIES ABOUT THE SUITABILITY OF
 * THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NON-INFRINGEMENT.  SUN SHALL NOT BE LIABLE FOR
 * ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR
 * DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES
 *
 * Copyright (c) 1995-2002 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Derived from main.c and pppd.h, which are:
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define RCSID	"$Id: sys-solaris.c,v 1.16 2008/01/30 14:26:53 carlsonj Exp $"

#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#ifndef CRTSCTS
#include <sys/termiox.h>
#endif
#include <signal.h>
#include <utmpx.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/ioccom.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysmacros.h>
#include <sys/systeminfo.h>
#include <sys/dlpi.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <net/ppp_defs.h>
#include <net/pppio.h>
#include <netinet/in.h>
#ifdef SOL2
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <inet/common.h>
#include <inet/mib2.h>
#include <sys/ethernet.h>
#endif

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "ccp.h"

#if !defined(PPP_DRV_NAME)
#define PPP_DRV_NAME	"ppp"
#endif /* !defined(PPP_DRV_NAME) */

#if !defined(PPP_DEV_NAME)
#define PPP_DEV_NAME	"/dev/" PPP_DRV_NAME
#endif /* !defined(PPP_DEV_NAME) */

#if !defined(AHDLC_MOD_NAME)
#define AHDLC_MOD_NAME	"ppp_ahdl"
#endif /* !defined(AHDLC_MOD_NAME) */

#if !defined(COMP_MOD_NAME)
#define COMP_MOD_NAME	"ppp_comp"
#endif /* !defined(COMP_MOD_NAME) */

#if !defined(IP_DEV_NAME)
#define	IP_DEV_NAME	"/dev/ip"
#endif /* !defined(IP_DEV_NAME) */

#if !defined(IP_MOD_NAME)
#define	IP_MOD_NAME	"ip"
#endif /* !defined(IP_MOD_NAME) */

#if !defined(UDP_DEV_NAME) && defined(SOL2)
#define	UDP_DEV_NAME	"/dev/udp"
#endif /* !defined(UDP_DEV_NAME) && defined(SOL2) */

#if !defined(UDP6_DEV_NAME) && defined(SOL2)
#define	UDP6_DEV_NAME	"/dev/udp6"
#endif /* !defined(UDP6_DEV_NAME) && defined(SOL2) */

static const char rcsid[] = RCSID;

#if defined(SOL2)
/*
 * "/dev/udp" is used as a multiplexor to PLINK the interface stream
 * under. It is used in place of "/dev/ip" since STREAMS will not let
 * a driver be PLINK'ed under itself, and "/dev/ip" is typically the
 * driver at the bottom of the tunneling interfaces stream.
 */
static char *mux_dev_name = UDP_DEV_NAME;
#else
static char *mux_dev_name = IP_DEV_NAME;
#endif
static int	pppfd;
static int	fdmuxid = -1;
static int	ipfd;
static int	ipmuxid = -1;

#if defined(INET6) && defined(SOL2)
static int	ip6fd;		/* IP file descriptor */
static int	ip6muxid = -1;	/* Multiplexer file descriptor */
static int	if6_is_up = 0;	/* IPv6 interface has been marked up */

#define _IN6_LLX_FROM_EUI64(l, s, eui64, as) do {	\
	s->sin6_addr.s6_addr32[0] = htonl(as); 	\
	eui64_copy(eui64, s->sin6_addr.s6_addr32[2]);	\
	s->sin6_family = AF_INET6;		\
	l.lifr_addr.ss_family = AF_INET6;	\
	l.lifr_addrlen = 64;			\
	l.lifr_addr = laddr;			\
	} while (0)

#define IN6_LLADDR_FROM_EUI64(l, s, eui64)  \
    _IN6_LLX_FROM_EUI64(l, s, eui64, 0xfe800000)

#define IN6_LLTOKEN_FROM_EUI64(l, s, eui64) \
    _IN6_LLX_FROM_EUI64(l, s, eui64, 0)

#endif /* defined(INET6) && defined(SOL2) */

#if defined(INET6) && defined(SOL2)
static char	first_ether_name[LIFNAMSIZ];	/* Solaris 8 and above */
#else
static char	first_ether_name[IFNAMSIZ];	/* Before Solaris 8 */
#define MAXIFS		256			/* Max # of interfaces */
#endif /* defined(INET6) && defined(SOL2) */

static int	restore_term;
static struct termios inittermios;
#ifndef CRTSCTS
static struct termiox inittermiox;
static int	termiox_ok;
#endif
static struct winsize wsinfo;	/* Initial window size info */
static pid_t	tty_sid;	/* original session ID for terminal */

extern u_char	inpacket_buf[];	/* borrowed from main.c */

#define MAX_POLLFDS	32
static struct pollfd pollfds[MAX_POLLFDS];
static int n_pollfds;

static int	link_mtu, link_mru;

#define NMODULES	32
static int	tty_nmodules;
static char	tty_modules[NMODULES][FMNAMESZ+1];
static int	tty_npushed;

static int	if_is_up;	/* Interface has been marked up */
static u_int32_t remote_addr;		/* IP address of peer */
static u_int32_t default_route_gateway;	/* Gateway for default route added */
static u_int32_t proxy_arp_addr;	/* Addr for proxy arp entry added */

/* Prototypes for procedures local to this file. */
static int translate_speed __P((int));
static int baud_rate_of __P((int));
static int get_ether_addr __P((u_int32_t, struct sockaddr *));
static int get_hw_addr __P((char *, u_int32_t, struct sockaddr *));
static int get_hw_addr_dlpi __P((char *, struct sockaddr *));
static int dlpi_attach __P((int, int));
static int dlpi_info_req __P((int));
static int dlpi_get_reply __P((int, union DL_primitives *, int, int));
static int strioctl __P((int, int, void *, int, int));

#ifdef SOL2
/*
 * sifppa - Sets interface ppa
 *
 * without setting the ppa, ip module will return EINVAL upon setting the
 * interface UP (SIOCSxIFFLAGS). This is because ip module in 2.8 expects
 * two DLPI_INFO_REQ to be sent down to the driver (below ip) before
 * IFF_UP can be set. Plumbing the device causes one DLPI_INFO_REQ to
 * be sent down, and the second DLPI_INFO_REQ is sent upon receiving
 * IF_UNITSEL (old) or SIOCSLIFNAME (new) ioctls. Such setting of the ppa
 * is required because the ppp DLPI provider advertises itself as
 * a DLPI style 2 type, which requires a point of attachment to be
 * specified. The only way the user can specify a point of attachment
 * is via SIOCSLIFNAME or IF_UNITSEL.
 *
 * Such changes in the behavior of ip module was made to meet new or
 * evolving standards requirements.
 *
 */
static int
sifppa(fd, ppa)
    int fd;
    int ppa;
{
    return (int)ioctl(fd, IF_UNITSEL, (char *)&ppa);
}
#endif /* SOL2 */

#if defined(SOL2) && defined(INET6)
/*
 * get_first_ethernet - returns the first Ethernet interface name found in 
 * the system, or NULL if none is found
 *
 * NOTE: This is the lifreq version (Solaris 8 and above)
 */
char *
get_first_ethernet()
{
    struct lifnum lifn;
    struct lifconf lifc;
    struct lifreq *plifreq;
    struct lifreq lifr;
    int	fd, num_ifs, i, found;
    uint_t fl, req_size;
    char *req;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
	return 0;
    }

    /*
     * Find out how many interfaces are running
     */
    lifn.lifn_family = AF_UNSPEC;
    lifn.lifn_flags = LIFC_NOXMIT;
    if (ioctl(fd, SIOCGLIFNUM, &lifn) < 0) {
	close(fd);
	error("could not determine number of interfaces: %m");
	return 0;
    }

    num_ifs = lifn.lifn_count;
    req_size = num_ifs * sizeof(struct lifreq);
    req = malloc(req_size);
    if (req == NULL) {
	close(fd);
	error("out of memory");
	return 0;
    }

    /*
     * Get interface configuration info for all interfaces
     */
    lifc.lifc_family = AF_UNSPEC;
    lifc.lifc_flags = LIFC_NOXMIT;
    lifc.lifc_len = req_size;
    lifc.lifc_buf = req;
    if (ioctl(fd, SIOCGLIFCONF, &lifc) < 0) {
	close(fd);
	free(req);
	error("SIOCGLIFCONF: %m");
	return 0;
    }

    /*
     * And traverse each interface to look specifically for the first
     * occurence of an Ethernet interface which has been marked up
     */
    plifreq = lifc.lifc_req;
    found = 0;
    for (i = lifc.lifc_len / sizeof(struct lifreq); i > 0; i--, plifreq++) {

	if (strchr(plifreq->lifr_name, ':') != NULL)
	    continue;

	memset(&lifr, 0, sizeof(lifr));
	strncpy(lifr.lifr_name, plifreq->lifr_name, sizeof(lifr.lifr_name));
	if (ioctl(fd, SIOCGLIFFLAGS, &lifr) < 0) {
	    close(fd);
	    free(req);
	    error("SIOCGLIFFLAGS: %m");
	    return 0;
	}
	fl = lifr.lifr_flags;

	if ((fl & (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|IFF_LOOPBACK|IFF_NOARP))
		!= (IFF_UP | IFF_BROADCAST))
	    continue;

	found = 1;
	break;
    }
    free(req);
    close(fd);

    if (found) {
	strncpy(first_ether_name, lifr.lifr_name, sizeof(first_ether_name));
	return (char *)first_ether_name;
    } else
	return NULL;
}
#else
/*
 * get_first_ethernet - returns the first Ethernet interface name found in 
 * the system, or NULL if none is found
 *
 * NOTE: This is the ifreq version (before Solaris 8). 
 */
char *
get_first_ethernet()
{
    struct ifconf ifc;
    struct ifreq *pifreq;
    struct ifreq ifr;
    int	fd, num_ifs, i, found;
    uint_t fl, req_size;
    char *req;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
	return 0;
    }

    /*
     * Find out how many interfaces are running
     */
    if (ioctl(fd, SIOCGIFNUM, (char *)&num_ifs) < 0) {
	num_ifs = MAXIFS;
    }

    req_size = num_ifs * sizeof(struct ifreq);
    req = malloc(req_size);
    if (req == NULL) {
	close(fd);
	error("out of memory");
	return 0;
    }

    /*
     * Get interface configuration info for all interfaces
     */
    ifc.ifc_len = req_size;
    ifc.ifc_buf = req;
    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
	close(fd);
	free(req);
	error("SIOCGIFCONF: %m");
	return 0;
    }

    /*
     * And traverse each interface to look specifically for the first
     * occurence of an Ethernet interface which has been marked up
     */
    pifreq = ifc.ifc_req;
    found = 0;
    for (i = ifc.ifc_len / sizeof(struct ifreq); i > 0; i--, pifreq++) {

	if (strchr(pifreq->ifr_name, ':') != NULL)
	    continue;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, pifreq->ifr_name, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
	    close(fd);
	    free(req);
	    error("SIOCGIFFLAGS: %m");
	    return 0;
	}
	fl = ifr.ifr_flags;

	if ((fl & (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|IFF_LOOPBACK|IFF_NOARP))
		!= (IFF_UP | IFF_BROADCAST))
	    continue;

	found = 1;
	break;
    }
    free(req);
    close(fd);

    if (found) {
	strncpy(first_ether_name, ifr.ifr_name, sizeof(first_ether_name));
	return (char *)first_ether_name;
    } else
	return NULL;
}
#endif /* defined(SOL2) && defined(INET6) */

#if defined(SOL2)
/*
 * get_if_hwaddr - get the hardware address for the specified
 * network interface device.
 */
int
get_if_hwaddr(u_char *addr, char *if_name)
{
    struct sockaddr s_eth_addr;
    struct ether_addr *eth_addr = (struct ether_addr *)&s_eth_addr.sa_data;

    if (if_name == NULL)
	return -1;

    /*
     * Send DL_INFO_REQ to the driver to solicit its MAC address
     */
    if (!get_hw_addr_dlpi(if_name, &s_eth_addr)) {
	error("could not obtain hardware address for %s", if_name);
	return -1;
    }

    memcpy(addr, eth_addr->ether_addr_octet, 6);
    return 1;
}
#endif /* SOL2 */

#if defined(SOL2) && defined(INET6)
/*
 * slifname - Sets interface ppa and flags
 *
 * in addition to the comments stated in sifppa(), IFF_IPV6 bit must
 * be set in order to declare this as an IPv6 interface
 */
static int
slifname(fd, ppa)
    int fd;
    int ppa;
{
    struct  lifreq lifr;
    int	    ret;

    memset(&lifr, 0, sizeof(lifr));
    ret = ioctl(fd, SIOCGLIFFLAGS, &lifr);
    if (ret < 0)
	goto slifname_done;

    lifr.lifr_flags |= IFF_IPV6;
    lifr.lifr_flags &= ~(IFF_BROADCAST | IFF_IPV4);
    lifr.lifr_ppa = ppa;
    strlcpy(lifr.lifr_name, ifname, sizeof(lifr.lifr_name));

    ret = ioctl(fd, SIOCSLIFNAME, &lifr);

slifname_done:
    return ret;


}


/*
 * ether_to_eui64 - Convert 48-bit Ethernet address into 64-bit EUI
 *
 * walks the list of valid ethernet interfaces, and convert the first
 * found 48-bit MAC address into EUI 64. caller also assumes that
 * the system has a properly configured Ethernet interface for this
 * function to return non-zero.
 */
int
ether_to_eui64(eui64_t *p_eui64)
{
    struct sockaddr s_eth_addr;
    struct ether_addr *eth_addr = (struct ether_addr *)&s_eth_addr.sa_data;
    char *if_name;

    if ((if_name = get_first_ethernet()) == NULL) {
	error("no persistent id can be found");
	return 0;
    }
 
    /*
     * Send DL_INFO_REQ to the driver to solicit its MAC address
     */
    if (!get_hw_addr_dlpi(if_name, &s_eth_addr)) {
	error("could not obtain hardware address for %s", if_name);
	return 0;
    }

    /*
     * And convert the EUI-48 into EUI-64, per RFC 2472 [sec 4.1]
     */
    p_eui64->e8[0] = (eth_addr->ether_addr_octet[0] & 0xFF) | 0x02;
    p_eui64->e8[1] = (eth_addr->ether_addr_octet[1] & 0xFF);
    p_eui64->e8[2] = (eth_addr->ether_addr_octet[2] & 0xFF);
    p_eui64->e8[3] = 0xFF;
    p_eui64->e8[4] = 0xFE;
    p_eui64->e8[5] = (eth_addr->ether_addr_octet[3] & 0xFF);
    p_eui64->e8[6] = (eth_addr->ether_addr_octet[4] & 0xFF);
    p_eui64->e8[7] = (eth_addr->ether_addr_octet[5] & 0xFF);

    return 1;
}
#endif /* defined(SOL2) && defined(INET6) */

/*
 * sys_init - System-dependent initialization.
 */
void
sys_init()
{
    int ifd, x;
    struct ifreq ifr;
#if defined(INET6) && defined(SOL2)
    int i6fd;
    struct lifreq lifr;
#endif /* defined(INET6) && defined(SOL2) */
#if !defined(SOL2)
    struct {
	union DL_primitives prim;
	char space[64];
    } reply;
#endif /* !defined(SOL2) */

    ipfd = open(mux_dev_name, O_RDWR, 0);
    if (ipfd < 0)
	fatal("Couldn't open IP device: %m");

#if defined(INET6) && defined(SOL2)
    ip6fd = open(UDP6_DEV_NAME, O_RDWR, 0);
    if (ip6fd < 0)
	fatal("Couldn't open IP device (2): %m");
#endif /* defined(INET6) && defined(SOL2) */

    if (default_device && !notty)
	tty_sid = getsid((pid_t)0);

    pppfd = open(PPP_DEV_NAME, O_RDWR | O_NONBLOCK, 0);
    if (pppfd < 0)
	fatal("Can't open %s: %m", PPP_DEV_NAME);
    if (kdebugflag & 1) {
	x = PPPDBG_LOG + PPPDBG_DRIVER;
	strioctl(pppfd, PPPIO_DEBUG, &x, sizeof(int), 0);
    }

    /* Assign a new PPA and get its unit number. */
    if (strioctl(pppfd, PPPIO_NEWPPA, &ifunit, 0, sizeof(int)) < 0)
	fatal("Can't create new PPP interface: %m");

#if defined(SOL2)
    /*
     * Since sys_init() is called prior to ifname being set in main(),
     * we need to get the ifname now, otherwise slifname(), and others,
     * will fail, or maybe, I should move them to a later point ?
     * <adi.masputra@sun.com>
     */
    sprintf(ifname, PPP_DRV_NAME "%d", ifunit);
#endif /* defined(SOL2) */
    /*
     * Open the ppp device again and link it under the ip multiplexor.
     * IP will assign a unit number which hopefully is the same as ifunit.
     * I don't know any way to be certain they will be the same. :-(
     */
    ifd = open(PPP_DEV_NAME, O_RDWR, 0);
    if (ifd < 0)
	fatal("Can't open %s (2): %m", PPP_DEV_NAME);
    if (kdebugflag & 1) {
	x = PPPDBG_LOG + PPPDBG_DRIVER;
	strioctl(ifd, PPPIO_DEBUG, &x, sizeof(int), 0);
    }

#if defined(INET6) && defined(SOL2)
    i6fd = open(PPP_DEV_NAME, O_RDWR, 0);
    if (i6fd < 0) {
	close(ifd);
	fatal("Can't open %s (3): %m", PPP_DEV_NAME);
    }
    if (kdebugflag & 1) {
	x = PPPDBG_LOG + PPPDBG_DRIVER;
	strioctl(i6fd, PPPIO_DEBUG, &x, sizeof(int), 0);
    }
#endif /* defined(INET6) && defined(SOL2) */

#if defined(SOL2)
    if (ioctl(ifd, I_PUSH, IP_MOD_NAME) < 0) {
	close(ifd);
#if defined(INET6)
	close(i6fd);
#endif /* defined(INET6) */
	fatal("Can't push IP module: %m");
    }

    /*
     * Assign ppa according to the unit number returned by ppp device
     * after plumbing is completed above.
     */
    if (sifppa(ifd, ifunit) < 0) {
        close (ifd);
#if defined(INET6)
	close(i6fd);
#endif /* defined(INET6) */
        fatal("Can't set ppa for unit %d: %m", ifunit);
    }

#if defined(INET6)
    /*
     * An IPv6 interface is created anyway, even when the user does not 
     * explicitly enable it. Note that the interface will be marked
     * IPv6 during slifname().
     */
    if (ioctl(i6fd, I_PUSH, IP_MOD_NAME) < 0) {
	close(ifd);
	close(i6fd);
	fatal("Can't push IP module (2): %m");
    }

    /*
     * Assign ppa according to the unit number returned by ppp device
     * after plumbing is completed above. In addition, mark the interface
     * as an IPv6 interface.
     */
    if (slifname(i6fd, ifunit) < 0) {
	close(ifd);
	close(i6fd);
	fatal("Can't set ifname for unit %d: %m", ifunit);
    }
#endif /* defined(INET6) */

    ipmuxid = ioctl(ipfd, I_PLINK, ifd);
    close(ifd);
    if (ipmuxid < 0) {
#if defined(INET6)
	close(i6fd);
#endif /* defined(INET6) */
	fatal("Can't I_PLINK PPP device to IP: %m");
    }

    memset(&ifr, 0, sizeof(ifr));
    sprintf(ifr.ifr_name, "%s", ifname);
    ifr.ifr_ip_muxid = ipmuxid;

    /*
     * In Sol 8 and later, STREAMS dynamic module plumbing feature exists.
     * This is so that an arbitrary module can be inserted, or deleted, 
     * between ip module and the device driver without tearing down the 
     * existing stream. Such feature requires the mux ids, which is set 
     * by SIOCSIFMUXID (or SIOCLSIFMUXID).
     */
    if (ioctl(ipfd, SIOCSIFMUXID, &ifr) < 0) {
	ioctl(ipfd, I_PUNLINK, ipmuxid);
#if defined(INET6)
	close(i6fd);
#endif /* defined(INET6) */
	fatal("SIOCSIFMUXID: %m");
    }

#else /* else if !defined(SOL2) */

    if (dlpi_attach(ifd, ifunit) < 0 ||
	dlpi_get_reply(ifd, &reply.prim, DL_OK_ACK, sizeof(reply)) < 0) {
	close(ifd);
	fatal("Can't attach to ppp%d: %m", ifunit);
    }

    ipmuxid = ioctl(ipfd, I_LINK, ifd);
    close(ifd);
    if (ipmuxid < 0)
	fatal("Can't link PPP device to IP: %m");
#endif /* defined(SOL2) */

#if defined(INET6) && defined(SOL2)
    ip6muxid = ioctl(ip6fd, I_PLINK, i6fd);
    close(i6fd);
    if (ip6muxid < 0) {
	ioctl(ipfd, I_PUNLINK, ipmuxid);
	fatal("Can't I_PLINK PPP device to IP (2): %m");
    }

    memset(&lifr, 0, sizeof(lifr));
    sprintf(lifr.lifr_name, "%s", ifname);
    lifr.lifr_ip_muxid = ip6muxid;

    /*
     * Let IP know of the mux id [see comment for SIOCSIFMUXID above]
     */
    if (ioctl(ip6fd, SIOCSLIFMUXID, &lifr) < 0) {
	ioctl(ipfd, I_PUNLINK, ipmuxid);
	ioctl(ip6fd, I_PUNLINK, ip6muxid);
	fatal("Can't link PPP device to IP (2): %m");
    }
#endif /* defined(INET6) && defined(SOL2) */

#if !defined(SOL2)
    /* Set the interface name for the link. */
    slprintf(ifr.ifr_name, sizeof(ifr.ifr_name), PPP_DRV_NAME "%d", ifunit);
    ifr.ifr_metric = ipmuxid;
    if (strioctl(ipfd, SIOCSIFNAME, (char *)&ifr, sizeof ifr, 0) < 0)
	fatal("Can't set interface name %s: %m", ifr.ifr_name);
#endif /* !defined(SOL2) */

    n_pollfds = 0;
}

/*
 * sys_cleanup - restore any system state we modified before exiting:
 * mark the interface down, delete default route and/or proxy arp entry.
 * This should call die() because it's called from die().
 */
void
sys_cleanup()
{
#if defined(SOL2)
    struct ifreq ifr;
#if defined(INET6)
    struct lifreq lifr;
#endif /* defined(INET6) */
#endif /* defined(SOL2) */

#if defined(SOL2) && defined(INET6)
    if (if6_is_up)
	sif6down(0);
#endif /* defined(SOL2) && defined(INET6) */
    if (if_is_up)
	sifdown(0);
    if (default_route_gateway)
	cifdefaultroute(0, default_route_gateway, default_route_gateway);
    if (proxy_arp_addr)
	cifproxyarp(0, proxy_arp_addr);
#if defined(SOL2)
    /*
     * Make sure we ask ip what the muxid, because 'ifconfig modlist' will
     * unlink and re-link the modules, causing the muxid to change.
     */
    memset(&ifr, 0, sizeof(ifr));
    sprintf(ifr.ifr_name, "%s", ifname);
    if (ioctl(ipfd, SIOCGIFFLAGS, &ifr) < 0) {
	error("SIOCGIFFLAGS: %m");
	return;
    }

    if (ioctl(ipfd, SIOCGIFMUXID, &ifr) < 0) {
	error("SIOCGIFMUXID: %m");
	return;
    }

    ipmuxid = ifr.ifr_ip_muxid;
     
    if (ioctl(ipfd, I_PUNLINK, ipmuxid) < 0) {
	error("Can't I_PUNLINK PPP from IP: %m");
	return;
    }
#if defined(INET6)
    /*
     * Make sure we ask ip what the muxid, because 'ifconfig modlist' will
     * unlink and re-link the modules, causing the muxid to change.
     */
    memset(&lifr, 0, sizeof(lifr));
    sprintf(lifr.lifr_name, "%s", ifname);
    if (ioctl(ip6fd, SIOCGLIFFLAGS, &lifr) < 0) {
	error("SIOCGLIFFLAGS: %m");
	return;
    }

    if (ioctl(ip6fd, SIOCGLIFMUXID, &lifr) < 0) {
	error("SIOCGLIFMUXID: %m");
	return;
    }

    ip6muxid = lifr.lifr_ip_muxid;

    if (ioctl(ip6fd, I_PUNLINK, ip6muxid) < 0) {
	error("Can't I_PUNLINK PPP from IP (2): %m");
    }
#endif /* defined(INET6) */
#endif /* defined(SOL2) */
}

/*
 * sys_close - Clean up in a child process before execing.
 */
void
sys_close()
{
    close(ipfd);
#if defined(INET6) && defined(SOL2)
    close(ip6fd);
#endif /* defined(INET6) && defined(SOL2) */
    if (pppfd >= 0)
	close(pppfd);
}

/*
 * sys_check_options - check the options that the user specified
 */
int
sys_check_options()
{
    return 1;
}

#if 0
/*
 * daemon - Detach us from controlling terminal session.
 */
int
daemon(nochdir, noclose)
    int nochdir, noclose;
{
    int pid;

    if ((pid = fork()) < 0)
	return -1;
    if (pid != 0)
	exit(0);		/* parent dies */
    setsid();
    if (!nochdir)
	chdir("/");
    if (!noclose) {
	fclose(stdin);		/* don't need stdin, stdout, stderr */
	fclose(stdout);
	fclose(stderr);
    }
    return 0;
}
#endif

/*
 * ppp_available - check whether the system has any ppp interfaces
 */
int
ppp_available()
{
    struct stat buf;

    return stat(PPP_DEV_NAME, &buf) >= 0;
}

/*
 * any_compressions - see if compression is enabled or not
 *
 * In the STREAMS implementation of kernel-portion pppd,
 * the comp STREAMS module performs the ACFC, PFC, as well
 * CCP and VJ compressions. However, if the user has explicitly
 * declare to not enable them from the command line, there is
 * no point of having the comp module be pushed on the stream.
 */
static int
any_compressions()
{
    if ((!lcp_wantoptions[0].neg_accompression) &&
	(!lcp_wantoptions[0].neg_pcompression) &&
	(!ccp_protent.enabled_flag) &&
	(!ipcp_wantoptions[0].neg_vj)) {
	    return 0;
    }
    return 1;
}

/*
 * tty_establish_ppp - Turn the serial port into a ppp interface.
 */
int
tty_establish_ppp(fd)
    int fd;
{
    int i;

    /* Pop any existing modules off the tty stream. */
    for (i = 0;; ++i)
	if (ioctl(fd, I_LOOK, tty_modules[i]) < 0
	    || strcmp(tty_modules[i], "ptem") == 0
	    || ioctl(fd, I_POP, 0) < 0)
	    break;
    tty_nmodules = i;

    /* Push the async hdlc module and the compressor module. */
    tty_npushed = 0;

    if(!sync_serial) {
        if (ioctl(fd, I_PUSH, AHDLC_MOD_NAME) < 0) {
            error("Couldn't push PPP Async HDLC module: %m");
	    return -1;
        }
        ++tty_npushed;
    }
    if (kdebugflag & 4) {
	i = PPPDBG_LOG + PPPDBG_AHDLC;
	strioctl(pppfd, PPPIO_DEBUG, &i, sizeof(int), 0);
    }
    /*
     * There's no need to push comp module if we don't intend
     * to compress anything
     */
    if (any_compressions()) { 
        if (ioctl(fd, I_PUSH, COMP_MOD_NAME) < 0)
	    error("Couldn't push PPP compression module: %m");
	else
	    ++tty_npushed;
    }

    if (kdebugflag & 2) {
	i = PPPDBG_LOG; 
	if (any_compressions())
	    i += PPPDBG_COMP;
	strioctl(pppfd, PPPIO_DEBUG, &i, sizeof(int), 0);
    }

    /* Link the serial port under the PPP multiplexor. */
    if ((fdmuxid = ioctl(pppfd, I_LINK, fd)) < 0) {
	error("Can't link tty to PPP mux: %m");
	return -1;
    }

    return pppfd;
}

/*
 * tty_disestablish_ppp - Restore the serial port to normal operation.
 * It attempts to reconstruct the stream with the previously popped
 * modules.  This shouldn't call die() because it's called from die().
 */
void
tty_disestablish_ppp(fd)
    int fd;
{
    int i;

    if (fdmuxid >= 0) {
	if (ioctl(pppfd, I_UNLINK, fdmuxid) < 0) {
	    if (!hungup)
		error("Can't unlink tty from PPP mux: %m");
	}
	fdmuxid = -1;

	if (!hungup) {
	    while (tty_npushed > 0 && ioctl(fd, I_POP, 0) >= 0)
		--tty_npushed;
	    for (i = tty_nmodules - 1; i >= 0; --i)
		if (ioctl(fd, I_PUSH, tty_modules[i]) < 0)
		    error("Couldn't restore tty module %s: %m",
			   tty_modules[i]);
	}
	if (hungup && default_device && tty_sid > 0) {
	    /*
	     * If we have received a hangup, we need to send a SIGHUP
	     * to the terminal's controlling process.  The reason is
	     * that the original stream head for the terminal hasn't
	     * seen the M_HANGUP message (it went up through the ppp
	     * driver to the stream head for our fd to /dev/ppp).
	     */
	    kill(tty_sid, SIGHUP);
	}
    }
}

/*
 * Check whether the link seems not to be 8-bit clean.
 */
void
clean_check()
{
    int x;
    char *s;

    if (strioctl(pppfd, PPPIO_GCLEAN, &x, 0, sizeof(x)) < 0)
	return;
    s = NULL;
    switch (~x) {
    case RCV_B7_0:
	s = "bit 7 set to 1";
	break;
    case RCV_B7_1:
	s = "bit 7 set to 0";
	break;
    case RCV_EVNP:
	s = "odd parity";
	break;
    case RCV_ODDP:
	s = "even parity";
	break;
    }
    if (s != NULL) {
	warn("Serial link is not 8-bit clean:");
	warn("All received characters had %s", s);
    }
}

/*
 * List of valid speeds.
 */
struct speed {
    int speed_int, speed_val;
} speeds[] = {
#ifdef B50
    { 50, B50 },
#endif
#ifdef B75
    { 75, B75 },
#endif
#ifdef B110
    { 110, B110 },
#endif
#ifdef B134
    { 134, B134 },
#endif
#ifdef B150
    { 150, B150 },
#endif
#ifdef B200
    { 200, B200 },
#endif
#ifdef B300
    { 300, B300 },
#endif
#ifdef B600
    { 600, B600 },
#endif
#ifdef B1200
    { 1200, B1200 },
#endif
#ifdef B1800
    { 1800, B1800 },
#endif
#ifdef B2000
    { 2000, B2000 },
#endif
#ifdef B2400
    { 2400, B2400 },
#endif
#ifdef B3600
    { 3600, B3600 },
#endif
#ifdef B4800
    { 4800, B4800 },
#endif
#ifdef B7200
    { 7200, B7200 },
#endif
#ifdef B9600
    { 9600, B9600 },
#endif
#ifdef B19200
    { 19200, B19200 },
#endif
#ifdef B38400
    { 38400, B38400 },
#endif
#ifdef EXTA
    { 19200, EXTA },
#endif
#ifdef EXTB
    { 38400, EXTB },
#endif
#ifdef B57600
    { 57600, B57600 },
#endif
#ifdef B76800
    { 76800, B76800 },
#endif
#ifdef B115200
    { 115200, B115200 },
#endif
#ifdef B153600
    { 153600, B153600 },
#endif
#ifdef B230400
    { 230400, B230400 },
#endif
#ifdef B307200
    { 307200, B307200 },
#endif
#ifdef B460800
    { 460800, B460800 },
#endif
    { 0, 0 }
};

/*
 * Translate from bits/second to a speed_t.
 */
static int
translate_speed(bps)
    int bps;
{
    struct speed *speedp;

    if (bps == 0)
	return 0;
    for (speedp = speeds; speedp->speed_int; speedp++)
	if (bps == speedp->speed_int)
	    return speedp->speed_val;
    warn("speed %d not supported", bps);
    return 0;
}

/*
 * Translate from a speed_t to bits/second.
 */
static int
baud_rate_of(speed)
    int speed;
{
    struct speed *speedp;

    if (speed == 0)
	return 0;
    for (speedp = speeds; speedp->speed_int; speedp++)
	if (speed == speedp->speed_val)
	    return speedp->speed_int;
    return 0;
}

/*
 * set_up_tty: Set up the serial port on `fd' for 8 bits, no parity,
 * at the requested speed, etc.  If `local' is true, set CLOCAL
 * regardless of whether the modem option was specified.
 */
void
set_up_tty(fd, local)
    int fd, local;
{
    int speed;
    struct termios tios;
#if !defined (CRTSCTS)
    struct termiox tiox;
#endif

    if (!sync_serial && tcgetattr(fd, &tios) < 0)
	fatal("tcgetattr: %m");

#ifndef CRTSCTS
    termiox_ok = 1;
    if (!sync_serial && ioctl (fd, TCGETX, &tiox) < 0) {
	termiox_ok = 0;
	if (errno != ENOTTY)
	    error("TCGETX: %m");
    }
#endif

    if (!restore_term) {
	inittermios = tios;
#ifndef CRTSCTS
	inittermiox = tiox;
#endif
	if (!sync_serial)
	    ioctl(fd, TIOCGWINSZ, &wsinfo);
    }

    tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CLOCAL);
#ifdef CRTSCTS
    if (crtscts > 0)
	tios.c_cflag |= CRTSCTS;
    else if (crtscts < 0)
	tios.c_cflag &= ~CRTSCTS;
#else
    if (crtscts != 0 && !termiox_ok) {
	error("Can't set RTS/CTS flow control");
    } else if (crtscts > 0) {
	tiox.x_hflag |= RTSXOFF|CTSXON;
    } else if (crtscts < 0) {
	tiox.x_hflag &= ~(RTSXOFF|CTSXON);
    }
#endif

    if (stop_bits >= 2)
	tios.c_cflag |= CSTOPB;

    tios.c_cflag |= CS8 | CREAD | HUPCL;
    if (local || !modem)
	tios.c_cflag |= CLOCAL;
    tios.c_iflag = IGNBRK | IGNPAR;
    tios.c_oflag = 0;
    tios.c_lflag = 0;
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;

    if (crtscts == -2) {
	tios.c_iflag |= IXON | IXOFF;
	tios.c_cc[VSTOP] = 0x13;	/* DC3 = XOFF = ^S */
	tios.c_cc[VSTART] = 0x11;	/* DC1 = XON  = ^Q */
    }

    speed = translate_speed(inspeed);
    if (speed) {
	cfsetospeed(&tios, speed);
	cfsetispeed(&tios, speed);
    } else {
	speed = cfgetospeed(&tios);
	/*
	 * We can't proceed if the serial port speed is 0,
	 * since that implies that the serial port is disabled.
	 */
	if ((speed == B0) && !sync_serial)
	    fatal("Baud rate for %s is 0; need explicit baud rate", devnam);
    }

    if (!sync_serial && tcsetattr(fd, TCSAFLUSH, &tios) < 0)
	fatal("tcsetattr: %m");

#ifndef CRTSCTS
    if (!sync_serial && termiox_ok && ioctl (fd, TCSETXF, &tiox) < 0){
	error("TCSETXF: %m");
    }
#endif

    baud_rate = inspeed = baud_rate_of(speed);
    if (!sync_serial)
	restore_term = 1;
}

/*
 * restore_tty - restore the terminal to the saved settings.
 */
void
restore_tty(fd)
    int fd;
{
    if (restore_term) {
	if (!default_device) {
	    /*
	     * Turn off echoing, because otherwise we can get into
	     * a loop with the tty and the modem echoing to each other.
	     * We presume we are the sole user of this tty device, so
	     * when we close it, it will revert to its defaults anyway.
	     */
	    inittermios.c_lflag &= ~(ECHO | ECHONL);
	}
	if (!sync_serial && tcsetattr(fd, TCSAFLUSH, &inittermios) < 0)
	    if (!hungup && errno != ENXIO)
		warn("tcsetattr: %m");
#ifndef CRTSCTS
	if (!sync_serial && ioctl (fd, TCSETXF, &inittermiox) < 0){
	    if (!hungup && errno != ENXIO)
		error("TCSETXF: %m");
	}
#endif
	if (!sync_serial)
	    ioctl(fd, TIOCSWINSZ, &wsinfo);
	restore_term = 0;
    }
}

/*
 * setdtr - control the DTR line on the serial port.
 * This is called from die(), so it shouldn't call die().
 */
void
setdtr(fd, on)
int fd, on;
{
    int modembits = TIOCM_DTR;

    ioctl(fd, (on? TIOCMBIS: TIOCMBIC), &modembits);
}

/*
 * open_loopback - open the device we use for getting packets
 * in demand mode.  Under Solaris 2, we use our existing fd
 * to the ppp driver.
 */
int
open_ppp_loopback()
{
    return pppfd;
}

/*
 * output - Output PPP packet.
 */
void
output(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    struct strbuf data;
    int retries;
    struct pollfd pfd;

    dump_packet("sent", p, len);
    if (snoop_send_hook) snoop_send_hook(p, len);

    data.len = len;
    data.buf = (caddr_t) p;
    retries = 4;
    while (putmsg(pppfd, NULL, &data, 0) < 0) {
	if (--retries < 0 || (errno != EWOULDBLOCK && errno != EAGAIN)) {
	    if (errno != ENXIO)
		error("Couldn't send packet: %m");
	    break;
	}
	pfd.fd = pppfd;
	pfd.events = POLLOUT;
	poll(&pfd, 1, 250);	/* wait for up to 0.25 seconds */
    }
}


/*
 * wait_input - wait until there is data available,
 * for the length of time specified by *timo (indefinite
 * if timo is NULL).
 */
void
wait_input(timo)
    struct timeval *timo;
{
    int t;

    t = timo == NULL? -1: timo->tv_sec * 1000 + timo->tv_usec / 1000;
    if (poll(pollfds, n_pollfds, t) < 0 && errno != EINTR)
	fatal("poll: %m");
}

/*
 * add_fd - add an fd to the set that wait_input waits for.
 */
void add_fd(fd)
    int fd;
{
    int n;

    for (n = 0; n < n_pollfds; ++n)
	if (pollfds[n].fd == fd)
	    return;
    if (n_pollfds < MAX_POLLFDS) {
	pollfds[n_pollfds].fd = fd;
	pollfds[n_pollfds].events = POLLIN | POLLPRI | POLLHUP;
	++n_pollfds;
    } else
	error("Too many inputs!");
}

/*
 * remove_fd - remove an fd from the set that wait_input waits for.
 */
void remove_fd(fd)
    int fd;
{
    int n;

    for (n = 0; n < n_pollfds; ++n) {
	if (pollfds[n].fd == fd) {
	    while (++n < n_pollfds)
		pollfds[n-1] = pollfds[n];
	    --n_pollfds;
	    break;
	}
    }
}

#if 0
/*
 * wait_loop_output - wait until there is data available on the
 * loopback, for the length of time specified by *timo (indefinite
 * if timo is NULL).
 */
void
wait_loop_output(timo)
    struct timeval *timo;
{
    wait_input(timo);
}

/*
 * wait_time - wait for a given length of time or until a
 * signal is received.
 */
void
wait_time(timo)
    struct timeval *timo;
{
    int n;

    n = select(0, NULL, NULL, NULL, timo);
    if (n < 0 && errno != EINTR)
	fatal("select: %m");
}
#endif


/*
 * read_packet - get a PPP packet from the serial device.
 */
int
read_packet(buf)
    u_char *buf;
{
    struct strbuf ctrl, data;
    int flags, len;
    unsigned char ctrlbuf[sizeof(union DL_primitives) + 64];

    for (;;) {
	data.maxlen = PPP_MRU + PPP_HDRLEN;
	data.buf = (caddr_t) buf;
	ctrl.maxlen = sizeof(ctrlbuf);
	ctrl.buf = (caddr_t) ctrlbuf;
	flags = 0;
	len = getmsg(pppfd, &ctrl, &data, &flags);
	if (len < 0) {
	    if (errno == EAGAIN || errno == EINTR)
		return -1;
	    fatal("Error reading packet: %m");
	}

	if (ctrl.len <= 0)
	    return data.len;

	/*
	 * Got a M_PROTO or M_PCPROTO message.  Interpret it
	 * as a DLPI primitive??
	 */
	if (debug)
	    dbglog("got dlpi prim 0x%x, len=%d",
		   ((union DL_primitives *)ctrlbuf)->dl_primitive, ctrl.len);

    }
}

/*
 * get_loop_output - get outgoing packets from the ppp device,
 * and detect when we want to bring the real link up.
 * Return value is 1 if we need to bring up the link, 0 otherwise.
 */
int
get_loop_output()
{
    int len;
    int rv = 0;

    while ((len = read_packet(inpacket_buf)) > 0) {
	if (loop_frame(inpacket_buf, len))
	    rv = 1;
    }
    return rv;
}

/*
 * netif_set_mtu - set the MTU on the PPP network interface.
 */
void
netif_set_mtu(unit, mtu)
    int unit, mtu;
{
    struct ifreq ifr;
#if defined(INET6) && defined(SOL2)
    struct lifreq lifr;
    int	fd;
#endif /* defined(INET6) && defined(SOL2) */

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_metric = link_mtu;
    if (ioctl(ipfd, SIOCSIFMTU, &ifr) < 0) {
	error("Couldn't set IP MTU (%s): %m", ifr.ifr_name);
    }

#if defined(INET6) && defined(SOL2) 
    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0)
	error("Couldn't open IPv6 socket: %m");

    memset(&lifr, 0, sizeof(lifr));
    strlcpy(lifr.lifr_name, ifname, sizeof(lifr.lifr_name));
    lifr.lifr_mtu = link_mtu;
    if (ioctl(fd, SIOCSLIFMTU, &lifr) < 0) {
	close(fd);
	error("Couldn't set IPv6 MTU (%s): %m", ifr.ifr_name);
    }
    close(fd);
#endif /* defined(INET6) && defined(SOL2) */
}

/*
 * tty_send_config - configure the transmit characteristics of
 * the ppp interface.
 */
void
tty_send_config(mtu, asyncmap, pcomp, accomp)
    int mtu;
    u_int32_t asyncmap;
    int pcomp, accomp;
{
    int cf[2];

    link_mtu = mtu;
    if (strioctl(pppfd, PPPIO_MTU, &mtu, sizeof(mtu), 0) < 0) {
	if (hungup && errno == ENXIO) {
	    ++error_count;
	    return;
	}
	error("Couldn't set MTU: %m");
    }
    if (fdmuxid >= 0) {
	if (!sync_serial) {
	    if (strioctl(pppfd, PPPIO_XACCM, &asyncmap, sizeof(asyncmap), 0) < 0)
		error("Couldn't set transmit ACCM: %m");
	}
	cf[0] = (pcomp? COMP_PROT: 0) + (accomp? COMP_AC: 0);
	cf[1] = COMP_PROT | COMP_AC;
	if (any_compressions() &&
	    strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0)
	    error("Couldn't set prot/AC compression: %m");
    }
}

/*
 * tty_set_xaccm - set the extended transmit ACCM for the interface.
 */
void
tty_set_xaccm(accm)
    ext_accm accm;
{
    if (sync_serial)
	return;

    if (fdmuxid >= 0
	&& strioctl(pppfd, PPPIO_XACCM, accm, sizeof(ext_accm), 0) < 0) {
	if (!hungup || errno != ENXIO)
	    warn("Couldn't set extended ACCM: %m");
    }
}

/*
 * tty_recv_config - configure the receive-side characteristics of
 * the ppp interface.
 */
void
tty_recv_config(mru, asyncmap, pcomp, accomp)
    int mru;
    u_int32_t asyncmap;
    int pcomp, accomp;
{
    int cf[2];

    link_mru = mru;
    if (strioctl(pppfd, PPPIO_MRU, &mru, sizeof(mru), 0) < 0) {
	if (hungup && errno == ENXIO) {
	    ++error_count;
	    return;
	}
	error("Couldn't set MRU: %m");
    }
    if (fdmuxid >= 0) {
	if (!sync_serial) {
	    if (strioctl(pppfd, PPPIO_RACCM, &asyncmap, sizeof(asyncmap), 0) < 0)
		error("Couldn't set receive ACCM: %m");
	}
	cf[0] = (pcomp? DECOMP_PROT: 0) + (accomp? DECOMP_AC: 0);
	cf[1] = DECOMP_PROT | DECOMP_AC;
	if (any_compressions() &&
	    strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0)
	    error("Couldn't set prot/AC decompression: %m");
    }
}

/*
 * ccp_test - ask kernel whether a given compression method
 * is acceptable for use.
 */
int
ccp_test(unit, opt_ptr, opt_len, for_transmit)
    int unit, opt_len, for_transmit;
    u_char *opt_ptr;
{
    if (strioctl(pppfd, (for_transmit? PPPIO_XCOMP: PPPIO_RCOMP),
		 opt_ptr, opt_len, 0) >= 0)
	return 1;
    return (errno == ENOSR)? 0: -1;
}

/*
 * ccp_flags_set - inform kernel about the current state of CCP.
 */
void
ccp_flags_set(unit, isopen, isup)
    int unit, isopen, isup;
{
    int cf[2];

    cf[0] = (isopen? CCP_ISOPEN: 0) + (isup? CCP_ISUP: 0);
    cf[1] = CCP_ISOPEN | CCP_ISUP | CCP_ERROR | CCP_FATALERROR;
    if (strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0) {
	if (!hungup || errno != ENXIO)
	    error("Couldn't set kernel CCP state: %m");
    }
}

/*
 * get_idle_time - return how long the link has been idle.
 */
int
get_idle_time(u, ip)
    int u;
    struct ppp_idle *ip;
{
    return strioctl(pppfd, PPPIO_GIDLE, ip, 0, sizeof(struct ppp_idle)) >= 0;
}

/*
 * get_ppp_stats - return statistics for the link.
 */
int
get_ppp_stats(u, stats)
    int u;
    struct pppd_stats *stats;
{
    struct ppp_stats s;

    if (!sync_serial && 
	strioctl(pppfd, PPPIO_GETSTAT, &s, 0, sizeof(s)) < 0) {
	error("Couldn't get link statistics: %m");
	return 0;
    }
    stats->bytes_in = s.p.ppp_ibytes;
    stats->bytes_out = s.p.ppp_obytes;
    stats->pkts_in = s.p.ppp_ipackets;
    stats->pkts_out = s.p.ppp_opackets;
    return 1;
}

#if 0
/*
 * set_filters - transfer the pass and active filters to the kernel.
 */
int
set_filters(pass, active)
    struct bpf_program *pass, *active;
{
    int ret = 1;

    if (pass->bf_len > 0) {
	if (strioctl(pppfd, PPPIO_PASSFILT, pass,
		     sizeof(struct bpf_program), 0) < 0) {
	    error("Couldn't set pass-filter in kernel: %m");
	    ret = 0;
	}
    }
    if (active->bf_len > 0) {
	if (strioctl(pppfd, PPPIO_ACTIVEFILT, active,
		     sizeof(struct bpf_program), 0) < 0) {
	    error("Couldn't set active-filter in kernel: %m");
	    ret = 0;
	}
    }
    return ret;
}
#endif

/*
 * ccp_fatal_error - returns 1 if decompression was disabled as a
 * result of an error detected after decompression of a packet,
 * 0 otherwise.  This is necessary because of patent nonsense.
 */
int
ccp_fatal_error(unit)
    int unit;
{
    int cf[2];

    cf[0] = cf[1] = 0;
    if (strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0) {
	if (errno != ENXIO && errno != EINVAL)
	    error("Couldn't get compression flags: %m");
	return 0;
    }
    return cf[0] & CCP_FATALERROR;
}

/*
 * sifvjcomp - config tcp header compression
 */
int
sifvjcomp(u, vjcomp, xcidcomp, xmaxcid)
    int u, vjcomp, xcidcomp, xmaxcid;
{
    int cf[2];
    char maxcid[2];

    if (vjcomp) {
	maxcid[0] = xcidcomp;
	maxcid[1] = 15;		/* XXX should be rmaxcid */
	if (strioctl(pppfd, PPPIO_VJINIT, maxcid, sizeof(maxcid), 0) < 0) {
	    error("Couldn't initialize VJ compression: %m");
	}
    }

    cf[0] = (vjcomp? COMP_VJC + DECOMP_VJC: 0)	/* XXX this is wrong */
	+ (xcidcomp? COMP_VJCCID + DECOMP_VJCCID: 0);
    cf[1] = COMP_VJC + DECOMP_VJC + COMP_VJCCID + DECOMP_VJCCID;
    if (strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0) {
	if (vjcomp)
	    error("Couldn't enable VJ compression: %m");
    }

    return 1;
}

/*
 * sifname - Config the interface name.
 */
int
sifname (u, newname)
    int u;
    const char *newname;
{
    error("Couldn't set interface name %s: %s", newname, "Unsupported");
    return 0;
}

/*
 * sifup - Config the interface up and enable IP packets to pass.
 */
int
sifup(u)
    int u;
{
    struct ifreq ifr;

    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(ipfd, SIOCGIFFLAGS, &ifr) < 0) {
	error("Couldn't mark interface up (get): %m");
	return 0;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(ipfd, SIOCSIFFLAGS, &ifr) < 0) {
	error("Couldn't mark interface up (set): %m");
	return 0;
    }
    if_is_up = 1;
    return 1;
}

/*
 * sifdown - Config the interface down and disable IP.
 */
int
sifdown(u)
    int u;
{
    struct ifreq ifr;

    if (ipmuxid < 0)
	return 1;
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(ipfd, SIOCGIFFLAGS, &ifr) < 0) {
	error("Couldn't mark interface down (get): %m");
	return 0;
    }
    ifr.ifr_flags &= ~IFF_UP;
    if (ioctl(ipfd, SIOCSIFFLAGS, &ifr) < 0) {
	error("Couldn't mark interface down (set): %m");
	return 0;
    }
    if_is_up = 0;
    return 1;
}

/*
 * sifnpmode - Set the mode for handling packets for a given NP.
 */
int
sifnpmode(u, proto, mode)
    int u;
    int proto;
    enum NPmode mode;
{
    int npi[2];

    npi[0] = proto;
    npi[1] = (int) mode;
    if (strioctl(pppfd, PPPIO_NPMODE, &npi, 2 * sizeof(int), 0) < 0) {
	error("ioctl(set NP %d mode to %d): %m", proto, mode);
	return 0;
    }
    return 1;
}

#if defined(SOL2) && defined(INET6)
/*
 * sif6up - Config the IPv6 interface up and enable IPv6 packets to pass.
 */
int
sif6up(u)
    int u;
{
    struct lifreq lifr;
    int fd;

    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
	return 0;
    }

    memset(&lifr, 0, sizeof(lifr));
    strlcpy(lifr.lifr_name, ifname, sizeof(lifr.lifr_name));
    if (ioctl(fd, SIOCGLIFFLAGS, &lifr) < 0) {
	close(fd);
	return 0;
    }

    lifr.lifr_flags |= IFF_UP;
    strlcpy(lifr.lifr_name, ifname, sizeof(lifr.lifr_name));
    if (ioctl(fd, SIOCSLIFFLAGS, &lifr) < 0) {
	close(fd);
	return 0;
    }

    if6_is_up = 1;
    close(fd);
    return 1;
}

/*
 * sifdown - Config the IPv6 interface down and disable IPv6.
 */
int
sif6down(u)
    int u;
{
    struct lifreq lifr;
    int fd;

    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0)
	return 0;

    memset(&lifr, 0, sizeof(lifr));
    strlcpy(lifr.lifr_name, ifname, sizeof(lifr.lifr_name));
    if (ioctl(fd, SIOCGLIFFLAGS, &lifr) < 0) {
	close(fd);
	return 0;
    }

    lifr.lifr_flags &= ~IFF_UP;
    strlcpy(lifr.lifr_name, ifname, sizeof(lifr.lifr_name));
    if (ioctl(fd, SIOCGLIFFLAGS, &lifr) < 0) {
	close(fd);
	return 0;
    }

    if6_is_up = 0;
    close(fd);
    return 1;
}

/*
 * sif6addr - Config the interface with an IPv6 link-local address
 */
int
sif6addr(u, o, h)
    int u;
    eui64_t o, h;
{
    struct lifreq lifr;
    struct sockaddr_storage laddr;
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&laddr;
    int fd;

    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0)
	return 0;

    memset(&lifr, 0, sizeof(lifr));
    strlcpy(lifr.lifr_name, ifname, sizeof(lifr.lifr_name));

    /*
     * Do this because /dev/ppp responds to DL_PHYS_ADDR_REQ with
     * zero values, hence the interface token came to be zero too,
     * and without this, in.ndpd will complain
     */
    IN6_LLTOKEN_FROM_EUI64(lifr, sin6, o);
    if (ioctl(fd, SIOCSLIFTOKEN, &lifr) < 0) {
	close(fd);
	return 0;
    }

    /*
     * Set the interface address and destination address
     */
    IN6_LLADDR_FROM_EUI64(lifr, sin6, o);
    if (ioctl(fd, SIOCSLIFADDR, &lifr) < 0) {
	close(fd);
	return 0;
    }

    memset(&lifr, 0, sizeof(lifr));
    strlcpy(lifr.lifr_name, ifname, sizeof(lifr.lifr_name));
    IN6_LLADDR_FROM_EUI64(lifr, sin6, h);
    if (ioctl(fd, SIOCSLIFDSTADDR, &lifr) < 0) {
	close(fd);
	return 0;
    }

    return 1;
}

/*
 * cif6addr - Remove the IPv6 address from interface
 */
int
cif6addr(u, o, h)
    int u;
    eui64_t o, h;
{
    return 1;
}

#endif /* defined(SOL2) && defined(INET6) */


#define INET_ADDR(x)	(((struct sockaddr_in *) &(x))->sin_addr.s_addr)

/*
 * sifaddr - Config the interface IP addresses and netmask.
 */
int
sifaddr(u, o, h, m)
    int u;
    u_int32_t o, h, m;
{
    struct ifreq ifr;
    int ret = 1;

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_addr.sa_family = AF_INET;
    INET_ADDR(ifr.ifr_addr) = m;
    if (ioctl(ipfd, SIOCSIFNETMASK, &ifr) < 0) {
	error("Couldn't set IP netmask: %m");
	ret = 0;
    }
    ifr.ifr_addr.sa_family = AF_INET;
    INET_ADDR(ifr.ifr_addr) = o;
    if (ioctl(ipfd, SIOCSIFADDR, &ifr) < 0) {
	error("Couldn't set local IP address: %m");
	ret = 0;
    }

    /*
     * On some systems, we have to explicitly set the point-to-point
     * flag bit before we can set a destination address.
     */
    if (ioctl(ipfd, SIOCGIFFLAGS, &ifr) >= 0
	&& (ifr.ifr_flags & IFF_POINTOPOINT) == 0) {
	ifr.ifr_flags |= IFF_POINTOPOINT;
	if (ioctl(ipfd, SIOCSIFFLAGS, &ifr) < 0) {
	    error("Couldn't mark interface pt-to-pt: %m");
	    ret = 0;
	}
    }
    ifr.ifr_dstaddr.sa_family = AF_INET;
    INET_ADDR(ifr.ifr_dstaddr) = h;
    if (ioctl(ipfd, SIOCSIFDSTADDR, &ifr) < 0) {
	error("Couldn't set remote IP address: %m");
	ret = 0;
    }

    remote_addr = h;
    return ret;
}

/*
 * cifaddr - Clear the interface IP addresses, and delete routes
 * through the interface if possible.
 */
int
cifaddr(u, o, h)
    int u;
    u_int32_t o, h;
{
#if defined(__USLC__)		/* was: #if 0 */
    cifroute(unit, ouraddr, hisaddr);
    if (ipmuxid >= 0) {
	notice("Removing ppp interface unit");
	if (ioctl(ipfd, I_UNLINK, ipmuxid) < 0) {
	    error("Can't remove ppp interface unit: %m");
	    return 0;
	}
	ipmuxid = -1;
    }
#endif
    remote_addr = 0;
    return 1;
}

/*
 * sifdefaultroute - assign a default route through the address given.
 */
int
sifdefaultroute(u, l, g)
    int u;
    u_int32_t l, g;
{
    struct rtentry rt;

#if defined(__USLC__)
    g = l;			/* use the local address as gateway */
#endif
    memset(&rt, 0, sizeof(rt));
    rt.rt_dst.sa_family = AF_INET;
    INET_ADDR(rt.rt_dst) = 0;
    rt.rt_gateway.sa_family = AF_INET;
    INET_ADDR(rt.rt_gateway) = g;
    rt.rt_flags = RTF_GATEWAY;

    if (ioctl(ipfd, SIOCADDRT, &rt) < 0) {
	error("Can't add default route: %m");
	return 0;
    }

    default_route_gateway = g;
    return 1;
}

/*
 * cifdefaultroute - delete a default route through the address given.
 */
int
cifdefaultroute(u, l, g)
    int u;
    u_int32_t l, g;
{
    struct rtentry rt;

#if defined(__USLC__)
    g = l;			/* use the local address as gateway */
#endif
    memset(&rt, 0, sizeof(rt));
    rt.rt_dst.sa_family = AF_INET;
    INET_ADDR(rt.rt_dst) = 0;
    rt.rt_gateway.sa_family = AF_INET;
    INET_ADDR(rt.rt_gateway) = g;
    rt.rt_flags = RTF_GATEWAY;

    if (ioctl(ipfd, SIOCDELRT, &rt) < 0) {
	error("Can't delete default route: %m");
	return 0;
    }

    default_route_gateway = 0;
    return 1;
}

/*
 * sifproxyarp - Make a proxy ARP entry for the peer.
 */
int
sifproxyarp(unit, hisaddr)
    int unit;
    u_int32_t hisaddr;
{
    struct arpreq arpreq;

    memset(&arpreq, 0, sizeof(arpreq));
    if (!get_ether_addr(hisaddr, &arpreq.arp_ha))
	return 0;

    arpreq.arp_pa.sa_family = AF_INET;
    INET_ADDR(arpreq.arp_pa) = hisaddr;
    arpreq.arp_flags = ATF_PERM | ATF_PUBL;
    if (ioctl(ipfd, SIOCSARP, (caddr_t) &arpreq) < 0) {
	error("Couldn't set proxy ARP entry: %m");
	return 0;
    }

    proxy_arp_addr = hisaddr;
    return 1;
}

/*
 * cifproxyarp - Delete the proxy ARP entry for the peer.
 */
int
cifproxyarp(unit, hisaddr)
    int unit;
    u_int32_t hisaddr;
{
    struct arpreq arpreq;

    memset(&arpreq, 0, sizeof(arpreq));
    arpreq.arp_pa.sa_family = AF_INET;
    INET_ADDR(arpreq.arp_pa) = hisaddr;
    if (ioctl(ipfd, SIOCDARP, (caddr_t)&arpreq) < 0) {
	error("Couldn't delete proxy ARP entry: %m");
	return 0;
    }

    proxy_arp_addr = 0;
    return 1;
}

/*
 * get_ether_addr - get the hardware address of an interface on the
 * the same subnet as ipaddr.
 */
#define MAX_IFS		32

static int
get_ether_addr(ipaddr, hwaddr)
    u_int32_t ipaddr;
    struct sockaddr *hwaddr;
{
    struct ifreq *ifr, *ifend, ifreq;
    int nif;
    struct ifconf ifc;
    u_int32_t ina, mask;

    /*
     * Scan through the system's network interfaces.
     */
#ifdef SIOCGIFNUM
    if (ioctl(ipfd, SIOCGIFNUM, &nif) < 0)
#endif
	nif = MAX_IFS;
    ifc.ifc_len = nif * sizeof(struct ifreq);
    ifc.ifc_buf = (caddr_t) malloc(ifc.ifc_len);
    if (ifc.ifc_buf == 0)
	return 0;
    if (ioctl(ipfd, SIOCGIFCONF, &ifc) < 0) {
	warn("Couldn't get system interface list: %m");
	free(ifc.ifc_buf);
	return 0;
    }
    ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifr = ifc.ifc_req; ifr < ifend; ++ifr) {
	if (ifr->ifr_addr.sa_family != AF_INET)
	    continue;
	/*
	 * Check that the interface is up, and not point-to-point or loopback.
	 */
	strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
	if (ioctl(ipfd, SIOCGIFFLAGS, &ifreq) < 0)
	    continue;
	if ((ifreq.ifr_flags &
	     (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|IFF_LOOPBACK|IFF_NOARP))
	    != (IFF_UP|IFF_BROADCAST))
	    continue;
	/*
	 * Get its netmask and check that it's on the right subnet.
	 */
	if (ioctl(ipfd, SIOCGIFNETMASK, &ifreq) < 0)
	    continue;
	ina = INET_ADDR(ifr->ifr_addr);
	mask = INET_ADDR(ifreq.ifr_addr);
	if ((ipaddr & mask) == (ina & mask))
	    break;
    }

    if (ifr >= ifend) {
	warn("No suitable interface found for proxy ARP");
	free(ifc.ifc_buf);
	return 0;
    }

    info("found interface %s for proxy ARP", ifr->ifr_name);
    if (!get_hw_addr(ifr->ifr_name, ina, hwaddr)) {
	error("Couldn't get hardware address for %s", ifr->ifr_name);
	free(ifc.ifc_buf);
	return 0;
    }

    free(ifc.ifc_buf);
    return 1;
}

/*
 * get_hw_addr_dlpi - obtain the hardware address using DLPI
 */
static int
get_hw_addr_dlpi(name, hwaddr)
    char *name;
    struct sockaddr *hwaddr;
{
    char *q;
    int unit, iffd, adrlen;
    unsigned char *adrp;
    char ifdev[24];
    struct {
	union DL_primitives prim;
	char space[64];
    } reply;

    /*
     * We have to open the device and ask it for its hardware address.
     * First split apart the device name and unit.
     */
    slprintf(ifdev, sizeof(ifdev), "/dev/%s", name);
    for (q = ifdev + strlen(ifdev); --q >= ifdev; )
	if (!isdigit(*q))
	    break;
    unit = atoi(q+1);
    q[1] = 0;

    /*
     * Open the device and do a DLPI attach and phys_addr_req.
     */
    iffd = open(ifdev, O_RDWR);
    if (iffd < 0) {
	error("Can't open %s: %m", ifdev);
	return 0;
    }
    if (dlpi_attach(iffd, unit) < 0
	|| dlpi_get_reply(iffd, &reply.prim, DL_OK_ACK, sizeof(reply)) < 0
	|| dlpi_info_req(iffd) < 0
	|| dlpi_get_reply(iffd, &reply.prim, DL_INFO_ACK, sizeof(reply)) < 0) {
	close(iffd);
	return 0;
    }

    adrlen = reply.prim.info_ack.dl_addr_length;
    adrp = (unsigned char *)&reply + reply.prim.info_ack.dl_addr_offset;

#if DL_CURRENT_VERSION >= 2
    if (reply.prim.info_ack.dl_sap_length < 0)
	adrlen += reply.prim.info_ack.dl_sap_length;
    else
	adrp += reply.prim.info_ack.dl_sap_length;
#endif

    hwaddr->sa_family = AF_UNSPEC;
    memcpy(hwaddr->sa_data, adrp, adrlen);

    return 1;
}
/*
 * get_hw_addr - obtain the hardware address for a named interface.
 */
static int
get_hw_addr(name, ina, hwaddr)
    char *name;
    u_int32_t ina;
    struct sockaddr *hwaddr;
{
    /* New way - get the address by doing an arp request. */
    int s;
    struct arpreq req;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
	return 0;
    memset(&req, 0, sizeof(req));
    req.arp_pa.sa_family = AF_INET;
    INET_ADDR(req.arp_pa) = ina;
    if (ioctl(s, SIOCGARP, &req) < 0) {
	error("Couldn't get ARP entry for %s: %m", ip_ntoa(ina));
	return 0;
    }
    *hwaddr = req.arp_ha;
    hwaddr->sa_family = AF_UNSPEC;

    return 1;
}

static int
dlpi_attach(fd, ppa)
    int fd, ppa;
{
    dl_attach_req_t req;
    struct strbuf buf;

    req.dl_primitive = DL_ATTACH_REQ;
    req.dl_ppa = ppa;
    buf.len = sizeof(req);
    buf.buf = (void *) &req;
    return putmsg(fd, &buf, NULL, RS_HIPRI);
}

static int
dlpi_info_req(fd)
    int fd;
{
    dl_info_req_t req;
    struct strbuf buf;

    req.dl_primitive = DL_INFO_REQ;
    buf.len = sizeof(req);
    buf.buf = (void *) &req;
    return putmsg(fd, &buf, NULL, RS_HIPRI);
}

static int
dlpi_get_reply(fd, reply, expected_prim, maxlen)
    union DL_primitives *reply;
    int fd, expected_prim, maxlen;
{
    struct strbuf buf;
    int flags, n;
    struct pollfd pfd;

    /*
     * Use poll to wait for a message with a timeout.
     */
    pfd.fd = fd;
    pfd.events = POLLIN | POLLPRI;
    do {
	n = poll(&pfd, 1, 1000);
    } while (n == -1 && errno == EINTR);
    if (n <= 0)
	return -1;

    /*
     * Get the reply.
     */
    buf.maxlen = maxlen;
    buf.buf = (void *) reply;
    flags = 0;
    if (getmsg(fd, &buf, NULL, &flags) < 0)
	return -1;

    if (buf.len < sizeof(ulong)) {
	if (debug)
	    dbglog("dlpi response short (len=%d)\n", buf.len);
	return -1;
    }

    if (reply->dl_primitive == expected_prim)
	return 0;

    if (debug) {
	if (reply->dl_primitive == DL_ERROR_ACK) {
	    dbglog("dlpi error %d (unix errno %d) for prim %x\n",
		   reply->error_ack.dl_errno, reply->error_ack.dl_unix_errno,
		   reply->error_ack.dl_error_primitive);
	} else {
	    dbglog("dlpi unexpected response prim %x\n",
		   reply->dl_primitive);
	}
    }

    return -1;
}

/*
 * Return user specified netmask, modified by any mask we might determine
 * for address `addr' (in network byte order).
 * Here we scan through the system's list of interfaces, looking for
 * any non-point-to-point interfaces which might appear to be on the same
 * network as `addr'.  If we find any, we OR in their netmask to the
 * user-specified netmask.
 */
u_int32_t
GetMask(addr)
    u_int32_t addr;
{
    u_int32_t mask, nmask, ina;
    struct ifreq *ifr, *ifend, ifreq;
    int nif;
    struct ifconf ifc;

    addr = ntohl(addr);
    if (IN_CLASSA(addr))	/* determine network mask for address class */
	nmask = IN_CLASSA_NET;
    else if (IN_CLASSB(addr))
	nmask = IN_CLASSB_NET;
    else
	nmask = IN_CLASSC_NET;
    /* class D nets are disallowed by bad_ip_adrs */
    mask = netmask | htonl(nmask);

    /*
     * Scan through the system's network interfaces.
     */
#ifdef SIOCGIFNUM
    if (ioctl(ipfd, SIOCGIFNUM, &nif) < 0)
#endif
	nif = MAX_IFS;
    ifc.ifc_len = nif * sizeof(struct ifreq);
    ifc.ifc_buf = (caddr_t) malloc(ifc.ifc_len);
    if (ifc.ifc_buf == 0)
	return mask;
    if (ioctl(ipfd, SIOCGIFCONF, &ifc) < 0) {
	warn("Couldn't get system interface list: %m");
	free(ifc.ifc_buf);
	return mask;
    }
    ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifr = ifc.ifc_req; ifr < ifend; ++ifr) {
	/*
	 * Check the interface's internet address.
	 */
	if (ifr->ifr_addr.sa_family != AF_INET)
	    continue;
	ina = INET_ADDR(ifr->ifr_addr);
	if ((ntohl(ina) & nmask) != (addr & nmask))
	    continue;
	/*
	 * Check that the interface is up, and not point-to-point or loopback.
	 */
	strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
	if (ioctl(ipfd, SIOCGIFFLAGS, &ifreq) < 0)
	    continue;
	if ((ifreq.ifr_flags & (IFF_UP|IFF_POINTOPOINT|IFF_LOOPBACK))
	    != IFF_UP)
	    continue;
	/*
	 * Get its netmask and OR it into our mask.
	 */
	if (ioctl(ipfd, SIOCGIFNETMASK, &ifreq) < 0)
	    continue;
	mask |= INET_ADDR(ifreq.ifr_addr);
    }

    free(ifc.ifc_buf);
    return mask;
}

/*
 * logwtmp - write an accounting record to the /var/adm/wtmp file.
 */
void
logwtmp(line, name, host)
    const char *line, *name, *host;
{
    static struct utmpx utmpx;

    if (name[0] != 0) {
	/* logging in */
	strncpy(utmpx.ut_user, name, sizeof(utmpx.ut_user));
	strncpy(utmpx.ut_line, line, sizeof(utmpx.ut_line));
	strncpy(utmpx.ut_host, host, sizeof(utmpx.ut_host));
	if (*host != '\0') {
	    utmpx.ut_syslen = strlen(host) + 1;
	    if (utmpx.ut_syslen > sizeof(utmpx.ut_host))
		utmpx.ut_syslen = sizeof(utmpx.ut_host);
	}
	utmpx.ut_pid = getpid();
	utmpx.ut_type = USER_PROCESS;
    } else {
	utmpx.ut_type = DEAD_PROCESS;
    }
    gettimeofday(&utmpx.ut_tv, NULL);
    updwtmpx("/var/adm/wtmpx", &utmpx);
}

/*
 * get_host_seed - return the serial number of this machine.
 */
int
get_host_seed()
{
    char buf[32];

    if (sysinfo(SI_HW_SERIAL, buf, sizeof(buf)) < 0) {
	error("sysinfo: %m");
	return 0;
    }
    return (int) strtoul(buf, NULL, 16);
}

static int
strioctl(fd, cmd, ptr, ilen, olen)
    int fd, cmd, ilen, olen;
    void *ptr;
{
    struct strioctl str;

    str.ic_cmd = cmd;
    str.ic_timout = 0;
    str.ic_len = ilen;
    str.ic_dp = ptr;
    if (ioctl(fd, I_STR, &str) == -1)
	return -1;
    if (str.ic_len != olen)
	dbglog("strioctl: expected %d bytes, got %d for cmd %x\n",
	       olen, str.ic_len, cmd);
    return 0;
}

#if 0
/*
 * lock - create a lock file for the named lock device
 */

#define LOCK_PREFIX	"/var/spool/locks/LK."
static char lock_file[40];	/* name of lock file created */

int
lock(dev)
    char *dev;
{
    int n, fd, pid;
    struct stat sbuf;
    char ascii_pid[12];

    if (stat(dev, &sbuf) < 0) {
	error("Can't get device number for %s: %m", dev);
	return -1;
    }
    if ((sbuf.st_mode & S_IFMT) != S_IFCHR) {
	error("Can't lock %s: not a character device", dev);
	return -1;
    }
    slprintf(lock_file, sizeof(lock_file), "%s%03d.%03d.%03d",
	     LOCK_PREFIX, major(sbuf.st_dev),
	     major(sbuf.st_rdev), minor(sbuf.st_rdev));

    while ((fd = open(lock_file, O_EXCL | O_CREAT | O_RDWR, 0644)) < 0) {
	if (errno == EEXIST
	    && (fd = open(lock_file, O_RDONLY, 0)) >= 0) {
	    /* Read the lock file to find out who has the device locked */
	    n = read(fd, ascii_pid, 11);
	    if (n <= 0) {
		error("Can't read pid from lock file %s", lock_file);
		close(fd);
	    } else {
		ascii_pid[n] = 0;
		pid = atoi(ascii_pid);
		if (pid > 0 && kill(pid, 0) == -1 && errno == ESRCH) {
		    /* pid no longer exists - remove the lock file */
		    if (unlink(lock_file) == 0) {
			close(fd);
			notice("Removed stale lock on %s (pid %d)",
			       dev, pid);
			continue;
		    } else
			warn("Couldn't remove stale lock on %s",
			       dev);
		} else
		    notice("Device %s is locked by pid %d",
			   dev, pid);
	    }
	    close(fd);
	} else
	    error("Can't create lock file %s: %m", lock_file);
	lock_file[0] = 0;
	return -1;
    }

    slprintf(ascii_pid, sizeof(ascii_pid), "%10d\n", getpid());
    write(fd, ascii_pid, 11);

    close(fd);
    return 1;
}

/*
 * unlock - remove our lockfile
 */
void
unlock()
{
    if (lock_file[0]) {
	unlink(lock_file);
	lock_file[0] = 0;
    }
}
#endif

/*
 * cifroute - delete a route through the addresses given.
 */
int
cifroute(u, our, his)
    int u;
    u_int32_t our, his;
{
    struct rtentry rt;

    memset(&rt, 0, sizeof(rt));
    rt.rt_dst.sa_family = AF_INET;
    INET_ADDR(rt.rt_dst) = his;
    rt.rt_gateway.sa_family = AF_INET;
    INET_ADDR(rt.rt_gateway) = our;
    rt.rt_flags = RTF_HOST;

    if (ioctl(ipfd, SIOCDELRT, &rt) < 0) {
	error("Can't delete route: %m");
	return 0;
    }

    return 1;
}

/*
 * have_route_to - determine if the system has a route to the specified
 * IP address.  Returns 0 if not, 1 if so, -1 if we can't tell.
 * `addr' is in network byte order.
 * For demand mode to work properly, we have to ignore routes
 * through our own interface.
 */
#ifndef T_CURRENT		/* needed for Solaris 2.5 */
#define T_CURRENT	MI_T_CURRENT
#endif

int
have_route_to(addr)
    u_int32_t addr;
{
#ifdef SOL2
    int fd, r, flags, i;
    struct {
	struct T_optmgmt_req req;
	struct opthdr hdr;
    } req;
    union {
	struct T_optmgmt_ack ack;
	unsigned char space[64];
    } ack;
    struct opthdr *rh;
    struct strbuf cbuf, dbuf;
    int nroutes;
    mib2_ipRouteEntry_t routes[8];
    mib2_ipRouteEntry_t *rp;

    fd = open(mux_dev_name, O_RDWR);
    if (fd < 0) {
	warn("have_route_to: couldn't open %s: %m", mux_dev_name);
	return -1;
    }

    req.req.PRIM_type = T_OPTMGMT_REQ;
    req.req.OPT_offset = (char *) &req.hdr - (char *) &req;
    req.req.OPT_length = sizeof(req.hdr);
    req.req.MGMT_flags = T_CURRENT;

    req.hdr.level = MIB2_IP;
    req.hdr.name = 0;
    req.hdr.len = 0;

    cbuf.buf = (char *) &req;
    cbuf.len = sizeof(req);

    if (putmsg(fd, &cbuf, NULL, 0) == -1) {
	warn("have_route_to: putmsg: %m");
	close(fd);
	return -1;
    }

    for (;;) {
	cbuf.buf = (char *) &ack;
	cbuf.maxlen = sizeof(ack);
	dbuf.buf = (char *) routes;
	dbuf.maxlen = sizeof(routes);
	flags = 0;
	r = getmsg(fd, &cbuf, &dbuf, &flags);
	if (r == -1) {
	    warn("have_route_to: getmsg: %m");
	    close(fd);
	    return -1;
	}

	if (cbuf.len < sizeof(struct T_optmgmt_ack)
	    || ack.ack.PRIM_type != T_OPTMGMT_ACK
	    || ack.ack.MGMT_flags != T_SUCCESS
	    || ack.ack.OPT_length < sizeof(struct opthdr)) {
	    dbglog("have_route_to: bad message len=%d prim=%d",
		   cbuf.len, ack.ack.PRIM_type);
	    close(fd);
	    return -1;
	}

	rh = (struct opthdr *) ((char *)&ack + ack.ack.OPT_offset);
	if (rh->level == 0 && rh->name == 0)
	    break;
	if (rh->level != MIB2_IP || rh->name != MIB2_IP_21) {
	    while (r == MOREDATA)
		r = getmsg(fd, NULL, &dbuf, &flags);
	    continue;
	}

	for (;;) {
	    nroutes = dbuf.len / sizeof(mib2_ipRouteEntry_t);
	    for (rp = routes, i = 0; i < nroutes; ++i, ++rp) {
		if (rp->ipRouteMask != ~0) {
		    dbglog("have_route_to: dest=%x gw=%x mask=%x\n",
			   rp->ipRouteDest, rp->ipRouteNextHop,
			   rp->ipRouteMask);
		    if (((addr ^ rp->ipRouteDest) & rp->ipRouteMask) == 0
			&& rp->ipRouteNextHop != remote_addr)
			return 1;
		}
	    }
	    if (r == 0)
		break;
	    r = getmsg(fd, NULL, &dbuf, &flags);
	}
    }
    close(fd);
    return 0;
#else
    return -1;
#endif /* SOL2 */
}

/*
 * get_pty - get a pty master/slave pair and chown the slave side to
 * the uid given.  Assumes slave_name points to MAXPATHLEN bytes of space.
 */
int
get_pty(master_fdp, slave_fdp, slave_name, uid)
    int *master_fdp;
    int *slave_fdp;
    char *slave_name;
    int uid;
{
    int mfd, sfd;
    char *pty_name;

    mfd = open("/dev/ptmx", O_RDWR);
    if (mfd < 0) {
	error("Couldn't open pty master: %m");
	return 0;
    }

    pty_name = ptsname(mfd);
    if (pty_name == NULL) {
	error("Couldn't get name of pty slave");
	close(mfd);
	return 0;
    }
    if (chown(pty_name, uid, -1) < 0)
	warn("Couldn't change owner of pty slave: %m");
    if (chmod(pty_name, S_IRUSR | S_IWUSR) < 0)
	warn("Couldn't change permissions on pty slave: %m");
    if (unlockpt(mfd) < 0)
	warn("Couldn't unlock pty slave: %m");

    sfd = open(pty_name, O_RDWR);
    if (sfd < 0) {
	error("Couldn't open pty slave %s: %m", pty_name);
	close(mfd);
	return 0;
    }
    if (ioctl(sfd, I_PUSH, "ptem") < 0)
	warn("Couldn't push ptem module on pty slave: %m");

    dbglog("Using %s", pty_name);
    strlcpy(slave_name, pty_name, MAXPATHLEN);
    *master_fdp = mfd;
    *slave_fdp = sfd;

    return 1;
}
