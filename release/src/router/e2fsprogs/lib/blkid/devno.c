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
#include <string.h>
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

#include "blkidP.h"

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

/*
 * This function adds an entry to the directory list
 */
static void add_to_dirlist(const char *name, struct dir_list **list)
{
	struct dir_list *dp;

	dp = malloc(sizeof(struct dir_list));
	if (!dp)
		return;
	dp->name = blkid_strdup(name);
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
	char	path[1024];
	int	dirlen;
	struct stat st;

	if ((dir = opendir(dirname)) == NULL)
		return;
	dirlen = strlen(dirname) + 2;
	while ((dp = readdir(dir)) != 0) {
		if (dirlen + strlen(dp->d_name) >= sizeof(path))
			continue;

		if (dp->d_name[0] == '.' &&
		    ((dp->d_name[1] == 0) ||
		     ((dp->d_name[1] == '.') && (dp->d_name[2] == 0))))
			continue;

		sprintf(path, "%s/%s", dirname, dp->d_name);
		if (stat(path, &st) < 0)
			continue;

		if (S_ISBLK(st.st_mode) && st.st_rdev == devno) {
			*devname = blkid_strdup(path);
			DBG(DEBUG_DEVNO,
			    printf("found 0x%llx at %s (%p)\n", (long long)devno,
				   path, *devname));
			break;
		}
		if (list && S_ISDIR(st.st_mode) && !lstat(path, &st) &&
		    S_ISDIR(st.st_mode))
			add_to_dirlist(path, list);
	}
	closedir(dir);
	return;
}

/* Directories where we will try to search for device numbers */
static const char *devdirs[] = { "/devices", "/devfs", "/dev", NULL };

/*
 * This function finds the pathname to a block device with a given
 * device number.  It returns a pointer to allocated memory to the
 * pathname on success, and NULL on failure.
 */
char *blkid_devno_to_devname(dev_t devno)
{
	struct dir_list *list = NULL, *new_list = NULL;
	char *devname = NULL;
	const char **dir;

	/*
	 * Add the starting directories to search in reverse order of
	 * importance, since we are using a stack...
	 */
	for (dir = devdirs; *dir; dir++)
		add_to_dirlist(*dir, &list);

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

	if (!devname) {
		DBG(DEBUG_DEVNO,
		    printf("blkid: couldn't find devno 0x%04lx\n",
			   (unsigned long) devno));
	} else {
		DBG(DEBUG_DEVNO,
		    printf("found devno 0x%04llx as %s\n", (long long)devno, devname));
	}


	return devname;
}

#ifdef TEST_PROGRAM
int main(int argc, char** argv)
{
	char	*devname, *tmp;
	int	major, minor;
	dev_t	devno;
	const char *errmsg = "Couldn't parse %s: %s\n";

	blkid_debug_mask = DEBUG_ALL;
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
	return 0;
}
#endif
