#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <unistd.h>
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <stddef.h>
#include <errno.h>

#ifndef S_ISLNK
#define S_ISLNK(mode) (((mode) & (_S_IFMT)) == (_S_IFLNK))
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define progver "%s: scan/change symbolic links - v1.3 - by Mark Lord\n\n"
static char *progname;
static int verbose = 0, fix_links = 0, recurse = 0, delete = 0, shorten = 0,
		testing = 0, single_fs = 1;

/*
 * tidypath removes excess slashes and "." references from a path string
 */

static int substr (char *s, char *old, char *new)
{
	char *tmp = NULL;
	int oldlen = strlen(old), newlen = 0;

	if (NULL == strstr(s, old))
		return 0;

	if (new)
		newlen = strlen(new);

	if (newlen > oldlen) {
		if ((tmp = malloc(strlen(s))) == NULL) {
			fprintf(stderr, "no memory\n");
			exit (1);
		}
	}

	while (NULL != (s = strstr(s, old))) {
		char *p, *old_s = s;

		if (new) {
			if (newlen > oldlen)
				old_s = strcpy(tmp, s);
			p = new;
			while (*p)
				*s++ = *p++;
		}
		p = old_s + oldlen;
		while ((*s++ = *p++));
	}
	if (tmp)
		free(tmp);
	return 1;
}


static int tidy_path (char *path)
{
	int tidied = 0;
	char *s, *p;

	s = path + strlen(path) - 1;
	if (s[0] != '/') {	/* tmp trailing slash simplifies things */
		s[1] = '/';
		s[2] = '\0';
	}
	while (substr(path, "/./", "/"))
		tidied = 1;
	while (substr(path, "//", "/"))
		tidied = 1;

	while ((p = strstr(path,"/../")) != NULL) {
		s = p+3;
		for (p--; p != path; p--) if (*p == '/') break;
		if (*p != '/')
			break;
		while ((*p++ = *s++));
		tidied = 1;
	}
	if (*path == '\0')
		strcpy(path,"/");
	p = path + strlen(path) - 1;
	if (p != path && *p == '/')
		*p-- = '\0';	/* remove tmp trailing slash */
	while (p != path && *p == '/') {	/* remove any others */
		*p-- = '\0';
		tidied = 1;
	}
	while (!strncmp(path,"./",2)) {
		for (p = path, s = path+2; (*p++ = *s++););
		tidied = 1;
	}
	return tidied;
}

static int shorten_path (char *path, char *abspath)
{
	static char dir[PATH_MAX];
	int shortened = 0;
	char *p;

	/* get rid of unnecessary "../dir" sequences */
	while (abspath && strlen(abspath) > 1 && (p = strstr(path,"../"))) {
		/* find innermost occurance of "../dir", and save "dir" */
		int slashes = 2;
		char *a, *s, *d = dir;
		while ((s = strstr(p+3, "../"))) {
			++slashes;
			p = s;
		}
		s = p+3;
		*d++ = '/';
		while (*s && *s != '/')
			*d++ = *s++;
		*d++ = '/';
		*d = '\0';
		if (!strcmp(dir,"//"))
			break;
		/* note: p still points at ../dir */
		if (*s != '/' || !*++s)
			break;
		a = abspath + strlen(abspath) - 1;
		while (slashes-- > 0) {
			if (a <= abspath)
				goto ughh;
			while (*--a != '/') {
				if (a <= abspath)
					goto ughh;
			}
		}
		if (strncmp(dir, a, strlen(dir)))
			break;
		while ((*p++ = *s++)); /* delete the ../dir */
		shortened = 1;
	}
ughh:
	return shortened;
}


static void fix_symlink (char *path, dev_t my_dev)
{
	static char lpath[PATH_MAX], new[PATH_MAX], abspath[PATH_MAX];
	char *p, *np, *lp, *tail, *msg;
	struct stat stbuf, lstbuf;
	int c, fix_abs = 0, fix_messy = 0, fix_long = 0;

	if ((c = readlink(path, lpath, sizeof(lpath))) == -1) {
		perror(path);
		return;
	}
	lpath[c] = '\0';	/* readlink does not null terminate it */

	/* construct the absolute address of the link */
	abspath[0] = '\0';
	if (lpath[0] != '/') {
		strcat(abspath,path);
		c = strlen(abspath);
		if ((c > 0) && (abspath[c-1] == '/'))
			abspath[c-1] = '\0'; /* cut trailing / */
		if ((p = strrchr(abspath,'/')) != NULL)
			*p = '\0'; /* cut last component */
		strcat(abspath,"/");
	}
	strcat(abspath,lpath);
	(void) tidy_path(abspath);

	/* check for various things */
	if (stat(abspath, &stbuf) == -1) {
		printf("dangling: %s -> %s\n", path, lpath);
		if (delete) {
			if (unlink (path)) {
				perror(path); 
			} else
				printf("deleted:  %s -> %s\n", path, lpath);
		}
		return;
	}

	if (single_fs)
		lstat(abspath, &lstbuf); /* if the above didn't fail, then this shouldn't */
	
	if (single_fs && lstbuf.st_dev != my_dev) {
		msg = "other_fs:";
	} else if (lpath[0] == '/') {
		msg = "absolute:";
		fix_abs = 1;
	} else if (verbose) {
		msg = "relative:";
	} else
		msg = NULL;
	fix_messy = tidy_path(strcpy(new,lpath));
	if (shorten)
		fix_long = shorten_path(new, path);
	if (!fix_abs) {
		if (fix_messy)
			msg = "messy:   ";
		else if (fix_long)
			msg = "lengthy: ";
	}
	if (msg != NULL)
		printf("%s %s -> %s\n", msg, path, lpath);
	if (!(fix_links || testing) || !(fix_messy || fix_abs || fix_long))
		return;

	if (fix_abs) {
		/* convert an absolute link to relative: */
		/* point tail at first part of lpath that differs from path */
		/* point p    at first part of path  that differs from lpath */
		(void) tidy_path(lpath);
		tail = lp = lpath;
		p = path;
		while (*p && (*p == *lp)) {
			if (*lp++ == '/') {
				tail = lp;
				while (*++p == '/');
			}
		}

		/* now create new, with "../"s followed by tail */
		np = new;
		while (*p) {
			if (*p++ == '/') {
				*np++ = '.';
				*np++ = '.';
				*np++ = '/';
				while (*p == '/') ++p;
			}
		}
		strcpy (np, tail);
		(void) tidy_path(new);
		if (shorten) (void) shorten_path(new, path);
	}
	shorten_path(new,path);
	if (!testing) {
		if (unlink (path)) {
			perror(path);
			return;
		}
		if (symlink(new, path)) {
			perror(path);
			return;
		}
	}
	printf("changed:  %s -> %s\n", path, new);
}

static void dirwalk (char *path, int pathlen, dev_t dev)
{
 	char *name;
	DIR *dfd;
	static struct stat st;
	static struct dirent *dp;

	if ((dfd = opendir(path)) == NULL) {
		perror(path);
		return;
	}

	name = path + pathlen;
	if (*(name-1) != '/')
		*name++ = '/'; 

	while ((dp = readdir(dfd)) != NULL ) {
		strcpy(name, dp->d_name);
                if (strcmp(name, ".") && strcmp(name,"..")) {
			if (lstat(path, &st) == -1) {
				perror(path);
			} else if (st.st_dev == dev) {
				if (S_ISLNK(st.st_mode)) {
					fix_symlink (path, dev);
				} else if (recurse && S_ISDIR(st.st_mode)) {
					dirwalk(path, strlen(path), dev);
				}
			}
		}
	} 
	closedir(dfd);
	path[pathlen] = '\0';
}

static void usage_error (void)
{
	fprintf(stderr, progver, progname);
	fprintf(stderr, "Usage:\t%s [-cdorstv] LINK|DIR ...\n\n", progname);
	fprintf(stderr, "Flags:"
		"\t-c == change absolute/messy links to relative\n"
		"\t-d == delete dangling links\n"
		"\t-o == warn about links across file systems\n"
		"\t-r == recurse into subdirs\n"
		"\t-s == shorten lengthy links (displayed in output only when -c not specified)\n"
		"\t-t == show what would be done by -c\n"
		"\t-v == verbose (show all symlinks)\n\n");
	exit(1);
}

int main(int argc, char **argv)
{
#if defined (_GNU_SOURCE) && defined (__GLIBC__)
	static char path[PATH_MAX+2];
	char* cwd = get_current_dir_name();
#else
	static char path[PATH_MAX+2], cwd[PATH_MAX+2];
#endif
	int dircount = 0;
	char c, *p;

	if  ((progname = (char *) strrchr(*argv, '/')) == NULL)
                progname = *argv;
        else
                progname++;

#if defined (_GNU_SOURCE) && defined (__GLIBC__)
	if (NULL == cwd) {
		fprintf(stderr,"get_current_dir_name() failed\n");
#else
	if (NULL == getcwd(cwd,PATH_MAX)) {
		fprintf(stderr,"getcwd() failed\n");
#endif
		exit (1);
	}
#if defined (_GNU_SOURCE) && defined (__GLIBC__)
	cwd = realloc(cwd, strlen(cwd)+2);
	if (cwd == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit (1);
	}
#endif
	if (!*cwd || cwd[strlen(cwd)-1] != '/')
		strcat(cwd,"/");

	while (--argc) {
		p = *++argv;
		if (*p == '-') {
			if (*++p == '\0')
				usage_error();
			while ((c = *p++)) {
				     if (c == 'c')	fix_links = 1;
				else if (c == 'd')	delete    = 1;
				else if (c == 'o')	single_fs = 0;
				else if (c == 'r')	recurse   = 1;
				else if (c == 's')	shorten   = 1;
				else if (c == 't')	testing   = 1;
				else if (c == 'v')	verbose   = 1;
				else			usage_error();
			}
		} else {
			struct stat st;
			if (*p == '/')
				*path = '\0';
			else
				strcpy(path,cwd);
			tidy_path(strcat(path, p));
			if (lstat(path, &st) == -1)
				perror(path);
			else if (S_ISLNK(st.st_mode))
				fix_symlink(path, st.st_dev);
			else
				dirwalk(path, strlen(path), st.st_dev);
			++dircount;
		}
	}
	if (dircount == 0)
		usage_error();
	exit (0);
}
