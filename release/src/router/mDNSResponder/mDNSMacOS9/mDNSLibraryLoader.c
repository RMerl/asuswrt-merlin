/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

$Log: mDNSLibraryLoader.c,v $
Revision 1.2  2006/08/14 23:24:29  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.1  2004/03/12 21:30:26  cheshire
Build a System-Context Shared Library from mDNSCore, for the benefit of developers
like Muse Research who want to be able to use mDNS/DNS-SD from GPL-licensed code.

 */

#include <Resources.h>
#include <CodeFragments.h>
#include "ShowInitIcon.h"

extern pascal OSErr FragRegisterFileLibs(ConstFSSpecPtr fss, Boolean unregister);

extern void main(void)
	{
	OSStatus err;
	FCBPBRec fcbPB;
	FSSpec fss;

	// 1. Show our "icon march" icon
	ShowInitIcon(128, true);

	// 2. Find our FSSpec
	fss.name[0] = 0;
	fcbPB.ioNamePtr = fss.name;
	fcbPB.ioVRefNum = 0;
	fcbPB.ioRefNum = (short)CurResFile();
	fcbPB.ioFCBIndx = 0;
	err = PBGetFCBInfoSync(&fcbPB);

	// 3. Tell CFM that we're a CFM library container file	
	fss.vRefNum = fcbPB.ioFCBVRefNum;
	fss.parID = fcbPB.ioFCBParID;
	if (err == noErr) err = FragRegisterFileLibs(&fss, false);

	// 4. Now that CFM knows we're a library container, tell it to go and get our library
	if (err == noErr)
		{
		CFragConnectionID c;
		Ptr m;
		Str255 e;
		THz oldZone = GetZone();
		SetZone(SystemZone());
		err = GetSharedLibrary("\pDarwin;mDNS", kPowerPCCFragArch, kLoadCFrag, &c, &m, e);
		SetZone(oldZone);
		}
	}

// There's no CFM stub library for the FragRegisterFileLibs() call, so we'll make our own
#if __ide_target("FragRegisterFileLibsStub")
#pragma export on
pascal OSErr FragRegisterFileLibs(ConstFSSpecPtr fss, Boolean unregister)
	{
	(void)fss;			// Unused
	(void)unregister;	// Unused
	return(0);
	}
#endif
