/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2009, Apple Computer, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DLLStub.h"

static int		g_defaultErrorCode = kDNSServiceErr_Unknown;
static DLLStub	g_glueLayer;


// ------------------------------------------
// DLLStub implementation
// ------------------------------------------
DLLStub * DLLStub::m_instance;

DLLStub::DLLStub()
:
	m_library( LoadLibrary( TEXT( "dnssd.dll" ) ) )
{
	m_instance = this;
}


DLLStub::~DLLStub()
{
	if ( m_library != NULL )
	{
		FreeLibrary( m_library );
		m_library = NULL;
	}

	m_instance = NULL;
}


bool
DLLStub::GetProcAddress( FARPROC * func, LPCSTR lpProcName )
{ 
	if ( m_instance && m_instance->m_library )
	{
		// Only call ::GetProcAddress if *func is NULL. This allows
		// the calling code to cache the funcptr value, and we get
		// some performance benefit.

		if ( *func == NULL )
		{
			*func = ::GetProcAddress( m_instance->m_library, lpProcName );
		}
	}
	else
	{
		*func = NULL;
	}

	return ( *func != NULL );
}


int DNSSD_API
DNSServiceRefSockFD(DNSServiceRef sdRef)
{
	typedef int (DNSSD_API * Func)(DNSServiceRef sdRef);
	static Func func = NULL;
	int ret = -1;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceProcessResult(DNSServiceRef sdRef)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef sdRef);
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef );
	}
	
	return ret;
}


void DNSSD_API
DNSServiceRefDeallocate(DNSServiceRef sdRef)
{
	typedef void (DNSSD_API * Func)(DNSServiceRef sdRef);
	static Func func = NULL;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		func( sdRef );
	}
}


DNSServiceErrorType DNSSD_API
DNSServiceEnumerateDomains
		(
		DNSServiceRef                       *sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		DNSServiceDomainEnumReply           callBack,
		void                                *context
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef*, DNSServiceFlags, uint32_t, DNSServiceDomainEnumReply, void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, flags, interfaceIndex, callBack, context );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceRegister
		(
		DNSServiceRef                       *sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		const char                          *name,
		const char                          *regtype,
		const char                          *domain,
		const char                          *host,
		uint16_t                            port,
		uint16_t                            txtLen,
		const void                          *txtRecord,
		DNSServiceRegisterReply             callBack,
		void                                *context
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef*, DNSServiceFlags, uint32_t, const char*, const char*, const char*, const char*, uint16_t, uint16_t, const void*, DNSServiceRegisterReply, void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, flags, interfaceIndex, name, regtype, domain, host, port, txtLen, txtRecord, callBack, context );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceAddRecord
		(
		DNSServiceRef                       sdRef,
		DNSRecordRef                        *RecordRef,
		DNSServiceFlags                     flags,
		uint16_t                            rrtype,
		uint16_t                            rdlen,
		const void                          *rdata,
		uint32_t                            ttl
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef, DNSRecordRef*, DNSServiceFlags, uint16_t, uint16_t, const void*, uint32_t );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, RecordRef, flags, rrtype, rdlen, rdata, ttl );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceUpdateRecord
		(
		DNSServiceRef                       sdRef,
		DNSRecordRef                        RecordRef,     /* may be NULL */
		DNSServiceFlags                     flags,
		uint16_t                            rdlen,
		const void                          *rdata,
		uint32_t                            ttl
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef, DNSRecordRef, DNSServiceFlags, uint16_t, const void*, uint32_t );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, RecordRef, flags, rdlen, rdata, ttl );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceRemoveRecord
		(
		DNSServiceRef                 sdRef,
		DNSRecordRef                  RecordRef,
		DNSServiceFlags               flags
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef, DNSRecordRef, DNSServiceFlags );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, RecordRef, flags );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceBrowse
		(
		DNSServiceRef                       *sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		const char                          *regtype,
		const char                          *domain,    /* may be NULL */
		DNSServiceBrowseReply               callBack,
		void                                *context    /* may be NULL */
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef*, DNSServiceFlags, uint32_t, const char*, const char*, DNSServiceBrowseReply, void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, flags, interfaceIndex, regtype, domain, callBack, context );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceResolve
		(
		DNSServiceRef                       *sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		const char                          *name,
		const char                          *regtype,
		const char                          *domain,
		DNSServiceResolveReply              callBack,
		void                                *context  /* may be NULL */
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef*, DNSServiceFlags, uint32_t, const char*, const char*, const char*, DNSServiceResolveReply, void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, flags, interfaceIndex, name, regtype, domain, callBack, context );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceConstructFullName
		(
		char                            *fullName,
		const char                      *service,      /* may be NULL */
		const char                      *regtype,
		const char                      *domain
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)( char*, const char*, const char*, const char* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( fullName, service, regtype, domain );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceCreateConnection(DNSServiceRef *sdRef)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)( DNSServiceRef* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceRegisterRecord
		(
		DNSServiceRef                       sdRef,
		DNSRecordRef                        *RecordRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		const char                          *fullname,
		uint16_t                            rrtype,
		uint16_t                            rrclass,
		uint16_t                            rdlen,
		const void                          *rdata,
		uint32_t                            ttl,
		DNSServiceRegisterRecordReply       callBack,
		void                                *context    /* may be NULL */
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef, DNSRecordRef*, DNSServiceFlags, uint32_t, const char*, uint16_t, uint16_t, uint16_t, const void*, uint16_t, DNSServiceRegisterRecordReply, void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, RecordRef, flags, interfaceIndex, fullname, rrtype, rrclass, rdlen, rdata, ttl, callBack, context );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceQueryRecord
		(
		DNSServiceRef                       *sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		const char                          *fullname,
		uint16_t                            rrtype,
		uint16_t                            rrclass,
		DNSServiceQueryRecordReply          callBack,
		void                                *context  /* may be NULL */
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef*, DNSServiceFlags, uint32_t, const char*, uint16_t, uint16_t, DNSServiceQueryRecordReply, void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, flags, interfaceIndex, fullname, rrtype, rrclass, callBack, context );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceReconfirmRecord
		(
		DNSServiceFlags                    flags,
		uint32_t                           interfaceIndex,
		const char                         *fullname,
		uint16_t                           rrtype,
		uint16_t                           rrclass,
		uint16_t                           rdlen,
		const void                         *rdata
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)( DNSServiceFlags, uint32_t, const char*, uint16_t, uint16_t, uint16_t, const void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( flags, interfaceIndex, fullname, rrtype, rrclass, rdlen, rdata );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceNATPortMappingCreate
		(
		DNSServiceRef                    *sdRef,
		DNSServiceFlags                  flags,
		uint32_t                         interfaceIndex,
		DNSServiceProtocol               protocol,          /* TCP and/or UDP          */
		uint16_t                         internalPort,      /* network byte order      */
		uint16_t                         externalPort,      /* network byte order      */
		uint32_t                         ttl,               /* time to live in seconds */
		DNSServiceNATPortMappingReply    callBack,
		void                             *context           /* may be NULL             */
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef*, DNSServiceFlags, uint32_t, DNSServiceProtocol, uint16_t, uint16_t, uint16_t, DNSServiceNATPortMappingReply, void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, flags, interfaceIndex, protocol, internalPort, externalPort, ttl, callBack, context );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceGetAddrInfo
		(
		DNSServiceRef                    *sdRef,
		DNSServiceFlags                  flags,
		uint32_t                         interfaceIndex,
		DNSServiceProtocol               protocol,
		const char                       *hostname,
		DNSServiceGetAddrInfoReply       callBack,
		void                             *context          /* may be NULL */
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)(DNSServiceRef*, DNSServiceFlags, uint32_t, DNSServiceProtocol, const char*, DNSServiceGetAddrInfoReply, void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( sdRef, flags, interfaceIndex, protocol, hostname, callBack, context );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
DNSServiceGetProperty
		(
		const char *property,  /* Requested property (i.e. kDNSServiceProperty_DaemonVersion) */
		void       *result,    /* Pointer to place to store result */
		uint32_t   *size       /* size of result location */
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)( const char*, void*, uint32_t* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( property, result, size );
	}
	
	return ret;
}


void DNSSD_API
TXTRecordCreate
		(
		TXTRecordRef     *txtRecord,
		uint16_t         bufferLen,
		void             *buffer
		)
{
	typedef void (DNSSD_API * Func)( TXTRecordRef*, uint16_t, void* );
	static Func func = NULL;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		func( txtRecord, bufferLen, buffer );
	}
}


void DNSSD_API
TXTRecordDeallocate
		(
		TXTRecordRef     *txtRecord
		)
{
	typedef void (DNSSD_API * Func)( TXTRecordRef* );
	static Func func = NULL;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		func( txtRecord );
	}
}


DNSServiceErrorType DNSSD_API
TXTRecordSetValue
		(
		TXTRecordRef     *txtRecord,
		const char       *key,
		uint8_t          valueSize,        /* may be zero */
		const void       *value            /* may be NULL */
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)( TXTRecordRef*, const char*, uint8_t, const void* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( txtRecord, key, valueSize, value );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
TXTRecordRemoveValue
		(
		TXTRecordRef     *txtRecord,
		const char       *key
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)( TXTRecordRef*, const char* );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( txtRecord, key );
	}
	
	return ret;
}


int DNSSD_API
TXTRecordContainsKey
		(
		uint16_t         txtLen,
		const void       *txtRecord,
		const char       *key
		)
{
	typedef int (DNSSD_API * Func)( uint16_t, const void*, const char* );
	static Func func = NULL;
	int ret = 0;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( txtLen, txtRecord, key );
	}
	
	return ret;
}


uint16_t DNSSD_API
TXTRecordGetCount
		(
		uint16_t         txtLen,
		const void       *txtRecord
		)
{
	typedef uint16_t (DNSSD_API * Func)( uint16_t, const void* );
	static Func func = NULL;
	uint16_t ret = 0;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( txtLen, txtRecord );
	}
	
	return ret;
}


uint16_t DNSSD_API
TXTRecordGetLength
		(
		const TXTRecordRef *txtRecord
		)
{
	typedef uint16_t (DNSSD_API * Func)( const TXTRecordRef* );
	static Func func = NULL;
	uint16_t ret = 0;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( txtRecord );
	}
	
	return ret;
}


const void * DNSSD_API
TXTRecordGetBytesPtr
		(
		const TXTRecordRef *txtRecord
		)
{
	typedef const void* (DNSSD_API * Func)( const TXTRecordRef* );
	static Func func = NULL;
	const void* ret = NULL;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( txtRecord );
	}
	
	return ret;
}


const void * DNSSD_API
TXTRecordGetValuePtr
		(
		uint16_t         txtLen,
		const void       *txtRecord,
		const char       *key,
		uint8_t          *valueLen
		)
{
	typedef const void* (DNSSD_API * Func)( uint16_t, const void*, const char*, uint8_t* );
	static Func func = NULL;
	const void* ret = NULL;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( txtLen, txtRecord, key, valueLen );
	}
	
	return ret;
}


DNSServiceErrorType DNSSD_API
TXTRecordGetItemAtIndex
		(
		uint16_t         txtLen,
		const void       *txtRecord,
		uint16_t         itemIndex,
		uint16_t         keyBufLen,
		char             *key,
		uint8_t          *valueLen,
		const void       **value
		)
{
	typedef DNSServiceErrorType (DNSSD_API * Func)( uint16_t, const void*, uint16_t, uint16_t, char*, uint8_t*, const void** );
	static Func func = NULL;
	DNSServiceErrorType ret = g_defaultErrorCode;

	if ( DLLStub::GetProcAddress( ( FARPROC* ) &func, __FUNCTION__ ) )
	{
		ret = func( txtLen, txtRecord, itemIndex, keyBufLen, key, valueLen, value );
	}
	
	return ret;
}