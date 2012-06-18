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
    
$Log: Application.h,v $
Revision 1.3  2006/08/14 23:25:55  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2004/07/13 21:24:27  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.1  2004/06/18 04:04:37  rpantos
Move up one level

Revision 1.2  2004/01/30 02:56:33  bradley
Updated to support full Unicode display. Added support for all services on www.dns-sd.org.

Revision 1.1  2003/08/21 02:16:10  bradley
DNSServiceBrowser for HTTP services for Windows CE/PocketPC.

*/

#if !defined(AFX_APPLICATION_H__E2E51302_D643_458E_A7A5_5157233D1E5C__INCLUDED_)
#define AFX_APPLICATION_H__E2E51302_D643_458E_A7A5_5157233D1E5C__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include	"Resource.h"

//===========================================================================================================================
//	Application
//===========================================================================================================================

class	Application : public CWinApp
{
	public:
		
		Application();

		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(Application)
		public:
		virtual BOOL InitInstance();
		//}}AFX_VIRTUAL

		//{{AFX_MSG(Application)
			// NOTE - the ClassWizard will add and remove member functions here.
			//    DO NOT EDIT what you see in these blocks of generated code !
		//}}AFX_MSG
		DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft eMbedded Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_APPLICATION_H__E2E51302_D643_458E_A7A5_5157233D1E5C__INCLUDED_)
