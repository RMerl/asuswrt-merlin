/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
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
/*
	File:		SRepair.c

	Contains:	This file contains the Scavenger repair routines.
	
	Written by:	Bill Bruffey

	Copyright:	© 1986, 1990, 1992-1999 by Apple Computer, Inc., all rights reserved.

*/

#include "Scavenger.h"
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "../cache.h"

enum {
	clearBlocks,
	addBitmapBit,
	deleteExtents
};

/* internal routine prototypes */

void	SetOffset (void *buffer, UInt16 btNodeSize, SInt16 recOffset, SInt16 vecOffset);
#define SetOffset(buffer,nodesize,offset,record)		(*(SInt16 *) ((Byte *) (buffer) + (nodesize) + (-2 * (record))) = (offset))
static	OSErr	UpdateBTreeHeader( SFCB * fcbPtr );
static	OSErr	FixBTreeHeaderReservedFields( SGlobPtr GPtr, short refNum );
static	OSErr	UpdBTM( SGlobPtr GPtr, short refNum);
static	OSErr	UpdateVolumeBitMap( SGlobPtr GPtr, Boolean preAllocateOverlappedExtents );
static	OSErr	DoMinorOrders( SGlobPtr GPtr );
static	OSErr	UpdVal( SGlobPtr GPtr, RepairOrderPtr rP );
static	int		DelFThd( SGlobPtr GPtr, UInt32 fid );
static	OSErr	FixDirThread( SGlobPtr GPtr, UInt32 did );
static	OSErr	FixOrphanedFiles ( SGlobPtr GPtr );
static	OSErr	RepairReservedBTreeFields ( SGlobPtr GPtr );
static 	OSErr   GetCatalogRecord(SGlobPtr GPtr, UInt32 fileID, Boolean isHFSPlus, CatalogKey *catKey, CatalogRecord *catRecord, UInt16 *recordSize); 
static 	OSErr   RepairAttributesCheckABT(SGlobPtr GPtr, Boolean isHFSPlus);
static 	OSErr   RepairAttributesCheckCBT(SGlobPtr GPtr, Boolean isHFSPlus);
static	OSErr	RepairAttributes( SGlobPtr GPtr );
static	OSErr	FixFinderFlags( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixLinkCount( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixBSDInfo( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	DeleteUnlinkedFile( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixOrphanedExtent( SGlobPtr GPtr );
static 	OSErr	FixFileSize(SGlobPtr GPtr, RepairOrderPtr p);
static  OSErr 	VolumeObjectFixVHBorMDB( Boolean * fixedIt );
static 	OSErr 	VolumeObjectRestoreWrapper( void );
static	OSErr	FixBloatedThreadRecords( SGlob *GPtr );
static	OSErr	FixMissingThreadRecords( SGlob *GPtr );
static	OSErr	FixEmbededVolDescription( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixWrapperExtents( SGlobPtr GPtr, RepairOrderPtr p );
static  OSErr	FixIllegalNames( SGlobPtr GPtr, RepairOrderPtr roPtr );
static HFSCatalogNodeID GetObjectID( CatalogRecord * theRecPtr );
static 	OSErr	FixMissingDirectory( SGlob *GPtr, UInt32 theObjID, UInt32 theParID );

/* Functions to fix overlapping extents */
static	OSErr	FixOverlappingExtents(SGlobPtr GPtr);
static 	int 	CompareExtentBlockCount(const void *first, const void *second);
static 	OSErr 	MoveExtent(SGlobPtr GPtr, ExtentInfo *extentInfo);
static 	OSErr 	CreateCorruptFileSymlink(SGlobPtr GPtr, UInt32 parentID, ExtentInfo *extentInfo);
static 	OSErr 	SearchExtentInVH(SGlobPtr GPtr, ExtentInfo *extentInfo, UInt32 *foundExtentIndex, Boolean *noMoreExtents);
static 	OSErr 	UpdateExtentInVH (SGlobPtr GPtr, ExtentInfo *extentInfo, UInt32 foundExtentIndex);
static 	OSErr 	SearchExtentInCatalogBT(SGlobPtr GPtr, ExtentInfo *extentInfo, CatalogKey *catKey, CatalogRecord *catRecord, UInt16 *recordSize, UInt32 *foundExtentIndex, Boolean *noMoreExtents);
static 	OSErr 	UpdateExtentInCatalogBT (SGlobPtr GPtr, ExtentInfo *extentInfo, CatalogKey *catKey, CatalogRecord *catRecord, UInt16 *recordSize, UInt32 foundExtentIndex);
static 	OSErr 	SearchExtentInExtentBT(SGlobPtr GPtr, ExtentInfo *extentInfo, HFSPlusExtentKey *extentKey, HFSPlusExtentRecord *extentRecord, UInt16 *recordSize, UInt32 *foundExtentIndex);
static 	OSErr 	FindExtentInExtentRec (Boolean isHFSPlus, UInt32 startBlock, UInt32 blockCount, const HFSPlusExtentRecord extentData, UInt32 *foundExtentIndex, Boolean *noMoreExtents);

/* Functions to copy disk blocks or data buffer to disk */
static 	OSErr 	CopyDiskBlocks(SGlobPtr GPtr, const UInt32 startAllocationBlock, const UInt32 blockCount, const UInt32 newStartAllocationBlock );
static 	OSErr 	WriteDiskBlocks(SGlobPtr GPtr, UInt32 startBlock, UInt32 blockCount, u_char *buffer, int buflen);

/* Functions to create file and directory by name */
static 	OSErr 	CreateFileByName(SGlobPtr GPtr, UInt32 parentID, UInt16 fileType, u_char *fileName, unsigned int filenameLen, u_char *data, unsigned int dataLen);
static 	UInt32 	CreateDirByName(SGlob *GPtr , const u_char *dirName, const UInt32 parentID);

static 	int		BuildFolderRec( u_int16_t theMode, UInt32 theObjID, Boolean isHFSPlus, CatalogRecord * theRecPtr );
static 	int		BuildThreadRec( CatalogKey * theKeyPtr, CatalogRecord * theRecPtr, Boolean isHFSPlus, Boolean isDirectory );
static 	int 	BuildFileRec(UInt16 fileType, UInt16 fileMode, UInt32 fileID, Boolean isHFSPlus, CatalogRecord *catRecord);


OSErr	RepairVolume( SGlobPtr GPtr )
{
	OSErr			err;
	
	SetDFAStage( kAboutToRepairStage );											//	Notify callers repair is starting...
 	err = CheckForStop( GPtr ); ReturnIfError( err );							//	Permit the user to interrupt
	
	//
	//	Do the repair
	//
	SetDFAStage( kRepairStage );									//	Stops GNE from being called, and changes behavior of MountCheck

	err = MRepair( GPtr );
	
	return( err );
}


/*------------------------------------------------------------------------------
Routine:	MRepair		- (Minor Repair)
Function:	Performs minor repair operations.
Input:		GPtr		-	pointer to scavenger global area
Output:		MRepair		-	function result:			
------------------------------------------------------------------------------*/

int MRepair( SGlobPtr GPtr )
{
	OSErr			err;
	SVCB			*calculatedVCB	= GPtr->calculatedVCB;
	Boolean			isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );

	if ( GPtr->CBTStat & S_RebuildBTree )
	{
		/* we currently only support rebuilding the catalog B-Tree file.  */
		/* once we do the rebuild we will force another verify since the */
		/* first verify was aborted when we determined a rebuild was necessary */
		err = RebuildCatalogBTree( GPtr );
		return( err );
	}
 
	//  Handle repair orders.  Note that these must be done *BEFORE* the MDB is updated.
	err = DoMinorOrders( GPtr );
	ReturnIfError( err );
  	err = CheckForStop( GPtr ); ReturnIfError( err );

	/* Clear Catalog status for things repaired by DoMinorOrders */
	GPtr->CatStat &= ~(S_FileAllocation | S_Permissions | S_UnlinkedFile | S_LinkCount | S_IllName);

	/*
	 * Fix missing thread records
	 */
	if (GPtr->CatStat & S_MissingThread) {
		err = FixMissingThreadRecords(GPtr);
		ReturnIfError(err);
		
		GPtr->CatStat &= ~S_MissingThread;
		GPtr->CBTStat |= S_BTH;  /* leaf record count changed */
	}

	//	2210409, in System 8.1, moving file or folder would cause HFS+ thread records to be
	//	520 bytes in size.  We only shrink the threads if other repairs are needed.
	if ( GPtr->VeryMinorErrorsStat & S_BloatedThreadRecordFound )
	{
		(void) FixBloatedThreadRecords( GPtr );
		GPtr->VeryMinorErrorsStat &= ~S_BloatedThreadRecordFound;
	}

	//
	//	we will update the following data structures regardless of whether we have done
	//	major or minor repairs, so we might end up doing this multiple times. Look into this.
	//
	
	//
	//	Isolate and fix Overlapping Extents
	//
 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	if ( (GPtr->VIStat & S_OverlappingExtents) != 0 )
	{
		err = FixOverlappingExtents( GPtr );						//	Isolate and fix Overlapping Extents
		ReturnIfError( err );
		
		GPtr->VIStat &= ~S_OverlappingExtents;
		GPtr->VIStat |= S_VBM;										//	Now that we changed the extents, we need to rebuild the bitmap
		InvalidateCalculatedVolumeBitMap( GPtr );					//	Invalidate our BitMap
	}
	
	//
	//	FixOrphanedFiles
	//
 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
				
	if ( (GPtr->CBTStat & S_Orphan) != 0 )
	{
		err = FixOrphanedFiles ( GPtr );							//	Orphaned file were found
		ReturnIfError( err );
		GPtr->CBTStat |= S_BTH;  									// leaf record count may change - 2913311
	}

	//
	//	FixOrphanedExtent records
	//
	if ( (GPtr->EBTStat & S_OrphanedExtent) != 0 )					//	Orphaned extents were found
	{
		err = FixOrphanedExtent( GPtr );
		GPtr->EBTStat &= ~S_OrphanedExtent;
	//	if ( err == errRebuildBtree )
	//		goto RebuildBtrees;
		ReturnIfError( err );
	}

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	//
	//	Update the extent BTree header and bit map
	//
 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	if ( (GPtr->EBTStat & S_BTH) || (GPtr->EBTStat & S_ReservedBTH) )
	{
		err = UpdateBTreeHeader( GPtr->calculatedExtentsFCB );	//	update extent file BTH

		if ( (err == noErr) && (GPtr->EBTStat & S_ReservedBTH) )
		{
			err = FixBTreeHeaderReservedFields( GPtr, kCalculatedExtentRefNum );
		}

		ReturnIfError( err );
	}


	if ( (GPtr->EBTStat & S_BTM) != 0 )
	{
		err = UpdBTM( GPtr, kCalculatedExtentRefNum );				//	update extent file BTM
		ReturnIfError( err );
	}
	
	//
	//	Update the catalog BTree header and bit map
	//

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	if ( (GPtr->CBTStat & S_BTH) || (GPtr->CBTStat & S_ReservedBTH) )
	{
		err = UpdateBTreeHeader( GPtr->calculatedCatalogFCB );	//	update catalog BTH

		if ( (err == noErr) && (GPtr->CBTStat & S_ReservedBTH) )
		{
			err = FixBTreeHeaderReservedFields( GPtr, kCalculatedCatalogRefNum );
		}

		ReturnIfError( err );
	}

	if ( GPtr->CBTStat & S_BTM )
	{
		err = UpdBTM( GPtr, kCalculatedCatalogRefNum );				//	update catalog BTM
		ReturnIfError( err );
	}

	if ( (GPtr->CBTStat & S_ReservedNotZero) != 0 )
	{
		err = RepairReservedBTreeFields( GPtr );					//	update catalog fields
		ReturnIfError( err );
	}

	// Check consistency of attribute btree and corresponding bits in
	// catalog btree 
	if ( (GPtr->ABTStat & S_AttributeCount) || 
	     (GPtr->ABTStat & S_SecurityCount)) 
	{
		err = RepairAttributes( GPtr );
		ReturnIfError( err );
	}
	
	// Update the attribute BTree header and bit map 
	if ( (GPtr->ABTStat & S_BTH) )
	{
		err = UpdateBTreeHeader( GPtr->calculatedAttributesFCB );	//	update attribute BTH
		ReturnIfError( err );
	}

	if ( GPtr->ABTStat & S_BTM )
	{
		err = UpdBTM( GPtr, kCalculatedAttributesRefNum );		//	update attribute BTM
		ReturnIfError( err );
	}

	//
	//	Update the volume bit map
	//
	// Note, moved volume bit map update to end after other repairs
	// (except the MDB / VolumeHeader) have been completed
	//
 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	if ( (GPtr->VIStat & S_VBM) != 0 )
	{
		err = UpdateVolumeBitMap( GPtr, false );					//	update VolumeBitMap
		ReturnIfError( err );
		InvalidateCalculatedVolumeBitMap( GPtr );					//	Invalidate our BitMap
	}

	//
	//	Fix missing Primary or Alternate VHB or MDB
	//

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
				
	if ( (GPtr->VIStat & S_MDB) != 0 )		//	fix MDB / VolumeHeader
	{
		Boolean		fixedIt = false;
		err = VolumeObjectFixVHBorMDB( &fixedIt );
		ReturnIfError( err );
		// don't call FlushAlternateVolumeControlBlock if we fixed it since that would 
		// mean our calculated VCB has not been completely set up.
		if ( fixedIt ) {
			GPtr->VIStat &= ~S_MDB; 
			MarkVCBClean( calculatedVCB );
		}
	}

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
				
	if ( (GPtr->VIStat & S_WMDB) != 0  )		//	fix wrapper MDB
	{
		err = VolumeObjectRestoreWrapper();
		ReturnIfError( err );
	}

	//
	//	Update the MDB / VolumeHeader
	//
	// Note, moved MDB / VolumeHeader update to end 
	// after all other repairs have been completed.
	//

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
				
	if ( (GPtr->VIStat & S_MDB) != 0 || IsVCBDirty(calculatedVCB) ) //	update MDB / VolumeHeader
	{
		MarkVCBDirty(calculatedVCB);								// make sure its dirty
		calculatedVCB->vcbAttributes |= kHFSVolumeUnmountedMask;
		err = FlushAlternateVolumeControlBlock( calculatedVCB, isHFSPlus );	//	Writes real & alt blocks
		ReturnIfError( err );
	}

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
 
 	// if we had minor repairs that failed we still want to fix as much as possible
 	// so we wait until now to indicate the volume still has problems
 	if ( GPtr->minorRepairErrors )
 		err = R_RFail;
	
	return( err );													//	all done
}



//
//	Internal Routines
//

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Routine:	VolumeObjectFixVHBorMDB
//
//	Function:	When the primary or alternate Volume Header Block or Master 
//				Directory Block is damaged or missing use the undamaged one to 
//				restore the other.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static OSErr VolumeObjectFixVHBorMDB( Boolean* fixedItPtr )
{
	OSErr				err;
	OSErr				err2;
	VolumeObjectPtr		myVOPtr;
	BlockDescriptor  	myPrimary;
	BlockDescriptor  	myAlternate;

	myVOPtr = GetVolumeObjectPtr( );
	myPrimary.buffer = NULL;
	myAlternate.buffer = NULL;
	err = noErr;
	
	// bail if both are OK
	if ( VolumeObjectIsHFS() ) {
		if ( (myVOPtr->flags & kVO_PriMDBOK) != 0 &&
			 (myVOPtr->flags & kVO_AltMDBOK) != 0 )
			goto ExitThisRoutine;
	}
	else {
		if ( (myVOPtr->flags & kVO_PriVHBOK) != 0 &&
			 (myVOPtr->flags & kVO_AltVHBOK) != 0 )
			goto ExitThisRoutine;
	}
			
	// it's OK if one of the primary or alternate is invalid
	err = GetVolumeObjectPrimaryBlock( &myPrimary );
	if ( !(err == noErr || err == badMDBErr || err == noMacDskErr) )
		goto ExitThisRoutine;

	// invalidate if we have not marked the primary as OK
	if ( VolumeObjectIsHFS( ) ) {
		if ( (myVOPtr->flags & kVO_PriMDBOK) == 0 )
			err = badMDBErr;
	}
	else if ( (myVOPtr->flags & kVO_PriVHBOK) == 0 ) {
		err = badMDBErr;
	}

	err2 = GetVolumeObjectAlternateBlock( &myAlternate );
	if ( !(err2 == noErr || err2 == badMDBErr || err2 == noMacDskErr) )
		goto ExitThisRoutine;

	// invalidate if we have not marked the alternate as OK
	if ( VolumeObjectIsHFS( ) ) {
		if ( (myVOPtr->flags & kVO_AltMDBOK) == 0 )
			err2 = badMDBErr;
	}
	else if ( (myVOPtr->flags & kVO_AltVHBOK) == 0 ) {
		err2 = badMDBErr;
	}
		
	// primary is OK so use it to restore alternate
	if ( err == noErr ) {
		CopyMemory( myPrimary.buffer, myAlternate.buffer, Blk_Size );
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myAlternate, kForceWriteBlock );		
		myAlternate.buffer = NULL;
		*fixedItPtr = true;
		if ( VolumeObjectIsHFS( ) )
			myVOPtr->flags |= kVO_AltMDBOK;
		else
			myVOPtr->flags |= kVO_AltVHBOK;
	}
	// alternate is OK so use it to restore the primary
	else if ( err2 == noErr ) {
		CopyMemory( myAlternate.buffer, myPrimary.buffer, Blk_Size );
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myPrimary, kForceWriteBlock );		
		myPrimary.buffer = NULL;
		*fixedItPtr = true;
		if ( VolumeObjectIsHFS( ) )
			myVOPtr->flags |= kVO_PriMDBOK;
		else
			myVOPtr->flags |= kVO_PriVHBOK;
		err = noErr;
	}
	else
		err = noMacDskErr;

ExitThisRoutine:
	if ( myPrimary.buffer != NULL )
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myPrimary, kReleaseBlock );
	if ( myAlternate.buffer != NULL )
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myAlternate, kReleaseBlock );

	return( err );
	
} /* VolumeObjectFixVHBorMDB */
		

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Routine:	VolumeObjectRestoreWrapper
//
//	Function:	When the primary or alternate Master Directory Block is damaged 
//				or missing use the undamaged one to restore the other.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static OSErr VolumeObjectRestoreWrapper( void )
{
	OSErr				err;
	OSErr				err2;
	VolumeObjectPtr		myVOPtr;
	BlockDescriptor  	myPrimary;
	BlockDescriptor  	myAlternate;

	myVOPtr = GetVolumeObjectPtr( );
	myPrimary.buffer = NULL;
	myAlternate.buffer = NULL;
				
	// it's OK if one of the MDB is invalid
	err = GetVolumeObjectPrimaryMDB( &myPrimary );
	if ( !(err == noErr || err == badMDBErr || err == noMacDskErr) )
		goto ExitThisRoutine;
	err2 = GetVolumeObjectAlternateMDB( &myAlternate );
	if ( !(err2 == noErr || err2 == badMDBErr || err2 == noMacDskErr) )
		goto ExitThisRoutine;

	// primary is OK so use it to restore alternate
	if ( err == noErr && (myVOPtr->flags & kVO_PriMDBOK) != 0 ) {
		CopyMemory( myPrimary.buffer, myAlternate.buffer, Blk_Size );
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myAlternate, kForceWriteBlock );		
		myAlternate.buffer = NULL;
		myVOPtr->flags |= kVO_AltMDBOK;
	}
	// alternate is OK so use it to restore the primary
	else if ( err2 == noErr && (myVOPtr->flags & kVO_AltMDBOK) != 0 ) {
		CopyMemory( myAlternate.buffer, myPrimary.buffer, Blk_Size );
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myPrimary, kForceWriteBlock );		
		myPrimary.buffer = NULL;
		myVOPtr->flags |= kVO_PriMDBOK;
		err = noErr;
	}
	else
		err = noMacDskErr;

ExitThisRoutine:
	if ( myPrimary.buffer != NULL )
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myPrimary, kReleaseBlock );
	if ( myAlternate.buffer != NULL )
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myAlternate, kReleaseBlock );

	return( err );
	
} /* VolumeObjectRestoreWrapper */
		

/*------------------------------------------------------------------------------
Routine:	UpdateBTreeHeader - (Update BTree Header)

Function:	Replaces a BTH on disk with info from a scavenger BTCB.
			
Input:		GPtr		-	pointer to scavenger global area
			refNum		-	file refnum

Output:		UpdateBTreeHeader	-	function result:			
								0	= no error
								n 	= error
------------------------------------------------------------------------------*/

static	OSErr	UpdateBTreeHeader( SFCB * fcbPtr )
{
	OSErr err;

	M_BTreeHeaderDirty( ((BTreeControlBlockPtr) fcbPtr->fcbBtree) );	
	err = BTFlushPath( fcbPtr );
	
	return( err );

} /* End UpdateBTreeHeader */


		
/*------------------------------------------------------------------------------
Routine:	FixBTreeHeaderReservedFields

Function:	Fix reserved fields in BTree Header
			
Input:		GPtr		-	pointer to scavenger global area
			refNum		-	file refnum

Output:		0	= no error
			n 	= error
------------------------------------------------------------------------------*/

static	OSErr	FixBTreeHeaderReservedFields( SGlobPtr GPtr, short refNum )
{
	OSErr        err;
	BTHeaderRec  header;

	err = GetBTreeHeader(GPtr, ResolveFCB(refNum), &header);
	ReturnIfError( err );
	
	if ( (header.clumpSize % GPtr->calculatedVCB->vcbBlockSize) != 0 )
		header.clumpSize = GPtr->calculatedVCB->vcbBlockSize;
		
	header.reserved1	= 0;
	header.btreeType	= kHFSBTreeType;  //	control file
/*
 * TBD - we'll need to repair an invlid keyCompareType field.
 */
#if 0
	if (-->TBD<--)
		header.keyCompareType = kHFSBinaryCompare;
#endif
	ClearMemory( header.reserved3, sizeof(header.reserved3) );

	return( err );
}


		

/*------------------------------------------------------------------------------

Routine:	UpdBTM - (Update BTree Map)

Function:	Replaces a BTM on disk with a scavenger BTM.
			
Input:		GPtr		-	pointer to scavenger global area
			refNum		-	file refnum

Output:		UpdBTM	-	function result:			
								0	= no error
								n 	= error
------------------------------------------------------------------------------*/

static	OSErr	UpdBTM( SGlobPtr GPtr, short refNum )
{
	OSErr				err;
	UInt16				recSize;
	SInt32				mapSize;
	SInt16				size;
	SInt16				recIndx;
	Ptr					p;
	Ptr					btmP;
	Ptr					sbtmP;
	UInt32				nodeNum;
	NodeRec				node;
	UInt32				fLink;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( refNum );

	//	Set up
	mapSize			= ((BTreeExtensionsRec*)calculatedBTCB->refCon)->BTCBMSize;

	//
	//	update the map records
	//
	if ( mapSize > 0 )
	{
		nodeNum	= 0;
		recIndx	= 2;
		sbtmP	= ((BTreeExtensionsRec*)calculatedBTCB->refCon)->BTCBMPtr;
		
		do
		{
			GPtr->TarBlock = nodeNum;								//	set target node number
				
			err = GetNode( calculatedBTCB, nodeNum, &node );
			ReturnIfError( err );									//	could't get map node
	
			//	Locate the map record
			recSize = GetRecordSize( calculatedBTCB, (BTNodeDescriptor *)node.buffer, recIndx );
			btmP = (Ptr)GetRecordAddress( calculatedBTCB, (BTNodeDescriptor *)node.buffer, recIndx );
			fLink	= ((NodeDescPtr)node.buffer)->fLink;
			size	= ( recSize  > mapSize ) ? mapSize : recSize;
				
			CopyMemory( sbtmP, btmP, size );						//	update it
			
			err = UpdateNode( calculatedBTCB, &node );				//	write it, and unlock buffer
			
			mapSize	-= size;										//	move to next map record
			if ( mapSize == 0 )										//	more to go?
				break;												//	no, zero remainder of record
			if ( fLink == 0 )										//	out of bitmap blocks in file?
			{
				RcdError( GPtr, E_ShortBTM );
				(void) ReleaseNode(calculatedBTCB, &node);
				return( E_ShortBTM );
			}
				
			nodeNum	= fLink;
			sbtmP	+= size;
			recIndx	= 0;
			
		} while ( mapSize > 0 );

		//	clear the unused portion of the map record
		for ( p = btmP + size ; p < btmP + recSize ; p++ )
			*p = 0; 

		err = UpdateNode( calculatedBTCB, &node );					//	Write it, and unlock buffer
	}
	
	return( noErr );												//	All done
}	//	end UpdBTM


		

/*------------------------------------------------------------------------------

Routine:	UpdateVolumeBitMap - (Update Volume Bit Map)

Function:	Replaces the VBM on disk with the scavenger VBM.
			
Input:		GPtr			-	pointer to scavenger global area

Output:		UpdateVolumeBitMap			- 	function result:
									0 = no error
									n = error
			GPtr->VIStat	-	S_VBM flag set if VBM is damaged.
------------------------------------------------------------------------------*/

static	OSErr	UpdateVolumeBitMap( SGlobPtr GPtr, Boolean preAllocateOverlappedExtents )
{
	GPtr->TarID = VBM_FNum;

	return ( CheckVolumeBitMap(GPtr, true) );
}


/*------------------------------------------------------------------------------

Routine:	DoMinorOrders

Function:	Execute minor repair orders.

Input:		GPtr	- ptr to scavenger global data

Outut:		function result:
				0 - no error
				n - error
------------------------------------------------------------------------------*/

static	OSErr	DoMinorOrders( SGlobPtr GPtr )				//	the globals
{
	RepairOrderPtr		p;
	OSErr				err	= noErr;						//	initialize to "no error"
	
	while( (p = GPtr->MinorRepairsP) && (err == noErr) )	//	loop over each repair order
	{
		GPtr->MinorRepairsP = p->link;						//	unlink next from list
		
		switch( p->type )									//	branch on repair type
		{
			case E_RtDirCnt:								//	the valence errors
			case E_RtFilCnt:								//	(of which there are several)
			case E_DirCnt:
			case E_FilCnt:
			case E_DirVal:
				err = UpdVal( GPtr, p );					//	handle a valence error
				break;
			
			case E_LockedDirName:
				err = FixFinderFlags( GPtr, p );
				break;

			case E_UnlinkedFile:
				err = DeleteUnlinkedFile( GPtr, p );
				break;

			case E_InvalidLinkCount:
				err = FixLinkCount( GPtr, p );
				break;
			
			case E_InvalidPermissions:
			case E_InvalidUID:
				err = FixBSDInfo( GPtr, p );
				break;
			
			case E_NoFile:									//	dangling file thread
				err = DelFThd( GPtr, p->parid );			//	delete the dangling thread
				break;

			//¥¥	E_NoFile case is never hit since VLockedChk() registers the error, 
			//¥¥	and returns the error causing the verification to quit.
			case E_EntryNotFound:
				GPtr->EBTStat |= S_OrphanedExtent;
				break;

			//¥¥	Same with E_NoDir
			case E_NoDir:									//	missing directory record
				err = FixDirThread( GPtr, p->parid );		//	fix the directory thread record
				break;
			
			case E_InvalidMDBdrAlBlSt:
				err = FixEmbededVolDescription( GPtr, p );
				break;
			
			case E_InvalidWrapperExtents:
				err = FixWrapperExtents(GPtr, p);
				break;

            case E_IllegalName:
                err = FixIllegalNames( GPtr, p );
				break;

			case E_PEOF:
			case E_LEOF:
				err = FixFileSize(GPtr, p);
				break;
			default:										//	unknown repair type
				err = IntError( GPtr, R_IntErr );			//	treat as an internal error
				break;
		}
		
		DisposeMemory( p );								//	free the node
	}
	
	return( err );											//	return error code to our caller
}



/*------------------------------------------------------------------------------

Routine:	DelFThd - (delete file thread)

Function:	Executes the delete dangling file thread repair orders.  These are typically
			threads left after system 6 deletes an aliased file, since system 6 is not
			aware of aliases and thus will not delete the thread along with the file.

Input:		GPtr	- global data
			fid		- the thread record's key's parent-ID

Output:		0 - no error
			n - deletion failed
Modification History:
	29Oct90		KST		CBTDelete was using "k" as key which points to cache buffer.
-------------------------------------------------------------------------------*/

static	int	DelFThd( SGlobPtr GPtr, UInt32 fid )				//	the file ID
{
	CatalogRecord		record;
	CatalogKey			foundKey;
	CatalogKey			key;
	UInt32				hint;								//	as returned by CBTSearch
	OSErr				result;								//	status return
	UInt16				recSize;
	Boolean				isHFSPlus;
	ExtentRecord		zeroExtents;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	
	BuildCatalogKey( fid, (const CatalogName*) nil, isHFSPlus, &key );
	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recSize, &hint );
	
	if ( result )	return ( IntError( GPtr, result ) );
	
	if ( (record.recordType != kHFSFileThreadRecord) && (record.recordType != kHFSPlusFileThreadRecord) )	//	quit if not a file thread
		return ( IntError( GPtr, R_IntErr ) );
	
	//	Zero the record on disk
	ClearMemory( (Ptr)&zeroExtents, sizeof(ExtentRecord) );
	result	= ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &key, hint, &zeroExtents, recSize, &hint );
	if ( result )	return ( IntError( GPtr, result ) );
	
	result	= DeleteBTreeRecord( GPtr->calculatedCatalogFCB, &key );
	if ( result )	return ( IntError( GPtr, result ) );
	
	//	After deleting a record, we'll need to write back the BT header and map,
	//	to reflect the updated record count etc.
	   
	GPtr->CBTStat |= S_BTH + S_BTM;							//	set flags to write back hdr and map

	return( noErr );										//	successful return
}
	

/*------------------------------------------------------------------------------

Routine:	FixDirThread - (fix directory thread record's parent ID info)

Function:	Executes the missing directory record repair orders most likely caused by 
			disappearing folder bug.  This bug causes some folders to jump to Desktop 
			from the root window.  The catalog directory record for such a folder has 
			the Desktop folder as the parent but its thread record still the root 
			directory as its parent.

Input:		GPtr	- global data
			did		- the thread record's key's parent-ID

Output:		0 - no error
			n - deletion failed
-------------------------------------------------------------------------------*/

static	OSErr	FixDirThread( SGlobPtr GPtr, UInt32 did )	//	the dir ID
{
	UInt8				*dataPtr;
	UInt32				hint;							//	as returned by CBTSearch
	OSErr				result;							//	status return
	UInt16				recSize;
	CatalogName			catalogName;					//	temporary name record
	CatalogName			*keyName;						//	temporary name record
	register short 		index;							//	loop index for all records in the node
	UInt32  			curLeafNode;					//	current leaf node being checked
	CatalogRecord		record;
	CatalogKey			foundKey;
	CatalogKey			key;
	CatalogKey		 	*keyP;
	SInt16				recordType;
	UInt32				folderID;
	NodeRec				node;
	NodeDescPtr			nodeDescP;
	UInt32				newParDirID		= 0;			//	the parent ID where the dir record is really located
	Boolean				isHFSPlus;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( kCalculatedCatalogRefNum );

	isHFSPlus = VolumeObjectIsHFSPlus( );

	BuildCatalogKey( did, (const CatalogName*) nil, isHFSPlus, &key );
	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recSize, &hint );
	
	if ( result )
		return( IntError( GPtr, result ) );
	if ( (record.recordType != kHFSFolderThreadRecord) && (record.recordType != kHFSPlusFolderThreadRecord) )			//	quit if not a directory thread
		return ( IntError( GPtr, R_IntErr ) );
		
	curLeafNode = calculatedBTCB->freeNodes;
	
	while ( curLeafNode )
	{
		result = GetNode( calculatedBTCB, curLeafNode, &node );
		if ( result != noErr ) return( IntError( GPtr, result ) );
		
		nodeDescP = node.buffer;

		// loop on number of records in node
		for ( index = 0 ; index < nodeDescP->numRecords ; index++ )
		{
			GetRecordByIndex( calculatedBTCB, (NodeDescPtr)nodeDescP, index, (BTreeKey **)&keyP, &dataPtr, &recSize );
			
			recordType	= ((CatalogRecord *)dataPtr)->recordType;
			folderID	= recordType == kHFSPlusFolderRecord ? ((HFSPlusCatalogFolder *)dataPtr)->folderID : ((HFSCatalogFolder *)dataPtr)->folderID;
			
			// did we locate a directory record whode dirID match the the thread's key's parent dir ID?
			if ( (folderID == did) && ( recordType == kHFSPlusFolderRecord || recordType == kHFSFolderRecord ) )
			{
				newParDirID	= recordType == kHFSPlusFolderRecord ? keyP->hfsPlus.parentID : keyP->hfs.parentID;
				keyName		= recordType == kHFSPlusFolderRecord ? (CatalogName *)&keyP->hfsPlus.nodeName : (CatalogName *)&keyP->hfs.nodeName;
				CopyCatalogName( keyName, &catalogName, isHFSPlus );
				break;
			}
		}

		if ( newParDirID ) {
			(void) ReleaseNode(calculatedBTCB, &node);
			break;
		}

		curLeafNode = nodeDescP->fLink;	 // sibling of this leaf node
		
		(void) ReleaseNode(calculatedBTCB, &node);
	}
		
	if ( newParDirID == 0 )
	{
		return ( IntError( GPtr, R_IntErr ) ); // ¥¥  Try fixing by creating a new directory record?
	}
	else
	{
		(void) SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recSize, &hint );

		if ( isHFSPlus )
		{
			HFSPlusCatalogThread	*largeCatalogThreadP	= (HFSPlusCatalogThread *) &record;
			
			largeCatalogThreadP->parentID = newParDirID;
			CopyCatalogName( &catalogName, (CatalogName *) &largeCatalogThreadP->nodeName, isHFSPlus );
		}
		else
		{
			HFSCatalogThread	*smallCatalogThreadP	= (HFSCatalogThread *) &record;
			
			smallCatalogThreadP->parentID = newParDirID;
			CopyCatalogName( &catalogName, (CatalogName *)&smallCatalogThreadP->nodeName, isHFSPlus );
		}
		
		result = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recSize, &hint );
	}

	return( noErr );										//	successful return
}
	

/*------------------------------------------------------------------------------

Routine:	UpdVal - (Update Valence)

Function:	Replaces out of date valences with correct vals computed during scavenge.
			
Input:		GPtr			-	pointer to scavenger global area
			p				- 	pointer to the repair order

Output:		UpdVal			- 	function result:
									0 = no error
									n = error
------------------------------------------------------------------------------*/

static	OSErr	UpdVal( SGlobPtr GPtr, RepairOrderPtr p )					//	the valence repair order
{
	OSErr				result;						//	status return
	Boolean				isHFSPlus;
	UInt32				hint;						//	as returned by CBTSearch
	UInt16				recSize;
	CatalogRecord		record;
	CatalogKey			foundKey;
	CatalogKey			key;
	SVCB				*calculatedVCB = GPtr->calculatedVCB;

	isHFSPlus = VolumeObjectIsHFSPlus( );

	switch( p->type )
	{
		case E_RtDirCnt: //	invalid count of Dirs in Root
			if ( (UInt16)p->incorrect != calculatedVCB->vcbNmRtDirs )
				return ( IntError( GPtr, R_IntErr ) );
			calculatedVCB->vcbNmRtDirs = (UInt16)p->correct;
			GPtr->VIStat |= S_MDB;
			break;
			
		case E_RtFilCnt:
			if ( (UInt16)p->incorrect != calculatedVCB->vcbNmFls )
				return ( IntError( GPtr, R_IntErr ) );
			calculatedVCB->vcbNmFls = (UInt16)p->correct;
			GPtr->VIStat |= S_MDB;
			break;
			
		case E_DirCnt:
			if ( (UInt32)p->incorrect != calculatedVCB->vcbFolderCount )
				return ( IntError( GPtr, R_IntErr ) );
			calculatedVCB->vcbFolderCount = (UInt32)p->correct;
			GPtr->VIStat |= S_MDB;
			break;
			
		case E_FilCnt:
			if ( (UInt32)p->incorrect != calculatedVCB->vcbFileCount )
				return ( IntError( GPtr, R_IntErr ) );
			calculatedVCB->vcbFileCount = (UInt32)p->correct;
			GPtr->VIStat |= S_MDB;
			break;
	
		case E_DirVal:
			BuildCatalogKey( p->parid, (CatalogName *)&p->name, isHFSPlus, &key );
			result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint,
					&foundKey, &record, &recSize, &hint );
			if ( result )
				return ( IntError( GPtr, result ) );
				
			if ( record.recordType == kHFSPlusFolderRecord )
			{
				if ( (UInt32)p->incorrect != record.hfsPlusFolder.valence)
					return ( IntError( GPtr, R_IntErr ) );
				record.hfsPlusFolder.valence = (UInt32)p->correct;
			}
			else
			{
				if ( (UInt16)p->incorrect != record.hfsFolder.valence )
					return ( IntError( GPtr, R_IntErr ) );
				record.hfsFolder.valence = (UInt16)p->correct;
			}
				
			result = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &key, hint,\
						&record, recSize, &hint );
			if ( result )
				return ( IntError( GPtr, result ) );
			break;
	}
		
	return( noErr );														//	no error
}

/*------------------------------------------------------------------------------

Routine:	FixFinderFlags

Function:	Changes some of the Finder flag bits for directories.
			
Input:		GPtr			-	pointer to scavenger global area
			p				- 	pointer to the repair order

Output:		FixFinderFlags	- 	function result:
									0 = no error
									n = error
------------------------------------------------------------------------------*/

static	OSErr	FixFinderFlags( SGlobPtr GPtr, RepairOrderPtr p )				//	the repair order
{
	CatalogRecord		record;
	CatalogKey			foundKey;
	CatalogKey			key;
	UInt32				hint;												//	as returned by CBTSearch
	OSErr				result;												//	status return
	UInt16				recSize;
	Boolean				isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );

	BuildCatalogKey( p->parid, (CatalogName *)&p->name, isHFSPlus, &key );

	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recSize, &hint );
	if ( result )
		return ( IntError( GPtr, result ) );

	if ( record.recordType == kHFSPlusFolderRecord )
	{
		HFSPlusCatalogFolder	*largeCatalogFolderP	= (HFSPlusCatalogFolder *) &record;	
		if ( (UInt16) p->incorrect != SWAP_BE16(largeCatalogFolderP->userInfo.frFlags) )
		{
			//	Another repar order may have affected the flags
			if ( p->correct < p->incorrect )
				largeCatalogFolderP->userInfo.frFlags &= SWAP_BE16(~((UInt16)p->maskBit));
			else
				largeCatalogFolderP->userInfo.frFlags |= SWAP_BE16((UInt16)p->maskBit);
		}
		else
		{
			largeCatalogFolderP->userInfo.frFlags = SWAP_BE16((UInt16)p->correct);
		}
	//	largeCatalogFolderP->contentModDate = timeStamp;
	}
	else
	{
		HFSCatalogFolder	*smallCatalogFolderP	= (HFSCatalogFolder *) &record;	
		if ( p->incorrect != SWAP_BE16(smallCatalogFolderP->userInfo.frFlags) )		//	do we know what we're doing?
		{
			//	Another repar order may have affected the flags
			if ( p->correct < p->incorrect )
				smallCatalogFolderP->userInfo.frFlags &= SWAP_BE16(~((UInt16)p->maskBit));
			else
				smallCatalogFolderP->userInfo.frFlags |= SWAP_BE16((UInt16)p->maskBit);
		}
		else
		{
			smallCatalogFolderP->userInfo.frFlags = SWAP_BE16((UInt16)p->correct);
		}
		
	//	smallCatalogFolderP->modifyDate = timeStamp;						// also update the modify date! -DJB
	}

	result = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recSize, &hint );	//	write the node back to the file
	if ( result )
		return( IntError( GPtr, result ) );
		
	return( noErr );														//	no error
}


/*------------------------------------------------------------------------------
FixLinkCount:  Adjust a data node link count (BSD hard link)
               (HFS Plus volumes only)
------------------------------------------------------------------------------*/
static OSErr
FixLinkCount(SGlobPtr GPtr, RepairOrderPtr p)
{
	SFCB *fcb;
	CatalogRecord rec;
	HFSPlusCatalogKey * keyp;
	FSBufferDescriptor btRecord;
	BTreeIterator btIterator;
	size_t len;
	OSErr result;
	UInt16 recSize;
	Boolean				isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	if (!isHFSPlus)
		return (0);
	fcb = GPtr->calculatedCatalogFCB;

	ClearMemory(&btIterator, sizeof(btIterator));
	btIterator.hint.nodeNum = p->hint;
	keyp = (HFSPlusCatalogKey*)&btIterator.key;
	keyp->parentID = p->parid;
	/* name was stored in UTF-8 */
	(void) utf_decodestr(&p->name[1], p->name[0], keyp->nodeName.unicode, &len);
	keyp->nodeName.length = len / 2;
	keyp->keyLength = kHFSPlusCatalogKeyMinimumLength + len;

	btRecord.bufferAddress = &rec;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(rec);
	
	result = BTSearchRecord(fcb, &btIterator, kInvalidMRUCacheKey,
			&btRecord, &recSize, &btIterator);
	if (result)
		return (IntError(GPtr, result));

	if (rec.recordType != kHFSPlusFileRecord)
		return (noErr);

	if ((UInt32)p->correct != rec.hfsPlusFile.bsdInfo.special.linkCount) {
		if (GPtr->logLevel >= kDebugLog)
		    printf("\t%s: fixing link count from %d to %d\n",
		        &p->name[1], rec.hfsPlusFile.bsdInfo.special.linkCount, (int)p->correct);

		rec.hfsPlusFile.bsdInfo.special.linkCount = (UInt32)p->correct;
		result = BTReplaceRecord(fcb, &btIterator, &btRecord, recSize);
		if (result)
			return (IntError(GPtr, result));
	}
		
	return (noErr);
}


/*------------------------------------------------------------------------------
FixIllegalNames:  Fix catalog enties that have illegal names.
    RepairOrder.name[] holds the old (illegal) name followed by the new name.
    The new name has been checked to make sure it is unique within the target
    directory.  The names will look like this:
        2 byte length of old name (in unicode characters not bytes)
        unicode characters for old name
        2 byte length of new name (in unicode characters not bytes)
        unicode characters for new name
------------------------------------------------------------------------------*/
static OSErr
FixIllegalNames( SGlobPtr GPtr, RepairOrderPtr roPtr )
{
	OSErr 				result;
	Boolean				isHFSPlus;
	Boolean				isDirectory;
	UInt16				recSize;
	SFCB *				fcbPtr;
    CatalogName *		oldNamePtr;
    CatalogName *		newNamePtr;
	UInt32				hint;				
	CatalogRecord		record;
	CatalogKey			key;
	CatalogKey			newKey;

	isHFSPlus = VolumeObjectIsHFSPlus( );
 	fcbPtr = GPtr->calculatedCatalogFCB;
    
	oldNamePtr = (CatalogName *) &roPtr->name;
    if ( isHFSPlus )
    {
        int					myLength;
        u_int16_t *			myPtr = (u_int16_t *) oldNamePtr;
        myLength = *myPtr; // get length of old name
        myPtr += (1 + myLength);  // bump past length of old name and old name
        newNamePtr = (CatalogName *) myPtr;
    }
    else
    {
        int					myLength;
        u_char *			myPtr = (u_char *) oldNamePtr;
        myLength = *myPtr; // get length of old name
        myPtr += (1 + myLength);  // bump past length of old name and old name
        newNamePtr = (CatalogName *) myPtr;
    }

	// make sure new name isn't already there
	BuildCatalogKey( roPtr->parid, newNamePtr, isHFSPlus, &newKey );
	result = SearchBTreeRecord( fcbPtr, &newKey, kNoHint, NULL, &record, &recSize, NULL );
	if ( result == noErr ) {
		if ( GPtr->logLevel >= kDebugLog ) {
        	printf( "\treplacement name already exists \n" );
			printf( "\tduplicate name is 0x" );
			PrintName( newNamePtr->ustr.length, (UInt8 *) &newNamePtr->ustr.unicode, true );
		}
        goto ErrorExit;
    }
    
    // get catalog record for object with the illegal name.  We will restore this
    // info with our new (valid) name. 
	BuildCatalogKey( roPtr->parid, oldNamePtr, isHFSPlus, &key );
	result = SearchBTreeRecord( fcbPtr, &key, kNoHint, NULL, &record, &recSize, &hint );
	if ( result != noErr ) {
        goto ErrorExit;
    }
 
    result	= DeleteBTreeRecord( fcbPtr, &key );
	if ( result != noErr ) {
        goto ErrorExit;
    }
 
    // insert record back into the catalog using the new name
    result	= InsertBTreeRecord( fcbPtr, &newKey, &record, recSize, &hint );
	if ( result != noErr ) {
        goto ErrorExit;
    }

	isDirectory = false;
    switch( record.recordType )
    {
        case kHFSFolderRecord:
        case kHFSPlusFolderRecord:	
			isDirectory = true;	 break;
    }
 
    // now we need to remove the old thread record and create a new one with
    // our new name.
	BuildCatalogKey( GetObjectID( &record ), NULL, isHFSPlus, &key );
	result = SearchBTreeRecord( fcbPtr, &key, kNoHint, NULL, &record, &recSize, &hint );
	if ( result != noErr ) {
        goto ErrorExit;
    }
 
    result	= DeleteBTreeRecord( fcbPtr, &key );
	if ( result != noErr ) {
        goto ErrorExit;
    }

    // insert thread record with new name as thread data
	recSize = BuildThreadRec( &newKey, &record, isHFSPlus, isDirectory );
 	result = InsertBTreeRecord( fcbPtr, &key, &record, recSize, &hint );
	if ( result != noErr ) {
        goto ErrorExit;
	}

    return( noErr );
 
ErrorExit:
	GPtr->minorRepairErrors = true;
    if ( GPtr->logLevel >= kDebugLog )
        printf( "\t%s - repair failed for type 0x%02X %d \n", __FUNCTION__, roPtr->type, roPtr->type );
    return( noErr );  // errors in this routine should not be fatal
    
} /* FixIllegalNames */


/*------------------------------------------------------------------------------
FixBSDInfo:  Reset or repair BSD info
                 (HFS Plus volumes only)
------------------------------------------------------------------------------*/
static OSErr
FixBSDInfo(SGlobPtr GPtr, RepairOrderPtr p)
{
	SFCB 						*fcb;
	CatalogRecord 				rec;
	FSBufferDescriptor 			btRecord;
	BTreeIterator 				btIterator;
	Boolean						isHFSPlus;
	OSErr 						result;
	UInt16 						recSize;
	size_t 						namelen;
	unsigned char 				filename[256];

	isHFSPlus = VolumeObjectIsHFSPlus( );
	if (!isHFSPlus)
		return (0);
	fcb = GPtr->calculatedCatalogFCB;

	ClearMemory(&btIterator, sizeof(btIterator));
	btIterator.hint.nodeNum = p->hint;
	BuildCatalogKey(p->parid, (CatalogName *)&p->name, true,
		(CatalogKey*)&btIterator.key);
	btRecord.bufferAddress = &rec;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(rec);
	
	result = BTSearchRecord(fcb, &btIterator, kInvalidMRUCacheKey,
			&btRecord, &recSize, &btIterator);
	if (result)
		return (IntError(GPtr, result));

	if (rec.recordType != kHFSPlusFileRecord &&
	    rec.recordType != kHFSPlusFolderRecord)
		return (noErr);

	utf_encodestr(((HFSUniStr255 *)&p->name)->unicode,
		((HFSUniStr255 *)&p->name)->length << 1, filename, &namelen);
	filename[namelen] = '\0';

	if (p->type == E_InvalidPermissions &&
	    ((UInt16)p->incorrect == rec.hfsPlusFile.bsdInfo.fileMode)) {
		if (GPtr->logLevel >= kDebugLog)
		    printf("\t\"%s\": fixing mode from %07o to %07o\n",
			   filename, (int)p->incorrect, (int)p->correct);

		rec.hfsPlusFile.bsdInfo.fileMode = (UInt16)p->correct;
		result = BTReplaceRecord(fcb, &btIterator, &btRecord, recSize);
	}

	if (p->type == E_InvalidUID) {
		if ((UInt32)p->incorrect == rec.hfsPlusFile.bsdInfo.ownerID) {
			if (GPtr->logLevel >= kDebugLog) {
				printf("\t\"%s\": replacing UID %d with %d\n",
				filename, (int)p->incorrect, (int)p->correct);
			}
			rec.hfsPlusFile.bsdInfo.ownerID = (UInt32)p->correct;
		}
		/* Fix group ID if neccessary */
		if ((UInt32)p->incorrect == rec.hfsPlusFile.bsdInfo.groupID)
			rec.hfsPlusFile.bsdInfo.groupID = (UInt32)p->correct;
		result = BTReplaceRecord(fcb, &btIterator, &btRecord, recSize);
	}

	if (result)
		return (IntError(GPtr, result));
	else	
		return (noErr);
}


/*------------------------------------------------------------------------------
DeleteUnlinkedFile:  Delete orphaned data node (BSD unlinked file)
		     Also used to delete empty "HFS+ Private Data" directories
                     (HFS Plus volumes only)
------------------------------------------------------------------------------*/
static OSErr
DeleteUnlinkedFile(SGlobPtr GPtr, RepairOrderPtr p)
{
	CatalogName 		name;
	CatalogName 		*cNameP;
	Boolean				isHFSPlus;
	size_t 				len;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	if (!isHFSPlus)
		return (0);

	if (p->name[0] > 0) {
		/* name was stored in UTF-8 */
		(void) utf_decodestr(&p->name[1], p->name[0], name.ustr.unicode, &len);
		name.ustr.length = len / 2;
		cNameP = &name;
	} else {
		cNameP = NULL;
	}

	(void) DeleteCatalogNode(GPtr->calculatedVCB, p->parid, cNameP, p->hint);

	GPtr->VIStat |= S_MDB;
	GPtr->VIStat |= S_VBM;

	return (noErr);
}

/*
 * Fix a file's PEOF or LEOF (truncate)
 * (HFS Plus volumes only)
 */
static OSErr
FixFileSize(SGlobPtr GPtr, RepairOrderPtr p)
{
	SFCB *fcb;
	CatalogRecord rec;
	HFSPlusCatalogKey * keyp;
	FSBufferDescriptor btRecord;
	BTreeIterator btIterator;
	size_t len;
	Boolean	isHFSPlus;
	Boolean replace;
	OSErr result;
	UInt16 recSize;
	UInt64 bytes;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	if (!isHFSPlus)
		return (0);
	fcb = GPtr->calculatedCatalogFCB;
	replace = false;

	ClearMemory(&btIterator, sizeof(btIterator));
	btIterator.hint.nodeNum = p->hint;
	keyp = (HFSPlusCatalogKey*)&btIterator.key;
	keyp->parentID = p->parid;

	/* name was stored in UTF-8 */
	(void) utf_decodestr(&p->name[1], p->name[0], keyp->nodeName.unicode, &len);
	keyp->nodeName.length = len / 2;
	keyp->keyLength = kHFSPlusCatalogKeyMinimumLength + len;

	btRecord.bufferAddress = &rec;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(rec);
	
	result = BTSearchRecord(fcb, &btIterator, kInvalidMRUCacheKey,
			&btRecord, &recSize, &btIterator);
	if (result)
		return (IntError(GPtr, result));

	if (rec.recordType != kHFSPlusFileRecord)
		return (noErr);

	if (p->type == E_PEOF) {
		bytes = p->correct * (UInt64)GPtr->calculatedVCB->vcbBlockSize;
		if ((p->forkType == kRsrcFork) &&
		    ((UInt32)p->incorrect == rec.hfsPlusFile.resourceFork.totalBlocks)) {

			rec.hfsPlusFile.resourceFork.totalBlocks = (UInt32)p->correct;
			replace = true;
			/*
			 * Make sure our new block count is large
			 * enough to cover the current LEOF.  If
			 * its not we must truncate the fork.
			 */
			if (rec.hfsPlusFile.resourceFork.logicalSize > bytes) {
				rec.hfsPlusFile.resourceFork.logicalSize = bytes;
			}
		} else if ((p->forkType == kDataFork) &&
		           ((UInt32)p->incorrect == rec.hfsPlusFile.dataFork.totalBlocks)) {

			rec.hfsPlusFile.dataFork.totalBlocks = (UInt32)p->correct;
			replace = true;
			/*
			 * Make sure our new block count is large
			 * enough to cover the current LEOF.  If
			 * its not we must truncate the fork.
			 */
			if (rec.hfsPlusFile.dataFork.logicalSize > bytes) {
				rec.hfsPlusFile.dataFork.logicalSize = bytes;
			}
		}
	} else /* E_LEOF */ {
		if ((p->forkType == kRsrcFork) &&
		    (p->incorrect == rec.hfsPlusFile.resourceFork.logicalSize)) {

			rec.hfsPlusFile.resourceFork.logicalSize = p->correct;
			replace = true;

		} else if ((p->forkType == kDataFork) &&
		           (p->incorrect == rec.hfsPlusFile.dataFork.logicalSize)) {

			rec.hfsPlusFile.dataFork.logicalSize = p->correct;
			replace = true;
		}
	}

	if (replace) {
		result = BTReplaceRecord(fcb, &btIterator, &btRecord, recSize);
		if (result)
			return (IntError(GPtr, result));
	}
		
	return (noErr);
}

/*------------------------------------------------------------------------------

Routine:	FixEmbededVolDescription

Function:	If the "mdb->drAlBlSt" field has been modified, i.e. Norton Disk Doctor
			3.5 tried to "Fix" an HFS+ volume, it reduces the value in the 
			"mdb->drAlBlSt" field.  If this field is changed, the file system can 
			no longer find the VolumeHeader or AltVolumeHeader.
			
Input:		GPtr			-	pointer to scavenger global area
			p				- 	pointer to the repair order

Output:		FixMDBdrAlBlSt	- 	function result:
									0 = no error
									n = error
------------------------------------------------------------------------------*/

static	OSErr	FixEmbededVolDescription( SGlobPtr GPtr, RepairOrderPtr p )
{
	OSErr					err;
	HFSMasterDirectoryBlock	*mdb;
	EmbededVolDescription	*desc;
	SVCB					*vcb = GPtr->calculatedVCB;
	BlockDescriptor  		block;

	desc = (EmbededVolDescription *) &(p->name);
	block.buffer = NULL;
	
	/* Fix the Alternate MDB */
	err = GetVolumeObjectAlternateMDB( &block );
	if ( err != noErr )
		goto ExitThisRoutine;
	mdb = (HFSMasterDirectoryBlock *) block.buffer;
	
	mdb->drAlBlSt			= desc->drAlBlSt;
	mdb->drEmbedSigWord		= desc->drEmbedSigWord;
	mdb->drEmbedExtent.startBlock	= desc->drEmbedExtent.startBlock;
	mdb->drEmbedExtent.blockCount	= desc->drEmbedExtent.blockCount;
	
	err = ReleaseVolumeBlock( vcb, &block, kForceWriteBlock );
	block.buffer = NULL;
	if ( err != noErr )
		goto ExitThisRoutine;
	
	/* Fix the MDB */
	err = GetVolumeObjectPrimaryMDB( &block );
	if ( err != noErr )
		goto ExitThisRoutine;
	mdb = (HFSMasterDirectoryBlock *) block.buffer;
	
	mdb->drAlBlSt			= desc->drAlBlSt;
	mdb->drEmbedSigWord		= desc->drEmbedSigWord;
	mdb->drEmbedExtent.startBlock	= desc->drEmbedExtent.startBlock;
	mdb->drEmbedExtent.blockCount	= desc->drEmbedExtent.blockCount;
	err = ReleaseVolumeBlock( vcb, &block, kForceWriteBlock );
	block.buffer = NULL;
	
ExitThisRoutine:
	if ( block.buffer != NULL )
		err = ReleaseVolumeBlock( vcb, &block, kReleaseBlock );
	
	return( err );
}




/*------------------------------------------------------------------------------

Routine:	FixWrapperExtents

Function:	When Norton Disk Doctor 2.0 tries to repair an HFS Plus volume, it
			assumes that the first catalog extent must be a fixed number of
			allocation blocks after the start of the first extents extent (in the
			wrapper).  In reality, the first catalog extent should start immediately
			after the first extents extent.
			
Input:		GPtr			-	pointer to scavenger global area
			p				- 	pointer to the repair order

Output:
			0 = no error
			n = error
------------------------------------------------------------------------------*/

static	OSErr	FixWrapperExtents( SGlobPtr GPtr, RepairOrderPtr p )
{
#if !LINUX
#pragma unused (p)
#endif

	OSErr						err;
	HFSMasterDirectoryBlock		*mdb;
	SVCB						*vcb = GPtr->calculatedVCB;
	BlockDescriptor  			block;

	/* Get the Alternate MDB */
	block.buffer = NULL;
	err = GetVolumeObjectAlternateMDB( &block );
	if ( err != noErr )
		goto ExitThisRoutine;
	mdb = (HFSMasterDirectoryBlock	*) block.buffer;

	/* Fix the wrapper catalog's first (and only) extent */
	mdb->drCTExtRec[0].startBlock = mdb->drXTExtRec[0].startBlock +
	                                mdb->drXTExtRec[0].blockCount;
	
	err = ReleaseVolumeBlock(vcb, &block, kForceWriteBlock);
	block.buffer = NULL;
	if ( err != noErr )
		goto ExitThisRoutine;
	
	/* Fix the MDB */
	err = GetVolumeObjectPrimaryMDB( &block );
	if ( err != noErr )
		goto ExitThisRoutine;
	mdb = (HFSMasterDirectoryBlock	*) block.buffer;
	
	mdb->drCTExtRec[0].startBlock = mdb->drXTExtRec[0].startBlock +
	                                mdb->drXTExtRec[0].blockCount;
	
	err = ReleaseVolumeBlock(vcb, &block, kForceWriteBlock);
	block.buffer = NULL;

ExitThisRoutine:
	if ( block.buffer != NULL )
		(void) ReleaseVolumeBlock( vcb, &block, kReleaseBlock );
	
	return( err );
}


//
//	Entries in the extents BTree which do not have a corresponding catalog entry get fixed here
//	This routine will run slowly if the extents file is large because we require a Catalog BTree
//	search for each extent record.
//
static	OSErr	FixOrphanedExtent( SGlobPtr GPtr )
{
#if 0
	OSErr				err;
	UInt32				hint;
	UInt32				recordSize;
	UInt32				maxRecords;
	UInt32				numberOfFilesInList;
	ExtentKey			*extentKeyPtr;
	ExtentRecord		*extentDataPtr;
	ExtentRecord		extents;
	ExtentRecord		zeroExtents;
	CatalogKey			foundExtentKey;
	CatalogRecord		catalogData;
	CatalogRecord		threadData;
	HFSCatalogNodeID	fileID;
	BTScanState			scanState;

	HFSCatalogNodeID	lastFileID			= -1;
	UInt32			recordsFound		= 0;
	Boolean			mustRebuildBTree	= false;
	Boolean			isHFSPlus;
	SVCB			*calculatedVCB		= GPtr->calculatedVCB;
	UInt32			**dataHandle		= GPtr->validFilesList;
	SFCB *			fcb = GPtr->calculatedExtentsFCB;

	//	Set Up
	isHFSPlus = VolumeObjectIsHFSPlus( );
	//
	//	Use the BTree scanner since we use MountCheck to find orphaned extents, and MountCheck uses the scanner
	err = BTScanInitialize( fcb, 0, 0, 0, gFSBufferPtr, gFSBufferSize, &scanState );
	if ( err != noErr )	return( badMDBErr );

	ClearMemory( (Ptr)&zeroExtents, sizeof(ExtentRecord) );

	if ( isHFSPlus )
	{
		maxRecords = fcb->fcbLogicalSize / sizeof(HFSPlusExtentRecord);
	}
	else
	{
		maxRecords = fcb->fcbLogicalSize / sizeof(HFSExtentRecord);
		numberOfFilesInList = GetHandleSize((Handle) dataHandle) / sizeof(UInt32);
		qsort( *dataHandle, numberOfFilesInList, sizeof (UInt32), cmpLongs );	// Sort the list of found file IDs
	}


	while ( recordsFound < maxRecords )
	{
		err = BTScanNextRecord( &scanState, false, (void **) &extentKeyPtr, (void **) &extentDataPtr, &recordSize );

		if ( err != noErr )
		{
			if ( err == btNotFound )
				err	= noErr;
			break;
		}

		++recordsFound;
		fileID = (isHFSPlus == true) ? extentKeyPtr->hfsPlus.fileID : extentKeyPtr->hfs.fileID;
		
		if ( (fileID > kHFSBadBlockFileID) && (lastFileID != fileID) )	// Keep us out of reserved file trouble
		{
			lastFileID	= fileID;
			
			if ( isHFSPlus )
			{
				err = LocateCatalogThread( calculatedVCB, fileID, &threadData, (UInt16*)&recordSize, &hint );	//	This call returns nodeName as either Str31 or HFSUniStr255, no need to call PrepareInputName()
				
				if ( err == noErr )									//	Thread is found, just verify actual record exists.
				{
					err = LocateCatalogNode( calculatedVCB, threadData.hfsPlusThread.parentID, (const CatalogName *) &(threadData.hfsPlusThread.nodeName), kNoHint, &foundExtentKey, &catalogData, &hint );
				}
				else if ( err == cmNotFound )
				{
					err = SearchBTreeRecord( GPtr->calculatedExtentsFCB, extentKeyPtr, kNoHint, &foundExtentKey, &extents, (UInt16*)&recordSize, &hint );
					if ( err == noErr )
					{	//¥¥ can't we just delete btree record?
						err = ReplaceBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey, hint, &zeroExtents, recordSize, &hint );
						err	= DeleteBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey );	//	Delete the orphaned extent
					}
				}
					
				if ( err != noErr )
					mustRebuildBTree	= true;						//	if we have errors here we should rebuild the extents btree
			}
			else
			{
				if ( ! bsearch( &fileID, *dataHandle, numberOfFilesInList, sizeof(UInt32), cmpLongs ) )
				{
					err = SearchBTreeRecord( GPtr->calculatedExtentsFCB, extentKeyPtr, kNoHint, &foundExtentKey, &extents, (UInt16*)&recordSize, &hint );
					if ( err == noErr )
					{	//¥¥ can't we just delete btree record?
						err = ReplaceBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey, hint, &zeroExtents, recordSize, &hint );
						err = DeleteBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey );	//	Delete the orphaned extent
					}
					
					if ( err != noErr )
						mustRebuildBTree	= true;						//	if we have errors here we should rebuild the extents btree
				}
			}
		}
	}

	if ( mustRebuildBTree == true )
	{
		GPtr->EBTStat |= S_RebuildBTree;
		err	= errRebuildBtree;
	}

	return( err );
#else
	return (0);
#endif
}

/* Function: FixOrphanedFiles
 *
 * Description:
 *	Incorrect number of thread records get fixed in this function.
 *
 *	The function traverses the entire catalog Btree.  
 *
 *	For a file/folder record, it tries to lookup its corresponding thread
 *	record.  If the thread record does not exist, a new thread record is 
 *  created.  If the thread record is not correct, the incorrect thread 
 *  record is deleted and a new thread record is created.  The parent ID 
 *  and the name of the file/folder are compared for correctness. 
 *	For plain HFS, a thread record is only looked-up if kHFSThreadExistsMask is set.
 *
 *	For a thread record, it tries to lookup its corresponding file/folder 
 *	record.  If its does not exist or is not correct, the thread record
 *	is deleted.  The file/folder ID is compared for correctness.
 *
 * Input:	1. GPtr - pointer to global scavenger area
 * 
 * Return value:	
 * 		      zero means success
 *		      non-zero means failure
 */
static	OSErr	FixOrphanedFiles ( SGlobPtr GPtr )
{
	CatalogKey			key;
	CatalogKey			foundKey;
	CatalogKey			tempKey;
	CatalogRecord		record;
	CatalogRecord		threadRecord;
	CatalogRecord		record2;
	HFSCatalogNodeID	parentID;
	HFSCatalogNodeID	cNodeID = 0;
	BTreeIterator		savedIterator;
	UInt32				hint;
	UInt32				hint2;
	UInt32				threadHint;
	OSErr				err;
	UInt16				recordSize;
	SInt16				recordType;
	SInt16				selCode				= 0x8001;	/* Get first record */
	Boolean				isHFSPlus;
	BTreeControlBlock	*btcb				= GetBTreeControlBlock( kCalculatedCatalogRefNum );

	isHFSPlus = VolumeObjectIsHFSPlus( );
	CopyMemory( &btcb->lastIterator, &savedIterator, sizeof(BTreeIterator) );

	do
	{
		//	Save/Restore Iterator around calls to GetBTreeRecord
		CopyMemory( &savedIterator, &btcb->lastIterator, sizeof(BTreeIterator) );
		err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
		if ( err != noErr ) break;
		CopyMemory( &btcb->lastIterator, &savedIterator, sizeof(BTreeIterator) );
	
		selCode		= 1;	//	 kNextRecord			
		recordType	= record.recordType;
		
		switch( recordType )
		{
			case kHFSFileRecord:
				//	If the small file is not supposed to have a thread, just break
				if ( ( record.hfsFile.flags & kHFSThreadExistsMask ) == 0 )
					break;
			
			case kHFSFolderRecord:
			case kHFSPlusFolderRecord:
			case kHFSPlusFileRecord:
				
				//	Locate the thread associated with this record
				
				(void) CheckForStop( GPtr );		//	rotate cursor

				parentID	= isHFSPlus == true ? foundKey.hfsPlus.parentID : foundKey.hfs.parentID;
				threadHint	= hint;
				
				switch( recordType )
				{
					case kHFSFolderRecord:		cNodeID		= record.hfsFolder.folderID;		break;
					case kHFSFileRecord:		cNodeID		= record.hfsFile.fileID;			break;
					case kHFSPlusFolderRecord:	cNodeID		= record.hfsPlusFolder.folderID;	break;
					case kHFSPlusFileRecord:	cNodeID		= record.hfsPlusFile.fileID;		break;
				}

				//-- Build the key for the file thread
				BuildCatalogKey( cNodeID, nil, isHFSPlus, &key );

				err = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, 
										 &tempKey, &threadRecord, &recordSize, &hint2 );

				/* We found a thread record for this file/folder record. */
				if (err == noErr) {
					/* Check if the parent ID and nodeName are same.  If not, we are 
					 * missing a correct thread record.  Force btNotFound in such case.
					 */
					if (isHFSPlus) {
						/* Compare parent ID */
						if (parentID != threadRecord.hfsPlusThread.parentID) {
							(void) DeleteBTreeRecord(GPtr->calculatedCatalogFCB, &tempKey);
							err = btNotFound;
							if (GPtr->logLevel >= kDebugLog) {
								printf ("\t%s: parentID for id=%u do not match (fileKey=%u threadRecord=%u)\n", __FUNCTION__, cNodeID, parentID, threadRecord.hfsPlusThread.parentID);
							}
						}

						/* Compare nodeName from file/folder key and thread record */
						if (!((foundKey.hfsPlus.nodeName.length == threadRecord.hfsPlusThread.nodeName.length) 
						    && (!bcmp(foundKey.hfsPlus.nodeName.unicode, 
								      threadRecord.hfsPlusThread.nodeName.unicode,
									  foundKey.hfsPlus.nodeName.length * 2)))) {
							(void) DeleteBTreeRecord(GPtr->calculatedCatalogFCB, &tempKey);
							err = btNotFound;
							if (GPtr->logLevel >= kDebugLog) {
								printf ("\t%s: nodeName for id=%u do not match\n", __FUNCTION__, cNodeID);
							}
						}
					} else {
						/* Compare parent ID */
						if (parentID != threadRecord.hfsThread.parentID) {
							(void) DeleteBTreeRecord(GPtr->calculatedCatalogFCB, &tempKey);
							err = btNotFound;
							if (GPtr->logLevel >= kDebugLog) {
								printf ("\t%s: parentID for id=%u do not match (fileKey=%u threadRecord=%u)\n", __FUNCTION__, cNodeID, parentID, threadRecord.hfsThread.parentID);
							}
						}

						/* Compare nodeName from file/folder key and thread record */
						if (!((foundKey.hfs.nodeName[0] == threadRecord.hfsThread.nodeName[0]) 
						    && (!bcmp(&foundKey.hfs.nodeName[1], 
								      &threadRecord.hfsThread.nodeName[1],
									  foundKey.hfs.nodeName[0])))) {
							(void) DeleteBTreeRecord(GPtr->calculatedCatalogFCB, &tempKey);
							err = btNotFound;
							if (GPtr->logLevel >= kDebugLog) {
								printf ("\t%s: nodeName for id=%u do not match\n", __FUNCTION__, cNodeID);
							}
						}
					}
				} /* err == noErr */ 

				//	For missing thread records, just create the thread
				if ( err == btNotFound )
				{
					//	Create the missing thread record.
					Boolean		isDirectory;
					
					isDirectory = false;
					switch( recordType )
					{
						case kHFSFolderRecord:
						case kHFSPlusFolderRecord:	
							isDirectory = true;		
							break;
					}

					//-- Fill out the data for the new file thread from the key 
					// of catalog file/folder record
					recordSize = BuildThreadRec( &foundKey, &threadRecord, isHFSPlus, 
												 isDirectory );
					err = InsertBTreeRecord( GPtr->calculatedCatalogFCB, &key,
											 &threadRecord, recordSize, &threadHint );
					if (GPtr->logLevel >= kDebugLog) {
						printf ("\t%s: Created thread record for id=%u (err=%u)\n", __FUNCTION__, cNodeID, err);
					}
				}
			
				break;
			
			
			case kHFSFolderThreadRecord:
			case kHFSFileThreadRecord:
			case kHFSPlusFolderThreadRecord:
			case kHFSPlusFileThreadRecord:
				
				//	Find the catalog record, if it does not exist, delete the existing thread.
				if ( isHFSPlus )
					BuildCatalogKey( record.hfsPlusThread.parentID, (const CatalogName *)&record.hfsPlusThread.nodeName, isHFSPlus, &key );
				else
					BuildCatalogKey( record.hfsThread.parentID, (const CatalogName *)&record.hfsThread.nodeName, isHFSPlus, &key );
				
				err = SearchBTreeRecord ( GPtr->calculatedCatalogFCB, &key, kNoHint, &tempKey, &record2, &recordSize, &hint2 );

				/* We found a file/folder record for this thread record. */
				if (err == noErr) {
					/* Check if the file/folder ID are same.  If not, we are missing a 
					 * correct file/folder record.  Delete the extra thread record
					 */
					if (isHFSPlus) {
						/* Compare file/folder ID */
						if (foundKey.hfsPlus.parentID != record2.hfsPlusFile.fileID) {
							err = btNotFound;
							if (GPtr->logLevel >= kDebugLog) {
								printf ("\t%s: fileID do not match (threadKey=%u fileRecord=%u), parentID=%u\n", __FUNCTION__, foundKey.hfsPlus.parentID, record2.hfsPlusFile.fileID, record.hfsPlusThread.parentID);
							}
						}
					} else {
						/* Compare file/folder ID */
						if (foundKey.hfs.parentID != record2.hfsFile.fileID) {
							err = btNotFound;
							if (GPtr->logLevel >= kDebugLog) {
								if (recordType == kHFSFolderThreadRecord) {
									printf ("\t%s: fileID do not match (threadKey=%u fileRecord=%u), parentID=%u\n", __FUNCTION__, foundKey.hfs.parentID, record2.hfsFolder.folderID, record.hfsThread.parentID);
								} else {
									printf ("\t%s: fileID do not match (threadKey=%u fileRecord=%u), parentID=%u\n", __FUNCTION__, foundKey.hfs.parentID, record2.hfsFile.fileID, record.hfsThread.parentID);
								}
							}
						}
					}
				} /* if (err == noErr) */

				if ( err != noErr )
				{
					err = DeleteBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey );
					if (GPtr->logLevel >= kDebugLog) {
						if (isHFSPlus) {
							printf ("\t%s: Deleted thread record for id=%d (err=%d)\n", __FUNCTION__, foundKey.hfsPlus.parentID, err);
						} else {
							printf ("\t%s: Deleted thread record for id=%d (err=%d)\n", __FUNCTION__, foundKey.hfs.parentID, err);
						}
					}
				}
				
				break;
				
			default:
				if (GPtr->logLevel >= kDebugLog) {
					printf ("\t%s: Unknown record type.\n", __FUNCTION__);
				}
				break;

		}
	} while ( err == noErr );

	if ( err == btNotFound )
		err = noErr;				 						//	all done, no more catalog records

//	if (err == noErr)
//		err = BTFlushPath( GPtr->calculatedCatalogFCB );

	return( err );
}

static	OSErr	RepairReservedBTreeFields ( SGlobPtr GPtr )
{
	CatalogRecord		record;
	CatalogKey			foundKey;
	UInt16 				recordSize;
	SInt16				selCode;
	UInt32				hint;
	UInt32				*reserved;
	OSErr				err;

	selCode = 0x8001;															//	 start with 1st record			

	err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
	if ( err != noErr ) goto EXIT;

	selCode = 1;																//	 get next record from now on		
	
	do
	{
		switch( record.recordType )
		{
			case kHFSPlusFolderRecord:
				if ( record.hfsPlusFolder.flags != 0 )
				{
					record.hfsPlusFolder.flags = 0;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;
				
			case kHFSPlusFileRecord:
				//	Note: bit 7 (mask 0x80) of flags is unused in HFS or HFS Plus.  However, Inside Macintosh: Files
				//	describes it as meaning the file record is in use.  Some non-Apple implementations end up setting
				//	this bit, so we just ignore it.
				if ( ( record.hfsPlusFile.flags  & (UInt16) ~(0X83) ) != 0 )
				{
					record.hfsPlusFile.flags &= 0X83;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;

			case kHFSFolderRecord:
				if ( record.hfsFolder.flags != 0 )
				{
					record.hfsFolder.flags = 0;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;

			case kHFSFolderThreadRecord:
			case kHFSFileThreadRecord:
				reserved = (UInt32*) &(record.hfsThread.reserved);
				if ( reserved[0] || reserved[1] )
				{
					reserved[0]	= 0;
					reserved[1]	= 0;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;

			case kHFSFileRecord:
				//	Note: bit 7 (mask 0x80) of flags is unused in HFS or HFS Plus.  However, Inside Macintosh: Files
				//	describes it as meaning the file record is in use.  Some non-Apple implementations end up setting
				//	this bit, so we just ignore it.
				if ( 	( ( record.hfsFile.flags  & (UInt8) ~(0X83) ) != 0 )
					||	( record.hfsFile.dataStartBlock != 0 )	
					||	( record.hfsFile.rsrcStartBlock != 0 )	
					||	( record.hfsFile.reserved != 0 )			)
				{
					record.hfsFile.flags			&= 0X83;
					record.hfsFile.dataStartBlock	= 0;
					record.hfsFile.rsrcStartBlock	= 0;
					record.hfsFile.reserved		= 0;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;
				
			default:
				break;
		}

		if ( err != noErr ) goto EXIT;
		
		err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
	
	} while ( err == noErr );

	if ( err == btNotFound )
		err = noErr;				 						//	all done, no more catalog records

EXIT:
	return( err );
}

/* Function:	GetCatalogRecord
 *
 * Description:
 * This function returns a catalog file/folder record for a given
 * file/folder ID from the catalog BTree.
 *
 * Input:	1. GPtr - pointer to global scavenger area
 *        	2. fileID - file ID to search the file/folder record  
 *      	3. isHFSPlus - boolean value to indicate if volume is HFSPlus 
 *
 * Output:	1. catKey - catalog key
 *        	2. catRecord - catalog record for given ID
 *         	3. recordSize - size of catalog record return back 
 *
 * Return value:	
 * 		      zero means success
 *		      non-zero means failure
 */
static OSErr GetCatalogRecord(SGlobPtr GPtr, UInt32 fileID, Boolean isHFSPlus, CatalogKey *catKey, CatalogRecord *catRecord, UInt16 *recordSize) 
{
	OSErr err = noErr;
	CatalogKey catThreadKey;
	CatalogName catalogName;
	UInt32 hint;

	/* Look up for catalog thread record for the file that owns attribute */
	BuildCatalogKey(fileID, NULL, isHFSPlus, &catThreadKey);
	err = SearchBTreeRecord(GPtr->calculatedCatalogFCB, &catThreadKey, kNoHint, catKey, catRecord, recordSize, &hint);
	if (err) {
#if DEBUG_XATTR
		printf ("%s: No matching catalog thread record found\n", __FUNCTION__);
#endif
		goto out;
	}

#if DEBUG_XATTR
	printf ("%s(%s,%d):1 recordType=%x, flags=%x\n", __FUNCTION__, __FILE__, __LINE__, 
				catRecord->hfsPlusFile.recordType, 
				catRecord->hfsPlusFile.flags);
#endif

	/* We were expecting a thread record.  The recordType says it is a file 
	 * record or folder record.  Return error.
	 */
	if ((catRecord->hfsPlusFile.recordType == kHFSPlusFolderRecord) ||
	    (catRecord->hfsPlusFile.recordType == kHFSPlusFileRecord)) {
		err = fsBTRecordNotFoundErr; 
		goto out;
	}
	
	/* It is either a file thread record or folder thread record.
	 * Look up for catalog record for the file that owns attribute */
	CopyCatalogName((CatalogName *)&(catRecord->hfsPlusThread.nodeName), &catalogName, isHFSPlus);
	BuildCatalogKey(catRecord->hfsPlusThread.parentID, &catalogName, isHFSPlus, catKey); 
	err = SearchBTreeRecord(GPtr->calculatedCatalogFCB, catKey, kNoHint, catKey, catRecord, recordSize, &hint);
	if (err) {
#if DEBUG_XATTR	
		printf ("%s: No matching catalog record found\n", __FUNCTION__);
#endif
		goto out;
	}
	
#if DEBUG_XATTR
	printf ("%s(%s,%d):2 recordType=%x, flags=%x\n", __FUNCTION__, __FILE__, __LINE__, 
				catRecord->hfsPlusFile.recordType, 
				catRecord->hfsPlusFile.flags);
#endif

out:
	return err;
}

/* Function:	RepairAttributesCheckABT
 *
 * Description:
 * This function is called from RepairAttributes (to repair extended 
 * attributes) during repair stage of fsck_hfs.
 *
 * 1. Make full pass through attribute BTree.
 * 2. For every unique fileID, lookup its catalog record in Catalog BTree.
 * 3. If found, check the attributes/security bit in catalog record.
 *              If not set correctly, set it and replace the catalog record.
 * 4. If not found, return error 
 * 
 * Input:	1. GPtr - pointer to global scavenger area
 *      	2. isHFSPlus - boolean value to indicate if volume is HFSPlus 
 *
 * Output:	err - Function result 
 * 		      zero means success
 *		      non-zero means failure
 */
static OSErr RepairAttributesCheckABT(SGlobPtr GPtr, Boolean isHFSPlus) 
{
	OSErr err = noErr;
	UInt16 selCode;		/* select access pattern for BTree */
	UInt32 hint;

	HFSPlusAttrRecord attrRecord;
	HFSPlusAttrKey attrKey;
	UInt16 attrRecordSize;
	CatalogRecord catRecord; 
	CatalogKey catKey;
	UInt16 catRecordSize;

	lastAttrID lastID;	/* fileID for the last attribute searched */
	Boolean didRecordChange = false; 	/* whether catalog record was changed after checks */

#if DEBUG_XATTR
	char attrName[XATTR_MAXNAMELEN];
	size_t len;
#endif

	lastID.fileID = 0;
	lastID.hasSecurity = false;
	
	selCode = 0x8001;	/* Get first record from BTree */
	err = GetBTreeRecord(GPtr->calculatedAttributesFCB, selCode, &attrKey, &attrRecord, &attrRecordSize, &hint);
	if (err != noErr) goto out; 

	selCode = 1;	/* Get next record */
	do {
#if DEBUG_XATTR
		/* Convert unicode attribute name to char for ACL check */
		(void) utf_encodestr(attrKey.attrName, attrKey.attrNameLen * 2, attrName, &len);
		attrName[len] = '\0';
		printf ("%s(%s,%d): Found attrName=%s for fileID=%d\n", __FUNCTION__, __FILE__, __LINE__, attrName, attrKey.fileID);
#endif
	
		if (attrKey.fileID != lastID.fileID) {
			/* We found an attribute with new file ID */
			
			/* Replace the previous catalog record only if we updated the flags */
			if (didRecordChange == true) {
				err = ReplaceBTreeRecord(GPtr->calculatedCatalogFCB, &catKey , kNoHint, &catRecord, catRecordSize, &hint);
				if (err) {
#if DEBUG_XATTR
					printf ("%s: Error in replacing Catalog Record\n", __FUNCTION__);
#endif		
					goto out;
				}
			}
			
			didRecordChange = false; /* reset to indicate new record has not changed */

			/* Get the catalog record for the new fileID */
			err = GetCatalogRecord(GPtr, attrKey.fileID, isHFSPlus, &catKey, &catRecord, &catRecordSize);
			if (err) {
				/* No catalog record was found for this fileID. */
#if DEBUG_XATTR
				printf ("%s: No matching catalog record found\n", __FUNCTION__);
#endif
				/* 3984119 - Do not delete extended attributes for file IDs less
	 			 * kHFSFirstUserCatalogNodeID but not equal to kHFSRootFolderID 
	 			 * in prime modulus checksum.  These file IDs do not have 
	 			 * any catalog record
	 			 */
				if ((attrKey.fileID < kHFSFirstUserCatalogNodeID) && 
	    			    (attrKey.fileID != kHFSRootFolderID)) { 
#if DEBUG_XATTR
					printf ("%s: Ignore catalog check for fileID=%d for attribute=%s\n", __FUNCTION__, attrKey.fileID, attrName); 
#endif
					goto getnext;
				}

				/* Delete this orphan extended attribute */
				err = DeleteBTreeRecord(GPtr->calculatedAttributesFCB, &attrKey);
				if (err) {
#if DEBUG_XATTR
					printf ("%s: Error in deleting attribute record\n", __FUNCTION__);
#endif
					goto out;
				}
#if DEBUG_XATTR
				printf ("%s: Deleted attribute=%s for fileID=%d\n", __FUNCTION__, attrName, attrKey.fileID);
#endif
				/* set flags to write back header and map */
				GPtr->ABTStat |= S_BTH + S_BTM;	
				goto getnext;
			} 

			lastID.fileID = attrKey.fileID;	/* set last fileID to the new ID */
			lastID.hasSecurity = false; /* reset to indicate new fileID does not have security */
				
			/* Check the Attribute bit */
			if (!(catRecord.hfsPlusFile.flags & kHFSHasAttributesMask)) {
				/* kHFSHasAttributeBit should be set */
				catRecord.hfsPlusFile.flags |= kHFSHasAttributesMask;
				didRecordChange = true;
			}

			/* Check if attribute is ACL */
			if (!bcmp(attrKey.attrName, GPtr->securityAttrName, GPtr->securityAttrLen)) {
				lastID.hasSecurity = true;
				/* Check the security bit */
				if (!(catRecord.hfsPlusFile.flags & kHFSHasSecurityMask)) {
					/* kHFSHasSecurityBit should be set */
					catRecord.hfsPlusFile.flags |= kHFSHasSecurityMask;
					didRecordChange = true;
				}
			}
		} else {
			/* We have seen attribute for fileID in past */

			/* If last time we saw this fileID did not have an ACL and this 
			 * extended attribute is an ACL, ONLY check consistency of 
			 * security bit from Catalog record 
			 */
			if ((lastID.hasSecurity == false) && !bcmp(attrKey.attrName, GPtr->securityAttrName, GPtr->securityAttrLen)) {
				lastID.hasSecurity = true;
				/* Check the security bit */
				if (!(catRecord.hfsPlusFile.flags & kHFSHasSecurityMask)) {
					/* kHFSHasSecurityBit should be set */
					catRecord.hfsPlusFile.flags |= kHFSHasSecurityMask;
					didRecordChange = true;
				}
			}
		}
		
getnext:
		/* Access the next record */
		err = GetBTreeRecord(GPtr->calculatedAttributesFCB, selCode, &attrKey, &attrRecord, &attrRecordSize, &hint);
	} while (err == noErr);

	err = noErr;

	/* Replace the catalog record for last extended attribute in the attributes BTree
	 * only if we updated the flags
	 */
	if (didRecordChange == true) {
		err = ReplaceBTreeRecord(GPtr->calculatedCatalogFCB, &catKey , kNoHint, &catRecord, catRecordSize, &hint);
		if (err) {
#if DEBUG_XATTR
			printf ("%s: Error in replacing Catalog Record\n", __FUNCTION__);
#endif		
			goto out;
		}
	}

out:
	return err;
}

/* Function:	RepairAttributesCheckCBT
 *
 * Description:
 * This function is called from RepairAttributes (to repair extended 
 * attributes) during repair stage of fsck_hfs.
 *
 * NOTE: The case where attribute exists and bit is not set is being taken care in 
 * RepairAttributesCheckABT.  This function determines relationship from catalog
 * Btree to attribute Btree, and not vice-versa. 

 * 1. Make full pass through catalog BTree.
 * 2. For every fileID, if the attributes/security bit is set, 
 *    lookup all the extended attributes in the attributes BTree.
 * 3. If found, check that if bits are set correctly.
 * 4. If not found, clear the bits.
 * 
 * Input:	1. GPtr - pointer to global scavenger area
 *      	2. isHFSPlus - boolean value to indicate if volume is HFSPlus 
 *
 * Output:	err - Function result 
 * 		      zero means success
 *		      non-zero means failure
 */
static OSErr RepairAttributesCheckCBT(SGlobPtr GPtr, Boolean isHFSPlus)
{
	OSErr err = noErr;
	UInt16 selCode;		/* select access pattern for BTree */
	UInt16 recordSize;
	UInt32 hint;

	HFSPlusAttrKey *attrKey;
	CatalogRecord catRecord; 
	CatalogKey catKey;

	Boolean didRecordChange = false; 	/* whether catalog record was changed after checks */

	BTreeIterator iterator;

	UInt32 curFileID;
	Boolean curRecordHasAttributes = false;
	Boolean curRecordHasSecurity = false;

	selCode = 0x8001;	/* Get first record from BTree */
	err = GetBTreeRecord(GPtr->calculatedCatalogFCB, selCode, &catKey, &catRecord, &recordSize, &hint);
	if ( err != noErr ) goto out;

	selCode = 1;	/* Get next record */
	do {
		/* Check only file record and folder record, else skip to next record */
		if ( (catRecord.hfsPlusFile.recordType != kHFSPlusFileRecord) && 
		     (catRecord.hfsPlusFile.recordType != kHFSPlusFolderRecord)) {
			goto getnext;
		}

		/* Check if catalog record has attribute and/or security bit set, else
		 * skip to next record 
		 */
		if ( ((catRecord.hfsPlusFile.flags & kHFSHasAttributesMask) == 0) && 
		   	 ((catRecord.hfsPlusFile.flags & kHFSHasSecurityMask) == 0) ) {
			 goto getnext;
		}

		/* Initialize some flags */
		didRecordChange = false;
		curRecordHasSecurity = false;
		curRecordHasAttributes = false;

		/* Access all extended attributes for this fileID */ 
		curFileID = catRecord.hfsPlusFile.fileID;

		/* Initialize the iterator and attribute key */
		ClearMemory(&iterator, sizeof(BTreeIterator));
		attrKey = (HFSPlusAttrKey *)&iterator.key;
		attrKey->keyLength = kHFSPlusAttrKeyMinimumLength;
		attrKey->fileID = curFileID;

		/* Search for attribute with NULL name.  This will place the iterator at correct fileID location in BTree */	
		err = BTSearchRecord(GPtr->calculatedAttributesFCB, &iterator, kInvalidMRUCacheKey, NULL, NULL, &iterator);
		if (err && (err != btNotFound)) {
#if DEBUG_XATTR
			printf ("%s: No matching attribute record found\n", __FUNCTION__);
#endif
			goto out;
		}

		/* Iterate over to all extended attributes for given fileID */
		err = BTIterateRecord(GPtr->calculatedAttributesFCB, kBTreeNextRecord, &iterator, NULL, NULL);

		/* Check only if we did _find_ an attribute record for the current fileID */
		while ((err == noErr) && (attrKey->fileID == curFileID)) {
			/* Current record should have attribute bit set */
			curRecordHasAttributes = true;

			/* Current record should have security bit set */
			if (!bcmp(attrKey->attrName, GPtr->securityAttrName, GPtr->securityAttrLen)) {
				curRecordHasSecurity = true;
			}

			/* Get the next record */
			err = BTIterateRecord(GPtr->calculatedAttributesFCB, kBTreeNextRecord, &iterator, NULL, NULL);
		}

		/* Determine if we need to update the catalog record */
		if ((curRecordHasAttributes == false) && (catRecord.hfsPlusFile.flags & kHFSHasAttributesMask)) {
			/* If no attribute exists and attributes bit is set, clear it */
			catRecord.hfsPlusFile.flags &= ~kHFSHasAttributesMask;		
			didRecordChange = true;
		}
				
		if ((curRecordHasSecurity == false) && (catRecord.hfsPlusFile.flags & kHFSHasSecurityMask)) {
			/* If no security attribute exists and security bit is set, clear it */
			catRecord.hfsPlusFile.flags &= ~kHFSHasSecurityMask;		
			didRecordChange = true;
		}
			 
		/* If there was any change in catalog record, write it back to disk */
		if (didRecordChange == true) {
			err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &catKey , kNoHint, &catRecord, recordSize, &hint );
			if (err) {
#if DEBUG_XATTR
				printf ("%s: Error writing catalog record\n", __FUNCTION__);
#endif
				goto out;
			}
		}

getnext:
		/* Access the next record */
		err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &catKey, &catRecord, &recordSize, &hint );
	} while (err == noErr); 

	err = noErr;

out:
	return err;
}

/* Function:	RepairAttributes
 *
 * Description:
 * This function fixes the extended attributes consistency by
 * calling two functions:
 * 1. RepairAttributesCheckABT:  Traverses attributes Btree and
 * checks if each attribute has correct bits set in its corresponding 
 * catalog record.
 * 2. RepairAttributesCheckCBT:  Traverses catalog Btree and checks
 * if each catalog record that has attribute/security bit set have
 * corresponding extended attributes. 
 * 
 * Input:	1. GPtr - pointer to global scavenger area
 *
 * Output:	err - Function result 
 * 		      zero means success
 *		      non-zero means failure
 */
static OSErr RepairAttributes(SGlobPtr GPtr)
{
	OSErr err = noErr;
	Boolean isHFSPlus;
	
	/* Check if the volume is HFS Plus volume */
	isHFSPlus = VolumeObjectIsHFSPlus();
	if (!isHFSPlus) {
		goto out;
	}

	/* Traverse Attributes BTree and access required records in Catalog BTree */
	err = RepairAttributesCheckABT(GPtr, isHFSPlus);
	if (err) {
		goto out;
	}

	/* Traverse Catalog BTree and access required records in Attributes BTree */
	err = RepairAttributesCheckCBT(GPtr, isHFSPlus);
	if (err) {
		goto out;
	}

out:
	return err;
}

/*------------------------------------------------------------------------------

Function:	cmpLongs

Function:	compares two longs.
			
Input:		*a:  pointer to first number
			*b:  pointer to second number

Output:		<0 if *a < *b
			0 if a == b
			>0 if a > b
------------------------------------------------------------------------------*/

int cmpLongs ( const void *a, const void *b )
{
	return( *(long*)a - *(long*)b );
}

/* Function: FixOverlappingExtents
 *
 * Description: Fix overlapping extents problem.  The implementation copies all
 * the extents existing in overlapping extents to a new location and updates the
 * extent record to point to the new extent.  At the end of repair, symlinks are
 * created with name "fileID filename" to point to the file involved in
 * overlapping extents problem.  Note that currently only HFS Plus volumes are 
 * repaired.
 *
 * PARTIAL SUCCESS: This function handles partial success in the following
 * two ways:
 *		a. The function pre-allocates space for the all the extents.  If the
 *		allocation fails, it proceeds to allocate space for other extents
 *		instead of returning error.  
 *		b. If moving an extent succeeds and symlink creation fails, the function
 *		proceeds to another extent.
 * If the repair encounters either a or b condition, appropriate message is
 * printed at the end of the function.  
 * If even a single repair operation succeeds (moving of extent), the function 
 * returns zero.
 *
 * Current limitations:
 *	1. A regular file instead of symlink is created under following conditions:
 *		a. The volume is plain HFS volume.  HFS does not support symlink
 *		creation.
 *		b. The path the new symlink points to is greater than PATH_MAX bytes.
 *		c. The path the new symlink points has some intermediate component
 *		greater than NAME_MAX bytes.
 *	2. Contiguous disk space for every new extent is expected.  The extent is
 *	not broken into multiple extents if contiguous space is not available on the
 *	disk.
 *	3. The current fix for overlapping extent only works for HFS Plus volumes. 
 *  Plain HFS volumes have problem in accessing the catalog record by fileID.
 *	4. Plain HFS volumes might have encoding issues with the newly created
 *	symlink and its data.
 *
 * Input:
 *	GPtr - global scavenger pointer
 *
 * Output:
 *	returns zero on success/partial success (moving of one extent succeeds),
 *			non-zero on failure.
 */
static OSErr FixOverlappingExtents(SGlobPtr GPtr)
{
	OSErr err = noErr;
	Boolean isHFSPlus;
	unsigned int i;
	unsigned int numOverlapExtents = 0;
	UInt32 newBlockCount;
	UInt32 damagedDirID = 0;
	ExtentInfo *extentInfo;
	ExtentsTable **extentsTableH = GPtr->overlappedExtents;

	unsigned int status = 0;
#define S_DISKFULL			0x01	/* error due to disk full */
#define S_SYMCREATE			0x02	/* error in creating symlinks/damaged files dir */
#define S_LOOKDAMAGEDDIR	0x04	/* look for links to file in damamged dir */
#define	S_MOVEEXTENT		0x08	/* moving extent succeeded */

	isHFSPlus = VolumeObjectIsHFSPlus();
	if (isHFSPlus == false) {
		/* Do not repair plain HFS volumes */
		err = R_RFail;
		goto out;
	}

	if (extentsTableH == NULL) {
		/* No overlapping extents exist */
		goto out;
	}

	numOverlapExtents = (**extentsTableH).count;

	/* Optimization - sort the overlap extent list based on blockCount to 
	 * start allocating contiguous space for largest number of blocks first
	 */
	qsort((**extentsTableH).extentInfo, numOverlapExtents, sizeof(ExtentInfo), 
		  CompareExtentBlockCount);

#if DEBUG_OVERLAP
	/* Print all overlapping extents structure */
	for (i=0; i<numOverlapExtents; i++) {
		extentInfo	= &((**extentsTableH).extentInfo[i]);
		printf ("%d: fileID = %d, startBlock = %d, blockCount = %d\n", i, extentInfo->fileID, extentInfo->startBlock, extentInfo->blockCount);
	}
#endif

	/* Update the on-disk volume bitmap with in-memory volume bitmap */
	err = UpdateVolumeBitMap(GPtr, false);
	if (err != noErr) {
#if DEBUG_OVERLAP
		printf ("%s: Error in updating on-disk bitmap\n", __FUNCTION__);
#endif
		goto out;
	}

	/* Pre-allocate free space for all overlapping extents */
	for (i=0; i<numOverlapExtents; i++) {
		extentInfo	= &((**extentsTableH).extentInfo[i]);
		err = BlockAllocate(GPtr->calculatedVCB, 0, extentInfo->blockCount, 
							extentInfo->blockCount, true, 
							&(extentInfo->newStartBlock), &newBlockCount);
		if ((err != noErr) && (newBlockCount != extentInfo->blockCount)) {
			/* Not enough disk space */
			status |= S_DISKFULL;
#if DEBUG_OVERLAP
			printf ("%s: Not enough disk space to allocate extent for fileID = %d (start=%d, count=%d)\n", __FUNCTION__, extentInfo->fileID, extentInfo->startBlock, extentInfo->blockCount);
#endif
		}
	}

	/* Lookup or create damagedFiles directory */
	damagedDirID = CreateDirByName(GPtr, (u_char *)"DamagedFiles", kHFSRootFolderID);
	if (damagedDirID == 0) {
		status |= S_SYMCREATE;
#if DEBUG_OVERLAP
		printf ("%s: Error in create/lookup for Damaged Files directory\n", __FUNCTION__);
#endif
	}

	/* For every extent info, copy the extent into new location and create symlink */
	for (i=0; i<numOverlapExtents; i++) {
		extentInfo	= &((**extentsTableH).extentInfo[i]);

		/* Do not repair this extent as no new extent was allocated */
		if (extentInfo->newStartBlock == 0) {
			continue;
		}

		/* Move extent data to new location */
		err	= MoveExtent(GPtr, extentInfo);
		if (err != noErr) {
			extentInfo->hasRepair = false;
#if DEBUG_OVERLAP
			printf ("%s: Extent move failed for extent for fileID = %d (old=%d, new=%d, count=%d) (err=%d)\n", __FUNCTION__, extentInfo->fileID, extentInfo->startBlock, extentInfo->newStartBlock, extentInfo->blockCount, err);
#endif
		} else {
			/* Mark the overlapping extent as repaired */
			extentInfo->hasRepair = true;
			status |= S_MOVEEXTENT;
#if DEBUG_OVERLAP
			printf ("%s: Extent move success for extent for fileID = %d (old=%d, new=%d, count=%d)\n", __FUNCTION__, extentInfo->fileID, extentInfo->startBlock, extentInfo->newStartBlock, extentInfo->blockCount);
#endif
		}

		/* Create symlink for the corrupt file */
		if (damagedDirID != 0) {
			err = CreateCorruptFileSymlink(GPtr, damagedDirID, extentInfo);
			if (err != noErr) {
				status |= S_SYMCREATE;
#if DEBUG_OVERLAP
				printf ("%s: Error in creating symlink for fileID = %d (err=%d)\n", __FUNCTION__, extentInfo->fileID, err);
#endif
			} else {
				status |= S_LOOKDAMAGEDDIR;	
#if DEBUG_OVERLAP
				printf ("%s: Created symlink for fileID = %d (old=%d, new=%d, count=%d)\n", __FUNCTION__, extentInfo->fileID, extentInfo->startBlock, extentInfo->newStartBlock, extentInfo->blockCount);
#endif
			}
		}
	}

out:
	/* Release all blocks used by overlap extents that are repaired */
	for (i=0; i<numOverlapExtents; i++) {
		extentInfo	= &((**extentsTableH).extentInfo[i]);
		if (extentInfo->hasRepair == true) {
			ReleaseBitmapBits (extentInfo->startBlock, extentInfo->blockCount);
		}
	}

	/* For all un-repaired extents, 
	 *	1. Release all blocks allocated for new extent.
	 *	2. Mark all blocks used for the old extent (since the overlapping region
	 *	have been marked free in the for loop above.
	 */
	for (i=0; i<numOverlapExtents; i++) {
		extentInfo	= &((**extentsTableH).extentInfo[i]);
		if (extentInfo->hasRepair == false) {
			CaptureBitmapBits (extentInfo->startBlock, extentInfo->blockCount);

			if (extentInfo->newStartBlock != 0) {
				ReleaseBitmapBits (extentInfo->newStartBlock, extentInfo->blockCount);
			}
		}
	}

	/* Update the volume free count since the release and alloc above might 
	 * have worked on single bit multiple times 
	 */
	UpdateFreeBlockCount (GPtr);
	
	/* Print correct status messages */
	if (status & S_DISKFULL) {
		PrintError (GPtr, E_DiskFull, 0);
	} 
	if (status & S_SYMCREATE) {
		PrintError (GPtr, E_SymlinkCreate, 0);
	}
	if (status & S_LOOKDAMAGEDDIR) {
		PrintStatus (GPtr, M_LookDamagedDir, 0);
	}

	/* If moving of even one extent succeeds, return success */
	if (status & S_MOVEEXTENT) {
		err = noErr;
	}

	return err;
} /* FixOverlappingExtents */

/* Function: CompareExtentBlockCount
 *
 * Description: Compares the blockCount from two ExtentInfo and return the
 * comparison result. (since we have to arrange in descending order)
 *
 * Input:
 *	first and second - void pointers to ExtentInfo structure.
 *
 * Output:
 *	<0 if first > second
 * 	=0 if first == second
 *	>0 if first < second
 */
static int CompareExtentBlockCount(const void *first, const void *second)
{
	return (((ExtentInfo *)second)->blockCount - 
			((ExtentInfo *)first)->blockCount);
} /* CompareExtentBlockCount */

/* Function: MoveExtent
 *
 * Description: Move data from old extent to new extent and update corresponding
 * records.
 * 1. Search the extent record for the overlapping extent.
 *		If the fileID < kHFSFirstUserCatalogNodeID, 
 *			Ignore repair for BadBlock, RepairCatalog, BogusExtent files.
 *			Search for extent record in volume header. 
 *		Else, 
 *			Search for extent record in catalog BTree.  If the extent list does
 *			not end in catalog record and extent record not found in catalog
 *			record, search in extents BTree.
 * 2. If found, copy disk blocks from old extent to new extent.  
 * 3. If it succeeds, update extent record with new start block and write back
 *    to disk.
 * This function does not take care to deallocate blocks from old start block.
 *
 * Input: 
 *	GPtr - Global Scavenger structure pointer
 *  extentInfo - Current overlapping extent details.
 *
 * Output:
 * 	err: zero on success, non-zero on failure
 *		paramErr - Invalid paramter, ex. file ID is less than
 *		kHFSFirstUserCatalogNodeID.  
 */
static OSErr MoveExtent(SGlobPtr GPtr, ExtentInfo *extentInfo)
{
	OSErr err = noErr;
	Boolean isHFSPlus;

	CatalogRecord catRecord;
	CatalogKey catKey;
	HFSPlusExtentKey extentKey;
	HFSPlusExtentRecord extentData;
	UInt16 recordSize;

	UInt16 foundLocation;
#define CATALOG_BTREE	1
#define VOLUMEHEADER	2
#define	EXTENTS_BTREE	3

	UInt32 foundExtentIndex = 0;
	Boolean noMoreExtents = true;
	
	isHFSPlus = VolumeObjectIsHFSPlus();

	/* Find correct location of this overlapping extent */
	if (extentInfo->fileID < kHFSFirstUserCatalogNodeID) {
		/* Ignore these fileIDs */
		if ((extentInfo->fileID == kHFSBadBlockFileID) ||
			(extentInfo->fileID == kHFSRepairCatalogFileID) ||
			(extentInfo->fileID == kHFSBogusExtentFileID)) {
#if DEBUG_OVERLAP
			printf ("%s: Ignoring repair for fileID = %d\n",	__FUNCTION__, extentInfo->fileID);
#endif
			err = paramErr;
			goto out;
		}

		/* Search for extent record in the volume header */
		err = SearchExtentInVH (GPtr, extentInfo, &foundExtentIndex, &noMoreExtents);
		foundLocation = VOLUMEHEADER;
	} else {
		/* Search the extent record from the catalog btree */
		err = SearchExtentInCatalogBT (GPtr, extentInfo, &catKey, &catRecord, 
									  &recordSize, &foundExtentIndex, &noMoreExtents);
		foundLocation = CATALOG_BTREE;
	}
	if (err != noErr) {
		if (noMoreExtents == false) { 
			/* search extent in extents overflow btree */
			err = SearchExtentInExtentBT (GPtr, extentInfo, &extentKey, 
										  &extentData, &recordSize, &foundExtentIndex);
			foundLocation = EXTENTS_BTREE;
			if (err != noErr) {
#if DEBUG_OVERLAP
				printf ("%s: No matching extent record found in extents btree for fileID = %d (err=%d)\n", __FUNCTION__, extentInfo->fileID, err);
#endif
				goto out;
			}
		} else {
			/* No more extents exist for this file */
#if DEBUG_OVERLAP
			printf ("%s: No matching extent record found for fileID = %d (err=%d)\n",	__FUNCTION__, extentInfo->fileID, err);
#endif
			goto out;
		}
	}

	/* Copy disk blocks from old extent to new extent */
	err = CopyDiskBlocks(GPtr, extentInfo->startBlock, extentInfo->blockCount, 
						 extentInfo->newStartBlock);
	if (err != noErr) {
#if DEBUG_OVERLAP
		printf ("%s: Error in copying disk blocks for fileID = %d (err=%d)\n", __FUNCTION__, extentInfo->fileID, err);
#endif
		goto out;
	}
	
	/* Replace the old start block in extent record with new start block */
	if (foundLocation == CATALOG_BTREE) {
		err = UpdateExtentInCatalogBT(GPtr, extentInfo, &catKey, &catRecord, 
									  &recordSize, foundExtentIndex);
	} else if (foundLocation == VOLUMEHEADER) {
		err = UpdateExtentInVH(GPtr, extentInfo, foundExtentIndex);
	} else if (foundLocation == EXTENTS_BTREE) {
		extentData[foundExtentIndex].startBlock = extentInfo->newStartBlock;
		err = UpdateExtentRecord(GPtr->calculatedVCB, NULL, &extentKey, extentData, kNoHint);
	}
	if (err != noErr) {
#if DEBUG_OVERLAP
		printf ("%s: Error in updating extent record (err=%d)\n", __FUNCTION__, err);
#endif
		goto out;
	}

out:
	return err;
} /* MoveExtent */

/* Function: CreateCorruptFileSymlink
 *
 * Description: Create symlink to point to the corrupt files that had
 * overlapping extents.
 * 
 * If fileID >= kHFSFirstUserCatalogNodeID,
 *	Lookup the filename and path to the file based on file ID.  Create the
 *	new file name as "fileID filename" and data as the relative path of the file
 *	from the root of the volume.
 *	If either
 *		the volume is plain HFS, or
 *		the length of the path pointed by data is greater than PATH_MAX, or
 *		the length of any intermediate path component is greater than NAME_MAX,
 *		Create a plain file with given data.
 *	Else
 *		Create a symlink.
 * Else	
 *	Find the name of file based on ID (ie. Catalog BTree, etc), and create plain
 *	regular file with name "fileID filename" and data as "System File:
 *	filename".
 *
 * Input:
 *	1. GPtr - global scavenger structure pointer.
 *	2. targetParentID - parent directory where symlink should be created.
 * 	3. extentInfo - details about overlapping extent for creating symlink
 *
 * Output:
 * 	returns zero on success, non-zero on failure.
 *		memFullErr - Not enough memory
 */
static OSErr CreateCorruptFileSymlink(SGlobPtr GPtr, UInt32 targetParentID, ExtentInfo *extentInfo)
{
	OSErr err = noErr;
	Boolean isHFSPlus;
	char *filename = NULL;
	unsigned int filenamelen; 
	char *data = NULL;
	unsigned int datalen;
	unsigned int filenameoffset;
	unsigned int dataoffset;
	UInt16 status;
	UInt16 fileType;
	
	isHFSPlus = VolumeObjectIsHFSPlus();

	/* Allocate (kHFSPlusMaxFileNameChars * 4) for unicode - utf8 conversion */
	filenamelen = kHFSPlusMaxFileNameChars * 4;
	filename = malloc(filenamelen);
	if (!filename) {
		err = memFullErr;
		goto out;
	}

	/* Allocate (PATH_MAX * 4) instead of PATH_MAX for unicode - utf8 conversion */
	datalen = PATH_MAX * 4;
	data = malloc(datalen);
	if (!data) {
		err = memFullErr;
		goto out;
	}

	/* Find filename, path for fileID >= 16 and determine new fileType */
	if (extentInfo->fileID >= kHFSFirstUserCatalogNodeID) {
		char *name;
		char *path;

		/* construct symlink data with .. prefix */
		dataoffset = sprintf (data, "..");
		path = data + dataoffset;
		datalen -= dataoffset;

		/* construct new filename prefix with fileID<space> */
		filenameoffset = sprintf (filename, "%08x ", extentInfo->fileID);
		name = filename + filenameoffset;
		filenamelen -= filenameoffset;

		/* find file name and path (data for symlink) for fileID */
		err = GetFileNamePathByID(GPtr, extentInfo->fileID, path, &datalen, 
								  name, &filenamelen, &status);
		if (err != noErr) {
#if DEBUG_OVERLAP
			printf ("%s: Error in getting name/path for fileID = %d (err=%d)\n", __FUNCTION__, extentInfo->fileID, err);
#endif
			goto out;
		}

		/* update length of path and filename */
		filenamelen += filenameoffset;
		datalen += dataoffset;

		/* If 
		 * (1) the volume is plain HFS volume, or
		 * (2) any intermediate component in path was more than NAME_MAX bytes, or
		 * (3) the entire path was greater than PATH_MAX bytes 
		 * then create regular file
		 * else create symlink.
		 */
		if (!isHFSPlus || (status & FPATH_BIGNAME) || (datalen > PATH_MAX)) {
			/* create file */
			fileType = S_IFREG;
		} else {
			/* create symlink */
			fileType = S_IFLNK;
		}
	} else {
		/* for fileID < 16, create regular file */
		fileType = S_IFREG;

		/* construct the name of the file */
		filenameoffset = sprintf (filename, "%08x ", extentInfo->fileID);
		filenamelen -= filenameoffset;
		err = GetSystemFileName (extentInfo->fileID, (filename + filenameoffset), &filenamelen);
		filenamelen += filenameoffset;
		
		/* construct the data of the file */
		dataoffset = sprintf (data, "System File: ");
		datalen -= dataoffset;
		err = GetSystemFileName (extentInfo->fileID, (data + dataoffset), &datalen);
		datalen += dataoffset;
	}

	/* Create new file */
	err = CreateFileByName (GPtr, targetParentID, fileType, (u_char *)filename, 
							filenamelen, (u_char *)data, datalen);
	/* Mask error if file already exists */
	if (err == EEXIST) {
		err = noErr;
	}
	if (err != noErr) {
#if DEBUG_OVERLAP
		printf ("%s: Error in creating fileType = %d for fileID = %d (err=%d)\n", __FUNCTION__, fileType, extentInfo->fileID, err);	
#endif
		goto out;
	}

out:
	if (data) {
		free (data);
	}
	if (filename) {
		free (filename);
	}

	return err;
} /* CreateCorruptFileSymlink */

/* Function: SearchExtentInVH
 *	
 * Description: Search extent with given information (fileID, startBlock,
 * blockCount) in volume header.  
 *
 * Input:
 *	1. GPtr - global scavenger structure pointer.
 *	2. extentInfo - Information about extent to be searched.
 *
 * Output:
 *	Returns zero on success, fnfErr on failure.
 *	1. *foundExtentIndex - Index in extent record which matches the input data. 
 *	2. *noMoreExtents - Indicates that no more extents will exist for this
 *	fileID in extents BTree.
 */
static OSErr SearchExtentInVH(SGlobPtr GPtr, ExtentInfo *extentInfo, UInt32 *foundExtentIndex, Boolean *noMoreExtents)
{
	OSErr err = fnfErr;
	Boolean isHFSPlus;
	SFCB *fcb = NULL;

	isHFSPlus = VolumeObjectIsHFSPlus();
	*noMoreExtents = true;
	
	/* Find correct FCB structure */
	switch (extentInfo->fileID) {
		case kHFSExtentsFileID:
			fcb = GPtr->calculatedVCB->vcbExtentsFile;
			break;

		case kHFSCatalogFileID:
			fcb = GPtr->calculatedVCB->vcbCatalogFile;
			break;

		case kHFSAllocationFileID:
			fcb = GPtr->calculatedVCB->vcbAllocationFile;
			break;

		case kHFSStartupFileID:
			fcb = GPtr->calculatedVCB->vcbStartupFile;
			break;
			
		case kHFSAttributesFileID:
			fcb = GPtr->calculatedVCB->vcbAttributesFile;
			break;
	};

	/* If extent found, find correct extent index */
	if (fcb != NULL) {
		if (isHFSPlus) {
			err = FindExtentInExtentRec(isHFSPlus, extentInfo->startBlock, 
										extentInfo->blockCount, fcb->fcbExtents32, 
										foundExtentIndex, noMoreExtents);
		} else {
			err = FindExtentInExtentRec(isHFSPlus, extentInfo->startBlock, 
										extentInfo->blockCount, 
										(*(HFSPlusExtentRecord *)fcb->fcbExtents16),
										foundExtentIndex, noMoreExtents);
		}
	}
	return err;
} /* SearchExtentInVH */

/* Function: UpdateExtentInVH
 *
 * Description:	Update the extent record for given fileID and index in the
 * volume header with new start block.  
 *
 * Input:
 *	1. GPtr - global scavenger structure pointer.
 *	2. extentInfo - Information about extent to be searched.
 *	3. foundExtentIndex - Index in extent record to update.
 *
 * Output:
 *	Returns zero on success, fnfErr on failure.  This function will fail an
 *	incorrect fileID is passed.
 */
static OSErr UpdateExtentInVH (SGlobPtr GPtr, ExtentInfo *extentInfo, UInt32 foundExtentIndex)
{
	OSErr err = fnfErr;
	Boolean isHFSPlus;
	SFCB *fcb = NULL;

	isHFSPlus = VolumeObjectIsHFSPlus();
	
	/* Find correct FCB structure */
	switch (extentInfo->fileID) {
		case kHFSExtentsFileID:
			fcb = GPtr->calculatedVCB->vcbExtentsFile;
			break;

		case kHFSCatalogFileID:
			fcb = GPtr->calculatedVCB->vcbCatalogFile;
			break;

		case kHFSAllocationFileID:
			fcb = GPtr->calculatedVCB->vcbAllocationFile;
			break;

		case kHFSStartupFileID:
			fcb = GPtr->calculatedVCB->vcbStartupFile;
			break;
			
		case kHFSAttributesFileID:
			fcb = GPtr->calculatedVCB->vcbAttributesFile;
			break;
	};

	/* If extent found, find correct extent index */
	if (fcb != NULL) {
		if (isHFSPlus) {
			fcb->fcbExtents32[foundExtentIndex].startBlock = extentInfo->newStartBlock;
		} else {
			fcb->fcbExtents16[foundExtentIndex].startBlock = extentInfo->newStartBlock;
		}
		MarkVCBDirty(GPtr->calculatedVCB);
		err = noErr;
	}
	return err;
} /* UpdateExtentInVH */

/* Function: SearchExtentInCatalogBT
 *	
 * Description: Search extent with given information (fileID, startBlock,
 * blockCount) in catalog BTree.
 *
 * Input:
 *	1. GPtr - global scavenger structure pointer.
 *	2. extentInfo - Information about extent to be searched.
 *
 * Output:
 *	Returns zero on success, non-zero on failure.
 *	1. *catKey - Catalog key for given fileID, if found.
 *	2. *catRecord - Catalog record for given fileID, if found.
 *	3. *recordSize - Size of the record being returned.
 *	4. *foundExtentIndex - Index in extent record which matches the input data. 
 *	5. *noMoreExtents - Indicates that no more extents will exist for this
 *	fileID in extents BTree.
 */
static OSErr SearchExtentInCatalogBT(SGlobPtr GPtr, ExtentInfo *extentInfo, CatalogKey *catKey, CatalogRecord *catRecord, UInt16 *recordSize, UInt32 *foundExtentIndex, Boolean *noMoreExtents)
{							 
	OSErr err;
	Boolean isHFSPlus;
	
	isHFSPlus = VolumeObjectIsHFSPlus();

	/* Search catalog btree for this file ID */
	err = GetCatalogRecord(GPtr, extentInfo->fileID, isHFSPlus, catKey, catRecord, 
						   recordSize);
	if (err != noErr) {
#if DEBUG_OVERLAP
		printf ("%s: No matching catalog record found for fileID = %d (err=%d)\n", __FUNCTION__, extentInfo->fileID, err);
#endif
		goto out;
	}

	if (isHFSPlus) {
		/* HFS Plus */
		if (extentInfo->forkType == kDataFork) {	
			/* data fork */
			err = FindExtentInExtentRec(isHFSPlus, extentInfo->startBlock, 
										extentInfo->blockCount, 
										catRecord->hfsPlusFile.dataFork.extents, 
										foundExtentIndex, noMoreExtents);
		} else { 
			/* resource fork */
			err = FindExtentInExtentRec(isHFSPlus, extentInfo->startBlock, 
										extentInfo->blockCount, 
										catRecord->hfsPlusFile.resourceFork.extents, 
										foundExtentIndex, noMoreExtents);
		}
	} else {
		/* HFS */
		if (extentInfo->forkType == kDataFork) {	
			/* data fork */
			err = FindExtentInExtentRec(isHFSPlus, extentInfo->startBlock, 
										extentInfo->blockCount, 
										(*(HFSPlusExtentRecord *)catRecord->hfsFile.dataExtents),
										foundExtentIndex, noMoreExtents);
		} else { 
			/* resource fork */
			err = FindExtentInExtentRec(isHFSPlus, extentInfo->startBlock, 
										extentInfo->blockCount, 
										(*(HFSPlusExtentRecord *)catRecord->hfsFile.rsrcExtents),
										foundExtentIndex, noMoreExtents);
		}
	}

out:
	return err;
} /* SearchExtentInCatalogBT */

/* Function: UpdateExtentInCatalogBT
 *	
 * Description: Update extent record with given information (fileID, startBlock,
 * blockCount) in catalog BTree.
 *
 * Input:
 *	1. GPtr - global scavenger structure pointer.
 *	2. extentInfo - Information about extent to be searched.
 *	3. *catKey - Catalog key for record to update.
 *	4. *catRecord - Catalog record to update.
 *	5. *recordSize - Size of the record.
 *	6. foundExtentIndex - Index in extent record to update.
 *
 * Output:
 *	Returns zero on success, non-zero on failure.
 */
static OSErr UpdateExtentInCatalogBT (SGlobPtr GPtr, ExtentInfo *extentInfo, CatalogKey *catKey, CatalogRecord *catRecord, UInt16 *recordSize, UInt32 foundInExtentIndex)
{
	OSErr err;
	Boolean isHFSPlus;
	UInt32 foundHint;
	
	isHFSPlus = VolumeObjectIsHFSPlus();

	/* Modify the catalog record */
	if (isHFSPlus) {
		if (extentInfo->forkType == kDataFork) {
			catRecord->hfsPlusFile.dataFork.extents[foundInExtentIndex].startBlock = extentInfo->newStartBlock;
		} else {
			catRecord->hfsPlusFile.resourceFork.extents[foundInExtentIndex].startBlock = extentInfo->newStartBlock;
		}
	} else {
		if (extentInfo->forkType == kDataFork) {
			catRecord->hfsFile.dataExtents[foundInExtentIndex].startBlock = extentInfo->newStartBlock;
		} else {
			catRecord->hfsFile.rsrcExtents[foundInExtentIndex].startBlock = extentInfo->newStartBlock;
		}
	}

	/* Replace the catalog record */
	err = ReplaceBTreeRecord (GPtr->calculatedCatalogFCB, catKey, kNoHint, 
							  catRecord, *recordSize, &foundHint);
	if (err != noErr) {
#if DEBUG_OVERLAP
		printf ("%s: Error in replacing catalog record for fileID = %d (err=%d)\n", __FUNCTION__, extentInfo->fileID, err);
#endif
	}
	return err;
} /* UpdateExtentInCatalogBT */

/* Function: SearchExtentInExtentBT
 *	
 * Description: Search extent with given information (fileID, startBlock,
 * blockCount) in Extent BTree.
 *
 * Input:
 *	1. GPtr - global scavenger structure pointer.
 *	2. extentInfo - Information about extent to be searched.
 *
 * Output:
 *	Returns zero on success, non-zero on failure.
 *		fnfErr - desired extent record was not found.
 *	1. *extentKey - Extent key, if found.
 *	2. *extentRecord - Extent record, if found.
 *	3. *recordSize - Size of the record being returned.
 *	4. *foundExtentIndex - Index in extent record which matches the input data. 
 */
static OSErr SearchExtentInExtentBT(SGlobPtr GPtr, ExtentInfo *extentInfo, HFSPlusExtentKey *extentKey, HFSPlusExtentRecord *extentRecord, UInt16 *recordSize, UInt32 *foundExtentIndex) 
{
	OSErr err = noErr;
	Boolean isHFSPlus;
	Boolean noMoreExtents = true;
	UInt32 hint;

	isHFSPlus = VolumeObjectIsHFSPlus();

	/* set up extent key */
	BuildExtentKey (isHFSPlus, extentInfo->forkType, extentInfo->fileID, 0, extentKey);
	err = SearchBTreeRecord (GPtr->calculatedExtentsFCB, extentKey, kNoHint, 
							 extentKey, extentRecord, recordSize, &hint);
	if ((err != noErr) && (err != btNotFound)) {
#if DEBUG_OVERLAP
		printf ("%s: Error on searching first record for fileID = %d in Extents Btree (err=%d)\n", __FUNCTION__, extentInfo->fileID, err);
#endif
		goto out;
	}
	
	if (err == btNotFound)
	{
		/* Position to the first record for the given fileID */
		err = GetBTreeRecord (GPtr->calculatedExtentsFCB, 1, extentKey, 
							  extentRecord, recordSize, &hint);
	}
	
	while (err == noErr)
	{
		/* Break out if we see different fileID, forkType in the BTree */
		if (isHFSPlus) {
			if ((extentKey->fileID != extentInfo->fileID) || 
				(extentKey->forkType != extentInfo->forkType)) {
				err = fnfErr;
				break;
			}
		} else {
			if ((((HFSExtentKey *)extentKey)->fileID != extentInfo->fileID) || 
				(((HFSExtentKey *)extentKey)->forkType != extentInfo->forkType)) {
				err = fnfErr;
				break;
			}
		}

		/* Check the extents record for corresponding startBlock, blockCount */
		err = FindExtentInExtentRec(isHFSPlus, extentInfo->startBlock, 
									extentInfo->blockCount, *extentRecord,
									foundExtentIndex, &noMoreExtents);
		if (err == noErr) {
			break;
		}
		if (noMoreExtents == true) {
			err = fnfErr;
			break;
		}

		/* Try next record for this fileID and forkType */
		err = GetBTreeRecord (GPtr->calculatedExtentsFCB, 1, extentKey, 
							  extentRecord, recordSize, &hint);
	}
	
out:
	return err;
} /* SearchExtentInExtentBT */

/* Function: FindExtentInExtentRec
 *
 * Description: Traverse the given extent record (size based on if the volume is
 * HFS or HFSPlus volume) and find the index location if the given startBlock
 * and blockCount match.
 *
 * Input:
 *	1. isHFSPlus - If the volume is plain HFS or HFS Plus.
 *	2. startBlock - startBlock to be searched in extent record.
 *	3. blockCount - blockCount to be searched in extent record.
 *	4. extentData - Extent Record to be searched.
 * 
 * Output:
 *	Returns zero if the match is found, else fnfErr on failure.
 *	1. *foundExtentIndex - Index in extent record which matches the input data. 
 *	2. *noMoreExtents - Indicates that no more extents exist after this extent
 *	record.
 */
static OSErr FindExtentInExtentRec (Boolean isHFSPlus, UInt32 startBlock, UInt32 blockCount, const HFSPlusExtentRecord extentData, UInt32 *foundExtentIndex, Boolean *noMoreExtents)
{
	OSErr err = noErr;
	UInt32 numOfExtents;
	Boolean foundExtent;
	int i;

	foundExtent = false;
	*noMoreExtents = false;
	*foundExtentIndex = 0;

	if (isHFSPlus) {
		numOfExtents = kHFSPlusExtentDensity;
	} else {
		numOfExtents = kHFSExtentDensity;
	}
	
	for (i=0; i<numOfExtents; i++) {
		if (extentData[i].blockCount == 0) {
			/* no more extents left to check */
			*noMoreExtents = true;
			break;
		}
		if ((startBlock == extentData[i].startBlock) &&
			(blockCount == extentData[i].blockCount)) {
			foundExtent = true;
			*foundExtentIndex = i;
			break;
		}
	}

	if (foundExtent == false) {
		err = fnfErr;
	}

	return err;
} /* FindExtentInExtentRec */

/* Function: GetSystemFileName
 *
 * Description: Return the name of the system file based on fileID
 *
 * Input:
 *	1. fileID - fileID whose name is to be returned.
 *	2. *filenamelen - length of filename buffer.
 *
 * Output:
 *	1. *filename - filename, is limited by the length of filename buffer passed
 *	in *filenamelen.
 *	2. *filenamelen - length of the filename
 *	Always returns zero.
 */
OSErr GetSystemFileName(UInt32 fileID, char *filename, unsigned int *filenamelen)
{
	OSErr err = noErr;
	unsigned int len;

	if (filename) {
		len = *filenamelen - 1;
		switch (fileID) {
			case kHFSExtentsFileID:
				strncpy (filename, "Extents Overflow BTree", len);
				break;

			case kHFSCatalogFileID:
				strncpy (filename, "Catalog BTree", len); 
				break;

			case kHFSAllocationFileID:
				strncpy (filename, "Allocation File", len); 
				break;

			case kHFSStartupFileID:
				strncpy (filename, "Startup File", len); 
				break;
			
			case kHFSAttributesFileID:
				strncpy (filename, "Attributes BTree", len); 
				break;

			case kHFSBadBlockFileID:
				strncpy (filename, "Bad Allocation File", len); 
				break;

			case kHFSRepairCatalogFileID:
				strncpy (filename, "Repair Catalog File", len); 
				break;

			case kHFSBogusExtentFileID:
				strncpy (filename, "Bogus Extents File", len); 
				break;

			default:
				strncpy (filename, "Unknown File", len); 
				break;
		};
		filename[len] = '\0';
		*filenamelen = strlen (filename);
	}
	return err;
}

/* structure to store the intermediate path components during BTree traversal.
 * This is used as a LIFO linked list 
 */
struct fsPathString
{
	char *name;
	unsigned int namelen;
	struct fsPathString *childPtr;
};

/* Function: GetFileNamePathByID 
 *
 * Description: Return the file/directory name and/or full path by ID.  The
 * length of the strings returned is limited by string lengths passed as
 * parameters. 
 * The function lookups catalog thread record for given fileID and its parents
 * until it reaches the Root Folder.
 *
 * Note:
 * 	1. The path returned currently does not return mangled names. 
 *	2. Either one or both of fullPath and fileName can be NULL.  
 *	3. fullPath and fileName are returned as NULL-terminated UTF8 strings. 
 *	4. Returns error if fileID < kHFSFirstUserCatalogID.
 *
 * Input:
 *	1. GPtr - global scavenger structure pointer
 *	2. fileID - fileID for the target file/directory for finding the path
 *	3. fullPathLen - size of array to return full path
 *	4. fileNameLen - size of array to return file name
 *
 * Output:
 *	Return value: zero on success, non-zero on failure
 *		memFullErr - Not enough memory
 *		paramErr - Invalid paramter
 *
 *	The data in *fileNameLen and *fullPathLen is undefined on error.
 *
 *	1. fullPath - If fullPath is non-NULL, full path of file/directory is
 *	returned (size limited by fullPathLen)
 *	2. *fullPathLen - length of fullPath returned.
 *	3. fileName - If fileName is non-NULL, file name of fileID is returned (size
 *	limited by fileNameLen).
 *	4. *fileNameLen - length of fileName returned.
 *	5. *status - status of operation, any of the following bits can be set
 *	(defined in dfalib/Scavenger.h).
 *		FNAME_BUF2SMALL	- filename buffer is too small.
 *		FNAME_BIGNAME	- filename is more than NAME_MAX bytes.
 *		FPATH_BUF2SMALL	- path buffer is too small.
 *		FPATH_BIGNAME	- one or more intermediate path component is greater
 *						  than NAME_MAX bytes.
 *		F_RESERVE_FILEID- fileID is less than kHFSFirstUserCatalogNodeID.
 */
OSErr GetFileNamePathByID(SGlobPtr GPtr, UInt32 fileID, char *fullPath, unsigned int *fullPathLen, char *fileName, unsigned int *fileNameLen, u_int16_t *status)
{
	OSErr err = noErr;
	Boolean isHFSPlus;
	UInt16 recordSize;
	UInt16 curStatus = 0;
	UInt32 hint;
	CatalogKey catKey;
	CatalogRecord catRecord;
	struct fsPathString *listHeadPtr = NULL;
	struct fsPathString *listTailPtr = NULL;
	struct fsPathString *curPtr = NULL;
	u_char *filename = NULL;
	size_t namelen;

	if (!fullPath && !fileName) {
		goto out;
	}

	if (fileID < kHFSFirstUserCatalogNodeID) {
		curStatus = F_RESERVE_FILEID;
		err = paramErr;
		goto out;
	}

	isHFSPlus = VolumeObjectIsHFSPlus();

	if (isHFSPlus) {
		filename = malloc(kHFSPlusMaxFileNameChars * 3 + 1);
	} else {
		filename = malloc(kHFSMaxFileNameChars + 1);
	}
	if (!filename) {
		err = memFullErr;
#if DEBUG_OVERLAP
		printf ("%s: Not enough memory (err=%d)\n", __FUNCTION__, err);
#endif
		goto out;
	}

	while (fileID != kHFSRootFolderID) {
		/* lookup for thread record for this fileID */
		BuildCatalogKey(fileID, NULL, isHFSPlus, &catKey);
		err = SearchBTreeRecord(GPtr->calculatedCatalogFCB, &catKey, kNoHint,
								&catKey, &catRecord, &recordSize, &hint);
		if (err) {
#if DEBUG_OVERLAP
			printf ("%s: Error finding thread record for fileID = %d (err=%d)\n", __FUNCTION__, fileID, err);
#endif
			goto out;
		}

		/* Check if this is indeed a thread record */
		if ((catRecord.hfsPlusThread.recordType != kHFSPlusFileThreadRecord) &&
			(catRecord.hfsPlusThread.recordType != kHFSPlusFolderThreadRecord) &&
		    (catRecord.hfsThread.recordType != kHFSFileThreadRecord) &&
		    (catRecord.hfsThread.recordType != kHFSFolderThreadRecord)) {
			err = paramErr;
#if DEBUG_OVERLAP
			printf ("%s: Error finding valid thread record for fileID = %d\n", __FUNCTION__, fileID);
#endif
			goto out;
		}
								
		/* Convert the name string to utf8 */
		if (isHFSPlus) {
			(void) utf_encodestr(catRecord.hfsPlusThread.nodeName.unicode, 
								 catRecord.hfsPlusThread.nodeName.length * 2,
								 filename, &namelen);
		} else {
			namelen = catRecord.hfsThread.nodeName[0];
			memcpy (filename, catKey.hfs.nodeName, namelen);
		}

		/* Store the path name in LIFO linked list */
		curPtr = malloc(sizeof(struct fsPathString));
		if (!curPtr) {
			err = memFullErr;
#if DEBUG_OVERLAP
			printf ("%s: Not enough memory (err=%d)\n", __FUNCTION__, err);
#endif
			goto out;
		}

		/* Do not NULL terminate the string */
		curPtr->namelen = namelen;
		curPtr->name = malloc(namelen);
		if (!curPtr->name) {
			err = memFullErr;
#if DEBUG_OVERLAP
			printf ("%s: Not enough memory (err=%d)\n", __FUNCTION__, err);
#endif
		}
		memcpy (curPtr->name, filename, namelen); 
		curPtr->childPtr = listHeadPtr;
		listHeadPtr = curPtr;
		if (listTailPtr == NULL) {
			listTailPtr = curPtr;
		}
		
		/* lookup for parentID */
		if (isHFSPlus) {
			fileID = catRecord.hfsPlusThread.parentID;
		} else {
			fileID = catRecord.hfsThread.parentID;
		}
	
		/* no need to find entire path, bail out */
		if (fullPath == NULL) {
			break;
		}
	}

	/* return the name of the file/directory */
	if (fileName) {
		/* Do not overflow the buffer length passed */
		if (*fileNameLen < (listTailPtr->namelen + 1)) {
			*fileNameLen = *fileNameLen - 1;
			curStatus |= FNAME_BUF2SMALL;
		} else {
			*fileNameLen = listTailPtr->namelen;
		}
		if (*fileNameLen > NAME_MAX) {
			curStatus |= FNAME_BIGNAME;
		}
		memcpy (fileName, listTailPtr->name, *fileNameLen);
		fileName[*fileNameLen] = '\0';
	}

	/* return the full path of the file/directory */
	if (fullPath) {
		/* Do not overflow the buffer length passed and reserve last byte for NULL termination */
		unsigned int bytesRemain = *fullPathLen - 1;

		*fullPathLen = 0;
		while (listHeadPtr != NULL) {
			if (bytesRemain == 0) {
				break;
			}
			memcpy ((fullPath + *fullPathLen), "/", 1);
			*fullPathLen += 1;
			bytesRemain--; 

			if (bytesRemain == 0) {
				break;
			}
			if (bytesRemain < listHeadPtr->namelen) {
				namelen = bytesRemain;
				curStatus |= FPATH_BUF2SMALL;
			} else {
				namelen = listHeadPtr->namelen;
			}
			if (namelen > NAME_MAX) {
				curStatus |= FPATH_BIGNAME;
			}
			memcpy ((fullPath + *fullPathLen), listHeadPtr->name, namelen);
			*fullPathLen += namelen;
			bytesRemain -= namelen;

			curPtr = listHeadPtr;
			listHeadPtr = listHeadPtr->childPtr;
			free(curPtr->name);
			free(curPtr);
		}

		fullPath[*fullPathLen] = '\0';
	}

	err = noErr;

out:
	if (status) {
		*status = curStatus;
	}

	/* free any remaining allocated memory */
	while (listHeadPtr != NULL) {
		curPtr = listHeadPtr;
		listHeadPtr = listHeadPtr->childPtr;
		if (curPtr->name) {
			free (curPtr->name);
		}
		free (curPtr);
	}
	if (filename) {
		free (filename);
	}
	
	return err;
} /* GetFileNamePathByID */

/* Function: CopyDiskBlocks
 *
 * Description: Copy data from source extent to destination extent 
 * for blockCount on the disk.
 *
 * Input: 
 *	1. GPtr - pointer to global scavenger structure.
 * 	2. startAllocationBlock - startBlock for old extent
 * 	3. blockCount - total blocks to copy
 * 	4. newStartAllocationBlock - startBlock for new extent
 *
 * Output:
 * 	err, zero on success, non-zero on failure.
 *		memFullErr - Not enough memory
 */
OSErr CopyDiskBlocks(SGlobPtr GPtr, const UInt32 startAllocationBlock, const UInt32 blockCount, const UInt32 newStartAllocationBlock )
{
	OSErr err = noErr;
	int i;
	int drive;
	char *tmpBuffer = NULL;
	SVCB *vcb;
	UInt32 sectorsPerBlock;
	UInt32 sectorsInBuffer;
	UInt32 ioReqCount;
	UInt32 oldDiskBlock;
	UInt32 newDiskBlock;
	UInt32 actBytes;
	UInt32 numberOfBuffersToWrite;

	tmpBuffer = malloc(DISK_IOSIZE);
	if (!tmpBuffer) {
		err = memFullErr;
#if DEBUG_OVERLAP
		printf ("%s: Not enough memory (err=%d)\n", __FUNCTION__, err);
#endif
		goto out;
	}
	
	vcb = GPtr->calculatedVCB;
	drive = vcb->vcbDriveNumber;
	sectorsPerBlock = vcb->vcbBlockSize / Blk_Size;
	ioReqCount = DISK_IOSIZE;
	sectorsInBuffer = DISK_IOSIZE / Blk_Size;
	numberOfBuffersToWrite = ((blockCount * sectorsPerBlock) + sectorsInBuffer - 1) / sectorsInBuffer; 

	/*
	 * Make sure all changes to the source blocks have been flushed to disk
	 * so that we end up copying current content.  And we might as well
	 * remove the old blocks since we'll never refer to them again.
	 */
	CacheFlushRange(vcb->vcbBlockCache,
					startAllocationBlock * vcb->vcbBlockSize,
					blockCount * vcb->vcbBlockSize,
					true);


	for (i=0; i<numberOfBuffersToWrite; i++) {
		if (i == (numberOfBuffersToWrite - 1)) {
			/* last buffer */	
			ioReqCount = ((blockCount * sectorsPerBlock) - (i * sectorsInBuffer)) * Blk_Size;
		}
		
		/* Calculate the old disk block offset */
		oldDiskBlock = vcb->vcbAlBlSt + (sectorsPerBlock * startAllocationBlock) + (i * sectorsInBuffer);

		/* Read up to one buffer full */
		err = DeviceRead(vcb->vcbDriverReadRef, drive, tmpBuffer, 
						 (SInt64)((UInt64)oldDiskBlock << Log2BlkLo), ioReqCount, &actBytes);
		if (err != noErr) {
			goto out;
		}
		
		/* Calculate the new disk block offset */
		newDiskBlock = vcb->vcbAlBlSt + (sectorsPerBlock * newStartAllocationBlock) + (i * sectorsInBuffer);

		/* Write up to one buffer full */
		err = DeviceWrite(vcb->vcbDriverWriteRef, drive, tmpBuffer, 
						  (SInt64)((UInt64)newDiskBlock << Log2BlkLo), ioReqCount, &actBytes);
		if (err != noErr) {
			goto out;
		}
#if 0 // DEBUG_OVERLAP
		printf ("%s: Moved %d bytes of data from block %d to %d\n", __FUNCTION__, ioReqCount, oldDiskBlock, newDiskBlock);
#endif
	}

out:
	if (tmpBuffer) {
		free (tmpBuffer);
	}
	return err;
} /* CopyDiskBlocks */

/* Function: WriteDiskBlocks
 * 
 * Description: Write given buffer data to disk blocks.
 *
 * Input:
 *	1. GPtr - global scavenger structure pointer
 *	2. startBlock - starting block number for writing data.
 *	3. blockCount - total number of contiguous blocks to be written
 *	4. buffer - data buffer to be written to disk
 *	5. bufLen - length of data buffer to be written to disk.
 *
 * Output:
 * 	returns zero on success, non-zero on failure.
 *		memFullErr - Not enough memory
 */
OSErr WriteDiskBlocks(SGlobPtr GPtr, UInt32 startBlock, UInt32 blockCount, u_char *buffer, int bufLen) 
{
	OSErr err = noErr;
	int i;
	SVCB *vcb;
	int drive;
	u_char *dataBuffer;
	UInt32 sectorsPerBlock;
	UInt32 sectorsInBuffer;
	UInt32 ioReqCount;
	UInt32 diskBlock;
	UInt32 actBytes;
	UInt32 numberOfBuffersToWrite;
	UInt32 bufOffset = 0;

	dataBuffer = buffer;
	vcb = GPtr->calculatedVCB;
	drive = vcb->vcbDriveNumber;
	sectorsPerBlock = vcb->vcbBlockSize / Blk_Size;
	ioReqCount = DISK_IOSIZE;
	sectorsInBuffer = DISK_IOSIZE / Blk_Size;
	numberOfBuffersToWrite = ((blockCount * sectorsPerBlock) + sectorsInBuffer - 1) / sectorsInBuffer; 

	for (i=0; i<numberOfBuffersToWrite; i++) {
		if (i == (numberOfBuffersToWrite - 1)) {
			/* last buffer - should be Blk_Size multiple */	
			ioReqCount = bufLen - bufOffset;
			if (ioReqCount % Blk_Size) {
				ioReqCount = ((ioReqCount / Blk_Size) + 1) * Blk_Size;
			} else {
				ioReqCount = (ioReqCount / Blk_Size) * Blk_Size;
			}
			
			dataBuffer = calloc (1, ioReqCount);
			if (!dataBuffer) {
				err = memFullErr;
				goto out;
			}

			memcpy (dataBuffer, buffer + bufOffset, (bufLen - bufOffset));
			bufOffset = 0;
		}
		
		/* Calculate the new disk block offset */
		diskBlock = vcb->vcbAlBlSt + (sectorsPerBlock * startBlock) + (i * sectorsInBuffer);
		
		/* Write up to one buffer full */
		err = DeviceWrite(vcb->vcbDriverWriteRef, drive, dataBuffer + bufOffset, 
						 (SInt64)((UInt64)diskBlock << Log2BlkLo), ioReqCount, &actBytes);
		if (err != noErr) {
			goto out;
		}

		bufOffset += ioReqCount;
	}

out:
	if (dataBuffer != buffer) {
		free (dataBuffer);
	}
	return err;
} /* WriteDiskBlocks */

//	2210409, in System 8.1, moving file or folder would cause HFS+ thread records to be
//	520 bytes in size.  We only shrink the threads if other repairs are needed.
static	OSErr	FixBloatedThreadRecords( SGlob *GPtr )
{
	CatalogRecord		record;
	CatalogKey			foundKey;
	UInt32				hint;
	UInt16 				recordSize;
	SInt16				i = 0;
	OSErr				err;
	SInt16				selCode				= 0x8001;										//	 Start with 1st record

	err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
	ReturnIfError( err );

	selCode = 1;																//	 Get next record from now on		

	do
	{
		if ( ++i > 10 ) { (void) CheckForStop( GPtr ); i = 0; }					//	Spin the cursor every 10 entries
		
		if (  (recordSize == sizeof(HFSPlusCatalogThread)) && ((record.recordType == kHFSPlusFolderThreadRecord) || (record.recordType == kHFSPlusFileThreadRecord)) )
		{
			// HFS Plus has varaible sized threads so adjust to actual length
			recordSize -= ( sizeof(record.hfsPlusThread.nodeName.unicode) - (record.hfsPlusThread.nodeName.length * sizeof(UniChar)) );

			err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
			ReturnIfError( err );
		}

		err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
	} while ( err == noErr );

	if ( err == btNotFound )
		err = noErr;
		
	return( err );
}


static OSErr
FixMissingThreadRecords( SGlob *GPtr )
{
	struct MissingThread *	mtp;
	FSBufferDescriptor    	btRecord;
	BTreeIterator         	iterator;
	OSStatus           		result;
	UInt16              	dataSize;
	Boolean					headsUp;
	UInt32					lostAndFoundDirID;

	lostAndFoundDirID = 0;
	headsUp = false;
	for (mtp = GPtr->missingThreadList; mtp != NULL; mtp = mtp->link) {
		if ( mtp->threadID == 0 )
			continue;

		// if the thread record information in the MissingThread struct is not there
		// then we have a missing directory in addition to a missing thread record 
		// for that directory.  We will recreate the missing directory in our 
		// lost+found directory.
		if ( mtp->thread.parentID == 0 ) {
			char 	myString[32];
			if ( lostAndFoundDirID == 0 )
				lostAndFoundDirID = CreateDirByName( GPtr , (u_char *)"lost+found", kHFSRootFolderID);
			if ( lostAndFoundDirID == 0 ) {
				if ( GPtr->logLevel >= kDebugLog )
					printf( "\tCould not create lost+found directory \n" );
				return( R_RFail );
			}
			sprintf( myString, "%ld", (long)mtp->threadID );
			PrintError( GPtr, E_NoDir, 1, myString );
			result = FixMissingDirectory( GPtr, mtp->threadID, lostAndFoundDirID );
			if ( result != 0 ) {
				if ( GPtr->logLevel >= kDebugLog )
					printf( "\tCould not recreate a missing directory \n" );
					printf ("result = %d\n", (int) result);
				return( R_RFail );
			}
			else
				headsUp = true;
			continue;
		}

		dataSize = 10 + (mtp->thread.nodeName.length * 2);
		btRecord.bufferAddress = (void *)&mtp->thread;
		btRecord.itemSize = dataSize;
		btRecord.itemCount = 1;
		iterator.hint.nodeNum = 0;
		BuildCatalogKey(mtp->threadID, NULL, true, (CatalogKey*)&iterator.key);

		result = BTInsertRecord(GPtr->calculatedCatalogFCB, &iterator, &btRecord, dataSize);
		if (result)
			return (IntError(GPtr, R_IntErr));
		mtp->threadID = 0;
	}
	if ( headsUp )
		PrintStatus( GPtr, M_Look, 0 );

	return (0);
}


static OSErr
FixMissingDirectory( SGlob *GPtr, UInt32 theObjID, UInt32 theParID )
{
	Boolean				isHFSPlus;
	UInt16				recSize;
	OSErr				result;		
	int					nameLen;
	UInt32				hint;		
	UInt32				myItemsCount;		
	char 				myString[ 32 ];
	CatalogName			myName;
	CatalogRecord		catRec;
	CatalogKey			myKey, myThreadKey;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	
	// we will use the object ID of the missing directory as the name since we have
	// no way to find the original name and this should make it unique within our
	// lost+found directory.
	sprintf( myString, "%ld", (long)theObjID );
	nameLen = strlen( myString );

    if ( isHFSPlus )
    {
        int		i;
        myName.ustr.length = nameLen;
        for ( i = 0; i < myName.ustr.length; i++ )
            myName.ustr.unicode[ i ] = (u_int16_t) myString[ i ];
    }
    else
    {
        myName.pstr[0] = nameLen;
        memcpy( &myName.pstr[1], &myString[0], nameLen );
    }

	// make sure the name is not already used 
	BuildCatalogKey( theParID, &myName, isHFSPlus, &myKey );
	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &myKey, kNoHint, 
								NULL, &catRec, &recSize, &hint );
	if ( result == noErr )
		return( R_IntErr );
	
    // insert new directory and thread record into the catalog
	recSize = BuildThreadRec( &myKey, &catRec, isHFSPlus, true );
	BuildCatalogKey( theObjID, NULL, isHFSPlus, &myThreadKey );
	result	= InsertBTreeRecord( GPtr->calculatedCatalogFCB, &myThreadKey, &catRec, recSize, &hint );
	if ( result != noErr )
		return( result );

	// need to look up all objects in the directory so we can set the valance
	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &myThreadKey, kNoHint, 
								NULL, &catRec, &recSize, &hint );
	if ( result != noErr )
		return( result );

	myItemsCount = 0;
	for ( ;; ) {
		CatalogKey		foundKey;
		result = GetBTreeRecord( GPtr->calculatedCatalogFCB, 1, &foundKey, &catRec, &recSize, &hint );
		if ( result != noErr )
			break;
		if ( isHFSPlus ) {
			if ( foundKey.hfsPlus.parentID != theObjID )
				break;
		} else {
			if ( foundKey.hfs.parentID != theObjID )
				break;
		}
		if ( catRec.recordType == kHFSPlusFolderRecord || catRec.recordType == kHFSPlusFileRecord ||
			 catRec.recordType == kHFSFolderRecord || catRec.recordType == kHFSFileRecord ) {
			myItemsCount++;
		}
	}

	recSize = BuildFolderRec( 01777, theObjID, isHFSPlus, &catRec );
	if ( isHFSPlus )
		catRec.hfsPlusFolder.valence = myItemsCount;
	else
		catRec.hfsFolder.valence = myItemsCount;
	result	= InsertBTreeRecord( GPtr->calculatedCatalogFCB, &myKey, &catRec, recSize, &hint );
	if ( result != noErr )
		return( result );
	
	/* update parent directory to reflect addition of new directory */
	result = UpdateFolderCount( GPtr->calculatedVCB, theParID, NULL, 
								((isHFSPlus) ? kHFSPlusFolderRecord : kHFSFolderRecord), 
								kNoHint, 1 );

	/* update our header node on disk from our BTreeControlBlock */
	UpdateBTreeHeader( GPtr->calculatedCatalogFCB );
		
	return( result );
	
} /* FixMissingDirectory */


static HFSCatalogNodeID 
GetObjectID( CatalogRecord * theRecPtr )
{
    HFSCatalogNodeID	myObjID;
    
	switch ( theRecPtr->recordType ) {
	case kHFSPlusFolderRecord:
        myObjID = theRecPtr->hfsPlusFolder.folderID;
		break;
	case kHFSPlusFileRecord:
        myObjID = theRecPtr->hfsPlusFile.fileID;
		break;
	case kHFSFolderRecord:
        myObjID = theRecPtr->hfsFolder.folderID;
		break;
	case kHFSFileRecord:
        myObjID = theRecPtr->hfsFile.fileID;
		break;
	default:
        myObjID = 0;
	}
	
    return( myObjID );
    
} /* GetObjectID */

/* Function: CreateFileByName
 *
 * Description: Create a file with given fileName of type fileType containing
 * data of length dataLen.  This function assumes that the name of symlink
 * to be created is passed as UTF8
 *
 * Input:
 *	1. GPtr - pointer to global scavenger structure
 *	2. parentID - ID of parent directory to create the new file.
 *	3. fileName - name of the file to create in UTF8 format.  
 *	4. fileNameLen - length of the filename to be created.  
 *			If the volume is HFS Plus, the filename is delimited to
 *			kHFSPlusMaxFileNameChars characters.
 *			If the volume is plain HFS, the filename is delimited to
 *			kHFSMaxFileNameChars characters.
 *	5. fileType - file type (currently supported S_IFREG, S_IFLNK).
 *	6. data - data content of first data fork of the file
 *	7. dataLen - length of data to be written
 *
 * Output:
 *	returns zero on success, non-zero on failure.
 *		memFullErr - Not enough memory
 *		paramErr - Invalid paramter
 */
OSErr CreateFileByName(SGlobPtr GPtr, UInt32 parentID, UInt16 fileType, u_char *fileName, unsigned int filenameLen, u_char *data, unsigned int dataLen)
{	
	OSErr err = noErr;
	Boolean isHFSPlus;
	Boolean isCatUpdated = false;

	CatalogName fName;
	CatalogRecord catRecord;
	CatalogKey catKey;
	CatalogKey threadKey;
	UInt32 hint;
	UInt16 recordSize;

	UInt32 totalBlocks = 0;
	UInt32 startBlock = 0;
	UInt32 blockCount = 0;
	UInt32 nextCNID;

	isHFSPlus = VolumeObjectIsHFSPlus();

	/* Construct unicode name for file name to construct catalog key */
	if (isHFSPlus) {
		/* Convert utf8 filename to Unicode filename */
		size_t namelen;

		if (filenameLen < kHFSPlusMaxFileNameChars) {
			(void) utf_decodestr (fileName, filenameLen, fName.ustr.unicode, &namelen);
			namelen /= 2;
			fName.ustr.length = namelen;
		} else {
			/* The resulting may result in more than kHFSPlusMaxFileNameChars chars */
			UInt16 *unicodename;

			/* Allocate a large array to convert the utf-8 to utf-16 */
			unicodename = malloc (filenameLen * 4);
			if (unicodename == NULL) {
				err = memFullErr;
				goto out;
			}

			(void) utf_decodestr (fileName, filenameLen, unicodename, &namelen);
			namelen /= 2;

			/* Chopping unicode string based on length might affect unicode 
			 * chars that take more than one UInt16 - very rare possiblity.
			 */
			if (namelen > kHFSPlusMaxFileNameChars) {
				namelen = kHFSPlusMaxFileNameChars;
			}

			memcpy (fName.ustr.unicode, unicodename, (namelen * 2));
			free (unicodename);
			fName.ustr.length = namelen;
		}
	} else {
		if (filenameLen > kHFSMaxFileNameChars) {
			filenameLen = kHFSMaxFileNameChars;
		}
		fName.pstr[0] = filenameLen;
		memcpy(&fName.pstr[1], fileName, filenameLen);
	}

	/* Make sure that a file with same name does not exist in parent dir */
	BuildCatalogKey(parentID, &fName, isHFSPlus, &catKey);
	err = SearchBTreeRecord(GPtr->calculatedCatalogFCB, &catKey, kNoHint, NULL, 
							&catRecord, &recordSize, &hint);
	if (err != fsBTRecordNotFoundErr) {
#if DEBUG_OVERLAP
		printf ("%s: %s probably exists in dirID = %d (err=%d)\n", __FUNCTION__, fileName, parentID, err);
#endif
		err = EEXIST;
		goto out;
	}

	if (data) {
		/* Calculate correct number of blocks required for data */
		if (dataLen % (GPtr->calculatedVCB->vcbBlockSize)) {
			totalBlocks = (dataLen / (GPtr->calculatedVCB->vcbBlockSize)) + 1;
		} else {
			totalBlocks = dataLen / (GPtr->calculatedVCB->vcbBlockSize);
		}
	
		if (totalBlocks) {
			/* Allocate disk space for the data */
			err = BlockAllocate(GPtr->calculatedVCB, 0, totalBlocks, totalBlocks, 
								true, &startBlock, &blockCount);
			if (err != noErr) {
#if DEBUG_OVERLAP
				printf ("%s: Not enough disk space (err=%d)\n", __FUNCTION__, err);
#endif
				goto out;
			}

			/* Write the data to the blocks */
			err = WriteDiskBlocks(GPtr, startBlock, blockCount, data, dataLen);
			if (err != noErr) {
#if DEBUG_OVERLAP
				printf ("%s: Error in writing data of %s to disk (err=%d)\n", __FUNCTION__, fileName, err);
#endif
				goto out;
			}
		}
	}

	/* Build and insert thread record */
	nextCNID = GPtr->calculatedVCB->vcbNextCatalogID;
	if (!isHFSPlus && nextCNID == 0xffffFFFF) {
		goto out;
	}
	recordSize = BuildThreadRec(&catKey, &catRecord, isHFSPlus, false);
	for (;;) {
		BuildCatalogKey(nextCNID, NULL, isHFSPlus, &threadKey);
		err	= InsertBTreeRecord(GPtr->calculatedCatalogFCB, &threadKey, &catRecord,
								recordSize, &hint );
		if (err == fsBTDuplicateRecordErr && isHFSPlus) {
			/* Allow CNIDs on HFS Plus volumes to wrap around */
			++nextCNID;
			if (nextCNID < kHFSFirstUserCatalogNodeID) {
				GPtr->calculatedVCB->vcbAttributes |= kHFSCatalogNodeIDsReusedMask;
				MarkVCBDirty(GPtr->calculatedVCB);
				nextCNID = kHFSFirstUserCatalogNodeID;
			}
			continue;
		}
		break;
	}
	if (err != noErr) {
#if DEBUG_OVERLAP
		printf ("%s: Error inserting thread record for file = %s (err=%d)\n", __FUNCTION__, fileName, err);
#endif
		goto out;
	}

	/* Build file record */
	recordSize = BuildFileRec(fileType, 0666, nextCNID, isHFSPlus, &catRecord);
	if (recordSize == 0) {
#if DEBUG_OVERLAP
		printf ("%s: Incorrect fileType\n", __FUNCTION__);
#endif

		/* Remove the thread record inserted above */
		err = DeleteBTreeRecord (GPtr->calculatedCatalogFCB, &threadKey);
		if (err != noErr) {
#if DEBUG_OVERLAP
			printf ("%s: Error in removing thread record\n", __FUNCTION__);
#endif
		}
		err = paramErr;
		goto out;
	}

	/* Update startBlock, blockCount, etc */
	if (isHFSPlus) {
		catRecord.hfsPlusFile.dataFork.logicalSize = dataLen; 
		catRecord.hfsPlusFile.dataFork.totalBlocks = totalBlocks;
		catRecord.hfsPlusFile.dataFork.extents[0].startBlock = startBlock;
		catRecord.hfsPlusFile.dataFork.extents[0].blockCount = blockCount;
	} else {
		catRecord.hfsFile.dataLogicalSize = dataLen;
		catRecord.hfsFile.dataPhysicalSize = totalBlocks * GPtr->calculatedVCB->vcbBlockSize;
		catRecord.hfsFile.dataExtents[0].startBlock = startBlock;
		catRecord.hfsFile.dataExtents[0].blockCount = blockCount;
	}

	/* Insert catalog file record */
    err	= InsertBTreeRecord(GPtr->calculatedCatalogFCB, &catKey, &catRecord, recordSize, &hint );
	if (err == noErr) {
		isCatUpdated = true;

#if DEBUG_OVERLAP
	printf ("Created \"%s\" with ID = %d startBlock = %d, blockCount = %d, dataLen = %d\n", fileName, nextCNID, startBlock, blockCount, dataLen);
#endif
	} else {
#if DEBUG_OVERLAP
		printf ("%s: Error in inserting file record for file = %s (err=%d)\n", __FUNCTION__, fileName, err);
#endif

		/* remove the thread record inserted above */
		err = DeleteBTreeRecord (GPtr->calculatedCatalogFCB, &threadKey);
		if (err != noErr) {
#if DEBUG_OVERLAP
			printf ("%s: Error in removing thread record\n", __FUNCTION__);
#endif
		}
		err = paramErr;
		goto out;
	}

	/* Update volume header */
	GPtr->calculatedVCB->vcbNextCatalogID = nextCNID + 1;
	if (GPtr->calculatedVCB->vcbNextCatalogID < kHFSFirstUserCatalogNodeID) {
		GPtr->calculatedVCB->vcbAttributes |= kHFSCatalogNodeIDsReusedMask;
		GPtr->calculatedVCB->vcbNextCatalogID = kHFSFirstUserCatalogNodeID;
	}
	MarkVCBDirty( GPtr->calculatedVCB );
	
	/* update our header node on disk from our BTreeControlBlock */
	UpdateBTreeHeader(GPtr->calculatedCatalogFCB);

	/* update parent directory to reflect addition of new file */
	err = UpdateFolderCount(GPtr->calculatedVCB, parentID, NULL, kHFSPlusFileRecord, kNoHint, 1);
	if (err != noErr) {
#if DEBUG_OVERLAP
		printf ("%s: Error in updating parent folder count (err=%d)\n", __FUNCTION__, err);
#endif
		goto out;
	}

out:
	/* On error, if catalog record was not inserted and disk block were allocated,
	 * deallocate the blocks 
	 */
	if (err && (isCatUpdated == false) && startBlock) {
		ReleaseBitmapBits (startBlock, blockCount);
	}

	return err;
} /* CreateFileByName */

/* Function: CreateDirByName
 *
 * Description: Create directory with name dirName in a directory with ID as
 * parentID.  The function assumes that the dirName passed is ASCII.
 *
 * Input: 
 *	GPtr - global scavenger structure pointer
 * 	dirName - name of directory to be created
 *	parentID - dirID of the parent directory for new directory
 *
 * Output:
 * 	on success, ID of the new directory created.
 * 	on failure, zero.
 *  
 */
UInt32 CreateDirByName(SGlob *GPtr , const u_char *dirName, const UInt32 parentID)
{
	Boolean				isHFSPlus;
	UInt16				recSize;
	UInt16				myMode;
	int					result;		
	int					nameLen;
	UInt32				hint;		
	UInt32				nextCNID;		
	SFCB *				fcbPtr;
	CatalogKey			myKey;
	CatalogName			myName;
	CatalogRecord		catRec;
	
	isHFSPlus = VolumeObjectIsHFSPlus( );
  	fcbPtr = GPtr->calculatedCatalogFCB;
	nameLen = strlen( (char *)dirName );

    if ( isHFSPlus )
    {
        int		i;
        myName.ustr.length = nameLen;
        for ( i = 0; i < myName.ustr.length; i++ )
            myName.ustr.unicode[ i ] = (u_int16_t) dirName[ i ];
    }
    else
    {
        myName.pstr[0] = nameLen;
        memcpy( &myName.pstr[1], &dirName[0], nameLen );
    }

	// see if we already have a lost and found directory
	BuildCatalogKey( parentID, &myName, isHFSPlus, &myKey );
	result = SearchBTreeRecord( fcbPtr, &myKey, kNoHint, NULL, &catRec, &recSize, &hint );
	if ( result == noErr ) {
		if ( isHFSPlus ) {
			if ( catRec.recordType == kHFSPlusFolderRecord )
				return( catRec.hfsPlusFolder.folderID ); 
		}
		else if ( catRec.recordType == kHFSFolderRecord )
			return( catRec.hfsFolder.folderID ); 	
        return( 0 );  // something already there but not a directory
	}
  
    // insert new directory and thread record into the catalog
	nextCNID = GPtr->calculatedVCB->vcbNextCatalogID;
	if ( !isHFSPlus && nextCNID == 0xFFFFFFFF )
		return( 0 );

	recSize = BuildThreadRec( &myKey, &catRec, isHFSPlus, true );
	for (;;) {
		CatalogKey			key;
		
		BuildCatalogKey( nextCNID, NULL, isHFSPlus, &key );
		result	= InsertBTreeRecord( fcbPtr, &key, &catRec, recSize, &hint );
		if ( result == fsBTDuplicateRecordErr && isHFSPlus ) {
			/*
			 * Allow CNIDs on HFS Plus volumes to wrap around
			 */
			++nextCNID;
			if ( nextCNID < kHFSFirstUserCatalogNodeID ) {
				GPtr->calculatedVCB->vcbAttributes |= kHFSCatalogNodeIDsReusedMask;
				MarkVCBDirty( GPtr->calculatedVCB );
				nextCNID = kHFSFirstUserCatalogNodeID;
			}
			continue;
		}
		break;
	}
	if ( result != 0 )
		return( 0 ); 	
	
	myMode = ( GPtr->lostAndFoundMode == 0 ) ? 01777 : GPtr->lostAndFoundMode;
	recSize = BuildFolderRec( myMode, nextCNID, isHFSPlus, &catRec );
    result	= InsertBTreeRecord( fcbPtr, &myKey, &catRec, recSize, &hint );
	if ( result != 0 )
		return( 0 );

	/* Update volume header */
	GPtr->calculatedVCB->vcbNextCatalogID = nextCNID + 1;
	if ( GPtr->calculatedVCB->vcbNextCatalogID < kHFSFirstUserCatalogNodeID ) {
		GPtr->calculatedVCB->vcbAttributes |= kHFSCatalogNodeIDsReusedMask;
		GPtr->calculatedVCB->vcbNextCatalogID = kHFSFirstUserCatalogNodeID;
	}
	MarkVCBDirty( GPtr->calculatedVCB );
	
	/* update parent directory to reflect addition of new directory */
	result = UpdateFolderCount( GPtr->calculatedVCB, parentID, NULL, kHFSPlusFolderRecord, kNoHint, 1 );

	/* update our header node on disk from our BTreeControlBlock */
	UpdateBTreeHeader( GPtr->calculatedCatalogFCB );

	return( nextCNID );

} /* CreateDirByName */

/*
 * Build a catalog node folder record with the given input.
 */
static int
BuildFolderRec( u_int16_t theMode, UInt32 theObjID, Boolean isHFSPlus, CatalogRecord * theRecPtr )
{
	UInt16				recSize;
	UInt32 				createTime;
	
	ClearMemory( (Ptr)theRecPtr, sizeof(*theRecPtr) );
	
	if ( isHFSPlus ) {
		createTime = GetTimeUTC();
		theRecPtr->hfsPlusFolder.recordType = kHFSPlusFolderRecord;
		theRecPtr->hfsPlusFolder.folderID = theObjID;
		theRecPtr->hfsPlusFolder.createDate = createTime;
		theRecPtr->hfsPlusFolder.contentModDate = createTime;
		theRecPtr->hfsPlusFolder.attributeModDate = createTime;
		theRecPtr->hfsPlusFolder.bsdInfo.ownerID = getuid( );
		theRecPtr->hfsPlusFolder.bsdInfo.groupID = getgid( );
		theRecPtr->hfsPlusFolder.bsdInfo.fileMode = S_IFDIR;
		theRecPtr->hfsPlusFolder.bsdInfo.fileMode |= theMode;
		recSize= sizeof(HFSPlusCatalogFolder);
	}
	else {
		createTime = GetTimeLocal( true );
		theRecPtr->hfsFolder.recordType = kHFSFolderRecord;
		theRecPtr->hfsFolder.folderID = theObjID;
		theRecPtr->hfsFolder.createDate = createTime;
		theRecPtr->hfsFolder.modifyDate = createTime;
		recSize= sizeof(HFSCatalogFolder);
	}

	return( recSize );
	
} /* BuildFolderRec */


/*
 * Build a catalog node thread record from a catalog key
 * and return the size of the record.
 */
static int
BuildThreadRec( CatalogKey * theKeyPtr, CatalogRecord * theRecPtr, 
				Boolean isHFSPlus, Boolean isDirectory )
{
	int size = 0;

	if ( isHFSPlus ) {
		HFSPlusCatalogKey *key = (HFSPlusCatalogKey *)theKeyPtr;
		HFSPlusCatalogThread *rec = (HFSPlusCatalogThread *)theRecPtr;

		size = sizeof(HFSPlusCatalogThread);
		if ( isDirectory )
			rec->recordType = kHFSPlusFolderThreadRecord;
		else
			rec->recordType = kHFSPlusFileThreadRecord;
		rec->reserved = 0;
		rec->parentID = key->parentID;			
		bcopy(&key->nodeName, &rec->nodeName,
			sizeof(UniChar) * (key->nodeName.length + 1));

		/* HFS Plus has varaible sized thread records */
		size -= (sizeof(rec->nodeName.unicode) -
			  (rec->nodeName.length * sizeof(UniChar)));
	} 
	else /* HFS standard */ {
		HFSCatalogKey *key = (HFSCatalogKey *)theKeyPtr;
		HFSCatalogThread *rec = (HFSCatalogThread *)theRecPtr;

		size = sizeof(HFSCatalogThread);
		bzero(rec, size);
		if ( isDirectory )
			rec->recordType = kHFSFolderThreadRecord;
		else
			rec->recordType = kHFSFileThreadRecord;
		rec->parentID = key->parentID;
		bcopy(key->nodeName, rec->nodeName, key->nodeName[0]+1);
	}
	
	return (size);
	
} /* BuildThreadRec */

/* Function: BuildFileRec
 *
 * Description: Build a catalog file record with given fileID, fileType 
 * and fileMode.
 *
 * Input:
 *	1. fileType - currently supports S_IFREG, S_IFLNK
 *	2. fileMode - file mode desired.
 *	3. fileID - file ID
 *	4. isHFSPlus - indicates whether the record is being created for 
 *	HFSPlus volume or plain HFS volume.
 *	5. catRecord - pointer to catalog record
 *
 * Output:
 *	returns size of the catalog record.
 *	on success, non-zero value; on failure, zero.
 */
static int BuildFileRec(UInt16 fileType, UInt16 fileMode, UInt32 fileID, Boolean isHFSPlus, CatalogRecord *catRecord)
{
	UInt16 recordSize = 0;
	UInt32 createTime;
	
	/* We only support creating S_IFREG and S_IFLNK and S_IFLNK is not supported
	 * on plain HFS 
	 */
	if (((fileType != S_IFREG) && (fileType != S_IFLNK)) || 
	 	((isHFSPlus == false) && (fileType == S_IFLNK))) {
		goto out;
	}

	ClearMemory((Ptr)catRecord, sizeof(*catRecord));
	
	if ( isHFSPlus ) {
		createTime = GetTimeUTC();
		catRecord->hfsPlusFile.recordType = kHFSPlusFileRecord;
		catRecord->hfsPlusFile.fileID = fileID;
		catRecord->hfsPlusFile.createDate = createTime;
		catRecord->hfsPlusFile.contentModDate = createTime;
		catRecord->hfsPlusFile.attributeModDate = createTime;
		catRecord->hfsPlusFile.bsdInfo.ownerID = getuid();
		catRecord->hfsPlusFile.bsdInfo.groupID = getgid();
		catRecord->hfsPlusFile.bsdInfo.fileMode = fileType;
		catRecord->hfsPlusFile.bsdInfo.fileMode |= fileMode;
		if (fileType == S_IFLNK) {
			catRecord->hfsPlusFile.userInfo.fdType = kSymLinkFileType;
			catRecord->hfsPlusFile.userInfo.fdCreator = kSymLinkCreator;
		} else {
			catRecord->hfsPlusFile.userInfo.fdType = kTextFileType;
			catRecord->hfsPlusFile.userInfo.fdCreator = kTextFileCreator;
		}
		recordSize= sizeof(HFSPlusCatalogFile);
	}
	else {
		createTime = GetTimeLocal(true);
		catRecord->hfsFile.recordType = kHFSFileRecord;
		catRecord->hfsFile.fileID = fileID;
		catRecord->hfsFile.createDate = createTime;
		catRecord->hfsFile.modifyDate = createTime;
		catRecord->hfsFile.userInfo.fdType = kTextFileType;
		catRecord->hfsFile.userInfo.fdCreator = kTextFileCreator;
		recordSize= sizeof(HFSCatalogFile);
	}

out:
	return(recordSize);
} /* BuildFileRec */


