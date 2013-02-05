/*
 * nfsumount.c -- Linux NFS umount
 * Copyright (C) 2006 Amit Gud <agud@redhat.com>
 *
 * - Basic code and wrapper around NFS umount code originally
 *   in util-linux/mount/nfsmount.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <mntent.h>
#include <sys/mount.h>
#include <ctype.h>
#include <pwd.h>

#include "xcommon.h"
#include "fstab.h"
#include "nls.h"

#include "mount_constants.h"
#include "mount.h"
#include "error.h"
#include "network.h"
#include "parse_opt.h"
#include "parse_dev.h"

#if !defined(MNT_FORCE)
/* dare not try to include <linux/mount.h> -- lots of errors */
#define MNT_FORCE 1
#endif

#if !defined(MNT_DETACH)
#define MNT_DETACH 2
#endif

extern char *progname;
extern int nomtab;
extern int verbose;
int force;
int lazy;
int remount;


static int try_remount(const char *spec, const char *node)
{
	int res;

	res = mount(spec, node, NULL,
		    MS_MGC_VAL | MS_REMOUNT | MS_RDONLY, NULL);
	if (res == 0) {
		struct mntent remnt;
		nfs_error(_("%s: %s busy - remounted read-only"),
				progname, spec);
		remnt.mnt_type = remnt.mnt_fsname = NULL;
		remnt.mnt_dir = xstrdup(node);
		remnt.mnt_opts = xstrdup("ro");
		if (!nomtab)
			update_mtab(node, &remnt);
	} else if (errno != EBUSY) {    /* hmm ... */
		perror(_("remount"));
		nfs_error(_("%s: could not remount %s read-only"),
				progname, spec);
	}
	return res;
}

static int del_mtab(const char *spec, const char *node)
{
	int umnt_err, res;

	umnt_err = 0;
	if (lazy) {
		res = umount2 (node, MNT_DETACH);
		if (res < 0)
			umnt_err = errno;
		goto writemtab;
	}

	if (force) {
		res = umount2 (node, MNT_FORCE);
		if (res == -1) {
			int errsv = errno;
			perror(_("umount2"));
			errno = errsv;
			if (errno == ENOSYS) {
				if (verbose)
					printf(_("no umount2, trying umount...\n"));
				res = umount (node);
			}
		}
	} else
		res = umount (node);

	if (res < 0) {
		if (remount && errno == EBUSY && spec) {
			res = try_remount(spec, node);
			if (res)
				goto writemtab;
			return 0;
		} else
			umnt_err = errno;
	}

	if (res >= 0) {
		/* Umount succeeded */
		if (verbose)
			printf(_("%s umounted\n"), spec ? spec : node);
	}

 writemtab:
	if (!nomtab &&
	    (umnt_err == 0 || umnt_err == EINVAL || umnt_err == ENOENT)) {
		update_mtab(node, NULL);
	}

	if (res >= 0)
		return 0;

	if (umnt_err)
		umount_error(umnt_err, node);
	return EX_FILEIO;
}

/*
 * Discover mount server's hostname/address by examining mount options
 *
 * Returns a pointer to a string that the caller must free, on
 * success; otherwise NULL is returned.
 */
static char *nfs_umount_hostname(struct mount_options *options,
				 char *hostname)
{
	char *option;

	option = po_get(options, "mountaddr");
	if (option)
		goto out;
	option = po_get(options, "mounthost");
	if (option)
		goto out;
	option = po_get(options, "addr");
	if (option)
		goto out;

	return hostname;

out:
	free(hostname);
	return strdup(option);
}

/*
 * Returns EX_SUCCESS if mount options and device name have been
 * parsed successfully; otherwise EX_FAIL.
 */
static int nfs_umount_do_umnt(struct mount_options *options,
			      char **hostname, char **dirname)
{
	struct sockaddr_storage address;
	struct sockaddr *sap = (struct sockaddr *)&address;
	socklen_t salen = sizeof(address);
	struct pmap nfs_pmap, mnt_pmap;

	nfs_options2pmap(options, &nfs_pmap, &mnt_pmap);

	*hostname = nfs_umount_hostname(options, *hostname);
	if (!*hostname) {
		nfs_error(_("%s: out of memory"), progname);
		return EX_FAIL;
	}

	if (nfs_name_to_address(*hostname, AF_UNSPEC, sap, &salen)) {
		if (nfs_advise_umount(sap, salen, &mnt_pmap, dirname) != 0)
			return EX_SUCCESS;
		else
			nfs_error(_("%s: Server failed to unmount '%s:%s'"),
					progname, *hostname, *dirname);
	}
	return EX_FAIL;
}

/*
 * Pick up certain mount options used during the original mount
 * from /etc/mtab.  The basics include the server's IP address and
 * the server pathname of the share to unregister.
 *
 * These options might also describe the mount port, mount protocol
 * version, and transport protocol used to punch through a firewall.
 * We will need this information to get through the firewall again
 * to do the umount.
 *
 * Note that option parsing failures won't necessarily cause the
 * umount request to fail.  Those values will be left zero in the
 * pmap tuple.  If the GETPORT call later fails to disambiguate them,
 * then we fail.
 */
static int nfs_umount23(const char *devname, char *string)
{
	char *hostname, *dirname;
	struct mount_options *options;
	int result = EX_FAIL;

	if (!nfs_parse_devname(devname, &hostname, &dirname))
		return EX_USAGE;

	options = po_split(string);
	if (options) {
		result = nfs_umount_do_umnt(options, &hostname, &dirname);
		po_destroy(options);
	} else
		nfs_error(_("%s: option parsing error"), progname);

	free(hostname);
	free(dirname);
	return result;
}

static struct option umount_longopts[] =
{
  { "force", 0, 0, 'f' },
  { "help", 0, 0, 'h' },
  { "no-mtab", 0, 0, 'n' },
  { "verbose", 0, 0, 'v' },
  { "read-only", 0, 0, 'r' },
  { NULL, 0, 0, 0 }
};

static void umount_usage(void)
{
	printf(_("usage: %s dir [-fvnrlh]\n"), progname);
	printf(_("options:\n\t-f\t\tforce unmount\n"));
	printf(_("\t-v\tverbose\n"));
	printf(_("\t-n\tDo not update /etc/mtab\n"));
	printf(_("\t-r\tremount\n"));
	printf(_("\t-l\tlazy unmount\n"));
	printf(_("\t-h\tprint this help\n\n"));
}

int nfsumount(int argc, char *argv[])
{
	int c, ret;
	char *spec;
	struct mntentchn *mc;

	if (argc < 2) {
		umount_usage();
		return EX_USAGE;
	}

	spec = argv[1];

	argv += 1;
	argc -= 1;

	argv[0] = argv[-1]; /* So that getopt error messages are correct */
	while ((c = getopt_long (argc, argv, "fvnrlh",
				umount_longopts, NULL)) != -1) {

		switch (c) {
		case 'f':
			++force;
			break;
		case 'v':
			++verbose;
			break;
		case 'n':
			++nomtab;
			break;
		case 'r':
			++remount;
			break;
		case 'l':
			++lazy;
			break;
		case 'h':
		default:
			umount_usage();
			return EX_USAGE;
		}
	}
	if (optind != argc) {
		umount_usage();
		return EX_USAGE;
	}
	
	if (spec == NULL || (*spec != '/' && strchr(spec,':') == NULL)) {
		nfs_error(_("%s: %s: not found\n"), progname, spec);
		return EX_USAGE;
	}

	if (*spec == '/')
		mc = getmntdirbackward(spec, NULL);
	else
		mc = getmntdevbackward(spec, NULL);
	if (!mc && verbose)
		printf(_("Could not find %s in mtab\n"), spec);

	if (mc && strcmp(mc->m.mnt_type, "nfs") != 0 &&
	    strcmp(mc->m.mnt_type, "nfs4") != 0) {
		nfs_error(_("%s: %s on %s is not an NFS filesystem"),
				progname, mc->m.mnt_fsname, mc->m.mnt_dir);
		return EX_USAGE;
	}

	if (getuid() != 0) {
		/* only permitted if "user=" or "users" is in mount options */
		if (!mc) {
			/* umount might call us twice.  The second time there will
			 * be no entry in mtab and we should just exit quietly
			 */
			return EX_SUCCESS;

		only_root:
			nfs_error(_("%s: You are not permitted to unmount %s"),
					progname, spec);
			return EX_USAGE;
		}
		if (hasmntopt(&mc->m, "users") == NULL) {
			char *opt = hasmntopt(&mc->m, "user");
			struct passwd *pw;
			char *comma;
			int len;
			if (!opt)
				goto only_root;
			if (opt[4] != '=')
				goto only_root;
			comma = strchr(opt, ',');
			if (comma)
				len = comma - (opt + 5);
			else
				len = strlen(opt+5);
			pw = getpwuid(getuid());
			if (pw == NULL || strlen(pw->pw_name) != len
			    || strncmp(pw->pw_name, opt+5, len) != 0)
				goto only_root;
		}
	}

	ret = 0;
	if (mc) {
		if (!lazy && strcmp(mc->m.mnt_type, "nfs4") != 0)
			/* We ignore the error from nfs_umount23.
			 * If the actual umount succeeds (in del_mtab),
			 * we don't want to signal an error, as that
			 * could cause /sbin/mount to retry!
			 */
			nfs_umount23(mc->m.mnt_fsname, mc->m.mnt_opts);
		ret = del_mtab(mc->m.mnt_fsname, mc->m.mnt_dir) ?: ret;
	} else if (*spec != '/') {
		if (!lazy)
			ret = nfs_umount23(spec, "tcp,v3");
	} else
		ret = del_mtab(NULL, spec);

	return ret;
}
