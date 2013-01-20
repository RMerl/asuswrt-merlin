/* vi: set sw=4 ts=4: */
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "blkidP.h"

int blkid_debug_mask = 0;

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

	cache = xzalloc(sizeof(struct blkid_struct_cache));

	INIT_LIST_HEAD(&cache->bic_devs);
	INIT_LIST_HEAD(&cache->bic_tags);

	if (filename && !strlen(filename))
		filename = 0;
	if (!filename && (getuid() == geteuid()))
		filename = getenv("BLKID_FILE");
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

#ifdef TEST_PROGRAM
int main(int argc, char** argv)
{
	blkid_cache cache = NULL;
	int ret;

	blkid_debug_mask = DEBUG_ALL;
	if ((argc > 2)) {
		fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
		exit(1);
	}

	if ((ret = blkid_get_cache(&cache, argv[1])) < 0) {
		fprintf(stderr, "error %d parsing cache file %s\n", ret,
			argv[1] ? argv[1] : BLKID_CACHE_FILE);
		exit(1);
	}
	if ((ret = blkid_get_cache(&cache, bb_dev_null)) != 0) {
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
