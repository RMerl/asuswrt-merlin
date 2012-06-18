/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup, plus a whole lot more.
   
   Copyright (C) Andrew Tridgell              1992-2000
   Copyright (C) John H Terpstra              1996-2002
   Copyright (C) Luke Kenneth Casson Leighton 1996-2000
   Copyright (C) Paul Ashton                  1998-2000
   Copyright (C) Simo Sorce                   2001-2002
   Copyright (C) Martin Pool		      2002
   
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

#ifndef _SMB_H
#define _SMB_H

/* deny modes */
#define DENY_DOS 0
#define DENY_ALL 1
#define DENY_WRITE 2
#define DENY_READ 3
#define DENY_NONE 4
#define DENY_FCB 7

/* open modes */
#define DOS_OPEN_RDONLY 0
#define DOS_OPEN_WRONLY 1
#define DOS_OPEN_RDWR 2
#define DOS_OPEN_FCB 0xF


/**********************************/
/* SMBopen field definitions      */
#define OPEN_FLAGS_DENY_MASK  0x70
#define OPEN_FLAGS_DENY_DOS   0x00
#define OPEN_FLAGS_DENY_ALL   0x10
#define OPEN_FLAGS_DENY_WRITE 0x20
#define OPEN_FLAGS_DENY_READ  0x30
#define OPEN_FLAGS_DENY_NONE  0x40

#define OPEN_FLAGS_MODE_MASK  0x0F
#define OPEN_FLAGS_OPEN_READ     0
#define OPEN_FLAGS_OPEN_WRITE    1
#define OPEN_FLAGS_OPEN_RDWR     2
#define OPEN_FLAGS_FCB        0xFF


/**********************************/
/* SMBopenX field definitions     */

/* OpenX Flags field. */
#define OPENX_FLAGS_ADDITIONAL_INFO      0x01
#define OPENX_FLAGS_REQUEST_OPLOCK       0x02
#define OPENX_FLAGS_REQUEST_BATCH_OPLOCK 0x04
#define OPENX_FLAGS_EA_LEN               0x08
#define OPENX_FLAGS_EXTENDED_RETURN      0x10

/* desired access (open_mode), split info 4 4-bit nibbles */
#define OPENX_MODE_ACCESS_MASK   0x000F
#define OPENX_MODE_ACCESS_READ   0x0000
#define OPENX_MODE_ACCESS_WRITE  0x0001
#define OPENX_MODE_ACCESS_RDWR   0x0002
#define OPENX_MODE_ACCESS_EXEC   0x0003
#define OPENX_MODE_ACCESS_FCB    0x000F

#define OPENX_MODE_DENY_SHIFT    4
#define OPENX_MODE_DENY_MASK     (0xF        << OPENX_MODE_DENY_SHIFT)
#define OPENX_MODE_DENY_DOS      (DENY_DOS   << OPENX_MODE_DENY_SHIFT)
#define OPENX_MODE_DENY_ALL      (DENY_ALL   << OPENX_MODE_DENY_SHIFT)
#define OPENX_MODE_DENY_WRITE    (DENY_WRITE << OPENX_MODE_DENY_SHIFT)
#define OPENX_MODE_DENY_READ     (DENY_READ  << OPENX_MODE_DENY_SHIFT)
#define OPENX_MODE_DENY_NONE     (DENY_NONE  << OPENX_MODE_DENY_SHIFT)
#define OPENX_MODE_DENY_FCB      (0xF        << OPENX_MODE_DENY_SHIFT)

#define OPENX_MODE_LOCALITY_MASK 0x0F00 /* what does this do? */

#define OPENX_MODE_NO_CACHE      0x1000
#define OPENX_MODE_WRITE_THRU    0x4000

/* open function values */
#define OPENX_OPEN_FUNC_MASK  0x3
#define OPENX_OPEN_FUNC_FAIL  0x0
#define OPENX_OPEN_FUNC_OPEN  0x1
#define OPENX_OPEN_FUNC_TRUNC 0x2

/* The above can be OR'ed with... */
#define OPENX_OPEN_FUNC_CREATE 0x10

/* openx action in reply */
#define OPENX_ACTION_EXISTED    1
#define OPENX_ACTION_CREATED    2
#define OPENX_ACTION_TRUNCATED  3


/**********************************/
/* SMBntcreateX field definitions */

/* ntcreatex flags field. */
#define NTCREATEX_FLAGS_REQUEST_OPLOCK       0x02
#define NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK 0x04
#define NTCREATEX_FLAGS_OPEN_DIRECTORY       0x08 /* TODO: opens parent? we need
						     a test suite for this */
#define NTCREATEX_FLAGS_EXTENDED             0x10

/* the ntcreatex access_mask field 
   this is split into 4 pieces
   AAAABBBBCCCCCCCCDDDDDDDDDDDDDDDD
   A -> GENERIC_RIGHT_*
   B -> SEC_RIGHT_*
   C -> STD_RIGHT_*
   D -> SA_RIGHT_*
   
   which set of SA_RIGHT_* bits is applicable depends on the type
   of object.
*/



/* ntcreatex share_access field */
#define NTCREATEX_SHARE_ACCESS_NONE   0
#define NTCREATEX_SHARE_ACCESS_READ   1
#define NTCREATEX_SHARE_ACCESS_WRITE  2
#define NTCREATEX_SHARE_ACCESS_DELETE 4
#define NTCREATEX_SHARE_ACCESS_MASK   7

/* ntcreatex open_disposition field */
#define NTCREATEX_DISP_SUPERSEDE 0     /* supersede existing file (if it exists) */
#define NTCREATEX_DISP_OPEN 1          /* if file exists open it, else fail */
#define NTCREATEX_DISP_CREATE 2        /* if file exists fail, else create it */
#define NTCREATEX_DISP_OPEN_IF 3       /* if file exists open it, else create it */
#define NTCREATEX_DISP_OVERWRITE 4     /* if exists overwrite, else fail */
#define NTCREATEX_DISP_OVERWRITE_IF 5  /* if exists overwrite, else create */

/* ntcreatex create_options field */
#define NTCREATEX_OPTIONS_DIRECTORY                 0x0001
#define NTCREATEX_OPTIONS_WRITE_THROUGH             0x0002
#define NTCREATEX_OPTIONS_SEQUENTIAL_ONLY           0x0004
#define NTCREATEX_OPTIONS_NO_INTERMEDIATE_BUFFERING 0x0008
#define NTCREATEX_OPTIONS_SYNC_ALERT	            0x0010
#define NTCREATEX_OPTIONS_ASYNC_ALERT	            0x0020
#define NTCREATEX_OPTIONS_NON_DIRECTORY_FILE        0x0040
#define NTCREATEX_OPTIONS_TREE_CONNECTION           0x0080
#define NTCREATEX_OPTIONS_COMPLETE_IF_OPLOCKED      0x0100
#define NTCREATEX_OPTIONS_NO_EA_KNOWLEDGE           0x0200
#define NTCREATEX_OPTIONS_OPEN_FOR_RECOVERY         0x0400
#define NTCREATEX_OPTIONS_RANDOM_ACCESS             0x0800
#define NTCREATEX_OPTIONS_DELETE_ON_CLOSE           0x1000
#define NTCREATEX_OPTIONS_OPEN_BY_FILE_ID           0x2000
#define NTCREATEX_OPTIONS_BACKUP_INTENT             0x4000
#define NTCREATEX_OPTIONS_NO_COMPRESSION            0x8000
/* Must be ignored by the server, per MS-SMB 2.2.8 */
#define NTCREATEX_OPTIONS_OPFILTER              0x00100000
#define NTCREATEX_OPTIONS_REPARSE_POINT         0x00200000
/* Don't pull this file off tape in a HSM system */
#define NTCREATEX_OPTIONS_NO_RECALL             0x00400000
/* Must be ignored by the server, per MS-SMB 2.2.8 */
#define NTCREATEX_OPTIONS_FREE_SPACE_QUERY      0x00800000

#define NTCREATEX_OPTIONS_MUST_IGNORE_MASK      (NTCREATEX_OPTIONS_TREE_CONNECTION | \
						 NTCREATEX_OPTIONS_OPEN_FOR_RECOVERY | \
						 NTCREATEX_OPTIONS_FREE_SPACE_QUERY | \
						 0x000F0000)

#define NTCREATEX_OPTIONS_NOT_SUPPORTED_MASK    (NTCREATEX_OPTIONS_OPEN_BY_FILE_ID)

#define NTCREATEX_OPTIONS_INVALID_PARAM_MASK    (NTCREATEX_OPTIONS_OPFILTER | \
						 NTCREATEX_OPTIONS_SYNC_ALERT | \
						 NTCREATEX_OPTIONS_ASYNC_ALERT | \
						 NTCREATEX_OPTIONS_OPFILTER | \
						 0xFF000000)

/*
 * We reuse some ignored flags for private use.
 * This values have different meaning for some ntvfs backends.
 *
 * TODO: use values that are ignore for sure...
 */
#define NTCREATEX_OPTIONS_PRIVATE_DENY_DOS      0x00010000
#define NTCREATEX_OPTIONS_PRIVATE_DENY_FCB      0x00020000
#define NTCREATEX_OPTIONS_PRIVATE_MASK          (NTCREATEX_OPTIONS_PRIVATE_DENY_DOS | \
						 NTCREATEX_OPTIONS_PRIVATE_DENY_FCB)

/* ntcreatex impersonation field */
#define NTCREATEX_IMPERSONATION_ANONYMOUS      0
#define NTCREATEX_IMPERSONATION_IDENTIFICATION 1
#define NTCREATEX_IMPERSONATION_IMPERSONATION  2
#define NTCREATEX_IMPERSONATION_DELEGATION     3

/* ntcreatex security flags bit field */
#define NTCREATEX_SECURITY_DYNAMIC             1
#define NTCREATEX_SECURITY_ALL                 2

/* ntcreatex create_action in reply */
#define NTCREATEX_ACTION_EXISTED     1
#define NTCREATEX_ACTION_CREATED     2
#define NTCREATEX_ACTION_TRUNCATED   3
/* the value 5 can also be returned when you try to create a directory with
   incorrect parameters - what does it mean? maybe created temporary file? */
#define NTCREATEX_ACTION_UNKNOWN 5

#define SMB_MAGIC 0x424D53FF /* 0xFF 'S' 'M' 'B' */

/* the basic packet size, assuming no words or bytes. Does not include the NBT header */
#define MIN_SMB_SIZE 35

/* when using NBT encapsulation every packet has a 4 byte header */
#define NBT_HDR_SIZE 4

/* offsets into message header for common items - NOTE: These have
   changed from being offsets from the base of the NBT packet to the base of the SMB packet.
   this has reduced all these values by 4
*/
#define HDR_COM 4
#define HDR_RCLS 5
#define HDR_REH 6
#define HDR_ERR 7
#define HDR_FLG 9
#define HDR_FLG2 10
#define HDR_PIDHIGH 12
#define HDR_SS_FIELD 14
#define HDR_TID 24
#define HDR_PID 26
#define HDR_UID 28
#define HDR_MID 30
#define HDR_WCT 32
#define HDR_VWV 33


/* types of buffers in core SMB protocol */
#define SMB_DATA_BLOCK 0x1
#define SMB_ASCII4     0x4


/* flag defines. CIFS spec 3.1.1 */
#define FLAG_SUPPORT_LOCKREAD       0x01
#define FLAG_CLIENT_BUF_AVAIL       0x02
#define FLAG_RESERVED               0x04
#define FLAG_CASELESS_PATHNAMES     0x08
#define FLAG_CANONICAL_PATHNAMES    0x10
#define FLAG_REQUEST_OPLOCK         0x20
#define FLAG_REQUEST_BATCH_OPLOCK   0x40
#define FLAG_REPLY                  0x80

/* the complete */
#define SMBmkdir      0x00   /* create directory */
#define SMBrmdir      0x01   /* delete directory */
#define SMBopen       0x02   /* open file */
#define SMBcreate     0x03   /* create file */
#define SMBclose      0x04   /* close file */
#define SMBflush      0x05   /* flush file */
#define SMBunlink     0x06   /* delete file */
#define SMBmv         0x07   /* rename file */
#define SMBgetatr     0x08   /* get file attributes */
#define SMBsetatr     0x09   /* set file attributes */
#define SMBread       0x0A   /* read from file */
#define SMBwrite      0x0B   /* write to file */
#define SMBlock       0x0C   /* lock byte range */
#define SMBunlock     0x0D   /* unlock byte range */
#define SMBctemp      0x0E   /* create temporary file */
#define SMBmknew      0x0F   /* make new file */
#define SMBchkpth     0x10   /* check directory path */
#define SMBexit       0x11   /* process exit */
#define SMBlseek      0x12   /* seek */
#define SMBtcon       0x70   /* tree connect */
#define SMBtconX      0x75   /* tree connect and X*/
#define SMBtdis       0x71   /* tree disconnect */
#define SMBnegprot    0x72   /* negotiate protocol */
#define SMBdskattr    0x80   /* get disk attributes */
#define SMBsearch     0x81   /* search directory */
#define SMBsplopen    0xC0   /* open print spool file */
#define SMBsplwr      0xC1   /* write to print spool file */
#define SMBsplclose   0xC2   /* close print spool file */
#define SMBsplretq    0xC3   /* return print queue */
#define SMBsends      0xD0   /* send single block message */
#define SMBsendb      0xD1   /* send broadcast message */
#define SMBfwdname    0xD2   /* forward user name */
#define SMBcancelf    0xD3   /* cancel forward */
#define SMBgetmac     0xD4   /* get machine name */
#define SMBsendstrt   0xD5   /* send start of multi-block message */
#define SMBsendend    0xD6   /* send end of multi-block message */
#define SMBsendtxt    0xD7   /* send text of multi-block message */

/* Core+ protocol */
#define SMBlockread	  0x13   /* Lock a range and read */
#define SMBwriteunlock 0x14 /* write then range then unlock it */
#define SMBreadbraw   0x1a  /* read a block of data with no smb header */
#define SMBwritebraw  0x1d  /* write a block of data with no smb header */
#define SMBwritec     0x20  /* secondary write request */
#define SMBwriteclose 0x2c  /* write a file then close it */

/* dos extended protocol */
#define SMBreadBraw      0x1A   /* read block raw */
#define SMBreadBmpx      0x1B   /* read block multiplexed */
#define SMBreadBs        0x1C   /* read block (secondary response) */
#define SMBwriteBraw     0x1D   /* write block raw */
#define SMBwriteBmpx     0x1E   /* write block multiplexed */
#define SMBwriteBs       0x1F   /* write block (secondary request) */
#define SMBwriteC        0x20   /* write complete response */
#define SMBsetattrE      0x22   /* set file attributes expanded */
#define SMBgetattrE      0x23   /* get file attributes expanded */
#define SMBlockingX      0x24   /* lock/unlock byte ranges and X */
#define SMBtrans         0x25   /* transaction - name, bytes in/out */
#define SMBtranss        0x26   /* transaction (secondary request/response) */
#define SMBioctl         0x27   /* IOCTL */
#define SMBioctls        0x28   /* IOCTL  (secondary request/response) */
#define SMBcopy          0x29   /* copy */
#define SMBmove          0x2A   /* move */
#define SMBecho          0x2B   /* echo */
#define SMBopenX         0x2D   /* open and X */
#define SMBreadX         0x2E   /* read and X */
#define SMBwriteX        0x2F   /* write and X */
#define SMBsesssetupX    0x73   /* Session Set Up & X (including User Logon) */
#define SMBffirst        0x82   /* find first */
#define SMBfunique       0x83   /* find unique */
#define SMBfclose        0x84   /* find close */
#define SMBkeepalive     0x85   /* keepalive */
#define SMBinvalid       0xFE   /* invalid command */

/* Extended 2.0 protocol */
#define SMBtrans2        0x32   /* TRANS2 protocol set */
#define SMBtranss2       0x33   /* TRANS2 protocol set, secondary command */
#define SMBfindclose     0x34   /* Terminate a TRANSACT2_FINDFIRST */
#define SMBfindnclose    0x35   /* Terminate a TRANSACT2_FINDNOTIFYFIRST */
#define SMBulogoffX      0x74   /* user logoff */

/* NT SMB extensions. */
#define SMBnttrans       0xA0   /* NT transact */
#define SMBnttranss      0xA1   /* NT transact secondary */
#define SMBntcreateX     0xA2   /* NT create and X */
#define SMBntcancel      0xA4   /* NT cancel */
#define SMBntrename      0xA5   /* NT rename */

/* used to indicate end of chain */
#define SMB_CHAIN_NONE   0xFF

/* These are the trans subcommands */
#define TRANSACT_SETNAMEDPIPEHANDLESTATE  0x01 
#define TRANSACT_DCERPCCMD                0x26
#define TRANSACT_WAITNAMEDPIPEHANDLESTATE 0x53

/* These are the NT transact sub commands. */
#define NT_TRANSACT_CREATE                1
#define NT_TRANSACT_IOCTL                 2
#define NT_TRANSACT_SET_SECURITY_DESC     3
#define NT_TRANSACT_NOTIFY_CHANGE         4
#define NT_TRANSACT_RENAME                5
#define NT_TRANSACT_QUERY_SECURITY_DESC   6

/* this is used on a TConX. I'm not sure the name is very helpful though */
#define SMB_SUPPORT_SEARCH_BITS        0x0001
#define SMB_SHARE_IN_DFS               0x0002

/* Named pipe write mode flags. Used in writeX calls. */
#define PIPE_RAW_MODE 0x4
#define PIPE_START_MESSAGE 0x8

/* the desired access to use when opening a pipe */
#define DESIRED_ACCESS_PIPE 0x2019f
 

/* Mapping of generic access rights for files to specific rights. */
#define FILE_GENERIC_ALL (STANDARD_RIGHTS_REQUIRED_ACCESS| NT_ACCESS_SYNCHRONIZE_ACCESS|FILE_ALL_ACCESS)

#define FILE_GENERIC_READ (STANDARD_RIGHTS_READ_ACCESS|FILE_READ_DATA|FILE_READ_ATTRIBUTES|\
							FILE_READ_EA|NT_ACCESS_SYNCHRONIZE_ACCESS)

#define FILE_GENERIC_WRITE (STANDARD_RIGHTS_WRITE_ACCESS|FILE_WRITE_DATA|FILE_WRITE_ATTRIBUTES|\
			    FILE_WRITE_EA|FILE_APPEND_DATA|NT_ACCESS_SYNCHRONIZE_ACCESS)

#define FILE_GENERIC_EXECUTE (STANDARD_RIGHTS_EXECUTE_ACCESS|FILE_READ_ATTRIBUTES|\
			    FILE_EXECUTE|NT_ACCESS_SYNCHRONIZE_ACCESS)


/* FileAttributes (search attributes) field */
#define FILE_ATTRIBUTE_READONLY		0x0001
#define FILE_ATTRIBUTE_HIDDEN		0x0002
#define FILE_ATTRIBUTE_SYSTEM		0x0004
#define FILE_ATTRIBUTE_VOLUME		0x0008
#define FILE_ATTRIBUTE_DIRECTORY	0x0010
#define FILE_ATTRIBUTE_ARCHIVE		0x0020
#define FILE_ATTRIBUTE_DEVICE		0x0040
#define FILE_ATTRIBUTE_NORMAL		0x0080
#define FILE_ATTRIBUTE_TEMPORARY	0x0100
#define FILE_ATTRIBUTE_SPARSE		0x0200
#define FILE_ATTRIBUTE_REPARSE_POINT	0x0400
#define FILE_ATTRIBUTE_COMPRESSED	0x0800
#define FILE_ATTRIBUTE_OFFLINE		0x1000
#define FILE_ATTRIBUTE_NONINDEXED	0x2000
#define FILE_ATTRIBUTE_ENCRYPTED	0x4000
#define FILE_ATTRIBUTE_ALL_MASK 	0x7FFF

/* Flags - combined with attributes. */
#define FILE_FLAG_WRITE_THROUGH    0x80000000L
#define FILE_FLAG_NO_BUFFERING     0x20000000L
#define FILE_FLAG_RANDOM_ACCESS    0x10000000L
#define FILE_FLAG_SEQUENTIAL_SCAN  0x08000000L
#define FILE_FLAG_DELETE_ON_CLOSE  0x04000000L
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000L /* only if backup/restore privilege? */
#define FILE_FLAG_POSIX_SEMANTICS  0x01000000L

/* Responses when opening a file. */
#define FILE_WAS_SUPERSEDED 0
#define FILE_WAS_OPENED 1
#define FILE_WAS_CREATED 2
#define FILE_WAS_OVERWRITTEN 3

/* File type flags */
#define FILE_TYPE_DISK  0
#define FILE_TYPE_BYTE_MODE_PIPE 1
#define FILE_TYPE_MESSAGE_MODE_PIPE 2
#define FILE_TYPE_PRINTER 3
#define FILE_TYPE_COMM_DEVICE 4
#define FILE_TYPE_UNKNOWN 0xFFFF

/* Flag for NT transact rename call. */
#define RENAME_REPLACE_IF_EXISTS 1

/* flags for SMBntrename call */
#define RENAME_FLAG_MOVE_CLUSTER_INFORMATION 0x102 /* ???? */
#define RENAME_FLAG_HARD_LINK                0x103
#define RENAME_FLAG_RENAME                   0x104
#define RENAME_FLAG_COPY                     0x105

/* Filesystem Attributes. */
#define FILE_CASE_SENSITIVE_SEARCH 0x01
#define FILE_CASE_PRESERVED_NAMES 0x02
#define FILE_UNICODE_ON_DISK 0x04
/* According to cifs9f, this is 4, not 8 */
/* Acconding to testing, this actually sets the security attribute! */
#define FILE_PERSISTENT_ACLS 0x08
/* These entries added from cifs9f --tsb */
#define FILE_FILE_COMPRESSION 0x10
#define FILE_VOLUME_QUOTAS 0x20
/* I think this is wrong. JRA #define FILE_DEVICE_IS_MOUNTED 0x20 */
#define FILE_VOLUME_SPARSE_FILE 0x40
#define FILE_VOLUME_IS_COMPRESSED 0x8000

/* ChangeNotify flags. */
#define FILE_NOTIFY_CHANGE_FILE_NAME	0x00000001
#define FILE_NOTIFY_CHANGE_DIR_NAME	0x00000002
#define FILE_NOTIFY_CHANGE_ATTRIBUTES	0x00000004
#define FILE_NOTIFY_CHANGE_SIZE		0x00000008
#define FILE_NOTIFY_CHANGE_LAST_WRITE	0x00000010
#define FILE_NOTIFY_CHANGE_LAST_ACCESS	0x00000020
#define FILE_NOTIFY_CHANGE_CREATION	0x00000040
#define FILE_NOTIFY_CHANGE_EA		0x00000080
#define FILE_NOTIFY_CHANGE_SECURITY	0x00000100
#define FILE_NOTIFY_CHANGE_STREAM_NAME	0x00000200
#define FILE_NOTIFY_CHANGE_STREAM_SIZE	0x00000400
#define FILE_NOTIFY_CHANGE_STREAM_WRITE	0x00000800

#define FILE_NOTIFY_CHANGE_NAME \
	(FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME)

/* change notify action results */
#define NOTIFY_ACTION_ADDED 1
#define NOTIFY_ACTION_REMOVED 2
#define NOTIFY_ACTION_MODIFIED 3
#define NOTIFY_ACTION_OLD_NAME 4
#define NOTIFY_ACTION_NEW_NAME 5
#define NOTIFY_ACTION_ADDED_STREAM 6
#define NOTIFY_ACTION_REMOVED_STREAM 7
#define NOTIFY_ACTION_MODIFIED_STREAM 8

/* seek modes for smb_seek */
#define SEEK_MODE_START   0
#define SEEK_MODE_CURRENT 1
#define SEEK_MODE_END     2

/* where to find the base of the SMB packet proper */
/* REWRITE TODO: smb_base needs to be removed */
#define smb_base(buf) (((char *)(buf))+4)

/* we don't allow server strings to be longer than 48 characters as
   otherwise NT will not honour the announce packets */
#define MAX_SERVER_STRING_LENGTH 48

/* This was set by JHT in liaison with Jeremy Allison early 1997
 * History:
 * Version 4.0 - never made public
 * Version 4.10 - New to 1.9.16p2, lost in space 1.9.16p3 to 1.9.16p9
 *              - Reappeared in 1.9.16p11 with fixed smbd services
 * Version 4.20 - To indicate that nmbd and browsing now works better
 * Version 4.50 - Set at release of samba-2.2.0 by JHT
 *
 *  Note: In the presence of NT4.X do not set above 4.9
 *        Setting this above 4.9 can have undesired side-effects.
 *        This may change again in Samba-3.0 after further testing. JHT
 */
 
#define DEFAULT_MAJOR_VERSION 0x04
#define DEFAULT_MINOR_VERSION 0x09

/* Browser Election Values */
#define BROWSER_ELECTION_VERSION	0x010f
#define BROWSER_CONSTANT	0xaa55

/* Sercurity mode bits. */
#define NEGOTIATE_SECURITY_USER_LEVEL		0x01
#define NEGOTIATE_SECURITY_CHALLENGE_RESPONSE	0x02
#define NEGOTIATE_SECURITY_SIGNATURES_ENABLED	0x04
#define NEGOTIATE_SECURITY_SIGNATURES_REQUIRED	0x08

/* NT Flags2 bits - cifs6.txt section 3.1.2 */
#define FLAGS2_LONG_PATH_COMPONENTS    0x0001
#define FLAGS2_EXTENDED_ATTRIBUTES     0x0002
#define FLAGS2_SMB_SECURITY_SIGNATURES 0x0004
#define FLAGS2_IS_LONG_NAME            0x0040
#define FLAGS2_EXTENDED_SECURITY       0x0800 
#define FLAGS2_DFS_PATHNAMES           0x1000
#define FLAGS2_READ_PERMIT_EXECUTE     0x2000
#define FLAGS2_32_BIT_ERROR_CODES      0x4000 
#define FLAGS2_UNICODE_STRINGS         0x8000


/* CIFS protocol capabilities */
#define CAP_RAW_MODE		0x00000001
#define CAP_MPX_MODE		0x00000002
#define CAP_UNICODE		0x00000004
#define CAP_LARGE_FILES		0x00000008
#define CAP_NT_SMBS		0x00000010
#define CAP_RPC_REMOTE_APIS	0x00000020
#define CAP_STATUS32		0x00000040
#define CAP_LEVEL_II_OPLOCKS	0x00000080
#define CAP_LOCK_AND_READ	0x00000100
#define CAP_NT_FIND		0x00000200
#define CAP_DFS			0x00001000
#define CAP_W2K_SMBS		0x00002000
#define CAP_LARGE_READX		0x00004000
#define CAP_LARGE_WRITEX	0x00008000
#define CAP_UNIX		0x00800000 /* Capabilities for UNIX extensions. Created by HP. */
#define CAP_EXTENDED_SECURITY	0x80000000

/*
 * Global value meaning that the smb_uid field should be
 * ingored (in share level security and protocol level == CORE)
 */

#define UID_FIELD_INVALID 0

/* Lock types. */
#define LOCKING_ANDX_SHARED_LOCK     0x01
#define LOCKING_ANDX_OPLOCK_RELEASE  0x02
#define LOCKING_ANDX_CHANGE_LOCKTYPE 0x04
#define LOCKING_ANDX_CANCEL_LOCK     0x08
#define LOCKING_ANDX_LARGE_FILES     0x10

/*
 * Bits we test with.
 */

#define OPLOCK_NONE      0
#define OPLOCK_EXCLUSIVE 1
#define OPLOCK_BATCH     2
#define OPLOCK_LEVEL_II  4

#define CORE_OPLOCK_GRANTED (1<<5)
#define EXTENDED_OPLOCK_GRANTED (1<<15)

/*
 * Return values for oplock types.
 */

#define NO_OPLOCK_RETURN 0
#define EXCLUSIVE_OPLOCK_RETURN 1
#define BATCH_OPLOCK_RETURN 2
#define LEVEL_II_OPLOCK_RETURN 3

/* oplock levels sent in oplock break */
#define OPLOCK_BREAK_TO_NONE     0
#define OPLOCK_BREAK_TO_LEVEL_II 1


#define CMD_REPLY 0x8000

/* The maximum length of a trust account password.
   Used when we randomly create it, 15 char passwords
   exceed NT4's max password length */

#define DEFAULT_TRUST_ACCOUNT_PASSWORD_LENGTH 14


/*
  filesystem attribute bits
*/
#define FS_ATTR_CASE_SENSITIVE_SEARCH             0x00000001
#define FS_ATTR_CASE_PRESERVED_NAMES              0x00000002
#define FS_ATTR_UNICODE_ON_DISK                   0x00000004
#define FS_ATTR_PERSISTANT_ACLS                   0x00000008
#define FS_ATTR_COMPRESSION                       0x00000010
#define FS_ATTR_QUOTAS                            0x00000020
#define FS_ATTR_SPARSE_FILES                      0x00000040
#define FS_ATTR_REPARSE_POINTS                    0x00000080
#define FS_ATTR_REMOTE_STORAGE                    0x00000100
#define FS_ATTR_LFN_SUPPORT                       0x00004000
#define FS_ATTR_IS_COMPRESSED                     0x00008000
#define FS_ATTR_OBJECT_IDS                        0x00010000
#define FS_ATTR_ENCRYPTION                        0x00020000
#define FS_ATTR_NAMED_STREAMS                     0x00040000

#define smb_len(buf) (PVAL(buf,3)|(PVAL(buf,2)<<8)|(PVAL(buf,1)<<16))
#define _smb_setlen(buf,len) do {(buf)[0] = 0; (buf)[1] = ((len)&0x10000)>>16; \
        (buf)[2] = ((len)&0xFF00)>>8; (buf)[3] = (len)&0xFF;} while (0)
#define _smb2_setlen(buf,len) do {(buf)[0] = 0; (buf)[1] = ((len)&0xFF0000)>>16; \
        (buf)[2] = ((len)&0xFF00)>>8; (buf)[3] = (len)&0xFF;} while (0)

#include "libcli/raw/trans2.h"
#include "libcli/raw/interfaces.h"

#endif /* _SMB_H */
