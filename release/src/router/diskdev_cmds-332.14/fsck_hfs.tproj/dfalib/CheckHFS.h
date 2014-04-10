/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* External API to CheckHFS */

enum {
	kNeverCheck = 0,	/* never check (clean/dirty status only) */
	kDirtyCheck = 1,	/* only check if dirty */
	kAlwaysCheck = 2,	/* always check */
	kPartialCheck = 3,	/* used with kForceRepairs in order to set up environment */
	kForceCheck = 4,

	kNeverRepair = 0,	/* never repair */
	kMinorRepairs = 1,	/* only do minor repairs (fsck preen) */
	kMajorRepairs = 2,	/* do all possible repairs */
	kForceRepairs = 3,	/* force a repair of catalog B-Tree */
	
	kNeverLog = 0,
	kFatalLog = 1,		/* (fsck preen) */
	kVerboseLog = 2,	/* (Disk First Aid) */
	kDebugLog = 3
};

enum {
	R_NoMem			= 1,	/* not enough memory to do scavenge */
	R_IntErr		= 2,	/* internal Scavenger error */
	R_NoVol			= 3,	/* no volume in drive */
	R_RdErr			= 4,	/* unable to read from disk */
	R_WrErr			= 5,	/* unable to write to disk */
	R_BadSig		= 6,	/* not HFS/HFS+ signature */
	R_VFail			= 7,	/* verify failed */
	R_RFail			= 8,	/* repair failed */
	R_UInt			= 9,	/* user interrupt */
	R_Modified		= 10,	/* volume modifed by another app */
	R_BadVolumeHeader	= 11,	/* Invalid VolumeHeader */
	R_FileSharingIsON	= 12,	/* File Sharing is on */
	R_Dirty			= 13,	/* Dirty, but no checks were done */

	Max_RCode		= 13	/* maximum result code */
};

extern int gGUIControl;

extern int CheckHFS(	int fsReadRef, int fsWriteRef, 
						int checkLevel, int repairLevel, 
						int logLevel, int guiControl, 
						int lostAndFoundMode, int canWrite,
						int *modified  );
