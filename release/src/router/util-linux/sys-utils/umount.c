/*
 * umount(8) -- mount a filesystem
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

#include <libmount.h>

#include "nls.h"
#include "c.h"
#include "env.h"
#include "optutils.h"
#include "exitcodes.h"

static int table_parser_errcb(struct libmnt_table *tb __attribute__((__unused__)),
			const char *filename, int line)
{
	if (filename)
		warnx(_("%s: parse error: ignore entry at line %d."),
							filename, line);
	return 0;
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
static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, _(
		" %1$s [-hV]\n"
		" %1$s -a [options]\n"
		" %1$s [options] <source> | <directory>\n"),
		program_invocation_short_name);

	fputs(USAGE_OPTIONS, out);
	fprintf(out, _(
	" -a, --all               umount all filesystems\n"
	" -c, --no-canonicalize   don't canonicalize paths\n"
	" -d, --detach-loop       if mounted loop device, also free this loop device\n"
	"     --fake              dry run; skip the umount(2) syscall\n"
	" -f, --force             force unmount (in case of an unreachable NFS system)\n"));
	fprintf(out, _(
	" -i, --internal-only     don't call the umount.<type> helpers\n"
	" -n, --no-mtab           don't write to /etc/mtab\n"
	" -l, --lazy              detach the filesystem now, and cleanup all later\n"));
	fprintf(out, _(
	" -O, --test-opts <list>  limit the set of filesystems (use with -a)\n"
	" -r, --read-only         In case unmounting fails, try to remount read-only\n"
	" -t, --types <list>      limit the set of filesystem types\n"
	" -v, --verbose           say what is being done\n"));

	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("umount(8)"));

	exit(out == stderr ? MOUNT_EX_USAGE : MOUNT_EX_SUCCESS);
}

static void __attribute__((__noreturn__)) exit_non_root(const char *option)
{
	const uid_t ruid = getuid();
	const uid_t euid = geteuid();

	if (ruid == 0 && euid != 0) {
		/* user is root, but setuid to non-root */
		if (option)
			errx(MOUNT_EX_USAGE,
				_("only root can use \"--%s\" option "
				 "(effective UID is %u)"),
					option, euid);
		errx(MOUNT_EX_USAGE, _("only root can do that "
				 "(effective UID is %u)"), euid);
	}
	if (option)
		errx(MOUNT_EX_USAGE, _("only root can use \"--%s\" option"), option);
	errx(MOUNT_EX_USAGE, _("only root can do that"));
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

static int mk_exit_code(struct libmnt_context *cxt, int rc)
{
	int syserr;
	const char *tgt = mnt_context_get_target(cxt);

	if (mnt_context_helper_executed(cxt))
		/*
		 * /sbin/umount.<type> called, return status
		 */
		return mnt_context_get_helper_status(cxt);

	if (rc == 0 && mnt_context_get_status(cxt) == 1)
		/*
		 * Libmount success && syscall success.
		 */
		return MOUNT_EX_SUCCESS;


	if (!mnt_context_syscall_called(cxt)) {
		/*
		 * libmount errors (extra library checks)
		 */
		return handle_generic_errors(rc, _("%s: umount failed"), tgt);

	} else if (mnt_context_get_syscall_errno(cxt) == 0) {
		/*
		 * umount(2) syscall success, but something else failed
		 * (probably error in mtab processing).
		 */
		if (rc < 0)
			return handle_generic_errors(rc,
				_("%s: filesystem umounted, but mount(8) failed"),
				tgt);

		return MOUNT_EX_SOFTWARE;	/* internal error */

	}

	/*
	 * umount(2) errors
	 */
	syserr = mnt_context_get_syscall_errno(cxt);

	switch(syserr) {
	case ENXIO:
		warnx(_("%s: invalid block device"), tgt);	/* ??? */
		break;
	case EINVAL:
		warnx(_("%s: not mounted"), tgt);
		break;
	case EIO:
		warnx(_("%s: can't write superblock"), tgt);
		break;
	case EBUSY:
		warnx(_("%s: target is busy.\n"
		       "        (In some cases useful info about processes that use\n"
		       "         the device is found by lsof(8) or fuser(1))"),
			tgt);
		break;
	case ENOENT:
		warnx(_("%s: not found"), tgt);
		break;
	case EPERM:
		warnx(_("%s: must be superuser to umount"), tgt);
		break;
	case EACCES:
		warnx(_("%s: block devices not permitted on fs"), tgt);
		break;
	default:
		errno = syserr;
		warn(_("%s"), tgt);
		break;
	}
	return MOUNT_EX_FAIL;
}

static int umount_all(struct libmnt_context *cxt)
{
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;
	int mntrc, ignored, rc = 0;

	itr = mnt_new_iter(MNT_ITER_BACKWARD);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		return -ENOMEM;
	}

	while (mnt_context_next_umount(cxt, itr, &fs, &mntrc, &ignored) == 0) {

		const char *tgt = mnt_fs_get_target(fs);

		if (ignored) {
			if (mnt_context_is_verbose(cxt))
				printf(_("%-25s: ignored\n"), tgt);
		} else {
			rc |= mk_exit_code(cxt, mntrc);

			if (mnt_context_is_verbose(cxt))
				printf("%-25s: successfully umounted\n", tgt);
		}
	}

	mnt_free_iter(itr);
	return rc;
}

static int umount_one(struct libmnt_context *cxt, const char *spec)
{
	int rc;

	if (!spec)
		return -EINVAL;

	if (mnt_context_set_target(cxt, spec))
		err(MOUNT_EX_SYSERR, _("failed to set umount target"));

	rc = mnt_context_umount(cxt);
	rc = mk_exit_code(cxt, rc);

	mnt_reset_context(cxt);
	return rc;
}

int main(int argc, char **argv)
{
	int c, rc = 0, all = 0;
	struct libmnt_context *cxt;
	char *types = NULL;

	enum {
		UMOUNT_OPT_FAKE = CHAR_MAX + 1,
	};

	static const struct option longopts[] = {
		{ "all", 0, 0, 'a' },
		{ "detach-loop", 0, 0, 'd' },
		{ "fake", 0, 0, UMOUNT_OPT_FAKE },
		{ "force", 0, 0, 'f' },
		{ "help", 0, 0, 'h' },
		{ "internal-only", 0, 0, 'i' },
		{ "lazy", 0, 0, 'l' },
		{ "no-canonicalize", 0, 0, 'c' },
		{ "no-mtab", 0, 0, 'n' },
		{ "read-only", 0, 0, 'r' },
		{ "test-opts", 1, 0, 'O' },
		{ "types", 1, 0, 't' },
		{ "verbose", 0, 0, 'v' },
		{ "version", 0, 0, 'V' },
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

	while ((c = getopt_long(argc, argv, "acdfhilnrO:t:vV",
					longopts, NULL)) != -1) {


		/* only few options are allowed for non-root users */
		if (mnt_context_is_restricted(cxt) && !strchr("hdilVv", c))
			exit_non_root(option_to_longopt(c, longopts));

		switch(c) {
		case 'a':
			all = 1;
			break;
		case 'c':
			mnt_context_disable_canonicalize(cxt, TRUE);
			break;
		case 'd':
			mnt_context_enable_loopdel(cxt, TRUE);
			break;
		case UMOUNT_OPT_FAKE:
			mnt_context_enable_fake(cxt, TRUE);
			break;
		case 'f':
			mnt_context_enable_force(cxt, TRUE);
			break;
		case 'h':
			usage(stdout);
			break;
		case 'i':
			mnt_context_disable_helpers(cxt, TRUE);
			break;
		case 'l':
			mnt_context_enable_lazy(cxt, TRUE);
			break;
		case 'n':
			mnt_context_disable_mtab(cxt, TRUE);
			break;
		case 'r':
			mnt_context_enable_rdonly_umount(cxt, TRUE);
			break;
		case 'O':
			if (mnt_context_set_options_pattern(cxt, optarg))
				err(MOUNT_EX_SYSERR, _("failed to set options pattern"));
			break;
		case 't':
			types = optarg;
			break;
		case 'v':
			mnt_context_enable_verbose(cxt, TRUE);
			break;
		case 'V':
			print_version();
			break;
		default:
			usage(stderr);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (all) {
		if (!types)
			types = "noproc,nodevfs,nodevpts,nosysfs,norpc_pipefs,nonfsd";

		mnt_context_set_fstype_pattern(cxt, types);
		rc = umount_all(cxt);

	} else if (argc < 1) {
		usage(stderr);

	} else while (argc--)
		rc += umount_one(cxt, *argv++);

	mnt_free_context(cxt);
	return rc;
}

