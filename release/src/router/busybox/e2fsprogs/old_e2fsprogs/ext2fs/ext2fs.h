/* vi: set sw=4 ts=4: */
/*
 * ext2fs.h --- ext2fs
 *
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */
#ifndef EXT2FS_EXT2FS_H
#define EXT2FS_EXT2FS_H 1


#define EXT2FS_ATTR(x)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Where the master copy of the superblock is located, and how big
 * superblocks are supposed to be.  We define SUPERBLOCK_SIZE because
 * the size of the superblock structure is not necessarily trustworthy
 * (some versions have the padding set up so that the superblock is
 * 1032 bytes long).
 */
#define SUPERBLOCK_OFFSET	1024
#define SUPERBLOCK_SIZE		1024

/*
 * The last ext2fs revision level that this version of the library is
 * able to support.
 */
#define EXT2_LIB_CURRENT_REV	EXT2_DYNAMIC_REV

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "ext2_types.h"
#include "ext2_fs.h"

typedef __u32		ext2_ino_t;
typedef __u32		blk_t;
typedef __u32		dgrp_t;
typedef __u32		ext2_off_t;
typedef __s64		e2_blkcnt_t;
typedef __u32		ext2_dirhash_t;

#include "ext2_io.h"
#include "ext2_err.h"

typedef struct struct_ext2_filsys *ext2_filsys;

struct ext2fs_struct_generic_bitmap {
	errcode_t	magic;
	ext2_filsys	fs;
	__u32		start, end;
	__u32		real_end;
	char	*	description;
	char	*	bitmap;
	errcode_t	base_error_code;
	__u32		reserved[7];
};

#define EXT2FS_MARK_ERROR	0
#define EXT2FS_UNMARK_ERROR	1
#define EXT2FS_TEST_ERROR	2

typedef struct ext2fs_struct_generic_bitmap *ext2fs_generic_bitmap;
typedef struct ext2fs_struct_generic_bitmap *ext2fs_inode_bitmap;
typedef struct ext2fs_struct_generic_bitmap *ext2fs_block_bitmap;

#define EXT2_FIRST_INODE(s)	EXT2_FIRST_INO(s)

/*
 * badblocks list definitions
 */

typedef struct ext2_struct_u32_list *ext2_badblocks_list;
typedef struct ext2_struct_u32_iterate *ext2_badblocks_iterate;

typedef struct ext2_struct_u32_list *ext2_u32_list;
typedef struct ext2_struct_u32_iterate *ext2_u32_iterate;

/* old */
typedef struct ext2_struct_u32_list *badblocks_list;
typedef struct ext2_struct_u32_iterate *badblocks_iterate;

#define BADBLOCKS_FLAG_DIRTY	1

/*
 * ext2_dblist structure and abstractions (see dblist.c)
 */
struct ext2_db_entry {
	ext2_ino_t	ino;
	blk_t	blk;
	int	blockcnt;
};

typedef struct ext2_struct_dblist *ext2_dblist;

#define DBLIST_ABORT	1

/*
 * ext2_fileio definitions
 */

#define EXT2_FILE_WRITE		0x0001
#define EXT2_FILE_CREATE	0x0002

#define EXT2_FILE_MASK		0x00FF

#define EXT2_FILE_BUF_DIRTY	0x4000
#define EXT2_FILE_BUF_VALID	0x2000

typedef struct ext2_file *ext2_file_t;

#define EXT2_SEEK_SET	0
#define EXT2_SEEK_CUR	1
#define EXT2_SEEK_END	2

/*
 * Flags for the ext2_filsys structure and for ext2fs_open()
 */
#define EXT2_FLAG_RW			0x01
#define EXT2_FLAG_CHANGED		0x02
#define EXT2_FLAG_DIRTY			0x04
#define EXT2_FLAG_VALID			0x08
#define EXT2_FLAG_IB_DIRTY		0x10
#define EXT2_FLAG_BB_DIRTY		0x20
#define EXT2_FLAG_SWAP_BYTES		0x40
#define EXT2_FLAG_SWAP_BYTES_READ	0x80
#define EXT2_FLAG_SWAP_BYTES_WRITE	0x100
#define EXT2_FLAG_MASTER_SB_ONLY	0x200
#define EXT2_FLAG_FORCE			0x400
#define EXT2_FLAG_SUPER_ONLY		0x800
#define EXT2_FLAG_JOURNAL_DEV_OK	0x1000
#define EXT2_FLAG_IMAGE_FILE		0x2000

/*
 * Special flag in the ext2 inode i_flag field that means that this is
 * a new inode.  (So that ext2_write_inode() can clear extra fields.)
 */
#define EXT2_NEW_INODE_FL	0x80000000

/*
 * Flags for mkjournal
 *
 * EXT2_MKJOURNAL_V1_SUPER	Make a (deprecated) V1 journal superblock
 */
#define EXT2_MKJOURNAL_V1_SUPER	0x0000001

struct struct_ext2_filsys {
	errcode_t			magic;
	io_channel			io;
	int				flags;
	char *				device_name;
	struct ext2_super_block	*	super;
	unsigned int			blocksize;
	int				fragsize;
	dgrp_t				group_desc_count;
	unsigned long			desc_blocks;
	struct ext2_group_desc *	group_desc;
	int				inode_blocks_per_group;
	ext2fs_inode_bitmap		inode_map;
	ext2fs_block_bitmap		block_map;
	errcode_t (*get_blocks)(ext2_filsys fs, ext2_ino_t ino, blk_t *blocks);
	errcode_t (*check_directory)(ext2_filsys fs, ext2_ino_t ino);
	errcode_t (*write_bitmaps)(ext2_filsys fs);
	errcode_t (*read_inode)(ext2_filsys fs, ext2_ino_t ino,
				struct ext2_inode *inode);
	errcode_t (*write_inode)(ext2_filsys fs, ext2_ino_t ino,
				struct ext2_inode *inode);
	ext2_badblocks_list		badblocks;
	ext2_dblist			dblist;
	__u32				stride;	/* for mke2fs */
	struct ext2_super_block *	orig_super;
	struct ext2_image_hdr *		image_header;
	__u32				umask;
	/*
	 * Reserved for future expansion
	 */
	__u32				reserved[8];

	/*
	 * Reserved for the use of the calling application.
	 */
	void *				priv_data;

	/*
	 * Inode cache
	 */
	struct ext2_inode_cache		*icache;
	io_channel			image_io;
};

#include "bitops.h"

/*
 * Return flags for the block iterator functions
 */
#define BLOCK_CHANGED	1
#define BLOCK_ABORT	2
#define BLOCK_ERROR	4

/*
 * Block interate flags
 *
 * BLOCK_FLAG_APPEND, or BLOCK_FLAG_HOLE, indicates that the interator
 * function should be called on blocks where the block number is zero.
 * This is used by ext2fs_expand_dir() to be able to add a new block
 * to an inode.  It can also be used for programs that want to be able
 * to deal with files that contain "holes".
 *
 * BLOCK_FLAG_TRAVERSE indicates that the iterator function for the
 * indirect, doubly indirect, etc. blocks should be called after all
 * of the blocks containined in the indirect blocks are processed.
 * This is useful if you are going to be deallocating blocks from an
 * inode.
 *
 * BLOCK_FLAG_DATA_ONLY indicates that the iterator function should be
 * called for data blocks only.
 *
 * BLOCK_FLAG_NO_LARGE is for internal use only.  It informs
 * ext2fs_block_iterate2 that large files won't be accepted.
 */
#define BLOCK_FLAG_APPEND	1
#define BLOCK_FLAG_HOLE		1
#define BLOCK_FLAG_DEPTH_TRAVERSE	2
#define BLOCK_FLAG_DATA_ONLY	4

#define BLOCK_FLAG_NO_LARGE	0x1000

/*
 * Magic "block count" return values for the block iterator function.
 */
#define BLOCK_COUNT_IND		(-1)
#define BLOCK_COUNT_DIND	(-2)
#define BLOCK_COUNT_TIND	(-3)
#define BLOCK_COUNT_TRANSLATOR	(-4)

#if 0
/*
 * Flags for ext2fs_move_blocks
 */
#define EXT2_BMOVE_GET_DBLIST	0x0001
#define EXT2_BMOVE_DEBUG	0x0002
#endif

/*
 * Flags for directory block reading and writing functions
 */
#define EXT2_DIRBLOCK_V2_STRUCT	0x0001

/*
 * Return flags for the directory iterator functions
 */
#define DIRENT_CHANGED	1
#define DIRENT_ABORT	2
#define DIRENT_ERROR	3

/*
 * Directory iterator flags
 */

#define DIRENT_FLAG_INCLUDE_EMPTY	1
#define DIRENT_FLAG_INCLUDE_REMOVED	2

#define DIRENT_DOT_FILE		1
#define DIRENT_DOT_DOT_FILE	2
#define DIRENT_OTHER_FILE	3
#define DIRENT_DELETED_FILE	4

/*
 * Inode scan definitions
 */
typedef struct ext2_struct_inode_scan *ext2_inode_scan;

/*
 * ext2fs_scan flags
 */
#define EXT2_SF_CHK_BADBLOCKS	0x0001
#define EXT2_SF_BAD_INODE_BLK	0x0002
#define EXT2_SF_BAD_EXTRA_BYTES	0x0004
#define EXT2_SF_SKIP_MISSING_ITABLE	0x0008

/*
 * ext2fs_check_if_mounted flags
 */
#define EXT2_MF_MOUNTED		1
#define EXT2_MF_ISROOT		2
#define EXT2_MF_READONLY	4
#define EXT2_MF_SWAP		8
#define EXT2_MF_BUSY		16

/*
 * Ext2/linux mode flags.  We define them here so that we don't need
 * to depend on the OS's sys/stat.h, since we may be compiling on a
 * non-Linux system.
 */
#define LINUX_S_IFMT  00170000
#define LINUX_S_IFSOCK 0140000
#define LINUX_S_IFLNK	 0120000
#define LINUX_S_IFREG  0100000
#define LINUX_S_IFBLK  0060000
#define LINUX_S_IFDIR  0040000
#define LINUX_S_IFCHR  0020000
#define LINUX_S_IFIFO  0010000
#define LINUX_S_ISUID  0004000
#define LINUX_S_ISGID  0002000
#define LINUX_S_ISVTX  0001000

#define LINUX_S_IRWXU 00700
#define LINUX_S_IRUSR 00400
#define LINUX_S_IWUSR 00200
#define LINUX_S_IXUSR 00100

#define LINUX_S_IRWXG 00070
#define LINUX_S_IRGRP 00040
#define LINUX_S_IWGRP 00020
#define LINUX_S_IXGRP 00010

#define LINUX_S_IRWXO 00007
#define LINUX_S_IROTH 00004
#define LINUX_S_IWOTH 00002
#define LINUX_S_IXOTH 00001

#define LINUX_S_ISLNK(m)	(((m) & LINUX_S_IFMT) == LINUX_S_IFLNK)
#define LINUX_S_ISREG(m)	(((m) & LINUX_S_IFMT) == LINUX_S_IFREG)
#define LINUX_S_ISDIR(m)	(((m) & LINUX_S_IFMT) == LINUX_S_IFDIR)
#define LINUX_S_ISCHR(m)	(((m) & LINUX_S_IFMT) == LINUX_S_IFCHR)
#define LINUX_S_ISBLK(m)	(((m) & LINUX_S_IFMT) == LINUX_S_IFBLK)
#define LINUX_S_ISFIFO(m)	(((m) & LINUX_S_IFMT) == LINUX_S_IFIFO)
#define LINUX_S_ISSOCK(m)	(((m) & LINUX_S_IFMT) == LINUX_S_IFSOCK)

/*
 * ext2 size of an inode
 */
#define EXT2_I_SIZE(i)	((i)->i_size | ((__u64) (i)->i_size_high << 32))

/*
 * ext2_icount_t abstraction
 */
#define EXT2_ICOUNT_OPT_INCREMENT	0x01

typedef struct ext2_icount *ext2_icount_t;

/*
 * Flags for ext2fs_bmap
 */
#define BMAP_ALLOC	0x0001
#define BMAP_SET	0x0002

/*
 * Flags for imager.c functions
 */
#define IMAGER_FLAG_INODEMAP	1
#define IMAGER_FLAG_SPARSEWRITE	2

/*
 * For checking structure magic numbers...
 */

#define EXT2_CHECK_MAGIC(struct, code) \
	  if ((struct)->magic != (code)) return (code)


/*
 * For ext2 compression support
 */
#define EXT2FS_COMPRESSED_BLKADDR ((blk_t) 0xffffffff)
#define HOLE_BLKADDR(_b) ((_b) == 0 || (_b) == EXT2FS_COMPRESSED_BLKADDR)

/*
 * Features supported by this version of the library
 */
#define EXT2_LIB_FEATURE_COMPAT_SUPP	(EXT2_FEATURE_COMPAT_DIR_PREALLOC|\
					 EXT2_FEATURE_COMPAT_IMAGIC_INODES|\
					 EXT3_FEATURE_COMPAT_HAS_JOURNAL|\
					 EXT2_FEATURE_COMPAT_RESIZE_INO|\
					 EXT2_FEATURE_COMPAT_DIR_INDEX|\
					 EXT2_FEATURE_COMPAT_EXT_ATTR)

/* This #ifdef is temporary until compression is fully supported */
#ifdef ENABLE_COMPRESSION
#ifndef I_KNOW_THAT_COMPRESSION_IS_EXPERIMENTAL
/* If the below warning bugs you, then have
   `CPPFLAGS=-DI_KNOW_THAT_COMPRESSION_IS_EXPERIMENTAL' in your
   environment at configure time. */
 #warning "Compression support is experimental"
#endif
#define EXT2_LIB_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE|\
					 EXT2_FEATURE_INCOMPAT_COMPRESSION|\
					 EXT3_FEATURE_INCOMPAT_JOURNAL_DEV|\
					 EXT2_FEATURE_INCOMPAT_META_BG|\
					 EXT3_FEATURE_INCOMPAT_RECOVER)
#else
#define EXT2_LIB_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE|\
					 EXT3_FEATURE_INCOMPAT_JOURNAL_DEV|\
					 EXT2_FEATURE_INCOMPAT_META_BG|\
					 EXT3_FEATURE_INCOMPAT_RECOVER)
#endif
#define EXT2_LIB_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER|\
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE)
/*
 * function prototypes
 */

/* alloc.c */
extern errcode_t ext2fs_new_inode(ext2_filsys fs, ext2_ino_t dir, int mode,
				  ext2fs_inode_bitmap map, ext2_ino_t *ret);
extern errcode_t ext2fs_new_block(ext2_filsys fs, blk_t goal,
				  ext2fs_block_bitmap map, blk_t *ret);
extern errcode_t ext2fs_get_free_blocks(ext2_filsys fs, blk_t start,
					blk_t finish, int num,
					ext2fs_block_bitmap map,
					blk_t *ret);
extern errcode_t ext2fs_alloc_block(ext2_filsys fs, blk_t goal,
				    char *block_buf, blk_t *ret);

/* alloc_sb.c */
extern int ext2fs_reserve_super_and_bgd(ext2_filsys fs,
					dgrp_t group,
					ext2fs_block_bitmap bmap);

/* alloc_stats.c */
void ext2fs_inode_alloc_stats(ext2_filsys fs, ext2_ino_t ino, int inuse);
void ext2fs_inode_alloc_stats2(ext2_filsys fs, ext2_ino_t ino,
			       int inuse, int isdir);
void ext2fs_block_alloc_stats(ext2_filsys fs, blk_t blk, int inuse);

/* alloc_tables.c */
extern errcode_t ext2fs_allocate_tables(ext2_filsys fs);
extern errcode_t ext2fs_allocate_group_table(ext2_filsys fs, dgrp_t group,
					     ext2fs_block_bitmap bmap);

/* badblocks.c */
extern errcode_t ext2fs_u32_list_create(ext2_u32_list *ret, int size);
extern errcode_t ext2fs_u32_list_add(ext2_u32_list bb, __u32 blk);
extern int ext2fs_u32_list_find(ext2_u32_list bb, __u32 blk);
extern int ext2fs_u32_list_test(ext2_u32_list bb, blk_t blk);
extern errcode_t ext2fs_u32_list_iterate_begin(ext2_u32_list bb,
					       ext2_u32_iterate *ret);
extern int ext2fs_u32_list_iterate(ext2_u32_iterate iter, blk_t *blk);
extern void ext2fs_u32_list_iterate_end(ext2_u32_iterate iter);
extern errcode_t ext2fs_u32_copy(ext2_u32_list src, ext2_u32_list *dest);
extern int ext2fs_u32_list_equal(ext2_u32_list bb1, ext2_u32_list bb2);

extern errcode_t ext2fs_badblocks_list_create(ext2_badblocks_list *ret,
					    int size);
extern errcode_t ext2fs_badblocks_list_add(ext2_badblocks_list bb,
					   blk_t blk);
extern int ext2fs_badblocks_list_test(ext2_badblocks_list bb,
				    blk_t blk);
extern int ext2fs_u32_list_del(ext2_u32_list bb, __u32 blk);
extern void ext2fs_badblocks_list_del(ext2_u32_list bb, __u32 blk);
extern errcode_t
	ext2fs_badblocks_list_iterate_begin(ext2_badblocks_list bb,
					    ext2_badblocks_iterate *ret);
extern int ext2fs_badblocks_list_iterate(ext2_badblocks_iterate iter,
					 blk_t *blk);
extern void ext2fs_badblocks_list_iterate_end(ext2_badblocks_iterate iter);
extern errcode_t ext2fs_badblocks_copy(ext2_badblocks_list src,
				       ext2_badblocks_list *dest);
extern int ext2fs_badblocks_equal(ext2_badblocks_list bb1,
				  ext2_badblocks_list bb2);
extern int ext2fs_u32_list_count(ext2_u32_list bb);

/* bb_compat */
extern errcode_t badblocks_list_create(badblocks_list *ret, int size);
extern errcode_t badblocks_list_add(badblocks_list bb, blk_t blk);
extern int badblocks_list_test(badblocks_list bb, blk_t blk);
extern errcode_t badblocks_list_iterate_begin(badblocks_list bb,
					      badblocks_iterate *ret);
extern int badblocks_list_iterate(badblocks_iterate iter, blk_t *blk);
extern void badblocks_list_iterate_end(badblocks_iterate iter);
extern void badblocks_list_free(badblocks_list bb);

/* bb_inode.c */
extern errcode_t ext2fs_update_bb_inode(ext2_filsys fs,
					ext2_badblocks_list bb_list);

/* bitmaps.c */
extern errcode_t ext2fs_write_inode_bitmap(ext2_filsys fs);
extern errcode_t ext2fs_write_block_bitmap (ext2_filsys fs);
extern errcode_t ext2fs_read_inode_bitmap (ext2_filsys fs);
extern errcode_t ext2fs_read_block_bitmap(ext2_filsys fs);
extern errcode_t ext2fs_allocate_generic_bitmap(__u32 start,
						__u32 end,
						__u32 real_end,
						const char *descr,
						ext2fs_generic_bitmap *ret);
extern errcode_t ext2fs_allocate_block_bitmap(ext2_filsys fs,
					      const char *descr,
					      ext2fs_block_bitmap *ret);
extern errcode_t ext2fs_allocate_inode_bitmap(ext2_filsys fs,
					      const char *descr,
					      ext2fs_inode_bitmap *ret);
extern errcode_t ext2fs_fudge_inode_bitmap_end(ext2fs_inode_bitmap bitmap,
					       ext2_ino_t end, ext2_ino_t *oend);
extern errcode_t ext2fs_fudge_block_bitmap_end(ext2fs_block_bitmap bitmap,
					       blk_t end, blk_t *oend);
extern void ext2fs_clear_inode_bitmap(ext2fs_inode_bitmap bitmap);
extern void ext2fs_clear_block_bitmap(ext2fs_block_bitmap bitmap);
extern errcode_t ext2fs_read_bitmaps(ext2_filsys fs);
extern errcode_t ext2fs_write_bitmaps(ext2_filsys fs);

/* block.c */
extern errcode_t ext2fs_block_iterate(ext2_filsys fs,
				      ext2_ino_t	ino,
				      int	flags,
				      char *block_buf,
				      int (*func)(ext2_filsys fs,
						  blk_t	*blocknr,
						  int	blockcnt,
						  void	*priv_data),
				      void *priv_data);
errcode_t ext2fs_block_iterate2(ext2_filsys fs,
				ext2_ino_t	ino,
				int	flags,
				char *block_buf,
				int (*func)(ext2_filsys fs,
					    blk_t	*blocknr,
					    e2_blkcnt_t	blockcnt,
					    blk_t	ref_blk,
					    int		ref_offset,
					    void	*priv_data),
				void *priv_data);

/* bmap.c */
extern errcode_t ext2fs_bmap(ext2_filsys fs, ext2_ino_t ino,
			     struct ext2_inode *inode,
			     char *block_buf, int bmap_flags,
			     blk_t block, blk_t *phys_blk);


#if 0
/* bmove.c */
extern errcode_t ext2fs_move_blocks(ext2_filsys fs,
				    ext2fs_block_bitmap reserve,
				    ext2fs_block_bitmap alloc_map,
				    int flags);
#endif

/* check_desc.c */
extern errcode_t ext2fs_check_desc(ext2_filsys fs);

/* closefs.c */
extern errcode_t ext2fs_close(ext2_filsys fs);
extern errcode_t ext2fs_flush(ext2_filsys fs);
extern int ext2fs_bg_has_super(ext2_filsys fs, int group_block);
extern int ext2fs_super_and_bgd_loc(ext2_filsys fs,
				    dgrp_t group,
				    blk_t *ret_super_blk,
				    blk_t *ret_old_desc_blk,
				    blk_t *ret_new_desc_blk,
				    int *ret_meta_bg);
extern void ext2fs_update_dynamic_rev(ext2_filsys fs);

/* cmp_bitmaps.c */
extern errcode_t ext2fs_compare_block_bitmap(ext2fs_block_bitmap bm1,
					     ext2fs_block_bitmap bm2);
extern errcode_t ext2fs_compare_inode_bitmap(ext2fs_inode_bitmap bm1,
					     ext2fs_inode_bitmap bm2);

/* dblist.c */

extern errcode_t ext2fs_get_num_dirs(ext2_filsys fs, ext2_ino_t *ret_num_dirs);
extern errcode_t ext2fs_init_dblist(ext2_filsys fs, ext2_dblist *ret_dblist);
extern errcode_t ext2fs_add_dir_block(ext2_dblist dblist, ext2_ino_t ino,
				      blk_t blk, int blockcnt);
extern void ext2fs_dblist_sort(ext2_dblist dblist,
			       int (*sortfunc)(const void *,
							   const void *));
extern errcode_t ext2fs_dblist_iterate(ext2_dblist dblist,
	int (*func)(ext2_filsys fs, struct ext2_db_entry *db_info,
		    void	*priv_data),
       void *priv_data);
extern errcode_t ext2fs_set_dir_block(ext2_dblist dblist, ext2_ino_t ino,
				      blk_t blk, int blockcnt);
extern errcode_t ext2fs_copy_dblist(ext2_dblist src,
				    ext2_dblist *dest);
extern int ext2fs_dblist_count(ext2_dblist dblist);

/* dblist_dir.c */
extern errcode_t
	ext2fs_dblist_dir_iterate(ext2_dblist dblist,
				  int	flags,
				  char	*block_buf,
				  int (*func)(ext2_ino_t	dir,
					      int		entry,
					      struct ext2_dir_entry *dirent,
					      int	offset,
					      int	blocksize,
					      char	*buf,
					      void	*priv_data),
				  void *priv_data);

/* dirblock.c */
extern errcode_t ext2fs_read_dir_block(ext2_filsys fs, blk_t block,
				       void *buf);
extern errcode_t ext2fs_read_dir_block2(ext2_filsys fs, blk_t block,
					void *buf, int flags);
extern errcode_t ext2fs_write_dir_block(ext2_filsys fs, blk_t block,
					void *buf);
extern errcode_t ext2fs_write_dir_block2(ext2_filsys fs, blk_t block,
					 void *buf, int flags);

/* dirhash.c */
extern errcode_t ext2fs_dirhash(int version, const char *name, int len,
				const __u32 *seed,
				ext2_dirhash_t *ret_hash,
				ext2_dirhash_t *ret_minor_hash);


/* dir_iterate.c */
extern errcode_t ext2fs_dir_iterate(ext2_filsys fs,
			      ext2_ino_t dir,
			      int flags,
			      char *block_buf,
			      int (*func)(struct ext2_dir_entry *dirent,
					  int	offset,
					  int	blocksize,
					  char	*buf,
					  void	*priv_data),
			      void *priv_data);
extern errcode_t ext2fs_dir_iterate2(ext2_filsys fs,
			      ext2_ino_t dir,
			      int flags,
			      char *block_buf,
			      int (*func)(ext2_ino_t	dir,
					  int	entry,
					  struct ext2_dir_entry *dirent,
					  int	offset,
					  int	blocksize,
					  char	*buf,
					  void	*priv_data),
			      void *priv_data);

/* dupfs.c */
extern errcode_t ext2fs_dup_handle(ext2_filsys src, ext2_filsys *dest);

/* expanddir.c */
extern errcode_t ext2fs_expand_dir(ext2_filsys fs, ext2_ino_t dir);

/* ext_attr.c */
extern errcode_t ext2fs_read_ext_attr(ext2_filsys fs, blk_t block, void *buf);
extern errcode_t ext2fs_write_ext_attr(ext2_filsys fs, blk_t block,
				       void *buf);
extern errcode_t ext2fs_adjust_ea_refcount(ext2_filsys fs, blk_t blk,
					   char *block_buf,
					   int adjust, __u32 *newcount);

/* fileio.c */
extern errcode_t ext2fs_file_open2(ext2_filsys fs, ext2_ino_t ino,
				   struct ext2_inode *inode,
				   int flags, ext2_file_t *ret);
extern errcode_t ext2fs_file_open(ext2_filsys fs, ext2_ino_t ino,
				  int flags, ext2_file_t *ret);
extern ext2_filsys ext2fs_file_get_fs(ext2_file_t file);
extern errcode_t ext2fs_file_close(ext2_file_t file);
extern errcode_t ext2fs_file_flush(ext2_file_t file);
extern errcode_t ext2fs_file_read(ext2_file_t file, void *buf,
				  unsigned int wanted, unsigned int *got);
extern errcode_t ext2fs_file_write(ext2_file_t file, const void *buf,
				   unsigned int nbytes, unsigned int *written);
extern errcode_t ext2fs_file_llseek(ext2_file_t file, __u64 offset,
				   int whence, __u64 *ret_pos);
extern errcode_t ext2fs_file_lseek(ext2_file_t file, ext2_off_t offset,
				   int whence, ext2_off_t *ret_pos);
errcode_t ext2fs_file_get_lsize(ext2_file_t file, __u64 *ret_size);
extern ext2_off_t ext2fs_file_get_size(ext2_file_t file);
extern errcode_t ext2fs_file_set_size(ext2_file_t file, ext2_off_t size);

/* finddev.c */
extern char *ext2fs_find_block_device(dev_t device);

/* flushb.c */
extern errcode_t ext2fs_sync_device(int fd, int flushb);

/* freefs.c */
extern void ext2fs_free(ext2_filsys fs);
extern void ext2fs_free_generic_bitmap(ext2fs_inode_bitmap bitmap);
extern void ext2fs_free_block_bitmap(ext2fs_block_bitmap bitmap);
extern void ext2fs_free_inode_bitmap(ext2fs_inode_bitmap bitmap);
extern void ext2fs_free_dblist(ext2_dblist dblist);
extern void ext2fs_badblocks_list_free(ext2_badblocks_list bb);
extern void ext2fs_u32_list_free(ext2_u32_list bb);

/* getsize.c */
extern errcode_t ext2fs_get_device_size(const char *file, int blocksize,
					blk_t *retblocks);

/* getsectsize.c */
errcode_t ext2fs_get_device_sectsize(const char *file, int *sectsize);

/* imager.c */
extern errcode_t ext2fs_image_inode_write(ext2_filsys fs, int fd, int flags);
extern errcode_t ext2fs_image_inode_read(ext2_filsys fs, int fd, int flags);
extern errcode_t ext2fs_image_super_write(ext2_filsys fs, int fd, int flags);
extern errcode_t ext2fs_image_super_read(ext2_filsys fs, int fd, int flags);
extern errcode_t ext2fs_image_bitmap_write(ext2_filsys fs, int fd, int flags);
extern errcode_t ext2fs_image_bitmap_read(ext2_filsys fs, int fd, int flags);

/* ind_block.c */
errcode_t ext2fs_read_ind_block(ext2_filsys fs, blk_t blk, void *buf);
errcode_t ext2fs_write_ind_block(ext2_filsys fs, blk_t blk, void *buf);

/* initialize.c */
extern errcode_t ext2fs_initialize(const char *name, int flags,
				   struct ext2_super_block *param,
				   io_manager manager, ext2_filsys *ret_fs);

/* icount.c */
extern void ext2fs_free_icount(ext2_icount_t icount);
extern errcode_t ext2fs_create_icount2(ext2_filsys fs, int flags,
				       unsigned int size,
				       ext2_icount_t hint, ext2_icount_t *ret);
extern errcode_t ext2fs_create_icount(ext2_filsys fs, int flags,
				      unsigned int size,
				      ext2_icount_t *ret);
extern errcode_t ext2fs_icount_fetch(ext2_icount_t icount, ext2_ino_t ino,
				     __u16 *ret);
extern errcode_t ext2fs_icount_increment(ext2_icount_t icount, ext2_ino_t ino,
					 __u16 *ret);
extern errcode_t ext2fs_icount_decrement(ext2_icount_t icount, ext2_ino_t ino,
					 __u16 *ret);
extern errcode_t ext2fs_icount_store(ext2_icount_t icount, ext2_ino_t ino,
				     __u16 count);
extern ext2_ino_t ext2fs_get_icount_size(ext2_icount_t icount);
errcode_t ext2fs_icount_validate(ext2_icount_t icount, FILE *);

/* inode.c */
extern errcode_t ext2fs_flush_icache(ext2_filsys fs);
extern errcode_t ext2fs_get_next_inode_full(ext2_inode_scan scan,
					    ext2_ino_t *ino,
					    struct ext2_inode *inode,
					    int bufsize);
extern errcode_t ext2fs_open_inode_scan(ext2_filsys fs, int buffer_blocks,
				  ext2_inode_scan *ret_scan);
extern void ext2fs_close_inode_scan(ext2_inode_scan scan);
extern errcode_t ext2fs_get_next_inode(ext2_inode_scan scan, ext2_ino_t *ino,
			       struct ext2_inode *inode);
extern errcode_t ext2fs_inode_scan_goto_blockgroup(ext2_inode_scan scan,
						   int	group);
extern void ext2fs_set_inode_callback
	(ext2_inode_scan scan,
	 errcode_t (*done_group)(ext2_filsys fs,
				 dgrp_t group,
				 void * priv_data),
	 void *done_group_data);
extern int ext2fs_inode_scan_flags(ext2_inode_scan scan, int set_flags,
				   int clear_flags);
extern errcode_t ext2fs_read_inode_full(ext2_filsys fs, ext2_ino_t ino,
					struct ext2_inode * inode,
					int bufsize);
extern errcode_t ext2fs_read_inode (ext2_filsys fs, ext2_ino_t ino,
			    struct ext2_inode * inode);
extern errcode_t ext2fs_write_inode_full(ext2_filsys fs, ext2_ino_t ino,
					 struct ext2_inode * inode,
					 int bufsize);
extern errcode_t ext2fs_write_inode(ext2_filsys fs, ext2_ino_t ino,
			    struct ext2_inode * inode);
extern errcode_t ext2fs_write_new_inode(ext2_filsys fs, ext2_ino_t ino,
			    struct ext2_inode * inode);
extern errcode_t ext2fs_get_blocks(ext2_filsys fs, ext2_ino_t ino, blk_t *blocks);
extern errcode_t ext2fs_check_directory(ext2_filsys fs, ext2_ino_t ino);

/* inode_io.c */
extern io_manager inode_io_manager;
extern errcode_t ext2fs_inode_io_intern(ext2_filsys fs, ext2_ino_t ino,
					char **name);
extern errcode_t ext2fs_inode_io_intern2(ext2_filsys fs, ext2_ino_t ino,
					 struct ext2_inode *inode,
					 char **name);

/* ismounted.c */
extern errcode_t ext2fs_check_if_mounted(const char *file, int *mount_flags);
extern errcode_t ext2fs_check_mount_point(const char *device, int *mount_flags,
					  char *mtpt, int mtlen);

/* namei.c */
extern errcode_t ext2fs_lookup(ext2_filsys fs, ext2_ino_t dir, const char *name,
			 int namelen, char *buf, ext2_ino_t *inode);
extern errcode_t ext2fs_namei(ext2_filsys fs, ext2_ino_t root, ext2_ino_t cwd,
			const char *name, ext2_ino_t *inode);
errcode_t ext2fs_namei_follow(ext2_filsys fs, ext2_ino_t root, ext2_ino_t cwd,
			      const char *name, ext2_ino_t *inode);
extern errcode_t ext2fs_follow_link(ext2_filsys fs, ext2_ino_t root, ext2_ino_t cwd,
			ext2_ino_t inode, ext2_ino_t *res_inode);

/* native.c */
int ext2fs_native_flag(void);

/* newdir.c */
extern errcode_t ext2fs_new_dir_block(ext2_filsys fs, ext2_ino_t dir_ino,
				ext2_ino_t parent_ino, char **block);

/* mkdir.c */
extern errcode_t ext2fs_mkdir(ext2_filsys fs, ext2_ino_t parent, ext2_ino_t inum,
			      const char *name);

/* mkjournal.c */
extern errcode_t ext2fs_create_journal_superblock(ext2_filsys fs,
						  __u32 size, int flags,
						  char  **ret_jsb);
extern errcode_t ext2fs_add_journal_device(ext2_filsys fs,
					   ext2_filsys journal_dev);
extern errcode_t ext2fs_add_journal_inode(ext2_filsys fs, blk_t size,
					  int flags);

/* openfs.c */
extern errcode_t ext2fs_open(const char *name, int flags, int superblock,
			     unsigned int block_size, io_manager manager,
			     ext2_filsys *ret_fs);
extern errcode_t ext2fs_open2(const char *name, const char *io_options,
			      int flags, int superblock,
			      unsigned int block_size, io_manager manager,
			      ext2_filsys *ret_fs);
extern blk_t ext2fs_descriptor_block_loc(ext2_filsys fs, blk_t group_block,
					 dgrp_t i);
errcode_t ext2fs_get_data_io(ext2_filsys fs, io_channel *old_io);
errcode_t ext2fs_set_data_io(ext2_filsys fs, io_channel new_io);
errcode_t ext2fs_rewrite_to_io(ext2_filsys fs, io_channel new_io);

/* get_pathname.c */
extern errcode_t ext2fs_get_pathname(ext2_filsys fs, ext2_ino_t dir, ext2_ino_t ino,
			       char **name);

/* link.c */
errcode_t ext2fs_link(ext2_filsys fs, ext2_ino_t dir, const char *name,
		      ext2_ino_t ino, int flags);
errcode_t ext2fs_unlink(ext2_filsys fs, ext2_ino_t dir, const char *name,
			ext2_ino_t ino, int flags);

/* read_bb.c */
extern errcode_t ext2fs_read_bb_inode(ext2_filsys fs,
				      ext2_badblocks_list *bb_list);

/* read_bb_file.c */
extern errcode_t ext2fs_read_bb_FILE2(ext2_filsys fs, FILE *f,
				      ext2_badblocks_list *bb_list,
				      void *priv_data,
				      void (*invalid)(ext2_filsys fs,
						      blk_t blk,
						      char *badstr,
						      void *priv_data));
extern errcode_t ext2fs_read_bb_FILE(ext2_filsys fs, FILE *f,
				     ext2_badblocks_list *bb_list,
				     void (*invalid)(ext2_filsys fs,
						     blk_t blk));

/* res_gdt.c */
extern errcode_t ext2fs_create_resize_inode(ext2_filsys fs);

/* rs_bitmap.c */
extern errcode_t ext2fs_resize_generic_bitmap(__u32 new_end,
					      __u32 new_real_end,
					      ext2fs_generic_bitmap bmap);
extern errcode_t ext2fs_resize_inode_bitmap(__u32 new_end, __u32 new_real_end,
					    ext2fs_inode_bitmap bmap);
extern errcode_t ext2fs_resize_block_bitmap(__u32 new_end, __u32 new_real_end,
					    ext2fs_block_bitmap bmap);
extern errcode_t ext2fs_copy_bitmap(ext2fs_generic_bitmap src,
				    ext2fs_generic_bitmap *dest);

/* swapfs.c */
extern void ext2fs_swap_ext_attr(char *to, char *from, int bufsize,
				 int has_header);
extern void ext2fs_swap_super(struct ext2_super_block * super);
extern void ext2fs_swap_group_desc(struct ext2_group_desc *gdp);
extern void ext2fs_swap_inode_full(ext2_filsys fs, struct ext2_inode_large *t,
				   struct ext2_inode_large *f, int hostorder,
				   int bufsize);
extern void ext2fs_swap_inode(ext2_filsys fs,struct ext2_inode *t,
			      struct ext2_inode *f, int hostorder);

/* valid_blk.c */
extern int ext2fs_inode_has_valid_blocks(struct ext2_inode *inode);

/* version.c */
extern int ext2fs_parse_version_string(const char *ver_string);
extern int ext2fs_get_library_version(const char **ver_string,
				      const char **date_string);

/* write_bb_file.c */
extern errcode_t ext2fs_write_bb_FILE(ext2_badblocks_list bb_list,
				      unsigned int flags,
				      FILE *f);


/* inline functions */
extern errcode_t ext2fs_get_mem(unsigned long size, void *ptr);
extern errcode_t ext2fs_free_mem(void *ptr);
extern errcode_t ext2fs_resize_mem(unsigned long old_size,
				   unsigned long size, void *ptr);
extern void ext2fs_mark_super_dirty(ext2_filsys fs);
extern void ext2fs_mark_changed(ext2_filsys fs);
extern int ext2fs_test_changed(ext2_filsys fs);
extern void ext2fs_mark_valid(ext2_filsys fs);
extern void ext2fs_unmark_valid(ext2_filsys fs);
extern int ext2fs_test_valid(ext2_filsys fs);
extern void ext2fs_mark_ib_dirty(ext2_filsys fs);
extern void ext2fs_mark_bb_dirty(ext2_filsys fs);
extern int ext2fs_test_ib_dirty(ext2_filsys fs);
extern int ext2fs_test_bb_dirty(ext2_filsys fs);
extern int ext2fs_group_of_blk(ext2_filsys fs, blk_t blk);
extern int ext2fs_group_of_ino(ext2_filsys fs, ext2_ino_t ino);
extern blk_t ext2fs_inode_data_blocks(ext2_filsys fs,
				      struct ext2_inode *inode);

#ifdef __cplusplus
}
#endif

#endif
