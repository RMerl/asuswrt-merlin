// DNSSD.cpp : Implementation of CDNSSD

#include "stdafx.h"
#include "DNSSD.h"
#include "DNSSDService.h"
#include "TXTRecord.h"
#include <dns_sd.h>
#include <CommonServices.h>
#include <DebugServices.h>
#include "StringServices.h"


// CDNSSD

STDMETHODIMP CDNSSD::Browse(DNSSDFlags flags, ULONG ifIndex, BSTR regtype, BSTR domain, IBrowseListener* listener, IDNSSDService** browser )
{
	CComObject<CDNSSDService>	*	object		= NULL;
	std::string						regtypeUTF8;
	std::string						domainUTF8;
	DNSServiceRef					sref		= NULL;
	DNSServiceErrorType				err			= 0;
	HRESULT							hr			= 0;
	BOOL							ok;

	// Initialize
	*browser = NULL;

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
	hr = object->FinalConstruct();
	require_action( hr == S_OK, exit, err = kDNSServiceErr_Unknown );
	object->AddRef();

	err = DNSServiceBrowse( &sref, flags, ifIndex, regtypeUTF8.c_str(), domainUTF8.c_str(), ( DNSServiceBrowseReply ) &BrowseReply, object );
	require_noerr( err, exit );

	object->SetServiceRef( sref );
	object->SetListener( listener );

	err = object->Run();
	require_noerr( err, exit );

	*browser = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSD::Resolve(DNSSDFlags flags, ULONG ifIndex, BSTR serviceName, BSTR regType, BSTR domain, IResolveListener* listener, IDNSSDService** service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	std::string						serviceNameUTF8;
	std::string						regTypeUTF8;
	std::string						domainUTF8;
	DNSServiceRef					sref			= NULL;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

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
	hr = object->FinalConstruct();
	require_action( hr == S_OK, exit, err = kDNSServiceErr_Unknown );
	object->AddRef();

	err = DNSServiceResolve( &sref, flags, ifIndex, serviceNameUTF8.c_str(), regTypeUTF8.c_str(), domainUTF8.c_str(), ( DNSServiceResolveReply ) &ResolveReply, object );
	require_noerr( err, exit );

	object->SetServiceRef( sref );
	object->SetListener( listener );

	err = object->Run();
	require_noerr( err, exit );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSD::EnumerateDomains(DNSSDFlags flags, ULONG ifIndex, IDomainListener *listener, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	DNSServiceRef					sref			= NULL;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;

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
	hr = object->FinalConstruct();
	require_action( hr == S_OK, exit, err = kDNSServiceErr_Unknown );
	object->AddRef();

	err = DNSServiceEnumerateDomains( &sref, flags, ifIndex, ( DNSServiceDomainEnumReply ) &DomainEnumReply, object );
	require_noerr( err, exit );

	object->SetServiceRef( sref );
	object->SetListener( listener );

	err = object->Run();
	require_noerr( err, exit );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSD::Register(DNSSDFlags flags, ULONG ifIndex, BSTR serviceName, BSTR regType, BSTR domain, BSTR host, USHORT port, ITXTRecord *record, IRegisterListener *listener, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	std::string						serviceNameUTF8;
	std::string						regTypeUTF8;
	std::string						domainUTF8;
	std::string						hostUTF8;
	const void					*	txtRecord		= NULL;
	uint16_t						txtLen			= 0;
	DNSServiceRef					sref			= NULL;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

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
	hr = object->FinalConstruct();
	require_action( hr == S_OK, exit, err = kDNSServiceErr_Unknown );
	object->AddRef();

	if ( record )
	{
		CComObject< CTXTRecord > * realTXTRecord;

		realTXTRecord = ( CComObject< CTXTRecord >* ) record;

		txtRecord	= realTXTRecord->GetBytes();
		txtLen		= realTXTRecord->GetLen();
	}

	err = DNSServiceRegister( &sref, flags, ifIndex, serviceNameUTF8.c_str(), regTypeUTF8.c_str(), domainUTF8.c_str(), hostUTF8.c_str(), port, txtLen, txtRecord, ( DNSServiceRegisterReply ) &RegisterReply, object );
	require_noerr( err, exit );

	object->SetServiceRef( sref );
	object->SetListener( listener );

	err = object->Run();
	require_noerr( err, exit );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSD::QueryRecord(DNSSDFlags flags, ULONG ifIndex, BSTR fullname, DNSSDRRType rrtype, DNSSDRRClass rrclass, IQueryRecordListener *listener, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	DNSServiceRef					sref			= NULL;
	std::string						fullNameUTF8;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

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
	hr = object->FinalConstruct();
	require_action( hr == S_OK, exit, err = kDNSServiceErr_Unknown );
	object->AddRef();

	err = DNSServiceQueryRecord( &sref, flags, ifIndex, fullNameUTF8.c_str(), ( uint16_t ) rrtype, ( uint16_t ) rrclass, ( DNSServiceQueryRecordReply ) &QueryRecordReply, object );
	require_noerr( err, exit );

	object->SetServiceRef( sref );
	object->SetListener( listener );

	err = object->Run();
	require_noerr( err, exit );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSD::GetAddrInfo(DNSSDFlags flags, ULONG ifIndex, DNSSDAddressFamily addressFamily, BSTR hostName, IGetAddrInfoListener *listener, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	DNSServiceRef					sref			= NULL;
	std::string						hostNameUTF8;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;
	BOOL							ok;

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
	hr = object->FinalConstruct();
	require_action( hr == S_OK, exit, err = kDNSServiceErr_Unknown );
	object->AddRef();

	err = DNSServiceGetAddrInfo( &sref, flags, ifIndex, addressFamily, hostNameUTF8.c_str(), ( DNSServiceGetAddrInfoReply ) &GetAddrInfoReply, object );
	require_noerr( err, exit );

	object->SetServiceRef( sref );
	object->SetListener( listener );

	err = object->Run();
	require_noerr( err, exit );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSD::CreateConnection(IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object	= NULL;
	DNSServiceRef					sref	= NULL;
	DNSServiceErrorType				err		= 0;
	HRESULT							hr		= 0;

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
	hr = object->FinalConstruct();
	require_action( hr == S_OK, exit, err = kDNSServiceErr_Unknown );
	object->AddRef();

	err = DNSServiceCreateConnection( &sref );
	require_noerr( err, exit );

	object->SetServiceRef( sref );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSD::NATPortMappingCreate(DNSSDFlags flags, ULONG ifIndex, DNSSDAddressFamily addressFamily, DNSSDProtocol protocol, USHORT internalPort, USHORT externalPort, ULONG ttl, INATPortMappingListener *listener, IDNSSDService **service)
{
	CComObject<CDNSSDService>	*	object			= NULL;
	DNSServiceRef					sref			= NULL;
	DNSServiceProtocol				prot			= 0;
	DNSServiceErrorType				err				= 0;
	HRESULT							hr				= 0;

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
	hr = object->FinalConstruct();
	require_action( hr == S_OK, exit, err = kDNSServiceErr_Unknown );
	object->AddRef();

	prot = ( addressFamily | protocol );

	err = DNSServiceNATPortMappingCreate( &sref, flags, ifIndex, prot, internalPort, externalPort, ttl, ( DNSServiceNATPortMappingReply ) &NATPortMappingReply, object );
	require_noerr( err, exit );

	object->SetServiceRef( sref );
	object->SetListener( listener );

	err = object->Run();
	require_noerr( err, exit );

	*service = object;

exit:

	if ( err && object )
	{
		object->Release();
	}

	return err;
}


STDMETHODIMP CDNSSD::GetProperty(BSTR prop, VARIANT * value )
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


void DNSSD_API
CDNSSD::DomainEnumReply
    (
    DNSServiceRef                       sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            ifIndex,
    DNSServiceErrorType                 errorCode,
    const char                          *replyDomainUTF8,
    void                                *context
    )
{
	CComObject<CDNSSDService> * service;
	int err;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( !service->Stopped() )
	{
		IDomainListener	* listener;

		listener = ( IDomainListener* ) service->GetListener();
		require_action( listener, exit, err = kDNSServiceErr_Unknown );

		if ( !errorCode )
		{
			CComBSTR replyDomain;
		
			UTF8ToBSTR( replyDomainUTF8, replyDomain );

			if ( flags & kDNSServiceFlagsAdd )
			{
				listener->DomainFound( service, ( DNSSDFlags ) flags, ifIndex, replyDomain );
			}
			else
			{
				listener->DomainLost( service, ( DNSSDFlags ) flags, ifIndex, replyDomain );
			}
		}
		else
		{
			listener->EnumDomainsFailed( service, ( DNSSDError ) errorCode );
		}
	}

exit:

	return;
}


void DNSSD_API
CDNSSD::BrowseReply
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
	CComObject<CDNSSDService> * service;
	int err;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( !service->Stopped() )
	{
		IBrowseListener	* listener;

		listener = ( IBrowseListener* ) service->GetListener();
		require_action( listener, exit, err = kDNSServiceErr_Unknown );

		if ( !errorCode )
		{
			CComBSTR	serviceName;
			CComBSTR	regType;
			CComBSTR	replyDomain;
		
			UTF8ToBSTR( serviceNameUTF8, serviceName );
			UTF8ToBSTR( regTypeUTF8, regType );
			UTF8ToBSTR( replyDomainUTF8, replyDomain );

			if ( flags & kDNSServiceFlagsAdd )
			{
				listener->ServiceFound( service, ( DNSSDFlags ) flags, ifIndex, serviceName, regType, replyDomain );
			}
			else
			{
				listener->ServiceLost( service, ( DNSSDFlags ) flags, ifIndex, serviceName, regType, replyDomain );
			}
		}
		else
		{
			listener->BrowseFailed( service, ( DNSSDError ) errorCode );
		}
	}

exit:

	return;
}


void DNSSD_API
CDNSSD::ResolveReply
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
	CComObject<CDNSSDService> * service;
	int err;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( !service->Stopped() )
	{
		IResolveListener * listener;

		listener = ( IResolveListener* ) service->GetListener();
		require_action( listener, exit, err = kDNSServiceErr_Unknown );

		if ( !errorCode )
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

			char buf[ 64 ];
			sprintf( buf, "txtLen = %d", txtLen );
			OutputDebugStringA( buf );

			if ( txtLen > 0 )
			{
				record->SetBytes( txtRecord, txtLen );
			}

			listener->ServiceResolved( service, ( DNSSDFlags ) flags, ifIndex, fullName, hostName, port, record );
		}
		else
		{
			listener->ResolveFailed( service, ( DNSSDError ) errorCode );
		}
	}

exit:

	return;
}


void DNSSD_API
CDNSSD::RegisterReply
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
	CComObject<CDNSSDService> * service;
	int err;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( !service->Stopped() )
	{
		IRegisterListener * listener;

		listener = ( IRegisterListener* ) service->GetListener();
		require_action( listener, exit, err = kDNSServiceErr_Unknown );

		if ( !errorCode )
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

			listener->ServiceRegistered( service, ( DNSSDFlags ) flags, serviceName, regType, domain );
		}
		else
		{
			listener->ServiceRegisterFailed( service, ( DNSSDError ) errorCode );
		}
	}

exit:

	return;
}


void DNSSD_API
CDNSSD::QueryRecordReply
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
	CComObject<CDNSSDService> * service;
	int err;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( !service->Stopped() )
	{
		IQueryRecordListener * listener;

		listener = ( IQueryRecordListener* ) service->GetListener();
		require_action( listener, exit, err = kDNSServiceErr_Unknown );

		if ( !errorCode )
		{
			CComBSTR	fullName;
			VARIANT		var;
			BOOL		ok;

			ok = UTF8ToBSTR( fullNameUTF8, fullName );
			require_action( ok, exit, err = kDNSServiceErr_Unknown );
			ok = ByteArrayToVariant( rdata, rdlen, &var );
			require_action( ok, exit, err = kDNSServiceErr_Unknown );

			listener->QueryRecordAnswered( service, ( DNSSDFlags ) flags, ifIndex, fullName, ( DNSSDRRType ) rrtype, ( DNSSDRRClass ) rrclass, var, ttl );
		}
		else
		{
			listener->QueryRecordFailed( service, ( DNSSDError ) errorCode );
		}
	}

exit:

	return;
}


void DNSSD_API
CDNSSD::GetAddrInfoReply
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
	CComObject<CDNSSDService> * service;
	int err;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( !service->Stopped() )
	{
		IGetAddrInfoListener * listener;

		listener = ( IGetAddrInfoListener* ) service->GetListener();
		require_action( listener, exit, err = kDNSServiceErr_Unknown );

		if ( !errorCode )
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

			listener->GetAddrInfoReply( service, ( DNSSDFlags ) flags, ifIndex, hostName, addressFamily, address, ttl );
		}
		else
		{
			listener->GetAddrInfoFailed( service, ( DNSSDError ) errorCode );
		}
	}

exit:

	return;
}


void DNSSD_API
CDNSSD::NATPortMappingReply
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
	CComObject<CDNSSDService> * service;
	int err;
	
	service = ( CComObject< CDNSSDService>* ) context;
	require_action( service, exit, err = kDNSServiceErr_Unknown );

	if ( !service->Stopped() )
	{
		INATPortMappingListener * listener;

		listener = ( INATPortMappingListener* ) service->GetListener();
		require_action( listener, exit, err = kDNSServiceErr_Unknown );

		if ( !errorCode )
		{
			listener->MappingCreated( service, ( DNSSDFlags ) flags, ifIndex, externalAddress, ( DNSSDAddressFamily ) ( protocol & 0x8 ), ( DNSSDProtocol ) ( protocol & 0x80 ), internalPort, externalPort, ttl  );
		}
		else
		{
			listener->MappingFailed( service, ( DNSSDError ) errorCode );
		}
	}

exit:

	return;
}

