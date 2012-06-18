/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
    
$Log: Secret.c,v $
Revision 1.3  2009/07/17 19:50:25  herscher
<rdar://problem/5265747> ControlPanel doesn't display key and password in dialog box

Revision 1.2  2009/06/25 21:11:52  herscher
Fix compilation error when building Control Panel.

Revision 1.1  2009/06/22 23:25:04  herscher
<rdar://problem/5265747> ControlPanel doesn't display key and password in dialog box. Refactor Lsa calls into Secret.h and Secret.c, which is used by both the ControlPanel and mDNSResponder system service.


*/

#include "Secret.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <ntsecapi.h>
#include <lm.h>
#include "DebugServices.h"


mDNSlocal OSStatus MakeLsaStringFromUTF8String( PLSA_UNICODE_STRING output, const char * input );
mDNSlocal OSStatus MakeUTF8StringFromLsaString( char * output, size_t len, PLSA_UNICODE_STRING input );


BOOL
LsaGetSecret( const char * inDomain, char * outDomain, unsigned outDomainSize, char * outKey, unsigned outKeySize, char * outSecret, unsigned outSecretSize )
{
	PLSA_UNICODE_STRING		domainLSA;
	PLSA_UNICODE_STRING		keyLSA;
	PLSA_UNICODE_STRING		secretLSA;
	size_t					i;
	size_t					dlen;
	LSA_OBJECT_ATTRIBUTES	attrs;
	LSA_HANDLE				handle = NULL;
	NTSTATUS				res;
	OSStatus				err;

	check( inDomain );
	check( outDomain );
	check( outKey );
	check( outSecret );

	// Initialize

	domainLSA	= NULL;
	keyLSA		= NULL;
	secretLSA	= NULL;

	// Make sure we have enough space to add trailing dot

	dlen = strlen( inDomain );
	err = strcpy_s( outDomain, outDomainSize - 2, inDomain );
	require_noerr( err, exit );

	// If there isn't a trailing dot, add one because the mDNSResponder
	// presents names with the trailing dot.

	if ( outDomain[ dlen - 1 ] != '.' )
	{
		outDomain[ dlen++ ] = '.';
		outDomain[ dlen ] = '\0';
	}

	// Canonicalize name by converting to lower case (keychain and some name servers are case sensitive)

	for ( i = 0; i < dlen; i++ )
	{
		outDomain[i] = (char) tolower( outDomain[i] );  // canonicalize -> lower case
	}

	// attrs are reserved, so initialize to zeroes.

	ZeroMemory( &attrs, sizeof( attrs ) );

	// Get a handle to the Policy object on the local system

	res = LsaOpenPolicy( NULL, &attrs, POLICY_GET_PRIVATE_INFORMATION, &handle );
	err = translate_errno( res == 0, LsaNtStatusToWinError( res ), kUnknownErr );
	require_noerr( err, exit );

	// Get the encrypted data

	domainLSA = ( PLSA_UNICODE_STRING ) malloc( sizeof( LSA_UNICODE_STRING ) );
	require_action( domainLSA != NULL, exit, err = mStatus_NoMemoryErr );
	err = MakeLsaStringFromUTF8String( domainLSA, outDomain );
	require_noerr( err, exit );

	// Retrieve the key

	res = LsaRetrievePrivateData( handle, domainLSA, &keyLSA );
	err = translate_errno( res == 0, LsaNtStatusToWinError( res ), kUnknownErr );
	require_noerr_quiet( err, exit );

	// <rdar://problem/4192119> Lsa secrets use a flat naming space.  Therefore, we will prepend "$" to the keyname to
	// make sure it doesn't conflict with a zone name.	
	// Strip off the "$" prefix.

	err = MakeUTF8StringFromLsaString( outKey, outKeySize, keyLSA );
	require_noerr( err, exit );
	require_action( outKey[0] == '$', exit, err = kUnknownErr );
	memcpy( outKey, outKey + 1, strlen( outKey ) );

	// Retrieve the secret

	res = LsaRetrievePrivateData( handle, keyLSA, &secretLSA );
	err = translate_errno( res == 0, LsaNtStatusToWinError( res ), kUnknownErr );
	require_noerr_quiet( err, exit );

	// Convert the secret to UTF8 string

	err = MakeUTF8StringFromLsaString( outSecret, outSecretSize, secretLSA );
	require_noerr( err, exit );

exit:

	if ( domainLSA != NULL )
	{
		if ( domainLSA->Buffer != NULL )
		{
			free( domainLSA->Buffer );
		}

		free( domainLSA );
	}

	if ( keyLSA != NULL )
	{
		LsaFreeMemory( keyLSA );
	}

	if ( secretLSA != NULL )
	{
		LsaFreeMemory( secretLSA );
	}

	if ( handle )
	{
		LsaClose( handle );
		handle = NULL;
	}

	return ( !err ) ? TRUE : FALSE;
}


mDNSBool
LsaSetSecret( const char * inDomain, const char * inKey, const char * inSecret )
{
	size_t					inDomainLength;
	size_t					inKeyLength;
	char					domain[ 1024 ];
	char					key[ 1024 ];
	LSA_OBJECT_ATTRIBUTES	attrs;
	LSA_HANDLE				handle = NULL;
	NTSTATUS				res;
	LSA_UNICODE_STRING		lucZoneName;
	LSA_UNICODE_STRING		lucKeyName;
	LSA_UNICODE_STRING		lucSecretName;
	BOOL					ok = TRUE;
	OSStatus				err;

	require_action( inDomain != NULL, exit, ok = FALSE );
	require_action( inKey != NULL, exit, ok = FALSE );
	require_action( inSecret != NULL, exit, ok = FALSE );

	// If there isn't a trailing dot, add one because the mDNSResponder
	// presents names with the trailing dot.

	ZeroMemory( domain, sizeof( domain ) );
	inDomainLength = strlen( inDomain );
	require_action( inDomainLength > 0, exit, ok = FALSE );
	err = strcpy_s( domain, sizeof( domain ) - 2, inDomain );
	require_action( !err, exit, ok = FALSE );

	if ( domain[ inDomainLength - 1 ] != '.' )
	{
		domain[ inDomainLength++ ] = '.';
		domain[ inDomainLength ] = '\0';
	}

	// <rdar://problem/4192119>
	//
	// Prepend "$" to the key name, so that there will
	// be no conflict between the zone name and the key
	// name

	ZeroMemory( key, sizeof( key ) );
	inKeyLength = strlen( inKey );
	require_action( inKeyLength > 0 , exit, ok = FALSE );
	key[ 0 ] = '$';
	err = strcpy_s( key + 1, sizeof( key ) - 3, inKey );
	require_action( !err, exit, ok = FALSE );
	inKeyLength++;

	if ( key[ inKeyLength - 1 ] != '.' )
	{
		key[ inKeyLength++ ] = '.';
		key[ inKeyLength ] = '\0';
	}

	// attrs are reserved, so initialize to zeroes.

	ZeroMemory( &attrs, sizeof( attrs ) );

	// Get a handle to the Policy object on the local system

	res = LsaOpenPolicy( NULL, &attrs, POLICY_ALL_ACCESS, &handle );
	err = translate_errno( res == 0, LsaNtStatusToWinError( res ), kUnknownErr );
	require_noerr( err, exit );

	// Intializing PLSA_UNICODE_STRING structures

	err = MakeLsaStringFromUTF8String( &lucZoneName, domain );
	require_noerr( err, exit );

	err = MakeLsaStringFromUTF8String( &lucKeyName, key );
	require_noerr( err, exit );

	err = MakeLsaStringFromUTF8String( &lucSecretName, inSecret );
	require_noerr( err, exit );

	// Store the private data.

	res = LsaStorePrivateData( handle, &lucZoneName, &lucKeyName );
	err = translate_errno( res == 0, LsaNtStatusToWinError( res ), kUnknownErr );
	require_noerr( err, exit );

	res = LsaStorePrivateData( handle, &lucKeyName, &lucSecretName );
	err = translate_errno( res == 0, LsaNtStatusToWinError( res ), kUnknownErr );
	require_noerr( err, exit );

exit:

	if ( handle )
	{
		LsaClose( handle );
		handle = NULL;
	}

	return ok;
}


//===========================================================================================================================
//	MakeLsaStringFromUTF8String
//===========================================================================================================================

mDNSlocal OSStatus
MakeLsaStringFromUTF8String( PLSA_UNICODE_STRING output, const char * input )
{
	int			size;
	OSStatus	err;
	
	check( input );
	check( output );

	output->Buffer = NULL;

	size = MultiByteToWideChar( CP_UTF8, 0, input, -1, NULL, 0 );
	err = translate_errno( size > 0, GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	output->Length = (USHORT)( size * sizeof( wchar_t ) );
	output->Buffer = (PWCHAR) malloc( output->Length );
	require_action( output->Buffer, exit, err = mStatus_NoMemoryErr );
	size = MultiByteToWideChar( CP_UTF8, 0, input, -1, output->Buffer, size );
	err = translate_errno( size > 0, GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	// We're going to subtrace one wchar_t from the size, because we didn't
	// include it when we encoded the string

	output->MaximumLength = output->Length;
	output->Length		-= sizeof( wchar_t );
	
exit:

	if ( err && output->Buffer )
	{
		free( output->Buffer );
		output->Buffer = NULL;
	}

	return( err );
}



//===========================================================================================================================
//	MakeUTF8StringFromLsaString
//===========================================================================================================================

mDNSlocal OSStatus
MakeUTF8StringFromLsaString( char * output, size_t len, PLSA_UNICODE_STRING input )
{
	size_t		size;
	OSStatus	err = kNoErr;

	// The Length field of this structure holds the number of bytes,
	// but WideCharToMultiByte expects the number of wchar_t's. So
	// we divide by sizeof(wchar_t) to get the correct number.

	size = (size_t) WideCharToMultiByte(CP_UTF8, 0, input->Buffer, ( input->Length / sizeof( wchar_t ) ), NULL, 0, NULL, NULL);
	err = translate_errno( size != 0, GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
	// Ensure that we have enough space (Add one for trailing '\0')

	require_action( ( size + 1 ) <= len, exit, err = mStatus_NoMemoryErr );

	// Convert the string

	size = (size_t) WideCharToMultiByte( CP_UTF8, 0, input->Buffer, ( input->Length / sizeof( wchar_t ) ), output, (int) size, NULL, NULL);	
	err = translate_errno( size != 0, GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	// have to add the trailing 0 because WideCharToMultiByte doesn't do it,
	// although it does return the correct size

	output[size] = '\0';

exit:

	return err;
}

