/* $Id: ip_helper_symbian.cpp 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/log.h>
#include <pj/string.h>
#include <pj/compat/socket.h>


#include "os_symbian.h"

#define THIS_FILE	"ip_helper_symbian.cpp"
#define TRACE_ME	0

static pj_status_t rsock_enum_interface(int af,
					unsigned *p_cnt,
					pj_sockaddr ifs[]) 
{
    TInt rc;
    RSocket rSock;
    TPckgBuf<TSoInetInterfaceInfo> info;
    unsigned i;
    
    if (PjSymbianOS::Instance()->Connection()) {
    	
    	rc = rSock.Open(PjSymbianOS::Instance()->SocketServ(), 
    			af, PJ_SOCK_DGRAM, KProtocolInetUdp,
    			*PjSymbianOS::Instance()->Connection());
    } else {
    	
    	rc = rSock.Open(PjSymbianOS::Instance()->SocketServ(), 
    			af, PJ_SOCK_DGRAM, KProtocolInetUdp);
    			
    }
        
    if (rc != KErrNone)
	return PJ_RETURN_OS_ERROR(rc);
    
    rSock.SetOpt(KSoInetEnumInterfaces, KSolInetIfCtrl);
    
    for (i=0; i<*p_cnt &&
    		rSock.GetOpt(KSoInetNextInterface, KSolInetIfCtrl, 
    		             info) == KErrNone; ) 
    {
    	TInetAddr &iAddress = info().iAddress;
    	int namelen;

#if TRACE_ME
		if (1) {
			pj_sockaddr a;
			char ipaddr[PJ_INET6_ADDRSTRLEN+2];
			
			namelen = sizeof(pj_sockaddr);
			if (PjSymbianOS::Addr2pj(iAddress, a, &namelen, 
									 PJ_FALSE) == PJ_SUCCESS) 
			{
				PJ_LOG(5,(THIS_FILE, "Enum: found address %s", 
						pj_sockaddr_print(&a, ipaddr, sizeof(ipaddr), 2)));
			}
		}
#endif
    	
    	namelen = sizeof(ifs[i]);
    	if (PjSymbianOS::Addr2pj(iAddress, ifs[i], &namelen, 
    							 PJ_TRUE) != PJ_SUCCESS)
    	{
    	    continue;
    	}

    	if (ifs[i].addr.sa_family != af)
		    continue;
    	
    	++i;
    }
    
    rSock.Close();
    
    // Done
    *p_cnt = i;
    
    return PJ_SUCCESS;
}
					
/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(int af,
					 unsigned *p_cnt,
					 pj_sockaddr ifs[])
{
    unsigned start;
    pj_status_t status = PJ_SUCCESS;

    start = 0;
    	    
    /* Get IPv6 interface first. */
    if (af==PJ_AF_INET6 || af==PJ_AF_UNSPEC) {
    	unsigned max = *p_cnt;
    	status = rsock_enum_interface(PJ_AF_INET6, &max, &ifs[start]);
    	if (status == PJ_SUCCESS) {
    	    (*p_cnt) -= max;
    	    start += max;
    	}
    }
    
    /* Get IPv4 interface. */
    if (af==PJ_AF_INET || af==PJ_AF_UNSPEC) {
    	unsigned max = *p_cnt;
    	status = rsock_enum_interface(PJ_AF_INET, &max, &ifs[start]);
    	if (status == PJ_SUCCESS) {
    	    (*p_cnt) -= max;
    	    start += max;
    	}
    }
    
    *p_cnt = start;
    
    return start ? PJ_SUCCESS : PJ_ENOTFOUND;
}

/*
 * Enumerate the IP routing table for this host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *p_cnt,
				     pj_ip_route_entry routes[])
{
    PJ_ASSERT_RETURN(p_cnt && *p_cnt > 0 && routes, PJ_EINVAL);
    *p_cnt = 0;
    return PJ_ENOTSUP;
}

