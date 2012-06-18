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
    
$Log: TXTRecord.cpp,v $
Revision 1.1  2009/05/26 04:43:54  herscher
<rdar://problem/3948252> COM component that can be used with any .NET language and VB.


*/

#include "stdafx.h"
#include "TXTRecord.h"
#include "StringServices.h"
#include <DebugServices.h>


// CTXTRecord


STDMETHODIMP CTXTRecord::SetValue(BSTR key, VARIANT value)
{
	std::string			keyUTF8;
	ByteArray			valueArray;
	BOOL				ok;
	DNSServiceErrorType	err;
	HRESULT				hr = S_OK;

	if ( !m_allocated )
	{
		TXTRecordCreate( &m_tref, 0, NULL );
		m_allocated = TRUE;
	}

	ok = BSTRToUTF8( key, keyUTF8 );
	require_action( ok, exit, hr = S_FALSE );

	ok = VariantToByteArray( &value, valueArray );
	require_action( ok, exit, hr = S_FALSE );

	err = TXTRecordSetValue( &m_tref, keyUTF8.c_str(), ( uint8_t ) valueArray.size(), &valueArray[ 0 ] );
	require_action( !err, exit, hr = S_FALSE );

exit:

	return hr;
}

STDMETHODIMP CTXTRecord::RemoveValue(BSTR key)
{
	HRESULT hr = S_OK;

	if ( m_allocated )
	{
		std::string			keyUTF8;
		BOOL				ok;
		DNSServiceErrorType	err;

		ok = BSTRToUTF8( key, keyUTF8 );
		require_action( ok, exit, hr = S_FALSE );

		err = TXTRecordRemoveValue( &m_tref, keyUTF8.c_str() );
		require_action( !err, exit, hr = S_FALSE );
	}

exit:

	return hr;
}

STDMETHODIMP CTXTRecord::ContainsKey(BSTR key, VARIANT_BOOL* retval)
{
	std::string keyUTF8;
	int			ret	= 0;
	HRESULT		err	= S_OK;

	if ( m_byteArray.size() > 0 )
	{
		BOOL ok;

		ok = BSTRToUTF8( key, keyUTF8 );
		require_action( ok, exit, err = S_FALSE );

		ret = TXTRecordContainsKey( ( uint16_t ) m_byteArray.size(), &m_byteArray[ 0 ], keyUTF8.c_str() );
	}

	*retval = ( ret ) ? VARIANT_TRUE : VARIANT_FALSE;

exit:

	return err;
}

STDMETHODIMP CTXTRecord::GetValueForKey(BSTR key, VARIANT* value)
{
	std::string		keyUTF8;
	const void	*	rawValue;
	uint8_t			rawValueLen;
	BOOL			ok	= TRUE;
	HRESULT			hr	= S_OK;

	VariantClear( value );

	if ( m_byteArray.size() > 0 )
	{
		ok = BSTRToUTF8( key, keyUTF8 );
		require_action( ok, exit, hr = S_FALSE );

		rawValue = TXTRecordGetValuePtr( ( uint16_t ) m_byteArray.size(), &m_byteArray[ 0 ], keyUTF8.c_str(), &rawValueLen );

		if ( rawValue )
		{
			ok = ByteArrayToVariant( rawValue, rawValueLen, value );
			require_action( ok, exit, hr = S_FALSE );
		}
	}

exit:

	return hr;
}

STDMETHODIMP CTXTRecord::GetCount(ULONG* count)
{
	*count = 0;

	if ( m_byteArray.size() > 0 )
	{
		*count = TXTRecordGetCount( ( uint16_t ) m_byteArray.size(), &m_byteArray[ 0 ] );
	}

	return S_OK;
}

STDMETHODIMP CTXTRecord::GetKeyAtIndex(ULONG index, BSTR* retval)
{
	char				keyBuf[ 64 ];
	uint8_t				rawValueLen;
	const void		*	rawValue;
	CComBSTR			temp;
	DNSServiceErrorType	err;
	BOOL				ok;
	HRESULT				hr = S_OK;

	err = TXTRecordGetItemAtIndex( ( uint16_t ) m_byteArray.size(), &m_byteArray[ 0 ], ( uint16_t ) index, sizeof( keyBuf ), keyBuf, &rawValueLen, &rawValue );
	require_action( !err, exit, hr = S_FALSE );

	ok = UTF8ToBSTR( keyBuf, temp );
	require_action( ok, exit, hr = S_FALSE );

	*retval = temp;

exit:

	return hr;
}

STDMETHODIMP CTXTRecord::GetValueAtIndex(ULONG index, VARIANT* retval)
{
	char				keyBuf[ 64 ];
	uint8_t				rawValueLen;
	const void		*	rawValue;
	CComBSTR			temp;
	DNSServiceErrorType	err;
	BOOL				ok;
	HRESULT				hr = S_OK;

	err = TXTRecordGetItemAtIndex( ( uint16_t ) m_byteArray.size(), &m_byteArray[ 0 ], ( uint16_t ) index, sizeof( keyBuf ), keyBuf, &rawValueLen, &rawValue );
	require_action( !err, exit, hr = S_FALSE );

	ok = ByteArrayToVariant( rawValue, rawValueLen, retval );
	require_action( ok, exit, hr = S_FALSE );

exit:

	return hr;
}


void
CTXTRecord::SetBytes
	(
	const unsigned char	*	bytes,
	uint16_t				len
	)
{
	check ( bytes != NULL );
	check( len );

	m_byteArray.reserve( len );
	m_byteArray.assign( bytes, bytes + len );
}
