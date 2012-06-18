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
    
$Log: BrowserDialog.h,v $
Revision 1.3  2006/08/14 23:25:55  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2004/07/13 21:24:27  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.1  2004/06/18 04:04:37  rpantos
Move up one level

Revision 1.4  2004/01/30 02:56:33  bradley
Updated to support full Unicode display. Added support for all services on www.dns-sd.org.

Revision 1.3  2003/10/14 03:28:50  bradley
Insert services in sorted order to make them easier to find. Defer service adds/removes to the main
thread to avoid potential problems with multi-threaded MFC message map access. Added some asserts.

Revision 1.2  2003/10/10 03:43:34  bradley
Added support for launching a web browser to go to the browsed web site on a single-tap.

Revision 1.1  2003/08/21 02:16:10  bradley
DNSServiceBrowser for HTTP services for Windows CE/PocketPC.

*/

#if !defined(AFX_BROWSERDIALOG_H__DECC5C82_C1C6_4630_B8D5_E1DDE570A061__INCLUDED_)
#define AFX_BROWSERDIALOG_H__DECC5C82_C1C6_4630_B8D5_E1DDE570A061__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include	"afxtempl.h"
#include	"Resource.h"

#include	"DNSServices.h"

//===========================================================================================================================
//	BrowserDialog
//===========================================================================================================================

class	BrowserDialog : public CDialog
{
	public:
		
		BrowserDialog( CWnd *inParent = NULL );
		
		//{{AFX_DATA(BrowserDialog)
		enum { IDD = IDD_APPLICATION_DIALOG };
		CListCtrl	mBrowserList;
		//}}AFX_DATA

		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(BrowserDialog)
		protected:
		virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
		//}}AFX_VIRTUAL
		
		static void
			OnBrowserCallBack( 
				void *					inContext, 
				DNSBrowserRef			inRef, 
				DNSStatus				inStatusCode,  
				const DNSBrowserEvent *	inEvent );
		
	protected:
		
		struct	BrowserEntry
		{
			CString		name;
			CString		ip;
			CString		text;
		};
		
		HICON										mIcon;
		DNSBrowserRef								mBrowser;
		CArray < BrowserEntry, BrowserEntry >		mBrowserEntries;
		
		// Generated message map functions
		//{{AFX_MSG(BrowserDialog)
		virtual BOOL OnInitDialog();
		afx_msg void OnBrowserListDoubleClick(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg LONG OnServiceAdd( WPARAM inWParam, LPARAM inLParam );
		afx_msg LONG OnServiceRemove( WPARAM inWParam, LPARAM inLParam );
		//}}AFX_MSG
		DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft eMbedded Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BROWSERDIALOG_H__DECC5C82_C1C6_4630_B8D5_E1DDE570A061__INCLUDED_)
