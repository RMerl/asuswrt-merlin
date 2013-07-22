/*
 * wrappers for "at" functions.
 *
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#ifndef UTIL_LINUX_AT_H
#define UTIL_LINUX_AT_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "c.h"

extern int fstat_at(int dir, const char *dirname,
			const char *filename, struct stat *st, int nofollow);

extern int open_at(int dir, const char *dirname,
			const char *filename, int flags);

extern FILE *fopen_at(int dir, const char *dirname, const char *filename,
			int flags, const char *mode);

extern ssize_t readlink_at(int dir, const char *dirname, const char *pathname,
                    char *buf, size_t bufsiz);


#endif /* UTIL_LINUX_AT_H */
