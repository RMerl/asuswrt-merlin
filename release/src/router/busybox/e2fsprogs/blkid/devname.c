/* vi: set sw=4 ts=4: */
/*
 * devname.c - get a dev by its device inode name
 *
 * Copyright (C) Andries Brouwer
 * Copyright (C) 1999, 2000, 2001, 2002, 2003 Theodore Ts'o
 * Copyright (C) 2001 Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif
#include <time.h>

#include "blkidP.h"

/*
 * Find a dev struct in the cache by device name, if available.
 *
 * If there is no entry with the specified device name, and the create
 * flag is set, then create an empty device entry.
 */
blkid_dev blkid_get_dev(blkid_cache cache, const char *devname, int flags)
{
	blkid_dev dev = NULL, tmp;
	struct list_head *p;

	if (!cache || !devname)
		return NULL;

	list_for_each(p, &cache->bic_devs) {
		tmp = list_entry(p, struct blkid_struct_dev, bid_devs);
		if (strcmp(tmp->bid_name, devname))
			continue;

		DBG(DEBUG_DEVNAME,
		    printf("found devname %s in cache\n", tmp->bid_name));
		dev = tmp;
		break;
	}

	if (!dev && (flags & BLKID_DEV_CREATE)) {
		dev = blkid_new_dev();
		if (!dev)
			return NULL;
		dev->bid_name = blkid_strdup(devname);
		dev->bid_cache = cache;
		list_add_tail(&dev->bid_devs, &cache->bic_devs);
		cache->bic_flags |= BLKID_BIC_FL_CHANGED;
	}

	if (flags & BLKID_DEV_VERIFY)
		dev = blkid_verify(cache, dev);
	return dev;
}

/*
 * Probe a single block device to add to the device cache.
 */
static void probe_one(blkid_cache cache, const char *ptname,
		      dev_t devno, int pri)
{
	blkid_dev dev = NULL;
	struct list_head *p;
	const char **dir;
	char *devname = NULL;

	/* See if we already have this device number in the cache. */
	list_for_each(p, &cache->bic_devs) {
		blkid_dev tmp = list_entry(p, struct blkid_struct_dev,
					   bid_devs);
		if (tmp->bid_devno == devno) {
			dev = blkid_verify(cache, tmp);
			break;
		}
	}
	if (dev && dev->bid_devno == devno)
		goto set_pri;

	/*
	 * Take a quick look at /dev/ptname for the device number.  We check
	 * all of the likely device directories.  If we don't find it, or if
	 * the stat information doesn't check out, use blkid_devno_to_devname()
	 * to find it via an exhaustive search for the device major/minor.
	 */
	for (dir = blkid_devdirs; *dir; dir++) {
		struct stat st;
		char device[256];

		sprintf(device, "%s/%s", *dir, ptname);
		if ((dev = blkid_get_dev(cache, device, BLKID_DEV_FIND)) &&
		    dev->bid_devno == devno)
			goto set_pri;

		if (stat(device, &st) == 0 && S_ISBLK(st.st_mode) &&
		    st.st_rdev == devno) {
			devname = blkid_strdup(device);
			break;
		}
	}
	if (!devname) {
		devname = blkid_devno_to_devname(devno);
		if (!devname)
			return;
	}
	dev = blkid_get_dev(cache, devname, BLKID_DEV_NORMAL);
	free(devname);

set_pri:
	if (!pri && !strncmp(ptname, "md", 2))
		pri = BLKID_PRI_MD;
	if (dev)
		dev->bid_pri = pri;
}

#define PROC_PARTITIONS "/proc/partitions"
#define VG_DIR		"/proc/lvm/VGs"

/*
 * This function initializes the UUID cache with devices from the LVM
 * proc hierarchy.  We currently depend on the names of the LVM
 * hierarchy giving us the device structure in /dev.  (XXX is this a
 * safe thing to do?)
 */
#ifdef VG_DIR
#include <dirent.h>
static dev_t lvm_get_devno(const char *lvm_device)
{
	FILE *lvf;
	char buf[1024];
	int ma, mi;
	dev_t ret = 0;

	DBG(DEBUG_DEVNAME, printf("opening %s\n", lvm_device));
	if ((lvf = fopen_for_read(lvm_device)) == NULL) {
		DBG(DEBUG_DEVNAME, printf("%s: (%d) %s\n", lvm_device, errno,
					  strerror(errno)));
		return 0;
	}

	while (fgets(buf, sizeof(buf), lvf)) {
		if (sscanf(buf, "device: %d:%d", &ma, &mi) == 2) {
			ret = makedev(ma, mi);
			break;
		}
	}
	fclose(lvf);

	return ret;
}

static void lvm_probe_all(blkid_cache cache)
{
	DIR		*vg_list;
	struct dirent	*vg_iter;
	int		vg_len = strlen(VG_DIR);
	dev_t		dev;

	if ((vg_list = opendir(VG_DIR)) == NULL)
		return;

	DBG(DEBUG_DEVNAME, printf("probing LVM devices under %s\n", VG_DIR));

	while ((vg_iter = readdir(vg_list)) != NULL) {
		DIR		*lv_list;
		char		*vdirname;
		char		*vg_name;
		struct dirent	*lv_iter;

		vg_name = vg_iter->d_name;
		if (LONE_CHAR(vg_name, '.') || !strcmp(vg_name, ".."))
			continue;
		vdirname = xmalloc(vg_len + strlen(vg_name) + 8);
		sprintf(vdirname, "%s/%s/LVs", VG_DIR, vg_name);

		lv_list = opendir(vdirname);
		free(vdirname);
		if (lv_list == NULL)
			continue;

		while ((lv_iter = readdir(lv_list)) != NULL) {
			char		*lv_name, *lvm_device;

			lv_name = lv_iter->d_name;
			if (LONE_CHAR(lv_name, '.') || !strcmp(lv_name, ".."))
				continue;

			lvm_device = xmalloc(vg_len + strlen(vg_name) +
					    strlen(lv_name) + 8);
			sprintf(lvm_device, "%s/%s/LVs/%s", VG_DIR, vg_name,
				lv_name);
			dev = lvm_get_devno(lvm_device);
			sprintf(lvm_device, "%s/%s", vg_name, lv_name);
			DBG(DEBUG_DEVNAME, printf("LVM dev %s: devno 0x%04X\n",
						  lvm_device,
						  (unsigned int) dev));
			probe_one(cache, lvm_device, dev, BLKID_PRI_LVM);
			free(lvm_device);
		}
		closedir(lv_list);
	}
	closedir(vg_list);
}
#endif

#define PROC_EVMS_VOLUMES "/proc/evms/volumes"

static int
evms_probe_all(blkid_cache cache)
{
	char line[100];
	int ma, mi, sz, num = 0;
	FILE *procpt;
	char device[110];

	procpt = fopen_for_read(PROC_EVMS_VOLUMES);
	if (!procpt)
		return 0;
	while (fgets(line, sizeof(line), procpt)) {
		if (sscanf(line, " %d %d %d %*s %*s %[^\n ]",
			    &ma, &mi, &sz, device) != 4)
			continue;

		DBG(DEBUG_DEVNAME, printf("Checking partition %s (%d, %d)\n",
					  device, ma, mi));

		probe_one(cache, device, makedev(ma, mi), BLKID_PRI_EVMS);
		num++;
	}
	fclose(procpt);
	return num;
}

/*
 * Read the device data for all available block devices in the system.
 */
int blkid_probe_all(blkid_cache cache)
{
	FILE *proc;
	char line[1024];
	char ptname0[128], ptname1[128], *ptname = NULL;
	char *ptnames[2];
	dev_t devs[2];
	int ma, mi;
	unsigned long long sz;
	int lens[2] = { 0, 0 };
	int which = 0, last = 0;

	ptnames[0] = ptname0;
	ptnames[1] = ptname1;

	if (!cache)
		return -BLKID_ERR_PARAM;

	if (cache->bic_flags & BLKID_BIC_FL_PROBED &&
	    time(NULL) - cache->bic_time < BLKID_PROBE_INTERVAL)
		return 0;

	blkid_read_cache(cache);
	evms_probe_all(cache);
#ifdef VG_DIR
	lvm_probe_all(cache);
#endif

	proc = fopen_for_read(PROC_PARTITIONS);
	if (!proc)
		return -BLKID_ERR_PROC;

	while (fgets(line, sizeof(line), proc)) {
		last = which;
		which ^= 1;
		ptname = ptnames[which];

		if (sscanf(line, " %d %d %llu %128[^\n ]",
			   &ma, &mi, &sz, ptname) != 4)
			continue;
		devs[which] = makedev(ma, mi);

		DBG(DEBUG_DEVNAME, printf("read partition name %s\n", ptname));

		/* Skip whole disk devs unless they have no partitions
		 * If we don't have a partition on this dev, also
		 * check previous dev to see if it didn't have a partn.
		 * heuristic: partition name ends in a digit.
		 *
		 * Skip extended partitions.
		 * heuristic: size is 1
		 *
		 * FIXME: skip /dev/{ida,cciss,rd} whole-disk devs
		 */

		lens[which] = strlen(ptname);
		if (isdigit(ptname[lens[which] - 1])) {
			DBG(DEBUG_DEVNAME,
			    printf("partition dev %s, devno 0x%04X\n",
				   ptname, (unsigned int) devs[which]));

			if (sz > 1)
				probe_one(cache, ptname, devs[which], 0);
			lens[which] = 0;
			lens[last] = 0;
		} else if (lens[last] && strncmp(ptnames[last], ptname,
						 lens[last])) {
			DBG(DEBUG_DEVNAME,
			    printf("whole dev %s, devno 0x%04X\n",
				   ptnames[last], (unsigned int) devs[last]));
			probe_one(cache, ptnames[last], devs[last], 0);
			lens[last] = 0;
		}
	}

	/* Handle the last device if it wasn't partitioned */
	if (lens[which])
		probe_one(cache, ptname, devs[which], 0);

	fclose(proc);

	cache->bic_time = time(NULL);
	cache->bic_flags |= BLKID_BIC_FL_PROBED;
	blkid_flush_cache(cache);
	return 0;
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
	blkid_cache cache = NULL;
	int ret;

	blkid_debug_mask = DEBUG_ALL;
	if (argc != 1) {
		fprintf(stderr, "Usage: %s\n"
			"Probe all devices and exit\n", argv[0]);
		exit(1);
	}
	if ((ret = blkid_get_cache(&cache, bb_dev_null)) != 0) {
		fprintf(stderr, "%s: error creating cache (%d)\n",
			argv[0], ret);
		exit(1);
	}
	if (blkid_probe_all(cache) < 0)
		printf("%s: error probing devices\n", argv[0]);

	blkid_put_cache(cache);
	return 0;
}
#endif
