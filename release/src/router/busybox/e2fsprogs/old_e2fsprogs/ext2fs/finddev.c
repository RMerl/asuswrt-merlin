/* vi: set sw=4 ts=4: */
/*
 * finddev.c -- this routine attempts to find a particular device in
 *	/dev
 *
 * Copyright (C) 2000 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <dirent.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

struct dir_list {
	char	*name;
	struct dir_list *next;
};

/*
 * This function adds an entry to the directory list
 */
static void add_to_dirlist(const char *name, struct dir_list **list)
{
	struct dir_list *dp;

	dp = xmalloc(sizeof(struct dir_list));
	dp->name = xmalloc(strlen(name)+1);
	strcpy(dp->name, name);
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
	*list = 0;
}

static int scan_dir(char *dir_name, dev_t device, struct dir_list **list,
		    char **ret_path)
{
	DIR	*dir;
	struct dirent *dp;
	char	path[1024], *cp;
	int	dirlen;
	struct stat st;

	dirlen = strlen(dir_name);
	if ((dir = opendir(dir_name)) == NULL)
		return errno;
	dp = readdir(dir);
	while (dp) {
		if (dirlen + strlen(dp->d_name) + 2 >= sizeof(path))
			goto skip_to_next;
		if (dp->d_name[0] == '.' &&
		    ((dp->d_name[1] == 0) ||
		     ((dp->d_name[1] == '.') && (dp->d_name[2] == 0))))
			goto skip_to_next;
		sprintf(path, "%s/%s", dir_name, dp->d_name);
		if (stat(path, &st) < 0)
			goto skip_to_next;
		if (S_ISDIR(st.st_mode))
			add_to_dirlist(path, list);
		if (S_ISBLK(st.st_mode) && st.st_rdev == device) {
			cp = xmalloc(strlen(path)+1);
			strcpy(cp, path);
			*ret_path = cp;
			goto success;
		}
	skip_to_next:
		dp = readdir(dir);
	}
success:
	closedir(dir);
	return 0;
}

/*
 * This function finds the pathname to a block device with a given
 * device number.  It returns a pointer to allocated memory to the
 * pathname on success, and NULL on failure.
 */
char *ext2fs_find_block_device(dev_t device)
{
	struct dir_list *list = NULL, *new_list = NULL;
	struct dir_list *current;
	char	*ret_path = NULL;

	/*
	 * Add the starting directories to search...
	 */
	add_to_dirlist("/devices", &list);
	add_to_dirlist("/devfs", &list);
	add_to_dirlist("/dev", &list);

	while (list) {
		current = list;
		list = list->next;
#ifdef DEBUG
		printf("Scanning directory %s\n", current->name);
#endif
		scan_dir(current->name, device, &new_list, &ret_path);
		free(current->name);
		free(current);
		if (ret_path)
			break;
		/*
		 * If we're done checking at this level, descend to
		 * the next level of subdirectories. (breadth-first)
		 */
		if (list == 0) {
			list = new_list;
			new_list = 0;
		}
	}
	free_dirlist(&list);
	free_dirlist(&new_list);
	return ret_path;
}


#ifdef DEBUG
int main(int argc, char** argv)
{
	char	*devname, *tmp;
	int	major, minor;
	dev_t	device;
	const char *errmsg = "Cannot parse %s: %s\n";

	if ((argc != 2) && (argc != 3)) {
		fprintf(stderr, "Usage: %s device_number\n", argv[0]);
		fprintf(stderr, "\t: %s major minor\n", argv[0]);
		exit(1);
	}
	if (argc == 2) {
		device = strtoul(argv[1], &tmp, 0);
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
		device = makedev(major, minor);
		printf("Looking for device 0x%04x (%d:%d)\n", device,
		       major, minor);
	}
	devname = ext2fs_find_block_device(device);
	if (devname) {
		printf("Found device!  %s\n", devname);
		free(devname);
	} else {
		printf("Cannot find device.\n");
	}
	return 0;
}

#endif
