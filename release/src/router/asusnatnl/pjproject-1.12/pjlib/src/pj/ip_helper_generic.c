/* $Id: ip_helper_generic.c 4403 2013-02-27 14:43:38Z riza $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/ip_helper.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>
#include <pj/compat/socket.h>
#include <pj/sock.h>
#include <pj/sock_select.h>

#if defined(PJ_CONFIG_IPHONE) && PJ_CONFIG_IPHONE != 0 //BSD
#include <net/if_dl.h>
#endif

/* Set to 1 to enable tracing */
#if 1
#   include <pj/log.h>
#   define THIS_FILE	"ip_helper_generic.c"
#   define TRACE_(exp)	PJ_LOG(5,exp)
    static const char *get_os_errmsg(void)
    {
	static char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(pj_get_os_error(), errmsg, sizeof(errmsg));
	return errmsg;
    }
    static const char *get_addr(void *addr)
    {
	static char txt[PJ_INET6_ADDRSTRLEN];
	struct sockaddr *ad = (struct sockaddr*)addr;
	if (ad->sa_family != PJ_AF_INET && ad->sa_family != PJ_AF_INET6)
    {
        PJ_LOG(1, (THIS_FILE, "ad->sa_family=%d", ad->sa_family));
	    return "?";
    }
	return pj_inet_ntop2(ad->sa_family, pj_sockaddr_get_addr(ad),
			     txt, sizeof(txt));
    }
#else
#   define TRACE_(exp)
#endif


#if 0
    /* dummy */

#elif !defined(PJ_DARWINOS) && \
      defined(SIOCGIFCONF) && \
      defined(PJ_HAS_NET_IF_H) && PJ_HAS_NET_IF_H != 0

/* Note: this does not work with IPv6 */
static pj_status_t if_enum_by_af(int af,
				 unsigned *p_cnt,
				 pj_sockaddr ifs[])
{
    pj_sock_t sock;
    char buf[512];
    struct ifconf ifc;
    struct ifreq *ifr;
    int i, count;
    pj_status_t status;

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);
    
    TRACE_((THIS_FILE, "Starting interface enum with SIOCGIFCONF for af=%d",
	    af));

    status = pj_sock_socket(af, PJ_SOCK_DGRAM, 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Query available interfaces */
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
	int oserr = pj_get_netos_error();
	TRACE_((THIS_FILE, " ioctl(SIOCGIFCONF) failed: %s", get_os_errmsg()));
	pj_sock_close(sock);
	return PJ_RETURN_OS_ERROR(oserr);
    }

    /* Interface interfaces */
    ifr = (struct ifreq*) ifc.ifc_req;
    count = ifc.ifc_len / sizeof(struct ifreq);
    if (count > *p_cnt)
	count = *p_cnt;

    *p_cnt = 0;	
    for (i=0; i<count; ++i) {    
	struct ifreq *itf = &ifr[i];
        struct ifreq iff = *itf;
	struct sockaddr *ad = &itf->ifr_addr;
	
	TRACE_((THIS_FILE, " checking interface %s", itf->ifr_name));

	/* Skip address with different family */
	if (ad->sa_family != af) {
	    TRACE_((THIS_FILE, "  address %s (af=%d) ignored",
		    get_addr(ad), (int)ad->sa_family));       
	    continue;
	}
		    
        if (ioctl(sock, SIOCGIFFLAGS, &iff) != 0) {
	    TRACE_((THIS_FILE, "  ioctl(SIOCGIFFLAGS) failed: %s",
		    get_os_errmsg()));
	    continue;	/* Failed to get flags, continue */
	}
    
	if ((iff.ifr_flags & IFF_UP)==0) {
	    TRACE_((THIS_FILE, "  interface is down"));
	    continue; /* Skip when interface is down */
	}

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
	if (iff.ifr_flags & IFF_LOOPBACK) {
	    TRACE_((THIS_FILE, "  loopback interface"));
	    continue; /* Skip loopback interface */
	}
#endif

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if (af==pj_AF_INET() &&
	    (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
	{
	    TRACE_((THIS_FILE, "  address %s ignored (0.0.0.0/8 class)", 
		    get_addr(ad), ad->sa_family));       
	    continue;
	}

	TRACE_((THIS_FILE, "  address %s (af=%d) added at index %d", 
		get_addr(ad), ad->sa_family, *p_cnt));

	pj_bzero(&ifs[*p_cnt], sizeof(ifs[0]));
	pj_memcpy(&ifs[*p_cnt], ad, pj_sockaddr_get_len(ad));
	PJ_SOCKADDR_RESET_LEN(&ifs[*p_cnt]);
	(*p_cnt)++;
    }

    /* Done with socket */
    pj_sock_close(sock);

	TRACE_((THIS_FILE, "done, found %d address(es)", *p_cnt));
	if (!*p_cnt)
		PJ_LOG(4, ("ip_helper_generic.c", "if_enum_by_af() interface not found(3)."));
    return (*p_cnt != 0) ? PJ_SUCCESS : PJ_ENOTFOUND;
}

#elif defined(PJ_HAS_IFADDRS_H) && PJ_HAS_IFADDRS_H != 0 && \
defined(PJ_HAS_NET_IF_H) && PJ_HAS_NET_IF_H != 0
/* Using getifaddrs() is preferred since it can work with both IPv4 and IPv6 */
static pj_status_t if_enum_by_af(int af,
				 unsigned *p_cnt,
				 pj_sockaddr ifs[])
{
    struct ifaddrs *ifap = NULL, *it;
    unsigned max;

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);

    TRACE_((THIS_FILE, "Starting interface enum with getifaddrs() for af=%d",
	    af));

    if (getifaddrs(&ifap) != 0) {
	TRACE_((THIS_FILE, " getifarrds() failed: %s", get_os_errmsg()));
	return PJ_RETURN_OS_ERROR(pj_get_netos_error());
    }

    it = ifap;
    max = *p_cnt;
    *p_cnt = 0;
    for (; it!=NULL && *p_cnt < max; it = it->ifa_next) {
	struct sockaddr *ad = it->ifa_addr;

	TRACE_((THIS_FILE, " checking %s", it->ifa_name));

	if ((it->ifa_flags & IFF_UP)==0) {
	    TRACE_((THIS_FILE, "  interface is down"));
	    continue; /* Skip when interface is down */
	}

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
	if (it->ifa_flags & IFF_LOOPBACK) {
	    TRACE_((THIS_FILE, "  loopback interface"));
	    continue; /* Skip loopback interface */
	}
#endif

	if (ad==NULL) {
	    TRACE_((THIS_FILE, "  NULL address ignored"));
	    continue; /* reported to happen on Linux 2.6.25.9
			 with ppp interface */
	}

	if (ad->sa_family != af) {
	    TRACE_((THIS_FILE, "  address %s ignored (af=%d)",
		    get_addr(ad), ad->sa_family));
	    continue; /* Skip when interface is down */
	}

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if (af==pj_AF_INET() &&
	    (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
	{
	    TRACE_((THIS_FILE, "  address %s ignored (0.0.0.0/8 class)",
		    get_addr(ad), ad->sa_family));
	    continue;
	}

	TRACE_((THIS_FILE, "  address %s (af=%d) added at index %d",
		get_addr(ad), ad->sa_family, *p_cnt));

	pj_bzero(&ifs[*p_cnt], sizeof(ifs[0]));
	pj_memcpy(&ifs[*p_cnt], ad, pj_sockaddr_get_len(ad));
	PJ_SOCKADDR_RESET_LEN(&ifs[*p_cnt]);
	(*p_cnt)++;
    }

    freeifaddrs(ifap);
    TRACE_((THIS_FILE, "done, found %d address(es)", *p_cnt));
	if (!*p_cnt)
		PJ_LOG(4, ("ip_helper_generic.c", "if_enum_by_af() interface not found(4)."));
    return (*p_cnt != 0) ? PJ_SUCCESS : PJ_ENOTFOUND;
}


#elif defined(PJ_HAS_NET_IF_H) && PJ_HAS_NET_IF_H != 0
/* Note: this does not work with IPv6 */
static pj_status_t if_enum_by_af(int af, unsigned *p_cnt, pj_sockaddr ifs[])
{
    struct if_nameindex *if_list;
    struct ifreq ifreq;
    pj_sock_t sock;
    unsigned i, max_count;
    pj_status_t status;

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);

    TRACE_((THIS_FILE, "Starting if_nameindex() for af=%d", af));

    status = pj_sock_socket(af, PJ_SOCK_DGRAM, 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    if_list = if_nameindex();
	if (if_list == NULL) {
		PJ_LOG(4, ("ip_helper_generic.c", "if_enum_by_af() interface not found(1)."));
		return PJ_ENOTFOUND;
	}


    max_count = *p_cnt;
    *p_cnt = 0;
    for (i=0; if_list[i].if_index && *p_cnt<max_count; ++i) {
	struct sockaddr *ad;
	int rc;

	strncpy(ifreq.ifr_name, if_list[i].if_name, IFNAMSIZ);

	TRACE_((THIS_FILE, " checking interface %s", ifreq.ifr_name));

	if ((rc=ioctl(sock, SIOCGIFFLAGS, &ifreq)) != 0) {
	    TRACE_((THIS_FILE, "  ioctl(SIOCGIFFLAGS) failed: %s",
		    get_os_errmsg()));
	    continue;	/* Failed to get flags, continue */
	}

	if ((ifreq.ifr_flags & IFF_UP)==0) {
	    TRACE_((THIS_FILE, "  interface is down"));
	    continue; /* Skip when interface is down */
	}

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
	if (ifreq.ifr_flags & IFF_LOOPBACK) {
	    TRACE_((THIS_FILE, "  loopback interface"));
	    continue; /* Skip loopback interface */
	}
#endif

	/* Note: SIOCGIFADDR does not work for IPv6! */
	if ((rc=ioctl(sock, SIOCGIFADDR, &ifreq)) != 0) {
	    TRACE_((THIS_FILE, "  ioctl(SIOCGIFADDR) failed: %s",
		    get_os_errmsg()));
	    continue;	/* Failed to get address, continue */
	}

	ad = (struct sockaddr*) &ifreq.ifr_addr;

	if (ad->sa_family != af) {
	    TRACE_((THIS_FILE, "  address %s family %d ignored", 
			       get_addr(&ifreq.ifr_addr),
			       ifreq.ifr_addr.sa_family));
	    continue;	/* Not address family that we want, continue */
	}

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if (af==pj_AF_INET() &&
	    (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
	{
	    TRACE_((THIS_FILE, "  address %s ignored (0.0.0.0/8 class)", 
		    get_addr(ad), ad->sa_family));
	    continue;
	}

	/* Got an address ! */
	TRACE_((THIS_FILE, "  address %s (af=%d) added at index %d", 
		get_addr(ad), ad->sa_family, *p_cnt));

	pj_bzero(&ifs[*p_cnt], sizeof(ifs[0]));
	pj_memcpy(&ifs[*p_cnt], ad, pj_sockaddr_get_len(ad));
	PJ_SOCKADDR_RESET_LEN(&ifs[*p_cnt]);
	(*p_cnt)++;
    }

    if_freenameindex(if_list);
    pj_sock_close(sock);

	TRACE_((THIS_FILE, "done, found %d address(es)", *p_cnt));
	if (!*p_cnt)
		PJ_LOG(4, ("ip_helper_generic.c", "if_enum_by_af() interface not found(2)."));
    return (*p_cnt != 0) ? PJ_SUCCESS : PJ_ENOTFOUND;
}

#else
static pj_status_t if_enum_by_af(int af,
				 unsigned *p_cnt,
				 pj_sockaddr ifs[])
{
    pj_status_t status;

    PJ_ASSERT_RETURN(p_cnt && *p_cnt > 0 && ifs, PJ_EINVAL);

    pj_bzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Just get one default route */
    status = pj_getdefaultipinterface(af, &ifs[0]);
    if (status != PJ_SUCCESS)
	return status;

    *p_cnt = 1;
    return PJ_SUCCESS;
}
#endif /* SIOCGIFCONF */

/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(int af,
					 unsigned *p_cnt,
					 pj_sockaddr ifs[])
{
    unsigned start;
    pj_status_t status;

    start = 0;
    if (af==PJ_AF_INET6 || af==PJ_AF_UNSPEC) {
	unsigned max = *p_cnt;
	status = if_enum_by_af(PJ_AF_INET6, &max, &ifs[start]);
	if (status == PJ_SUCCESS) {
	    start += max;
	    (*p_cnt) -= max;
	}
    }

    if (af==PJ_AF_INET || af==PJ_AF_UNSPEC) {
	unsigned max = *p_cnt;
	status = if_enum_by_af(PJ_AF_INET, &max, &ifs[start]);
	if (status == PJ_SUCCESS) {
	    start += max;
	    (*p_cnt) -= max;
	}
    }

    *p_cnt = start;

	if (!*p_cnt)
		PJ_LOG(4, ("ip_helper_generic.c", "pj_enum_ip_interface() interface not found."));
    return (*p_cnt != 0) ? PJ_SUCCESS : PJ_ENOTFOUND;
}

/*
 * Enumerate the IP routing table for this host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *p_cnt,
				     pj_ip_route_entry routes[])
{
    pj_sockaddr itf;
    pj_status_t status;

    PJ_ASSERT_RETURN(p_cnt && *p_cnt > 0 && routes, PJ_EINVAL);

    pj_bzero(routes, sizeof(routes[0]) * (*p_cnt));

    /* Just get one default route */
    status = pj_getdefaultipinterface(PJ_AF_INET, &itf);
    if (status != PJ_SUCCESS)
	return status;
    
    routes[0].ipv4.if_addr.s_addr = itf.ipv4.sin_addr.s_addr;
    routes[0].ipv4.dst_addr.s_addr = 0;
    routes[0].ipv4.mask.s_addr = 0;
    *p_cnt = 1;

    return PJ_SUCCESS;
}

#if 0
    /* dummy */
//#else
#elif defined(SIOCGIFHWADDR) && \
    defined(PJ_HAS_NET_IF_H) && PJ_HAS_NET_IF_H != 0 // LINUX

PJ_DEF(pj_status_t) pj_all_physical_addresses(char *local_mac, pj_uint32_t *local_mac_len) {
	pj_status_t status = PJ_SUCCESS;
	char mac_addrs[4500];
	pj_uint32_t mac_addrs_len = 0;
    struct ifreq *ifr;
    struct ifconf ifc;
	pj_sock_t sock;
	int i;
	int numif;

	PJ_ASSERT_RETURN(local_mac && local_mac_len, PJ_EINVAL);

    PJ_LOG(4, (THIS_FILE, "pj_all_physical_addresses() starting interface enum with SIOCGIFCONF"));

	// initialize a socket
	status = pj_sock_socket(pj_AF_INET(), PJ_SOCK_DGRAM, 0, &sock);
	if (status != PJ_SUCCESS)
		return status;

	memset(&ifc, 0, sizeof(struct ifconf));
	// get interface configuration
	if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
		int oserr = pj_get_netos_error();
		PJ_LOG(1, (THIS_FILE, " pj_all_physical_addresses() ioctl(SIOCGIFCONF) failed: %s", get_os_errmsg()));
		pj_sock_close(sock);
		return PJ_RETURN_OS_ERROR(oserr);
	}

	// prepare sufficient buffer
	PJ_LOG(4, (THIS_FILE, "pj_all_physical_addresses() ifc_len=%d", ifc.ifc_len));

	if ((ifr = (struct ifreq*) malloc(ifc.ifc_len)) == NULL)
	{
		int oserr = pj_get_netos_error();
		PJ_LOG(1, (THIS_FILE, " pj_all_physical_addresses() malloc failed: %s", get_os_errmsg()));
		pj_sock_close(sock);
		return PJ_RETURN_OS_ERROR(oserr);
	}
	ifc.ifc_ifcu.ifcu_req = ifr;

	// get interface configuration
	if (ioctl(sock, SIOCGIFCONF, &ifc) < 0)
	{
		int oserr = pj_get_netos_error();
		PJ_LOG(1, (THIS_FILE, " pj_all_physical_addresses() ioctl(SIOCGIFCONF) failed: %s", get_os_errmsg()));
		pj_sock_close(sock);
		free(ifr);
		return PJ_RETURN_OS_ERROR(oserr);
    }

	// go through all interfaces
	memset(mac_addrs, 0, sizeof(mac_addrs));
	numif = ifc.ifc_len / sizeof(struct ifreq);
	PJ_LOG(4, (THIS_FILE, "pj_all_physical_addresses() ifc_len=%d, numif=%d", ifc.ifc_len, numif));
	for (i = 0; i < numif; i++)
	{
		struct ifreq *r = &ifr[i];
		struct ifreq iff = *r;
		char tmp_mac[18];
        	PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() check interface: %s",
				r->ifr_name));
		if (!strcmp(r->ifr_name, "lo"))
			continue; // skip loopback interface

        if (ioctl(sock, SIOCGIFFLAGS, &iff) != 0) {
        	PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() ioctl(SIOCGIFFLAGS) failed: %s",
				get_os_errmsg()));
			continue;	/* Failed to get flags, continue */
		}

		if ((iff.ifr_flags & IFF_UP)==0) {
			PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() interface is down"));
		    continue; /* Skip when interface is down */
		}

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
		if (iff.ifr_flags & IFF_LOOPBACK) {
			PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() loopback interface"));
			continue; /* Skip loopback interface */
		}
#endif

#if PJ_IP_HELPER_IGNORE_POINT_TO_POINT_IF
		if (iff.ifr_flags & IFF_POINTOPOINT) {
			PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() point-to-point interface"));
			continue; /* Skip point-to-poi interface */
		}
#endif

		// get MAC address
		if(ioctl(sock, SIOCGIFHWADDR, r) < 0)
		{
			int oserr = pj_get_netos_error();
			PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() ioctl(SIOCGIFHWADDR) failed: %s", get_os_errmsg()));
			pj_sock_close(sock);
			free(ifr);
			continue;
		}

		// construct string format target hardware address
		sprintf(tmp_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
				r->ifr_hwaddr.sa_data[0]&0xff, r->ifr_hwaddr.sa_data[1]&0xff, r->ifr_hwaddr.sa_data[2]&0xff,
				r->ifr_hwaddr.sa_data[3]&0xff, r->ifr_hwaddr.sa_data[4]&0xff, r->ifr_hwaddr.sa_data[5]&0xff);

		if(strstr(mac_addrs, tmp_mac))
		{
			PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() tmp_mac[%s] already exists in mak_addrs[%s]", tmp_mac, mac_addrs));
			continue; /* Skip point-to-poi interface */
		}


		PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() tmp_mac %s", tmp_mac));
		if (mac_addrs_len == 0)
		{
			sprintf(mac_addrs, "%s", tmp_mac);
			mac_addrs_len += 17;
		}
		else
		{
			sprintf(mac_addrs, "%s,%s", mac_addrs, tmp_mac);
			mac_addrs_len += 18;
		}
	}
	pj_sock_close(sock);
	free(ifr);

	// check if there is enuough memory
	if (mac_addrs_len > *local_mac_len)
	{
		PJ_LOG(1, (THIS_FILE, " pj_all_physical_addresses() buffer size[%d] too small. needed[%d]",
				*local_mac_len, mac_addrs_len));
		return PJ_ETOOSMALL;
	}

	memset(local_mac, 0, sizeof(local_mac));
	memcpy(local_mac, mac_addrs, mac_addrs_len);
	*local_mac_len = mac_addrs_len;
	return status;
}

PJ_DEF(pj_status_t) pj_get_physical_address_by_ip(int af,
						   pj_sockaddr addr,
						   unsigned char *physical_addr,
						   pj_uint32_t *physical_addr_len,
						   char *ifname,
						   pj_uint32_t *ifname_len)
{
	pj_status_t status = PJ_SUCCESS;
    struct ifreq *ifr;  
    struct ifconf ifc;
	pj_sock_t sock;
	int i;  
	int numif;

	PJ_ASSERT_RETURN(physical_addr && physical_addr_len, PJ_EINVAL);
	PJ_ASSERT_RETURN(*physical_addr_len >= 6, PJ_ETOOSMALL);
	
	// initialize a socket
	status = pj_sock_socket(af, PJ_SOCK_DGRAM, 0, &sock);
	if (status != PJ_SUCCESS)
		return status;

	memset(&ifc, 0, sizeof(struct ifconf));
	// get interface configuration
	if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
		int oserr = pj_get_netos_error();
		PJ_LOG(1, (THIS_FILE, " pj_get_physical_address_by_ip() ioctl(SIOCGIFCONF) failed: %s", get_os_errmsg()));
		pj_sock_close(sock);
		return PJ_RETURN_OS_ERROR(oserr);
	}
			   
	// prepare sufficient buffer
	PJ_LOG(4, (THIS_FILE, "pj_get_physical_address_by_ip() ifc_len=%d", ifc.ifc_len));

	if ((ifr = (struct ifreq*) malloc(ifc.ifc_len)) == NULL)
	{  
		int oserr = pj_get_netos_error();
		PJ_LOG(1, (THIS_FILE, " pj_get_physical_address_by_ip() malloc failed: %s", get_os_errmsg()));
		pj_sock_close(sock);
		return PJ_RETURN_OS_ERROR(oserr);
	}
	ifc.ifc_ifcu.ifcu_req = ifr;

	// get interface configuration
	if (ioctl(sock, SIOCGIFCONF, &ifc) < 0)  
	{ 
		int oserr = pj_get_netos_error();                                                                                             
		PJ_LOG(1, (THIS_FILE, " pj_get_physical_address_by_ip() ioctl(SIOCGIFCONF) failed: %s", get_os_errmsg()));
		pj_sock_close(sock);
		free(ifr);
		return PJ_RETURN_OS_ERROR(oserr);
    }  

	// go through all interfaces
	numif = ifc.ifc_len / sizeof(struct ifreq);
	PJ_LOG(4, (THIS_FILE, "pj_get_physical_address_by_ip() ifc_len=%d, numif=%d", ifc.ifc_len, numif));
	for (i = 0; i < numif; i++)  
	{  
		struct ifreq *r = &ifr[i];
		struct ifreq iff = *r;
		struct sockaddr_in *sin = (struct sockaddr_in*)&r->ifr_addr;  
		struct sockaddr *ad = &r->ifr_addr;
    	PJ_LOG(4, (THIS_FILE, "pj_get_physical_address_by_ip() ifr_addr=%s, addr=%s", get_addr(ad), get_addr(&addr)));
		if (!strcmp(r->ifr_name, "lo"))  
			continue; // skip loopback interface
        
		/* Skip address with different family */
		if (ad->sa_family != af) {
	    	PJ_LOG(4, (THIS_FILE, "pj_get_physical_address_by_ip() address %s (af=%d) ignored",
	    			get_addr(ad), (int)ad->sa_family));
			continue;
		}
        
        if (ioctl(sock, SIOCGIFFLAGS, &iff) != 0) {
        	PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() ioctl(SIOCGIFFLAGS) failed: %s",
				get_os_errmsg()));
			continue;	/* Failed to get flags, continue */
		}
    
		if ((iff.ifr_flags & IFF_UP)==0) {
			PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() interface is down"));
		    continue; /* Skip when interface is down */
		}

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
		if (iff.ifr_flags & IFF_LOOPBACK) {
			PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() loopback interface"));
			continue; /* Skip loopback interface */
		}
#endif

#if PJ_IP_HELPER_IGNORE_POINT_TO_POINT_IF
		if (iff.ifr_flags & IFF_POINTOPOINT) {
			PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() point-to-point interface"));
			continue; /* Skip point-to-poi interface */
		}
#endif

		/* Ignore 0.0.0.0/8 address. This is a special address
		 * which doesn't seem to have practical use.
		 */
		if (af==pj_AF_INET() &&
			(pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
		{
			PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() address %s ignored (0.0.0.0/8 class)",
				get_addr(ad), ad->sa_family));       
			continue;
		}
		
		PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() ad->family=%d, family=%d", ad->sa_family, addr.addr.sa_family));
		pj_sockaddr_set_port((pj_sockaddr *)ad, 0);
		pj_sockaddr_set_port(&addr, 0);
		if (pj_sockaddr_cmp(ad, &addr) == 0)
		{
			// get MAC address  
			if(ioctl(sock, SIOCGIFHWADDR, r) < 0)  
			{  
				int oserr = pj_get_netos_error();                                                                                             
				PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() ioctl(SIOCGIFHWADDR) failed: %s", get_os_errmsg()));
				pj_sock_close(sock);
				free(ifr);
				continue;
			}
			memcpy(physical_addr, r->ifr_hwaddr.sa_data, 6);
			*physical_addr_len = 6;
            PJ_LOG(4, (THIS_FILE, " get mac by ioctl(SIOCGIFHWADDR)"));
			PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() iff.ifrn_name=%s", iff.ifr_name));
            
			// DEAN 2013-11-19
			if (ifname && ifname_len)
			{
				memset(ifname, 0, *ifname_len);
				*ifname_len = strlen(iff.ifr_name);
				memcpy(ifname, iff.ifr_name, *ifname_len);
			}
            
			pj_sock_close(sock);
			//ee(ifr);

			return status;
		}
	}
	pj_sock_close(sock);
	free(ifr);
	PJ_LOG(4, ("ip_helper_generic.c", "pj_get_physical_address_by_ip() interface not found."));
	return PJ_ENOTFOUND;  
}


#elif defined(PJ_HAS_IFADDRS_H) && PJ_HAS_IFADDRS_H != 0 && \
    defined(PJ_HAS_NET_IF_H) && PJ_HAS_NET_IF_H != 0   //BSD

PJ_DEF(pj_status_t) pj_all_physical_addresses(char *local_mac, pj_uint32_t *local_mac_len) {
	pj_status_t status = PJ_SUCCESS;
	char mac_addrs[4500];
	pj_uint32_t mac_addrs_len = 0;

    struct ifaddrs *ifap = NULL, *it;

    PJ_ASSERT_RETURN(local_mac && local_mac_len, PJ_EINVAL);

    	PJ_LOG(4, (THIS_FILE, "pj_all_physical_addresses() starting interface enum with getifaddrs()"));

    if (getifaddrs(&ifap) != 0) {
        PJ_LOG(1, (THIS_FILE, " pj_all_physical_addresses() getifarrds() failed: %s", get_os_errmsg()));
        return PJ_RETURN_OS_ERROR(pj_get_netos_error());
    }

    it = ifap;
    for (; it!=NULL; it = it->ifa_next) {
		char tmp_mac[18];
        unsigned char physical_addr[6];
        struct sockaddr *ad = it->ifa_addr;
        
        PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() checking %s", it->ifa_name));
        
        struct sockaddr_dl* sdl = (struct sockaddr_dl *)ad;
        
        PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() checking %s", it->ifa_name));

        if ((it->ifa_flags & IFF_UP)==0) {
            PJ_LOG(3, (THIS_FILE, "  pj_all_physical_addresses() interface is down"));
            continue; /* Skip when interface is down */
        }

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
        if (it->ifa_flags & IFF_LOOPBACK) {
            PJ_LOG(3, (THIS_FILE, "  pj_all_physical_addresses() loopback interface"));
            continue; /* Skip loopback interface */
        }
#endif

#if PJ_IP_HELPER_IGNORE_POINT_TO_POINT_IF
		if (it->ifa_flags & IFF_POINTOPOINT) {
			PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() point-to-point interface"));
			continue; /* Skip point-to-poi interface */
		}
#endif
        
        if (6 == sdl->sdl_alen) {
            memcpy(physical_addr, LLADDR(sdl), sdl->sdl_alen);
            
            // construct string format target hardware address
            sprintf(tmp_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                    physical_addr[0]&0xff, physical_addr[1]&0xff, physical_addr[2]&0xff,
                    physical_addr[3]&0xff, physical_addr[4]&0xff, physical_addr[5]&0xff);
            
            if(strstr(mac_addrs, tmp_mac))
            {
                PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() tmp_mac[%s] already exists in mak_addrs[%s]", tmp_mac, mac_addrs));
                continue; /* Skip point-to-poi interface */
            }
            
            
            PJ_LOG(4, (THIS_FILE, " pj_all_physical_addresses() tmp_mac %s", tmp_mac));
            if (mac_addrs_len == 0)
            {
                sprintf(mac_addrs, "%s", tmp_mac);
                mac_addrs_len += 17;
            }
            else
            {
                sprintf(mac_addrs, "%s,%s", mac_addrs, tmp_mac);
                mac_addrs_len += 18;
            }
        }
	}

	// check if there is enuough memory
	if (mac_addrs_len > *local_mac_len)
	{
		PJ_LOG(1, (THIS_FILE, " pj_all_physical_addresses() buffer size[%d] too small. needed[%d]",
				*local_mac_len, mac_addrs_len));
		return PJ_ETOOSMALL;
	}

	memset(local_mac, 0, sizeof(local_mac));
	memcpy(local_mac, mac_addrs, mac_addrs_len);
	*local_mac_len = mac_addrs_len;
	return status;
}

PJ_DEF(pj_status_t) pj_get_if_name_by_ip(int af,
                                                  pj_sockaddr addr,
                                                  char *ifname,
                                                  pj_uint32_t *ifname_len)
{
    struct ifaddrs *ifap = NULL, *it;
    
    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6 ||
                     ifname || ifname_len || *ifname_len > 0, PJ_EINVAL);
    
    if (getifaddrs(&ifap) != 0) {
        PJ_LOG(1, (THIS_FILE, " pj_get_if_name_by_ip() getifarrds() failed: %s", get_os_errmsg()));
        return PJ_RETURN_OS_ERROR(pj_get_netos_error());
    }
    
    it = ifap;
    for (; it!=NULL; it = it->ifa_next) {
        struct sockaddr *ad = it->ifa_addr;
        
        PJ_LOG(4, (THIS_FILE, " pj_get_if_name_by_ip() checking %s", it->ifa_name));
        
        if ((it->ifa_flags & IFF_UP)==0) {
            PJ_LOG(3, (THIS_FILE, "  pj_get_if_name_by_ip() interface is down"));
            continue; /* Skip when interface is down */
        }
        
#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
        if (it->ifa_flags & IFF_LOOPBACK) {
            PJ_LOG(3, (THIS_FILE, "  pj_get_if_name_by_ip() loopback interface"));
            continue; /* Skip loopback interface */
        }
#endif

#if PJ_IP_HELPER_IGNORE_POINT_TO_POINT_IF
		if (it->ifa_flags & IFF_POINTOPOINT) {
			PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() point-to-point interface"));
			continue; /* Skip point-to-poi interface */
		}
#endif
        
        if (ad==NULL) {
            PJ_LOG(3, (THIS_FILE, "  pj_get_if_name_by_ip() NULL address ignored"));
            continue; /* reported to happen on Linux 2.6.25.9
                       with ppp interface */
        }
        
        if (ad->sa_family != af) {
            PJ_LOG(3, (THIS_FILE, "  pj_get_if_name_by_ip() address %s ignored (af=%d)",
                       get_addr(ad), ad->sa_family));
            continue; /* Skip when interface is down */
        }
        
        /* Ignore 0.0.0.0/8 address. This is a special address
         * which doesn't seem to have practical use.
         */
        if (af==pj_AF_INET() &&
            (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
        {
            PJ_LOG(3, (THIS_FILE, "  pj_get_if_name_by_ip() address %s ignored (0.0.0.0/8 class)",
                       get_addr(ad), ad->sa_family));
            continue;
        }
        
        PJ_LOG(4, (THIS_FILE, "  pj_get_if_name_by_ip() address %s (af=%d)",
                   get_addr(ad), ad->sa_family));
        
		pj_sockaddr_set_port((pj_sockaddr *)ad, 0);
		pj_sockaddr_set_port(&addr, 0);
		if (pj_sockaddr_cmp(ad, &addr) == 0)
		{
            PJ_LOG(4, (THIS_FILE, "  pj_get_if_name_by_ip() it->ifa_name=%s",
                       it->ifa_name));
            if (it->ifa_name && *ifname_len < strlen(it->ifa_name))
                return PJ_ETOOSMALL;
            
			// DEAN 2013-11-19
			if (it->ifa_name)
			{
				memset(ifname, 0, *ifname_len);
				*ifname_len = strlen(it->ifa_name);
				memcpy(ifname, it->ifa_name, *ifname_len);
			}
            
			return PJ_SUCCESS;
        }
    }
    
	freeifaddrs(ifap);
	PJ_LOG(4, ("ip_helper_generic.c", "pj_get_physical_address_by_ip() interface not found."));
    return PJ_ENOTFOUND;
}

PJ_DEF(pj_status_t) pj_get_physical_address_by_ip(int af,
                                                  pj_sockaddr addr,
                                                  unsigned char *physical_addr,
                                                  pj_uint32_t *physical_addr_len,
                                                  char *ifname,
                                                  pj_uint32_t *ifname_len)
{
    pj_status_t status;
    struct ifaddrs *ifap = NULL, *it;
    char tmp_ifname[IFNAMSIZ];
    pj_uint32_t tmp_ifname_len = sizeof(tmp_ifname);
    
    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);
    
    if (getifaddrs(&ifap) != 0) {
        PJ_LOG(1, (THIS_FILE, " pj_get_physical_address_by_ip() getifarrds() failed: %s", get_os_errmsg()));
        return PJ_RETURN_OS_ERROR(pj_get_netos_error());
    }
    
    status = pj_get_if_name_by_ip(af, addr, tmp_ifname, &tmp_ifname_len);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, " pj_get_physical_address_by_ip() pj_get_if_name_by_ip() failed: %d", status));
        return status;
    }
    
    PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() pj_get_if_name_by_ip() ifname=%s", tmp_ifname));
    
    it = ifap;
    for (; it!=NULL; it = it->ifa_next) {
        struct sockaddr *ad = it->ifa_addr;
        
        PJ_LOG(4, (THIS_FILE, " pj_get_physical_address_by_ip() checking %s", it->ifa_name));
        
        struct sockaddr_dl* sdl = (struct sockaddr_dl *)ad;
            
        // MAC address
        if (ad->sa_family != AF_LINK)
        {
            PJ_LOG(1, (THIS_FILE, "  pj_get_physical_address_by_ip() ad->sa_family(%d) != AF_LINK",
                        ad->sa_family));
            continue;
        }
            
        // MAC address
        if (strcmp(it->ifa_name, tmp_ifname) != 0)
        {
               PJ_LOG(1, (THIS_FILE, "  pj_get_physical_address_by_ip() ifname doesn't match. it->ifa_name=%s, tmp_ifname=%s",
                        it->ifa_name, tmp_ifname));
            continue;
        }
                
        if (6 == sdl->sdl_alen) {
            memcpy(physical_addr, LLADDR(sdl), sdl->sdl_alen);
            *physical_addr_len = sdl->sdl_alen;
        }
            
        // DEAN 2013-11-19
        if (ifname && ifname_len && it->ifa_name)
        {
            memset(ifname, 0, *ifname_len);
            *ifname_len = strlen(it->ifa_name);
            memcpy(ifname, it->ifa_name, *ifname_len);
        }
            
        return PJ_SUCCESS;
    }
    
	freeifaddrs(ifap);
	PJ_LOG(4, ("ip_helper_generic.c", "pj_get_physical_address_by_ip() interface not found."));
    return PJ_ENOTFOUND;

}
#endif

PJ_DEF(pj_status_t) pj_physical_address(char *local_addr, char *local_mac, pj_uint32_t *local_mac_len) {
	pj_sockaddr      s_addr;
	unsigned char    mac[6];
	unsigned long    mac_len = sizeof(mac);
	pj_status_t      status = PJ_SUCCESS;

	PJ_ASSERT_RETURN(local_mac && local_mac_len, PJ_EINVAL);
	PJ_ASSERT_RETURN(*local_mac_len >= 17, PJ_ETOOSMALL);

	if (!local_addr)
	{
		// get local default ip address
		status = pj_getdefaultipinterface(pj_AF_INET(), &s_addr);
		if (status != PJ_SUCCESS)
			return status;
	} 
	else
	{
		pj_str_t addr = pj_str(local_addr);
		// convert address string to pj_sock_addr
		status = pj_sockaddr_in_set_str_addr(&s_addr, &addr);
		if (status != PJ_SUCCESS)
			return status;
	}

	status = pj_get_physical_address_by_ip(pj_AF_INET(), s_addr, mac, &mac_len,
		NULL, NULL);
	if (status != PJ_SUCCESS)
	{
		PJ_LOG(4, (THIS_FILE, "pj_default_physical_address() failed status=%d\n", status));
		return status;
	}

	// construct string format target hardware address
	sprintf(local_mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
		mac[0]&0xff, mac[1]&0xff, mac[2]&0xff,
		mac[3]&0xff, mac[4]&0xff, mac[5]&0xff);

	*local_mac_len = 17;
	return status;
}

#if 0
#define MAC_BCAST_ADDR  "\xff\xff\xff\xff\xff\xff"

struct arpMsg {
    struct ethhdr ethh;			                   /* Ethernet header */
    unsigned short htype;                          /* hardware type (must be ARPHRD_ETHER) */
    unsigned short ptype;                          /* protocol type (must be ETH_P_IP) */
    unsigned char  hlen;                           /* hardware address length (must be 6) */
    unsigned char  plen;                           /* protocol address length (must be 4) */
    unsigned short operation;                      /* ARP opcode */
    unsigned char  sHaddr[6];                      /* sender's hardware address */
    unsigned char  sInaddr[4];                     /* sender's IP address */
    unsigned char  tHaddr[6];                      /* target's hardware address */
    unsigned char  tInaddr[4];                     /* target's IP address */
    unsigned char  pad[18];                        /* pad for min. Ethernet payload (60 bytes) */
};

PJ_DEF(pj_status_t) pj_resolve_mac_by_arp(char *target_addr, char *target_mac, 
										  pj_uint32_t *target_mac_len, int timeout) {
	pj_sock_t	     s;		      /* socket */
	struct sockaddr  addr;	       /* for interface name */
	int              addr_len;
	char             source_addr[PJ_INET6_ADDRSTRLEN+10];
	pj_sockaddr      s_addr;
	char             source_mac[16];	   /* for source mac */
	unsigned char    s_mac[8];	   /* for source mac in byte*/
	pj_uint32_t      s_mac_len;    /* for source mac byte len*/
	char             s_ifname[IFNAMSIZ];  /* interface name */
	pj_uint32_t      s_ifname_len; /* for interface name len*/
	struct arpMsg    arp;
	pj_ssize_t       arp_len;
	pj_fd_set_t		 fdset;
	pj_time_val      tm;
	time_t           prevTime;
	pj_status_t      status = PJ_SUCCESS;
	int              i;

	pj_in_addr dst_addr, src_addr;
	int optval = 1;

	PJ_ASSERT_RETURN(target_addr && target_mac && target_mac_len, PJ_EINVAL);
	PJ_ASSERT_RETURN(*target_mac_len >= 17, PJ_ETOOSMALL);

	// get local default ip address
	status = pj_getdefaultipinterface(pj_AF_INET(), &s_addr);
	if (status != PJ_SUCCESS)
		return status;

	// get local default hardware address by default ip address
	s_mac_len = sizeof(s_mac) / sizeof(s_mac[0]);
	s_ifname_len = sizeof(s_ifname);
	memset(s_ifname, 0, s_ifname_len);
	status = pj_get_physical_address_by_ip(pj_AF_INET(), s_addr, s_mac, &s_mac_len,
											s_ifname, &s_ifname_len);
	if (status != PJ_SUCCESS)
	{
		PJ_LOG(1, (THIS_FILE, " pj_get_physical_address() failed status=%d", status));
		return status;
	}

	// construct string format default hardware address for debugging log
	memset(source_mac, 0, sizeof(source_mac));
	sprintf(source_mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
	  s_mac[0]&0xff, s_mac[1]&0xff, s_mac[2]&0xff,
	  s_mac[3]&0xff, s_mac[4]&0xff, s_mac[5]&0xff);

	printf("resolve_mac_by_arp() souce_addr=%s, source_mac=%s\n", 
	  pj_sockaddr_print(&s_addr, source_addr, sizeof(source_addr), 0), source_mac);

	// convert string format target address to byte format
	dst_addr = pj_inet_addr2(target_addr);
	
	// prepare ARP message
	memset(&arp, 0, sizeof(arp));
	memcpy(arp.ethh.h_dest, MAC_BCAST_ADDR, 6); /* MAC DA */
	memcpy(arp.ethh.h_source, s_mac, 6); /* MAC SA */
	arp.ethh.h_proto = pj_htons(ETH_P_ARP); /* protocol type (Ethernet) */
	arp.htype = pj_htons(ARPHRD_ETHER);	       /* hardware type */
	arp.ptype = pj_htons(ETH_P_IP);		   /* protocol type (ARP message) */
	arp.hlen = 6;						  /* hardware address length */
	arp.plen = 4;						  /* protocol address length */
	arp.operation = pj_htons(ARPOP_REQUEST);		 /* ARP op code */
	*((pj_in_addr *)arp.sInaddr) = s_addr.ipv4.sin_addr;	     /* source IP address */
	memcpy(arp.sHaddr, s_mac, 6);		    /* source hardware address */
	memcpy(arp.tHaddr, MAC_BCAST_ADDR, 6); /* target hardware address */
	*((unsigned int *)arp.tInaddr) = dst_addr.s_addr;	     /* target IP address */

	// initialize socket
	status = pj_sock_socket (pj_AF_INET(), SOCK_PACKET, pj_htons(ETH_P_ARP), &s);
	if(status != PJ_SUCCESS) {
		PJ_LOG(1, (THIS_FILE, " pj_get_physical_address() Could not open raw socket. status=%d", status));
	    return status;
	}

	status = pj_sock_setsockopt(s, pj_SOL_SOCKET(), pj_SO_BROADCAST(), &optval, sizeof(optval));
	if(status != PJ_SUCCESS) {
		PJ_LOG(1, (THIS_FILE, " pj_get_physical_address() Could not setsocketopt on raw socket"));
	    return status;
	}

	// prepare packet sending target
	memset(&addr, 0, sizeof(addr));
	strncpy(addr.sa_data, s_ifname, s_ifname_len);

	// send ARP packet
	arp_len = sizeof(arp);
	addr_len = sizeof(addr);
	status = pj_sock_sendto(s, &arp, &arp_len, 0, &addr, addr_len);
	if (status != PJ_SUCCESS)
	{ 
		PJ_LOG(1, (THIS_FILE, " pj_get_physical_address() pj_sock_sendto() failed status=%d", status));
		return status;
	}
	PJ_LOG(4, (THIS_FILE, " pj_get_physical_address() ARP send ok. wait for response."));

	/* wait arp reply, and check it */
	tm.msec = 0;
	tm.sec = timeout;
	while ( timeout > 0 ) {
		PJ_FD_ZERO(&fdset);
		PJ_FD_SET(s, &fdset);
		tm.sec = timeout;
		status = pj_sock_select(s+1, &fdset, (pj_fd_set_t *)NULL, (pj_fd_set_t *)NULL, &tm);
		if (status < 0) {
			PJ_LOG(1, (THIS_FILE, " pj_get_physical_address() Error on ARPING request: %d", status));
			return status;
		} else if ( PJ_FD_ISSET(s, &fdset) ) {
			arp_len = sizeof(arp);
			status = pj_sock_recv(s, &arp, &arp_len, 0);
			if (status != PJ_SUCCESS)
			{
				PJ_LOG(1, (THIS_FILE, " pj_get_physical_address() pj_sock_recv() failed status=%d", status));
				return status;
			}

			PJ_LOG(4, (THIS_FILE, " pj_get_physical_address() pj_sock_recv() %d bytes, sInaddr=%d, sin_addr=%d, operation=%d, ARPOP_REPLY=%d",
					arp_len, arp.sInaddr, s_addr.ipv4.sin_addr, arp.operation, ARPOP_REPLY));
			if (arp.operation == pj_htons(ARPOP_REPLY) &&               // confirm this is ARP reply packet
				memcmp(arp.tHaddr, s_mac, 6) == 0 &&                    // compare received target hardware address and local hardware address
			   ((pj_in_addr *)arp.sInaddr)->s_addr == dst_addr.s_addr)  // compare received source ip address and target ip address
			{
				sprintf(target_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
					arp.sHaddr[0]&0xff, arp.sHaddr[1]&0xff, arp.sHaddr[2]&0xff,
					arp.sHaddr[3]&0xff, arp.sHaddr[4]&0xff, arp.sHaddr[5]&0xff);

				break;
			}
		}
		timeout -= time(NULL) - prevTime;
		time(&prevTime);
		if (timeout <= 0)
		{
			PJ_LOG(1, (THIS_FILE, " arp timeout"));
			status = PJ_ETIMEDOUT;
			break;
		}
	}

	pj_sock_close(s);

	return status;
}
#endif
