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

$Log: ControlPanel.cpp,v $
Revision 1.5  2009/03/30 20:00:19  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/5187308> Move build train to Visual Studio 2005

Revision 1.4  2007/04/27 20:42:11  herscher
<rdar://problem/5078828> mDNS: Bonjour Control Panel for Windows doesn't work on Vista

Revision 1.3  2006/08/14 23:25:28  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2005/03/03 19:55:22  shersche
<rdar://problem/4034481> ControlPanel source code isn't saving CVS log info


*/

    
#include "ControlPanel.h"
#include "ConfigDialog.h"
#include "ConfigPropertySheet.h"
#include "resource.h"

#include <DebugServices.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


static CCPApp theApp;


//---------------------------------------------------------------------------------------------------------------------------
//	GetControlPanelApp
//---------------------------------------------------------------------------------------------------------------------------

CCPApp*
GetControlPanelApp()
{
	CCPApp * pApp = (CCPApp*) AfxGetApp();

	check( pApp );
	check( pApp->IsKindOf( RUNTIME_CLASS( CCPApp ) ) );

	return pApp;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CPlApplet
//---------------------------------------------------------------------------------------------------------------------------

LONG APIENTRY
CPlApplet(HWND hWndCPl, UINT uMsg, LONG lParam1, LONG lParam2)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CCPApp * pApp = GetControlPanelApp();

	return ( LONG ) pApp->OnCplMsg(hWndCPl, uMsg, lParam1, lParam2);
}


IMPLEMENT_DYNAMIC(CCPApplet, CCmdTarget);


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApplet::CCPApplet
//---------------------------------------------------------------------------------------------------------------------------

CCPApplet::CCPApplet(UINT resourceId, UINT descId, CRuntimeClass * uiClass)
:
	m_resourceId(resourceId),
	m_descId(descId),
	m_uiClass(uiClass),
	m_pageNumber(0)
{
	check( uiClass );
	check( uiClass->IsDerivedFrom( RUNTIME_CLASS( CDialog ) ) || 
	       uiClass->IsDerivedFrom( RUNTIME_CLASS( CPropertySheet ) ) );

	m_name.LoadString(resourceId);
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApplet::~CCPApplet
//---------------------------------------------------------------------------------------------------------------------------

CCPApplet::~CCPApplet()
{
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApplet::OnStartParms
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CCPApplet::OnStartParms(CWnd * pParentWnd, LPCTSTR extra)
{
	DEBUG_UNUSED( pParentWnd );

	if ( extra )
	{
		m_pageNumber = ::_ttoi( extra ) - 1;
	}

	return 0;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApplet::OnRun
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CCPApplet::OnRun(CWnd* pParentWnd)
{
	LRESULT		lResult = 1;
	CWnd	*	pWnd;

	InitCommonControls();

	pWnd = (CWnd*) m_uiClass->CreateObject(); 

	if ( pWnd )
	{
		lResult = ERROR_SUCCESS;

		if ( pWnd->IsKindOf( RUNTIME_CLASS( CPropertySheet ) ) )
		{
			CPropertySheet * pSheet = (CPropertySheet*) pWnd;

			pSheet->Construct(m_name, pParentWnd, m_pageNumber);

			pSheet->DoModal();
		}
		else
		{
			check( pWnd->IsKindOf( RUNTIME_CLASS( CDialog ) ) );

			CDialog * pDialog = (CDialog*) pWnd;

      		pDialog->DoModal();
    	}

		delete pWnd;
  	}

	return lResult;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApplet::OnInquire
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CCPApplet::OnInquire(CPLINFO* pInfo)
{
	pInfo->idIcon = m_resourceId;
	pInfo->idName = m_resourceId;
	pInfo->idInfo = m_descId;
	pInfo->lData  = reinterpret_cast<LONG>(this);

	return 0;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApplet::OnNewInquire
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CCPApplet::OnNewInquire(NEWCPLINFO* pInfo)
{
	DEBUG_UNUSED( pInfo );

	return 1;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApplet::OnSelect
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CCPApplet::OnSelect()
{
	return 0;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApplet::OnStop
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CCPApplet::OnStop()
{
	return 0;
}


IMPLEMENT_DYNAMIC(CCPApp, CWinApp);

//---------------------------------------------------------------------------------------------------------------------------
//	CCPApp::CCPApp
//---------------------------------------------------------------------------------------------------------------------------

CCPApp::CCPApp()
{
	debug_initialize( kDebugOutputTypeWindowsEventLog, "DNS-SD Control Panel", GetModuleHandle( NULL ) );
	debug_set_property( kDebugPropertyTagPrintLevel, kDebugLevelInfo );
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApp::~CCPApp
//---------------------------------------------------------------------------------------------------------------------------

CCPApp::~CCPApp()
{
	while ( !m_applets.IsEmpty() )
	{
    	delete m_applets.RemoveHead();
	}
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApp::AddApplet
//---------------------------------------------------------------------------------------------------------------------------

void
CCPApp::AddApplet( CCPApplet * applet )
{
	check( applet );

	m_applets.AddTail( applet );
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApp::OnInit
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CCPApp::OnInit()
{
	CCPApplet * applet;

	try
	{
		applet = new CCPApplet( IDR_APPLET, IDS_APPLET_DESCRIPTION, RUNTIME_CLASS( CConfigPropertySheet ) );
	}
	catch (...)
	{
		applet = NULL;
	}
   
	require_action( applet, exit, kNoMemoryErr );
	
	AddApplet( applet );

exit:

	return m_applets.GetCount();
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApp::OnExit
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CCPApp::OnExit()
{
  return 1;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApp::OnCplMsg
//---------------------------------------------------------------------------------------------------------------------------

LRESULT
CCPApp::OnCplMsg(HWND hWndCPl, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
	LRESULT lResult = 1;

	switch ( uMsg )
	{
		case CPL_INIT:
		{
			lResult = OnInit();
		}
		break;

		case CPL_EXIT:
		{
			lResult = OnExit();
		}
		break;

		case CPL_GETCOUNT:
		{      
    		lResult = m_applets.GetCount();
  		}
		break;

		default:
  		{
    		POSITION pos = m_applets.FindIndex( lParam1 );
			check( pos );

			CCPApplet * applet = m_applets.GetAt( pos );  
			check( applet );

    		switch (uMsg)
    		{
      			case CPL_INQUIRE:
      			{
					LPCPLINFO pInfo = reinterpret_cast<LPCPLINFO>(lParam2);
        			lResult = applet->OnInquire(pInfo);
				}  
        		break;

				case CPL_NEWINQUIRE:
      			{
        			LPNEWCPLINFO pInfo = reinterpret_cast<LPNEWCPLINFO>(lParam2);
					lResult = applet->OnNewInquire(pInfo);
				}  
        		break;

				case CPL_STARTWPARMS:
      			{
        			CWnd * pParentWnd = CWnd::FromHandle(hWndCPl);
        			LPCTSTR lpszExtra = reinterpret_cast<LPCTSTR>(lParam2);
        			lResult = applet->OnStartParms(pParentWnd, lpszExtra);
				}
				break;

				case CPL_DBLCLK:
				{
        			CWnd* pParentWnd = CWnd::FromHandle(hWndCPl);
        			lResult = applet->OnRun(pParentWnd);
				}
        		break;

				case CPL_SELECT:
				{
        			lResult = applet->OnSelect();
				}
				break;

				case CPL_STOP:
				{
					lResult = applet->OnStop();
				}
				break;

				default:
				{
					// TRACE(_T("Warning, Received an unknown control panel message:%d\n"), uMsg);
					lResult = 1;
				}
				break;
    		}
  		}
		break;
	}

	return lResult;
}
