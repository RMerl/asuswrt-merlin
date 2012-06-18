/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
    
$Log: LoginDialog.cpp,v $
Revision 1.2  2006/08/14 23:25:49  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.1  2004/06/18 04:04:36  rpantos
Move up one level

Revision 1.2  2004/01/30 02:56:32  bradley
Updated to support full Unicode display. Added support for all services on www.dns-sd.org.

Revision 1.1  2003/12/25 03:47:28  bradley
Login dialog to get the username/password from the user.

*/

#include	<assert.h>
#include	<stdlib.h>

#include	"stdafx.h"

#include	"LoginDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//===========================================================================================================================
//	Message Map
//===========================================================================================================================

BEGIN_MESSAGE_MAP( LoginDialog, CDialog )
END_MESSAGE_MAP()

//===========================================================================================================================
//	LoginDialog
//===========================================================================================================================

LoginDialog::LoginDialog( CWnd *inParent )
	: CDialog( LoginDialog::IDD, inParent )
{
	//
}

//===========================================================================================================================
//	OnInitDialog
//===========================================================================================================================

BOOL	LoginDialog::OnInitDialog( void )
{
	CDialog::OnInitDialog();
	return( TRUE );
}

//===========================================================================================================================
//	DoDataExchange
//===========================================================================================================================

void	LoginDialog::DoDataExchange( CDataExchange *inDX )
{
	CDialog::DoDataExchange( inDX );
}

//===========================================================================================================================
//	OnOK
//===========================================================================================================================

void	LoginDialog::OnOK( void )
{
	const CWnd *		control;
		
	// Username
	
	control = GetDlgItem( IDC_LOGIN_USERNAME_TEXT );
	assert( control );
	if( control )
	{
		control->GetWindowText( mUsername );
	}
	
	// Password
	
	control = GetDlgItem( IDC_LOGIN_PASSWORD_TEXT );
	assert( control );
	if( control )
	{
		control->GetWindowText( mPassword );
	}
	
	CDialog::OnOK();
}

//===========================================================================================================================
//	GetLogin
//===========================================================================================================================

BOOL	LoginDialog::GetLogin( CString &outUsername, CString &outPassword )
{
	if( DoModal() == IDOK )
	{
		outUsername = mUsername;
		outPassword = mPassword;
		return( TRUE );
	}
	return( FALSE );
}
