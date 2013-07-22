/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include "blkidP.h"

static void blkid_probe_to_tags(blkid_probe pr, blkid_dev dev)
{
	const char *data;
	const char *name;
	int nvals, n;
	size_t len;

	nvals = blkid_probe_numof_values(pr);

	for (n = 0; n < nvals; n++) {
		if (blkid_probe_get_value(pr, n, &name, &data, &len) == 0)
			blkid_set_tag(dev, name, data, len);
	}
}

/*
 * Verify that the data in dev is consistent with what is on the actual
 * block device (using the devname field only).  Normally this will be
 * called when finding items in the cache, but for long running processes
 * is also desirable to revalidate an item before use.
 *
 * If we are unable to revalidate the data, we return the old data and
 * do not set the BLKID_BID_FL_VERIFIED flag on it.
 */
blkid_dev blkid_verify(blkid_cache cache, blkid_dev dev)
{
	struct stat st;
	time_t diff, now;
	char *fltr[2];
	int fd;

	if (!dev)
		return NULL;

	now = time(0);
	diff = now - dev->bid_time;

	if (stat(dev->bid_name, &st) < 0) {
		DBG(DEBUG_PROBE,
		    printf("blkid_verify: error %s (%d) while "
			   "trying to stat %s\n", strerror(errno), errno,
			   dev->bid_name));
	open_err:
		if ((errno == EPERM) || (errno == EACCES) || (errno == ENOENT)) {
			/* We don't have read permission, just return cache data. */
			DBG(DEBUG_PROBE, printf("returning unverified data for %s\n",
						dev->bid_name));
			return dev;
		}
		blkid_free_dev(dev);
		return NULL;
	}

	if (now >= dev->bid_time &&
#ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
	    (st.st_mtime < dev->bid_time ||
	        (st.st_mtime == dev->bid_time &&
		 st.st_mtim.tv_nsec / 1000 <= dev->bid_utime)) &&
#else
	    st.st_mtime <= dev->bid_time &&
#endif
	    (diff < BLKID_PROBE_MIN ||
		(dev->bid_flags & BLKID_BID_FL_VERIFIED &&
		 diff < BLKID_PROBE_INTERVAL)))
		return dev;

#ifndef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
	DBG(DEBUG_PROBE,
	    printf("need to revalidate %s (cache time %lu, stat time %lu,\n\t"
		   "time since last check %lu)\n",
		   dev->bid_name, (unsigned long)dev->bid_time,
		   (unsigned long)st.st_mtime, (unsigned long)diff));
#else
	DBG(DEBUG_PROBE,
	    printf("need to revalidate %s (cache time %lu.%lu, stat time %lu.%lu,\n\t"
		   "time since last check %lu)\n",
		   dev->bid_name,
		   (unsigned long)dev->bid_time, (unsigned long)dev->bid_utime,
		   (unsigned long)st.st_mtime, (unsigned long)st.st_mtim.tv_nsec / 1000,
		   (unsigned long)diff));
#endif

	if (!cache->probe) {
		cache->probe = blkid_new_probe();
		if (!cache->probe) {
			blkid_free_dev(dev);
			return NULL;
		}
	}

	fd = open(dev->bid_name, O_RDONLY);
	if (fd < 0) {
		DBG(DEBUG_PROBE, printf("blkid_verify: error %s (%d) while "
					"opening %s\n", strerror(errno), errno,
					dev->bid_name));
		goto open_err;
	}

	if (blkid_probe_set_device(cache->probe, fd, 0, 0)) {
		/* failed to read the device */
		close(fd);
		blkid_free_dev(dev);
		return NULL;
	}

	blkid_probe_enable_superblocks(cache->probe, TRUE);

	blkid_probe_set_superblocks_flags(cache->probe,
		BLKID_SUBLKS_LABEL | BLKID_SUBLKS_UUID |
		BLKID_SUBLKS_TYPE | BLKID_SUBLKS_SECTYPE);

	/*
	 * If we already know the type, then try that first.
	 */
	if (dev->bid_type) {
		blkid_tag_iterate iter;
		const char *type, *value;

		fltr[0] = dev->bid_type;
		fltr[1] = NULL;

		blkid_probe_filter_superblocks_type(cache->probe,
				BLKID_FLTR_ONLYIN, fltr);

		if (!blkid_do_probe(cache->probe))
			goto found_type;
		blkid_probe_invert_superblocks_filter(cache->probe);

		/*
		 * Zap the device filesystem information and try again
		 */
		DBG(DEBUG_PROBE,
		    printf("previous fs type %s not valid, "
			   "trying full probe\n", dev->bid_type));
		iter = blkid_tag_iterate_begin(dev);
		while (blkid_tag_next(iter, &type, &value) == 0)
			blkid_set_tag(dev, type, 0, 0);
		blkid_tag_iterate_end(iter);
	}

	/*
	 * Probe for all types.
	 */
	if (blkid_do_safeprobe(cache->probe)) {
		/* found nothing or error */
		blkid_free_dev(dev);
		dev = NULL;
	}

found_type:
	if (dev) {
#ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
		struct timeval tv;
		if (!gettimeofday(&tv, NULL)) {
			dev->bid_time = tv.tv_sec;
			dev->bid_utime = tv.tv_usec;
		} else
#endif
			dev->bid_time = time(0);

		dev->bid_devno = st.st_rdev;
		dev->bid_flags |= BLKID_BID_FL_VERIFIED;
		cache->bic_flags |= BLKID_BIC_FL_CHANGED;

		blkid_probe_to_tags(cache->probe, dev);

		DBG(DEBUG_PROBE, printf("%s: devno 0x%04llx, type %s\n",
			   dev->bid_name, (long long)st.st_rdev, dev->bid_type));
	}

	blkid_reset_probe(cache->probe);
	blkid_probe_reset_superblocks_filter(cache->probe);
	close(fd);
	return dev;
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
	blkid_dev dev;
	blkid_cache cache;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s device\n"
			"Probe a single device to determine type\n", argv[0]);
		exit(1);
	}
	if ((ret = blkid_get_cache(&cache, "/dev/null")) != 0) {
		fprintf(stderr, "%s: error creating cache (%d)\n",
			argv[0], ret);
		exit(1);
	}
	dev = blkid_get_dev(cache, argv[1], BLKID_DEV_NORMAL);
	if (!dev) {
		printf("%s: %s has an unsupported type\n", argv[0], argv[1]);
		return (1);
	}
	printf("TYPE='%s'\n", dev->bid_type ? dev->bid_type : "(null)");
	if (dev->bid_label)
		printf("LABEL='%s'\n", dev->bid_label);
	if (dev->bid_uuid)
		printf("UUID='%s'\n", dev->bid_uuid);

	blkid_free_dev(dev);
	return (0);
}
#endif
