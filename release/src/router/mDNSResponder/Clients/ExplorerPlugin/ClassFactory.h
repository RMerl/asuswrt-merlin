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
    
$Log: ClassFactory.h,v $
Revision 1.3  2006/08/14 23:24:00  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2004/07/13 21:24:21  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.1  2004/06/18 04:34:59  rpantos
Move to Clients from mDNSWindows

Revision 1.1  2004/01/30 03:01:56  bradley
Explorer Plugin to browse for DNS-SD advertised Web and FTP servers from within Internet Explorer.

*/

#ifndef __CLASS_FACTORY__
#define __CLASS_FACTORY__

#include	"StdAfx.h"

//===========================================================================================================================
//	ClassFactory
//===========================================================================================================================

class	ClassFactory : public IClassFactory
{
	protected:
	
		DWORD		mRefCount;
		CLSID		mCLSIDObject;
	
	public:
	
		ClassFactory( CLSID inCLSID );
		~ClassFactory( void );
		
		// IUnknown methods
		
		STDMETHODIMP			QueryInterface( REFIID inID, LPVOID *outResult );
		STDMETHODIMP_( DWORD )	AddRef( void );
		STDMETHODIMP_( DWORD )	Release( void );

		// IClassFactory methods
		
		STDMETHODIMP	CreateInstance( LPUNKNOWN inUnknown, REFIID inID, LPVOID *outObject );
		STDMETHODIMP	LockServer( BOOL inLock );
};

#endif	// __CLASS_FACTORY__
