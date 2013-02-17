/*
 *  idmapd.c
 *
 *  Userland daemon for idmap.
 *
 *  Copyright (c) 2002 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Marius Aamodt Eriksen <marius@umich.edu>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the University nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>

#include "nfs_idmap.h"

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <ctype.h>
#include <nfsidmap.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "xlog.h"
#include "cfg.h"
#include "queue.h"
#include "nfslib.h"

#ifndef PIPEFS_DIR
#define PIPEFS_DIR  "/var/lib/nfs/rpc_pipefs/"
#endif

#ifndef NFSD_DIR
#define NFSD_DIR  "/proc/net/rpc"
#endif

#ifndef CLIENT_CACHE_TIMEOUT_FILE
#define CLIENT_CACHE_TIMEOUT_FILE "/proc/sys/fs/nfs/idmap_cache_timeout"
#endif

#ifndef NFS4NOBODY_USER
#define NFS4NOBODY_USER "nobody"
#endif

#ifndef NFS4NOBODY_GROUP
#define NFS4NOBODY_GROUP "nobody"
#endif

/* From Niels */
#define CONF_SAVE(w, f) do {			\
	char *p = f;				\
	if (p != NULL)				\
		(w) = p;			\
} while (0)

#define IC_IDNAME 0
#define IC_IDNAME_CHAN  NFSD_DIR "/nfs4.idtoname/channel"
#define IC_IDNAME_FLUSH NFSD_DIR "/nfs4.idtoname/flush"

#define IC_NAMEID 1
#define IC_NAMEID_CHAN  NFSD_DIR "/nfs4.nametoid/channel"
#define IC_NAMEID_FLUSH NFSD_DIR "/nfs4.nametoid/flush"

struct idmap_client {
	short                      ic_which;
	char                       ic_clid[30];
	char                      *ic_id;
	char                       ic_path[PATH_MAX];
	int                        ic_fd;
	int                        ic_dirfd;
	int                        ic_scanned;
	struct event               ic_event;
	TAILQ_ENTRY(idmap_client)  ic_next;
};
static struct idmap_client nfsd_ic[2] = {
{IC_IDNAME, "Server", "", IC_IDNAME_CHAN, -1, -1, 0},
{IC_NAMEID, "Server", "", IC_NAMEID_CHAN, -1, -1, 0},
};

TAILQ_HEAD(idmap_clientq, idmap_client);

static void dirscancb(int, short, void *);
static void clntscancb(int, short, void *);
static void svrreopen(int, short, void *);
static int  nfsopen(struct idmap_client *);
static void nfscb(int, short, void *);
static void nfsdcb(int, short, void *);
static int  validateascii(char *, u_int32_t);
static int  addfield(char **, ssize_t *, char *);
static int  getfield(char **, char *, size_t);

static void imconv(struct idmap_client *, struct idmap_msg *);
static void idtonameres(struct idmap_msg *);
static void nametoidres(struct idmap_msg *);

static int nfsdopen(void);
static int nfsdopenone(struct idmap_client *);
static void nfsdreopen(void);

size_t  strlcat(char *, const char *, size_t);
size_t  strlcpy(char *, const char *, size_t);
ssize_t atomicio(ssize_t (*f) (int, void*, size_t),
		 int, void *, size_t);
void    mydaemon(int, int);
void    release_parent(void);

static int verbose = 0;
#define DEFAULT_IDMAP_CACHE_EXPIRY 600 /* seconds */
static int cache_entry_expiration = 0;
static char pipefsdir[PATH_MAX];
static char *nobodyuser, *nobodygroup;
static uid_t nobodyuid;
static gid_t nobodygid;

/* Used by cfg.c */
char *conf_path;

static int
flush_nfsd_cache(char *path, time_t now)
{
	int fd;
	char stime[20];

	sprintf(stime, "%ld\n", now);
	fd = open(path, O_RDWR);
	if (fd == -1)
		return -1;
	if (write(fd, stime, strlen(stime)) != strlen(stime)) {
		errx(1, "Flushing nfsd cache failed: errno %d (%s)",
			errno, strerror(errno));
	}
	close(fd);
	return 0;
}

static int
flush_nfsd_idmap_cache(void)
{
	time_t now = time(NULL);
	int ret;

	ret = flush_nfsd_cache(IC_IDNAME_FLUSH, now);
	if (ret)
		return ret;
	ret = flush_nfsd_cache(IC_NAMEID_FLUSH, now);
	return ret;
}

int
main(int argc, char **argv)
{
	int fd = 0, opt, fg = 0, nfsdret = -1;
	struct idmap_clientq icq;
	struct event rootdirev, clntdirev, svrdirev;
	struct event initialize;
	struct passwd *pw;
	struct group *gr;
	struct stat sb;
	char *xpipefsdir = NULL;
	int serverstart = 1, clientstart = 1;
	int ret;
	char *progname;

	conf_path = _PATH_IDMAPDCONF;
	nobodyuser = NFS4NOBODY_USER;
	nobodygroup = NFS4NOBODY_GROUP;
	strlcpy(pipefsdir, PIPEFS_DIR, sizeof(pipefsdir));

	if ((progname = strrchr(argv[0], '/')))
		progname++;
	else
		progname = argv[0];
	xlog_open(progname);

#define GETOPTSTR "vfd:p:U:G:c:CS"
	opterr=0; /* Turn off error messages */
	while ((opt = getopt(argc, argv, GETOPTSTR)) != -1) {
		if (opt == 'c')
			conf_path = optarg;
		if (opt == '?') {
			if (strchr(GETOPTSTR, optopt))
				errx(1, "'-%c' option requires an argument.", optopt);
			else
				errx(1, "'-%c' is an invalid argument.", optopt);
		}
	}
	optind = 1;

	if (stat(conf_path, &sb) == -1 && (errno == ENOENT || errno == EACCES)) {
		warn("Skipping configuration file \"%s\"", conf_path);
		conf_path = NULL;
	} else {
		conf_init();
		verbose = conf_get_num("General", "Verbosity", 0);
		cache_entry_expiration = conf_get_num("General",
				"Cache-Expiration", DEFAULT_IDMAP_CACHE_EXPIRY);
		CONF_SAVE(xpipefsdir, conf_get_str("General", "Pipefs-Directory"));
		if (xpipefsdir != NULL)
			strlcpy(pipefsdir, xpipefsdir, sizeof(pipefsdir));
		CONF_SAVE(nobodyuser, conf_get_str("Mapping", "Nobody-User"));
		CONF_SAVE(nobodygroup, conf_get_str("Mapping", "Nobody-Group"));
	}

	while ((opt = getopt(argc, argv, GETOPTSTR)) != -1)
		switch (opt) {
		case 'v':
			verbose++;
			break;
		case 'f':
			fg = 1;
			break;
		case 'p':
			strlcpy(pipefsdir, optarg, sizeof(pipefsdir));
			break;
		case 'd':
		case 'U':
		case 'G':
			errx(1, "the -d, -U, and -G options have been removed;"
				" please use the configuration file instead.");
		case 'C':
			serverstart = 0;
			break;
		case 'S':
			clientstart = 0;
			break;
		default:
			break;
		}

	if (!serverstart && !clientstart)
		errx(1, "it is illegal to specify both -C and -S");

	strncat(pipefsdir, "/nfs", sizeof(pipefsdir));

	if ((pw = getpwnam(nobodyuser)) == NULL)
		errx(1, "Could not find user \"%s\"", nobodyuser);
	nobodyuid = pw->pw_uid;

	if ((gr = getgrnam(nobodygroup)) == NULL)
		errx(1, "Could not find group \"%s\"", nobodygroup);
	nobodygid = gr->gr_gid;

#ifdef HAVE_NFS4_SET_DEBUG
	nfs4_set_debug(verbose, xlog_warn);
#endif
	if (conf_path == NULL)
		conf_path = _PATH_IDMAPDCONF;
	if (nfs4_init_name_mapping(conf_path))
		errx(1, "Unable to create name to user id mappings.");

	if (!fg)
		mydaemon(0, 0);

	event_init();

	if (verbose > 0)
		xlog_warn("Expiration time is %d seconds.",
			     cache_entry_expiration);
	if (serverstart) {
		nfsdret = nfsdopen();
		if (nfsdret == 0) {
			ret = flush_nfsd_idmap_cache();
			if (ret)
				xlog_err("main: Failed to flush nfsd idmap cache\n: %s", strerror(errno));
		}
	}

	if (clientstart) {
		struct timeval now = {
			.tv_sec = 0,
			.tv_usec = 0,
		};

		if (cache_entry_expiration != DEFAULT_IDMAP_CACHE_EXPIRY) {
			int timeout_fd, len;
			char timeout_buf[12];
			if ((timeout_fd = open(CLIENT_CACHE_TIMEOUT_FILE,
					       O_RDWR)) == -1) {
				xlog_warn("Unable to open '%s' to set "
					     "client cache expiration time "
					     "to %d seconds\n",
					     CLIENT_CACHE_TIMEOUT_FILE,
					     cache_entry_expiration);
			} else {
				len = snprintf(timeout_buf, sizeof(timeout_buf),
					       "%d", cache_entry_expiration);
				if ((write(timeout_fd, timeout_buf, len)) != len)
					xlog_warn("Error writing '%s' to "
						     "'%s' to set client "
						     "cache expiration time\n",
						     timeout_buf,
						     CLIENT_CACHE_TIMEOUT_FILE);
				close(timeout_fd);
			}
		}

		if ((fd = open(pipefsdir, O_RDONLY)) == -1)
			xlog_err("main: open(%s): %s", pipefsdir, strerror(errno));

		if (fcntl(fd, F_SETSIG, SIGUSR1) == -1)
			xlog_err("main: fcntl(%s): %s", pipefsdir, strerror(errno));

		if (fcntl(fd, F_NOTIFY,
			DN_CREATE | DN_DELETE | DN_MODIFY | DN_MULTISHOT) == -1) {
			xlog_err("main: fcntl(%s): %s", pipefsdir, strerror(errno));
			if (errno == EINVAL)
				xlog_err("main: Possibly no Dnotify support in kernel.");
		}
		TAILQ_INIT(&icq);

		/* These events are persistent */
		signal_set(&rootdirev, SIGUSR1, dirscancb, &icq);
		signal_add(&rootdirev, NULL);
		signal_set(&clntdirev, SIGUSR2, clntscancb, &icq);
		signal_add(&clntdirev, NULL);
		signal_set(&svrdirev, SIGHUP, svrreopen, NULL);
		signal_add(&svrdirev, NULL);

		/* Fetch current state */
		/* (Delay till start of event_dispatch to avoid possibly losing
		 * a SIGUSR1 between here and the call to event_dispatch().) */
		evtimer_set(&initialize, dirscancb, &icq);
		evtimer_add(&initialize, &now);
	}

	if (nfsdret != 0 && fd == 0)
		xlog_err("main: Neither NFS client nor NFSd found");

	release_parent();

	if (event_dispatch() < 0)
		xlog_err("main: event_dispatch returns errno %d (%s)",
			    errno, strerror(errno));
	/* NOTREACHED */
	return 1;
}

static void
dirscancb(int fd, short which, void *data)
{
	int nent, i;
	struct dirent **ents;
	struct idmap_client *ic, *nextic;
	char path[PATH_MAX];
	struct idmap_clientq *icq = data;

	nent = scandir(pipefsdir, &ents, NULL, alphasort);
	if (nent == -1) {
		xlog_warn("dirscancb: scandir(%s): %s", pipefsdir, strerror(errno));
		return;
	}

	for (i = 0;  i < nent; i++) {
		if (ents[i]->d_reclen > 4 &&
		    strncmp(ents[i]->d_name, "clnt", 4) == 0) {
			TAILQ_FOREACH(ic, icq, ic_next)
			    if (strcmp(ents[i]->d_name + 4, ic->ic_clid) == 0)
				    break;
			if (ic != NULL)
				goto next;

			if ((ic = calloc(1, sizeof(*ic))) == NULL)
				goto out;
			strlcpy(ic->ic_clid, ents[i]->d_name + 4,
			    sizeof(ic->ic_clid));
			path[0] = '\0';
			snprintf(path, sizeof(path), "%s/%s",
			    pipefsdir, ents[i]->d_name);

			if ((ic->ic_dirfd = open(path, O_RDONLY, 0)) == -1) {
				xlog_warn("dirscancb: open(%s): %s", path, strerror(errno));
				free(ic);
				goto out;
			}

			strlcat(path, "/idmap", sizeof(path));
			strlcpy(ic->ic_path, path, sizeof(ic->ic_path));

			if (verbose > 0)
				xlog_warn("New client: %s", ic->ic_clid);

			if (nfsopen(ic) == -1) {
				close(ic->ic_dirfd);
				free(ic);
				goto out;
			}

			ic->ic_id = "Client";

			TAILQ_INSERT_TAIL(icq, ic, ic_next);

		next:
			ic->ic_scanned = 1;
		}
	}

	ic = TAILQ_FIRST(icq);
	while(ic != NULL) {
		nextic=TAILQ_NEXT(ic, ic_next);
		if (!ic->ic_scanned) {
			event_del(&ic->ic_event);
			close(ic->ic_fd);
			close(ic->ic_dirfd);
			TAILQ_REMOVE(icq, ic, ic_next);
			if (verbose > 0) {
				xlog_warn("Stale client: %s", ic->ic_clid);
				xlog_warn("\t-> closed %s", ic->ic_path);
			}
			free(ic);
		} else
			ic->ic_scanned = 0;
		ic = nextic;
	}

out:
	for (i = 0;  i < nent; i++)
		free(ents[i]);
	free(ents);
	return;
}

static void
svrreopen(int fd, short which, void *data)
{
	nfsdreopen();
}

static void
clntscancb(int fd, short which, void *data)
{
	struct idmap_clientq *icq = data;
	struct idmap_client *ic;

	TAILQ_FOREACH(ic, icq, ic_next)
		if (ic->ic_fd == -1 && nfsopen(ic) == -1) {
			close(ic->ic_dirfd);
			TAILQ_REMOVE(icq, ic, ic_next);
			free(ic);
		}
}

static void
nfsdcb(int fd, short which, void *data)
{
	struct idmap_client *ic = data;
	struct idmap_msg im;
	u_char buf[IDMAP_MAXMSGSZ + 1];
	size_t len;
	ssize_t bsiz;
	char *bp, typebuf[IDMAP_MAXMSGSZ],
		buf1[IDMAP_MAXMSGSZ], authbuf[IDMAP_MAXMSGSZ], *p;
	unsigned long tmp;

	if (which != EV_READ)
		goto out;

	if ((len = read(ic->ic_fd, buf, sizeof(buf))) <= 0) {
		xlog_warn("nfsdcb: read(%s) failed: errno %d (%s)",
			     ic->ic_path, len?errno:0, 
			     len?strerror(errno):"End of File");
		goto out;
	}

	/* Get rid of newline and terminate buffer*/
	buf[len - 1] = '\0';
	bp = (char *)buf;

	memset(&im, 0, sizeof(im));

	/* Authentication name -- ignored for now*/
	if (getfield(&bp, authbuf, sizeof(authbuf)) == -1) {
		xlog_warn("nfsdcb: bad authentication name in upcall\n");
		return;
	}
	if (getfield(&bp, typebuf, sizeof(typebuf)) == -1) {
		xlog_warn("nfsdcb: bad type in upcall\n");
		return;
	}
	if (verbose > 0)
		xlog_warn("nfsdcb: authbuf=%s authtype=%s",
			     authbuf, typebuf);

	im.im_type = strcmp(typebuf, "user") == 0 ?
		IDMAP_TYPE_USER : IDMAP_TYPE_GROUP;

	switch (ic->ic_which) {
	case IC_NAMEID:
		im.im_conv = IDMAP_CONV_NAMETOID;
		if (getfield(&bp, im.im_name, sizeof(im.im_name)) == -1) {
			xlog_warn("nfsdcb: bad name in upcall\n");
			return;
		}
		break;
	case IC_IDNAME:
		im.im_conv = IDMAP_CONV_IDTONAME;
		if (getfield(&bp, buf1, sizeof(buf1)) == -1) {
			xlog_warn("nfsdcb: bad id in upcall\n");
			return;
		}
		tmp = strtoul(buf1, (char **)NULL, 10);
		im.im_id = (u_int32_t)tmp;
		if ((tmp == ULONG_MAX && errno == ERANGE)
				|| (unsigned long)im.im_id != tmp) {
			xlog_warn("nfsdcb: id '%s' too big!\n", buf1);
			return;
		}
		break;
	default:
		xlog_warn("nfsdcb: Unknown which type %d", ic->ic_which);
		return;
	}

	imconv(ic, &im);

	buf[0] = '\0';
	bp = (char *)buf;
	bsiz = sizeof(buf);

	/* Authentication name */
	addfield(&bp, &bsiz, authbuf);

	switch (ic->ic_which) {
	case IC_NAMEID:
		/* Type */
		p = im.im_type == IDMAP_TYPE_USER ? "user" : "group";
		addfield(&bp, &bsiz, p);
		/* Name */
		addfield(&bp, &bsiz, im.im_name);
		/* expiry */
		snprintf(buf1, sizeof(buf1), "%lu",
			 time(NULL) + cache_entry_expiration);
		addfield(&bp, &bsiz, buf1);
		/* Note that we don't want to write the id if the mapping
		 * failed; instead, by leaving it off, we write a negative
		 * cache entry which will result in an error returned to
		 * the client.  We don't want a chown or setacl referring
		 * to an unknown user to result in giving permissions to
		 * "nobody"! */
		if (im.im_status == IDMAP_STATUS_SUCCESS) {
			/* ID */
			snprintf(buf1, sizeof(buf1), "%u", im.im_id);
			addfield(&bp, &bsiz, buf1);

		}
		//if (bsiz == sizeof(buf)) /* XXX */

		bp[-1] = '\n';

		break;
	case IC_IDNAME:
		/* Type */
		p = im.im_type == IDMAP_TYPE_USER ? "user" : "group";
		addfield(&bp, &bsiz, p);
		/* ID */
		snprintf(buf1, sizeof(buf1), "%u", im.im_id);
		addfield(&bp, &bsiz, buf1);
		/* expiry */
		snprintf(buf1, sizeof(buf1), "%lu",
			 time(NULL) + cache_entry_expiration);
		addfield(&bp, &bsiz, buf1);
		/* Note we're ignoring the status field in this case; we'll
		 * just map to nobody instead. */
		/* Name */
		addfield(&bp, &bsiz, im.im_name);

		bp[-1] = '\n';

		break;
	default:
		xlog_warn("nfsdcb: Unknown which type %d", ic->ic_which);
		return;
	}

	bsiz = sizeof(buf) - bsiz;

	if (atomicio((void*)write, ic->ic_fd, buf, bsiz) != bsiz)
		xlog_warn("nfsdcb: write(%s) failed: errno %d (%s)",
			     ic->ic_path, errno, strerror(errno));

out:
	event_add(&ic->ic_event, NULL);
}

static void
imconv(struct idmap_client *ic, struct idmap_msg *im)
{
	switch (im->im_conv) {
	case IDMAP_CONV_IDTONAME:
		idtonameres(im);
		if (verbose > 1)
			xlog_warn("%s %s: (%s) id \"%d\" -> name \"%s\"",
			    ic->ic_id, ic->ic_clid,
			    im->im_type == IDMAP_TYPE_USER ? "user" : "group",
			    im->im_id, im->im_name);
		break;
	case IDMAP_CONV_NAMETOID:
		if (validateascii(im->im_name, sizeof(im->im_name)) == -1) {
			im->im_status |= IDMAP_STATUS_INVALIDMSG;
			return;
		}
		nametoidres(im);
		if (verbose > 1)
			xlog_warn("%s %s: (%s) name \"%s\" -> id \"%d\"",
			    ic->ic_id, ic->ic_clid,
			    im->im_type == IDMAP_TYPE_USER ? "user" : "group",
			    im->im_name, im->im_id);
		break;
	default:
		xlog_warn("imconv: Invalid conversion type (%d) in message",
			     im->im_conv);
		im->im_status |= IDMAP_STATUS_INVALIDMSG;
		break;
	}
}

static void
nfscb(int fd, short which, void *data)
{
	struct idmap_client *ic = data;
	struct idmap_msg im;

	if (which != EV_READ)
		goto out;

	if (atomicio(read, ic->ic_fd, &im, sizeof(im)) != sizeof(im)) {
		if (verbose > 0)
			xlog_warn("nfscb: read(%s): %s", ic->ic_path, strerror(errno));
		if (errno == EPIPE)
			return;
		goto out;
	}

	imconv(ic, &im);

	/* XXX: I don't like ignoring this error in the id->name case,
	 * but we've never returned it, and I need to check that the client
	 * can handle it gracefully before starting to return it now. */

	if (im.im_status == IDMAP_STATUS_LOOKUPFAIL)
		im.im_status = IDMAP_STATUS_SUCCESS;

	if (atomicio((void*)write, ic->ic_fd, &im, sizeof(im)) != sizeof(im))
		xlog_warn("nfscb: write(%s): %s", ic->ic_path, strerror(errno));
out:
	event_add(&ic->ic_event, NULL);
}

static void
nfsdreopen_one(struct idmap_client *ic)
{
	int fd;

	if (verbose > 0)
		xlog_warn("ReOpening %s", ic->ic_path);

	if ((fd = open(ic->ic_path, O_RDWR, 0)) != -1) {
		if ((ic->ic_event.ev_flags & EVLIST_INIT))
			event_del(&ic->ic_event);
		if (ic->ic_fd != -1)
			close(ic->ic_fd);

		ic->ic_event.ev_fd = ic->ic_fd = fd;
		event_set(&ic->ic_event, ic->ic_fd, EV_READ, nfsdcb, ic);
		event_add(&ic->ic_event, NULL);
	} else {
		xlog_warn("nfsdreopen: Opening '%s' failed: errno %d (%s)",
			ic->ic_path, errno, strerror(errno));
	}
}

static void
nfsdreopen()
{
	nfsdreopen_one(&nfsd_ic[IC_NAMEID]);
	nfsdreopen_one(&nfsd_ic[IC_IDNAME]);
	return;
}

static int
nfsdopen(void)
{
	return ((nfsdopenone(&nfsd_ic[IC_NAMEID]) == 0 &&
		    nfsdopenone(&nfsd_ic[IC_IDNAME]) == 0) ? 0 : -1);
}

static int
nfsdopenone(struct idmap_client *ic)
{
	if ((ic->ic_fd = open(ic->ic_path, O_RDWR, 0)) == -1) {
		if (verbose > 0)
			xlog_warn("nfsdopenone: Opening %s failed: "
				"errno %d (%s)",
				ic->ic_path, errno, strerror(errno));
		return (-1);
	}

	event_set(&ic->ic_event, ic->ic_fd, EV_READ, nfsdcb, ic);
	event_add(&ic->ic_event, NULL);

	if (verbose > 0)
		xlog_warn("Opened %s", ic->ic_path);

	return (0);
}

static int
nfsopen(struct idmap_client *ic)
{
	if ((ic->ic_fd = open(ic->ic_path, O_RDWR, 0)) == -1) {
		switch (errno) {
		case ENOENT:
			fcntl(ic->ic_dirfd, F_SETSIG, SIGUSR2);
			fcntl(ic->ic_dirfd, F_NOTIFY,
			    DN_CREATE | DN_DELETE | DN_MULTISHOT);
			break;
		default:
			xlog_warn("nfsopen: open(%s): %s", ic->ic_path, strerror(errno));
			return (-1);
		}
	} else {
		event_set(&ic->ic_event, ic->ic_fd, EV_READ, nfscb, ic);
		event_add(&ic->ic_event, NULL);
		fcntl(ic->ic_dirfd, F_SETSIG, 0);
		fcntl(ic->ic_dirfd, F_NOTIFY, 0);
		if (verbose > 0)
			xlog_warn("Opened %s", ic->ic_path);
	}

	return (0);
}

static void
idtonameres(struct idmap_msg *im)
{
	char domain[NFS4_MAX_DOMAIN_LEN];
	int ret = 0;

	ret = nfs4_get_default_domain(NULL, domain, sizeof(domain));
	switch (im->im_type) {
	case IDMAP_TYPE_USER:
		ret = nfs4_uid_to_name(im->im_id, domain, im->im_name,
				sizeof(im->im_name));
		if (ret) {
			if (strlen(nobodyuser) < sizeof(im->im_name))
				strcpy(im->im_name, nobodyuser);
			else
				strcpy(im->im_name, NFS4NOBODY_USER);
		}
		break;
	case IDMAP_TYPE_GROUP:
		ret = nfs4_gid_to_name(im->im_id, domain, im->im_name,
				sizeof(im->im_name));
		if (ret) {
			if (strlen(nobodygroup) < sizeof(im->im_name))
				strcpy(im->im_name, nobodygroup);
			else
				strcpy(im->im_name, NFS4NOBODY_GROUP);
		}
		break;
	}
	if (ret)
		im->im_status = IDMAP_STATUS_LOOKUPFAIL;
	else
		im->im_status = IDMAP_STATUS_SUCCESS;
}

static void
nametoidres(struct idmap_msg *im)
{
	uid_t uid;
	gid_t gid;
	int ret = 0;

	/* XXX: move nobody stuff to library calls
	 * (nfs4_get_nobody_user(domain), nfs4_get_nobody_group(domain)) */

	im->im_status = IDMAP_STATUS_SUCCESS;

	switch (im->im_type) {
	case IDMAP_TYPE_USER:
		ret = nfs4_name_to_uid(im->im_name, &uid);
		im->im_id = (u_int32_t) uid;
		if (ret) {
			im->im_status = IDMAP_STATUS_LOOKUPFAIL;
			im->im_id = nobodyuid;
		}
		return;
	case IDMAP_TYPE_GROUP:
		ret = nfs4_name_to_gid(im->im_name, &gid);
		im->im_id = (u_int32_t) gid;
		if (ret) {
			im->im_status = IDMAP_STATUS_LOOKUPFAIL;
			im->im_id = nobodygid;
		}
		return;
	}
}

static int
validateascii(char *string, u_int32_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (string[i] == '\0')
			break;

		if (string[i] & 0x80)
			return (-1);
	}

	if ((i >= len) || string[i] != '\0')
		return (-1);

	return (i + 1);
}

static int
addfield(char **bpp, ssize_t *bsizp, char *fld)
{
	char ch, *bp = *bpp;
	ssize_t bsiz = *bsizp;

	while ((ch = *fld++) != '\0' && bsiz > 0) {
		switch(ch) {
		case ' ':
		case '\t':
		case '\n':
		case '\\':
			if (bsiz >= 4) {
				bp += snprintf(bp, bsiz, "\\%03o", ch);
				bsiz -= 4;
			}
			break;
		default:
			*bp++ = ch;
			bsiz--;
			break;
		}
	}

	if (bsiz < 1 || ch != '\0')
		return (-1);

	*bp++ = ' ';
	bsiz--;

	*bpp = bp;
	*bsizp = bsiz;

	return (0);
}

static int
getfield(char **bpp, char *fld, size_t fldsz)
{
	char *bp;
	u_int val, n;

	while ((bp = strsep(bpp, " ")) != NULL && bp[0] == '\0')
		;

	if (bp == NULL || bp[0] == '\0' || bp[0] == '\n')
		return (-1);

	while (*bp != '\0' && fldsz > 1) {
		if (*bp == '\\') {
			if ((n = sscanf(bp, "\\%03o", &val)) != 1)
				return (-1);
			if (val > (char)-1)
				return (-1);
			*fld++ = (char)val;
			bp += 4;
		} else {
			*fld++ = *bp;
			bp++;
		}
		fldsz--;
	}

	if (*bp != '\0')
		return (-1);
	*fld = '\0';

	return (0);
}
/*
 * mydaemon creates a pipe between the partent and child
 * process. The parent process will wait until the
 * child dies or writes a '1' on the pipe signaling
 * that it started successfully.
 */
int pipefds[2] = { -1, -1};

void
mydaemon(int nochdir, int noclose)
{
	int pid, status, tempfd;

	if (pipe(pipefds) < 0)
		err(1, "mydaemon: pipe() failed: errno %d", errno);

	if ((pid = fork ()) < 0)
		err(1, "mydaemon: fork() failed: errno %d", errno);

	if (pid != 0) {
		/*
		 * Parent. Wait for status from child.
		 */
		close(pipefds[1]);
		if (read(pipefds[0], &status, 1) != 1)
			exit(1);
		exit (0);
	}
	/* Child.	*/
	close(pipefds[0]);
	setsid ();
	if (nochdir == 0) {
		if (chdir ("/") == -1)
			err(1, "mydaemon: chdir() failed: errno %d", errno);
	}

	while (pipefds[1] <= 2) {
		pipefds[1] = dup(pipefds[1]);
		if (pipefds[1] < 0)
			err(1, "mydaemon: dup() failed: errno %d", errno);
	}

	if (noclose == 0) {
		tempfd = open("/dev/null", O_RDWR);
		if (tempfd < 0)
			tempfd = open("/", O_RDONLY);
		if (tempfd >= 0) {
			dup2(tempfd, 0);
			dup2(tempfd, 1);
			dup2(tempfd, 2);
			closeall(3);
		} else
			closeall(0);
	}

	return;
}
void
release_parent(void)
{
	int status;

	if (pipefds[1] > 0) {
		if (write(pipefds[1], &status, 1) != 1) {
			err(1, "Writing to parent pipe failed: errno %d (%s)\n",
				errno, strerror(errno));
		}
		close(pipefds[1]);
		pipefds[1] = -1;
	}
}
