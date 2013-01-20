/* vi: set sw=4 ts=4: */
/*
 * resolve.c - resolve names and tags into specific devices
 *
 * Copyright (C) 2001, 2003 Theodore Ts'o.
 * Copyright (C) 2001 Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "blkidP.h"
#include "probe.h"

/*
 * Find a tagname (e.g. LABEL or UUID) on a specific device.
 */
char *blkid_get_tag_value(blkid_cache cache, const char *tagname,
			  const char *devname)
{
	blkid_tag found;
	blkid_dev dev;
	blkid_cache c = cache;
	char *ret = NULL;

	DBG(DEBUG_RESOLVE, printf("looking for %s on %s\n", tagname, devname));

	if (!devname)
		return NULL;

	if (!cache) {
		if (blkid_get_cache(&c, NULL) < 0)
			return NULL;
	}

	if ((dev = blkid_get_dev(c, devname, BLKID_DEV_NORMAL)) &&
	    (found = blkid_find_tag_dev(dev, tagname)))
		ret = blkid_strdup(found->bit_val);

	if (!cache)
		blkid_put_cache(c);

	return ret;
}

/*
 * Locate a device name from a token (NAME=value string), or (name, value)
 * pair.  In the case of a token, value is ignored.  If the "token" is not
 * of the form "NAME=value" and there is no value given, then it is assumed
 * to be the actual devname and a copy is returned.
 */
char *blkid_get_devname(blkid_cache cache, const char *token,
			const char *value)
{
	blkid_dev dev;
	blkid_cache c = cache;
	char *t = NULL, *v = NULL;
	char *ret = NULL;

	if (!token)
		return NULL;

	if (!cache) {
		if (blkid_get_cache(&c, NULL) < 0)
			return NULL;
	}

	DBG(DEBUG_RESOLVE,
	    printf("looking for %s%s%s %s\n", token, value ? "=" : "",
		   value ? value : "", cache ? "in cache" : "from disk"));

	if (!value) {
		if (!strchr(token, '='))
			return blkid_strdup(token);
		blkid_parse_tag_string(token, &t, &v);
		if (!t || !v)
			goto errout;
		token = t;
		value = v;
	}

	dev = blkid_find_dev_with_tag(c, token, value);
	if (!dev)
		goto errout;

	ret = blkid_strdup(blkid_dev_devname(dev));

errout:
	free(t);
	free(v);
	if (!cache) {
		blkid_put_cache(c);
	}
	return ret;
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
	char *value;
	blkid_cache cache;

	blkid_debug_mask = DEBUG_ALL;
	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Usage:\t%s tagname=value\n"
			"\t%s tagname devname\n"
			"Find which device holds a given token or\n"
			"Find what the value of a tag is in a device\n",
			argv[0], argv[0]);
		exit(1);
	}
	if (blkid_get_cache(&cache, bb_dev_null) < 0) {
		fprintf(stderr, "Can't get blkid cache\n");
		exit(1);
	}

	if (argv[2]) {
		value = blkid_get_tag_value(cache, argv[1], argv[2]);
		printf("%s has tag %s=%s\n", argv[2], argv[1],
		       value ? value : "<missing>");
	} else {
		value = blkid_get_devname(cache, argv[1], NULL);
		printf("%s has tag %s\n", value ? value : "<none>", argv[1]);
	}
	blkid_put_cache(cache);
	return value ? 0 : 1;
}
#endif
