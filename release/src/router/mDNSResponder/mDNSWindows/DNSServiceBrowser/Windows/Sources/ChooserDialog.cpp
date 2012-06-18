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
    
$Log: ChooserDialog.cpp,v $
Revision 1.5  2008/10/23 22:33:26  cheshire
Changed "NOTE:" to "Note:" so that BBEdit 9 stops putting those comment lines into the funtion popup menu

Revision 1.4  2006/08/14 23:25:49  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.3  2005/02/10 22:35:35  cheshire
<rdar://problem/3727944> Update name

Revision 1.2  2004/07/13 21:24:26  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.1  2004/06/18 04:04:36  rpantos
Move up one level

Revision 1.10  2004/04/23 01:19:41  bradley
Changed TXT record new line delimiter from \n to \r\n so it works now that it is an edit text.

Revision 1.9  2004/03/07 05:51:04  bradley
Updated service type list table to include all service types from dns-sd.org as of 2004-03-06.
Added separate Service Type and Service Description columns so both are show in the window.

Revision 1.8  2004/01/30 02:56:32  bradley
Updated to support full Unicode display. Added support for all services on www.dns-sd.org.

Revision 1.7  2003/12/25 03:42:04  bradley
Added login dialog to get username/password when going to FTP sites. Added more services.

Revision 1.6  2003/10/31 12:18:30  bradley
Added display of the resolved host name. Show separate TXT record entries on separate lines.

Revision 1.5  2003/10/16 09:21:56  bradley
Ignore non-IPv4 resolves until mDNS on Windows supports IPv6.

Revision 1.4  2003/10/10 03:41:29  bradley
Changed HTTP double-click handling to work with or without the path= prefix in the TXT record.

Revision 1.3  2003/10/09 19:50:40  bradley
Sort service type list by description.

Revision 1.2  2003/10/09 19:41:29  bradley
Changed quit handling to go through normal close path so dialog is freed on quit. Integrated changes
from Andrew van der Stock for the addition of an _rfb._tcp service type for a VNC Remote Framebuffer
Server for KDE support. Widened service type list to handle larger service type descriptions.

Revision 1.1  2003/08/21 02:06:47  bradley
Moved DNSServiceBrowser for non-Windows CE into Windows sub-folder.

Revision 1.7  2003/08/20 06:45:56  bradley
Updated for IP address changes in DNSServices. Added support for browsing for Xserve RAID.

Revision 1.6  2003/08/12 19:56:28  cheshire
Update to APSL 2.0

Revision 1.5  2003/07/13 01:03:55  cheshire
Diffs provided by Bob Bradley to provide provide proper display of Unicode names

Revision 1.4  2003/07/02 21:20:06  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.3  2002/09/21 20:44:55  zarzycki
Added APSL info

Revision 1.2  2002/09/20 08:39:21  bradley
Make sure each resolved item matches the selected service type to handle resolved that may have
been queued up on the Windows Message Loop. Reduce column to fit when scrollbar is present.

Revision 1.1  2002/09/20 06:12:52  bradley
DNSServiceBrowser for Windows

*/

#include	<assert.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>

#include	<algorithm>
#include	<memory>

#include	"stdafx.h"

#include	"DNSServices.h"

#include	"Application.h"
#include	"AboutDialog.h"
#include	"LoginDialog.h"
#include	"Resource.h"

#include	"ChooserDialog.h"

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

// Menus

enum
{
	kChooserMenuIndexFile	= 0, 
	kChooserMenuIndexHelp	= 1 
};

// Domain List
	
#define kDomainListDefaultDomainColumnWidth		 		164 
	
// Service List

#define kServiceListDefaultServiceColumnTypeWidth		146
#define kServiceListDefaultServiceColumnDescWidth		230
	
// Chooser List
	
#define kChooserListDefaultNameColumnWidth				190
#define kChooserListDefaultIPColumnWidth				120

// Windows User Messages

#define	WM_USER_DOMAIN_ADD								( WM_USER + 0x100 )
#define	WM_USER_DOMAIN_REMOVE							( WM_USER + 0x101 )
#define	WM_USER_SERVICE_ADD								( WM_USER + 0x102 )
#define	WM_USER_SERVICE_REMOVE							( WM_USER + 0x103 )
#define	WM_USER_RESOLVE									( WM_USER + 0x104 )

#if 0
#pragma mark == Constants - Service Table ==
#endif

//===========================================================================================================================
//	Constants - Service Table
//===========================================================================================================================

struct	KnownServiceEntry
{
	const char *		serviceType;
	const char *		description;
	const char *		urlScheme;
	bool				useText;
};

static const KnownServiceEntry		kKnownServiceTable[] =
{
	{ "_accountedge._tcp.",	 		"MYOB AccountEdge", 										"", 			false },
	{ "_aecoretech._tcp.", 			"Apple Application Engineering Services",					"", 			false },
	{ "_afpovertcp._tcp.", 			"Apple File Sharing (AFP)", 								"afp://", 		false },
	{ "_airport._tcp.", 			"AirPort Base Station",										"", 			false }, 
	{ "_apple-sasl._tcp.", 			"Apple Password Server", 									"", 			false },
	{ "_aquamon._tcp.", 			"AquaMon", 													"", 			false },
	{ "_async._tcp", 				"address-o-sync", 											"", 			false },
	{ "_auth._tcp.", 				"Authentication Service",									"", 			false },
	{ "_bootps._tcp.", 				"Bootstrap Protocol Server",								"", 			false },
	{ "_bousg._tcp.", 				"Bag Of Unusual Strategy Games",							"", 			false },
	{ "_browse._udp.", 				"DNS Service Discovery",									"", 			false },
	{ "_cheat._tcp.", 				"The Cheat",												"", 			false },
	{ "_chess._tcp", 				"Project Gridlock", 										"", 			false },
	{ "_chfts._tcp", 				"Fluid Theme Server", 										"", 			false },
	{ "_clipboard._tcp", 			"Clipboard Sharing", 										"", 			false },
	{ "_contactserver._tcp.", 		"Now Up-to-Date & Contact",									"", 			false },
	{ "_cvspserver._tcp", 			"CVS PServer", 												"", 			false },
	{ "_cytv._tcp.", 				"CyTV Network streaming for Elgato EyeTV",					"", 			false },
	{ "_daap._tcp.", 				"Digital Audio Access Protocol (iTunes)",					"daap://",		false }, 
	{ "_distcc._tcp", 				"Distributed Compiler", 									"", 			false },
	{ "_dns-sd._udp", 				"DNS Service Discovery", 									"", 			false },
	{ "_dpap._tcp.", 				"Digital Picture Access Protocol (iPhoto)",					"", 			false },
	{ "_earphoria._tcp.", 			"Earphoria",												"", 			false },
	{ "_ecbyesfsgksc._tcp.", 		"Net Monitor Anti-Piracy Service",							"",				false },
	{ "_eheap._tcp.", 				"Interactive Room Software",								"",				false },
	{ "_embrace._tcp.", 			"DataEnvoy",												"",				false },
	{ "_eppc._tcp.", 				"Remote AppleEvents", 										"eppc://", 		false }, 
	{ "_exec._tcp.", 				"Remote Process Execution",									"",				false },
	{ "_facespan._tcp.", 			"FaceSpan",													"",				false },
	{ "_fjork._tcp.", 				"Fjork",													"",				false },
	{ "_ftp._tcp.", 				"File Transfer (FTP)", 										"ftp://", 		false }, 
	{ "_ftpcroco._tcp.", 			"Crocodile FTP Server",										"",				false },
	{ "_gbs-smp._tcp.", 			"SnapMail",													"",				false },
	{ "_gbs-stp._tcp.", 			"SnapTalk",													"",				false },
	{ "_grillezvous._tcp.", 		"Roxio ToastAnywhere(tm) Recorder Sharing",					"",				false },
	{ "_h323._tcp.", 				"H.323",													"",				false },
	{ "_hotwayd._tcp", 				"Hotwayd", 													"", 			false },
	{ "_http._tcp.", 				"Web Server (HTTP)", 										"http://", 		true  }, 
	{ "_hydra._tcp", 				"SubEthaEdit", 												"", 			false },
	{ "_ica-networking._tcp.", 		"Image Capture Networking",									"", 			false }, 
	{ "_ichalkboard._tcp.", 		"iChalk",													"", 			false }, 
	{ "_ichat._tcp.", 				"iChat",				 									"ichat://",		false }, 
	{ "_iconquer._tcp.",	 		"iConquer",													"", 			false }, 
	{ "_imap._tcp.", 				"Internet Message Access Protocol",							"",				false },
	{ "_imidi._tcp.", 				"iMidi",													"",				false },
	{ "_ipp._tcp.", 				"Printer (IPP)", 											"ipp://", 		false },
	{ "_ishare._tcp.", 				"iShare",													"",				false },
	{ "_isparx._tcp.", 				"iSparx",													"",				false },
	{ "_istorm._tcp", 				"iStorm", 													"", 			false },
	{ "_iwork._tcp.", 				"iWork Server",												"",				false },
	{ "_liaison._tcp.", 			"Liaison",													"",				false },
	{ "_login._tcp.", 				"Remote Login a la Telnet",									"",				false },
	{ "_lontalk._tcp.", 			"LonTalk over IP (ANSI 852)",								"",				false },
	{ "_lonworks._tcp.", 			"Echelon LNS Remote Client",								"",				false },
	{ "_macfoh-remote._tcp.", 		"MacFOH Remote",											"",				false },
	{ "_moneyworks._tcp.", 			"MoneyWorks",												"",				false },
	{ "_mp3sushi._tcp", 			"MP3 Sushi", 												"", 			false },
	{ "_mttp._tcp.", 				"MenuTunes Sharing",										"",				false },
	{ "_ncbroadcast._tcp.", 		"Network Clipboard Broadcasts",								"",				false },
	{ "_ncdirect._tcp.", 			"Network Clipboard Direct Transfers",						"",				false },
	{ "_ncsyncserver._tcp.", 		"Network Clipboard Sync Server",							"",				false },
	{ "_newton-dock._tcp.", 		"Escale",													"",				false },
	{ "_nfs._tcp", 					"NFS", 														"", 			false },
	{ "_nssocketport._tcp.", 		"DO over NSSocketPort",										"",				false },
	{ "_omni-bookmark._tcp.", 		"OmniWeb",													"",				false },
	{ "_openbase._tcp.", 			"OpenBase SQL",												"",				false },
	{ "_p2pchat._tcp.", 			"Peer-to-Peer Chat",										"",				false },
	{ "_pdl-datastream._tcp.", 		"Printer (PDL)", 											"pdl://", 		false }, 
	{ "_poch._tcp.", 				"Parallel OperatiOn and Control Heuristic",					"",				false },
	{ "_pop_2_ambrosia._tcp.",		"Pop-Pop",													"",				false },
	{ "_pop3._tcp", 				"POP3 Server", 												"", 			false },
	{ "_postgresql._tcp", 			"PostgreSQL Server", 										"", 			false },
	{ "_presence._tcp", 			"iChat AV", 												"", 			false },
	{ "_printer._tcp.", 			"Printer (LPR)", 											"lpr://", 		false }, 
	{ "_ptp._tcp.", 				"Picture Transfer (PTP)", 									"ptp://", 		false },
	{ "_register._tcp", 			"DNS Service Discovery", 									"", 			false },
	{ "_rfb._tcp.", 				"Remote Frame Buffer",										"",				false },
	{ "_riousbprint._tcp.", 		"Remote I/O USB Printer Protocol",							"",				false },
	{ "_rtsp._tcp.", 				"Real Time Stream Control Protocol",						"",				false },
	{ "_safarimenu._tcp", 			"Safari Menu", 												"", 			false },
	{ "_scone._tcp", 				"Scone", 													"", 			false },
	{ "_sdsharing._tcp.", 			"Speed Download",											"",				false },
	{ "_seeCard._tcp.", 			"seeCard",													"",				false },
	{ "_services._udp.", 			"DNS Service Discovery",									"",				false },
	{ "_shell._tcp.", 				"like exec, but automatic authentication",					"",				false },
	{ "_shout._tcp.", 				"Shout",													"",				false },
	{ "_shoutcast._tcp", 			"Nicecast", 												"", 			false },
	{ "_smb._tcp.", 				"Windows File Sharing (SMB)", 								"smb://", 		false }, 
	{ "_soap._tcp.", 				"Simple Object Access Protocol", 							"", 			false }, 
	{ "_spincrisis._tcp.", 			"Spin Crisis",												"",				false },
	{ "_spl-itunes._tcp.", 			"launchTunes",												"",				false },
	{ "_spr-itunes._tcp.", 			"netTunes",													"",				false },
	{ "_ssh._tcp.", 				"Secure Shell (SSH)", 										"ssh://", 		false }, 
	{ "_ssscreenshare._tcp", 		"Screen Sharing", 											"", 			false },
	{ "_sge-exec._tcp", 			"Sun Grid Engine (Execution Host)", 						"", 			false },
	{ "_sge-qmaster._tcp", 			"Sun Grid Engine (Master)", 								"", 			false },
	{ "_stickynotes._tcp", 			"Sticky Notes", 											"", 			false },
	{ "_strateges._tcp", 			"Strateges", 												"", 			false },
	{ "_sxqdea._tcp", 				"Synchronize! Pro X", 										"", 			false },
	{ "_sybase-tds._tcp", 			"Sybase Server", 											"", 			false },
	{ "_tce._tcp", 					"Power Card", 												"", 			false },
	{ "_teamlist._tcp", 			"ARTIS Team Task",											"", 			false },
	{ "_teleport._tcp", 			"teleport",													"", 			false },
	{ "_telnet._tcp.", 				"Telnet", 													"telnet://", 	false }, 
	{ "_tftp._tcp.", 				"Trivial File Transfer (TFTP)", 							"tftp://", 		false }, 
	{ "_tinavigator._tcp.", 		"TI Navigator", 											"", 			false }, 
	{ "_tivo_servemedia._tcp", 		"TiVo",														"", 			false },
	{ "_upnp._tcp.", 				"Universal Plug and Play", 									"", 			false }, 
	{ "_utest._tcp.", 				"uTest", 													"", 			false }, 
	{ "_vue4rendercow._tcp",		"VueProRenderCow",											"", 			false },
	{ "_webdav._tcp.", 				"WebDAV", 													"webdav://",	false }, 
	{ "_whamb._tcp.", 				"Whamb", 													"",				false }, 
	{ "_workstation._tcp", 			"Macintosh Manager",										"", 			false },
	{ "_ws._tcp", 					"Web Services",												"", 			false },
	{ "_xserveraid._tcp.", 			"Xserve RAID",												"xsr://", 		false }, 
	{ "_xsync._tcp.",	 			"Xserve RAID Synchronization",								"",		 		false }, 
	
	{ "",	 						"",															"",		 		false }, 
	
	// Unofficial and invalid service types that will be phased out:
	
	{ "_clipboardsharing._tcp.",			"ClipboardSharing",									"",		 		false }, 
	{ "_MacOSXDupSuppress._tcp.",			"Mac OS X Duplicate Suppression",					"",		 		false }, 
	{ "_netmonitorserver._tcp.",			"Net Monitor Server",								"",		 		false }, 
	{ "_networkclipboard._tcp.",			"Network Clipboard",								"",		 		false }, 
	{ "_slimdevices_slimp3_cli._tcp.",		"SliMP3 Server Command-Line Interface",				"",		 		false }, 
	{ "_slimdevices_slimp3_http._tcp.",		"SliMP3 Server Web Interface",						"",		 		false }, 
	{ "_tieducationalhandhelddevice._tcp.",	"TI Connect Manager",								"",		 		false }, 
	{ "_tivo_servemedia._tcp.",				"TiVo",												"",		 		false }, 
	
	{ NULL,							NULL,														NULL,			false }, 
};

#if 0
#pragma mark == Structures ==
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

struct	DomainEventInfo
{
	DNSBrowserEventType		eventType;
	CString					domain;
	DNSNetworkAddress		ifIP;
};

struct	ServiceEventInfo
{
	DNSBrowserEventType		eventType;
	std::string				name;
	std::string				type;
	std::string				domain;
	DNSNetworkAddress		ifIP;
};

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void
	BrowserCallBack( 
		void *					inContext, 
		DNSBrowserRef			inRef, 
		DNSStatus				inStatusCode,
		const DNSBrowserEvent *	inEvent );

static char *	DNSNetworkAddressToString( const DNSNetworkAddress *inAddr, char *outString );

static DWORD	UTF8StringToStringObject( const char *inUTF8, CString &inObject );
static DWORD	StringObjectToUTF8String( CString &inObject, std::string &outUTF8 );

#if 0
#pragma mark == Message Map ==
#endif

//===========================================================================================================================
//	Message Map
//===========================================================================================================================

BEGIN_MESSAGE_MAP(ChooserDialog, CDialog)
	//{{AFX_MSG_MAP(ChooserDialog)
	ON_WM_SYSCOMMAND()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_DOMAIN_LIST, OnDomainListChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_SERVICE_LIST, OnServiceListChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CHOOSER_LIST, OnChooserListChanged)
	ON_NOTIFY(NM_DBLCLK, IDC_CHOOSER_LIST, OnChooserListDoubleClick)
	ON_COMMAND(ID_HELP_ABOUT, OnAbout)
	ON_WM_INITMENUPOPUP()
	ON_WM_ACTIVATE()
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
	ON_COMMAND(ID_FILE_EXIT, OnExit)
	ON_WM_CLOSE()
	ON_WM_NCDESTROY()
	//}}AFX_MSG_MAP
	ON_MESSAGE( WM_USER_DOMAIN_ADD, OnDomainAdd )
	ON_MESSAGE( WM_USER_DOMAIN_REMOVE, OnDomainRemove )
	ON_MESSAGE( WM_USER_SERVICE_ADD, OnServiceAdd )
	ON_MESSAGE( WM_USER_SERVICE_REMOVE, OnServiceRemove )
	ON_MESSAGE( WM_USER_RESOLVE, OnResolve )
END_MESSAGE_MAP()

#if 0
#pragma mark == Routines ==
#endif

//===========================================================================================================================
//	ChooserDialog
//===========================================================================================================================

ChooserDialog::ChooserDialog( CWnd *inParent )
	: CDialog( ChooserDialog::IDD, inParent)
{
	//{{AFX_DATA_INIT(ChooserDialog)
		// Note: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	
	// Load menu accelerator table.

	mMenuAcceleratorTable = ::LoadAccelerators( AfxGetInstanceHandle(), MAKEINTRESOURCE( IDR_CHOOSER_DIALOG_MENU_ACCELERATORS ) );
	assert( mMenuAcceleratorTable );
	
	mBrowser 			= NULL;
	mIsServiceBrowsing	= false;
}

//===========================================================================================================================
//	~ChooserDialog
//===========================================================================================================================

ChooserDialog::~ChooserDialog( void )
{
	if( mBrowser )
	{
		DNSStatus		err;
		
		err = DNSBrowserRelease( mBrowser, 0 );
		assert( err == kDNSNoErr );
	}
}

//===========================================================================================================================
//	DoDataExchange
//===========================================================================================================================

void ChooserDialog::DoDataExchange( CDataExchange *pDX )
{
	CDialog::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(ChooserDialog)
	DDX_Control(pDX, IDC_SERVICE_LIST, mServiceList);
	DDX_Control(pDX, IDC_DOMAIN_LIST, mDomainList);
	DDX_Control(pDX, IDC_CHOOSER_LIST, mChooserList);
	//}}AFX_DATA_MAP
}

//===========================================================================================================================
//	OnInitDialog
//===========================================================================================================================

BOOL	ChooserDialog::OnInitDialog( void )
{
	HICON			icon;
	BOOL			result;
	CString			tempString;
	DNSStatus		err;
	
	// Initialize our parent.

	CDialog::OnInitDialog();
	
	// Set up the window icon.
	
	icon = AfxGetApp()->LoadIcon( IDR_MAIN_ICON );
	assert( icon );
	if( icon )
	{
		SetIcon( icon, TRUE );		// Set big icon
		SetIcon( icon, FALSE );		// Set small icon
	}
	
	// Set up the Domain List.
	
	result = tempString.LoadString( IDS_CHOOSER_DOMAIN_COLUMN_NAME );
	assert( result );
	mDomainList.InsertColumn( 0, tempString, LVCFMT_LEFT, kDomainListDefaultDomainColumnWidth );
	
	// Set up the Service List.
	
	result = tempString.LoadString( IDS_CHOOSER_SERVICE_COLUMN_TYPE );
	assert( result );
	mServiceList.InsertColumn( 0, tempString, LVCFMT_LEFT, kServiceListDefaultServiceColumnTypeWidth );
	
	result = tempString.LoadString( IDS_CHOOSER_SERVICE_COLUMN_DESC );
	assert( result );
	mServiceList.InsertColumn( 1, tempString, LVCFMT_LEFT, kServiceListDefaultServiceColumnDescWidth );
	
	PopulateServicesList();
	
	// Set up the Chooser List.
	
	result = tempString.LoadString( IDS_CHOOSER_CHOOSER_NAME_COLUMN_NAME );
	assert( result );
	mChooserList.InsertColumn( 0, tempString, LVCFMT_LEFT, kChooserListDefaultNameColumnWidth );
	
	result = tempString.LoadString( IDS_CHOOSER_CHOOSER_IP_COLUMN_NAME );
	assert( result );
	mChooserList.InsertColumn( 1, tempString, LVCFMT_LEFT, kChooserListDefaultIPColumnWidth );
	
	// Set up the other controls.
	
	UpdateInfoDisplay();
	
	// Start browsing for domains.
	
	err = DNSBrowserCreate( 0, BrowserCallBack, this, &mBrowser );
	assert( err == kDNSNoErr );
	
	err = DNSBrowserStartDomainSearch( mBrowser, 0 );
	assert( err == kDNSNoErr );
	
	return( true );
}

//===========================================================================================================================
//	OnFileClose
//===========================================================================================================================

void ChooserDialog::OnFileClose() 
{
	OnClose();
}

//===========================================================================================================================
//	OnActivate
//===========================================================================================================================

void ChooserDialog::OnActivate( UINT nState, CWnd* pWndOther, BOOL bMinimized )
{
	// Always make the active window the "main" window so modal dialogs work better and the app quits after closing 
	// the last window.

	gApp.m_pMainWnd = this;

	CDialog::OnActivate(nState, pWndOther, bMinimized);
}

//===========================================================================================================================
//	PostNcDestroy
//===========================================================================================================================

void	ChooserDialog::PostNcDestroy() 
{
	// Call the base class to do the normal cleanup.

	delete this;
}

//===========================================================================================================================
//	PreTranslateMessage
//===========================================================================================================================

BOOL	ChooserDialog::PreTranslateMessage(MSG* pMsg) 
{
	BOOL		result;
	
	result = false;
	assert( mMenuAcceleratorTable );
	if( mMenuAcceleratorTable )
	{
		result = ::TranslateAccelerator( m_hWnd, mMenuAcceleratorTable, pMsg );
	}
	if( !result )
	{
		result = CDialog::PreTranslateMessage( pMsg );
	}
	return( result );
}

//===========================================================================================================================
//	OnInitMenuPopup
//===========================================================================================================================

void	ChooserDialog::OnInitMenuPopup( CMenu *pPopupMenu, UINT nIndex, BOOL bSysMenu ) 
{
	CDialog::OnInitMenuPopup( pPopupMenu, nIndex, bSysMenu );

	switch( nIndex )
	{
		case kChooserMenuIndexFile:
			break;

		case kChooserMenuIndexHelp:
			break;

		default:
			break;
	}
}

//===========================================================================================================================
//	OnExit
//===========================================================================================================================

void ChooserDialog::OnExit() 
{
	OnClose();
}

//===========================================================================================================================
//	OnAbout
//===========================================================================================================================

void	ChooserDialog::OnAbout() 
{
	AboutDialog		dialog;
	
	dialog.DoModal();
}

//===========================================================================================================================
//	OnSysCommand
//===========================================================================================================================

void	ChooserDialog::OnSysCommand( UINT inID, LPARAM inParam ) 
{
	CDialog::OnSysCommand( inID, inParam );
}

//===========================================================================================================================
//	OnClose
//===========================================================================================================================

void ChooserDialog::OnClose() 
{
	StopBrowsing();
	
	gApp.m_pMainWnd = this;
	DestroyWindow();
}

//===========================================================================================================================
//	OnNcDestroy
//===========================================================================================================================

void ChooserDialog::OnNcDestroy() 
{
	gApp.m_pMainWnd = this;

	CDialog::OnNcDestroy();
}

//===========================================================================================================================
//	OnDomainListChanged
//===========================================================================================================================

void	ChooserDialog::OnDomainListChanged( NMHDR *pNMHDR, LRESULT *pResult ) 
{
	UNUSED_ALWAYS( pNMHDR );
	
	// Domain list changes have similar effects to service list changes so reuse that code path by calling it here.
	
	OnServiceListChanged( NULL, NULL );
	
	*pResult = 0;
}

//===========================================================================================================================
//	OnServiceListChanged
//===========================================================================================================================

void	ChooserDialog::OnServiceListChanged( NMHDR *pNMHDR, LRESULT *pResult ) 
{
	int				selectedType;
	int				selectedDomain;
	
	UNUSED_ALWAYS( pNMHDR );
	
	// Stop any existing service search.
	
	StopBrowsing();
	
	// If a domain and service type are selected, start searching for the service type on the domain.
	
	selectedType 	= mServiceList.GetNextItem( -1, LVNI_SELECTED );
	selectedDomain 	= mDomainList.GetNextItem( -1, LVNI_SELECTED );
	
	if( ( selectedType >= 0 ) && ( selectedDomain >= 0 ) )
	{
		CString				s;
		std::string			utf8;
		const char *		type;
		
		s = mDomainList.GetItemText( selectedDomain, 0 );
		StringObjectToUTF8String( s, utf8 );
		type = mServiceTypes[ selectedType ].serviceType.c_str();
		if( *type != '\0' )
		{
			StartBrowsing( type, utf8.c_str() );
		}
	}
	
	if( pResult )
	{
		*pResult = 0;
	}
}

//===========================================================================================================================
//	OnChooserListChanged
//===========================================================================================================================

void	ChooserDialog::OnChooserListChanged( NMHDR *pNMHDR, LRESULT *pResult ) 
{
	UNUSED_ALWAYS( pNMHDR );
	
	UpdateInfoDisplay();
	*pResult = 0;
}

//===========================================================================================================================
//	OnChooserListDoubleClick
//===========================================================================================================================

void	ChooserDialog::OnChooserListDoubleClick( NMHDR *pNMHDR, LRESULT *pResult )
{
	int		selectedItem;
	
	UNUSED_ALWAYS( pNMHDR );
	
	// Display the service instance if it is selected. Otherwise, clear all the info.
	
	selectedItem = mChooserList.GetNextItem( -1, LVNI_SELECTED );
	if( selectedItem >= 0 )
	{
		ServiceInstanceInfo *			p;
		CString							url;
		const KnownServiceEntry *		service;
		
		assert( selectedItem < (int) mServiceInstances.size() );
		p = &mServiceInstances[ selectedItem ];
		
		// Search for a known service type entry that matches.
		
		for( service = kKnownServiceTable; service->serviceType; ++service )
		{
			if( p->type == service->serviceType )
			{
				break;
			}
		}
		if( service->serviceType )
		{
			const char *		text;
			
			// Create a URL representing the service instance.
			
			if( strcmp( service->serviceType, "_smb._tcp." ) == 0 )
			{
				// Special case for SMB (no port number).
				
				url.Format( TEXT( "%s%s/" ), service->urlScheme, (const char *) p->ip.c_str() ); 
			}
			else if( strcmp( service->serviceType, "_ftp._tcp." ) == 0 )
			{
				// Special case for FTP to get login info.

				LoginDialog		dialog;
				CString			username;
				CString			password;
				
				if( !dialog.GetLogin( username, password ) )
				{
					goto exit;
				}
				
				// Build URL in the following format:
				//
				// ftp://[username[:password]@]<ip>
				
				url += service->urlScheme;
				if( username.GetLength() > 0 )
				{
					url += username;
					if( password.GetLength() > 0 )
					{
						url += ':';
						url += password;
					}
					url += '@';
				}
				url += p->ip.c_str();
			}
			else if( strcmp( service->serviceType, "_http._tcp." ) == 0 )
			{
				// Special case for HTTP to exclude "path=" if present.
				
				text = service->useText ? p->text.c_str() : "";
				if( strncmp( text, "path=", 5 ) == 0 )
				{
					text += 5;
				}
				if( *text != '/' )
				{
					url.Format( TEXT( "%s%s/%s" ), service->urlScheme, (const char *) p->ip.c_str(), text );
				}
				else
				{
					url.Format( TEXT( "%s%s%s" ), service->urlScheme, (const char *) p->ip.c_str(), text );
				}
			}
			else
			{
				text = service->useText ? p->text.c_str() : "";
				url.Format( TEXT( "%s%s/%s" ), service->urlScheme, (const char *) p->ip.c_str(), text ); 
			}
			
			// Let the system open the URL in the correct app.
			
			{
				CWaitCursor		waitCursor;
				
				ShellExecute( NULL, TEXT( "open" ), url, TEXT( "" ), TEXT( "c:\\" ), SW_SHOWNORMAL );
			}
		}
	}

exit:
	*pResult = 0;
}

//===========================================================================================================================
//	OnCancel
//===========================================================================================================================

void ChooserDialog::OnCancel() 
{
	// Do nothing.
}

//===========================================================================================================================
//	PopulateServicesList
//===========================================================================================================================

void	ChooserDialog::PopulateServicesList( void )
{
	ServiceTypeVector::iterator		i;
	CString							type;
	CString							desc;
	std::string						tmp;
	
	// Add a fixed list of known services.
	
	if( mServiceTypes.empty() )
	{
		const KnownServiceEntry *		service;
		
		for( service = kKnownServiceTable; service->serviceType; ++service )
		{
			ServiceTypeInfo		info;
			
			info.serviceType 	= service->serviceType;
			info.description 	= service->description;
			info.urlScheme 		= service->urlScheme;
			mServiceTypes.push_back( info );
		}
	}
	
	// Add each service to the list.
	
	for( i = mServiceTypes.begin(); i != mServiceTypes.end(); ++i )
	{
		const char *		p;
		const char *		q;
		
		p  = ( *i ).serviceType.c_str();
		if( *p == '_' ) ++p;							// Skip leading '_'.
		q  = strchr( p, '.' );							// Find first '.'.
		if( q )	tmp.assign( p, (size_t)( q - p ) );		// Use only up to the first '.'.
		else	tmp.assign( p );						// No '.' so use the entire string.
		UTF8StringToStringObject( tmp.c_str(), type );
		UTF8StringToStringObject( ( *i ).description.c_str(), desc );
		
		int		n;
		
		n = mServiceList.GetItemCount();
		mServiceList.InsertItem( n, type );
		mServiceList.SetItemText( n, 1, desc );
	}
	
	// Select the first service type by default.
	
	if( !mServiceTypes.empty() )
	{
		mServiceList.SetItemState( 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
	}
}

//===========================================================================================================================
//	UpdateInfoDisplay
//===========================================================================================================================

void	ChooserDialog::UpdateInfoDisplay( void )
{
	int							selectedItem;
	std::string					name;
	CString						s;
	std::string					ip;
	std::string					ifIP;
	std::string					text;
	std::string					textNewLines;
	std::string					hostName;
	CWnd *						item;
	std::string::iterator		i;
	
	// Display the service instance if it is selected. Otherwise, clear all the info.
	
	selectedItem = mChooserList.GetNextItem( -1, LVNI_SELECTED );
	if( selectedItem >= 0 )
	{
		ServiceInstanceInfo *		p;
		
		assert( selectedItem < (int) mServiceInstances.size() );
		p = &mServiceInstances[ selectedItem ];
		
		name 		= p->name;
		ip 			= p->ip;
		ifIP 		= p->ifIP;
		text 		= p->text;
		hostName	= p->hostName;

		// Sync up the list items with the actual data (IP address may change).
		
		UTF8StringToStringObject( ip.c_str(), s );
		mChooserList.SetItemText( selectedItem, 1, s );
	}
	
	// Name
	
	item = (CWnd *) this->GetDlgItem( IDC_INFO_NAME_TEXT );
	assert( item );
	UTF8StringToStringObject( name.c_str(), s );
	item->SetWindowText( s );
	
	// IP
	
	item = (CWnd *) this->GetDlgItem( IDC_INFO_IP_TEXT );
	assert( item );
	UTF8StringToStringObject( ip.c_str(), s );
	item->SetWindowText( s );
	
	// Interface
	
	item = (CWnd *) this->GetDlgItem( IDC_INFO_INTERFACE_TEXT );
	assert( item );
	UTF8StringToStringObject( ifIP.c_str(), s );
	item->SetWindowText( s );
	

	item = (CWnd *) this->GetDlgItem( IDC_INFO_HOST_NAME_TEXT );
	assert( item );
	UTF8StringToStringObject( hostName.c_str(), s );
	item->SetWindowText( s );

	// Text
	
	item = (CWnd *) this->GetDlgItem( IDC_INFO_TEXT_TEXT );
	assert( item );
	for( i = text.begin(); i != text.end(); ++i )
	{
		if( *i == '\1' )
		{
			textNewLines += "\r\n";
		}
		else
		{
			textNewLines += *i;
		}
	}
	UTF8StringToStringObject( textNewLines.c_str(), s );
	item->SetWindowText( s );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	OnDomainAdd
//===========================================================================================================================

LONG	ChooserDialog::OnDomainAdd( WPARAM inWParam, LPARAM inLParam )
{
	DomainEventInfo *						p;
	std::auto_ptr < DomainEventInfo >		pAutoPtr;
	int										n;
	int										i;
	CString									domain;
	CString									s;
	bool									found;
	
	UNUSED_ALWAYS( inWParam );
	
	assert( inLParam );
	p = reinterpret_cast <DomainEventInfo *> ( inLParam );
	pAutoPtr.reset( p );
	
	// Search to see if we already know about this domain. If not, add it to the list.
	
	found = false;
	domain = p->domain;
	n = mDomainList.GetItemCount();
	for( i = 0; i < n; ++i )
	{
		s = mDomainList.GetItemText( i, 0 );
		if( s == domain )
		{
			found = true;
			break;
		}
	}
	if( !found )
	{
		int		selectedItem;
		
		mDomainList.InsertItem( n, domain );
		
		// If no domains are selected and the domain being added is a default domain, select it.
		
		selectedItem = mDomainList.GetNextItem( -1, LVNI_SELECTED );
		if( ( selectedItem < 0 ) && ( p->eventType == kDNSBrowserEventTypeAddDefaultDomain ) )
		{
			mDomainList.SetItemState( n, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
		}
	}
	return( 0 );
}

//===========================================================================================================================
//	OnDomainRemove
//===========================================================================================================================

LONG	ChooserDialog::OnDomainRemove( WPARAM inWParam, LPARAM inLParam )
{
	DomainEventInfo *						p;
	std::auto_ptr < DomainEventInfo >		pAutoPtr;
	int										n;
	int										i;
	CString									domain;
	CString									s;
	bool									found;
	
	UNUSED_ALWAYS( inWParam );
	
	assert( inLParam );
	p = reinterpret_cast <DomainEventInfo *> ( inLParam );
	pAutoPtr.reset( p );
	
	// Search to see if we know about this domain. If so, remove it from the list.
	
	found = false;
	domain = p->domain;
	n = mDomainList.GetItemCount();
	for( i = 0; i < n; ++i )
	{
		s = mDomainList.GetItemText( i, 0 );
		if( s == domain )
		{
			found = true;
			break;
		}
	}
	if( found )
	{
		mDomainList.DeleteItem( i );
	}
	return( 0 );
}

//===========================================================================================================================
//	OnServiceAdd
//===========================================================================================================================

LONG	ChooserDialog::OnServiceAdd( WPARAM inWParam, LPARAM inLParam )
{
	ServiceEventInfo *						p;
	std::auto_ptr < ServiceEventInfo >		pAutoPtr;
	
	UNUSED_ALWAYS( inWParam );
	
	assert( inLParam );
	p = reinterpret_cast <ServiceEventInfo *> ( inLParam );
	pAutoPtr.reset( p );
	
	return( 0 );
}

//===========================================================================================================================
//	OnServiceRemove
//===========================================================================================================================

LONG	ChooserDialog::OnServiceRemove( WPARAM inWParam, LPARAM inLParam )
{
	ServiceEventInfo *						p;
	std::auto_ptr < ServiceEventInfo >		pAutoPtr;
	bool									found;
	int										n;
	int										i;
	
	UNUSED_ALWAYS( inWParam );
	
	assert( inLParam );
	p = reinterpret_cast <ServiceEventInfo *> ( inLParam );
	pAutoPtr.reset( p );
	
	// Search to see if we know about this service instance. If so, remove it from the list.
	
	found = false;
	n = (int) mServiceInstances.size();
	for( i = 0; i < n; ++i )
	{
		ServiceInstanceInfo *		q;
		
		// If the name, type, domain, and interface match, treat it as the same service instance.
		
		q = &mServiceInstances[ i ];
		if( ( p->name 	== q->name ) 	&& 
			( p->type 	== q->type ) 	&& 
			( p->domain	== q->domain ) )
		{
			found = true;
			break;
		}
	}
	if( found )
	{
		mChooserList.DeleteItem( i );
		assert( i < (int) mServiceInstances.size() );
		mServiceInstances.erase( mServiceInstances.begin() + i );
	}
	return( 0 );
}

//===========================================================================================================================
//	OnResolve
//===========================================================================================================================

LONG	ChooserDialog::OnResolve( WPARAM inWParam, LPARAM inLParam )
{
	ServiceInstanceInfo *						p;
	std::auto_ptr < ServiceInstanceInfo >		pAutoPtr;
	int											selectedType;
	int											n;
	int											i;
	bool										found;
	
	UNUSED_ALWAYS( inWParam );
	
	assert( inLParam );
	p = reinterpret_cast <ServiceInstanceInfo *> ( inLParam );
	pAutoPtr.reset( p );
	
	// Make sure it is for an item of the correct type. This handles any resolves that may have been queued up.
	
	selectedType = mServiceList.GetNextItem( -1, LVNI_SELECTED );
	assert( selectedType >= 0 );
	if( selectedType >= 0 )
	{
		assert( selectedType <= (int) mServiceTypes.size() );
		if( p->type != mServiceTypes[ selectedType ].serviceType )
		{
			goto exit;
		}
	}
	
	// Search to see if we know about this service instance. If so, update its info. Otherwise, add it to the list.
	
	found = false;
	n = (int) mServiceInstances.size();
	for( i = 0; i < n; ++i )
	{
		ServiceInstanceInfo *		q;
		
		// If the name, type, domain, and interface matches, treat it as the same service instance.
				
		q = &mServiceInstances[ i ];
		if( ( p->name 	== q->name ) 	&& 
			( p->type 	== q->type ) 	&& 
			( p->domain	== q->domain ) 	&& 
			( p->ifIP 	== q->ifIP ) )
		{
			found = true;
			break;
		}
	}
	if( found )
	{
		mServiceInstances[ i ] = *p;
	}
	else
	{
		CString		s;
		
		mServiceInstances.push_back( *p );
		UTF8StringToStringObject( p->name.c_str(), s );
		mChooserList.InsertItem( n, s );
		
		UTF8StringToStringObject( p->ip.c_str(), s );
		mChooserList.SetItemText( n, 1, s );
		
		// If this is the only item, select it.
		
		if( n == 0 )
		{
			mChooserList.SetItemState( n, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
		}
	}
	UpdateInfoDisplay();

exit:
	return( 0 );
}

//===========================================================================================================================
//	StartBrowsing
//===========================================================================================================================

void	ChooserDialog::StartBrowsing( const char *inType, const char *inDomain )
{
	DNSStatus		err;
	
	assert( mServiceInstances.empty() );
	assert( mChooserList.GetItemCount() == 0 );
	assert( !mIsServiceBrowsing );
	
	mChooserList.DeleteAllItems();
	mServiceInstances.clear();
	
	mIsServiceBrowsing = true;
	err = DNSBrowserStartServiceSearch( mBrowser, kDNSBrowserFlagAutoResolve, inType, inDomain );
	assert( err == kDNSNoErr );
}

//===========================================================================================================================
//	StopBrowsing
//===========================================================================================================================

void	ChooserDialog::StopBrowsing( void )
{
	// If searching, stop.
	
	if( mIsServiceBrowsing )
	{
		DNSStatus		err;
		
		mIsServiceBrowsing = false;
		err = DNSBrowserStopServiceSearch( mBrowser, 0 );
		assert( err == kDNSNoErr );
	}
	
	// Remove all service instances.
	
	mChooserList.DeleteAllItems();
	assert( mChooserList.GetItemCount() == 0 );
	mServiceInstances.clear();
	assert( mServiceInstances.empty() );
	UpdateInfoDisplay();
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	BrowserCallBack
//===========================================================================================================================

static void
	BrowserCallBack( 
		void *					inContext, 
		DNSBrowserRef			inRef, 
		DNSStatus				inStatusCode,
		const DNSBrowserEvent *	inEvent )
{
	ChooserDialog *		dialog;
	UINT 				message;
	BOOL				posted;
	
	UNUSED_ALWAYS( inStatusCode );
	UNUSED_ALWAYS( inRef );
	
	// Check parameters.
	
	assert( inContext );
	dialog = reinterpret_cast <ChooserDialog *> ( inContext );
	
	try
	{
		switch( inEvent->type )
		{
			case kDNSBrowserEventTypeRelease:
				break;
			
			// Domains
			
			case kDNSBrowserEventTypeAddDomain:
			case kDNSBrowserEventTypeAddDefaultDomain:
			case kDNSBrowserEventTypeRemoveDomain:
			{
				DomainEventInfo *						domain;
				std::auto_ptr < DomainEventInfo >		domainAutoPtr;
				
				domain = new DomainEventInfo;
				domainAutoPtr.reset( domain );
				
				domain->eventType 	= inEvent->type;
				domain->domain 		= inEvent->data.addDomain.domain;
				domain->ifIP		= inEvent->data.addDomain.interfaceIP;
				
				message = ( inEvent->type == kDNSBrowserEventTypeRemoveDomain ) ? WM_USER_DOMAIN_REMOVE : WM_USER_DOMAIN_ADD;
				posted = ::PostMessage( dialog->GetSafeHwnd(), message, 0, (LPARAM) domain );
				assert( posted );
				if( posted )
				{
					domainAutoPtr.release();
				}
				break;
			}
			
			// Services
			
			case kDNSBrowserEventTypeAddService:
			case kDNSBrowserEventTypeRemoveService:
			{
				ServiceEventInfo *						service;
				std::auto_ptr < ServiceEventInfo >		serviceAutoPtr;
				
				service = new ServiceEventInfo;
				serviceAutoPtr.reset( service );
				
				service->eventType 	= inEvent->type;
				service->name 		= inEvent->data.addService.name;
				service->type 		= inEvent->data.addService.type;
				service->domain		= inEvent->data.addService.domain;
				service->ifIP		= inEvent->data.addService.interfaceIP;
				
				message = ( inEvent->type == kDNSBrowserEventTypeAddService ) ? WM_USER_SERVICE_ADD : WM_USER_SERVICE_REMOVE;
				posted = ::PostMessage( dialog->GetSafeHwnd(), message, 0, (LPARAM) service );
				assert( posted );
				if( posted )
				{
					serviceAutoPtr.release();
				}
				break;
			}
			
			// Resolves
			
			case kDNSBrowserEventTypeResolved:
				if( inEvent->data.resolved->address.addressType == kDNSNetworkAddressTypeIPv4  )
				{
					ServiceInstanceInfo *						serviceInstance;
					std::auto_ptr < ServiceInstanceInfo >		serviceInstanceAutoPtr;
					char										s[ 32 ];
					
					serviceInstance = new ServiceInstanceInfo;
					serviceInstanceAutoPtr.reset( serviceInstance );
					
					serviceInstance->name 		= inEvent->data.resolved->name;
					serviceInstance->type 		= inEvent->data.resolved->type;
					serviceInstance->domain		= inEvent->data.resolved->domain;
					serviceInstance->ip			= DNSNetworkAddressToString( &inEvent->data.resolved->address, s );
					serviceInstance->ifIP		= DNSNetworkAddressToString( &inEvent->data.resolved->interfaceIP, s );
					serviceInstance->text 		= inEvent->data.resolved->textRecord;
					serviceInstance->hostName	= inEvent->data.resolved->hostName;

					posted = ::PostMessage( dialog->GetSafeHwnd(), WM_USER_RESOLVE, 0, (LPARAM) serviceInstance );
					assert( posted );
					if( posted )
					{
						serviceInstanceAutoPtr.release();
					}
				}
				break;
			
			default:
				break;
		}
	}
	catch( ... )
	{
		// Don't let exceptions escape.
	}
}

//===========================================================================================================================
//	DNSNetworkAddressToString
//
//	Note: Currently only supports IPv4 network addresses.
//===========================================================================================================================

static char *	DNSNetworkAddressToString( const DNSNetworkAddress *inAddr, char *outString )
{
	const DNSUInt8 *		p;
	DNSUInt16				port;
	
	p = inAddr->u.ipv4.addr.v8;
	port = ntohs( inAddr->u.ipv4.port.v16 );
	if( port != kDNSPortInvalid )
	{
		sprintf( outString, "%u.%u.%u.%u:%u", p[ 0 ], p[ 1 ], p[ 2 ], p[ 3 ], port );
	}
	else
	{
		sprintf( outString, "%u.%u.%u.%u", p[ 0 ], p[ 1 ], p[ 2 ], p[ 3 ] );
	}
	return( outString );
}

//===========================================================================================================================
//	UTF8StringToStringObject
//===========================================================================================================================

static DWORD	UTF8StringToStringObject( const char *inUTF8, CString &inObject )
{
	DWORD		err;
	int			n;
	BSTR		unicode;
	
	unicode = NULL;
	
	n = MultiByteToWideChar( CP_UTF8, 0, inUTF8, -1, NULL, 0 );
	if( n > 0 )
	{
		unicode = (BSTR) malloc( (size_t)( n * sizeof( wchar_t ) ) );
		if( !unicode )
		{
			err = ERROR_INSUFFICIENT_BUFFER;
			goto exit;
		}

		n = MultiByteToWideChar( CP_UTF8, 0, inUTF8, -1, unicode, n );
		try
		{
			inObject = unicode;
		}
		catch( ... )
		{
			err = ERROR_NO_UNICODE_TRANSLATION;
			goto exit;
		}
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

//===========================================================================================================================
//	StringObjectToUTF8String
//===========================================================================================================================

static DWORD	StringObjectToUTF8String( CString &inObject, std::string &outUTF8 )
{
	DWORD		err;
	BSTR		unicode;
	int			nUnicode;
	int			n;
	char *		utf8;
	
	unicode = NULL;
	utf8	= NULL;
	
	nUnicode = inObject.GetLength();
	if( nUnicode > 0 )
	{
		unicode = inObject.AllocSysString();
		n = WideCharToMultiByte( CP_UTF8, 0, unicode, nUnicode, NULL, 0, NULL, NULL );
		assert( n > 0 );
		
		utf8 = (char *) malloc( (size_t) n );
		assert( utf8 );
		if( !utf8 ) { err = ERROR_INSUFFICIENT_BUFFER; goto exit; }
		
		n = WideCharToMultiByte( CP_UTF8, 0, unicode, nUnicode, utf8, n, NULL, NULL );
		assert( n > 0 );
		
		try
		{
			outUTF8.assign( utf8, n );
		}
		catch( ... )
		{
			err = ERROR_NO_UNICODE_TRANSLATION;
			goto exit;
		}
	}
	else
	{
		outUTF8.clear();
	}
	err = 0;
	
exit:
	if( unicode )
	{
		SysFreeString( unicode );
	}
	if( utf8 )
	{
		free( utf8 );
	}
	return( err );
}
