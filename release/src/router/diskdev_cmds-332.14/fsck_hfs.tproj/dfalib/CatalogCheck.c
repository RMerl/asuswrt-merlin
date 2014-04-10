/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include "Scavenger.h"
#include "DecompDataEnums.h"
#include "DecompData.h"

/*
 * information collected when visiting catalog records
 */
struct CatalogIterationSummary {
	UInt32 parentID;
	UInt32 rootDirCount;   /* hfs only */
	UInt32 rootFileCount;  /* hfs only */
	UInt32 dirCount;
	UInt32 dirThreads;
	UInt32 fileCount;
	UInt32 filesWithThreads; /* hfs only */
	UInt32 fileThreads;
	UInt32 nextCNID;
	UInt64 encodings;
	void * hardLinkRef;
};

/* Globals used during Catalog record checks */
struct CatalogIterationSummary gCIS;

SGlobPtr gScavGlobals;

/* Local routines for checking catalog structures */
static int  CheckCatalogRecord(SGlobPtr GPtr, const HFSPlusCatalogKey *key,
                               const CatalogRecord *rec, UInt16 reclen);
static int  CheckCatalogRecord_HFS(const HFSCatalogKey *key,
                                   const CatalogRecord *rec, UInt16 reclen);

static int  CheckIfAttributeExists(const UInt32 fileID); 
static int  CheckDirectory(const HFSPlusCatalogKey * key, const HFSPlusCatalogFolder * dir);
static int  CheckFile(const HFSPlusCatalogKey * key, const HFSPlusCatalogFile * file);
static int  CheckThread(const HFSPlusCatalogKey * key, const HFSPlusCatalogThread * thread);

static int  CheckDirectory_HFS(const HFSCatalogKey * key, const HFSCatalogFolder * dir);
static int  CheckFile_HFS(const HFSCatalogKey * key, const HFSCatalogFile * file);
static int  CheckThread_HFS(const HFSCatalogKey * key, const HFSCatalogThread * thread);

static void CheckBSDInfo(const HFSPlusCatalogKey * key, const HFSPlusBSDInfo * bsdInfo, int isdir);
static int  CheckCatalogName(u_int16_t charCount, const u_int16_t *uniChars,
                             u_int32_t parentID, Boolean thread);
static int  CheckCatalogName_HFS(u_int16_t charCount, const u_char *filename,
                                 u_int32_t parentID, Boolean thread);

static int  RecordBadAllocation(UInt32 parID, unsigned char * filename, UInt32 forkType, UInt32 oldBlkCnt, UInt32 newBlkCnt);
static int  RecordTruncation(UInt32 parID, unsigned char * filename, UInt32 forkType, UInt64 oldSize,  UInt64 newSize);

static int  CaptureMissingThread(UInt32 threadID, const HFSPlusCatalogKey *nextKey);
static 	OSErr	UniqueDotName( 	SGlobPtr GPtr, 
                                CatalogName * theNewNamePtr, 
                                UInt32 theParID, 
                                Boolean isSingleDotName,
                                Boolean isHFSPlus );
static Boolean 	FixDecomps(	u_int16_t charCount, const u_int16_t *inFilename, HFSUniStr255 *outFilename );

static int FindOrigOverlapFiles(SGlobPtr GPtr);
static void CheckHFSPlusExtentRecords(SGlobPtr GPtr, UInt32 fileID, HFSPlusExtentRecord extent, UInt8 forkType); 
static void CheckHFSExtentRecords(SGlobPtr GPtr, UInt32 fileID, HFSExtentRecord extent, UInt8 forkType);
static Boolean DoesOverlap(SGlobPtr GPtr, UInt32 fileID, UInt32 startBlock, UInt32 blockCount, UInt8 forkType); 

void PrintOverlapFiles (SGlobPtr GPtr);
static int CompareExtentFileID(const void *first, const void *second);

/*
 * CheckCatalogBTree - Verifies the catalog B-tree structure
 *
 * Causes CheckCatalogRecord to be called for every leaf record
 */
OSErr
CheckCatalogBTree( SGlobPtr GPtr )
{
	OSErr err;
	int hfsplus;
	
	gScavGlobals = GPtr;
	hfsplus = VolumeObjectIsHFSPlus( );

	ClearMemory(&gCIS, sizeof(gCIS));
	gCIS.parentID = kHFSRootParentID;
	gCIS.nextCNID = kHFSFirstUserCatalogNodeID;

	if (hfsplus)
        	HardLinkCheckBegin(gScavGlobals, &gCIS.hardLinkRef);

	/* for compatibility, init these globals */
	gScavGlobals->TarID = kHFSCatalogFileID;
	GetVolumeObjectBlockNum( &gScavGlobals->TarBlock );

	/*
	 * Check out the BTree structure
	 */
	err = BTCheck(gScavGlobals, kCalculatedCatalogRefNum, (CheckLeafRecordProcPtr)CheckCatalogRecord);
	if (err) goto exit;

	if (GPtr->VIStat & S_OverlappingExtents) {
		/* If BTCheck found any overlapped extents, fsck_hfs has reported 
		 * all the fileIDs except the original fileIDs.  Traverse thei volume header,
	 	 * catalog btree and extents btree to find the original fileIDs 
	 	 */
		err = FindOrigOverlapFiles(GPtr);
		if (err) goto exit;
		
		/* Print all unique overlapping file IDs and paths */
		(void) PrintOverlapFiles(GPtr);
	}

	if (gCIS.dirCount != gCIS.dirThreads) {
		RcdError(gScavGlobals, E_IncorrectNumThdRcd);
		gScavGlobals->CBTStat |= S_Orphan;  /* a directory record is missing */
	}

	if (hfsplus && (gCIS.fileCount != gCIS.fileThreads)) {
		RcdError(gScavGlobals, E_IncorrectNumThdRcd);
		gScavGlobals->CBTStat |= S_Orphan;
	}

	if (!hfsplus && (gCIS.fileThreads != gCIS.filesWithThreads)) {
		RcdError(gScavGlobals, E_IncorrectNumThdRcd);
		gScavGlobals->CBTStat |= S_Orphan;
	}

	gScavGlobals->calculatedVCB->vcbEncodingsBitmap = gCIS.encodings;
	gScavGlobals->calculatedVCB->vcbNextCatalogID = gCIS.nextCNID;
	gScavGlobals->calculatedVCB->vcbFolderCount = gCIS.dirCount - 1;
	gScavGlobals->calculatedVCB->vcbFileCount = gCIS.fileCount;
	if (!hfsplus) {
		gScavGlobals->calculatedVCB->vcbNmRtDirs = gCIS.rootDirCount;
		gScavGlobals->calculatedVCB->vcbNmFls = gCIS.rootFileCount;
	}

	/*
	 * Check out the allocation map structure
	 */
	err = BTMapChk(gScavGlobals, kCalculatedCatalogRefNum);
	if (err) goto exit;

	/*
	 * Compare BTree header record on disk with scavenger's BTree header record 
	 */
	err = CmpBTH(gScavGlobals, kCalculatedCatalogRefNum);
	if (err) goto exit;

	/*
	 * Compare BTree map on disk with scavenger's BTree map
	 */
	err = CmpBTM(gScavGlobals, kCalculatedCatalogRefNum);

	if (hfsplus)
		(void) CheckHardLinks(gCIS.hardLinkRef);

 exit:
	if (hfsplus)
		HardLinkCheckEnd(gCIS.hardLinkRef);

	return (err);
}

/*
 * CheckCatalogRecord - verify a catalog record
 *
 * Called in leaf-order for every leaf record in the Catalog B-tree
 */
static int
CheckCatalogRecord(SGlobPtr GPtr, const HFSPlusCatalogKey *key, const CatalogRecord *rec, UInt16 reclen)
{
	int 						result = 0;
	Boolean						isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	++gScavGlobals->itemsProcessed;

	if (!isHFSPlus)
		return CheckCatalogRecord_HFS((HFSCatalogKey *)key, rec, reclen);

	gScavGlobals->CNType = rec->recordType;

	switch (rec->recordType) {
	case kHFSPlusFolderRecord:
		++gCIS.dirCount;
		if (reclen != sizeof(HFSPlusCatalogFolder)){
			RcdError(gScavGlobals, E_LenDir);
			result = E_LenDir;
			break;
		}
		if (key->parentID != gCIS.parentID) {
			result = CaptureMissingThread(key->parentID, key);
			if (result) break;
			/* Pretend thread record was there */
			++gCIS.dirThreads;
			gCIS.parentID = key->parentID;
		}
		result = CheckDirectory(key, (HFSPlusCatalogFolder *)rec);
		break;

	case kHFSPlusFileRecord:
		++gCIS.fileCount;
		if (reclen != sizeof(HFSPlusCatalogFile)){
			RcdError(gScavGlobals, E_LenFil);
			result = E_LenFil;
			break;
		}
		if (key->parentID != gCIS.parentID) {
			result = CaptureMissingThread(key->parentID, key);
			if (result) break;
			/* Pretend thread record was there */
			++gCIS.dirThreads;
			gCIS.parentID = key->parentID;
		}
		result = CheckFile(key, (HFSPlusCatalogFile *)rec);
		break;

	case kHFSPlusFolderThreadRecord:
		++gCIS.dirThreads;
		gCIS.parentID = key->parentID;
		/* Fall through */

	case kHFSPlusFileThreadRecord:
		if (rec->recordType == kHFSPlusFileThreadRecord)
			++gCIS.fileThreads;

		if (reclen > sizeof(HFSPlusCatalogThread) ||
			reclen < sizeof(HFSPlusCatalogThread) - sizeof(HFSUniStr255)) {
			RcdError(gScavGlobals, E_LenThd);
			result = E_LenThd;
			break;
		} else if (reclen == sizeof(HFSPlusCatalogThread)) {
			gScavGlobals->VeryMinorErrorsStat |= S_BloatedThreadRecordFound;
		}
		result = CheckThread(key, (HFSPlusCatalogThread *)rec);
		break;

	default:
		RcdError(gScavGlobals, E_CatRec);
		result = E_CatRec;
	}
	
	return (result);
}

/*
 * CheckCatalogRecord_HFS - verify an HFS catalog record
 *
 * Called in leaf-order for every leaf record in the Catalog B-tree
 */
static int
CheckCatalogRecord_HFS(const HFSCatalogKey *key, const CatalogRecord *rec, UInt16 reclen)
{
	int result = 0;

	gScavGlobals->CNType = rec->recordType;

	switch (rec->recordType) {
	case kHFSFolderRecord:
		++gCIS.dirCount;
		if (key->parentID == kHFSRootFolderID )
			++gCIS.rootDirCount;
		if (reclen != sizeof(HFSCatalogFolder)){
			RcdError(gScavGlobals, E_LenDir);
			result = E_LenDir;
			break;
		}
		if (key->parentID != gCIS.parentID) {
			result = CaptureMissingThread(key->parentID, (HFSPlusCatalogKey *)key);
			if (result) break;
			/* Pretend thread record was there */
			++gCIS.dirThreads;
			gCIS.parentID = key->parentID;
		}
		result = CheckDirectory_HFS(key, (HFSCatalogFolder *)rec);
		break;

	case kHFSFileRecord:
		++gCIS.fileCount;
		if (key->parentID == kHFSRootFolderID )
			++gCIS.rootFileCount;
		if (reclen != sizeof(HFSCatalogFile)){
			RcdError(gScavGlobals, E_LenFil);
			result = E_LenFil;
			break;
		}
		if (key->parentID != gCIS.parentID) {
			result = CaptureMissingThread(key->parentID, (HFSPlusCatalogKey *)key);
			if (result) break;
			/* Pretend thread record was there */
			++gCIS.dirThreads;
			gCIS.parentID = key->parentID;
		}
		result = CheckFile_HFS(key, (HFSCatalogFile *)rec);
		break;

	case kHFSFolderThreadRecord:
		++gCIS.dirThreads;
		gCIS.parentID = key->parentID;
		/* Fall through */
	case kHFSFileThreadRecord:
		if (rec->recordType == kHFSFileThreadRecord)
			++gCIS.fileThreads;

		if (reclen != sizeof(HFSCatalogThread)) {
			RcdError(gScavGlobals, E_LenThd);
			result = E_LenThd;
			break;
		}
		result = CheckThread_HFS(key, (HFSCatalogThread *)rec);
		break;


	default:
		RcdError(gScavGlobals, E_CatRec);
		result = E_CatRec;
	}
	
	return (result);
}

/* CheckIfAttributeExists - check if atleast one extend attribute
 *                          exists for given file/folder ID 
 * 
 * Returns: 0 - if not extended attribute exists
 *          1 - if atleast one extended attribute exists
 *          n - some error occured 
 */
static int 
CheckIfAttributeExists(const UInt32 fileID) 
{
	int result = 0;
	HFSPlusAttrKey *attrKey;
	BTreeIterator iterator;

	/* Initialize the iterator, note that other fields in iterator are initialized to zero */
	ClearMemory(&iterator, sizeof(BTreeIterator));

	/* Initialize attribute key to iterator key.  Other fields initialized to zero */
	attrKey = (HFSPlusAttrKey *)&iterator.key;
	attrKey->keyLength = kHFSPlusAttrKeyMinimumLength;
	attrKey->fileID = fileID;

	/* Search for attribute with NULL name.  This will place the iterator at correct fileID location in BTree */	
	result = BTSearchRecord(gScavGlobals->calculatedAttributesFCB, &iterator, kInvalidMRUCacheKey, NULL, NULL, &iterator);
	if (result && (result != btNotFound)) {
#if 0 //DEBUG_XATTR
		printf ("%s: No matching attribute record found\n", __FUNCTION__);
#endif
		goto out;
	}

	/* Iterate to next record and check there is a valid extended attribute for this file ID */ 
	result = BTIterateRecord(gScavGlobals->calculatedAttributesFCB, kBTreeNextRecord, &iterator, NULL, NULL);
	if ((result == noErr) && (attrKey->fileID == fileID)) {
		/* Set success return value only if we did _find_ an attribute record for the current fileID */
		result = 1;
	} 
out:
#if 0 //DEBUG_XATTR
	printf ("%s: Retval=%d for fileID=%d\n", __FUNCTION__, result, fileID); 
#endif
	return result;
}

/*
 * CheckDirectory - verify a catalog directory record
 *
 * Also collects info for later processing.
 * Called in leaf-order for every directory record in the Catalog B-tree
 */
static int 
CheckDirectory(const HFSPlusCatalogKey * key, const HFSPlusCatalogFolder * dir)
{
	UInt32 dirID;
	int result = 0;

	dirID = dir->folderID;

	/* Directory cannot have these two flags set */
	if ((dir->flags & (kHFSFileLockedMask | kHFSThreadExistsMask)) != 0) {
		RcdError(gScavGlobals, E_CatalogFlagsNotZero);
		gScavGlobals->CBTStat |= S_ReservedNotZero;
	}

	/* 3971173, 3977448 : HFS does not clear the attribute bit after it deletes the 
	 * last attribute.  Fix it later in 3977448 by removing the call for checking attrs
	 */
	result = CheckIfAttributeExists(dir->folderID);
	if (result == 1) {
		/* Atleast one extended attribute exists for this folder ID */
		/* 3843779 : Record if directory record has attribute and/or security bit set */
		RecordXAttrBits(gScavGlobals, dir->flags, dir->folderID, kCalculatedCatalogRefNum);
#if DEBUG_XATTR
		printf ("%s: Record folderID=%d for prime modulus calculations\n", __FUNCTION__, dir->folderID); 
#endif
	}
#if 0 //DEBUG_XATTR
	else {
		if (result == 0) 
			printf ("%s: Ignoring folder ID %d from prime modulus calculuation\n", __FUNCTION__, dir->folderID); 
	}
#endif 
	result = 0;	/* ignore result from record xattr */

	if (dirID < kHFSFirstUserCatalogNodeID  &&
            dirID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}
	if (dirID >= gCIS.nextCNID )
		gCIS.nextCNID = dirID + 1;

	gCIS.encodings |= (u_int64_t)(1ULL << MapEncodingToIndex(dir->textEncoding & 0x7F));

	CheckBSDInfo(key, &dir->bsdInfo, true);
	
	CheckCatalogName(key->nodeName.length, &key->nodeName.unicode[0], key->parentID, false);
	
	return (result);
}

/*
 * CheckFile - verify a HFS+ catalog file record
 * - sanity check values
 * - collect info for later processing
 *
 * Called in leaf-order for every file record in the Catalog B-tree
 */
static int
CheckFile(const HFSPlusCatalogKey * key, const HFSPlusCatalogFile * file)
{
	UInt32 fileID;
	UInt32 blocks;
	UInt64 bytes;
	int result = 0;
	size_t	len;
	unsigned char filename[256 * 3];

	(void) utf_encodestr(key->nodeName.unicode,
				key->nodeName.length * 2,
				filename, &len);
	filename[len] = '\0';

	/* 3843017 : Check for reserved field removed to support new bits in future */

	/* 3971173, 3977448 : HFS does not clear the attribute bit after it deletes the 
	 * last attribute.  Fix it later in 3977448 by removing the call for checking attrs
	 */
	result = CheckIfAttributeExists(file->fileID);
	if (result == 1) {
		/* Atleast one extended attribute exists for this file ID */
		/* 3843779 : Record if file record has attribute and/or security bit set */
		RecordXAttrBits(gScavGlobals, file->flags, file->fileID, kCalculatedCatalogRefNum);
#if DEBUG_XATTR
		printf ("%s: Record fileID=%d for prime modulus calculations\n", __FUNCTION__, file->fileID); 
#endif
	}
#if 0 //DEBUG_XATTR
	else { 
		if (result == 0) 
			printf ("%s: Ignoring file ID %d from prime modulus calculuation\n", __FUNCTION__, file->fileID);
	}
#endif 
	result = 0;	/* ignore result from record xattr */

	fileID = file->fileID;
	if (fileID < kHFSFirstUserCatalogNodeID) {
		RcdError(gScavGlobals, E_InvalidID);
		result = E_InvalidID;
		return (result);
	}
	if (fileID >= gCIS.nextCNID )
		gCIS.nextCNID = fileID + 1;

	gCIS.encodings |= (u_int64_t)(1ULL << MapEncodingToIndex(file->textEncoding & 0x7F));

	CheckBSDInfo(key, &file->bsdInfo, false);

	/* check out data fork extent info */
	result = CheckFileExtents(gScavGlobals, file->fileID, 0,
                                file->dataFork.extents, &blocks);
	if (result != noErr)
		return (result);

	if (file->dataFork.totalBlocks != blocks) {
		result = RecordBadAllocation(key->parentID, filename, kDataFork,
					file->dataFork.totalBlocks, blocks);
		if (result)
			return (result);
	} else {
		bytes = (UInt64)blocks * (UInt64)gScavGlobals->calculatedVCB->vcbBlockSize;
		if (file->dataFork.logicalSize > bytes) {
			result = RecordTruncation(key->parentID, filename, kDataFork,
					file->dataFork.logicalSize, bytes);
			if (result)
				return (result);
		}
	}
	/* check out resource fork extent info */
	result = CheckFileExtents(gScavGlobals, file->fileID, 0xFF,
	                          file->resourceFork.extents, &blocks);
	if (result != noErr)
		return (result);

	if (file->resourceFork.totalBlocks != blocks) {
		result = RecordBadAllocation(key->parentID, filename, kRsrcFork,
					file->resourceFork.totalBlocks, blocks);
		if (result)
			return (result);
	} else {
		bytes = (UInt64)blocks * (UInt64)gScavGlobals->calculatedVCB->vcbBlockSize;
		if (file->resourceFork.logicalSize > bytes) {
			result = RecordTruncation(key->parentID, filename, kRsrcFork,
					file->resourceFork.logicalSize, bytes);
			if (result)
				return (result);
		}
	}

	/* Collect indirect link info for later */
	if (SWAP_BE32(file->userInfo.fdType) == kHardLinkFileType  &&
            SWAP_BE32(file->userInfo.fdCreator) == kHFSPlusCreator)
		CaptureHardLink(gCIS.hardLinkRef, file->bsdInfo.special.iNodeNum);

	CheckCatalogName(key->nodeName.length, &key->nodeName.unicode[0], key->parentID, false);

	return (result);
}

/*
 * CheckThread - verify a catalog thread
 *
 * Called in leaf-order for every thread record in the Catalog B-tree
 */
static int 
CheckThread(const HFSPlusCatalogKey * key, const HFSPlusCatalogThread * thread)
{
	int result = 0;

	if (key->nodeName.length != 0) {
		RcdError(gScavGlobals, E_ThdKey);
		return (E_ThdKey);
	}

	result = CheckCatalogName(thread->nodeName.length, &thread->nodeName.unicode[0],
                         thread->parentID, true);
	if (result != noErr) {
		RcdError(gScavGlobals, E_ThdCN);
		return (E_ThdCN);
	}	

	if (key->parentID < kHFSFirstUserCatalogNodeID  &&
            key->parentID != kHFSRootParentID  &&
            key->parentID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}

	if (thread->parentID == kHFSRootParentID) {
		if (key->parentID != kHFSRootFolderID) {
			RcdError(gScavGlobals, E_InvalidID);
			return (E_InvalidID);
		}
	} else if (thread->parentID < kHFSFirstUserCatalogNodeID &&
	           thread->parentID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}

	return (0);
}

/*
 * CheckDirectory - verify an HFS catalog directory record
 *
 * Also collects info for later processing.
 * Called in leaf-order for every directory record in the Catalog B-tree
 */
static int 
CheckDirectory_HFS(const HFSCatalogKey * key, const HFSCatalogFolder * dir)
{
	UInt32 dirID;
	int result = 0;

	dirID = dir->folderID;

	/* Directory cannot have these two flags set */
	if ((dir->flags & (kHFSFileLockedMask | kHFSThreadExistsMask)) != 0) {
		RcdError(gScavGlobals, E_CatalogFlagsNotZero);
		gScavGlobals->CBTStat |= S_ReservedNotZero;
	}

	if (dirID < kHFSFirstUserCatalogNodeID  &&
            dirID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}
	if (dirID >= gCIS.nextCNID )
		gCIS.nextCNID = dirID + 1;
	
	CheckCatalogName_HFS(key->nodeName[0], &key->nodeName[1], key->parentID, false);
	
	return (result);
}

/*
 * CheckFile_HFS - verify a HFS catalog file record
 * - sanity check values
 * - collect info for later processing
 *
 * Called in b-tree leaf order for every HFS file
 * record in the Catalog B-tree.
 */
static int
CheckFile_HFS(const HFSCatalogKey * key, const HFSCatalogFile * file)
{
	UInt32 fileID;
	UInt32 blocks;
	int result = 0;

	if (file->flags & kHFSThreadExistsMask)
		++gCIS.filesWithThreads;

	/* 3843017 : Check for reserved field removed to support new bits in future */
	if ((file->dataStartBlock)          ||
	    (file->rsrcStartBlock)          ||
	    (file->reserved))
	{
		RcdError(gScavGlobals, E_CatalogFlagsNotZero);
		gScavGlobals->CBTStat |= S_ReservedNotZero;
	}

	fileID = file->fileID;
	if (fileID < kHFSFirstUserCatalogNodeID) {
		RcdError(gScavGlobals, E_InvalidID);
		result = E_InvalidID;
		return (result);
	}
	if (fileID >= gCIS.nextCNID )
		gCIS.nextCNID = fileID + 1;

	/* check out data fork extent info */
	result = CheckFileExtents(gScavGlobals, file->fileID, 0,
                                file->dataExtents, &blocks);
	if (result != noErr)
		return (result);
	if (file->dataPhysicalSize > ((UInt64)blocks * (UInt64)gScavGlobals->calculatedVCB->vcbBlockSize)) {
		PrintError(gScavGlobals, E_PEOF, 1, "");
		return (noErr);		/* we don't fix this, ignore the error */
	}
	if (file->dataLogicalSize > file->dataPhysicalSize) {
		PrintError(gScavGlobals, E_LEOF, 1, "");
                return (noErr);		/* we don't fix this, ignore the error */
	}

	/* check out resource fork extent info */
	result = CheckFileExtents(gScavGlobals, file->fileID, 0xFF,
				file->rsrcExtents, &blocks);
	if (result != noErr)
		return (result);
	if (file->rsrcPhysicalSize > ((UInt64)blocks * (UInt64)gScavGlobals->calculatedVCB->vcbBlockSize)) {
		PrintError(gScavGlobals, E_PEOF, 1, "");
                return (noErr);		/* we don't fix this, ignore the error */
	}
	if (file->rsrcLogicalSize > file->rsrcPhysicalSize) {
		PrintError(gScavGlobals, E_LEOF, 1, "");
                return (noErr);		/* we don't fix this, ignore the error */
	}
#if 1
	/* Keeping handle in globals of file ID's for HFS volume only */
	if (PtrAndHand(&file->fileID, (Handle)gScavGlobals->validFilesList, sizeof(UInt32) ) )
		return (R_NoMem);
#endif
	CheckCatalogName_HFS(key->nodeName[0], &key->nodeName[1], key->parentID, false);

	return (result);
}

/*
 * CheckThread - verify a catalog thread
 *
 * Called in leaf-order for every thread record in the Catalog B-tree
 */
static int
CheckThread_HFS(const HFSCatalogKey * key, const HFSCatalogThread * thread)
{
	int result = 0;

	if (key->nodeName[0] != 0) {
		RcdError(gScavGlobals, E_ThdKey);
		return (E_ThdKey);
	}

	result = CheckCatalogName_HFS(thread->nodeName[0], &thread->nodeName[1],
                         	  thread->parentID, true);
	if (result != noErr) {
		RcdError(gScavGlobals, E_ThdCN);
		return (E_ThdCN);
	}	

	if (key->parentID < kHFSFirstUserCatalogNodeID  &&
            key->parentID != kHFSRootParentID  &&
            key->parentID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}

	if (thread->parentID == kHFSRootParentID) {
		if (key->parentID != kHFSRootFolderID) {
			RcdError(gScavGlobals, E_InvalidID);
			return (E_InvalidID);
		}
	} else if (thread->parentID < kHFSFirstUserCatalogNodeID &&
	           thread->parentID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}

	return (0);
}


/* File types from BSD Mode */
#define FT_MASK    0170000	/* Mask of file type. */
#define FT_FIFO    0010000	/* Named pipe (fifo). */
#define FT_CHR     0020000	/* Character device. */
#define FT_DIR     0040000	/* Directory file. */
#define FT_BLK     0060000	/* Block device. */
#define FT_REG     0100000	/* Regular file. */
#define FT_LNK     0120000	/* Symbolic link. */
#define FT_SOCK    0140000	/* BSD domain socket. */

/*
 * CheckBSDInfo - Check BSD Pemissions data
 * (HFS Plus volumes only)
 *
 * if repairable then log the error and create a repair order
 */
static void
CheckBSDInfo(const HFSPlusCatalogKey * key, const HFSPlusBSDInfo * bsdInfo, int isdir)
{
#define kObsoleteUnknownUID  (-3)
#define kUnknownUID          (99)	

	Boolean reset = false;

	/* skip uninitialized BSD info */
	if (bsdInfo->fileMode == 0)
		return;
	
	switch (bsdInfo->fileMode & FT_MASK) {
	  case FT_DIR:
		if (!isdir)
			reset = true;
		break;
	  case FT_REG:
	  case FT_BLK:
	  case FT_CHR:
	  case FT_LNK:
	  case FT_SOCK:
	  case FT_FIFO:
		if (isdir)
			reset = true;
		break;
	  default:
		reset = true;
	}
	
	if (reset ||
	    ((long)bsdInfo->ownerID == kObsoleteUnknownUID) ||
	    ((long)bsdInfo->groupID == kObsoleteUnknownUID)) {
		RepairOrderPtr p;
		int n;
		
		if (reset) {
			gScavGlobals->TarBlock = bsdInfo->fileMode & FT_MASK;
			RcdError(gScavGlobals, E_InvalidPermissions);
		}

		n = CatalogNameSize( (CatalogName *) &key->nodeName, true );
		
		p = AllocMinorRepairOrder(gScavGlobals, n);
		if (p == NULL) return;

 		CopyCatalogName((const CatalogName *)&key->nodeName,
		(CatalogName*)&p->name, true);
		
		if (reset) {
			p->type      = E_InvalidPermissions;
			p->correct   = 0;
			p->incorrect = bsdInfo->fileMode;
		} else {
			p->type      = E_InvalidUID;
			p->correct   = kUnknownUID;
			if ((long)bsdInfo->ownerID == kObsoleteUnknownUID)
				p->incorrect = bsdInfo->ownerID;
			else 
				p->incorrect = bsdInfo->groupID;
		}

                p->parid = key->parentID;
		p->hint = 0;
		
		gScavGlobals->CatStat |= S_Permissions;
	}
}

/*
 * Validate a Unicode filename for HFS+ volumes
 *
 * check character count
 * check for illegal names
 *
 * if repairable then log the error and create a repair order
 */
static int
CheckCatalogName(u_int16_t charCount, const u_int16_t *uniChars, u_int32_t parentID, Boolean thread)
{
	OSErr 				result;
	u_int16_t *			myPtr;
	RepairOrderPtr 		roPtr;
	int					myLength;
    CatalogName			newName;

	if ((charCount == 0) || (charCount > kHFSPlusMaxFileNameChars))
		return( E_CName );
        
    // only do the remaining checks for files or directories
    if ( thread )
        return( noErr );
 
    // look for objects with illegal names of "." or "..".  We only do this for
    // file or folder catalog records (the thread records will be taken care of
    // in the repair routines).
    if ( charCount < 3 && *uniChars == 0x2E )
    {
        if ( charCount == 1 || (charCount == 2 && *(uniChars + 1) == 0x2E) )
        {
			PrintError( gScavGlobals, E_IllegalName, 0 );
            if ( gScavGlobals->logLevel >= kDebugLog ) {
                printf( "\tillegal name is 0x" );
                PrintName( charCount, (UInt8 *) uniChars, true );
           }
          
            // get a new name to use when we rename the file system object
            result = UniqueDotName( gScavGlobals, &newName, parentID, 
                                    ((charCount == 1) ? true : false), true );
            if ( result != noErr ) 
                return( noErr );

            // we will copy the old and new names to our RepairOrder.  The names will
            // look like this:
            // 	 2 byte length of old name
            //   unicode characters for old name
            //   2 byte length of new name
            //   unicode characters for new name 
            myLength = (charCount + 1) * 2; // bytes needed for old name 
            myLength += ((newName.ustr.length + 1) * 2); // bytes needed for new name

            roPtr = AllocMinorRepairOrder( gScavGlobals, myLength );
            if ( roPtr == NULL ) 
                return( noErr );

            myPtr = (u_int16_t *) &roPtr->name;
            *myPtr++ = charCount; // copy in length of old name and bump past it
            CopyMemory( uniChars, myPtr, (charCount * 2) ); // copy in old name
            myPtr += charCount; // bump past old name
            *myPtr++ = newName.ustr.length; // copy in length of new name and bump past it
            CopyMemory( newName.ustr.unicode, myPtr, (newName.ustr.length * 2) ); // copy in new name
            if ( gScavGlobals->logLevel >= kDebugLog ) {
                printf( "\treplacement name is 0x" );
                PrintName( newName.ustr.length, (UInt8 *) &newName.ustr.unicode, true );
           }

 			roPtr->type = E_IllegalName;
            roPtr->parid = parentID;
            gScavGlobals->CatStat |= S_IllName;
            return( E_IllegalName );
        }
    }

    // look for Unicode decomposition errors in file system object names created before Jaguar (10.2)
    if ( FixDecomps( charCount, uniChars, &newName.ustr ) )
    {
        PrintError( gScavGlobals, E_IllegalName, 0 );
        if ( gScavGlobals->logLevel >= kDebugLog ) {
            printf( "\tillegal name is 0x" );
            PrintName( charCount, (UInt8 *) uniChars, true );
        }

        // we will copy the old and new names to our RepairOrder.  The names will
        // look like this:
        // 	 2 byte length of old name
        //   unicode characters for old name
        //   2 byte length of new name
        //   unicode characters for new name 
        myLength = (charCount + 1) * 2; // bytes needed for old name 
        myLength += ((newName.ustr.length + 1) * 2); // bytes needed for new name

        roPtr = AllocMinorRepairOrder( gScavGlobals, myLength );
        if ( roPtr == NULL ) 
            return( noErr );

        myPtr = (u_int16_t *) &roPtr->name;
        *myPtr++ = charCount; // copy in length of old name and bump past it
        CopyMemory( uniChars, myPtr, (charCount * 2) ); // copy in old name
        myPtr += charCount; // bump past old name
        *myPtr++ = newName.ustr.length; // copy in length of new name and bump past it
        CopyMemory( newName.ustr.unicode, myPtr, (newName.ustr.length * 2) ); // copy in new name
        if ( gScavGlobals->logLevel >= kDebugLog ) {
            printf( "\treplacement name is 0x" );
            PrintName( newName.ustr.length, (UInt8 *) &newName.ustr.unicode, true );
        }

        roPtr->type = E_IllegalName;
        roPtr->parid = parentID;
        gScavGlobals->CatStat |= S_IllName;
        return( E_IllegalName );
    }

	return( noErr );
}


/*
 * Validate an HFS filename 
 *
 * check character count
 * check for illegal names
 *
 * if repairable then log the error and create a repair order
 */
static int
CheckCatalogName_HFS(u_int16_t charCount, const u_char *filename, u_int32_t parentID, Boolean thread)
{
	u_char *			myPtr;
	RepairOrderPtr 		roPtr;
	int					myLength;
    CatalogName			newName;

	if ((charCount == 0) || (charCount > kHFSMaxFileNameChars))
		return( E_CName );

     // only do the remaining checks for files or directories
    if ( thread )
        return( noErr );
       
    // look for objects with illegal names of "." or "..".  We only do this for
    // file or folder catalog records (the thread records will be taken care of
    // in the repair routines).
    if ( charCount < 3 && *filename == 0x2E )
    {
        if ( charCount == 1 || (charCount == 2 && *(filename + 1) == 0x2E) )
        {
            OSErr 				result;
			PrintError( gScavGlobals, E_IllegalName, 0 );
            if ( gScavGlobals->logLevel >= kDebugLog ) {
                printf( "\tillegal name is 0x" );
                PrintName( charCount, filename, false );
            }

            // get a new name to use when we rename the file system object
            result = UniqueDotName( gScavGlobals, &newName, parentID, 
                                    ((charCount == 1) ? true : false), false );
            if ( result != noErr ) 
                return( noErr );
            
            // we will copy the old and new names to our RepairOrder.  The names will
            // look like this:
            // 	 1 byte length of old name
            //   characters for old name
            //   1 byte length of new name
            //   characters for new name 
            myLength = charCount + 1; // bytes needed for old name 
            myLength += (newName.pstr[0] + 1); // bytes needed for new name
            roPtr = AllocMinorRepairOrder( gScavGlobals, myLength );
            if ( roPtr == NULL ) 
                return( noErr );
 
            myPtr = (u_char *)&roPtr->name[0];
            *myPtr++ = charCount; // copy in length of old name and bump past it
            CopyMemory( filename, myPtr, charCount );
            myPtr += charCount; // bump past old name
            *myPtr++ = newName.pstr[0]; // copy in length of new name and bump past it
            CopyMemory( &newName.pstr[1], myPtr, newName.pstr[0] ); // copy in new name
            if ( gScavGlobals->logLevel >= kDebugLog ) {
                printf( "\treplacement name is 0x" );
                PrintName( newName.pstr[0], &newName.pstr[1], false );
            }

 			roPtr->type = E_IllegalName;
            roPtr->parid = parentID;
            gScavGlobals->CatStat |= S_IllName;
            return( E_IllegalName );
        }
    }

	return( noErr );
}


/*------------------------------------------------------------------------------
UniqueDotName:  figure out a unique name we can use to rename a file system
object that has the illegal name of "." or ".."
------------------------------------------------------------------------------*/
static OSErr
UniqueDotName( 	SGlobPtr GPtr, 
            	CatalogName * theNewNamePtr, 
            	UInt32 theParID, 
                Boolean isSingleDotName,
             	Boolean isHFSPlus )
{
    u_char				newChar;
	OSErr 				result;
	int					nameLen;
	UInt16				recSize;
	SFCB *				fcbPtr;
    u_char *			myPtr;
	CatalogRecord		record;
    CatalogKey			catKey;
    u_char				dotName[] = {'d', 'o', 't', 'd', 'o', 't', 0x0d, 0x00};

  	fcbPtr = GPtr->calculatedCatalogFCB;
 
    // create key with new name
    if ( isSingleDotName )
        myPtr = &dotName[3];
    else
        myPtr = &dotName[0];
    
	nameLen = strlen((char *) myPtr );
    if ( isHFSPlus )
    {
        int		i;
        theNewNamePtr->ustr.length = nameLen;
        for ( i = 0; i < theNewNamePtr->ustr.length; i++ )
            theNewNamePtr->ustr.unicode[ i ] = (u_int16_t) *(myPtr + i);
    }
    else
    {
        theNewNamePtr->pstr[0] = nameLen;
        memcpy( &theNewNamePtr->pstr[1], myPtr, nameLen );
    }
    
    // if the name is already in use we will try appending ascii characters
    // from '0' (0x30) up to '~' (0x7E) 
    for ( newChar = 0x30; newChar < 0x7F; newChar++ )
    {
        // make sure new name isn't already there
        BuildCatalogKey( theParID, theNewNamePtr, isHFSPlus, &catKey );
        result = SearchBTreeRecord( fcbPtr, &catKey, kNoHint, NULL, &record, &recSize, NULL );
        if ( result != noErr )
            return( noErr );
            
        // new name is already there, try another
        if ( isHFSPlus )
        {
            theNewNamePtr->ustr.unicode[ nameLen ] = (u_int16_t) newChar;
            theNewNamePtr->ustr.length = nameLen + 1;
        }
        else
        {
            theNewNamePtr->pstr[ 0 ] = nameLen + 1;
            theNewNamePtr->pstr[ nameLen + 1 ] = newChar;
        }
    }

    return( -1 );
    
} /* UniqueDotName */


/* 
 * RecordBadAllocation
 *
 * Record a repair to adjust a file's allocation size.
 *
 * This could also trigger a truncation if the new block
 * count isn't large enough to cover the current LEOF.
 */
static int
RecordBadAllocation(UInt32 parID, unsigned char * filename, UInt32 forkType, UInt32 oldBlkCnt, UInt32 newBlkCnt)
{
	RepairOrderPtr 			p;
	char 					goodstr[16];
	char 					badstr[16];
	int 					n;
	Boolean 				isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	PrintError(gScavGlobals, E_PEOF, 1, filename);
	sprintf(goodstr, "%d", newBlkCnt);
	sprintf(badstr, "%d", oldBlkCnt);
	PrintError(gScavGlobals, E_BadValue, 2, goodstr, badstr);

	/* Only HFS+ is repaired here */
	if ( !isHFSPlus )
		return (E_PEOF);

	n = strlen((char *)filename);
	p = AllocMinorRepairOrder(gScavGlobals, n + 1);
	if (p == NULL)
		return (R_NoMem);

	p->type = E_PEOF;
	p->forkType = forkType;
	p->incorrect = oldBlkCnt;
	p->correct = newBlkCnt;
	p->hint = 0;
	p->parid = parID;
	p->name[0] = n;  /* pascal string */
	CopyMemory(filename, &p->name[1], n);

	gScavGlobals->CatStat |= S_FileAllocation;
	return (0);
}


/* 
 * RecordTruncation
 *
 * Record a repair to trucate a file's logical size.
 */
static int
RecordTruncation(UInt32 parID, unsigned char * filename, UInt32 forkType, UInt64 oldSize, UInt64 newSize)
{
	RepairOrderPtr	 		p;
	char 					oldSizeStr[48];
	char 					newSizeStr[48];
	int 					n;
	Boolean 				isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	PrintError(gScavGlobals, E_LEOF, 1, filename);
	sprintf(oldSizeStr, "%qd", oldSize);
	sprintf(newSizeStr, "%qd", newSize);
	PrintError(gScavGlobals, E_BadValue, 2, newSizeStr, oldSizeStr);

	/* Only HFS+ is repaired here */
	if ( !isHFSPlus )
		return (E_LEOF);

	n = strlen((char *)filename);
	p = AllocMinorRepairOrder(gScavGlobals, n + 1);
	if (p == NULL)
		return (R_NoMem);

	p->type = E_LEOF;
	p->forkType = forkType;
	p->incorrect = oldSize;
	p->correct = newSize;
	p->hint = 0;
	p->parid = parID;
	p->name[0] = n;  /* pascal string */
	CopyMemory(filename, &p->name[1], n);

	gScavGlobals->CatStat |= S_FileAllocation;
	return (0);
}


/*
 * CaptureMissingThread
 *
 * Capture info for a missing thread record so it
 * can be repaired later.  The next key is saved
 * so that the Catalog Hierarchy check can proceed.
 * The thread PID/NAME are initialized during the
 * Catalog Hierarchy check phase.
 */
static int
CaptureMissingThread(UInt32 threadID, const HFSPlusCatalogKey *nextKey)
{
	MissingThread 			*mtp;
	char 					idStr[16];
	Boolean 				isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	
	sprintf(idStr, "%d", threadID);
	PrintError(gScavGlobals, E_NoThd, 1, idStr);

	/* Only HFS+ missing threads are repaired here */
	if ( !isHFSPlus)
		return (E_NoThd);
	
	mtp = (MissingThread *) AllocateClearMemory(sizeof(MissingThread));
	if (mtp == NULL)
		return (R_NoMem);
	
	/* add it to the list of missing threads */
	mtp->link = gScavGlobals->missingThreadList;
	gScavGlobals->missingThreadList = mtp;
	
	mtp->threadID = threadID;	
	CopyMemory(nextKey, &mtp->nextKey, nextKey->keyLength + 2);

	if (gScavGlobals->RepLevel == repairLevelNoProblemsFound)
		gScavGlobals->RepLevel = repairLevelVolumeRecoverable;

	gScavGlobals->CatStat |= S_MissingThread;
	return (noErr);
}


/*
 FixDecomps.  Originally written by Peter Edberg for use in fsck_hfs.

 If inFilename needs updating and the function was able to do this without
 overflowing the 255-character limit, it returns 1 (true) and outFIlename
 contains the update file. If inFilename did not need updating, or an update
 would overflow the limit, the function returns 0 (false) and the contents of
 outFilename are undefined.
 
Function implementation:

Characters that don't require any special handling have combining class 0 and do
not begin a decomposition sequence (of 1-3 characters) that needs updating. For
these characters, the function just copies them from inFilename to outFilename
and sets the pointer outNameCombSeqPtr to NULL (when this pointer is not NULL,
it points to the beginning of a sequence of combining marks that continues up to
the current character; if the current character is combining, it may need to be
reordered into that sequence). The copying operation in cheap, and postponing it
until we know the filename needs modification would make the code much more
complicated.

This copying operation may be invoked from many places in the code, some deeply
nested - any time the code determines that the current character needs no
special handling. For this reason it has a label (CopyBaseChar) and is located
at the end of the character processing loop; various places in the code use goto
statements to jump to it (this is a situation where they are justified).

The main function loop has 4 sections.

First, it quickly determines if the high 12 bits of the character indicate that
it is in a range that has neither nonzero combining class nor any decomposition
sequences that need updating. If so, the code jumps straight to CopyBaseChar.

Second, the code determines if the character is part of a sequence that needs
updating. It checks if the current character has a corresponding action in the
replaceData array. If so, depending on the action, it may need to check for
additional matching characters in inFilename. If the sequence of 1-3 characters
is successfully matched, then a replacement sequence of 1-3 characters is
inserted at the corresponding position in outFilename. While this replacement
sequence is being inserted, each character must be checked to see if it has
nonzero combining class and needs reordering (some replacement sequences consist
entirely of combining characters and may interact with combining characters in
the filename before the updated sequence).

Third, the code handles characters whose high-order 12 bits indicated that some
action was required, but were not part of sequences that needed updating (these
may include characters that were examined in the second part but were part of
sequences that did not completely match, so there are also goto fallthroughs to
this code - labeled CheckCombClass - from the second part). These characters
have to be checked for nonzero combining class; if so, they are reordered as
necessary. Each time a new nonzero class character is encountered, it is added
to outFIlename at the correct point in any active combining character sequence
(with other characters in the sequence moved as necessary), so the sequence
pointed to by outNameCombSeqPtr is always in correct order up to the current
character.

Finally, the fourth part has the default handlers to just copy characters to
outFilename.

 */
static Boolean 
FixDecomps(	u_int16_t charCount, const u_int16_t *inFilename, HFSUniStr255 *outFilename ) 
{
    // input filename: address of curr input char,
	const u_int16_t *	inNamePtr		= inFilename;
    // and of last input char.
	const u_int16_t *	inNameLastPtr	= &inFilename[charCount - 1];	
    // output filename buffer: addr of next output char,
	u_int16_t *	outNamePtr			= outFilename->unicode;	
    // and of last possible output char.
	u_int16_t *	outNameLastPtr		= &outFilename->unicode[kHFSPlusMaxFileNameChars - 1];	
	u_int16_t *	outNameCombSeqPtr	= NULL;	// if non-NULL, start of output combining seq we are processing.
	u_int32_t	maxClassValueInSeq	= 0;
	Boolean		didModifyName		= 0;

	while (inNamePtr <= inNameLastPtr) {
		u_int16_t	shiftUniChar;	// this must be 16 bits for the kShiftUniCharOffset wraparound to work
		int32_t		rangeIndex;
		u_int32_t	shiftUniCharLo;
		u_int32_t	replDataIndex;
		u_int32_t	currCharClass;

		shiftUniChar = *inNamePtr + kShiftUniCharOffset;
		if ( shiftUniChar >= kShiftUniCharLimit )
			goto CopyBaseChar;
		rangeIndex = classAndReplIndex[shiftUniChar >> kLoFieldBitSize];
		if ( rangeIndex < 0 )
			goto CopyBaseChar;
		shiftUniCharLo = shiftUniChar & kLoFieldMask;
		replDataIndex = replaceRanges[rangeIndex][shiftUniCharLo];

		if ( replDataIndex > 0 ) {
			// we have a possible substitution (replDataIndex != 0)
			const u_int16_t *	replDataPtr;
			u_int32_t			action;
			u_int32_t			copyCount	= 0;

			replDataPtr = &replaceData[replDataIndex];
			action = *replDataPtr++;
			switch (action) {
				case kReplaceCurWithTwo:
				case kReplaceCurWithThree:
					inNamePtr++;
					copyCount = (action == kReplaceCurWithTwo)? 2: 3;
					break;
				// the next 3 cases can have a first char or replacement char with nonzero combining class
				case kIfNextOneMatchesReplaceAllWithOne:
				case kIfNextOneMatchesReplaceAllWithTwo:
					if (inNamePtr + 1 <= inNameLastPtr && *(inNamePtr + 1) == *replDataPtr++) {
						inNamePtr += 2;
						copyCount = (action == kIfNextOneMatchesReplaceAllWithOne)? 1: 2;
					} else {
						// No substitution; check for comb class & copy char
						goto CheckCombClass;
					}
					break;
				case kIfNextTwoMatchReplaceAllWithOne:
					if ( inNamePtr + 2 <= inNameLastPtr && 
                         *(inNamePtr + 1) == *replDataPtr++ && 
                         *(inNamePtr + 2) == *replDataPtr++) 
                    {
						inNamePtr += 3;
						copyCount = 1;
					} else {
						// No substitution; check for comb class & copy char
						goto CheckCombClass;
					}
					break;
			}

			// now copy copyCount chars (1-3) from replDataPtr to output, checking for comb class etc.
			if (outNamePtr + copyCount - 1 > outNameLastPtr) {
				didModifyName = 0;
				break;
			}
			while (copyCount-- > 0) {
				currCharClass = 0;
				shiftUniChar = *replDataPtr + kShiftUniCharOffset;
				if ( shiftUniChar < kShiftUniCharLimit ) {
					rangeIndex = classAndReplIndex[shiftUniChar >> kLoFieldBitSize];
					if (rangeIndex >= 0) {
						shiftUniCharLo = shiftUniChar & kLoFieldMask;
						currCharClass = combClassRanges[rangeIndex][shiftUniCharLo];
					}
				}
				// This part is similar to CheckCombClass below, which has more detailed
				// comments; see them for info.
				if ( currCharClass == 0 ) {
					outNameCombSeqPtr = NULL;
					*outNamePtr++ = *replDataPtr++;
				} else if ( outNameCombSeqPtr == NULL ) {
					outNameCombSeqPtr = outNamePtr;
					maxClassValueInSeq = currCharClass;
					*outNamePtr++ = *replDataPtr++;
				} else if ( currCharClass >= maxClassValueInSeq ) {
					// Sequence is already in correct order with current char,
					// just update maxClassValueInSeq
					maxClassValueInSeq = currCharClass;
					*outNamePtr++ = *replDataPtr++;
				} else if ( outNamePtr - outNameCombSeqPtr == 1) {
					// Here we know we need to reorder.
					// If the sequence is two chars, just interchange them
					*outNamePtr++ = *outNameCombSeqPtr;
					*outNameCombSeqPtr = *replDataPtr++;
				} else {
					// General reordering case for three or more chars.
					u_int16_t *	outNameCombCharPtr;
					u_int32_t	combCharClass;
					
					outNameCombCharPtr = outNamePtr++;
					while (outNameCombCharPtr > outNameCombSeqPtr) {
						shiftUniChar = *(outNameCombCharPtr - 1) + kShiftUniCharOffset;
						rangeIndex = classAndReplIndex[shiftUniChar >> kLoFieldBitSize];
						shiftUniCharLo = shiftUniChar & kLoFieldMask;
						combCharClass = combClassRanges[rangeIndex][shiftUniCharLo];
						if (combCharClass <= currCharClass)
							break;
						*outNameCombCharPtr = *(outNameCombCharPtr - 1);
						outNameCombCharPtr--;
					}
					*outNameCombCharPtr = *replDataPtr++;
				}
			}
			didModifyName = 1;
			continue;
		} /* end of replDataIndex > 0 */

	CheckCombClass:
		// check for combining class
		currCharClass = combClassRanges[rangeIndex][shiftUniCharLo];
		if ( currCharClass == 0 ) {
			goto CopyBaseChar;
		}
		if ( outNameCombSeqPtr == NULL ) {
			// The current char is the first of a possible sequence of chars
			// with nonzero combining class. Initialize sequence stuff, then
			// just copy char to output.
			outNameCombSeqPtr = outNamePtr;
			maxClassValueInSeq = currCharClass;
			goto CopyChar;
		}
		if ( currCharClass >= maxClassValueInSeq ) {
			// The sequence of chars with nonzero combining class is already
			// in correct order through the current char; just update the max
			// class value found in the sequence.
			maxClassValueInSeq = currCharClass;
			goto CopyChar;
		}

		// This char is at least the second in a sequence of chars with
		// nonzero combining class in the output buffer; outNameCombSeqPtr
		// points to the first in the sequence. Need to put the current
		// char into the correct place in the sequence (previous chars in
		// the sequence are already in correct order, but the current char
		// is out of place).
		
		// First make sure there is room for the new char
		if (outNamePtr > outNameLastPtr) {
			didModifyName = 0;
			break;
		}

		if (outNamePtr - outNameCombSeqPtr == 1) {
			// If the sequence is two chars, just interchange them
			*outNamePtr++ = *outNameCombSeqPtr;
			*outNameCombSeqPtr = *inNamePtr++;
		} else {
			// General case: Starting at previous end of sequence, move chars to
			// next position in string as long as their class is higher than current
			// char; insert current char where we stop. We could cache the
			// combining classes instead of re-determining them, but having multiple
			// combining marks is rare enough that it wouldn't be worth the overhead.
			// At least we don't have to recheck shiftUniChar < kShiftUniCharLimit,
			// rangeIndex != 0, etc.)
			u_int16_t *	outNameCombCharPtr;
			u_int32_t	combCharClass;

			outNameCombCharPtr = outNamePtr++;
			while (outNameCombCharPtr > outNameCombSeqPtr) {
				shiftUniChar = *(outNameCombCharPtr - 1) + kShiftUniCharOffset;
				rangeIndex = classAndReplIndex[shiftUniChar >> kLoFieldBitSize];
				shiftUniCharLo = shiftUniChar & kLoFieldMask;
				combCharClass = combClassRanges[rangeIndex][shiftUniCharLo];
				if (combCharClass <= currCharClass)
					break;
				*outNameCombCharPtr = *(outNameCombCharPtr - 1);
				outNameCombCharPtr--;
			}
			*outNameCombCharPtr = *inNamePtr++;
		}
		didModifyName = 1;
		continue;

	CopyBaseChar:
		outNameCombSeqPtr = NULL;
	CopyChar:
		// nothing special happens with this char, just copy to output
		if (outNamePtr <= outNameLastPtr) {
			*outNamePtr++ = *inNamePtr++;
		} else {
			didModifyName = 0;
			break;
		}
	} /* end of while( inNamePtr <= inNameLastPtr ) */

	if (didModifyName) {
		outFilename->length = outNamePtr - outFilename->unicode;
	}
	return didModifyName;
    
} /* FixDecomps */

/* Function :  DoesOverlap
 * 
 * Description: 
 * This function takes a start block and the count of blocks in a 
 * given extent and compares it against the list of overlapped 
 * extents in the global structure.   
 * This is useful in finding the original files that overlap with
 * the files found in catalog btree check.  If a file is found
 * overlapping, it is added to the overlap list. 
 * 
 * Input: 
 * 1. GPtr - global scavenger pointer.
 * 2. fileID - file ID being checked.
 * 3. startBlock - start block in extent.
 * 4. blockCount - total number of blocks in extent.
 * 5. forkType - type of fork being check (kDataFork, kRsrcFork).
 * 
 * Output: isOverlapped - Boolean value of true or false.
 */
static Boolean DoesOverlap(SGlobPtr GPtr, UInt32 fileID, UInt32 startBlock, UInt32 blockCount, UInt8 forkType) 
{
	int i;
	Boolean isOverlapped = false;
	ExtentInfo	*curExtentInfo;
	ExtentsTable **extentsTableH = GPtr->overlappedExtents;

	for (i = 0; i < (**extentsTableH).count; i++) {
		curExtentInfo = &((**extentsTableH).extentInfo[i]);
		/* Check extents */
		if (curExtentInfo->startBlock < startBlock) {
			if ((curExtentInfo->startBlock + curExtentInfo->blockCount) > startBlock) {
				isOverlapped = true;
				break;
			}
		} else {	/* curExtentInfo->startBlock >= startBlock */
			if (curExtentInfo->startBlock < (startBlock + blockCount)) {
				isOverlapped = true;
				break;
			}
		}
	} /* for loop Extents Table */	

	/* Add this extent to overlap list */
	if (isOverlapped) {
		AddExtentToOverlapList(GPtr, fileID, startBlock, blockCount, forkType);
	}

	return isOverlapped;
} /* DoesOverlap */

/* Function : CheckHFSPlusExtentRecords
 * 
 * Description: 
 * For all valid extents, this function calls DoesOverlap to find
 * if a given extent is overlapping with another extent existing
 * in the overlap list.
 * 
 * Input: 
 * 1. GPtr - global scavenger pointer.
 * 2. fileID - file ID being checked.
 * 3. extent - extent information to check.
 * 4. forkType - type of fork being check (kDataFork, kRsrcFork).
 * 
 * Output: None.
 */
static void CheckHFSPlusExtentRecords(SGlobPtr GPtr, UInt32 fileID, HFSPlusExtentRecord extent, UInt8 forkType) 
{
	int i;

	/* Check for overlapping extents for all extents in given extent data */
	for (i = 0; i < kHFSPlusExtentDensity; i++) {
		if (extent[i].startBlock == 0) {
			break;
		}
		DoesOverlap(GPtr, fileID, extent[i].startBlock, extent[i].blockCount, forkType);
	} 
	return;
} /* CheckHFSPlusExtentRecords */ 

/* Function : CheckHFSExtentRecords
 * 
 * Description: 
 * For all valid extents, this function calls DoesOverlap to find
 * if a given extent is overlapping with another extent existing
 * in the overlap list.
 * 
 * Input: 
 * 1. GPtr - global scavenger pointer.
 * 2. fileID - file ID being checked.
 * 3. extent - extent information to check.
 * 4. forkType - type of fork being check (kDataFork, kRsrcFork).
 * 
 * Output: None.
 */
static void CheckHFSExtentRecords(SGlobPtr GPtr, UInt32 fileID, HFSExtentRecord extent, UInt8 forkType) 
{
	int i;

	/* Check for overlapping extents for all extents in given extents */
	for (i = 0; i < kHFSExtentDensity; i++) {
		if (extent[i].startBlock == 0) {
			break;
		}
		DoesOverlap(GPtr, fileID, extent[i].startBlock, extent[i].blockCount, forkType);
	}
	return;
} /* CheckHFSExtentRecords */ 

/* Function: FindOrigOverlapFiles 
 * 
 * Description:
 * This function is called only if catalog btree check results in
 * overlapped extents errors.  The catalog btree check does not find
 * out the original files whose extents are overlapping with one
 * being reported in its check.  This function finds out all the 
 * original files whose that are being overlapped.  
 * 
 * This function relies on comparison of extents with Overlap list
 * created in verify stage.  The list is also updated with the 
 * overlapped extents found in this function. 
 * 
 * 1. Compare extents for all the files located in volume header.
 * 2. Traverse catalog btree and compare extents of all files.
 * 3. Traverse extents btree and compare extents for all entries.
 * 
 * Input: GPtr - pointer to global scanvenger area.
 * 
 * Output: err - function result
 *			zero means success
 *			non-zero means failure
 */
static int FindOrigOverlapFiles(SGlobPtr GPtr)
{
	OSErr err = noErr;
	Boolean isHFSPlus; 

	UInt16 selCode;		/* select access pattern for BTree */
	UInt16 recordSize;
	UInt32 hint;
	CatalogRecord catRecord; 
	CatalogKey catKey;
	ExtentRecord extentRecord; 
	ExtentKey extentKey;
	SVCB *calculatedVCB = GPtr->calculatedVCB;

	isHFSPlus = VolumeObjectIsHFSPlus();

	/* Check file extents from volume header */
	if (isHFSPlus) {
		/* allocation file */
		if (calculatedVCB->vcbAllocationFile) {
			CheckHFSPlusExtentRecords(GPtr, calculatedVCB->vcbAllocationFile->fcbFileID, 
		                              calculatedVCB->vcbAllocationFile->fcbExtents32, kDataFork);
		}

		/* extents file */
		if (calculatedVCB->vcbExtentsFile) {
			CheckHFSPlusExtentRecords(GPtr, calculatedVCB->vcbExtentsFile->fcbFileID, 
		                              calculatedVCB->vcbExtentsFile->fcbExtents32, kDataFork);
		}

		/* catalog file */
		if (calculatedVCB->vcbCatalogFile) {
			CheckHFSPlusExtentRecords(GPtr, calculatedVCB->vcbCatalogFile->fcbFileID, 
		                              calculatedVCB->vcbCatalogFile->fcbExtents32, kDataFork);
		}

		/* attributes file */
		if (calculatedVCB->vcbAttributesFile) {
			CheckHFSPlusExtentRecords(GPtr, calculatedVCB->vcbAttributesFile->fcbFileID, 
		                              calculatedVCB->vcbAttributesFile->fcbExtents32, kDataFork);	
	   	}

		/* startup file */
		if (calculatedVCB->vcbStartupFile) {
			CheckHFSPlusExtentRecords(GPtr, calculatedVCB->vcbStartupFile->fcbFileID, 
		                              calculatedVCB->vcbStartupFile->fcbExtents32, kDataFork);
		}
	} else {
		/* extents file */
		if (calculatedVCB->vcbExtentsFile) {
			CheckHFSExtentRecords(GPtr, calculatedVCB->vcbExtentsFile->fcbFileID, 
		                          calculatedVCB->vcbExtentsFile->fcbExtents16, kDataFork);
		}

		/* catalog file */
		if (calculatedVCB->vcbCatalogFile) {
			CheckHFSExtentRecords(GPtr, calculatedVCB->vcbCatalogFile->fcbFileID, 
		                          calculatedVCB->vcbCatalogFile->fcbExtents16, kDataFork);
		}
	}

	/* Traverse the catalog btree */ 
	selCode = 0x8001;	/* Get first record from BTree */
	err = GetBTreeRecord(GPtr->calculatedCatalogFCB, selCode, &catKey, &catRecord, &recordSize, &hint);
	if (err != noErr) {
		goto traverseExtents;
	} 
	selCode = 1;	/* Get next record */
	do {
		if ((catRecord.recordType == kHFSPlusFileRecord) ||  
		    (catRecord.recordType == kHFSFileRecord)) {
			
			if (isHFSPlus) {
				/* HFSPlus data fork */
				CheckHFSPlusExtentRecords(GPtr, catRecord.hfsPlusFile.fileID, 
			    	                      catRecord.hfsPlusFile.dataFork.extents, kDataFork);

				/* HFSPlus resource fork */
				CheckHFSPlusExtentRecords(GPtr, catRecord.hfsPlusFile.fileID, 
			    	                      catRecord.hfsPlusFile.resourceFork.extents, kRsrcFork);
			} else {
				/* HFS data extent */
				CheckHFSExtentRecords(GPtr, catRecord.hfsFile.fileID, 
			    	                  catRecord.hfsFile.dataExtents, kDataFork);

				/* HFS resource extent */
				CheckHFSExtentRecords(GPtr, catRecord.hfsFile.fileID,
				                      catRecord.hfsFile.rsrcExtents, kRsrcFork);
			}
		}

		/* Access the next record */
		err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &catKey, &catRecord, &recordSize, &hint );
	} while (err == noErr); 

traverseExtents:
	/* Traverse the extents btree */ 
	selCode = 0x8001;	/* Get first record from BTree */
	err = GetBTreeRecord(GPtr->calculatedExtentsFCB, selCode, &extentKey, &extentRecord, &recordSize, &hint);
	if (err != noErr) {
		goto out;
	}
	selCode = 1;	/* Get next record */
	do {
		if (isHFSPlus) {
			CheckHFSPlusExtentRecords(GPtr, extentKey.hfsPlus.fileID, 
			                          extentRecord.hfsPlus, extentKey.hfsPlus.forkType);
		} else {
			CheckHFSExtentRecords(GPtr, extentKey.hfs.fileID, extentRecord.hfs, 
			                      extentKey.hfs.forkType);
		}

		/* Access the next record */
		err = GetBTreeRecord(GPtr->calculatedExtentsFCB, selCode, &extentKey, &extentRecord, &recordSize, &hint);
	} while (err == noErr); 

out:
	if (err == btNotFound) {
		err = noErr;
	}
	return err;
} /* FindOrigOverlapFiles */

/* Function: PrintOverlapFiles
 *
 * Description: Print the information about all unique overlapping files.  
 * 1. Sort the overlap extent in increasing order of fileID
 * 2. For every unique fileID, prefix the string with fileID and find the
 *    filename/path based on fileID.
 *		If fileID > kHFSFirstUserCatalogNodeID, find path to file
 *		Else, find name of the system file.
 * 3. Print the new string.
 * Note that the path is printed only for HFS Plus volumes and not for 
 * plain HFS volumes.  This is done by not allocating buffer for finding
 * file path.
 *
 * Input:
 *	GPtr - Global scavenger structure pointer.
 *
 * Output:
 *	nothing (void)
 */
void PrintOverlapFiles (SGlobPtr GPtr)
{
	OSErr err;
	ExtentsTable **extentsTableH;
	ExtentInfo *extentInfo;
	unsigned int numOverlapExtents;
	unsigned int buflen, filepathlen;
	char *filepath = NULL;
	char filenum[32];
	UInt32 lastID = 0;
	UInt32 bytesWritten;
	Boolean printMsg;
	Boolean	isHFSPlus;
	int i;
	
	isHFSPlus = VolumeObjectIsHFSPlus();

	extentsTableH = GPtr->overlappedExtents;
	numOverlapExtents = (**extentsTableH).count;
	
	/* Sort the list according to file ID */
	qsort((**extentsTableH).extentInfo, numOverlapExtents, sizeof(ExtentInfo), 
		  CompareExtentFileID);

	buflen = PATH_MAX * 4;
	/* Allocate buffer to read data */
	if (isHFSPlus) {
		filepath = malloc (buflen);
	}
	
	for (i = 0; i < numOverlapExtents; i++) {
		extentInfo = &((**extentsTableH).extentInfo[i]);

		/* Skip the same fileID */
		if (lastID == extentInfo->fileID) {
			continue;
		}

		lastID = extentInfo->fileID;
		printMsg = false;

		if (filepath) {
			/* prefix with the file ID */
			bytesWritten = sprintf (filepath, "%u ", extentInfo->fileID);
			filepathlen = buflen - bytesWritten;
	
			if (extentInfo->fileID >= kHFSFirstUserCatalogNodeID) {
				/* Lookup the file path */
				err = GetFileNamePathByID (GPtr, extentInfo->fileID, (filepath + bytesWritten), &filepathlen, NULL, NULL, NULL);
			} else {
				/* Get system filename */
			 	err = GetSystemFileName (extentInfo->fileID, (filepath + bytesWritten), &filepathlen);
			}

			if (err == noErr) {
				/* print fileID, filepath */
				PrintError(GPtr, E_OvlExt, 1, filepath);
				printMsg = true;
			}
		}

		if (printMsg == false) {
			/* print only filenumber */
			sprintf(filenum, "%u", extentInfo->fileID);
			PrintError(GPtr, E_OvlExt, 1, filenum);
		}
	}

	if (filepath) {
		free (filepath);
	}

	return;
} /* PrintOverlapFiles */

/* Function: CompareExtentFileID
 *
 * Description: Compares the fileID from two ExtentInfo and return the
 * comparison result. (since we have to arrange in ascending order)
 *
 * Input:
 *	first and second - void pointers to ExtentInfo structure.
 *
 * Output:
 *	>0 if first > second
 * 	=0 if first == second
 *	<0 if first < second
 */
static int CompareExtentFileID(const void *first, const void *second)
{
	return (((ExtentInfo *)first)->fileID - 
			((ExtentInfo *)second)->fileID);
} /* CompareExtentFileID */
