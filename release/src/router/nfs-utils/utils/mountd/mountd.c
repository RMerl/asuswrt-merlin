/*
 * utils/mountd/mountd.c
 *
 * Authenticate mount requests and retrieve file handle.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include "xmalloc.h"
#include "misc.h"
#include "mountd.h"
#include "rpcmisc.h"
#include "pseudoflavors.h"

extern void	cache_open(void);
extern struct nfs_fh_len *cache_get_filehandle(nfs_export *exp, int len, char *p);
extern int cache_export(nfs_export *exp, char *path);

extern void my_svc_run(void);

static void		usage(const char *, int exitcode);
static exports		get_exportlist(void);
static struct nfs_fh_len *get_rootfh(struct svc_req *, dirpath *, nfs_export **, mountstat3 *, int v3);

int reverse_resolve = 0;
int new_cache = 0;
int manage_gids;
int use_ipaddr = -1;

/* PRC: a high-availability callout program can be specified with -H
 * When this is done, the program will receive callouts whenever clients
 * send mount or unmount requests -- the callout is not needed for 2.6 kernel */
char *ha_callout_prog = NULL;

/* Number of mountd threads to start.   Default is 1 and
 * that's probably enough unless you need hundreds of
 * clients to be able to mount at once.  */
static int num_threads = 1;
/* Arbitrary limit on number of threads */
#define MAX_THREADS 64

static struct option longopts[] =
{
	{ "foreground", 0, 0, 'F' },
	{ "descriptors", 1, 0, 'o' },
	{ "debug", 1, 0, 'd' },
	{ "help", 0, 0, 'h' },
	{ "exports-file", 1, 0, 'f' },
	{ "nfs-version", 1, 0, 'V' },
	{ "no-nfs-version", 1, 0, 'N' },
	{ "version", 0, 0, 'v' },
	{ "port", 1, 0, 'p' },
	{ "no-tcp", 0, 0, 'n' },
	{ "ha-callout", 1, 0, 'H' },
	{ "state-directory-path", 1, 0, 's' },
	{ "num-threads", 1, 0, 't' },
	{ "reverse-lookup", 0, 0, 'r' },
	{ "manage-gids", 0, 0, 'g' },
	{ NULL, 0, 0, 0 }
};

static int nfs_version = -1;

static void
unregister_services (void)
{
	if (nfs_version & (0x1 << 1)) {
		pmap_unset (MOUNTPROG, MOUNTVERS);
		pmap_unset (MOUNTPROG, MOUNTVERS_POSIX);
	}
	if (nfs_version & (0x1 << 2))
		pmap_unset (MOUNTPROG, MOUNTVERS_NFSV3);
}

static void
cleanup_lockfiles (void)
{
	unlink(_PATH_XTABLCK);
	unlink(_PATH_ETABLCK);
	unlink(_PATH_RMTABLCK);
}

/* Wait for all worker child processes to exit and reap them */
static void
wait_for_workers (void)
{
	int status;
	pid_t pid;

	for (;;) {

		pid = waitpid(0, &status, 0);

		if (pid < 0) {
			if (errno == ECHILD)
				return; /* no more children */
			xlog(L_FATAL, "mountd: can't wait: %s\n",
					strerror(errno));
		}

		/* Note: because we SIG_IGN'd SIGCHLD earlier, this
		 * does not happen on 2.6 kernels, and waitpid() blocks
		 * until all the children are dead then returns with
		 * -ECHILD.  But, we don't need to do anything on the
		 * death of individual workers, so we don't care. */
		xlog(L_NOTICE, "mountd: reaped child %d, status %d\n",
				(int)pid, status);
	}
}

/* Fork num_threads worker children and wait for them */
static void
fork_workers(void)
{
	int i;
	pid_t pid;

	xlog(L_NOTICE, "mountd: starting %d threads\n", num_threads);

	for (i = 0 ; i < num_threads ; i++) {
		pid = fork();
		if (pid < 0) {
			xlog(L_FATAL, "mountd: cannot fork: %s\n",
					strerror(errno));
		}
		if (pid == 0) {
			/* worker child */

			/* Re-enable the default action on SIGTERM et al
			 * so that workers die naturally when sent them.
			 * Only the parent unregisters with pmap and
			 * hence needs to do special SIGTERM handling. */
			struct sigaction sa;
			sa.sa_handler = SIG_DFL;
			sa.sa_flags = 0;
			sigemptyset(&sa.sa_mask);
			sigaction(SIGHUP, &sa, NULL);
			sigaction(SIGINT, &sa, NULL);
			sigaction(SIGTERM, &sa, NULL);

			/* fall into my_svc_run in caller */
			return;
		}
	}

	/* in parent */
	wait_for_workers();
	unregister_services();
	cleanup_lockfiles();
	xlog(L_NOTICE, "mountd: no more workers, exiting\n");
	exit(0);
}

/*
 * Signal handler.
 */
static void 
killer (int sig)
{
	unregister_services();
	if (num_threads > 1) {
		/* play Kronos and eat our children */
		kill(0, SIGTERM);
		wait_for_workers();
	}
	cleanup_lockfiles();
	xlog (L_FATAL, "Caught signal %d, un-registering and exiting.", sig);
}

static void
sig_hup (int sig)
{
	/* don't exit on SIGHUP */
	xlog (L_NOTICE, "Received SIGHUP... Ignoring.\n", sig);
	return;
}

bool_t
mount_null_1_svc(struct svc_req *rqstp, void *argp, void *resp)
{
	return 1;
}

bool_t
mount_mnt_1_svc(struct svc_req *rqstp, dirpath *path, fhstatus *res)
{
	struct nfs_fh_len *fh;

	xlog(D_CALL, "MNT1(%s) called", *path);
	fh = get_rootfh(rqstp, path, NULL, &res->fhs_status, 0);
	if (fh)
		memcpy(&res->fhstatus_u.fhs_fhandle, fh->fh_handle, 32);
	return 1;
}

bool_t
mount_dump_1_svc(struct svc_req *rqstp, void *argp, mountlist *res)
{
	struct sockaddr_in *addr = nfs_getrpccaller_in(rqstp->rq_xprt);

	xlog(D_CALL, "dump request from %s.", inet_ntoa(addr->sin_addr));
	*res = mountlist_list();

	return 1;
}

bool_t
mount_umnt_1_svc(struct svc_req *rqstp, dirpath *argp, void *resp)
{
	struct sockaddr_in *sin = nfs_getrpccaller_in(rqstp->rq_xprt);
	nfs_export	*exp;
	char		*p = *argp;
	char		rpath[MAXPATHLEN+1];

	if (*p == '\0')
		p = "/";

	if (realpath(p, rpath) != NULL) {
		rpath[sizeof (rpath) - 1] = '\0';
		p = rpath;
	}

	if (!(exp = auth_authenticate("unmount", sin, p))) {
		return 1;
	}

	mountlist_del(inet_ntoa(sin->sin_addr), p);
	return 1;
}

bool_t
mount_umntall_1_svc(struct svc_req *rqstp, void *argp, void *resp)
{
	/* Reload /etc/xtab if necessary */
	auth_reload();

	mountlist_del_all(nfs_getrpccaller_in(rqstp->rq_xprt));
	return 1;
}

bool_t
mount_export_1_svc(struct svc_req *rqstp, void *argp, exports *resp)
{
	struct sockaddr_in *addr = nfs_getrpccaller_in(rqstp->rq_xprt);

	xlog(D_CALL, "export request from %s.", inet_ntoa(addr->sin_addr));
	*resp = get_exportlist();
		
	return 1;
}

bool_t
mount_exportall_1_svc(struct svc_req *rqstp, void *argp, exports *resp)
{
	struct sockaddr_in *addr = nfs_getrpccaller_in(rqstp->rq_xprt);

	xlog(D_CALL, "exportall request from %s.", inet_ntoa(addr->sin_addr));
	*resp = get_exportlist();

	return 1;
}

/*
 * MNTv2 pathconf procedure
 *
 * The protocol doesn't include a status field, so Sun apparently considers
 * it good practice to let anyone snoop on your system, even if it's
 * pretty harmless data such as pathconf. We don't.
 *
 * Besides, many of the pathconf values don't make much sense on NFS volumes.
 * FIFOs and tty device files represent devices on the *client*, so there's
 * no point in getting the server's buffer sizes etc.
 */
bool_t
mount_pathconf_2_svc(struct svc_req *rqstp, dirpath *path, ppathcnf *res)
{
	struct sockaddr_in *sin = nfs_getrpccaller_in(rqstp->rq_xprt);
	struct stat	stb;
	nfs_export	*exp;
	char		rpath[MAXPATHLEN+1];
	char		*p = *path;

	memset(res, 0, sizeof(*res));

	if (*p == '\0')
		p = "/";

	/* Reload /etc/xtab if necessary */
	auth_reload();

	/* Resolve symlinks */
	if (realpath(p, rpath) != NULL) {
		rpath[sizeof (rpath) - 1] = '\0';
		p = rpath;
	}

	/* Now authenticate the intruder... */
	exp = auth_authenticate("pathconf", sin, p);
	if (!exp) {
		return 1;
	} else if (stat(p, &stb) < 0) {
		xlog(L_WARNING, "can't stat exported dir %s: %s",
				p, strerror(errno));
		return 1;
	}

	res->pc_link_max  = pathconf(p, _PC_LINK_MAX);
	res->pc_max_canon = pathconf(p, _PC_MAX_CANON);
	res->pc_max_input = pathconf(p, _PC_MAX_INPUT);
	res->pc_name_max  = pathconf(p, _PC_NAME_MAX);
	res->pc_path_max  = pathconf(p, _PC_PATH_MAX);
	res->pc_pipe_buf  = pathconf(p, _PC_PIPE_BUF);
	res->pc_vdisable  = pathconf(p, _PC_VDISABLE);

	/* Can't figure out what to do with pc_mask */
	res->pc_mask[0]   = 0;
	res->pc_mask[1]   = 0;

	return 1;
}

/*
 * We should advertise the preferred flavours first. (See RFC 2623
 * section 2.7.)  We leave that to the administrator, by advertising
 * flavours in the order they were listed in /etc/exports.  AUTH_NULL is
 * dropped from the list to avoid backward compatibility issue with
 * older Linux clients, who inspect the list in reversed order.
 *
 * XXX: It might be more helpful to rearrange these so that flavors
 * giving more access (as determined from readonly and id-squashing
 * options) come first.  (If we decide to do that we should probably do
 * that when reading the exports rather than here.)
 */
static void set_authflavors(struct mountres3_ok *ok, nfs_export *exp)
{
	struct sec_entry *s;
	static int flavors[SECFLAVOR_COUNT];
	int i = 0;

	for (s = exp->m_export.e_secinfo; s->flav; s++) {
		if (s->flav->fnum == AUTH_NULL)
			continue;
		flavors[i] = s->flav->fnum;
		i++;
	}
	ok->auth_flavors.auth_flavors_val = flavors;
	ok->auth_flavors.auth_flavors_len = i;
}

/*
 * NFSv3 MOUNT procedure
 */
bool_t
mount_mnt_3_svc(struct svc_req *rqstp, dirpath *path, mountres3 *res)
{
	struct mountres3_ok *ok = &res->mountres3_u.mountinfo;
	nfs_export *exp;
	struct nfs_fh_len *fh;

	xlog(D_CALL, "MNT3(%s) called", *path);
	fh = get_rootfh(rqstp, path, &exp, &res->fhs_status, 1);
	if (!fh)
		return 1;

	ok->fhandle.fhandle3_len = fh->fh_size;
	ok->fhandle.fhandle3_val = (char *)fh->fh_handle;
	set_authflavors(ok, exp);
	return 1;
}

static struct nfs_fh_len *
get_rootfh(struct svc_req *rqstp, dirpath *path, nfs_export **expret,
		mountstat3 *error, int v3)
{
	struct sockaddr_in *sin = nfs_getrpccaller_in(rqstp->rq_xprt);
	struct stat	stb, estb;
	nfs_export	*exp;
	struct nfs_fh_len *fh;
	char		rpath[MAXPATHLEN+1];
	char		*p = *path;

	if (*p == '\0')
		p = "/";

	/* Reload /var/lib/nfs/etab if necessary */
	auth_reload();

	/* Resolve symlinks */
	if (realpath(p, rpath) != NULL) {
		rpath[sizeof (rpath) - 1] = '\0';
		p = rpath;
	}

	/* Now authenticate the intruder... */
	exp = auth_authenticate("mount", sin, p);
	if (!exp) {
		*error = NFSERR_ACCES;
		return NULL;
	}
	if (stat(p, &stb) < 0) {
		xlog(L_WARNING, "can't stat exported dir %s: %s",
				p, strerror(errno));
		if (errno == ENOENT)
			*error = NFSERR_NOENT;
		else
			*error = NFSERR_ACCES;
		return NULL;
	}
	if (!S_ISDIR(stb.st_mode) && !S_ISREG(stb.st_mode)) {
		xlog(L_WARNING, "%s is not a directory or regular file", p);
		*error = NFSERR_NOTDIR;
		return NULL;
	}
	if (stat(exp->m_export.e_path, &estb) < 0) {
		xlog(L_WARNING, "can't stat export point %s: %s",
		     p, strerror(errno));
		*error = NFSERR_NOENT;
		return NULL;
	}
	if (estb.st_dev != stb.st_dev
		   && (!new_cache
			   || !(exp->m_export.e_flags & NFSEXP_CROSSMOUNT))) {
		xlog(L_WARNING, "request to export directory %s below nearest filesystem %s",
		     p, exp->m_export.e_path);
		*error = NFSERR_ACCES;
		return NULL;
	}
	if (exp->m_export.e_mountpoint &&
		   !is_mountpoint(exp->m_export.e_mountpoint[0]?
				  exp->m_export.e_mountpoint:
				  exp->m_export.e_path)) {
		xlog(L_WARNING, "request to export an unmounted filesystem: %s",
		     p);
		*error = NFSERR_NOENT;
		return NULL;
	}

	if (new_cache) {
		/* This will be a static private nfs_export with just one
		 * address.  We feed it to kernel then extract the filehandle,
		 * 
		 */

		if (cache_export(exp, p)) {
			*error = NFSERR_ACCES;
			return NULL;
		}
		fh = cache_get_filehandle(exp, v3?64:32, p);
		if (fh == NULL) {
			*error = NFSERR_ACCES;
			return NULL;
		}
	} else {
		int did_export = 0;
	retry:
		if (exp->m_exported<1) {
			export_export(exp);
			did_export = 1;
		}
		if (!exp->m_xtabent)
			xtab_append(exp);

		if (v3)
			fh = getfh_size ((struct sockaddr *) sin, p, 64);
		if (!v3 || (fh == NULL && errno == EINVAL)) {
			/* We first try the new nfs syscall. */
			fh = getfh ((struct sockaddr *) sin, p);
			if (fh == NULL && errno == EINVAL)
				/* Let's try the old one. */
				fh = getfh_old ((struct sockaddr *) sin,
						stb.st_dev, stb.st_ino);
		}
		if (fh == NULL && !did_export) {
			exp->m_exported = 0;
			goto retry;
		}

		if (fh == NULL) {
			xlog(L_WARNING, "getfh failed: %s", strerror(errno));
			*error = NFSERR_ACCES;
			return NULL;
		}
	}
	*error = NFS_OK;
	mountlist_add(inet_ntoa(sin->sin_addr), p);
	if (expret)
		*expret = exp;
	return fh;
}

static exports
get_exportlist(void)
{
	static exports		elist = NULL;
	struct exportnode	*e, *ne;
	struct groupnode	*g, *ng, *c, **cp;
	nfs_export		*exp;
	int			i;
	static unsigned int	ecounter;
	unsigned int		acounter;

	acounter = auth_reload();
	if (elist && acounter == ecounter)
		return elist;

	ecounter = acounter;

	for (e = elist; e != NULL; e = ne) {
		ne = e->ex_next;
		for (g = e->ex_groups; g != NULL; g = ng) {
			ng = g->gr_next;
			xfree(g->gr_name);
			xfree(g);
		}
		xfree(e->ex_dir);
		xfree(e);
	}
	elist = NULL;

	for (i = 0; i < MCL_MAXTYPES; i++) {
		for (exp = exportlist[i].p_head; exp; exp = exp->m_next) {
			for (e = elist; e != NULL; e = e->ex_next) {
				if (!strcmp(exp->m_export.e_path, e->ex_dir))
					break;
			}
			if (!e) {
				e = (struct exportnode *) xmalloc(sizeof(*e));
				e->ex_next = elist;
				e->ex_groups = NULL;
				e->ex_dir = xstrdup(exp->m_export.e_path);
				elist = e;
			}

			/* We need to check if we should remove
			   previous ones. */
			if (i == MCL_ANONYMOUS && e->ex_groups) {
				for (g = e->ex_groups; g; g = ng) {
					ng = g->gr_next;
					xfree(g->gr_name);
					xfree(g);
				}
				e->ex_groups = NULL;
				continue;
			}

			if (i != MCL_FQDN && e->ex_groups) {
			  struct hostent 	*hp;

			  cp = &e->ex_groups;
			  while ((c = *cp) != NULL) {
			    if (client_gettype (c->gr_name) == MCL_FQDN
			        && (hp = gethostbyname(c->gr_name))) {
			      hp = hostent_dup (hp);
			      if (client_check(exp->m_client, hp)) {
				*cp = c->gr_next;
				xfree(c->gr_name);
				xfree(c);
				xfree (hp);
				continue;
			      }
			      xfree (hp);
			    }
			    cp = &(c->gr_next);
			  }
			}

			if (exp->m_export.e_hostname [0] != '\0') {
				for (g = e->ex_groups; g; g = g->gr_next)
					if (strcmp (exp->m_export.e_hostname,
						    g->gr_name) == 0)
						break;
				if (g)
					continue;
				g = (struct groupnode *) xmalloc(sizeof(*g));
				g->gr_name = xstrdup(exp->m_export.e_hostname);
				g->gr_next = e->ex_groups;
				e->ex_groups = g;
			}
		}
	}

	return elist;
}

int
main(int argc, char **argv)
{
	char	*export_file = _PATH_EXPORTS;
	char    *state_dir = NFS_STATEDIR;
	int	foreground = 0;
	int	port = 0;
	int	descriptors = 0;
	int	c;
	struct sigaction sa;
	struct rlimit rlim;

	/* Parse the command line options and arguments. */
	opterr = 0;
	while ((c = getopt_long(argc, argv, "o:nFd:f:p:P:hH:N:V:vrs:t:g", longopts, NULL)) != EOF)
		switch (c) {
		case 'g':
			manage_gids = 1;
			break;
		case 'o':
			descriptors = atoi(optarg);
			if (descriptors <= 0) {
				fprintf(stderr, "%s: bad descriptors: %s\n",
					argv [0], optarg);
				usage(argv [0], 1);
			}
			break;
		case 'F':
			foreground = 1;
			break;
		case 'd':
			xlog_sconfig(optarg, 1);
			break;
		case 'f':
			export_file = optarg;
			break;
		case 'H': /* PRC: specify a high-availability callout program */
			ha_callout_prog = optarg;
			break;
		case 'h':
			usage(argv [0], 0);
			break;
		case 'P':	/* XXX for nfs-server compatibility */
		case 'p':
			port = atoi(optarg);
			if (port <= 0 || port > 65535) {
				fprintf(stderr, "%s: bad port number: %s\n",
					argv [0], optarg);
				usage(argv [0], 1);
			}
			break;
		case 'N':
			nfs_version &= ~(1 << (atoi (optarg) - 1));
			break;
		case 'n':
			_rpcfdtype = SOCK_DGRAM;
			break;
		case 'r':
			reverse_resolve = 1;
			break;
		case 's':
			if ((state_dir = xstrdup(optarg)) == NULL) {
				fprintf(stderr, "%s: xstrdup(%s) failed!\n",
					argv[0], optarg);
				exit(1);
			}
			break;
		case 't':
			num_threads = atoi (optarg);
			break;
		case 'V':
			nfs_version |= 1 << (atoi (optarg) - 1);
			break;
		case 'v':
			printf("kmountd %s\n", VERSION);
			exit(0);
		case 0:
			break;
		case '?':
		default:
			usage(argv [0], 1);
		}

	/* No more arguments allowed.
	 * Require at least one valid version (2, 3, or 4)
	 */
	if (optind != argc || !(nfs_version & 0xE))
		usage(argv [0], 1);

	if (chdir(state_dir)) {
		fprintf(stderr, "%s: chdir(%s) failed: %s\n",
			argv [0], state_dir, strerror(errno));
		exit(1);
	}

	if (getrlimit (RLIMIT_NOFILE, &rlim) != 0)
		fprintf(stderr, "%s: getrlimit (RLIMIT_NOFILE) failed: %s\n",
				argv [0], strerror(errno));
	else {
		/* glibc sunrpc code dies if getdtablesize > FD_SETSIZE */
		if ((descriptors == 0 && rlim.rlim_cur > FD_SETSIZE) ||
		    descriptors > FD_SETSIZE)
			descriptors = FD_SETSIZE;
		if (descriptors) {
			rlim.rlim_cur = descriptors;
			if (setrlimit (RLIMIT_NOFILE, &rlim) != 0) {
				fprintf(stderr, "%s: setrlimit (RLIMIT_NOFILE) failed: %s\n",
					argv [0], strerror(errno));
				exit(1);
			}
		}
	}
	/* Initialize logging. */
	if (!foreground) xlog_stderr(0);
	xlog_open("mountd");

	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	/* WARNING: the following works on Linux and SysV, but not BSD! */
	sigaction(SIGCHLD, &sa, NULL);

	/* Daemons should close all extra filehandles ... *before* RPC init. */
	if (!foreground)
		closeall(3);

	new_cache = check_new_cache();
	if (new_cache)
		cache_open();

	if (nfs_version & 0x1)
		rpc_init("mountd", MOUNTPROG, MOUNTVERS,
			 mount_dispatch, port);
	if (nfs_version & (0x1 << 1))
		rpc_init("mountd", MOUNTPROG, MOUNTVERS_POSIX,
			 mount_dispatch, port);
	if (nfs_version & (0x1 << 2))
		rpc_init("mountd", MOUNTPROG, MOUNTVERS_NFSV3,
			 mount_dispatch, port);

	sa.sa_handler = killer;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sa.sa_handler = sig_hup;
	sigaction(SIGHUP, &sa, NULL);

	auth_init(export_file);

	if (!foreground) {
		/* We first fork off a child. */
		if ((c = fork()) > 0)
			exit(0);
		if (c < 0) {
			xlog(L_FATAL, "mountd: cannot fork: %s\n",
						strerror(errno));
		}
		/* Now we remove ourselves from the foreground.
		   Redirect stdin/stdout/stderr first. */
		{
			int fd = open("/dev/null", O_RDWR);
			(void) dup2(fd, 0);
			(void) dup2(fd, 1);
			(void) dup2(fd, 2);
			if (fd > 2) (void) close(fd);
		}
		setsid();
	}

	/* silently bounds check num_threads */
	if (foreground)
		num_threads = 1;
	else if (num_threads < 1)
		num_threads = 1;
	else if (num_threads > MAX_THREADS)
		num_threads = MAX_THREADS;

	if (num_threads > 1)
		fork_workers();

	my_svc_run();

	xlog(L_ERROR, "Ack! Gack! svc_run returned!\n");
	exit(1);
}

static void
usage(const char *prog, int n)
{
	fprintf(stderr,
"Usage: %s [-F|--foreground] [-h|--help] [-v|--version] [-d kind|--debug kind]\n"
"	[-o num|--descriptors num] [-f exports-file|--exports-file=file]\n"
"	[-p|--port port] [-V version|--nfs-version version]\n"
"	[-N version|--no-nfs-version version] [-n|--no-tcp]\n"
"	[-H ha-callout-prog] [-s|--state-directory-path path]\n"
"	[-g|--manage-gids] [-t num|--num-threads=num]\n", prog);
	exit(n);
}
