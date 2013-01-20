/*
 * blkid.h - Interface for libblkid, a library to identify block devices
 *
 * Copyright (C) 2001 Andreas Dilger
 * Copyright (C) 2003 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#ifndef _BLKID_BLKID_H
#define _BLKID_BLKID_H

#include <sys/types.h>
#include <blkid/blkid_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLKID_VERSION	"1.0.0"
#define BLKID_DATE	"12-Feb-2003"

typedef struct blkid_struct_dev *blkid_dev;
typedef struct blkid_struct_cache *blkid_cache;
typedef __s64 blkid_loff_t;

typedef struct blkid_struct_tag_iterate *blkid_tag_iterate;
typedef struct blkid_struct_dev_iterate *blkid_dev_iterate;

/*
 * Flags for blkid_get_dev
 *
 * BLKID_DEV_CREATE	Create an empty device structure if not found
 * 			in the cache.
 * BLKID_DEV_VERIFY	Make sure the device structure corresponds
 * 			with reality.
 * BLKID_DEV_FIND	Just look up a device entry, and return NULL
 * 			if it is not found.
 * BLKID_DEV_NORMAL	Get a valid device structure, either from the
 * 			cache or by probing the device.
 */
#define BLKID_DEV_FIND		0x0000
#define BLKID_DEV_CREATE	0x0001
#define BLKID_DEV_VERIFY	0x0002
#define BLKID_DEV_NORMAL	(BLKID_DEV_CREATE | BLKID_DEV_VERIFY)

/* cache.c */
extern void blkid_put_cache(blkid_cache cache);
extern int blkid_get_cache(blkid_cache *cache, const char *filename);
extern void blkid_gc_cache(blkid_cache cache);

/* dev.c */
extern const char *blkid_dev_devname(blkid_dev dev);

extern blkid_dev_iterate blkid_dev_iterate_begin(blkid_cache cache);
extern int blkid_dev_set_search(blkid_dev_iterate iter,
				char *search_type, char *search_value);
extern int blkid_dev_next(blkid_dev_iterate iterate, blkid_dev *dev);
extern void blkid_dev_iterate_end(blkid_dev_iterate iterate);

/* devno.c */
extern char *blkid_devno_to_devname(dev_t devno);

/* devname.c */
extern int blkid_probe_all(blkid_cache cache);
extern int blkid_probe_all_new(blkid_cache cache);
extern blkid_dev blkid_get_dev(blkid_cache cache, const char *devname,
			       int flags);

/* getsize.c */
extern blkid_loff_t blkid_get_dev_size(int fd);

/* probe.c */
int blkid_known_fstype(const char *fstype);
extern blkid_dev blkid_verify(blkid_cache cache, blkid_dev dev);

/* read.c */

/* resolve.c */
extern char *blkid_get_tag_value(blkid_cache cache, const char *tagname,
				       const char *devname);
extern char *blkid_get_devname(blkid_cache cache, const char *token,
			       const char *value);

/* tag.c */
extern blkid_tag_iterate blkid_tag_iterate_begin(blkid_dev dev);
extern int blkid_tag_next(blkid_tag_iterate iterate,
			      const char **type, const char **value);
extern void blkid_tag_iterate_end(blkid_tag_iterate iterate);
extern int blkid_dev_has_tag(blkid_dev dev, const char *type,
			     const char *value);
extern blkid_dev blkid_find_dev_with_tag(blkid_cache cache,
					 const char *type,
					 const char *value);
extern int blkid_parse_tag_string(const char *token, char **ret_type,
				  char **ret_val);

/* version.c */
extern int blkid_parse_version_string(const char *ver_string);
extern int blkid_get_library_version(const char **ver_string,
				     const char **date_string);

#ifdef __cplusplus
}
#endif

#endif /* _BLKID_BLKID_H */
