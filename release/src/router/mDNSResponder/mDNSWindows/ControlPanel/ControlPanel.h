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

$Log: ControlPanel.h,v $
Revision 1.3  2006/08/14 23:25:28  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2005/03/03 19:55:21  shersche
<rdar://problem/4034481> ControlPanel source code isn't saving CVS log info


*/

    
#pragma once

#include "stdafx.h"

//---------------------------------------------------------------------------------------------------------------------------
//	CCPApplet
//---------------------------------------------------------------------------------------------------------------------------

class CCPApplet : public CCmdTarget
{
public:

	CCPApplet( UINT nResourceID, UINT nDescriptionID, CRuntimeClass* pUIClass );

	virtual ~CCPApplet();

protected:

	virtual LRESULT OnRun(CWnd* pParentWnd);
	virtual LRESULT OnStartParms(CWnd* pParentWnd, LPCTSTR lpszExtra);
	virtual LRESULT OnInquire(CPLINFO* pInfo);
	virtual LRESULT OnNewInquire(NEWCPLINFO* pInfo);
	virtual LRESULT OnSelect();
	virtual LRESULT OnStop();

	CRuntimeClass	*	m_uiClass;
	UINT				m_resourceId;
	UINT				m_descId;
	CString				m_name;
	int					m_pageNumber;
  
	friend class CCPApp;

	DECLARE_DYNAMIC(CCPApplet);
};


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApp
//---------------------------------------------------------------------------------------------------------------------------

class CCPApp : public CWinApp
{
public:

	CCPApp();
	virtual ~CCPApp();

	void AddApplet( CCPApplet* pApplet );

protected:

	CList<CCPApplet*, CCPApplet*&> m_applets;

	friend LONG APIENTRY
	CPlApplet(HWND hWndCPl, UINT uMsg, LONG lParam1, LONG lParam2);

	virtual LRESULT OnCplMsg(HWND hWndCPl, UINT msg, LPARAM lp1, LPARAM lp2);
	virtual LRESULT OnInit();
	virtual LRESULT OnExit();

	DECLARE_DYNAMIC(CCPApp);
};


CCPApp * GetControlPanelApp();
