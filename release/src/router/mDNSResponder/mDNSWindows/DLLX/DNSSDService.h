/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2009 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

    Change History (most recent first):
    
$Log: DNSSDService.h,v $
Revision 1.1  2009/05/26 04:43:54  herscher
<rdar://problem/3948252> COM component that can be used with any .NET language and VB.


*/

#pragma once
#include "resource.h"       // main symbols

#include "DLLX.h"
#include "DNSSDEventManager.h"
#include <CommonServices.h>
#include <DebugServices.h>
#include <dns_sd.h>
#include <map>


#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif



// CDNSSDService

class ATL_NO_VTABLE CDNSSDService :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CDNSSDService, &CLSID_DNSSDService>,
	public IDispatchImpl<IDNSSDService, &IID_IDNSSDService, &LIBID_Bonjour, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
public:

	typedef CComObjectRootEx<CComSingleThreadModel> Super;

	CDNSSDService()
	:
		m_isPrimary( FALSE ),
		m_eventManager( NULL ),
		m_stopped( FALSE ),
		m_primary( NULL ),
		m_subord( NULL )
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_DNSSDSERVICE)


BEGIN_COM_MAP(CDNSSDService)
	COM_INTERFACE_ENTRY(IDNSSDService)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT
	FinalConstruct();

	void 
	FinalRelease();

public:

	inline DNSServiceRef
	GetPrimaryRef()
	{
		return m_primary;
	}

	inline void
	SetPrimaryRef( DNSServiceRef primary )
	{
		m_primary = primary;
	}

	inline DNSServiceRef
	GetSubordRef()
	{
		return m_subord;
	}

	inline void
	SetSubordRef( DNSServiceRef subord )
	{
		m_subord = subord;
	}

	inline CDNSSDEventManager*
	GetEventManager()
	{
		return m_eventManager;
	}

	inline void
	SetEventManager( IDNSSDEventManager * eventManager )
	{
		if ( m_eventManager )
		{
			m_eventManager->Release();
			m_eventManager = NULL;
		}

		if ( eventManager )
		{
			m_eventManager = dynamic_cast< CDNSSDEventManager* >( eventManager );
			check( m_eventManager );
			m_eventManager->AddRef();
		}
	}

	inline BOOL
	Stopped()
	{
		return m_stopped;
	}

private:

	static void DNSSD_API
	DomainEnumReply
		(
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            ifIndex,
		DNSServiceErrorType                 errorCode,
		const char                          *replyDomain,
		void                                *context
		);

	static void DNSSD_API
	BrowseReply
		(
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		DNSServiceErrorType                 errorCode,
		const char                          *serviceName,
		const char                          *regtype,
		const char                          *replyDomain,
		void                                *context
		);

	static void DNSSD_API
	ResolveReply
		(
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		DNSServiceErrorType                 errorCode,
		const char                          *fullname,
		const char                          *hosttarget,
		uint16_t                            port,
		uint16_t                            txtLen,
		const unsigned char                 *txtRecord,
		void                                *context
		);

	static void DNSSD_API
	RegisterReply
		(
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		DNSServiceErrorType                 errorCode,
		const char                          *name,
		const char                          *regtype,
		const char                          *domain,
		void                                *context
		);

	static void DNSSD_API
	QueryRecordReply
		(
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		DNSServiceErrorType                 errorCode,
		const char                          *fullname,
		uint16_t                            rrtype,
		uint16_t                            rrclass,
		uint16_t                            rdlen,
		const void                          *rdata,
		uint32_t                            ttl,
		void                                *context
		);

	static void DNSSD_API
    GetAddrInfoReply
		(
		DNSServiceRef                    sdRef,
		DNSServiceFlags                  flags,
		uint32_t                         interfaceIndex,
		DNSServiceErrorType              errorCode,
		const char                       *hostname,
		const struct sockaddr            *address,
		uint32_t                         ttl,
		void                             *context
		);

	static void DNSSD_API
	NATPortMappingReply
		(
		DNSServiceRef                    sdRef,
		DNSServiceFlags                  flags,
		uint32_t                         interfaceIndex,
		DNSServiceErrorType              errorCode,
		uint32_t                         externalAddress,   /* four byte IPv4 address in network byte order */
		DNSServiceProtocol               protocol,
		uint16_t                         internalPort,
		uint16_t                         externalPort,      /* may be different than the requested port     */
		uint32_t                         ttl,               /* may be different than the requested ttl      */
		void                             *context
		);

	static void DNSSD_API
	RegisterRecordReply
		(
		DNSServiceRef                       sdRef,
		DNSRecordRef                        RecordRef,
		DNSServiceFlags                     flags,
		DNSServiceErrorType                 errorCode,
		void                                *context
		);

	inline BOOL
	ShouldHandleReply( DNSServiceErrorType errorCode, CDNSSDEventManager *& eventManager );
	
	static LRESULT CALLBACK
	WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

	typedef std::map< SOCKET, CDNSSDService* > SocketMap;

	static BOOL				m_registeredWindowClass;
	static HWND				m_hiddenWindow;
	static SocketMap		m_socketMap;
	CDNSSDEventManager	*	m_eventManager;
	BOOL					m_stopped;
	BOOL					m_isPrimary;
	DNSServiceRef			m_primary;
	DNSServiceRef			m_subord;
public:
	STDMETHOD(EnumerateDomains)(DNSSDFlags flags, ULONG ifIndex, IDNSSDEventManager *eventManager, IDNSSDService **service);  
	STDMETHOD(Browse)(DNSSDFlags flags, ULONG interfaceIndex, BSTR regtype, BSTR domain, IDNSSDEventManager* eventManager, IDNSSDService** sdref);
	STDMETHOD(Resolve)(DNSSDFlags flags, ULONG ifIndex, BSTR serviceName, BSTR regType, BSTR domain, IDNSSDEventManager* eventManager, IDNSSDService** service);
	STDMETHOD(Register)(DNSSDFlags flags, ULONG ifIndex, BSTR name, BSTR regType, BSTR domain, BSTR host, USHORT port, ITXTRecord *record, IDNSSDEventManager *eventManager, IDNSSDService **service);
	STDMETHOD(QueryRecord)(DNSSDFlags flags, ULONG ifIndex, BSTR fullname, DNSSDRRType rrtype, DNSSDRRClass rrclass, IDNSSDEventManager *eventManager, IDNSSDService **service);      
	STDMETHOD(RegisterRecord)(DNSSDFlags flags, ULONG ifIndex, BSTR fullname, DNSSDRRType rrtype, DNSSDRRClass rrclass, VARIANT rdata, ULONG ttl, IDNSSDEventManager* eventManager, IDNSSDRecord** record);
	STDMETHOD(AddRecord)(DNSSDFlags flags, DNSSDRRType rrtype, VARIANT rdata, ULONG ttl, IDNSSDRecord ** record);
	STDMETHOD(ReconfirmRecord)(DNSSDFlags flags, ULONG ifIndex, BSTR fullname, DNSSDRRType rrtype, DNSSDRRClass rrclass, VARIANT rdata);
	STDMETHOD(GetProperty)(BSTR prop, VARIANT * value);
	STDMETHOD(GetAddrInfo)(DNSSDFlags flags, ULONG ifIndex, DNSSDAddressFamily addressFamily, BSTR hostname, IDNSSDEventManager *eventManager, IDNSSDService **service);      
	STDMETHOD(NATPortMappingCreate)(DNSSDFlags flags, ULONG ifIndex, DNSSDAddressFamily addressFamily, DNSSDProtocol protocol, USHORT internalPort, USHORT externalPort, ULONG ttl, IDNSSDEventManager *eventManager, IDNSSDService **service);
	STDMETHOD(Stop)(void);
};

OBJECT_ENTRY_AUTO(__uuidof(DNSSDService), CDNSSDService)
