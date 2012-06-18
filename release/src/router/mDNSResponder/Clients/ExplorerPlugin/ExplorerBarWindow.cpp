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
    
$Log: ExplorerBarWindow.cpp,v $
Revision 1.23  2009/03/30 18:47:40  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/5187308> Move build train to Visual Studio 2005

Revision 1.22  2006/08/14 23:24:00  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.21  2005/04/06 01:13:07  shersche
<rdar://problem/4066195> Use the product icon instead of globe icon for 'About' link.

Revision 1.20  2005/03/18 02:43:02  shersche
<rdar://problem/4046443> Use standard IE website icon for 'About Bonjour', only using globe icon if standard icon cannot be loaded

Revision 1.19  2005/03/16 03:46:27  shersche
<rdar://problem/4045657> Use Bonjour icon for all discovered sites

Revision 1.18  2005/02/26 01:24:05  shersche
Remove display lines in tree control

Revision 1.17  2005/02/25 19:57:30  shersche
<rdar://problem/4023323> Remove FTP browsing from plugin

Revision 1.16  2005/02/08 23:31:06  shersche
Move "About ..." item underneath WebSites, change icons for discovered sites and "About ..." item

Revision 1.15  2005/01/27 22:38:27  shersche
add About item to tree list

Revision 1.14  2005/01/25 17:55:39  shersche
<rdar://problem/3911084> Get bitmaps from non-localizable resource module
Bug #: 3911084

Revision 1.13  2005/01/06 21:13:09  shersche
<rdar://problem/3796779> Handle kDNSServiceErr_Firewall return codes
Bug #: 3796779

Revision 1.12  2004/10/26 00:56:03  cheshire
Use "#if 0" instead of commenting out code

Revision 1.11  2004/10/18 23:49:17  shersche
<rdar://problem/3841564> Remove trailing dot from hostname, because some flavors of Windows have difficulty parsing hostnames with a trailing dot.
Bug #: 3841564

Revision 1.10  2004/09/02 02:18:58  cheshire
Minor textual cleanup to improve readability

Revision 1.9  2004/09/02 02:11:56  cheshire
<rdar://problem/3783611> Fix incorrect testing of MoreComing flag

Revision 1.8  2004/07/26 05:47:31  shersche
use TXTRecord APIs, fix bug in locating service to be removed

Revision 1.7  2004/07/22 16:08:20  shersche
clean up debug print statements, re-enable code inadvertently commented out

Revision 1.6  2004/07/22 05:27:20  shersche
<rdar://problem/3735827> Check to make sure error isn't WSAEWOULDBLOCK when canceling browse
Bug #: 3735827

Revision 1.5  2004/07/20 06:49:18  shersche
clean up socket handling code

Revision 1.4  2004/07/13 21:24:21  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.3  2004/06/27 14:59:59  shersche
reference count service info to handle multi-homed hosts

Revision 1.2  2004/06/23 16:09:34  shersche
Add the resolve DNSServiceRef to list of extant refs.  This fixes the "doesn't resolve when double clicking" problem

Submitted by: Scott Herscher

Revision 1.1  2004/06/18 04:34:59  rpantos
Move to Clients from mDNSWindows

Revision 1.5  2004/04/15 01:00:05  bradley
Removed support for automatically querying for A/AAAA records when resolving names. Platforms
without .local name resolving support will need to manually query for A/AAAA records as needed.

Revision 1.4  2004/04/09 21:03:15  bradley
Changed port numbers to use network byte order for consistency with other platforms.

Revision 1.3  2004/04/08 09:43:43  bradley
Changed callback calling conventions to __stdcall so they can be used with C# delegates.

Revision 1.2  2004/02/21 04:36:19  bradley
Enable dot local name lookups now that the NSP is being installed.

Revision 1.1  2004/01/30 03:01:56  bradley
Explorer Plugin to browse for DNS-SD advertised Web and FTP servers from within Internet Explorer.

*/

#include	"StdAfx.h"

#include	"CommonServices.h"
#include	"DebugServices.h"
#include	"WinServices.h"
#include	"dns_sd.h"

#include	"ExplorerBar.h"
#include	"LoginDialog.h"
#include	"Resource.h"

#include	"ExplorerBarWindow.h"
#include	"ExplorerPlugin.h"

// MFC Debugging

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

// Control IDs

#define	IDC_EXPLORER_TREE				1234

// Private Messages

#define WM_PRIVATE_SERVICE_EVENT				( WM_USER + 0x100 )

// TXT records

#define	kTXTRecordKeyPath				"path"

// IE Icon resource

#define kIEIconResource					32529


#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

DEBUG_LOCAL int			FindServiceArrayIndex( const ServiceInfoArray &inArray, const ServiceInfo &inService, int &outIndex );

#if 0
#pragma mark == Message Map ==
#endif

//===========================================================================================================================
//	Message Map
//===========================================================================================================================

BEGIN_MESSAGE_MAP( ExplorerBarWindow, CWnd )
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_NOTIFY( NM_DBLCLK, IDC_EXPLORER_TREE, OnDoubleClick )
	ON_MESSAGE( WM_PRIVATE_SERVICE_EVENT, OnServiceEvent )
END_MESSAGE_MAP()

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	ExplorerBarWindow
//===========================================================================================================================

ExplorerBarWindow::ExplorerBarWindow( void )
{
	mOwner				= NULL;
	mResolveServiceRef	= NULL;
}

//===========================================================================================================================
//	~ExplorerBarWindow
//===========================================================================================================================

ExplorerBarWindow::~ExplorerBarWindow( void )
{
	//
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	OnCreate
//===========================================================================================================================

int	ExplorerBarWindow::OnCreate( LPCREATESTRUCT inCreateStruct )
{
	AFX_MANAGE_STATE( AfxGetStaticModuleState() );
	
	HINSTANCE		module = NULL;
	OSStatus		err;
	CRect			rect;
	CBitmap			bitmap;
	CString			s;
	
	err = CWnd::OnCreate( inCreateStruct );
	require_noerr( err, exit );
	
	GetClientRect( rect );
	mTree.Create( WS_TABSTOP | WS_VISIBLE | WS_CHILD | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_NOHSCROLL , rect, this, 
		IDC_EXPLORER_TREE );
	
	ServiceHandlerEntry *		e;
	
	// Web Site Handler
	
	e = new ServiceHandlerEntry;
	check( e );
	e->type				= "_http._tcp";
	e->urlScheme		= "http://";
	e->ref				= NULL;
	e->obj				= this;
	e->needsLogin		= false;
	mServiceHandlers.Add( e );

	s.LoadString( IDS_ABOUT );
	m_about = mTree.InsertItem( s, 0, 0 );

	err = DNSServiceBrowse( &e->ref, 0, 0, e->type, NULL, BrowseCallBack, e );
	require_noerr( err, exit );

	err = WSAAsyncSelect((SOCKET) DNSServiceRefSockFD(e->ref), m_hWnd, WM_PRIVATE_SERVICE_EVENT, FD_READ|FD_CLOSE);
	require_noerr( err, exit );

	m_serviceRefs.push_back(e->ref);

	m_imageList.Create( 16, 16, ILC_MASK | ILC_COLOR16, 2, 0);

	bitmap.Attach( ::LoadBitmap( GetNonLocalizedResources(), MAKEINTRESOURCE( IDB_LOGO ) ) );
	m_imageList.Add( &bitmap, (CBitmap*) NULL );
	bitmap.Detach();

	mTree.SetImageList(&m_imageList, TVSIL_NORMAL);
	
exit:

	if ( module )
	{
		FreeLibrary( module );
		module = NULL;
	}

	// Cannot talk to the mDNSResponder service. Show the error message and exit (with kNoErr so they can see it).
	if ( err )
	{
		if ( err == kDNSServiceErr_Firewall )
		{
			s.LoadString( IDS_FIREWALL );
		}
		else
		{
			s.LoadString( IDS_MDNSRESPONDER_NOT_AVAILABLE );
		}
		
		mTree.DeleteAllItems();
		mTree.InsertItem( s, 0, 0, TVI_ROOT, TVI_LAST );
		
		err = kNoErr;
	}

	return( err );
}

//===========================================================================================================================
//	OnDestroy
//===========================================================================================================================

void	ExplorerBarWindow::OnDestroy( void ) 
{
	// Stop any resolves that may still be pending (shouldn't be any).
	
	StopResolve();
	
	// Clean up the extant browses
	while (m_serviceRefs.size() > 0)
	{
		//
		// take the head of the list
		//
		DNSServiceRef ref = m_serviceRefs.front();

		//
		// Stop will remove it from the list
		//
		Stop( ref );
	}

	// Clean up the service handlers.
	
	int		i;
	int		n;
	
	n = (int) mServiceHandlers.GetSize();
	for( i = 0; i < n; ++i )
	{
		delete mServiceHandlers[ i ];
	}
	
	CWnd::OnDestroy();
}

//===========================================================================================================================
//	OnSize
//===========================================================================================================================

void	ExplorerBarWindow::OnSize( UINT inType, int inX, int inY ) 
{
	CWnd::OnSize( inType, inX, inY );
	mTree.MoveWindow( 0, 0, inX, inY );
}

//===========================================================================================================================
//	OnDoubleClick
//===========================================================================================================================

void	ExplorerBarWindow::OnDoubleClick( NMHDR *inNMHDR, LRESULT *outResult )
{
	HTREEITEM			item;
	ServiceInfo *		service;
	OSStatus			err;
	
	DEBUG_UNUSED( inNMHDR );
	
	item = mTree.GetSelectedItem();
	require( item, exit );
	
	// Tell Internet Explorer to go to the URL if it's about item
	
	if ( item == m_about )
	{
		CString url;

		check( mOwner );

		url.LoadString( IDS_ABOUT_URL );
		mOwner->GoToURL( url );
	}
	else
	{
		service = reinterpret_cast < ServiceInfo * > ( mTree.GetItemData( item ) );
		require_quiet( service, exit );
		
		err = StartResolve( service );
		require_noerr( err, exit );
	}

exit:
	*outResult = 0;
}


//===========================================================================================================================
//	OnServiceEvent
//===========================================================================================================================

LRESULT
ExplorerBarWindow::OnServiceEvent(WPARAM inWParam, LPARAM inLParam)
{
	if (WSAGETSELECTERROR(inLParam) && !(HIWORD(inLParam)))
    {
		dlog( kDebugLevelError, "OnServiceEvent: window error\n" );
    }
    else
    {
		SOCKET sock = (SOCKET) inWParam;

		// iterate thru list
		ServiceRefList::iterator it;

		for (it = m_serviceRefs.begin(); it != m_serviceRefs.end(); it++)
		{
			DNSServiceRef ref = *it;

			check(ref != NULL);

			if ((SOCKET) DNSServiceRefSockFD(ref) == sock)
			{
				DNSServiceErrorType err;

				err = DNSServiceProcessResult(ref);

				if (err != 0)
				{
					CString s;

					s.LoadString( IDS_MDNSRESPONDER_NOT_AVAILABLE );
					mTree.DeleteAllItems();
					mTree.InsertItem( s, 0, 0, TVI_ROOT, TVI_LAST );

					Stop(ref);
				}

				break;
			}
		}
	}

	return ( 0 );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	BrowseCallBack
//===========================================================================================================================

void DNSSD_API
	ExplorerBarWindow::BrowseCallBack(
		DNSServiceRef 			inRef,
		DNSServiceFlags 		inFlags,
		uint32_t 				inInterfaceIndex,
		DNSServiceErrorType 	inErrorCode,
		const char *			inName,	
		const char *			inType,	
		const char *			inDomain,	
		void *					inContext )
{
	ServiceHandlerEntry *		obj;
	ServiceInfo *				service;
	OSStatus					err;
	
	DEBUG_UNUSED( inRef );
	
	obj		=	NULL;
	service = NULL;
	
	require_noerr( inErrorCode, exit );
	obj = reinterpret_cast < ServiceHandlerEntry * > ( inContext );
	check( obj );
	check( obj->obj );
	
	//
	// set the UI to hold off on updates
	//
	obj->obj->mTree.SetRedraw(FALSE);

	try
	{
		service = new ServiceInfo;
		require_action( service, exit, err = kNoMemoryErr );
		
		err = UTF8StringToStringObject( inName, service->displayName );
		check_noerr( err );

		service->name = _strdup( inName );
		require_action( service->name, exit, err = kNoMemoryErr );
		
		service->type = _strdup( inType );
		require_action( service->type, exit, err = kNoMemoryErr );
		
		service->domain = _strdup( inDomain );
		require_action( service->domain, exit, err = kNoMemoryErr );
		
		service->ifi 		= inInterfaceIndex;
		service->handler	= obj;

		service->refs		= 1;
		
		if (inFlags & kDNSServiceFlagsAdd) obj->obj->OnServiceAdd   (service);
		else                               obj->obj->OnServiceRemove(service);
	
		service = NULL;
	}
	catch( ... )
	{
		dlog( kDebugLevelError, "BrowseCallBack: exception thrown\n" );
	}
	
exit:
	//
	// If no more coming, then update UI
	//
	if (obj && obj->obj && ((inFlags & kDNSServiceFlagsMoreComing) == 0))
	{
		obj->obj->mTree.SetRedraw(TRUE);
		obj->obj->mTree.Invalidate();
	}

	if( service )
	{
		delete service;
	}
}

//===========================================================================================================================
//	OnServiceAdd
//===========================================================================================================================

LONG	ExplorerBarWindow::OnServiceAdd( ServiceInfo * service )
{
	ServiceHandlerEntry *		handler;
	int							cmp;
	int							index;
	
	
	check( service );
	handler = service->handler; 
	check( handler );
	
	cmp = FindServiceArrayIndex( handler->array, *service, index );
	if( cmp == 0 )
	{
		// Found a match so update the item. The index is index + 1 so subtract 1.
		
		index -= 1;
		check( index < handler->array.GetSize() );

		handler->array[ index ]->refs++;

		delete service;
	}
	else
	{
		HTREEITEM		afterItem;
		
		// Insert the new item in sorted order.
		
		afterItem = ( index > 0 ) ? handler->array[ index - 1 ]->item : m_about;
		handler->array.InsertAt( index, service );
		service->item = mTree.InsertItem( service->displayName, 0, 0, NULL, afterItem );
		mTree.SetItemData( service->item, (DWORD_PTR) service );
	}
	return( 0 );
}

//===========================================================================================================================
//	OnServiceRemove
//===========================================================================================================================

LONG	ExplorerBarWindow::OnServiceRemove( ServiceInfo * service )
{
	ServiceHandlerEntry *		handler;
	int							cmp;
	int							index;
	
	
	check( service );
	handler = service->handler; 
	check( handler );
	
	// Search to see if we know about this service instance. If so, remove it from the list.
	
	cmp = FindServiceArrayIndex( handler->array, *service, index );
	check( cmp == 0 );

	if( cmp == 0 )
	{
		// Possibly found a match remove the item. The index
		// is index + 1 so subtract 1.
		index -= 1;
		check( index < handler->array.GetSize() );

		if ( --handler->array[ index ]->refs == 0 )
		{
			mTree.DeleteItem( handler->array[ index ]->item );
			delete handler->array[ index ];
			handler->array.RemoveAt( index );
		}
	}

	delete service;
	return( 0 );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	StartResolve
//===========================================================================================================================

OSStatus	ExplorerBarWindow::StartResolve( ServiceInfo *inService )
{
	OSStatus		err;
	
	check( inService );
	
	// Stop any current resolve that may be in progress.
	
	StopResolve();
	
	// Resolve the service.
	err = DNSServiceResolve( &mResolveServiceRef, 0, 0, 
		inService->name, inService->type, inService->domain, (DNSServiceResolveReply) ResolveCallBack, inService->handler );
	require_noerr( err, exit );

	err = WSAAsyncSelect((SOCKET) DNSServiceRefSockFD(mResolveServiceRef), m_hWnd, WM_PRIVATE_SERVICE_EVENT, FD_READ|FD_CLOSE);
	require_noerr( err, exit );
	
	m_serviceRefs.push_back(mResolveServiceRef);

exit:
	return( err );
}

//===========================================================================================================================
//	StopResolve
//===========================================================================================================================

void	ExplorerBarWindow::StopResolve( void )
{
	if( mResolveServiceRef )
	{
		Stop( mResolveServiceRef );
		mResolveServiceRef = NULL;
	}
}

//===========================================================================================================================
//	ResolveCallBack
//===========================================================================================================================

void DNSSD_API
	ExplorerBarWindow::ResolveCallBack(
		DNSServiceRef			inRef,
		DNSServiceFlags			inFlags,
		uint32_t				inInterfaceIndex,
		DNSServiceErrorType		inErrorCode,
		const char *			inFullName,	
		const char *			inHostName, 
		uint16_t 				inPort,
		uint16_t 				inTXTSize,
		const char *			inTXT,
		void *					inContext )
{
	ExplorerBarWindow *			obj;
	ServiceHandlerEntry *		handler;
	OSStatus					err;
	
	DEBUG_UNUSED( inRef );
	DEBUG_UNUSED( inFlags );
	DEBUG_UNUSED( inErrorCode );
	DEBUG_UNUSED( inFullName );
	
	require_noerr( inErrorCode, exit );
	handler = (ServiceHandlerEntry *) inContext;
	check( handler );
	obj = handler->obj;
	check( obj );
	
	try
	{
		ResolveInfo *		resolve;
		int					idx;
		
		dlog( kDebugLevelNotice, "resolved %s on ifi %d to %s\n", inFullName, inInterfaceIndex, inHostName );
		
		// Stop resolving after the first good result.
		
		obj->StopResolve();
		
		// Post a message to the main thread so it can handle it since MFC is not thread safe.
		
		resolve = new ResolveInfo;
		require_action( resolve, exit, err = kNoMemoryErr );
		
		UTF8StringToStringObject( inHostName, resolve->host );

		// rdar://problem/3841564
		// 
		// strip trailing dot from hostname because some flavors of Windows
		// have trouble parsing it.

		idx = resolve->host.ReverseFind('.');

		if ((idx > 1) && ((resolve->host.GetLength() - 1) == idx))
		{
			resolve->host.Delete(idx, 1);
		}

		resolve->port		= ntohs( inPort );
		resolve->ifi		= inInterfaceIndex;
		resolve->handler	= handler;
		
		err = resolve->txt.SetData( inTXT, inTXTSize );
		check_noerr( err );
		
		obj->OnResolve(resolve);
	}
	catch( ... )
	{
		dlog( kDebugLevelError, "ResolveCallBack: exception thrown\n" );
	}

exit:
	return;
}

//===========================================================================================================================
//	OnResolve
//===========================================================================================================================

LONG	ExplorerBarWindow::OnResolve( ResolveInfo * resolve )
{
	CString				url;
	uint8_t *			path;
	uint8_t				pathSize;
	char *				pathPrefix;
	CString				username;
	CString				password;
	
	
	check( resolve );
		
	// Get login info if needed.
	
	if( resolve->handler->needsLogin )
	{
		LoginDialog		dialog;
		
		if( !dialog.GetLogin( username, password ) )
		{
			goto exit;
		}
	}
	
	// If the HTTP TXT record is a "path=" entry, use it as the resource path. Otherwise, use "/".
	
	pathPrefix = "";
	if( strcmp( resolve->handler->type, "_http._tcp" ) == 0 )
	{
		uint8_t	*	txtData;
		uint16_t	txtLen;	

		resolve->txt.GetData( &txtData, &txtLen );

		path	 = (uint8_t*) TXTRecordGetValuePtr(txtLen, txtData, kTXTRecordKeyPath, &pathSize);

		if (path == NULL)
		{
			path = (uint8_t*) "";
			pathSize = 1;
		}
	}
	else
	{
		path		= (uint8_t *) "";
		pathSize	= 1;
	}

	// Build the URL in the following format:
	//
	// <urlScheme>[<username>[:<password>]@]<name/ip>[<path>]

	url.AppendFormat( TEXT( "%S" ), resolve->handler->urlScheme );					// URL Scheme
	if( username.GetLength() > 0 )
	{
		url.AppendFormat( TEXT( "%s" ), username );									// Username
		if( password.GetLength() > 0 )
		{
			url.AppendFormat( TEXT( ":%s" ), password );							// Password
		}
		url.AppendFormat( TEXT( "@" ) );
	}
	
	url += resolve->host;															// Host
	url.AppendFormat( TEXT( ":%d" ), resolve->port );								// :Port
	url.AppendFormat( TEXT( "%S" ), pathPrefix );									// Path Prefix ("/" or empty).
	url.AppendFormat( TEXT( "%.*S" ), (int) pathSize, (char *) path );				// Path (possibly empty).
	
	// Tell Internet Explorer to go to the URL.
	
	check( mOwner );
	mOwner->GoToURL( url );

exit:
	delete resolve;
	return( 0 );
}

//===========================================================================================================================
//	Stop
//===========================================================================================================================
void ExplorerBarWindow::Stop( DNSServiceRef ref )
{
	m_serviceRefs.remove( ref );

	WSAAsyncSelect(DNSServiceRefSockFD( ref ), m_hWnd, WM_PRIVATE_SERVICE_EVENT, 0);

	DNSServiceRefDeallocate( ref );
}


#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	FindServiceArrayIndex
//===========================================================================================================================

DEBUG_LOCAL int	FindServiceArrayIndex( const ServiceInfoArray &inArray, const ServiceInfo &inService, int &outIndex )
{
	int		result;
	int		lo;
	int		hi;
	int		mid;
	
	result 	= -1;
	mid		= 0;
	lo 		= 0;
	hi 		= (int)( inArray.GetSize() - 1 );
	while( lo <= hi )
	{
		mid = ( lo + hi ) / 2;
		result = inService.displayName.CompareNoCase( inArray[ mid ]->displayName );
#if 0
		if( result == 0 )
		{
			result = ( (int) inService.ifi ) - ( (int) inArray[ mid ]->ifi );
		}
#endif
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
		mid += 1;	// Bump index so new item is inserted after matching item.
	}
	else if( result > 0 )
	{
		mid += 1;
	}
	outIndex = mid;
	return( result );
}
