/* vi: set sw=4 ts=4: */
/*
 * iod.c		- Iterate a function on each entry of a directory
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/*
 * History:
 * 93/10/30	- Creation
 */

#include "e2p.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int iterate_on_dir (const char * dir_name,
		    int (*func) (const char *, struct dirent *, void *),
		    void * private)
{
	DIR * dir;
	struct dirent *de, *dep;
	int	max_len, len;

	max_len = PATH_MAX + sizeof(struct dirent);
	de = xmalloc(max_len+1);
	memset(de, 0, max_len+1);

	dir = opendir (dir_name);
	if (dir == NULL) {
		free(de);
		return -1;
	}
	while ((dep = readdir (dir))) {
		len = sizeof(struct dirent);
		if (len < dep->d_reclen)
			len = dep->d_reclen;
		if (len > max_len)
			len = max_len;
		memcpy(de, dep, len);
		(*func) (dir_name, de, private);
	}
	free(de);
	closedir(dir);
	return 0;
}
