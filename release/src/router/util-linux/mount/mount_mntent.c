/* Private version of the libc *mntent() routines. */
/* Note slightly different prototypes. */

/* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 */

#include <stdio.h>
#include <string.h>		/* for index */
#include <ctype.h>		/* for isdigit */
#include <sys/stat.h>		/* for umask */

#include "mount_mntent.h"
#include "sundries.h"		/* for xmalloc */
#include "nls.h"
#include "mangle.h"

static int
is_space_or_tab (char c) {
	return (c == ' ' || c == '\t');
}

static char *
skip_spaces(char *s) {
	while (is_space_or_tab(*s))
		s++;
	return s;
}

/*
 * fstat'ing the file and allocating a buffer holding all of it
 * may be a bad idea: if the file is /proc/mounts, the stat
 * returns 0.
 * (On the other hand, mangling and unmangling is meaningless
 *  for /proc/mounts.)
 */

mntFILE *
my_setmntent (const char *file, char *mode) {
	mntFILE *mfp = xmalloc(sizeof(*mfp));
	mode_t old_umask = umask(077);

	mfp->mntent_fp = fopen(file, mode);
	umask(old_umask);
	mfp->mntent_file = xstrdup(file);
	mfp->mntent_errs = (mfp->mntent_fp == NULL);
	mfp->mntent_softerrs = 0;
	mfp->mntent_lineno = 0;
	return mfp;
}

void
my_endmntent (mntFILE *mfp) {
	if (mfp) {
		if (mfp->mntent_fp)
			fclose(mfp->mntent_fp);
		free(mfp->mntent_file);
		free(mfp);
	}
}

int
my_addmntent (mntFILE *mfp, struct my_mntent *mnt) {
	char *m1, *m2, *m3, *m4;
	int res;

	if (fseek (mfp->mntent_fp, 0, SEEK_END))
		return 1;			/* failure */

	m1 = mangle(mnt->mnt_fsname);
	m2 = mangle(mnt->mnt_dir);
	m3 = mangle(mnt->mnt_type);
	m4 = mnt->mnt_opts ? mangle(mnt->mnt_opts) : "rw";

	res = fprintf (mfp->mntent_fp, "%s %s %s %s %d %d\n",
		       m1, m2, m3, m4, mnt->mnt_freq, mnt->mnt_passno);

	free(m1);
	free(m2);
	free(m3);
	if (mnt->mnt_opts)
		free(m4);
	return (res < 0) ? 1 : 0;
}

/* Read the next entry from the file fp. Stop reading at an incorrect entry. */
struct my_mntent *
my_getmntent (mntFILE *mfp) {
	static char buf[4096];
	static struct my_mntent me;
	char *s;

 again:
	if (mfp->mntent_errs || mfp->mntent_softerrs >= ERR_MAX)
		return NULL;

	/* read the next non-blank non-comment line */
	do {
		if (fgets (buf, sizeof(buf), mfp->mntent_fp) == NULL)
			return NULL;

		mfp->mntent_lineno++;
		s = strchr (buf, '\n');
		if (s == NULL) {
			/* Missing final newline?  Otherwise extremely */
			/* long line - assume file was corrupted */
			if (feof(mfp->mntent_fp)) {
				fprintf(stderr, _("[mntent]: warning: no final "
					"newline at the end of %s\n"),
					mfp->mntent_file);
				s = strchr (buf, 0);
			} else {
				mfp->mntent_errs = 1;
				goto err;
			}
		}
		*s = 0;
		if (--s >= buf && *s == '\r')
			*s = 0;
		s = skip_spaces(buf);
	} while (*s == '\0' || *s == '#');

	me.mnt_fsname = unmangle(s, &s);
	s = skip_spaces(s);
	me.mnt_dir = unmangle(s, &s);
	s = skip_spaces(s);
	me.mnt_type = unmangle(s, &s);
	s = skip_spaces(s);
	me.mnt_opts = unmangle(s, &s);
	s = skip_spaces(s);

	if (!me.mnt_fsname || !me.mnt_dir || !me.mnt_type)
		goto err;

	if (isdigit(*s)) {
		me.mnt_freq = atoi(s);
		while(isdigit(*s)) s++;
	} else
		me.mnt_freq = 0;
	if(*s && !is_space_or_tab(*s))
		goto err;

	s = skip_spaces(s);
	if(isdigit(*s)) {
		me.mnt_passno = atoi(s);
		while(isdigit(*s)) s++;
	} else
		me.mnt_passno = 0;
	if(*s && !is_space_or_tab(*s))
		goto err;

	/* allow more stuff, e.g. comments, on this line */

	return &me;

 err:
	mfp->mntent_softerrs++;
	fprintf(stderr, _("[mntent]: line %d in %s is bad%s\n"),
		mfp->mntent_lineno, mfp->mntent_file,
		(mfp->mntent_errs || mfp->mntent_softerrs >= ERR_MAX) ?
		_("; rest of file ignored") : "");
	goto again;
}
