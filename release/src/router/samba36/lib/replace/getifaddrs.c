/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Jeremy Allison 2007
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007

     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#define SOCKET_WRAPPER_NOT_REPLACE

#include "replace.h"
#include "system/network.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef SIOCGIFCONF
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#endif

#ifdef HAVE_IFACE_GETIFADDRS
#define _FOUND_IFACE_ANY
#else

void rep_freeifaddrs(struct ifaddrs *ifp)
{
	if (ifp != NULL) {
		free(ifp->ifa_name);
		free(ifp->ifa_addr);
		free(ifp->ifa_netmask);
		free(ifp->ifa_dstaddr);
		freeifaddrs(ifp->ifa_next);
		free(ifp);
	}
}

static struct sockaddr *sockaddr_dup(struct sockaddr *sa)
{
	struct sockaddr *ret;
	socklen_t socklen;
#ifdef HAVE_SOCKADDR_SA_LEN
	socklen = sa->sa_len;
#else
	socklen = sizeof(struct sockaddr_storage);
#endif
	ret = calloc(1, socklen);
	if (ret == NULL)
		return NULL;
	memcpy(ret, sa, socklen);
	return ret;
}
#endif

#if HAVE_IFACE_IFCONF

/* this works for Linux 2.2, Solaris 2.5, SunOS4, HPUX 10.20, OSF1
   V4.0, Ultrix 4.4, SCO Unix 3.2, IRIX 6.4 and FreeBSD 3.2.

   It probably also works on any BSD style system.  */

int rep_getifaddrs(struct ifaddrs **ifap)
{
	struct ifconf ifc;
	char buff[8192];
	int fd, i, n;
	struct ifreq *ifr=NULL;
	struct ifaddrs *curif;
	struct ifaddrs *lastif = NULL;

	*ifap = NULL;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		return -1;
	}
  
	ifc.ifc_len = sizeof(buff);
	ifc.ifc_buf = buff;

	if (ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
		close(fd);
		return -1;
	} 

	ifr = ifc.ifc_req;
  
	n = ifc.ifc_len / sizeof(struct ifreq);

	/* Loop through interfaces, looking for given IP address */
	for (i=n-1; i>=0; i--) {
		if (ioctl(fd, SIOCGIFFLAGS, &ifr[i]) == -1) {
			freeifaddrs(*ifap);
			return -1;
		}

		curif = calloc(1, sizeof(struct ifaddrs));
		curif->ifa_name = strdup(ifr[i].ifr_name);
		curif->ifa_flags = ifr[i].ifr_flags;
		curif->ifa_dstaddr = NULL;
		curif->ifa_data = NULL;
		curif->ifa_next = NULL;

		curif->ifa_addr = NULL;
		if (ioctl(fd, SIOCGIFADDR, &ifr[i]) != -1) {
			curif->ifa_addr = sockaddr_dup(&ifr[i].ifr_addr);
		}

		curif->ifa_netmask = NULL;
		if (ioctl(fd, SIOCGIFNETMASK, &ifr[i]) != -1) {
			curif->ifa_netmask = sockaddr_dup(&ifr[i].ifr_addr);
		}

		if (lastif == NULL) {
			*ifap = curif;
		} else {
			lastif->ifa_next = curif;
		}
		lastif = curif;
	}

	close(fd);

	return 0;
}  

#define _FOUND_IFACE_ANY
#endif /* HAVE_IFACE_IFCONF */
#ifdef HAVE_IFACE_IFREQ

#ifndef I_STR
#include <sys/stropts.h>
#endif

/****************************************************************************
this should cover most of the streams based systems
Thanks to Andrej.Borsenkow@mow.siemens.ru for several ideas in this code
****************************************************************************/
int rep_getifaddrs(struct ifaddrs **ifap)
{
	struct ifreq ifreq;
	struct strioctl strioctl;
	char buff[8192];
	int fd, i, n;
	struct ifreq *ifr=NULL;
	struct ifaddrs *curif;
	struct ifaddrs *lastif = NULL;

	*ifap = NULL;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		return -1;
	}
  
	strioctl.ic_cmd = SIOCGIFCONF;
	strioctl.ic_dp  = buff;
	strioctl.ic_len = sizeof(buff);
	if (ioctl(fd, I_STR, &strioctl) < 0) {
		close(fd);
		return -1;
	} 

	/* we can ignore the possible sizeof(int) here as the resulting
	   number of interface structures won't change */
	n = strioctl.ic_len / sizeof(struct ifreq);

	/* we will assume that the kernel returns the length as an int
           at the start of the buffer if the offered size is a
           multiple of the structure size plus an int */
	if (n*sizeof(struct ifreq) + sizeof(int) == strioctl.ic_len) {
		ifr = (struct ifreq *)(buff + sizeof(int));  
	} else {
		ifr = (struct ifreq *)buff;  
	}

	/* Loop through interfaces */

	for (i = 0; i<n; i++) {
		ifreq = ifr[i];
  
		curif = calloc(1, sizeof(struct ifaddrs));
		if (lastif == NULL) {
			*ifap = curif;
		} else {
			lastif->ifa_next = curif;
		}

		strioctl.ic_cmd = SIOCGIFFLAGS;
		strioctl.ic_dp  = (char *)&ifreq;
		strioctl.ic_len = sizeof(struct ifreq);
		if (ioctl(fd, I_STR, &strioctl) != 0) {
			freeifaddrs(*ifap);
			return -1;
		}

		curif->ifa_flags = ifreq.ifr_flags;
		
		strioctl.ic_cmd = SIOCGIFADDR;
		strioctl.ic_dp  = (char *)&ifreq;
		strioctl.ic_len = sizeof(struct ifreq);
		if (ioctl(fd, I_STR, &strioctl) != 0) {
			freeifaddrs(*ifap);
			return -1;
		}

		curif->ifa_name = strdup(ifreq.ifr_name);
		curif->ifa_addr = sockaddr_dup(&ifreq.ifr_addr);
		curif->ifa_dstaddr = NULL;
		curif->ifa_data = NULL;
		curif->ifa_next = NULL;
		curif->ifa_netmask = NULL;

		strioctl.ic_cmd = SIOCGIFNETMASK;
		strioctl.ic_dp  = (char *)&ifreq;
		strioctl.ic_len = sizeof(struct ifreq);
		if (ioctl(fd, I_STR, &strioctl) != 0) {
			freeifaddrs(*ifap);
			return -1;
		}

		curif->ifa_netmask = sockaddr_dup(&ifreq.ifr_addr);

		lastif = curif;
	}

	close(fd);

	return 0;
}

#define _FOUND_IFACE_ANY
#endif /* HAVE_IFACE_IFREQ */
#ifdef HAVE_IFACE_AIX

/****************************************************************************
this one is for AIX (tested on 4.2)
****************************************************************************/
int rep_getifaddrs(struct ifaddrs **ifap)
{
	char buff[8192];
	int fd, i;
	struct ifconf ifc;
	struct ifreq *ifr=NULL;
	struct ifaddrs *curif;
	struct ifaddrs *lastif = NULL;

	*ifap = NULL;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		return -1;
	}

	ifc.ifc_len = sizeof(buff);
	ifc.ifc_buf = buff;

	if (ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
		close(fd);
		return -1;
	}

	ifr = ifc.ifc_req;

	/* Loop through interfaces */
	i = ifc.ifc_len;

	while (i > 0) {
		unsigned int inc;

		inc = ifr->ifr_addr.sa_len;

		if (ioctl(fd, SIOCGIFADDR, ifr) != 0) {
			freeaddrinfo(*ifap);
			return -1;
		}

		curif = calloc(1, sizeof(struct ifaddrs));
		if (lastif == NULL) {
			*ifap = curif;
		} else {
			lastif->ifa_next = curif;
		}

		curif->ifa_name = strdup(ifr->ifr_name);
		curif->ifa_addr = sockaddr_dup(&ifr->ifr_addr);
		curif->ifa_dstaddr = NULL;
		curif->ifa_data = NULL;
		curif->ifa_netmask = NULL;
		curif->ifa_next = NULL;

		if (ioctl(fd, SIOCGIFFLAGS, ifr) != 0) {
			freeaddrinfo(*ifap);
			return -1;
		}

		curif->ifa_flags = ifr->ifr_flags;

		if (ioctl(fd, SIOCGIFNETMASK, ifr) != 0) {
			freeaddrinfo(*ifap);
			return -1;
		}

		curif->ifa_netmask = sockaddr_dup(&ifr->ifr_addr);

		lastif = curif;

	next:
		/*
		 * Patch from Archie Cobbs (archie@whistle.com).  The
		 * addresses in the SIOCGIFCONF interface list have a
		 * minimum size. Usually this doesn't matter, but if
		 * your machine has tunnel interfaces, etc. that have
		 * a zero length "link address", this does matter.  */

		if (inc < sizeof(ifr->ifr_addr))
			inc = sizeof(ifr->ifr_addr);
		inc += IFNAMSIZ;

		ifr = (struct ifreq*) (((char*) ifr) + inc);
		i -= inc;
	}

	close(fd);
	return 0;
}

#define _FOUND_IFACE_ANY
#endif /* HAVE_IFACE_AIX */
#ifndef _FOUND_IFACE_ANY
int rep_getifaddrs(struct ifaddrs **ifap)
{
	errno = ENOSYS;
	return -1;
}
#endif
