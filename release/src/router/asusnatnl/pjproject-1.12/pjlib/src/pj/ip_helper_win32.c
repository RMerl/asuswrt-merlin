/* $Id: ip_helper_win32.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/config.h>
#include <pj/log.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* PMIB_ICMP_EX is not declared in VC6, causing error.
 * But EVC4, which also claims to be VC6, does have it! 
 */
#if defined(_MSC_VER) && _MSC_VER==1200 && !defined(PJ_WIN32_WINCE)
#   define PMIB_ICMP_EX void*
#endif
#include <winsock2.h>

/* If you encounter error "Cannot open include file: 'Iphlpapi.h' here,
 * you need to install newer Platform SDK. Presumably you're using
 * Microsoft Visual Studio 6?
 */
#include <Iphlpapi.h>

#include <pj/ip_helper.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>

#define THIS_FILE	"ip_helper_win32.c"

/* Dealing with Unicode quirks:

 There seems to be a difference with GetProcAddress() API signature between
 Windows (i.e. Win32) and Windows CE (e.g. Windows Mobile). On Windows, the
 API is declared as:

   FARPROC GetProcAddress(
     HMODULE hModule,
     LPCSTR lpProcName);
 
 while on Windows CE:

   FARPROC GetProcAddress(
     HMODULE hModule,
     LPCWSTR lpProcName);

 Notice the difference with lpProcName argument type. This means that on 
 Windows, even on Unicode Windows, the lpProcName always takes ANSI format, 
 while on Windows CE, the argument follows the UNICODE setting.

 Because of this, we use a different Unicode treatment here than the usual
 PJ_NATIVE_STRING_IS_UNICODE PJLIB setting (<pj/unicode.h>):
   - GPA_TEXT macro: convert literal string to platform's native literal 
         string
   - gpa_char: the platform native character type

 Note that "GPA" and "gpa" are abbreviations for GetProcAddress.
*/
#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
    /* on CE, follow the PJLIB Unicode setting */
#   define GPA_TEXT(x)	PJ_T(x)
#   define gpa_char	pj_char_t
#else
    /* on non-CE, always use ANSI format */
#   define GPA_TEXT(x)	x
#   define gpa_char	char
#endif


typedef DWORD (WINAPI *PFN_GetIpAddrTable)(PMIB_IPADDRTABLE pIpAddrTable, 
					   PULONG pdwSize, 
					   BOOL bOrder);
typedef DWORD (WINAPI *PFN_GetAdapterAddresses)(ULONG Family,
					        ULONG Flags,
					        PVOID Reserved,
					        PIP_ADAPTER_ADDRESSES AdapterAddresses,
					        PULONG SizePointer);
typedef DWORD (WINAPI *PFN_GetIpForwardTable)(PMIB_IPFORWARDTABLE pIpForwardTable,
					      PULONG pdwSize, 
					      BOOL bOrder);
typedef DWORD (WINAPI *PFN_GetIfEntry)(PMIB_IFROW pIfRow);

static HANDLE s_hDLL;
static PFN_GetIpAddrTable s_pfnGetIpAddrTable;
static PFN_GetAdapterAddresses s_pfnGetAdapterAddresses;
static PFN_GetIpForwardTable s_pfnGetIpForwardTable;
static PFN_GetIfEntry s_pfnGetIfEntry;


static void unload_iphlp_module(int inst_id)
{
	PJ_UNUSED_ARG(inst_id);
    FreeLibrary(s_hDLL);
    s_hDLL = NULL;
    s_pfnGetIpAddrTable = NULL;
    s_pfnGetIpForwardTable = NULL;
    s_pfnGetIfEntry = NULL;
    s_pfnGetAdapterAddresses = NULL;
}

static FARPROC GetIpHlpApiProc(gpa_char *lpProcName)
{
    if(NULL == s_hDLL) {
	s_hDLL = LoadLibrary(PJ_T("IpHlpApi"));
	if(NULL != s_hDLL) {
	    pj_atexit(0, &unload_iphlp_module);
	}
    }
	
    if(NULL != s_hDLL)
	return GetProcAddress(s_hDLL, lpProcName);
    
    return NULL;
}

static DWORD MyGetIpAddrTable(PMIB_IPADDRTABLE pIpAddrTable, 
			      PULONG pdwSize, 
			      BOOL bOrder)
{
    if(NULL == s_pfnGetIpAddrTable) {
	s_pfnGetIpAddrTable = (PFN_GetIpAddrTable) 
	    GetIpHlpApiProc(GPA_TEXT("GetIpAddrTable"));
    }
    
    if(NULL != s_pfnGetIpAddrTable) {
	return s_pfnGetIpAddrTable(pIpAddrTable, pdwSize, bOrder);
    }
    
    return ERROR_NOT_SUPPORTED;
}

static DWORD MyGetAdapterAddresses(ULONG Family,
				   ULONG Flags,
				   PVOID Reserved,
				   PIP_ADAPTER_ADDRESSES AdapterAddresses,
				   PULONG SizePointer)
{
    if(NULL == s_pfnGetAdapterAddresses) {
	s_pfnGetAdapterAddresses = (PFN_GetAdapterAddresses) 
	    GetIpHlpApiProc(GPA_TEXT("GetAdaptersAddresses"));
    }
    
    if(NULL != s_pfnGetAdapterAddresses) {
	return s_pfnGetAdapterAddresses(Family, Flags, Reserved,
					AdapterAddresses, SizePointer);
    }
    
    return ERROR_NOT_SUPPORTED;
}

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
static DWORD MyGetIfEntry(MIB_IFROW *pIfRow)
{
    if(NULL == s_pfnGetIfEntry) {
	s_pfnGetIfEntry = (PFN_GetIfEntry) 
	    GetIpHlpApiProc(GPA_TEXT("GetIfEntry"));
    }
    
    if(NULL != s_pfnGetIfEntry) {
	return s_pfnGetIfEntry(pIfRow);
    }
    
    return ERROR_NOT_SUPPORTED;
}
#endif


static DWORD MyGetIpForwardTable(PMIB_IPFORWARDTABLE pIpForwardTable, 
				 PULONG pdwSize, 
				 BOOL bOrder)
{
    if(NULL == s_pfnGetIpForwardTable) {
	s_pfnGetIpForwardTable = (PFN_GetIpForwardTable) 
	    GetIpHlpApiProc(GPA_TEXT("GetIpForwardTable"));
    }
    
    if(NULL != s_pfnGetIpForwardTable) {
	return s_pfnGetIpForwardTable(pIpForwardTable, pdwSize, bOrder);
    }
    
    return ERROR_NOT_SUPPORTED;
}

/* Enumerate local IP interface using GetIpAddrTable()
 * for IPv4 addresses only.
 */
static pj_status_t enum_ipv4_interface(unsigned *p_cnt,
				       pj_sockaddr ifs[])
{
    char ipTabBuff[512];
    MIB_IPADDRTABLE *pTab = (MIB_IPADDRTABLE*)ipTabBuff;
    ULONG tabSize = sizeof(ipTabBuff);
    unsigned i, count;
    DWORD rc = NO_ERROR;

    PJ_ASSERT_RETURN(p_cnt && ifs, PJ_EINVAL);

    /* Get IP address table */
    rc = MyGetIpAddrTable(pTab, &tabSize, FALSE);
    if (rc != NO_ERROR) {
	if (rc == ERROR_INSUFFICIENT_BUFFER) {
	    /* Retry with larger buffer */
	    pTab = (MIB_IPADDRTABLE*)malloc(tabSize);
	    if (pTab)
		rc = MyGetIpAddrTable(pTab, &tabSize, FALSE);
	}

	if (rc != NO_ERROR) {
	    if (pTab != (MIB_IPADDRTABLE*)ipTabBuff)
		free(pTab);
	    return PJ_RETURN_OS_ERROR(rc);
	}
    }

    /* Reset result */
    pj_bzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Now fill out the entries */
    count = (pTab->dwNumEntries < *p_cnt) ? pTab->dwNumEntries : *p_cnt;
    *p_cnt = 0;
    for (i=0; i<count; ++i) {
	MIB_IFROW ifRow;

	/* Ignore 0.0.0.0 address (interface is down?) */
	if (pTab->table[i].dwAddr == 0)
	    continue;

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if ((pj_ntohl(pTab->table[i].dwAddr) >> 24) == 0)
	    continue;

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
	/* Investigate the type of this interface */
	pj_bzero(&ifRow, sizeof(ifRow));
	ifRow.dwIndex = pTab->table[i].dwIndex;
	if (MyGetIfEntry(&ifRow) != 0)
	    continue;

	if (ifRow.dwType == MIB_IF_TYPE_LOOPBACK)
	    continue;
#endif

	ifs[*p_cnt].ipv4.sin_family = PJ_AF_INET;
	ifs[*p_cnt].ipv4.sin_addr.s_addr = pTab->table[i].dwAddr;
	(*p_cnt)++;
    }

    if (pTab != (MIB_IPADDRTABLE*)ipTabBuff)
	free(pTab);

	if (!*p_cnt)
		PJ_LOG(4, ("ip_helper_win32.c", "enum_ipv4_interface() interface not found."));
    return (*p_cnt) ? PJ_SUCCESS : PJ_ENOTFOUND;
}

/* Enumerate local IP interface using GetAdapterAddresses(),
 * which works for both IPv4 and IPv6.
 */
static pj_status_t enum_ipv4_ipv6_interface(int af,
					    unsigned *p_cnt,
					    pj_sockaddr ifs[])
{
    pj_uint8_t buffer[600];
    IP_ADAPTER_ADDRESSES *adapter = (IP_ADAPTER_ADDRESSES*)buffer;
    void *adapterBuf = NULL;
    ULONG size = sizeof(buffer);
    ULONG flags;
    unsigned i;
    DWORD rc;

    flags = GAA_FLAG_SKIP_FRIENDLY_NAME |
	    GAA_FLAG_SKIP_DNS_SERVER |
	    GAA_FLAG_SKIP_MULTICAST;

    rc = MyGetAdapterAddresses(af, flags, NULL, adapter, &size);
    if (rc != ERROR_SUCCESS) {
	if (rc == ERROR_BUFFER_OVERFLOW) {
	    /* Retry with larger memory size */
	    adapterBuf = malloc(size);
	    adapter = (IP_ADAPTER_ADDRESSES*) adapterBuf;
	    if (adapter != NULL)
		rc = MyGetAdapterAddresses(af, flags, NULL, adapter, &size);
	} 

	if (rc != ERROR_SUCCESS) {
	    if (adapterBuf)
		free(adapterBuf);
	    return PJ_RETURN_OS_ERROR(rc);
	}
    }

    /* Reset result */
    pj_bzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Enumerate interface */
    for (i=0; i<*p_cnt && adapter; adapter = adapter->Next) {
	if (adapter->FirstUnicastAddress) {
	    SOCKET_ADDRESS *pAddr = &adapter->FirstUnicastAddress->Address;

	    /* Ignore address family which we didn't request, just in case */
	    if (pAddr->lpSockaddr->sa_family != PJ_AF_INET &&
		pAddr->lpSockaddr->sa_family != PJ_AF_INET6)
	    {
		continue;
	    }

	    /* Apply some filtering to known IPv4 unusable addresses */
	    if (pAddr->lpSockaddr->sa_family == PJ_AF_INET) {
		const pj_sockaddr_in *addr_in = 
		    (const pj_sockaddr_in*)pAddr->lpSockaddr;

		/* Ignore 0.0.0.0 address (interface is down?) */
		if (addr_in->sin_addr.s_addr == 0)
		    continue;

		/* Ignore 0.0.0.0/8 address. This is a special address
		 * which doesn't seem to have practical use.
		 */
		if ((pj_ntohl(addr_in->sin_addr.s_addr) >> 24) == 0)
		    continue;
	    }

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
	    /* Ignore loopback interfaces */
	    /* This should have been IF_TYPE_SOFTWARE_LOOPBACK according to
	     * MSDN, and this macro should have been declared in Ipifcons.h, 
	     * but some SDK versions don't have it.
	     */
	    if (adapter->IfType == MIB_IF_TYPE_LOOPBACK)
		continue;
#endif

	    /* Ignore down interface */
	    if (adapter->OperStatus != IfOperStatusUp)
		continue;

	    ifs[i].addr.sa_family = pAddr->lpSockaddr->sa_family;
	    pj_memcpy(&ifs[i], pAddr->lpSockaddr, pAddr->iSockaddrLength);
	    ++i;
	}
    }

    if (adapterBuf)
	free(adapterBuf);

    *p_cnt = i;
	if (!*p_cnt)
		PJ_LOG(4, ("ip_helper_win32.c", "enum_ipv4_ipv6_interface() interface not found."));
    return (*p_cnt) ? PJ_SUCCESS : PJ_ENOTFOUND;
}


/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(int af,
					 unsigned *p_cnt,
					 pj_sockaddr ifs[])
{
    pj_status_t status = -1;

    PJ_ASSERT_RETURN(p_cnt && ifs, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==PJ_AF_UNSPEC || af==PJ_AF_INET || af==PJ_AF_INET6,
		     PJ_EAFNOTSUP);

    status = enum_ipv4_ipv6_interface(af, p_cnt, ifs);
    if (status != PJ_SUCCESS && (af==PJ_AF_INET || af==PJ_AF_UNSPEC))
	status = enum_ipv4_interface(p_cnt, ifs);
    return status;
}

/*
 * Enumerate the IP routing table for this host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *p_cnt,
				     pj_ip_route_entry routes[])
{
    char ipTabBuff[1024];
    MIB_IPADDRTABLE *pIpTab;
    char rtabBuff[1024];
    MIB_IPFORWARDTABLE *prTab;
    ULONG tabSize;
    unsigned i, count;
    DWORD rc = NO_ERROR;

    PJ_ASSERT_RETURN(p_cnt && routes, PJ_EINVAL);

    pIpTab = (MIB_IPADDRTABLE *)ipTabBuff;
    prTab = (MIB_IPFORWARDTABLE *)rtabBuff;

    /* First get IP address table */
    tabSize = sizeof(ipTabBuff);
    rc = MyGetIpAddrTable(pIpTab, &tabSize, FALSE);
    if (rc != NO_ERROR)
	return PJ_RETURN_OS_ERROR(rc);

    /* Next get IP route table */
    tabSize = sizeof(rtabBuff);

    rc = MyGetIpForwardTable(prTab, &tabSize, 1);
    if (rc != NO_ERROR)
	return PJ_RETURN_OS_ERROR(rc);

    /* Reset routes */
    pj_bzero(routes, sizeof(routes[0]) * (*p_cnt));

    /* Now fill out the route entries */
    count = (prTab->dwNumEntries < *p_cnt) ? prTab->dwNumEntries : *p_cnt;
    *p_cnt = 0;
    for (i=0; i<count; ++i) {
	unsigned j;

	/* Find interface entry */
	for (j=0; j<pIpTab->dwNumEntries; ++j) {
	    if (pIpTab->table[j].dwIndex == prTab->table[i].dwForwardIfIndex)
		break;
	}

	if (j==pIpTab->dwNumEntries)
	    continue;	/* Interface not found */

	routes[*p_cnt].ipv4.if_addr.s_addr = pIpTab->table[j].dwAddr;
	routes[*p_cnt].ipv4.dst_addr.s_addr = prTab->table[i].dwForwardDest;
	routes[*p_cnt].ipv4.mask.s_addr = prTab->table[i].dwForwardMask;

	(*p_cnt)++;
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_physical_address_by_ip(int af,
						   pj_sockaddr addr,
						   unsigned char *physical_addr,
						   pj_uint32_t *physical_addr_len,
						   char *ifname,
						   pj_uint32_t *ifname_len)
{
    pj_uint8_t buffer[600];
    IP_ADAPTER_ADDRESSES *adapter = (IP_ADAPTER_ADDRESSES*)buffer;
    void *adapterBuf = NULL;
    ULONG size = sizeof(buffer);
    ULONG flags;
    unsigned i;
    DWORD rc;
	pj_status_t status = PJ_ENOTFOUND;

	PJ_ASSERT_RETURN(physical_addr && physical_addr_len, PJ_EINVAL);
	PJ_ASSERT_RETURN(*physical_addr_len >= 6, PJ_ETOOSMALL);

    flags = GAA_FLAG_SKIP_FRIENDLY_NAME |
	    GAA_FLAG_SKIP_DNS_SERVER |
	    GAA_FLAG_SKIP_MULTICAST;

    rc = MyGetAdapterAddresses(af, flags, NULL, adapter, &size);
    if (rc != ERROR_SUCCESS) {
		if (rc == ERROR_BUFFER_OVERFLOW) {
			/* Retry with larger memory size */
			adapterBuf = malloc(size);
			adapter = (IP_ADAPTER_ADDRESSES*) adapterBuf;
			if (adapter != NULL)
			rc = MyGetAdapterAddresses(af, flags, NULL, adapter, &size);
		} 

		if (rc != ERROR_SUCCESS) {
			if (adapterBuf)
			free(adapterBuf);
			return PJ_RETURN_OS_ERROR(rc);
		}

		/* Enumerate interface */
		for (i=0; i<8 && adapter; adapter = adapter->Next) {
			if (adapter->FirstUnicastAddress) {
				SOCKET_ADDRESS *pAddr = &adapter->FirstUnicastAddress->Address;
				pj_sockaddr_in *addr_in;

				/* Ignore address family which we didn't request, just in case */
				if (pAddr->lpSockaddr->sa_family != PJ_AF_INET &&
				pAddr->lpSockaddr->sa_family != PJ_AF_INET6)
				{
				continue;
				}

				/* Apply some filtering to known IPv4 unusable addresses */
				if (pAddr->lpSockaddr->sa_family == PJ_AF_INET) {
				addr_in = (pj_sockaddr_in*)pAddr->lpSockaddr;

				/* Ignore 0.0.0.0 address (interface is down?) */
				if (addr_in->sin_addr.s_addr == 0)
					continue;

				/* Ignore 0.0.0.0/8 address. This is a special address
				 * which doesn't seem to have practical use.
				 */
				if ((pj_ntohl(addr_in->sin_addr.s_addr) >> 24) == 0)
					continue;
				}

		#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
				/* Ignore loopback interfaces */
				/* This should have been IF_TYPE_SOFTWARE_LOOPBACK according to
				 * MSDN, and this macro should have been declared in Ipifcons.h, 
				 * but some SDK versions don't have it.
				 */
				if (adapter->IfType == MIB_IF_TYPE_LOOPBACK)
				continue;
		#endif

				/* Ignore down interface */
				if (adapter->OperStatus != IfOperStatusUp)
					continue;

				if (adapter->PhysicalAddressLength > *physical_addr_len)
					return PJ_ENOMEM;

				*physical_addr_len = adapter->PhysicalAddressLength;
				pj_sockaddr_set_port(addr_in, 0);
				pj_sockaddr_set_port(&addr, 0);
				if (pj_sockaddr_cmp(addr_in, &addr) == 0)
				{
					memcpy(physical_addr, adapter->PhysicalAddress, *physical_addr_len);

					// DEAN 2013-11-19
					if (ifname && ifname_len)
					{
						if (*ifname_len < strlen(adapter->AdapterName))
							return PJ_ETOOSMALL;
						memset(ifname, 0, *ifname_len);
						*ifname_len = strlen(adapter->AdapterName);
						memcpy(ifname, adapter->AdapterName, *ifname_len);
					}
					status = PJ_SUCCESS;
					break;
				}
			}
		}

		if (adapterBuf)
		free(adapterBuf);

		if (status == PJ_ENOTFOUND)
			PJ_LOG(4, ("ip_helper_win32.c", "pj_get_physical_address_by_ip() interface not found(1)."));
		return status;
	}
	if (status == PJ_ENOTFOUND)
		PJ_LOG(4, ("ip_helper_win32.c", "pj_get_physical_address_by_ip() interface not found(1)."));
	return status;
}

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
		status = pj_sockaddr_in_set_str_addr((pj_sockaddr_in *)&s_addr, &addr);
		if (status != PJ_SUCCESS)
			return status;
	}

	status = pj_get_physical_address_by_ip(pj_AF_INET(), s_addr, mac, (pj_uint32_t *)&mac_len,
		NULL, NULL);
	if (status != PJ_SUCCESS)
	{
		printf("pj_default_physical_address() failed status=%d\n", status);
		return status;
	}

	// construct string format target hardware address
	sprintf(local_mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
		mac[0]&0xff, mac[1]&0xff, mac[2]&0xff,
		mac[3]&0xff, mac[4]&0xff, mac[5]&0xff);

	*local_mac_len = 17;
	return status;
}

PJ_DEF(pj_status_t) pj_all_physical_addresses(char *local_mac, 
											   pj_uint32_t *local_mac_len)
{
	pj_uint8_t buffer[600];
	char mac_addrs[4500];
	pj_uint32_t mac_addrs_len = 0;
    IP_ADAPTER_ADDRESSES *adapter = (IP_ADAPTER_ADDRESSES*)buffer;
    void *adapterBuf = NULL;
    ULONG size = sizeof(buffer);
    ULONG flags;
    unsigned i;
    DWORD rc;
	pj_status_t status = PJ_ENOTFOUND;

	PJ_ASSERT_RETURN(local_mac && local_mac_len, PJ_EINVAL);

    flags = GAA_FLAG_SKIP_FRIENDLY_NAME |
	    GAA_FLAG_SKIP_DNS_SERVER |
	    GAA_FLAG_SKIP_MULTICAST;

    rc = MyGetAdapterAddresses(pj_AF_INET(), flags, NULL, adapter, &size);
    if (rc != ERROR_SUCCESS) {
		if (rc == ERROR_BUFFER_OVERFLOW) {
			/* Retry with larger memory size */
			adapterBuf = malloc(size);
			adapter = (IP_ADAPTER_ADDRESSES*) adapterBuf;
			if (adapter != NULL)
			rc = MyGetAdapterAddresses(pj_AF_INET(), flags, NULL, adapter, &size);
		} 

		if (rc != ERROR_SUCCESS) {
			if (adapterBuf)
			free(adapterBuf);
			return PJ_RETURN_OS_ERROR(rc);
		}

		/* Enumerate interface */
		memset(mac_addrs, 0, sizeof(mac_addrs));
		for (i=0; i<8 && adapter; adapter = adapter->Next) {
			char tmp_mac[18];
			if (adapter->FirstUnicastAddress) {

		#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
				/* Ignore loopback interfaces */
				/* This should have been IF_TYPE_SOFTWARE_LOOPBACK according to
				 * MSDN, and this macro should have been declared in Ipifcons.h, 
				 * but some SDK versions don't have it.
				 */
				if (adapter->IfType == MIB_IF_TYPE_LOOPBACK)
				{
					PJ_LOG(2, (THIS_FILE, " pj_all_physical_addresses() skip loopback interface."));
					continue;
				}
		#endif

				/* Ignore down interface */
				if (adapter->OperStatus != IfOperStatusUp)
				{
					PJ_LOG(2, (THIS_FILE, " pj_all_physical_addresses() interface status is not up."));
					continue;
				}


				if (adapter->PhysicalAddressLength != 6)
				{
					PJ_LOG(2, (THIS_FILE, " pj_all_physical_addresses() interface physical addrs len is not 6."));
					continue;
				}

				// construct string format target hardware address
				sprintf(tmp_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
					adapter->PhysicalAddress[0]&0xff, adapter->PhysicalAddress[1]&0xff, adapter->PhysicalAddress[2]&0xff,
					adapter->PhysicalAddress[3]&0xff, adapter->PhysicalAddress[4]&0xff, adapter->PhysicalAddress[5]&0xff);

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

		if (adapterBuf)
			free(adapterBuf);

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

		if (status == PJ_ENOTFOUND)
			PJ_LOG(4, ("ip_helper_win32.c", "pj_all_physical_addresses() interface not found(1)."));
		return status;
	}
	if (status == PJ_ENOTFOUND)
		PJ_LOG(4, ("ip_helper_win32.c", "pj_all_physical_addresses() interface not found(2)."));
	return status;
}

PJ_DEF(pj_status_t) pj_resolve_mac_by_arp(char *target_addr, char *target_mac, 
										  pj_uint32_t *target_mac_len, int timeout) {
   pj_sockaddr      s_addr;
   pj_status_t      status = PJ_SUCCESS;
   pj_in_addr dst_addr;
   unsigned char dst_mac[6];
   unsigned long dst_mac_len = sizeof(dst_mac);
   pj_timestamp start, end;
   dst_addr = pj_inet_addr2(target_addr);

   PJ_UNUSED_ARG(timeout);

   PJ_ASSERT_RETURN(target_addr && target_mac && target_mac_len, PJ_EINVAL);
   PJ_ASSERT_RETURN(*target_mac_len >= 17, PJ_ETOOSMALL);

	// get local default ip address
   status = pj_getdefaultipinterface(pj_AF_INET(), &s_addr);
   if (status != PJ_SUCCESS)
	   return status;

   // send ARP packet
   pj_get_timestamp(&start);
   status = SendARP(dst_addr.s_addr, s_addr.ipv4.sin_addr.s_addr, dst_mac, &dst_mac_len);
   pj_get_timestamp(&end);
   printf("Send ARP consume %d ms\n", pj_elapsed_msec(&start, &end));
   if (status != PJ_SUCCESS)
	   return status;

   // construct string format target hardware address
   sprintf(target_mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
	   dst_mac[0]&0xff, dst_mac[1]&0xff, dst_mac[2]&0xff,
	   dst_mac[3]&0xff, dst_mac[4]&0xff, dst_mac[5]&0xff);

   *target_mac_len = 17;

   return status;

}
