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

#include <libmount.h>

#include "nls.h"
#include "c.h"

/*** TODO: DOCS:
 *
 *  -p, --pass-fd	is unsupported
 *  --guess-fstype	is unsupported
 *  -c =                --no-canonicalize
 */

/* exit status */
#define EX_SUCCESS	0
#define EX_USAGE	1	/* incorrect invocation or permission */
#define EX_SYSERR	2	/* out of memory, cannot fork, ... */
#define EX_SOFTWARE	4	/* internal mount bug or wrong version */
#define EX_USER		8	/* user interrupt */
#define EX_FILEIO      16	/* problems writing, locking, ... mtab/fstab */
#define EX_FAIL	       32	/* mount failure */
#define EX_SOMEOK      64	/* some mount succeeded */

static void __attribute__((__noreturn__)) exit_non_root(const char *option)
{
	const uid_t ruid = getuid();
	const uid_t euid = geteuid();

	if (ruid == 0 && euid != 0) {
		/* user is root, but setuid to non-root */
		if (option)
			errx(EX_USAGE, _("only root can use \"--%s\" option "
					 "(effective UID is %u)"),
					option, euid);
		errx(EX_USAGE, _("only root can do that "
				 "(effective UID is %u)"), euid);
	}
	if (option)
		errx(EX_USAGE, _("only root can use \"--%s\" option"), option);
	errx(EX_USAGE, _("only root can do that"));
}

static void __attribute__((__noreturn__)) print_version(void)
{
	const char *ver = NULL;

	mnt_get_library_version(&ver);

	printf("%s from %s (libmount %s)\n",
			program_invocation_short_name, PACKAGE_STRING, ver);
	exit(EX_SUCCESS);
}

static int table_parser_errcb(struct libmnt_table *tb __attribute__((__unused__)),
			const char *filename, int line)
{
	if (filename)
		warnx(_("%s: parse error: ignore entry at line %d."),
							filename, line);
	return 0;
}

static const char *opt_to_longopt(int c, const struct option *opts)
{
	const struct option *o;

	for (o = opts; o->name; o++)
		if (o->val == c)
			return o->name;
	return NULL;
}

static void print_all(struct libmnt_context *cxt, char *pattern, int show_label)
{
	struct libmnt_table *tb;
	struct libmnt_iter *itr = NULL;
	struct libmnt_fs *fs;
	struct libmnt_cache *cache = NULL;

	if (mnt_context_get_mtab(cxt, &tb))
		err(EX_SYSERR, _("failed to read mtab"));

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		err(EX_SYSERR, _("failed to initialize libmount iterator"));
	if (show_label)
		cache = mnt_new_cache();

	while (mnt_table_next_fs(tb, itr, &fs) == 0) {
		const char *type = mnt_fs_get_fstype(fs);
		const char *src = mnt_fs_get_source(fs);
		const char *optstr = mnt_fs_get_options(fs);
		char *xsrc;

		if (type && pattern && !mnt_match_fstype(type, pattern))
			continue;

		xsrc = mnt_pretty_path(src, cache);
		printf ("%s on %s", xsrc, mnt_fs_get_target(fs));
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
 * ... -F is not supported yet (TODO)
 */
static int mount_all(struct libmnt_context *cxt,
		     int forkme __attribute__((unused)))
{
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;
	int mntrc, ignored, rc = EX_SUCCESS;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		return EX_SYSERR;
	}

	while (mnt_context_next_mount(cxt, itr, &fs, &mntrc, &ignored) == 0) {

		const char *tgt = mnt_fs_get_target(fs);

		if (ignored) {
			if (mnt_context_is_verbose(cxt))
				printf(ignored == 1 ? _("%-20s: ignored\n") :
						      _("%-20s: already mounted\n"),
						tgt);
		} else if (!mnt_context_get_status(cxt)) {
			if (mntrc > 0) {
				errno = mntrc;
				printf(_("%-20s: failed: %s\n"), tgt,
						strerror(mntrc));
				rc |= EX_FAIL;
			} else {
				printf(_("%-20s: failed\n"), tgt);
				rc |= EX_SYSERR;
			}
		} else {
			if (mnt_context_is_verbose(cxt))
				printf("%-20s: successfully mounted\n", tgt);

			rc |= EX_SOMEOK;
		}
	}

	return rc;
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fprintf(out, _("Usage:\n"
		" %1$s [-lhV]\n"
		" %1$s -a [options]\n"
		" %1$s [options] <source> | <directory>\n"
		" %1$s [options] <source> <directory>\n"
		" %1$s <operation> <mountpoint> [<target>]\n"),
		program_invocation_short_name);

	fprintf(out, _(
	"\nOptions:\n"
	" -a, --all               mount all filesystems mentioned in fstab\n"
	" -c, --no-canonicalize   don't canonicalize paths\n"
	" -f, --fake              dry run; skip the mount(2) syscall\n"
	" -F, --fork              fork off for each device (use with -a)\n"));
	fprintf(out, _(
	" -h, --help              display this help text and exit\n"
	" -i, --internal-only     don't call the mount.<type> helpers\n"
	" -l, --show-labels       lists all mounts with LABELs\n"
	" -n, --no-mtab           don't write to /etc/mtab\n"));
	fprintf(out, _(
	" -o, --options <list>    comma-separated list of mount options\n"
	" -O, --test-opts <list>  limit the set of filesystems (use with -a)\n"
	" -r, --read-only         mount the filesystem read-only (same as -o ro)\n"
	" -t, --types <list>      limit the set of filesystem types\n"));
	fprintf(out, _(
	" -v, --verbose           say what is being done\n"
	" -V, --version           display version information and exit\n"
	" -w, --read-write        mount the filesystem read-write (default)\n"));

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

	fprintf(out, _("\nFor more information see mount(8).\n"));

	exit(out == stderr ? EX_USAGE : EX_SUCCESS);
}

int main(int argc, char **argv)
{
	int c, rc = EX_SUCCESS, all = 0, forkme = 0, show_labels = 0;
	struct libmnt_context *cxt;
	char *source = NULL, *srcbuf = NULL;
	char *types = NULL;
	unsigned long oper = 0;

	static const struct option longopts[] = {
		{ "all", 0, 0, 'a' },
		{ "fake", 0, 0, 'f' },
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
		{ "types", 1, 0, 't' },
		{ "uuid", 1, 0, 'U' },
		{ "label", 1, 0, 'L'},
		{ "bind", 0, 0, 'B' },
		{ "move", 0, 0, 'M' },
		{ "rbind", 0, 0, 'R' },
		{ "make-shared", 0, 0, 136 },
		{ "make-slave", 0, 0, 137 },
		{ "make-private", 0, 0, 138 },
		{ "make-unbindable", 0, 0, 139 },
		{ "make-rshared", 0, 0, 140 },
		{ "make-rslave", 0, 0, 141 },
		{ "make-rprivate", 0, 0, 142 },
		{ "make-runbindable", 0, 0, 143 },
		{ "no-canonicalize", 0, 0, 'c' },
		{ "internal-only", 0, 0, 'i' },
		{ "show-labels", 0, 0, 'l' },
		{ NULL, 0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	mnt_init_debug(0);
	cxt = mnt_new_context();
	if (!cxt)
		err(EX_SYSERR, _("libmount context allocation failed"));

	mnt_context_set_tables_errcb(cxt, table_parser_errcb);

	while ((c = getopt_long(argc, argv, "aBcfFhilL:Mno:O:rRsU:vVwt:",
					longopts, NULL)) != -1) {

		/* only few options are allowed for non-root users */
		if (mnt_context_is_restricted(cxt) && !strchr("hlLUVv", c))
			exit_non_root(opt_to_longopt(c, longopts));

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
			forkme = 1;
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
				err(EX_SYSERR, _("failed to append options"));
			break;
		case 'v':
			mnt_context_enable_verbose(cxt, TRUE);
			break;
		case 'V':
			print_version();
			break;
		case 'w':
			if (mnt_context_append_options(cxt, "rw"))
				err(EX_SYSERR, _("failed to append options"));
			break;
		case 'o':
			if (mnt_context_append_options(cxt, optarg))
				err(EX_SYSERR, _("failed to append options"));
			break;
		case 'O':
			if (mnt_context_set_options_pattern(cxt, optarg))
				err(EX_SYSERR, _("failed to set options pattern"));
			break;
		case 'L':
		case 'U':
			if (source)
				errx(EX_USAGE, _("only one <source> may be specified"));
			if (asprintf(&srcbuf, "%s=\"%s\"",
				     c == 'L' ? "LABEL" : "UUID", optarg) <= 0)
				err(EX_SYSERR, _("failed to allocate source buffer"));
			source = srcbuf;
			break;
		case 'l':
			show_labels = 1;
			break;
		case 't':
			types = optarg;
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
		case 136:
			oper = MS_SHARED;
			break;
		case 137:
			oper = MS_SLAVE;
			break;
		case 138:
			oper = MS_PRIVATE;
			break;
		case 139:
			oper = MS_UNBINDABLE;
			break;
		case 140:
			oper = (MS_SHARED | MS_REC);
			break;
		case 141:
			oper = (MS_SLAVE | MS_REC);
			break;
		case 142:
			oper = (MS_PRIVATE | MS_REC);
			break;
		case 143:
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

	if (all) {
		/*
		 * A) Mount all
		 */
		rc = mount_all(cxt, forkme);
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

	if (oper)
		mnt_context_set_mflags(cxt, oper);

	rc = mnt_context_mount(cxt);
	if (rc) {
		/* TODO: call mnt_context_strerror() */
		rc = EX_FAIL;
	} else
		rc = EX_SUCCESS;
done:
	free(srcbuf);
	mnt_free_context(cxt);
	return rc;
}

