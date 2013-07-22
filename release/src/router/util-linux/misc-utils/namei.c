/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
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
 *
 * The original namei(1) was writtent by:
 *	Roger S. Southwick (May 2, 1990)
 *	Steve Tell (March 28, 1991)
 *	Arkadiusz Mikiewicz (1999-02-22)
 *	Li Zefan (2007-09-10).
 */

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pwd.h>
#include <grp.h>

#include "c.h"
#include "xalloc.h"
#include "nls.h"
#include "widechar.h"
#include "strutils.h"

#ifndef MAXSYMLINKS
#define MAXSYMLINKS 256
#endif

#ifndef LOGIN_NAME_MAX
#define LOGIN_NAME_MAX 256
#endif

#define NAMEI_NOLINKS	(1 << 1)
#define NAMEI_MODES	(1 << 2)
#define NAMEI_MNTS	(1 << 3)
#define NAMEI_OWNERS	(1 << 4)
#define NAMEI_VERTICAL	(1 << 5)


struct namei {
	struct stat	st;		/* item lstat() */
	char		*name;		/* item name */
	char		*abslink;	/* absolute symlink path */
	int		relstart;	/* offset of relative path in 'abslink' */
	struct namei	*next;		/* next item */
	int		level;
	int		mountpoint;	/* is mount point */
	int		noent;		/* is this item not existing */
};

struct idcache {
	unsigned long int	id;
	char			*name;
	struct idcache		*next;
};

static int flags;
static int uwidth;		/* maximal width of username */
static int gwidth;		/* maximal width of groupname */
static struct idcache *gcache;	/* groupnames */
static struct idcache *ucache;	/* usernames */

static struct idcache *
get_id(struct idcache *ic, unsigned long int id)
{
	while(ic) {
		if (ic->id == id)
			return ic;
		ic = ic->next;
	}
	return NULL;
}

static void
free_idcache(struct idcache *ic)
{
	while(ic) {
		struct idcache *next = ic->next;
		free(ic->name);
		free(ic);
		ic = next;
	}
}

static void
add_id(struct idcache **ic, char *name, unsigned long int id, int *width)
{
	struct idcache *nc, *x;
	int w = 0;

	nc = xcalloc(1, sizeof(*nc));
	nc->id = id;

	if (name) {
#ifdef HAVE_WIDECHAR
		wchar_t wc[LOGIN_NAME_MAX + 1];

		if (mbstowcs(wc, name, LOGIN_NAME_MAX) > 0) {
			wc[LOGIN_NAME_MAX] = '\0';
			w = wcswidth(wc, LOGIN_NAME_MAX);
		}
		else
#endif
			w = strlen(name);
	}
	/* note, we ignore names with non-printable widechars */
	if (w > 0)
		nc->name = xstrdup(name);
	else if (asprintf(&nc->name, "%lu", id) == -1)
		nc->name = NULL;

	for (x = *ic; x && x->next; x = x->next);

	/* add 'nc' at end of the 'ic' list */
	if (x)
		x->next = nc;
	else
		*ic = nc;
	if (w <= 0)
		w = strlen(nc->name);
	*width = *width < w ? w : *width;

	return;
}

static void
add_uid(unsigned long int id)
{
	struct idcache *ic = get_id(ucache, id);

	if (!ic) {
		struct passwd *pw = getpwuid((uid_t) id);
		add_id(&ucache, pw ? pw->pw_name : NULL, id, &uwidth);
	}
}

static void
add_gid(unsigned long int id)
{
	struct idcache *ic = get_id(gcache, id);

	if (!ic) {
		struct group *gr = getgrgid((gid_t) id);
		add_id(&gcache, gr ? gr->gr_name : NULL, id, &gwidth);
	}
}

static void
free_namei(struct namei *nm)
{
	while (nm) {
		struct namei *next = nm->next;
		free(nm->name);
		free(nm->abslink);
		free(nm);
		nm = next;
	}
}

static void
readlink_to_namei(struct namei *nm, const char *path)
{
	char sym[PATH_MAX];
	ssize_t sz;

	sz = readlink(path, sym, sizeof(sym));
	if (sz < 1)
		err(EXIT_FAILURE, _("failed to read symlink: %s"), path);
	if (*sym != '/') {
		char *p = strrchr(path, '/');

		nm->relstart = p ? p - path : 0;
		if (nm->relstart)
			sz += nm->relstart + 1;
	}
	nm->abslink = xmalloc(sz + 1);

	if (*sym != '/' && nm->relstart) {
		/* create the absolute path from the relative symlink */
		memcpy(nm->abslink, path, nm->relstart);
		*(nm->abslink + nm->relstart) = '/';
		nm->relstart++;
		memcpy(nm->abslink + nm->relstart, sym, sz - nm->relstart);
	} else
		memcpy(nm->abslink, sym, sz);
	nm->abslink[sz] = '\0';
}

static struct stat *
dotdot_stat(const char *dirname, struct stat *st)
{
	char *path;
	size_t len;

#define DOTDOTDIR	"/.."

	if (!dirname)
		return NULL;

	len = strlen(dirname);
	path = xmalloc(len + sizeof(DOTDOTDIR));

	memcpy(path, dirname, len);
	memcpy(path + len, DOTDOTDIR, sizeof(DOTDOTDIR));

	if (stat(path, st))
		err(EXIT_FAILURE, _("could not stat '%s'"), path);
	free(path);
	return st;
}

static struct namei *
new_namei(struct namei *parent, const char *path, const char *fname, int lev)
{
	struct namei *nm;

	if (!fname)
		return NULL;
	nm = xcalloc(1, sizeof(*nm));
	if (parent)
		parent->next = nm;

	nm->level = lev;
	nm->name = xstrdup(fname);

	nm->noent = (lstat(path, &nm->st) == -1);
	if (nm->noent)
		return nm;

	if (S_ISLNK(nm->st.st_mode))
		readlink_to_namei(nm, path);
	if (flags & NAMEI_OWNERS) {
		add_uid(nm->st.st_uid);
		add_gid(nm->st.st_gid);
	}

	if ((flags & NAMEI_MNTS) && S_ISDIR(nm->st.st_mode)) {
		struct stat stbuf, *sb = NULL;

		if (parent && S_ISDIR(parent->st.st_mode))
			sb = &parent->st;
		else if (!parent || S_ISLNK(parent->st.st_mode))
			sb = dotdot_stat(path, &stbuf);

		if (sb && (sb->st_dev != nm->st.st_dev ||   /* different device */
		           sb->st_ino == nm->st.st_ino))    /* root directory */
			nm->mountpoint = 1;
	}

	return nm;
}

static struct namei *
add_namei(struct namei *parent, const char *orgpath, int start, struct namei **last)
{
	struct namei *nm = NULL, *first = NULL;
	char *fname, *end, *path;
	int level = 0;

	if (!orgpath)
		return NULL;
	if (parent) {
		nm = parent;
		level = parent->level + 1;
	}
	path = xstrdup(orgpath);
	fname = path + start;

	/* root directory */
	if (*fname == '/') {
		while (*fname == '/')
			fname++; /* eat extra '/' */
		first = nm = new_namei(nm, "/", "/", level);
	}

	for (end = fname; fname && end; ) {
		/* set end of filename */
		if (*fname) {
			end = strchr(fname, '/');
			if (end)
				*end = '\0';

			/* create a new entry */
			nm = new_namei(nm, path, fname, level);
		} else
			end = NULL;
		if (!first)
			first = nm;
		/* set begin of the next filename */
		if (end) {
			*end++ = '/';
			while (*end == '/')
				end++; /* eat extra '/' */
		}
		fname = end;
	}

	if (last)
		*last = nm;

	free(path);

	return first;
}

static int
follow_symlinks(struct namei *nm)
{
	int symcount = 0;

	for (; nm; nm = nm->next) {
		struct namei *next, *last;

		if (nm->noent)
			continue;
		if (!S_ISLNK(nm->st.st_mode))
			continue;
		if (++symcount > MAXSYMLINKS) {
			/* drop the rest of the list */
			free_namei(nm->next);
			nm->next = NULL;
			return -1;
		}
		next = nm->next;
		nm->next = add_namei(nm, nm->abslink, nm->relstart, &last);
		if (last)
			last->next = next;
		else
			nm->next = next;
	}
	return 0;
}

static int
print_namei(struct namei *nm, char *path)
{
	int i;

	if (path)
		printf("f: %s\n", path);

	for (; nm; nm = nm->next) {
		char md[11];

		if (nm->noent) {
			printf(_("%s - No such file or directory\n"), nm->name);
			return -1;
		}

		strmode(nm->st.st_mode, md);

		if (nm->mountpoint)
			md[0] = 'D';

		if (!(flags & NAMEI_VERTICAL)) {
			for (i = 0; i < nm->level; i++)
				fputs("  ", stdout);
			fputc(' ', stdout);
		}

		if (flags & NAMEI_MODES)
			printf("%s", md);
		else
			printf("%c", md[0]);

		if (flags & NAMEI_OWNERS) {
			printf(" %-*s", uwidth,
				get_id(ucache, nm->st.st_uid)->name);
			printf(" %-*s", gwidth,
				get_id(gcache, nm->st.st_gid)->name);
		}

		if (flags & NAMEI_VERTICAL)
			for (i = 0; i < nm->level; i++)
				fputs("  ", stdout);

		if (S_ISLNK(nm->st.st_mode))
			printf(" %s -> %s\n", nm->name,
					nm->abslink + nm->relstart);
		else
			printf(" %s\n", nm->name);
	}
	return 0;
}

static void usage(int rc)
{
	const char *p = program_invocation_short_name;
	FILE *out = rc == EXIT_FAILURE ? stderr : stdout;

	if (!*p)
		p = "namei";

	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options] pathname [pathname ...]\n"), p);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -h, --help          displays this help text\n"
		" -V, --version       output version information and exit\n"
		" -x, --mountpoints   show mount point directories with a 'D'\n"
		" -m, --modes         show the mode bits of each file\n"
		" -o, --owners        show owner and group name of each file\n"
		" -l, --long          use a long listing format (-m -o -v) \n"
		" -n, --nosymlinks    don't follow symlinks\n"
		" -v, --vertical      vertical align of modes and owners\n"), out);

	fputs(_("\nFor more information see namei(1).\n"), out);
	exit(rc);
}

static const struct option longopts[] =
{
	{ "help",	0, 0, 'h' },
	{ "version",    0, 0, 'V' },
	{ "mountpoints",0, 0, 'x' },
	{ "modes",	0, 0, 'm' },
	{ "owners",	0, 0, 'o' },
	{ "long",       0, 0, 'l' },
	{ "nolinks",	0, 0, 'n' },
	{ "vertical",   0, 0, 'v' },
	{ NULL,		0, 0, 0 },
};

int
main(int argc, char **argv)
{
	int c;
	int rc = EXIT_SUCCESS;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "hVlmnovx", longopts, NULL)) != -1) {
		switch(c) {
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
						  PACKAGE_STRING);
			return EXIT_SUCCESS;
		case 'l':
			flags |= (NAMEI_OWNERS | NAMEI_MODES | NAMEI_VERTICAL);
			break;
		case 'm':
			flags |= NAMEI_MODES;
			break;
		case 'n':
			flags |= NAMEI_NOLINKS;
			break;
		case 'o':
			flags |= NAMEI_OWNERS;
			break;
		case 'x':
			flags |= NAMEI_MNTS;
			break;
		case 'v':
			flags |= NAMEI_VERTICAL;
			break;
		default:
			usage(EXIT_FAILURE);
		}
	}

	if (optind == argc) {
		warnx(_("pathname argument is missing"));
		usage(EXIT_FAILURE);
	}

	for(; optind < argc; optind++) {
		char *path = argv[optind];
		struct namei *nm = NULL;
		struct stat st;

		if (stat(path, &st) != 0)
			rc = EXIT_FAILURE;

		nm = add_namei(NULL, path, 0, NULL);
		if (nm) {
			int sml = 0;
			if (!(flags & NAMEI_NOLINKS))
				sml = follow_symlinks(nm);
			if (print_namei(nm, path)) {
				rc = EXIT_FAILURE;
				continue;
			}
			free_namei(nm);
			if (sml == -1) {
				rc = EXIT_FAILURE;
				warnx(_("%s: exceeded limit of symlinks"), path);
				continue;
			}
		}
	}

	free_idcache(ucache);
	free_idcache(gcache);

	return rc;
}

