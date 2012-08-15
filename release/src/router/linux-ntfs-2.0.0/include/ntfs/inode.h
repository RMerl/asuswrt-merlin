/*
 * inode.h - Defines for NTFS inode handling. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001,2002 Anton Altaparmakov
 * Copyright (c) 2004-2007 Yura Pakhuchiy
 * Copyright (c) 2004-2005 Richard Russon
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

#ifndef _NTFS_INODE_H
#define _NTFS_INODE_H

/* Forward declaration */
typedef struct _ntfs_inode ntfs_inode;

#include "list.h"
#include "types.h"
#include "layout.h"
#include "support.h"
#include "volume.h"

/**
 * enum ntfs_inode_state_bits -
 *
 * Defined bits for the state field in the ntfs_inode structure.
 * (f) = files only, (d) = directories only
 */
typedef enum {
	NI_Dirty,		/* 1: Mft record needs to be written to disk. */

	/* Below fields only make sense for base inodes. */
	NI_AttrList,		/* 1: Mft record contains an attribute list. */
	NI_AttrListDirty,	/* 1: Attribute list needs to be written to the
				      mft record and then to disk. */
	NI_FileNameDirty,	/* 1: FILE_NAME attributes need to be updated
				      in the index. */
} ntfs_inode_state_bits;

#define  test_nino_flag(ni, flag)	   test_bit(NI_##flag, (ni)->state)
#define   set_nino_flag(ni, flag)	    set_bit(NI_##flag, (ni)->state)
#define clear_nino_flag(ni, flag)	  clear_bit(NI_##flag, (ni)->state)

#define test_and_set_nino_flag(ni, flag)	\
				   test_and_set_bit(NI_##flag, (ni)->state)
#define test_and_clear_nino_flag(ni, flag)	\
				 test_and_clear_bit(NI_##flag, (ni)->state)

#define NInoDirty(ni)				  test_nino_flag(ni, Dirty)
#define NInoSetDirty(ni)			   set_nino_flag(ni, Dirty)
#define NInoClearDirty(ni)			 clear_nino_flag(ni, Dirty)
#define NInoTestAndSetDirty(ni)		  test_and_set_nino_flag(ni, Dirty)
#define NInoTestAndClearDirty(ni)	test_and_clear_nino_flag(ni, Dirty)

#define NInoAttrList(ni)			  test_nino_flag(ni, AttrList)
#define NInoSetAttrList(ni)			   set_nino_flag(ni, AttrList)
#define NInoClearAttrList(ni)			 clear_nino_flag(ni, AttrList)


#define  test_nino_al_flag(ni, flag)	 test_nino_flag(ni, AttrList##flag)
#define   set_nino_al_flag(ni, flag)	  set_nino_flag(ni, AttrList##flag)
#define clear_nino_al_flag(ni, flag)	clear_nino_flag(ni, AttrList##flag)

#define test_and_set_nino_al_flag(ni, flag)	\
				 test_and_set_nino_flag(ni, AttrList##flag)
#define test_and_clear_nino_al_flag(ni, flag)	\
			       test_and_clear_nino_flag(ni, AttrList##flag)

#define NInoAttrListDirty(ni)			    test_nino_al_flag(ni, Dirty)
#define NInoAttrListSetDirty(ni)		     set_nino_al_flag(ni, Dirty)
#define NInoAttrListClearDirty(ni)		   clear_nino_al_flag(ni, Dirty)
#define NInoAttrListTestAndSetDirty(ni)	    test_and_set_nino_al_flag(ni, Dirty)
#define NInoAttrListTestAndClearDirty(ni) test_and_clear_nino_al_flag(ni, Dirty)

#define NInoFileNameDirty(ni)			\
					  test_nino_flag(ni, FileNameDirty)
#define NInoFileNameSetDirty(ni)		\
					   set_nino_flag(ni, FileNameDirty)
#define NInoFileNameClearDirty(ni)		\
					 clear_nino_flag(ni, FileNameDirty)
#define NInoFileNameTestAndSetDirty(ni)		\
				  test_and_set_nino_flag(ni, FileNameDirty)
#define NInoFileNameTestAndClearDirty(ni)	\
				test_and_clear_nino_flag(ni, FileNameDirty)

/**
 * struct _ntfs_inode - The NTFS in-memory inode structure.
 *
 * It is just used as an extension to the fields already provided in the VFS
 * inode.
 */
struct _ntfs_inode {
	u64 mft_no;		/* Inode / mft record number. */
	MFT_RECORD *mrec;	/* The actual mft record of the inode. */
	ntfs_volume *vol;	/* Pointer to the ntfs volume of this inode. */
	unsigned long state;	/* NTFS specific flags describing this inode.
				   See ntfs_inode_state_bits above. */
	FILE_ATTR_FLAGS flags;	/* Flags describing the file.
				   (Copy from STANDARD_INFORMATION) */
	/*
	 * Attribute list support (for use by the attribute lookup functions).
	 * Setup during ntfs_open_inode() for all inodes with attribute lists.
	 * Only valid if NI_AttrList is set in state.
	 */
	u32 attr_list_size;	/* Length of attribute list value in bytes. */
	u8 *attr_list;		/* Attribute list value itself. */
	/* Below fields are always valid. */
	s32 nr_extents;		/* For a base mft record, the number of
				   attached extent inodes (0 if none), for
				   extent records this is -1. */
	union {		/* This union is only used if nr_extents != 0. */
		ntfs_inode **extent_nis;/* For nr_extents > 0, array of the
					   ntfs inodes of the extent mft
					   records belonging to this base
					   inode which have been loaded. */
		ntfs_inode *base_ni;	/* For nr_extents == -1, the ntfs
					   inode of the base mft record. */
	};

	/* Below fields are valid only for base inode. */

	/*
	 * These two fields are used to sync filename index and guaranteed to be
	 * correct, however value in index itself maybe wrong (windows itself
	 * do not update them properly).
	 */
	s64 data_size;		/* Data size of unnamed DATA attribute. */
	s64 allocated_size;	/* Allocated size stored in the filename
				   index. (NOTE: Equal to allocated size of
				   the unnamed data attribute for normal or
				   encrypted files and to compressed size
				   of the unnamed data attribute for sparse or
				   compressed files.) */

	/*
	 * These four fields are copy of relevant fields from
	 * STANDARD_INFORMATION attribute and used to sync it and FILE_NAME
	 * attribute in the index.
	 */
	time_t creation_time;
	time_t last_data_change_time;
	time_t last_mft_change_time;
	time_t last_access_time;

	/* These 2 fields are used to keep track of opened inodes. */
	struct list_head list_entry;	/* Keep pointers to the next/prev list
					   entry. */
	int nr_references;		/* How many times this inode was
					   opened.  We really close inode only
					   when this reaches zero. */

	struct list_head attr_cache;	/* List of opened attributes. */
};

extern void __ntfs_inode_add_to_cache(ntfs_inode *ni);

extern ntfs_inode *ntfs_inode_allocate(ntfs_volume *vol);

extern ntfs_inode *ntfs_inode_open(ntfs_volume *vol, const MFT_REF mref);

extern int ntfs_inode_close(ntfs_inode *ni);

extern ntfs_inode *ntfs_extent_inode_open(ntfs_inode *base_ni,
		const leMFT_REF mref);

extern int ntfs_inode_attach_all_extents(ntfs_inode *ni);

/**
 * ntfs_inode_mark_dirty - set the inode (and its base inode if it exists) dirty
 * @ni:		ntfs inode to set dirty
 *
 * Set the inode @ni dirty so it is written out later (at the latest at
 * ntfs_inode_close() time). If @ni is an extent inode, set the base inode
 * dirty, too.
 *
 * This function cannot fail.
 */
static __inline__ void ntfs_inode_mark_dirty(ntfs_inode *ni)
{
	NInoSetDirty(ni);
	if (ni->nr_extents == -1)
		NInoSetDirty(ni->base_ni);
}

typedef enum {
	NTFS_UPDATE_ATIME = 1 << 0,
	NTFS_UPDATE_MTIME = 1 << 1,
	NTFS_UPDATE_CTIME = 1 << 2,
} ntfs_time_update_flags;

extern void ntfs_inode_update_times(ntfs_inode *ni,
		ntfs_time_update_flags mask);

extern int ntfs_inode_sync(ntfs_inode *ni);

extern int ntfs_inode_add_attrlist(ntfs_inode *ni);

extern int ntfs_inode_free_space(ntfs_inode *ni, int size);

extern int ntfs_inode_badclus_bad(u64 mft_no, ATTR_RECORD *a);

#endif /* defined _NTFS_INODE_H */
