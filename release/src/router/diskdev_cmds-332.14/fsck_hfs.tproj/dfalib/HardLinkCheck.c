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

#define DEBUG_HARDLINKCHECK 0

#define kBadLinkID 0xffffffff

/* info saved for each indirect link encountered */
struct IndirectLinkInfo {
	UInt32	linkID;
	UInt32	linkCount;
};

struct HardLinkInfo {
	UInt32	privDirID;
	UInt32	linkSlots;
	UInt32	slotsUsed;
	SGlobPtr globals;
	struct IndirectLinkInfo *linkInfo;
};


HFSPlusCatalogKey gMetaDataDirKey = {
	48,		/* key length */
	2,		/* parent directory ID */
	{
		21,	/* number of unicode characters */
		{
			'\0','\0','\0','\0',
			'H','F','S','+',' ',
			'P','r','i','v','a','t','e',' ',
			'D','a','t','a'
		}
	}
};


/* private local routines */
static int  GetPrivateDir(SGlobPtr gp, CatalogRecord *rec);
static int  RecordOrphanINode(SGlobPtr gp, UInt32 parID, unsigned char * filename);
static int  RecordBadLinkCount(SGlobPtr gp, UInt32 parID, unsigned char * filename,
                                int badcnt, int goodcnt);
static void hash_insert(UInt32 linkID, int m, int n, struct IndirectLinkInfo *linkInfo);
static struct IndirectLinkInfo * hash_search(UInt32 linkID, int m, int n, struct IndirectLinkInfo *linkInfo);

int average_miss_probe = 0;
int average_hit_probe = 0;

#if DEBUG_HARDLINKCHECK
int hash_search_hits = 0;
int hash_search_misses = 0;
#endif // DEBUG_HARDLINKCHECK

/*
 * HardLinkCheckBegin
 *
 * Get ready to capture indirect link info.
 * Called before iterating over all the catalog leaf nodes.
 */
int
HardLinkCheckBegin(SGlobPtr gp, void** cookie)
{
	struct HardLinkInfo *info;
	CatalogRecord rec;
	UInt32 folderID;
	int entries, slots;

	if (GetPrivateDir(gp, &rec) == 0 && rec.hfsPlusFolder.valence != 0) {
		entries = rec.hfsPlusFolder.valence + 10;
		folderID = rec.hfsPlusFolder.folderID;
	} else {
		entries = 1000;
		folderID = 0;
	}
	
	for (slots = 1; slots <= entries; slots <<= 1)
		continue;
	if (slots < (entries + (entries/3)))
		slots <<= 1;
		
#if DEBUG_HARDLINKCHECK
	printf("hash table size is %d for %d entries\n", slots, entries); 
#endif // DEBUG_HARDLINKCHECK

	info = (struct HardLinkInfo *) malloc(sizeof(struct HardLinkInfo));
	info->linkInfo = (struct IndirectLinkInfo *) calloc(slots, sizeof(struct IndirectLinkInfo));

	info->privDirID = folderID;
	info->linkSlots = slots;
	info->slotsUsed = 0;
	info->globals = gp;
	
	* cookie = info;
	return (0);
}

/*
 * HardLinkCheckEnd
 *
 * Dispose of captured data.
 * Called after calling CheckHardLinks.
 */
void
HardLinkCheckEnd(void * cookie)
{
	if (cookie) {
		struct HardLinkInfo *		infoPtr;
		
		infoPtr = (struct HardLinkInfo *) cookie;
		if (infoPtr->linkInfo) {

#if DEBUG_HARDLINKCHECK
{
	struct IndirectLinkInfo *	linkInfoPtr;
	int		i;
	printf("hash table summary:\n");
	linkInfoPtr = infoPtr->linkInfo;
	for (i = 0; i < infoPtr->linkSlots; ++linkInfoPtr, ++i) {
		printf("%5d --> %d (%d)\n", i, linkInfoPtr->linkID, linkInfoPtr->linkCount);
	}
}
#endif // DEBUG_HARDLINKCHECK

			DisposeMemory(infoPtr->linkInfo);
		}
		DisposeMemory(cookie);
	}
}

/*
 * CaptureHardLink
 *
 * Capture indirect link info.
 * Called for every indirect link in the catalog.
 */
void
CaptureHardLink(void * cookie, UInt32 linkID)
{
	struct HardLinkInfo * info = (struct HardLinkInfo *) cookie;
	struct IndirectLinkInfo *linkInfo;

	if (linkID == 0)
		linkID = kBadLinkID;	/* reported later */
	linkInfo = hash_search(linkID, info->linkSlots, info->slotsUsed, info->linkInfo);
	if (linkInfo)
		++linkInfo->linkCount;
	else
		hash_insert(linkID, info->linkSlots, info->slotsUsed++, info->linkInfo);
}

/*
 * CheckHardLinks
 *
 * Check indirect node link counts aginst the indirect
 * links that were found. There are 4 possible problems
 * that can occur.
 *  1. orphaned indirect node (i.e. no links found)
 *  2. orphaned indirect link (i.e. indirect node missing)
 *  3. incorrect link count
 *  4. indirect link id was 0 (i.e. link id wasn't preserved)
 */
int
CheckHardLinks(void *cookie)
{
	struct HardLinkInfo *info = (struct HardLinkInfo *)cookie;
	SGlobPtr gp;
	UInt32              folderID;
	UInt32 linkID;
	SFCB              * fcb;
	CatalogRecord       rec;
	HFSPlusCatalogKey * keyp;
	BTreeIterator       iterator;
	FSBufferDescriptor  btrec;
	UInt16              reclen;
	size_t              len;
	int linkCount;
	int prefixlen;
	int result;
	struct IndirectLinkInfo * linkInfo;
	unsigned char filename[64];

	/* All done if no hard links exist. */
	if (info == NULL || (info->privDirID == 0 && info->slotsUsed == 0))
		return (0);

	gp = info->globals;
	PrintStatus(gp, M_MultiLinkChk, 0);

	folderID = info->privDirID;
	linkInfo = info->linkInfo;

#if DEBUG_HARDLINKCHECK
	printf("hashtable: %d entries inserted, %d search hits, %d search misses\n",
			info->slotsUsed, hash_search_hits, hash_search_misses);
	printf("average_miss_probe = %d, average_hit_probe = %d\n",
			average_miss_probe/hash_search_misses, average_hit_probe/hash_search_hits);
#endif // DEBUG_HARDLINKCHECK

	fcb = gp->calculatedCatalogFCB;
	prefixlen = strlen(HFS_INODE_PREFIX);
	ClearMemory(&iterator, sizeof(iterator));
	keyp = (HFSPlusCatalogKey*)&iterator.key;
	btrec.bufferAddress = &rec;
	btrec.itemCount = 1;
	btrec.itemSize = sizeof(rec);
	/*
	 * position iterator at folder thread record
	 * (i.e. one record before first child)
	 */
	ClearMemory(&iterator, sizeof(iterator));
	BuildCatalogKey(folderID, NULL, true, (CatalogKey *)keyp);
	result = BTSearchRecord(fcb, &iterator, kInvalidMRUCacheKey, &btrec,
				&reclen, &iterator);
	if (result) goto exit;

	/* Visit all the children in private directory. */
	for (;;) {
		result = BTIterateRecord(fcb, kBTreeNextRecord, &iterator,
					&btrec, &reclen);
		if (result || keyp->parentID != folderID)
			break;
		if (rec.recordType != kHFSPlusFileRecord)
			continue;

		(void) utf_encodestr(keyp->nodeName.unicode,
					keyp->nodeName.length * 2,
					filename, &len);
		filename[len] = '\0';
		
		/* Report Orphaned nodes in verify or on read-only volumes when:
		 * 1. Volume was unmounted cleanly.
		 * 2. fsck is being run in debug mode.
		 */
		if ((strstr((char *)filename, HFS_DELETE_PREFIX) == (char *)filename) &&
			(gp->logLevel == kDebugLog)) {
			RecordOrphanINode(gp, folderID, filename);
			continue;		
		}
				
		if (strstr((char *)filename, HFS_INODE_PREFIX) != (char *)filename)
			continue;
		
		linkID = atol((char *)&filename[prefixlen]);
		linkCount = rec.hfsPlusFile.bsdInfo.special.linkCount;
		linkInfo = hash_search(linkID, info->linkSlots, info->slotsUsed, info->linkInfo);
		if (linkInfo) {
			if (linkCount != linkInfo->linkCount)
				RecordBadLinkCount(gp, folderID, filename,
						   linkCount, linkInfo->linkCount);
		} else {
			/* no match means this is an orphan indirect node */
			RecordOrphanINode(gp, folderID, filename);
		}
		filename[0] = '\0';
	}

	/*
	 * Any remaining indirect links are orphans.
	 *
	 * TBD: what to do with them...
	 */
#if 0
	if (gp->logLevel >= kDebugLog) {
	    for (i = 0; i < info->slotsUsed; ++i) {
		if (linkInfo[i].linkID == kBadLinkID) {
		    printf("missing link number (copied under Mac OS 9 ?)\n");
		    /*
		     * To do: loop through each file ID and report
		     */
		} else if (linkInfo[i].linkID != 0) {
		    printf("\torphaned link (indirect node %d missing)\n", linkInfo[i].linkID);
		}
	}
#endif

exit:
	return (result);
}

/*
 * GetPrivateDir
 *
 * Get catalog entry for the "HFS+ Private Data" directory.
 * The indirect nodes are stored in this directory.
 */
static int
GetPrivateDir(SGlobPtr gp, CatalogRecord * rec)
{
	HFSPlusCatalogKey * keyp;
	BTreeIterator       iterator;
	FSBufferDescriptor  btrec;
	UInt16              reclen;
	int                 result;
	Boolean 			isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	if (!isHFSPlus)
		return (-1);

	ClearMemory(&iterator, sizeof(iterator));
	keyp = (HFSPlusCatalogKey*)&iterator.key;

	btrec.bufferAddress = rec;
	btrec.itemCount = 1;
	btrec.itemSize = sizeof(CatalogRecord);

	/* look up record for HFS+ private folder */
	ClearMemory(&iterator, sizeof(iterator));
	CopyMemory(&gMetaDataDirKey, keyp, sizeof(gMetaDataDirKey));
	result = BTSearchRecord(gp->calculatedCatalogFCB, &iterator,
	                        kInvalidMRUCacheKey, &btrec, &reclen, &iterator);

	return (result);
}

/*
 * RecordOrphanINode
 *
 * Record a repair to delete an orphaned indirect node.
 */
static int
RecordOrphanINode(SGlobPtr gp, UInt32 parID, unsigned char* filename)
{
	RepairOrderPtr p;
	int n;
	
	PrintError(gp, E_UnlinkedFile, 1, filename);
	
	n = strlen((char *)filename);
	p = AllocMinorRepairOrder(gp, n + 1);
	if (p == NULL)
		return (R_NoMem);

	p->type = E_UnlinkedFile;
	p->correct = 0;
	p->incorrect = 0;
	p->hint = 0;
	p->parid = parID;
	p->name[0] = n;  /* pascal string */
	CopyMemory(filename, &p->name[1], n);

	gp->CatStat |= S_UnlinkedFile;
	return (noErr);
}


/* 
 * RecordBadLinkCount
 *
 * Record a repair to adjust an indirect node's link count.
 */
static int
RecordBadLinkCount(SGlobPtr gp, UInt32 parID, unsigned char * filename,
                   int badcnt, int goodcnt)
{
	RepairOrderPtr p;
	char goodstr[16];
	char badstr[16];
	int n;
	
	PrintError(gp, E_InvalidLinkCount, 1, filename);
	sprintf(goodstr, "%d", goodcnt);
	sprintf(badstr, "%d", badcnt);
	PrintError(gp, E_BadValue, 2, goodstr, badstr);

	n = strlen((char *)filename);
	p = AllocMinorRepairOrder(gp, n + 1);
	if (p == NULL)
		return (R_NoMem);

	p->type = E_InvalidLinkCount;
	p->incorrect = badcnt;
	p->correct = goodcnt;
	p->hint = 0;
	p->parid = parID;
	p->name[0] = n;  /* pascal string */
	CopyMemory(filename, &p->name[1], n);

	gp->CatStat |= S_LinkCount;
	return (0);
}


static void
hash_insert(UInt32 linkID, int m, int n, struct IndirectLinkInfo *linkInfo)
{
	int i, last;
	
	i = linkID & (m - 1);
	
	last = (i + (m-1)) % m;
	while ((i != last) && (linkInfo[i].linkID != 0) && (linkInfo[i].linkID != linkID))
		i = (i + 1) % m;
	
	if (linkInfo[i].linkID == 0) {
		linkInfo[i].linkID = linkID;
		linkInfo[i].linkCount = 1;
	} else if (linkInfo[i].linkID == linkID) {
		printf("hash: duplicate insert! (%d)\n", linkID);
		exit(13);
	} else {
		printf("hash table full (%d entries) \n", n);
		exit(14);
	}
}


static struct IndirectLinkInfo *
hash_search(UInt32 linkID, int m, int n, struct IndirectLinkInfo *linkInfo)
{
	int i, last;
	int p = 1;

	
	i = linkID & (m - 1);

	last = (i + (n-1)) % m;
	while ((i != last) && linkInfo[i].linkID && (linkInfo[i].linkID != linkID)) {
		i = (i + 1) % m;
		++p;
	}
	
	if (linkInfo[i].linkID == linkID) {
#if DEBUG_HARDLINKCHECK
		++hash_search_hits;
#endif // DEBUG_HARDLINKCHECK
		average_hit_probe += p;
		return (&linkInfo[i]);
	} else {
#if DEBUG_HARDLINKCHECK
		++hash_search_misses;
#endif // DEBUG_HARDLINKCHECK
		average_miss_probe += p;
		return (NULL);
	}
}
