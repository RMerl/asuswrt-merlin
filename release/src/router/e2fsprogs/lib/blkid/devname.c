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

#define _GNU_SOURCE 1

#include <stdio.h>
#include <string.h>
#include <limits.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <dirent.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_SYS_MKDEV_H
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
	struct list_head *p, *pnext;

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
		if (access(devname, F_OK) < 0)
			return NULL;
		dev = blkid_new_dev();
		if (!dev)
			return NULL;
		dev->bid_time = INT_MIN;
		dev->bid_name = blkid_strdup(devname);
		dev->bid_cache = cache;
		list_add_tail(&dev->bid_devs, &cache->bic_devs);
		cache->bic_flags |= BLKID_BIC_FL_CHANGED;
	}

	if (flags & BLKID_DEV_VERIFY) {
		dev = blkid_verify(cache, dev);
		if (!dev || !(dev->bid_flags & BLKID_BID_FL_VERIFIED))
			return dev;
		/*
		 * If the device is verified, then search the blkid
		 * cache for any entries that match on the type, uuid,
		 * and label, and verify them; if a cache entry can
		 * not be verified, then it's stale and so we remove
		 * it.
		 */
		list_for_each_safe(p, pnext, &cache->bic_devs) {
			blkid_dev dev2;
			if (!p)
				break;
			dev2 = list_entry(p, struct blkid_struct_dev, bid_devs);
			if (dev2->bid_flags & BLKID_BID_FL_VERIFIED)
				continue;
			if (!dev->bid_type || !dev2->bid_type ||
			    strcmp(dev->bid_type, dev2->bid_type))
				continue;
			if (dev->bid_label && dev2->bid_label &&
			    strcmp(dev->bid_label, dev2->bid_label))
				continue;
			if (dev->bid_uuid && dev2->bid_uuid &&
			    strcmp(dev->bid_uuid, dev2->bid_uuid))
				continue;
			if ((dev->bid_label && !dev2->bid_label) ||
			    (!dev->bid_label && dev2->bid_label) ||
			    (dev->bid_uuid && !dev2->bid_uuid) ||
			    (!dev->bid_uuid && dev2->bid_uuid))
				continue;
			dev2 = blkid_verify(cache, dev2);
			if (dev2 && !(dev2->bid_flags & BLKID_BID_FL_VERIFIED))
				blkid_free_dev(dev2);
		}
	}
	return dev;
}

/* Directories where we will try to search for device names */
static const char *dirlist[] = { "/dev", "/devfs", "/devices", NULL };

static int is_dm_leaf(const char *devname)
{
	struct dirent	*de, *d_de;
	DIR		*dir, *d_dir;
	char		path[256];
	int		ret = 1;

	if ((dir = opendir("/sys/block")) == NULL)
		return 0;
	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..") ||
		    !strcmp(de->d_name, devname) ||
		    strncmp(de->d_name, "dm-", 3) ||
		    strlen(de->d_name) > sizeof(path)-32)
			continue;
		sprintf(path, "/sys/block/%s/slaves", de->d_name);
		if ((d_dir = opendir(path)) == NULL)
			continue;
		while ((d_de = readdir(d_dir)) != NULL) {
			if (!strcmp(d_de->d_name, devname)) {
				ret = 0;
				break;
			}
		}
		closedir(d_dir);
		if (!ret)
			break;
	}
	closedir(dir);
	return ret;
}

/*
 * Since 2.6.29 (patch 784aae735d9b0bba3f8b9faef4c8b30df3bf0128) kernel sysfs
 * provides the real DM device names in /sys/block/<ptname>/dm/name
 */
static char *get_dm_name(const char *ptname)
{
	FILE	*f;
	size_t	sz;
	char	path[256], name[256], *res = NULL;

	snprintf(path, sizeof(path), "/sys/block/%s/dm/name", ptname);
	if ((f = fopen(path, "r")) == NULL)
		return NULL;

	/* read "<name>\n" from sysfs */
	if (fgets(name, sizeof(name), f) && (sz = strlen(name)) > 1) {
		name[sz - 1] = '\0';
		snprintf(path, sizeof(path), "/dev/mapper/%s", name);
		res = blkid_strdup(path);
	}
	fclose(f);
	return res;
}

/*
 * Probe a single block device to add to the device cache.
 */
static void probe_one(blkid_cache cache, const char *ptname,
		      dev_t devno, int pri, int only_if_new)
{
	blkid_dev dev = NULL;
	struct list_head *p, *pnext;
	const char **dir;
	char *devname = NULL;

	/* See if we already have this device number in the cache. */
	list_for_each_safe(p, pnext, &cache->bic_devs) {
		blkid_dev tmp = list_entry(p, struct blkid_struct_dev,
					   bid_devs);
		if (tmp->bid_devno == devno) {
			if (only_if_new && !access(tmp->bid_name, F_OK))
				return;
			dev = blkid_verify(cache, tmp);
			if (dev && (dev->bid_flags & BLKID_BID_FL_VERIFIED))
				break;
			dev = 0;
		}
	}
	if (dev && dev->bid_devno == devno)
		goto set_pri;

	/* Try to translate private device-mapper dm-<N> names
	 * to standard /dev/mapper/<name>.
	 */
	if (!strncmp(ptname, "dm-", 3) && isdigit(ptname[3])) {
		devname = get_dm_name(ptname);
		if (!devname)
			blkid__scan_dir("/dev/mapper", devno, 0, &devname);
		if (devname)
			goto get_dev;
	}

	/*
	 * Take a quick look at /dev/ptname for the device number.  We check
	 * all of the likely device directories.  If we don't find it, or if
	 * the stat information doesn't check out, use blkid_devno_to_devname()
	 * to find it via an exhaustive search for the device major/minor.
	 */
	for (dir = dirlist; *dir; dir++) {
		struct stat st;
		char device[256];

		sprintf(device, "%s/%s", *dir, ptname);
		if ((dev = blkid_get_dev(cache, device, BLKID_DEV_FIND)) &&
		    dev->bid_devno == devno)
			goto set_pri;

		if (stat(device, &st) == 0 && S_ISBLK(st.st_mode) &&
		    st.st_rdev == devno) {
			devname = blkid_strdup(device);
			goto get_dev;
		}
	}
	/* Do a short-cut scan of /dev/mapper first */
	if (!devname)
		devname = get_dm_name(ptname);
	if (!devname)
		blkid__scan_dir("/dev/mapper", devno, 0, &devname);
	if (!devname) {
		devname = blkid_devno_to_devname(devno);
		if (!devname)
			return;
	}
get_dev:
	dev = blkid_get_dev(cache, devname, BLKID_DEV_NORMAL);
	free(devname);
set_pri:
	if (dev) {
		if (pri)
			dev->bid_pri = pri;
		else if (!strncmp(dev->bid_name, "/dev/mapper/", 11)) {
			dev->bid_pri = BLKID_PRI_DM;
			if (is_dm_leaf(ptname))
				dev->bid_pri += 5;
		} else if (!strncmp(ptname, "md", 2))
			dev->bid_pri = BLKID_PRI_MD;
 	}
	return;
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
static dev_t lvm_get_devno(const char *lvm_device)
{
	FILE *lvf;
	char buf[1024];
	int ma, mi;
	dev_t ret = 0;

	DBG(DEBUG_DEVNAME, printf("opening %s\n", lvm_device));
	if ((lvf = fopen(lvm_device, "r")) == NULL) {
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

static void lvm_probe_all(blkid_cache cache, int only_if_new)
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
		if (!strcmp(vg_name, ".") || !strcmp(vg_name, ".."))
			continue;
		vdirname = malloc(vg_len + strlen(vg_name) + 8);
		if (!vdirname)
			goto exit;
		sprintf(vdirname, "%s/%s/LVs", VG_DIR, vg_name);

		lv_list = opendir(vdirname);
		free(vdirname);
		if (lv_list == NULL)
			continue;

		while ((lv_iter = readdir(lv_list)) != NULL) {
			char		*lv_name, *lvm_device;

			lv_name = lv_iter->d_name;
			if (!strcmp(lv_name, ".") || !strcmp(lv_name, ".."))
				continue;

			lvm_device = malloc(vg_len + strlen(vg_name) +
					    strlen(lv_name) + 8);
			if (!lvm_device) {
				closedir(lv_list);
				goto exit;
			}
			sprintf(lvm_device, "%s/%s/LVs/%s", VG_DIR, vg_name,
				lv_name);
			dev = lvm_get_devno(lvm_device);
			sprintf(lvm_device, "%s/%s", vg_name, lv_name);
			DBG(DEBUG_DEVNAME, printf("LVM dev %s: devno 0x%04X\n",
						  lvm_device,
						  (unsigned int) dev));
			probe_one(cache, lvm_device, dev, BLKID_PRI_LVM,
				  only_if_new);
			free(lvm_device);
		}
		closedir(lv_list);
	}
exit:
	closedir(vg_list);
}
#endif

#define PROC_EVMS_VOLUMES "/proc/evms/volumes"

static int
evms_probe_all(blkid_cache cache, int only_if_new)
{
	char line[100];
	int ma, mi, sz, num = 0;
	FILE *procpt;
	char device[110];

	procpt = fopen(PROC_EVMS_VOLUMES, "r");
	if (!procpt)
		return 0;
	while (fgets(line, sizeof(line), procpt)) {
		if (sscanf (line, " %d %d %d %*s %*s %[^\n ]",
			    &ma, &mi, &sz, device) != 4)
			continue;

		DBG(DEBUG_DEVNAME, printf("Checking partition %s (%d, %d)\n",
					  device, ma, mi));

		probe_one(cache, device, makedev(ma, mi), BLKID_PRI_EVMS,
			  only_if_new);
		num++;
	}
	fclose(procpt);
	return num;
}

/*
 * Read the device data for all available block devices in the system.
 */
static int probe_all(blkid_cache cache, int only_if_new)
{
	FILE *proc;
	char line[1024];
	char ptname0[128], ptname1[128], *ptname = 0;
	char *ptnames[2];
	dev_t devs[2];
	int ma, mi;
	unsigned long long sz;
	int lens[2] = { 0, 0 };
	int which = 0, last = 0;
	struct list_head *p, *pnext;

	ptnames[0] = ptname0;
	ptnames[1] = ptname1;

	if (!cache)
		return -BLKID_ERR_PARAM;

	if (cache->bic_flags & BLKID_BIC_FL_PROBED &&
	    time(0) - cache->bic_time < BLKID_PROBE_INTERVAL)
		return 0;

	blkid_read_cache(cache);
	evms_probe_all(cache, only_if_new);
#ifdef VG_DIR
	lvm_probe_all(cache, only_if_new);
#endif

	proc = fopen(PROC_PARTITIONS, "r");
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

		/* Skip whole disk devs unless they have no partitions.
		 * If base name of device has changed, also
		 * check previous dev to see if it didn't have a partn.
		 * heuristic: partition name ends in a digit, & partition
		 * names contain whole device name as substring.
		 *
		 * Skip extended partitions.
		 * heuristic: size is 1
		 *
		 * FIXME: skip /dev/{ida,cciss,rd} whole-disk devs
		 */

		lens[which] = strlen(ptname);

		/* ends in a digit, clearly a partition, so check */
		if (isdigit(ptname[lens[which] - 1])) {
			DBG(DEBUG_DEVNAME,
			    printf("partition dev %s, devno 0x%04X\n",
				   ptname, (unsigned int) devs[which]));

			if (sz > 1)
				probe_one(cache, ptname, devs[which], 0,
					  only_if_new);
			lens[which] = 0;	/* mark as checked */
		}

		/*
		 * If last was a whole disk and we just found a partition
		 * on it, remove the whole-disk dev from the cache if
		 * it exists.
		 */
		if (lens[last] && !strncmp(ptnames[last], ptname, lens[last])) {
			list_for_each_safe(p, pnext, &cache->bic_devs) {
				blkid_dev tmp;

				/* find blkid dev for the whole-disk devno */
				tmp = list_entry(p, struct blkid_struct_dev,
						 bid_devs);
				if (tmp->bid_devno == devs[last]) {
					DBG(DEBUG_DEVNAME,
						printf("freeing %s\n",
						       tmp->bid_name));
					blkid_free_dev(tmp);
					cache->bic_flags |= BLKID_BIC_FL_CHANGED;
					break;
				}
			}
			lens[last] = 0;
		}
		/*
		 * If last was not checked because it looked like a whole-disk
		 * dev, and the device's base name has changed,
		 * check last as well.
		 */
		if (lens[last] && strncmp(ptnames[last], ptname, lens[last])) {
			DBG(DEBUG_DEVNAME,
			    printf("whole dev %s, devno 0x%04X\n",
				   ptnames[last], (unsigned int) devs[last]));
			probe_one(cache, ptnames[last], devs[last], 0,
				  only_if_new);
			lens[last] = 0;
		}
	}

	/* Handle the last device if it wasn't partitioned */
	if (lens[which])
		probe_one(cache, ptname, devs[which], 0, only_if_new);

	fclose(proc);
	blkid_flush_cache(cache);
	return 0;
}

int blkid_probe_all(blkid_cache cache)
{
	int ret;

	DBG(DEBUG_PROBE, printf("Begin blkid_probe_all()\n"));
	ret = probe_all(cache, 0);
	cache->bic_time = time(0);
	cache->bic_flags |= BLKID_BIC_FL_PROBED;
	DBG(DEBUG_PROBE, printf("End blkid_probe_all()\n"));
	return ret;
}

int blkid_probe_all_new(blkid_cache cache)
{
	int ret;

	DBG(DEBUG_PROBE, printf("Begin blkid_probe_all_new()\n"));
	ret = probe_all(cache, 1);
	DBG(DEBUG_PROBE, printf("End blkid_probe_all_new()\n"));
	return ret;
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
	if ((ret = blkid_get_cache(&cache, "/dev/null")) != 0) {
		fprintf(stderr, "%s: error creating cache (%d)\n",
			argv[0], ret);
		exit(1);
	}
	if (blkid_probe_all(cache) < 0)
		printf("%s: error probing devices\n", argv[0]);

	blkid_put_cache(cache);
	return (0);
}
#endif
