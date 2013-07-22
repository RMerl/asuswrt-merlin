/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2004  Ferenc Havasi <havasi@inf.u-szeged.hu>,
 *                     Zoltan Sogor <weth@inf.u-szeged.hu>,
 *                     Patrik Kluba <pajko@halom.u-szeged.hu>,
 *                     University of Szeged, Hungary
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 */

#ifndef JFFS2_SUMMARY_H
#define JFFS2_SUMMARY_H

#include <linux/uio.h>
#include <linux/jffs2.h>

#define DIRTY_SPACE(x) do { typeof(x) _x = (x); \
	c->free_size -= _x; c->dirty_size += _x; \
	jeb->free_size -= _x ; jeb->dirty_size += _x; \
}while(0)
#define USED_SPACE(x) do { typeof(x) _x = (x); \
	c->free_size -= _x; c->used_size += _x; \
	jeb->free_size -= _x ; jeb->used_size += _x; \
}while(0)
#define WASTED_SPACE(x) do { typeof(x) _x = (x); \
	c->free_size -= _x; c->wasted_size += _x; \
	jeb->free_size -= _x ; jeb->wasted_size += _x; \
}while(0)
#define UNCHECKED_SPACE(x) do { typeof(x) _x = (x); \
	c->free_size -= _x; c->unchecked_size += _x; \
	jeb->free_size -= _x ; jeb->unchecked_size += _x; \
}while(0)

#define BLK_STATE_ALLFF		0
#define BLK_STATE_CLEAN		1
#define BLK_STATE_PARTDIRTY	2
#define BLK_STATE_CLEANMARKER	3
#define BLK_STATE_ALLDIRTY	4
#define BLK_STATE_BADBLOCK	5

#define JFFS2_SUMMARY_NOSUM_SIZE 0xffffffff
#define JFFS2_SUMMARY_INODE_SIZE (sizeof(struct jffs2_sum_inode_flash))
#define JFFS2_SUMMARY_DIRENT_SIZE(x) (sizeof(struct jffs2_sum_dirent_flash) + (x))
#define JFFS2_SUMMARY_XATTR_SIZE (sizeof(struct jffs2_sum_xattr_flash))
#define JFFS2_SUMMARY_XREF_SIZE (sizeof(struct jffs2_sum_xref_flash))

/* Summary structures used on flash */

struct jffs2_sum_unknown_flash
{
	jint16_t nodetype;	/* node type */
} __attribute__((packed));

struct jffs2_sum_inode_flash
{
	jint16_t nodetype;	/* node type */
	jint32_t inode;		/* inode number */
	jint32_t version;	/* inode version */
	jint32_t offset;	/* offset on jeb */
	jint32_t totlen; 	/* record length */
} __attribute__((packed));

struct jffs2_sum_dirent_flash
{
	jint16_t nodetype;	/* == JFFS_NODETYPE_DIRENT */
	jint32_t totlen;	/* record length */
	jint32_t offset;	/* ofset on jeb */
	jint32_t pino;		/* parent inode */
	jint32_t version;	/* dirent version */
	jint32_t ino; 		/* == zero for unlink */
	uint8_t nsize;		/* dirent name size */
	uint8_t type;		/* dirent type */
	uint8_t name[0];	/* dirent name */
} __attribute__((packed));

struct jffs2_sum_xattr_flash
{
	jint16_t nodetype;	/* == JFFS2_NODETYPE_XATR */
	jint32_t xid;		/* xattr identifier */
	jint32_t version;	/* version number */
	jint32_t offset;	/* offset on jeb */
	jint32_t totlen;	/* node length */
} __attribute__((packed));

struct jffs2_sum_xref_flash
{
	jint16_t nodetype;	/* == JFFS2_NODETYPE_XREF */
	jint32_t offset;	/* offset on jeb */
} __attribute__((packed));

union jffs2_sum_flash
{
	struct jffs2_sum_unknown_flash u;
	struct jffs2_sum_inode_flash i;
	struct jffs2_sum_dirent_flash d;
	struct jffs2_sum_xattr_flash x;
	struct jffs2_sum_xref_flash r;
};

/* Summary structures used in the memory */

struct jffs2_sum_unknown_mem
{
	union jffs2_sum_mem *next;
	jint16_t nodetype;	/* node type */
} __attribute__((packed));

struct jffs2_sum_inode_mem
{
	union jffs2_sum_mem *next;
	jint16_t nodetype;	/* node type */
	jint32_t inode;		/* inode number */
	jint32_t version;	/* inode version */
	jint32_t offset;	/* offset on jeb */
	jint32_t totlen; 	/* record length */
} __attribute__((packed));

struct jffs2_sum_dirent_mem
{
	union jffs2_sum_mem *next;
	jint16_t nodetype;	/* == JFFS_NODETYPE_DIRENT */
	jint32_t totlen;	/* record length */
	jint32_t offset;	/* ofset on jeb */
	jint32_t pino;		/* parent inode */
	jint32_t version;	/* dirent version */
	jint32_t ino; 		/* == zero for unlink */
	uint8_t nsize;		/* dirent name size */
	uint8_t type;		/* dirent type */
	uint8_t name[0];	/* dirent name */
} __attribute__((packed));

struct jffs2_sum_xattr_mem
{
	union jffs2_sum_mem *next;
	jint16_t nodetype;
	jint32_t xid;
	jint32_t version;
	jint32_t offset;
	jint32_t totlen;
} __attribute__((packed));

struct jffs2_sum_xref_mem
{
	union jffs2_sum_mem *next;
	jint16_t nodetype;
	jint32_t offset;
} __attribute__((packed));

union jffs2_sum_mem
{
	struct jffs2_sum_unknown_mem u;
	struct jffs2_sum_inode_mem i;
	struct jffs2_sum_dirent_mem d;
	struct jffs2_sum_xattr_mem x;
	struct jffs2_sum_xref_mem r;
};

struct jffs2_summary
{
	uint32_t sum_size;
	uint32_t sum_num;
	uint32_t sum_padded;
	union jffs2_sum_mem *sum_list_head;
	union jffs2_sum_mem *sum_list_tail;
};

/* Summary marker is stored at the end of every sumarized erase block */

struct jffs2_sum_marker
{
	jint32_t offset;	/* offset of the summary node in the jeb */
	jint32_t magic; 	/* == JFFS2_SUM_MAGIC */
};

#define JFFS2_SUMMARY_FRAME_SIZE (sizeof(struct jffs2_raw_summary) + sizeof(struct jffs2_sum_marker))

#endif
