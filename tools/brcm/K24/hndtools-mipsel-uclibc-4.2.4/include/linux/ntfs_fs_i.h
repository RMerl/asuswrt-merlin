#ifndef _LINUX_NTFS_FS_I_H
#define _LINUX_NTFS_FS_I_H

typedef enum {
	NTFS_AT_END = 0xffffffff
} NTFS_ATTR_TYPES;

typedef struct {
	void *rl;
	struct rw_semaphore lock;
} ntfs_run_list;

struct inode;

struct ntfs_inode_info {
	s64 initialized_size;	/* Copy from $DATA/$INDEX_ALLOCATION. */
	s64 allocated_size;	/* Copy from $DATA/$INDEX_ALLOCATION. */
	unsigned long state;	/* NTFS specific flags describing this inode.
				   See ntfs_inode_state_bits below. */
	unsigned long mft_no;	/* Number of the mft record / inode. */
	u16 seq_no;		/* Sequence number of the mft record. */
	atomic_t count;		/* Inode reference count for book keeping. */
	void *vol;		/* Pointer to the ntfs volume of this inode. */
	/*
	 * If NInoAttr() is true, the below fields describe the attribute which
	 * this fake inode belongs to. The actual inode of this attribute is
	 * pointed to by base_ntfs_ino and nr_extents is always set to -1 (see
	 * below). For real inodes, we also set the type (AT_DATA for files and
	 * AT_INDEX_ALLOCATION for directories), with the name = NULL and
	 * name_len = 0 for files and name = I30 (global constant) and
	 * name_len = 4 for directories.
	 */
	NTFS_ATTR_TYPES type;	/* Attribute type of this fake inode. */
	void *name;		/* Attribute name of this fake inode. */
	u32 name_len;		/* Attribute name length of this fake inode. */
	ntfs_run_list run_list;	/* If state has the NI_NonResident bit set,
				   the run list of the unnamed data attribute
				   (if a file) or of the index allocation
				   attribute (directory) or of the attribute
				   described by the fake inode (if NInoAttr()).
				   If run_list.rl is NULL, the run list has not
				   been read in yet or has been unmapped. If
				   NI_NonResident is clear, the attribute is
				   resident (file and fake inode) or there is
				   no $I30 index allocation attribute
				   (small directory). In the latter case
				   run_list.rl is always NULL.*/
	/*
	 * The following fields are only valid for real inodes and extent
	 * inodes.
	 */
	struct semaphore mrec_lock; /* Lock for serializing access to the
				   mft record belonging to this inode. */
	struct page *page;	/* The page containing the mft record of the
				   inode. This should only be touched by the
				   (un)map_mft_record*() functions. */
	int page_ofs;		/* Offset into the page at which the mft record
				   begins. This should only be touched by the
				   (un)map_mft_record*() functions. */
	/*
	 * Attribute list support (only for use by the attribute lookup
	 * functions). Setup during read_inode for all inodes with attribute
	 * lists. Only valid if NI_AttrList is set in state, and attr_list_rl is
	 * further only valid if NI_AttrListNonResident is set.
	 */
	u32 attr_list_size;	/* Length of attribute list value in bytes. */
	u8 *attr_list;		/* Attribute list value itself. */
	ntfs_run_list attr_list_rl; /* Run list for the attribute list value. */
	union {
		struct { /* It is a directory or $MFT. */
			struct inode *bmp_ino;	/* Attribute inode for the
						   directory index $BITMAP. */
			u32 index_block_size;	/* Size of an index block. */
			u32 index_vcn_size;	/* Size of a vcn in this
						   directory index. */
			u8 index_block_size_bits; /* Log2 of the above. */
			u8 index_vcn_size_bits;	/* Log2 of the above. */
		} m;
		struct { /* It is a compressed file or fake inode. */
			s64 compressed_size;		/* Copy from $DATA. */
			u32 compression_block_size;     /* Size of a compression
						           block (cb). */
			u8 compression_block_size_bits; /* Log2 of the size of
							   a cb. */
			u8 compression_block_clusters;  /* Number of clusters
							   per compression
							   block. */
		} f;
	} c;
	struct semaphore extent_lock;	/* Lock for accessing/modifying the
					   below . */
	s32 nr_extents;	/* For a base mft record, the number of attached extent
			   inodes (0 if none), for extent records and for fake
			   inodes describing an attribute this is -1. */
	union {		/* This union is only used if nr_extents != 0. */
		void **extent_ntfs_inos;	/* For nr_extents > 0, array of
						   the ntfs inodes of the extent
						   mft records belonging to
						   this base inode which have
						   been loaded. */
		void *base_ntfs_ino;		/* For nr_extents == -1, the
						   ntfs inode of the base mft
						   record. For fake inodes, the
						   real (base) inode to which
						   the attribute belongs. */
	} e;
};

#endif

