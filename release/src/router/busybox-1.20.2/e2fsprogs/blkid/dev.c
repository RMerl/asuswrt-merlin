/* vi: set sw=4 ts=4: */
/*
 * dev.c - allocation/initialization/free routines for dev
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

#include "blkidP.h"

blkid_dev blkid_new_dev(void)
{
	blkid_dev dev;

	dev = xzalloc(sizeof(struct blkid_struct_dev));

	INIT_LIST_HEAD(&dev->bid_devs);
	INIT_LIST_HEAD(&dev->bid_tags);

	return dev;
}

void blkid_free_dev(blkid_dev dev)
{
	if (!dev)
		return;

	DBG(DEBUG_DEV,
	    printf("  freeing dev %s (%s)\n", dev->bid_name, dev->bid_type));
	DBG(DEBUG_DEV, blkid_debug_dump_dev(dev));

	list_del(&dev->bid_devs);
	while (!list_empty(&dev->bid_tags)) {
		blkid_tag tag = list_entry(dev->bid_tags.next,
					   struct blkid_struct_tag,
					   bit_tags);
		blkid_free_tag(tag);
	}
	if (dev->bid_name)
		free(dev->bid_name);
	free(dev);
}

/*
 * Given a blkid device, return its name
 */
const char *blkid_dev_devname(blkid_dev dev)
{
	return dev->bid_name;
}

#ifdef CONFIG_BLKID_DEBUG
void blkid_debug_dump_dev(blkid_dev dev)
{
	struct list_head *p;

	if (!dev) {
		printf("  dev: NULL\n");
		return;
	}

	printf("  dev: name = %s\n", dev->bid_name);
	printf("  dev: DEVNO=\"0x%0llx\"\n", dev->bid_devno);
	printf("  dev: TIME=\"%lu\"\n", dev->bid_time);
	printf("  dev: PRI=\"%d\"\n", dev->bid_pri);
	printf("  dev: flags = 0x%08X\n", dev->bid_flags);

	list_for_each(p, &dev->bid_tags) {
		blkid_tag tag = list_entry(p, struct blkid_struct_tag, bit_tags);
		if (tag)
			printf("    tag: %s=\"%s\"\n", tag->bit_name,
			       tag->bit_val);
		else
			printf("    tag: NULL\n");
	}
	bb_putchar('\n');
}
#endif

/*
 * dev iteration routines for the public libblkid interface.
 *
 * These routines do not expose the list.h implementation, which are a
 * contamination of the namespace, and which force us to reveal far, far
 * too much of our internal implemenation.  I'm not convinced I want
 * to keep list.h in the long term, anyway.  It's fine for kernel
 * programming, but performance is not the #1 priority for this
 * library, and I really don't like the tradeoff of type-safety for
 * performance for this application.  [tytso:20030125.2007EST]
 */

/*
 * This series of functions iterate over all devices in a blkid cache
 */
#define DEV_ITERATE_MAGIC	0x01a5284c

struct blkid_struct_dev_iterate {
	int			magic;
	blkid_cache		cache;
	struct list_head	*p;
};

blkid_dev_iterate blkid_dev_iterate_begin(blkid_cache cache)
{
	blkid_dev_iterate	iter;

	iter = xmalloc(sizeof(struct blkid_struct_dev_iterate));
	iter->magic = DEV_ITERATE_MAGIC;
	iter->cache = cache;
	iter->p	= cache->bic_devs.next;
	return iter;
}

/*
 * Return 0 on success, -1 on error
 */
extern int blkid_dev_next(blkid_dev_iterate iter,
			  blkid_dev *dev)
{
	*dev = 0;
	if (!iter || iter->magic != DEV_ITERATE_MAGIC ||
	    iter->p == &iter->cache->bic_devs)
		return -1;
	*dev = list_entry(iter->p, struct blkid_struct_dev, bid_devs);
	iter->p = iter->p->next;
	return 0;
}

void blkid_dev_iterate_end(blkid_dev_iterate iter)
{
	if (!iter || iter->magic != DEV_ITERATE_MAGIC)
		return;
	iter->magic = 0;
	free(iter);
}

#ifdef TEST_PROGRAM
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif

void usage(char *prog)
{
	fprintf(stderr, "Usage: %s [-f blkid_file] [-m debug_mask]\n", prog);
	fprintf(stderr, "\tList all devices and exit\n", prog);
	exit(1);
}

int main(int argc, char **argv)
{
	blkid_dev_iterate	iter;
	blkid_cache		cache = NULL;
	blkid_dev		dev;
	int			c, ret;
	char			*tmp;
	char			*file = NULL;
	char			*search_type = NULL;
	char			*search_value = NULL;

	while ((c = getopt (argc, argv, "m:f:")) != EOF)
		switch (c) {
		case 'f':
			file = optarg;
			break;
		case 'm':
			blkid_debug_mask = strtoul (optarg, &tmp, 0);
			if (*tmp) {
				fprintf(stderr, "Invalid debug mask: %d\n",
					optarg);
				exit(1);
			}
			break;
		case '?':
			usage(argv[0]);
		}
	if (argc >= optind+2) {
		search_type = argv[optind];
		search_value = argv[optind+1];
		optind += 2;
	}
	if (argc != optind)
		usage(argv[0]);

	if ((ret = blkid_get_cache(&cache, file)) != 0) {
		fprintf(stderr, "%s: error creating cache (%d)\n",
			argv[0], ret);
		exit(1);
	}

	iter = blkid_dev_iterate_begin(cache);
	if (search_type)
		blkid_dev_set_search(iter, search_type, search_value);
	while (blkid_dev_next(iter, &dev) == 0) {
		printf("Device: %s\n", blkid_dev_devname(dev));
	}
	blkid_dev_iterate_end(iter);


	blkid_put_cache(cache);
	return 0;
}
#endif
