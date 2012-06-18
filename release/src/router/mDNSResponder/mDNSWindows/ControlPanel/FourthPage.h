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

$Log: FourthPage.h,v $
Revision 1.1  2009/07/01 19:20:37  herscher
<rdar://problem/6713286> UI changes for configuring sleep proxy settings.


*/

#pragma once

#include "stdafx.h"
#include "resource.h"

#include <DebugServices.h>
#include <list>
#include "afxcmn.h"

#include "afxwin.h"





//---------------------------------------------------------------------------------------------------------------------------
//	CFourthPage
//---------------------------------------------------------------------------------------------------------------------------

class CFourthPage : public CPropertyPage
{
public:
	CFourthPage();
	~CFourthPage();

protected:

	//{{AFX_DATA(CFourthPage)
	enum { IDD = IDR_APPLET_PAGE4 };
	//}}AFX_DATA

	//{{AFX_VIRTUAL(CFourthPage)
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	DECLARE_DYNCREATE(CFourthPage)

	//{{AFX_MSG(CFourthPage)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	
private:
	
	typedef std::list<CString> StringList;

	afx_msg BOOL
	OnSetActive();
	
	afx_msg void
	OnOK();
	
	void
	SetModified( BOOL bChanged = TRUE );
	
	void
	Commit();

	BOOL			m_modified;

public:
private:

	CButton m_checkBox;

public:

	afx_msg void OnBnClickedPowerManagement();
};
