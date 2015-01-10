/*
 * mount(8) -- mount a filesystem
 *
 * Copyright (C) 2011 Red Hat, Inc. All rights reserved.
 * Written by Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <libmount.h>

#include "nls.h"
#include "c.h"
#include "env.h"
#include "optutils.h"
#include "strutils.h"
#include "xgetpass.h"
#include "exitcodes.h"

/*** TODO: DOCS:
 *
 *  --guess-fstype	is unsupported
 *
 *  --options-mode={ignore,append,prepend,replace}	MNT_OMODE_{IGNORE, ...}
 *  --options-source={fstab,mtab,disable}		MNT_OMODE_{FSTAB,MTAB,NOTAB}
 *  --options-source-force				MNT_OMODE_FORCE
 */

static int passfd = -1;
static int readwrite;

static int mk_exit_code(struct libmnt_context *cxt, int rc);

static void __attribute__((__noreturn__)) exit_non_root(const char *option)
{
	const uid_t ruid = getuid();
	const uid_t euid = geteuid();

	if (ruid == 0 && euid != 0) {
		/* user is root, but setuid to non-root */
		if (option)
			errx(MOUNT_EX_USAGE, _("only root can use \"--%s\" option "
					 "(effective UID is %u)"),
					option, euid);
		errx(MOUNT_EX_USAGE, _("only root can do that "
				 "(effective UID is %u)"), euid);
	}
	if (option)
		errx(MOUNT_EX_USAGE, _("only root can use \"--%s\" option"), option);
	errx(MOUNT_EX_USAGE, _("only root can do that"));
}

static void __attribute__((__noreturn__)) print_version(void)
{
	const char *ver = NULL;
	const char **features = NULL, **p;

	mnt_get_library_version(&ver);
	mnt_get_library_features(&features);

	printf(_("%s from %s (libmount %s"),
			program_invocation_short_name,
			PACKAGE_STRING,
			ver);
	p = features;
	while (p && *p) {
		fputs(p == features ? ": " : ", ", stdout);
		fputs(*p++, stdout);
	}
	fputs(")\n", stdout);
	exit(MOUNT_EX_SUCCESS);
}

static int table_parser_errcb(struct libmnt_table *tb __attribute__((__unused__)),
			const char *filename, int line)
{
	if (filename)
		warnx(_("%s: parse error: ignore entry at line %d."),
							filename, line);
	return 0;
}

static char *encrypt_pass_get(struct libmnt_context *cxt)
{
	if (!cxt)
		return 0;

#ifdef MCL_FUTURE
	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
		warn(_("couldn't lock into memory"));
		return NULL;
	}
#endif
	return xgetpass(passfd, _("Password: "));
}

static void encrypt_pass_release(struct libmnt_context *cxt
			__attribute__((__unused__)), char *pwd)
{
	char *p = pwd;

	while (p && *p)
		*p++ = '\0';

	free(pwd);
	munlockall();
}

static void print_all(struct libmnt_context *cxt, char *pattern, int show_label)
{
	struct libmnt_table *tb;
	struct libmnt_iter *itr = NULL;
	struct libmnt_fs *fs;
	struct libmnt_cache *cache = NULL;

	if (mnt_context_get_mtab(cxt, &tb))
		err(MOUNT_EX_SYSERR, _("failed to read mtab"));

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		err(MOUNT_EX_SYSERR, _("failed to initialize libmount iterator"));
	if (show_label)
		cache = mnt_new_cache();

	while (mnt_table_next_fs(tb, itr, &fs) == 0) {
		const char *type = mnt_fs_get_fstype(fs);
		const char *src = mnt_fs_get_source(fs);
		const char *optstr = mnt_fs_get_options(fs);
		char *xsrc = NULL;

		if (type && pattern && !mnt_match_fstype(type, pattern))
			continue;

		if (!mnt_fs_is_pseudofs(fs))
			xsrc = mnt_pretty_path(src, cache);
		printf ("%s on %s", xsrc ? xsrc : src, mnt_fs_get_target(fs));
		if (type)
			printf (" type %s", type);
		if (optstr)
			printf (" (%s)", optstr);
		if (show_label && src) {
			char *lb = mnt_cache_find_tag_value(cache, src, "LABEL");
			if (lb)
				printf (" [%s]", lb);
		}
		fputc('\n', stdout);
		free(xsrc);
	}

	mnt_free_cache(cache);
	mnt_free_iter(itr);
}

/*
 * mount -a [-F]
 */
static int mount_all(struct libmnt_context *cxt)
{
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;
	int mntrc, ignored, rc = MOUNT_EX_SUCCESS;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		return MOUNT_EX_SYSERR;
	}

	while (mnt_context_next_mount(cxt, itr, &fs, &mntrc, &ignored) == 0) {

		const char *tgt = mnt_fs_get_target(fs);

		if (ignored) {
			if (mnt_context_is_verbose(cxt))
				printf(ignored == 1 ? _("%-25s: ignored\n") :
						      _("%-25s: already mounted\n"),
						tgt);

		} else if (mnt_context_is_fork(cxt)) {
			if (mnt_context_is_verbose(cxt))
				printf("%-25s: mount successfully forked\n", tgt);
		} else {
			rc |= mk_exit_code(cxt, mntrc);

			if (mnt_context_get_status(cxt)) {
				rc |= MOUNT_EX_SOMEOK;

				if (mnt_context_is_verbose(cxt))
					printf("%-25s: successfully mounted\n", tgt);
			}
		}
	}

	if (mnt_context_is_parent(cxt)) {
		/* wait for mount --fork children */
		int nerrs = 0, nchildren = 0;

		rc = mnt_context_wait_for_children(cxt, &nchildren, &nerrs);
		if (!rc && nchildren)
			rc = nchildren == nerrs ? MOUNT_EX_FAIL : MOUNT_EX_SOMEOK;
	}

	mnt_free_iter(itr);
	return rc;
}

/*
 * Handles generic errors like ENOMEM, ...
 *
 * rc = 0 success
 *     <0 error (usually -errno)
 *
 * Returns exit status (MOUNT_EX_*) and prints error message.
 */
static int handle_generic_errors(int rc, const char *msg, ...)
{
	va_list va;

	va_start(va, msg);
	errno = -rc;

	switch(errno) {
	case EINVAL:
	case EPERM:
		vwarn(msg, va);
		rc = MOUNT_EX_USAGE;
		break;
	case ENOMEM:
		vwarn(msg, va);
		rc = MOUNT_EX_SYSERR;
		break;
	default:
		vwarn(msg, va);
		rc = MOUNT_EX_FAIL;
		break;
	}
	va_end(va);
	return rc;
}

#if defined(HAVE_LIBSELINUX) && defined(HAVE_SECURITY_GET_INITIAL_CONTEXT)
#include <selinux/selinux.h>
#include <selinux/context.h>

static void selinux_warning(struct libmnt_context *cxt, const char *tgt)
{

	if (tgt && mnt_context_is_verbose(cxt) && is_selinux_enabled() > 0) {
		security_context_t raw = NULL, def = NULL;

		if (getfilecon(tgt, &raw) > 0
		    && security_get_initial_context("file", &def) == 0) {

		if (!selinux_file_context_cmp(raw, def))
			printf(_(
	"mount: %s does not contain SELinux labels.\n"
	"       You just mounted an file system that supports labels which does not\n"
	"       contain labels, onto an SELinux box. It is likely that confined\n"
	"       applications will generate AVC messages and not be allowed access to\n"
	"       this file system.  For more details see restorecon(8) and mount(8).\n"),
				tgt);
		}
		freecon(raw);
		freecon(def);
	}
}
#else
# define selinux_warning(_x, _y)
#endif


/*
 * rc = 0 success
 *     <0 error (usually -errno or -1)
 *
 * Returns exit status (MOUNT_EX_*) and/or prints error message.
 */
static int mk_exit_code(struct libmnt_context *cxt, int rc)
{
	int syserr;
	struct stat st;
	unsigned long uflags = 0, mflags = 0;

	int restricted = mnt_context_is_restricted(cxt);
	const char *tgt = mnt_context_get_target(cxt);
	const char *src = mnt_context_get_source(cxt);

try_readonly:
	if (mnt_context_helper_executed(cxt))
		/*
		 * /sbin/mount.<type> called, return status
		 */
		return mnt_context_get_helper_status(cxt);

	if (rc == 0 && mnt_context_get_status(cxt) == 1) {
		/*
		 * Libmount success && syscall success.
		 */
		selinux_warning(cxt, tgt);

		return MOUNT_EX_SUCCESS;	/* mount(2) success */
	}

	if (!mnt_context_syscall_called(cxt)) {
		/*
		 * libmount errors (extra library checks)
		 */
		switch (rc) {
		case -EPERM:
			warnx(_("only root can mount %s on %s"), src, tgt);
			return MOUNT_EX_USAGE;
		case -EBUSY:
			warnx(_("%s is already mounted"), src);
			return MOUNT_EX_USAGE;
		}

		if (src == NULL || tgt == NULL) {
			if (mflags & MS_REMOUNT)
				warnx(_("%s not mounted"), src ? src : tgt);
			else
				warnx(_("can't find %s in %s"), src ? src : tgt,
						mnt_get_fstab_path());
			return MOUNT_EX_USAGE;
		}

		if (!mnt_context_get_fstype(cxt)) {
			if (restricted)
				warnx(_("I could not determine the filesystem type, "
					"and none was specified"));
			else
				warnx(_("you must specify the filesystem type"));
			return MOUNT_EX_USAGE;
		}
		return handle_generic_errors(rc, _("%s: mount failed"),
					     tgt ? tgt : src);

	} else if (mnt_context_get_syscall_errno(cxt) == 0) {
		/*
		 * mount(2) syscall success, but something else failed
		 * (probably error in mtab processing).
		 */
		if (rc < 0)
			return handle_generic_errors(rc,
				_("%s: filesystem mounted, but mount(8) failed"),
				tgt ? tgt : src);

		return MOUNT_EX_SOFTWARE;	/* internal error */

	}

	/*
	 * mount(2) errors
	 */
	syserr = mnt_context_get_syscall_errno(cxt);

	mnt_context_get_mflags(cxt, &mflags);		/* mount(2) flags */
	mnt_context_get_user_mflags(cxt, &uflags);	/* userspace flags */

	switch(syserr) {
	case EPERM:
		if (geteuid() == 0) {
			if (stat(tgt, &st) || !S_ISDIR(st.st_mode))
				warnx(_("mount point %s is not a directory"), tgt);
			else
				warnx(_("permission denied"));
		} else
			warnx(_("must be superuser to use mount"));
	      break;

	case EBUSY:
	{
		struct libmnt_table *tb;

		if (mflags & MS_REMOUNT) {
			warnx(_("%s is busy"), tgt);
			break;
		}

		warnx(_("%s is already mounted or %s busy"), src, tgt);

		if (src && mnt_context_get_mtab(cxt, &tb) == 0) {
			struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_FORWARD);
			struct libmnt_fs *fs;

			while(mnt_table_next_fs(tb, itr, &fs) == 0) {
				const char *s = mnt_fs_get_srcpath(fs),
					   *t = mnt_fs_get_target(fs);

				if (t && s && streq_except_trailing_slash(s, src))
					fprintf(stderr, _(
						"       %s is already mounted on %s\n"), s, t);
			}
			mnt_free_iter(itr);
		}
		break;
	}
	case ENOENT:
		if (lstat(tgt, &st))
			warnx(_("mount point %s does not exist"), tgt);
		else if (stat(tgt, &st))
			warnx(_("mount point %s is a symbolic link to nowhere"), tgt);
		else if (stat(src, &st)) {
			if (uflags & MNT_MS_NOFAIL)
				return MOUNT_EX_SUCCESS;

			warnx(_("special device %s does not exist"), src);
		} else {
			errno = syserr;
			warn(_("mount(2) failed"));	/* print errno */
		}
		break;

	case ENOTDIR:
		if (stat(tgt, &st) || ! S_ISDIR(st.st_mode))
			warnx(_("mount point %s is not a directory"), tgt);
		else if (stat(src, &st) && errno == ENOTDIR) {
			if (uflags & MNT_MS_NOFAIL)
				return MOUNT_EX_SUCCESS;

			warnx(_("special device %s does not exist "
				 "(a path prefix is not a directory)"), src);
		} else {
			errno = syserr;
			warn(_("mount(2) failed"));     /* print errno */
		}
		break;

	case EINVAL:
		if (mflags & MS_REMOUNT)
			warnx(_("%s not mounted or bad option"), tgt);
		else
			warnx(_("wrong fs type, bad option, bad superblock on %s,\n"
				"       missing codepage or helper program, or other error"),
				src);

		if (mnt_fs_is_netfs(mnt_context_get_fs(cxt)))
			fprintf(stderr, _(
				"       (for several filesystems (e.g. nfs, cifs) you might\n"
				"       need a /sbin/mount.<type> helper program)\n"));

		fprintf(stderr, _(
				"       In some cases useful info is found in syslog - try\n"
				"       dmesg | tail or so\n"));
		break;

	case EMFILE:
		warnx(_("mount table full"));
		break;

	case EIO:
		warnx(_("%s: can't read superblock"), src);
		break;

	case ENODEV:
		warnx(_("unknown filesystem type '%s'"), mnt_context_get_fstype(cxt));
		break;

	case ENOTBLK:
		if (uflags & MNT_MS_NOFAIL)
			return MOUNT_EX_SUCCESS;

		if (stat(src, &st))
			warnx(_("%s is not a block device, and stat(2) fails?"), src);
		else if (S_ISBLK(st.st_mode))
			warnx(_("the kernel does not recognize %s as a block device\n"
				"       (maybe `modprobe driver'?)"), src);
		else if (S_ISREG(st.st_mode))
			warnx(_("%s is not a block device (maybe try `-o loop'?)"), src);
		else
			warnx(_(" %s is not a block device"), src);
		break;

	case ENXIO:
		if (uflags & MNT_MS_NOFAIL)
			return MOUNT_EX_SUCCESS;

		warnx(_("%s is not a valid block device"), src);
		break;

	case EACCES:
	case EROFS:
		if (mflags & MS_RDONLY)
			warnx(_("cannot mount %s read-only"), src);

		else if (readwrite)
			warnx(_("%s is write-protected but explicit `-w' flag given"), src);

		else if (mflags & MS_REMOUNT)
			warnx(_("cannot remount %s read-write, is write-protected"), src);

		else {
			warnx(_("%s is write-protected, mounting read-only"), src);

			mnt_context_reset_status(cxt);
			mnt_context_set_mflags(cxt, mflags | MS_RDONLY);
			rc = mnt_context_do_mount(cxt);
			if (!rc)
				rc = mnt_context_finalize_mount(cxt);

			goto try_readonly;
		}
		break;

	case ENOMEDIUM:
		warnx(_("no medium found on %s"), src);
		break;

	default:
		warn(_("mount %s on %s failed"), src, tgt);
		break;
	}

	return MOUNT_EX_FAIL;
}

static struct libmnt_table *append_fstab(struct libmnt_context *cxt,
					 struct libmnt_table *fstab,
					 const char *path)
{

	if (!fstab) {
		fstab = mnt_new_table();
		if (!fstab)
			err(MOUNT_EX_SYSERR, _("failed to initialize libmount table"));

		mnt_table_set_parser_errcb(fstab, table_parser_errcb);
		mnt_context_set_fstab(cxt, fstab);
	}

	if (mnt_table_parse_fstab(fstab, path))
		errx(MOUNT_EX_USAGE,_("%s: failed to parse"), path);

	return fstab;
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, _(
		" %1$s [-lhV]\n"
		" %1$s -a [options]\n"
		" %1$s [options] <source> | <directory>\n"
		" %1$s [options] <source> <directory>\n"
		" %1$s <operation> <mountpoint> [<target>]\n"),
		program_invocation_short_name);

	fputs(USAGE_OPTIONS, out);
	fprintf(out, _(
	" -a, --all               mount all filesystems mentioned in fstab\n"
	" -c, --no-canonicalize   don't canonicalize paths\n"
	" -f, --fake              dry run; skip the mount(2) syscall\n"
	" -F, --fork              fork off for each device (use with -a)\n"
	" -T, --fstab <path>      alternative file to /etc/fstab\n"));
	fprintf(out, _(
	" -h, --help              display this help text and exit\n"
	" -i, --internal-only     don't call the mount.<type> helpers\n"
	" -l, --show-labels       lists all mounts with LABELs\n"
	" -n, --no-mtab           don't write to /etc/mtab\n"));
	fprintf(out, _(
	" -o, --options <list>    comma-separated list of mount options\n"
	" -O, --test-opts <list>  limit the set of filesystems (use with -a)\n"
	" -p, --pass-fd <num>     read the passphrase from file descriptor\n"
	" -r, --read-only         mount the filesystem read-only (same as -o ro)\n"
	" -t, --types <list>      limit the set of filesystem types\n"));
	fprintf(out, _(
	" -v, --verbose           say what is being done\n"
	" -V, --version           display version information and exit\n"
	" -w, --read-write        mount the filesystem read-write (default)\n"));

	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);

	fprintf(out, _(
	"\nSource:\n"
	" -L, --label <label>     synonym for LABEL=<label>\n"
	" -U, --uuid <uuid>       synonym for UUID=<uuid>\n"
	" LABEL=<label>           specifies device by filesystem label\n"
	" UUID=<uuid>             specifies device by filesystem UUID\n"));
	fprintf(out, _(
	" <device>                specifies device by path\n"
	" <directory>             mountpoint for bind mounts (see --bind/rbind)\n"
	" <file>                  regular file for loopdev setup\n"));

	fprintf(out, _(
	"\nOperations:\n"
	" -B, --bind              mount a subtree somewhere else (same as -o bind)\n"
	" -M, --move              move a subtree to some other place\n"
	" -R, --rbind             mount a subtree and all submounts somewhere else\n"));
	fprintf(out, _(
	" --make-shared           mark a subtree as shared\n"
	" --make-slave            mark a subtree as slave\n"
	" --make-private          mark a subtree as private\n"
	" --make-unbindable       mark a subtree as unbindable\n"));
	fprintf(out, _(
	" --make-rshared          recursively mark a whole subtree as shared\n"
	" --make-rslave           recursively mark a whole subtree as slave\n"
	" --make-rprivate         recursively mark a whole subtree as private\n"
	" --make-runbindable      recursively mark a whole subtree as unbindable\n"));

	fprintf(out, USAGE_MAN_TAIL("mount(8)"));

	exit(out == stderr ? MOUNT_EX_USAGE : MOUNT_EX_SUCCESS);
}

int main(int argc, char **argv)
{
	int c, rc = MOUNT_EX_SUCCESS, all = 0, show_labels = 0;
	struct libmnt_context *cxt;
	struct libmnt_table *fstab = NULL;
	char *source = NULL, *srcbuf = NULL;
	char *types = NULL;
	unsigned long oper = 0;

	enum {
		MOUNT_OPT_SHARED = CHAR_MAX + 1,
		MOUNT_OPT_SLAVE,
		MOUNT_OPT_PRIVATE,
		MOUNT_OPT_UNBINDABLE,
		MOUNT_OPT_RSHARED,
		MOUNT_OPT_RSLAVE,
		MOUNT_OPT_RPRIVATE,
		MOUNT_OPT_RUNBINDABLE
	};

	static const struct option longopts[] = {
		{ "all", 0, 0, 'a' },
		{ "fake", 0, 0, 'f' },
		{ "fstab", 1, 0, 'T' },
		{ "fork", 0, 0, 'F' },
		{ "help", 0, 0, 'h' },
		{ "no-mtab", 0, 0, 'n' },
		{ "read-only", 0, 0, 'r' },
		{ "ro", 0, 0, 'r' },
		{ "verbose", 0, 0, 'v' },
		{ "version", 0, 0, 'V' },
		{ "read-write", 0, 0, 'w' },
		{ "rw", 0, 0, 'w' },
		{ "options", 1, 0, 'o' },
		{ "test-opts", 1, 0, 'O' },
		{ "pass-fd", 1, 0, 'p' },
		{ "types", 1, 0, 't' },
		{ "uuid", 1, 0, 'U' },
		{ "label", 1, 0, 'L'},
		{ "bind", 0, 0, 'B' },
		{ "move", 0, 0, 'M' },
		{ "rbind", 0, 0, 'R' },
		{ "make-shared", 0, 0, MOUNT_OPT_SHARED },
		{ "make-slave", 0, 0, MOUNT_OPT_SLAVE },
		{ "make-private", 0, 0, MOUNT_OPT_PRIVATE },
		{ "make-unbindable", 0, 0, MOUNT_OPT_UNBINDABLE },
		{ "make-rshared", 0, 0, MOUNT_OPT_RSHARED },
		{ "make-rslave", 0, 0, MOUNT_OPT_RSLAVE },
		{ "make-rprivate", 0, 0, MOUNT_OPT_RPRIVATE },
		{ "make-runbindable", 0, 0, MOUNT_OPT_RUNBINDABLE },
		{ "no-canonicalize", 0, 0, 'c' },
		{ "internal-only", 0, 0, 'i' },
		{ "show-labels", 0, 0, 'l' },
		{ NULL, 0, 0, 0 }
	};

	sanitize_env();
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	mnt_init_debug(0);
	cxt = mnt_new_context();
	if (!cxt)
		err(MOUNT_EX_SYSERR, _("libmount context allocation failed"));

	mnt_context_set_tables_errcb(cxt, table_parser_errcb);

	while ((c = getopt_long(argc, argv, "aBcfFhilL:Mno:O:p:rRsU:vVwt:T:",
					longopts, NULL)) != -1) {

		/* only few options are allowed for non-root users */
		if (mnt_context_is_restricted(cxt) && !strchr("hlLUVvpr", c))
			exit_non_root(option_to_longopt(c, longopts));

		switch(c) {
		case 'a':
			all = 1;
			break;
		case 'c':
			mnt_context_disable_canonicalize(cxt, TRUE);
			break;
		case 'f':
			mnt_context_enable_fake(cxt, TRUE);
			break;
		case 'F':
			mnt_context_enable_fork(cxt, TRUE);
			break;
		case 'h':
			usage(stdout);
			break;
		case 'i':
			mnt_context_disable_helpers(cxt, TRUE);
			break;
		case 'n':
			mnt_context_disable_mtab(cxt, TRUE);
			break;
		case 'r':
			if (mnt_context_append_options(cxt, "ro"))
				err(MOUNT_EX_SYSERR, _("failed to append options"));
			readwrite = 0;
			break;
		case 'v':
			mnt_context_enable_verbose(cxt, TRUE);
			break;
		case 'V':
			print_version();
			break;
		case 'w':
			if (mnt_context_append_options(cxt, "rw"))
				err(MOUNT_EX_SYSERR, _("failed to append options"));
			readwrite = 1;
			break;
		case 'o':
			if (mnt_context_append_options(cxt, optarg))
				err(MOUNT_EX_SYSERR, _("failed to append options"));
			break;
		case 'O':
			if (mnt_context_set_options_pattern(cxt, optarg))
				err(MOUNT_EX_SYSERR, _("failed to set options pattern"));
			break;
		case 'p':
			passfd = strtol_or_err(optarg,
					_("invalid passphrase file descriptor"));
			break;
		case 'L':
		case 'U':
			if (source)
				errx(MOUNT_EX_USAGE, _("only one <source> may be specified"));
			if (asprintf(&srcbuf, "%s=\"%s\"",
				     c == 'L' ? "LABEL" : "UUID", optarg) <= 0)
				err(MOUNT_EX_SYSERR, _("failed to allocate source buffer"));
			source = srcbuf;
			break;
		case 'l':
			show_labels = 1;
			break;
		case 't':
			types = optarg;
			break;
		case 'T':
			fstab = append_fstab(cxt, fstab, optarg);
			break;
		case 's':
			mnt_context_enable_sloppy(cxt, TRUE);
			break;
		case 'B':
			oper = MS_BIND;
			break;
		case 'M':
			oper = MS_MOVE;
			break;
		case 'R':
			oper = (MS_BIND | MS_REC);
			break;
		case MOUNT_OPT_SHARED:
			oper = MS_SHARED;
			break;
		case MOUNT_OPT_SLAVE:
			oper = MS_SLAVE;
			break;
		case MOUNT_OPT_PRIVATE:
			oper = MS_PRIVATE;
			break;
		case MOUNT_OPT_UNBINDABLE:
			oper = MS_UNBINDABLE;
			break;
		case MOUNT_OPT_RSHARED:
			oper = (MS_SHARED | MS_REC);
			break;
		case MOUNT_OPT_RSLAVE:
			oper = (MS_SLAVE | MS_REC);
			break;
		case MOUNT_OPT_RPRIVATE:
			oper = (MS_PRIVATE | MS_REC);
			break;
		case MOUNT_OPT_RUNBINDABLE:
			oper = (MS_UNBINDABLE | MS_REC);
			break;
		default:
			usage(stderr);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (!source && !argc && !all) {
		if (oper)
			usage(stderr);
		print_all(cxt, types, show_labels);
		goto done;
	}

	if (oper && (types || all || source))
		usage(stderr);

	if (types && (all || strchr(types, ',') ||
			     strncmp(types, "no", 2) == 0))
		mnt_context_set_fstype_pattern(cxt, types);
	else if (types)
		mnt_context_set_fstype(cxt, types);

	mnt_context_set_passwd_cb(cxt, encrypt_pass_get, encrypt_pass_release);

	if (all) {
		/*
		 * A) Mount all
		 */
		rc = mount_all(cxt);
		goto done;

	} else if (argc == 0 && source) {
		/*
		 * B) mount -L|-U
		 */
		mnt_context_set_source(cxt, source);

	} else if (argc == 1) {
		/*
		 * C) mount [-L|-U] <target>
		 *    mount <source|target>
		 */
		if (source) {
			if (mnt_context_is_restricted(cxt))
				exit_non_root(NULL);
			 mnt_context_set_source(cxt, source);
		}
		mnt_context_set_target(cxt, argv[0]);

	} else if (argc == 2 && !source) {
		/*
		 * D) mount <source> <target>
		 */
		if (mnt_context_is_restricted(cxt))
			exit_non_root(NULL);
		mnt_context_set_source(cxt, argv[0]);
		mnt_context_set_target(cxt, argv[1]);
	} else
		usage(stderr);

	if (oper) {
		/* MS_PROPAGATION operations, let's set the mount flags */
		mnt_context_set_mflags(cxt, oper);

		/* For -make* or --bind is fstab unnecessary */
		mnt_context_set_optsmode(cxt, MNT_OMODE_NOTAB);
	}

	rc = mnt_context_mount(cxt);
	rc = mk_exit_code(cxt, rc);

done:
	free(srcbuf);
	mnt_free_context(cxt);
	mnt_free_table(fstab);
	return rc;
}

