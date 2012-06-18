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

$Log: SharedSecret.h,v $
Revision 1.5  2009/06/22 23:25:11  herscher
<rdar://problem/5265747> ControlPanel doesn't display key and password in dialog box. Refactor Lsa calls into Secret.h and Secret.c, which is used by both the ControlPanel and mDNSResponder system service.

Revision 1.4  2006/08/14 23:25:28  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.3  2005/04/06 02:04:49  shersche
<rdar://problem/4066485> Registering with shared secret doesn't work

Revision 1.2  2005/03/03 19:55:21  shersche
<rdar://problem/4034481> ControlPanel source code isn't saving CVS log info


*/

    
#pragma once

#include "resource.h"


//---------------------------------------------------------------------------------------------------------------------------
//	CSharedSecret
//---------------------------------------------------------------------------------------------------------------------------

class CSharedSecret : public CDialog
{
	DECLARE_DYNAMIC(CSharedSecret)

public:
	CSharedSecret(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSharedSecret();

// Dialog Data
	enum { IDD = IDR_SECRET };

	void
	Load( CString zone );

	void
	Commit( CString zone );

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

public:

	CString m_key;
	CString m_secret;
};
