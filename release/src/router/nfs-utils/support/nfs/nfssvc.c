/*
 * support/nfs/nfssvc.c
 *
 * Run an NFS daemon.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>


#include "nfslib.h"

#define NFSD_PORTS_FILE     "/proc/fs/nfsd/portlist"
#define NFSD_VERS_FILE    "/proc/fs/nfsd/versions"
#define NFSD_THREAD_FILE  "/proc/fs/nfsd/threads"

static void
nfssvc_setfds(int port, unsigned int ctlbits, char *haddr)
{
	int fd, n, on=1;
	char buf[BUFSIZ];
	int udpfd = -1, tcpfd = -1;
	struct sockaddr_in sin;

	fd = open(NFSD_PORTS_FILE, O_RDONLY);
	if (fd < 0)
		return;
	n = read(fd, buf, BUFSIZ);
	close(fd);
	if (n != 0)
		return;
	/* there are no ports currently open, so it is safe to
	 * try to open some and pass them through.
	 * Note: If the user explicitly asked for 'udp', then
	 * we should probably check if that is open, and should
	 * open it if not.  However we don't yet.  All sockets
	 * have to be opened when the first daemon is started.
	 */
	fd = open(NFSD_PORTS_FILE, O_WRONLY);
	if (fd < 0)
		return;
	sin.sin_family = AF_INET;
	sin.sin_port   = htons(port);
	sin.sin_addr.s_addr =  inet_addr(haddr);

	if (NFSCTL_UDPISSET(ctlbits)) {
		udpfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (udpfd < 0) {
			syslog(LOG_ERR, "nfssvc: unable to create UPD socket: "
				"errno %d (%s)\n", errno, strerror(errno));
			exit(1);
		}
		if (bind(udpfd, (struct  sockaddr  *)&sin, sizeof(sin)) < 0){
			syslog(LOG_ERR, "nfssvc: unable to bind UPD socket: "
				"errno %d (%s)\n", errno, strerror(errno));
			exit(1);
		}
	}

	if (NFSCTL_TCPISSET(ctlbits)) {
		tcpfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (tcpfd < 0) {
			syslog(LOG_ERR, "nfssvc: unable to createt tcp socket: "
				"errno %d (%s)\n", errno, strerror(errno));
			exit(1);
		}
		if (setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
			syslog(LOG_ERR, "nfssvc: unable to set SO_REUSEADDR: "
				"errno %d (%s)\n", errno, strerror(errno));
			exit(1);
		}
		if (bind(tcpfd, (struct  sockaddr  *)&sin, sizeof(sin)) < 0){
			syslog(LOG_ERR, "nfssvc: unable to bind TCP socket: "
				"errno %d (%s)\n", errno, strerror(errno));
			exit(1);
		}
		if (listen(tcpfd, 64) < 0){
			syslog(LOG_ERR, "nfssvc: unable to create listening socket: "
				"errno %d (%s)\n", errno, strerror(errno));
			exit(1);
		}
	}
	if (udpfd >= 0) {
		snprintf(buf, BUFSIZ,"%d\n", udpfd); 
		if (write(fd, buf, strlen(buf)) != strlen(buf)) {
			syslog(LOG_ERR, 
			       "nfssvc: writing fds to kernel failed: errno %d (%s)", 
			       errno, strerror(errno));
		}
		close(fd);
		fd = -1;
	}
	if (tcpfd >= 0) {
		if (fd < 0)
			fd = open(NFSD_PORTS_FILE, O_WRONLY);
		snprintf(buf, BUFSIZ,"%d\n", tcpfd); 
		if (write(fd, buf, strlen(buf)) != strlen(buf)) {
			syslog(LOG_ERR, 
			       "nfssvc: writing fds to kernel failed: errno %d (%s)", 
			       errno, strerror(errno));
		}
	}
	close(fd);

	return;
}
static void
nfssvc_versbits(unsigned int ctlbits)
{
	int fd, n, off;
	char buf[BUFSIZ], *ptr;

	ptr = buf;
	off = 0;
	fd = open(NFSD_VERS_FILE, O_WRONLY);
	if (fd < 0)
		return;

	for (n = NFSD_MINVERS; n <= NFSD_MAXVERS; n++) {
		if (NFSCTL_VERISSET(ctlbits, n))
		    off += snprintf(ptr+off, BUFSIZ - off, "+%d ", n);
		else
		    off += snprintf(ptr+off, BUFSIZ - off, "-%d ", n);
	}
	snprintf(ptr+off, BUFSIZ - off, "\n");
	if (write(fd, buf, strlen(buf)) != strlen(buf)) {
		syslog(LOG_ERR, "nfssvc: Setting version failed: errno %d (%s)", 
			errno, strerror(errno));
	}
	close(fd);

	return;
}
int
nfssvc(int port, int nrservs, unsigned int versbits, unsigned protobits,
	char *haddr)
{
	struct nfsctl_arg	arg;
	int fd;

	/* Note: must set versions before fds so that
	 * the ports get registered with portmap against correct
	 * versions
	 */
	nfssvc_versbits(versbits);
	nfssvc_setfds(port, protobits, haddr);

	fd = open(NFSD_THREAD_FILE, O_WRONLY);
	if (fd < 0)
		fd = open("/proc/fs/nfs/threads", O_WRONLY);
	if (fd >= 0) {
		/* 2.5+ kernel with nfsd filesystem mounted.
		 * Just write the number in.
		 * Cannot handle port number yet, but does anyone care?
		 */
		char buf[20];
		int n;
		snprintf(buf, 20,"%d\n", nrservs);
		n = write(fd, buf, strlen(buf));
		close(fd);
		if (n != strlen(buf))
			return -1;
		else
			return 0;
	}

	arg.ca_version = NFSCTL_VERSION;
	arg.ca_svc.svc_nthreads = nrservs;
	arg.ca_svc.svc_port = port;
	return nfsctl(NFSCTL_SVC, &arg, NULL);
}
