/*
 * blkidP.h - Internal interfaces for libblkid
 *
 * Copyright (C) 2001 Andreas Dilger
 * Copyright (C) 2003 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#ifndef _BLKID_BLKIDP_H
#define _BLKID_BLKIDP_H

#include <sys/types.h>
#include <stdio.h>

#include <blkid/blkid.h>

#include <blkid/list.h>

#ifdef __GNUC__
#define __BLKID_ATTR(x) __attribute__(x)
#else
#define __BLKID_ATTR(x)
#endif


/*
 * This describes the attributes of a specific device.
 * We can traverse all of the tags by bid_tags (linking to the tag bit_names).
 * The bid_label and bid_uuid fields are shortcuts to the LABEL and UUID tag
 * values, if they exist.
 */
struct blkid_struct_dev
{
	struct list_head	bid_devs;	/* All devices in the cache */
	struct list_head	bid_tags;	/* All tags for this device */
	blkid_cache		bid_cache;	/* Dev belongs to this cache */
	char			*bid_name;	/* Device inode pathname */
	char			*bid_type;	/* Preferred device TYPE */
	int			bid_pri;	/* Device priority */
	dev_t			bid_devno;	/* Device major/minor number */
	time_t			bid_time;	/* Last update time of device */
	unsigned int		bid_flags;	/* Device status bitflags */
	char			*bid_label;	/* Shortcut to device LABEL */
	char			*bid_uuid;	/* Shortcut to binary UUID */
};

#define BLKID_BID_FL_VERIFIED	0x0001	/* Device data validated from disk */
#define BLKID_BID_FL_INVALID	0x0004	/* Device is invalid */

/*
 * Each tag defines a NAME=value pair for a particular device.  The tags
 * are linked via bit_names for a single device, so that traversing the
 * names list will get you a list of all tags associated with a device.
 * They are also linked via bit_values for all devices, so one can easily
 * search all tags with a given NAME for a specific value.
 */
struct blkid_struct_tag
{
	struct list_head	bit_tags;	/* All tags for this device */
	struct list_head	bit_names;	/* All tags with given NAME */
	char			*bit_name;	/* NAME of tag (shared) */
	char			*bit_val;	/* value of tag */
	blkid_dev		bit_dev;	/* pointer to device */
};
typedef struct blkid_struct_tag *blkid_tag;

/*
 * Minimum number of seconds between device probes, even when reading
 * from the cache.  This is to avoid re-probing all devices which were
 * just probed by another program that does not share the cache.
 */
#define BLKID_PROBE_MIN		2

/*
 * Time in seconds an entry remains verified in the in-memory cache
 * before being reverified (in case of long-running processes that
 * keep a cache in memory and continue to use it for a long time).
 */
#define BLKID_PROBE_INTERVAL	200

/* This describes an entire blkid cache file and probed devices.
 * We can traverse all of the found devices via bic_list.
 * We can traverse all of the tag types by bic_tags, which hold empty tags
 * for each tag type.  Those tags can be used as list_heads for iterating
 * through all devices with a specific tag type (e.g. LABEL).
 */
struct blkid_struct_cache
{
	struct list_head	bic_devs;	/* List head of all devices */
	struct list_head	bic_tags;	/* List head of all tag types */
	time_t			bic_time;	/* Last probe time */
	time_t			bic_ftime; 	/* Mod time of the cachefile */
	unsigned int		bic_flags;	/* Status flags of the cache */
	char			*bic_filename;	/* filename of cache */
};

#define BLKID_BIC_FL_PROBED	0x0002	/* We probed /proc/partition devices */
#define BLKID_BIC_FL_CHANGED	0x0004	/* Cache has changed from disk */

extern char *blkid_strdup(const char *s);
extern char *blkid_strndup(const char *s, const int length);

#define BLKID_CACHE_FILE "/etc/blkid.tab"

#define BLKID_ERR_IO	 5
#define BLKID_ERR_PROC	 9
#define BLKID_ERR_MEM	12
#define BLKID_ERR_CACHE	14
#define BLKID_ERR_DEV	19
#define BLKID_ERR_PARAM	22
#define BLKID_ERR_BIG	27

/*
 * Priority settings for different types of devices
 */
#define BLKID_PRI_DM	40
#define BLKID_PRI_EVMS	30
#define BLKID_PRI_LVM	20
#define BLKID_PRI_MD	10

#if defined(TEST_PROGRAM) && !defined(CONFIG_BLKID_DEBUG)
#define CONFIG_BLKID_DEBUG
#endif

#define DEBUG_CACHE	0x0001
#define DEBUG_DUMP	0x0002
#define DEBUG_DEV	0x0004
#define DEBUG_DEVNAME	0x0008
#define DEBUG_DEVNO	0x0010
#define DEBUG_PROBE	0x0020
#define DEBUG_READ	0x0040
#define DEBUG_RESOLVE	0x0080
#define DEBUG_SAVE	0x0100
#define DEBUG_TAG	0x0200
#define DEBUG_INIT	0x8000
#define DEBUG_ALL	0xFFFF

#ifdef CONFIG_BLKID_DEBUG
#include <stdio.h>
extern int	blkid_debug_mask;
#define DBG(m,x)	if ((m) & blkid_debug_mask) x;
#else
#define DBG(m,x)
#endif

#ifdef CONFIG_BLKID_DEBUG
extern void blkid_debug_dump_dev(blkid_dev dev);
extern void blkid_debug_dump_tag(blkid_tag tag);
#endif

/* devno.c */
struct dir_list {
	char	*name;
	struct dir_list *next;
};
extern void blkid__scan_dir(char *, dev_t, struct dir_list **, char **);

/* lseek.c */
extern blkid_loff_t blkid_llseek(int fd, blkid_loff_t offset, int whence);

/* read.c */
extern void blkid_read_cache(blkid_cache cache);

/* save.c */
extern int blkid_flush_cache(blkid_cache cache);

/*
 * Functions to create and find a specific tag type: tag.c
 */
extern void blkid_free_tag(blkid_tag tag);
extern blkid_tag blkid_find_tag_dev(blkid_dev dev, const char *type);
extern int blkid_set_tag(blkid_dev dev, const char *name,
			 const char *value, const int vlength);

/*
 * Functions to create and find a specific tag type: dev.c
 */
extern blkid_dev blkid_new_dev(void);
extern void blkid_free_dev(blkid_dev dev);

#ifdef __cplusplus
}
#endif

#endif /* _BLKID_BLKIDP_H */
