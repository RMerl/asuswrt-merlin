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
    
$Log: DNSSDRecord.h,v $
Revision 1.1  2009/05/26 04:43:54  herscher
<rdar://problem/3948252> COM component that can be used with any .NET language and VB.


*/

#pragma once
#include "resource.h"       // main symbols

#include "DLLX.h"
#include "DNSSDService.h"
#include <dns_sd.h>


#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif



// CDNSSDRecord

class ATL_NO_VTABLE CDNSSDRecord :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CDNSSDRecord, &CLSID_DNSSDRecord>,
	public IDispatchImpl<IDNSSDRecord, &IID_IDNSSDRecord, &LIBID_Bonjour, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
public:
	CDNSSDRecord()
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_DNSSDRECORD)


BEGIN_COM_MAP(CDNSSDRecord)
	COM_INTERFACE_ENTRY(IDNSSDRecord)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()



	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

	inline CDNSSDService*
	GetServiceObject()
	{
		return m_serviceObject;
	}

	inline void
	SetServiceObject( CDNSSDService * serviceObject )
	{
		m_serviceObject = serviceObject;
	}

	inline DNSRecordRef
	GetRecordRef()
	{
		return m_rref;
	}

	inline void
	SetRecordRef( DNSRecordRef rref )
	{
		m_rref = rref;
	}

public:

	STDMETHOD(Update)(DNSSDFlags flags, VARIANT rdata, ULONG ttl);
	STDMETHOD(Remove)(DNSSDFlags flags);

private:

	CDNSSDService *	m_serviceObject;
	DNSRecordRef	m_rref;
};

OBJECT_ENTRY_AUTO(__uuidof(DNSSDRecord), CDNSSDRecord)
