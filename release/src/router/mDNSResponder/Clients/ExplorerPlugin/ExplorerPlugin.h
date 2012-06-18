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
    
$Log: ExplorerPlugin.h,v $
Revision 1.5  2009/03/30 18:52:02  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/5187308> Move build train to Visual Studio 2005

Revision 1.4  2006/08/14 23:24:00  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.3  2005/01/25 18:35:38  shersche
Declare APIs for obtaining handles to resource modules

Revision 1.2  2004/07/13 21:24:21  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.1  2004/06/18 04:34:59  rpantos
Move to Clients from mDNSWindows

Revision 1.1  2004/01/30 03:01:56  bradley
Explorer Plugin to browse for DNS-SD advertised Web and FTP servers from within Internet Explorer.

*/

#pragma once

//===========================================================================================================================
//	Globals
//===========================================================================================================================

// {9999A076-A9E2-4c99-8A2B-632FC9429223}
DEFINE_GUID(CLSID_ExplorerBar, 
0x9999a076, 0xa9e2, 0x4c99, 0x8a, 0x2b, 0x63, 0x2f, 0xc9, 0x42, 0x92, 0x23);

extern HINSTANCE		gInstance;
extern int				gDLLRefCount;
extern HINSTANCE		GetNonLocalizedResources();
extern HINSTANCE		GetLocalizedResources();


class CExplorerPluginApp : public CWinApp
{
public:

	CExplorerPluginApp();
	virtual ~CExplorerPluginApp();

protected:

	virtual BOOL    InitInstance();
	virtual int     ExitInstance();

	DECLARE_DYNAMIC(CExplorerPluginApp);
};
