/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

$Log: stdafx.h,v $
Revision 1.2  2006/08/14 23:23:57  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.1  2005/05/20 22:01:02  bradley
Bonjour for Windows example code to browse for HTTP services and deliver via Window messages.

*/

// Standard Windows pre-compiled header file.

#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include	<windows.h>
#include	<winsock2.h>

#include	<stdlib.h>
#include	<malloc.h>
#include	<memory.h>
#include	<tchar.h>
