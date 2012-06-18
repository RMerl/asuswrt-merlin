/* vi: set sw=4 ts=4: */
/*
 * bitops.h --- Bitmap frobbing code.  The byte swapping routines are
 *	also included here.
 *
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 * i386 bitops operations taken from <asm/bitops.h>, Copyright 1992,
 * Linus Torvalds.
 */
#include <string.h>

extern int ext2fs_set_bit(unsigned int nr,void * addr);
extern int ext2fs_clear_bit(unsigned int nr, void * addr);
extern int ext2fs_test_bit(unsigned int nr, const void * addr);
extern __u16 ext2fs_swab16(__u16 val);
extern __u32 ext2fs_swab32(__u32 val);

#ifdef WORDS_BIGENDIAN
#define ext2fs_cpu_to_le32(x) ext2fs_swab32((x))
#define ext2fs_le32_to_cpu(x) ext2fs_swab32((x))
#define ext2fs_cpu_to_le16(x) ext2fs_swab16((x))
#define ext2fs_le16_to_cpu(x) ext2fs_swab16((x))
#define ext2fs_cpu_to_be32(x) ((__u32)(x))
#define ext2fs_be32_to_cpu(x) ((__u32)(x))
#define ext2fs_cpu_to_be16(x) ((__u16)(x))
#define ext2fs_be16_to_cpu(x) ((__u16)(x))
#else
#define ext2fs_cpu_to_le32(x) ((__u32)(x))
#define ext2fs_le32_to_cpu(x) ((__u32)(x))
#define ext2fs_cpu_to_le16(x) ((__u16)(x))
#define ext2fs_le16_to_cpu(x) ((__u16)(x))
#define ext2fs_cpu_to_be32(x) ext2fs_swab32((x))
#define ext2fs_be32_to_cpu(x) ext2fs_swab32((x))
#define ext2fs_cpu_to_be16(x) ext2fs_swab16((x))
#define ext2fs_be16_to_cpu(x) ext2fs_swab16((x))
#endif

/*
 * EXT2FS bitmap manipulation routines.
 */

/* Support for sending warning messages from the inline subroutines */
extern const char *ext2fs_block_string;
extern const char *ext2fs_inode_string;
extern const char *ext2fs_mark_string;
extern const char *ext2fs_unmark_string;
extern const char *ext2fs_test_string;
extern void ext2fs_warn_bitmap(errcode_t errcode, unsigned long arg,
			       const char *description);
extern void ext2fs_warn_bitmap2(ext2fs_generic_bitmap bitmap,
				int code, unsigned long arg);

extern int ext2fs_mark_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);
extern int ext2fs_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
				       blk_t block);
extern int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);

extern int ext2fs_mark_inode_bitmap(ext2fs_inode_bitmap bitmap, ext2_ino_t inode);
extern int ext2fs_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
				       ext2_ino_t inode);
extern int ext2fs_test_inode_bitmap(ext2fs_inode_bitmap bitmap, ext2_ino_t inode);

extern void ext2fs_fast_mark_block_bitmap(ext2fs_block_bitmap bitmap,
					  blk_t block);
extern void ext2fs_fast_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
					    blk_t block);
extern int ext2fs_fast_test_block_bitmap(ext2fs_block_bitmap bitmap,
					 blk_t block);

extern void ext2fs_fast_mark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					  ext2_ino_t inode);
extern void ext2fs_fast_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					    ext2_ino_t inode);
extern int ext2fs_fast_test_inode_bitmap(ext2fs_inode_bitmap bitmap,
					 ext2_ino_t inode);
extern blk_t ext2fs_get_block_bitmap_start(ext2fs_block_bitmap bitmap);
extern ext2_ino_t ext2fs_get_inode_bitmap_start(ext2fs_inode_bitmap bitmap);
extern blk_t ext2fs_get_block_bitmap_end(ext2fs_block_bitmap bitmap);
extern ext2_ino_t ext2fs_get_inode_bitmap_end(ext2fs_inode_bitmap bitmap);

extern void ext2fs_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					   blk_t block, int num);
extern void ext2fs_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					     blk_t block, int num);
extern int ext2fs_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
					  blk_t block, int num);
extern void ext2fs_fast_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						blk_t block, int num);
extern void ext2fs_fast_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						  blk_t block, int num);
extern int ext2fs_fast_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
					       blk_t block, int num);
extern void ext2fs_set_bitmap_padding(ext2fs_generic_bitmap map);

/* These two routines moved to gen_bitmap.c */
extern int ext2fs_mark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					 __u32 bitno);
extern int ext2fs_unmark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					   blk_t bitno);
