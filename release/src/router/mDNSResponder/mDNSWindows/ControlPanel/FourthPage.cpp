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

$Log: FourthPage.cpp,v $
Revision 1.1  2009/07/01 19:20:37  herscher
<rdar://problem/6713286> UI changes for configuring sleep proxy settings.



*/

#include "FourthPage.h"
#include "resource.h"

#include "ConfigPropertySheet.h"
#include "SharedSecret.h"

#include <WinServices.h>
    
#define MAX_KEY_LENGTH 255


IMPLEMENT_DYNCREATE(CFourthPage, CPropertyPage)


//---------------------------------------------------------------------------------------------------------------------------
//	CFourthPage::CFourthPage
//---------------------------------------------------------------------------------------------------------------------------

CFourthPage::CFourthPage()
:
	CPropertyPage(CFourthPage::IDD)
{
	//{{AFX_DATA_INIT(CFourthPage)
	//}}AFX_DATA_INIT
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFourthPage::~CFourthPage
//---------------------------------------------------------------------------------------------------------------------------

CFourthPage::~CFourthPage()
{
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFourthPage::DoDataExchange
//---------------------------------------------------------------------------------------------------------------------------

void CFourthPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFourthPage)
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_POWER_MANAGEMENT, m_checkBox);
}

BEGIN_MESSAGE_MAP(CFourthPage, CPropertyPage)
	//{{AFX_MSG_MAP(CFourthPage)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_POWER_MANAGEMENT, &CFourthPage::OnBnClickedPowerManagement)
END_MESSAGE_MAP()


//---------------------------------------------------------------------------------------------------------------------------
//	CFourthPage::SetModified
//---------------------------------------------------------------------------------------------------------------------------

void CFourthPage::SetModified( BOOL bChanged )
{
	m_modified = bChanged;

	CPropertyPage::SetModified( bChanged );
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFourthPage::OnSetActive
//---------------------------------------------------------------------------------------------------------------------------

BOOL
CFourthPage::OnSetActive()
{
	CConfigPropertySheet	*	psheet;
	HKEY						key = NULL;
	DWORD						dwSize;
	DWORD						enabled;
	DWORD						err;
	BOOL						b = CPropertyPage::OnSetActive();

	psheet = reinterpret_cast<CConfigPropertySheet*>(GetParent());
	require_quiet( psheet, exit );

	m_checkBox.SetCheck( 0 );

	// Now populate the browse domain box

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode L"\\Power Management", &key );
	require_noerr( err, exit );

	dwSize = sizeof( DWORD );
	err = RegQueryValueEx( key, L"Enabled", NULL, NULL, (LPBYTE) &enabled, &dwSize );
	require_noerr( err, exit );

	m_checkBox.SetCheck( enabled );
 
exit:

	if ( key )
	{
		RegCloseKey( key );
	}

	return b;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFourthPage::OnOK
//---------------------------------------------------------------------------------------------------------------------------

void
CFourthPage::OnOK()
{
	if ( m_modified )
	{
		Commit();
	}
}



//---------------------------------------------------------------------------------------------------------------------------
//	CFourthPage::Commit
//---------------------------------------------------------------------------------------------------------------------------

void
CFourthPage::Commit()
{
	HKEY		key		= NULL;
	DWORD		enabled;
	DWORD		err;

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode L"\\Power Management", &key );
	require_noerr( err, exit );

	enabled = m_checkBox.GetCheck();
	err = RegSetValueEx( key, L"Enabled", NULL, REG_DWORD, (LPBYTE) &enabled, sizeof( enabled ) );
	require_noerr( err, exit );
	
exit:

	if ( key )
	{
		RegCloseKey( key );
	}
}


//---------------------------------------------------------------------------------------------------------------------------
//	CFourthPage::OnBnClickedRemoveBrowseDomain
//---------------------------------------------------------------------------------------------------------------------------


void CFourthPage::OnBnClickedPowerManagement()
{
	char buf[ 256 ];

	sprintf( buf, "check box: %d", m_checkBox.GetCheck() );
	OutputDebugStringA( buf );
	// TODO: Add your control notification handler code here

	SetModified( TRUE );
}
