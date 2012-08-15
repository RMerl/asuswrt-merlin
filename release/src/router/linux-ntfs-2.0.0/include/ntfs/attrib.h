/*
 * attrib.h - Exports for attribute handling. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
 * Copyright (c) 2004-2007 Yura Pakhuchiy
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_ATTRIB_H
#define _NTFS_ATTRIB_H

/* Forward declarations */
typedef struct _ntfs_attr ntfs_attr;
typedef struct _ntfs_attr_search_ctx ntfs_attr_search_ctx;

#include "list.h"
#include "types.h"
#include "inode.h"
#include "unistr.h"
#include "runlist.h"
#include "volume.h"
#include "debug.h"
#include "logging.h"
#include "crypto.h"

extern ntfschar AT_UNNAMED[];

/**
 * enum ntfs_lcn_special_values - special return values for ntfs_*_vcn_to_lcn()
 *
 * Special return values for ntfs_rl_vcn_to_lcn() and ntfs_attr_vcn_to_lcn().
 *
 * TODO: Describe them.
 */
typedef enum {
	LCN_HOLE		= -1,	/* Keep this as highest value or die! */
	LCN_RL_NOT_MAPPED	= -2,
	LCN_ENOENT		= -3,
	LCN_EINVAL		= -4,
	LCN_EIO			= -5,
} ntfs_lcn_special_values;

/**
 * struct ntfs_attr_search_ctx - search context used in attribute search functions
 * @mrec:	buffer containing mft record to search
 * @attr:	attribute record in @mrec where to begin/continue search
 * @is_first:	if true lookup_attr() begins search with @attr, else after @attr
 *
 * Structure must be initialized to zero before the first call to one of the
 * attribute search functions. Initialize @mrec to point to the mft record to
 * search, and @attr to point to the first attribute within @mrec (not necessary
 * if calling the _first() functions), and set @is_first to TRUE (not necessary
 * if calling the _first() functions).
 *
 * If @is_first is TRUE, the search begins with @attr. If @is_first is FALSE,
 * the search begins after @attr. This is so that, after the first call to one
 * of the search attribute functions, we can call the function again, without
 * any modification of the search context, to automagically get the next
 * matching attribute.
 */
struct _ntfs_attr_search_ctx {
	MFT_RECORD *mrec;
	ATTR_RECORD *attr;
	BOOL is_first;
	ntfs_inode *ntfs_ino;
	ATTR_LIST_ENTRY *al_entry;
	ntfs_inode *base_ntfs_ino;
	MFT_RECORD *base_mrec;
	ATTR_RECORD *base_attr;
};

extern void ntfs_attr_reinit_search_ctx(ntfs_attr_search_ctx *ctx);
extern ntfs_attr_search_ctx *ntfs_attr_get_search_ctx(ntfs_inode *ni,
		MFT_RECORD *mrec);
extern void ntfs_attr_put_search_ctx(ntfs_attr_search_ctx *ctx);

extern int ntfs_attr_lookup(const ATTR_TYPES type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const VCN lowest_vcn, const u8 *val, const u32 val_len,
		ntfs_attr_search_ctx *ctx);

extern ATTR_DEF *ntfs_attr_find_in_attrdef(const ntfs_volume *vol,
		const ATTR_TYPES type);

/**
 * ntfs_attrs_walk - syntactic sugar for walking all attributes in an inode
 * @ctx:	initialised attribute search context
 *
 * Syntactic sugar for walking attributes in an inode.
 *
 * Return 0 on success and -1 on error with errno set to the error code from
 * ntfs_attr_lookup().
 *
 * Example: When you want to enumerate all attributes in an open ntfs inode
 *	    @ni, you can simply do:
 *
 *	int err;
 *	ntfs_attr_search_ctx *ctx = ntfs_attr_get_search_ctx(ni, NULL);
 *	if (!ctx)
 *		// Error code is in errno. Handle this case.
 *	while (!(err = ntfs_attrs_walk(ctx))) {
 *		ATTR_RECORD *attr = ctx->attr;
 *		// attr now contains the next attribute. Do whatever you want
 *		// with it and then just continue with the while loop.
 *	}
 *	if (err && errno != ENOENT)
 *		// Ooops. An error occurred! You should handle this case.
 *	// Now finished with all attributes in the inode.
 */
static __inline__ int ntfs_attrs_walk(ntfs_attr_search_ctx *ctx)
{
	return ntfs_attr_lookup(AT_UNUSED, NULL, 0, CASE_SENSITIVE, 0,
			NULL, 0, ctx);
}

/**
 * struct ntfs_attr - ntfs in memory non-resident attribute structure
 * @rl:			if not NULL, the decompressed runlist
 * @ni:			base ntfs inode to which this attribute belongs
 * @type:		attribute type
 * @name:		Unicode name of the attribute
 * @name_len:		length of @name in Unicode characters
 * @state:		NTFS attribute specific flags describing this attribute
 * @allocated_size:	copy from the attribute record
 * @data_size:		copy from the attribute record
 * @initialized_size:	copy from the attribute record
 * @compressed_size:	copy from the attribute record
 * @compression_block_size:		size of a compression block (cb)
 * @compression_block_size_bits:	log2 of the size of a cb
 * @compression_block_clusters:		number of clusters per cb
 * @crypto:		(valid only for encrypted) see description below
 *
 * This structure exists purely to provide a mechanism of caching the runlist
 * of an attribute. If you want to operate on a particular attribute extent,
 * you should not be using this structure at all. If you want to work with a
 * resident attribute, you should not be using this structure at all. As a
 * fail-safe check make sure to test NAttrNonResident() and if it is false, you
 * know you shouldn't be using this structure.
 *
 * If you want to work on a resident attribute or on a specific attribute
 * extent, you should use ntfs_lookup_attr() to retrieve the attribute (extent)
 * record, edit that, and then write back the mft record (or set the
 * corresponding ntfs inode dirty for delayed write back).
 *
 * @rl is the decompressed runlist of the attribute described by this
 * structure. Obviously this only makes sense if the attribute is not resident,
 * i.e. NAttrNonResident() is true. If the runlist hasn't been decompressed yet
 * @rl is NULL, so be prepared to cope with @rl == NULL.
 *
 * @ni is the base ntfs inode of the attribute described by this structure.
 *
 * @type is the attribute type (see layout.h for the definition of ATTR_TYPES),
 * @name and @name_len are the little endian Unicode name and the name length
 * in Unicode characters of the attribute, respectively.
 *
 * @state contains NTFS attribute specific flags describing this attribute
 * structure. See ntfs_attr_state_bits above.
 *
 * @crypto points to private structure of crypto code. You should not access
 * fields of this structure, but you can check whether it is NULL or not. If it
 * is not NULL, then we successfully obtained FEK (File Encryption Key) and
 * ntfs_attr_p{read,write} calls probably would succeed. If it is NULL, then we
 * failed to obtain FEK (do not have corresponding PFX file, wrong password,
 * etc..) or library was compiled without crypto support. Attribute size can be
 * changed without knowledge of FEK, so you can use ntfs_attr_truncate in any
 * case.
 * NOTE: This field valid only if attribute encrypted (eg., NAttrEncrypted
 * returns non-zero).
 */
struct _ntfs_attr {
	runlist_element *rl;
	ntfs_inode *ni;
	ATTR_TYPES type;
	ntfschar *name;
	u32 name_len;
	unsigned long state;
	s64 allocated_size;
	s64 data_size;
	s64 initialized_size;
	s64 compressed_size;
	u32 compression_block_size;
	u8 compression_block_size_bits;
	u8 compression_block_clusters;
	ntfs_crypto_attr *crypto;
	struct list_head list_entry;
	int nr_references;
};

/**
 * enum ntfs_attr_state_bits - bits for the state field in the ntfs_attr
 * structure
 */
typedef enum {
	NA_Initialized,		/* 1: structure is initialized. */
	NA_NonResident,		/* 1: Attribute is not resident. */
} ntfs_attr_state_bits;

#define  test_nattr_flag(na, flag)	 test_bit(NA_##flag, (na)->state)
#define   set_nattr_flag(na, flag)	  set_bit(NA_##flag, (na)->state)
#define clear_nattr_flag(na, flag)	clear_bit(NA_##flag, (na)->state)

#define NAttrInitialized(na)		 test_nattr_flag(na, Initialized)
#define NAttrSetInitialized(na)		  set_nattr_flag(na, Initialized)
#define NAttrClearInitialized(na)	clear_nattr_flag(na, Initialized)

#define NAttrNonResident(na)		 test_nattr_flag(na, NonResident)
#define NAttrSetNonResident(na)		  set_nattr_flag(na, NonResident)
#define NAttrClearNonResident(na)	clear_nattr_flag(na, NonResident)

#define GenNAttrIno(func_name,flag)					\
static inline int NAttr##func_name(ntfs_attr *na)			\
{									\
	if (na->type == AT_DATA && na->name == AT_UNNAMED)		\
		return (na->ni->flags & FILE_ATTR_##flag) ? 1 : 0;	\
	return 0;							\
}									\
static inline void NAttrSet##func_name(ntfs_attr *na)			\
{									\
	if (na->type == AT_DATA && na->name == AT_UNNAMED)		\
		na->ni->flags |= FILE_ATTR_##flag;			\
	else								\
		ntfs_log_trace("BUG! Should be called only for "	\
				"unnamed data attribute.\n");		\
}									\
static inline void NAttrClear##func_name(ntfs_attr *na)			\
{									\
	if (na->type == AT_DATA && na->name == AT_UNNAMED)		\
		na->ni->flags &= ~FILE_ATTR_##flag;			\
}

GenNAttrIno(Compressed, COMPRESSED)
GenNAttrIno(Encrypted, ENCRYPTED)
GenNAttrIno(Sparse, SPARSE_FILE)

/**
 * union attr_val - Union of all known attribute values
 *
 * For convenience. Used in the attr structure.
 */
typedef union {
	u8 _default;	/* Unnamed u8 to serve as default when just using
			   a_val without specifying any of the below. */
	STANDARD_INFORMATION std_inf;
	ATTR_LIST_ENTRY al_entry;
	FILE_NAME_ATTR filename;
	OBJECT_ID_ATTR obj_id;
	SECURITY_DESCRIPTOR_ATTR sec_desc;
	VOLUME_NAME vol_name;
	VOLUME_INFORMATION vol_inf;
	DATA_ATTR data;
	INDEX_ROOT index_root;
	INDEX_BLOCK index_blk;
	BITMAP_ATTR bmp;
	REPARSE_POINT reparse;
	EA_INFORMATION ea_inf;
	EA_ATTR ea;
	PROPERTY_SET property_set;
	LOGGED_UTILITY_STREAM logged_util_stream;
	EFS_ATTR_HEADER efs;
} attr_val;

extern void ntfs_attr_init(ntfs_attr *na, const BOOL non_resident,
		const BOOL compressed, const BOOL encrypted, const BOOL sparse,
		const s64 allocated_size, const s64 data_size,
		const s64 initialized_size, const s64 compressed_size,
		const u8 compression_unit);

extern ntfs_attr *ntfs_attr_open(ntfs_inode *ni, const ATTR_TYPES type,
		ntfschar *name, u32 name_len);
extern void ntfs_attr_close(ntfs_attr *na);

extern s64 ntfs_attr_pread(ntfs_attr *na, const s64 pos, s64 count,
		void *b);
extern s64 ntfs_attr_pwrite(ntfs_attr *na, const s64 pos, s64 count,
		const void *b);

extern void *ntfs_attr_readall(ntfs_inode *ni, const ATTR_TYPES type,
			       ntfschar *name, u32 name_len, s64 *data_size);

extern s64 ntfs_attr_mst_pread(ntfs_attr *na, const s64 pos,
		const s64 bk_cnt, const u32 bk_size, void *dst);
extern s64 ntfs_attr_mst_pwrite(ntfs_attr *na, const s64 pos,
		s64 bk_cnt, const u32 bk_size, void *src);

extern int ntfs_attr_map_runlist(ntfs_attr *na, VCN vcn);
extern int ntfs_attr_map_runlist_range(ntfs_attr *na, VCN from_vcn, VCN to_vcn);
extern int ntfs_attr_map_whole_runlist(ntfs_attr *na);

extern LCN ntfs_attr_vcn_to_lcn(ntfs_attr *na, const VCN vcn);
extern runlist_element *ntfs_attr_find_vcn(ntfs_attr *na, const VCN vcn);

extern int ntfs_attr_size_bounds_check(const ntfs_volume *vol,
		const ATTR_TYPES type, const s64 size);
extern int ntfs_attr_can_be_non_resident(const ntfs_volume *vol,
		const ATTR_TYPES type);
extern int ntfs_attr_can_be_resident(const ntfs_volume *vol,
		const ATTR_TYPES type);

extern int ntfs_make_room_for_attr(MFT_RECORD *m, u8 *pos, u32 size);

extern int ntfs_resident_attr_record_add(ntfs_inode *ni, ATTR_TYPES type,
		ntfschar *name, u8 name_len, u8 *val, u32 size,
		ATTR_FLAGS flags);
extern int ntfs_non_resident_attr_record_add(ntfs_inode *ni, ATTR_TYPES type,
		ntfschar *name, u8 name_len, VCN lowest_vcn, int dataruns_size,
		ATTR_FLAGS flags);
extern int ntfs_attr_record_rm(ntfs_attr_search_ctx *ctx);

extern int ntfs_attr_add(ntfs_inode *ni, ATTR_TYPES type,
		ntfschar *name, u8 name_len, u8 *val, s64 size);
extern int ntfs_attr_rm(ntfs_attr *na);

extern int ntfs_attr_record_resize(MFT_RECORD *m, ATTR_RECORD *a, u32 new_size);

extern int ntfs_resident_attr_value_resize(MFT_RECORD *m, ATTR_RECORD *a,
		const u32 new_size);

extern int ntfs_attr_record_move_to(ntfs_attr_search_ctx *ctx, ntfs_inode *ni);
extern int ntfs_attr_record_move_away(ntfs_attr_search_ctx *ctx, int extra);

extern int ntfs_attr_update_mapping_pairs(ntfs_attr *na, VCN from_vcn);

extern int __ntfs_attr_truncate(ntfs_attr *na, const s64 newsize, BOOL sparse);
extern int ntfs_attr_truncate(ntfs_attr *na, const s64 newsize);

extern int ntfs_attr_exist(ntfs_inode *ni, const ATTR_TYPES type,
		ntfschar *name, u32 name_len);

static __inline__ ntfschar *ntfs_attr_get_name(ATTR_RECORD *attr)
{
	return (ntfschar*)((u8*)attr + le16_to_cpu(attr->name_offset));
}

// FIXME / TODO: Above here the file is cleaned up. (AIA)
/**
 * get_attribute_value_length - return the length of the value of an attribute
 * @a:	pointer to a buffer containing the attribute record
 *
 * Return the byte size of the attribute value of the attribute @a (as it
 * would be after eventual decompression and filling in of holes if sparse).
 * If we return 0, check errno. If errno is 0 the actual length was 0,
 * otherwise errno describes the error.
 *
 * FIXME: Describe possible errnos.
 */
s64 ntfs_get_attribute_value_length(const ATTR_RECORD *a);

/**
 * get_attribute_value - return the attribute value of an attribute
 * @vol:	volume on which the attribute is present
 * @a:		attribute to get the value of
 * @b:		destination buffer for the attribute value
 *
 * Make a copy of the attribute value of the attribute @a into the destination
 * buffer @b. Note, that the size of @b has to be at least equal to the value
 * returned by get_attribute_value_length(@a).
 *
 * Return number of bytes copied. If this is zero check errno. If errno is 0
 * then nothing was read due to a zero-length attribute value, otherwise
 * errno describes the error.
 */
s64 ntfs_get_attribute_value(const ntfs_volume *vol, const ATTR_RECORD *a,
		u8 *b);

#endif /* defined _NTFS_ATTRIB_H */

