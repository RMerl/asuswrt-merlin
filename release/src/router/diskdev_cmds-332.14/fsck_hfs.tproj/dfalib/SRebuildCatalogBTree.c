/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2002 Apple Computer, Inc.  All Rights
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
/*
	File:		SRebuildCatalogBTree.c

	Contains:	This file contains the catalog BTree rebuild code.
	
	Written by:	Jerry Cottingham

	Copyright:	© 1986, 1990, 1992-2002 by Apple Computer, Inc., all rights reserved.

*/

#define SHOW_ELAPSED_TIMES  0
#define DEBUG_REBUILD  0

#if SHOW_ELAPSED_TIMES
#include <sys/time.h>
#endif

#include "Scavenger.h"
#include "../cache.h"

/* internal routine prototypes */

static OSErr 	CreateNewCatalogBTree( SGlobPtr theSGlobPtr );
static OSErr 	DeleteCatalogBTree( SGlobPtr theSGlobPtr, SFCB * theFCBPtr );
static OSErr 	InitializeBTree(	BTreeControlBlock * theBTreeCBPtr,
        							UInt32 *			theBytesUsedPtr,
        							UInt32 *			theMapNodeCountPtr 	);
static OSErr 	ReleaseExtentsInExtentsBTree(	SGlobPtr theSGlobPtr, 
												SFCB * theFCBPtr );
static OSErr 	ValidateRecordLength(	SGlobPtr theSGlobPtr,
										CatalogRecord * theRecPtr,
										UInt32 theRecSize );
static OSErr 	WriteMapNodes(  BTreeControlBlock * theBTreeCBPtr, 
								UInt32 				theFirstMapNode, 
								UInt32 				theNodeCount );

#if DEBUG_REBUILD 
static void PrintBTHeaderRec( BTHeaderRec * thePtr );
static void PrintNodeDescriptor( NodeDescPtr thePtr );
static void PrintBTreeKey( KeyPtr thePtr, BTreeControlBlock * theBTreeCBPtr );
static void PrintIndexNodeRec( UInt32 theNodeNum );
static void PrintLeafNodeRec( HFSPlusCatalogFolder * thePtr );
#endif 

void SETOFFSET ( void *buffer, UInt16 btNodeSize, SInt16 recOffset, SInt16 vecOffset );
#define SETOFFSET( buf,ndsiz,offset,rec )		\
	( *(SInt16 *)((UInt8 *)(buf) + (ndsiz) + (-2 * (rec))) = (offset) )


//_________________________________________________________________________________
//
//	Routine:	RebuildCatalogBTree
//
//	Purpose:	Attempt to rebuild the catalog B-Tree file using an existing
//				catalog B-Tree file.  When successful a new catalog file will
//				exist and the old one will be deleted.  The MDB an alternate MDB
//				will be updated to point to the new file.  
//				
//				The current implementation requires leaf node records to be in 
//				good shape.  We will rebuild if index nodes are damaged or leaf 
//				nodes have key order problems but not much else.  The rebuild
//				relies on the btree scanner code to locate leaf nodes then it
//				extracts leaf node records and adds them to the new catalog file.
//				Any kind of insert error will abort the rebuild (leaving the 
//				existing catalog file as it was found).
//
//	Inputs:
//		SGlobPtr->calculatedCatalogBTCB
//				need this as a model and in order to extract leaf records.
//		SGlobPtr->calculatedCatalogFCB
//				need this as a model and in order to extract leaf records.
//		SGlobPtr->calculatedRepairFCB
//				pointer to our repair FCB.
//		SGlobPtr->calculatedRepairBTCB
//				pointer to our repair BTreeControlBlock.
//
//	Outputs:
//		SGlobPtr->calculatedVCB
//				this will get mostly filled in here.  On input it is not fully
//				set up.
//		SGlobPtr->calculatedCatalogFCB
//				tis will refer to the new catalog file.
//
//	Result:
//		various error codes when problem occur or noErr if rebuild completed
//
//	to do:
//		have an option where we get back what we can.
//
//	Notes:
//		- we currently only support rebuilding the catalog file B-Tree
//		- requires calculatedCatalogBTCB and calculatedCatalogFCB to be valid!
//_________________________________________________________________________________

OSErr	RebuildCatalogBTree( SGlobPtr theSGlobPtr )
{
	BlockDescriptor  		myBlockDescriptor;
	BTreeKeyPtr 			myCurrentKeyPtr;
	CatalogRecord * 		myCurrentDataPtr;
	SFCB *					myFCBPtr = NULL;
	SVCB *					myVCBPtr;
	UInt32					myDataSize;
	UInt32					myHint;
	OSErr					myErr;
	Boolean 				isHFSPlus;
	
#if SHOW_ELAPSED_TIMES 
	struct timeval 			myStartTime;
	struct timeval 			myEndTime;
	struct timeval 			myElapsedTime;
	struct timezone 		zone;
#endif
 
 	theSGlobPtr->TarID = kHFSCatalogFileID;  /* target = catalog file */
	theSGlobPtr->TarBlock = 0;
	myBlockDescriptor.buffer = NULL;
	myVCBPtr = theSGlobPtr->calculatedVCB;

	myErr = BTScanInitialize( theSGlobPtr->calculatedCatalogFCB, 
							  &theSGlobPtr->scanState );
	if ( noErr != myErr )
		goto ExitThisRoutine;

	// some VCB fields that we need may not have been calculated so we get it from the MDB.
	// this can happen because the fsck_hfs code path to fully set up the VCB may have been 
	// aborted if an error was found that would trigger a catalog rebuild.  For example,
	// if a leaf record was found to have a keys out of order then the verify phase of the
	// B-Tree check would be aborted and we would come directly (if allowable) to here.
	isHFSPlus = ( myVCBPtr->vcbSignature == kHFSPlusSigWord );

	myErr = GetVolumeObjectVHBorMDB( &myBlockDescriptor );
	if ( noErr != myErr )
		goto ExitThisRoutine;

	if ( isHFSPlus )
	{
		HFSPlusVolumeHeader	*		myVHBPtr;
		
		myVHBPtr = (HFSPlusVolumeHeader	*) myBlockDescriptor.buffer;
		myVCBPtr->vcbFreeBlocks = myVHBPtr->freeBlocks;
		myVCBPtr->vcbFileCount = myVHBPtr->fileCount;
		myVCBPtr->vcbFolderCount = myVHBPtr->folderCount;
		myVCBPtr->vcbEncodingsBitmap = myVHBPtr->encodingsBitmap;
		myVCBPtr->vcbRsrcClumpSize = myVHBPtr->rsrcClumpSize;
		myVCBPtr->vcbDataClumpSize = myVHBPtr->dataClumpSize;
		
		//	check out creation and last mod dates
		myVCBPtr->vcbCreateDate	= myVHBPtr->createDate;	
		myVCBPtr->vcbModifyDate	= myVHBPtr->modifyDate;		
		myVCBPtr->vcbCheckedDate = myVHBPtr->checkedDate;		
		myVCBPtr->vcbBackupDate = myVHBPtr->backupDate;	
		myVCBPtr->vcbCatalogFile->fcbClumpSize = myVHBPtr->catalogFile.clumpSize;

		// 3882639: Removed check for volume attributes in HFS Plus 
		myVCBPtr->vcbAttributes = myVHBPtr->attributes;

		CopyMemory( myVHBPtr->finderInfo, myVCBPtr->vcbFinderInfo, sizeof(myVCBPtr->vcbFinderInfo) );
	}
	else
	{
		HFSMasterDirectoryBlock	*	myMDBPtr;
		myMDBPtr = (HFSMasterDirectoryBlock	*) myBlockDescriptor.buffer;
		myVCBPtr->vcbFreeBlocks = myMDBPtr->drFreeBks;
		myVCBPtr->vcbFileCount = myMDBPtr->drFilCnt;
		myVCBPtr->vcbFolderCount = myMDBPtr->drDirCnt;
		myVCBPtr->vcbDataClumpSize = myMDBPtr->drClpSiz;
		myVCBPtr->vcbCatalogFile->fcbClumpSize = myMDBPtr->drCTClpSiz;
		myVCBPtr->vcbNmRtDirs = myMDBPtr->drNmRtDirs;

		//	check out creation and last mod dates
		myVCBPtr->vcbCreateDate	= myMDBPtr->drCrDate;		
		myVCBPtr->vcbModifyDate	= myMDBPtr->drLsMod;			

		//	verify volume attribute flags
		if ( (myMDBPtr->drAtrb & VAtrb_Msk) == 0 )
			myVCBPtr->vcbAttributes = myMDBPtr->drAtrb;
		else 
			myVCBPtr->vcbAttributes = VAtrb_DFlt;
		myVCBPtr->vcbBackupDate = myMDBPtr->drVolBkUp;		
		myVCBPtr->vcbVSeqNum = myMDBPtr->drVSeqNum;		
		CopyMemory( myMDBPtr->drFndrInfo, myVCBPtr->vcbFinderInfo, sizeof(myMDBPtr->drFndrInfo) );
	}
	(void) ReleaseVolumeBlock( myVCBPtr, &myBlockDescriptor, kReleaseBlock );
	myBlockDescriptor.buffer = NULL;

	// create the new catalog BTree file
	myErr = CreateNewCatalogBTree( theSGlobPtr );
	if ( noErr != myErr )
		goto ExitThisRoutine;
	myFCBPtr = theSGlobPtr->calculatedRepairFCB;

#if SHOW_ELAPSED_TIMES
	gettimeofday( &myStartTime, &zone );
#endif
	
	while ( true )
	{
		/* scan the btree catalog file for leaf records */
		myErr = BTScanNextRecord( &theSGlobPtr->scanState, 
								  (void **) &myCurrentKeyPtr, 
								  (void **) &myCurrentDataPtr, 
								  &myDataSize  );
		if ( noErr != myErr )
			break;
		
		/* do some validation on the record */
		theSGlobPtr->TarBlock = theSGlobPtr->scanState.nodeNum;
		myErr = ValidateRecordLength( theSGlobPtr, myCurrentDataPtr, myDataSize );
		if ( noErr != myErr )
		{
#if DEBUG_REBUILD 
			{
				printf( "%s - ValidateRecordLength failed! \n", __FUNCTION__ );
				printf( "%s - record %d in node %d is not recoverable. \n", 
						__FUNCTION__, (theSGlobPtr->scanState.recordNum - 1), 
						theSGlobPtr->scanState.nodeNum );
			}
#endif
			myErr = R_RFail;
			break;  // this implementation does not handle partial rebuilds (all or none)
		}

		/* insert this record into the new btree file */
		myErr = InsertBTreeRecord( myFCBPtr, myCurrentKeyPtr,
								   myCurrentDataPtr, myDataSize, &myHint );
		if ( noErr != myErr )
		{
#if DEBUG_REBUILD 
			{
				printf( "%s - InsertBTreeRecord failed with err %d 0x%02X \n", 
					__FUNCTION__, myErr, myErr );
				printf( "%s - record %d in node %d is not recoverable. \n", 
						__FUNCTION__, (theSGlobPtr->scanState.recordNum - 1), 
						theSGlobPtr->scanState.nodeNum );
				PrintBTreeKey( myCurrentKeyPtr, theSGlobPtr->calculatedCatalogBTCB );
			}
#endif
			myErr = R_RFail;
			break;  // this implementation does not handle partial rebuilds (all or none)
		}
	}

#if SHOW_ELAPSED_TIMES
	gettimeofday( &myEndTime, &zone );
	timersub( &myEndTime, &myStartTime, &myElapsedTime );
	printf( "\n%s - rebuild catalog elapsed time \n", __FUNCTION__ );
	printf( ">>>>>>>>>>>>> secs %d msecs %d \n\n", myElapsedTime.tv_sec, myElapsedTime.tv_usec );
#endif

	if ( btNotFound == myErr )
		myErr = noErr;
	if ( noErr != myErr )
		goto ExitThisRoutine;

	/* update our header node on disk from our BTreeControlBlock */
	myErr = BTFlushPath( myFCBPtr );
	if ( noErr != myErr ) 
		goto ExitThisRoutine;
	myErr = CacheFlush( myVCBPtr->vcbBlockCache );
	if ( noErr != myErr ) 
		goto ExitThisRoutine;

	/* switch old catalog file with our new one */
	theSGlobPtr->calculatedRepairFCB = theSGlobPtr->calculatedCatalogFCB;
	theSGlobPtr->calculatedCatalogFCB = myFCBPtr;
	myVCBPtr->vcbCatalogFile = myFCBPtr;
	
	// todo - add code to allow new catalog file to be allocated in extents.
	// Note when we do allow this the swap of catalog files gets even more 
	// tricky since extent record key contains the file ID.  The rebuilt 
	// catalog file has file ID kHFSRepairCatalogFileID when it is created.
	
	MarkVCBDirty( myVCBPtr );
	myErr = FlushAlternateVolumeControlBlock( myVCBPtr, isHFSPlus );
	if ( noErr != myErr )
	{
		// we may be totally screwed if we get here, try to recover
		theSGlobPtr->calculatedCatalogFCB = theSGlobPtr->calculatedRepairFCB;
		theSGlobPtr->calculatedRepairFCB = myFCBPtr;
		myVCBPtr->vcbCatalogFile = theSGlobPtr->calculatedCatalogFCB;
		MarkVCBDirty( myVCBPtr );
		(void) FlushAlternateVolumeControlBlock( myVCBPtr, isHFSPlus );
		goto ExitThisRoutine;
	}	

	/* release space occupied by old catalog BTree file */
	(void) DeleteCatalogBTree( theSGlobPtr, theSGlobPtr->calculatedRepairFCB );

ExitThisRoutine:
	if ( myBlockDescriptor.buffer != NULL )
		(void) ReleaseVolumeBlock( myVCBPtr, &myBlockDescriptor, kReleaseBlock );

	if ( myErr != noErr && myFCBPtr != NULL ) 
		(void) DeleteCatalogBTree( theSGlobPtr, myFCBPtr );
	BTScanTerminate( &theSGlobPtr->scanState  );

	return( myErr );
	
} /* RebuildCatalogBTree */


//_________________________________________________________________________________
//
//	Routine:	CreateNewCatalogBTree
//
//	Purpose:	Create and Initialize a new catalog B-Tree on the target volume 
//				using the physical size of the existing catalog file as an initial 
//				size.
//
//	NOTES:		we force this to be contiguous in order to get this into Jaguar.
//				Allowing the new catalog file to go into extents makes the swap
//				of the old and new catalog files complicated.  The extent records
//				are keyed by file ID and the new (rebuilt) catalog starts out as
//				file Id kHFSRepairCatalogFileID.  If there were extents then we
//				would have to fix up the extent records in the extent B-Tree.
//
//	todo:		Don't force new catalog file to be contiguous
//
//	Inputs:
//		SGlobPtr	global state set up by fsck_hfs.  We depend upon the 
//					manufactured catalog and repair FCBs.
//
//	Outputs:
//		calculatedRepairBTCB 	fill in the BTreeControlBlock for new B-Tree file.
//		calculatedRepairFCB		fill in the SFCB for the new B-Tree file
//
//	Result:
//		various error codes when problems occur or noErr if all is well
//
//_________________________________________________________________________________

static OSErr CreateNewCatalogBTree( SGlobPtr theSGlobPtr )
{
	OSErr					myErr;
	BTreeControlBlock *		myBTreeCBPtr;
	SVCB *					myVCBPtr;
	SFCB *					myFCBPtr;
	UInt32 					myBytesUsed = 0;
	UInt32 					myMapNodeCount;
	FSSize					myNewEOF;
	BTHeaderRec				myHeaderRec;
	
	myBTreeCBPtr = theSGlobPtr->calculatedRepairBTCB;
	myFCBPtr = theSGlobPtr->calculatedRepairFCB;
	ClearMemory( (Ptr) myFCBPtr, sizeof( *myFCBPtr ) );
	ClearMemory( (Ptr) myBTreeCBPtr, sizeof( *myBTreeCBPtr ) );

	// Create new Catalog FCB
	myVCBPtr = theSGlobPtr->calculatedCatalogFCB->fcbVolume;
	myFCBPtr->fcbFileID = kHFSRepairCatalogFileID;
	myFCBPtr->fcbVolume = myVCBPtr;
	myFCBPtr->fcbBtree = myBTreeCBPtr;
	myFCBPtr->fcbBlockSize = theSGlobPtr->calculatedCatalogBTCB->nodeSize; 

	// Create new BTree Control Block
	myBTreeCBPtr->fcbPtr = myFCBPtr;
	myBTreeCBPtr->btreeType = kHFSBTreeType;
	myBTreeCBPtr->keyCompareType = theSGlobPtr->calculatedCatalogBTCB->keyCompareType;
	myBTreeCBPtr->keyCompareProc = theSGlobPtr->calculatedCatalogBTCB->keyCompareProc;
	myBTreeCBPtr->nodeSize = theSGlobPtr->calculatedCatalogBTCB->nodeSize;
	myBTreeCBPtr->maxKeyLength = theSGlobPtr->calculatedCatalogBTCB->maxKeyLength;
	if ( myVCBPtr->vcbSignature == kHFSPlusSigWord )
		myBTreeCBPtr->attributes |= ( kBTBigKeysMask + kBTVariableIndexKeysMask );
	myBTreeCBPtr->getBlockProc = GetFileBlock;
	myBTreeCBPtr->releaseBlockProc = ReleaseFileBlock;
	myBTreeCBPtr->setEndOfForkProc = SetEndOfForkProc;

	myNewEOF = theSGlobPtr->calculatedCatalogFCB->fcbPhysicalSize;

	myErr = myBTreeCBPtr->setEndOfForkProc( myBTreeCBPtr->fcbPtr, myNewEOF, 0 );
	ReturnIfError( myErr );

	/* now set real values in our BTree Control Block */
	myFCBPtr->fcbLogicalSize = myFCBPtr->fcbPhysicalSize;		// new B-tree looks at fcbLogicalSize
	myFCBPtr->fcbClumpSize = myVCBPtr->vcbCatalogFile->fcbClumpSize; 
	
	myBTreeCBPtr->totalNodes = ( myFCBPtr->fcbPhysicalSize / myBTreeCBPtr->nodeSize );
	myBTreeCBPtr->freeNodes = myBTreeCBPtr->totalNodes;

	// Initialize our new BTree (write out header node and an empty leaf node)
	myErr = InitializeBTree( myBTreeCBPtr, &myBytesUsed, &myMapNodeCount );
	ReturnIfError( myErr );
	
	// Update our BTreeControlBlock from BTHeaderRec we just wrote out
	myErr = GetBTreeHeader( theSGlobPtr, myFCBPtr, &myHeaderRec );
	ReturnIfError( myErr );
	
	myBTreeCBPtr->treeDepth = myHeaderRec.treeDepth;
	myBTreeCBPtr->rootNode = myHeaderRec.rootNode;
	myBTreeCBPtr->leafRecords = myHeaderRec.leafRecords;
	myBTreeCBPtr->firstLeafNode = myHeaderRec.firstLeafNode;
	myBTreeCBPtr->lastLeafNode = myHeaderRec.lastLeafNode;
	myBTreeCBPtr->totalNodes = myHeaderRec.totalNodes;
	myBTreeCBPtr->freeNodes = myHeaderRec.freeNodes;
	myBTreeCBPtr->maxKeyLength = myHeaderRec.maxKeyLength;

	if ( myMapNodeCount > 0 )
	{
		myErr = WriteMapNodes( myBTreeCBPtr, (myBytesUsed / myBTreeCBPtr->nodeSize ), myMapNodeCount );
		ReturnIfError( myErr );
	}

	return( myErr );
	
} /* CreateNewCatalogBTree */


/*
 * InitializeBTree
 *	
 * This routine manufactures and writes out a Catalog B-Tree header 
 * node and an empty leaf node.
 *
 * Note: Since large volumes can have bigger b-trees they
 * might need to have map nodes setup.
 *
 * this routine originally came from newfs_hfs.tproj ( see 
 * WriteCatalogFile in file makehfs.c) and was modified for fsck_hfs.
 */
static OSErr InitializeBTree(	BTreeControlBlock * theBTreeCBPtr,
        						UInt32 *			theBytesUsedPtr,
        						UInt32 *			theMapNodeCountPtr 	)
{
	OSErr				myErr;
	BlockDescriptor		myNode;
	Boolean 			isHFSPlus = false;
	SVCB *				myVCBPtr;
	BTNodeDescriptor *	myNodeDescPtr;
	BTHeaderRec *		myHeaderRecPtr;
	UInt8 *				myBufferPtr;
	UInt8 *				myBitMapPtr;
	UInt32				myNodeBitsInHeader;
	UInt32				temp;
	SInt16				myOffset;

	myVCBPtr = theBTreeCBPtr->fcbPtr->fcbVolume;
	isHFSPlus = ( myVCBPtr->vcbSignature == kHFSPlusSigWord) ;
	*theMapNodeCountPtr = 0;

	myErr = GetNewNode( theBTreeCBPtr, kHeaderNodeNum, &myNode );
	ReturnIfError( myErr );
	
	myBufferPtr = (UInt8 *) myNode.buffer;
	
	/* FILL IN THE NODE DESCRIPTOR:  */
	myNodeDescPtr 		= (BTNodeDescriptor *) myBufferPtr;
	myNodeDescPtr->kind 	= kBTHeaderNode;
	myNodeDescPtr->numRecords = 3;
	myOffset = sizeof( BTNodeDescriptor );

	SETOFFSET( myBufferPtr, theBTreeCBPtr->nodeSize, myOffset, 1 );

	/* FILL IN THE HEADER RECORD:  */
	myHeaderRecPtr = (BTHeaderRec *)((UInt8 *)myBufferPtr + myOffset);
	myHeaderRecPtr->treeDepth		= 1;
	myHeaderRecPtr->rootNode		= 1;
	myHeaderRecPtr->firstLeafNode	= 1;
	myHeaderRecPtr->lastLeafNode	= 1;
	myHeaderRecPtr->nodeSize		= theBTreeCBPtr->nodeSize;
	myHeaderRecPtr->totalNodes		= theBTreeCBPtr->totalNodes;
	myHeaderRecPtr->freeNodes		= myHeaderRecPtr->totalNodes - 2;  /* header and leaf */
	myHeaderRecPtr->clumpSize		= theBTreeCBPtr->fcbPtr->fcbClumpSize;

	if ( isHFSPlus ) 
	{
		myHeaderRecPtr->attributes	|= (kBTVariableIndexKeysMask + kBTBigKeysMask);
		myHeaderRecPtr->maxKeyLength = kHFSPlusCatalogKeyMaximumLength;
		myHeaderRecPtr->keyCompareType = theBTreeCBPtr->keyCompareType;
	} 
	else 
		myHeaderRecPtr->maxKeyLength = kHFSCatalogKeyMaximumLength;

	myOffset += sizeof( BTHeaderRec );
	SETOFFSET( myBufferPtr, theBTreeCBPtr->nodeSize, myOffset, 2 );

	myOffset += kBTreeHeaderUserBytes;
	SETOFFSET( myBufferPtr, theBTreeCBPtr->nodeSize, myOffset, 3 );

	/* FIGURE OUT HOW MANY MAP NODES (IF ANY):  */
	myNodeBitsInHeader = 8 * (theBTreeCBPtr->nodeSize
							- sizeof(BTNodeDescriptor)
							- sizeof(BTHeaderRec)
							- kBTreeHeaderUserBytes
							- (4 * sizeof(SInt16)) );

	if ( myHeaderRecPtr->totalNodes > myNodeBitsInHeader ) 
	{
		UInt32	nodeBitsInMapNode;
		
		myNodeDescPtr->fLink = myHeaderRecPtr->lastLeafNode + 1;
		nodeBitsInMapNode = 8 * (theBTreeCBPtr->nodeSize
								- sizeof(BTNodeDescriptor)
								- (2 * sizeof(SInt16))
								- 2 );
		*theMapNodeCountPtr = (myHeaderRecPtr->totalNodes - myNodeBitsInHeader +
			(nodeBitsInMapNode - 1)) / nodeBitsInMapNode;
		myHeaderRecPtr->freeNodes = myHeaderRecPtr->freeNodes - *theMapNodeCountPtr;
	}

	/* 
	 * FILL IN THE MAP RECORD, MARKING NODES THAT ARE IN USE.
	 * Note - worst case (32MB alloc blk) will have only 18 nodes in use.
	 */
	myBitMapPtr = ((UInt8 *)myBufferPtr + myOffset);
	temp = myHeaderRecPtr->totalNodes - myHeaderRecPtr->freeNodes;

	/* Working a byte at a time is endian safe */
	while ( temp >= 8 ) 
	{ 
		*myBitMapPtr = 0xFF; 
		temp -= 8; 
		myBitMapPtr++; 
	}
	*myBitMapPtr = ~(0xFF >> temp);
	myOffset += myNodeBitsInHeader / 8;

	SETOFFSET( myBufferPtr, theBTreeCBPtr->nodeSize, myOffset, 4 );

	*theBytesUsedPtr = 
		( myHeaderRecPtr->totalNodes - myHeaderRecPtr->freeNodes - *theMapNodeCountPtr ) 
			* theBTreeCBPtr->nodeSize;

	/* write header node */
	myErr = UpdateNode( theBTreeCBPtr, &myNode );
	M_ExitOnError( myErr );
	
	/*
	 * Add an empty leaf node to the new BTree
	 */
	myErr = GetNewNode( theBTreeCBPtr, kHeaderNodeNum + 1, &myNode );
	ReturnIfError( myErr );
	
	myBufferPtr = (UInt8 *) myNode.buffer;
	
	/* FILL IN THE NODE DESCRIPTOR:  */
	myNodeDescPtr 			= (BTNodeDescriptor *) myBufferPtr;
	myNodeDescPtr->kind 	= kBTLeafNode;
	myNodeDescPtr->height 	= 1;
	myNodeDescPtr->numRecords = 0;
	myOffset = sizeof( BTNodeDescriptor );

	SETOFFSET( myBufferPtr, theBTreeCBPtr->nodeSize, myOffset, 1 );

	// write leaf node
	myErr = UpdateNode( theBTreeCBPtr, &myNode );
	M_ExitOnError( myErr );

	return	noErr;

ErrorExit:
	(void) ReleaseNode( theBTreeCBPtr, &myNode );
		
	return( myErr );
	
} /* InitializeBTree */


/*
 * WriteMapNodes
 *	
 * This routine manufactures and writes out a Catalog B-Tree map 
 * node (or nodes if there are more than one).
 *
 * this routine originally came from newfs_hfs.tproj ( see 
 * WriteMapNodes in file makehfs.c) and was modified for fsck_hfs.
 */

static OSErr WriteMapNodes(	BTreeControlBlock * theBTreeCBPtr, 
							UInt32 				theFirstMapNode, 
							UInt32 				theNodeCount )
{
	OSErr				myErr;
	UInt16				i;
	UInt32				mapRecordBytes;
	BTNodeDescriptor *	myNodeDescPtr;
	BlockDescriptor		myNode;

	myNode.buffer = NULL;
	
	/*
	 * Note - worst case (32MB alloc blk) will have
	 * only 18 map nodes. So don't bother optimizing
	 * this section to do multiblock writes!
	 */
	for ( i = 0; i < theNodeCount; i++ ) 
	{
		myErr = GetNewNode( theBTreeCBPtr, theFirstMapNode, &myNode );
		M_ExitOnError( myErr );
	
		myNodeDescPtr = (BTNodeDescriptor *) myNode.buffer;
		myNodeDescPtr->kind			= kBTMapNode;
		myNodeDescPtr->numRecords	= 1;
		
		/* note: must be long word aligned (hence the extra -2) */
		mapRecordBytes = theBTreeCBPtr->nodeSize - sizeof(BTNodeDescriptor) - 2 * sizeof(SInt16) - 2;	
	
		SETOFFSET( myNodeDescPtr, theBTreeCBPtr->nodeSize, sizeof(BTNodeDescriptor), 1 );
		SETOFFSET( myNodeDescPtr, theBTreeCBPtr->nodeSize, sizeof(BTNodeDescriptor) + mapRecordBytes, 2) ;

		if ( (i + 1) < theNodeCount )
			myNodeDescPtr->fLink = ++theFirstMapNode;  /* point to next map node */
		else
			myNodeDescPtr->fLink = 0;  /* this is the last map node */

		myErr = UpdateNode( theBTreeCBPtr, &myNode );
		M_ExitOnError( myErr );
	}
	
	return	noErr;

ErrorExit:
	(void) ReleaseNode( theBTreeCBPtr, &myNode );
		
	return( myErr );
	
} /* WriteMapNodes */


/*
 * DeleteCatalogBTree
 *	
 * This routine will realease all space associated with the Catalog BTree
 * file represented by the FCB passed in.  
 *
 */

enum
{
	kDataForkType			= 0,
	kResourceForkType		= 0xFF
};

static OSErr DeleteCatalogBTree( SGlobPtr theSGlobPtr, SFCB * theFCBPtr )
{
	OSErr			myErr;
	SVCB *			myVCBPtr;
	int				i;
	Boolean 		isHFSPlus;
	Boolean			checkExtentsBTree = true;

	myVCBPtr = theFCBPtr->fcbVolume;
	isHFSPlus = ( myVCBPtr->vcbSignature == kHFSPlusSigWord) ;
	
	if ( isHFSPlus )
	{
		for ( i = 0; i < kHFSPlusExtentDensity; ++i ) 
		{
			if ( theFCBPtr->fcbExtents32[ i ].blockCount == 0 )
			{
				checkExtentsBTree = false;
				break;
			}
			(void) BlockDeallocate( myVCBPtr, 
									theFCBPtr->fcbExtents32[ i ].startBlock, 
									theFCBPtr->fcbExtents32[ i ].blockCount );
			theFCBPtr->fcbExtents32[ i ].startBlock = 0;
			theFCBPtr->fcbExtents32[ i ].blockCount = 0;
		}
	}
	else
	{
		for ( i = 0; i < kHFSExtentDensity; ++i ) 
		{
			if ( theFCBPtr->fcbExtents16[ i ].blockCount == 0 )
			{
				checkExtentsBTree = false;
				break;
			}
			(void) BlockDeallocate( myVCBPtr, 
									theFCBPtr->fcbExtents16[ i ].startBlock, 
									theFCBPtr->fcbExtents16[ i ].blockCount );
			theFCBPtr->fcbExtents16[ i ].startBlock = 0;
			theFCBPtr->fcbExtents16[ i ].blockCount = 0;
		}
	}
	
	if ( checkExtentsBTree )
	{
		(void) ReleaseExtentsInExtentsBTree( theSGlobPtr, theFCBPtr );
		(void) FlushExtentFile( myVCBPtr );
	}

	(void) MarkVCBDirty( myVCBPtr );
	(void) FlushAlternateVolumeControlBlock( myVCBPtr, isHFSPlus );
	myErr = noErr;
	
	return( myErr );
	
} /* DeleteCatalogBTree */


/*
 * ReleaseExtentsInExtentsBTree
 *	
 * This routine will locate extents in the extent BTree then release the space
 * associated with the extents.  It will also delete the BTree record for the
 * extent.
 *
 */

static OSErr ReleaseExtentsInExtentsBTree( 	SGlobPtr theSGlobPtr, 
											SFCB * theFCBPtr )
{
	BTreeIterator       myIterator;
	ExtentRecord		myExtentRecord;
	FSBufferDescriptor  myBTRec;
	ExtentKey *			myKeyPtr;
	SVCB *				myVCBPtr;
	UInt16              myRecLen;
	UInt16				i;
	OSErr				myErr;
	Boolean 			isHFSPlus;

	myVCBPtr = theFCBPtr->fcbVolume;
	isHFSPlus = ( myVCBPtr->vcbSignature == kHFSPlusSigWord );

	// position just before the first extent record for the given File ID.  We 
	// pass in the file ID and a start block of 0 which will put us in a 
	// position for BTIterateRecord (with kBTreeNextRecord) to get the first 
	// extent record.
	ClearMemory( &myIterator, sizeof(myIterator) );
	myBTRec.bufferAddress = &myExtentRecord;
	myBTRec.itemCount = 1;
	myBTRec.itemSize = sizeof(myExtentRecord);
	myKeyPtr = (ExtentKey *) &myIterator.key;

	BuildExtentKey( isHFSPlus, kDataForkType, theFCBPtr->fcbFileID, 
					0, (void *) myKeyPtr );

	// it is now a simple process of getting the next extent record and 
	// cleaning up the allocated space for each one until we hit a 
	// different file ID.
	for ( ;; )
	{
		myErr = BTIterateRecord( theSGlobPtr->calculatedExtentsFCB, 
								 kBTreeNextRecord, &myIterator,
								 &myBTRec, &myRecLen );
		if ( noErr != myErr )
		{
			myErr = noErr;
			break;
		}
		
		/* deallocate space for the extents we found */
		if ( isHFSPlus )
		{
			// we're done if this is a different File ID
			if ( myKeyPtr->hfsPlus.fileID != theFCBPtr->fcbFileID ||
				 myKeyPtr->hfsPlus.forkType != kDataForkType )
					break;

			for ( i = 0; i < kHFSPlusExtentDensity; ++i ) 
			{
				if ( myExtentRecord.hfsPlus[ i ].blockCount == 0 )
					break;

				(void) BlockDeallocate( myVCBPtr, 
										myExtentRecord.hfsPlus[ i ].startBlock,
										myExtentRecord.hfsPlus[ i ].blockCount );
			}
		}
		else
		{
			// we're done if this is a different File ID
			if ( myKeyPtr->hfs.fileID != theFCBPtr->fcbFileID ||
				 myKeyPtr->hfs.forkType != kDataForkType )
					break;

			for ( i = 0; i < kHFSExtentDensity; ++i ) 
			{
				if ( myExtentRecord.hfs[ i ].blockCount == 0 )
					break;

				(void) BlockDeallocate( myVCBPtr, 
										myExtentRecord.hfs[ i ].startBlock,
										myExtentRecord.hfs[ i ].blockCount );
			}
		}

		/* get rid of this extent BTree record */
		myErr = DeleteBTreeRecord( theSGlobPtr->calculatedExtentsFCB, myKeyPtr );
	}
	
	return( myErr );

} /* ReleaseExtentsInExtentsBTree */


/*
 * ValidateRecordLength
 *	
 * This routine will make sure the given HFS (plus and standard) catalog record
 * is of the correct length.
 *
 */

static OSErr ValidateRecordLength( 	SGlobPtr theSGlobPtr,
									CatalogRecord * theRecPtr,
									UInt32 theRecSize )
{
	SVCB *					myVCBPtr;
	Boolean 				isHFSPlus = false;

	myVCBPtr = theSGlobPtr->calculatedVCB;
	isHFSPlus = ( myVCBPtr->vcbSignature == kHFSPlusSigWord );
	
	if ( isHFSPlus )
	{
		switch ( theRecPtr->recordType ) 
		{
		case kHFSPlusFolderRecord:
			if ( theRecSize != sizeof( HFSPlusCatalogFolder ) )
			{
				return( -1 );
			}
			break;
	
		case kHFSPlusFileRecord:
			if ( theRecSize != sizeof(HFSPlusCatalogFile) )
			{
				return( -1 );
			}
			break;
	
		case kHFSPlusFolderThreadRecord:
			/* Fall through */
	
		case kHFSPlusFileThreadRecord:
			if ( theRecSize > sizeof(HFSPlusCatalogThread) ||
				 theRecSize < sizeof(HFSPlusCatalogThread) - sizeof(HFSUniStr255) + sizeof(UniChar) ) 
			{
				return( -1 );
			}
			break;
	
		default:
			return( -1 );
		}
	}
	else
	{
		switch ( theRecPtr->recordType ) 
		{
		case kHFSFolderRecord:
			if ( theRecSize != sizeof(HFSCatalogFolder) )
				return( -1 );
			break;
	
		case kHFSFileRecord:
			if ( theRecSize != sizeof(HFSCatalogFile) )
				return( -1 );
			break;
	
		case kHFSFolderThreadRecord:
			/* Fall through */
		case kHFSFileThreadRecord:
			if ( theRecSize != sizeof(HFSCatalogThread)) 
				return( -1 );
			break;
	
		default:
			return( -1 );
		}
	}
	
	return( noErr );
	
} /* ValidateRecordLength */


#if DEBUG_REBUILD 
static void PrintNodeDescriptor( NodeDescPtr thePtr )
{
	printf( "\n xxxxxxxx BTNodeDescriptor xxxxxxxx \n" );
	printf( "   fLink %d \n", thePtr->fLink );
	printf( "   bLink %d \n", thePtr->bLink );
	printf( "   kind %d ", thePtr->kind );
	if ( thePtr->kind == kBTLeafNode )
		printf( "%s \n", "kBTLeafNode" );
	else if ( thePtr->kind == kBTIndexNode )
		printf( "%s \n", "kBTIndexNode" );
	else if ( thePtr->kind == kBTHeaderNode )
		printf( "%s \n", "kBTHeaderNode" );
	else if ( thePtr->kind == kBTMapNode )
		printf( "%s \n", "kBTMapNode" );
	else
		printf( "do not know?? \n" );
	printf( "   height %d \n", thePtr->height );
	printf( "   numRecords %d \n", thePtr->numRecords );

} /* PrintNodeDescriptor */


static void PrintBTHeaderRec( BTHeaderRec * thePtr )
{
	printf( "\n xxxxxxxx BTHeaderRec xxxxxxxx \n" );
	printf( "   treeDepth %d \n", thePtr->treeDepth );
	printf( "   rootNode %d \n", thePtr->rootNode );
	printf( "   leafRecords %d \n", thePtr->leafRecords );
	printf( "   firstLeafNode %d \n", thePtr->firstLeafNode );
	printf( "   lastLeafNode %d \n", thePtr->lastLeafNode );
	printf( "   nodeSize %d \n", thePtr->nodeSize );
	printf( "   maxKeyLength %d \n", thePtr->maxKeyLength );
	printf( "   totalNodes %d \n", thePtr->totalNodes );
	printf( "   freeNodes %d \n", thePtr->freeNodes );
	printf( "   clumpSize %d \n", thePtr->clumpSize );
	printf( "   btreeType 0x%02X \n", thePtr->btreeType );
	printf( "   attributes 0x%02X \n", thePtr->attributes );

} /* PrintBTHeaderRec */

			
static void PrintBTreeKey( KeyPtr thePtr, BTreeControlBlock * theBTreeCBPtr )
{
	int		myKeyLength, i;
	UInt8 *	myPtr = (UInt8 *)thePtr;

	myKeyLength = CalcKeySize( theBTreeCBPtr, thePtr) ;
	printf( "\n xxxxxxxx BTreeKey xxxxxxxx \n" );
	printf( "   length %d \n", myKeyLength );
	for ( i = 0; i < myKeyLength; i++ )
		printf( "%02X", *(myPtr + i) );
	printf( "\n" );
	
} /* PrintBTreeKey */
			
static void PrintIndexNodeRec( UInt32 theNodeNum )
{
	printf( "\n xxxxxxxx IndexNodeRec xxxxxxxx \n" );
	printf( "   node number %d \n", theNodeNum );

} /* PrintIndexNodeRec */
			
static void PrintLeafNodeRec( HFSPlusCatalogFolder * thePtr )
{
	printf( "\n xxxxxxxx LeafNodeRec xxxxxxxx \n" );
	printf( "   recordType %d ", thePtr->recordType );
	if ( thePtr->recordType == kHFSPlusFolderRecord )
		printf( "%s \n", "kHFSPlusFolderRecord" );
	else if ( thePtr->recordType == kHFSPlusFileRecord )
		printf( "%s \n", "kHFSPlusFileRecord" );
	else if ( thePtr->recordType == kHFSPlusFolderThreadRecord )
		printf( "%s \n", "kHFSPlusFolderThreadRecord" );
	else if ( thePtr->recordType == kHFSPlusFileThreadRecord )
		printf( "%s \n", "kHFSPlusFileThreadRecord" );
	else
		printf( "do not know?? \n" );

} /* PrintLeafNodeRec */


#endif // DEBUG_REBUILD
