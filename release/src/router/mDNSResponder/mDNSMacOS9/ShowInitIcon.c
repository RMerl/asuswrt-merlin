// ShowInitIcon - version 1.0.1, May 30th, 1995
// This code is intended to let INIT writers easily display an icon at startup time.
// View in Geneva 9pt, 4-space tabs

// Written by: Peter N Lewis <peter@mail.peter.com.au>, Jim Walker <JWWalker@aol.com>
// and François Pottier <pottier@dmi.ens.fr>, with thanks to previous ShowINIT authors.
// Send comments and bug reports to François Pottier.

// This version features:
// - Short and readable code.
// - Correctly wraps around when more than one row of icons has been displayed.
// - works with System 6
// - Built with Universal Headers & CodeWarrior. Should work with other headers/compilers.

#include <Memory.h>
#include <Resources.h>
#include <Icons.h>
#include <OSUtils.h>
#include "ShowInitIcon.h"

// You should set SystemSixOrLater in your headers to avoid including glue for SysEnvirons.

// ---------------------------------------------------------------------------------------------------------------------
// Set this flag to 1 if you want to compile this file into a stand-alone resource (see note below).
// Set it to 0 if you want to include this source file into your INIT project.

#if 0
#define ShowInitIcon main
#endif

// ---------------------------------------------------------------------------------------------------------------------
// The ShowINIT mechanism works by having each INIT read/write data from these globals.
// The MPW C compiler doesn't accept variables declared at an absolute address, so I use these macros instead.
// Only one macro is defined per variable; there is no need to define a Set and a Get accessor like in <LowMem.h>.

#define	LMVCheckSum		(* (unsigned short*) 0x928)
#define	LMVCoord		(* (         short*) 0x92A)
#define	LMHCoord		(* (         short*) 0x92C)
#define	LMHCheckSum		(* (unsigned short*) 0x92E)

// ---------------------------------------------------------------------------------------------------------------------
// Prototypes for the subroutines. The main routine comes first; this is necessary to make THINK C's "Custom Header" option work.

static unsigned short CheckSum (short x);
static void ComputeIconRect (Rect* iconRect, Rect* screenBounds);
static void AdvanceIconPosition (Rect* iconRect);
static void DrawBWIcon (short iconID, Rect *iconRect);

// ---------------------------------------------------------------------------------------------------------------------
// Main routine.

typedef struct {
	QDGlobals			qd;									// Storage for the QuickDraw globals
	long				qdGlobalsPtr;							// A5 points to this place; it will contain a pointer to qd
} QDStorage;

pascal void ShowInitIcon (short iconFamilyID, Boolean advance)
{
	long				oldA5;								// Original value of register A5
	QDStorage			qds;									// Fake QD globals
	CGrafPort			colorPort;
	GrafPort			bwPort;
	Rect				destRect;
	SysEnvRec		environment;							// Machine configuration.
	
	oldA5 = SetA5((long) &qds.qdGlobalsPtr);						// Tell A5 to point to the end of the fake QD Globals
	InitGraf(&qds.qd.thePort);								// Initialize the fake QD Globals
	
	SysEnvirons(curSysEnvVers, &environment);					// Find out what kind of machine this is

	ComputeIconRect(&destRect, &qds.qd.screenBits.bounds);			// Compute where the icon should be drawn

	if (environment.systemVersion >= 0x0700 && environment.hasColorQD) {
		OpenCPort(&colorPort);
		PlotIconID(&destRect, atNone, ttNone, iconFamilyID);
		CloseCPort(&colorPort);
	}
	else {
		OpenPort(&bwPort);
		DrawBWIcon(iconFamilyID, &destRect);
		ClosePort(&bwPort);
	}
	
	if (advance)
		AdvanceIconPosition (&destRect);
		
	SetA5(oldA5); 											// Restore A5 to its previous value
}

// ---------------------------------------------------------------------------------------------------------------------
// A checksum is used to make sure that the data in there was left by another ShowINIT-aware INIT.

static unsigned short CheckSum (short x)
{
	return (unsigned short)(((x << 1) | (x >> 15)) ^ 0x1021);
}

// ---------------------------------------------------------------------------------------------------------------------
// ComputeIconRect computes where the icon should be displayed.

static void ComputeIconRect (Rect* iconRect, Rect* screenBounds)
{
	if (CheckSum(LMHCoord) != LMHCheckSum)					// If we are first, we need to initialize the shared data.
		LMHCoord = 8;
	if (CheckSum(LMVCoord) != LMVCheckSum)
		LMVCoord = (short)(screenBounds->bottom - 40);
	
	if (LMHCoord + 34 > screenBounds->right) {					// Check whether we must wrap
		iconRect->left = 8;
		iconRect->top = (short)(LMVCoord - 40);
	}
	else {
		iconRect->left = LMHCoord;
		iconRect->top = LMVCoord;
	}
	iconRect->right  = (short)(iconRect->left + 32);
	iconRect->bottom = (short)(iconRect->top  + 32);
}

// AdvanceIconPosition updates the shared global variables so that the next extension will draw its icon beside ours.

static void AdvanceIconPosition (Rect* iconRect)
{
	LMHCoord = (short)(iconRect->left + 40);					// Update the shared data
	LMVCoord = iconRect->top;
	LMHCheckSum = CheckSum(LMHCoord);
	LMVCheckSum = CheckSum(LMVCoord);
}

// DrawBWIcon draws the 'ICN#' member of the icon family. It works under System 6.

static void DrawBWIcon (short iconID, Rect *iconRect)
{
	Handle		icon;
	BitMap		source, destination;
	GrafPtr		port;
	
	icon = Get1Resource('ICN#', iconID);
	if (icon != NULL) {
		HLock(icon);
														// Prepare the source and destination bitmaps.
		source.baseAddr = *icon + 128;						// Mask address.
		source.rowBytes = 4;
		SetRect(&source.bounds, 0, 0, 32, 32);
		GetPort(&port);
		destination = port->portBits;
														// Transfer the mask.
		CopyBits(&source, &destination, &source.bounds, iconRect, srcBic, nil);
														// Then the icon.
		source.baseAddr = *icon;
		CopyBits(&source, &destination, &source.bounds, iconRect, srcOr, nil);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Notes

// Checking for PlotIconID:
// We (PNL) now check for system 7 and colour QD, and use colour graf ports and PlotIconID only if both are true
// Otherwise we use B&W grafport and draw using PlotBWIcon.
