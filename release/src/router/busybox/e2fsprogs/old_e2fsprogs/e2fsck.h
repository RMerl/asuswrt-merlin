/* vi: set sw=4 ts=4: */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <setjmp.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stddef.h>
#include <assert.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <mntent.h>
#include <dirent.h>
#include "ext2fs/kernel-list.h"
#include <sys/types.h>
#include <linux/types.h>

/*
 * Now pull in the real linux/jfs.h definitions.
 */
#include "ext2fs/kernel-jbd.h"



#include "fsck.h"

#include "ext2fs/ext2_fs.h"
#include "blkid/blkid.h"
#include "ext2fs/ext2_ext_attr.h"
#include "uuid/uuid.h"
#include "libbb.h"

#ifdef HAVE_CONIO_H
#undef HAVE_TERMIOS_H
#include <conio.h>
#define read_a_char()   getch()
#else
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#endif


/*
 * The last ext2fs revision level that this version of e2fsck is able to
 * support
 */
#define E2FSCK_CURRENT_REV      1

/* Used by the region allocation code */
typedef __u32 region_addr_t;
typedef struct region_struct *region_t;

struct dx_dirblock_info {
	int             type;
	blk_t           phys;
	int             flags;
	blk_t           parent;
	ext2_dirhash_t  min_hash;
	ext2_dirhash_t  max_hash;
	ext2_dirhash_t  node_min_hash;
	ext2_dirhash_t  node_max_hash;
};

/*
These defines are used in the type field of dx_dirblock_info
*/

#define DX_DIRBLOCK_ROOT        1
#define DX_DIRBLOCK_LEAF        2
#define DX_DIRBLOCK_NODE        3


/*
The following defines are used in the 'flags' field of a dx_dirblock_info
*/
#define DX_FLAG_REFERENCED      1
#define DX_FLAG_DUP_REF         2
#define DX_FLAG_FIRST           4
#define DX_FLAG_LAST            8

/*
 * E2fsck options
 */
#define E2F_OPT_READONLY        0x0001
#define E2F_OPT_PREEN           0x0002
#define E2F_OPT_YES             0x0004
#define E2F_OPT_NO              0x0008
#define E2F_OPT_TIME            0x0010
#define E2F_OPT_CHECKBLOCKS     0x0040
#define E2F_OPT_DEBUG           0x0080
#define E2F_OPT_FORCE           0x0100
#define E2F_OPT_WRITECHECK      0x0200
#define E2F_OPT_COMPRESS_DIRS   0x0400

/*
 * E2fsck flags
 */
#define E2F_FLAG_ABORT          0x0001 /* Abort signaled */
#define E2F_FLAG_CANCEL         0x0002 /* Cancel signaled */
#define E2F_FLAG_SIGNAL_MASK    0x0003
#define E2F_FLAG_RESTART        0x0004 /* Restart signaled */

#define E2F_FLAG_SETJMP_OK      0x0010 /* Setjmp valid for abort */

#define E2F_FLAG_PROG_BAR       0x0020 /* Progress bar on screen */
#define E2F_FLAG_PROG_SUPPRESS  0x0040 /* Progress suspended */
#define E2F_FLAG_JOURNAL_INODE  0x0080 /* Create a new ext3 journal inode */
#define E2F_FLAG_SB_SPECIFIED   0x0100 /* The superblock was explicitly
					* specified by the user */
#define E2F_FLAG_RESTARTED      0x0200 /* E2fsck has been restarted */
#define E2F_FLAG_RESIZE_INODE   0x0400 /* Request to recreate resize inode */


/*Don't know where these come from*/
#define READ 0
#define WRITE 1
#define cpu_to_be32(n) htonl(n)
#define be32_to_cpu(n) ntohl(n)

/*
 * We define a set of "latch groups"; these are problems which are
 * handled as a set.  The user answers once for a particular latch
 * group.
 */
#define PR_LATCH_MASK         0x0ff0 /* Latch mask */
#define PR_LATCH_BLOCK        0x0010 /* Latch for illegal blocks (pass 1) */
#define PR_LATCH_BBLOCK       0x0020 /* Latch for bad block inode blocks (pass 1) */
#define PR_LATCH_IBITMAP      0x0030 /* Latch for pass 5 inode bitmap proc. */
#define PR_LATCH_BBITMAP      0x0040 /* Latch for pass 5 inode bitmap proc. */
#define PR_LATCH_RELOC        0x0050 /* Latch for superblock relocate hint */
#define PR_LATCH_DBLOCK       0x0060 /* Latch for pass 1b dup block headers */
#define PR_LATCH_LOW_DTIME    0x0070 /* Latch for pass1 orphaned list refugees */
#define PR_LATCH_TOOBIG       0x0080 /* Latch for file to big errors */
#define PR_LATCH_OPTIMIZE_DIR 0x0090 /* Latch for optimize directories */

#define PR_LATCH(x)     ((((x) & PR_LATCH_MASK) >> 4) - 1)

/*
 * Latch group descriptor flags
 */
#define PRL_YES         0x0001  /* Answer yes */
#define PRL_NO          0x0002  /* Answer no */
#define PRL_LATCHED     0x0004  /* The latch group is latched */
#define PRL_SUPPRESS    0x0008  /* Suppress all latch group questions */

#define PRL_VARIABLE    0x000f  /* All the flags that need to be reset */

/*
 * Pre-Pass 1 errors
 */

#define PR_0_BB_NOT_GROUP       0x000001  /* Block bitmap not in group */
#define PR_0_IB_NOT_GROUP       0x000002  /* Inode bitmap not in group */
#define PR_0_ITABLE_NOT_GROUP   0x000003  /* Inode table not in group */
#define PR_0_SB_CORRUPT         0x000004  /* Superblock corrupt */
#define PR_0_FS_SIZE_WRONG      0x000005  /* Filesystem size is wrong */
#define PR_0_NO_FRAGMENTS       0x000006  /* Fragments not supported */
#define PR_0_BLOCKS_PER_GROUP   0x000007  /* Bad blocks_per_group */
#define PR_0_FIRST_DATA_BLOCK   0x000008  /* Bad first_data_block */
#define PR_0_ADD_UUID           0x000009  /* Adding UUID to filesystem */
#define PR_0_RELOCATE_HINT      0x00000A  /* Relocate hint */
#define PR_0_MISC_CORRUPT_SUPER 0x00000B  /* Miscellaneous superblock corruption */
#define PR_0_GETSIZE_ERROR      0x00000C  /* Error determing physical device size of filesystem */
#define PR_0_INODE_COUNT_WRONG  0x00000D  /* Inode count in the superblock incorrect */
#define PR_0_HURD_CLEAR_FILETYPE 0x00000E /* The Hurd does not support the filetype feature */
#define PR_0_JOURNAL_BAD_INODE  0x00000F  /* The Hurd does not support the filetype feature */
#define PR_0_JOURNAL_UNSUPP_MULTIFS 0x000010 /* The external journal has multiple filesystems (which we can't handle yet) */
#define PR_0_CANT_FIND_JOURNAL  0x000011  /* Can't find external journal */
#define PR_0_EXT_JOURNAL_BAD_SUPER 0x000012/* External journal has bad superblock */
#define PR_0_JOURNAL_BAD_UUID   0x000013  /* Superblock has a bad journal UUID */
#define PR_0_JOURNAL_UNSUPP_SUPER 0x000014 /* Journal has an unknown superblock type */
#define PR_0_JOURNAL_BAD_SUPER  0x000015  /* Journal superblock is corrupt */
#define PR_0_JOURNAL_HAS_JOURNAL 0x000016 /* Journal superblock is corrupt */
#define PR_0_JOURNAL_RECOVER_SET 0x000017 /* Superblock has recovery flag set but no journal */
#define PR_0_JOURNAL_RECOVERY_CLEAR 0x000018 /* Journal has data, but recovery flag is clear */
#define PR_0_JOURNAL_RESET_JOURNAL 0x000019 /* Ask if we should clear the journal */
#define PR_0_FS_REV_LEVEL       0x00001A  /* Filesystem revision is 0, but feature flags are set */
#define PR_0_ORPHAN_CLEAR_INODE             0x000020 /* Clearing orphan inode */
#define PR_0_ORPHAN_ILLEGAL_BLOCK_NUM       0x000021 /* Illegal block found in orphaned inode */
#define PR_0_ORPHAN_ALREADY_CLEARED_BLOCK   0x000022 /* Already cleared block found in orphaned inode */
#define PR_0_ORPHAN_ILLEGAL_HEAD_INODE      0x000023 /* Illegal orphan inode in superblock */
#define PR_0_ORPHAN_ILLEGAL_INODE           0x000024 /* Illegal inode in orphaned inode list */
#define PR_0_JOURNAL_UNSUPP_ROCOMPAT        0x000025 /* Journal has unsupported read-only feature - abort */
#define PR_0_JOURNAL_UNSUPP_INCOMPAT        0x000026 /* Journal has unsupported incompatible feature - abort */
#define PR_0_JOURNAL_UNSUPP_VERSION         0x000027 /* Journal has unsupported version number */
#define PR_0_MOVE_JOURNAL                   0x000028 /* Moving journal to hidden file */
#define PR_0_ERR_MOVE_JOURNAL               0x000029 /* Error moving journal */
#define PR_0_CLEAR_V2_JOURNAL               0x00002A /* Clearing V2 journal superblock */
#define PR_0_JOURNAL_RUN                    0x00002B /* Run journal anyway */
#define PR_0_JOURNAL_RUN_DEFAULT            0x00002C /* Run journal anyway by default */
#define PR_0_BACKUP_JNL                     0x00002D /* Backup journal inode blocks */
#define PR_0_NONZERO_RESERVED_GDT_BLOCKS    0x00002E /* Reserved blocks w/o resize_inode */
#define PR_0_CLEAR_RESIZE_INODE             0x00002F /* Resize_inode not enabled, but resize inode is non-zero */
#define PR_0_RESIZE_INODE_INVALID           0x000030 /* Resize inode invalid */

/*
 * Pass 1 errors
 */

#define PR_1_PASS_HEADER              0x010000  /* Pass 1: Checking inodes, blocks, and sizes */
#define PR_1_ROOT_NO_DIR              0x010001  /* Root directory is not an inode */
#define PR_1_ROOT_DTIME               0x010002  /* Root directory has dtime set */
#define PR_1_RESERVED_BAD_MODE        0x010003  /* Reserved inode has bad mode */
#define PR_1_ZERO_DTIME               0x010004  /* Deleted inode has zero dtime */
#define PR_1_SET_DTIME                0x010005  /* Inode in use, but dtime set */
#define PR_1_ZERO_LENGTH_DIR          0x010006  /* Zero-length directory */
#define PR_1_BB_CONFLICT              0x010007  /* Block bitmap conflicts with some other fs block */
#define PR_1_IB_CONFLICT              0x010008  /* Inode bitmap conflicts with some other fs block */
#define PR_1_ITABLE_CONFLICT          0x010009  /* Inode table conflicts with some other fs block */
#define PR_1_BB_BAD_BLOCK             0x01000A  /* Block bitmap is on a bad block */
#define PR_1_IB_BAD_BLOCK             0x01000B  /* Inode bitmap is on a bad block */
#define PR_1_BAD_I_SIZE               0x01000C  /* Inode has incorrect i_size */
#define PR_1_BAD_I_BLOCKS             0x01000D  /* Inode has incorrect i_blocks */
#define PR_1_ILLEGAL_BLOCK_NUM        0x01000E  /* Illegal block number in inode */
#define PR_1_BLOCK_OVERLAPS_METADATA  0x01000F  /* Block number overlaps fs metadata */
#define PR_1_INODE_BLOCK_LATCH        0x010010  /* Inode has illegal blocks (latch question) */
#define PR_1_TOO_MANY_BAD_BLOCKS      0x010011  /* Too many bad blocks in inode */
#define PR_1_BB_ILLEGAL_BLOCK_NUM     0x010012  /* Illegal block number in bad block inode */
#define PR_1_INODE_BBLOCK_LATCH       0x010013  /* Bad block inode has illegal blocks (latch question) */
#define PR_1_DUP_BLOCKS_PREENSTOP     0x010014  /* Duplicate or bad blocks in use! */
#define PR_1_BBINODE_BAD_METABLOCK    0x010015  /* Bad block used as bad block indirect block */
#define PR_1_BBINODE_BAD_METABLOCK_PROMPT 0x010016 /* Inconsistency can't be fixed prompt */
#define PR_1_BAD_PRIMARY_BLOCK        0x010017  /* Bad primary block */
#define PR_1_BAD_PRIMARY_BLOCK_PROMPT 0x010018  /* Bad primary block prompt */
#define PR_1_BAD_PRIMARY_SUPERBLOCK   0x010019  /* Bad primary superblock */
#define PR_1_BAD_PRIMARY_GROUP_DESCRIPTOR 0x01001A /* Bad primary block group descriptors */
#define PR_1_BAD_SUPERBLOCK           0x01001B  /* Bad superblock in group */
#define PR_1_BAD_GROUP_DESCRIPTORS    0x01001C  /* Bad block group descriptors in group */
#define PR_1_PROGERR_CLAIMED_BLOCK    0x01001D  /* Block claimed for no reason */
#define PR_1_RELOC_BLOCK_ALLOCATE     0x01001E  /* Error allocating blocks for relocating metadata */
#define PR_1_RELOC_MEMORY_ALLOCATE    0x01001F  /* Error allocating block buffer during relocation process */
#define PR_1_RELOC_FROM_TO            0x010020  /* Relocating metadata group information from X to Y */
#define PR_1_RELOC_TO                 0x010021  /* Relocating metatdata group information to X */
#define PR_1_RELOC_READ_ERR           0x010022  /* Block read error during relocation process */
#define PR_1_RELOC_WRITE_ERR          0x010023  /* Block write error during relocation process */
#define PR_1_ALLOCATE_IBITMAP_ERROR   0x010024  /* Error allocating inode bitmap */
#define PR_1_ALLOCATE_BBITMAP_ERROR   0x010025  /* Error allocating block bitmap */
#define PR_1_ALLOCATE_ICOUNT          0x010026  /* Error allocating icount structure */
#define PR_1_ALLOCATE_DBCOUNT         0x010027  /* Error allocating dbcount */
#define PR_1_ISCAN_ERROR              0x010028  /* Error while scanning inodes */
#define PR_1_BLOCK_ITERATE            0x010029  /* Error while iterating over blocks */
#define PR_1_ICOUNT_STORE             0x01002A  /* Error while storing inode count information */
#define PR_1_ADD_DBLOCK               0x01002B  /* Error while storing directory block information */
#define PR_1_READ_INODE               0x01002C  /* Error while reading inode (for clearing) */
#define PR_1_SUPPRESS_MESSAGES        0x01002D  /* Suppress messages prompt */
#define PR_1_SET_IMAGIC    0x01002F  /* Imagic flag set on an inode when filesystem doesn't support it */
#define PR_1_SET_IMMUTABLE            0x010030  /* Immutable flag set on a device or socket inode */
#define PR_1_COMPR_SET                0x010031  /* Compression flag set on a non-compressed filesystem */
#define PR_1_SET_NONZSIZE             0x010032  /* Non-zero size on device, fifo or socket inode */
#define PR_1_FS_REV_LEVEL             0x010033  /* Filesystem revision is 0, but feature flags are set */
#define PR_1_JOURNAL_INODE_NOT_CLEAR  0x010034  /* Journal inode not in use, needs clearing */
#define PR_1_JOURNAL_BAD_MODE         0x010035  /* Journal inode has wrong mode */
#define PR_1_LOW_DTIME                0x010036  /* Inode that was part of orphan linked list */
#define PR_1_ORPHAN_LIST_REFUGEES     0x010037  /* Latch question which asks how to deal with low dtime inodes */
#define PR_1_ALLOCATE_REFCOUNT        0x010038  /* Error allocating refcount structure */
#define PR_1_READ_EA_BLOCK            0x010039  /* Error reading Extended Attribute block */
#define PR_1_BAD_EA_BLOCK             0x01003A  /* Invalid Extended Attribute block */
#define PR_1_EXTATTR_READ_ABORT   0x01003B  /* Error reading Extended Attribute block while fixing refcount -- abort */
#define PR_1_EXTATTR_REFCOUNT         0x01003C  /* Extended attribute reference count incorrect */
#define PR_1_EXTATTR_WRITE            0x01003D  /* Error writing Extended Attribute block while fixing refcount */
#define PR_1_EA_MULTI_BLOCK           0x01003E  /* Multiple EA blocks not supported */
#define PR_1_EA_ALLOC_REGION          0x01003F  /* Error allocating EA region allocation structure */
#define PR_1_EA_ALLOC_COLLISION       0x010040  /* Error EA allocation collision */
#define PR_1_EA_BAD_NAME              0x010041  /* Bad extended attribute name */
#define PR_1_EA_BAD_VALUE             0x010042  /* Bad extended attribute value */
#define PR_1_INODE_TOOBIG             0x010043  /* Inode too big (latch question) */
#define PR_1_TOOBIG_DIR               0x010044  /* Directory too big */
#define PR_1_TOOBIG_REG               0x010045  /* Regular file too big */
#define PR_1_TOOBIG_SYMLINK           0x010046  /* Symlink too big */
#define PR_1_HTREE_SET                0x010047  /* INDEX_FL flag set on a non-HTREE filesystem */
#define PR_1_HTREE_NODIR              0x010048  /* INDEX_FL flag set on a non-directory */
#define PR_1_HTREE_BADROOT            0x010049  /* Invalid root node in HTREE directory */
#define PR_1_HTREE_HASHV              0x01004A  /* Unsupported hash version in HTREE directory */
#define PR_1_HTREE_INCOMPAT           0x01004B  /* Incompatible flag in HTREE root node */
#define PR_1_HTREE_DEPTH              0x01004C  /* HTREE too deep */
#define PR_1_BB_FS_BLOCK   0x01004D  /* Bad block has indirect block that conflicts with filesystem block */
#define PR_1_RESIZE_INODE_CREATE      0x01004E  /* Resize inode failed */
#define PR_1_EXTRA_ISIZE              0x01004F  /* inode->i_size is too long */
#define PR_1_ATTR_NAME_LEN            0x010050  /* attribute name is too long */
#define PR_1_ATTR_VALUE_OFFSET        0x010051  /* wrong EA value offset */
#define PR_1_ATTR_VALUE_BLOCK         0x010052  /* wrong EA blocknumber */
#define PR_1_ATTR_VALUE_SIZE          0x010053  /* wrong EA value size */
#define PR_1_ATTR_HASH                0x010054  /* wrong EA hash value */

/*
 * Pass 1b errors
 */

#define PR_1B_PASS_HEADER       0x011000  /* Pass 1B: Rescan for duplicate/bad blocks */
#define PR_1B_DUP_BLOCK_HEADER  0x011001  /* Duplicate/bad block(s) header */
#define PR_1B_DUP_BLOCK         0x011002  /* Duplicate/bad block(s) in inode */
#define PR_1B_DUP_BLOCK_END     0x011003  /* Duplicate/bad block(s) end */
#define PR_1B_ISCAN_ERROR       0x011004  /* Error while scanning inodes */
#define PR_1B_ALLOCATE_IBITMAP_ERROR 0x011005  /* Error allocating inode bitmap */
#define PR_1B_BLOCK_ITERATE     0x0110006  /* Error while iterating over blocks */
#define PR_1B_ADJ_EA_REFCOUNT   0x0110007  /* Error adjusting EA refcount */
#define PR_1C_PASS_HEADER       0x012000  /* Pass 1C: Scan directories for inodes with dup blocks. */
#define PR_1D_PASS_HEADER       0x013000  /* Pass 1D: Reconciling duplicate blocks */
#define PR_1D_DUP_FILE          0x013001  /* File has duplicate blocks */
#define PR_1D_DUP_FILE_LIST     0x013002  /* List of files sharing duplicate blocks */
#define PR_1D_SHARE_METADATA    0x013003  /* File sharing blocks with filesystem metadata  */
#define PR_1D_NUM_DUP_INODES    0x013004  /* Report of how many duplicate/bad inodes */
#define PR_1D_DUP_BLOCKS_DEALT  0x013005  /* Duplicated blocks already reassigned or cloned. */
#define PR_1D_CLONE_QUESTION    0x013006  /* Clone duplicate/bad blocks? */
#define PR_1D_DELETE_QUESTION   0x013007  /* Delete file? */
#define PR_1D_CLONE_ERROR       0x013008  /* Couldn't clone file (error) */

/*
 * Pass 2 errors
 */

#define PR_2_PASS_HEADER        0x020000  /* Pass 2: Checking directory structure */
#define PR_2_BAD_INODE_DOT      0x020001  /* Bad inode number for '.' */
#define PR_2_BAD_INO            0x020002  /* Directory entry has bad inode number */
#define PR_2_UNUSED_INODE       0x020003  /* Directory entry has deleted or unused inode */
#define PR_2_LINK_DOT           0x020004  /* Directry entry is link to '.' */
#define PR_2_BB_INODE           0x020005  /* Directory entry points to inode now located in a bad block */
#define PR_2_LINK_DIR           0x020006  /* Directory entry contains a link to a directory */
#define PR_2_LINK_ROOT          0x020007  /* Directory entry contains a link to the root directry */
#define PR_2_BAD_NAME           0x020008  /* Directory entry has illegal characters in its name */
#define PR_2_MISSING_DOT        0x020009  /* Missing '.' in directory inode */
#define PR_2_MISSING_DOT_DOT    0x02000A  /* Missing '..' in directory inode */
#define PR_2_1ST_NOT_DOT        0x02000B  /* First entry in directory inode doesn't contain '.' */
#define PR_2_2ND_NOT_DOT_DOT    0x02000C  /* Second entry in directory inode doesn't contain '..' */
#define PR_2_FADDR_ZERO         0x02000D  /* i_faddr should be zero */
#define PR_2_FILE_ACL_ZERO      0x02000E  /* i_file_acl should be zero */
#define PR_2_DIR_ACL_ZERO       0x02000F  /* i_dir_acl should be zero */
#define PR_2_FRAG_ZERO          0x020010  /* i_frag should be zero */
#define PR_2_FSIZE_ZERO         0x020011  /* i_fsize should be zero */
#define PR_2_BAD_MODE           0x020012  /* inode has bad mode */
#define PR_2_DIR_CORRUPTED      0x020013  /* directory corrupted */
#define PR_2_FILENAME_LONG      0x020014  /* filename too long */
#define PR_2_DIRECTORY_HOLE     0x020015  /* Directory inode has a missing block (hole) */
#define PR_2_DOT_NULL_TERM      0x020016  /* '.' is not NULL terminated */
#define PR_2_DOT_DOT_NULL_TERM  0x020017  /* '..' is not NULL terminated */
#define PR_2_BAD_CHAR_DEV       0x020018  /* Illegal character device in inode */
#define PR_2_BAD_BLOCK_DEV      0x020019  /* Illegal block device in inode */
#define PR_2_DUP_DOT            0x02001A  /* Duplicate '.' entry */
#define PR_2_DUP_DOT_DOT        0x02001B  /* Duplicate '..' entry */
#define PR_2_NO_DIRINFO         0x02001C  /* Internal error: couldn't find dir_info */
#define PR_2_FINAL_RECLEN       0x02001D  /* Final rec_len is wrong */
#define PR_2_ALLOCATE_ICOUNT    0x02001E  /* Error allocating icount structure */
#define PR_2_DBLIST_ITERATE     0x02001F  /* Error iterating over directory blocks */
#define PR_2_READ_DIRBLOCK      0x020020  /* Error reading directory block */
#define PR_2_WRITE_DIRBLOCK     0x020021  /* Error writing directory block */
#define PR_2_ALLOC_DIRBOCK      0x020022  /* Error allocating new directory block */
#define PR_2_DEALLOC_INODE      0x020023  /* Error deallocating inode */
#define PR_2_SPLIT_DOT          0x020024  /* Directory entry for '.' is big.  Split? */
#define PR_2_BAD_FIFO           0x020025  /* Illegal FIFO */
#define PR_2_BAD_SOCKET         0x020026  /* Illegal socket */
#define PR_2_SET_FILETYPE       0x020027  /* Directory filetype not set */
#define PR_2_BAD_FILETYPE       0x020028  /* Directory filetype incorrect */
#define PR_2_CLEAR_FILETYPE     0x020029  /* Directory filetype set when it shouldn't be */
#define PR_2_NULL_NAME          0x020030  /* Directory filename can't be zero-length  */
#define PR_2_INVALID_SYMLINK    0x020031  /* Invalid symlink */
#define PR_2_FILE_ACL_BAD       0x020032  /* i_file_acl (extended attribute) is bad */
#define PR_2_FEATURE_LARGE_FILES 0x020033  /* Filesystem contains large files, but has no such flag in sb */
#define PR_2_HTREE_NOTREF       0x020034  /* Node in HTREE directory not referenced */
#define PR_2_HTREE_DUPREF       0x020035  /* Node in HTREE directory referenced twice */
#define PR_2_HTREE_MIN_HASH     0x020036  /* Node in HTREE directory has bad min hash */
#define PR_2_HTREE_MAX_HASH     0x020037  /* Node in HTREE directory has bad max hash */
#define PR_2_HTREE_CLEAR        0x020038  /* Clear invalid HTREE directory */
#define PR_2_HTREE_BADBLK       0x02003A  /* Bad block in htree interior node */
#define PR_2_ADJ_EA_REFCOUNT    0x02003B  /* Error adjusting EA refcount */
#define PR_2_HTREE_BAD_ROOT     0x02003C  /* Invalid HTREE root node */
#define PR_2_HTREE_BAD_LIMIT    0x02003D  /* Invalid HTREE limit */
#define PR_2_HTREE_BAD_COUNT    0x02003E  /* Invalid HTREE count */
#define PR_2_HTREE_HASH_ORDER   0x02003F  /* HTREE interior node has out-of-order hashes in table */
#define PR_2_HTREE_BAD_DEPTH    0x020040  /* Node in HTREE directory has bad depth */
#define PR_2_DUPLICATE_DIRENT   0x020041  /* Duplicate directory entry found */
#define PR_2_NON_UNIQUE_FILE    0x020042  /* Non-unique filename found */
#define PR_2_REPORT_DUP_DIRENT  0x020043  /* Duplicate directory entry found */

/*
 * Pass 3 errors
 */

#define PR_3_PASS_HEADER            0x030000  /* Pass 3: Checking directory connectivity */
#define PR_3_NO_ROOT_INODE          0x030001  /* Root inode not allocated */
#define PR_3_EXPAND_LF_DIR          0x030002  /* No room in lost+found */
#define PR_3_UNCONNECTED_DIR        0x030003  /* Unconnected directory inode */
#define PR_3_NO_LF_DIR              0x030004  /* /lost+found not found */
#define PR_3_BAD_DOT_DOT            0x030005  /* .. entry is incorrect */
#define PR_3_NO_LPF                 0x030006  /* Bad or non-existent /lost+found.  Cannot reconnect */
#define PR_3_CANT_EXPAND_LPF        0x030007  /* Could not expand /lost+found */
#define PR_3_CANT_RECONNECT         0x030008  /* Could not reconnect inode */
#define PR_3_ERR_FIND_LPF           0x030009  /* Error while trying to find /lost+found */
#define PR_3_ERR_LPF_NEW_BLOCK      0x03000A  /* Error in ext2fs_new_block while creating /lost+found */
#define PR_3_ERR_LPF_NEW_INODE      0x03000B  /* Error in ext2fs_new_inode while creating /lost+found */
#define PR_3_ERR_LPF_NEW_DIR_BLOCK  0x03000C  /* Error in ext2fs_new_dir_block while creating /lost+found */
#define PR_3_ERR_LPF_WRITE_BLOCK    0x03000D  /* Error while writing directory block for /lost+found */
#define PR_3_ADJUST_INODE           0x03000E  /* Error while adjusting inode count */
#define PR_3_FIX_PARENT_ERR         0x03000F  /* Couldn't fix parent directory -- error */
#define PR_3_FIX_PARENT_NOFIND      0x030010  /* Couldn't fix parent directory -- couldn't find it */
#define PR_3_ALLOCATE_IBITMAP_ERROR 0x030011  /* Error allocating inode bitmap */
#define PR_3_CREATE_ROOT_ERROR      0x030012  /* Error creating root directory */
#define PR_3_CREATE_LPF_ERROR       0x030013  /* Error creating lost and found directory */
#define PR_3_ROOT_NOT_DIR_ABORT     0x030014  /* Root inode is not directory; aborting */
#define PR_3_NO_ROOT_INODE_ABORT    0x030015  /* Cannot proceed without a root inode. */
#define PR_3_NO_DIRINFO             0x030016  /* Internal error: couldn't find dir_info */
#define PR_3_LPF_NOTDIR             0x030017  /* Lost+found is not a directory */

/*
 * Pass 3a --- rehashing diretories
 */
#define PR_3A_PASS_HEADER         0x031000  /* Pass 3a: Reindexing directories */
#define PR_3A_OPTIMIZE_ITER       0x031001  /* Error iterating over directories */
#define PR_3A_OPTIMIZE_DIR_ERR    0x031002  /* Error rehash directory */
#define PR_3A_OPTIMIZE_DIR_HEADER 0x031003  /* Rehashing dir header */
#define PR_3A_OPTIMIZE_DIR        0x031004  /* Rehashing directory %d */
#define PR_3A_OPTIMIZE_DIR_END    0x031005  /* Rehashing dir end */

/*
 * Pass 4 errors
 */

#define PR_4_PASS_HEADER        0x040000  /* Pass 4: Checking reference counts */
#define PR_4_ZERO_LEN_INODE     0x040001  /* Unattached zero-length inode */
#define PR_4_UNATTACHED_INODE   0x040002  /* Unattached inode */
#define PR_4_BAD_REF_COUNT      0x040003  /* Inode ref count wrong */
#define PR_4_INCONSISTENT_COUNT 0x040004  /* Inconsistent inode count information cached */

/*
 * Pass 5 errors
 */

#define PR_5_PASS_HEADER            0x050000  /* Pass 5: Checking group summary information */
#define PR_5_INODE_BMAP_PADDING     0x050001  /* Padding at end of inode bitmap is not set. */
#define PR_5_BLOCK_BMAP_PADDING     0x050002  /* Padding at end of block bitmap is not set. */
#define PR_5_BLOCK_BITMAP_HEADER    0x050003  /* Block bitmap differences header */
#define PR_5_BLOCK_UNUSED           0x050004  /* Block not used, but marked in bitmap */
#define PR_5_BLOCK_USED             0x050005  /* Block used, but not marked used in bitmap */
#define PR_5_BLOCK_BITMAP_END       0x050006  /* Block bitmap differences end */
#define PR_5_INODE_BITMAP_HEADER    0x050007  /* Inode bitmap differences header */
#define PR_5_INODE_UNUSED           0x050008  /* Inode not used, but marked in bitmap */
#define PR_5_INODE_USED             0x050009  /* Inode used, but not marked used in bitmap */
#define PR_5_INODE_BITMAP_END       0x05000A  /* Inode bitmap differences end */
#define PR_5_FREE_INODE_COUNT_GROUP 0x05000B  /* Free inodes count for group wrong */
#define PR_5_FREE_DIR_COUNT_GROUP   0x05000C  /* Directories count for group wrong */
#define PR_5_FREE_INODE_COUNT       0x05000D  /* Free inodes count wrong */
#define PR_5_FREE_BLOCK_COUNT_GROUP 0x05000E  /* Free blocks count for group wrong */
#define PR_5_FREE_BLOCK_COUNT       0x05000F  /* Free blocks count wrong */
#define PR_5_BMAP_ENDPOINTS         0x050010  /* Programming error: bitmap endpoints don't match */
#define PR_5_FUDGE_BITMAP_ERROR     0x050011  /* Internal error: fudging end of bitmap */
#define PR_5_COPY_IBITMAP_ERROR     0x050012  /* Error copying in replacement inode bitmap */
#define PR_5_COPY_BBITMAP_ERROR     0x050013  /* Error copying in replacement block bitmap */
#define PR_5_BLOCK_RANGE_UNUSED     0x050014  /* Block range not used, but marked in bitmap */
#define PR_5_BLOCK_RANGE_USED       0x050015  /* Block range used, but not marked used in bitmap */
#define PR_5_INODE_RANGE_UNUSED     0x050016  /* Inode range not used, but marked in bitmap */
#define PR_5_INODE_RANGE_USED       0x050017  /* Inode rangeused, but not marked used in bitmap */


/*
 * The directory information structure; stores directory information
 * collected in earlier passes, to avoid disk i/o in fetching the
 * directory information.
 */
struct dir_info {
	ext2_ino_t              ino;    /* Inode number */
	ext2_ino_t              dotdot; /* Parent according to '..' */
	ext2_ino_t              parent; /* Parent according to treewalk */
};



/*
 * The indexed directory information structure; stores information for
 * directories which contain a hash tree index.
 */
struct dx_dir_info {
	ext2_ino_t              ino;            /* Inode number */
	int                     numblocks;      /* number of blocks */
	int                     hashversion;
	short                   depth;          /* depth of tree */
	struct dx_dirblock_info *dx_block;      /* Array of size numblocks */
};

/*
 * Define the extended attribute refcount structure
 */
typedef struct ea_refcount *ext2_refcount_t;

struct e2fsck_struct {
	ext2_filsys fs;
	const char *program_name;
	char *filesystem_name;
	char *device_name;
	char *io_options;
	int     flags;          /* E2fsck internal flags */
	int     options;
	blk_t   use_superblock; /* sb requested by user */
	blk_t   superblock;     /* sb used to open fs */
	int     blocksize;      /* blocksize */
	blk_t   num_blocks;     /* Total number of blocks */
	int     mount_flags;
	blkid_cache blkid;      /* blkid cache */

	jmp_buf abort_loc;

	unsigned long abort_code;

	int (*progress)(e2fsck_t ctx, int pass, unsigned long cur,
			unsigned long max);

	ext2fs_inode_bitmap inode_used_map; /* Inodes which are in use */
	ext2fs_inode_bitmap inode_bad_map; /* Inodes which are bad somehow */
	ext2fs_inode_bitmap inode_dir_map; /* Inodes which are directories */
	ext2fs_inode_bitmap inode_imagic_map; /* AFS inodes */
	ext2fs_inode_bitmap inode_reg_map; /* Inodes which are regular files*/

	ext2fs_block_bitmap block_found_map; /* Blocks which are in use */
	ext2fs_block_bitmap block_dup_map; /* Blks referenced more than once */
	ext2fs_block_bitmap block_ea_map; /* Blocks which are used by EA's */

	/*
	 * Inode count arrays
	 */
	ext2_icount_t   inode_count;
	ext2_icount_t inode_link_info;

	ext2_refcount_t refcount;
	ext2_refcount_t refcount_extra;

	/*
	 * Array of flags indicating whether an inode bitmap, block
	 * bitmap, or inode table is invalid
	 */
	int *invalid_inode_bitmap_flag;
	int *invalid_block_bitmap_flag;
	int *invalid_inode_table_flag;
	int invalid_bitmaps;    /* There are invalid bitmaps/itable */

	/*
	 * Block buffer
	 */
	char *block_buf;

	/*
	 * For pass1_check_directory and pass1_get_blocks
	 */
	ext2_ino_t stashed_ino;
	struct ext2_inode *stashed_inode;

	/*
	 * Location of the lost and found directory
	 */
	ext2_ino_t lost_and_found;
	int bad_lost_and_found;

	/*
	 * Directory information
	 */
	int             dir_info_count;
	int             dir_info_size;
	struct dir_info *dir_info;

	/*
	 * Indexed directory information
	 */
	int             dx_dir_info_count;
	int             dx_dir_info_size;
	struct dx_dir_info *dx_dir_info;

	/*
	 * Directories to hash
	 */
	ext2_u32_list   dirs_to_hash;

	/*
	 * Tuning parameters
	 */
	int process_inode_size;
	int inode_buffer_blocks;

	/*
	 * ext3 journal support
	 */
	io_channel      journal_io;
	char    *journal_name;

	/*
	 * How we display the progress update (for unix)
	 */
	int progress_fd;
	int progress_pos;
	int progress_last_percent;
	unsigned int progress_last_time;
	int interactive;        /* Are we connected directly to a tty? */
	char start_meta[2], stop_meta[2];

	/* File counts */
	int fs_directory_count;
	int fs_regular_count;
	int fs_blockdev_count;
	int fs_chardev_count;
	int fs_links_count;
	int fs_symlinks_count;
	int fs_fast_symlinks_count;
	int fs_fifo_count;
	int fs_total_count;
	int fs_sockets_count;
	int fs_ind_count;
	int fs_dind_count;
	int fs_tind_count;
	int fs_fragmented;
	int large_files;
	int fs_ext_attr_inodes;
	int fs_ext_attr_blocks;

	int ext_attr_ver;

	/*
	 * For the use of callers of the e2fsck functions; not used by
	 * e2fsck functions themselves.
	 */
	void *priv_data;
};


#define tid_gt(x, y)  ((x - y) > 0)

static inline int tid_geq(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference >= 0);
}
