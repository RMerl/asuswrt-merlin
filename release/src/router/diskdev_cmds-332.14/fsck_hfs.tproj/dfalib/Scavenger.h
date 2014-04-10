/*
 * Copyright (c) 1999-2005 Apple Computer, Inc. All rights reserved.
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
/* Scavenger.h */

#ifndef __SCAVENGER__
#define __SCAVENGER__

#define pascal

#include "SRuntime.h"
#include "BTree.h"
#include "BTreePrivate.h"
#include "CheckHFS.h"
#include "BTreeScanner.h"
#include "hfs_endian.h"

#if LINUX
#define XATTR_MAXNAMELEN 127
#include <limits.h>
#else
#include <sys/xattr.h>
#include <sys/acl.h>
#include <sys/kauth.h>
#include <sys/syslimits.h>
#endif
#include <sys/errno.h>

#ifdef __cplusplus
extern	"C" {
#endif


#define kFSCKMountVersion	0x6673636B	/* 'fsck' made changes */

enum {
	Log2BlkLo				= 9,					// number of left shifts to convert bytes to block.lo
	Log2BlkHi				= 23					// number of right shifts to convert bytes to block.hi
};

enum {
	kNoHint						= 0
};


//
// Misc constants
//

/* IO size for reading or writing disk blocks */
#define DISK_IOSIZE	32768

#define kMaxReScan	3	/* max times to re-scan volume on repair success */

#define	kBTreeHeaderUserBytes	128

#define	kBusErrorValue	0x50FF8001

//ее Danger! This should not be hard coded
#define	kMaxClumpSize	0x100000	/* max clump size is 1MB (2048 btree nodes) */

#define	MDB_FNum	1				/* file number representing the MDB */
#define	AMDB_FNum	-1				/* file number representing the alternate MDB */
#define	VBM_FNum	2				/* file number representing the volume bit map */
#define	MDB_BlkN	2				/* logical block number for the MDB */

#define kCalculatedExtentRefNum			( 0 )
#define kCalculatedCatalogRefNum		( 1*sizeof(SFCB) )
#define kCalculatedAllocationsRefNum	( 2*sizeof(SFCB) )
#define kCalculatedAttributesRefNum		( 3*sizeof(SFCB) )
#define kCalculatedStartupRefNum		( 4*sizeof(SFCB) )
#define kCalculatedRepairRefNum			( 5*sizeof(SFCB) )

#define	Max_ABSiz	0x7FFFFE00		/* max allocation block size (multiple of 512 */
#define	Blk_Size	512				/* size of a logical block */
#define kHFSBlockSize 512			/* HFS block size */

// only the lower 7 bits are considered to be invalid, all others are valid -djb
#define	VAtrb_Msk	0x007F			/* volume attribute mask - invalid bits */
#define	VAtrb_DFlt	0x0100			/* default volume attribute flags */
#define	VAtrb_Cons	0x0100			/* volume consistency flag */
#define kHFSCatalogNodeIDsReused 0x1000		

/*
 *	File type and creator for symbolic links (from xnu/bsd/hfs/hfs.h)
 */
enum {
	kSymLinkFileType  = 0x736C6E6B,	/* 'slnk' */
	kSymLinkCreator   = 0x72686170	/* 'rhap' */
};

/*
 *	File type and creator for TextEdit documents
 */
enum {
	kTextFileType		= 0x54455854,	/* 'TEXT' */
	kTextFileCreator	= 0x74747874,	/* 'ttxt' */
};

/*------------------------------------------------------------------------------
 BTree data structures
------------------------------------------------------------------------------*/

/* misc BTree constants */

#define	BTMaxDepth	8				/* max tree depth */
#define	Num_HRecs	3				/* number of records in BTree Header node */
#define	Num_MRecs	1				/* number of records in BTree Map node */



//	DFA extensions to the HFS/HFS+ BTreeControlBlock
typedef struct BTreeExtensionsRec
{
	Ptr 				BTCBMPtr;			//	pointer to scavenger BTree bit map
	UInt32				BTCBMSize;			//	size of the bitmap, bytes
	BTreeControlBlock	*altBTCB;			//	BTCB DFA builds up
	UInt32				realFreeNodeCount;	//	Number of real free nodes, taken from disk, for more accurate progress information
} BTreeExtensionsRec;


	
/*
 * Scavenger BTree Path Record (STPR)
 */
typedef struct STPR {
	UInt32			TPRNodeN;		/* node number */
	SInt16			TPRRIndx;		/* record index */
	SInt16			unused;			/* not used - makes debugging easier */
	UInt32			TPRLtSib;		/* node number of left sibling node */
	UInt32			TPRRtSib;		/* node number of right sibling node */
	} STPR, *STPRPtr;
	
typedef	STPR SBTPT[BTMaxDepth]; 		/* BTree path table */
	
#define	LenSBTPT	( sizeof(STPR) * BTMaxDepth )	/* length of BTree Path Table */




/*------------------------------------------------------------------------------
 CM (Catalog Manager) data structures
 ------------------------------------------------------------------------------*/

//
//	Misc constants
//
#define CMMaxDepth	100				/* max catalog depth (Same as Finder 7.0) */

#define fNameLocked 4096

union CatalogName {
	Str31 							pstr;
	HFSUniStr255 					ustr;
};
typedef union CatalogName				CatalogName;
	
//
//	Scavenger Directory Path Record (SDPR)
//
typedef struct SDPR {
	UInt32				directoryID;		//	directory ID
	UInt32				offspringIndex;		//	offspring index
	UInt32				directoryHint;		//	BTree hint for directory record
	long				threadHint;			//	BTree hint for thread record
	HFSCatalogNodeID	parentDirID;		//	parent directory ID
	CatalogName			directoryName;		//	directory CName
} SDPR;
	
typedef	SDPR SDPT[CMMaxDepth]; 			//	directory path table
	
#define	LenSDPT	( sizeof(SDPR) * CMMaxDepth )	//	length of Tree Path Table


enum {
//	kInvalidMRUCacheKey			= -1L,							/* flag to denote current MRU cache key is invalid*/
	kDefaultNumMRUCacheBlocks	= 16							/* default number of blocks in each cache*/
};


/*
 * UTCacheReadIP and UTCacheWriteIP cacheOption
 */

enum {
	noCacheBit   = 5,	/* don't cache this please */
	noCacheMask  = 0x0020,
	rdVerifyBit  = 6,	/* read verify */
	rdVerifyMask = 0x0040
};


/*------------------------------------------------------------------------------
 Low-level File System Error codes 
------------------------------------------------------------------------------*/

/* The DCE bits are defined as follows (for the word of flags): */

enum
{
	Is_AppleTalk		= 0,
	Is_Agent			= 1,			// future use
	FollowsNewRules		= 2,			// New DRVR Rules Bit
	Is_Open				= 5,
	Is_Ram_Based		= 6,
	Is_Active			= 7,
	Read_Enable			= 8,
	Write_Enable		= 9,
	Control_Enable		= 10,
	Status_Enable		= 11,
	Needs_Goodbye		= 12,
	Needs_Time			= 13,
	Needs_Lock			= 14,

	Is_AppleTalk_Mask	= 1 << Is_AppleTalk,
	Is_Agent_Mask		= 1 << Is_Agent,
	FollowsRules_Mask	= 1 << FollowsNewRules,
	Is_Open_Mask		= 1 << Is_Open,
	Is_Ram_Based_Mask	= 1 << Is_Ram_Based,
	Is_Active_Mask		= 1 << Is_Active,
	Read_Enable_Mask	= 1 << Read_Enable,
	Write_Enable_Mask	= 1 << Write_Enable,
	Control_Enable_Mask	= 1 << Control_Enable,
	Status_Enable_Mask	= 1 << Status_Enable,
	Needs_Goodbye_Mask	= 1 << Needs_Goodbye,
	Needs_Time_Mask		= 1 << Needs_Time,
	Needs_Lock_Mask		= 1 << Needs_Lock
};

enum {
	cdInternalErr					= -1312,		//	internal CheckDisk error
	cdVolumeNotFoundErr				= -1313,		//	cound not find volume (could be offline)
	cdCannotReadErr					= -1314,		//	unable to read from disk
	cdCannotWriteErr				= -1315,		//	unable to write to disk
	cdNotHFSVolumeErr				= -1316,		//	not an HFS disk
	cdUnrepairableErr				= -1317,		//	volume needs major repairs that CheckDisk cannot fix
	cdRepairFailedErr				= -1318,		//	repair failed
	cdUserCanceledErr				= -1319,		//	user interrupt
	cdVolumeInUseErr				= -1320,		//	volume modifed by another app
	cdNeedsRepairsErr				= -1321,		//	volume needs repairs (see repairInfo for additional info)
	cdReMountErr					= -1322,		//	Cannot remount volume
	cdUnknownProcessesErr			= -1323,		//	Volume cannot be unmounted and unknown processes are running
	cdDamagedWrapperErr				= -1324,		//	HFS Wrapper damaged error.
	cdIncompatibleOSErr				= -1325,		//	Current OS version is incompatible
	cdMemoryFullErr					= -1326			//	not enough memory to check disk
};


enum {
	fsDSIntErr	= -127	/* non-hardware Internal file system error */
};

//	Repair Info - additional info returned when a repair is attempted
enum {
	kFileSharingEnabled		= 0x00000001,
	kDiskIsLocked			= 0x00000002,
	kDiskIsBoot			= 0x00000004,
	kDiskHasOpenFiles		= 0x00000008,
	kVolumeHadOverlappingExtents	= 0x00000010,	// repairLevelSomeDataLoss
	kVolumeClean			= 0x00000020,

	kRepairsWereMade		= 0x80000000
};

//	Input parameters to CheckDisk
enum
{
	ignoreRunningProcessesMask		= 0x00000001,	//	Assumes caller has shut down processes
	checkDiskVersionMask			= 0x00000004	//	Will just return back the version in repairInfo.
};

//	Message types, so the user can treat and display accordingly
enum {
	kStatusMessage					= 0x0000,
	kTitleMessage					= 0x0001,
	kErrorMessage					= 0x0002
};

//	<10> Current stage of CheckDisk passed to cancel proc.
//	File System is marked busy during kRepairStage, so WaitNextEvent and I/O cannot be done during this stage.

enum {
	kHFSStage						= 0,
	kRepairStage,
	kVerifyStage,
	kAboutToRepairStage
};

//	Resource ID of 'STR ' resource containing the name of of the folder to create aliases to damaged files.
enum {
	rDamagedFilesDirSTRid			= -20886
};

//	Type of volume
enum {
	kUnknownVolumeType 	= 0,
	kHFSVolumeType,
	kEmbededHFSPlusVolumeType,
	kPureHFSPlusVolumeType
};


enum {
	kStatusLines	= 131,
	kFirstError		= 500,
	
	kHighLevelInfo	= 1100,
	kBasicInfo		= 1200,
	kErrorInfo		= 1202,

	kErrorBase		= -1310
};


/*------------------------------------------------------------------------------
 Minor Repair Interface (records compiled during scavenge, later repaired)
 Note that not all repair types use all of these fields.
 -----------------------------------------------------------------------------*/
 
 typedef struct	RepairOrder			/* a node describing a needed minor repair */
 {
 	struct RepairOrder	*link;	/* link to next node, or NULL */
	SInt16			type;	/* type of error, as an error code (E_DirVal etc) */
	SInt16		forkType;	/* which file fork */
	UInt64		correct;	/* correct valence */
	UInt64		incorrect;	/* valence as found in volume (for consistency chk) */
	UInt32		maskBit;	/* incorrect bit */
	UInt32		hint;		/* B-tree node hint */
	UInt32		parid;		/* parent ID */
	unsigned char name[1];	/* dir or file name */
 } RepairOrder, *RepairOrderPtr;


 typedef struct	EmbededVolDescription
 {
 	SInt16				drAlBlSt;
	UInt16 				drEmbedSigWord;
 	HFSExtentDescriptor	drEmbedExtent;
 } EmbededVolDescription;


// define the correct drive queue structure
typedef struct ExtendedDrvQueue
{
	char dQVolumeLocked;
	char dQDiskInDrive;
	char dQUsedInternally;
	char dQDiskIsSingleSided;
	QElemPtr qLink;
	short qType;
	short dQDrive;
	short dQRefNum;
	short dQFSID;
	short dQDrvSz;
	short dQDrvSz2;
}ExtendedDrvQueue;


/*------------------------------------------------------------------------------
 Scavenger Global Area - (SGlob) 
------------------------------------------------------------------------------*/
typedef struct MissingThread
{
	struct MissingThread  *link;		/* link to next node, or NULL */
	UInt32                threadID;
	HFSPlusCatalogKey     nextKey;
	HFSPlusCatalogThread  thread;
} MissingThread;

#define kDataFork	0
#define kRsrcFork	(-1)

struct ExtentInfo {
	HFSCatalogNodeID fileID;
	UInt32	startBlock;
	UInt32 	blockCount;
	UInt32	newStartBlock;
	UInt8	forkType;
	Boolean	hasThread;
	Boolean hasRepair;
};
typedef struct ExtentInfo ExtentInfo;

struct ExtentsTable {
	UInt32 							count;
	ExtentInfo 						extentInfo[1];
};
typedef struct ExtentsTable ExtentsTable;


struct FileIdentifier {
	Boolean 						hasThread;
	HFSCatalogNodeID 				fileID;
	HFSCatalogNodeID 				parID;			//	Used for files on HFS volumes without threads
	Str31		 					name;			//	Used for files on HFS volumes without threads
};
typedef struct FileIdentifier FileIdentifier;

struct FileIdentifierTable {
	UInt32 							count;
	FileIdentifier 					fileIdentifier[1];
};
typedef struct FileIdentifierTable FileIdentifierTable;

/* Universal Extent Key */

union ExtentKey {
	HFSExtentKey 					hfs;
	HFSPlusExtentKey 				hfsPlus;
};
typedef union ExtentKey					ExtentKey;
/* Universal extent descriptor */

union ExtentDescriptor {
	HFSExtentDescriptor 			hfs;
	HFSPlusExtentDescriptor 		hfsPlus;
};
typedef union ExtentDescriptor			ExtentDescriptor;
/* Universal extent record */

union ExtentRecord {
	HFSExtentRecord 				hfs;
	HFSPlusExtentRecord 			hfsPlus;
};
typedef union ExtentRecord				ExtentRecord;
/* Universal catalog key */

union CatalogKey {
	HFSCatalogKey 					hfs;
	HFSPlusCatalogKey 				hfsPlus;
};
typedef union CatalogKey				CatalogKey;
/* Universal catalog data record */

union CatalogRecord {
	SInt16 							recordType;
	HFSCatalogFolder 				hfsFolder;
	HFSCatalogFile 					hfsFile;
	HFSCatalogThread 				hfsThread;
	HFSPlusCatalogFolder 			hfsPlusFolder;
	HFSPlusCatalogFile 				hfsPlusFile;
	HFSPlusCatalogThread 			hfsPlusThread;
};
typedef union CatalogRecord				CatalogRecord;

/*
  	Key for records in the attributes file.  Fields are compared in the order:
  		cnid, attributeName, startBlock
*/

struct AttributeKey {
	UInt16 							keyLength;					/* must set kBTBigKeysMask and kBTVariableIndexKeysMask in BTree header's attributes */
	UInt16 							pad;
	HFSCatalogNodeID 				cnid;						/* file or folder ID */
	UInt32 							startBlock;					/* block # relative to start of attribute */
	UInt16     attrNameLen;     /* number of unicode characters */
	UInt16     attrName[127];   /* attribute name (Unicode) */
};
typedef struct AttributeKey				AttributeKey;
enum {
	kAttributeKeyMaximumLength	= sizeof(AttributeKey) - sizeof(UInt16),
	kAttributeKeyMinimumLength	= kAttributeKeyMaximumLength - 127 * sizeof(UInt16) + sizeof(UInt16)
};

struct HIOParam {
	QElemPtr 						qLink;						/*queue link in header*/
	short 							qType;						/*type byte for safety check*/
	short 							ioTrap;						/*FS: the Trap*/
	Ptr 							ioCmdAddr;					/*FS: address to dispatch to*/
	void* 				ioCompletion;				/*completion routine addr (0 for synch calls)*/
	OSErr 							ioResult;					/*result code*/
	StringPtr 						ioNamePtr;					/*ptr to Vol:FileName string*/
	short 							ioVRefNum;					/*volume refnum (DrvNum for Eject and MountVol)*/
	short 							ioRefNum;
	SInt8 							ioVersNum;
	SInt8 							ioPermssn;
	Ptr 							ioMisc;
	Ptr 							ioBuffer;
	long 							ioReqCount;
	long 							ioActCount;
	short 							ioPosMode;
	long 							ioPosOffset;
};
typedef struct HIOParam HIOParam;

typedef HIOParam *						HIOParamPtr;


struct FCBArray {
	UInt32		length;		/* first word is FCB part length*/
	SFCB		fcb[1];		/* fcb array*/
};
typedef struct FCBArray FCBArray;

/*
	UserCancel callback routine
	
	Input:
			progress:			number from 1 to 100 indicating current progress
			progressChanged:	boolean flag that is true if progress number has been updated
			context:			pointer to context data (if any) that the caller passed to CheckDisk
			
	Output:
			return true if the user wants to cancel the CheckDisk operation
 */

typedef int (*UserCancelProcPtr)(UInt16 progress, UInt16 secondsRemaining, Boolean progressChanged, UInt16 stage, void *context);


#if  0

	//--	User Cancel Proc
	typedef UniversalProcPtr UserCancelUPP;
	
	enum {
		uppUserCancelProcInfo = kPascalStackBased
			 | RESULT_SIZE(kTwoByteCode)
			 | STACK_ROUTINE_PARAMETER(1, kTwoByteCode)
			 | STACK_ROUTINE_PARAMETER(2, kTwoByteCode)
			 | STACK_ROUTINE_PARAMETER(3, kTwoByteCode)
			 | STACK_ROUTINE_PARAMETER(4, kTwoByteCode)
			 | STACK_ROUTINE_PARAMETER(5, kFourByteCode)
	};
	
	#define NewUserCancelProc(userRoutine)		\
			(UserCancelUPP) NewRoutineDescriptor((ProcPtr)(userRoutine), uppUserCancelProcInfo, GetCurrentArchitecture())
	
	#define CallUserCancelProc(userRoutine, progress, secondsRemaining, progressChanged, stage, context)		\
			CallUniversalProc((UniversalProcPtr)(userRoutine), uppUserCancelProcInfo, (progress), (secondsRemaining), (progressChanged), (stage), (context))

#else /* not CFM */

	typedef UserCancelProcPtr UserCancelUPP;
	
	#define NewUserCancelProc(userRoutine)		\
			((UserCancelUPP) (userRoutine))
	
	#define CallUserCancelProc(userRoutine, progress, secondsRemaining, progressChanged, stage, context)		\
			(*(userRoutine))((progress), (secondsRemaining), (progressChanged), (stage), (context))

#endif


/*
	UserMessage callback routine
	
	Input:
			message:			message from CheckDisk
			messageType:		type of message
			context:			pointer to context data (if any) that the caller passed to CheckDisk
			
	Output:
			return true if the user wants to cancel the CheckDisk operation
 */


typedef pascal void (*UserMessageProcPtr)(StringPtr message, SInt16 messageType, void *context);

#if 0

	//--	User Message Proc
	typedef UniversalProcPtr UserMessageUPP;
	
	enum {
		uppUserMessageProcInfo = kPascalStackBased
			 | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
			 | STACK_ROUTINE_PARAMETER(2, kTwoByteCode)
			 | STACK_ROUTINE_PARAMETER(3, kFourByteCode)
	};
	
	#define NewUserMessageProc(userRoutine)		\
			(UserMessageUPP) NewRoutineDescriptor((ProcPtr)(userRoutine), uppUserMessageProcInfo, GetCurrentArchitecture())
	
	#define CallUserMessageProc(userRoutine, message, messageType, context)		\
			CallUniversalProc((UniversalProcPtr)(userRoutine), uppUserMessageProcInfo, (message), (messageType), (context))

#else /* not CFM */

	typedef UserMessageProcPtr UserMessageUPP;
	
	#define NewUserMessageProc(userRoutine)		\
			((UserMessageUPP) (userRoutine))
	
	#define CallUserMessageProc(userRoutine, message, messageType, context)		\
			(*(userRoutine))((message), (messageType), (context))

#endif

/* 3843779 Structure to detemine consistency of attribute data and 
 * corresponding bit in catalog record.  Based on Chinese Remainder
 * Theorem
 */
typedef struct PrimeBuckets {
	UInt32	n32[32];
	UInt32	n27[27];
	UInt32	n25[25];
	UInt32	n7[7];
	UInt32	n11[11];
	UInt32	n13[13];
	UInt32	n17[17];
	UInt32	n19[19];
	UInt32	n23[23];
	UInt32	n29[29];
	UInt32	n31[31];
} PrimeBuckets;

/* Record last attribute ID checked, used in CheckAttributeRecord, initialized in ScavSetup */
typedef struct lastAttrID {
	UInt32 fileID;
	Boolean hasSecurity;
} lastAttrID;

/* 	
	VolumeObject encapsulates all infomration about the multiple volume anchor blocks (VHB and MSD) 
	on HFS and HFS+ volumes.  An HFS volume will have two MDBs (primary and alternate HFSMasterDirectoryBlock), 
	a pure HFS+ volume will have two VHBs (primary and alternate HFSPlusVolumeHeader), and a wrapped HFS+ 
	volume will have two MDBs and two VHBs.
*/

/* values for VolumeObject.flags */
enum {
	kVO_Inited			= 0x00000001,	   	// this structured has been initialized
	kVO_PriVHBOK		= 0x00000002,	   	// the primary Volume Header Block is valid
	kVO_AltVHBOK		= 0x00000004,	   	// the alternate Volume Header Block is valid
	kVO_PriMDBOK		= 0x00000008,	   	// the primary Master Directory Block is valid
	kVO_AltMDBOK		= 0x00000010,	   	// the alternate Master Directory Block is valid
};

typedef struct VolumeObject {
	UInt32			flags;
	SVCB * 			vcbPtr;				// pointer to VCB used for this volume
	UInt32			volumeType;			// (kHFSVolumeType or kEmbededHFSPlusVolumeType or kPureHFSPlusVolumeType)
	UInt32			embeddedOffset;		// offset of embedded HFS+ (in bytes) volume into HFS wrapper volume
										//   NOTE - UInt32 is OK since we don't support HFS Wrappers on TB volumes
	UInt32			sectorSize;			// size of a sector for this device
	UInt64			totalDeviceSectors;	// total number of sectors for this volume (from GetDeviceSize)
	UInt64			totalEmbeddedSectors; // total number of sectors for embedded volume
	// location of all possible volume anchor blocks (MDB and VHB) on this volume.  These locations
	// are the sector offset into the volume.  Only wrapped HFS+ volumes use all 4 of these.
	UInt64			primaryVHB;			// not used for HFS volumes
	UInt64			alternateVHB;		// not used for HFS volumes
	UInt64			primaryMDB;			// not used for pure HFS+ volumes
	UInt64			alternateMDB;		// not used for pure HFS+ volumes
} VolumeObject, *VolumeObjectPtr;


typedef struct SGlob {
	SInt16				DrvNum;					//	drive number of target drive
	SInt16				RepLevel;				//	repair level, 1 = minor repair, 2 = major repair
	SInt16				ScavRes;				//	scavenge result code
	OSErr				ErrCode;    			//	error code
	OSErr				IntErr;     			//	internal error code
	UInt16				VIStat;					//	scavenge status flags for volume info 
	UInt16				ABTStat;				//	scavenge status flags for Attributes BTree 
	UInt16				EBTStat;				//	scavenge status flags for extent BTree 
	UInt16				CBTStat;				//	scavenge status flags for catalog BTree 
	UInt16				CatStat;				//	scavenge status flags for catalog file
	UInt16				VeryMinorErrorsStat;	//	scavenge status flags for very minor errors
	UInt16				JStat;					//	scavange status flags for journal errors
	DrvQElPtr			DrvPtr;					//	pointer to driveQ element for target drive
	UInt32				TarID;					//	target ID (CNID of data structure being verified)
	UInt64				TarBlock;				//	target block/node number being verified
	SInt16				BTLevel;				//	current BTree enumeration level
	SBTPT				*BTPTPtr;				//	BTree path table pointer
	SInt16				DirLevel;				//	current directory enumeration level
	SDPT				*DirPTPtr;				//	directory path table pointer
	SInt16				CNType;					//	current CNode type
	UInt32				ParID;					//	current parent DirID
	CatalogName			CName;					//	current CName
	RepairOrderPtr		MinorRepairsP;			//	ptr to list of problems for later repair
	MissingThread		*missingThreadList;
	Ptr 				FCBAPtr;				//	pointer to scavenger FCB array
	UInt32				**validFilesList;		//	List of valid HFS file IDs

	ExtentsTable		**overlappedExtents;	//	List of overlapped extents
	FileIdentifierTable	**fileIdentifierTable;	//	List of files for post processing

	UInt32				inputFlags;				//	Caller can specify some DFA behaviors

	UInt32				volumeFeatures;			//	bit vector of volume and OS features
	Boolean				usersAreConnected;		//	true if user are connected
	Boolean				fileSharingOn;			//	true if file sharing is on
	UInt32				altBlockLocation;
	Boolean				checkingWrapper;
	SInt16				numExtents;				//	Number of memory resident extents.  3 or 8
	OSErr				volumeErrorCode;
	
	UserCancelUPP		userCancelProc;
	UserMessageUPP		userMessageProc;
	void				*userContext;

	UInt64				onePercent;
	UInt64				itemsToProcess;
	UInt64				itemsProcessed;
	UInt64				lastProgress;
	long				startTicks;
	UInt16				secondsRemaining;

	long				lastTickCount;

	
	SVCB			*calculatedVCB;
	SFCB			*calculatedExtentsFCB;
	SFCB			*calculatedCatalogFCB;
	SFCB			*calculatedAllocationsFCB;
	SFCB			*calculatedAttributesFCB;
	SFCB			*calculatedStartupFCB;
	SFCB			*calculatedRepairFCB;	
	BTreeControlBlock	*calculatedExtentsBTCB;
	BTreeControlBlock	*calculatedCatalogBTCB;
	BTreeControlBlock	*calculatedRepairBTCB;
	BTreeControlBlock	*calculatedAttributesBTCB;

	Boolean			cleanUnmount;
	Boolean			guiControl;
	int				logLevel;
	int				chkLevel;
	int             repairLevel;
	Boolean			minorRepairErrors;	// indicates some minor repairs failed
	int				canWrite;  	// we can safely write to the block device
	int				lostAndFoundMode;  // used when creating lost+found directory
	BTScanState		scanState;

	unsigned char	volumeName[256]; /* volume name in ASCII or UTF-8 */

	PrimeBuckets 	CBTAttrBucket;		/* prime number buckets for Attribute bit in Catalog btree */
	PrimeBuckets 	CBTSecurityBucket;	/* prime number buckets for Security bit in Catalog btree */
	PrimeBuckets 	ABTAttrBucket;		/* prime number buckets for Attribute bit in Attribute btree */
	PrimeBuckets 	ABTSecurityBucket;	/* prime number buckets for Security bit in Attribute btree */
	lastAttrID 	lastAttrFileID; 	/* Record last attribute ID checked, used in CheckAttributeRecord, initialized in ScavSetup */
	UInt16		securityAttrName[XATTR_MAXNAMELEN];	/* Store security attribute name in UTF16, to avoid frequent conversion */
	size_t  	securityAttrLen;
} SGlob, *SGlobPtr;


enum
{
	supportsTrashVolumeCacheFeatureMask		= 1,
	supportsHFSPlusVolsFeatureMask			= 2,
	volumeIsMountedMask						= 4
};

/* scavenger flags */	
	
/* volume info status flags (contents of VIStat) */

#define	S_MDB					0x8000	//	MDB/VHB damaged
#define	S_AltMDB				0x4000	//	Unused	/* alternate MDB damaged */
#define	S_VBM					0x2000	//	volume bit map damaged
#define	S_WMDB					0x1000	//	wrapper MDB is damaged
#define	S_OverlappingExtents	0x0800	//	Overlapping extents found
#define	S_BadMDBdrAlBlSt		0x0400	//	Invalid drAlBlSt field in MDB
#define S_InvalidWrapperExtents	0x0200	//	Invalid catalog extent start in MDB

/* BTree status flags (contents of EBTStat, CBTStat and ABTStat) */

#define	S_BTH					0x8000	/* BTree header damaged */
#define	S_BTM					0x4000	/* BTree map damaged */
#define	S_Indx					0x2000	//	Unused	/* index structure damaged */
#define	S_Leaf					0x1000	//	Unused	/* leaf structure damaged */
#define S_Orphan				0x0800  // orphaned file
#define S_OrphanedExtent		0x0400  // orphaned extent
#define S_ReservedNotZero		0x0200  // the flags or reserved fields are not zero
#define S_RebuildBTree			0x0100  // similar to S_Indx, S_Leaf, but if one is bad we stop checking and the other may also be bad.
#define S_ReservedBTH			0x0080  // fields in the BTree header should be zero but are not
#define S_AttributeCount		0x0040	// incorrect number of xattr in attribute btree in comparison with attribute bit in catalog btree
#define S_SecurityCount			0x0020	// incorrect number of security xattrs in attribute btree in comparison with security bit in catalog btree

/* catalog file status flags (contents of CatStat) */

#define	S_IllName			0x8000	/* illegal name found */
#define	S_Valence			0x4000	/* a directory valence is out of sync */
#define	S_FThd				0x2000	/* dangling file thread records exist */
#define	S_DFCorruption		0x1000	/* disappearing folder corruption detected */
#define	S_NoDir				0x0800	/* missing directory record */
#define S_LockedDirName		0x0400  // locked dir name
#define S_MissingThread		0x0200  /* missing thread record */
#define S_UnlinkedFile		0x0100	/* orphaned link node */
#define S_LinkCount			0x0080	/* data node link count needs repair */
#define S_Permissions		0x0040	/* BSD permissions need repair */
#define S_FileAllocation	0x0020	/* peof or leof needs adjustment */

/* VeryMinorErrorsStat */

#define S_BloatedThreadRecordFound	0x8000  // 2210409, excessivly large thread record found

/* user file status flags (contents of FilStat) */

//#define S_LockedName			0x4000  // locked file name

/* Journal status flag (contents of JStat) */
#define S_BadJournal		0x8000	/* Bad journal content */

/*------------------------------------------------------------------------------
 ScavCtrl Interface
------------------------------------------------------------------------------*/

//	Command Codes (commands to ScavControl)
enum
{
	scavInitialize		= 1,			//	Start initial volume check
	scavVerify,							//	Start verify operation
	scavRepair,							//	Start repair opeation
	scavTerminate,						//	Cleanup after scavenge
};


//	Repair Levels
enum
{
	repairLevelNoProblemsFound				= 0,
	repairLevelRepairIfOtherErrorsExist,		//	Bloated thread records, ...
	repairLevelVeryMinorErrors,					//	Missing Custom Icon, Locked Directory name,..., Errors that don't need fixing from CheckDisk (Installer), Non Volume corruption bugs.
	repairLevelVolumeRecoverable,				//	Minor Volume corruption exists
	repairLevelSomeDataLoss,					//	Overlapping extents, some data loss but no scavaging will get it back
	repairLevelWillCauseDataLoss,				//	Missing leaf nodes, repair will lose nodes without scavaging (proceed at your own risk, check disk with other utils)
	repairLevelCatalogBtreeRebuild,				//	Catalog Btree is damaged, repair may lose some data
	repairLevelUnrepairable						//	DFA cannot repair volume
};


/* Status messages written to summary */
enum {
	M_FirstMessage              =  1,

	M_CheckingHFSVolume         =  1,
	M_CheckingHFSPlusVolume     =  2,
	M_ExtBTChk                  =  3,
	M_CatBTChk                  =  4,
	M_CatHChk                   =  5,
	M_AttrBTChk                 =  6,
	M_VolumeBitMapChk           =  7,
	M_VInfoChk                  =  8,
	M_MultiLinkChk              =  9,
	M_Orphaned                  = 10,
	M_RebuildingExtentsBTree    = 11,
	M_RebuildingCatalogBTree    = 12,
	M_RebuildingAttributesBTree = 13,
	M_Repair                    = 14,
	M_NeedsRepair               = 15,
	M_AllOK                     = 16,
	M_RepairOK                  = 17,
	M_RepairFailed              = 18,
	M_CheckFailed               = 19,
	M_Rescan	               	= 20,
	M_Look		               	= 21,
	M_OtherWriters		       	= 22,
	M_CaseSensitive		       	= 23,
	M_ReRepairFailed			= 24,			
	M_LookDamagedDir			= 25,

	M_LastMessage               = 25
};

/*------------------------------------------------------------------------------
 Scavenger Result/Error Codes
------------------------------------------------------------------------------*/

/*
 * Scavenger errors.
 * If negative, they are unrecoverable (scavenging terminates).
 * If positive, they are recoverable (scavenging continues).
 *
 */	
enum {
	E_FirstError		=  500,

	E_PEOF			=  500,	/* Invalid PEOF */
	E_LEOF			=  501,	/* Invalid LEOF */
	E_DirVal		=  502,	/* Invalid directory valence */
	E_CName			=  503,	/* Invalid CName */
	E_NHeight		=  504,	/* Invalid node height */
	E_NoFile		=  505,	/* Missing file record for file thread */
	E_ABlkSz		= -506,	/* Invalid allocation block size */
	E_NABlks		= -507,	/* Invalid number of allocation blocks */
	E_VBMSt			= -508,	/* Invalid VBM start block */
	E_ABlkSt		= -509,	/* Invalid allocation block start */

	E_ExtEnt		= -510,	/* Invalid extent entry */
	E_OvlExt		=  511,	/* overlapped extent allocation */
	E_LenBTH		= -512,	/* Invalid BTH length */
	E_ShortBTM		= -513,	/* BT map too short to repair */
	E_BTRoot		= -514,	/* Invalid root node number */
	E_NType			= -515,	/* Invalid node type */
	E_NRecs			= -516,	/* Invalid record count */
	E_IKey			= -517,	/* Invalid index key */
	E_IndxLk		= -518,	/* Invalid index link */
	E_SibLk			= -519,	/* Invalid sibling link */

	E_BadNode		= -520,	/* Invalid node structure */
	E_OvlNode		= -521,	/* overlapped node allocation */
	E_MapLk			= -522,	/* Invalid map node linkage */
	E_KeyLen		= -523,	/* Invalid key length */
	E_KeyOrd		= -524,	/* Keys out of order */
	E_BadMapN		= -525,	/* Invalid map node */
	E_BadHdrN		= -526,	/* Invalid header node */
	E_BTDepth		= -527,	/* exceeded maximum BTree depth */
	E_CatRec		= -528,	/* Invalid catalog record type */
	E_LenDir		= -529,	/* Invalid directory record length */

	E_LenThd		= -530,	/* Invalid thread record length */
	E_LenFil		= -531,	/* Invalid file record length */
	E_NoRtThd		= -532,	/* Missing thread record for root directory */
	E_NoThd			= -533,	/* Missing thread record */
	E_NoDir			= 534,	/* Missing directory record */
	E_ThdKey		= -535,	/* Invalid key for thread record */
	E_ThdCN			= -536,	/* Invalid  parent CName in thread record */
	E_LenCDR		= -537,	/* Invalid catalog record length */
	E_DirLoop		= -538,	/* loop in directory hierarchy */
	E_RtDirCnt		=  539,	/* Invalid root directory count */

	E_RtFilCnt		=  540,	/* Invalid root file count */
	E_DirCnt		=  541,	/* Invalid volume directory count */
	E_FilCnt		=  542,	/* Invalid volume file count */
	E_CatPEOF		= -543,	/* Invalid catalog PEOF */
	E_ExtPEOF		= -544,	/* Invalid extent file PEOF */
	E_CatDepth		=  545,	/* exceeded maximum catalog depth */
	E_NoFThdFlg		= -546,	/* file thread flag not set in file record */
	E_CatalogFlagsNotZero	=  547,
	E_BadFileName		= -548,	/* Invalid file/folder name problem */
	E_InvalidClumpSize	=  549,	/* bad file clump size */

	E_InvalidBTreeHeader	=  550,	/* Invalid btree header */
	E_LockedDirName		=  551,	/* Inappropriate locked folder name */
	E_EntryNotFound		= -552,	/* volume catalog entry not found */
	E_FreeBlocks		=  553,
	E_MDBDamaged		=  554,	/* MDB Damaged */
	E_VolumeHeaderDamaged	=  555,	/* Volume Header Damaged */
	E_VBMDamaged		=  556,	/* Volume Bit Map needs minor repair */
	E_InvalidNodeSize	= -557,	/* Bad BTree node size */
	E_LeafCnt		=  558,
	E_BadValue		=  559,

	E_InvalidID		=  560,
	E_VolumeHeaderTooNew	=  561,
	E_DiskFull		= -562,
	E_InternalFileOverlap	= -563,	/* This is a serious error */
	E_InvalidVolumeHeader	= -564,
	E_InvalidMDBdrAlBlSt	=  565,
	E_InvalidWrapperExtents	=  566,
	E_InvalidLinkCount	=  567,	/* Invalid Hardlink count */
	E_UnlinkedFile		=  568,	/* Unlinked file needs to be deleted */
	E_InvalidPermissions	=  569,

	E_InvalidUID		=  570,
	E_IllegalName		=  571,
	E_IncorrectNumThdRcd	=  572,
	/* Init the next two errors to pre-existing messages.  Fix them in 3964748 */
	E_IncorrectAttrCount	=  547,	/* Incorrect attributes in attr btree with attr bits in catalog btree */
	E_IncorrectSecurityCount=  547, /* Incorrect security attributes in attr btree with security bits in catalog btree */
	E_SymlinkCreate		=  573,

	E_LastError		=  573
};


/* Internal DFA error codes */
enum {
	errRebuildBtree				= -1001		/* BTree requires rebuilding. */
};


enum {																/*	extendFileContigMask		= 0x0002*/
	kEFContigBit				= 1,							/*	force contiguous allocation*/
	kEFContigMask				= 0x02,
	kEFAllBit					= 0,							/*	allocate all requested bytes or none*/
	kEFAllMask					= 0x01,
	kEFNoClumpBit				= 2,							/*	Don't round up requested size to multiple of clump size*/
	kEFNoClumpMask				= 0x04,							/*	TruncateFile option flags*/
	kTFTrunExtBit				= 0,							/*	truncate to the extent containing new PEOF*/
	kTFTrunExtMask				= 1
};



// Encoding vs. Index
//
// For runtime table lookups and for the volume encoding bitmap we
// need to map some encodings to keep them in a reasonable range.
//

enum {
	kTextEncodingMacRoman		= 0L,
	kTextEncodingMacFarsi		= 0x8C,	 /* Like MacArabic but uses Farsi digits*/
																/* The following use script code 7, smCyrillic*/
	kTextEncodingMacUkrainian	= 0x98,	/* The following use script code 32, smUnimplemented*/

	kIndexMacUkrainian	= 48,		// MacUkrainian encoding is 152
	kIndexMacFarsi		= 49		// MacFarsi encoding is 140
};

#define MapEncodingToIndex(e) \
	( (e) < 48 ? (e) : ( (e) == kTextEncodingMacUkrainian ? kIndexMacUkrainian : ( (e) == kTextEncodingMacFarsi ? kIndexMacFarsi : kTextEncodingMacRoman) ) )

#define MapIndexToEncoding(i) \
	( (i) == kIndexMacFarsi ? kTextEncodingMacFarsi : ( (i) == kIndexMacUkrainian ? kTextEncodingMacUkrainian : (i) ) )

#define ValidMacEncoding(e)	\
	( ((e) < 39)  ||  ((e) == kTextEncodingMacFarsi)  ||  ((e) == kTextEncodingMacUkrainian) )




extern	void	WriteMsg( SGlobPtr GPtr, short messageID, short messageType );
extern	void	WriteError( SGlobPtr GPtr, short msgID, UInt32 tarID, UInt64 tarBlock );
extern	short	CheckPause( void );

extern void	PrintError(SGlobPtr GPtr, short error, int vargc, ...);
extern void	PrintStatus(SGlobPtr GPtr, short status, int vargc, ...);


/* ------------------------------- From SControl.c ------------------------------- */

void			ScavCtrl( SGlobPtr GPtr, UInt32 ScavOp, short *ScavRes );

extern	short	CheckForStop( SGlobPtr GPtr );
	

/* ------------------------------- From SRepair.c -------------------------------- */

extern	OSErr	RepairVolume( SGlobPtr GPtr );

extern	int		MRepair( SGlobPtr GPtr );

extern	int		FixDFCorruption( const SGlobPtr GPtr, RepairOrderPtr DFOrderP );

extern	OSErr	ProcessFileExtents( SGlobPtr GPtr, SFCB *fcb, UInt8 forkType, UInt16 flags, Boolean isExtentsBTree, Boolean *hasOverflowExtents, UInt32 *blocksUsed  );

/* Function to get return file path/name given an ID */
extern 	OSErr 	GetSystemFileName(UInt32 fileID, char *filename, unsigned int *filenamelen);
extern 	OSErr 	GetFileNamePathByID(SGlobPtr GPtr, UInt32 fileID, char *fullPath, unsigned int *fullPathLen, char *fileName, unsigned int *fileNameLen, u_int16_t *status);
#define FNAME_BUF2SMALL	0x001	/* filename buffer was too small */
#define FNAME_BIGNAME	0x002	/* filename is greater than NAME_MAX bytes */
#define FPATH_BUF2SMALL	0x010	/* path buffer was too small */
#define	FPATH_BIGNAME	0x020	/* intermediate component in path is greater than NAME_MAX bytes */
#define	F_RESERVE_FILEID 0x100	/* file ID was less than kHFSFirstUserCatalogNodeID */

/* ------------------------------- From SUtils.c --------------------------------- */

extern	int		AllocBTN( SGlobPtr GPtr, short FilRefN, UInt32 NodeNum );

extern	int		IntError( SGlobPtr GPtr, OSErr ErrCode );

extern	void	RcdError( SGlobPtr GPtr, OSErr ErrCode );

extern	RepairOrderPtr AllocMinorRepairOrder( SGlobPtr GPtr, int extraBytes );

extern	void	SetDFAStage( UInt32 stage );
extern	UInt32	GetDFAGlobals( void );

extern void		InitializeVolumeObject( SGlobPtr GPtr );
extern void 	CheckEmbeddedVolInfoInMDBs( SGlobPtr GPtr );
extern VolumeObjectPtr  GetVolumeObjectPtr( void );
extern OSErr	GetVolumeObjectVHB( BlockDescriptor * theBlockDescPtr );
extern void 	GetVolumeObjectBlockNum( UInt64 * theBlockNumPtr );
extern OSErr 	GetVolumeObjectAlternateBlock( BlockDescriptor * theBlockDescPtr );
extern OSErr 	GetVolumeObjectPrimaryBlock( BlockDescriptor * theBlockDescPtr );
extern void 	GetVolumeObjectAlternateBlockNum( UInt64 * theBlockNumPtr );
extern void 	GetVolumeObjectPrimaryBlockNum( UInt64 * theBlockNumPtr );
extern OSErr	GetVolumeObjectAlternateMDB( BlockDescriptor * theBlockDescPtr );
extern OSErr 	GetVolumeObjectPrimaryMDB( BlockDescriptor * theBlockDescPtr );
extern OSErr 	GetVolumeObjectVHBorMDB( BlockDescriptor * theBlockDescPtr );
extern void 	PrintName( int theCount, const UInt8 *theNamePtr, Boolean isUnicodeString );
extern void 	PrintVolumeObject( void );
extern Boolean 	VolumeObjectIsValid( void );
extern Boolean 	VolumeObjectIsHFSPlus( void );
extern Boolean 	VolumeObjectIsHFS( void );
extern Boolean 	VolumeObjectIsEmbeddedHFSPlus( void );
extern Boolean 	VolumeObjectIsPureHFSPlus( void );
extern void 	RecordXAttrBits(SGlobPtr GPtr, UInt16 flags, HFSCatalogNodeID fileid, UInt16 btreetype); 
extern int ComparePrimeBuckets(SGlobPtr GPtr, UInt16 BitMask); 

extern	void	InvalidateCalculatedVolumeBitMap( SGlobPtr GPtr );

extern	OSErr	GetVolumeFeatures( SGlobPtr GPtr );

OSErr	FlushAlternateVolumeControlBlock( SVCB *vcb, Boolean isHFSPlus );

extern	void	ConvertToHFSPlusExtent(const HFSExtentRecord oldExtents, HFSPlusExtentRecord newExtents);


/* ------------------------------- From SVerify1.c -------------------------------- */

extern	OSErr	CheckCatalogBTree( SGlobPtr GPtr );	//	catalog btree check

extern	OSErr	CatFlChk( SGlobPtr GPtr );		//	catalog file check
	
extern	OSErr	CatHChk( SGlobPtr GPtr );		//	catalog hierarchy check

extern	OSErr	ExtBTChk( SGlobPtr GPtr );		//	extent btree check

extern	OSErr	ExtFlChk( SGlobPtr GPtr );		//	extent file check

extern	OSErr	AttrBTChk( SGlobPtr GPtr );		//	attributes btree check

extern	OSErr	IVChk( SGlobPtr GPtr );

extern	int	CheckForClean( SGlobPtr GPtr, Boolean markClean );

extern  int	CheckIfJournaled(SGlobPtr GPtr);

extern	OSErr	VInfoChk( SGlobPtr GPtr );

extern	OSErr	VLockedChk( SGlobPtr GPtr );

extern	void	BuildExtentKey( Boolean isHFSPlus, UInt8 forkType, HFSCatalogNodeID fileNumber, UInt32 blockNumber, void * key );

extern	OSErr	OrphanedFileCheck( SGlobPtr GPtr, Boolean *problemsFound );

extern	int		cmpLongs (const void *a, const void *b);

extern	OSErr	AddExtentToOverlapList( SGlobPtr GPtr, HFSCatalogNodeID fileNumber, UInt32 extentStartBlock, UInt32 extentBlockCount, UInt8 forkType );

/* ------------------------------- From SVerify2.c -------------------------------- */

typedef int (* CheckLeafRecordProcPtr)(SGlobPtr GPtr, void *key, void *record, UInt16 recordLen);

extern	int  BTCheck(SGlobPtr GPtr, short refNum, CheckLeafRecordProcPtr checkLeafRecord);

extern	int		BTMapChk( SGlobPtr GPtr, short FilRefN );

extern	OSErr	ChkCName( SGlobPtr GPtr, const CatalogName *name, Boolean unicode );	//	check catalog name

extern	OSErr	CmpBTH( SGlobPtr GPtr, SInt16 fileRefNum );

extern	int		CmpBTM( SGlobPtr GPtr, short FilRefN );

extern	int		CmpMDB( SGlobPtr GPtr, HFSMasterDirectoryBlock * mdbP);

extern	int		CmpVBM( SGlobPtr GPtr );

extern	OSErr	CmpBlock( void *block1P, void *block2P, UInt32 length ); /* same as 'memcmp', but EQ/NEQ only */
	
extern	OSErr	ChkExtRec ( SGlobPtr GPtr, const void *extents );


/* -------------------------- From SRebuildCatalogBTree.c ------------------------- */

extern	OSErr 	RebuildCatalogBTree( SGlobPtr theSGlobPtr );


/* -------------------------- From SCatalog.c ------------------------- */

extern OSErr	UpdateFolderCount( 	SVCB *vcb, 
									HFSCatalogNodeID pid, 
									const CatalogName *name, 
									SInt16 newType,
									UInt32 hint, 
									SInt16 valenceDelta );

/* ------------------------------- From SExtents.c -------------------------------- */
OSErr	ZeroFileBlocks( SVCB *vcb, SFCB *fcb, UInt32 startingSector, UInt32 numberOfSectors );

OSErr MapFileBlockC (
	SVCB		*vcb,				// volume that file resides on
	SFCB			*fcb,				// FCB of file
	UInt32			numberOfBytes,		// number of contiguous bytes desired
	UInt32			sectorOffset,		// starting offset within file (in 512-byte sectors)
	UInt64			*startSector,		// first 512-byte volume sector (NOT an allocation block)
	UInt32			*availableBytes);	// number of contiguous bytes (up to numberOfBytes)

OSErr DeallocateFile(SVCB *vcb, CatalogRecord * fileRec);

OSErr ExtendFileC (
	SVCB		*vcb,				// volume that file resides on
	SFCB			*fcb,				// FCB of file to truncate
	UInt32			sectorsToAdd,		// number of sectors to allocate
	UInt32			flags,				// EFContig and/or EFAll
	UInt32			*actualSectorsAdded); // number of bytes actually allocated
	
OSErr FlushExtentFile( SVCB *vcb );

void ExtDataRecToExtents(
	const HFSExtentRecord	oldExtents,
	HFSPlusExtentRecord	newExtents);

OSErr UpdateExtentRecord (
	const SVCB		*vcb,
	SFCB					*fcb,
	const HFSPlusExtentKey	*extentFileKey,
	HFSPlusExtentRecord		extentData,
	UInt32					extentBTreeHint);

OSErr	CheckFileExtents( SGlobPtr GPtr, UInt32 fileNumber, UInt8 forkType,
                          const void *extents, UInt32 *blocksUsed );
OSErr	GetBTreeHeader( SGlobPtr GPtr, SFCB* fcb, BTHeaderRec *header );
OSErr	CompareVolumeBitMap( SGlobPtr GPtr, SInt32 whichBuffer );
OSErr	CompareVolumeHeader( SGlobPtr GPtr, HFSPlusVolumeHeader *vh );
OSErr	CreateExtentsBTreeControlBlock( SGlobPtr GPtr );
OSErr	CreateCatalogBTreeControlBlock( SGlobPtr GPtr );
OSErr	CreateAttributesBTreeControlBlock( SGlobPtr GPtr );
OSErr	CreateExtendedAllocationsFCB( SGlobPtr GPtr );


OSErr	CacheWriteInPlace( SVCB *vcb, UInt32 fileRefNum,  HIOParam *iopb, UInt64 currentPosition,
	UInt32 maximumBytes, UInt32 *actualBytes );


/* Generic B-tree call back routines */
OSStatus GetBlockProc (SFCB *filePtr, UInt32 blockNum, GetBlockOptions options, BlockDescriptor *block);
OSStatus ReleaseBlockProc (SFCB *filePtr, BlockDescPtr blockPtr, ReleaseBlockOptions options);
OSStatus SetEndOfForkProc (SFCB *filePtr, FSSize minEOF, FSSize maxEOF);
OSStatus SetBlockSizeProc (SFCB *filePtr, ByteCount blockSize, ItemCount minBlockCount);

void DFA_PrepareInputName(ConstStr31Param name, Boolean isHFSPlus, CatalogName *catalogName);

extern	UInt32	CatalogNameSize( const CatalogName *name, Boolean isHFSPlus);

void	SetupFCB( SVCB *vcb, SInt16 refNum, UInt32 fileID, UInt32 fileClumpSize );


extern	void	CalculateItemCount( SGlob *GPtr, UInt64 *itemCount, UInt64 *onePercent );



//	Macros
extern		BTreeControlBlock*	GetBTreeControlBlock( short refNum );
#define		GetBTreeControlBlock(refNum)	((BTreeControlBlock*) ResolveFCB((refNum))->fcbBtree)

/*	The following macro marks a VCB as dirty by setting the upper 8 bits of the flags*/
EXTERN_API_C( void )
MarkVCBDirty					(SVCB *			vcb);

#define	MarkVCBDirty(vcb)	((void) (vcb->vcbFlags |= 0xFF00))
EXTERN_API_C( void )
MarkVCBClean					(SVCB *			vcb);

#define	MarkVCBClean(vcb)	((void) (vcb->vcbFlags &= 0x00FF))
EXTERN_API_C( Boolean )
IsVCBDirty						(SVCB *			vcb);

#define	IsVCBDirty(vcb)		((Boolean) ((vcb->vcbFlags & 0xFF00) != 0))


extern	pascal void M_Debugger(void);
extern	pascal void M_DebugStr(ConstStr255Param debuggerMsg);
#if ( DEBUG_BUILD )
	#define	M_Debuger()					Debugger()
	#define	M_DebugStr( debuggerMsg )	DebugStr( debuggerMsg )
#else
	#define	M_Debuger()
	#define	M_DebugStr( debuggerMsg )
#endif


/*	Test for error and return if error occurred*/
EXTERN_API_C( void )
ReturnIfError					(OSErr 					result);

#define	ReturnIfError(result)					if ( (result) != noErr ) return (result); else ;
/*	Test for passed condition and return if true*/
EXTERN_API_C( void )
ReturnErrorIf					(Boolean 				condition,
								 OSErr 					result);

#define	ReturnErrorIf(condition, error)			if ( (condition) )	return( (error) );
/*	Exit function on error*/
EXTERN_API_C( void )
ExitOnError						(OSErr 					result);

#define	ExitOnError( result )					if ( ( result ) != noErr )	goto ErrorExit; else ;

/*	Return the low 16 bits of a 32 bit value, pinned if too large*/
EXTERN_API_C( UInt16 )
LongToShort						(UInt32 				l);

#define	LongToShort( l )	l <= (UInt32)0x0000FFFF ? ((UInt16) l) : ((UInt16) 0xFFFF)


EXTERN_API_C( UInt32 )
GetDFAStage						(void);

EXTERN_API_C(OSErr)
DeleteCatalogNode(SVCB *vcb, UInt32 pid, const CatalogName * name, UInt32 hint);

EXTERN_API_C(OSErr)
GetCatalogNode(SVCB *vcb, UInt32 pid, const CatalogName * name, UInt32 hint, CatalogRecord *data);

EXTERN_API_C( SInt32 )
CompareCatalogKeys				(HFSCatalogKey *		searchKey,
								 HFSCatalogKey *		trialKey);

EXTERN_API_C( SInt32 )
CompareExtendedCatalogKeys		(HFSPlusCatalogKey *	searchKey,
								 HFSPlusCatalogKey *	trialKey);
EXTERN_API_C( SInt32 )
CaseSensitiveCatalogKeyCompare (HFSPlusCatalogKey * searchKey,
                                HFSPlusCatalogKey * trialKey);

EXTERN_API_C( SInt32 )
CompareExtentKeys				(const HFSExtentKey *	searchKey,
								 const HFSExtentKey *	trialKey);

EXTERN_API_C( SInt32 )
CompareExtentKeysPlus			(const HFSPlusExtentKey * searchKey,
								 const HFSPlusExtentKey * trialKey);
EXTERN_API_C( SInt32 )
CompareAttributeKeys			(const AttributeKey * searchKey,  const AttributeKey * trialKey);
EXTERN_API( SFCB* )
ResolveFCB						(short 					fileRefNum);

EXTERN_API_C( OSErr )
ValidVolumeHeader				(HFSPlusVolumeHeader *	volumeHeader);


/* Old B-tree Manager API (going away soon!) */

EXTERN_API_C( OSErr )
SearchBTreeRecord				(SFCB 				*fcb,
								 const void *			key,
								 UInt32 				hint,
								 void *					foundKey,
								 void *					data,
								 UInt16 *				dataSize,
								 UInt32 *				newHint);

EXTERN_API_C( OSErr )
GetBTreeRecord					(SFCB 				*fcb,
								 SInt16 				selectionIndex,
								 void *					key,
								 void *					data,
								 UInt16 *				dataSize,
								 UInt32 *				newHint);

EXTERN_API_C( OSErr )
InsertBTreeRecord				(SFCB 				*fcb,
								 const void *			key,
								 const void *			data,
								 UInt16 				dataSize,
								 UInt32 *				newHint);

EXTERN_API_C( OSErr )
DeleteBTreeRecord				(SFCB 				*fcb,
								 const void *			key);

EXTERN_API_C( OSErr )
ReplaceBTreeRecord				(SFCB 				*fcb,
								 const void *			key,
								 UInt32 				hint,
								 void *					newData,
								 UInt16 				dataSize,
								 UInt32 *				newHint);

EXTERN_API_C( void )
InitBTreeHeader					(UInt32 				fileSize,
								 UInt32 				clumpSize,
								 UInt16 				nodeSize,
								 UInt16 				recordCount,
								 UInt16 				keySize,
								 UInt32 				attributes,
								 UInt32 *				mapNodes,
								 void *					buffer);

EXTERN_API_C( OSErr )
UpdateFreeCount					(SVCB *			vcb);


EXTERN_API_C(Boolean)
NodesAreContiguous(	SFCB		*fcb,
			UInt32		nodeSize);



UInt32 GetTimeUTC(void);
UInt32 GetTimeLocal(Boolean forHFS);

OSErr FlushVolumeControlBlock( SVCB *vcb );

pascal short ResolveFileRefNum(SFCB * fileCtrlBlockPtr);

extern UInt32	CatalogNameLength( const CatalogName *name, Boolean isHFSPlus);

extern void		CopyCatalogName( const CatalogName *srcName, CatalogName *dstName, Boolean isHFSPLus);

extern	void	UpdateCatalogName( ConstStr31Param srcName, Str31 destName);

extern	void	BuildCatalogKey( HFSCatalogNodeID parentID, const CatalogName *name, Boolean isHFSPlus,
								 CatalogKey *key);

extern void		UpdateVolumeEncodings( SVCB *volume, TextEncoding encoding);


OSErr BlockAllocate (SVCB *vcb, UInt32 startingBlock, UInt32 blocksRequested, UInt32 blocksMaximum,
			Boolean forceContiguous, UInt32 *actualStartBlock, UInt32 *actualNumBlocks);
OSErr	BlockDeallocate ( SVCB *vcb, UInt32 firstBlock, UInt32 numBlocks);
UInt32	DivideAndRoundUp( UInt32 numerator, UInt32 denominator);

OSErr InitializeBlockCache ( UInt32 blockSize, UInt32 blockCount );

void	SetFCBSPtr( Ptr value );
Ptr	GetFCBSPtr( void );


/* 
 * UTF-8 conversion routines
 */
extern int utf_decodestr(const unsigned char *, size_t, u_int16_t *, size_t *);
extern int utf_encodestr(const u_int16_t *, size_t, unsigned char *, size_t *);

/* 
 * HardLink checking routines
 */
extern int   HardLinkCheckBegin(SGlobPtr gp, void** cookie);
extern void  HardLinkCheckEnd(void * cookie);
extern void  CaptureHardLink(void * cookie, UInt32 linkID);
extern int   CheckHardLinks(void *cookie);

/*
 * Volume Bitmap checking routines
 */
extern int  BitMapCheckBegin(SGlobPtr g);
extern int  BitMapCheckEnd(void);
extern int  CaptureBitmapBits(UInt32 startBit, UInt32 bitCount);
extern int  ReleaseBitmapBits(UInt32 startBit, UInt32 bitCount);
extern int  CheckVolumeBitMap(SGlobPtr g, Boolean repair);
extern void UpdateFreeBlockCount(SGlobPtr g);

#ifdef __cplusplus
};
#endif

#if LINUX
#undef XATTR_MAXNAMELEN
#endif

#endif /* __SCAVENGER__ */
