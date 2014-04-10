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
/*
	File:		SControl.c

	Contains:	This file contains the routines which control the scavenging operations.

	Version:	xxx put version here xxx

	Written by:	Bill Bruffey

	Copyright:	й 1985, 1986, 1992-1999 by Apple Computer, Inc., all rights reserved.
*/

#define SHOW_ELAPSED_TIMES  0


#if SHOW_ELAPSED_TIMES
#include <sys/time.h>
#endif

#include "Scavenger.h"

#define	DisplayTimeRemaining 0


int gGUIControl;



// Static function prototypes

static void printVerifyStatus( SGlobPtr GPtr );
static Boolean IsBlueBoxSharedDrive ( DrvQElPtr dqPtr );
static int ScavSetUp( SGlobPtr GPtr );
static int ScavTerm( SGlobPtr GPtr );

/* this procedure recieves progress calls and will allow canceling of procedures - we are using it here to print out the progress of the current operation for DFA and DiskUtility - ESP 1/10/00 */

int cancelProc(UInt16 progress, UInt16 secondsRemaining, Boolean progressChanged, UInt16 stage, void *context)
{

    if (progressChanged)
        printf("(%d %%)\n", progress);
    fflush(stdout);
    return 0;
}


/*------------------------------------------------------------------------------

External
 Routines:		CheckHFS	- Controls the scavenging process.

------------------------------------------------------------------------------*/

int
CheckHFS( 	int fsReadRef, int fsWriteRef, int checkLevel, int repairLevel, 
			int logLevel, int guiControl, int lostAndFoundMode, 
			int canWrite, int *modified )
{
	SGlob				dataArea;	// Allocate the scav globals
	short				temp; 	
	FileIdentifierTable	*fileIdentifierTable	= nil;
	OSErr				err = noErr;
	OSErr				scavError = 0;
	int					scanCount = 0;
	int					isJournaled = 0;
	Boolean 			autoRepair;

	autoRepair = (fsWriteRef != -1 && repairLevel != kNeverRepair);

DoAgain:
	ClearMemory( &dataArea, sizeof(SGlob) );

	//	Initialize some scavanger globals
	dataArea.itemsProcessed		= 0;	//	Initialize to 0% complete
	dataArea.itemsToProcess		= 1;
	dataArea.chkLevel			= checkLevel;
	dataArea.repairLevel		= repairLevel;
	dataArea.logLevel			= logLevel;
	dataArea.canWrite			= canWrite;
	dataArea.lostAndFoundMode	= lostAndFoundMode;
	dataArea.DrvNum				= fsReadRef;
    
    /* there are cases where we cannot get the name of the volume so we */
    /* set our default name to one blank */
	dataArea.volumeName[ 0 ] = ' ';
	dataArea.volumeName[ 1 ] = '\0';

	if (guiControl) {
	    dataArea.guiControl = true;
    	dataArea.userCancelProc = cancelProc;
	}
	//
	//	Initialize the scavenger
	//
	ScavCtrl( &dataArea, scavInitialize, &scavError );
	if ( checkLevel == kNeverCheck || (checkLevel == kDirtyCheck && dataArea.cleanUnmount) ||
						scavError == R_NoMem || scavError == R_BadSig) {
		// also need to bail when allocate fails in ScavSetUp or we bus error!
		goto termScav;
	}

	isJournaled = CheckIfJournaled( &dataArea );
	if (isJournaled != 0
	    && scanCount == 0
	    && checkLevel != kForceCheck
	    && !(checkLevel == kPartialCheck && repairLevel == kForceRepairs)) {
		if (!guiControl) {
	    		printf("fsck_hfs: Volume is journaled.  No checking performed.\n");
	    		printf("fsck_hfs: Use the -f option to force checking.\n");
		}
		scavError = 0;
		goto termScav;
	}
	dataArea.calculatedVCB->vcbDriveNumber = fsReadRef;
	dataArea.calculatedVCB->vcbDriverWriteRef = fsWriteRef;

	//
	//	Now verify the volume
	//
	if ( scavError == noErr )
		ScavCtrl( &dataArea, scavVerify, &scavError );
        	
	if (scavError == noErr && logLevel >= kDebugLog)
		printVerifyStatus(&dataArea);

	// Looped for maximum times for verify and repair.  This was the last verify and
	// we bail out if problems were found
	if (scanCount >= kMaxReScan && (dataArea.RepLevel != repairLevelNoProblemsFound)) {
		PrintStatus(&dataArea, M_ReRepairFailed, 1, dataArea.volumeName);
		scavError = R_RFail;
		goto termScav;
	}

	// mark the volume clean if there are no errors and we have write access
	// and either the volume is not marked as clean or if the volume is journaled.
	// 
	if ( scavError == noErr && fsWriteRef != -1 &&
		 (dataArea.cleanUnmount == false || isJournaled != 0) ) 
		CheckForClean(&dataArea, true);		/* mark volume clean */

	if ( dataArea.RepLevel == repairLevelUnrepairable ) 
		err = cdUnrepairableErr;

	if ( !autoRepair &&
	     (dataArea.RepLevel == repairLevelVolumeRecoverable ||
	      dataArea.RepLevel == repairLevelCatalogBtreeRebuild  ||
	      dataArea.RepLevel == repairLevelUnrepairable) ) {
		PrintStatus(&dataArea, M_NeedsRepair, 1, dataArea.volumeName);
		scavError = R_VFail;
		goto termScav;
	}

	if ( scavError == noErr && dataArea.RepLevel == repairLevelNoProblemsFound ) {
		if (scanCount == 0) {
			PrintStatus(&dataArea, M_AllOK, 1, dataArea.volumeName);
		} else {
			PrintStatus(&dataArea, M_RepairOK, 1, dataArea.volumeName);
		}
	}

	//
	//	Repair the volume if it needs repairs, its repairable and we were able to unmount it
	//
	if ( dataArea.RepLevel == repairLevelNoProblemsFound && repairLevel == kForceRepairs )
	{
		dataArea.CBTStat |= S_RebuildBTree;
		dataArea.RepLevel = repairLevelCatalogBtreeRebuild;
	}
		
	if ( ((scavError == noErr) || (scavError == errRebuildBtree))  &&
		 (autoRepair == true)	&&
		 (dataArea.RepLevel != repairLevelUnrepairable)	&&
		 (dataArea.RepLevel != repairLevelNoProblemsFound) )
	{	
		// we cannot repair a volume when others have write access to the block device
		// for the volume 

		if ( dataArea.canWrite == 0 ) {
			scavError = R_WrErr;
			PrintStatus( &dataArea, M_OtherWriters, 0 );
		}
		else
			ScavCtrl( &dataArea, scavRepair, &scavError );
		
		if ( scavError == noErr )
		{
			*modified = 1;	/* Report back that we made repairs */

			/* we just repaired a volume, so scan it again to check if it corrected everything properly */
			ScavCtrl( &dataArea, scavTerminate, &temp );
			repairLevel = kMajorRepairs;
			checkLevel = kAlwaysCheck;
			PrintStatus(&dataArea, M_Rescan, 0);
			scanCount++;
			goto DoAgain;
		}
		else 
			PrintStatus(&dataArea, M_RepairFailed, 1, dataArea.volumeName);
	}
	else if ( scavError != noErr ) {
		PrintStatus(&dataArea, M_CheckFailed, 0);
		if ( logLevel >= kDebugLog )
			printf("volume check failed with error %d \n", scavError);
	}

	//	Set up structures for post processing
	if ( (autoRepair == true) && (dataArea.fileIdentifierTable != nil) )
	{
	//	*repairInfo = *repairInfo | kVolumeHadOverlappingExtents;	//	Report back that volume has overlapping extents
		fileIdentifierTable	= (FileIdentifierTable *) AllocateMemory( GetHandleSize( (Handle) dataArea.fileIdentifierTable ) );
		CopyMemory( *(dataArea.fileIdentifierTable), fileIdentifierTable, GetHandleSize( (Handle) dataArea.fileIdentifierTable ) );
	}


	//
	//	Post processing
	//
	if ( fileIdentifierTable != nil )
	{
		DisposeMemory( fileIdentifierTable );
	}

termScav:
	if (err == noErr) {
		err = scavError;
	}
		
	//
	//	Terminate the scavenger
	//

	if ( logLevel >= kDebugLog && 
		 (err != noErr || dataArea.RepLevel != repairLevelNoProblemsFound) )
		PrintVolumeObject();

	ScavCtrl( &dataArea, scavTerminate, &temp );		//	Note: use a temp var so that real scav error can be returned
		
	return( err );
}


/*------------------------------------------------------------------------------

Function:	ScavCtrl - (Scavenger Control)

Function:	Controls the scavenging process.  Interfaces with the User Interface
			Layer (written in PASCAL).

Input:		ScavOp		-	scavenging operation to be performed:

								scavInitialize	= start initial volume check
								scavVerify	= start verify
								scavRepair	= start repair
								scavTerminate		= finished scavenge 

			GPtr		-	pointer to scavenger global area			


Output:		ScavRes		-	scavenge result code (R_xxx, or 0 if no error)

------------------------------------------------------------------------------*/

void ScavCtrl( SGlobPtr GPtr, UInt32 ScavOp, short *ScavRes )
{
	OSErr			result;
	short			stat;
#if SHOW_ELAPSED_TIMES
	struct timeval 	myStartTime;
	struct timeval 	myEndTime;
	struct timeval 	myElapsedTime;
	struct timezone zone;
#endif

	//
	//	initialize some stuff
	//
	result			= noErr;						//	assume good status
	*ScavRes		= 0;	
	GPtr->ScavRes	= 0;
	
	//		
	//	dispatch next scavenge operation
	//
	switch ( ScavOp )
	{
		case scavInitialize:								//	INITIAL VOLUME CHECK
		{
			int clean;

			if ( ( result = ScavSetUp( GPtr ) ) )			//	set up BEFORE CheckForStop
				break;
			if ( IsBlueBoxSharedDrive( GPtr->DrvPtr ) )
				break;
			if ( ( result = CheckForStop( GPtr ) ) )		//	in order to initialize wrCnt
				break;
		
			/* Call for all chkLevel options and check return value only 
			 * for kDirtyCheck for preen option and kNeverCheck for quick option
			 */
			clean = CheckForClean(GPtr, false);
			if ((GPtr->chkLevel == kDirtyCheck) || (GPtr->chkLevel == kNeverCheck)) {
				if (clean == 1) {
					/* volume was unmounted cleanly */
					GPtr->cleanUnmount = true;
					break;
				}

				if (GPtr->chkLevel == kNeverCheck) {
					if (clean == -1)
						result = R_BadSig;
					else if (clean == 0) {
						/*
					 	 * We lie for journaled file systems since
					     * they get cleaned up in mount by replaying
					 	 * the journal.
					 	 * Note: CheckIfJournaled will return negative
					     * if it finds lastMountedVersion = FSK!.
					     */
						if (CheckIfJournaled(GPtr))
							GPtr->cleanUnmount = true;
						else
							result = R_Dirty;
					}
					break;
				}
			}
			
			if (CheckIfJournaled(GPtr)
			    && GPtr->chkLevel != kForceCheck
			    && !(GPtr->chkLevel == kPartialCheck && GPtr->repairLevel == kForceRepairs)
				&& !(GPtr->chkLevel == kAlwaysCheck && GPtr->repairLevel == kMajorRepairs)) {
			    break;
			}

			result = IVChk( GPtr );
			
			break;
		}
	
		case scavVerify:								//	VERIFY
		{

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myStartTime, &zone );
#endif

			if ( BitMapCheckBegin(GPtr) != 0)
				break;

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myEndTime, &zone );
			timersub( &myEndTime, &myStartTime, &myElapsedTime );
			printf( "\n%s - BitMapCheck elapsed time \n", __FUNCTION__ );
			printf( "########## secs %d msecs %d \n\n", 
				myElapsedTime.tv_sec, myElapsedTime.tv_usec );
#endif

			if ( IsBlueBoxSharedDrive( GPtr->DrvPtr ) )
				break;
			if ( ( result = CheckForStop( GPtr ) ) )
				break;

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myStartTime, &zone );
#endif

			if ( ( result = CreateExtentsBTreeControlBlock( GPtr ) ) )	//	Create the calculated BTree structures
				break;
			if ( ( result = CreateCatalogBTreeControlBlock( GPtr ) ) )
				break;
			if ( ( result = CreateAttributesBTreeControlBlock( GPtr ) ) )
				break;
			if ( ( result = CreateExtendedAllocationsFCB( GPtr ) ) )
				break;

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myEndTime, &zone );
			timersub( &myEndTime, &myStartTime, &myElapsedTime );
			printf( "\n%s - create control blocks elapsed time \n", __FUNCTION__ );
			printf( ">>>>>>>>>>>>> secs %d msecs %d \n\n", 
				myElapsedTime.tv_sec, myElapsedTime.tv_usec );
#endif

			//	Now that preflight of the BTree structures is calculated, compute the CheckDisk items
			CalculateItemCount( GPtr, &GPtr->itemsToProcess, &GPtr->onePercent );
			GPtr->itemsProcessed += GPtr->onePercent;	// We do this 4 times as set up in CalculateItemCount() to smooth the scroll
			
			if ( ( result = VLockedChk( GPtr ) ) )
				break;

			GPtr->itemsProcessed += GPtr->onePercent;	// We do this 4 times as set up in CalculateItemCount() to smooth the scroll
			WriteMsg( GPtr, M_ExtBTChk, kStatusMessage );

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myStartTime, &zone );
#endif
				
			if ((result = ExtBTChk(GPtr)))
				break;

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myEndTime, &zone );
			timersub( &myEndTime, &myStartTime, &myElapsedTime );
			printf( "\n%s - ExtBTChk elapsed time \n", __FUNCTION__ );
			printf( ">>>>>>>>>>>>> secs %d msecs %d \n\n", 
				myElapsedTime.tv_sec, myElapsedTime.tv_usec );
#endif
				
			if ((result = CheckForStop(GPtr)))
				break;
			
			GPtr->itemsProcessed += GPtr->onePercent;	// We do this 4 times as set up in CalculateItemCount() to smooth the scroll

			if ((result = ExtFlChk(GPtr)))
				break;
			if ((result = CheckForStop(GPtr)))
				break;
			
			GPtr->itemsProcessed += GPtr->onePercent;	// We do this 4 times as set up in CalculateItemCount() to smooth the scroll
			GPtr->itemsProcessed += GPtr->onePercent;
			WriteMsg( GPtr, M_CatBTChk, kStatusMessage );

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myStartTime, &zone );
#endif
				
			if ( GPtr->chkLevel == kPartialCheck )
			{
				/* skip the rest of the verify code path the first time */
				/* through when we are rebuilding the catalog B-Tree file. */
				/* we will be back here after the rebuild.  */
				GPtr->CBTStat |= S_RebuildBTree;
				result = errRebuildBtree;
				break;
			}
			
			if ((result = CheckCatalogBTree(GPtr)))
				break;

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myEndTime, &zone );
			timersub( &myEndTime, &myStartTime, &myElapsedTime );
			printf( "\n%s - CheckCatalogBTree elapsed time \n", __FUNCTION__ );
			printf( ">>>>>>>>>>>>> secs %d msecs %d \n\n", 
				myElapsedTime.tv_sec, myElapsedTime.tv_usec );
#endif

			if ((result = CheckForStop(GPtr)))
				break;

			WriteMsg( GPtr, M_CatHChk, kStatusMessage );

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myStartTime, &zone );
#endif
				
			if ((result = CatHChk(GPtr)))
				break;

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myEndTime, &zone );
			timersub( &myEndTime, &myStartTime, &myElapsedTime );
			printf( "\n%s - CatHChk elapsed time \n", __FUNCTION__ );
			printf( ">>>>>>>>>>>>> secs %d msecs %d \n\n", 
				myElapsedTime.tv_sec, myElapsedTime.tv_usec );
#endif

			if ((result = CheckForStop(GPtr)))
				break;
			if ((result = AttrBTChk(GPtr)))
				break;
			if ((result = CheckForStop(GPtr)))
				break;

			WriteMsg( GPtr, M_VolumeBitMapChk, kStatusMessage );

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myStartTime, &zone );
#endif
				
			if ((result = CheckVolumeBitMap(GPtr, false)))
				break;

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myEndTime, &zone );
			timersub( &myEndTime, &myStartTime, &myElapsedTime );
			printf( "\n%s - CheckVolumeBitMap elapsed time \n", __FUNCTION__ );
			printf( ">>>>>>>>>>>>> secs %d msecs %d \n\n", 
				myElapsedTime.tv_sec, myElapsedTime.tv_usec );
#endif 

			if ((result = CheckForStop(GPtr)))
				break;

			WriteMsg( GPtr, M_VInfoChk, kStatusMessage );

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myStartTime, &zone );
#endif
				
			if ((result = VInfoChk(GPtr)))
				break;

#if SHOW_ELAPSED_TIMES
			gettimeofday( &myEndTime, &zone );
			timersub( &myEndTime, &myStartTime, &myElapsedTime );
			printf( "\n%s - VInfoChk elapsed time \n", __FUNCTION__ );
			printf( ">>>>>>>>>>>>> secs %d msecs %d \n\n", 
				myElapsedTime.tv_sec, myElapsedTime.tv_usec );
#endif

			stat =	GPtr->VIStat  | GPtr->ABTStat | GPtr->EBTStat | GPtr->CBTStat | 
					GPtr->CatStat | GPtr->JStat;
			
			if ( stat != 0 )
			{
				if ( (GPtr->RepLevel == repairLevelNoProblemsFound) || (GPtr->RepLevel == repairLevelVolumeRecoverable) )
				{
					//	2200106, We isolate very minor errors so that if the volume cannot be unmounted
					//	CheckDisk will just return noErr
					short minorErrors = (GPtr->CatStat & ~S_LockedDirName)  |
								GPtr->VIStat | GPtr->ABTStat | GPtr->EBTStat | GPtr->CBTStat | GPtr->JStat;
					if ( minorErrors == 0 )
						GPtr->RepLevel =  repairLevelVeryMinorErrors;
					else
						GPtr->RepLevel =  repairLevelVolumeRecoverable;
				}
			}
			else if ( GPtr->RepLevel == repairLevelNoProblemsFound )
			{
			}

			GPtr->itemsProcessed = GPtr->itemsToProcess;
			result = CheckForStop(GPtr);				//	one last check for modified volume
			break;
		}					
		
		case scavRepair:									//	REPAIR
		{
			if ( IsBlueBoxSharedDrive( GPtr->DrvPtr ) )
				break;
			if ( ( result = CheckForStop(GPtr) ) )
				break;
			if ( GPtr->CBTStat & S_RebuildBTree )
				WriteMsg( GPtr, M_RebuildingCatalogBTree, kTitleMessage );
			else
				WriteMsg( GPtr, M_Repair, kTitleMessage );
			result = RepairVolume( GPtr );
			break;
		}
		
		case scavTerminate:									//	CLEANUP AFTER SCAVENGE
		{
			result = ScavTerm(GPtr);
			break;
		}
	}													//	end ScavOp switch


	//
	//	Map internal error codes to scavenger result codes
	//
	
	if ( (result < 0) || (result > Max_RCode) )
	{
		switch ( ScavOp )
		{	
			case scavInitialize:
			case scavVerify:
				if ( result == ioErr )
					result = R_RdErr;
				else if ( result == errRebuildBtree )
				{
					GPtr->RepLevel = repairLevelCatalogBtreeRebuild;
					break;
				}
				else
					result = R_VFail;
				GPtr->RepLevel = repairLevelUnrepairable;
				break;
			case scavRepair:
				result = R_RFail;
				break;
			default:
				result = R_IntErr;
		}
	}
	
	GPtr->ScavRes = result;

	*ScavRes = result;
		
}	//	end of ScavCtrl



/*------------------------------------------------------------------------------

Function: 	CheckForStop

Function:	Checks for the user hitting the "STOP" button during a scavenge,
			which interrupts the operation.  Additionally, we monitor the write
			count of a mounted volume, to be sure that the volume is not
			modified by another app while we scavenge.

Input:		GPtr		-  	pointer to scavenger global area

Output:		Function result:
						0 - ok to continue
						R_UInt - STOP button hit
						R_Modified - another app has touched the volume
-------------------------------------------------------------------------------*/

short CheckForStop( SGlob *GPtr )
{
	OSErr	err			= noErr;						//	Initialize err to noErr
	long	ticks		= TickCount();
	UInt16	dfaStage	= (UInt16) GetDFAStage();

        //printf("%d, %d", dfaStage, kAboutToRepairStage);

	//if ( ((ticks - 10) > GPtr->lastTickCount) || (dfaStage == kAboutToRepairStage) )			//	To reduce cursor flicker on fast machines, call through on a timed interval
	//{
		if ( GPtr->userCancelProc != nil )
		{
			UInt64	progress = 0;
			Boolean	progressChanged;
		//	UInt16	elapsedTicks;
			
			if ( dfaStage != kRepairStage )
			{
				progress = GPtr->itemsProcessed * 100;
				progress /= GPtr->itemsToProcess;
				progressChanged = ( progress != GPtr->lastProgress );
				GPtr->lastProgress = progress;
				
				#if( DisplayTimeRemaining )					
					if ( (progressChanged) && (progress > 5) )
					{
						elapsedTicks	=	TickCount() - GPtr->startTicks;
						GPtr->secondsRemaining	= ( ( ( 100 * elapsedTicks ) / progress ) - elapsedTicks ) / 60;
					}
				#endif
				err = CallUserCancelProc( GPtr->userCancelProc, (UInt16)progress, (UInt16)GPtr->secondsRemaining, progressChanged, dfaStage, GPtr->userContext );
			}
			else
			{
				(void) CallUserCancelProc( GPtr->userCancelProc, (UInt16)progress, 0, false, dfaStage, GPtr->userContext );
			}
			
		}
		
		if ( err != noErr )
			err = R_UInt;
	#if 0	
		if ( GPtr->realVCB )							// If the volume is mounted
			if ( GPtr->realVCB->vcbWrCnt != GPtr->wrCnt )
				err = R_Modified;					// Its been modified behind our back
	#endif
		GPtr->lastTickCount	= ticks;
	//}
			
	return ( err );
}
		


/*------------------------------------------------------------------------------

Function:	ScavSetUp - (Scavenger Set Up)

Function:	Sets up scavenger globals for a new scavenge operation.  Memory is 
			allocated for the Scavenger's static data structures (VCB, FCBs,
			BTCBs, and TPTs). The contents of the data structures are
			initialized to zero.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		ScavSetUp	-	function result:			
								0	= no error
								n 	= error code
------------------------------------------------------------------------------*/

struct ScavStaticStructures {
	SVCB				vcb;
	SFCB				fcbList[6];		
	BTreeControlBlock	btcb[4];			// 4 btcb's
	SDPT				dirPath;			// scavenger directory path table
	SBTPT				btreePath;			// scavenger BTree path table
};
typedef struct ScavStaticStructures ScavStaticStructures;


static int ScavSetUp( SGlob *GPtr)
{
	OSErr  err;
	SVCB * vcb;
#if !BSD
	DrvQEl	*drvP;
	short	ioRefNum;
#endif

	GPtr->MinorRepairsP = nil;
	
	GPtr->itemsProcessed = 0;
	GPtr->lastProgress = 0;
	GPtr->startTicks = TickCount();
	
	//
	//	allocate the static data structures (VCB, FCB's, BTCB'S, DPT and BTPT)
	//
 	{
		ScavStaticStructures	*pointer;
		
		pointer = (ScavStaticStructures *) AllocateClearMemory( sizeof(ScavStaticStructures) );
		if ( pointer == nil ) {
			if ( GPtr->logLevel >= kDebugLog ) {
				printf( "\t error %d - could not allocate %i bytes of memory \n",
					R_NoMem, sizeof(ScavStaticStructures) );
			}
			return( R_NoMem );
		}
	
		GPtr->calculatedVCB = vcb	= &pointer->vcb;
		vcb->vcbGPtr = GPtr;

		GPtr->FCBAPtr				= (Ptr) &pointer->fcbList;
		GPtr->calculatedExtentsFCB		= &pointer->fcbList[0];
		GPtr->calculatedCatalogFCB		= &pointer->fcbList[1];
		GPtr->calculatedAllocationsFCB	= &pointer->fcbList[2];
		GPtr->calculatedAttributesFCB	= &pointer->fcbList[3];
		GPtr->calculatedStartupFCB		= &pointer->fcbList[4];
		GPtr->calculatedRepairFCB		= &pointer->fcbList[5];
		
		GPtr->calculatedExtentsBTCB		= &pointer->btcb[0];
		GPtr->calculatedCatalogBTCB		= &pointer->btcb[1];
		GPtr->calculatedRepairBTCB		= &pointer->btcb[2];
		GPtr->calculatedAttributesBTCB	= &pointer->btcb[3];
		
		GPtr->BTPTPtr					= (SBTPT*) &pointer->btreePath;
		GPtr->DirPTPtr					= (SDPT*) &pointer->dirPath;
	}
	
	
	SetDFAStage( kVerifyStage );
	SetFCBSPtr( GPtr->FCBAPtr );

	//
	//	locate the driveQ element for drive being scavenged
	//
 	GPtr->DrvPtr	= 0;							//	<8> initialize so we can know if drive disappears

	//
	//	Set up Real structures
	//
#if !BSD 
	err = FindDrive( &ioRefNum, &(GPtr->DrvPtr), GPtr->DrvNum );
#endif	
	if ( IsBlueBoxSharedDrive( GPtr->DrvPtr ) )
		return noErr;
	
	err = GetVolumeFeatures( GPtr );				//	Sets up GPtr->volumeFeatures and GPtr->realVCB

#if !BSD
	if ( GPtr->DrvPtr == NULL )						//	<8> drive is no longer there!
		return ( R_NoVol );
	else
		drvP = GPtr->DrvPtr;

	//	Save current value of vcbWrCnt, to detect modifications to volume by other apps etc
	if ( GPtr->volumeFeatures & volumeIsMountedMask )
	{
		FlushVol( nil, GPtr->realVCB->vcbVRefNum );	//	Ask HFS to update all changes to disk
		GPtr->wrCnt = GPtr->realVCB->vcbWrCnt;		//	Remember write count after writing changes
	}
#endif

	//	Finish initializing the VCB

	//	The calculated structures
#if BSD
	InitBlockCache(vcb);
	vcb->vcbDriveNumber = GPtr->DrvNum;
	vcb->vcbDriverReadRef = GPtr->DrvNum;
	vcb->vcbDriverWriteRef = -1;	/* XXX need to get real fd here */
#else
	vcb->vcbDriveNumber = drvP->dQDrive;
	vcb->vcbDriverReadRef = drvP->dQRefNum;
	vcb->vcbDriverWriteRef = drvP->dQRefNum;
	vcb->vcbFSID = drvP->dQFSID;
#endif
//	vcb->vcbVRefNum = Vol_RefN;

	//
	//	finish initializing the FCB's
	//
	{
		SFCB *fcb;
		
		// Create Calculated Extents FCB
		fcb			= GPtr->calculatedExtentsFCB;
		fcb->fcbFileID		= kHFSExtentsFileID;
		fcb->fcbVolume		= vcb;
		fcb->fcbBtree		= GPtr->calculatedExtentsBTCB;
		vcb->vcbExtentsFile	= fcb;
			
		// Create Calculated Catalog FCB
		fcb			= GPtr->calculatedCatalogFCB;
		fcb->fcbFileID 		= kHFSCatalogFileID;
		fcb->fcbVolume		= vcb;
		fcb->fcbBtree		= GPtr->calculatedCatalogBTCB;
		vcb->vcbCatalogFile	= fcb;
	
		// Create Calculated Allocations FCB
		fcb			= GPtr->calculatedAllocationsFCB;
		fcb->fcbFileID		= kHFSAllocationFileID;
		fcb->fcbVolume 		= vcb;
		fcb->fcbBtree		= NULL;		//	no BitMap B-Tree
		vcb->vcbAllocationFile	= fcb;
	
		// Create Calculated Attributes FCB
		fcb			= GPtr->calculatedAttributesFCB;
		fcb->fcbFileID		= kHFSAttributesFileID;
		fcb->fcbVolume 		= vcb;
		fcb->fcbBtree		= GPtr->calculatedAttributesBTCB;
		vcb->vcbAttributesFile	= fcb;

		/* Create Calculated Startup FCB */
		fcb			= GPtr->calculatedStartupFCB;
		fcb->fcbFileID		= kHFSStartupFileID;
		fcb->fcbVolume 		= vcb;
		fcb->fcbBtree		= NULL;
		vcb->vcbStartupFile	= fcb;
	}
	
	//	finish initializing the BTCB's
	{
		BTreeControlBlock	*btcb;
		
		btcb			= GPtr->calculatedExtentsBTCB;		// calculatedExtentsBTCB
		btcb->fcbPtr		= GPtr->calculatedExtentsFCB;
		btcb->getBlockProc	= GetFileBlock;
		btcb->releaseBlockProc	= ReleaseFileBlock;
		btcb->setEndOfForkProc	= SetEndOfForkProc;
		
		btcb			= GPtr->calculatedCatalogBTCB;		// calculatedCatalogBTCB
		btcb->fcbPtr		= GPtr->calculatedCatalogFCB;
		btcb->getBlockProc	= GetFileBlock;
		btcb->releaseBlockProc	= ReleaseFileBlock;
		btcb->setEndOfForkProc	= SetEndOfForkProc;
	
		btcb			= GPtr->calculatedAttributesBTCB;	// calculatedAttributesBTCB
		btcb->fcbPtr		= GPtr->calculatedAttributesFCB;
		btcb->getBlockProc	= GetFileBlock;
		btcb->releaseBlockProc	= ReleaseFileBlock;
		btcb->setEndOfForkProc	= SetEndOfForkProc;
	}

	
	//
	//	Initialize some global stuff
	//

	GPtr->RepLevel			= repairLevelNoProblemsFound;
	GPtr->ErrCode			= 0;
	GPtr->IntErr			= noErr;
	GPtr->VIStat			= 0;
	GPtr->ABTStat			= 0;
	GPtr->EBTStat			= 0;
	GPtr->CBTStat			= 0;
	GPtr->CatStat			= 0;
	GPtr->VeryMinorErrorsStat	= 0;
	GPtr->JStat			= 0;

	/* Assume that the volume is dirty unmounted */
	GPtr->cleanUnmount		= false;

 	// 
 	// Initialize VolumeObject
 	//
 	
 	InitializeVolumeObject( GPtr );

	/* Check if the volume type of initialized object is valid.  If not, return error */
	if (VolumeObjectIsValid() == false) {
		return (R_BadSig);
	}
 	
	// Keep a valid file id list for HFS volumes
	GPtr->validFilesList = (UInt32**)NewHandle( 0 );
	if ( GPtr->validFilesList == nil ) {
		if ( GPtr->logLevel >= kDebugLog ) {
			printf( "\t error %d - could not allocate file ID list \n", R_NoMem );
		}
		return( R_NoMem );
	}

	// Convert the security attribute name from utf8 to utf16.  This will
	// avoid repeated conversion of all extended attributes to compare with
	// security attribute name
	(void) utf_decodestr((unsigned char *)KAUTH_FILESEC_XATTR, strlen(KAUTH_FILESEC_XATTR), GPtr->securityAttrName, &GPtr->securityAttrLen);

	return( noErr );

} /* end of ScavSetUp */

		


/*------------------------------------------------------------------------------

Function:	ScavTerm - (Scavenge Termination))

Function:	Terminates the current scavenging operation.  Memory for the
			VCB, FCBs, BTCBs, volume bit map, and BTree bit maps is
			released.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		ScavTerm	-	function result:			
								0	= no error
								n 	= error code
------------------------------------------------------------------------------*/

static int ScavTerm( SGlobPtr GPtr )
{
	SFCB			*fcbP;
	BTreeControlBlock	*btcbP;
	RepairOrderPtr		rP;
	OSErr			err;

	(void) BitMapCheckEnd();

	while( (rP = GPtr->MinorRepairsP) != nil )		//	loop freeing leftover (undone) repair orders
	{
		GPtr->MinorRepairsP = rP->link;				//	(in case repairs were not made)
		DisposeMemory(rP);
		err = MemError();
	}
	
	if( GPtr->validFilesList != nil )
		DisposeHandle( (Handle) GPtr->validFilesList );
	
	if( GPtr->overlappedExtents != nil )
		DisposeHandle( (Handle) GPtr->overlappedExtents );
	
	if( GPtr->fileIdentifierTable != nil )
		DisposeHandle( (Handle) GPtr->fileIdentifierTable );
	
	if( GPtr->calculatedVCB == nil )								//	already freed?
		return( noErr );

	//	If the FCB's and BTCB's have been set up, dispose of them
	fcbP = GPtr->calculatedExtentsFCB;	// release extent file BTree bit map
	if ( fcbP != nil )
	{
		btcbP = (BTreeControlBlock*)fcbP->fcbBtree;
		if ( btcbP != nil)
		{
			if( btcbP->refCon != (UInt32)nil )
			{
				if(((BTreeExtensionsRec*)btcbP->refCon)->BTCBMPtr != nil)
				{
					DisposeMemory(((BTreeExtensionsRec*)btcbP->refCon)->BTCBMPtr);
					err = MemError();
				}
				DisposeMemory( (Ptr)btcbP->refCon );
				err = MemError();
				btcbP->refCon = (UInt32)nil;
			}
				
			fcbP = GPtr->calculatedCatalogFCB;	//	release catalog BTree bit map
			btcbP = (BTreeControlBlock*)fcbP->fcbBtree;
				
			if( btcbP->refCon != (UInt32)nil )
			{
				if(((BTreeExtensionsRec*)btcbP->refCon)->BTCBMPtr != nil)
				{
					DisposeMemory(((BTreeExtensionsRec*)btcbP->refCon)->BTCBMPtr);
					err = MemError();
				}
				DisposeMemory( (Ptr)btcbP->refCon );
				err = MemError();
				btcbP->refCon = (UInt32)nil;
			}
		}
	}

	DisposeMemory( GPtr->calculatedVCB );						//	Release our block of data structures	
	err = MemError();

	GPtr->calculatedVCB = nil;

	return( noErr );
}

#define BLUE_BOX_SHARED_DRVR_NAME "\p.BlueBoxShared"
#define BLUE_BOX_FLOPPY_WHERE_STRING "\pdisk%d (Shared)"
#define SONY_DRVR_NAME "\p.Sony"

/*------------------------------------------------------------------------------

Routine:	IsBlueBoxSharedDrive

Function: 	Given a DQE address, return a boolean that determines whether
			or not a drive is a Blue Box disk being accessed via Shared mode.
			Such drives do not support i/o and cannot be scavenged.
			
Input:		Arg 1	- DQE pointer

Output:		D0.L -	0 if drive not to be used
					1 otherwise			
------------------------------------------------------------------------------*/

struct IconAndStringRec {
	char		icon[ 256 ];
	Str255		string;
};
typedef struct IconAndStringRec IconAndStringRec, * IconAndStringRecPtr;


Boolean IsBlueBoxSharedDrive ( DrvQElPtr dqPtr )
{
#if 0
	Str255			blueBoxSharedDriverName		= BLUE_BOX_SHARED_DRVR_NAME;
	Str255			blueBoxFloppyWhereString	= BLUE_BOX_FLOPPY_WHERE_STRING;
	Str255			sonyDriverName				= SONY_DRVR_NAME;
	DCtlHandle		driverDCtlHandle;
	DCtlPtr			driverDCtlPtr;
	DRVRHeaderPtr	drvrHeaderPtr;
	StringPtr		driverName;
	
	if ( dqPtr == NULL )
		return false;

	// Now look at the name of the Driver name. If it is .BlueBoxShared keep it out of the list of available disks.
	driverDCtlHandle = GetDCtlEntry(dqPtr->dQRefNum);
	driverDCtlPtr = *driverDCtlHandle;
	if((((driverDCtlPtr->dCtlFlags) & Is_Native_Mask) == 0) && (driverDCtlPtr->dCtlDriver != nil))
	{
		if (((driverDCtlPtr->dCtlFlags) & Is_Ram_Based_Mask) == 0)
		{
			drvrHeaderPtr = (DRVRHeaderPtr)driverDCtlPtr->dCtlDriver;
		}	
		else
		{
			//еее bek - lock w/o unlock/restore?  should be getstate/setstate?
			HLock((Handle)(driverDCtlPtr)->dCtlDriver);
			drvrHeaderPtr = (DRVRHeaderPtr)*((Handle)(driverDCtlPtr->dCtlDriver));
			
		}
		driverName = (StringPtr)&(drvrHeaderPtr->drvrName);
		if (!(IdenticalString(driverName,blueBoxSharedDriverName,nil)))
		{
			return( true );
		}

		// Special case for the ".Sony" floppy driver which might be accessed in Shared mode inside the Blue Box
		// Test its "where" string instead of the driver name.
		if (!(IdenticalString(driverName,sonyDriverName,nil)))
		{
			CntrlParam			paramBlock;
		
			paramBlock.ioCompletion	= nil;
			paramBlock.ioNamePtr	= nil;
			paramBlock.ioVRefNum	= dqPtr->dQDrive;
			paramBlock.ioCRefNum	= dqPtr->dQRefNum;
			paramBlock.csCode		= kDriveIcon;						// return physical icon
			
			// If PBControl(kDriveIcon) returns an error then the driver is not the Blue Box driver.
			if ( noErr == PBControlSync( (ParmBlkPtr) &paramBlock ) )
			{
				IconAndStringRecPtr		iconAndStringRecPtr;
				StringPtr				whereStringPtr;
				
				iconAndStringRecPtr = * (IconAndStringRecPtr*) & paramBlock.csParam;
				whereStringPtr = (StringPtr) & iconAndStringRecPtr->string;
				if (!(IdenticalString(whereStringPtr,blueBoxFloppyWhereString,nil)))
				{
					return( true );
				}
			}
		}
	}
#endif
	
	return false;
}

		


/*------------------------------------------------------------------------------

Function:	printVerifyStatus - (Print Verify Status)

Function:	Prints out the Verify Status words.
			
Input:		GPtr	-	pointer to scavenger global area

Output:		None.
------------------------------------------------------------------------------*/
static 
void printVerifyStatus(SGlobPtr GPtr)
{
    UInt16 stat;
                    
    stat = GPtr->VIStat | GPtr->ABTStat | GPtr->EBTStat | GPtr->CBTStat | GPtr->CatStat;
                    
    if ( stat != 0 ) {
        printf("   Verify Status: VIStat = 0x%04x, ABTStat = 0x%04x EBTStat = 0x%04x\n", 
                GPtr->VIStat, GPtr->ABTStat, GPtr->EBTStat);     
        printf("                  CBTStat = 0x%04x CatStat = 0x%04x\n", 
                GPtr->CBTStat, GPtr->CatStat);
    }
}
