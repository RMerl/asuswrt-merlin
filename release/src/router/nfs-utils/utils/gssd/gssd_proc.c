/*
  gssd_proc.c

  Copyright (c) 2000-2004 The Regents of the University of Michigan.
  All rights reserved.

  Copyright (c) 2000 Dug Song <dugsong@UMICH.EDU>.
  Copyright (c) 2001 Andy Adamson <andros@UMICH.EDU>.
  Copyright (c) 2002 Marius Aamodt Eriksen <marius@UMICH.EDU>.
  Copyright (c) 2002 Bruce Fields <bfields@UMICH.EDU>
  Copyright (c) 2004 Kevin Coffman <kwc@umich.edu>
  All rights reserved, all wrongs reversed.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/param.h>
#include <rpc/rpc.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fsuid.h>

#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <dirent.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <gssapi/gssapi.h>
#include <netdb.h>

#include "gssd.h"
#include "err_util.h"
#include "gss_util.h"
#include "krb5_util.h"
#include "context.h"
#include "nfsrpc.h"

/*
 * pollarray:
 *      array of struct pollfd suitable to pass to poll. initialized to
 *      zero - a zero struct is ignored by poll() because the events mask is 0.
 *
 * clnt_list:
 *      linked list of struct clnt_info which associates a clntXXX directory
 *	with an index into pollarray[], and other basic data about that client.
 *
 * Directory structure: created by the kernel nfs client
 *      {pipefs_nfsdir}/clntXX             : one per rpc_clnt struct in the kernel
 *      {pipefs_nfsdir}/clntXX/krb5        : read uid for which kernel wants
 *					    a context, write the resulting context
 *      {pipefs_nfsdir}/clntXX/info        : stores info such as server name
 *
 * Algorithm:
 *      Poll all {pipefs_nfsdir}/clntXX/krb5 files.  When ready, data read
 *      is a uid; performs rpcsec_gss context initialization protocol to
 *      get a cred for that user.  Writes result to corresponding krb5 file
 *      in a form the kernel code will understand.
 *      In addition, we make sure we are notified whenever anything is
 *      created or destroyed in {pipefs_nfsdir} or in an of the clntXX directories,
 *      and rescan the whole {pipefs_nfsdir} when this happens.
 */

struct pollfd * pollarray;

int pollsize;  /* the size of pollaray (in pollfd's) */

/*
 * convert a presentation address string to a sockaddr_storage struct. Returns
 * true on success and false on failure.
 *
 * Note that we do not populate the sin6_scope_id field here for IPv6 addrs.
 * gssd nececessarily relies on hostname resolution and DNS AAAA records
 * do not generally contain scope-id's. This means that GSSAPI auth really
 * can't work with IPv6 link-local addresses.
 *
 * We *could* consider changing this if we did something like adopt the
 * Microsoft "standard" of using the ipv6-literal.net domainname, but it's
 * not really feasible at present.
 */
static int
addrstr_to_sockaddr(struct sockaddr *sa, const char *addr, const int port)
{
	struct sockaddr_in	*s4 = (struct sockaddr_in *) sa;
#ifdef IPV6_SUPPORTED
	struct sockaddr_in6	*s6 = (struct sockaddr_in6 *) sa;
#endif /* IPV6_SUPPORTED */

	if (inet_pton(AF_INET, addr, &s4->sin_addr)) {
		s4->sin_family = AF_INET;
		s4->sin_port = htons(port);
#ifdef IPV6_SUPPORTED
	} else if (inet_pton(AF_INET6, addr, &s6->sin6_addr)) {
		s6->sin6_family = AF_INET6;
		s6->sin6_port = htons(port);
#endif /* IPV6_SUPPORTED */
	} else {
		printerr(0, "ERROR: unable to convert %s to address\n", addr);
		return 0;
	}

	return 1;
}

/*
 * convert a sockaddr to a hostname
 */
static char *
sockaddr_to_hostname(const struct sockaddr *sa, const char *addr)
{
	socklen_t		addrlen;
	int			err;
	char 			*hostname;
	char			hbuf[NI_MAXHOST];

	switch (sa->sa_family) {
	case AF_INET:
		addrlen = sizeof(struct sockaddr_in);
		break;
#ifdef IPV6_SUPPORTED
	case AF_INET6:
		addrlen = sizeof(struct sockaddr_in6);
		break;
#endif /* IPV6_SUPPORTED */
	default:
		printerr(0, "ERROR: unrecognized addr family %d\n",
			 sa->sa_family);
		return NULL;
	}

	err = getnameinfo(sa, addrlen, hbuf, sizeof(hbuf), NULL, 0,
			  NI_NAMEREQD);
	if (err) {
		printerr(0, "ERROR: unable to resolve %s to hostname: %s\n",
			 addr, err == EAI_SYSTEM ? strerror(err) :
						   gai_strerror(err));
		return NULL;
	}

	hostname = strdup(hbuf);

	return hostname;
}

/* XXX buffer problems: */
static int
read_service_info(char *info_file_name, char **servicename, char **servername,
		  int *prog, int *vers, char **protocol,
		  struct sockaddr *addr) {
#define INFOBUFLEN 256
	char		buf[INFOBUFLEN + 1];
	static char	dummy[128];
	int		nbytes;
	static char	service[128];
	static char	address[128];
	char		program[16];
	char		version[16];
	char		protoname[16];
	char		cb_port[128];
	char		*p;
	int		fd = -1;
	int		numfields;
	int		port = 0;

	*servicename = *servername = *protocol = NULL;

	if ((fd = open(info_file_name, O_RDONLY)) == -1) {
		printerr(0, "ERROR: can't open %s: %s\n", info_file_name,
			 strerror(errno));
		goto fail;
	}
	if ((nbytes = read(fd, buf, INFOBUFLEN)) == -1)
		goto fail;
	close(fd);
	buf[nbytes] = '\0';

	numfields = sscanf(buf,"RPC server: %127s\n"
		   "service: %127s %15s version %15s\n"
		   "address: %127s\n"
		   "protocol: %15s\n",
		   dummy,
		   service, program, version,
		   address,
		   protoname);

	if (numfields == 5) {
		strcpy(protoname, "tcp");
	} else if (numfields != 6) {
		goto fail;
	}

	cb_port[0] = '\0';
	if ((p = strstr(buf, "port")) != NULL)
		sscanf(p, "port: %127s\n", cb_port);

	/* check service, program, and version */
	if(memcmp(service, "nfs", 3)) return -1;
	*prog = atoi(program + 1); /* skip open paren */
	*vers = atoi(version);
	if((*prog != 100003) || ((*vers != 2) && (*vers != 3) && (*vers != 4)))
		goto fail;

	if (cb_port[0] != '\0') {
		port = atoi(cb_port);
		if (port < 0 || port > 65535)
			goto fail;
	}

	if (!addrstr_to_sockaddr(addr, address, port))
		goto fail;

	*servername = sockaddr_to_hostname(addr, address);
	if (*servername == NULL)
		goto fail;

	nbytes = snprintf(buf, INFOBUFLEN, "%s@%s", service, *servername);
	if (nbytes > INFOBUFLEN)
		goto fail;

	if (!(*servicename = calloc(strlen(buf) + 1, 1)))
		goto fail;
	memcpy(*servicename, buf, strlen(buf));

	if (!(*protocol = strdup(protoname)))
		goto fail;
	return 0;
fail:
	printerr(0, "ERROR: failed to read service info\n");
	if (fd != -1) close(fd);
	free(*servername);
	free(*servicename);
	free(*protocol);
	*servicename = *servername = *protocol = NULL;
	return -1;
}

static void
destroy_client(struct clnt_info *clp)
{
	if (clp->krb5_poll_index != -1)
		memset(&pollarray[clp->krb5_poll_index], 0,
					sizeof(struct pollfd));
	if (clp->spkm3_poll_index != -1)
		memset(&pollarray[clp->spkm3_poll_index], 0,
					sizeof(struct pollfd));
	if (clp->dir_fd != -1) close(clp->dir_fd);
	if (clp->krb5_fd != -1) close(clp->krb5_fd);
	if (clp->spkm3_fd != -1) close(clp->spkm3_fd);
	free(clp->dirname);
	free(clp->servicename);
	free(clp->servername);
	free(clp->protocol);
	free(clp);
}

static struct clnt_info *
insert_new_clnt(void)
{
	struct clnt_info	*clp = NULL;

	if (!(clp = (struct clnt_info *)calloc(1,sizeof(struct clnt_info)))) {
		printerr(0, "ERROR: can't malloc clnt_info: %s\n",
			 strerror(errno));
		goto out;
	}
	clp->krb5_poll_index = -1;
	clp->spkm3_poll_index = -1;
	clp->krb5_fd = -1;
	clp->spkm3_fd = -1;
	clp->dir_fd = -1;

	TAILQ_INSERT_HEAD(&clnt_list, clp, list);
out:
	return clp;
}

static int
process_clnt_dir_files(struct clnt_info * clp)
{
	char	kname[32];
	char	sname[32];
	char	info_file_name[32];

	if (clp->krb5_fd == -1) {
		snprintf(kname, sizeof(kname), "%s/krb5", clp->dirname);
		clp->krb5_fd = open(kname, O_RDWR);
	}
	if (clp->spkm3_fd == -1) {
		snprintf(sname, sizeof(sname), "%s/spkm3", clp->dirname);
		clp->spkm3_fd = open(sname, O_RDWR);
	}
	if((clp->krb5_fd == -1) && (clp->spkm3_fd == -1))
		return -1;
	snprintf(info_file_name, sizeof(info_file_name), "%s/info",
			clp->dirname);
	if ((clp->servicename == NULL) &&
	     read_service_info(info_file_name, &clp->servicename,
				&clp->servername, &clp->prog, &clp->vers,
				&clp->protocol, (struct sockaddr *) &clp->addr))
		return -1;
	return 0;
}

static int
get_poll_index(int *ind)
{
	int i;

	*ind = -1;
	for (i=0; i<FD_ALLOC_BLOCK; i++) {
		if (pollarray[i].events == 0) {
			*ind = i;
			break;
		}
	}
	if (*ind == -1) {
		printerr(0, "ERROR: No pollarray slots open\n");
		return -1;
	}
	return 0;
}


static int
insert_clnt_poll(struct clnt_info *clp)
{
	if ((clp->krb5_fd != -1) && (clp->krb5_poll_index == -1)) {
		if (get_poll_index(&clp->krb5_poll_index)) {
			printerr(0, "ERROR: Too many krb5 clients\n");
			return -1;
		}
		pollarray[clp->krb5_poll_index].fd = clp->krb5_fd;
		pollarray[clp->krb5_poll_index].events |= POLLIN;
	}

	if ((clp->spkm3_fd != -1) && (clp->spkm3_poll_index == -1)) {
		if (get_poll_index(&clp->spkm3_poll_index)) {
			printerr(0, "ERROR: Too many spkm3 clients\n");
			return -1;
		}
		pollarray[clp->spkm3_poll_index].fd = clp->spkm3_fd;
		pollarray[clp->spkm3_poll_index].events |= POLLIN;
	}

	return 0;
}

static void
process_clnt_dir(char *dir)
{
	struct clnt_info *	clp;

	if (!(clp = insert_new_clnt()))
		goto fail_destroy_client;

	if (!(clp->dirname = calloc(strlen(dir) + 1, 1))) {
		goto fail_destroy_client;
	}
	memcpy(clp->dirname, dir, strlen(dir));
	if ((clp->dir_fd = open(clp->dirname, O_RDONLY)) == -1) {
		printerr(0, "ERROR: can't open %s: %s\n",
			 clp->dirname, strerror(errno));
		goto fail_destroy_client;
	}
	fcntl(clp->dir_fd, F_SETSIG, DNOTIFY_SIGNAL);
	fcntl(clp->dir_fd, F_NOTIFY, DN_CREATE | DN_DELETE | DN_MULTISHOT);

	if (process_clnt_dir_files(clp))
		goto fail_keep_client;

	if (insert_clnt_poll(clp))
		goto fail_destroy_client;

	return;

fail_destroy_client:
	if (clp) {
		TAILQ_REMOVE(&clnt_list, clp, list);
		destroy_client(clp);
	}
fail_keep_client:
	/* We couldn't find some subdirectories, but we keep the client
	 * around in case we get a notification on the directory when the
	 * subdirectories are created. */
	return;
}

void
init_client_list(void)
{
	TAILQ_INIT(&clnt_list);
	/* Eventually plan to grow/shrink poll array: */
	pollsize = FD_ALLOC_BLOCK;
	pollarray = calloc(pollsize, sizeof(struct pollfd));
}

/*
 * This is run after a DNOTIFY signal, and should clear up any
 * directories that are no longer around, and re-scan any existing
 * directories, since the DNOTIFY could have been in there.
 */
static void
update_old_clients(struct dirent **namelist, int size)
{
	struct clnt_info *clp;
	void *saveprev;
	int i, stillhere;

	for (clp = clnt_list.tqh_first; clp != NULL; clp = clp->list.tqe_next) {
		stillhere = 0;
		for (i=0; i < size; i++) {
			if (!strcmp(clp->dirname, namelist[i]->d_name)) {
				stillhere = 1;
				break;
			}
		}
		if (!stillhere) {
			printerr(2, "destroying client %s\n", clp->dirname);
			saveprev = clp->list.tqe_prev;
			TAILQ_REMOVE(&clnt_list, clp, list);
			destroy_client(clp);
			clp = saveprev;
		}
	}
	for (clp = clnt_list.tqh_first; clp != NULL; clp = clp->list.tqe_next) {
		if (!process_clnt_dir_files(clp))
			insert_clnt_poll(clp);
	}
}

/* Search for a client by directory name, return 1 if found, 0 otherwise */
static int
find_client(char *dirname)
{
	struct clnt_info	*clp;

	for (clp = clnt_list.tqh_first; clp != NULL; clp = clp->list.tqe_next)
		if (!strcmp(clp->dirname, dirname))
			return 1;
	return 0;
}

/* Used to read (and re-read) list of clients, set up poll array. */
int
update_client_list(void)
{
	struct dirent **namelist;
	int i, j;

	if (chdir(pipefs_nfsdir) < 0) {
		printerr(0, "ERROR: can't chdir to %s: %s\n",
			 pipefs_nfsdir, strerror(errno));
		return -1;
	}

	j = scandir(pipefs_nfsdir, &namelist, NULL, alphasort);
	if (j < 0) {
		printerr(0, "ERROR: can't scandir %s: %s\n",
			 pipefs_nfsdir, strerror(errno));
		return -1;
	}
	update_old_clients(namelist, j);
	for (i=0; i < j; i++) {
		if (i < FD_ALLOC_BLOCK
				&& !strncmp(namelist[i]->d_name, "clnt", 4)
				&& !find_client(namelist[i]->d_name))
			process_clnt_dir(namelist[i]->d_name);
		free(namelist[i]);
	}

	free(namelist);
	return 0;
}

static int
do_downcall(int k5_fd, uid_t uid, struct authgss_private_data *pd,
	    gss_buffer_desc *context_token)
{
	char    *buf = NULL, *p = NULL, *end = NULL;
	unsigned int timeout = context_timeout;
	unsigned int buf_size = 0;

	printerr(1, "doing downcall\n");
	buf_size = sizeof(uid) + sizeof(timeout) + sizeof(pd->pd_seq_win) +
		sizeof(pd->pd_ctx_hndl.length) + pd->pd_ctx_hndl.length +
		sizeof(context_token->length) + context_token->length;
	p = buf = malloc(buf_size);
	end = buf + buf_size;

	if (WRITE_BYTES(&p, end, uid)) goto out_err;
	if (WRITE_BYTES(&p, end, timeout)) goto out_err;
	if (WRITE_BYTES(&p, end, pd->pd_seq_win)) goto out_err;
	if (write_buffer(&p, end, &pd->pd_ctx_hndl)) goto out_err;
	if (write_buffer(&p, end, context_token)) goto out_err;

	if (write(k5_fd, buf, p - buf) < p - buf) goto out_err;
	if (buf) free(buf);
	return 0;
out_err:
	if (buf) free(buf);
	printerr(1, "Failed to write downcall!\n");
	return -1;
}

static int
do_error_downcall(int k5_fd, uid_t uid, int err)
{
	char	buf[1024];
	char	*p = buf, *end = buf + 1024;
	unsigned int timeout = 0;
	int	zero = 0;

	printerr(1, "doing error downcall\n");

	if (WRITE_BYTES(&p, end, uid)) goto out_err;
	if (WRITE_BYTES(&p, end, timeout)) goto out_err;
	/* use seq_win = 0 to indicate an error: */
	if (WRITE_BYTES(&p, end, zero)) goto out_err;
	if (WRITE_BYTES(&p, end, err)) goto out_err;

	if (write(k5_fd, buf, p - buf) < p - buf) goto out_err;
	return 0;
out_err:
	printerr(1, "Failed to write error downcall!\n");
	return -1;
}

/*
 * If the port isn't already set, do an rpcbind query to the remote server
 * using the program and version and get the port. 
 *
 * Newer kernels send the value of the port= mount option in the "info"
 * file for the upcall or '0' for NFSv2/3. For NFSv4 it sends the value
 * of the port= option or '2049'. The port field in a new sockaddr should
 * reflect the value that was sent by the kernel.
 */
static int
populate_port(struct sockaddr *sa, const socklen_t salen,
	      const rpcprog_t program, const rpcvers_t version,
	      const unsigned short protocol)
{
	struct sockaddr_in	*s4 = (struct sockaddr_in *) sa;
#ifdef IPV6_SUPPORTED
	struct sockaddr_in6	*s6 = (struct sockaddr_in6 *) sa;
#endif /* IPV6_SUPPORTED */
	unsigned short		port;

	/*
	 * Newer kernels send the port in the upcall. If we already have
	 * the port, there's no need to look it up.
	 */
	switch (sa->sa_family) {
	case AF_INET:
		if (s4->sin_port != 0) {
			printerr(2, "DEBUG: port already set to %d\n",
				 ntohs(s4->sin_port));
			return 1;
		}
		break;
#ifdef IPV6_SUPPORTED
	case AF_INET6:
		if (s6->sin6_port != 0) {
			printerr(2, "DEBUG: port already set to %d\n",
				 ntohs(s6->sin6_port));
			return 1;
		}
		break;
#endif /* IPV6_SUPPORTED */
	default:
		printerr(0, "ERROR: unsupported address family %d\n",
			    sa->sa_family);
		return 0;
	}

	/*
	 * Newer kernels that send the port in the upcall set the value to
	 * 2049 for NFSv4 mounts when one isn't specified. The check below is
	 * only for kernels that don't send the port in the upcall. For those
	 * we either have to do an rpcbind query or set it to the standard
	 * port. Doing a query could be problematic (firewalls, etc), so take
	 * the latter approach.
	 */
	if (program == 100003 && version == 4) {
		port = 2049;
		goto set_port;
	}

	port = nfs_getport(sa, salen, program, version, protocol);
	if (!port) {
		printerr(0, "ERROR: unable to obtain port for prog %ld "
			    "vers %ld\n", program, version);
		return 0;
	}

set_port:
	printerr(2, "DEBUG: setting port to %hu for prog %lu vers %lu\n", port,
		 program, version);

	switch (sa->sa_family) {
	case AF_INET:
		s4->sin_port = htons(port);
		break;
#ifdef IPV6_SUPPORTED
	case AF_INET6:
		s6->sin6_port = htons(port);
		break;
#endif /* IPV6_SUPPORTED */
	}

	return 1;
}

/*
 * Create an RPC connection and establish an authenticated
 * gss context with a server.
 */
int create_auth_rpc_client(struct clnt_info *clp,
			   CLIENT **clnt_return,
			   AUTH **auth_return,
			   uid_t uid,
			   int authtype)
{
	CLIENT			*rpc_clnt = NULL;
	struct rpc_gss_sec	sec;
	AUTH			*auth = NULL;
	uid_t			save_uid = -1;
	int			retval = -1;
	OM_uint32		min_stat;
	char			rpc_errmsg[1024];
	int			protocol;
	struct timeval		timeout = {5, 0};
	struct sockaddr		*addr = (struct sockaddr *) &clp->addr;
	socklen_t		salen;

	/* Create the context as the user (not as root) */
	save_uid = geteuid();
	if (setfsuid(uid) != 0) {
		printerr(0, "WARNING: Failed to setfsuid for "
			    "user with uid %d\n", uid);
		goto out_fail;
	}
	printerr(2, "creating context using fsuid %d (save_uid %d)\n",
			uid, save_uid);

	sec.qop = GSS_C_QOP_DEFAULT;
	sec.svc = RPCSEC_GSS_SVC_NONE;
	sec.cred = GSS_C_NO_CREDENTIAL;
	sec.req_flags = 0;
	if (authtype == AUTHTYPE_KRB5) {
		sec.mech = (gss_OID)&krb5oid;
		sec.req_flags = GSS_C_MUTUAL_FLAG;
	}
	else if (authtype == AUTHTYPE_SPKM3) {
		sec.mech = (gss_OID)&spkm3oid;
		/* XXX sec.req_flags = GSS_C_ANON_FLAG;
		 * Need a way to switch....
		 */
		sec.req_flags = GSS_C_MUTUAL_FLAG;
	}
	else {
		printerr(0, "ERROR: Invalid authentication type (%d) "
			"in create_auth_rpc_client\n", authtype);
		goto out_fail;
	}


	if (authtype == AUTHTYPE_KRB5) {
#ifdef HAVE_SET_ALLOWABLE_ENCTYPES
		/*
		 * Do this before creating rpc connection since we won't need
		 * rpc connection if it fails!
		 */
		if (limit_krb5_enctypes(&sec, uid)) {
			printerr(1, "WARNING: Failed while limiting krb5 "
				    "encryption types for user with uid %d\n",
				 uid);
			goto out_fail;
		}
#endif
	}

	/* create an rpc connection to the nfs server */

	printerr(2, "creating %s client for server %s\n", clp->protocol,
			clp->servername);

	if ((strcmp(clp->protocol, "tcp")) == 0) {
		protocol = IPPROTO_TCP;
	} else if ((strcmp(clp->protocol, "udp")) == 0) {
		protocol = IPPROTO_UDP;
	} else {
		printerr(0, "WARNING: unrecognized protocol, '%s', requested "
			 "for connection to server %s for user with uid %d\n",
			 clp->protocol, clp->servername, uid);
		goto out_fail;
	}

	switch (addr->sa_family) {
	case AF_INET:
		salen = sizeof(struct sockaddr_in);
		break;
#ifdef IPV6_SUPPORTED
	case AF_INET6:
		salen = sizeof(struct sockaddr_in6);
		break;
#endif /* IPV6_SUPPORTED */
	default:
		printerr(1, "ERROR: Unknown address family %d\n",
			 addr->sa_family);
		goto out_fail;
	}

	if (!populate_port(addr, salen, clp->prog, clp->vers, protocol))
		goto out_fail;

	rpc_clnt = nfs_get_rpcclient(addr, salen, protocol, clp->prog,
				     clp->vers, &timeout);
	if (!rpc_clnt) {
		snprintf(rpc_errmsg, sizeof(rpc_errmsg),
			 "WARNING: can't create %s rpc_clnt to server %s for "
			 "user with uid %d",
			 protocol == IPPROTO_TCP ? "tcp" : "udp",
			 clp->servername, uid);
		printerr(0, "%s\n",
			 clnt_spcreateerror(rpc_errmsg));
		goto out_fail;
	}

	printerr(2, "creating context with server %s\n", clp->servicename);
	auth = authgss_create_default(rpc_clnt, clp->servicename, &sec);
	if (!auth) {
		/* Our caller should print appropriate message */
		printerr(2, "WARNING: Failed to create %s context for "
			    "user with uid %d for server %s\n",
			(authtype == AUTHTYPE_KRB5 ? "krb5":"spkm3"),
			 uid, clp->servername);
		goto out_fail;
	}

	/* Success !!! */
	rpc_clnt->cl_auth = auth;
	*clnt_return = rpc_clnt;
	*auth_return = auth;
	retval = 0;

  out:
	if (sec.cred != GSS_C_NO_CREDENTIAL)
		gss_release_cred(&min_stat, &sec.cred);
	/* Restore euid to original value */
	if ((save_uid != -1) && (setfsuid(save_uid) != uid)) {
		printerr(0, "WARNING: Failed to restore fsuid"
			    " to uid %d from %d\n", save_uid, uid);
	}
	return retval;

  out_fail:
	/* Only destroy here if failure.  Otherwise, caller is responsible */
	if (rpc_clnt) clnt_destroy(rpc_clnt);

	goto out;
}


/*
 * this code uses the userland rpcsec gss library to create a krb5
 * context on behalf of the kernel
 */
void
handle_krb5_upcall(struct clnt_info *clp)
{
	uid_t			uid;
	CLIENT			*rpc_clnt = NULL;
	AUTH			*auth = NULL;
	struct authgss_private_data pd;
	gss_buffer_desc		token;
	char			**credlist = NULL;
	char			**ccname;
	char			**dirname;
	int			create_resp = -1;

	printerr(1, "handling krb5 upcall\n");

	token.length = 0;
	token.value = NULL;
	memset(&pd, 0, sizeof(struct authgss_private_data));

	if (read(clp->krb5_fd, &uid, sizeof(uid)) < sizeof(uid)) {
		printerr(0, "WARNING: failed reading uid from krb5 "
			    "upcall pipe: %s\n", strerror(errno));
		goto out;
	}

	if (uid != 0 || (uid == 0 && root_uses_machine_creds == 0)) {
		/* Tell krb5 gss which credentials cache to use */
		for (dirname = ccachesearch; *dirname != NULL; dirname++) {
			if (gssd_setup_krb5_user_gss_ccache(uid, clp->servername, *dirname) == 0)
				create_resp = create_auth_rpc_client(clp, &rpc_clnt, &auth, uid,
							     AUTHTYPE_KRB5);
			if (create_resp == 0)
				break;
		}
	}
	if (create_resp != 0) {
		if (uid == 0 && root_uses_machine_creds == 1) {
			int success = 0;

			gssd_refresh_krb5_machine_credential(clp->servername,
							     NULL);
			/*
			 * Get a list of credential cache names and try each
			 * of them until one works or we've tried them all
			 */
			if (gssd_get_krb5_machine_cred_list(&credlist)) {
				printerr(0, "ERROR: No credentials found "
					 "for connection to server %s\n",
					 clp->servername);
					goto out_return_error;
			}
			for (ccname = credlist; ccname && *ccname; ccname++) {
				gssd_setup_krb5_machine_gss_ccache(*ccname);
				if ((create_auth_rpc_client(clp, &rpc_clnt,
							    &auth, uid,
							    AUTHTYPE_KRB5)) == 0) {
					/* Success! */
					success++;
					break;
				}
				printerr(2, "WARNING: Failed to create krb5 context "
					 "for user with uid %d with credentials "
					 "cache %s for server %s\n",
					 uid, *ccname, clp->servername);
			}
			gssd_free_krb5_machine_cred_list(credlist);
			if (!success) {
				printerr(1, "WARNING: Failed to create krb5 context "
					 "for user with uid %d with any "
					 "credentials cache for server %s\n",
					 uid, clp->servername);
				goto out_return_error;
			}
		} else {
			printerr(1, "WARNING: Failed to create krb5 context "
				 "for user with uid %d for server %s\n",
				 uid, clp->servername);
			goto out_return_error;
		}
	}

	if (!authgss_get_private_data(auth, &pd)) {
		printerr(1, "WARNING: Failed to obtain authentication "
			    "data for user with uid %d for server %s\n",
			 uid, clp->servername);
		goto out_return_error;
	}

	if (serialize_context_for_kernel(pd.pd_ctx, &token, &krb5oid, NULL)) {
		printerr(0, "WARNING: Failed to serialize krb5 context for "
			    "user with uid %d for server %s\n",
			 uid, clp->servername);
		goto out_return_error;
	}

	do_downcall(clp->krb5_fd, uid, &pd, &token);

out:
	if (token.value)
		free(token.value);
#ifndef HAVE_LIBTIRPC
	if (pd.pd_ctx_hndl.length != 0)
		authgss_free_private_data(&pd);
#endif
	if (auth)
		AUTH_DESTROY(auth);
	if (rpc_clnt)
		clnt_destroy(rpc_clnt);
	return;

out_return_error:
	do_error_downcall(clp->krb5_fd, uid, -1);
	goto out;
}

/*
 * this code uses the userland rpcsec gss library to create an spkm3
 * context on behalf of the kernel
 */
void
handle_spkm3_upcall(struct clnt_info *clp)
{
	uid_t			uid;
	CLIENT			*rpc_clnt = NULL;
	AUTH			*auth = NULL;
	struct authgss_private_data pd;
	gss_buffer_desc		token;

	printerr(2, "handling spkm3 upcall\n");

	token.length = 0;
	token.value = NULL;

	if (read(clp->spkm3_fd, &uid, sizeof(uid)) < sizeof(uid)) {
		printerr(0, "WARNING: failed reading uid from spkm3 "
			 "upcall pipe: %s\n", strerror(errno));
		goto out;
	}

	if (create_auth_rpc_client(clp, &rpc_clnt, &auth, uid, AUTHTYPE_SPKM3)) {
		printerr(0, "WARNING: Failed to create spkm3 context for "
			    "user with uid %d\n", uid);
		goto out_return_error;
	}

	if (!authgss_get_private_data(auth, &pd)) {
		printerr(0, "WARNING: Failed to obtain authentication "
			    "data for user with uid %d for server %s\n",
			 uid, clp->servername);
		goto out_return_error;
	}

	if (serialize_context_for_kernel(pd.pd_ctx, &token, &spkm3oid, NULL)) {
		printerr(0, "WARNING: Failed to serialize spkm3 context for "
			    "user with uid %d for server\n",
			 uid, clp->servername);
		goto out_return_error;
	}

	do_downcall(clp->spkm3_fd, uid, &pd, &token);

out:
	if (token.value)
		free(token.value);
	if (auth)
		AUTH_DESTROY(auth);
	if (rpc_clnt)
		clnt_destroy(rpc_clnt);
	return;

out_return_error:
	do_error_downcall(clp->spkm3_fd, uid, -1);
	goto out;
}
