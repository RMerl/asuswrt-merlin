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
    
$Log: BrowserDialog.cpp,v $
Revision 1.4  2008/10/23 22:33:26  cheshire
Changed "NOTE:" to "Note:" so that BBEdit 9 stops putting those comment lines into the funtion popup menu

Revision 1.3  2006/08/14 23:25:55  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2004/07/13 21:24:27  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.1  2004/06/18 04:04:37  rpantos
Move up one level

Revision 1.5  2004/01/30 02:56:33  bradley
Updated to support full Unicode display. Added support for all services on www.dns-sd.org.

Revision 1.4  2003/10/16 09:21:56  bradley
Ignore non-IPv4 resolves until mDNS on Windows supports IPv6.

Revision 1.3  2003/10/14 03:28:50  bradley
Insert services in sorted order to make them easier to find. Defer service adds/removes to the main
thread to avoid potential problems with multi-threaded MFC message map access. Added some asserts.

Revision 1.2  2003/10/10 03:43:34  bradley
Added support for launching a web browser to go to the browsed web site on a single-tap.

Revision 1.1  2003/08/21 02:16:10  bradley
DNSServiceBrowser for HTTP services for Windows CE/PocketPC.

*/

#include	"stdafx.h"

#include	"Application.h"

#include	"DNSServices.h"

#include	"BrowserDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define	WM_USER_SERVICE_ADD			( WM_USER + 0x100 )
#define	WM_USER_SERVICE_REMOVE		( WM_USER + 0x101 )

//===========================================================================================================================
//	Message Map
//===========================================================================================================================

BEGIN_MESSAGE_MAP(BrowserDialog, CDialog)
	//{{AFX_MSG_MAP(BrowserDialog)
	ON_NOTIFY(NM_CLICK, IDC_BROWSE_LIST, OnBrowserListDoubleClick)
	ON_MESSAGE( WM_USER_SERVICE_ADD, OnServiceAdd )
	ON_MESSAGE( WM_USER_SERVICE_REMOVE, OnServiceRemove )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

static DWORD	UTF8StringToStringObject( const char *inUTF8, CString &inObject );

//===========================================================================================================================
//	BrowserDialog
//===========================================================================================================================

BrowserDialog::BrowserDialog( CWnd *inParent )
	: CDialog( BrowserDialog::IDD, inParent )
{
	//{{AFX_DATA_INIT(BrowserDialog)
		// Note: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32.
	
	mIcon = AfxGetApp()->LoadIcon( IDR_MAINFRAME );
	ASSERT( mIcon );
}

//===========================================================================================================================
//	DoDataExchange
//===========================================================================================================================

void	BrowserDialog::DoDataExchange( CDataExchange *pDX )
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(BrowserDialog)
	DDX_Control(pDX, IDC_BROWSE_LIST, mBrowserList);
	//}}AFX_DATA_MAP
}

//===========================================================================================================================
//	OnInitDialog
//===========================================================================================================================

BOOL	BrowserDialog::OnInitDialog()
{
	CString		s;
	
	CDialog::OnInitDialog();

	// Set the icon for this dialog. The framework does this automatically when the application's main window is not a dialog.
	
	SetIcon( mIcon, TRUE );		// Set big icon
	SetIcon( mIcon, FALSE );	// Set small icon
	
	CenterWindow( GetDesktopWindow() );

	// Set up the list.
	
	CRect		rect;
	
	s.LoadString( IDS_BROWSER_LIST_COLUMN_NAME );
	mBrowserList.GetWindowRect( rect );
	mBrowserList.InsertColumn( 0, s, LVCFMT_LEFT, rect.Width() - 8 );
	
	// Start browsing for services.

	DNSStatus		err;

	err = DNSBrowserCreate( 0, OnBrowserCallBack, this, &mBrowser );
	if( err )
	{
		AfxMessageBox( IDP_SOCKETS_INIT_FAILED );
		goto exit;
	}
	
	err = DNSBrowserStartServiceSearch( mBrowser, kDNSBrowserFlagAutoResolve, "_http._tcp", NULL );
	if( err )
	{
		AfxMessageBox( IDP_SOCKETS_INIT_FAILED );
		goto exit;
	}
	
exit:
	return( TRUE );
}


//===========================================================================================================================
//	OnBrowserListDoubleClick
//===========================================================================================================================

void	BrowserDialog::OnBrowserListDoubleClick( NMHDR *pNMHDR, LRESULT *pResult ) 
{
	int		selectedItem;

	(void) pNMHDR;	// Unused

	selectedItem = mBrowserList.GetNextItem( -1, LVNI_SELECTED );
	if( selectedItem >= 0 )
	{
		BrowserEntry *		entry;
		CString				temp;
		CString				url;
		
		// Build the URL from the IP and optional TXT record.

		entry = &mBrowserEntries[ selectedItem ];
		url += "http://" + entry->ip;
		temp = entry->text;
		if( temp.Find( TEXT( "path=" ) ) == 0 )
		{
			temp.Delete( 0, 5 );
		}
		if( temp.Find( '/' ) != 0 )
		{
			url += '/';
		}
		url += temp;

		// Let the system open the URL in the correct app.
		
		SHELLEXECUTEINFO		info;

		info.cbSize			= sizeof( info );
		info.fMask 			= 0;
		info.hwnd 			= NULL;
		info.lpVerb 		= NULL;
		info.lpFile 		= url;
		info.lpParameters 	= NULL;
		info.lpDirectory 	= NULL;
		info.nShow 			= SW_SHOWNORMAL;
		info.hInstApp 		= NULL;

		ShellExecuteEx( &info );
	}
	*pResult = 0;
}

//===========================================================================================================================
//	OnBrowserCallBack [static]
//===========================================================================================================================

void
	BrowserDialog::OnBrowserCallBack( 
		void *					inContext, 
		DNSBrowserRef			inRef, 
		DNSStatus				inStatusCode,
		const DNSBrowserEvent *	inEvent )
{
	BrowserDialog *		dialog;
	BrowserEntry *		entry;
	BOOL				posted;
	
	DNS_UNUSED( inStatusCode );
	dialog = reinterpret_cast < BrowserDialog * > ( inContext );
	ASSERT( dialog );
	
	switch( inEvent->type )
	{
		case kDNSBrowserEventTypeResolved:
			if( inEvent->data.resolved->address.addressType == kDNSNetworkAddressTypeIPv4  )
			{
				char		ip[ 64 ];

				sprintf( ip, "%u.%u.%u.%u:%u", 
					inEvent->data.resolved->address.u.ipv4.addr.v8[ 0 ], 
					inEvent->data.resolved->address.u.ipv4.addr.v8[ 1 ], 
					inEvent->data.resolved->address.u.ipv4.addr.v8[ 2 ], 
					inEvent->data.resolved->address.u.ipv4.addr.v8[ 3 ], 
					( inEvent->data.resolved->address.u.ipv4.port.v8[ 0 ] << 8 ) | 
					  inEvent->data.resolved->address.u.ipv4.port.v8[ 1 ] );
				
				entry = new BrowserEntry;
				ASSERT( entry );
				if( entry )
				{
					UTF8StringToStringObject( inEvent->data.resolved->name, entry->name );
					UTF8StringToStringObject( ip, entry->ip );
					UTF8StringToStringObject( inEvent->data.resolved->textRecord, entry->text );
					
					posted = ::PostMessage( dialog->GetSafeHwnd(), WM_USER_SERVICE_ADD, 0, (LPARAM) entry );
					ASSERT( posted );
					if( !posted )
					{
						delete entry;
					}
				}
			}
			break;

		case kDNSBrowserEventTypeRemoveService:
			entry = new BrowserEntry;
			ASSERT( entry );
			if( entry )
			{
				UTF8StringToStringObject( inEvent->data.removeService.name, entry->name );
				
				posted = ::PostMessage( dialog->GetSafeHwnd(), WM_USER_SERVICE_REMOVE, 0, (LPARAM) entry );
				ASSERT( posted );
				if( !posted )
				{
					delete entry;
				}
			}
			break;
		
		default:
			break;
	}
}

//===========================================================================================================================
//	BrowserAddService
//===========================================================================================================================

LONG	BrowserDialog::OnServiceAdd( WPARAM inWParam, LPARAM inLParam )
{
	BrowserEntry *		entry;
	INT_PTR				lo;
	INT_PTR				hi;
	INT_PTR				mid;
	int					result;
	
	(void) inWParam;	// Unused
	
	entry = reinterpret_cast < BrowserEntry * > ( inLParam );
	ASSERT( entry );
	
	result 	= -1;
	mid		= 0;
	lo 		= 0;
	hi 		= mBrowserEntries.GetSize() - 1;
	while( lo <= hi )
	{
		mid = ( lo + hi ) / 2;
		result = entry->name.CompareNoCase( mBrowserEntries[ mid ].name );
		if( result == 0 )
		{
			break;
		}
		else if( result < 0 )
		{
			hi = mid - 1;
		}
		else
		{
			lo = mid + 1;
		}
	}
	if( result == 0 )
	{
		mBrowserEntries[ mid ].ip	= entry->ip;
		mBrowserEntries[ mid ].text	= entry->text;
	}
	else
	{
		if( result > 0 )
		{
			mid += 1;
		}
		mBrowserEntries.InsertAt( mid, *entry );
		mBrowserList.InsertItem( mid, entry->name );
	}
	delete entry;
	return( 0 );
}

//===========================================================================================================================
//	OnServiceRemove
//===========================================================================================================================

LONG	BrowserDialog::OnServiceRemove( WPARAM inWParam, LPARAM inLParam )
{
	BrowserEntry *		entry;
	INT_PTR				hi;
	INT_PTR				lo;
	INT_PTR				mid;
	int					result;

	(void) inWParam;	// Unused
	
	entry = reinterpret_cast < BrowserEntry * > ( inLParam );
	ASSERT( entry );
	
	result 	= -1;
	mid		= 0;
	lo 		= 0;
	hi 		= mBrowserEntries.GetSize() - 1;
	while( lo <= hi )
	{
		mid = ( lo + hi ) / 2;
		result = entry->name.CompareNoCase( mBrowserEntries[ mid ].name );
		if( result == 0 )
		{
			break;
		}
		else if( result < 0 )
		{
			hi = mid - 1;
		}
		else
		{
			lo = mid + 1;
		}
	}
	if( result == 0 )
	{
		mBrowserList.DeleteItem( mid );
		mBrowserEntries.RemoveAt( mid );
	}
	delete entry;
	return( 0 );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	UTF8StringToStringObject
//===========================================================================================================================

static DWORD	UTF8StringToStringObject( const char *inUTF8, CString &inObject )
{
	DWORD			err;
	int				n;
	wchar_t *		unicode;
	
	unicode = NULL;
	
	n = MultiByteToWideChar( CP_UTF8, 0, inUTF8, -1, NULL, 0 );
	if( n > 0 )
	{
		unicode = (wchar_t *) malloc( (size_t)( n * sizeof( wchar_t ) ) );
		if( !unicode ) { err = ERROR_INSUFFICIENT_BUFFER; goto exit; };
		
		n = MultiByteToWideChar( CP_UTF8, 0, inUTF8, -1, unicode, n );
		inObject = unicode;
	}
	else
	{
		inObject = "";
	}
	err = 0;
	
exit:
	if( unicode )
	{
		free( unicode );
	}
	return( err );
}
