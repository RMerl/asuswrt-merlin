/* $Id: addr_resolv_symbian.cpp 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ip_helper.h>
#include <pj/log.h>
#include <pj/sock.h>
#include <pj/string.h>
#include <pj/unicode.h>

#include "os_symbian.h"
 
#define THIS_FILE 	"addr_resolv_symbian.cpp"
#define TRACE_ME	0


// PJLIB API: resolve hostname
PJ_DEF(pj_status_t) pj_gethostbyname(const pj_str_t *name, pj_hostent *he)
{
    static pj_addrinfo ai;
    static char *aliases[2];
    static char *addrlist[2];
    unsigned count = 1;
    pj_status_t status;
    
    status = pj_getaddrinfo(PJ_AF_INET, name, &count, &ai);
    if (status != PJ_SUCCESS)
    	return status;
    
    aliases[0] = ai.ai_canonname;
    aliases[1] = NULL;
    
    addrlist[0] = (char*) &ai.ai_addr.ipv4.sin_addr;
    addrlist[1] = NULL;
    
    pj_bzero(he, sizeof(*he));
    he->h_name = aliases[0];
    he->h_aliases = aliases;
    he->h_addrtype = PJ_AF_INET;
    he->h_length = 4;
    he->h_addr_list = addrlist;
    
    return PJ_SUCCESS;
}


// Resolve for specific address family
static pj_status_t getaddrinfo_by_af(int af, const pj_str_t *name,
				     unsigned *count, pj_addrinfo ai[]) 
{
    unsigned i;
    pj_status_t status;
    
    PJ_ASSERT_RETURN(name && count && ai, PJ_EINVAL);

#if !defined(PJ_HAS_IPV6) || !PJ_HAS_IPV6
    if (af == PJ_AF_INET6)
    	return PJ_EIPV6NOTSUP;
#endif
	
    // Return failure if access point is marked as down by app.
    PJ_SYMBIAN_CHECK_CONNECTION();

    // Get resolver for the specified address family
    RHostResolver &resv = PjSymbianOS::Instance()->GetResolver(af);

    // Convert name to Unicode
    wchar_t name16[PJ_MAX_HOSTNAME];
    pj_ansi_to_unicode(name->ptr, name->slen, name16, PJ_ARRAY_SIZE(name16));
    TPtrC16 data((const TUint16*)name16);

    // Resolve!
    TNameEntry nameEntry;
    TRequestStatus reqStatus;
    
    resv.GetByName(data, nameEntry, reqStatus);
    User::WaitForRequest(reqStatus);
    
    // Iterate each result
    i = 0;
    while (reqStatus == KErrNone && i < *count) {
    	
		// Get the resolved TInetAddr
		TInetAddr inetAddr(nameEntry().iAddr);
		int addrlen;

#if TRACE_ME
		if (1) {
			pj_sockaddr a;
			char ipaddr[PJ_INET6_ADDRSTRLEN+2];
			int namelen;
			
			namelen = sizeof(pj_sockaddr);
			if (PjSymbianOS::Addr2pj(inetAddr, a, &namelen, 
									 PJ_FALSE) == PJ_SUCCESS) 
			{
				PJ_LOG(5,(THIS_FILE, "resolve %.*s: %s", 
						(int)name->slen, name->ptr,
						pj_sockaddr_print(&a, ipaddr, sizeof(ipaddr), 2)));
			}
		}
#endif
		
		// Ignore if this is not the same address family
		// Not a good idea, as Symbian mapps IPv4 to IPv6.
		//fam = inetAddr.Family();
		//if (fam != af) {
		//    resv.Next(nameEntry, reqStatus);
		//    User::WaitForRequest(reqStatus);
		//    continue;
		//}
		
		// Convert IP address first to get IPv4 mapped address
		addrlen = sizeof(ai[i].ai_addr);
		status = PjSymbianOS::Addr2pj(inetAddr, ai[i].ai_addr, 
									  &addrlen, PJ_TRUE);
		if (status != PJ_SUCCESS)
		    return status;
		
		// Ignore if address family doesn't match
		if (ai[i].ai_addr.addr.sa_family != af) {
		    resv.Next(nameEntry, reqStatus);
		    User::WaitForRequest(reqStatus);
		    continue;
		}

		// Convert the official address to ANSI.
		pj_unicode_to_ansi((const wchar_t*)nameEntry().iName.Ptr(), 
				   nameEntry().iName.Length(),
			       	   ai[i].ai_canonname, sizeof(ai[i].ai_canonname));
	
		// Next
		++i;
		resv.Next(nameEntry, reqStatus);
		User::WaitForRequest(reqStatus);
    }

    *count = i;
    return PJ_SUCCESS;
}

/* Resolve IPv4/IPv6 address */
PJ_DEF(pj_status_t) pj_getaddrinfo(int af, const pj_str_t *nodename,
				   unsigned *count, pj_addrinfo ai[]) 
{
    unsigned start;
    pj_status_t status = PJ_EAFNOTSUP;
    
    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6 || af==PJ_AF_UNSPEC,
    		     PJ_EAFNOTSUP);
    PJ_ASSERT_RETURN(nodename && count && *count && ai, PJ_EINVAL);
    
    start = 0;
    
    if (af==PJ_AF_INET6 || af==PJ_AF_UNSPEC) {
        unsigned max = *count;
    	status = getaddrinfo_by_af(PJ_AF_INET6, nodename, 
    				   &max, &ai[start]);
    	if (status == PJ_SUCCESS) {
    	    (*count) -= max;
    	    start += max;
    	}
    }
    
    if (af==PJ_AF_INET || af==PJ_AF_UNSPEC) {
        unsigned max = *count;
    	status = getaddrinfo_by_af(PJ_AF_INET, nodename, 
    				   &max, &ai[start]);
    	if (status == PJ_SUCCESS) {
    	    (*count) -= max;
    	    start += max;
    	}
    }
    
    *count = start;
    
    if (*count) {
    	return PJ_SUCCESS;
    } else {
    	return status!=PJ_SUCCESS ? status : PJ_ENOTFOUND;
    }
}

