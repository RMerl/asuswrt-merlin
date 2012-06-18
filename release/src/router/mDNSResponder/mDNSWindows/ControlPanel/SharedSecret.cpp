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

$Log: SharedSecret.cpp,v $
Revision 1.7  2009/06/22 23:25:11  herscher
<rdar://problem/5265747> ControlPanel doesn't display key and password in dialog box. Refactor Lsa calls into Secret.h and Secret.c, which is used by both the ControlPanel and mDNSResponder system service.

Revision 1.6  2007/06/12 20:06:06  herscher
<rdar://problem/5263387> ControlPanel was inadvertently adding a trailing dot to all key names.

Revision 1.5  2006/08/14 23:25:28  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.4  2005/10/18 06:13:41  herscher
<rdar://problem/4192119> Prepend "$" to key name to ensure that secure updates work if the domain name and key name are the same

Revision 1.3  2005/04/06 02:04:49  shersche
<rdar://problem/4066485> Registering with shared secret doesn't work

Revision 1.2  2005/03/03 19:55:22  shersche
<rdar://problem/4034481> ControlPanel source code isn't saving CVS log info


*/

    
// SharedSecret.cpp : implementation file
//


#include <Secret.h>
#include "stdafx.h"
#include "SharedSecret.h"
#include <WinServices.h>

#include <DebugServices.h>


// SharedSecret dialog

IMPLEMENT_DYNAMIC(CSharedSecret, CDialog)


//---------------------------------------------------------------------------------------------------------------------------
//	CSharedSecret::CSharedSecret
//---------------------------------------------------------------------------------------------------------------------------

CSharedSecret::CSharedSecret(CWnd* pParent /*=NULL*/)
	: CDialog(CSharedSecret::IDD, pParent)
	, m_key(_T(""))
	, m_secret(_T(""))
{
}


//---------------------------------------------------------------------------------------------------------------------------
//	CSharedSecret::~CSharedSecret
//---------------------------------------------------------------------------------------------------------------------------

CSharedSecret::~CSharedSecret()
{
}


//---------------------------------------------------------------------------------------------------------------------------
//	CSharedSecret::DoDataExchange
//---------------------------------------------------------------------------------------------------------------------------

void CSharedSecret::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_KEY, m_key );
	DDX_Text(pDX, IDC_SECRET, m_secret );
}


BEGIN_MESSAGE_MAP(CSharedSecret, CDialog)
END_MESSAGE_MAP()


//---------------------------------------------------------------------------------------------------------------------------
//	CSharedSecret::Load
//---------------------------------------------------------------------------------------------------------------------------

void
CSharedSecret::Load( CString zone )
{
	char	zoneUTF8[ 256 ];
	char	outDomain[ 256 ];
	char	outKey[ 256 ];
	char	outSecret[ 256 ];

	StringObjectToUTF8String( zone, zoneUTF8, sizeof( zoneUTF8 ) );

	if ( LsaGetSecret( zoneUTF8, outDomain, sizeof( outDomain ) / sizeof( TCHAR ), outKey, sizeof( outKey ) / sizeof( TCHAR ), outSecret, sizeof( outSecret ) / sizeof( TCHAR ) ) )
	{
		m_key		= outKey;
		m_secret	= outSecret;
	}
	else
	{
		m_key = zone;
	}
}


//---------------------------------------------------------------------------------------------------------------------------
//	CSharedSecret::Commit
//---------------------------------------------------------------------------------------------------------------------------

void
CSharedSecret::Commit( CString zone )
{
	char	zoneUTF8[ 256 ];
	char	keyUTF8[ 256 ];
	char	secretUTF8[ 256 ];

	StringObjectToUTF8String( zone, zoneUTF8, sizeof( zoneUTF8 ) );
	StringObjectToUTF8String( m_key, keyUTF8, sizeof( keyUTF8 ) );
	StringObjectToUTF8String( m_secret, secretUTF8, sizeof( secretUTF8 ) );

	LsaSetSecret( zoneUTF8, keyUTF8, secretUTF8 );
}
