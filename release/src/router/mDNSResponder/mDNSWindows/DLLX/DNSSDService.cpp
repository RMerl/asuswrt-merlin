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
    
$Log: DNSSDService.cpp,v $
Revision 1.1  2009/05/26 04:43:54  herscher
<rdar://problem/3948252> COM component that can be used with any .NET language and VB.


*/

#pragma warning(disable:4995)

#include "stdafx.h"
#include <strsafe.h>
#include "DNSSDService.h"
#include "DNSSDEventManager.h"
#include "DNSSDRecord.h"
#include "TXTRecord.h"
#include "StringServices.h"
#include <DebugServices.h>


#define WM_SOCKET (WM_APP + 100)


// CDNSSDService

BOOL						CDNSSDService::m_registeredWindowClass	= FALSE;
HWND						CDNSSDService::m_hiddenWindow			= NULL;
CDNSSDService::SocketMap	CDNSSDService::m_socketMap;


HRESULT CDNSSDService::FinalConstruct()
{
	DNSServiceErrorType	err	= 0;
	HRESULT				hr	= S_OK;

	m_isPrimary = TRUE;
	err = DNSServiceCreateConnection( &m_primary );
	require_action( !err, exit, hr = E_FAIL );

	if ( !m_hiddenWindow )
	{
		TCHAR windowClassName[ 256 ];

		StringCchPrintf( windowClassName, sizeof( windowClassName ) / sizeof( TCHAR ), TEXT( "Bonjour Hidden Window %d" ), GetProcessId( NULL ) );

		if ( !m_registeredWindowClass )
		{
			WNDCLASS	wc;
			ATOM		atom;

			wc.style			= 0;
			wc.lpfnWndProc		= WndProc;
			wc.cbClsExtra		= 0;
			wc.cbWndExtra		= 0;
			wc.hInstance		= NULL;
			wc.hIcon			= NULL;
			wc.hCursor			= NULL;
			wc.hbrBackground	= NULL;
			wc.lpszMenuName		= NULL;
			wc.lpszClassName	= windowClassName;

			atom = RegisterClass(&wc);
			require_action( atom != NULL, exit, hr = E_FAIL );

			m_registeredWindowClass = TRUE;
		}

		m_hiddenWindow = CreateWindow( windowClassName, windowClassName, WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, GetModuleHandle( NULL ), NULL );
		require_action( m_hiddenWindow != NULL, exit, hr = E_FAIL );
	}

	err = WSAAsyncSelect( DNSServiceRefSockFD( m_primary ), m_hiddenWindow, WM_SOCKET, FD_READ );
	require_action( !err, exit, hr = E_FAIL );

	m_socketMap[ DNSServiceRefSockFD( m_primary ) ] = this;

exit:

	return hr;
}


void CDNSSDService::FinalRelease()
{
	dlog( kDebugLevelTrace, "FinalRelease()\n" ); 
	Stop();
}


STDMETHODIMP CDNSSDService::EnumerateDomains(DNSSDFlags flags, ULONG ifIndex, IDNSSDEventManager *eventManager, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object	= NULL;
	DNSServiceRef					subord	= NULL;
	DNSServiceErrorType				err		= 0;
	HRESULT							hr		= 0;

	check( m_primary );

	// Initialize
	*service = NULL;

	try
	{
		object = new CComObject<CDNSSDService>();
	}
	catch ( ... )
	{
		object = NULL;
	}

	require_action( object != NULL, exit, err = kDNSServiceErr_NoMemory );
	object->AddRef();

	subord = m_primary;
	err = DNSServiceEnumerateDomains( &subord, flags | kDNSServiceFlagsShareConnection, ifIndex, ( DNSServiceDomainEnumReply ) &DomainEnumReply, object );
	require_noerr( err, exit );

	object->SetPrimaryRef( m_primary );
	object->SetSubordRef( subord );
	object->SetEventManager( eventManager );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSDService::Browse(DNSSDFlags flags, ULONG ifIndex, BSTR regtype, BSTR domain, IDNSSDEventManager* eventManager, IDNSSDService** service )
{
	CComObject<CDNSSDService>	*	object		= NULL;
	std::string						regtypeUTF8;
	std::string						domainUTF8;
	DNSServiceRef					subord		= NULL;
	DNSServiceErrorType				err			= 0;
	HRESULT							hr			= 0;
	BOOL							ok;

	check( m_primary );

	// Initialize
	*service = NULL;

	// Convert BSTR params to utf8
	ok = BSTRToUTF8( regtype, regtypeUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );
	ok = BSTRToUTF8( domain, domainUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );

	try
	{
		object = new CComObject<CDNSSDService>();
	}
	catch ( ... )
	{
		object = NULL;
	}

	require_action( object != NULL, exit, err = kDNSServiceErr_NoMemory );
	object->AddRef();

	subord = m_primary;
	err = DNSServiceBrowse( &subord, flags | kDNSServiceFlagsShareConnection, ifIndex, regtypeUTF8.c_str(), ( domainUTF8.size() > 0 ) ? domainUTF8.c_str() : NULL, ( DNSServiceBrowseReply ) &BrowseReply, object );
	require_noerr( err, exit );

	object->SetPrimaryRef( m_primary );
	object->SetSubordRef( subord );
	object->SetEventManager( eventManager );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSDService::Resolve(DNSSDFlags flags, ULONG ifIndex, BSTR serviceName, BSTR regType, BSTR domain, IDNSSDEventManager* eventManager, IDNSSDService** service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	std::string						serviceNameUTF8;
	std::string						regTypeUTF8;
	std::string						domainUTF8;
	DNSServiceRef					subord			= NULL;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

	check( m_primary );

	// Initialize
	*service = NULL;

	// Convert BSTR params to utf8
	ok = BSTRToUTF8( serviceName, serviceNameUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );
	ok = BSTRToUTF8( regType, regTypeUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );
	ok = BSTRToUTF8( domain, domainUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );

	try
	{
		object = new CComObject<CDNSSDService>();
	}
	catch ( ... )
	{
		object = NULL;
	}

	require_action( object != NULL, exit, err = kDNSServiceErr_NoMemory );
	object->AddRef();

	subord = m_primary;
	err = DNSServiceResolve( &subord, flags | kDNSServiceFlagsShareConnection, ifIndex, serviceNameUTF8.c_str(), regTypeUTF8.c_str(), domainUTF8.c_str(), ( DNSServiceResolveReply ) &ResolveReply, object );
	require_noerr( err, exit );

	object->SetPrimaryRef( m_primary );
	object->SetSubordRef( subord );
	object->SetEventManager( eventManager );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSDService::Register(DNSSDFlags flags, ULONG ifIndex, BSTR serviceName, BSTR regType, BSTR domain, BSTR host, USHORT port, ITXTRecord *record, IDNSSDEventManager *eventManager, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	std::string						serviceNameUTF8;
	std::string						regTypeUTF8;
	std::string						domainUTF8;
	std::string						hostUTF8;
	const void					*	txtRecord		= NULL;
	uint16_t						txtLen			= 0;
	DNSServiceRef					subord			= NULL;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

	check( m_primary );

	// Initialize
	*service = NULL;

	// Convert BSTR params to utf8
	ok = BSTRToUTF8( serviceName, serviceNameUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );
	ok = BSTRToUTF8( regType, regTypeUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );
	ok = BSTRToUTF8( domain, domainUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );
	ok = BSTRToUTF8( host, hostUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );

	try
	{
		object = new CComObject<CDNSSDService>();
	}
	catch ( ... )
	{
		object = NULL;
	}

	require_action( object != NULL, exit, err = kDNSServiceErr_NoMemory );
	object->AddRef();

	if ( record )
	{
		CComObject< CTXTRecord > * realTXTRecord;

		realTXTRecord = ( CComObject< CTXTRecord >* ) record;

		txtRecord	= realTXTRecord->GetBytes();
		txtLen		= realTXTRecord->GetLen();
	}

	subord = m_primary;
	err = DNSServiceRegister( &subord, flags | kDNSServiceFlagsShareConnection, ifIndex, serviceNameUTF8.c_str(), regTypeUTF8.c_str(), ( domainUTF8.size() > 0 ) ? domainUTF8.c_str() : NULL, hostUTF8.c_str(), htons( port ), txtLen, txtRecord, ( DNSServiceRegisterReply ) &RegisterReply, object );
	require_noerr( err, exit );

	object->SetPrimaryRef( m_primary );
	object->SetSubordRef( subord );
	object->SetEventManager( eventManager );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSDService::QueryRecord(DNSSDFlags flags, ULONG ifIndex, BSTR fullname, DNSSDRRType rrtype, DNSSDRRClass rrclass, IDNSSDEventManager *eventManager, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	DNSServiceRef					subord			= NULL;
	std::string						fullNameUTF8;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

	check( m_primary );

	// Initialize
	*service = NULL;

	// Convert BSTR params to utf8
	ok = BSTRToUTF8( fullname, fullNameUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );

	try
	{
		object = new CComObject<CDNSSDService>();
	}
	catch ( ... )
	{
		object = NULL;
	}

	require_action( object != NULL, exit, err = kDNSServiceErr_NoMemory );
	object->AddRef();

	subord = m_primary;
	err = DNSServiceQueryRecord( &subord, flags | kDNSServiceFlagsShareConnection, ifIndex, fullNameUTF8.c_str(), ( uint16_t ) rrtype, ( uint16_t ) rrclass, ( DNSServiceQueryRecordReply ) &QueryRecordReply, object );
	require_noerr( err, exit );

	object->SetPrimaryRef( m_primary );
	object->SetSubordRef( subord );
	object->SetEventManager( eventManager );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSDService::RegisterRecord(DNSSDFlags flags, ULONG ifIndex, BSTR fullName, DNSSDRRType rrtype, DNSSDRRClass rrclass, VARIANT rdata, ULONG ttl, IDNSSDEventManager* eventManager, IDNSSDRecord** record)
{
	CComObject<CDNSSDRecord>	*	object			= NULL;
	DNSRecordRef					rref			= NULL;
	std::string						fullNameUTF8;
	std::vector< BYTE >				byteArray;
	const void					*	byteArrayPtr	= NULL;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

	check( m_primary );

	// Initialize
	*object = NULL;

	// Convert BSTR params to utf8
	ok = BSTRToUTF8( fullName, fullNameUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );

	// Convert the VARIANT
	ok = VariantToByteArray( &rdata, byteArray );
	require_action( ok, exit, err = kDNSServiceErr_Unknown );

	try
	{
		object = new CComObject<CDNSSDRecord>();
	}
	catch ( ... )
	{
		object = NULL;
	}

	require_action( object != NULL, exit, err = kDNSServiceErr_NoMemory );
	object->AddRef();

	err = DNSServiceRegisterRecord( m_primary, &rref, flags, ifIndex, fullNameUTF8.c_str(), rrtype, rrclass, ( uint16_t ) byteArray.size(), byteArray.size() > 0 ? &byteArray[ 0 ] : NULL, ttl, &RegisterRecordReply, object );
	require_noerr( err, exit );

	object->SetServiceObject( this );
	object->SetRecordRef( rref );
	this->SetEventManager( eventManager );

	*record = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSDService::AddRecord(DNSSDFlags flags, DNSSDRRType rrtype, VARIANT rdata, ULONG ttl, IDNSSDRecord ** record)
{
	CComObject<CDNSSDRecord>	*	object			= NULL;
	DNSRecordRef					rref			= NULL;
	std::vector< BYTE >				byteArray;
	const void					*	byteArrayPtr	= NULL;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

	check( m_primary );

	// Initialize
	*object = NULL;

	// Convert the VARIANT
	ok = VariantToByteArray( &rdata, byteArray );
	require_action( ok, exit, err = kDNSServiceErr_Unknown );

	try
	{
		object = new CComObject<CDNSSDRecord>();
	}
	catch ( ... )
	{
		object = NULL;
	}

	require_action( object != NULL, exit, err = kDNSServiceErr_NoMemory );
	object->AddRef();

	err = DNSServiceAddRecord( m_primary, &rref, flags, rrtype, ( uint16_t ) byteArray.size(), byteArray.size() > 0 ? &byteArray[ 0 ] : NULL, ttl );
	require_noerr( err, exit );

	object->SetServiceObject( this );
	object->SetRecordRef( rref );

	*record = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}

STDMETHODIMP CDNSSDService::ReconfirmRecord(DNSSDFlags flags, ULONG ifIndex, BSTR fullName, DNSSDRRType rrtype, DNSSDRRClass rrclass, VARIANT rdata)
{
	std::string						fullNameUTF8;
	std::vector< BYTE >				byteArray;
	const void					*	byteArrayPtr	= NULL;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

	// Convert BSTR params to utf8
	ok = BSTRToUTF8( fullName, fullNameUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );

	// Convert the VARIANT
	ok = VariantToByteArray( &rdata, byteArray );
	require_action( ok, exit, err = kDNSServiceErr_Unknown );

	err = DNSServiceReconfirmRecord( flags, ifIndex, fullNameUTF8.c_str(), rrtype, rrclass, ( uint16_t ) byteArray.size(), byteArray.size() > 0 ? &byteArray[ 0 ] : NULL );
	require_noerr( err, exit );

exit:

	return err;
}


STDMETHODIMP CDNSSDService::GetProperty(BSTR prop, VARIANT * value )
{
	std::string			propUTF8;
	std::vector< BYTE >	byteArray;
	SAFEARRAY		*	psa			= NULL;
	BYTE			*	pData		= NULL;
	uint32_t			elems		= 0;
	DNSServiceErrorType	err			= 0;
	BOOL				ok = TRUE;

	// Convert BSTR params to utf8
	ok = BSTRToUTF8( prop, propUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );

	// Setup the byte array
	require_action( V_VT( value ) == ( VT_ARRAY|VT_UI1 ), exit, err = kDNSServiceErr_Unknown );
	psa = V_ARRAY( value );
	require_action( psa, exit, err = kDNSServiceErr_Unknown );
	require_action( SafeArrayGetDim( psa ) == 1, exit, err = kDNSServiceErr_Unknown );
	byteArray.reserve( psa->rgsabound[0].cElements );
	byteArray.assign( byteArray.capacity(), 0 );
	elems = ( uint32_t ) byteArray.capacity();

	// Call the function and package the return value in the Variant
	err = DNSServiceGetProperty( propUTF8.c_str(), &byteArray[ 0 ], &elems );
	require_noerr( err, exit );
	ok = ByteArrayToVariant( &byteArray[ 0 ], elems, value );
	require_action( ok, exit, err = kDNSSDError_Unknown );

exit:

	if ( psa )
	{
		SafeArrayUnaccessData( psa );
		psa = NULL;
	}

	return err;
}

STDMETHODIMP CDNSSDService::GetAddrInfo(DNSSDFlags flags, ULONG ifIndex, DNSSDAddressFamily addressFamily, BSTR hostName, IDNSSDEventManager *eventManager, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	DNSServiceRef					subord			= NULL;
	std::string						hostNameUTF8;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

	check( m_primary );

	// Initialize
	*service = NULL;

	// Convert BSTR params to utf8
	ok = BSTRToUTF8( hostName, hostNameUTF8 );
	require_action( ok, exit, err = kDNSServiceErr_BadParam );

	try
	{
		object = new CComObject<CDNSSDService>();
	}
	catch ( ... )
	{
		object = NULL;
	}

	require_action( object != NULL, exit, err = kDNSServiceErr_NoMemory );
	object->AddRef();

	subord = m_primary;
	err = DNSServiceGetAddrInfo( &subord, flags | kDNSServiceFlagsShareConnection, ifIndex, addressFamily, hostNameUTF8.c_str(), ( DNSServiceGetAddrInfoReply ) &GetAddrInfoReply, object );
	require_noerr( err, exit );

	object->SetPrimaryRef( m_primary );
	object->SetSubordRef( subord );
	object->SetEventManager( eventManager );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSDService::NATPortMappingCreate(DNSSDFlags flags, ULONG ifIndex, DNSSDAddressFamily addressFamily, DNSSDProtocol protocol, USHORT internalPort, USHORT externalPort, ULONG ttl, IDNSSDEventManager *eventManager, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	DNSServiceRef					subord			= NULL;
	DNSServiceProtocol				prot			= 0;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;

	check( m_primary );

	// Initialize
	*service = NULL;

	try
	{
		object = new CComObject<CDNSSDService>();
	}
	catch ( ... )
	{
		object = NULL;
	}

	require_action( object != NULL, exit, err = kDNSServiceErr_NoMemory );
	object->AddRef();

	prot = ( addressFamily | protocol );

	subord = m_primary;
	err = DNSServiceNATPortMappingCreate( &subord, flags | kDNSServiceFlagsShareConnection, ifIndex, prot, htons( internalPort ), htons( externalPort ), ttl, ( DNSServiceNATPortMappingReply ) &NATPortMappingReply, object );
	require_noerr( err, exit );

	object->SetPrimaryRef( m_primary );
	object->SetSubordRef( subord );
	object->SetEventManager( eventManager );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSDService::Stop(void)
{
	if ( !m_stopped )
	{
		m_stopped = TRUE;

		dlog( kDebugLevelTrace, "Stop()\n" );

		if ( m_isPrimary && m_primary )
		{
			SocketMap::iterator it;

			if ( m_hiddenWindow )
			{
				WSAAsyncSelect( DNSServiceRefSockFD( m_primary ), m_hiddenWindow, 0, 0 );
			}

			it = m_socketMap.find( DNSServiceRefSockFD( m_primary ) );

			if ( it != m_socketMap.end() )
			{
				m_socketMap.erase( it );
			}

			DNSServiceRefDeallocate( m_primary );
			m_primary = NULL;
		}
		else if ( m_subord )
		{
			DNSServiceRefDeallocate( m_subord );
			m_subord = NULL;
		}

		if ( m_eventManager != NULL )
		{
			m_eventManager->Release();
			m_eventManager = NULL;
		}
	}

	return S_OK;
}


void DNSSD_API
CDNSSDService::DomainEnumReply
    (
    DNSServiceRef                       sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            ifIndex,
    DNSServiceErrorType                 errorCode,
    const char                          *replyDomainUTF8,
    void                                *context
    )
{
	CComObject<CDNSSDService>	* service		= NULL;
	CDNSSDEventManager			* eventManager	= NULL;
	int err = 0;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( service->ShouldHandleReply( errorCode, eventManager ) )
	{
		CComBSTR replyDomain;
		BOOL ok;
		
		ok = UTF8ToBSTR( replyDomainUTF8, replyDomain );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );

		if ( flags & kDNSServiceFlagsAdd )
		{
			eventManager->Fire_DomainFound( service, ( DNSSDFlags ) flags, ifIndex, replyDomain );
		}
		else
		{
			eventManager->Fire_DomainLost( service, ( DNSSDFlags ) flags, ifIndex, replyDomain );
		}
	}

exit:

	return;
}


void DNSSD_API
CDNSSDService::BrowseReply
		(
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            ifIndex,
		DNSServiceErrorType                 errorCode,
		const char                          *serviceNameUTF8,
		const char                          *regTypeUTF8,
		const char                          *replyDomainUTF8,
		void                                *context
		)
{
	CComObject<CDNSSDService>	* service		= NULL;
	CDNSSDEventManager			* eventManager	= NULL;
	int err = 0;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( service->ShouldHandleReply( errorCode, eventManager ) )
	{
		CComBSTR	serviceName;
		CComBSTR	regType;
		CComBSTR	replyDomain;
	
		UTF8ToBSTR( serviceNameUTF8, serviceName );
		UTF8ToBSTR( regTypeUTF8, regType );
		UTF8ToBSTR( replyDomainUTF8, replyDomain );

		if ( flags & kDNSServiceFlagsAdd )
		{
			eventManager->Fire_ServiceFound( service, ( DNSSDFlags ) flags, ifIndex, serviceName, regType, replyDomain );
		}
		else
		{
			eventManager->Fire_ServiceLost( service, ( DNSSDFlags ) flags, ifIndex, serviceName, regType, replyDomain );
		}
	}

exit:

	return;
}


void DNSSD_API
CDNSSDService::ResolveReply
		(
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            ifIndex,
		DNSServiceErrorType                 errorCode,
		const char                          *fullNameUTF8,
		const char                          *hostNameUTF8,
		uint16_t                            port,
		uint16_t                            txtLen,
		const unsigned char                 *txtRecord,
		void                                *context
		)
{
	CComObject<CDNSSDService>	* service		= NULL;
	CDNSSDEventManager			* eventManager	= NULL;
	int err = 0;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( service->ShouldHandleReply( errorCode, eventManager ) )
	{
		CComBSTR					fullName;
		CComBSTR					hostName;
		CComBSTR					regType;
		CComBSTR					replyDomain;
		CComObject< CTXTRecord >*	record;
		BOOL						ok;

		ok = UTF8ToBSTR( fullNameUTF8, fullName );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );
		ok = UTF8ToBSTR( hostNameUTF8, hostName );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );

		try
		{
			record = new CComObject<CTXTRecord>();
		}
		catch ( ... )
		{
			record = NULL;
		}

		require_action( record, exit, err = kDNSServiceErr_NoMemory );
		record->AddRef();

		if ( txtLen > 0 )
		{
			record->SetBytes( txtRecord, txtLen );
		}

		eventManager->Fire_ServiceResolved( service, ( DNSSDFlags ) flags, ifIndex, fullName, hostName, ntohs( port ), record );
	}

exit:

	return;
}


void DNSSD_API
CDNSSDService::RegisterReply
		(
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		DNSServiceErrorType                 errorCode,
		const char                          *serviceNameUTF8,
		const char                          *regTypeUTF8,
		const char                          *domainUTF8,
		void                                *context
		)
{
	CComObject<CDNSSDService>	* service		= NULL;
	CDNSSDEventManager			* eventManager	= NULL;
	int err = 0;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( service->ShouldHandleReply( errorCode, eventManager ) )
	{
		CComBSTR					serviceName;
		CComBSTR					regType;
		CComBSTR					domain;
		BOOL						ok;

		ok = UTF8ToBSTR( serviceNameUTF8, serviceName );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );
		ok = UTF8ToBSTR( regTypeUTF8, regType );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );
		ok = UTF8ToBSTR( domainUTF8, domain );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );

		eventManager->Fire_ServiceRegistered( service, ( DNSSDFlags ) flags, serviceName, regType, domain );
	}

exit:

	return;
}


void DNSSD_API
CDNSSDService::QueryRecordReply
		(
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            ifIndex,
		DNSServiceErrorType                 errorCode,
		const char                          *fullNameUTF8,
		uint16_t                            rrtype,
		uint16_t                            rrclass,
		uint16_t                            rdlen,
		const void                          *rdata,
		uint32_t                            ttl,
		void                                *context
		)
{
	CComObject<CDNSSDService>	* service		= NULL;
	CDNSSDEventManager			* eventManager	= NULL;
	int err = 0;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( service->ShouldHandleReply( errorCode, eventManager ) )
	{
		CComBSTR	fullName;
		VARIANT		var;
		BOOL		ok;

		ok = UTF8ToBSTR( fullNameUTF8, fullName );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );
		ok = ByteArrayToVariant( rdata, rdlen, &var );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );

		eventManager->Fire_QueryRecordAnswered( service, ( DNSSDFlags ) flags, ifIndex, fullName, ( DNSSDRRType ) rrtype, ( DNSSDRRClass ) rrclass, var, ttl );
	}

exit:

	return;
}


void DNSSD_API
CDNSSDService::GetAddrInfoReply
		(
		DNSServiceRef                    sdRef,
		DNSServiceFlags                  flags,
		uint32_t                         ifIndex,
		DNSServiceErrorType              errorCode,
		const char                       *hostNameUTF8,
		const struct sockaddr            *rawAddress,
		uint32_t                         ttl,
		void                             *context
		)
{
	CComObject<CDNSSDService>	* service		= NULL;
	CDNSSDEventManager			* eventManager	= NULL;
	int err = 0;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( service->ShouldHandleReply( errorCode, eventManager ) )
	{
		CComBSTR			hostName;
		DWORD				sockaddrLen;
		DNSSDAddressFamily	addressFamily;
		char				addressUTF8[INET6_ADDRSTRLEN];
		DWORD				addressLen = sizeof( addressUTF8 );
		CComBSTR			address;
		BOOL				ok;

		ok = UTF8ToBSTR( hostNameUTF8, hostName );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );

		switch ( rawAddress->sa_family )
		{
			case AF_INET:
			{
				addressFamily	= kDNSSDAddressFamily_IPv4;
				sockaddrLen		= sizeof( sockaddr_in );
			}
			break;

			case AF_INET6:
			{
				addressFamily	= kDNSSDAddressFamily_IPv6;
				sockaddrLen		= sizeof( sockaddr_in6 );
			}
			break;
		}

		err = WSAAddressToStringA( ( LPSOCKADDR ) rawAddress, sockaddrLen, NULL, addressUTF8, &addressLen );
		require_noerr( err, exit );
		ok = UTF8ToBSTR( addressUTF8, address );
		require_action( ok, exit, err = kDNSServiceErr_Unknown );

		eventManager->Fire_AddressFound( service, ( DNSSDFlags ) flags, ifIndex, hostName, addressFamily, address, ttl );
	}

exit:

	return;
}


void DNSSD_API
CDNSSDService::NATPortMappingReply
    (
    DNSServiceRef                    sdRef,
    DNSServiceFlags                  flags,
    uint32_t                         ifIndex,
    DNSServiceErrorType              errorCode,
    uint32_t                         externalAddress,   /* four byte IPv4 address in network byte order */
    DNSServiceProtocol               protocol,
    uint16_t                         internalPort,
    uint16_t                         externalPort,      /* may be different than the requested port     */
    uint32_t                         ttl,               /* may be different than the requested ttl      */
    void                             *context
    )
{
	CComObject<CDNSSDService>	* service		= NULL;
	CDNSSDEventManager			* eventManager	= NULL;
	int err = 0;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( service->ShouldHandleReply( errorCode, eventManager ) )
	{
		eventManager->Fire_MappingCreated( service, ( DNSSDFlags ) flags, ifIndex, externalAddress, ( DNSSDAddressFamily ) ( protocol & 0x8 ), ( DNSSDProtocol ) ( protocol & 0x80 ), ntohs( internalPort ), ntohs( externalPort ), ttl  );
	}

exit:

	return;
}


void DNSSD_API
CDNSSDService::RegisterRecordReply
		(
		DNSServiceRef		sdRef,
		DNSRecordRef		RecordRef,
		DNSServiceFlags		flags,
		DNSServiceErrorType	errorCode,
		void				*context
		)
{
	CComObject<CDNSSDRecord>	* record		= NULL;
	CDNSSDService				* service		= NULL;
	CDNSSDEventManager			* eventManager	= NULL;
	int err = 0;
	
	record = ( CComObject< CDNSSDRecord >* ) context;
	require_action( record, exit, err = kDNSServiceErr_Unknown );
	service = record->GetServiceObject();
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( service->ShouldHandleReply( errorCode, eventManager ) )
	{
		eventManager->Fire_RecordRegistered( record, ( DNSSDFlags ) flags );
	}

exit:

	return;
}


BOOL
CDNSSDService::ShouldHandleReply( DNSServiceErrorType errorCode, CDNSSDEventManager *& eventManager )
{
	BOOL ok = FALSE;

	if ( !this->Stopped() )
	{
		eventManager = this->GetEventManager();
		require_action( eventManager, exit, ok = FALSE );

		if ( !errorCode )
		{
			ok = TRUE;
		}
		else
		{
			eventManager->Fire_OperationFailed( this, ( DNSSDError ) errorCode );
		}
	}

exit:

	return ok;
}


LRESULT CALLBACK
CDNSSDService::WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if ( msg == WM_SOCKET )
	{
		SocketMap::iterator it;
			
		it = m_socketMap.find( ( SOCKET ) wParam );
		check( it != m_socketMap.end() );

		if ( it != m_socketMap.end() )
		{
			DNSServiceProcessResult( it->second->m_primary );
		}
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);;
}
