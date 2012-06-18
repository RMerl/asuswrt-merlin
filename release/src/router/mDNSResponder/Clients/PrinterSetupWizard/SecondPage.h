/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 1997-2004 Apple Computer, Inc. All rights reserved.
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
    
$Log: SecondPage.h,v $
Revision 1.9  2006/08/14 23:24:09  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.8  2005/03/20 20:08:37  shersche
<rdar://problem/4055670> Second screen should not select a printer by default

Revision 1.7  2005/01/31 23:54:30  shersche
<rdar://problem/3947508> Start browsing when printer wizard starts. Move browsing logic from CSecondPage object to CPrinterSetupWizardSheet object.

Revision 1.6  2005/01/04 21:09:14  shersche
Fix problems in parsing text records. Fix problems in remove event handling. Ensure that the same service can't be resolved more than once.

Revision 1.5  2004/12/31 07:25:27  shersche
Tidy up printer management, and fix memory leaks when hitting 'Cancel'

Revision 1.4  2004/12/30 01:02:46  shersche
<rdar://problem/3734478> Add Printer information box that displays description and location information when printer name is selected
Bug #: 3734478

Revision 1.3  2004/12/29 18:53:38  shersche
<rdar://problem/3725106>
<rdar://problem/3737413> Added support for LPR and IPP protocols as well as support for obtaining multiple text records. Reorganized and simplified codebase.
Bug #: 3725106, 3737413

Revision 1.2  2004/09/13 21:23:42  shersche
<rdar://problem/3796483> Add moreComing argument to OnAddPrinter and OnRemovePrinter callbacks
Bug #: 3796483

Revision 1.1  2004/06/18 04:36:57  rpantos
First checked in


*/

#pragma once

#include "PrinterSetupWizardSheet.h"
#include "CommonServices.h"
#include "UtilTypes.h"
#include "afxcmn.h"
#include "dns_sd.h"
#include "afxwin.h"
#include <map>

using namespace PrinterSetupWizard;

// CSecondPage dialog

class CSecondPage : public CPropertyPage
{
	DECLARE_DYNAMIC(CSecondPage)

public:
	CSecondPage();
	virtual ~CSecondPage();

// Dialog Data
	enum { IDD = IDD_SECOND_PAGE };

protected:

	void		 InitBrowseList();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg BOOL OnSetCursor(CWnd * pWnd, UINT nHitTest, UINT message);
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();

	DECLARE_MESSAGE_MAP()

public:

	HTREEITEM		m_emptyListItem;
	bool			m_selectOkay;
	CTreeCtrl		m_browseList;
	bool			m_initialized;
	bool			m_waiting;

	afx_msg void	OnTvnSelchangedBrowseList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void	OnNmClickBrowseList(NMHDR * pNMHDR, LRESULT * pResult);
	afx_msg void	OnTvnKeyDownBrowseList(NMHDR * pNMHDR, LRESULT * pResult );

	OSStatus
	OnAddPrinter(
			Printer		*	printer,
			bool			moreComing);

	OSStatus
	OnRemovePrinter(
			Printer		*	printer,
			bool			moreComing);

	void
	OnResolveService( Service * service );

private:

	void
	LoadTextAndDisableWindow( CString & text );
	
	void
	SetPrinterInformationState( BOOL state );

	std::string		m_selectedName;


private:



	CStatic m_printerInformation;

	CStatic m_descriptionLabel;

	CStatic m_descriptionField;

	CStatic m_locationLabel;

	CStatic m_locationField;


	bool	m_gotChoice;
};
