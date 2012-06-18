/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 1997-2004 Apple Computer, Inc. All rights reserved.
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
    
$Log: WinServices.h,v $
Revision 1.3  2009/06/22 23:25:05  herscher
<rdar://problem/5265747> ControlPanel doesn't display key and password in dialog box. Refactor Lsa calls into Secret.h and Secret.c, which is used by both the ControlPanel and mDNSResponder system service.

Revision 1.2  2006/08/14 23:25:21  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.1  2004/06/18 05:23:33  rpantos
First checked in


*/


#pragma once

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#	include <afxcmn.h>		// MFC support for Windows Common Controls
#endif

#include <winsock2.h>
#include <afxsock.h>		// MFC socket extensions
#include "CommonServices.h"


OSStatus	UTF8StringToStringObject( const char *inUTF8, CString &outObject );
OSStatus	StringObjectToUTF8String( CString &inObject, char* outUTF8, size_t outUTF8Len );
