/* Private version of the libc *mntent() routines. */
/* Note slightly different prototypes. */

/* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2006-06-08 Amit Gud <agud@redhat.com>
 * - Moved to nfs-utils/support/nfs from util-linux/mount
 */

#include <stdio.h>
#include <string.h>		/* for index */
#include <ctype.h>		/* for isdigit */
#include <sys/stat.h>		/* for umask */

#include "nfs_mntent.h"
#include "nls.h"
#include "xcommon.h"

/* Unfortunately the classical Unix /etc/mtab and /etc/fstab
   do not handle directory names containing spaces.
   Here we mangle them, replacing a space by \040.
   What do other Unices do? */

static unsigned char need_escaping[] = { ' ', '\t', '\n', '\\' };

static char *
mangle(const char *arg) {
	const unsigned char *s = (const unsigned char *)arg;
	char *ss, *sp;
	int n;

	n = strlen(arg);
	ss = sp = xmalloc(4*n+1);
	while(1) {
		for (n = 0; n < sizeof(need_escaping); n++) {
			if (*s == need_escaping[n]) {
				*sp++ = '\\';
				*sp++ = '0' + ((*s & 0300) >> 6);
				*sp++ = '0' + ((*s & 070) >> 3);
				*sp++ = '0' + (*s & 07);
				goto next;
			}
		}
		*sp++ = *s;
		if (*s == 0)
			break;
	next:
		s++;
	}
	return ss;
}

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

static char *
skip_nonspaces(char *s) {
	while (*s && !is_space_or_tab(*s))
		s++;
	return s;
}

#define isoctal(a) (((a) & ~7) == '0')

/* returns malloced pointer - no more strdup required */
static char *
unmangle(char *s) {
	char *ret, *ss, *sp;

	ss = skip_nonspaces(s);
	ret = sp = xmalloc(ss-s+1);
	while(s != ss) {
		if (*s == '\\' && isoctal(s[1]) && isoctal(s[2]) && isoctal(s[3])) {
			*sp++ = 64*(s[1] & 7) + 8*(s[2] & 7) + (s[3] & 7);
			s += 4;
		} else
			*sp++ = *s++;
	}
	*sp = 0;
	return ret;
}

/*
 * fstat'ing the file and allocating a buffer holding all of it
 * may be a bad idea: if the file is /proc/mounts, the stat
 * returns 0.
 * (On the other hand, mangling and unmangling is meaningless
 *  for /proc/mounts.)
 */

mntFILE *
nfs_setmntent (const char *file, char *mode) {
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
nfs_endmntent (mntFILE *mfp) {
	if (mfp) {
		if (mfp->mntent_fp)
			fclose(mfp->mntent_fp);
		if (mfp->mntent_file)
			free(mfp->mntent_file);
		free(mfp);
	}
}

int
nfs_addmntent (mntFILE *mfp, struct mntent *mnt) {
	char *m1, *m2, *m3, *m4;
	int res;

	if (fseek (mfp->mntent_fp, 0, SEEK_END))
		return 1;			/* failure */

	m1 = mangle(mnt->mnt_fsname);
	m2 = mangle(mnt->mnt_dir);
	m3 = mangle(mnt->mnt_type);
	m4 = mangle(mnt->mnt_opts);

	res = fprintf (mfp->mntent_fp, "%s %s %s %s %d %d\n",
		       m1, m2, m3, m4, mnt->mnt_freq, mnt->mnt_passno);

	free(m1);
	free(m2);
	free(m3);
	free(m4);
	return (res < 0) ? 1 : 0;
}

/* Read the next entry from the file fp. Stop reading at an incorrect entry. */
struct mntent *
nfs_getmntent (mntFILE *mfp) {
	static char buf[4096];
	static struct mntent me;
	char *s;

 again:
	if (mfp->mntent_errs || mfp->mntent_softerrs >= ERR_MAX)
		return NULL;

	/* read the next non-blank non-comment line */
	do {
		if (fgets (buf, sizeof(buf), mfp->mntent_fp) == NULL)
			return NULL;

		mfp->mntent_lineno++;
		s = index (buf, '\n');
		if (s == NULL) {
			/* Missing final newline?  Otherwise extremely */
			/* long line - assume file was corrupted */
			if (feof(mfp->mntent_fp)) {
				fprintf(stderr, _("[mntent]: warning: no final "
					"newline at the end of %s\n"),
					mfp->mntent_file);
				s = index (buf, 0);
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

	me.mnt_fsname = unmangle(s);
	s = skip_nonspaces(s);
	s = skip_spaces(s);
	me.mnt_dir = unmangle(s);
	s = skip_nonspaces(s);
	s = skip_spaces(s);
	me.mnt_type = unmangle(s);
	s = skip_nonspaces(s);
	s = skip_spaces(s);
	me.mnt_opts = unmangle(s);
	s = skip_nonspaces(s);
	s = skip_spaces(s);

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
