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

/* Summary for in-memory volume bitmap:
 * A binary search tree is used to store bitmap segments that are
 * partially full.  If a segment does not exist in the tree, it
 * can be assumed to be in the following state:
 *	1. Full if the coresponding segment map bit is set
 *	2. Empty (implied)
 */

#include "Scavenger.h"

#include <bitstring.h>

#define	bit_dealloc(p)	free(p)

#define _VBC_DEBUG_	0

enum {
	kBitsPerByte		= 8,
	kBitsPerWord		= 32,
	kBitsPerSegment		= 1024,
	kBytesPerSegment	= kBitsPerSegment / kBitsPerByte,
	kWordsPerSegment	= kBitsPerSegment / kBitsPerWord,

	kBitsWithinWordMask	= kBitsPerWord-1,
	kBitsWithinSegmentMask	= kBitsPerSegment-1,
	
	kBMS_NodesPerPool	= 450,
	kBMS_PoolMax		= 2000
};


#define kAllBitsSetInWord	0xFFFFFFFFul

enum {
	kSettingBits		= 1,
	kClearingBits		= 2,
	kTestingBits		= 3
};

#define kEmptySegment	0
#define kFullSegment	1

int gBitMapInited = 0;

/*
 * Bitmap segments that are full are marked in
 * the gFullSegmentList (a bit string).
 */
bitstr_t* gFullSegmentList;
UInt32    gBitsMarked;
UInt32    gTotalBits;
UInt32    gTotalSegments;
UInt32*   gFullBitmapSegment;   /* points to a FULL bitmap segment*/
UInt32*   gEmptyBitmapSegment;  /* points to an EMPTY bitmap segment*/

/*
 * Bitmap Segment (BMS) Tree node
 * Bitmap segments that are partially full are
 * saved in the BMS Tree.
 */
typedef struct BMS_Node {
	struct BMS_Node *left;
	struct BMS_Node *right;
	UInt32 segment;
	UInt32 bitmap[kWordsPerSegment];
} BMS_Node;

BMS_Node *gBMS_Root;           /* root of BMS tree */
BMS_Node *gBMS_FreeNodes;      /* list of free BMS nodes */
BMS_Node *gBMS_PoolList[kBMS_PoolMax];  /* list of BMS node pools */
int gBMS_PoolCount;            /* count of pools allocated */



/* Segment Tree routines (binary search tree) */
static int        BMS_InitTree(void);
static int        BMS_DisposeTree(void);
static BMS_Node * BMS_Lookup(UInt32 segment);
static BMS_Node * BMS_Insert(UInt32 segment, int segmentType);
static BMS_Node * BMS_Delete(UInt32 segment);
static void	  BMS_GrowNodePool(void);

#if _VBC_DEBUG_
static void       BMS_PrintTree(BMS_Node * root);
static void       BMS_MaxDepth(BMS_Node * root, int depth, int *maxdepth);
#endif

/*
 * Initialize our volume bitmap data structures
 */
int BitMapCheckBegin(SGlobPtr g)
{
	Boolean				isHFSPlus;

	if (gBitMapInited)
		return (0);

	isHFSPlus = VolumeObjectIsHFSPlus( );

	gFullBitmapSegment = (UInt32 *)malloc(kBytesPerSegment);
	memset((void *)gFullBitmapSegment, 0xff, kBytesPerSegment);

	gEmptyBitmapSegment = (UInt32 *)malloc(kBytesPerSegment);
	memset((void *)gEmptyBitmapSegment, 0x00, kBytesPerSegment);

	gTotalBits = g->calculatedVCB->vcbTotalBlocks;
	gTotalSegments = (gTotalBits / kBitsPerSegment);
	if (gTotalBits % kBitsPerSegment)
		++gTotalSegments;

	gFullSegmentList = bit_alloc(gTotalSegments);
	bit_nclear(gFullSegmentList, 0, gTotalSegments - 1);

	BMS_InitTree();
	gBitMapInited = 1;
	gBitsMarked = 0;

	if (isHFSPlus) {
		UInt16	alignBits;
		
		/*
			* Allocate the VolumeHeader in the volume bitmap.
			* Since the VH is the 3rd sector in we may need to
			* add some alignment allocation blocks before it.
			*/
		if (g->calculatedVCB->vcbBlockSize == 512)
			alignBits = 2;
		else if (g->calculatedVCB->vcbBlockSize == 1024)
			alignBits = 1;
		else
			alignBits = 0;

		(void) CaptureBitmapBits(0, 1 + alignBits);

		if (g->calculatedVCB->vcbBlockSize == 512)
			alignBits = 1;
		else
			alignBits = 0;
		
		(void) CaptureBitmapBits(gTotalBits - 1 - alignBits, 1 + alignBits);
	}

	return (0);
}

/* debugging stats */
int gFullSegments = 0;
int gSegmentNodes = 0;

int BitMapCheckEnd(void)
{
	if (gBitMapInited) {
#if _VBC_DEBUG_
		int maxdepth = 0;

		BMS_MaxDepth(gBMS_Root, 0, &maxdepth);
		printf("   %d full segments, %d segment nodes (max depth was %d nodes)\n",
		       gFullSegments, gSegmentNodes, maxdepth);
#endif
		free(gFullBitmapSegment);
		gFullBitmapSegment = NULL;

		free(gEmptyBitmapSegment);
		gEmptyBitmapSegment = NULL;

		bit_dealloc(gFullSegmentList);
		gFullSegmentList = NULL;

		BMS_DisposeTree();
		gBitMapInited = 0;
	}
	return (0);
}

/* Function: GetSegmentBitmap
 *
 * Description: Return bitmap segment corresponding to given startBit.
 * 
 *	1. Calculate the segment number for given bit.
 *	2. If the segment exists in full segment list,
 *			If bitOperation is to clear bits, 
 *			a. Remove segment from full segment list.
 *			b. Insert a full segment in the bitmap tree.
 *			Else return pointer to dummy full segment
 *	3. If segment found in tree, it is partially full.  Return it.
 *	4. If (2) and (3) are not true, it is a empty segment.
 *			If bitOperation is to set bits,
 *			a. Insert empty segment in the bitmap tree.
 *			Else return pointer to dummy empty segment.
 *
 * Input:	
 *	1. startBit - bit number (block number) to lookup
 *	2. buffer - pointer to return pointer to bitmap segment
 *	3. bitOperation - intent for new segment
 *		kSettingBits	- caller wants to set bits
 *		kClearingBits	- caller wants to clear bits
 *		kTestingBits	- caller wants to test bits.
 *
 * Output:
 *	1. buffer - pointer to desired segment 
 *	returns zero on success, -1 on failure.
 */
static int GetSegmentBitmap(UInt32 startBit, UInt32 **buffer, int bitOperation)
{
	UInt32 segment;
	BMS_Node *segNode = NULL;
	
	*buffer = NULL;
	segment = startBit / kBitsPerSegment;

	// for a full seqment...
	if (bit_test(gFullSegmentList, segment)) {
		if (bitOperation == kClearingBits) {
                    bit_clear(gFullSegmentList, segment);
					--gFullSegments;
                    if ((segNode = BMS_Insert(segment, kFullSegment)) != NULL)
                        *buffer = &segNode->bitmap[0];
		} else
			*buffer = gFullBitmapSegment;
	
	// for a  partially full segment..
	} else if ((segNode = BMS_Lookup(segment)) != NULL) {
			*buffer = &segNode->bitmap[0];
	
	// for an empty segment...
	} else {
		if (bitOperation == kSettingBits) { 
			if ((segNode = BMS_Insert(segment, kEmptySegment)) != NULL)
				*buffer = &segNode->bitmap[0];
		} else	
			*buffer = gEmptyBitmapSegment;
	}
		
	if (*buffer == NULL) {
#if _VBC_DEBUG_
		printf("GetSegmentBitmap: couldn't get a node for block %d, segment %d\n", startBit, segment);
#endif
		return (-1); /* oops */
	}

#if 0
	if (segNode) {
		int i;
		printf("  segment %d: L=0x%08x, R=0x%08x \n< ",
			(int)segNode->segment, (int)segNode->left, segNode->right);
		for (i = 0; i < kWordsPerSegment; ++i) {
			printf("0x%08x ", segNode->bitmap[i]);
			if ((i & 0x3) == 0x3)
				printf("\n  ");
		}
		printf("\n");
	}

	if (bitOperation == kSettingBits && *buffer && bcmp(*buffer, gFullBitmapSegment, kBytesPerSegment) == 0) {
		printf("*** segment %d (start blk %d) is already full!\n", segment, startBit);
		exit(5);
	}
	if (bitOperation == kClearingBits && *buffer && bcmp(*buffer, gEmptyBitmapSegment, kBytesPerSegment) == 0) {
		printf("*** segment %d (start blk %d) is already empty!\n", segment, startBit);
		exit(5);
	}
#endif

	return (0);
}

/* Function: TestSegmentBitmap
 *
 * Description:  Test if the current bitmap segment is a full
 * segment or empty segment. 
 * If full segment, delete the segment, set corresponding full segment
 * bit in gFullSegmentList, and update counters.
 * If empty list, delete the segment from list.  Note that we update 
 * the counter only for debugging purposes.
 *
 * Input: 
 *	startBit - startBit of segment to test
 *
 * Output:
 *	nothing (void).
 */
void TestSegmentBitmap(UInt32 startBit)
{
	UInt32 segment;
	BMS_Node *segNode = NULL;

	segment = startBit / kBitsPerSegment;

	if (bit_test(gFullSegmentList, segment))
		return;

	if ((segNode = BMS_Lookup(segment)) != NULL) {
#if 0
		int i;
		printf("> ");
		for (i = 0; i < kWordsPerSegment; ++i) {
			printf("0x%08x ", segNode->bitmap[i]);
			if ((i & 0x3) == 0x3)
				printf("\n  ");
		}
		printf("\n");
#endif
		if (segment != 0 && bcmp(&segNode->bitmap[0], gFullBitmapSegment, kBytesPerSegment) == 0) {
			if (BMS_Delete(segment) != NULL) {
				bit_set(gFullSegmentList, segment);
				/* debugging stats */
				++gFullSegments;
				--gSegmentNodes;
			}
		}
		
		if (segment != 0 && bcmp(&segNode->bitmap[0], gEmptyBitmapSegment, kBytesPerSegment) == 0) {
			if (BMS_Delete(segment) != NULL) {
				/* debugging stats */
				--gSegmentNodes;
			}
		}
	}
}

/* Function: CaptureBitmapBits
 *
 * Description: Set bits in the segmented bitmap from startBit upto 
 * bitCount bits.  
 *
 * Note: This function is independent of the previous state of the bit
 * to be set.  Therefore single bit can be set multiple times.  Setting a 
 * bit multiple times might result in incorrect total number of blocks used 
 * (which can be corrected using UpdateFreeBlockCount function).
 *
 * 1. Increment gBitsMarked with bitCount.
 * 2. If first bit does not start on word boundary, special case it.
 * 3. Set all whole words. 
 * 4. If not all bits in last word need to be set, special case it.
 * 5. For 2, 3, and 4, call TestSegmentBitmap after writing one segment or 
 * setting all bits to optimize full and empty segment list.
 * 
 * Input:
 *	startBit - bit number in segment bitmap to start set operation.
 *  bitCount - total number of bits to set.
 *
 * Output:
 *	zero on success, non-zero on failure.
 *	This function also returns E_OvlExt if any overlapping extent is found.
 */
int CaptureBitmapBits(UInt32 startBit, UInt32 bitCount)
{
	Boolean overlap;
	OSErr   err;
	UInt32  wordsLeft;
	UInt32  bitMask;
	UInt32  firstBit;
	UInt32  numBits;
	UInt32  *buffer;
	UInt32  *currentWord;

	overlap = false;
	if (bitCount == 0)
		return (0);

	if ((startBit + bitCount) > gTotalBits) {
		err = vcInvalidExtentErr;
		goto Exit;
	}

	/* count allocated bits */
	gBitsMarked += bitCount;

	/*
	 * Get the bitmap segment containing the first word to check
	 */
	err = GetSegmentBitmap(startBit, &buffer, kSettingBits);
	if (err != noErr) goto Exit;

	/* Initialize buffer stuff */
	{
		UInt32 wordIndexInSegment;

		wordIndexInSegment = (startBit & kBitsWithinSegmentMask) / kBitsPerWord;
		currentWord = buffer + wordIndexInSegment;
		wordsLeft = kWordsPerSegment - wordIndexInSegment;
	}
	
	/*
	 * If the first bit to check doesn't start on a word
	 * boundary in the bitmap, then treat that first word
	 * specially.
	 */
	firstBit = startBit % kBitsPerWord;
	if (firstBit != 0) {
		bitMask = kAllBitsSetInWord >> firstBit;  // turn off all bits before firstBit
		numBits = kBitsPerWord - firstBit;	// number of remaining bits in this word
		if (numBits > bitCount) {
			numBits = bitCount;	// entire allocation is inside this one word
			bitMask &= ~(kAllBitsSetInWord >> (firstBit + numBits)); // turn off bits after last
		}

		if (SWAP_BE32(*currentWord) & bitMask) {
			overlap = true;

		//	printf("(1) overlapping file blocks! word: 0x%08x, mask: 0x%08x\n", *currentWord, bitMask);
		}
		
		*currentWord |= SWAP_BE32(bitMask);  /* set the bits in the bitmap */
		
		bitCount -= numBits;
		++currentWord;
		--wordsLeft;
		if (wordsLeft == 0 || bitCount == 0)
			TestSegmentBitmap(startBit);
	}

	/*
	 * Set whole words (32 bits) at a time.
	 */
	bitMask = kAllBitsSetInWord;
	while (bitCount >= kBitsPerWord) {
		/* See if it's time to move to the next bitmap segment */
		if (wordsLeft == 0) {
			startBit += kBitsPerSegment;	 // generate a bit in the next bitmap segment
			
			err = GetSegmentBitmap(startBit, &buffer, kSettingBits);
			if (err != noErr) goto Exit;
			
			// Readjust currentWord, wordsLeft
			currentWord = buffer;
			wordsLeft = kWordsPerSegment;
		}
		
		if (SWAP_BE32(*currentWord) & bitMask) {
			overlap = true;

		//	printf("(2) overlapping file blocks! word: 0x%08x, mask: 0x%08x\n", *currentWord, bitMask);
		}
		
		*currentWord |= SWAP_BE32(bitMask);  /* set the bits in the bitmap */

		bitCount -= kBitsPerWord;
		++currentWord;
		--wordsLeft;
		if (wordsLeft == 0 || bitCount == 0)
			TestSegmentBitmap(startBit);
	}
	
	/*
	 * Check any remaining bits.
	 */
	if (bitCount != 0) {
		bitMask = ~(kAllBitsSetInWord >> bitCount);	// set first bitCount bits
		if (wordsLeft == 0) {
			startBit += kBitsPerSegment;
			
			err = GetSegmentBitmap(startBit, &buffer, kSettingBits);
			if (err != noErr) goto Exit;
			
			currentWord = buffer;
			wordsLeft = kWordsPerSegment;
		}
		
		if (SWAP_BE32(*currentWord) & bitMask) {
			overlap = true;

		//	printf("(3) overlapping file blocks! word: 0x%08x, mask: 0x%08x\n", *currentWord, bitMask);
		}
		
		*currentWord |= SWAP_BE32(bitMask);  /* set the bits in the bitmap */

		TestSegmentBitmap(startBit);
	}
Exit:
	return (overlap ? E_OvlExt : err);
}

/* Function: ReleaseBitMapBits
 *
 * Description: Clear bits in the segmented bitmap from startBit upto 
 * bitCount bits.  
 *
 * Note: This function is independent of the previous state of the bit
 * to clear.  Therefore single bit can be cleared multiple times.  Clearing a 
 * bit multiple times might result in incorrect total number of blocks used 
 * (which can be corrected using UpdateFreeBlockCount function).
 *
 * 1. Decrement gBitsMarked with bitCount.
 * 2. If first bit does not start on word boundary, special case it.
 * 3. Clear all whole words. 
 * 4. If partial bits in last word needs to be cleared, special case it.
 * 5. For 2, 3, and 4, call TestSegmentBitmap after writing one segment or 
 * clearing all bits to optimize full and empty segment list.
 * 
 * Input:
 *	startBit - bit number in segment bitmap to start clear operation.
 *  bitCount - total number of bits to clear.
 *
 * Output:
 *	zero on success, non-zero on failure.
 *	This function also returns E_OvlExt if any overlapping extent is found.
 */
int ReleaseBitmapBits(UInt32 startBit, UInt32 bitCount)
{
	Boolean overlap;
	OSErr   err;
	UInt32  wordsLeft;
	UInt32  bitMask;
	UInt32  firstBit;
	UInt32  numBits;
	UInt32  *buffer;
	UInt32  *currentWord;

	overlap = false;
	if (bitCount == 0)
		return (0);

	if ((startBit + bitCount) > gTotalBits) {
		err = vcInvalidExtentErr;
		goto Exit;
	}

	/* decrment allocated bits */
	gBitsMarked -= bitCount;

	/*
	 * Get the bitmap segment containing the first word to check
	 */
	err = GetSegmentBitmap(startBit, &buffer, kClearingBits);
	if (err != noErr) goto Exit;

	/* Initialize buffer stuff */
	{
		UInt32 wordIndexInSegment;

		wordIndexInSegment = (startBit & kBitsWithinSegmentMask) / kBitsPerWord;
		currentWord = buffer + wordIndexInSegment;
		wordsLeft = kWordsPerSegment - wordIndexInSegment;
	}
	
	/*
	 * If the first bit to check doesn't start on a word
	 * boundary in the bitmap, then treat that first word
	 * specially.
	 */
	firstBit = startBit % kBitsPerWord;
	if (firstBit != 0) {
		bitMask = kAllBitsSetInWord >> firstBit;  // turn off all bits before firstBit
		numBits = kBitsPerWord - firstBit;	// number of remaining bits in this word
		if (numBits > bitCount) {
			numBits = bitCount;	// entire deallocation is inside this one word
			bitMask &= ~(kAllBitsSetInWord >> (firstBit + numBits)); // turn off bits after last
		}

		if ((SWAP_BE32(*currentWord) & bitMask) != bitMask) {
			overlap = true;

		//	printf("(1) overlapping file blocks! word: 0x%08x, mask: 0x%08x\n", *currentWord, bitMask);
		}
		
		*currentWord &= SWAP_BE32(~bitMask);  /* clear the bits in the bitmap */
		
		bitCount -= numBits;
		++currentWord;
		--wordsLeft;
		if (wordsLeft == 0 || bitCount == 0)
			TestSegmentBitmap(startBit);
	}

	/*
	 * Clear whole words (32 bits) at a time.
	 */
	bitMask = kAllBitsSetInWord;
	while (bitCount >= kBitsPerWord) {
		/* See if it's time to move to the next bitmap segment */
		if (wordsLeft == 0) {
			startBit += kBitsPerSegment;	 // generate a bit in the next bitmap segment
			
			err = GetSegmentBitmap(startBit, &buffer, kClearingBits);
			if (err != noErr) goto Exit;
			
			// Readjust currentWord, wordsLeft
			currentWord = buffer;
			wordsLeft = kWordsPerSegment;
		}
		
		if ((SWAP_BE32(*currentWord) & bitMask) != bitMask) {
			overlap = true;

		//	printf("(2) overlapping file blocks! word: 0x%08x, mask: 0x%08x\n", *currentWord, bitMask);
		}
		
		*currentWord &= SWAP_BE32(~bitMask);  /* clear the bits in the bitmap */

		bitCount -= kBitsPerWord;
		++currentWord;
		--wordsLeft;
		if (wordsLeft == 0 || bitCount == 0)
			TestSegmentBitmap(startBit);
	}
	
	/*
	 * Check any remaining bits.
	 */
	if (bitCount != 0) {
		bitMask = ~(kAllBitsSetInWord >> bitCount);	// set first bitCount bits
		if (wordsLeft == 0) {
			startBit += kBitsPerSegment;
			
			err = GetSegmentBitmap(startBit, &buffer, kClearingBits);
			if (err != noErr) goto Exit;
			
			currentWord = buffer;
			wordsLeft = kWordsPerSegment;
		}
		
		if ((SWAP_BE32(*currentWord) & bitMask) != bitMask) {
			overlap = true;

		//	printf("(3) overlapping file blocks! word: 0x%08x, mask: 0x%08x\n", *currentWord, bitMask);
		}
		
		*currentWord &= SWAP_BE32(~bitMask);  /* set the bits in the bitmap */

		TestSegmentBitmap(startBit);
	}
Exit:
	return (overlap ? E_OvlExt : err);
}

/* Function: CheckVolumeBitMap
 *
 * Description: Compares the in-memory volume bitmap with the on-disk
 * volume bitmap. 
 * If repair is true, update the on-disk bitmap with the in-memory bitmap.
 * If repair is false and the bitmaps don't match, an error message is 
 * printed and check stops.
 *
 * Input:
 *	1. g - global scavenger structure
 *	2. repair - indicate if a repair operation is requested or not.
 *
 * Output:
 *	zero on success, non-zero on failure.
 */
int CheckVolumeBitMap(SGlobPtr g, Boolean repair)
{
	UInt8 *vbmBlockP;
	UInt32 *buffer;
	UInt32 bit;
	UInt32 bitsWithinFileBlkMask;
	UInt32 fileBlk;
	BlockDescriptor block;
	ReleaseBlockOptions relOpt;
	SFCB * fcb;
	SVCB * vcb;
	Boolean	 isHFSPlus;
	int err = 0;
	
	vcb = g->calculatedVCB;
	fcb = g->calculatedAllocationsFCB;
	isHFSPlus = VolumeObjectIsHFSPlus( );
	
	if ( vcb->vcbFreeBlocks != (vcb->vcbTotalBlocks - gBitsMarked) ) {
		vcb->vcbFreeBlocks = vcb->vcbTotalBlocks - gBitsMarked;
		MarkVCBDirty(vcb);
	}

	vbmBlockP = (UInt8 *)NULL;
	block.buffer = (void *)NULL;
	relOpt = kReleaseBlock;
	if ( isHFSPlus )
		bitsWithinFileBlkMask = (fcb->fcbBlockSize * 8) - 1;
	else
		bitsWithinFileBlkMask = (kHFSBlockSize * 8) - 1;
	fileBlk = (isHFSPlus ? 0 : vcb->vcbVBMSt);

	/* 
	 * Loop through all the bitmap segments and compare
	 * them against the on-disk bitmap.
	 */
	for (bit = 0; bit < gTotalBits; bit += kBitsPerSegment) {
		(void) GetSegmentBitmap(bit, &buffer, kTestingBits);

		/* 
		 * When we cross file block boundries read a new block from disk.
		 */
		if ((bit & bitsWithinFileBlkMask) == 0) {
			if (isHFSPlus) {
				if (block.buffer) {
					err = ReleaseFileBlock(fcb, &block, relOpt);
					ReturnIfError(err);
				}
				err = GetFileBlock(fcb, fileBlk, kGetBlock, &block);
			} else /* plain HFS */ {
				if (block.buffer) {
					err = ReleaseVolumeBlock(vcb, &block, relOpt | kSkipEndianSwap);
					ReturnIfError(err);
				}
				err = GetVolumeBlock(vcb, fileBlk, kGetBlock | kSkipEndianSwap, &block);
			}
			ReturnIfError(err);

			vbmBlockP = (UInt8 *) block.buffer;
			relOpt = kReleaseBlock;
			g->TarBlock = fileBlk;
			++fileBlk;
		}
		if (memcmp(buffer, vbmBlockP + (bit & bitsWithinFileBlkMask)/8, kBytesPerSegment) == 0)
			continue;

		if (repair) {
			bcopy(buffer, vbmBlockP + (bit & bitsWithinFileBlkMask)/8, kBytesPerSegment);
			relOpt = kForceWriteBlock;
		} else {
#if _VBC_DEBUG_
			int i;

			printf("  disk buffer + %d\n", (bit & bitsWithinFileBlkMask)/8);
			printf("  segment %d\nM ", bit / kBitsPerSegment);
			for (i = 0; i < kWordsPerSegment; ++i) {
				printf("0x%08x ", buffer[i]);
				if ((i & 0x7) == 0x7)
					printf("\n  ");
			}
			buffer = (UInt32*) (vbmBlockP + (bit & bitsWithinFileBlkMask)/8);
			printf("\nD ");
			for (i = 0; i < kWordsPerSegment; ++i) {
				printf("0x%08x ", buffer[i]);
				if ((i & 0x7) == 0x7)
					printf("\n  ");
			}
#endif
			PrintError(g, E_VBMDamaged, 0);
			g->VIStat = g->VIStat | S_VBM;
			break; /* stop checking after first miss */
		}
		++g->itemsProcessed;
	}

	if (block.buffer) {
		if (isHFSPlus)
			(void) ReleaseFileBlock(fcb, &block, relOpt);
		else
			(void) ReleaseVolumeBlock(vcb, &block, relOpt | kSkipEndianSwap);
	}

	return (0);
}

/* Function: UpdateFreeBlockCount
 *
 * Description: Re-calculate the total bits marked in in-memory bitmap 
 * by traversing the entire bitmap.  Update the total number of bits set in 
 * the in-memory volume bitmap and the volume free block count.
 *
 * Input:
 * 	g - global scavenger structure pointer.
 *
 * Output:
 *	nothing (void)
 */
void UpdateFreeBlockCount(SGlobPtr g)
{
	int i;
	UInt32 newBitsMarked = 0;
	UInt32 bit;
	UInt32 *buffer;
	UInt32 curWord;
	SVCB * vcb = g->calculatedVCB;
	
	/* Loop through all the bitmap segments */
	for (bit = 0; bit < gTotalBits; bit += kBitsPerSegment) {
		(void) GetSegmentBitmap(bit, &buffer, kTestingBits);

		/* All bits in segment are set */
		if (buffer == gFullBitmapSegment) {
			newBitsMarked += kBitsPerSegment;
			continue;
		}

		/* All bits in segment are clear */
		if (buffer == gEmptyBitmapSegment) {
			continue;
		}

		/* Segment is partially full */
		for (i = 0; i < kWordsPerSegment; i++) {
			if (buffer[i] == kAllBitsSetInWord) {
				newBitsMarked += kBitsPerWord;
			} else {
				curWord = buffer[i];
				while (curWord) {
					newBitsMarked += curWord & 1;
					curWord >>= 1;
				}
			}
		} 
	} 
	
	/* Update total bits marked count for in-memory bitmap */
	if (gBitsMarked != newBitsMarked) {
		gBitsMarked = newBitsMarked;
	}

	/* Update volume free block count */
	if (vcb->vcbFreeBlocks != (vcb->vcbTotalBlocks - gBitsMarked)) {
		vcb->vcbFreeBlocks = vcb->vcbTotalBlocks - gBitsMarked;
		MarkVCBDirty(vcb);
	}
}


/*
 * BITMAP SEGMENT TREE
 *
 * A binary search tree is used to store bitmap segments that are
 * partially full.  If a segment does not exist in the tree, it
 * can be assumed to be in the following state:
 *	1. Full if the coresponding segment map bit is set
 *	2. Empty (implied)
 */

static int
BMS_InitTree(void)
{
	gBMS_PoolCount = 0;
	BMS_GrowNodePool();

	gBMS_Root = gBMS_FreeNodes;
	gBMS_FreeNodes = gBMS_FreeNodes->right; 
	gBMS_Root->right = NULL; 

	return (0);
}


static int
BMS_DisposeTree(void)
{
	while(gBMS_PoolCount > 0)
		free(gBMS_PoolList[gBMS_PoolCount--]);

	gBMS_Root = gBMS_FreeNodes = 0;
	return (0);
}


static BMS_Node *
BMS_Lookup(UInt32 segment)
{
	BMS_Node *ptree = gBMS_Root;

	while (ptree && ptree->segment != segment) {

		if (segment > ptree->segment)
			ptree = ptree->right;
		else
			ptree = ptree->left;
	}

	return ((BMS_Node *)ptree);
}


static BMS_Node *
BMS_InsertTree(BMS_Node *NewEntry)
{
	BMS_Node *ptree;
	register UInt32 segment;

	segment = NewEntry->segment;
	ptree = gBMS_Root;
	if (ptree == (BMS_Node *)NULL) {
		*ptree = *NewEntry;
		return (NewEntry);
	}

	while (ptree) {
		if (segment > ptree->segment) { /* walk the right sub-tree */
			if (ptree->right)
				ptree = ptree->right;
			else {
				ptree->right = NewEntry;
				return (ptree);
			}
		}
		else { /* walk the left sub-tree */
			if (ptree->left) 
				ptree = ptree->left;
			else {
			    	ptree->left = NewEntry;
				return (ptree);
			}
		}
	}	

	return ((BMS_Node *)NULL);
}


/* insert a new segment into the tree */
static BMS_Node *
BMS_Insert(UInt32 segment, int segmentType) 
{
	BMS_Node *new; 

	if ((new = gBMS_FreeNodes) == NULL) {
		BMS_GrowNodePool();
		if ((new = gBMS_FreeNodes) == NULL)
			return ((BMS_Node *)NULL);
	}

	gBMS_FreeNodes = gBMS_FreeNodes->right; 

	++gSegmentNodes;  /* debugging stats */

	new->right = NULL; 
	new->segment = segment;
	if (segmentType == kFullSegment)
		bcopy(gFullBitmapSegment, new->bitmap, kBytesPerSegment);
	else
		bzero(new->bitmap, sizeof(new->bitmap));	

	if (BMS_InsertTree(new) != NULL)
		return (new);
	else
		return ((BMS_Node *)NULL);
}


static BMS_Node *
BMS_Delete(UInt32 segment)
{
	BMS_Node *seg_found, *pprevious, *pnext, *pnextl, *psub;

	pprevious = NULL; 
	seg_found = gBMS_Root;
	
	/* don't allow the root to be deleted! */
	if (seg_found->segment == segment)
		return ((BMS_Node *)NULL);

	while (seg_found && seg_found->segment != segment) {
		pprevious = seg_found;
		if (segment > seg_found->segment)
			seg_found = seg_found->right;
		else
			seg_found = seg_found->left;
	}

	if (seg_found) {
		/*
		 * we found the entry, now reorg the sub-trees
		 * spanning from our node.
		 */
		if ((pnext = seg_found->right)) {
			/*
			 * Tree pruning: take the left branch of the
			 * current node and place it at the lowest
			 * left branch of the current right branch 
			 */
			psub = pnext;
			
			/* walk the Right/Left sub tree from current node */
			while ((pnextl = psub->left))
				psub = pnextl;	
			
			/* plug the old left tree to the new ->Right leftmost node */	
			psub->left = seg_found->left;
		} else {  /* only left sub-tree, simple case */
			pnext = seg_found->left;
		}
		/* 
		 * Now, plug the current node sub tree to
		 * the good pointer of our parent node.
		 */
		if (pprevious->left == seg_found)
			pprevious->left = pnext;
		else
			pprevious->right = pnext;	
		
		/* add node back to the free-list */
		bzero(seg_found, sizeof(BMS_Node));
		seg_found->right = gBMS_FreeNodes; 
		gBMS_FreeNodes = seg_found; 		
	}
	
	return (seg_found);
}


static void
BMS_GrowNodePool(void)
{
	BMS_Node *nodePool;
	short i;

	if (gBMS_PoolCount > kBMS_PoolMax)
		return;

	nodePool = (BMS_Node *)malloc(sizeof(BMS_Node) * kBMS_NodesPerPool);
	if (nodePool != NULL) {
		bzero(&nodePool[0], sizeof(BMS_Node) * kBMS_NodesPerPool);
		for (i = 1 ; i < kBMS_NodesPerPool ; i++) {
			(&nodePool[i-1])->right = &nodePool[i];
		}
	
		gBMS_FreeNodes = &nodePool[0];
		gBMS_PoolList[gBMS_PoolCount++] = nodePool;
	}
}


#if _VBC_DEBUG_
static void
BMS_MaxDepth(BMS_Node * root, int depth, int *maxdepth)
{
	if (root) {
		depth++;
		if (depth > *maxdepth)
			*maxdepth = depth;
		BMS_MaxDepth(root->left, depth, maxdepth);
		BMS_MaxDepth(root->right, depth, maxdepth);
	}
}

static void
BMS_PrintTree(BMS_Node * root)
{
	if (root) {
		BMS_PrintTree(root->left);
		printf("seg %d\n", root->segment);
		BMS_PrintTree(root->right);
	}
}
#endif

