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
    
$Log: Application.cpp,v $
Revision 1.3  2006/08/14 23:25:49  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2004/07/13 21:24:26  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.1  2004/06/18 04:04:36  rpantos
Move up one level

Revision 1.2  2004/01/30 02:56:32  bradley
Updated to support full Unicode display. Added support for all services on www.dns-sd.org.

Revision 1.1  2003/08/21 02:06:47  bradley
Moved DNSServiceBrowser for non-Windows CE into Windows sub-folder.

Revision 1.5  2003/08/12 19:56:28  cheshire
Update to APSL 2.0

Revision 1.4  2003/07/02 21:20:06  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.3  2002/09/21 20:44:55  zarzycki
Added APSL info

Revision 1.2  2002/09/20 08:37:34  bradley
Increased the DNS record cache from the default of 64 to 512 entries for larger networks.

Revision 1.1  2002/09/20 06:12:51  bradley
DNSServiceBrowser for Windows

*/

#include	<assert.h>

#include	"StdAfx.h"

#include	"DNSServices.h"

#include	"Application.h"

#include	"ChooserDialog.h"

#include	"stdafx.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//===========================================================================================================================
//	Message Map
//===========================================================================================================================

BEGIN_MESSAGE_MAP(Application, CWinApp)
	//{{AFX_MSG_MAP(Application)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

//===========================================================================================================================
//	Globals
//===========================================================================================================================

Application		gApp;

//===========================================================================================================================
//	Application
//===========================================================================================================================

Application::Application( void )
{
	//
}

//===========================================================================================================================
//	InitInstance
//===========================================================================================================================

BOOL	Application::InitInstance()
{
	DNSStatus		err;
	
	// Standard MFC initialization.

#if( !defined( AFX_DEPRECATED ) )
	#ifdef _AFXDLL
		Enable3dControls();			// Call this when using MFC in a shared DLL
	#else
		Enable3dControlsStatic();	// Call this when linking to MFC statically
	#endif
#endif

	InitCommonControls();
	
	// Set up DNS Services.
	
	err = DNSServicesInitialize( 0, 512 );
	assert( err == kDNSNoErr );
	
	// Create the chooser dialog.
	
	ChooserDialog *		dialog;
	
	m_pMainWnd = NULL;
	dialog = new ChooserDialog;
	dialog->Create( IDD_CHOOSER_DIALOG );
	m_pMainWnd = dialog;
	dialog->ShowWindow( SW_SHOW );
	
	return( true );
}

//===========================================================================================================================
//	ExitInstance
//===========================================================================================================================

int	Application::ExitInstance( void )
{
	// Clean up DNS Services.
	
	DNSServicesFinalize();
	return( 0 );
}
