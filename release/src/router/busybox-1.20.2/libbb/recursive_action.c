/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"

#undef DEBUG_RECURS_ACTION

/*
 * Walk down all the directories under the specified
 * location, and do something (something specified
 * by the fileAction and dirAction function pointers).
 *
 * Unfortunately, while nftw(3) could replace this and reduce
 * code size a bit, nftw() wasn't supported before GNU libc 2.1,
 * and so isn't sufficiently portable to take over since glibc2.1
 * is so stinking huge.
 */

static int FAST_FUNC true_action(const char *fileName UNUSED_PARAM,
		struct stat *statbuf UNUSED_PARAM,
		void* userData UNUSED_PARAM,
		int depth UNUSED_PARAM)
{
	return TRUE;
}

/* fileAction return value of 0 on any file in directory will make
 * recursive_action() return 0, but it doesn't stop directory traversal
 * (fileAction/dirAction will be called on each file).
 *
 * If !ACTION_RECURSE, dirAction is called on the directory and its
 * return value is returned from recursive_action(). No recursion.
 *
 * If ACTION_RECURSE, recursive_action() is called on each directory.
 * If any one of these calls returns 0, current recursive_action() returns 0.
 *
 * If ACTION_DEPTHFIRST, dirAction is called after recurse.
 * If it returns 0, the warning is printed and recursive_action() returns 0.
 *
 * If !ACTION_DEPTHFIRST, dirAction is called before we recurse.
 * Return value of 0 (FALSE) or 2 (SKIP) prevents recursion
 * into that directory, instead recursive_action() returns 0 (if FALSE)
 * or 1 (if SKIP)
 *
 * ACTION_FOLLOWLINKS mainly controls handling of links to dirs.
 * 0: lstat(statbuf). Calls fileAction on link name even if points to dir.
 * 1: stat(statbuf). Calls dirAction and optionally recurse on link to dir.
 */

int FAST_FUNC recursive_action(const char *fileName,
		unsigned flags,
		int FAST_FUNC (*fileAction)(const char *fileName, struct stat *statbuf, void* userData, int depth),
		int FAST_FUNC (*dirAction)(const char *fileName, struct stat *statbuf, void* userData, int depth),
		void* userData,
		unsigned depth)
{
	struct stat statbuf;
	unsigned follow;
	int status;
	DIR *dir;
	struct dirent *next;

	if (!fileAction) fileAction = true_action;
	if (!dirAction) dirAction = true_action;

	follow = ACTION_FOLLOWLINKS;
	if (depth == 0)
		follow = ACTION_FOLLOWLINKS | ACTION_FOLLOWLINKS_L0;
	follow &= flags;
	status = (follow ? stat : lstat)(fileName, &statbuf);
	if (status < 0) {
#ifdef DEBUG_RECURS_ACTION
		bb_error_msg("status=%d flags=%x", status, flags);
#endif
		if ((flags & ACTION_DANGLING_OK)
		 && errno == ENOENT
		 && lstat(fileName, &statbuf) == 0
		) {
			/* Dangling link */
			return fileAction(fileName, &statbuf, userData, depth);
		}
		goto done_nak_warn;
	}

	/* If S_ISLNK(m), then we know that !S_ISDIR(m).
	 * Then we can skip checking first part: if it is true, then
	 * (!dir) is also true! */
	if ( /* (!(flags & ACTION_FOLLOWLINKS) && S_ISLNK(statbuf.st_mode)) || */
	 !S_ISDIR(statbuf.st_mode)
	) {
		return fileAction(fileName, &statbuf, userData, depth);
	}

	/* It's a directory (or a link to one, and followLinks is set) */

	if (!(flags & ACTION_RECURSE)) {
		return dirAction(fileName, &statbuf, userData, depth);
	}

	if (!(flags & ACTION_DEPTHFIRST)) {
		status = dirAction(fileName, &statbuf, userData, depth);
		if (!status)
			goto done_nak_warn;
		if (status == SKIP)
			return TRUE;
	}

	dir = opendir(fileName);
	if (!dir) {
		/* findutils-4.1.20 reports this */
		/* (i.e. it doesn't silently return with exit code 1) */
		/* To trigger: "find -exec rm -rf {} \;" */
		goto done_nak_warn;
	}
	status = TRUE;
	while ((next = readdir(dir)) != NULL) {
		char *nextFile;

		nextFile = concat_subpath_file(fileName, next->d_name);
		if (nextFile == NULL)
			continue;
		/* process every file (NB: ACTION_RECURSE is set in flags) */
		if (!recursive_action(nextFile, flags, fileAction, dirAction,
						userData, depth + 1))
			status = FALSE;
//		s = recursive_action(nextFile, flags, fileAction, dirAction,
//						userData, depth + 1);
		free(nextFile);
//#define RECURSE_RESULT_ABORT 3
//		if (s == RECURSE_RESULT_ABORT) {
//			closedir(dir);
//			return s;
//		}
//		if (s == FALSE)
//			status = FALSE;
	}
	closedir(dir);

	if (flags & ACTION_DEPTHFIRST) {
		if (!dirAction(fileName, &statbuf, userData, depth))
			goto done_nak_warn;
	}

	return status;

 done_nak_warn:
	if (!(flags & ACTION_QUIET))
		bb_simple_perror_msg(fileName);
	return FALSE;
}
