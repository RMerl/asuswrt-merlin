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
	File:		SVerify1.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.

*/

#include "Scavenger.h"

//	internal routine prototypes

static	int	RcdValErr( SGlobPtr GPtr, OSErr type, UInt32 correct, UInt32 incorrect, HFSCatalogNodeID parid );
		
static	int	RcdNameLockedErr( SGlobPtr GPtr, OSErr type, UInt32 incorrect );
	
static	OSErr	RcdMDBEmbededVolDescriptionErr( SGlobPtr GPtr, OSErr type, HFSMasterDirectoryBlock *mdb );

static	OSErr	CheckNodesFirstOffset( SGlobPtr GPtr, BTreeControlBlock *btcb );

static	Boolean	ExtentInfoExists( ExtentsTable **extentsTableH, ExtentInfo *extentInfo, Boolean *doesFileExist );

static OSErr	ScavengeVolumeType( SGlobPtr GPtr, HFSMasterDirectoryBlock *mdb, UInt32 *volumeType );
static OSErr	SeekVolumeHeader( SGlobPtr GPtr, UInt64 startSector, UInt32 numSectors, UInt64 *vHSector );

/*
 * Check if a volume is journaled.  
 *
 * returns:    	0 not journaled
 *		1 journaled
 *
 */
int
CheckIfJournaled(SGlobPtr GPtr)
{
#define kIDSector 2

	OSErr err;
	int result;
	HFSMasterDirectoryBlock	*mdbp;
	HFSPlusVolumeHeader *vhp;
	SVCB *vcb = GPtr->calculatedVCB;
	ReleaseBlockOptions rbOptions;
	BlockDescriptor block;

	vhp = (HFSPlusVolumeHeader *) NULL;
	rbOptions = kReleaseBlock;

	err = GetVolumeBlock(vcb, kIDSector, kGetBlock, &block);
	if (err) return (0);

	mdbp = (HFSMasterDirectoryBlock	*) block.buffer;

	if (mdbp->drSigWord == kHFSPlusSigWord || mdbp->drSigWord == kHFSXSigWord) {
		vhp = (HFSPlusVolumeHeader *) block.buffer;

	} else if (mdbp->drSigWord == kHFSSigWord) {

		if (mdbp->drEmbedSigWord == kHFSPlusSigWord) {
			UInt32 vhSector;
			UInt32 blkSectors;
			
			blkSectors = mdbp->drAlBlkSiz / 512;
			vhSector  = mdbp->drAlBlSt;
			vhSector += blkSectors * mdbp->drEmbedExtent.startBlock;
			vhSector += kIDSector;
	
			(void) ReleaseVolumeBlock(vcb, &block, kReleaseBlock);
			err = GetVolumeBlock(vcb, vhSector, kGetBlock, &block);
			if (err) return (0);

			vhp = (HFSPlusVolumeHeader *) block.buffer;
			mdbp = (HFSMasterDirectoryBlock	*) NULL;

		}
	}

	if ((vhp != NULL) && (ValidVolumeHeader(vhp) == noErr)) {
		result = ((vhp->attributes & kHFSVolumeJournaledMask) != 0);

		// even if journaling is enabled for this volume, we'll return
		// false if it wasn't unmounted cleanly and it was previously
		// mounted by someone that doesn't know about journaling.
		// or if lastMountedVersion is kFSKMountVersion
		if ( vhp->lastMountedVersion == kFSKMountVersion || 
			((vhp->lastMountedVersion != kHFSJMountVersion) && 
			(vhp->attributes & kHFSVolumeUnmountedMask) == 0)) {
			result = 0;
		}
	} else {
		result = 0;
	}

	(void) ReleaseVolumeBlock(vcb, &block, rbOptions);

	return (result);
}

/*
 * Check if a volume is clean (unmounted safely)
 *
 * returns:    -1 not an HFS/HFS+ volume
 *		0 dirty
 *		1 clean
 *
 * if markClean is true and the volume is dirty it
 * will be marked clean on disk.
 */
int
CheckForClean(SGlobPtr GPtr, Boolean markClean)
{
	OSErr err;
	int result = -1;
	HFSMasterDirectoryBlock	*mdbp;
	HFSPlusVolumeHeader *vhp;
	SVCB *vcb = GPtr->calculatedVCB;
	VolumeObjectPtr myVOPtr;
	ReleaseBlockOptions rbOptions;
	UInt64			blockNum;
	BlockDescriptor block;

	vhp = (HFSPlusVolumeHeader *) NULL;
	rbOptions = kReleaseBlock;
	myVOPtr = GetVolumeObjectPtr( );
	block.buffer = NULL;
	
	GetVolumeObjectBlockNum( &blockNum );
	if ( blockNum == 0 ) {
		if ( GPtr->logLevel >= kDebugLog )
			printf( "\t%s - unknown volume type \n", __FUNCTION__ );
		return (-1);
	}
	
	// get the VHB or MDB (depending on type of volume)
	err = GetVolumeObjectPrimaryBlock( &block );
	if (err) {
		if ( GPtr->logLevel >= kDebugLog )
			printf( "\t%s - could not get VHB/MDB at block %qd \n", __FUNCTION__, blockNum );
		err = -1;
		goto ExitThisRoutine;
	}

	if ( VolumeObjectIsHFSPlus( ) ) {
		vhp = (HFSPlusVolumeHeader *) block.buffer;
		
		result = 1;  // clean unmount
		if ( ((vhp->attributes & kHFSVolumeUnmountedMask) == 0) )
			result = 0;  // dirty unmount

		// if we are to mark volume clean and lastMountedVersion shows journaled we invalidate
		// the journal by setting lastMountedVersion to 'fsck'
		if ( markClean && ((vhp->lastMountedVersion == kHFSJMountVersion) || 
						   (vhp->lastMountedVersion == kFSKMountVersion)) ) {
			result = 0;  // force dirty unmount path
			vhp->lastMountedVersion = kFSCKMountVersion;
		}
		
		if (vhp->lastMountedVersion == kFSKMountVersion) {
			GPtr->JStat |= S_BadJournal;
			if ( GPtr->logLevel >= kDebugLog ) 
				printf ("\t%s - found bad journal signature \n", __FUNCTION__);
			/* XXX This is not correct error code for bad journal.  
			 * E_InvalidVolumeHeader is negative error.  Change ErrCode
			 * to non-negative, else it might indicate non-fixable error
			 * in scavenge process 
			 */
			RcdError (GPtr, E_InvalidVolumeHeader);
			GPtr->ErrCode = -E_InvalidVolumeHeader;
			result = 0;	//dirty unmount
		}
		
		if ( markClean && (result == 0) ) {
			vhp->attributes |= kHFSVolumeUnmountedMask;
			rbOptions = kForceWriteBlock;
		}
	}
	else if ( VolumeObjectIsHFS( ) ) {
		mdbp = (HFSMasterDirectoryBlock	*) block.buffer;
		
		result = (mdbp->drAtrb & kHFSVolumeUnmountedMask) != 0;
		if (markClean && (result == 0)) {
			mdbp->drAtrb |= kHFSVolumeUnmountedMask;
			rbOptions = kForceWriteBlock;
		}
	}

ExitThisRoutine:
	if ( block.buffer != NULL )
		(void) ReleaseVolumeBlock(vcb, &block, rbOptions);

	return (result);
}

/*------------------------------------------------------------------------------

Function:	IVChk - (Initial Volume Check)

Function:	Performs an initial check of the volume to be scavenged to confirm
			that the volume can be accessed and that it is a HFS/HFS+ volume.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		IVChk	-	function result:			
								0	= no error
								n 	= error code
------------------------------------------------------------------------------*/
#define					kBitsPerSector	4096

OSErr IVChk( SGlobPtr GPtr )
{
	OSErr						err;
	HFSMasterDirectoryBlock *	myMDBPtr;
	HFSPlusVolumeHeader *		myVHBPtr;
	UInt32					numABlks;
	UInt32					minABlkSz;
	UInt32					maxNumberOfAllocationBlocks;
	UInt32					realAllocationBlockSize;
	UInt32					realTotalBlocks;
	UInt32					i;
	BTreeControlBlock		*btcb;
	SVCB					*vcb	= GPtr->calculatedVCB;
	VolumeObjectPtr 		myVOPtr;
	UInt64					blockNum;
	UInt64					totalSectors;
	BlockDescriptor			myBlockDescriptor;
	
	//  Set up
	GPtr->TarID = AMDB_FNum;	//	target = alt MDB
	GPtr->TarBlock	= 0;
	maxNumberOfAllocationBlocks	= 0xFFFFFFFF;
	realAllocationBlockSize = 0;
	realTotalBlocks = 0;
	
	myBlockDescriptor.buffer = NULL;
	myVOPtr = GetVolumeObjectPtr( );
		
	// check volume size
	if ( myVOPtr->totalDeviceSectors < 3 ) {
		if ( GPtr->logLevel >= kDebugLog )
			printf("\tinvalid device information for volume - total sectors = %qd sector size = %d \n",
				myVOPtr->totalDeviceSectors, myVOPtr->sectorSize);
		return( 123 );
	}
	
	GetVolumeObjectBlockNum( &blockNum );
	if ( blockNum == 0 || myVOPtr->volumeType == kUnknownVolumeType ) {
		if ( GPtr->logLevel >= kDebugLog )
			printf( "\t%s - unknown volume type \n", __FUNCTION__ );
		err = R_BadSig;  /* doesn't bear the HFS signature */
		goto ReleaseAndBail;
	}

	// get Volume Header (HFS+) or Master Directory (HFS) block
	err = GetVolumeObjectVHBorMDB( &myBlockDescriptor );
	if ( err != noErr ) {
		if ( GPtr->logLevel >= kDebugLog )
			printf( "\t%s - bad volume header - err %d \n", __FUNCTION__, err );
		goto ReleaseAndBail;
	}
	myMDBPtr = (HFSMasterDirectoryBlock	*) myBlockDescriptor.buffer;

	// if this is an HFS (kHFSVolumeType) volume and the MDB indicates this
	// might contain an embedded HFS+ volume then we need to scan
	// for an embedded HFS+ volume.  I'm told there were some old problems
	// where we could lose track of the embedded volume.
	if ( VolumeObjectIsHFS( ) && 
		 (myMDBPtr->drEmbedSigWord != 0 || 
		  myMDBPtr->drEmbedExtent.blockCount != 0 || 
		  myMDBPtr->drEmbedExtent.startBlock != 0) ) {

		err = ScavengeVolumeType( GPtr, myMDBPtr, &myVOPtr->volumeType );
		if ( err == E_InvalidMDBdrAlBlSt )
			err = RcdMDBEmbededVolDescriptionErr( GPtr, E_InvalidMDBdrAlBlSt, myMDBPtr );
		
		if ( VolumeObjectIsEmbeddedHFSPlus( ) ) {
			// we changed volume types so let's get the VHB
			(void) ReleaseVolumeBlock( vcb, &myBlockDescriptor, kReleaseBlock );
			myBlockDescriptor.buffer = NULL;
			myMDBPtr = NULL;
			err = GetVolumeObjectVHB( &myBlockDescriptor );
			if ( err != noErr ) {
				if ( GPtr->logLevel >= kDebugLog )
					printf( "\t%s - bad volume header - err %d \n", __FUNCTION__, err );
				WriteError( GPtr, E_InvalidVolumeHeader, 1, 0 );
				err = E_InvalidVolumeHeader;
				goto ReleaseAndBail;
			}

			GetVolumeObjectBlockNum( &blockNum ); // get the new Volume header block number
		}
		else {
			if ( GPtr->logLevel >= kDebugLog )
				printf( "\t%s - bad volume header - err %d \n", __FUNCTION__, err );
			WriteError( GPtr, E_InvalidVolumeHeader, 1, 0 );
			err = E_InvalidVolumeHeader;
			goto ReleaseAndBail;
		}
	}

	totalSectors = ( VolumeObjectIsEmbeddedHFSPlus( ) ) ? myVOPtr->totalEmbeddedSectors : myVOPtr->totalDeviceSectors;

	// indicate what type of volume we are dealing with
	if ( VolumeObjectIsHFSPlus( ) ) {

		myVHBPtr = (HFSPlusVolumeHeader	*) myBlockDescriptor.buffer;
		WriteMsg( GPtr, M_CheckingHFSPlusVolume, kStatusMessage );
		GPtr->numExtents = kHFSPlusExtentDensity;
		vcb->vcbSignature = kHFSPlusSigWord;

		//	Further populate the VCB with VolumeHeader info
		vcb->vcbAlBlSt = myVOPtr->embeddedOffset / 512;
		vcb->vcbEmbeddedOffset = myVOPtr->embeddedOffset;
		realAllocationBlockSize		= myVHBPtr->blockSize;
		realTotalBlocks				= myVHBPtr->totalBlocks;
		vcb->vcbNextCatalogID	= myVHBPtr->nextCatalogID;
		vcb->vcbCreateDate	= myVHBPtr->createDate;
		vcb->vcbAttributes = myVHBPtr->attributes & kHFSCatalogNodeIDsReused;

		if ( myVHBPtr->attributesFile.totalBlocks == 0 )
			vcb->vcbAttributesFile = NULL;	/* XXX memory leak ? */

		//	Make sure the Extents B-Tree is set to use 16-bit key lengths.  
		//	We access it before completely setting up the control block.
		btcb = (BTreeControlBlock *) vcb->vcbExtentsFile->fcbBtree;
		btcb->attributes |= kBTBigKeysMask;
		
		// catch the case where the volume allocation block count is greater than 
		// maximum number of device allocation blocks. - bug 2916021
		numABlks = myVOPtr->totalDeviceSectors / ( myVHBPtr->blockSize / Blk_Size );
		if ( myVHBPtr->totalBlocks > numABlks ) {
			RcdError( GPtr, E_NABlks );
			err = E_NABlks;					
			if ( GPtr->logLevel >= kDebugLog ) {
				printf( "\t%s - volume header total allocation blocks is greater than device size \n", __FUNCTION__ );
				printf( "\tvolume allocation block count %d device allocation block count %d \n", 
						myVHBPtr->totalBlocks, numABlks );
			}
			goto ReleaseAndBail;
		}
	}
	else if ( VolumeObjectIsHFS( ) ) {

		WriteMsg( GPtr, M_CheckingHFSVolume, kStatusMessage );
		GPtr->numExtents			= kHFSExtentDensity;
		vcb->vcbSignature			= myMDBPtr->drSigWord;
		maxNumberOfAllocationBlocks	= 0xFFFF;
		//	set up next file ID, CheckBTreeKey makse sure we are under this value
		vcb->vcbNextCatalogID		= myMDBPtr->drNxtCNID;			
		vcb->vcbCreateDate			= myMDBPtr->drCrDate;

		realAllocationBlockSize		= myMDBPtr->drAlBlkSiz;
		realTotalBlocks				= myMDBPtr->drNmAlBlks;
	}

	GPtr->TarBlock	= blockNum;							//	target block

	//  verify volume allocation info
	//	Note: i is the number of sectors per allocation block
	numABlks = totalSectors;
	minABlkSz = Blk_Size;							//	init minimum ablock size
	//	loop while #ablocks won't fit
	for( i = 2; numABlks > maxNumberOfAllocationBlocks; i++ ) {
		minABlkSz = i * Blk_Size;					//	jack up minimum
		numABlks  = totalSectors / i;				//	recompute #ablocks, assuming this size
	 }

	vcb->vcbBlockSize = realAllocationBlockSize;
	numABlks = totalSectors / ( realAllocationBlockSize / Blk_Size );
	if ( VolumeObjectIsHFSPlus( ) ) {
		// HFS Plus allocation block size must be power of 2
		if ( (realAllocationBlockSize < minABlkSz) || 
			 (realAllocationBlockSize & (realAllocationBlockSize - 1)) != 0 )
			realAllocationBlockSize = 0;
	}
	else {
		if ( (realAllocationBlockSize < minABlkSz) || 
			 (realAllocationBlockSize > Max_ABSiz) || 
			 ((realAllocationBlockSize % Blk_Size) != 0) )
			realAllocationBlockSize = 0;
	}
	
	if ( realAllocationBlockSize == 0 ) {
		RcdError( GPtr, E_ABlkSz );
		err = E_ABlkSz;	  //	bad allocation block size
		goto ReleaseAndBail;
	}
	
	vcb->vcbTotalBlocks	= realTotalBlocks;
	vcb->vcbFreeBlocks	= 0;
	
	//	Only do these tests on HFS volumes, since they are either 
	//	or, getting the VolumeHeader would have already failed.
	if ( VolumeObjectIsHFS( ) ) {
		UInt32					bitMapSizeInSectors;

		// Calculate the volume bitmap size
		bitMapSizeInSectors = ( numABlks + kBitsPerSector - 1 ) / kBitsPerSector;			//	VBM size in blocks

		//¥¥	Calculate the validaty of HFS Allocation blocks, I think realTotalBlocks == numABlks
		numABlks = (totalSectors - 3 - bitMapSizeInSectors) / (realAllocationBlockSize / Blk_Size);	//	actual # of alloc blks

		if ( realTotalBlocks > numABlks ) {
			RcdError( GPtr, E_NABlks );
			err = E_NABlks;								//	invalid number of allocation blocks
			goto ReleaseAndBail;
		}

		if ( myMDBPtr->drVBMSt <= MDB_BlkN ) {
			RcdError(GPtr,E_VBMSt);
			err = E_VBMSt;								//	invalid VBM start block
			goto ReleaseAndBail;
		}	
		vcb->vcbVBMSt = myMDBPtr->drVBMSt;
		
		if (myMDBPtr->drAlBlSt < (myMDBPtr->drVBMSt + bitMapSizeInSectors)) {
			RcdError(GPtr,E_ABlkSt);
			err = E_ABlkSt;								//	invalid starting alloc block
			goto ReleaseAndBail;
		}
		vcb->vcbAlBlSt = myMDBPtr->drAlBlSt;
	}

ReleaseAndBail:
	if (myBlockDescriptor.buffer != NULL)
		(void) ReleaseVolumeBlock(vcb, &myBlockDescriptor, kReleaseBlock);
	
	return( err );		
}


static OSErr ScavengeVolumeType( SGlobPtr GPtr, HFSMasterDirectoryBlock *mdb, UInt32 *volumeType  )
{
	UInt64					vHSector;
	UInt64					startSector;
	UInt64					altVHSector;
	UInt64					hfsPlusSectors = 0;
	UInt32					sectorsPerBlock;
	UInt32					numSectorsToSearch;
	OSErr					err;
	HFSPlusVolumeHeader 	*volumeHeader;
	HFSExtentDescriptor		embededExtent;
	SVCB					*calculatedVCB			= GPtr->calculatedVCB;
	VolumeObjectPtr			myVOPtr;
	UInt16					embedSigWord			= mdb->drEmbedSigWord;
	BlockDescriptor 		block;

	/*
	 * If all of the embedded volume information is zero, then assume
	 * this really is a plain HFS disk like it says.  Otherwise, if
	 * you reinitialize a large HFS Plus volume as HFS, the original
	 * embedded volume's volume header and alternate volume header will
	 * still be there, and we'll try to repair the embedded volume.
	 */
	if (embedSigWord == 0  &&
		mdb->drEmbedExtent.blockCount == 0  &&
		mdb->drEmbedExtent.startBlock == 0)
	{
		*volumeType = kHFSVolumeType;
		return noErr;
	}
	
	myVOPtr = GetVolumeObjectPtr( );
	*volumeType	= kEmbededHFSPlusVolumeType;		//	Assume HFS+
	
	//
	//	First see if it is an HFS+ volume and the relevent structures look OK
	//
	if ( embedSigWord == kHFSPlusSigWord )
	{
		/* look for primary volume header */
		vHSector = (UInt64)mdb->drAlBlSt +
			((UInt64)(mdb->drAlBlkSiz / Blk_Size) * (UInt64)mdb->drEmbedExtent.startBlock) + 2;

		err = GetVolumeBlock(calculatedVCB, vHSector, kGetBlock, &block);
		volumeHeader = (HFSPlusVolumeHeader *) block.buffer;
		if ( err != noErr ) goto AssumeHFS;

		myVOPtr->primaryVHB = vHSector;
		err = ValidVolumeHeader( volumeHeader );
		(void) ReleaseVolumeBlock(calculatedVCB, &block, kReleaseBlock);
		if ( err == noErr ) {
			myVOPtr->flags |= kVO_PriVHBOK;
			return( noErr );
		}
	}
	
	sectorsPerBlock = mdb->drAlBlkSiz / Blk_Size;

	//	Search the end of the disk to see if a Volume Header is present at all
	if ( embedSigWord != kHFSPlusSigWord )
	{
		numSectorsToSearch = mdb->drAlBlkSiz / Blk_Size;
		startSector = myVOPtr->totalDeviceSectors - 4 - numSectorsToSearch;
		
		err = SeekVolumeHeader( GPtr, startSector, numSectorsToSearch, &altVHSector );
		if ( err != noErr ) goto AssumeHFS;
		
		//	We found the Alt VH, so this must be a damaged embeded HFS+ volume
		//	Now Scavenge for the Primary VolumeHeader
		myVOPtr->alternateVHB = altVHSector;
		myVOPtr->flags |= kVO_AltVHBOK;
		startSector = mdb->drAlBlSt + (4 * sectorsPerBlock);		// Start looking at 4th HFS allocation block
		numSectorsToSearch = 10 * sectorsPerBlock;			// search for VH in next 10 allocation blocks
		
		err = SeekVolumeHeader( GPtr, startSector, numSectorsToSearch, &vHSector );
		if ( err != noErr ) goto AssumeHFS;
	
		myVOPtr->primaryVHB = vHSector;
		myVOPtr->flags |= kVO_PriVHBOK;
		hfsPlusSectors	= altVHSector - vHSector + 1 + 2 + 1;	// numSectors + BB + end
		
		//	Fix the embeded extent
		embededExtent.blockCount	= hfsPlusSectors / sectorsPerBlock;
		embededExtent.startBlock	= (vHSector - 2 - mdb->drAlBlSt ) / sectorsPerBlock;
		embedSigWord				= kHFSPlusSigWord;
		
		myVOPtr->embeddedOffset = 
			(embededExtent.startBlock * mdb->drAlBlkSiz) + (mdb->drAlBlSt * Blk_Size);
	}
	else
	{
		embedSigWord				= mdb->drEmbedSigWord;
		embededExtent.blockCount	= mdb->drEmbedExtent.blockCount;
		embededExtent.startBlock	= mdb->drEmbedExtent.startBlock;
	}
		
	if ( embedSigWord == kHFSPlusSigWord )
	{
		startSector = 2 + mdb->drAlBlSt +
			((UInt64)embededExtent.startBlock * (mdb->drAlBlkSiz / Blk_Size));
			
		err = SeekVolumeHeader( GPtr, startSector, mdb->drAlBlkSiz / Blk_Size, &vHSector );
		if ( err != noErr ) goto AssumeHFS;
	
		//	Now replace the bad fields and mark the error
		mdb->drEmbedExtent.blockCount	= embededExtent.blockCount;
		mdb->drEmbedExtent.startBlock	= embededExtent.startBlock;
		mdb->drEmbedSigWord				= kHFSPlusSigWord;
		mdb->drAlBlSt					+= vHSector - startSector;								//	Fix the bad field
		myVOPtr->totalEmbeddedSectors = (mdb->drAlBlkSiz / Blk_Size) * mdb->drEmbedExtent.blockCount;
		myVOPtr->embeddedOffset =
			(mdb->drEmbedExtent.startBlock * mdb->drAlBlkSiz) + (mdb->drAlBlSt * Blk_Size);
		myVOPtr->primaryVHB = vHSector;
		myVOPtr->flags |= kVO_PriVHBOK;

		GPtr->VIStat = GPtr->VIStat | S_MDB;													//	write out our MDB
		return( E_InvalidMDBdrAlBlSt );
	}
	
AssumeHFS:
	*volumeType	= kHFSVolumeType;
	return( noErr );
	
} /* ScavengeVolumeType */


static OSErr SeekVolumeHeader( SGlobPtr GPtr, UInt64 startSector, UInt32 numSectors, UInt64 *vHSector )
{
	OSErr  err;
	HFSPlusVolumeHeader  *volumeHeader;
	SVCB  *calculatedVCB = GPtr->calculatedVCB;
	BlockDescriptor  block;

	for ( *vHSector = startSector ; *vHSector < startSector + numSectors  ; (*vHSector)++ )
	{
		err = GetVolumeBlock(calculatedVCB, *vHSector, kGetBlock, &block);
		volumeHeader = (HFSPlusVolumeHeader *) block.buffer;
		if ( err != noErr ) return( err );

		err = ValidVolumeHeader(volumeHeader);

		(void) ReleaseVolumeBlock(calculatedVCB, &block, kReleaseBlock);
		if ( err == noErr )
			return( noErr );
	}
	
	return( fnfErr );
}


#if 0 // not used at this time
static OSErr CheckWrapperExtents( SGlobPtr GPtr, HFSMasterDirectoryBlock *mdb )
{
	OSErr	err = noErr;
	
	//	See if Norton Disk Doctor 2.0 corrupted the catalog's first extent
	if ( mdb->drCTExtRec[0].startBlock >= mdb->drEmbedExtent.startBlock)
	{
		//	Fix the field in the in-memory copy, and record the error
		mdb->drCTExtRec[0].startBlock = mdb->drXTExtRec[0].startBlock + mdb->drXTExtRec[0].blockCount;
		GPtr->VIStat = GPtr->VIStat | S_MDB;													//	write out our MDB
		err = RcdInvalidWrapperExtents( GPtr, E_InvalidWrapperExtents );
	}
	
	return err;
}
#endif

/*------------------------------------------------------------------------------

Function:	CreateExtentsBTreeControlBlock

Function:	Create the calculated ExtentsBTree Control Block
			
Input:		GPtr	-	pointer to scavenger global area

Output:				-	0	= no error
						n 	= error code 
------------------------------------------------------------------------------*/

OSErr	CreateExtentsBTreeControlBlock( SGlobPtr GPtr )
{
	OSErr					err;
	SInt32					size;
	UInt32					numABlks;
	BTHeaderRec				header;
	BTreeControlBlock *  	btcb;
	SVCB *  				vcb;
	BlockDescriptor 	 	block;
	Boolean					isHFSPlus;

	//	Set up
	isHFSPlus = VolumeObjectIsHFSPlus( );
	GPtr->TarID = kHFSExtentsFileID;	// target = extent file
	GPtr->TarBlock	= kHeaderNodeNum;	// target block = header node
	vcb = GPtr->calculatedVCB;
	btcb = GPtr->calculatedExtentsBTCB;
	block.buffer = NULL;

	// get Volume Header (HFS+) or Master Directory (HFS) block
	err = GetVolumeObjectVHBorMDB( &block );
	if (err) goto exit;
	//
	//	check out allocation info for the Extents File 
	//
	if (isHFSPlus)
	{
		HFSPlusVolumeHeader *volumeHeader;

		volumeHeader = (HFSPlusVolumeHeader *) block.buffer;
	
		CopyMemory(volumeHeader->extentsFile.extents, GPtr->calculatedExtentsFCB->fcbExtents32, sizeof(HFSPlusExtentRecord) );
		
		err = CheckFileExtents( GPtr, kHFSExtentsFileID, 0, (void *)GPtr->calculatedExtentsFCB->fcbExtents32, &numABlks );	//	check out extent info

		if (err) goto exit;

		if ( volumeHeader->extentsFile.totalBlocks != numABlks )				//	check out the PEOF
		{
			RcdError( GPtr, E_ExtPEOF );
			err = E_ExtPEOF;
			goto exit;
		}
		else
		{
			GPtr->calculatedExtentsFCB->fcbLogicalSize  = volumeHeader->extentsFile.logicalSize;					//	Set Extents tree's LEOF
			GPtr->calculatedExtentsFCB->fcbPhysicalSize = (UInt64)volumeHeader->extentsFile.totalBlocks * 
														  (UInt64)volumeHeader->blockSize;	//	Set Extents tree's PEOF
		}

		//
		//	Set up the minimal BTreeControlBlock structure
		//
		
		//	Read the BTreeHeader from disk & also validate it's node size.
		err = GetBTreeHeader(GPtr, GPtr->calculatedExtentsFCB, &header);
		if (err) goto exit;
		
		btcb->maxKeyLength		= kHFSPlusExtentKeyMaximumLength;				//	max key length
		btcb->keyCompareProc	= (void *)CompareExtentKeysPlus;
		btcb->attributes		|=kBTBigKeysMask;								//	HFS+ Extent files have 16-bit key length
		btcb->leafRecords		= header.leafRecords;
		btcb->treeDepth			= header.treeDepth;
		btcb->rootNode			= header.rootNode;
		btcb->firstLeafNode		= header.firstLeafNode;
		btcb->lastLeafNode		= header.lastLeafNode;

		btcb->nodeSize			= header.nodeSize;
		btcb->totalNodes		= ( GPtr->calculatedExtentsFCB->fcbPhysicalSize / btcb->nodeSize );
		btcb->freeNodes			= btcb->totalNodes;								//	start with everything free

		//	Make sure the header nodes size field is correct by looking at the 1st record offset
		err	= CheckNodesFirstOffset( GPtr, btcb );
		if ( (err != noErr) && (btcb->nodeSize != 1024) )		//	default HFS+ Extents node size is 1024
		{
			btcb->nodeSize			= 1024;
			btcb->totalNodes		= ( GPtr->calculatedExtentsFCB->fcbPhysicalSize / btcb->nodeSize );
			btcb->freeNodes			= btcb->totalNodes;								//	start with everything free
			
			err = CheckNodesFirstOffset( GPtr, btcb );
			if (err) goto exit;
			
			GPtr->EBTStat |= S_BTH;								//	update the Btree header
		}
	}
	else	// Classic HFS
	{
		HFSMasterDirectoryBlock	*alternateMDB;

		alternateMDB = (HFSMasterDirectoryBlock *) block.buffer;
	
		CopyMemory(alternateMDB->drXTExtRec, GPtr->calculatedExtentsFCB->fcbExtents16, sizeof(HFSExtentRecord) );
	//	ExtDataRecToExtents(alternateMDB->drXTExtRec, GPtr->calculatedExtentsFCB->fcbExtents);

		
		err = CheckFileExtents( GPtr, kHFSExtentsFileID, 0, (void *)GPtr->calculatedExtentsFCB->fcbExtents16, &numABlks );	/* check out extent info */	
		if (err) goto exit;
	
		if (alternateMDB->drXTFlSize != ((UInt64)numABlks * (UInt64)GPtr->calculatedVCB->vcbBlockSize))//	check out the PEOF
		{
			RcdError(GPtr,E_ExtPEOF);
			err = E_ExtPEOF;
			goto exit;
		}
		else
		{
			GPtr->calculatedExtentsFCB->fcbPhysicalSize = alternateMDB->drXTFlSize;		//	set up PEOF and EOF in FCB
			GPtr->calculatedExtentsFCB->fcbLogicalSize = GPtr->calculatedExtentsFCB->fcbPhysicalSize;
		}

		//
		//	Set up the minimal BTreeControlBlock structure
		//
			
		//	Read the BTreeHeader from disk & also validate it's node size.
		err = GetBTreeHeader(GPtr, GPtr->calculatedExtentsFCB, &header);
		if (err) goto exit;

		btcb->maxKeyLength	= kHFSExtentKeyMaximumLength;						//	max key length
		btcb->keyCompareProc = (void *)CompareExtentKeys;
		btcb->leafRecords	= header.leafRecords;
		btcb->treeDepth		= header.treeDepth;
		btcb->rootNode		= header.rootNode;
		btcb->firstLeafNode	= header.firstLeafNode;
		btcb->lastLeafNode	= header.lastLeafNode;
		
		btcb->nodeSize		= header.nodeSize;
		btcb->totalNodes	= (GPtr->calculatedExtentsFCB->fcbPhysicalSize / btcb->nodeSize );
		btcb->freeNodes		= btcb->totalNodes;									//	start with everything free

		//	Make sure the header nodes size field is correct by looking at the 1st record offset
		err = CheckNodesFirstOffset( GPtr, btcb );
		if (err) goto exit;
	}

	if ( header.btreeType != kHFSBTreeType )
	{
		GPtr->EBTStat |= S_ReservedBTH;						//	Repair reserved fields in Btree header
	}

	//
	//	set up our DFA extended BTCB area.  Will we have enough memory on all HFS+ volumes.
	//
	btcb->refCon = (UInt32) AllocateClearMemory( sizeof(BTreeExtensionsRec) );			// allocate space for our BTCB extensions
	if ( btcb->refCon == (UInt32) nil ) {
		err = R_NoMem;
		goto exit;
	}
	size = (btcb->totalNodes + 7) / 8;											//	size of BTree bit map
	((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr = AllocateClearMemory(size);			//	get precleared bitmap
	if ( ((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr == nil )
	{
		err = R_NoMem;
		goto exit;
	}

	((BTreeExtensionsRec*)btcb->refCon)->BTCBMSize = size;				//	remember how long it is
	((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount = header.freeNodes;//	keep track of real free nodes for progress
exit:
	if ( block.buffer != NULL )
		(void) ReleaseVolumeBlock(vcb, &block, kReleaseBlock);
	
	return (err);
}



/*------------------------------------------------------------------------------

Function:	CheckNodesFirstOffset

Function:	Minimal check verifies that the 1st offset is within bounds.  If it's not
			the nodeSize may be wrong.  In the future this routine could be modified
			to try different size values until one fits.
			
------------------------------------------------------------------------------*/
#define GetRecordOffset(btreePtr,node,index)		(*(short *) ((UInt8 *)(node) + (btreePtr)->nodeSize - ((index) << 1) - kOffsetSize))
static	OSErr	CheckNodesFirstOffset( SGlobPtr GPtr, BTreeControlBlock *btcb )
{
	NodeRec		nodeRec;
	UInt16		offset;
	OSErr		err;
			
	(void) SetFileBlockSize(btcb->fcbPtr, btcb->nodeSize);

	err = GetNode( btcb, kHeaderNodeNum, &nodeRec );
	
	if ( err == noErr )
	{
		offset	= GetRecordOffset( btcb, (NodeDescPtr)nodeRec.buffer, 0 );
		if ( (offset < sizeof (BTNodeDescriptor)) ||			// offset < minimum
			 (offset & 1) ||									// offset is odd
			 (offset >= btcb->nodeSize) )						// offset beyond end of node
		{
			err	= fsBTInvalidNodeErr;
		}
	}
	
	if ( err != noErr )
		RcdError( GPtr, E_InvalidNodeSize );

	(void) ReleaseNode(btcb, &nodeRec);

	return( err );
}



/*------------------------------------------------------------------------------

Function:	ExtBTChk - (Extent BTree Check)

Function:	Verifies the extent BTree structure.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		ExtBTChk	-	function result:			
								0	= no error
								n 	= error code 
------------------------------------------------------------------------------*/

OSErr ExtBTChk( SGlobPtr GPtr )
{
	OSErr					err;

	//	Set up
	GPtr->TarID		= kHFSExtentsFileID;				//	target = extent file
	GetVolumeObjectBlockNum( &GPtr->TarBlock );			//	target block = VHB/MDB
 
	//
	//	check out the BTree structure
	//

	err = BTCheck(GPtr, kCalculatedExtentRefNum, NULL);
	ReturnIfError( err );														//	invalid extent file BTree

	//
	//	check out the allocation map structure
	//

	err = BTMapChk( GPtr, kCalculatedExtentRefNum );
	ReturnIfError( err );														//	Invalid extent BTree map

	//
	//	compare BTree header record on disk with scavenger's BTree header record 
	//

	err = CmpBTH( GPtr, kCalculatedExtentRefNum );
	ReturnIfError( err );

	//
	//	compare BTree map on disk with scavenger's BTree map
	//

	err = CmpBTM( GPtr, kCalculatedExtentRefNum );

	return( err );
}



/*------------------------------------------------------------------------------

Function:	ExtFlChk - (Extent File Check)

Function:	Verifies the extent file structure.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		ExtFlChk		-	function result:			
								0	= no error
								+n 	= error code
------------------------------------------------------------------------------*/

OSErr ExtFlChk( SGlobPtr GPtr )
{
	UInt32			attributes;
	void			*p;
	OSErr			result;
	SVCB 			*vcb;
	Boolean			isHFSPlus;
	BlockDescriptor  block;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	block.buffer = NULL;
	
	//
	//	process the bad block extents (created by the disk init pkg to hide badspots)
	//
	vcb = GPtr->calculatedVCB;

	result = GetVolumeObjectVHBorMDB( &block );
	if ( result != noErr ) goto ExitThisRoutine;		//	error, could't get it

	p = (void *) block.buffer;
	attributes = isHFSPlus == true ? ((HFSPlusVolumeHeader*)p)->attributes : ((HFSMasterDirectoryBlock*)p)->drAtrb;

	//¥¥ Does HFS+ honnor the same mask?
	if ( attributes & kHFSVolumeSparedBlocksMask )				//	if any badspots
	{
		HFSPlusExtentRecord		zeroXdr;						//	dummy passed to 'CheckFileExtents'
		UInt32					numBadBlocks;
		
		ClearMemory ( zeroXdr, sizeof( HFSPlusExtentRecord ) );
		result = CheckFileExtents( GPtr, kHFSBadBlockFileID, 0, (void *)zeroXdr, &numBadBlocks );	//	check and mark bitmap
	}

ExitThisRoutine:
	if ( block.buffer != NULL )
		(void) ReleaseVolumeBlock(vcb, &block, kReleaseBlock);

	return (result);
}


/*------------------------------------------------------------------------------

Function:	CreateCatalogBTreeControlBlock

Function:	Create the calculated CatalogBTree Control Block
			
Input:		GPtr	-	pointer to scavenger global area

Output:				-	0	= no error
						n 	= error code 
------------------------------------------------------------------------------*/
OSErr	CreateCatalogBTreeControlBlock( SGlobPtr GPtr )
{
	OSErr					err;
	SInt32					size;
	UInt32					numABlks;
	BTHeaderRec				header;
	BTreeControlBlock *  	btcb;
	SVCB *  				vcb;
	BlockDescriptor  		block;
	Boolean					isHFSPlus;

	//	Set up
	isHFSPlus = VolumeObjectIsHFSPlus( );
	GPtr->TarID		= kHFSCatalogFileID;
	GPtr->TarBlock	= kHeaderNodeNum;
	vcb = GPtr->calculatedVCB;
	btcb = GPtr->calculatedCatalogBTCB;
 	block.buffer = NULL;

	err = GetVolumeObjectVHBorMDB( &block );
	if ( err != noErr ) goto ExitThisRoutine;		//	error, could't get it
	//
	//	check out allocation info for the Catalog File 
	//
	if (isHFSPlus)
	{
		HFSPlusVolumeHeader * volumeHeader;

		volumeHeader = (HFSPlusVolumeHeader *) block.buffer;

		CopyMemory(volumeHeader->catalogFile.extents, GPtr->calculatedCatalogFCB->fcbExtents32, sizeof(HFSPlusExtentRecord) );

		err = CheckFileExtents( GPtr, kHFSCatalogFileID, 0, (void *)GPtr->calculatedCatalogFCB->fcbExtents32, &numABlks );	
		if (err) goto exit;

		if ( volumeHeader->catalogFile.totalBlocks != numABlks )
		{
			RcdError( GPtr, E_CatPEOF );
			err = E_CatPEOF;
			goto exit;
		}
		else
		{
			GPtr->calculatedCatalogFCB->fcbLogicalSize  = volumeHeader->catalogFile.logicalSize;
			GPtr->calculatedCatalogFCB->fcbPhysicalSize = (UInt64)volumeHeader->catalogFile.totalBlocks * 
														  (UInt64)volumeHeader->blockSize;  
		}

		//
		//	Set up the minimal BTreeControlBlock structure
		//

		//	read the BTreeHeader from disk & also validate it's node size.
		err = GetBTreeHeader(GPtr, GPtr->calculatedCatalogFCB, &header);
		if (err) goto exit;

		btcb->maxKeyLength		= kHFSPlusCatalogKeyMaximumLength;					//	max key length

		/*
		 * Figure out the type of key string compare
		 * (case-insensitive or case-sensitive)
		 *
		 * To do: should enforce an "HX" volume is require for kHFSBinaryCompare.
		 */
		if (header.keyCompareType == kHFSBinaryCompare)
		{
			btcb->keyCompareProc = (void *)CaseSensitiveCatalogKeyCompare;
			PrintStatus(GPtr, M_CaseSensitive, 0);
		}
		else
		{
			btcb->keyCompareProc = (void *)CompareExtendedCatalogKeys;
		}
		btcb->keyCompareType		= header.keyCompareType;
		btcb->leafRecords		= header.leafRecords;
		btcb->nodeSize			= header.nodeSize;
		btcb->totalNodes		= ( GPtr->calculatedCatalogFCB->fcbPhysicalSize / btcb->nodeSize );
		btcb->freeNodes			= btcb->totalNodes;									//	start with everything free
		btcb->attributes		|=(kBTBigKeysMask + kBTVariableIndexKeysMask);		//	HFS+ Catalog files have large, variable-sized keys

		btcb->treeDepth		= header.treeDepth;
		btcb->rootNode		= header.rootNode;
		btcb->firstLeafNode	= header.firstLeafNode;
		btcb->lastLeafNode	= header.lastLeafNode;


		//	Make sure the header nodes size field is correct by looking at the 1st record offset
		err	= CheckNodesFirstOffset( GPtr, btcb );
		if ( (err != noErr) && (btcb->nodeSize != 4096) )		//	default HFS+ Catalog node size is 4096
		{
			btcb->nodeSize			= 4096;
			btcb->totalNodes		= ( GPtr->calculatedCatalogFCB->fcbPhysicalSize / btcb->nodeSize );
			btcb->freeNodes			= btcb->totalNodes;								//	start with everything free
			
			err = CheckNodesFirstOffset( GPtr, btcb );
			if (err) goto exit;
			
			GPtr->CBTStat |= S_BTH;								//	update the Btree header
		}
	}
	else	//	HFS
	{
		HFSMasterDirectoryBlock	*alternateMDB;

		alternateMDB = (HFSMasterDirectoryBlock	*) block.buffer;

		CopyMemory( alternateMDB->drCTExtRec, GPtr->calculatedCatalogFCB->fcbExtents16, sizeof(HFSExtentRecord) );
	//	ExtDataRecToExtents(alternateMDB->drCTExtRec, GPtr->calculatedCatalogFCB->fcbExtents);

		err = CheckFileExtents( GPtr, kHFSCatalogFileID, 0, (void *)GPtr->calculatedCatalogFCB->fcbExtents16, &numABlks );	/* check out extent info */	
		if (err) goto exit;

		if (alternateMDB->drCTFlSize != ((UInt64)numABlks * (UInt64)vcb->vcbBlockSize))	//	check out the PEOF
		{
			RcdError( GPtr, E_CatPEOF );
			err = E_CatPEOF;
			goto exit;
		}
		else
		{
			GPtr->calculatedCatalogFCB->fcbPhysicalSize	= alternateMDB->drCTFlSize;			//	set up PEOF and EOF in FCB
			GPtr->calculatedCatalogFCB->fcbLogicalSize	= GPtr->calculatedCatalogFCB->fcbPhysicalSize;
		}

		//
		//	Set up the minimal BTreeControlBlock structure
		//

		//	read the BTreeHeader from disk & also validate it's node size.
		err = GetBTreeHeader(GPtr, GPtr->calculatedCatalogFCB, &header);
		if (err) goto exit;

		btcb->maxKeyLength		= kHFSCatalogKeyMaximumLength;						//	max key length
		btcb->keyCompareProc	= (void *) CompareCatalogKeys;
		btcb->leafRecords		= header.leafRecords;
		btcb->nodeSize			= header.nodeSize;
		btcb->totalNodes		= (GPtr->calculatedCatalogFCB->fcbPhysicalSize / btcb->nodeSize );
		btcb->freeNodes			= btcb->totalNodes;									//	start with everything free

		btcb->treeDepth		= header.treeDepth;
		btcb->rootNode		= header.rootNode;
		btcb->firstLeafNode	= header.firstLeafNode;
		btcb->lastLeafNode	= header.lastLeafNode;

		//	Make sure the header nodes size field is correct by looking at the 1st record offset
		err = CheckNodesFirstOffset( GPtr, btcb );
		if (err) goto exit;
	}
#if 0	
	printf("   Catalog B-tree is %qd bytes\n", (UInt64)btcb->totalNodes * (UInt64) btcb->nodeSize);
#endif

	if ( header.btreeType != kHFSBTreeType )
	{
		GPtr->CBTStat |= S_ReservedBTH;						//	Repair reserved fields in Btree header
	}

	//
	//	set up our DFA extended BTCB area.  Will we have enough memory on all HFS+ volumes.
	//

	btcb->refCon = (UInt32) AllocateClearMemory( sizeof(BTreeExtensionsRec) );			// allocate space for our BTCB extensions
	if ( btcb->refCon == (UInt32)nil ) {
		err = R_NoMem;
		goto exit;
	}
	size = (btcb->totalNodes + 7) / 8;											//	size of BTree bit map
	((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr = AllocateClearMemory(size);			//	get precleared bitmap
	if ( ((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr == nil )
	{
		err = R_NoMem;
		goto exit;
	}

	((BTreeExtensionsRec*)btcb->refCon)->BTCBMSize			= size;						//	remember how long it is
	((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount	= header.freeNodes;		//	keep track of real free nodes for progress

    /* it should be OK at this point to get volume name and stuff it into our global */
    {
        OSErr				result;
        UInt16				recSize;
        CatalogKey			key;
        CatalogRecord		record;
    
        BuildCatalogKey( kHFSRootFolderID, NULL, isHFSPlus, &key );
        result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, NULL, &record, &recSize, NULL );
        if ( result == noErr ) {
            if ( isHFSPlus ) {
                size_t 						len;
                HFSPlusCatalogThread *		recPtr = &record.hfsPlusThread;
                (void) utf_encodestr( recPtr->nodeName.unicode,
                                      recPtr->nodeName.length * 2,
                                      GPtr->volumeName, &len );
                GPtr->volumeName[len] = '\0';
            }
            else {
                HFSCatalogThread *		recPtr = &record.hfsThread;
                bcopy( &recPtr->nodeName[1], GPtr->volumeName, recPtr->nodeName[0] );
                GPtr->volumeName[ recPtr->nodeName[0] ] = '\0';
           }
        }
    }

exit:
ExitThisRoutine:
	if ( block.buffer != NULL )
		(void) ReleaseVolumeBlock(vcb, &block, kReleaseBlock);

	return (err);
}


/*------------------------------------------------------------------------------

Function:	CreateExtendedAllocationsFCB

Function:	Create the calculated ExtentsBTree Control Block
			
Input:		GPtr	-	pointer to scavenger global area

Output:				-	0	= no error
						n 	= error code 
------------------------------------------------------------------------------*/
OSErr	CreateExtendedAllocationsFCB( SGlobPtr GPtr )
{
	OSErr					err = 0;
	UInt32					numABlks;
	SVCB * 					vcb;
	Boolean					isHFSPlus;
	BlockDescriptor 		block;

	//	Set up
	isHFSPlus = VolumeObjectIsHFSPlus( );
	GPtr->TarID = kHFSAllocationFileID;
	GetVolumeObjectBlockNum( &GPtr->TarBlock );			//	target block = VHB/MDB
 	vcb = GPtr->calculatedVCB;
	block.buffer = NULL;
 
	//
	//	check out allocation info for the allocation File 
	//

	if ( isHFSPlus )
	{
		SFCB * fcb;
		HFSPlusVolumeHeader *volumeHeader;
		
		err = GetVolumeObjectVHB( &block );
		if ( err != noErr )
			goto exit;
		volumeHeader = (HFSPlusVolumeHeader *) block.buffer;

		fcb = GPtr->calculatedAllocationsFCB;
		CopyMemory( volumeHeader->allocationFile.extents, fcb->fcbExtents32, sizeof(HFSPlusExtentRecord) );

		err = CheckFileExtents( GPtr, kHFSAllocationFileID, 0, (void *)fcb->fcbExtents32, &numABlks );
		if (err) goto exit;

		//
		// The allocation file will get processed in whole allocation blocks, or
		// maximal-sized cache blocks, whichever is smaller.  This means the cache
		// doesn't need to cope with buffers that are larger than a cache block.
		//
		// The definition of CACHE_IOSIZE below matches the definition in fsck_hfs.c.
		// If you change it here, then change it there, too.  Or else put it in some
		// common header file.
		#define CACHE_IOSIZE	32768
		if (vcb->vcbBlockSize < CACHE_IOSIZE)
			(void) SetFileBlockSize (fcb, vcb->vcbBlockSize);
		else
			(void) SetFileBlockSize (fcb, CACHE_IOSIZE);
	
		if ( volumeHeader->allocationFile.totalBlocks != numABlks )
		{
			RcdError( GPtr, E_CatPEOF );
			err = E_CatPEOF;
			goto exit;
		}
		else
		{
			fcb->fcbLogicalSize  = volumeHeader->allocationFile.logicalSize;
			fcb->fcbPhysicalSize = (UInt64) volumeHeader->allocationFile.totalBlocks * 
								   (UInt64) volumeHeader->blockSize; 
		}

		/* while we're here, also get startup file extents... */
		fcb = GPtr->calculatedStartupFCB;
		CopyMemory( volumeHeader->startupFile.extents, fcb->fcbExtents32, sizeof(HFSPlusExtentRecord) );

		err = CheckFileExtents( GPtr, kHFSStartupFileID, 0, (void *)fcb->fcbExtents32, &numABlks );
		if (err) goto exit;

		fcb->fcbLogicalSize  = volumeHeader->startupFile.logicalSize;
		fcb->fcbPhysicalSize = (UInt64) volumeHeader->startupFile.totalBlocks * 
								(UInt64) volumeHeader->blockSize; 
	}

exit:
	if (block.buffer)
		(void) ReleaseVolumeBlock(vcb, &block, kReleaseBlock);
	
	return (err);

}


/*------------------------------------------------------------------------------

Function:	CatHChk - (Catalog Hierarchy Check)

Function:	Verifies the catalog hierarchy.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		CatHChk	-	function result:			
								0	= no error
								n 	= error code 
------------------------------------------------------------------------------*/

OSErr CatHChk( SGlobPtr GPtr )
{
	SInt16					i;
	OSErr					result;
	UInt16 					recSize;
	SInt16					selCode;
	UInt32					hint;
	UInt32					dirCnt;
	UInt32					filCnt;
	SInt16					rtdirCnt;
	SInt16					rtfilCnt;
	SVCB					*calculatedVCB;
	SDPR					*dprP;
	SDPR					*dprP1;
	CatalogKey				foundKey;
	Boolean					validKeyFound;
	CatalogKey				key;
	CatalogRecord			record;
	CatalogRecord			record2;
	HFSPlusCatalogFolder	*largeCatalogFolderP;
	HFSPlusCatalogFile		*largeCatalogFileP;
	HFSCatalogFile			*smallCatalogFileP;
	HFSCatalogFolder		*smallCatalogFolderP;
	CatalogName				catalogName;
	UInt32					valence;
	CatalogRecord			threadRecord;
	HFSCatalogNodeID		parID;
	Boolean					isHFSPlus;

	//	set up
	isHFSPlus = VolumeObjectIsHFSPlus( );
	calculatedVCB	= GPtr->calculatedVCB;
	GPtr->TarID		= kHFSCatalogFileID;						/* target = catalog file */
	GPtr->TarBlock	= 0;										/* no target block yet */

	//
	//	position to the beginning of catalog
	//
	
	//¥¥ Can we ignore this part by just taking advantage of setting the selCode = 0x8001;
	{ 
		BuildCatalogKey( 1, (const CatalogName *)nil, isHFSPlus, &key );
		result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &threadRecord, &recSize, &hint );
	
		GPtr->TarBlock = hint;									/* set target block */
		if ( result != btNotFound )
		{
			RcdError( GPtr, E_CatRec );
			return( E_CatRec );
		}
	}
	
	GPtr->DirLevel    = 1;
	dprP              = &(*GPtr->DirPTPtr)[0];					
	dprP->directoryID = 1;

	dirCnt = filCnt = rtdirCnt = rtfilCnt = 0;

	result	= noErr;
	selCode = 0x8001;  /* start with root directory */			

	//
	//	enumerate the entire catalog 
	//
	while ( (GPtr->DirLevel > 0) && (result == noErr) )
	{
		dprP = &(*GPtr->DirPTPtr)[GPtr->DirLevel -1];
		
		validKeyFound = true;
		record.recordType = 0;
		
		//	get the next record
		result = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recSize, &hint );
		
		GPtr->TarBlock = hint;									/* set target block */
		if ( result != noErr )
		{
			if ( result == btNotFound )
			{
				result = noErr;
				validKeyFound = false;
			}
			else
			{
				result = IntError( GPtr, result );				/* error from BTGetRecord */
				return( result );
			}
		}
		selCode = 1;											/* get next rec from now on */

		GPtr->itemsProcessed++;
		
		//
		//	 if same ParID ...
		//
		parID = isHFSPlus == true ? foundKey.hfsPlus.parentID : foundKey.hfs.parentID;
		if ( (validKeyFound == true) && (parID == dprP->directoryID) )
		{
			dprP->offspringIndex++;								/* increment offspring index */

			//	if new directory ...
	
			if ( record.recordType == kHFSPlusFolderRecord )
			{
 				result = CheckForStop( GPtr ); ReturnIfError( result );				//	Permit the user to interrupt
			
				largeCatalogFolderP = (HFSPlusCatalogFolder *) &record;				
				GPtr->TarID = largeCatalogFolderP->folderID;				//	target ID = directory ID 
				GPtr->CNType = record.recordType;							//	target CNode type = directory ID 
				CopyCatalogName( (const CatalogName *) &foundKey.hfsPlus.nodeName, &GPtr->CName, isHFSPlus );

				if ( dprP->directoryID > 1 )
				{
					GPtr->DirLevel++;										//	we have a new directory level 
					dirCnt++;
				}
				if ( dprP->directoryID == kHFSRootFolderID )				//	bump root dir count 
					rtdirCnt++;

				if ( GPtr->DirLevel > CMMaxDepth )
				{
					RcdError(GPtr,E_CatDepth);								//	error, exceeded max catalog depth
					return noErr;											//	abort this check, but let other checks proceed
				}

				dprP = &(*GPtr->DirPTPtr)[GPtr->DirLevel -1];
				dprP->directoryID		= largeCatalogFolderP->folderID;
				dprP->offspringIndex	= 1;
				dprP->directoryHint		= hint;
				dprP->parentDirID		= foundKey.hfsPlus.parentID;
				CopyCatalogName( (const CatalogName *) &foundKey.hfsPlus.nodeName, &dprP->directoryName, isHFSPlus );

				for ( i = 1; i < GPtr->DirLevel; i++ )
				{
					dprP1 = &(*GPtr->DirPTPtr)[i -1];
					if (dprP->directoryID == dprP1->directoryID)
					{
						RcdError( GPtr,E_DirLoop );							//	loop in directory hierarchy 
						return( E_DirLoop );
					}
				}
				
				/* 
				 * Find thread record
				 */
				BuildCatalogKey( dprP->directoryID, (const CatalogName *) nil, isHFSPlus, &key );
				result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &threadRecord, &recSize, &hint );
				if ( result != noErr ) {
					char idStr[16];
					struct MissingThread *mtp;
					
					/* Report the error */
					sprintf(idStr, "%d", dprP->directoryID);
					PrintError(GPtr, E_NoThd, 1, idStr);
					
					/* HFS will exit here */
					if ( !isHFSPlus )
						return (E_NoThd);
					/* 
					 * A directory thread is missing.  If we can find this
					 * ID on the missing-thread list then we know where the
					 * child entries reside and can resume our enumeration.
					 */
					for (mtp = GPtr->missingThreadList; mtp != NULL; mtp = mtp->link) {
						if (mtp->threadID == dprP->directoryID) {
							mtp->thread.recordType = kHFSPlusFolderThreadRecord;
							mtp->thread.parentID = dprP->parentDirID;
							CopyCatalogName(&dprP->directoryName, (CatalogName *)&mtp->thread.nodeName, isHFSPlus);

							/* Reposition to the first child of target directory */
							result = SearchBTreeRecord(GPtr->calculatedCatalogFCB, &mtp->nextKey,
							                           kNoHint, &foundKey, &threadRecord, &recSize, &hint);
							if (result) {
								return (E_NoThd);
							}
							selCode = 0; /* use current record instead of next */
							break;
						}
					}
					if (selCode != 0) {
						/* 
						 * A directory thread is missing but we know this
						 * directory has no children (since we didn't find
						 * its ID on the missing-thread list above).
						 *
						 * At this point we can resume the enumeration at
						 * our previous position in our parent directory.
						 */
						goto resumeAtParent;
					}
				}	
				dprP->threadHint = hint;
				GPtr->TarBlock = hint; 
			}

			//	LargeCatalogFile
			else if ( record.recordType == kHFSPlusFileRecord )
			{
				largeCatalogFileP = (HFSPlusCatalogFile *) &record;
				GPtr->TarID = largeCatalogFileP->fileID;					//	target ID = file number 
				GPtr->CNType = record.recordType;							//	target CNode type = thread 
				CopyCatalogName( (const CatalogName *) &foundKey.hfsPlus.nodeName, &GPtr->CName, isHFSPlus );
				filCnt++;
				if (dprP->directoryID == kHFSRootFolderID)
					rtfilCnt++;
			}	

			else if ( record.recordType == kHFSFolderRecord )
			{
 				result = CheckForStop( GPtr ); ReturnIfError( result );				//	Permit the user to interrupt
			
				smallCatalogFolderP = (HFSCatalogFolder *) &record;				
				GPtr->TarID = smallCatalogFolderP->folderID;				/* target ID = directory ID */
				GPtr->CNType = record.recordType;							/* target CNode type = directory ID */
				CopyCatalogName( (const CatalogName *) &key.hfs.nodeName, &GPtr->CName, isHFSPlus );	/* target CName = directory name */

				if (dprP->directoryID > 1)
				{
					GPtr->DirLevel++;										/* we have a new directory level */
					dirCnt++;
				}
				if (dprP->directoryID == kHFSRootFolderID)					/* bump root dir count */
					rtdirCnt++;

				if (GPtr->DirLevel > CMMaxDepth)
				{
					RcdError(GPtr,E_CatDepth);								/* error, exceeded max catalog depth */
					return noErr;											/* abort this check, but let other checks proceed */
				}

				dprP = &(*GPtr->DirPTPtr)[GPtr->DirLevel -1];			
				dprP->directoryID		= smallCatalogFolderP->folderID;
				dprP->offspringIndex	= 1;
				dprP->directoryHint		= hint;
				dprP->parentDirID		= foundKey.hfs.parentID;

				CopyCatalogName( (const CatalogName *) &foundKey.hfs.nodeName, &dprP->directoryName, isHFSPlus );

				for (i = 1; i < GPtr->DirLevel; i++)
				{
					dprP1 = &(*GPtr->DirPTPtr)[i -1];
					if (dprP->directoryID == dprP1->directoryID)
					{
						RcdError( GPtr,E_DirLoop );				/* loop in directory hierarchy */
						return( E_DirLoop );
					}
				}
				
				BuildCatalogKey( dprP->directoryID, (const CatalogName *)0, isHFSPlus, &key );
				result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &threadRecord, &recSize, &hint );
				if  (result != noErr )
				{
					result = IntError(GPtr,result);				/* error from BTSearch */
					return(result);
				}	
				dprP->threadHint	= hint;						/* save hint for thread */
				GPtr->TarBlock		= hint;						/* set target block */
			}

			//	HFSCatalogFile...
			else if ( record.recordType == kHFSFileRecord )
			{
				smallCatalogFileP = (HFSCatalogFile *) &record;
				GPtr->TarID = smallCatalogFileP->fileID;							/* target ID = file number */
				GPtr->CNType = record.recordType;									/* target CNode type = thread */
				CopyCatalogName( (const CatalogName *) &foundKey.hfs.nodeName, &GPtr->CName, isHFSPlus );	/* target CName = directory name */
				filCnt++;
				if (dprP->directoryID == kHFSRootFolderID)
					rtfilCnt++;
			}
			
			//	Unknown/Bad record type
			else
			{
				M_DebugStr("\p Unknown-Bad record type");
				return( 123 );
			}
		} 
		
		//
		//	 if not same ParID or no record
		//
		else if ( (record.recordType == kHFSFileThreadRecord) || (record.recordType == kHFSPlusFileThreadRecord) )			/* it's a file thread, skip past it */
		{
			GPtr->TarID				= parID;						//	target ID = file number
			GPtr->CNType			= record.recordType;			//	target CNode type = thread
			GPtr->CName.ustr.length	= 0;							//	no target CName
		}
		
		else
		{
resumeAtParent:
			GPtr->TarID = dprP->directoryID;						/* target ID = current directory ID */
			GPtr->CNType = record.recordType;						/* target CNode type = directory */
			CopyCatalogName( (const CatalogName *) &dprP->directoryName, &GPtr->CName, isHFSPlus );	// copy the string name

			//	re-locate current directory
			CopyCatalogName( (const CatalogName *) &dprP->directoryName, &catalogName, isHFSPlus );
			BuildCatalogKey( dprP->parentDirID, (const CatalogName *)&catalogName, isHFSPlus, &key );
			result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, dprP->directoryHint, &foundKey, &record2, &recSize, &hint );
			
			if ( result != noErr )
			{
				result = IntError(GPtr,result);						/* error from BTSearch */
				return(result);
			}
			GPtr->TarBlock = hint;									/* set target block */
			

			valence = isHFSPlus == true ? record2.hfsPlusFolder.valence : (UInt32)record2.hfsFolder.valence;

			if ( valence != dprP->offspringIndex -1 ) 				/* check its valence */
				if ( ( result = RcdValErr( GPtr, E_DirVal, dprP->offspringIndex -1, valence, dprP->parentDirID ) ) )
					return( result );

			GPtr->DirLevel--;										/* move up a level */			
			
			if(GPtr->DirLevel > 0)
			{										
				dprP = &(*GPtr->DirPTPtr)[GPtr->DirLevel -1];
				GPtr->TarID	= dprP->directoryID;					/* target ID = current directory ID */
				GPtr->CNType = record.recordType;					/* target CNode type = directory */
				CopyCatalogName( (const CatalogName *) &dprP->directoryName, &GPtr->CName, isHFSPlus );
			}
		}
	}		//	end while

	//
	//	verify directory and file counts (all nonfatal, repairable errors)
	//
	if (!isHFSPlus && (rtdirCnt != calculatedVCB->vcbNmRtDirs)) /* check count of dirs in root */
		if ( ( result = RcdValErr(GPtr,E_RtDirCnt,rtdirCnt,calculatedVCB->vcbNmRtDirs,0) ) )
			return( result );

	if (!isHFSPlus && (rtfilCnt != calculatedVCB->vcbNmFls)) /* check count of files in root */
		if ( ( result = RcdValErr(GPtr,E_RtFilCnt,rtfilCnt,calculatedVCB->vcbNmFls,0) ) )
			return( result );

	if (dirCnt != calculatedVCB->vcbFolderCount) /* check count of dirs in volume */
		if ( ( result = RcdValErr(GPtr,E_DirCnt,dirCnt,calculatedVCB->vcbFolderCount,0) ) )
			return( result );
		
	if (filCnt != calculatedVCB->vcbFileCount) /* check count of files in volume */
		if ( ( result = RcdValErr(GPtr,E_FilCnt,filCnt,calculatedVCB->vcbFileCount,0) ) )
			return( result );

	return( noErr );

}	/* end of CatHChk */



/*------------------------------------------------------------------------------

Function:	CreateAttributesBTreeControlBlock

Function:	Create the calculated AttributesBTree Control Block
			
Input:		GPtr	-	pointer to scavenger global area

Output:				-	0	= no error
						n 	= error code 
------------------------------------------------------------------------------*/
OSErr	CreateAttributesBTreeControlBlock( SGlobPtr GPtr )
{
	OSErr					err = 0;
	SInt32					size;
	UInt32					numABlks;
	BTreeControlBlock *  	btcb;
	SVCB *  				vcb;
	Boolean					isHFSPlus;
	BTHeaderRec				header;
	BlockDescriptor  		block;

	//	Set up
	isHFSPlus = VolumeObjectIsHFSPlus( );
	GPtr->TarID		= kHFSAttributesFileID;
	GPtr->TarBlock	= kHeaderNodeNum;
	block.buffer = NULL;
	btcb = GPtr->calculatedAttributesBTCB;
	vcb = GPtr->calculatedVCB;

	//
	//	check out allocation info for the Attributes File 
	//

	if (isHFSPlus)
	{
		HFSPlusVolumeHeader *volumeHeader;

		err = GetVolumeObjectVHB( &block );
		if ( err != noErr )
			goto exit;
		volumeHeader = (HFSPlusVolumeHeader *) block.buffer;

		CopyMemory( volumeHeader->attributesFile.extents, GPtr->calculatedAttributesFCB->fcbExtents32, sizeof(HFSPlusExtentRecord) );

		err = CheckFileExtents( GPtr, kHFSAttributesFileID, 0, (void *)GPtr->calculatedAttributesFCB->fcbExtents32, &numABlks );	
		if (err) goto exit;

		if ( volumeHeader->attributesFile.totalBlocks != numABlks )					//	check out the PEOF
		{
			RcdError( GPtr, E_CatPEOF );
			err = E_CatPEOF;
			goto exit;
		}
		else
		{
			GPtr->calculatedAttributesFCB->fcbLogicalSize  = (UInt64) volumeHeader->attributesFile.logicalSize;						//	Set Attributes tree's LEOF
			GPtr->calculatedAttributesFCB->fcbPhysicalSize = (UInt64) volumeHeader->attributesFile.totalBlocks * 
											(UInt64) volumeHeader->blockSize;	//	Set Attributes tree's PEOF
		}

		//
		//	See if we actually have an attributes BTree
		//
		if (numABlks == 0)
		{
			btcb->maxKeyLength		= 0;
			btcb->keyCompareProc	= 0;
			btcb->leafRecords		= 0;
			btcb->nodeSize			= 0;
			btcb->totalNodes		= 0;
			btcb->freeNodes			= 0;
			btcb->attributes		= 0;

			btcb->treeDepth		= 0;
			btcb->rootNode		= 0;
			btcb->firstLeafNode	= 0;
			btcb->lastLeafNode	= 0;
			
		//	GPtr->calculatedVCB->attributesRefNum = 0;
			GPtr->calculatedVCB->vcbAttributesFile = NULL;
		}
		else
		{
			//	read the BTreeHeader from disk & also validate it's node size.
			err = GetBTreeHeader(GPtr, GPtr->calculatedAttributesFCB, &header);
			if (err) goto exit;

			btcb->maxKeyLength		= kAttributeKeyMaximumLength;					//	max key length
			btcb->keyCompareProc	= (void *)CompareAttributeKeys;
			btcb->leafRecords		= header.leafRecords;
			btcb->nodeSize			= header.nodeSize;
			btcb->totalNodes		= ( GPtr->calculatedAttributesFCB->fcbPhysicalSize / btcb->nodeSize );
			btcb->freeNodes			= btcb->totalNodes;									//	start with everything free
			btcb->attributes		|=(kBTBigKeysMask + kBTVariableIndexKeysMask);		//	HFS+ Attributes files have large, variable-sized keys

			btcb->treeDepth		= header.treeDepth;
			btcb->rootNode		= header.rootNode;
			btcb->firstLeafNode	= header.firstLeafNode;
			btcb->lastLeafNode	= header.lastLeafNode;

			//
			//	Make sure the header nodes size field is correct by looking at the 1st record offset
			//
			err = CheckNodesFirstOffset( GPtr, btcb );
			if (err) goto exit;
		}
	}
	else
	{
		btcb->maxKeyLength		= 0;
		btcb->keyCompareProc	= 0;
		btcb->leafRecords		= 0;
		btcb->nodeSize			= 0;
		btcb->totalNodes		= 0;
		btcb->freeNodes			= 0;
		btcb->attributes		= 0;

		btcb->treeDepth		= 0;
		btcb->rootNode		= 0;
		btcb->firstLeafNode	= 0;
		btcb->lastLeafNode	= 0;
			
		GPtr->calculatedVCB->vcbAttributesFile = NULL;
	}

	//
	//	set up our DFA extended BTCB area.  Will we have enough memory on all HFS+ volumes.
	//
	btcb->refCon = (UInt32) AllocateClearMemory( sizeof(BTreeExtensionsRec) );			// allocate space for our BTCB extensions
	if ( btcb->refCon == (UInt32)nil ) {
		err = R_NoMem;
		goto exit;
	}

	if (btcb->totalNodes == 0)
	{
		((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr			= nil;
		((BTreeExtensionsRec*)btcb->refCon)->BTCBMSize			= 0;
		((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount	= 0;
	}
	else
	{
		if ( btcb->refCon == (UInt32)nil ) {
			err = R_NoMem;
			goto exit;
		}
		size = (btcb->totalNodes + 7) / 8;											//	size of BTree bit map
		((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr = AllocateClearMemory(size);			//	get precleared bitmap
		if ( ((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr == nil )
		{
			err = R_NoMem;
			goto exit;
		}

		((BTreeExtensionsRec*)btcb->refCon)->BTCBMSize			= size;						//	remember how long it is
		((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount	= header.freeNodes;		//	keep track of real free nodes for progress
	}

exit:
	if (block.buffer)
		(void) ReleaseVolumeBlock(vcb, &block, kReleaseBlock);

	return (err);
}

/*------------------------------------------------------------------------------

Function:	CheckAttributeRecord

Function:	This is call back function called for all leaf records in 
		Attribute BTree.  This function verifies that for every extended
		attribute and security attribute the corresponding bits in 
		Catalog BTree are set.  
			
Input:		GPtr		-	pointer to scavenger global area
		key		-	key for current attribute
		rec		- 	attribute record
		reclen		- 	length of the record

Output:		int		-	function result:			
								0	= no error
								n 	= error code 
------------------------------------------------------------------------------*/

static int 
CheckAttributeRecord(SGlobPtr GPtr, const HFSPlusAttrKey *key, const HFSPlusAttrRecord *rec, UInt16 reclen)
{
	int result = 0;
	Boolean isHFSPlus;

	/* Check if the volume is HFS Plus volume */
	isHFSPlus = VolumeObjectIsHFSPlus();
	if (!isHFSPlus) {
		goto out;
	}

#if DEBUG_XATTR
	char attrName[XATTR_MAXNAMELEN];
	size_t len;

	/* Convert unicode attribute name to char */
	(void) utf_encodestr(key->attrName, key->attrNameLen * 2, attrName, &len);
	attrName[len] = '\0';
	printf ("%s(%s,%d): fileID=%d AttributeName=%s\n", __FUNCTION__, __FILE__, __LINE__, key->fileID, attrName);
#endif
	
	/* 3984119 - Do not include extended attributes for file IDs less
	 * kHFSFirstUserCatalogNodeID but not equal to kHFSRootFolderID 
	 * in prime modulus checksum.  These file IDs do not have 
	 * any catalog record
	 */
	if ((key->fileID < kHFSFirstUserCatalogNodeID) && 
	    (key->fileID != kHFSRootFolderID)) {
		goto out;
	}
	
	if (GPtr->lastAttrFileID.fileID != key->fileID) {
		/* fsck is seeing this fileID in Attribute BTree for first time */
		GPtr->lastAttrFileID.fileID = key->fileID;
		GPtr->lastAttrFileID.hasSecurity = false;
		
		if (!bcmp(key->attrName, GPtr->securityAttrName, GPtr->securityAttrLen)) {
			/* file has both extended attribute and ACL */
			RecordXAttrBits(GPtr, kHFSHasAttributesMask | kHFSHasSecurityMask, key->fileID, kCalculatedAttributesRefNum);
			GPtr->lastAttrFileID.hasSecurity = true;
		} else {
			/* file only has extended attribute */
			RecordXAttrBits(GPtr, kHFSHasAttributesMask, key->fileID, kCalculatedAttributesRefNum);
		}
	} else {
		/* fsck has seen this fileID before */
		if ((GPtr->lastAttrFileID.hasSecurity == false) && !bcmp(key->attrName, GPtr->securityAttrName, GPtr->securityAttrLen)) {
			/* If GPtr->lastAttrFileID did not have security AND current xattr is ACL, 
			 * we ONLY record ACL (as we have already recorded extended attribute
			 */
			RecordXAttrBits(GPtr, kHFSHasSecurityMask, key->fileID, kCalculatedAttributesRefNum);
			GPtr->lastAttrFileID.hasSecurity = true;
		}
	}
out:
	return(result);
}
	
/*------------------------------------------------------------------------------

Function:	AttrBTChk - (Attributes BTree Check)

Function:	Verifies the attributes BTree structure.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		ExtBTChk	-	function result:			
								0	= no error
								n 	= error code 
------------------------------------------------------------------------------*/

OSErr AttrBTChk( SGlobPtr GPtr )
{
	OSErr					err;

	//
	//	If this volume has no attributes BTree, then skip this check
	//
	if (GPtr->calculatedVCB->vcbAttributesFile == NULL)
		return noErr;
	
	//	Write the status message here to avoid potential confusion to user.
	WriteMsg( GPtr, M_AttrBTChk, kStatusMessage );

	//	Set up
	GPtr->TarID		= kHFSAttributesFileID;				//	target = attributes file
	GetVolumeObjectBlockNum( &GPtr->TarBlock );			//	target block = VHB/MDB
 
	//
	//	check out the BTree structure
	//

	err = BTCheck( GPtr, kCalculatedAttributesRefNum, (CheckLeafRecordProcPtr)CheckAttributeRecord);
	ReturnIfError( err );														//	invalid attributes file BTree

	//	compare the attributes prime buckets calculated from catalog btree and attribute btree 
	err = ComparePrimeBuckets(GPtr, kHFSHasAttributesMask);
	ReturnIfError( err );

	//	compare the security prime buckets calculated from catalog btree and attribute btree 
	err = ComparePrimeBuckets(GPtr, kHFSHasSecurityMask);
	ReturnIfError( err );

	//
	//	check out the allocation map structure
	//

	err = BTMapChk( GPtr, kCalculatedAttributesRefNum );
	ReturnIfError( err );														//	Invalid attributes BTree map

	//
	//	compare BTree header record on disk with scavenger's BTree header record 
	//

	err = CmpBTH( GPtr, kCalculatedAttributesRefNum );
	ReturnIfError( err );

	//
	//	compare BTree map on disk with scavenger's BTree map
	//

	err = CmpBTM( GPtr, kCalculatedAttributesRefNum );

	return( err );
}


/*------------------------------------------------------------------------------

Name:		RcdValErr - (Record Valence Error)

Function:	Allocates a RepairOrder node and linkg it into the 'GPtr->RepairP'
			list, to describe an incorrect valence count for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			correct		- the correct valence, as computed here
			incorrect	- the incorrect valence as found in volume
			parid		- the parent id, if S_Valence error

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static int RcdValErr( SGlobPtr GPtr, OSErr type, UInt32 correct, UInt32 incorrect, HFSCatalogNodeID parid )	 /* the ParID, if needed */
{
	RepairOrderPtr	p;										/* the new node we compile */
	SInt16			n;										/* size of node we allocate */
	Boolean			isHFSPlus;
	char goodStr[32], badStr[32];

	isHFSPlus = VolumeObjectIsHFSPlus( );
	PrintError(GPtr, type, 0);
	sprintf(goodStr, "%d", correct);
	sprintf(badStr, "%d", incorrect);
	PrintError(GPtr, E_BadValue, 2, goodStr, badStr);
	
	if (type == E_DirVal)									/* if normal directory valence error */
		n = CatalogNameSize( &GPtr->CName, isHFSPlus); 
	else
		n = 0;												/* other errors don't need the name */
	
	p = AllocMinorRepairOrder( GPtr,n );					/* get the node */
	if (p==NULL) 											/* quit if out of room */
		return (R_NoMem);
	
	p->type			= type;									/* save error info */
	p->correct		= correct;
	p->incorrect	= incorrect;
	p->parid		= parid;
	
	if ( n != 0 ) 											/* if name needed */
		CopyCatalogName( (const CatalogName *) &GPtr->CName, (CatalogName*)&p->name, isHFSPlus ); 
	
	GPtr->CatStat |= S_Valence;								/* set flag to trigger repair */
	
	return( noErr );										/* successful return */
}


/*------------------------------------------------------------------------------

Name:		RcdMDBAllocationBlockStartErr - (Record Allocation Block Start Error)

Function:	Allocates a RepairOrder node and linking it into the 'GPtr->RepairP'
			list, to describe the error for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			correct		- the correct valence, as computed here
			incorrect	- the incorrect valence as found in volume

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static	OSErr	RcdMDBEmbededVolDescriptionErr( SGlobPtr GPtr, OSErr type, HFSMasterDirectoryBlock *mdb )
{
	RepairOrderPtr			p;											//	the new node we compile
	EmbededVolDescription	*desc;
		
	RcdError( GPtr, type );												//	first, record the error
	
	p = AllocMinorRepairOrder( GPtr, sizeof(EmbededVolDescription) );	//	get the node
	if ( p == nil )	return( R_NoMem );
	
	p->type							=  type;							//	save error info
	desc							=  (EmbededVolDescription *) &(p->name);
	desc->drAlBlSt					=  mdb->drAlBlSt;
	desc->drEmbedSigWord			=  mdb->drEmbedSigWord;
	desc->drEmbedExtent.startBlock	=  mdb->drEmbedExtent.startBlock;
	desc->drEmbedExtent.blockCount	=  mdb->drEmbedExtent.blockCount;
	
	GPtr->VIStat					|= S_InvalidWrapperExtents;			//	set flag to trigger repair
	
	return( noErr );													//	successful return
}


#if 0 // not used at this time
/*------------------------------------------------------------------------------

Name:		RcdInvalidWrapperExtents - (Record Invalid Wrapper Extents)

Function:	Allocates a RepairOrder node and linking it into the 'GPtr->RepairP'
			list, to describe the error for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			correct		- the correct valence, as computed here
			incorrect	- the incorrect valence as found in volume

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static	OSErr	RcdInvalidWrapperExtents( SGlobPtr GPtr, OSErr type )
{
	RepairOrderPtr			p;											//	the new node we compile
		
	RcdError( GPtr, type );												//	first, record the error
	
	p = AllocMinorRepairOrder( GPtr, 0 );	//	get the node
	if ( p == nil )	return( R_NoMem );
	
	p->type							=  type;							//	save error info
	
	GPtr->VIStat					|= S_BadMDBdrAlBlSt;				//	set flag to trigger repair
	
	return( noErr );													//	successful return
}
#endif


#if(0)	//	We just check and fix them in SRepair.c
/*------------------------------------------------------------------------------

Name:		RcdOrphanedExtentErr 

Function:	Allocates a RepairOrder node and linkg it into the 'GPtr->RepairP'
			list, to describe an locked volume name for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			incorrect	- the incorrect file flags as found in file record

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static OSErr RcdOrphanedExtentErr ( SGlobPtr GPtr, SInt16 type, void *theKey )
{
	RepairOrderPtr	p;										/* the new node we compile */
	SInt16			n;										/* size of node we allocate */
	Boolean			isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	RcdError( GPtr,type );									/* first, record the error */
	
	if ( isHFSPlus ) 
		n = sizeof( HFSPlusExtentKey );
	else
		n = sizeof( HFSExtentKey );
	
	p = AllocMinorRepairOrder( GPtr, n );					/* get the node */
	if ( p == NULL ) 										/* quit if out of room */
		return( R_NoMem );
	
	CopyMemory( theKey, p->name, n );					/* copy in the key */
	
	p->type = type;											/* save error info */
	
	GPtr->EBTStat |= S_OrphanedExtent;						/* set flag to trigger repair */
	
	return( noErr );										/* successful return */
}
#endif


/*------------------------------------------------------------------------------

Function:	VInfoChk - (Volume Info Check)

Function:	Verifies volume level information.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		VInfoChk	-	function result:			
								0	= no error
								n 	= error code
------------------------------------------------------------------------------*/

OSErr VInfoChk( SGlobPtr GPtr )
{
	OSErr					result;
	UInt16					recSize;
	Boolean					isHFSPlus;
	UInt32					hint;
	UInt64					maxClump;
	SVCB					*vcb;
	VolumeObjectPtr			myVOPtr;
	CatalogRecord			record;
	CatalogKey				foundKey;
	BlockDescriptor  		altBlock;
	BlockDescriptor  		priBlock;

	vcb = GPtr->calculatedVCB;
	altBlock.buffer = priBlock.buffer = NULL;
	isHFSPlus = VolumeObjectIsHFSPlus( );
	myVOPtr = GetVolumeObjectPtr( );
	
	// locate the catalog record for the root directoryÉ
	result = GetBTreeRecord( GPtr->calculatedCatalogFCB, 0x8001, &foundKey, &record, &recSize, &hint );
	GPtr->TarID = kHFSCatalogFileID;							/* target = catalog */
	GPtr->TarBlock = hint;										/* target block = returned hint */
	if ( result != noErr )
	{
		result = IntError( GPtr, result );
		return( result );
	}

	GPtr->TarID		= AMDB_FNum;								//	target = alternate MDB or VHB
	GetVolumeObjectAlternateBlockNum( &GPtr->TarBlock );
	result = GetVolumeObjectAlternateBlock( &altBlock );

	// invalidate if we have not marked the alternate as OK
	if ( isHFSPlus ) {
		if ( (myVOPtr->flags & kVO_AltVHBOK) == 0 )
			result = badMDBErr;
	}
	else if ( (myVOPtr->flags & kVO_AltMDBOK) == 0 ) {
		result = badMDBErr;
	}
	if ( result != noErr ) {
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		if ( VolumeObjectIsHFS( ) ) {
			WriteError( GPtr, E_MDBDamaged, 0, 0 );
			if ( GPtr->logLevel >= kDebugLog ) 
				printf("\tinvalid alternate MDB at %qd result %d \n", GPtr->TarBlock, result);
		}
		else {
			WriteError( GPtr, E_VolumeHeaderDamaged, 0, 0 );
			if ( GPtr->logLevel >= kDebugLog ) 
				printf("\tinvalid alternate VHB at %qd result %d \n", GPtr->TarBlock, result);
		}
		result = noErr;
		goto exit;
	}

	GPtr->TarID		= MDB_FNum;								// target = primary MDB or VHB 
	GetVolumeObjectPrimaryBlockNum( &GPtr->TarBlock );
	result = GetVolumeObjectPrimaryBlock( &priBlock );

	// invalidate if we have not marked the primary as OK
	if ( isHFSPlus ) {
		if ( (myVOPtr->flags & kVO_PriVHBOK) == 0 )
			result = badMDBErr;
	}
	else if ( (myVOPtr->flags & kVO_PriMDBOK) == 0 ) {
		result = badMDBErr;
	}
	if ( result != noErr ) {
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		if ( VolumeObjectIsHFS( ) ) {
			WriteError( GPtr, E_MDBDamaged, 1, 0 );
			if ( GPtr->logLevel >= kDebugLog )
				printf("\tinvalid primary MDB at %qd result %d \n", GPtr->TarBlock, result);
		}
		else {
			WriteError( GPtr, E_VolumeHeaderDamaged, 1, 0 );
			if ( GPtr->logLevel >= kDebugLog )
				printf("\tinvalid primary VHB at %qd result %d \n", GPtr->TarBlock, result);
		}
		result = noErr;
		goto exit;
	}
	
	// check to see that embedded HFS plus volumes still have both (alternate and primary) MDBs 
	if ( VolumeObjectIsEmbeddedHFSPlus( ) && 
		 ( (myVOPtr->flags & kVO_PriMDBOK) == 0 || (myVOPtr->flags & kVO_AltMDBOK) == 0 ) ) 
	{
		GPtr->VIStat |= S_WMDB;
		WriteError( GPtr, E_MDBDamaged, 0, 0 );
		if ( GPtr->logLevel >= kDebugLog )
			printf("\tinvalid wrapper MDB \n");
	}
	
	if ( isHFSPlus )
	{
		HFSPlusVolumeHeader *	volumeHeader;
		HFSPlusVolumeHeader *	alternateVolumeHeader;
		UInt64					totalSectors;
			
		alternateVolumeHeader = (HFSPlusVolumeHeader *) altBlock.buffer;
		volumeHeader = (HFSPlusVolumeHeader *) priBlock.buffer;
	
		maxClump = (UInt64) (vcb->vcbTotalBlocks / 4) * vcb->vcbBlockSize; /* max clump = 1/4 volume size */

		//	check out creation and last mod dates
		vcb->vcbCreateDate	= alternateVolumeHeader->createDate;	// use creation date in alt MDB
		vcb->vcbModifyDate	= volumeHeader->modifyDate;		// don't change last mod date
		vcb->vcbCheckedDate	= volumeHeader->checkedDate;		// don't change checked date

		// 3882639: Removed check for volume attributes in HFS Plus 
		vcb->vcbAttributes = volumeHeader->attributes;
	
		//	verify allocation map ptr
		if ( volumeHeader->nextAllocation < vcb->vcbTotalBlocks )
			vcb->vcbNextAllocation = volumeHeader->nextAllocation;
		else
			vcb->vcbNextAllocation = 0;

		//	verify default clump sizes
		if ( (volumeHeader->rsrcClumpSize > 0) && 
			 (volumeHeader->rsrcClumpSize <= kMaxClumpSize) && 
			 ((volumeHeader->rsrcClumpSize % vcb->vcbBlockSize) == 0) )
			vcb->vcbRsrcClumpSize = volumeHeader->rsrcClumpSize;
		else if ( (alternateVolumeHeader->rsrcClumpSize > 0) && 
				  (alternateVolumeHeader->rsrcClumpSize <= kMaxClumpSize) && 
				  ((alternateVolumeHeader->rsrcClumpSize % vcb->vcbBlockSize) == 0) )
			vcb->vcbRsrcClumpSize = alternateVolumeHeader->rsrcClumpSize;
		else
			vcb->vcbRsrcClumpSize = 4 * vcb->vcbBlockSize;
	
		if ( vcb->vcbRsrcClumpSize > kMaxClumpSize )
			vcb->vcbRsrcClumpSize = vcb->vcbBlockSize;	/* for very large volumes, just use 1 allocation block */

		if ( (volumeHeader->dataClumpSize > 0) && (volumeHeader->dataClumpSize <= kMaxClumpSize) && 
			 ((volumeHeader->dataClumpSize % vcb->vcbBlockSize) == 0) )
			vcb->vcbDataClumpSize = volumeHeader->dataClumpSize;
		else if ( (alternateVolumeHeader->dataClumpSize > 0) && 
				  (alternateVolumeHeader->dataClumpSize <= kMaxClumpSize) && 
				  ((alternateVolumeHeader->dataClumpSize % vcb->vcbBlockSize) == 0) )
			vcb->vcbDataClumpSize = alternateVolumeHeader->dataClumpSize;
		else
			vcb->vcbDataClumpSize = 4 * vcb->vcbBlockSize;
	
		if ( vcb->vcbDataClumpSize > kMaxClumpSize )
			vcb->vcbDataClumpSize = vcb->vcbBlockSize;	/* for very large volumes, just use 1 allocation block */

		/* Verify next CNode ID.
		 * If volumeHeader->nextCatalogID < vcb->vcbNextCatalogID, probably 
		 * nextCatalogID has wrapped around.
		 * If volumeHeader->nextCatalogID > vcb->vcbNextCatalogID, probably 
		 * many files were created and deleted, followed by no new file 
		 * creation.
		 */
		if ( (volumeHeader->nextCatalogID > vcb->vcbNextCatalogID) ) 
			vcb->vcbNextCatalogID = volumeHeader->nextCatalogID;
			
		//¥¥TBD location and unicode? volumename
		//	verify the volume name
		result = ChkCName( GPtr, (const CatalogName*) &foundKey.hfsPlus.nodeName, isHFSPlus );

		//	verify last backup date and backup seqence number
		vcb->vcbBackupDate = volumeHeader->backupDate;  /* don't change last backup date */
		
		//	verify write count
		vcb->vcbWriteCount = volumeHeader->writeCount;	/* don't change write count */

		//	check out extent file clump size
		if ( ((volumeHeader->extentsFile.clumpSize % vcb->vcbBlockSize) == 0) && 
			 (volumeHeader->extentsFile.clumpSize <= maxClump) )
			vcb->vcbExtentsFile->fcbClumpSize = volumeHeader->extentsFile.clumpSize;
		else if ( ((alternateVolumeHeader->extentsFile.clumpSize % vcb->vcbBlockSize) == 0) && 
				  (alternateVolumeHeader->extentsFile.clumpSize <= maxClump) )
			vcb->vcbExtentsFile->fcbClumpSize = alternateVolumeHeader->extentsFile.clumpSize;
		else		
			vcb->vcbExtentsFile->fcbClumpSize = 
			(alternateVolumeHeader->extentsFile.extents[0].blockCount * vcb->vcbBlockSize);
			
		//	check out catalog file clump size
		if ( ((volumeHeader->catalogFile.clumpSize % vcb->vcbBlockSize) == 0) && 
			 (volumeHeader->catalogFile.clumpSize <= maxClump) )
			vcb->vcbCatalogFile->fcbClumpSize = volumeHeader->catalogFile.clumpSize;
		else if ( ((alternateVolumeHeader->catalogFile.clumpSize % vcb->vcbBlockSize) == 0) && 
				  (alternateVolumeHeader->catalogFile.clumpSize <= maxClump) )
			vcb->vcbCatalogFile->fcbClumpSize = alternateVolumeHeader->catalogFile.clumpSize;
		else 
			vcb->vcbCatalogFile->fcbClumpSize = 
			(alternateVolumeHeader->catalogFile.extents[0].blockCount * vcb->vcbBlockSize);
			
		// make sure clump size is at least 1MB for volumes greater than 500MB
		totalSectors = ( VolumeObjectIsEmbeddedHFSPlus( ) ) ? myVOPtr->totalEmbeddedSectors 
															: myVOPtr->totalDeviceSectors;
		if ( (totalSectors * myVOPtr->sectorSize) > (1024 * 1024 * 500) ) 
		{
			if ( vcb->vcbCatalogFile->fcbClumpSize < (1024 * 1024) )
				vcb->vcbCatalogFile->fcbClumpSize = (1024 * 1024);
		}
	
		//	check out allocations file clump size
		if ( ((volumeHeader->allocationFile.clumpSize % vcb->vcbBlockSize) == 0) && 
			 (volumeHeader->allocationFile.clumpSize <= maxClump) )
			vcb->vcbAllocationFile->fcbClumpSize = volumeHeader->allocationFile.clumpSize;
		else if ( ((alternateVolumeHeader->allocationFile.clumpSize % vcb->vcbBlockSize) == 0) && 
				  (alternateVolumeHeader->allocationFile.clumpSize <= maxClump) )
			vcb->vcbAllocationFile->fcbClumpSize = alternateVolumeHeader->allocationFile.clumpSize;
		else
			vcb->vcbAllocationFile->fcbClumpSize = 
			(alternateVolumeHeader->allocationFile.extents[0].blockCount * vcb->vcbBlockSize);
	
		CopyMemory( volumeHeader->finderInfo, vcb->vcbFinderInfo, sizeof(vcb->vcbFinderInfo) );
		
		//	Now compare verified Volume Header info (in the form of a vcb) with Volume Header info on disk
		result = CompareVolumeHeader( GPtr, volumeHeader );
		
		// check to see that embedded volume info is correct in both wrapper MDBs 
		CheckEmbeddedVolInfoInMDBs( GPtr );

	}
	else		//	HFS
	{
		HFSMasterDirectoryBlock	*mdbP;
		HFSMasterDirectoryBlock	*alternateMDB;
		
		//	
		//	get volume name from BTree Key
		// 
		
		alternateMDB = (HFSMasterDirectoryBlock	*) altBlock.buffer;
		mdbP = (HFSMasterDirectoryBlock	*) priBlock.buffer;

		maxClump = (UInt64) (vcb->vcbTotalBlocks / 4) * vcb->vcbBlockSize; /* max clump = 1/4 volume size */

		//	check out creation and last mod dates
		vcb->vcbCreateDate	= alternateMDB->drCrDate;		/* use creation date in alt MDB */	
		vcb->vcbModifyDate	= mdbP->drLsMod;			/* don't change last mod date */

		//	verify volume attribute flags
		if ( (mdbP->drAtrb & VAtrb_Msk) == 0 )
			vcb->vcbAttributes = mdbP->drAtrb;
		else 
			vcb->vcbAttributes = VAtrb_DFlt;
	
		//	verify allocation map ptr
		if ( mdbP->drAllocPtr < vcb->vcbTotalBlocks )
			vcb->vcbNextAllocation = mdbP->drAllocPtr;
		else
			vcb->vcbNextAllocation = 0;

		//	verify default clump size
		if ( (mdbP->drClpSiz > 0) && 
			 (mdbP->drClpSiz <= maxClump) && 
			 ((mdbP->drClpSiz % vcb->vcbBlockSize) == 0) )
			vcb->vcbDataClumpSize = mdbP->drClpSiz;
		else if ( (alternateMDB->drClpSiz > 0) && 
				  (alternateMDB->drClpSiz <= maxClump) && 
				  ((alternateMDB->drClpSiz % vcb->vcbBlockSize) == 0) )
			vcb->vcbDataClumpSize = alternateMDB->drClpSiz;
		else
			vcb->vcbDataClumpSize = 4 * vcb->vcbBlockSize;
	
		if ( vcb->vcbDataClumpSize > kMaxClumpSize )
			vcb->vcbDataClumpSize = vcb->vcbBlockSize;	/* for very large volumes, just use 1 allocation block */
	
		//	verify next CNode ID
		if ( (mdbP->drNxtCNID > vcb->vcbNextCatalogID) && (mdbP->drNxtCNID <= (vcb->vcbNextCatalogID + 4096)) )
			vcb->vcbNextCatalogID = mdbP->drNxtCNID;
			
		//	verify the volume name
		result = ChkCName( GPtr, (const CatalogName*) &vcb->vcbVN, isHFSPlus );
		if ( result == noErr )		
			if ( CmpBlock( mdbP->drVN, vcb->vcbVN, vcb->vcbVN[0] + 1 ) == 0 )
				CopyMemory( mdbP->drVN, vcb->vcbVN, kHFSMaxVolumeNameChars + 1 ); /* ...we have a good one */		

		//	verify last backup date and backup seqence number
		vcb->vcbBackupDate = mdbP->drVolBkUp;		/* don't change last backup date */
		vcb->vcbVSeqNum = mdbP->drVSeqNum;		/* don't change last backup sequence # */
		
		//	verify write count
		vcb->vcbWriteCount = mdbP->drWrCnt;						/* don't change write count */

		//	check out extent file and catalog clump sizes
		if ( ((mdbP->drXTClpSiz % vcb->vcbBlockSize) == 0) && (mdbP->drXTClpSiz <= maxClump) )
			vcb->vcbExtentsFile->fcbClumpSize = mdbP->drXTClpSiz;
		else if ( ((alternateMDB->drXTClpSiz % vcb->vcbBlockSize) == 0) && (alternateMDB->drXTClpSiz <= maxClump) )
			vcb->vcbExtentsFile->fcbClumpSize = alternateMDB->drXTClpSiz;
		else		
			vcb->vcbExtentsFile->fcbClumpSize = (alternateMDB->drXTExtRec[0].blockCount * vcb->vcbBlockSize);
			
		if ( ((mdbP->drCTClpSiz % vcb->vcbBlockSize) == 0) && (mdbP->drCTClpSiz <= maxClump) )
			vcb->vcbCatalogFile->fcbClumpSize = mdbP->drCTClpSiz;
		else if ( ((alternateMDB->drCTClpSiz % vcb->vcbBlockSize) == 0) && (alternateMDB->drCTClpSiz <= maxClump) )
			vcb->vcbCatalogFile->fcbClumpSize = alternateMDB->drCTClpSiz;
		else
			vcb->vcbCatalogFile->fcbClumpSize = (alternateMDB->drCTExtRec[0].blockCount * vcb->vcbBlockSize);
	
		//	just copy Finder info for now
		CopyMemory(mdbP->drFndrInfo, vcb->vcbFinderInfo, sizeof(mdbP->drFndrInfo));
		
		//	now compare verified MDB info with MDB info on disk
		result = CmpMDB( GPtr, mdbP);
	}

exit:
	if (priBlock.buffer)
		(void) ReleaseVolumeBlock(vcb, &priBlock, kReleaseBlock);
	if (altBlock.buffer)
		(void) ReleaseVolumeBlock(vcb, &altBlock, kReleaseBlock);

	return (result);
	
}	/* end of VInfoChk */


/*------------------------------------------------------------------------------

Function:	VLockedChk - (Volume Name Locked Check)

Function:	Makes sure the volume name isn't locked.  If it is locked, generate a repair order.

			This function is not called if file sharing is operating.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		VInfoChk	-	function result:			
								0	= no error
								n 	= error code
------------------------------------------------------------------------------*/

OSErr	VLockedChk( SGlobPtr GPtr )
{
	UInt32				hint;
	CatalogKey			foundKey;
	CatalogRecord		record;
	UInt16				recSize;
	OSErr				result;
	UInt16				frFlags;
	Boolean				isHFSPlus;
	SVCB				*calculatedVCB	= GPtr->calculatedVCB;
	VolumeObjectPtr		myVOPtr;

	myVOPtr = GetVolumeObjectPtr( );
	isHFSPlus = VolumeObjectIsHFSPlus( );
	GPtr->TarID		= kHFSCatalogFileID;								/* target = catalog file */
	GPtr->TarBlock	= 0;												/* no target block yet */
	
	//
	//	locate the catalog record for the root directory
	//
	result = GetBTreeRecord( GPtr->calculatedCatalogFCB, 0x8001, &foundKey, &record, &recSize, &hint );
	
	if ( result)
	{
		RcdError( GPtr, E_EntryNotFound );
		return( E_EntryNotFound );
	}

	//	put the volume name in the VCB
	if ( isHFSPlus == false )
	{
		CopyMemory( foundKey.hfs.nodeName, calculatedVCB->vcbVN, sizeof(calculatedVCB->vcbVN) );
	}
	else if ( myVOPtr->volumeType != kPureHFSPlusVolumeType )
	{
		HFSMasterDirectoryBlock	*mdbP;
		BlockDescriptor  block;
		
		block.buffer = NULL;
		if ( (myVOPtr->flags & kVO_PriMDBOK) != 0 )
			result = GetVolumeObjectPrimaryMDB( &block );
		else
			result = GetVolumeObjectAlternateMDB( &block );
		if ( result == noErr ) {
			mdbP = (HFSMasterDirectoryBlock	*) block.buffer;
			CopyMemory( mdbP->drVN, calculatedVCB->vcbVN, sizeof(mdbP->drVN) );
		}
		if ( block.buffer != NULL )
			(void) ReleaseVolumeBlock(calculatedVCB, &block, kReleaseBlock );
		ReturnIfError(result);
	}
	else		//	Because we don't have the unicode converters, just fill it with a dummy name.
	{
		CopyMemory( "\x0dPure HFS Plus", calculatedVCB->vcbVN, sizeof(Str27) );
	}
		
	GPtr->TarBlock = hint;
	if ( isHFSPlus )
		CopyCatalogName( (const CatalogName *)&foundKey.hfsPlus.nodeName, &GPtr->CName, isHFSPlus );
	else
		CopyCatalogName( (const CatalogName *)&foundKey.hfs.nodeName, &GPtr->CName, isHFSPlus );
	
	if ( (record.recordType == kHFSPlusFolderRecord) || (record.recordType == kHFSFolderRecord) )
	{
		frFlags = record.recordType == kHFSPlusFolderRecord ?
			SWAP_BE16(record.hfsPlusFolder.userInfo.frFlags) :
			SWAP_BE16(record.hfsFolder.userInfo.frFlags);
	
		if ( frFlags & fNameLocked )												// name locked bit set?
			RcdNameLockedErr( GPtr, E_LockedDirName, frFlags );
	}	
	
	return( noErr );
}


/*------------------------------------------------------------------------------

Name:		RcdNameLockedErr 

Function:	Allocates a RepairOrder node and linkg it into the 'GPtr->RepairP'
			list, to describe an locked volume name for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			incorrect	- the incorrect file flags as found in file record

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static int RcdNameLockedErr( SGlobPtr GPtr, SInt16 type, UInt32 incorrect )									/* for a consistency check */
{
	RepairOrderPtr	p;										/* the new node we compile */
	int				n;										/* size of node we allocate */
	Boolean			isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	RcdError( GPtr, type );									/* first, record the error */
	
	n = CatalogNameSize( &GPtr->CName, isHFSPlus );
	
	p = AllocMinorRepairOrder( GPtr, n );					/* get the node */
	if ( p==NULL ) 											/* quit if out of room */
		return ( R_NoMem );
	
	CopyCatalogName( (const CatalogName *) &GPtr->CName, (CatalogName*)&p->name, isHFSPlus );
	
	p->type				= type;								/* save error info */
	p->correct			= incorrect & ~fNameLocked;			/* mask off the name locked bit */
	p->incorrect		= incorrect;
	p->maskBit			= (UInt16)fNameLocked;
	p->parid			= 1;
	
	GPtr->CatStat |= S_LockedDirName;						/* set flag to trigger repair */
	
	return( noErr );										/* successful return */
}


/*------------------------------------------------------------------------------

Function:	CheckFileExtents - (Check File Extents)

Function:	Verifies the extent info for a file.
			
Input:		GPtr		-	pointer to scavenger global area
			fileNumber	-	file number
			forkType	-	fork type ($00 = data fork, $FF = resource fork)
			extents		-	ptr to 1st extent record for the file

Output:
			CheckFileExtents	-	function result:			
								noErr	= no error
								n 		= error code
			blocksUsed	-	number of allocation blocks allocated to the file
------------------------------------------------------------------------------*/

OSErr	CheckFileExtents( SGlobPtr GPtr, UInt32 fileNumber, UInt8 forkType, 
						  const void *extents, UInt32 *blocksUsed )
{
	UInt32				blockCount;
	UInt32				extentBlockCount;
	UInt32				extentStartBlock;
	UInt32				hint;
	HFSPlusExtentKey	key;
	HFSPlusExtentKey	extentKey;
	HFSPlusExtentRecord	extentRecord;
	UInt16 				recSize;
	OSErr				err;
	SInt16				i;
	Boolean				firstRecord;
	Boolean				isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	firstRecord	= true;
	err			= noErr;
	blockCount	= 0;
	
	while ( (extents != nil) && (err == noErr) )
	{
		err = ChkExtRec( GPtr, extents );			//	checkout the extent record first
		if ( err != noErr )							//	Bad extent record, don't mark it
			break;
			
		for ( i=0 ; i<GPtr->numExtents ; i++ )		//	now checkout the extents
		{
			//	HFS+/HFS moving extent fields into local variables for evaluation
			if ( isHFSPlus == true )
			{
				extentBlockCount = ((HFSPlusExtentDescriptor *)extents)[i].blockCount;
				extentStartBlock = ((HFSPlusExtentDescriptor *)extents)[i].startBlock;
			}
			else
			{
				extentBlockCount = ((HFSExtentDescriptor *)extents)[i].blockCount;
				extentStartBlock = ((HFSExtentDescriptor *)extents)[i].startBlock;
			}
	
			if ( extentBlockCount == 0 )
				break;
			err = CaptureBitmapBits(extentStartBlock, extentBlockCount);
			if (err == E_OvlExt) {
				err = AddExtentToOverlapList(GPtr, fileNumber, extentStartBlock, extentBlockCount, forkType);
			}
			
			blockCount += extentBlockCount;
		}
		
		if ( fileNumber == kHFSExtentsFileID )		//	Extents file has no overflow extents
			break;
			
		if ( firstRecord == true )
		{
			firstRecord = false;

			//	Set up the extent key
			BuildExtentKey( isHFSPlus, forkType, fileNumber, blockCount, (void *)&key );

			err = SearchBTreeRecord( GPtr->calculatedExtentsFCB, &key, kNoHint, (void *) &extentKey, (void *) &extentRecord, &recSize, &hint );
			
			if ( err == btNotFound )
			{
				err = noErr;								//	 no more extent records
				extents = nil;
				break;
			}
			else if ( err != noErr )
			{
		 		err = IntError( GPtr, err );		//	error from SearchBTreeRecord
				return( err );
			}
		}
		else
		{
			err = GetBTreeRecord( GPtr->calculatedExtentsFCB, 1, &extentKey, extentRecord, &recSize, &hint );
			
			if ( err == btNotFound )
			{
				err = noErr;								//	 no more extent records
				extents = nil;
				break;
			}
			else if ( err != noErr )
			{
		 		err = IntError( GPtr, err ); 		/* error from BTGetRecord */
				return( err );
			}
			
			//	Check same file and fork
			if ( isHFSPlus )
			{
				if ( (extentKey.fileID != fileNumber) || (extentKey.forkType != forkType) )
					break;
			}
			else
			{
				if ( (((HFSExtentKey *) &extentKey)->fileID != fileNumber) || (((HFSExtentKey *) &extentKey)->forkType != forkType) )
					break;
			}
		}
		
		extents = (void *) &extentRecord;
	}
	
	*blocksUsed = blockCount;
	
	return( err );
}


void	BuildExtentKey( Boolean isHFSPlus, UInt8 forkType, HFSCatalogNodeID fileNumber, UInt32 blockNumber, void * key )
{
	if ( isHFSPlus )
	{
		HFSPlusExtentKey *hfsPlusKey	= (HFSPlusExtentKey*) key;
		
		hfsPlusKey->keyLength	= kHFSPlusExtentKeyMaximumLength;
		hfsPlusKey->forkType	= forkType;
		hfsPlusKey->pad			= 0;
		hfsPlusKey->fileID		= fileNumber;
		hfsPlusKey->startBlock	= blockNumber;
	}
	else
	{
		HFSExtentKey *hfsKey	= (HFSExtentKey*) key;

		hfsKey->keyLength		= kHFSExtentKeyMaximumLength;
		hfsKey->forkType		= forkType;
		hfsKey->fileID			= fileNumber;
		hfsKey->startBlock		= (UInt16) blockNumber;
	}
}



//
//	Adds this extent to our OverlappedExtentList for later repair.
//
OSErr	AddExtentToOverlapList( SGlobPtr GPtr, HFSCatalogNodeID fileNumber, UInt32 extentStartBlock, UInt32 extentBlockCount, UInt8 forkType )
{
	UInt32			newHandleSize;
	ExtentInfo		extentInfo;
	ExtentsTable	**extentsTableH;
	Boolean doesFileExist = false;
	
	ClearMemory(&extentInfo, sizeof(extentInfo));
	extentInfo.fileID		= fileNumber;
	extentInfo.startBlock	= extentStartBlock;
	extentInfo.blockCount	= extentBlockCount;
	extentInfo.forkType		= forkType;
	
	//	If it's uninitialized
	if ( GPtr->overlappedExtents == nil )
	{
		GPtr->overlappedExtents	= (ExtentsTable **) NewHandleClear( sizeof(ExtentsTable) );
		extentsTableH	= GPtr->overlappedExtents;
	}
	else
	{
		extentsTableH	= GPtr->overlappedExtents;

		if ( ExtentInfoExists( extentsTableH, &extentInfo, &doesFileExist ) == true )
			return( noErr );

		//	Grow the Extents table for a new entry.
		newHandleSize = ( sizeof(ExtentInfo) ) + ( GetHandleSize( (Handle)extentsTableH ) );
		SetHandleSize( (Handle)extentsTableH, newHandleSize );
	}

	/* Print the list of overlapping extents after catalog BTree check to
	 * avoid deadlocks from cache read
	 */
	
	//	Copy the new extents into the end of the table
	CopyMemory( &extentInfo, &((**extentsTableH).extentInfo[(**extentsTableH).count]), sizeof(ExtentInfo) );
	
	// 	Update the overlap extent bit
	GPtr->VIStat |= S_OverlappingExtents;

	//	Update the extent table count
	(**extentsTableH).count++;
	
	return( noErr );
}


static	Boolean	ExtentInfoExists( ExtentsTable **extentsTableH, ExtentInfo *extentInfo, Boolean *doesFileExist )
{
	UInt32		i;
	ExtentInfo	*aryExtentInfo;
	
	*doesFileExist = false;

	for ( i = 0 ; i < (**extentsTableH).count ; i++ )
	{
		aryExtentInfo	= &((**extentsTableH).extentInfo[i]);
		
		if ( extentInfo->fileID == aryExtentInfo->fileID )
		{
			*doesFileExist = true;

			if (	(extentInfo->startBlock == aryExtentInfo->startBlock)	&& 
					(extentInfo->blockCount == aryExtentInfo->blockCount)	&&
					(extentInfo->forkType	== aryExtentInfo->forkType)		)
			{
				return( true );
			}
		}
	}
	
	return( false );
}

