/*
 * devno.c - find a particular device by its device number (major/minor)
 *
 * Copyright (C) 2000, 2001, 2003 Theodore Ts'o
 * Copyright (C) 2001 Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <dirent.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif
#include <fcntl.h>
#include <inttypes.h>

#include "blkidP.h"
#include "pathnames.h"
#include "at.h"
#include "sysfs.h"

char *blkid_strndup(const char *s, int length)
{
	char *ret;

	if (!s)
		return NULL;

	if (!length)
		length = strlen(s);

	ret = malloc(length + 1);
	if (ret) {
		strncpy(ret, s, length);
		ret[length] = '\0';
	}
	return ret;
}

char *blkid_strdup(const char *s)
{
	return blkid_strndup(s, 0);
}

char *blkid_strconcat(const char *a, const char *b, const char *c)
{
	char *res, *p;
	size_t len, al, bl, cl;

	al = a ? strlen(a) : 0;
	bl = b ? strlen(b) : 0;
	cl = c ? strlen(c) : 0;

	len = al + bl + cl;
	if (!len)
		return NULL;
	p = res = malloc(len + 1);
	if (!res)
		return NULL;
	if (al) {
		memcpy(p, a, al);
		p += al;
	}
	if (bl) {
		memcpy(p, b, bl);
		p += bl;
	}
	if (cl) {
		memcpy(p, c, cl);
		p += cl;
	}
	*p = '\0';
	return res;
}

/*
 * This function adds an entry to the directory list
 */
static void add_to_dirlist(const char *dir, const char *subdir,
				struct dir_list **list)
{
	struct dir_list *dp;

	dp = malloc(sizeof(struct dir_list));
	if (!dp)
		return;
	dp->name = subdir ? blkid_strconcat(dir, "/", subdir) :
			    blkid_strdup(dir);
	if (!dp->name) {
		free(dp);
		return;
	}
	dp->next = *list;
	*list = dp;
}

/*
 * This function frees a directory list
 */
static void free_dirlist(struct dir_list **list)
{
	struct dir_list *dp, *next;

	for (dp = *list; dp; dp = next) {
		next = dp->next;
		free(dp->name);
		free(dp);
	}
	*list = NULL;
}

void blkid__scan_dir(char *dirname, dev_t devno, struct dir_list **list,
		     char **devname)
{
	DIR	*dir;
	struct dirent *dp;
	struct stat st;

	if ((dir = opendir(dirname)) == NULL)
		return;

	while ((dp = readdir(dir)) != 0) {
#ifdef _DIRENT_HAVE_D_TYPE
		if (dp->d_type != DT_UNKNOWN && dp->d_type != DT_BLK &&
		    dp->d_type != DT_LNK && dp->d_type != DT_DIR)
			continue;
#endif
		if (dp->d_name[0] == '.' &&
		    ((dp->d_name[1] == 0) ||
		     ((dp->d_name[1] == '.') && (dp->d_name[2] == 0))))
			continue;

		if (fstat_at(dirfd(dir), dirname, dp->d_name, &st, 0))
			continue;

		if (S_ISBLK(st.st_mode) && st.st_rdev == devno) {
			*devname = blkid_strconcat(dirname, "/", dp->d_name);
			DBG(DEBUG_DEVNO,
			    printf("found 0x%llx at %s\n", (long long)devno,
				   *devname));
			break;
		}

		if (!list || !S_ISDIR(st.st_mode))
			continue;

		/* add subdirectory (but not symlink) to the list */
#ifdef _DIRENT_HAVE_D_TYPE
		if (dp->d_type == DT_LNK)
			continue;
		if (dp->d_type == DT_UNKNOWN)
#endif
		{
			if (fstat_at(dirfd(dir), dirname, dp->d_name, &st, 1) ||
			    !S_ISDIR(st.st_mode))
				continue;	/* symlink or lstat() failed */
		}

		if (*dp->d_name == '.' || (
#ifdef _DIRENT_HAVE_D_TYPE
		    dp->d_type == DT_DIR &&
#endif
		    strcmp(dp->d_name, "shm") == 0))
			/* ignore /dev/.{udev,mount,mdadm} and /dev/shm */
			continue;

		add_to_dirlist(dirname, dp->d_name, list);
	}
	closedir(dir);
	return;
}

/* Directories where we will try to search for device numbers */
static const char *devdirs[] = { "/devices", "/devfs", "/dev", NULL };

/**
 * SECTION: misc
 * @title: Miscellaneous utils
 * @short_description: mix of various utils for low-level and high-level API
 */

/* returns basename and keeps dirname in the @path */
static char *stripoff_last_component(char *path)
{
	char *p = strrchr(path, '/');

	if (!p)
		return NULL;
	*p = '\0';
	return ++p;
}

static char *scandev_devno_to_devpath(dev_t devno)
{
	struct dir_list *list = NULL, *new_list = NULL;
	char *devname = NULL;
	const char **dir;

	/*
	 * Add the starting directories to search in reverse order of
	 * importance, since we are using a stack...
	 */
	for (dir = devdirs; *dir; dir++)
		add_to_dirlist(*dir, NULL, &list);

	while (list) {
		struct dir_list *current = list;

		list = list->next;
		DBG(DEBUG_DEVNO, printf("directory %s\n", current->name));
		blkid__scan_dir(current->name, devno, &new_list, &devname);
		free(current->name);
		free(current);
		if (devname)
			break;
		/*
		 * If we're done checking at this level, descend to
		 * the next level of subdirectories. (breadth-first)
		 */
		if (list == NULL) {
			list = new_list;
			new_list = NULL;
		}
	}
	free_dirlist(&list);
	free_dirlist(&new_list);

	return devname;
}

/**
 * blkid_devno_to_devname:
 * @devno: device number
 *
 * This function finds the pathname to a block device with a given
 * device number.
 *
 * Returns: a pointer to allocated memory to the pathname on success,
 * and NULL on failure.
 */
char *blkid_devno_to_devname(dev_t devno)
{
	char *path = NULL;
	char buf[PATH_MAX];

	path = sysfs_devno_to_devpath(devno, buf, sizeof(buf));
	if (path)
		path = strdup(path);
	if (!path)
		path = scandev_devno_to_devpath(devno);

	if (!path) {
		DBG(DEBUG_DEVNO,
		    printf("blkid: couldn't find devno 0x%04lx\n",
			   (unsigned long) devno));
	} else {
		DBG(DEBUG_DEVNO,
		    printf("found devno 0x%04llx as %s\n", (long long)devno, path));
	}

	return path;
}

static int get_dm_wholedisk(struct sysfs_cxt *cxt, char *diskname,
			    size_t len, dev_t *diskdevno)
{
	int rc = 0;
	char *name;

	/* Note, sysfs_get_slave() returns the first slave only,
	 * if there is more slaves, then return NULL
	 */
	name = sysfs_get_slave(cxt);
	if (!name)
		return -1;

	if (diskname && len) {
		strncpy(diskname, name, len);
		diskname[len - 1] = '\0';
	}

	if (diskdevno) {
		*diskdevno = sysfs_devname_to_devno(name, NULL);
		if (!*diskdevno)
			rc = -1;
	}

	free(name);
	return rc;
}

/**
 * blkid_devno_to_wholedisk:
 * @dev: device number
 * @diskname: buffer to return diskname (or NULL)
 * @len: diskname buffer size (or 0)
 * @diskdevno: pointer to returns devno of entire disk (or NULL)
 *
 * This function uses sysfs to convert the @devno device number to the *name*
 * of the whole disk. The function DOES NOT return full device name. The @dev
 * argument could be partition or whole disk -- both is converted.
 *
 * For example: sda1, 0x0801 --> sda, 0x0800
 *
 * For conversion to the full disk *path* use blkid_devno_to_devname(), for
 * example:
 *
 * <informalexample>
 *  <programlisting>
 *
 *	dev_t dev = 0x0801, disk;		// sda1 = 8:1
 *	char *diskpath, diskname[32];
 *
 *	blkid_devno_to_wholedisk(dev, diskname, sizeof(diskname), &disk);
 *	diskpath = blkid_devno_to_devname(disk);
 *
 *	// print "0x0801: sda, /dev/sda, 8:0
 *	printf("0x%x: %s, %s, %d:%d\n",
 *		dev, diskname, diskpath, major(disk), minor(disk));
 *
 *	free(diskpath);
 *
 *  </programlisting>
 * </informalexample>
 *
 * Returns: 0 on success or -1 in case of error.
 */
int blkid_devno_to_wholedisk(dev_t dev, char *diskname,
			size_t len, dev_t *diskdevno)
{
	struct sysfs_cxt cxt;
	int is_part = 0;

	if (!dev || sysfs_init(&cxt, dev, NULL) != 0)
		return -1;

	is_part = sysfs_has_attribute(&cxt, "partition");
	if (!is_part) {
		/*
		 * Extra case for partitions mapped by device-mapper.
		 *
		 * All regualar partitions (added by BLKPG ioctl or kernel PT
		 * parser) have the /sys/.../partition file. The partitions
		 * mapped by DM don't have such file, but they have "part"
		 * prefix in DM UUID.
		 */
		char *uuid = sysfs_strdup(&cxt, "dm/uuid");
		char *tmp = uuid;
		char *prefix = uuid ? strsep(&tmp, "-") : NULL;

		if (prefix && strncasecmp(prefix, "part", 4) == 0)
			is_part = 1;
		free(uuid);

		if (is_part &&
		    get_dm_wholedisk(&cxt, diskname, len, diskdevno) == 0)
			/*
			 * partitioned device, mapped by DM
			 */
			goto done;

		is_part = 0;
	}

	if (!is_part) {
		/*
		 * unpartitioned device
		 */
		if (diskname && len) {
			if (!sysfs_get_devname(&cxt, diskname, len))
				goto err;
		}
		if (diskdevno)
			*diskdevno = dev;

	} else {
		/*
		 * partitioned device
		 *	- readlink /sys/dev/block/8:1   = ../../block/sda/sda1
		 *	- dirname  ../../block/sda/sda1 = ../../block/sda
		 *	- basename ../../block/sda      = sda
		 */
		char linkpath[PATH_MAX];
		char *name;
		int linklen;

		linklen = sysfs_readlink(&cxt, NULL,
				linkpath, sizeof(linkpath) - 1);
		if (linklen < 0)
			goto err;
		linkpath[linklen] = '\0';

		stripoff_last_component(linkpath);		/* dirname */
		name = stripoff_last_component(linkpath);	/* basename */
		if (!name)
			goto err;

		if (diskname && len) {
			strncpy(diskname, name, len);
			diskname[len - 1] = '\0';
		}

		if (diskdevno) {
			*diskdevno = sysfs_devname_to_devno(name, NULL);
			if (!*diskdevno)
				goto err;
		}
	}

done:
	sysfs_deinit(&cxt);

	DBG(DEBUG_DEVNO,
	    printf("found entire diskname for devno 0x%04llx %s\n",
	    (long long) dev, diskname ? diskname : ""));
	return 0;
err:
	sysfs_deinit(&cxt);

	DBG(DEBUG_DEVNO,
	    printf("failed to convert 0x%04llx to wholedisk name, errno=%d\n",
	    (long long) dev, errno));
	return -1;
}

/*
 * Returns 1 if the @major number is associated with @drvname.
 */
int blkid_driver_has_major(const char *drvname, int major)
{
	FILE *f;
	char buf[128];
	int match = 0;

	f = fopen(_PATH_PROC_DEVICES, "r");
	if (!f)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {	/* skip to block dev section */
		if (strncmp("Block devices:\n", buf, sizeof(buf)) == 0)
			break;
	}

	while (fgets(buf, sizeof(buf), f)) {
		int maj;
		char name[64];

		if (sscanf(buf, "%d %64[^\n ]", &maj, name) != 2)
			continue;

		if (maj == major && strcmp(name, drvname) == 0) {
			match = 1;
			break;
		}
	}

	fclose(f);

	DBG(DEBUG_DEVNO, printf("major %d %s associated with '%s' driver\n",
			major, match ? "is" : "is NOT", drvname));
	return match;
}


#ifdef TEST_PROGRAM
int main(int argc, char** argv)
{
	char	*devname, *tmp;
	char	diskname[PATH_MAX];
	int	major, minor;
	dev_t	devno, disk_devno;
	const char *errmsg = "Couldn't parse %s: %s\n";

	blkid_init_debug(DEBUG_ALL);
	if ((argc != 2) && (argc != 3)) {
		fprintf(stderr, "Usage:\t%s device_number\n\t%s major minor\n"
			"Resolve a device number to a device name\n",
			argv[0], argv[0]);
		exit(1);
	}
	if (argc == 2) {
		devno = strtoul(argv[1], &tmp, 0);
		if (*tmp) {
			fprintf(stderr, errmsg, "device number", argv[1]);
			exit(1);
		}
	} else {
		major = strtoul(argv[1], &tmp, 0);
		if (*tmp) {
			fprintf(stderr, errmsg, "major number", argv[1]);
			exit(1);
		}
		minor = strtoul(argv[2], &tmp, 0);
		if (*tmp) {
			fprintf(stderr, errmsg, "minor number", argv[2]);
			exit(1);
		}
		devno = makedev(major, minor);
	}
	printf("Looking for device 0x%04llx\n", (long long)devno);
	devname = blkid_devno_to_devname(devno);
	free(devname);

	printf("Looking for whole-device for 0x%04llx\n", (long long)devno);
	blkid_devno_to_wholedisk(devno, diskname, sizeof(diskname), &disk_devno);

	return 0;
}
#endif
