/* 
   Unix SMB/CIFS implementation.
   SMB transaction2 handling
   Copyright (C) Jeremy Allison 1994-2002.
   Copyright (C) Andrew Tridgell 1995-2003.
   Copyright (C) James Peach 2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _TRANS2_H_
#define _TRANS2_H_

/* These are the TRANS2 sub commands */
#define TRANSACT2_OPEN                        0
#define TRANSACT2_FINDFIRST                   1
#define TRANSACT2_FINDNEXT                    2
#define TRANSACT2_QFSINFO                     3
#define TRANSACT2_SETFSINFO                   4
#define TRANSACT2_QPATHINFO                   5
#define TRANSACT2_SETPATHINFO                 6
#define TRANSACT2_QFILEINFO                   7
#define TRANSACT2_SETFILEINFO                 8
#define TRANSACT2_FSCTL                       9
#define TRANSACT2_IOCTL                     0xA
#define TRANSACT2_FINDNOTIFYFIRST           0xB
#define TRANSACT2_FINDNOTIFYNEXT            0xC
#define TRANSACT2_MKDIR                     0xD
#define TRANSACT2_SESSION_SETUP             0xE
#define TRANSACT2_GET_DFS_REFERRAL         0x10
#define TRANSACT2_REPORT_DFS_INCONSISTANCY 0x11


/* trans2 Query FS info levels */
/*
w2k3 TRANS2ALIASES:
Checking for QFSINFO aliases
        Found level    1 (0x001) of size 18 (0x12)
        Found level    2 (0x002) of size 12 (0x0c)
        Found level  258 (0x102) of size 26 (0x1a)
        Found level  259 (0x103) of size 24 (0x18)
        Found level  260 (0x104) of size  8 (0x08)
        Found level  261 (0x105) of size 20 (0x14)
        Found level 1001 (0x3e9) of size 26 (0x1a)
        Found level 1003 (0x3eb) of size 24 (0x18)
        Found level 1004 (0x3ec) of size  8 (0x08)
        Found level 1005 (0x3ed) of size 20 (0x14)
        Found level 1006 (0x3ee) of size 48 (0x30)
        Found level 1007 (0x3ef) of size 32 (0x20)
        Found level 1008 (0x3f0) of size 64 (0x40)
Found 13 levels with success status
        Level 261 (0x105) and level 1005 (0x3ed) are possible aliases
        Level 260 (0x104) and level 1004 (0x3ec) are possible aliases
        Level 259 (0x103) and level 1003 (0x3eb) are possible aliases
        Level 258 (0x102) and level 1001 (0x3e9) are possible aliases
Found 4 aliased levels
*/
#define SMB_QFS_ALLOCATION		                   1
#define SMB_QFS_VOLUME			                   2
#define SMB_QFS_VOLUME_INFO                            0x102
#define SMB_QFS_SIZE_INFO                              0x103
#define SMB_QFS_DEVICE_INFO                            0x104
#define SMB_QFS_ATTRIBUTE_INFO                         0x105
#define SMB_QFS_UNIX_INFO                              0x200
#define SMB_QFS_POSIX_INFO                             0x201
#define SMB_QFS_POSIX_WHOAMI                           0x202
#define SMB_QFS_VOLUME_INFORMATION			1001
#define SMB_QFS_SIZE_INFORMATION			1003
#define SMB_QFS_DEVICE_INFORMATION			1004
#define SMB_QFS_ATTRIBUTE_INFORMATION			1005
#define SMB_QFS_QUOTA_INFORMATION			1006
#define SMB_QFS_FULL_SIZE_INFORMATION			1007
#define SMB_QFS_OBJECTID_INFORMATION			1008


/* trans2 qfileinfo/qpathinfo */
/* w2k3 TRANS2ALIASES:
Checking for QPATHINFO aliases
setting up complex file \qpathinfo_aliases.txt
        Found level    1 (0x001) of size  22 (0x16)
        Found level    2 (0x002) of size  26 (0x1a)
        Found level    4 (0x004) of size  41 (0x29)
        Found level    6 (0x006) of size   0 (0x00)
        Found level  257 (0x101) of size  40 (0x28)
        Found level  258 (0x102) of size  24 (0x18)
        Found level  259 (0x103) of size   4 (0x04)
        Found level  260 (0x104) of size  48 (0x30)
        Found level  263 (0x107) of size 126 (0x7e)
        Found level  264 (0x108) of size  28 (0x1c)
        Found level  265 (0x109) of size  38 (0x26)
        Found level  267 (0x10b) of size  16 (0x10)
        Found level 1004 (0x3ec) of size  40 (0x28)
        Found level 1005 (0x3ed) of size  24 (0x18)
        Found level 1006 (0x3ee) of size   8 (0x08)
        Found level 1007 (0x3ef) of size   4 (0x04)
        Found level 1008 (0x3f0) of size   4 (0x04)
        Found level 1009 (0x3f1) of size  48 (0x30)
        Found level 1014 (0x3f6) of size   8 (0x08)
        Found level 1016 (0x3f8) of size   4 (0x04)
        Found level 1017 (0x3f9) of size   4 (0x04)
        Found level 1018 (0x3fa) of size 126 (0x7e)
        Found level 1021 (0x3fd) of size  28 (0x1c)
        Found level 1022 (0x3fe) of size  38 (0x26)
        Found level 1028 (0x404) of size  16 (0x10)
        Found level 1034 (0x40a) of size  56 (0x38)
        Found level 1035 (0x40b) of size   8 (0x08)
Found 27 levels with success status
        Level 267 (0x10b) and level 1028 (0x404) are possible aliases
        Level 265 (0x109) and level 1022 (0x3fe) are possible aliases
        Level 264 (0x108) and level 1021 (0x3fd) are possible aliases
        Level 263 (0x107) and level 1018 (0x3fa) are possible aliases
        Level 260 (0x104) and level 1009 (0x3f1) are possible aliases
        Level 259 (0x103) and level 1007 (0x3ef) are possible aliases
        Level 258 (0x102) and level 1005 (0x3ed) are possible aliases
        Level 257 (0x101) and level 1004 (0x3ec) are possible aliases
Found 8 aliased levels
*/
#define SMB_QFILEINFO_STANDARD                             1
#define SMB_QFILEINFO_EA_SIZE                              2
#define SMB_QFILEINFO_EA_LIST                              3
#define SMB_QFILEINFO_ALL_EAS                              4
#define SMB_QFILEINFO_IS_NAME_VALID                        6  /* only for QPATHINFO */
#define SMB_QFILEINFO_BASIC_INFO	               0x101
#define SMB_QFILEINFO_STANDARD_INFO	               0x102
#define SMB_QFILEINFO_EA_INFO		               0x103
#define SMB_QFILEINFO_NAME_INFO	                       0x104
#define SMB_QFILEINFO_ALL_INFO		               0x107
#define SMB_QFILEINFO_ALT_NAME_INFO	               0x108
#define SMB_QFILEINFO_STREAM_INFO	               0x109
#define SMB_QFILEINFO_COMPRESSION_INFO                 0x10b
#define SMB_QFILEINFO_UNIX_BASIC                       0x200
#define SMB_QFILEINFO_UNIX_LINK                        0x201
#define SMB_QFILEINFO_UNIX_INFO2                       0x20b
#define SMB_QFILEINFO_BASIC_INFORMATION			1004
#define SMB_QFILEINFO_STANDARD_INFORMATION		1005
#define SMB_QFILEINFO_INTERNAL_INFORMATION		1006
#define SMB_QFILEINFO_EA_INFORMATION			1007
#define SMB_QFILEINFO_ACCESS_INFORMATION		1008
#define SMB_QFILEINFO_NAME_INFORMATION			1009
#define SMB_QFILEINFO_POSITION_INFORMATION		1014
#define SMB_QFILEINFO_MODE_INFORMATION			1016
#define SMB_QFILEINFO_ALIGNMENT_INFORMATION		1017
#define SMB_QFILEINFO_ALL_INFORMATION			1018
#define SMB_QFILEINFO_ALT_NAME_INFORMATION	        1021
#define SMB_QFILEINFO_STREAM_INFORMATION		1022
#define SMB_QFILEINFO_COMPRESSION_INFORMATION		1028
#define SMB_QFILEINFO_NETWORK_OPEN_INFORMATION		1034
#define SMB_QFILEINFO_ATTRIBUTE_TAG_INFORMATION		1035



/* trans2 setfileinfo/setpathinfo levels */
/*
w2k3 TRANS2ALIASES
Checking for SETFILEINFO aliases
setting up complex file \setfileinfo_aliases.txt
        Found level    1 (0x001) of size   2 (0x02)
        Found level    2 (0x002) of size   2 (0x02)
        Found level  257 (0x101) of size  40 (0x28)
        Found level  258 (0x102) of size   2 (0x02)
        Found level  259 (0x103) of size   8 (0x08)
        Found level  260 (0x104) of size   8 (0x08)
        Found level 1004 (0x3ec) of size  40 (0x28)
        Found level 1010 (0x3f2) of size   2 (0x02)
        Found level 1013 (0x3f5) of size   2 (0x02)
        Found level 1014 (0x3f6) of size   8 (0x08)
        Found level 1016 (0x3f8) of size   4 (0x04)
        Found level 1019 (0x3fb) of size   8 (0x08)
        Found level 1020 (0x3fc) of size   8 (0x08)
        Found level 1023 (0x3ff) of size   8 (0x08)
        Found level 1025 (0x401) of size  16 (0x10)
        Found level 1029 (0x405) of size  72 (0x48)
        Found level 1032 (0x408) of size  56 (0x38)
        Found level 1039 (0x40f) of size   8 (0x08)
        Found level 1040 (0x410) of size   8 (0x08)
Found 19 valid levels

Checking for SETPATHINFO aliases
        Found level 1004 (0x3ec) of size  40 (0x28)
        Found level 1010 (0x3f2) of size   2 (0x02)
        Found level 1013 (0x3f5) of size   2 (0x02)
        Found level 1014 (0x3f6) of size   8 (0x08)
        Found level 1016 (0x3f8) of size   4 (0x04)
        Found level 1019 (0x3fb) of size   8 (0x08)
        Found level 1020 (0x3fc) of size   8 (0x08)
        Found level 1023 (0x3ff) of size   8 (0x08)
        Found level 1025 (0x401) of size  16 (0x10)
        Found level 1029 (0x405) of size  72 (0x48)
        Found level 1032 (0x408) of size  56 (0x38)
        Found level 1039 (0x40f) of size   8 (0x08)
        Found level 1040 (0x410) of size   8 (0x08)
Found 13 valid levels
*/
#define SMB_SFILEINFO_STANDARD                             1
#define SMB_SFILEINFO_EA_SET                               2
#define SMB_SFILEINFO_BASIC_INFO	               0x101
#define SMB_SFILEINFO_DISPOSITION_INFO		       0x102
#define SMB_SFILEINFO_ALLOCATION_INFO                  0x103
#define SMB_SFILEINFO_END_OF_FILE_INFO                 0x104
#define SMB_SFILEINFO_UNIX_BASIC                       0x200
#define SMB_SFILEINFO_UNIX_LINK                        0x201
#define SMB_SPATHINFO_UNIX_HLINK                       0x203
#define SMB_SPATHINFO_POSIX_ACL                        0x204
#define SMB_SPATHINFO_XATTR                            0x205
#define SMB_SFILEINFO_ATTR_FLAGS                       0x206	
#define SMB_SFILEINFO_UNIX_INFO2                       0x20b
#define SMB_SFILEINFO_BASIC_INFORMATION			1004
#define SMB_SFILEINFO_RENAME_INFORMATION		1010
#define SMB_SFILEINFO_LINK_INFORMATION			1011
#define SMB_SFILEINFO_DISPOSITION_INFORMATION		1013
#define SMB_SFILEINFO_POSITION_INFORMATION		1014
#define SMB_SFILEINFO_FULL_EA_INFORMATION		1015
#define SMB_SFILEINFO_MODE_INFORMATION			1016
#define SMB_SFILEINFO_ALLOCATION_INFORMATION		1019
#define SMB_SFILEINFO_END_OF_FILE_INFORMATION		1020
#define SMB_SFILEINFO_PIPE_INFORMATION			1023
#define SMB_SFILEINFO_VALID_DATA_INFORMATION		1039
#define SMB_SFILEINFO_SHORT_NAME_INFORMATION		1040

/* filemon shows FilePipeRemoteInformation */
#define SMB_SFILEINFO_1025				1025

/* vista scan responds */
#define SMB_SFILEINFO_1027				1027

/* filemon shows CopyOnWriteInformation */
#define SMB_SFILEINFO_1029				1029

/* filemon shows OleClassIdInformation */
#define SMB_SFILEINFO_1032				1032

/* vista scan responds to these */
#define SMB_SFILEINFO_1030				1030
#define SMB_SFILEINFO_1031				1031
#define SMB_SFILEINFO_1036				1036
#define SMB_SFILEINFO_1041				1041
#define SMB_SFILEINFO_1042				1042
#define SMB_SFILEINFO_1043				1043
#define SMB_SFILEINFO_1044				1044

/* trans2 findfirst levels */
/*
w2k3 TRANS2ALIASES:
Checking for FINDFIRST aliases
        Found level    1 (0x001) of size  68 (0x44)
        Found level    2 (0x002) of size  70 (0x46)
        Found level  257 (0x101) of size 108 (0x6c)
        Found level  258 (0x102) of size 116 (0x74)
        Found level  259 (0x103) of size  60 (0x3c)
        Found level  260 (0x104) of size 140 (0x8c)
        Found level  261 (0x105) of size 124 (0x7c)
        Found level  262 (0x106) of size 148 (0x94)
Found 8 levels with success status
Found 0 aliased levels
*/
#define SMB_FIND_STANDARD		    1
#define SMB_FIND_EA_SIZE		    2
#define SMB_FIND_EA_LIST		    3
#define SMB_FIND_DIRECTORY_INFO		0x101
#define SMB_FIND_FULL_DIRECTORY_INFO	0x102
#define SMB_FIND_NAME_INFO		0x103
#define SMB_FIND_BOTH_DIRECTORY_INFO	0x104
#define SMB_FIND_ID_FULL_DIRECTORY_INFO	0x105
#define SMB_FIND_ID_BOTH_DIRECTORY_INFO 0x106
#define SMB_FIND_UNIX_INFO              0x202
#define SMB_FIND_UNIX_INFO2             0x20b

/* flags on trans2 findfirst/findnext that control search */
#define FLAG_TRANS2_FIND_CLOSE          0x1
#define FLAG_TRANS2_FIND_CLOSE_IF_END   0x2
#define FLAG_TRANS2_FIND_REQUIRE_RESUME 0x4
#define FLAG_TRANS2_FIND_CONTINUE       0x8
#define FLAG_TRANS2_FIND_BACKUP_INTENT  0x10

/*
 * DeviceType and Characteristics returned in a
 * SMB_QFS_DEVICE_INFO call.
 */
#define QFS_DEVICETYPE_CD_ROM		        0x2
#define QFS_DEVICETYPE_CD_ROM_FILE_SYSTEM	0x3
#define QFS_DEVICETYPE_DISK			0x7
#define QFS_DEVICETYPE_DISK_FILE_SYSTEM	        0x8
#define QFS_DEVICETYPE_FILE_SYSTEM		0x9

/* Characteristics. */
#define QFS_TYPE_REMOVABLE_MEDIA		0x1
#define QFS_TYPE_READ_ONLY_DEVICE		0x2
#define QFS_TYPE_FLOPPY			        0x4
#define QFS_TYPE_WORM			        0x8
#define QFS_TYPE_REMOTE			        0x10
#define QFS_TYPE_MOUNTED			0x20
#define QFS_TYPE_VIRTUAL			0x40


/*
 * Thursby MAC extensions....
 */

/*
 * MAC CIFS Extensions have the range 0x300 - 0x2FF reserved.
 * Supposedly Microsoft have agreed to this.
 */

#define MIN_MAC_INFO_LEVEL                      0x300
#define MAX_MAC_INFO_LEVEL                      0x3FF
#define SMB_QFS_MAC_FS_INFO                     0x301



/* UNIX CIFS Extensions - created by HP */
/*
 * UNIX CIFS Extensions have the range 0x200 - 0x2FF reserved.
 * Supposedly Microsoft have agreed to this.
 */

#define MIN_UNIX_INFO_LEVEL 0x200
#define MAX_UNIX_INFO_LEVEL 0x2FF

#define INFO_LEVEL_IS_UNIX(level) (((level) >= MIN_UNIX_INFO_LEVEL) && ((level) <= MAX_UNIX_INFO_LEVEL))

#define SMB_MODE_NO_CHANGE                 0xFFFFFFFF     /* file mode value which */
                                              /* means "don't change it" */
#define SMB_UID_NO_CHANGE                  0xFFFFFFFF
#define SMB_GID_NO_CHANGE                  0xFFFFFFFF

#define SMB_SIZE_NO_CHANGE_LO              0xFFFFFFFF
#define SMB_SIZE_NO_CHANGE_HI              0xFFFFFFFF
 
#define SMB_TIME_NO_CHANGE_LO              0xFFFFFFFF
#define SMB_TIME_NO_CHANGE_HI              0xFFFFFFFF

/*
UNIX_BASIC  info level:

Offset Size         Name
0      LARGE_INTEGER EndOfFile                File size
8      LARGE_INTEGER Blocks                   Number of bytes used on disk (st_blocks).
16     LARGE_INTEGER CreationTime             Creation time
24     LARGE_INTEGER LastAccessTime           Last access time
32     LARGE_INTEGER LastModificationTime     Last modification time
40     LARGE_INTEGER Uid                      Numeric user id for the owner
48     LARGE_INTEGER Gid                      Numeric group id of owner
56     ULONG Type                             Enumeration specifying the pathname type:
                                              0 -- File
                                              1 -- Directory
                                              2 -- Symbolic link
                                              3 -- Character device
                                              4 -- Block device
                                              5 -- FIFO (named pipe)
                                              6 -- Unix domain socket

60     LARGE_INTEGER devmajor                 Major device number if type is device
68     LARGE_INTEGER devminor                 Minor device number if type is device
76     LARGE_INTEGER uniqueid                 This is a server-assigned unique id for the file. The client
                                              will typically map this onto an inode number. The scope of
                                              uniqueness is the share.
84     LARGE_INTEGER permissions              Standard UNIX file permissions  - see below.
92     LARGE_INTEGER nlinks                   The number of directory entries that map to this entry
                                              (number of hard links)

100 - end.
*/

/*
SMB_QUERY_FILE_UNIX_INFO2 is SMB_QUERY_FILE_UNIX_BASIC with create
time and file flags appended. The corresponding info level for
findfirst/findnext is SMB_FIND_FILE_UNIX_UNIX2.

Size    Offset  Value
---------------------
0      LARGE_INTEGER EndOfFile  	File size
8      LARGE_INTEGER Blocks     	Number of blocks used on disk
16     LARGE_INTEGER ChangeTime 	Attribute change time
24     LARGE_INTEGER LastAccessTime           Last access time
32     LARGE_INTEGER LastModificationTime     Last modification time
40     LARGE_INTEGER Uid        	Numeric user id for the owner
48     LARGE_INTEGER Gid        	Numeric group id of owner
56     ULONG Type               	Enumeration specifying the file type
60     LARGE_INTEGER devmajor   	Major device number if type is device
68     LARGE_INTEGER devminor   	Minor device number if type is device
76     LARGE_INTEGER uniqueid   	This is a server-assigned unique id
84     LARGE_INTEGER permissions        Standard UNIX permissions
92     LARGE_INTEGER nlinks     	Number of hard link)
100    LARGE_INTEGER CreationTime       Create/birth time
108    ULONG FileFlags          	File flags enumeration
112    ULONG FileFlagsMask      	Mask of valid flags
*/

/* UNIX filetype mappings. */

#define UNIX_TYPE_FILE      0
#define UNIX_TYPE_DIR       1
#define UNIX_TYPE_SYMLINK   2
#define UNIX_TYPE_CHARDEV   3
#define UNIX_TYPE_BLKDEV    4
#define UNIX_TYPE_FIFO      5
#define UNIX_TYPE_SOCKET    6
#define UNIX_TYPE_UNKNOWN   0xFFFFFFFF

/*
 * Oh this is fun. "Standard UNIX permissions" has no
 * meaning in POSIX. We need to define the mapping onto
 * and off the wire as this was not done in the original HP
 * spec. JRA.
 */

#define UNIX_X_OTH			0000001
#define UNIX_W_OTH			0000002
#define UNIX_R_OTH			0000004
#define UNIX_X_GRP			0000010
#define UNIX_W_GRP                      0000020
#define UNIX_R_GRP                      0000040
#define UNIX_X_USR                      0000100
#define UNIX_W_USR                      0000200
#define UNIX_R_USR                      0000400
#define UNIX_STICKY                     0001000
#define UNIX_SET_GID                    0002000
#define UNIX_SET_UID                    0004000

/* Masks for the above */
#define UNIX_OTH_MASK                   0000007
#define UNIX_GRP_MASK                   0000070
#define UNIX_USR_MASK                   0000700
#define UNIX_PERM_MASK                  0000777
#define UNIX_EXTRA_MASK                 0007000
#define UNIX_ALL_MASK                   0007777

/* Flags for the file_flags field in UNIX_INFO2: */
#define EXT_SECURE_DELETE               0x00000001
#define EXT_ENABLE_UNDELETE             0x00000002
#define EXT_SYNCHRONOUS                 0x00000004
#define EXT_IMMUTABLE			0x00000008
#define EXT_OPEN_APPEND_ONLY            0x00000010
#define EXT_DO_NOT_BACKUP               0x00000020
#define EXT_NO_UPDATE_ATIME             0x00000040
#define EXT_HIDDEN                      0x00000080

#define SMB_QFILEINFO_UNIX_LINK         0x201
#define SMB_SFILEINFO_UNIX_LINK         0x201
#define SMB_SFILEINFO_UNIX_HLINK        0x203

/*
 Info level for QVOLINFO - returns version of CIFS UNIX extensions, plus
 64-bits worth of capability fun :-).
*/

#define SMB_QUERY_CIFS_UNIX_INFO      0x200

/* Returns the following.

  UINT16             major version number
  UINT16             minor version number
  LARGE_INTEGER      capability bitfield

*/

#define CIFS_UNIX_MAJOR_VERSION 1
#define CIFS_UNIX_MINOR_VERSION 0

#define CIFS_UNIX_FCNTL_LOCKS_CAP           0x1
#define CIFS_UNIX_POSIX_ACLS_CAP            0x2

/* ... more as we think of them :-). */

#endif
