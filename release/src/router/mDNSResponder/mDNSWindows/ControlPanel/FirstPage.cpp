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

$Log: FirstPage.cpp,v $
Revision 1.7  2009/06/22 23:25:10  herscher
<rdar://problem/5265747> ControlPanel doesn't display key and password in dialog box. Refactor Lsa calls into Secret.h and Secret.c, which is used by both the ControlPanel and mDNSResponder system service.

Revision 1.6  2006/08/14 23:25:28  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.5  2005/10/05 20:46:50  herscher
<rdar://problem/4192011> Move Wide-Area preferences to another part of the registry so they don't removed during an update-install.

Revision 1.4  2005/04/05 03:52:14  shersche
<rdar://problem/4066485> Registering with shared secret key doesn't work. Additionally, mDNSResponder wasn't dynamically re-reading it's DynDNS setup after setting a shared secret key.

Revision 1.3  2005/03/07 18:27:42  shersche
<rdar://problem/4037940> Fix problem when ControlPanel commits changes to the browse domain list

Revision 1.2  2005/03/03 19:55:22  shersche
<rdar://problem/4034481> ControlPanel source code isn't saving CVS log info


*/

#include "FirstPage.h"
#include "resource.h"

#include "ConfigPropertySheet.h"
#include "SharedSecret.h"

#define MAX_KEY_LENGTH 255


IMPLEMENT_DYNCREATE(CFirstPage, CPropertyPage)

//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::CFirstPage
//---------------------------------------------------------------------------------------------------------------------------

CFirstPage::CFirstPage()
:
	CPropertyPage(CFirstPage::IDD),
	m_ignoreHostnameChange( false ),
	m_statusKey( NULL ),
	m_setupKey( NULL )
{
	//{{AFX_DATA_INIT(CFirstPage)
	//}}AFX_DATA_INIT

	OSStatus err;

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode L"\\DynDNS\\State\\Hostnames", &m_statusKey );
	check_noerr( err );

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode L"\\DynDNS\\Setup\\Hostnames", &m_setupKey );
	check_noerr( err );
}

CFirstPage::~CFirstPage()
{
	if ( m_statusKey )
	{
		RegCloseKey( m_statusKey );
		m_statusKey = NULL;
	}

	if ( m_setupKey )
	{
		RegCloseKey( m_setupKey );
		m_setupKey = NULL;
	}
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::DoDataExchange
//---------------------------------------------------------------------------------------------------------------------------

void CFirstPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFirstPage)
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_EDIT1, m_hostnameControl);
	DDX_Control(pDX, IDC_FAILURE, m_failureIcon);
	DDX_Control(pDX, IDC_SUCCESS, m_successIcon);
}

BEGIN_MESSAGE_MAP(CFirstPage, CPropertyPage)
	//{{AFX_MSG_MAP(CFirstPage)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedSharedSecret)
	ON_EN_CHANGE(IDC_EDIT1, OnEnChangeHostname)
END_MESSAGE_MAP()


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::OnEnChangedHostname
//---------------------------------------------------------------------------------------------------------------------------

void CFirstPage::OnEnChangeHostname()
{
	if ( !m_ignoreHostnameChange )
	{
		SetModified( TRUE );
	}
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::OnBnClickedSharedSecret
//---------------------------------------------------------------------------------------------------------------------------

void CFirstPage::OnBnClickedSharedSecret()
{
	CString name;

	m_hostnameControl.GetWindowText( name );

	CSharedSecret dlg;

	dlg.Load( name );

	if ( dlg.DoModal() == IDOK )
	{
		DWORD		wakeup = 0;
		DWORD		dwSize = sizeof( DWORD );
		OSStatus	err;

		dlg.Commit( name );

		// We have now updated the secret, however the system service
		// doesn't know about it yet.  So we're going to update the
		// registry with a dummy value which will cause the system
		// service to re-initialize it's DynDNS setup
		//

		RegQueryValueEx( m_setupKey, L"Wakeup", NULL, NULL, (LPBYTE) &wakeup, &dwSize );      

		wakeup++;
		
		err = RegSetValueEx( m_setupKey, L"Wakeup", 0, REG_DWORD, (LPBYTE) &wakeup, sizeof( DWORD ) );
		require_noerr( err, exit );
	}

exit:

	return;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::SetModified
//---------------------------------------------------------------------------------------------------------------------------

void CFirstPage::SetModified( BOOL bChanged )
{
	m_modified = bChanged ? true : false;

	CPropertyPage::SetModified( bChanged );
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::OnSetActive
//---------------------------------------------------------------------------------------------------------------------------

BOOL
CFirstPage::OnSetActive()
{
	TCHAR	name[kDNSServiceMaxDomainName + 1];
	DWORD	nameLen = ( kDNSServiceMaxDomainName + 1 ) * sizeof( TCHAR );
	DWORD	err;

	BOOL b = CPropertyPage::OnSetActive();

	m_modified = FALSE;

	if ( m_setupKey )
	{
		err = RegQueryValueEx( m_setupKey, L"", NULL, NULL, (LPBYTE) name, &nameLen );

		if ( !err )
		{
			m_ignoreHostnameChange = true;
			m_hostnameControl.SetWindowText( name );
			m_ignoreHostnameChange = false;
		}
	}

	// Check the status of this hostname

	err = CheckStatus();
	check_noerr( err );

	return b;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::OnOK
//---------------------------------------------------------------------------------------------------------------------------

void
CFirstPage::OnOK()
{
	if ( m_modified )
	{
		Commit();
	}
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::Commit
//---------------------------------------------------------------------------------------------------------------------------

void
CFirstPage::Commit()
{
	DWORD	enabled = 1;
	CString	name;
	DWORD	err;

	m_hostnameControl.GetWindowText( name );

	// Convert to lower case

	name.MakeLower();

	// Remove trailing dot

	name.TrimRight( '.' );

	err = RegSetValueEx( m_setupKey, L"", 0, REG_SZ, (LPBYTE) (LPCTSTR) name, ( name.GetLength() + 1 ) * sizeof( TCHAR ) );
	require_noerr( err, exit );
	
	err = RegSetValueEx( m_setupKey, L"Enabled", 0, REG_DWORD, (LPBYTE) &enabled, sizeof( DWORD ) );
	require_noerr( err, exit );

exit:

	return;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::CheckStatus
//---------------------------------------------------------------------------------------------------------------------------

OSStatus
CFirstPage::CheckStatus()
{
	DWORD		status = 0;
	DWORD		dwSize = sizeof( DWORD );
	OSStatus	err;

	// Get the status field 

	err = RegQueryValueEx( m_statusKey, L"Status", NULL, NULL, (LPBYTE) &status, &dwSize );      
	require_noerr( err, exit );

	ShowStatus( status );

exit:

	return kNoErr;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::ShowStatus
//---------------------------------------------------------------------------------------------------------------------------

void
CFirstPage::ShowStatus( DWORD status )
{
	if ( status )
	{
		m_failureIcon.ShowWindow( SW_HIDE );
		m_successIcon.ShowWindow( SW_SHOW );
	}
	else
	{
		m_failureIcon.ShowWindow( SW_SHOW );
		m_successIcon.ShowWindow( SW_HIDE );
	}
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFirstPage::OnRegistryChanged
//---------------------------------------------------------------------------------------------------------------------------

void
CFirstPage::OnRegistryChanged()
{
	CheckStatus();
}
