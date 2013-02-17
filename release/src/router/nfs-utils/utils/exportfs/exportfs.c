/*
 * utils/exportfs/exportfs.c
 *
 * Export file systems to knfsd
 *
 * Copyright (C) 1995, 1996, 1997 Olaf Kirch <okir@monad.swb.de>
 *
 * Extensive changes, 1999, Neil Brown <neilb@cse.unsw.edu.au>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/vfs.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <netdb.h>
#include <errno.h>
#include "xmalloc.h"
#include "misc.h"
#include "nfslib.h"
#include "exportfs.h"
#include "xmalloc.h"
#include "xlog.h"

static void	export_all(int verbose);
static void	exportfs(char *arg, char *options, int verbose);
static void	unexportfs(char *arg, int verbose);
static void	exports_update(int verbose);
static void	dump(int verbose);
static void	error(nfs_export *exp, int err);
static void	usage(const char *progname);
static void	validate_export(nfs_export *exp);

int
main(int argc, char **argv)
{
	char	*options = NULL;
	char	*progname = NULL;
	int	f_export = 1;
	int	f_all = 0;
	int	f_verbose = 0;
	int	f_reexport = 0;
	int	f_ignore = 0;
	int	i, c;
	int	new_cache = 0;
	int	force_flush = 0;

	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];

	xlog_open(progname);
	xlog_stderr(1);
	xlog_syslog(0);

	export_errno = 0;

	while ((c = getopt(argc, argv, "aio:ruvf")) != EOF) {
		switch(c) {
		case 'a':
			f_all = 1;
			break;
		case 'i':
			f_ignore = 1;
			break;
		case 'o':
			options = optarg;
			break;
		case 'r':
			f_reexport = 1;
			f_all = 1;
			break;
		case 'u':
			f_export = 0;
			break;
		case 'v':
			f_verbose = 1;
			break;
		case 'f':
			force_flush = 1;
			break;
		default:
			usage(progname);
			break;
		}
	}

	if (optind != argc && f_all) {
		xlog(L_ERROR, "extra arguments are not permitted with -a or -r");
		return 1;
	}
	if (f_ignore && (f_all || ! f_export)) {
		xlog(L_ERROR, "-i not meaningful with -a, -r or -u");
		return 1;
	}
	if (f_reexport && ! f_export) {
		xlog(L_ERROR, "-r and -u are incompatible");
		return 1;
	}
	new_cache = check_new_cache();
	if (optind == argc && ! f_all) {
		if (force_flush) {
			if (new_cache)
				cache_flush(1);
			else {
				xlog(L_ERROR, "-f is available only "
					"with new cache controls. "
					"Mount /proc/fs/nfsd first");
				return 1;
			}
			return 0;
		} else {
			xtab_export_read();
			dump(f_verbose);
			return 0;
		}
	}
	if (f_export && ! f_ignore)
		export_read(_PATH_EXPORTS);
	if (f_export) {
		if (f_all)
			export_all(f_verbose);
		else
			for (i = optind; i < argc ; i++)
				exportfs(argv[i], options, f_verbose);
	}
	/* If we are unexporting everything, then
	 * don't care about what should be exported, as that
	 * may require DNS lookups..
	 */
	if (! ( !f_export && f_all)) {
		/* note: xtab_*_read does not update entries if they already exist,
		 * so this will not lose new options
		 */
		if (!f_reexport)
			xtab_export_read();
		if (!f_export)
			for (i = optind ; i < argc ; i++)
				unexportfs(argv[i], f_verbose);
		if (!new_cache)
			rmtab_read();
	}
	if (!new_cache) {
		xtab_mount_read();
		exports_update(f_verbose);
	}
	xtab_export_write();
	if (new_cache)
		cache_flush(force_flush);
	if (!new_cache)
		xtab_mount_write();

	return export_errno;
}

static void
exports_update_one(nfs_export *exp, int verbose)
{
		/* check mountpoint option */
	if (exp->m_mayexport &&
	    exp->m_export.e_mountpoint &&
	    !is_mountpoint(exp->m_export.e_mountpoint[0]?
			   exp->m_export.e_mountpoint:
			   exp->m_export.e_path)) {
		printf("%s not exported as %s not a mountpoint.\n",
		       exp->m_export.e_path, exp->m_export.e_mountpoint);
		exp->m_mayexport = 0;
	}
	if (exp->m_mayexport && ((exp->m_exported<1) || exp->m_changed)) {
		if (verbose)
			printf("%sexporting %s:%s to kernel\n",
			       exp->m_exported ?"re":"",
			       exp->m_client->m_hostname,
			       exp->m_export.e_path);
		if (!export_export(exp))
			error(exp, errno);
	}
	if (exp->m_exported && ! exp->m_mayexport) {
		if (verbose)
			printf("unexporting %s:%s from kernel\n",
			       exp->m_client->m_hostname,
			       exp->m_export.e_path);
		if (!export_unexport(exp))
			error(exp, errno);
	}
}


/* we synchronise intention with reality.
 * entries with m_mayexport get exported
 * entries with m_exported but not m_mayexport get unexported
 * looking at m_client->m_type == MCL_FQDN and m_client->m_type == MCL_GSS only
 */
static void
exports_update(int verbose)
{
	nfs_export 	*exp;

	for (exp = exportlist[MCL_FQDN].p_head; exp; exp=exp->m_next) {
		exports_update_one(exp, verbose);
	}
	for (exp = exportlist[MCL_GSS].p_head; exp; exp=exp->m_next) {
		exports_update_one(exp, verbose);
	}
}
			
/*
 * export_all finds all entries and
 *    marks them xtabent and mayexport so that they get exported
 */
static void
export_all(int verbose)
{
	nfs_export	*exp;
	int		i;

	for (i = 0; i < MCL_MAXTYPES; i++) {
		for (exp = exportlist[i].p_head; exp; exp = exp->m_next) {
			if (verbose)
				printf("exporting %s:%s\n",
				       exp->m_client->m_hostname, 
				       exp->m_export.e_path);
			exp->m_xtabent = 1;
			exp->m_mayexport = 1;
			exp->m_changed = 1;
			exp->m_warned = 0;
			validate_export(exp);
		}
	}
}


static void
exportfs(char *arg, char *options, int verbose)
{
	struct exportent *eep;
	nfs_export	*exp;
	struct hostent	*hp = NULL;
	char		*path;
	char		*hname = arg;
	int		htype;

	if ((path = strchr(arg, ':')) != NULL)
		*path++ = '\0';

	if (!path || *path != '/') {
		xlog(L_ERROR, "Invalid exporting option: %s", arg);
		return;
	}

	if ((htype = client_gettype(hname)) == MCL_FQDN &&
	    (hp = gethostbyname(hname)) != NULL) {
		struct hostent *hp2 = hostent_dup (hp);
		hp = gethostbyaddr(hp2->h_addr, hp2->h_length,
				   hp2->h_addrtype);
		if (hp) {
			free(hp2);
			hp = hostent_dup(hp);
		} else
			hp = hp2;
		exp = export_find(hp, path);
		hname = hp->h_name;
	} else {
		exp = export_lookup(hname, path, 0);
	}

	if (!exp) {
		if (!(eep = mkexportent(hname, path, options)) ||
		    !(exp = export_create(eep, 0))) {
			if (hp) free (hp);
			return;
		}
	} else if (!updateexportent(&exp->m_export, options)) {
		if (hp) free (hp);
		return;
	}

	if (verbose)
		printf("exporting %s:%s\n", exp->m_client->m_hostname, 
			exp->m_export.e_path);
	exp->m_xtabent = 1;
	exp->m_mayexport = 1;
	exp->m_changed = 1;
	exp->m_warned = 0;
	validate_export(exp);
	if (hp) free (hp);
}

static void
unexportfs(char *arg, int verbose)
{
	nfs_export	*exp;
	struct hostent	*hp = NULL;
	char		*path;
	char		*hname = arg;
	int		htype;

	if ((path = strchr(arg, ':')) != NULL)
		*path++ = '\0';

	if (!path || *path != '/') {
		xlog(L_ERROR, "Invalid unexporting option: %s", arg);
		return;
	}

	if ((htype = client_gettype(hname)) == MCL_FQDN) {
		if ((hp = gethostbyname(hname)) != 0) {
			hp = hostent_dup (hp);
			hname = (char *) hp->h_name;
		}
	}

	for (exp = exportlist[htype].p_head; exp; exp = exp->m_next) {
		if (path && strcmp(path, exp->m_export.e_path))
			continue;
		if (htype != exp->m_client->m_type)
			continue;
		if (htype == MCL_FQDN
		    && !matchhostname(exp->m_export.e_hostname,
					  hname))
			continue;
		if (htype != MCL_FQDN
		    && strcasecmp(exp->m_export.e_hostname, hname))
			continue;
		if (verbose) {
#if 0
			if (exp->m_exported) {
				printf("unexporting %s:%s from kernel\n",
				       exp->m_client->m_hostname,
				       exp->m_export.e_path);
			}
			else
#endif
				printf("unexporting %s:%s\n",
					exp->m_client->m_hostname, 
					exp->m_export.e_path);
		}
#if 0
		if (exp->m_exported && !export_unexport(exp))
			error(exp, errno);
#endif
		exp->m_xtabent = 0;
		exp->m_mayexport = 0;
	}

	if (hp) free (hp);
}

static int can_test(void)
{
	int fd;
	int n;
	char *setup = "nfsd 0.0.0.0 2147483647 -test-client-\n";
	fd = open("/proc/net/rpc/auth.unix.ip/channel", O_WRONLY);
	if ( fd < 0) return 0;
	n = write(fd, setup, strlen(setup));
	close(fd);
	if (n < 0)
		return 0;
	fd = open("/proc/net/rpc/nfsd.export/channel", O_WRONLY);
	if ( fd < 0) return 0;
	close(fd);
	return 1;
}

static int test_export(char *path, int with_fsid)
{
	char buf[1024];
	int fd, n;

	sprintf(buf, "-test-client- %s 3 %d -1 -1 0\n",
		path,
		with_fsid ? NFSEXP_FSID : 0);
	fd = open("/proc/net/rpc/nfsd.export/channel", O_WRONLY);
	if (fd < 0)
		return 0;
	n = write(fd, buf, strlen(buf));
	close(fd);
	if (n < 0)
		return 0;
	return 1;
}

static void
validate_export(nfs_export *exp)
{
	/* Check that the given export point is potentially exportable.
	 * We just give warnings here, don't cause anything to fail.
	 * If a path doesn't exist, or is not a dir or file, give an warning
	 * otherwise trial-export to '-test-client-' and check for failure.
	 */
	struct stat stb;
	char *path = exp->m_export.e_path;
	struct statfs64 stf;
	int fs_has_fsid = 0;

	if (stat(path, &stb) < 0) {
		xlog(L_ERROR, "Failed to stat %s: %m \n", path);
		return;
	}
	if (!S_ISDIR(stb.st_mode) && !S_ISREG(stb.st_mode)) {
		xlog(L_ERROR, "%s is neither a directory nor a file. "
			"Remote access will fail", path);
		return;
	}
	if (!can_test())
		return;

	if (!statfs64(path, &stf) &&
	    (stf.f_fsid.__val[0] || stf.f_fsid.__val[1]))
		fs_has_fsid = 1;

	if ((exp->m_export.e_flags & NFSEXP_FSID) || exp->m_export.e_uuid ||
	    fs_has_fsid) {
		if ( !test_export(path, 1)) {
			xlog(L_ERROR, "%s does not support NFS export", path);
			return;
		}
	} else if ( ! test_export(path, 0)) {
		if (test_export(path, 1))
			xlog(L_ERROR, "%s requires fsid= for NFS export", path);
		else
			xlog(L_ERROR, "%s does not support NFS export", path);
		return;

	}
}


static char
dumpopt(char c, char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	printf("%c", c);
	vprintf(fmt, ap);
	va_end(ap);
	return ',';
}

static void
dump(int verbose)
{
	nfs_export	*exp;
	struct exportent *ep;
	int		htype;
	char		*hname, c;

	for (htype = 0; htype < MCL_MAXTYPES; htype++) {
		for (exp = exportlist[htype].p_head; exp; exp = exp->m_next) {
			ep = &exp->m_export;
			if (!exp->m_xtabent)
			    continue; /* neilb */
			if (htype == MCL_ANONYMOUS)
				hname = "<world>";
			else
				hname = ep->e_hostname;
			if (strlen(ep->e_path) > 14)
				printf("%-14s\n\t\t%s", ep->e_path, hname);
			else
				printf("%-14s\t%s", ep->e_path, hname);
			if (!verbose) {
				printf("\n");
				continue;
			}
			c = '(';
			if (ep->e_flags & NFSEXP_READONLY)
				c = dumpopt(c, "ro");
			else
				c = dumpopt(c, "rw");
			if (ep->e_flags & NFSEXP_ASYNC)
				c = dumpopt(c, "async");
			if (ep->e_flags & NFSEXP_GATHERED_WRITES)
				c = dumpopt(c, "wdelay");
			if (ep->e_flags & NFSEXP_NOHIDE)
				c = dumpopt(c, "nohide");
			if (ep->e_flags & NFSEXP_CROSSMOUNT)
				c = dumpopt(c, "crossmnt");
			if (ep->e_flags & NFSEXP_INSECURE_PORT)
				c = dumpopt(c, "insecure");
			if (ep->e_flags & NFSEXP_ROOTSQUASH)
				c = dumpopt(c, "root_squash");
			else
				c = dumpopt(c, "no_root_squash");
			if (ep->e_flags & NFSEXP_ALLSQUASH)
				c = dumpopt(c, "all_squash");
			if (ep->e_flags & NFSEXP_NOSUBTREECHECK)
				c = dumpopt(c, "no_subtree_check");
			if (ep->e_flags & NFSEXP_NOAUTHNLM)
				c = dumpopt(c, "insecure_locks");
			if (ep->e_flags & NFSEXP_NOACL)
				c = dumpopt(c, "no_acl");
			if (ep->e_flags & NFSEXP_FSID)
				c = dumpopt(c, "fsid=%d", ep->e_fsid);
			if (ep->e_uuid)
				c = dumpopt(c, "fsid=%s", ep->e_uuid);
			if (ep->e_mountpoint)
				c = dumpopt(c, "mountpoint%s%s", 
					    ep->e_mountpoint[0]?"=":"", 
					    ep->e_mountpoint);
			if (ep->e_anonuid != 65534)
				c = dumpopt(c, "anonuid=%d", ep->e_anonuid);
			if (ep->e_anongid != 65534)
				c = dumpopt(c, "anongid=%d", ep->e_anongid);
			switch(ep->e_fslocmethod) {
			case FSLOC_NONE:
				break;
			case FSLOC_REFER:
				c = dumpopt(c, "refer=%s", ep->e_fslocdata);
				break;
			case FSLOC_REPLICA:
				c = dumpopt(c, "replicas=%s", ep->e_fslocdata);
				break;
#ifdef DEBUG
			case FSLOC_STUB:
				c = dumpopt(c, "fsloc=stub");
				break;
#endif
			}
			secinfo_show(stdout, ep);
			printf("%c\n", (c != '(')? ')' : ' ');
		}
	}
}

static void
error(nfs_export *exp, int err)
{
	xlog(L_ERROR, "%s:%s: %s\n", exp->m_client->m_hostname,
		exp->m_export.e_path, strerror(err));
}

static void
usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-aruv] [host:/path]\n", progname);
	exit(1);
}
