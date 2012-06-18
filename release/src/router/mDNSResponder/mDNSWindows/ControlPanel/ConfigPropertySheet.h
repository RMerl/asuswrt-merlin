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

$Log: ConfigPropertySheet.h,v $
Revision 1.7  2009/07/01 19:20:37  herscher
<rdar://problem/6713286> UI changes for configuring sleep proxy settings.

Revision 1.6  2009/03/30 19:58:47  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/5187308> Move build train to Visual Studio 2005

Revision 1.5  2006/08/14 23:25:28  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.4  2005/03/03 19:55:21  shersche
<rdar://problem/4034481> ControlPanel source code isn't saving CVS log info


*/

#ifndef _ConfigPropertySheet_h
#define _ConfigPropertySheet_h

#include "stdafx.h"
#include "FirstPage.h"
#include "SecondPage.h"
#include "ThirdPage.h"
#include "FourthPage.h"

#include <RegNames.h>
#include <dns_sd.h>
#include <list>


//---------------------------------------------------------------------------------------------------------------------------
//	CConfigPropertySheet
//---------------------------------------------------------------------------------------------------------------------------

class CConfigPropertySheet : public CPropertySheet
{
public:

	CConfigPropertySheet();
	virtual ~CConfigPropertySheet();

	typedef std::list<CString> StringList;

	StringList	m_browseDomains;
	StringList	m_regDomains;

protected:

	CFirstPage	m_firstPage;
	CSecondPage	m_secondPage;
	CThirdPage m_thirdPage;
	CFourthPage m_fourthPage;

	//{{AFX_VIRTUAL(CConfigPropertySheet)
	//}}AFX_VIRTUAL

	DECLARE_DYNCREATE(CConfigPropertySheet)

	//{{AFX_MSG(CConfigPropertySheet)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	afx_msg BOOL	OnInitDialog();
	afx_msg BOOL	OnCommand( WPARAM wParam, LPARAM lParam );
	afx_msg LRESULT	OnDataReady( WPARAM inWParam, LPARAM inLParam );
	afx_msg LRESULT	OnRegistryChanged( WPARAM inWParam, LPARAM inLParam );
	void			OnEndDialog();

private:

	OSStatus
	SetupBrowsing();

	OSStatus
	TearDownBrowsing();

	OSStatus
	SetupRegistryNotifications();

	OSStatus
	TearDownRegistryNotifications();

	OSStatus
	DecodeDomainName( const char * raw, CString & decoded );

	const char*
	GetNextLabel( const char * cstr, char label[64] );

	static void DNSSD_API
	BrowseDomainsReply
				(
				DNSServiceRef			sdRef,
				DNSServiceFlags			flags,
				uint32_t				interfaceIndex,
				DNSServiceErrorType		errorCode,
				const char			*	replyDomain,
				void				*	context
				);

	static void DNSSD_API
	RegDomainsReply
				(
				DNSServiceRef			sdRef,
				DNSServiceFlags			flags,
				uint32_t				interfaceIndex,
				DNSServiceErrorType		errorCode,
				const char			*	replyDomain,
				void				*	context
				);

	// This thread will watch for registry changes

	static unsigned WINAPI
	WatchRegistry
				(
				LPVOID inParam
				);

	HKEY				m_statusKey;
	HANDLE				m_thread;
	HANDLE				m_threadExited;
	DNSServiceRef		m_browseDomainsRef;
	DNSServiceRef		m_regDomainsRef;
	CRITICAL_SECTION	m_lock;
};


#endif
