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

$Log: ConfigPropertySheet.cpp,v $
Revision 1.7  2009/07/01 19:20:37  herscher
<rdar://problem/6713286> UI changes for configuring sleep proxy settings.

Revision 1.6  2009/03/30 19:57:45  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/5187308> Move build train to Visual Studio 2005

Revision 1.5  2006/08/14 23:25:28  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.4  2005/10/05 20:46:50  herscher
<rdar://problem/4192011> Move Wide-Area preferences to another part of the registry so they don't removed during an update-install.

Revision 1.3  2005/03/03 19:55:22  shersche
<rdar://problem/4034481> ControlPanel source code isn't saving CVS log info


*/

#include "ConfigPropertySheet.h"
#include <WinServices.h>
#include <process.h>

// Custom events

#define WM_DATAREADY		( WM_USER + 0x100 )
#define WM_REGISTRYCHANGED	( WM_USER + 0x101 )


IMPLEMENT_DYNCREATE(CConfigPropertySheet, CPropertySheet)


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::CConfigPropertySheet
//---------------------------------------------------------------------------------------------------------------------------

CConfigPropertySheet::CConfigPropertySheet()
:
	CPropertySheet(),
	m_browseDomainsRef( NULL ),
	m_regDomainsRef( NULL ),
	m_thread( NULL ),
	m_threadExited( NULL )
{
	AddPage(&m_firstPage);
	AddPage(&m_secondPage);
	AddPage(&m_thirdPage);
	AddPage(&m_fourthPage );

	InitializeCriticalSection( &m_lock );
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::~CConfigPropertySheet
//---------------------------------------------------------------------------------------------------------------------------

CConfigPropertySheet::~CConfigPropertySheet()
{
	DeleteCriticalSection( &m_lock );
}


BEGIN_MESSAGE_MAP(CConfigPropertySheet, CPropertySheet)
	//{{AFX_MSG_MAP(CConfigPropertySheet)
	//}}AFX_MSG_MAP
	ON_MESSAGE( WM_DATAREADY, OnDataReady )
	ON_MESSAGE( WM_REGISTRYCHANGED, OnRegistryChanged )
END_MESSAGE_MAP()


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::OnInitDialog
//---------------------------------------------------------------------------------------------------------------------------

BOOL
CConfigPropertySheet::OnInitDialog()
{
	OSStatus err;

	BOOL b = CPropertySheet::OnInitDialog();

	err = SetupBrowsing();
	require_noerr( err, exit );

	err = SetupRegistryNotifications();
	require_noerr( err, exit );	

exit:

	return b;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::OnCommand
//---------------------------------------------------------------------------------------------------------------------------

BOOL
CConfigPropertySheet::OnCommand(WPARAM wParam, LPARAM lParam)
{
   // Check if OK or Cancel was hit

   if ( ( wParam == ID_WIZFINISH ) || ( wParam == IDOK ) || ( wParam == IDCANCEL ) ) 
   {
      OnEndDialog();
   }

   return CPropertySheet::OnCommand(wParam, lParam);
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::OnDataReady
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CConfigPropertySheet::OnDataReady(WPARAM inWParam, LPARAM inLParam)
{
	if (WSAGETSELECTERROR(inLParam) && !(HIWORD(inLParam)))
	{
		dlog( kDebugLevelError, "OnSocket: window error\n" );
	}
	else
	{
		SOCKET sock = (SOCKET) inWParam;

		if ( m_browseDomainsRef && DNSServiceRefSockFD( m_browseDomainsRef ) == (int) sock )
		{
			DNSServiceProcessResult( m_browseDomainsRef );
		}
		else if ( m_regDomainsRef && DNSServiceRefSockFD( m_regDomainsRef ) == (int) sock )
		{
			DNSServiceProcessResult( m_regDomainsRef );
		}
	}

	return 0;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::OnRegistryChanged
//---------------------------------------------------------------------------------------------------------------------------

afx_msg LRESULT
CConfigPropertySheet::OnRegistryChanged( WPARAM inWParam, LPARAM inLParam )
{
	DEBUG_UNUSED( inWParam );
	DEBUG_UNUSED( inLParam );

	if ( GetActivePage() == &m_firstPage )
	{
		m_firstPage.OnRegistryChanged();
	}

	return 0;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::OnEndDialog
//---------------------------------------------------------------------------------------------------------------------------

void
CConfigPropertySheet::OnEndDialog()
{
	OSStatus err;

	err = TearDownRegistryNotifications();
	check_noerr( err );

	err = TearDownBrowsing();
	check_noerr( err );
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::SetupBrowsing
//---------------------------------------------------------------------------------------------------------------------------

OSStatus
CConfigPropertySheet::SetupBrowsing()
{
	OSStatus err;

	// Start browsing for browse domains

	err = DNSServiceEnumerateDomains( &m_browseDomainsRef, kDNSServiceFlagsBrowseDomains, 0, BrowseDomainsReply, this );
	require_noerr( err, exit );

	err = WSAAsyncSelect( DNSServiceRefSockFD( m_browseDomainsRef ), m_hWnd, WM_DATAREADY, FD_READ|FD_CLOSE );
	require_noerr( err, exit );

	// Start browsing for registration domains

	err = DNSServiceEnumerateDomains( &m_regDomainsRef, kDNSServiceFlagsRegistrationDomains, 0, RegDomainsReply, this );
	require_noerr( err, exit );

	err = WSAAsyncSelect( DNSServiceRefSockFD( m_regDomainsRef ), m_hWnd, WM_DATAREADY, FD_READ|FD_CLOSE );
	require_noerr( err, exit );

exit:

	if ( err )
	{
		TearDownBrowsing();
	}

	return err;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::TearDownBrowsing
//---------------------------------------------------------------------------------------------------------------------------

OSStatus
CConfigPropertySheet::TearDownBrowsing()
{
	OSStatus err = kNoErr;

	if ( m_browseDomainsRef )
	{
		err = WSAAsyncSelect( DNSServiceRefSockFD( m_browseDomainsRef ), m_hWnd, 0, 0 );
		check_noerr( err );

		DNSServiceRefDeallocate( m_browseDomainsRef );
	
		m_browseDomainsRef = NULL;
	}

	if ( m_regDomainsRef )
	{
		err = WSAAsyncSelect( DNSServiceRefSockFD( m_regDomainsRef ), m_hWnd, 0, 0 );
		check_noerr( err );

		DNSServiceRefDeallocate( m_regDomainsRef );
	
		m_regDomainsRef = NULL;
	}

	return err;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::SetupRegistryNotifications
//---------------------------------------------------------------------------------------------------------------------------

OSStatus
CConfigPropertySheet::SetupRegistryNotifications()
{
	unsigned int	threadId;
	OSStatus		err;

	check( m_threadExited == NULL );
	check( m_thread == NULL );

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode L"\\DynDNS\\State\\Hostnames", &m_statusKey );
	require_noerr( err, exit );
	
	m_threadExited = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( m_threadExited, (OSStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	// Create thread with _beginthreadex() instead of CreateThread() to avoid memory leaks when using static run-time 
	// libraries. See <http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/createthread.asp>.
	
	m_thread = (HANDLE) _beginthreadex_compat( NULL, 0, WatchRegistry, this, 0, &threadId );
	err = translate_errno( m_thread, (OSStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

exit:

	if ( err )
	{
		TearDownRegistryNotifications();
	}

	return err;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::TearDownRegistryNotifications
//---------------------------------------------------------------------------------------------------------------------------

OSStatus
CConfigPropertySheet::TearDownRegistryNotifications()
{
	OSStatus err = kNoErr;

	if ( m_statusKey )
	{
		EnterCriticalSection( &m_lock );

		RegCloseKey( m_statusKey );
		m_statusKey = NULL;

		LeaveCriticalSection( &m_lock );
	}

	if ( m_threadExited )
	{
		err = WaitForSingleObject( m_threadExited, 5 * 1000 );
		require_noerr( err, exit );
	}

exit:

	if ( m_threadExited )
	{
		CloseHandle( m_threadExited );
		m_threadExited = NULL;
	}

	if ( m_thread )
	{
		CloseHandle( m_thread );
		m_thread = NULL;
	}

	return err;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::DecodeDomainName
//---------------------------------------------------------------------------------------------------------------------------

OSStatus
CConfigPropertySheet::DecodeDomainName( const char * raw, CString & decoded )
{
	char nextLabel[128] = "\0";
	char decodedDomainString[kDNSServiceMaxDomainName];
    char * buffer = (char *) raw;
    int labels = 0, i;
    char text[64];
	const char *label[128];
	OSStatus	err;
    
	// Initialize

	decodedDomainString[0] = '\0';

    // Count the labels

	while ( *buffer )
	{
		label[labels++] = buffer;
		buffer = (char *) GetNextLabel(buffer, text);
    }
        
    buffer = (char*) raw;

    for (i = 0; i < labels; i++)
	{
		buffer = (char *)GetNextLabel(buffer, nextLabel);
        strcat(decodedDomainString, nextLabel);
        strcat(decodedDomainString, ".");
    }
    
    // Remove trailing dot from domain name.
    
	decodedDomainString[ strlen( decodedDomainString ) - 1 ] = '\0';

	// Convert to Unicode

	err = UTF8StringToStringObject( decodedDomainString, decoded );

	return err;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::GetNextLabel
//---------------------------------------------------------------------------------------------------------------------------

const char*
CConfigPropertySheet::GetNextLabel( const char * cstr, char label[64] )
{
	char *ptr = label;
	while (*cstr && *cstr != '.')								// While we have characters in the label...
		{
		char c = *cstr++;
		if (c == '\\')
			{
			c = *cstr++;
			if (isdigit(cstr[-1]) && isdigit(cstr[0]) && isdigit(cstr[1]))
				{
				int v0 = cstr[-1] - '0';						// then interpret as three-digit decimal
				int v1 = cstr[ 0] - '0';
				int v2 = cstr[ 1] - '0';
				int val = v0 * 100 + v1 * 10 + v2;
				if (val <= 255) { c = (char)val; cstr += 2; }	// If valid three-digit decimal value, use it
				}
			}
		*ptr++ = c;
		if (ptr >= label+64) return(NULL);
		}
	if (*cstr) cstr++;											// Skip over the trailing dot (if present)
	*ptr++ = 0;
	return(cstr);
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::BrowseDomainsReply
//---------------------------------------------------------------------------------------------------------------------------

void DNSSD_API
CConfigPropertySheet::BrowseDomainsReply
							(
							DNSServiceRef			sdRef,
							DNSServiceFlags			flags,
							uint32_t				interfaceIndex,
							DNSServiceErrorType		errorCode,
							const char			*	replyDomain,
							void				*	context
							)
{
	CConfigPropertySheet	*	self = reinterpret_cast<CConfigPropertySheet*>(context);
	CString						decoded;
	OSStatus					err;

	DEBUG_UNUSED( sdRef );
	DEBUG_UNUSED( interfaceIndex );

	if ( errorCode )
	{
		goto exit;
	}

	check( replyDomain );
	
	// Ignore local domains

	if ( strcmp( replyDomain, "local." ) == 0 )
	{
		goto exit;
	}



	err = self->DecodeDomainName( replyDomain, decoded );
	require_noerr( err, exit );

	// Remove trailing '.'

	decoded.TrimRight( '.' );

	if ( flags & kDNSServiceFlagsAdd )
	{
		self->m_browseDomains.push_back( decoded );
	}
	else
	{
		self->m_browseDomains.remove( decoded );
	}

exit:

	return;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::RegDomainsReply
//---------------------------------------------------------------------------------------------------------------------------

void DNSSD_API
CConfigPropertySheet::RegDomainsReply
							(
							DNSServiceRef			sdRef,
							DNSServiceFlags			flags,
							uint32_t				interfaceIndex,
							DNSServiceErrorType		errorCode,
							const char			*	replyDomain,
							void				*	context
							)
{
	CConfigPropertySheet	*	self = reinterpret_cast<CConfigPropertySheet*>(context);
	CString						decoded;
	OSStatus					err;

	DEBUG_UNUSED( sdRef );
	DEBUG_UNUSED( interfaceIndex );

	if ( errorCode )
	{
		goto exit;
	}

	check( replyDomain );
	
	// Ignore local domains

	if ( strcmp( replyDomain, "local." ) == 0 )
	{
		goto exit;
	}

	err = self->DecodeDomainName( replyDomain, decoded );
	require_noerr( err, exit );

	// Remove trailing '.'

	decoded.TrimRight( '.' );

	if ( flags & kDNSServiceFlagsAdd )
	{
		if ( self->GetActivePage() == &self->m_secondPage )
		{
			self->m_secondPage.OnAddRegistrationDomain( decoded );
		}

		self->m_regDomains.push_back( decoded );
	}
	else
	{
		if ( self->GetActivePage() == &self->m_secondPage )
		{
			self->m_secondPage.OnRemoveRegistrationDomain( decoded );
		}

		self->m_regDomains.remove( decoded );
	}

exit:

	return;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet::WatchRegistry
//---------------------------------------------------------------------------------------------------------------------------

unsigned WINAPI
CConfigPropertySheet::WatchRegistry ( LPVOID inParam )
{
	bool done = false;

	CConfigPropertySheet * self = reinterpret_cast<CConfigPropertySheet*>(inParam);
	check( self );

	while ( !done )
	{
		RegNotifyChangeKeyValue( self->m_statusKey, TRUE, REG_NOTIFY_CHANGE_LAST_SET, NULL, FALSE );

		EnterCriticalSection( &self->m_lock );

		done = ( self->m_statusKey == NULL ) ? true : false;

		if ( !done )
		{
			self->PostMessage( WM_REGISTRYCHANGED, 0, 0 );
		}

		LeaveCriticalSection( &self->m_lock );
	}

	SetEvent( self->m_threadExited );

	return 0;
}
