/*
 * Copyright (C) 2007 Karel Zak <kzak@redhat.com>
 *
 * This file is part of util-linux.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

struct hlpPath
{
	const char *name;
	const char *path;
};

#define DEF_HLPPATH(_p)		{ #_p, _p }

struct hlpPath paths[] =
{
	DEF_HLPPATH(_PATH_DEFPATH),
	DEF_HLPPATH(_PATH_DEFPATH_ROOT),
	DEF_HLPPATH(_PATH_DEV_TTY),
	DEF_HLPPATH(_PATH_DEV_LOOP),
	DEF_HLPPATH(_PATH_SECURETTY),
	DEF_HLPPATH(_PATH_WTMPLOCK),
	DEF_HLPPATH(_PATH_HUSHLOGIN),
	DEF_HLPPATH(_PATH_MAILDIR),
	DEF_HLPPATH(_PATH_MOTDFILE),
	DEF_HLPPATH(_PATH_NOLOGIN),
	DEF_HLPPATH(_PATH_LOGIN),
	DEF_HLPPATH(_PATH_INITTAB),
	DEF_HLPPATH(_PATH_RC),
	DEF_HLPPATH(_PATH_REBOOT),
	DEF_HLPPATH(_PATH_SINGLE),
	DEF_HLPPATH(_PATH_SHUTDOWN_CONF),
	DEF_HLPPATH(_PATH_SECURE),
	DEF_HLPPATH(_PATH_USERTTY),
	DEF_HLPPATH(_PATH_UMOUNT),
	DEF_HLPPATH(_PATH_PASSWD),
	DEF_HLPPATH(_PATH_GSHADOW),
	DEF_HLPPATH(_PATH_PTMP),
	DEF_HLPPATH(_PATH_PTMPTMP),
	DEF_HLPPATH(_PATH_GROUP),
	DEF_HLPPATH(_PATH_GTMP),
	DEF_HLPPATH(_PATH_GTMPTMP),
	DEF_HLPPATH(_PATH_SHADOW_PASSWD),
	DEF_HLPPATH(_PATH_SHADOW_PTMP),
	DEF_HLPPATH(_PATH_SHADOW_PTMPTMP),
	DEF_HLPPATH(_PATH_SHADOW_GROUP),
	DEF_HLPPATH(_PATH_SHADOW_GTMP),
	DEF_HLPPATH(_PATH_SHADOW_GTMPTMP),
	DEF_HLPPATH(_PATH_WORDS),
	DEF_HLPPATH(_PATH_WORDS_ALT),
	DEF_HLPPATH(_PATH_UMOUNT),
	DEF_HLPPATH(_PATH_FILESYSTEMS),
	DEF_HLPPATH(_PATH_PROC_SWAPS),
	DEF_HLPPATH(_PATH_PROC_FILESYSTEMS),
	DEF_HLPPATH(_PATH_MOUNTED),
	DEF_HLPPATH(_PATH_MNTTAB),
	DEF_HLPPATH(_PATH_MOUNTED_LOCK),
	DEF_HLPPATH(_PATH_MOUNTED_TMP),
	DEF_HLPPATH(_PATH_DEV_BYLABEL),
	DEF_HLPPATH(_PATH_DEV_BYUUID),
	{ NULL, NULL }
};

int
main(int argc, char **argv)
{
	struct hlpPath *p;

	if (argc == 1) {
		for (p = paths; p->name; p++)
			printf("%20s %s\n", p->name, p->path);
		exit(EXIT_SUCCESS);
	} else {
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
			printf("%s <option>\n", argv[0]);
			fputs("options:\n", stdout);
			for (p = paths; p->name; p++)
				printf("\t%s\n", p->name);
			exit(EXIT_SUCCESS);
		}

		for (p = paths; p->name; p++) {
			if (strcmp(p->name, argv[1]) == 0) {
				printf("%s\n", p->path);
				exit(EXIT_SUCCESS);
			}
		}
	}

	exit(EXIT_FAILURE);
}

