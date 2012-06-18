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

$Log: ThirdPage.cpp,v $
Revision 1.5  2006/08/14 23:25:29  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.4  2005/10/05 20:46:50  herscher
<rdar://problem/4192011> Move Wide-Area preferences to another part of the registry so they don't removed during an update-install.

Revision 1.3  2005/03/07 18:27:42  shersche
<rdar://problem/4037940> Fix problem when ControlPanel commits changes to the browse domain list

Revision 1.2  2005/03/03 19:55:22  shersche
<rdar://problem/4034481> ControlPanel source code isn't saving CVS log info


*/

#include "ThirdPage.h"
#include "resource.h"

#include "ConfigPropertySheet.h"
#include "SharedSecret.h"

#include <WinServices.h>
    
#define MAX_KEY_LENGTH 255


IMPLEMENT_DYNCREATE(CThirdPage, CPropertyPage)


//---------------------------------------------------------------------------------------------------------------------------
//	CThirdPage::CThirdPage
//---------------------------------------------------------------------------------------------------------------------------

CThirdPage::CThirdPage()
:
	CPropertyPage(CThirdPage::IDD)
{
	//{{AFX_DATA_INIT(CThirdPage)
	//}}AFX_DATA_INIT

	m_firstTime = true;
}


//---------------------------------------------------------------------------------------------------------------------------
//	CThirdPage::~CThirdPage
//---------------------------------------------------------------------------------------------------------------------------

CThirdPage::~CThirdPage()
{
}


//---------------------------------------------------------------------------------------------------------------------------
//	CThirdPage::DoDataExchange
//---------------------------------------------------------------------------------------------------------------------------

void CThirdPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CThirdPage)
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_BROWSE_LIST, m_browseListCtrl);
	DDX_Control(pDX, IDC_REMOVE_BROWSE_DOMAIN, m_removeButton);
}

BEGIN_MESSAGE_MAP(CThirdPage, CPropertyPage)
	//{{AFX_MSG_MAP(CThirdPage)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_ADD_BROWSE_DOMAIN, OnBnClickedAddBrowseDomain)
	ON_BN_CLICKED(IDC_REMOVE_BROWSE_DOMAIN, OnBnClickedRemoveBrowseDomain)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_BROWSE_LIST, OnLvnItemchangedBrowseList)
END_MESSAGE_MAP()


//---------------------------------------------------------------------------------------------------------------------------
//	CThirdPage::SetModified
//---------------------------------------------------------------------------------------------------------------------------

void CThirdPage::SetModified( BOOL bChanged )
{
	m_modified = bChanged;

	CPropertyPage::SetModified( bChanged );
}


//---------------------------------------------------------------------------------------------------------------------------
//	CThirdPage::OnSetActive
//---------------------------------------------------------------------------------------------------------------------------

BOOL
CThirdPage::OnSetActive()
{
	CConfigPropertySheet	*	psheet;
	HKEY						key = NULL;
	HKEY						subKey = NULL;
	DWORD						dwSize;
	DWORD						enabled;
	DWORD						err;
	TCHAR						subKeyName[MAX_KEY_LENGTH];
	DWORD						cSubKeys = 0;
	DWORD						cbMaxSubKey;
	DWORD						cchMaxClass;
	int							nIndex;
    DWORD						i; 
	BOOL						b = CPropertyPage::OnSetActive();

	psheet = reinterpret_cast<CConfigPropertySheet*>(GetParent());
	require_quiet( psheet, exit );
	
	m_modified = FALSE;

	if ( m_firstTime )
	{
		m_browseListCtrl.SetExtendedStyle((m_browseListCtrl.GetStyle() & (~LVS_EX_GRIDLINES))|LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

		m_browseListCtrl.InsertColumn(0, L"", LVCFMT_LEFT, 20 );
		m_browseListCtrl.InsertColumn(1, L"", LVCFMT_LEFT, 345);

		m_firstTime = false;
	}

	m_initialized = false;

	// Clear out what's there

	m_browseListCtrl.DeleteAllItems();

	// Now populate the browse domain box

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode L"\\DynDNS\\Setup\\" kServiceDynDNSBrowseDomains, &key );
	require_noerr( err, exit );

	// Get information about this node

    err = RegQueryInfoKey( key, NULL, NULL, NULL, &cSubKeys, &cbMaxSubKey, &cchMaxClass, NULL, NULL, NULL, NULL, NULL );       
	require_noerr( err, exit );

	for ( i = 0; i < cSubKeys; i++)
	{	
		dwSize = MAX_KEY_LENGTH;
            
		err = RegEnumKeyEx( key, i, subKeyName, &dwSize, NULL, NULL, NULL, NULL );
		require_noerr( err, exit );

		err = RegOpenKey( key, subKeyName, &subKey );
		require_noerr( err, exit );

		dwSize = sizeof( DWORD );
		err = RegQueryValueEx( subKey, L"Enabled", NULL, NULL, (LPBYTE) &enabled, &dwSize );
		require_noerr( err, exit );

		nIndex = m_browseListCtrl.InsertItem( m_browseListCtrl.GetItemCount(), L"");
		m_browseListCtrl.SetItemText( nIndex, 1, subKeyName );
		m_browseListCtrl.SetCheck( nIndex, enabled );

		RegCloseKey( subKey );
		subKey = NULL;
    }

	m_browseListCtrl.SortItems( SortFunc, (DWORD_PTR) this );

	m_removeButton.EnableWindow( FALSE );
 
exit:

	if ( subKey )
	{
		RegCloseKey( subKey );
	}

	if ( key )
	{
		RegCloseKey( key );
	}

	m_initialized = true;

	return b;
}

 

//---------------------------------------------------------------------------------------------------------------------------
//	CThirdPage::OnOK
//---------------------------------------------------------------------------------------------------------------------------

void
CThirdPage::OnOK()
{
	if ( m_modified )
	{
		Commit();
	}
}



//---------------------------------------------------------------------------------------------------------------------------
//	CThirdPage::Commit
//---------------------------------------------------------------------------------------------------------------------------

void
CThirdPage::Commit()
{
	HKEY		key		= NULL;
	HKEY		subKey	= NULL;
	TCHAR		subKeyName[MAX_KEY_LENGTH];
	DWORD		cSubKeys = 0;
	DWORD		cbMaxSubKey;
	DWORD		cchMaxClass;
	DWORD		dwSize;
	int			i;
	DWORD		err;

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode L"\\DynDNS\\Setup\\" kServiceDynDNSBrowseDomains, &key );
	require_noerr( err, exit );

	// First, remove all the entries that are there

    err = RegQueryInfoKey( key, NULL, NULL, NULL, &cSubKeys, &cbMaxSubKey, &cchMaxClass, NULL, NULL, NULL, NULL, NULL );       
	require_noerr( err, exit );

	for ( i = 0; i < (int) cSubKeys; i++ )
	{	
		dwSize = MAX_KEY_LENGTH;
            
		err = RegEnumKeyEx( key, 0, subKeyName, &dwSize, NULL, NULL, NULL, NULL );
		require_noerr( err, exit );
			
		err = RegDeleteKey( key, subKeyName );
		require_noerr( err, exit );
	}

	// Now re-populate

	for ( i = 0; i < m_browseListCtrl.GetItemCount(); i++ )
	{
		DWORD enabled = (DWORD) m_browseListCtrl.GetCheck( i );

		err = RegCreateKey( key, m_browseListCtrl.GetItemText( i, 1 ), &subKey );
		require_noerr( err, exit );

		err = RegSetValueEx( subKey, L"Enabled", NULL, REG_DWORD, (LPBYTE) &enabled, sizeof( enabled ) );
		require_noerr( err, exit );

		RegCloseKey( subKey );
		subKey = NULL;
	}
	
exit:

	if ( subKey )
	{
		RegCloseKey( subKey );
	}

	if ( key )
	{
		RegCloseKey( key );
	}
}



//---------------------------------------------------------------------------------------------------------------------------
//	CThirdPage::OnBnClickedAddBrowseDomain
//---------------------------------------------------------------------------------------------------------------------------

void
CThirdPage::OnBnClickedAddBrowseDomain()
{
	CAddBrowseDomain dlg( GetParent() );

	if ( ( dlg.DoModal() == IDOK ) && ( dlg.m_text.GetLength() > 0 ) )
	{
		int nIndex;

		nIndex = m_browseListCtrl.InsertItem( m_browseListCtrl.GetItemCount(), L"");
		m_browseListCtrl.SetItemText( nIndex, 1, dlg.m_text );
		m_browseListCtrl.SetCheck( nIndex, 1 );

		m_browseListCtrl.SortItems( SortFunc, (DWORD_PTR) this );

		m_browseListCtrl.Invalidate();

		SetModified( TRUE );
	}
}


//---------------------------------------------------------------------------------------------------------------------------
//	CThirdPage::OnBnClickedRemoveBrowseDomain
//---------------------------------------------------------------------------------------------------------------------------

void
CThirdPage::OnBnClickedRemoveBrowseDomain()
{
	UINT	selectedCount = m_browseListCtrl.GetSelectedCount();
	int		nItem = -1;
	UINT	i;

	// Update all of the selected items.

	for ( i = 0; i < selectedCount; i++ )
	{
		nItem = m_browseListCtrl.GetNextItem( -1, LVNI_SELECTED );
		check( nItem != -1 );

		m_browseListCtrl.DeleteItem( nItem );

		SetModified( TRUE );
	}

	m_removeButton.EnableWindow( FALSE );
}


void
CThirdPage::OnLvnItemchangedBrowseList(NMHDR *pNMHDR, LRESULT *pResult)
{
	if ( m_browseListCtrl.GetSelectedCount() )
	{
		m_removeButton.EnableWindow( TRUE );
	}

	if ( m_initialized )
	{
		NM_LISTVIEW * pNMListView = (NM_LISTVIEW*)pNMHDR; 
	 
		BOOL bPrevState = (BOOL) ( ( ( pNMListView->uOldState & LVIS_STATEIMAGEMASK ) >> 12 ) - 1 ); 
	 
		if ( bPrevState < 0 )
		{
			bPrevState = 0;
		}


		BOOL bChecked = ( BOOL ) ( ( ( pNMListView->uNewState & LVIS_STATEIMAGEMASK ) >> 12) - 1 ); 
	 
		if ( bChecked < 0 )
		{
			bChecked = 0;
		}

		if ( bPrevState != bChecked )
		{
			SetModified( TRUE );
		}
	}

	*pResult = 0;
}



int CALLBACK 
CThirdPage::SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CString str1;
	CString	str2;
	int		ret = 0;

	CThirdPage * self = reinterpret_cast<CThirdPage*>( lParamSort );
	require_quiet( self, exit );

	str1 = self->m_browseListCtrl.GetItemText( (int) lParam1, 1 );
	str2 = self->m_browseListCtrl.GetItemText( (int) lParam2, 1 );

	ret = str1.Compare( str2 );

exit:

	return ret;
}


// CAddBrowseDomain dialog

IMPLEMENT_DYNAMIC(CAddBrowseDomain, CDialog)
CAddBrowseDomain::CAddBrowseDomain(CWnd* pParent /*=NULL*/)
	: CDialog(CAddBrowseDomain::IDD, pParent)
{
}

CAddBrowseDomain::~CAddBrowseDomain()
{
}

void CAddBrowseDomain::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO1, m_comboBox);
}


BOOL
CAddBrowseDomain::OnInitDialog()
{
	CConfigPropertySheet	*	psheet;
	CConfigPropertySheet::StringList::iterator		it;
	
	BOOL b = CDialog::OnInitDialog();

	psheet = reinterpret_cast<CConfigPropertySheet*>(GetParent());
	require_quiet( psheet, exit );

	for ( it = psheet->m_browseDomains.begin(); it != psheet->m_browseDomains.end(); it++ )
	{
		CString text = *it;

		if ( m_comboBox.FindStringExact( -1, *it ) == CB_ERR )
		{
			m_comboBox.AddString( *it );
		}
	}

exit:

	return b;
}


void
CAddBrowseDomain::OnOK()
{
	m_comboBox.GetWindowText( m_text );

	CDialog::OnOK();
}



BEGIN_MESSAGE_MAP(CAddBrowseDomain, CDialog)
END_MESSAGE_MAP()

