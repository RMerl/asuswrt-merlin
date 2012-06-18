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

$Log: ConfigDialog.h,v $
Revision 1.3  2006/08/14 23:25:28  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2005/03/03 19:55:21  shersche
<rdar://problem/4034481> ControlPanel source code isn't saving CVS log info


*/

#pragma once

#include "stdafx.h"
#include "resource.h"

//---------------------------------------------------------------------------------------------------------------------------
//	CConfigDialog
//---------------------------------------------------------------------------------------------------------------------------

class CConfigDialog : public CDialog
{
public:

	CConfigDialog();

protected:

	//{{AFX_DATA(CConfigDialog)
	enum { IDD = IDR_APPLET };
	//}}AFX_DATA

	//{{AFX_VIRTUAL(CConfigDialog)
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CConfigDialog)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	DECLARE_DYNCREATE(CConfigDialog)
};
