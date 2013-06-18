/*
 * Definitions of structures for vfsv0 quota format
 */

#ifndef _LINUX_QUOTA_TREE_H
#define _LINUX_QUOTA_TREE_H

#include <sys/types.h>

typedef u_int32_t qid_t;        /* Type in which we store ids in memory */

#define QT_TREEOFF	1	/* Offset of tree in file in blocks */
#define QT_TREEDEPTH	4	/* Depth of quota tree */
#define QT_BLKSIZE_BITS	10
#define QT_BLKSIZE (1 << QT_BLKSIZE_BITS)	/* Size of block with quota
						 * structures */

/*
 *  Structure of header of block with quota structures. It is padded to 16 bytes
 *  so there will be space for exactly 21 quota-entries in a block
 */
struct qt_disk_dqdbheader {
	u_int32_t dqdh_next_free;	/* Number of next block with free
					 * entry */
	u_int32_t dqdh_prev_free; /* Number of previous block with free
				   * entry */
	u_int16_t dqdh_entries; /* Number of valid entries in block */
	u_int16_t dqdh_pad1;
	u_int32_t dqdh_pad2;
} __attribute__ ((packed));

struct dquot;
struct quota_handle;

/* Operations */
struct qtree_fmt_operations {
	/* Convert given entry from in memory format to disk one */
	void (*mem2disk_dqblk)(void *disk, struct dquot *dquot);
	/* Convert given entry from disk format to in memory one */
	void (*disk2mem_dqblk)(struct dquot *dquot, void *disk);
	/* Is this structure for given id? */
	int (*is_id)(void *disk, struct dquot *dquot);
};

/* Inmemory copy of version specific information */
struct qtree_mem_dqinfo {
	unsigned int dqi_blocks;	/* # of blocks in quota file */
	unsigned int dqi_free_blk;	/* First block in list of free blocks */
	unsigned int dqi_free_entry;	/* First block with free entry */
	unsigned int dqi_entry_size;	/* Size of quota entry in quota file */
	struct qtree_fmt_operations *dqi_ops;	/* Operations for entry
						 * manipulation */
};

void qtree_write_dquot(struct dquot *dquot);
struct dquot *qtree_read_dquot(struct quota_handle *h, qid_t id);
void qtree_delete_dquot(struct dquot *dquot);
int qtree_entry_unused(struct qtree_mem_dqinfo *info, char *disk);
int qtree_scan_dquots(struct quota_handle *h,
		int (*process_dquot) (struct dquot *, void *), void *data);

int qtree_dqstr_in_blk(struct qtree_mem_dqinfo *info);

#endif /* _LINUX_QUOTAIO_TREE_H */
