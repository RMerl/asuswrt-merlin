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
    
$Log: StringServices.cpp,v $
Revision 1.2  2009/06/02 18:43:57  herscher
<rdar://problem/3948252> Allow component consumers to pass in null strings

Revision 1.1  2009/05/26 04:43:54  herscher
<rdar://problem/3948252> COM component that can be used with any .NET language and VB.


*/

#include "StringServices.h"
#include <DebugServices.h>


extern BOOL
BSTRToUTF8
	(
	BSTR			inString,
	std::string	&	outString
	)
{
	USES_CONVERSION;
	
	char	*	utf8String	= NULL;
	OSStatus    err			= kNoErr;

	outString = "";

	if ( inString )
	{
		TCHAR	*	utf16String	= NULL;
		size_t      size		= 0;

		utf16String = OLE2T( inString );
		require_action( utf16String != NULL, exit, err = kUnknownErr );

		if ( wcslen( utf16String ) > 0 )
		{
			size = (size_t) WideCharToMultiByte( CP_UTF8, 0, utf16String, ( int ) wcslen( utf16String ), NULL, 0, NULL, NULL );
			err = translate_errno( size != 0, GetLastError(), kUnknownErr );
			require_noerr( err, exit );

			try
			{
				utf8String = new char[ size + 1 ];
			}
			catch ( ... )
			{
				utf8String = NULL;
			}

			require_action( utf8String != NULL, exit, err = kNoMemoryErr );
			size = (size_t) WideCharToMultiByte( CP_UTF8, 0, utf16String, ( int ) wcslen( utf16String ), utf8String, (int) size, NULL, NULL);
			err = translate_errno( size != 0, GetLastError(), kUnknownErr );
			require_noerr( err, exit );

			// have to add the trailing 0 because WideCharToMultiByte doesn't do it,
			// although it does return the correct size

			utf8String[size] = '\0';
			outString = utf8String;
		}
	}

exit:

	if ( utf8String != NULL )
	{
		delete [] utf8String;
	}

	return ( !err ) ? TRUE : FALSE;
}


extern BOOL
UTF8ToBSTR
	(
	const char	*	inString,
	CComBSTR	&	outString
	)
{
	wchar_t	*	unicode	= NULL;
	OSStatus	err		= 0;

	if ( inString )
	{
		int n;

		n = MultiByteToWideChar( CP_UTF8, 0, inString, -1, NULL, 0 );
	    
		if ( n > 0 )
		{
			try
			{
				unicode = new wchar_t[ n ];
			}
			catch ( ... )
			{
				unicode = NULL;
			}

			require_action( unicode, exit, err = ERROR_INSUFFICIENT_BUFFER );

			n = MultiByteToWideChar( CP_UTF8, 0, inString, -1, unicode, n );
		}

		outString = unicode;
	}

exit:

    if ( unicode != NULL )
    {
        delete [] unicode;
	}

	return ( !err ) ? TRUE : FALSE;
}


BOOL
ByteArrayToVariant
	(
	const void	*	inArray,
	size_t			inArrayLen,
	VARIANT		*	outVariant
	)
{
	LPBYTE			buf	= NULL;
	HRESULT			hr	= 0;
	BOOL			ok	= TRUE;

	VariantClear( outVariant );
	outVariant->vt		= VT_ARRAY|VT_UI1;
	outVariant->parray	= SafeArrayCreateVector( VT_UI1, 0, ( ULONG ) inArrayLen );
	require_action( outVariant->parray, exit, ok = FALSE );
	hr = SafeArrayAccessData( outVariant->parray, (LPVOID *)&buf );
	require_action( hr == S_OK, exit, ok = FALSE );
	memcpy( buf, inArray, inArrayLen );
	hr = SafeArrayUnaccessData( outVariant->parray );
	require_action( hr == S_OK, exit, ok = FALSE );

exit:

	return ok;
}


extern BOOL
VariantToByteArray
	(
	VARIANT				*	inVariant,
	std::vector< BYTE >	&	outArray
	)
{
	SAFEARRAY	*	psa			= NULL;
	BYTE		*	pData		= NULL;
	ULONG			cElements	= 0;
	HRESULT			hr;
	BOOL			ok = TRUE;

	require_action( V_VT( inVariant ) == ( VT_ARRAY|VT_UI1 ), exit, ok = FALSE );
	psa = V_ARRAY( inVariant );
	require_action( psa, exit, ok = FALSE );
	require_action( SafeArrayGetDim( psa ) == 1, exit, ok = FALSE );
	hr = SafeArrayAccessData( psa, ( LPVOID* )&pData );
	require_action( hr == S_OK, exit, ok = FALSE );
	cElements = psa->rgsabound[0].cElements;
	outArray.reserve( cElements );
	outArray.assign( cElements, 0 );
	memcpy( &outArray[ 0 ], pData, cElements );
	SafeArrayUnaccessData( psa );

exit:

	return ok;
}