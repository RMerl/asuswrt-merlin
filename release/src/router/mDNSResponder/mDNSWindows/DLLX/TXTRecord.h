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
    
$Log: TXTRecord.h,v $
Revision 1.1  2009/05/26 04:43:54  herscher
<rdar://problem/3948252> COM component that can be used with any .NET language and VB.


*/

#pragma once
#include "resource.h"       // main symbols
#include "DLLX.h"
#include <vector>
#include <dns_sd.h>


#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif



// CTXTRecord

class ATL_NO_VTABLE CTXTRecord :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CTXTRecord, &CLSID_TXTRecord>,
	public IDispatchImpl<ITXTRecord, &IID_ITXTRecord, &LIBID_Bonjour, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
public:
	CTXTRecord()
	:
		m_allocated( FALSE )
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_TXTRECORD)


BEGIN_COM_MAP(CTXTRecord)
	COM_INTERFACE_ENTRY(ITXTRecord)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()



	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
		if ( m_allocated )
		{
			TXTRecordDeallocate( &m_tref );
		}
	}

public:

	STDMETHOD(SetValue)(BSTR key, VARIANT value);
	STDMETHOD(RemoveValue)(BSTR key);
	STDMETHOD(ContainsKey)(BSTR key, VARIANT_BOOL* retval);
	STDMETHOD(GetValueForKey)(BSTR key, VARIANT* value);
	STDMETHOD(GetCount)(ULONG* count);
	STDMETHOD(GetKeyAtIndex)(ULONG index, BSTR* retval);
	STDMETHOD(GetValueAtIndex)(ULONG index, VARIANT* retval);

private:

	typedef std::vector< BYTE > ByteArray;
	ByteArray		m_byteArray;
	BOOL			m_allocated;
	TXTRecordRef	m_tref;

public:

	uint16_t
	GetLen()
	{
		return TXTRecordGetLength( &m_tref );
	}

	const void*
	GetBytes()
	{
		return TXTRecordGetBytesPtr( &m_tref );
	}

	void
	SetBytes
		(
		const unsigned char	*	bytes,
		uint16_t				len
		);
};

OBJECT_ENTRY_AUTO(__uuidof(TXTRecord), CTXTRecord)
