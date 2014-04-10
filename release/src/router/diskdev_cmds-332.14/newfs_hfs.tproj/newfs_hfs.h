/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#if LINUX
#include "missing.h"
#else
#include <CoreFoundation/CFBase.h>*/
#endif

/*
 * Mac OS Finder flags
 */
enum {
	kHasBeenInited	= 0x0100,	/* Files only */														/* Clear if the file contains desktop database */																/* bit 0x0200 was the letter bit for AOCE, but is now reserved for future use */
	kHasCustomIcon	= 0x0400,	/* Files and folders */
	kIsStationery	= 0x0800,	/* Files only */
	kNameLocked	= 0x1000,	/* Files and folders */
	kHasBundle	= 0x2000,	/* Files only */
	kIsInvisible	= 0x4000,	/* Files and folders */
	kIsAlias	= 0x8000	/* Files only */
};


/* Finder types (mostly opaque in our usage) */
struct FInfo {
	UInt32		fileType;	/* The type of the file */
	UInt32 		fileCreator;	/* The file's creator */
	UInt16 		finderFlags;	/* ex: kHasBundle, kIsInvisible... */
	UInt8 		opaque[6];															/* If set to {0, 0}, the Finder will place the item automatically */
};
typedef struct FInfo FInfo;

struct FXInfo {
	UInt8	opaque[16];
};
typedef struct FXInfo FXInfo;

struct DInfo {
	UInt8	opaque[16];
};
typedef struct DInfo DInfo;

struct DXInfo {
	UInt8	opaque[16];
};
typedef struct DXInfo DXInfo;


enum {
	kMinHFSPlusVolumeSize	= (512 * 1024),
	
	kBytesPerSector		= 512,
	kBitsPerSector		= 4096,
	kBTreeHeaderUserBytes	= 128,
	kLog2SectorSize		= 9,
	kHFSNodeSize		= 512,
	kHFSMaxAllocationBlks	= 65536,
	
	kHFSPlusDataClumpFactor	= 16,
	kHFSPlusRsrcClumpFactor = 16,

	kWriteSeqNum		= 2,
	kHeaderBlocks		= 3,
	kTailBlocks		= 2,
	kMDBStart		= 2,
	kVolBitMapStart		= kHeaderBlocks,

	/* Desktop DB, Desktop DF, Finder, System, ReadMe */
	kWapperFileCount	= 5,
	/* Maximum wrapper size is 32MB */
	kMaxWrapperSize		= 1024 * 1024 * 32,
	/* Maximum volume that can be wrapped is 256GB */
	kMaxWrapableSectors	= (kMaxWrapperSize/8) * (65536/512)
};

/* B-tree key descriptor */
#define KD_SKIP		0
#define KD_BYTE		1
#define KD_SIGNBYTE	2
#define KD_STRING	3
#define KD_WORD		4
#define KD_SIGNWORD	5
#define KD_LONG		6
#define KD_SIGNLONG	7
#define KD_FIXLENSTR	8
#define KD_DTDBSTR	9
#define KD_USEPROC	10


enum {
	kTextEncodingMacRoman = 0L,
	kTextEncodingMacJapanese = 1
};


/*
 * The following constant sets the default block size.
 * This constant must be a power of 2 and meet the following constraints:
 *	MINBSIZE <= DFL_BLKSIZE <= MAXBSIZE
 *	sectorsize <= DFL_BLKSIZE
 */
#define HFSOPTIMALBLKSIZE	4096
#define HFSMINBSIZE		512
#define	DFL_BLKSIZE		HFSOPTIMALBLKSIZE


#define kDTDF_FileID	16
#define kDTDF_Name	"Desktop DF"
#define kDTDF_Chars	10
#define kDTDF_Type	0x4454464C /* 'DTFL' */
#define kDTDF_Creator	0x444D4752 /* 'DMGR' */

#define kDTDB_FileID	17
#define kDTDB_Name	"Desktop DB"
#define kDTDB_Chars	10
#define kDTDB_Type	0x4254464C /* 'BTFL' */
#define kDTDB_Creator	0x444D4752 /* 'DMGR' */
#define kDTDB_Size	1024

#define kReadMe_FileID	18
#define kReadMe_Name	"ReadMe"
#define kReadMe_Chars	6
#define kReadMe_Type	0x7474726F /* 'ttro' */
#define kReadMe_Creator	0x74747974 /* 'ttxt' */

#define kFinder_FileID	19
#define kFinder_Name	"Finder"
#define kFinder_Chars	6
#define kFinder_Type	0x464E4452 /* 'FNDR' */
#define kFinder_Creator	0x4D414353 /* 'MACS' */

#define kSystem_FileID	20
#define kSystem_Name	"System"
#define kSystem_Chars	6
#define kSystem_Type	0x7A737973 /* 'zsys' */
#define kSystem_Creator	0x4D414353 /* 'MACS' */



#if !defined(FALSE) && !defined(TRUE)
enum {
	FALSE = 0,
	TRUE  = 1
};
#endif


#define kDefaultVolumeNameStr	"untitled"

/*
 * This is the straight GMT conversion constant:
 *
 * 00:00:00 January 1, 1970 - 00:00:00 January 1, 1904
 * (3600 * 24 * ((365 * (1970 - 1904)) + (((1970 - 1904) / 4) + 1)))
 */
#define MAC_GMT_FACTOR		2082844800UL



struct DriveInfo {
	int	fd;
	UInt32	sectorSize;
	UInt32	sectorOffset;
	UInt32	sectorsPerIO;
	UInt64	totalSectors;
};
typedef struct DriveInfo DriveInfo;


enum {
	kMakeHFSWrapper    = 0x01,
	kMakeMaxHFSBitmap  = 0x02,
	kMakeStandardHFS   = 0x04,
	kMakeCaseSensitive = 0x08,
	kUseAccessPerms    = 0x10,
};


struct hfsparams {
	UInt32 		flags;			/* kMakeHFSWrapper, ... */
	UInt32 		blockSize;
	UInt32 		rsrcClumpSize;
	UInt32 		dataClumpSize;
	UInt32 		nextFreeFileID;
	UInt32 		catalogClumpSize;
	UInt32 		catalogNodeSize;
	UInt32 		extentsClumpSize;
	UInt32 		extentsNodeSize;
	UInt32 		attributesClumpSize;
	UInt32 		attributesNodeSize;
	UInt32 		allocationClumpSize;
	UInt32          createDate;             /* in UTC */
	UInt32		hfsAlignment;
	unsigned char volumeName[kHFSPlusMaxFileNameChars + 1];  /* in UTF-8 */
	UInt32		encodingHint;
	UInt32 		journaledHFS;
	UInt32 		journalSize;
	char 		*journalDevice;
	uid_t		owner;
	gid_t		group;
	mode_t		mask;
};
typedef struct hfsparams hfsparams_t;


extern int make_hfs(const DriveInfo *driveInfo, hfsparams_t *defaults,
				UInt32 *totalSectors, UInt32 *sectorOffset);

extern int make_hfsplus(const DriveInfo *driveInfo, hfsparams_t *defaults);


#if __STDC__
void	fatal(const char *fmt, ...);
#else
void	fatal();
#endif
