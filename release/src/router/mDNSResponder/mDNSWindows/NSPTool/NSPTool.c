/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
	
$Log: NSPTool.c,v $
Revision 1.4  2006/08/14 23:26:06  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.3  2004/08/26 04:46:49  shersche
Add -q switch for silent operation

Revision 1.2  2004/06/23 16:39:14  shersche
Fix extraneous warnings regarding implict casts

Submitted by: Scott Herscher (sherscher@apple.com)

Revision 1.1  2004/06/18 04:14:26  rpantos
Move up one level.

Revision 1.1  2004/01/30 03:02:58  bradley
NameSpace Provider Tool for installing, removing, list, etc. NameSpace Providers.

*/

#include	<stdio.h>
#include	<stdlib.h>

#include	"CommonServices.h"
#include	"DebugServices.h"

#include	<guiddef.h>
#include	<ws2spi.h>

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

int  					main( int argc, char *argv[] );
DEBUG_LOCAL void		Usage( void );
DEBUG_LOCAL int			ProcessArgs( int argc, char *argv[] );
DEBUG_LOCAL OSStatus	InstallNSP( const char *inName, const char *inGUID, const char *inPath );
DEBUG_LOCAL OSStatus	RemoveNSP( const char *inGUID );
DEBUG_LOCAL OSStatus	EnableNSP( const char *inGUID, BOOL inEnable );
DEBUG_LOCAL OSStatus	ListNameSpaces( void );
DEBUG_LOCAL OSStatus	ReorderNameSpaces( void );

DEBUG_LOCAL WCHAR *		CharToWCharString( const char *inCharString, WCHAR *outWCharString );
DEBUG_LOCAL char *		GUIDtoString( const GUID *inGUID, char *outString );
DEBUG_LOCAL OSStatus	StringToGUID( const char *inCharString, GUID *outGUID );

DEBUG_LOCAL BOOL gToolQuietMode = FALSE;

//===========================================================================================================================
//	main
//===========================================================================================================================

int main( int argc, char *argv[] )
{
	OSStatus		err;
	
	debug_initialize( kDebugOutputTypeMetaConsole );
	debug_set_property( kDebugPropertyTagPrintLevel, kDebugLevelVerbose );
	
	err = ProcessArgs( argc, argv );
	return( (int) err );
}

//===========================================================================================================================
//	Usage
//===========================================================================================================================

DEBUG_LOCAL void	Usage( void )
{
	fprintf( stderr, "\n" );
	fprintf( stderr, "NSP Tool 1.0d1\n" );
	fprintf( stderr, "  Name Space Provider Tool\n" );
	fprintf( stderr, "\n" );
	
	fprintf( stderr, "  -install <name> <guid> <path>   - Installs a Name Space Provider\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "      <name> Name of the NSP\n" );
	fprintf( stderr, "      <guid> GUID of the NSP\n" );
	fprintf( stderr, "      <path> Path to the NSP file\n" );
	fprintf( stderr, "\n" );
	
	fprintf( stderr, "  -remove <guid>                  - Removes a Name Space Provider\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "      <guid> GUID of the NSP\n" );
	fprintf( stderr, "\n" );
	
	fprintf( stderr, "  -enable/-disable <guid>         - Enables or Disables a Name Space Provider\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "      <guid> GUID of the NSP\n" );
	fprintf( stderr, "\n" );
	
	fprintf( stderr, "  -list                           - Lists Name Space Providers\n" );	
	fprintf( stderr, "  -reorder                        - Reorders Name Space Providers\n" );
	fprintf( stderr, "  -q                              - Enable quiet mode\n" );
	fprintf( stderr, "  -h[elp]                         - Help\n" );
	fprintf( stderr, "\n" );
}

//===========================================================================================================================
//	ProcessArgs
//===========================================================================================================================

DEBUG_LOCAL int ProcessArgs( int argc, char* argv[] )
{	
	OSStatus			err;
	int					i;
	const char *		name;
	const char *		guid;
	const char *		path;
			
	if( argc <= 1 )
	{
		Usage();
		err = 0;
		goto exit;
	}
	for( i = 1; i < argc; ++i )
	{
		if( strcmp( argv[ i ], "-install" ) == 0 )
		{
			// Install
			
			if( argc <= ( i + 3 ) )
			{
				fprintf( stderr, "\n### ERROR: missing arguments for %s\n\n", argv[ i ] );
				Usage();
				err = kParamErr;
				goto exit;
			}
			name = argv[ ++i ];
			guid = argv[ ++i ];
			path = argv[ ++i ];
			
			if( *name == '\0' )
			{
				name = "DotLocalNSP";
			}
			if( *guid == '\0' )
			{
				guid = "B600E6E9-553B-4a19-8696-335E5C896153";
			}
			
			err = InstallNSP( name, guid, path );
			require_noerr( err, exit );
		}
		else if( strcmp( argv[ i ], "-remove" ) == 0 )
		{
			// Remove
			
			if( argc <= ( i + 1 ) )
			{
				fprintf( stderr, "\n### ERROR: missing arguments for %s\n\n", argv[ i ] );
				Usage();
				err = kParamErr;
				goto exit;
			}
			guid = argv[ ++i ];
			if( *guid == '\0' )
			{
				guid = "B600E6E9-553B-4a19-8696-335E5C896153";
			}
			
			err = RemoveNSP( guid );
			require_noerr( err, exit );
		}
		else if( ( strcmp( argv[ i ], "-enable" )  == 0 ) || 
				 ( strcmp( argv[ i ], "-disable" ) == 0 ) )
		{
			BOOL		enable;
			
			// Enable/Disable
			
			enable = ( strcmp( argv[ i ], "-enable" ) == 0 );
			if( argc <= ( i + 1 ) )
			{
				fprintf( stderr, "\n### ERROR: missing arguments for %s\n\n", argv[ i ] );
				Usage();
				err = kParamErr;
				goto exit;
			}
			guid = argv[ ++i ];
			
			err = EnableNSP( guid, enable );
			require_noerr( err, exit );
		}
		else if( strcmp( argv[ i ], "-list" ) == 0 )
		{
			// List
						
			err = ListNameSpaces();
			require_noerr( err, exit );
		}
		else if( strcmp( argv[ i ], "-reorder" ) == 0 )
		{
			// Reorder
			
			err = ReorderNameSpaces();
			require_noerr( err, exit );
		}
		else if( strcmp( argv[ i ], "-q" ) == 0 )
		{
			gToolQuietMode = TRUE;
		}
		else if( ( strcmp( argv[ i ], "-help" ) == 0 ) || 
				 ( strcmp( argv[ i ], "-h" ) == 0 ) )
		{
			// Help
			
			Usage();
			err = 0;
			goto exit;
		}
		else
		{
			fprintf( stderr, "\n### ERROR: unknown argment: \"%s\"\n\n", argv[ i ] );
			Usage();
			err = kParamErr;
			goto exit;
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	InstallNSP
//===========================================================================================================================

OSStatus	InstallNSP( const char *inName, const char *inGUID, const char *inPath )
{
	OSStatus		err;
	size_t			size;
	WSADATA			wsd;
	WCHAR			name[ 256 ];
	GUID			guid;
	WCHAR			path[ MAX_PATH ];
	
	require_action( inName && ( *inName != '\0' ), exit, err = kParamErr );
	require_action( inGUID && ( *inGUID != '\0' ), exit, err = kParamErr );
	require_action( inPath && ( *inPath != '\0' ), exit, err = kParamErr );
	
	size = strlen( inName );
	require_action( size < sizeof_array( name ), exit, err = kSizeErr );
	CharToWCharString( inName, name );
	
	err = StringToGUID( inGUID, &guid );
	require_noerr( err, exit );
	
	size = strlen( inPath );
	require_action( size < sizeof_array( path ), exit, err = kSizeErr );
	CharToWCharString( inPath, path );
	
	err = WSAStartup( MAKEWORD( 2, 2 ), &wsd );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	require_noerr( err, exit );
	
	err = WSCInstallNameSpace( name, path, NS_DNS, 1, &guid );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	WSACleanup();
	require_noerr( err, exit );
	
	if (!gToolQuietMode)
	{
		fprintf( stderr, "Installed NSP \"%s\" (%s) at %s\n", inName, inGUID, inPath );
	}
	
exit:
	if( err != kNoErr )
	{
		fprintf( stderr, "### FAILED (%d) to install \"%s\" (%s) Name Space Provider at %s\n", err, inName, inGUID, inPath );
	}
	return( err );
}

//===========================================================================================================================
//	RemoveNSP
//===========================================================================================================================

DEBUG_LOCAL OSStatus	RemoveNSP( const char *inGUID )
{
	OSStatus		err;
	WSADATA			wsd;
	GUID			guid;
	
	require_action( inGUID && ( *inGUID != '\0' ), exit, err = kParamErr );
	
	err = StringToGUID( inGUID, &guid );
	require_noerr( err, exit );
	
	err = WSAStartup( MAKEWORD( 2, 2 ), &wsd );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	require_noerr( err, exit );
	
	err = WSCUnInstallNameSpace( &guid );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	WSACleanup();
	require_noerr( err, exit );
	
	if (!gToolQuietMode)
	{
		fprintf( stderr, "Removed NSP %s\n", inGUID );
	}
		
exit:
	if( err != kNoErr )
	{
		fprintf( stderr, "### FAILED (%d) to remove %s Name Space Provider\n", err, inGUID );
	}
	return( err );
}

//===========================================================================================================================
//	EnableNSP
//===========================================================================================================================

DEBUG_LOCAL OSStatus	EnableNSP( const char *inGUID, BOOL inEnable )
{
	OSStatus		err;
	WSADATA			wsd;
	GUID			guid;
	
	require_action( inGUID && ( *inGUID != '\0' ), exit, err = kParamErr );
	
	err = StringToGUID( inGUID, &guid );
	require_noerr( err, exit );
	
	err = WSAStartup( MAKEWORD( 2, 2 ), &wsd );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	require_noerr( err, exit );
	
	err = WSCEnableNSProvider( &guid, inEnable );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	WSACleanup();
	require_noerr( err, exit );
	
	if (!gToolQuietMode)
	{
		fprintf( stderr, "Removed NSP %s\n", inGUID );
	}
		
exit:
	if( err != kNoErr )
	{
		fprintf( stderr, "### FAILED (%d) to remove %s Name Space Provider\n", err, inGUID );
	}
	return( err );
}

//===========================================================================================================================
//	ListNameSpaces
//===========================================================================================================================

DEBUG_LOCAL OSStatus	ListNameSpaces( void )
{
	OSStatus				err;
	WSADATA					wsd;
	bool					started;
	int						n;
	int						i;
	DWORD					size;
	WSANAMESPACE_INFO *		array;
	char					s[ 256 ];
	
	array 	= NULL;
	started	= false;
	
	err = WSAStartup( MAKEWORD( 2, 2 ), &wsd );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	require_noerr( err, exit );
	started = true;
	
	// Build an array of all the NSPs. Call it first with NULL to get the size, allocate a buffer, then get them into it.
	
	size = 0;
	n = WSAEnumNameSpaceProviders( &size, NULL );
	err = translate_errno( n != SOCKET_ERROR, (OSStatus) GetLastError(), kUnknownErr );
	require_action( err == WSAEFAULT, exit, err = kUnknownErr );
	
	array = (WSANAMESPACE_INFO *) malloc( size );
	require_action( array, exit, err = kNoMemoryErr );
	
	n = WSAEnumNameSpaceProviders( &size, array );
	err = translate_errno( n != SOCKET_ERROR, (OSStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
	fprintf( stdout, "\n" );
	for( i = 0; i < n; ++i )
	{
		fprintf( stdout, "Name Space %d\n", i + 1 );
		fprintf( stdout, "    NSProviderId:   %s\n", GUIDtoString( &array[ i ].NSProviderId, s ) );
		fprintf( stdout, "    dwNameSpace:    %d\n", array[ i ].dwNameSpace );
		fprintf( stdout, "    fActive:        %s\n", array[ i ].fActive ? "YES" : "NO" );
		fprintf( stdout, "    dwVersion:      %d\n", array[ i ].dwVersion );
		fprintf( stdout, "    lpszIdentifier: \"%s\"\n", array[ i ].lpszIdentifier );
		fprintf( stdout, "\n" );
	}
	err = kNoErr;
	
exit:
	if( array )
	{
		free( array );
	}
	if( started )
	{
		WSACleanup();
	}
	if( err != kNoErr )
	{
		fprintf( stderr, "### FAILED (%d) to list Name Space Providers\n", err );
	}
	return( err );
}

//===========================================================================================================================
//	ReorderNameSpaces
//===========================================================================================================================

DEBUG_LOCAL OSStatus	ReorderNameSpaces( void )
{
	OSStatus				err;
	WSADATA					wsd;
	bool					started;
	int						n;
	int						i;
	DWORD					size;
	WSANAMESPACE_INFO *		array;
	WCHAR					name[ 256 ];
	WCHAR					path[ MAX_PATH ];
	
	array 	= NULL;
	started	= false;
		
	err = WSAStartup( MAKEWORD( 2, 2 ), &wsd );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	require_noerr( err, exit );
	started = true;
	
	// Build an array of all the NSPs. Call it first with NULL to get the size, allocate a buffer, then get them into it.
	
	size = 0;
	n = WSAEnumNameSpaceProviders( &size, NULL );
	err = translate_errno( n != SOCKET_ERROR, (OSStatus) GetLastError(), kUnknownErr );
	require_action( err == WSAEFAULT, exit, err = kUnknownErr );
	
	array = (WSANAMESPACE_INFO *) malloc( size );
	require_action( array, exit, err = kNoMemoryErr );
	
	n = WSAEnumNameSpaceProviders( &size, array );
	err = translate_errno( n != SOCKET_ERROR, (OSStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
	// Find the "Tcpip" NSP.
	
	for( i = 0; i < n; ++i )
	{
		if( strcmp( array[ i ].lpszIdentifier, "Tcpip" ) == 0 )
		{
			break;
		}
	}
	require_action( i < n, exit, err = kNotFoundErr );
	
	// Uninstall it then re-install it to move it to the end.
	
	size = (DWORD) strlen( array[ i ].lpszIdentifier );
	require_action( size < sizeof_array( name ), exit, err = kSizeErr );
	CharToWCharString( array[ i ].lpszIdentifier, name );
	
	size = (DWORD) strlen( "%SystemRoot%\\System32\\mswsock.dll" );
	require_action( size < sizeof_array( path ), exit, err = kSizeErr );
	CharToWCharString( "%SystemRoot%\\System32\\mswsock.dll", path );
	
	err = WSCUnInstallNameSpace( &array[ i ].NSProviderId );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	require_noerr( err, exit );
	
	err = WSCInstallNameSpace( name, path, NS_DNS, array[ i ].dwVersion, &array[ i ].NSProviderId );
	err = translate_errno( err == 0, errno_compat(), WSAEINVAL );
	require_noerr( err, exit );
		
	// Success!
	
	fprintf( stderr, "Reordered \"Tcpip\" NSP to to the bottom of the NSP chain\n" );	
	err = kNoErr;
	
exit:
	if( array )
	{
		free( array );
	}
	if( started )
	{
		WSACleanup();
	}
	if( err != kNoErr )
	{
		fprintf( stderr, "### FAILED (%d) to reorder Name Space Providers\n", err );
	}
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CharToWCharString
//===========================================================================================================================

DEBUG_LOCAL WCHAR *	CharToWCharString( const char *inCharString, WCHAR *outWCharString )
{
	const char *		src;
	WCHAR *				dst;
	char				c;
	
	check( inCharString );
	check( outWCharString );
	
	src = inCharString;
	dst = outWCharString;
	do
	{
		c = *src++;
		*dst++ = (WCHAR) c;
	
	}	while( c != '\0' );
	
	return( outWCharString );
}

//===========================================================================================================================
//	GUIDtoString
//===========================================================================================================================

DEBUG_LOCAL char *	GUIDtoString( const GUID *inGUID, char *outString )
{
	check( inGUID );
	check( outString );
	
	sprintf( outString, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", 
		inGUID->Data1, inGUID->Data2, inGUID->Data3, 
		inGUID->Data4[ 0 ], inGUID->Data4[ 1 ], inGUID->Data4[ 2 ], inGUID->Data4[ 3 ], 
		inGUID->Data4[ 4 ], inGUID->Data4[ 5 ], inGUID->Data4[ 6 ], inGUID->Data4[ 7 ] );
	return( outString );
}

//===========================================================================================================================
//	StringToGUID
//===========================================================================================================================

DEBUG_LOCAL OSStatus	StringToGUID( const char *inCharString, GUID *outGUID )
{
	OSStatus			err;
	int					n;
	unsigned int		v[ 8 ];
	
	check( inCharString );
	check( outGUID );
	
	n = sscanf( inCharString, "%lX-%hX-%hX-%02X%02X-%02X%02X%02X%02X%02X%02X", 
		&outGUID->Data1, &outGUID->Data2, &outGUID->Data3, 
		&v[ 0 ], &v[ 1 ], &v[ 2 ], &v[ 3 ], &v[ 4 ], &v[ 5 ], &v[ 6 ], &v[ 7 ] );
	require_action( n == 11, exit, err = kFormatErr );
	
	outGUID->Data4[ 0 ] = (unsigned char) v[ 0 ];
	outGUID->Data4[ 1 ] = (unsigned char) v[ 1 ];
	outGUID->Data4[ 2 ] = (unsigned char) v[ 2 ];
	outGUID->Data4[ 3 ] = (unsigned char) v[ 3 ];
	outGUID->Data4[ 4 ] = (unsigned char) v[ 4 ];
	outGUID->Data4[ 5 ] = (unsigned char) v[ 5 ];
	outGUID->Data4[ 6 ] = (unsigned char) v[ 6 ];
	outGUID->Data4[ 7 ] = (unsigned char) v[ 7 ];
	err = kNoErr;

exit:
	return( err );
}
