/* vi: set sw=4 ts=4: */
/*
 * jfs_dat.h --- stripped down header file which only contains the JFS
 *	on-disk data structures
 */

#define JFS_MAGIC_NUMBER 0xc03b3998U /* The first 4 bytes of /dev/random! */

/*
 * On-disk structures
 */

/*
 * Descriptor block types:
 */

#define JFS_DESCRIPTOR_BLOCK	1
#define JFS_COMMIT_BLOCK	2
#define JFS_SUPERBLOCK		3

/*
 * Standard header for all descriptor blocks:
 */
typedef struct journal_header_s
{
	__u32		h_magic;
	__u32		h_blocktype;
	__u32		h_sequence;
} journal_header_t;


/*
 * The block tag: used to describe a single buffer in the journal
 */
typedef struct journal_block_tag_s
{
	__u32		t_blocknr;	/* The on-disk block number */
	__u32		t_flags;	/* See below */
} journal_block_tag_t;

/* Definitions for the journal tag flags word: */
#define JFS_FLAG_ESCAPE		1	/* on-disk block is escaped */
#define JFS_FLAG_SAME_UUID	2	/* block has same uuid as previous */
#define JFS_FLAG_DELETED	4	/* block deleted by this transaction */
#define JFS_FLAG_LAST_TAG	8	/* last tag in this descriptor block */


/*
 * The journal superblock
 */
typedef struct journal_superblock_s
{
	journal_header_t s_header;

	/* Static information describing the journal */
	__u32		s_blocksize;	/* journal device blocksize */
	__u32		s_maxlen;	/* total blocks in journal file */
	__u32		s_first;	/* first block of log information */

	/* Dynamic information describing the current state of the log */
	__u32		s_sequence;	/* first commit ID expected in log */
	__u32		s_start;	/* blocknr of start of log */

} journal_superblock_t;

