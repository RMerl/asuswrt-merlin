/*
 * cache.c - allocation/initialization/free routines for cache
 *
 * Copyright (C) 2001 Andreas Dilger
 * Copyright (C) 2003 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#else
#define PR_GET_DUMPABLE 3
#endif
#if (!defined(HAVE_PRCTL) && defined(linux))
#include <sys/syscall.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "blkidP.h"

int blkid_debug_mask = 0;


static char *safe_getenv(const char *arg)
{
	if ((getuid() != geteuid()) || (getgid() != getegid()))
		return NULL;
#if HAVE_PRCTL
	if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0) == 0)
		return NULL;
#else
#if (defined(linux) && defined(SYS_prctl))
	if (syscall(SYS_prctl, PR_GET_DUMPABLE, 0, 0, 0, 0) == 0)
		return NULL;
#endif
#endif

#ifdef HAVE___SECURE_GETENV
	return __secure_getenv(arg);
#else
	return getenv(arg);
#endif
}

#if 0 /* ifdef CONFIG_BLKID_DEBUG */
static blkid_debug_dump_cache(int mask, blkid_cache cache)
{
	struct list_head *p;

	if (!cache) {
		printf("cache: NULL\n");
		return;
	}

	printf("cache: time = %lu\n", cache->bic_time);
	printf("cache: flags = 0x%08X\n", cache->bic_flags);

	list_for_each(p, &cache->bic_devs) {
		blkid_dev dev = list_entry(p, struct blkid_struct_dev, bid_devs);
		blkid_debug_dump_dev(dev);
	}
}
#endif

int blkid_get_cache(blkid_cache *ret_cache, const char *filename)
{
	blkid_cache cache;

#ifdef CONFIG_BLKID_DEBUG
	if (!(blkid_debug_mask & DEBUG_INIT)) {
		char *dstr = getenv("BLKID_DEBUG");

		if (dstr)
			blkid_debug_mask = strtoul(dstr, 0, 0);
		blkid_debug_mask |= DEBUG_INIT;
	}
#endif

	DBG(DEBUG_CACHE, printf("creating blkid cache (using %s)\n",
				filename ? filename : "default cache"));

	if (!(cache = (blkid_cache) calloc(1, sizeof(struct blkid_struct_cache))))
		return -BLKID_ERR_MEM;

	INIT_LIST_HEAD(&cache->bic_devs);
	INIT_LIST_HEAD(&cache->bic_tags);

	if (filename && !strlen(filename))
		filename = 0;
	if (!filename)
		filename = safe_getenv("BLKID_FILE");
	if (!filename)
		filename = BLKID_CACHE_FILE;
	cache->bic_filename = blkid_strdup(filename);

	blkid_read_cache(cache);

	*ret_cache = cache;
	return 0;
}

void blkid_put_cache(blkid_cache cache)
{
	if (!cache)
		return;

	(void) blkid_flush_cache(cache);

	DBG(DEBUG_CACHE, printf("freeing cache struct\n"));

	/* DBG(DEBUG_CACHE, blkid_debug_dump_cache(cache)); */

	while (!list_empty(&cache->bic_devs)) {
		blkid_dev dev = list_entry(cache->bic_devs.next,
					   struct blkid_struct_dev,
					    bid_devs);
		blkid_free_dev(dev);
	}

	while (!list_empty(&cache->bic_tags)) {
		blkid_tag tag = list_entry(cache->bic_tags.next,
					   struct blkid_struct_tag,
					   bit_tags);

		while (!list_empty(&tag->bit_names)) {
			blkid_tag bad = list_entry(tag->bit_names.next,
						   struct blkid_struct_tag,
						   bit_names);

			DBG(DEBUG_CACHE, printf("warning: unfreed tag %s=%s\n",
						bad->bit_name, bad->bit_val));
			blkid_free_tag(bad);
		}
		blkid_free_tag(tag);
	}
	free(cache->bic_filename);

	free(cache);
}

void blkid_gc_cache(blkid_cache cache)
{
	struct list_head *p, *pnext;
	struct stat st;

	if (!cache)
		return;

	list_for_each_safe(p, pnext, &cache->bic_devs) {
		blkid_dev dev = list_entry(p, struct blkid_struct_dev, bid_devs);
		if (!p)
			break;
		if (stat(dev->bid_name, &st) < 0) {
			DBG(DEBUG_CACHE,
			    printf("freeing %s\n", dev->bid_name));
			blkid_free_dev(dev);
			cache->bic_flags |= BLKID_BIC_FL_CHANGED;
		} else {
			DBG(DEBUG_CACHE,
			    printf("Device %s exists\n", dev->bid_name));
		}
	}
}


#ifdef TEST_PROGRAM
int main(int argc, char** argv)
{
	blkid_cache cache = NULL;
	int ret;

	blkid_debug_mask = DEBUG_ALL;
	if ((argc > 2)) {
		fprintf(stderr, "Usage: %s [filename] \n", argv[0]);
		exit(1);
	}

	if ((ret = blkid_get_cache(&cache, argv[1])) < 0) {
		fprintf(stderr, "error %d parsing cache file %s\n", ret,
			argv[1] ? argv[1] : BLKID_CACHE_FILE);
		exit(1);
	}
	if ((ret = blkid_get_cache(&cache, "/dev/null")) != 0) {
		fprintf(stderr, "%s: error creating cache (%d)\n",
			argv[0], ret);
		exit(1);
	}
	if ((ret = blkid_probe_all(cache) < 0))
		fprintf(stderr, "error probing devices\n");

	blkid_put_cache(cache);

	return ret;
}
#endif
