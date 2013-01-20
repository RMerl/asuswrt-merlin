/*
 * save.c - write the cache struct to disk
 *
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2003 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include "blkidP.h"

static int save_dev(blkid_dev dev, FILE *file)
{
	struct list_head *p;

	if (!dev || dev->bid_name[0] != '/')
		return 0;

	DBG(DEBUG_SAVE,
	    printf("device %s, type %s\n", dev->bid_name, dev->bid_type ?
		   dev->bid_type : "(null)"));

	fprintf(file,
		"<device DEVNO=\"0x%04lx\" TIME=\"%ld\"",
		(unsigned long) dev->bid_devno, (long) dev->bid_time);
	if (dev->bid_pri)
		fprintf(file, " PRI=\"%d\"", dev->bid_pri);
	list_for_each(p, &dev->bid_tags) {
		blkid_tag tag = list_entry(p, struct blkid_struct_tag, bit_tags);
		fprintf(file, " %s=\"%s\"", tag->bit_name,tag->bit_val);
	}
	fprintf(file, ">%s</device>\n", dev->bid_name);

	return 0;
}

/*
 * Write out the cache struct to the cache file on disk.
 */
int blkid_flush_cache(blkid_cache cache)
{
	struct list_head *p;
	char *tmp = NULL;
	const char *opened = NULL;
	const char *filename;
	FILE *file = NULL;
	int fd, ret = 0;
	struct stat st;

	if (!cache)
		return -BLKID_ERR_PARAM;

	if (list_empty(&cache->bic_devs) ||
	    !(cache->bic_flags & BLKID_BIC_FL_CHANGED)) {
		DBG(DEBUG_SAVE, printf("skipping cache file write\n"));
		return 0;
	}

	filename = cache->bic_filename ? cache->bic_filename: BLKID_CACHE_FILE;

	/* If we can't write to the cache file, then don't even try */
	if (((ret = stat(filename, &st)) < 0 && errno != ENOENT) ||
	    (ret == 0 && access(filename, W_OK) < 0)) {
		DBG(DEBUG_SAVE,
		    printf("can't write to cache file %s\n", filename));
		return 0;
	}

	/*
	 * Try and create a temporary file in the same directory so
	 * that in case of error we don't overwrite the cache file.
	 * If the cache file doesn't yet exist, it isn't a regular
	 * file (e.g. /dev/null or a socket), or we couldn't create
	 * a temporary file then we open it directly.
	 */
	if (ret == 0 && S_ISREG(st.st_mode)) {
		tmp = malloc(strlen(filename) + 8);
		if (tmp) {
			sprintf(tmp, "%s-XXXXXX", filename);
			fd = mkstemp(tmp);
			if (fd >= 0) {
				file = fdopen(fd, "w");
				opened = tmp;
			}
			fchmod(fd, 0644);
		}
	}

	if (!file) {
		file = fopen(filename, "w");
		opened = filename;
	}

	DBG(DEBUG_SAVE,
	    printf("writing cache file %s (really %s)\n",
		   filename, opened));

	if (!file) {
		ret = errno;
		goto errout;
	}

	list_for_each(p, &cache->bic_devs) {
		blkid_dev dev = list_entry(p, struct blkid_struct_dev, bid_devs);
		if (!dev->bid_type)
			continue;
		if ((ret = save_dev(dev, file)) < 0)
			break;
	}

	if (ret >= 0) {
		cache->bic_flags &= ~BLKID_BIC_FL_CHANGED;
		ret = 1;
	}

	fclose(file);
	if (opened != filename) {
		if (ret < 0) {
			unlink(opened);
			DBG(DEBUG_SAVE,
			    printf("unlinked temp cache %s\n", opened));
		} else {
			char *backup;

			backup = malloc(strlen(filename) + 5);
			if (backup) {
				sprintf(backup, "%s.old", filename);
				unlink(backup);
				link(filename, backup);
				free(backup);
			}
			rename(opened, filename);
			DBG(DEBUG_SAVE,
			    printf("moved temp cache %s\n", opened));
		}
	}

errout:
	free(tmp);
	return ret;
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
	blkid_cache cache = NULL;
	int ret;

	blkid_debug_mask = DEBUG_ALL;
	if (argc > 2) {
		fprintf(stderr, "Usage: %s [filename]\n"
			"Test loading/saving a cache (filename)\n", argv[0]);
		exit(1);
	}

	if ((ret = blkid_get_cache(&cache, "/dev/null")) != 0) {
		fprintf(stderr, "%s: error creating cache (%d)\n",
			argv[0], ret);
		exit(1);
	}
	if ((ret = blkid_probe_all(cache)) < 0) {
		fprintf(stderr, "error (%d) probing devices\n", ret);
		exit(1);
	}
	cache->bic_filename = blkid_strdup(argv[1]);

	if ((ret = blkid_flush_cache(cache)) < 0) {
		fprintf(stderr, "error (%d) saving cache\n", ret);
		exit(1);
	}

	blkid_put_cache(cache);

	return ret;
}
#endif
