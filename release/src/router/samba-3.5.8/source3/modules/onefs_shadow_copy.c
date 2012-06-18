/*
 * Unix SMB/CIFS implementation.
 *
 * OneFS shadow copy implementation that utilizes the file system's native
 * snapshot support. This file does all of the heavy lifting.
 *
 * Copyright (C) Dave Richards, 2007
 * Copyright (C) Tim Prouty, 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <ifs/ifs_syscalls.h>
#include <sys/types.h>
#include <sys/isi_enc.h>
#include <sys/module.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "onefs_shadow_copy.h"

/* Copied from ../include/proto.h */
void become_root(void);
void unbecome_root(void);

#define	SNAPSHOT_DIRECTORY	".snapshot"

#define	MAX_VERSIONS		64

/**
 * A snapshot object.
 *
 * During snapshot enumeration, snapshots are represented by snapshot objects
 * and are stored in a snapshot set.  The snapshot object represents one
 * snapshot within the set.  An important thing to note about the set is that
 * the key of the snapshot object is the tv_sec component of the is_time
 * member.  What this means is that we only store one snapshot for each
 * second.  If multiple snapshots were created within the same second, we'll
 * keep the earliest one and ignore the rest.  Thus, not all snapshots are
 * necessarily retained.
 */
struct osc_snapshot {
	char *			is_name;
	struct timespec		is_time;
	struct osc_snapshot * 	is_next;
};

/**
 * A snapshot context object.
 *
 * Snapshot contexts are used to pass information throughout the snapshot
 * enumeration routines.  As a result, snapshot contexts are stored on the
 * stack and are both created and destroyed within a single API function.
 */
struct osc_snapshot_ctx {
	void *		osc_set;
	struct timespec	osc_mtime;
};

/**
 * A directory context.
 *
 * Directory contexts are the underlying data structured used to enumerate
 * snapshot versions.  An opendir()-, readdir()- and closedir()-like interface
 * is provided that utilizes directory contexts.  At the API level, directory
 * contexts are passed around as void pointers.  Directory contexts are
 * allocated on the heap and their lifetime is dictated by the calling
 * routine.
 */
struct osc_directory_ctx {
	size_t		idc_pos;
	size_t		idc_len;
	size_t		idc_size;
	char **		idc_version;
};

/**
 * Return a file descriptor to the STF names directory.
 *
 * Opens the STF names directory and returns a file descriptor to it.
 * Subsequent calls return the same value (avoiding the need to re-open the
 * directory repeatedly).  Caveat caller: don't close the file descriptor or
 * you will be shot!
 */
static int
osc_get_names_directory_fd(void)
{
	static int fd = -1;

	if (fd == -1) {
		become_root();
		fd = pctl2_lin_open(STF_NAMES_LIN, HEAD_SNAPID, O_RDONLY);
		unbecome_root();
	}

	return fd;
}

/**
 * Compare two time values.
 *
 * Accepts two struct timespecs and compares the tv_sec components of these
 * values.  It returns -1 if the first value preceeds the second, 0 if they
 * are equal and +1 if the first values succeeds the second.
 */
static int
osc_time_compare(const struct timespec *tsp1, const struct timespec *tsp2)
{
	return (tsp1->tv_sec < tsp2->tv_sec) ? -1 :
	       (tsp1->tv_sec > tsp2->tv_sec) ? +1 : 0;
}

/**
 * Compare two timespec values.
 *
 * Compares two timespec values.  It returns -1 if the first value preceeds
 * the second, 0 if they are equal and +1 if the first values succeeds the
 * second.
 */
static int
osc_timespec_compare(const struct timespec *tsp1, const struct timespec *tsp2)
{
	return (tsp1->tv_sec  < tsp2->tv_sec)  ? -1 :
	       (tsp1->tv_sec  > tsp2->tv_sec)  ? +1 :
	       (tsp1->tv_nsec < tsp2->tv_nsec) ? -1 :
	       (tsp1->tv_nsec > tsp2->tv_nsec) ? +1 : 0;
}

/**
 * Determine whether a timespec value is zero.
 *
 * Return 1 if the struct timespec provided is zero and 0 otherwise.
 */
static int
osc_timespec_is_zero(const struct timespec *tsp)
{
	return (tsp->tv_sec  == 0) &&
	       (tsp->tv_nsec == 0);
}

/**
 * Create a snapshot object.
 *
 * Allocates and initializes a new snapshot object.  In addition to allocating
 * space for the snapshot object itself, space is allocated for the snapshot
 * name.  Both the name and time are then copied to the new object.
 */
static struct osc_snapshot *
osc_snapshot_create(const char *name, const struct timespec *tsp)
{
	struct osc_snapshot *isp;

	isp = malloc(sizeof *isp);
	if (isp == NULL)
		goto out;

	isp->is_name = malloc(strlen(name) + 1);
	if (isp->is_name == NULL) {
		free(isp);
		isp = NULL;
		goto out;
	}

	strcpy(isp->is_name, name);
	isp->is_time = *tsp;
	isp->is_next = NULL;

 out:
	return isp;
}

/**
 * Destroy a snapshot object.
 *
 * Frees both the name and the snapshot object itself.  Appropriate NULL
 * checking is performed because counting on free to do so is immoral.
 */
static void
osc_snapshot_destroy(struct osc_snapshot *isp)
{
	if (isp != NULL) {
		if (isp->is_name != NULL)
			free(isp->is_name);
		free(isp);
	}
}

/**
 * Destroy all snapshots in the snapshot list.
 *
 * Calls osc_snapshot_destroy() on each snapshot in the list.
 */
static void
osc_snapshot_destroy_list(struct osc_snapshot *isp)
{
	struct osc_snapshot *tmp;

	while (isp != NULL) {
		tmp = isp;
		isp = isp->is_next;
		osc_snapshot_destroy(tmp);
	}
}

/**
 * Compare two snapshot objects.
 *
 * Compare two snapshot objects.  It is really just a wrapper for
 * osc_time_compare(), which compare the time value of the two snapshots.
 * N.B. time value in this context refers only to the tv_sec component.
 */
static int
osc_snapshot_compare(const void *vp1, const void *vp2)
{
	const struct osc_snapshot *isp1 = vp1;
	const struct osc_snapshot *isp2 = vp2;

	return -osc_time_compare(&isp1->is_time, &isp2->is_time);
}

/**
 * Insert a snapshot into the snapshot set.
 *
 * Inserts a new snapshot into the snapshot set.  The key for snapshots is
 * their creation time (it's actually the seconds portion of the creation
 * time).  If a duplicate snapshot is found in the set, the new snapshot is
 * added to a linked list of snapshots for that second.
 */
static void
osc_snapshot_insert(struct osc_snapshot_ctx *oscp, const char *name,
    const struct timespec *tsp, int *errorp)
{
	struct osc_snapshot *isp1;
	struct osc_snapshot **ispp;

	isp1 = osc_snapshot_create(name, tsp);
	if (isp1 == NULL) {
		*errorp = 1;
		return;
	}

	ispp = tsearch(isp1, &oscp->osc_set, osc_snapshot_compare);
	if (ispp != NULL) {
		struct osc_snapshot *isp2 = *ispp;

		/* If this is the only snapshot for this second, we're done. */
		if (isp2 == isp1)
			return;

		/* Collision: add the new snapshot to the list. */
		isp1->is_next = isp2->is_next;
		isp2->is_next = isp1;

	} else
		*errorp = 1;

}

/**
 * Process the next snapshot.
 *
 * Called for (almost) every entry in a .snapshot directory, ("." and ".." are
 * ignored in osc_process_snapshot_directory()).  All other entries are passed
 * to osc_process_snapshot(), however.  These entries can fall into one of two
 * categories: snapshot names and snapshot aliases.  We only care about
 * snapshot names (as aliases are just redundant entries).  Once it verifies
 * that name represents a valid snapshot name, it calls fstat() to get the
 * creation time of the snapshot and then calls osc_snapshot_insert() to add
 * this entry to the snapshot set.
 */
static void
osc_process_snapshot(struct osc_snapshot_ctx *oscp, const char *name,
    int *errorp)
{
	int fd;
	struct stf_stat stf_stat;
	struct stat stbuf;

	fd = osc_get_names_directory_fd();
	if (fd == -1)
		goto out;

	fd = enc_openat(fd, name, ENC_DEFAULT, O_RDONLY);
	if (fd == -1)
		goto out;

	memset(&stf_stat, 0, sizeof stf_stat);
	if (ifs_snap_stat(fd, &stf_stat) == -1)
		goto out;

	if (stf_stat.sf_type != SF_STF)
		goto out;

	if (fstat(fd, &stbuf) == -1)
		goto out;

	osc_snapshot_insert(oscp, name, &stbuf.st_birthtimespec, errorp);

 out:
	if (fd != -1)
		close(fd);
}

/**
 * Process a snapshot directory.
 *
 * Opens the snapshot directory and calls osc_process_snapshot() for each
 * entry.  (Well ok, "." and ".."  are ignored.)  The goal here is to add all
 * snapshots in the directory to the snapshot set.
 */
static void
osc_process_snapshot_directory(struct osc_snapshot_ctx *oscp, int *errorp)
{
	int fd;
	struct stat stbuf;
	DIR *dirp;
	struct dirent *dp;

	fd = osc_get_names_directory_fd();
	if (fd == -1)
		goto out;

	if (fstat(fd, &stbuf) == -1)
		goto out;

	dirp = opendir(SNAPSHOT_DIRECTORY);
	if (dirp == NULL)
		goto out;

	for (;;) {
		dp = readdir(dirp);
		if (dp == NULL)
			break;

		if (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' ||
		    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
			continue;

		osc_process_snapshot(oscp, dp->d_name, errorp);
		if (*errorp)
			break;
	}

	closedir(dirp);

	if (!*errorp)
		oscp->osc_mtime = stbuf.st_mtimespec;

 out:
	return;
}

/**
 * Initialize a snapshot context object.
 *
 * Clears all members of the context object.
 */
static void
osc_snapshot_ctx_init(struct osc_snapshot_ctx *oscp)
{
	memset(oscp, 0, sizeof *oscp);
}

/**
 * Desoy a snapshot context object.
 *
 * Frees all snapshots associated with the snapshot context and then calls
 * osc_snapshot_ctx_init() to re-initialize the context object.
 */
static void
osc_snapshot_ctx_clean(struct osc_snapshot_ctx *oscp)
{
	struct osc_snapshot *tmp;

	while (oscp->osc_set != NULL) {
		tmp = *(void **)oscp->osc_set;
		tdelete(tmp, &oscp->osc_set, osc_snapshot_compare);
		osc_snapshot_destroy_list(tmp);
	}

	osc_snapshot_ctx_init(oscp);
}

/**
 * Return the "global" snapshot context.
 *
 * We maintain a single open snapshot context.  Return a pointer to it.
 */
static struct osc_snapshot_ctx *
osc_get_snapshot_ctx(void)
{
	static struct osc_snapshot_ctx osc = { 0, { 0, 0 } };

	return &osc;
}

/**
 * Determine whether a snapshot context is still valid.
 *
 * "Valid" in this context means "reusable".  We can re-use a previous
 * snapshot context iff we successfully built a previous snapshot context
 * and no snapshots have been created or deleted since we did so.
 * A "names" directory exists within our snapshot
 * implementation in which all snapshot names are entered.  Each time a
 * snapshot is created or deleted, an entry must be added or removed.
 * When this happens the modification time on the "names" directory
 * changes.  Therefore, a snapshot context is valid iff the context
 * pointer is non-NULL, the cached modification time is non-zero
 * (zero means uninitialized), and the modification time of the "names"
 * directory matches the cached value.
 */
static int
osc_snapshot_ctx_is_valid(struct osc_snapshot_ctx *oscp)
{
	int fd;
	struct stat stbuf;

	if (oscp == NULL)
		return 0;

	if (osc_timespec_is_zero(&oscp->osc_mtime))
		return 0;

	fd = osc_get_names_directory_fd();
	if (fd == -1)
		return 0;

	if (fstat(fd, &stbuf) == -1)
		return 0;

	if (osc_timespec_compare(&oscp->osc_mtime, &stbuf.st_mtimespec) != 0)
		return 0;

	return 1;
}

/**
 * Create and initialize a directory context.
 *
 * Allocates a directory context from the heap and initializes it.
 */
static struct osc_directory_ctx *
osc_directory_ctx_create(void)
{
	struct osc_directory_ctx *idcp;

	idcp = malloc(sizeof *idcp);
	if (idcp != NULL)
		memset(idcp, 0, sizeof *idcp);

	return idcp;
}

/**
 * Destroy a directory context.
 *
 * Frees any versions associated with the directory context and then frees the
 * context itself.
 */
static void
osc_directory_ctx_destroy(struct osc_directory_ctx *idcp)
{
	int i;

	if (idcp == NULL)
		return;

	for (i = 0; i < idcp->idc_len; i++)
		free(idcp->idc_version[i]);

	free(idcp);
}

/**
 * Expand the size of a directory context's version list.
 *
 * If osc_directory_ctx_append_version() detects that the version list is too
 * small to accomodate a new version string, it called
 * osc_directory_ctx_expand_version_list() to expand the version list.
 */
static void
osc_directory_ctx_expand_version_list(struct osc_snapshot_ctx *oscp,
    struct osc_directory_ctx *idcp, int *errorp)
{
	size_t size;
	char **cpp;

	size = idcp->idc_size * 2 ?: 1;

	cpp = realloc(idcp->idc_version, size * sizeof (char *));
	if (cpp == NULL) {
		*errorp = 1;
		return;
	}

	idcp->idc_size = size;
	idcp->idc_version = cpp;
}

/**
 * Append a new version to a directory context.
 *
 * Appends a snapshot version to the
 * directory context's version list.
 */
static void
osc_directory_ctx_append_version(struct osc_snapshot_ctx *oscp,
    struct osc_directory_ctx *idcp, const struct timespec *tsp, int *errorp)
{
	char *cp;
	struct tm *tmp;
	char text[64];

	if (idcp->idc_len >= MAX_VERSIONS)
		return;

	if (idcp->idc_len >= idcp->idc_size) {
		osc_directory_ctx_expand_version_list(oscp, idcp, errorp);
		if (*errorp)
			return;
	}

	tmp = gmtime(&tsp->tv_sec);
	if (tmp == NULL) {
		*errorp = 1;
		return;
	}

	snprintf(text, sizeof text,
	    "@GMT-%04u.%02u.%02u-%02u.%02u.%02u",
	    tmp->tm_year + 1900,
	    tmp->tm_mon + 1,
	    tmp->tm_mday,
	    tmp->tm_hour,
	    tmp->tm_min,
	    tmp->tm_sec);

	cp = malloc(strlen(text) + 1);
	if (cp == NULL) {
		*errorp = 1;
		return;
	}

	strcpy(cp, text);

	idcp->idc_version[idcp->idc_len++] = cp;
}

/**
 * Make a directory context from a snapshot context.
 *
 * Once a snapshot context has been completely filled-in,
 * osc_make_directory_ctx() is used to build a directory context from it.  The
 * idea here is to create version for each snapshot in the snapshot set.
 */
static void
osc_make_directory_ctx(struct osc_snapshot_ctx *oscp,
    struct osc_directory_ctx *idcp, int *errorp)
{
	static void
	walk(const void *vp, VISIT v, int level)
	{
		const struct osc_snapshot *isp;

		if ((v != postorder && v != leaf) || *errorp)
			return;

		isp = *(const struct osc_snapshot **)(u_long)vp;

		osc_directory_ctx_append_version(oscp, idcp, &isp->is_time,
		    errorp);
	}

	twalk(oscp->osc_set, walk);
}

/**
 * Open a version directory.
 *
 * Opens a version directory.  What this really means is that
 * osc_version_opendir() returns a handle to a directory context, which can be
 * used to retrieve version strings.
 */
void *
osc_version_opendir(void)
{
	int error = 0;
	struct osc_directory_ctx *idcp;
	struct osc_snapshot_ctx *oscp;

	idcp = osc_directory_ctx_create();
	if (idcp == NULL)
		goto error_out;

	oscp = osc_get_snapshot_ctx();

	if (!osc_snapshot_ctx_is_valid(oscp)) {
		osc_snapshot_ctx_clean(oscp);
		osc_process_snapshot_directory(oscp, &error);
		if (error)
			goto error_out;
	}

	osc_make_directory_ctx(oscp, idcp, &error);
	if (error)
		goto error_out;

	goto out;

 error_out:
	if (idcp != NULL) {
		osc_directory_ctx_destroy(idcp);
		idcp = NULL;
	}

 out:
	return (void *)idcp;
}

/**
 * Read the next version directory entry.
 *
 * Returns the name of the next version in the version directory, or NULL if
 * we're at the end of the directory.  What this really does is return the
 * next version from the version list stored in the directory context.
 */
char *
osc_version_readdir(void *vp)
{
	struct osc_directory_ctx *idcp = vp;

	if (idcp == NULL)
		return NULL;

	if (idcp->idc_pos >= idcp->idc_len)
		return NULL;

	return idcp->idc_version[idcp->idc_pos++];
}

/**
 * Close the version directory.
 *
 * Destroys the underlying directory context.
 */
void
osc_version_closedir(void *vp)
{
	struct osc_directory_ctx *idcp = vp;

	if (idcp != NULL)
		osc_directory_ctx_destroy(idcp);
}

/**
 * Canonicalize a path.
 *
 * Converts paths of the form @GMT-.. to paths of the form ../.snapshot/..
 * It's not the prettiest routine I've ever written, but what the heck?
 */
char *
osc_canonicalize_path(const char *path, char *snap_component)
{
	int error = 0;
	struct osc_snapshot_ctx *oscp;
	struct tm tm;
	int n;
	struct osc_snapshot is;
	struct osc_snapshot **ispp;
	struct osc_snapshot *isp;
	char *cpath = NULL;
	char *cpath2 = NULL;
	const char *snap_component_orig = snap_component;
	struct stat sb;

	oscp = osc_get_snapshot_ctx();

	if (!osc_snapshot_ctx_is_valid(oscp)) {
		osc_snapshot_ctx_clean(oscp);
		osc_process_snapshot_directory(oscp, &error);
		if (error)
			goto out;
	}

	memset(&tm, 0, sizeof tm);
	n = sscanf(snap_component,
	    "@GMT-%4u.%2u.%2u-%2u.%2u.%2u",
	    &tm.tm_year,
	    &tm.tm_mon,
	    &tm.tm_mday,
	    &tm.tm_hour,
	    &tm.tm_min,
	    &tm.tm_sec);
	if (n != 6)
		goto out;

	tm.tm_year -= 1900;
	tm.tm_mon -= 1;

	is.is_name = NULL;
	is.is_time.tv_sec = timegm(&tm);
	is.is_time.tv_nsec = 0;

	ispp = tfind(&is, &oscp->osc_set, osc_snapshot_compare);
	if (ispp == NULL)
		goto out;
	isp = *ispp;

	/* Determine the path after "@GMT-..." */
	while (*snap_component != '/' && *snap_component != '\0')
		snap_component++;

	while (*snap_component == '/')
		snap_component++;

	cpath = malloc(strlen(SNAPSHOT_DIRECTORY) + strlen(isp->is_name) +
	    strlen(path) + 3);

	if (cpath == NULL)
		goto out;

	/*
	 * Use the first snapshot that has a successful stat for the requested
	 * path.
	 */
	while (true) {

		sprintf(cpath, "%s/%s", SNAPSHOT_DIRECTORY, isp->is_name);

		/* Append path before "@GMT-..." */
		if (snap_component_orig != path) {
			strcat(cpath, "/");
			strncat(cpath, path, snap_component_orig - path);
		}

		/* Append path after "@GMT-..." */
		if (*snap_component != '\0') {
			strcat(cpath, "/");
			strcat(cpath, snap_component);
		}

		/* If there is a valid snapshot for this file, we're done. */
		if (stat(cpath, &sb) == 0)
			break;

		/* Try the next snapshot. If this was the last one, give up. */
		isp = isp->is_next;
		if (isp == NULL)
			break;

		/* If the realloc fails, give up. */
		cpath2 = realloc(cpath, strlen(SNAPSHOT_DIRECTORY) +
		    strlen(isp->is_name) + strlen(path) + 3);
		if (cpath2 == NULL)
			break;
		cpath = cpath2;
	}

 out:
	return cpath;
}
