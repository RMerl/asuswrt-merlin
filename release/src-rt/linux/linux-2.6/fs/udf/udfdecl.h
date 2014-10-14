#ifndef __UDF_DECL_H
#define __UDF_DECL_H

#include "ecma_167.h"
#include "osta_udf.h"

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/buffer_head.h>
#include <linux/udf_fs_i.h>

#include "udf_sb.h"
#include "udfend.h"
#include "udf_i.h"

#define UDF_PREALLOCATE
#define UDF_DEFAULT_PREALLOC_BLOCKS	8

#undef UDFFS_DEBUG

#ifdef UDFFS_DEBUG
#define udf_debug(f, a...) \
do { \
	printk(KERN_DEBUG "UDF-fs DEBUG %s:%d:%s: ", \
		__FILE__, __LINE__, __func__); \
	printk(f, ##a); \
} while (0)
#else
#define udf_debug(f, a...) /**/
#endif

#define udf_info(f, a...) \
	printk(KERN_INFO "UDF-fs INFO " f, ##a);


#define udf_fixed_to_variable(x) ( ( ( (x) >> 5 ) * 39 ) + ( (x) & 0x0000001F ) )
#define udf_variable_to_fixed(x) ( ( ( (x) / 39 ) << 5 ) + ( (x) % 39 ) )

#define UDF_EXTENT_LENGTH_MASK	0x3FFFFFFF
#define UDF_EXTENT_FLAG_MASK	0xC0000000

#define UDF_NAME_PAD		4
#define UDF_NAME_LEN		256
#define UDF_PATH_LEN		1023

static inline size_t udf_file_entry_alloc_offset(struct inode *inode)
{
	struct udf_inode_info *iinfo = UDF_I(inode);
	if (iinfo->i_use)
		return sizeof(struct unallocSpaceEntry);
	else if (iinfo->i_efe)
		return sizeof(struct extendedFileEntry) + iinfo->i_lenEAttr;
	else
		return sizeof(struct fileEntry) + iinfo->i_lenEAttr;
}

static inline size_t udf_ext0_offset(struct inode *inode)
{
	if (UDF_I(inode)->i_alloc_type == ICBTAG_FLAG_AD_IN_ICB)
		return udf_file_entry_alloc_offset(inode);
	else
		return 0;
}

/* computes tag checksum */
u8 udf_tag_checksum(const struct tag *t);

struct dentry;
struct inode;
struct task_struct;
struct buffer_head;
struct super_block;

extern const struct export_operations udf_export_ops;
extern const struct inode_operations udf_dir_inode_operations;
extern const struct file_operations udf_dir_operations;
extern const struct inode_operations udf_file_inode_operations;
extern const struct file_operations udf_file_operations;
extern const struct inode_operations udf_symlink_inode_operations;
extern const struct address_space_operations udf_aops;
extern const struct address_space_operations udf_adinicb_aops;
extern const struct address_space_operations udf_symlink_aops;

struct udf_fileident_bh {
	struct buffer_head *sbh;
	struct buffer_head *ebh;
	int soffset;
	int eoffset;
};

struct udf_vds_record {
	uint32_t block;
	uint32_t volDescSeqNum;
};

struct generic_desc {
	struct tag	descTag;
	__le32		volDescSeqNum;
};

struct ustr {
	uint8_t u_cmpID;
	uint8_t u_name[UDF_NAME_LEN - 2];
	uint8_t u_len;
};

struct extent_position {
	struct buffer_head *bh;
	uint32_t offset;
	struct kernel_lb_addr block;
};

/* super.c */

__attribute__((format(printf, 3, 4)))
extern void udf_warning(struct super_block *, const char *, const char *, ...);
static inline void udf_updated_lvid(struct super_block *sb)
{
	struct buffer_head *bh = UDF_SB(sb)->s_lvid_bh;

	BUG_ON(!bh);
	WARN_ON_ONCE(((struct logicalVolIntegrityDesc *)
		     bh->b_data)->integrityType !=
		     cpu_to_le32(LVID_INTEGRITY_TYPE_OPEN));
	sb->s_dirt = 1;
	UDF_SB(sb)->s_lvid_dirty = 1;
}
extern u64 lvid_get_unique_id(struct super_block *sb);

/* namei.c */
extern int udf_write_fi(struct inode *inode, struct fileIdentDesc *,
			struct fileIdentDesc *, struct udf_fileident_bh *,
			uint8_t *, uint8_t *);

/* file.c */
extern long udf_ioctl(struct file *, unsigned int, unsigned long);
/* inode.c */
extern struct inode *udf_iget(struct super_block *, struct kernel_lb_addr *);
extern int udf_expand_file_adinicb(struct inode *);
extern struct buffer_head *udf_expand_dir_adinicb(struct inode *, int *, int *);
extern struct buffer_head *udf_bread(struct inode *, int, int, int *);
extern int udf_setsize(struct inode *, loff_t);
extern void udf_read_inode(struct inode *);
extern void udf_evict_inode(struct inode *);
extern int udf_write_inode(struct inode *, struct writeback_control *wbc);
extern long udf_block_map(struct inode *, sector_t);
extern int8_t inode_bmap(struct inode *, sector_t, struct extent_position *,
			 struct kernel_lb_addr *, uint32_t *, sector_t *);
extern int udf_add_aext(struct inode *, struct extent_position *,
			struct kernel_lb_addr *, uint32_t, int);
extern void udf_write_aext(struct inode *, struct extent_position *,
			   struct kernel_lb_addr *, uint32_t, int);
extern int8_t udf_delete_aext(struct inode *, struct extent_position,
			      struct kernel_lb_addr, uint32_t);
extern int8_t udf_next_aext(struct inode *, struct extent_position *,
			    struct kernel_lb_addr *, uint32_t *, int);
extern int8_t udf_current_aext(struct inode *, struct extent_position *,
			       struct kernel_lb_addr *, uint32_t *, int);

/* misc.c */
extern struct buffer_head *udf_tgetblk(struct super_block *, int);
extern struct buffer_head *udf_tread(struct super_block *, int);
extern struct genericFormat *udf_add_extendedattr(struct inode *, uint32_t,
						  uint32_t, uint8_t);
extern struct genericFormat *udf_get_extendedattr(struct inode *, uint32_t,
						  uint8_t);
extern struct buffer_head *udf_read_tagged(struct super_block *, uint32_t,
					   uint32_t, uint16_t *);
extern struct buffer_head *udf_read_ptagged(struct super_block *,
					    struct kernel_lb_addr *, uint32_t,
					    uint16_t *);
extern void udf_update_tag(char *, int);
extern void udf_new_tag(char *, uint16_t, uint16_t, uint16_t, uint32_t, int);

/* lowlevel.c */
extern unsigned int udf_get_last_session(struct super_block *);
extern unsigned long udf_get_last_block(struct super_block *);

/* partition.c */
extern uint32_t udf_get_pblock(struct super_block *, uint32_t, uint16_t,
			       uint32_t);
extern uint32_t udf_get_pblock_virt15(struct super_block *, uint32_t, uint16_t,
				      uint32_t);
extern uint32_t udf_get_pblock_virt20(struct super_block *, uint32_t, uint16_t,
				      uint32_t);
extern uint32_t udf_get_pblock_spar15(struct super_block *, uint32_t, uint16_t,
				      uint32_t);
extern uint32_t udf_get_pblock_meta25(struct super_block *, uint32_t, uint16_t,
					  uint32_t);
extern int udf_relocate_blocks(struct super_block *, long, long *);

static inline uint32_t
udf_get_lb_pblock(struct super_block *sb, struct kernel_lb_addr *loc,
		  uint32_t offset)
{
	return udf_get_pblock(sb, loc->logicalBlockNum,
			loc->partitionReferenceNum, offset);
}

/* unicode.c */
extern int udf_get_filename(struct super_block *, uint8_t *, uint8_t *, int);
extern int udf_put_filename(struct super_block *, const uint8_t *, uint8_t *,
			    int);
extern int udf_build_ustr(struct ustr *, dstring *, int);
extern int udf_CS0toUTF8(struct ustr *, const struct ustr *);

/* ialloc.c */
extern void udf_free_inode(struct inode *);
extern struct inode *udf_new_inode(struct inode *, int, int *);

/* truncate.c */
extern void udf_truncate_tail_extent(struct inode *);
extern void udf_discard_prealloc(struct inode *);
extern void udf_truncate_extents(struct inode *);

/* balloc.c */
extern void udf_free_blocks(struct super_block *, struct inode *,
			    struct kernel_lb_addr *, uint32_t, uint32_t);
extern int udf_prealloc_blocks(struct super_block *, struct inode *, uint16_t,
			       uint32_t, uint32_t);
extern int udf_new_block(struct super_block *, struct inode *, uint16_t,
			 uint32_t, int *);

/* directory.c */
extern struct fileIdentDesc *udf_fileident_read(struct inode *, loff_t *,
						struct udf_fileident_bh *,
						struct fileIdentDesc *,
						struct extent_position *,
						struct kernel_lb_addr *, uint32_t *,
						sector_t *);
extern struct fileIdentDesc *udf_get_fileident(void *buffer, int bufsize,
					       int *offset);
extern struct long_ad *udf_get_filelongad(uint8_t *, int, uint32_t *, int);
extern struct short_ad *udf_get_fileshortad(uint8_t *, int, uint32_t *, int);

/* udftime.c */
extern struct timespec *udf_disk_stamp_to_time(struct timespec *dest,
						struct timestamp src);
extern struct timestamp *udf_time_to_disk_stamp(struct timestamp *dest, struct timespec src);

#endif				/* __UDF_DECL_H */
