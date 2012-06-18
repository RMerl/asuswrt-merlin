/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB transaction2 handling
   Copyright (C) Jeremy Allison 1994-1998

   Extensively modified by Andrew Tridgell, 1995

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _TRANS2_H_
#define _TRANS2_H_

/* Define the structures needed for the trans2 calls. */

/*******************************************************
 For DosFindFirst/DosFindNext - level 1

MAXFILENAMELEN = 255;
FDATE == uint16
FTIME == uint16
ULONG == uint32
USHORT == uint16

typedef struct _FILEFINDBUF {
Byte offset   Type     name                description
-------------+-------+-------------------+--------------
0             FDATE    fdateCreation;
2             FTIME    ftimeCreation;
4             FDATE    fdateLastAccess;
6             FTIME    ftimeLastAccess;
8             FDATE    fdateLastWrite;
10            FTIME    ftimeLastWrite;
12            ULONG    cbFile               file length in bytes
16            ULONG    cbFileAlloc          size of file allocation unit
20            USHORT   attrFile
22            UCHAR    cchName              length of name to follow (not including zero)
23            UCHAR    achName[MAXFILENAMELEN]; Null terminated name
} FILEFINDBUF;
*********************************************************/

#define l1_fdateCreation 0
#define l1_fdateLastAccess 4
#define l1_fdateLastWrite 8
#define l1_cbFile 12
#define l1_cbFileAlloc 16
#define l1_attrFile 20
#define l1_cchName 22
#define l1_achName 23

/**********************************************************
For DosFindFirst/DosFindNext - level 2

typedef struct _FILEFINDBUF2 {
Byte offset   Type     name                description
-------------+-------+-------------------+--------------
0             FDATE    fdateCreation;
2             FTIME    ftimeCreation;
4             FDATE    fdateLastAccess;
6             FTIME    ftimeLastAccess;
8             FDATE    fdateLastWrite;
10            FTIME    ftimeLastWrite;
12            ULONG    cbFile               file length in bytes
16            ULONG    cbFileAlloc          size of file allocation unit
20            USHORT   attrFile
22            ULONG    cbList               Extended attribute list (always 0)
26            UCHAR    cchName              length of name to follow (not including zero)
27            UCHAR    achName[MAXFILENAMELEN]; Null terminated name
} FILEFINDBUF2;
*************************************************************/

#define l2_fdateCreation 0
#define l2_fdateLastAccess 4
#define l2_fdateLastWrite 8
#define l2_cbFile 12
#define l2_cbFileAlloc 16
#define l2_attrFile 20
#define l2_cbList 22
#define l2_cchName 26
#define l2_achName 27


/**********************************************************
For DosFindFirst/DosFindNext - level 260

typedef struct _FILEFINDBUF260 {
Byte offset   Type     name                description
-------------+-------+-------------------+--------------
0              ULONG  NextEntryOffset;
4              ULONG  FileIndex;
8              LARGE_INTEGER CreationTime;
16             LARGE_INTEGER LastAccessTime;
24             LARGE_INTEGER LastWriteTime;
32             LARGE_INTEGER ChangeTime;
40             LARGE_INTEGER EndOfFile;
48             LARGE_INTEGER AllocationSize;
56             ULONG FileAttributes;
60             ULONG FileNameLength;
64             ULONG EaSize;
68             CHAR ShortNameLength;
70             UNICODE ShortName[12];
94             UNICODE FileName[];
*************************************************************/

#define l260_achName 94


/**********************************************************
For DosQueryPathInfo/DosQueryFileInfo/DosSetPathInfo/
DosSetFileInfo - level 1

typedef struct _FILESTATUS {
Byte offset   Type     name                description
-------------+-------+-------------------+--------------
0             FDATE    fdateCreation;
2             FTIME    ftimeCreation;
4             FDATE    fdateLastAccess;
6             FTIME    ftimeLastAccess;
8             FDATE    fdateLastWrite;
10            FTIME    ftimeLastWrite;
12            ULONG    cbFile               file length in bytes
16            ULONG    cbFileAlloc          size of file allocation unit
20            USHORT   attrFile
} FILESTATUS;
*************************************************************/

/* Use the l1_ defines from DosFindFirst */

/**********************************************************
For DosQueryPathInfo/DosQueryFileInfo/DosSetPathInfo/
DosSetFileInfo - level 2

typedef struct _FILESTATUS2 {
Byte offset   Type     name                description
-------------+-------+-------------------+--------------
0             FDATE    fdateCreation;
2             FTIME    ftimeCreation;
4             FDATE    fdateLastAccess;
6             FTIME    ftimeLastAccess;
8             FDATE    fdateLastWrite;
10            FTIME    ftimeLastWrite;
12            ULONG    cbFile               file length in bytes
16            ULONG    cbFileAlloc          size of file allocation unit
20            USHORT   attrFile
22            ULONG    cbList               Length of EA's (0)
} FILESTATUS2;
*************************************************************/

/* Use the l2_ #defines from DosFindFirst */

/**********************************************************
For DosQFSInfo/DosSetFSInfo - level 1

typedef struct _FSALLOCATE {
Byte offset   Type     name                description
-------------+-------+-------------------+--------------
0             ULONG    idFileSystem       id of file system
4             ULONG    cSectorUnit        number of sectors per allocation unit
8             ULONG    cUnit              number of allocation units
12            ULONG    cUnitAvail         Available allocation units
16            USHORT   cbSector           bytes per sector
} FSALLOCATE;
*************************************************************/

#define l1_idFileSystem 0
#define l1_cSectorUnit 4
#define l1_cUnit 8
#define l1_cUnitAvail 12
#define l1_cbSector 16

/**********************************************************
For DosQFSInfo/DosSetFSInfo - level 2

typedef struct _FSINFO {
Byte offset   Type     name                description
-------------+-------+-------------------+--------------
0             FDATE   vol_fdateCreation
2             FTIME   vol_ftimeCreation
4             UCHAR   vol_cch             length of volume name (excluding NULL)
5             UCHAR   vol_szVolLabel[12]  volume name
} FSINFO;
*************************************************************/

#define SMB_INFO_STANDARD               1
#define SMB_INFO_QUERY_EA_SIZE          2
#define SMB_INFO_QUERY_EAS_FROM_LIST    3
#define SMB_INFO_QUERY_ALL_EAS          4
#define SMB_INFO_IS_NAME_VALID          6
#define SMB_QUERY_FS_LABEL_INFO			0x101
#define SMB_QUERY_FS_VOLUME_INFO		0x102
#define SMB_QUERY_FS_SIZE_INFO			0x103
#define SMB_QUERY_FS_DEVICE_INFO		0x104
#define SMB_QUERY_FS_ATTRIBUTE_INFO		0x105


#define l2_vol_fdateCreation 0
#define l2_vol_cch 4
#define l2_vol_szVolLabel 5


#define SMB_QUERY_FILE_BASIC_INFO	0x101
#define SMB_QUERY_FILE_STANDARD_INFO	0x102
#define SMB_QUERY_FILE_EA_INFO		0x103
#define SMB_QUERY_FILE_NAME_INFO	0x104
#define SMB_QUERY_FILE_ALLOCATION_INFO	0x105
#define SMB_QUERY_FILE_END_OF_FILEINFO	0x106
#define SMB_QUERY_FILE_ALL_INFO		0x107
#define SMB_QUERY_FILE_ALT_NAME_INFO	0x108
#define SMB_QUERY_FILE_STREAM_INFO	0x109

#define SMB_FIND_FILE_DIRECTORY_INFO		0x101
#define SMB_FIND_FILE_FULL_DIRECTORY_INFO	0x102
#define SMB_FIND_FILE_NAMES_INFO		0x103
#define SMB_FIND_FILE_BOTH_DIRECTORY_INFO	0x104

#define SMB_SET_FILE_BASIC_INFO		0x101
#define SMB_SET_FILE_DISPOSITION_INFO	0x102
#define SMB_SET_FILE_ALLOCATION_INFO	0x103
#define SMB_SET_FILE_END_OF_FILE_INFO	0x104

/*
 * Thursby MAC extensions....
 */

#define SMB_MAC_QUERY_FS_INFO           0x301

#define DIRLEN_GUESS (45+MAX(l1_achName,l2_achName))

/*
 * DeviceType and Characteristics returned in a
 * SMB_QUERY_FS_DEVICE_INFO call.
 */

#define DEVICETYPE_CD_ROM		0x2
#define DEVICETYPE_CD_ROM_FILE_SYSTEM	0x3
#define DEVICETYPE_DISK			0x7
#define DEVICETYPE_DISK_FILE_SYSTEM	0x8
#define DEVICETYPE_FILE_SYSTEM		0x9

/* Characteristics. */
#define TYPE_REMOVABLE_MEDIA		0x1
#define TYPE_READ_ONLY_DEVICE		0x2
#define TYPE_FLOPPY			0x4
#define TYPE_WORM			0x8
#define TYPE_REMOTE			0x10
#define TYPE_MOUNTED			0x20
#define TYPE_VIRTUAL			0x40

#endif



