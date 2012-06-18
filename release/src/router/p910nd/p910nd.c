/*
 *	Port 9100+n daemon
 *	Accepts a connection from port 9100+n and copy stream to
 *	/dev/lpn, where n = 0,1,2.
 *
 *	GPLv2 license, read COPYING
 *
 *	Run standalone as: p910nd [0|1|2]
 *
 *	Run under inetd as:
 *	p910n stream tcp nowait root /usr/sbin/tcpd p910nd [0|1|2]
 *	 where p910n is an /etc/services entry for
 *	 port 9100, 9101 or 9102 as the case may be.
 *	 root can be replaced by any uid with rw permission on /dev/lpn
 *
 *	Port 9100+n will then be passively opened
 *	n defaults to 0
 *
 *	Version 0.93
 *	Fix open call to include mode, required for O_CREAT
 *
 *	Version 0.92
 *	Patches by Dave Brown.  Use raw I/O syscalls instead of
 *	stdio buffering.  Buffer system to handle talkative bidi
 *	devices better on low-powered hosts.
 *
 *	Version 0.91
 *	Patch by Hans Harder.  Close printer device after each use to
 *	avoid crashing with hotpluggable devices going away.
 *	Don't wait 10 seconds after successful open.
 *
 *	Version 0.9
 *	Patch by Kostas Liakakis to keep retrying every 10 seconds
 *	if EBUSY is returned by open_printer, apparently NetBSD
 *	does this if the printer is not on.
 *	Patch by Albert Bartoszko (al_bin@vp_pl), August 2006
 *	Work with hotpluggable devices
 *	Improve Makefile
 *
 *	(The last two patches conflict somewhat, Liakakis's patch
 *	retries opening the device every 10 seconds until successful,
 *	whereas Bartoszko's patch exits if the printer device cannot be opened.
 *	The problem is with a hotpluggable device, that device node may
 *	not appear again.
 *
 *	I have opted for Liakakis's behaviour. Let me know if this can
 *	be improved. Perhaps we need another option that chooses the
 *	behaviour. - Ken)
 *
 *	Version 0.8
 *	Allow specifying address to bind to
 *
 *	Version 0.7
 *	Bidirectional data transfer
 *
 *	Version 0.6
 *	Arne Bernin fixed some cast warnings, corrected the version number
 *	and added a -v option to print the version.
 *
 *	Version 0.5
 *	-DUSE_LIBWRAP and -lwrap enables hosts_access (tcpwrappers) checking.
 *
 *	Version 0.4
 *	Ken Yap (greenpossum@users.sourceforge.net), April 2001
 *	Placed under GPL.
 *
 *	Added -f switch to specify device which overrides /dev/lpn.
 *	But number is still required get distinct ports and locks.
 *
 *	Added locking so that two invocations of the daemon under inetd
 *	don't try to open the printer at the same time. This can happen
 *	even if there is one host running clients because the previous
 *	client can exit after it has sent all data but the printer has not
 *	finished printing and inetd starts up a new daemon when the next
 *	request comes in too soon.
 *
 *	Various things could be Linux specific. I don't
 *	think there is much demand for this program outside of PCs,
 *	but if you port it to other distributions or platforms,
 *	I'd be happy to receive your patches.
 */

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<getopt.h>
#include	<ctype.h>
#include	<string.h>
#include	<fcntl.h>
#include	<netdb.h>
#include	<syslog.h>
#include	<errno.h>
#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/resource.h>
#include	<sys/stat.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>

#ifdef	USE_LIBWRAP
#include	"tcpd.h"
int allow_severity, deny_severity;
extern int hosts_ctl(char *daemon, char *client_name, char *client_addr, char *client_user);
#endif

#define		BASEPORT	9100
#define		PIDFILE		"/var/run/p910%cd.pid"
#ifdef		LOCKFILE_DIR
#define		LOCKFILE	LOCKFILE_DIR "/p910%cd"
#else
#define		LOCKFILE	"/var/lock/subsys/p910%cd"
#endif
#ifndef		PRINTERFILE
#define         PRINTERFILE     "/dev/lp%c"
#endif
#define		LOGOPTS		LOG_ERR

#define		BUFFER_SIZE	8192

/* Circular buffer used for each direction. */
typedef struct {
	int detectEof;		/* If nonzero, EOF is marked when read returns 0 bytes. */
	int infd;		/* Input file descriptor. */
	int outfd;		/* Output file descriptor. */
	int startidx;		/* Index of the start of valid data. */
	int endidx;		/* Index of the end of valid data. */
	int bytes;		/* The number of bytes currently buffered. */
	int totalin;		/* Total bytes that have been read. */
	int totalout;		/* Total bytes that have been written. */
	int eof;		/* Nonzero indicates the input file has reached EOF. */
	int err;		/* Nonzero indicates an error detected on the output file. */
	char buffer[BUFFER_SIZE];	/* Buffered data goes here. */
} Buffer_t;

static char *progname;
static char version[] = "Version 0.93";
static char copyright[] = "Copyright (c) 2008 Ken Yap, GPLv2";
static int lockfd = -1;
static char *device = 0;
static int bidir = 0;
static char *bindaddr = 0;


/* Helper function: convert a struct sockaddr address (IPv4 and IPv6) to a string */
char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
	switch(sa->sa_family) {
		case AF_INET:
			inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
			break;
		case AF_INET6:
			inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
			break;
		default:
			strncpy(s, "Unknown AF", maxlen);
		return NULL;
	}
	return s;
}

uint16_t get_port(const struct sockaddr *sa)
{
	uint16_t port;
	switch(sa->sa_family) {
		case AF_INET:
			port = ntohs(((struct sockaddr_in *)sa)->sin_port);
			break;
		case AF_INET6:
			port = ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
			break;
		default:
			return 0;
	}
	return port;
}

void usage(void)
{
	fprintf(stderr, "%s %s %s\n", progname, version, copyright);
	fprintf(stderr, "Usage: %s [-f device] [-i bindaddr] [-bv] [0|1|2]\n", progname);
	exit(1);
}

void show_version(void)
{
	fprintf(stdout, "%s %s\n", progname, version);
}

int open_printer(int lpnumber)
{
	int lp;
	char lpname[sizeof(PRINTERFILE)];

#ifdef	TESTING
	(void)snprintf(lpname, sizeof(lpname), "/dev/tty");
#else
	(void)snprintf(lpname, sizeof(lpname), PRINTERFILE, lpnumber);
#endif
	if (device == 0)
		device = lpname;
	if ((lp = open(device, bidir ? O_RDWR : O_WRONLY)) == -1) {
		if (errno != EBUSY)
			syslog(LOGOPTS, "%s: %m, will try opening later\n", device);
	}
	return (lp);
}

int get_lock(int lpnumber)
{
	char lockname[sizeof(LOCKFILE)];
	struct flock lplock;

	(void)snprintf(lockname, sizeof(lockname), LOCKFILE, lpnumber);
	if ((lockfd = open(lockname, O_CREAT | O_RDWR, 0666)) < 0) {
		syslog(LOGOPTS, "%s: %m\n", lockname);
		return (0);
	}
	memset(&lplock, 0, sizeof(lplock));
	lplock.l_type = F_WRLCK;
	lplock.l_pid = getpid();
	if (fcntl(lockfd, F_SETLKW, &lplock) < 0) {
		syslog(LOGOPTS, "%s: %m\n", lockname);
		return (0);
	}
	return (1);
}

void free_lock(void)
{
	if (lockfd >= 0)
		(void)close(lockfd);
}

/* Initializes the buffer, at the start. */
void initBuffer(Buffer_t * b, int infd, int outfd, int detectEof)
{
	b->detectEof = detectEof;
	b->infd = infd;
	b->outfd = outfd;
	b->startidx = 0;
	b->endidx = 0;
	b->bytes = 0;
	b->totalin = 0;
	b->totalout = 0;
	b->eof = 0;
	b->err = 0;
}

/* Sets the readfds and writefds (used by select) based on current buffer state. */
void prepBuffer(Buffer_t * b, fd_set * readfds, fd_set * writefds)
{
	if (!b->err && b->bytes != 0) {
		FD_SET(b->outfd, writefds);
	}
	if (!b->eof && b->bytes < sizeof(b->buffer)) {
		FD_SET(b->infd, readfds);
	}
}

/* Reads data into a buffer from its input file. */
ssize_t readBuffer(Buffer_t * b)
{
	int avail;
	ssize_t result = 1;
	/* If err, the data will not be written, so no need to store it. */
	if (b->bytes == 0 || b->err) {
		/* The buffer is empty. */
		b->startidx = b->endidx = 0;
		avail = sizeof(b->buffer);
	} else if (b->bytes == sizeof(b->buffer)) {
		/* The buffer is full. */
		avail = 0;
	} else if (b->endidx > b->startidx) {
		/* The buffer is not wrapped: from endidx to end of buffer is free. */
		avail = sizeof(b->buffer) - b->endidx;
	} else {
		/* The buffer is wrapped: gap between endidx and startidx is free. */
		avail = b->startidx - b->endidx;
	}
	if (avail) {
		result = read(b->infd, b->buffer + b->endidx, avail);
		if (result > 0) {
			/* Some data was read. Update accordingly. */
			b->endidx += result;
			b->totalin += result;
			b->bytes += result;
			if (b->endidx == sizeof(b->buffer)) {
				/* Time to wrap the buffer. */
				b->endidx = 0;
			}
		} else if (result < 0 || b->detectEof) {
			/* Mark end-of-file on input error, or on zero return if detectEof. */
			b->eof = 1;
		}
	}
	/* Return the value returned by read(), which is -1, 0, or #bytes read. */
	return result;
}

/* Writes data from a buffer to the output file. */
ssize_t writeBuffer(Buffer_t * b)
{
	int avail;
	ssize_t result = 1;
	if (b->bytes == 0 || b->err) {
		/* Buffer is empty. */
		avail = 0;
	} else if (b->endidx > b->startidx) {
		/* Buffer is not wrapped. Can write all the data. */
		avail = b->endidx - b->startidx;
	} else {
		/* Buffer is wrapped. Can only write the top (first) part. */
		avail = sizeof(b->buffer) - b->startidx;
	}
	if (avail) {
		result = write(b->outfd, b->buffer + b->startidx, avail);
		if (result < 0) {
			/* Mark the output file in an error condition. */
			syslog(LOGOPTS, "write: %m\n");
			b->err = 1;
		} else {
			/* Zero or more bytes were written. */
			b->startidx += result;
			b->totalout += result;
			b->bytes -= result;
			if (b->startidx == sizeof(b->buffer)) {
				/* Unwrap the buffer. */
				b->startidx = 0;
			}
		}
	}
	/* Return the write() result, -1 (error), or #bytes written. */
	return result;
}

/* Copy network data from file descriptor fd (network) to lp (printer) until EOS */
/* If bidir, also copy data from printer (lp) to network (fd). */
int copy_stream(int fd, int lp)
{
	int result;
	Buffer_t networkToPrinterBuffer;
	initBuffer(&networkToPrinterBuffer, fd, lp, 1);

	if (bidir) {
		struct timeval now;
		struct timeval then;
		struct timeval timeout;
		int timer = 0;
		Buffer_t printerToNetworkBuffer;
		initBuffer(&printerToNetworkBuffer, lp, fd, 0);
		fd_set readfds;
		fd_set writefds;
		/* Initially read from both streams, don't write to either. */
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_SET(lp, &readfds);
		FD_SET(fd, &readfds);
		/* Finish when no longer reading fd, and no longer writing to lp. */
		/* Although the printer to network stream may not be finished, that does not matter. */
		while ((FD_ISSET(fd, &readfds)) || (FD_ISSET(lp, &writefds))) {
			int maxfd = lp > fd ? lp : fd;
			if (timer) {
				/* Delay after reading from the printer, so the */
				/* return stream cannot dominate. */
				/* Don't read from the printer until the timer expires. */
				gettimeofday(&now, 0);
				if ((now.tv_sec > then.tv_sec) || (now.tv_sec == then.tv_sec && now.tv_usec > then.tv_usec)) {
					timer = 0;
				} else {
					timeout.tv_sec = then.tv_sec - now.tv_sec;
					timeout.tv_usec = then.tv_usec - now.tv_usec;
					if (timeout.tv_usec < 0) {
						timeout.tv_usec += 1000000;
						timeout.tv_sec--;
					}
					FD_CLR(lp, &readfds);
				}
			}
			if (timer) {
				result = select(maxfd + 1, &readfds, &writefds, 0, &timeout);
			} else {
				result = select(maxfd + 1, &readfds, &writefds, 0, 0);
			}
			if (result < 0)
				return (result);
			if (FD_ISSET(fd, &readfds)) {
				/* Read network data. */
				result = readBuffer(&networkToPrinterBuffer);
			}
			if (FD_ISSET(lp, &readfds)) {
				/* Read printer data, but pace it more slowly. */
				result = readBuffer(&printerToNetworkBuffer);
				if (result >= 0) {
					gettimeofday(&then, 0);
					// wait 100 msec before reading again.
					then.tv_usec += 100000;
					if (then.tv_usec > 1000000) {
						then.tv_usec -= 1000000;
						then.tv_sec++;
					}
					timer = 1;
				}
			}
			if (FD_ISSET(lp, &writefds)) {
				/* Write data to printer. */
				result = writeBuffer(&networkToPrinterBuffer);
			}
			if (FD_ISSET(fd, &writefds)) {
				/* Write data to network. */
				result = writeBuffer(&printerToNetworkBuffer);
				/* If socket write error, stop reading from printer */
				if (result < 0)
					networkToPrinterBuffer.eof = 1;
			}
			/* Prepare for next iteration. */
			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			prepBuffer(&networkToPrinterBuffer, &readfds, &writefds);
			prepBuffer(&printerToNetworkBuffer, &readfds, &writefds);
		}
		syslog(LOG_NOTICE,
		       "Finished job: %d bytes received, %d bytes sent\n",
		       networkToPrinterBuffer.totalout, printerToNetworkBuffer.totalout);
		return (0);
	} else {
		/* Unidirectional: simply read from network, and write to printer. */
		while ((result = readBuffer(&networkToPrinterBuffer)) > 0) {
			(void)writeBuffer(&networkToPrinterBuffer);
		}
		syslog(LOG_NOTICE, "Finished job: %d bytes received\n", networkToPrinterBuffer.totalout);
		return (result);
	}
}

void one_job(int lpnumber)
{
	int lp, open_sleep = 10;
	struct sockaddr_storage client;
	socklen_t clientlen = sizeof(client);

	if (getpeername(0, (struct sockaddr *)&client, &clientlen) >= 0) {
		char host[INET6_ADDRSTRLEN];
		syslog(LOG_NOTICE, "Connection from %s port %hu\n", get_ip_str((struct sockaddr *)&client, host, sizeof(host)), get_port((struct sockaddr *)&client));
	}
	if (get_lock(lpnumber) == 0)
		return;
	/* Make sure lp device is open... */
	while ((lp = open_printer(lpnumber)) == -1) {
		sleep(open_sleep);
		if (open_sleep < 320) /* ~5 min interval to avoid spam in syslog */
			open_sleep *= 2;
	}
	if (copy_stream(0, lp) < 0)
		syslog(LOGOPTS, "copy_stream: %m\n");
	close(lp);
	free_lock();
}

void server(int lpnumber)
{
	struct rlimit resourcelimit;
#ifdef	USE_GETPROTOBYNAME
	struct protoent *proto;
#endif
	int netfd, fd, lp, one = 1;
	int open_sleep = 10;
	socklen_t clientlen;
	struct sockaddr_storage client;
	struct addrinfo hints, *res, *ressave;
	char pidfilename[sizeof(PIDFILE)];
	char service[sizeof(BASEPORT+lpnumber-'0')+1];
	FILE *f;

#ifndef	TESTING
	switch (fork()) {
	case -1:
		syslog(LOGOPTS, "fork: %m\n");
		exit(1);
	case 0:		/* child */
		break;
	default:		/* parent */
		exit(0);
	}
	/* Now in child process */
	resourcelimit.rlim_max = 0;
	if (getrlimit(RLIMIT_NOFILE, &resourcelimit) < 0) {
		syslog(LOGOPTS, "getrlimit: %m\n");
		exit(1);
	}
	for (fd = 0; fd < resourcelimit.rlim_max; ++fd)
		(void)close(fd);
	if (setsid() < 0) {
		syslog(LOGOPTS, "setsid: %m\n");
		exit(1);
	}
	(void)chdir("/");
	(void)umask(022);
	fd = open("/dev/null", O_RDWR);	/* stdin */
	(void)dup(fd);		/* stdout */
	(void)dup(fd);		/* stderr */
	(void)snprintf(pidfilename, sizeof(pidfilename), PIDFILE, lpnumber);
	if ((f = fopen(pidfilename, "w")) == NULL) {
		syslog(LOGOPTS, "%s: %m\n", pidfilename);
		exit(1);
	}
	(void)fprintf(f, "%d\n", getpid());
	(void)fclose(f);
	if (get_lock(lpnumber) == 0)
		exit(1);
#endif
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	(void)snprintf(service, sizeof(service), "%hu", (BASEPORT + lpnumber - '0'));
	if (getaddrinfo(bindaddr, service, &hints, &res) != 0) {
		syslog(LOGOPTS, "getaddr: %m\n");
		exit(1);
	}
	ressave = res;
	while (res) {
#ifdef	USE_GETPROTOBYNAME
		if ((proto = getprotobyname("tcp6")) == NULL) {
			if ((proto = getprotobyname("tcp")) == NULL) {
				syslog(LOGOPTS, "Cannot find protocol for TCP!\n");
				exit(1);
			}
		}
		if ((netfd = socket(res->ai_family, res->ai_socktype, proto->p_proto)) < 0)
#else
		if ((netfd = socket(res->ai_family, res->ai_socktype, IPPROTO_IP)) < 0)
#endif
		{
			syslog(LOGOPTS, "socket: %m\n");
			close(netfd);
			res = res->ai_next;
			continue;
		}
		if (setsockopt(netfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
			syslog(LOGOPTS, "setsocketopt: %m\n");
			close(netfd);
			res = res->ai_next;
			continue;
		}
		if (bind(netfd, res->ai_addr, res->ai_addrlen) < 0) {
			syslog(LOGOPTS, "bind: %m\n");
			close(netfd);
			res = res->ai_next;
			continue;
		}
		if (listen(netfd, 5) < 0) {
			syslog(LOGOPTS, "listen: %m\n");
			close(netfd);
			res = res->ai_next;
			continue;
		}
		break;
	}
	freeaddrinfo(ressave);
	clientlen = sizeof(client);
	memset(&client, 0, sizeof(client));
	while ((fd = accept(netfd, (struct sockaddr *)&client, &clientlen)) >= 0) {
#ifdef	USE_LIBWRAP
		if (hosts_ctl("p910nd", STRING_UNKNOWN, inet_ntoa(client.sin_addr), STRING_UNKNOWN) == 0) {
			syslog(LOGOPTS,
			       "Connection from %s port %hd rejected\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
			close(fd);
			continue;
		}
#endif
		char host[INET6_ADDRSTRLEN];
		syslog(LOG_NOTICE, "Connection from %s port %hu accepted\n", get_ip_str((struct sockaddr *)&client, host, sizeof(host)), get_port((struct sockaddr *)&client));
		/*write(fd, "Printing", 8); */

		/* Make sure lp device is open... */
		while ((lp = open_printer(lpnumber)) == -1) {
			sleep(open_sleep);
			if (open_sleep < 320) /* ~5 min interval to avoid spam in syslog */
				open_sleep *= 2;
		}
		open_sleep = 10;

		if (copy_stream(fd, lp) < 0)
			syslog(LOGOPTS, "copy_stream: %m\n");
		(void)close(fd);
		(void)close(lp);
	}
	syslog(LOGOPTS, "accept: %m\n");
	free_lock();
	exit(1);
}

int is_standalone(void)
{
	struct sockaddr_storage bind_addr;
	socklen_t ba_len;

	/*
	 * Check to see if a socket was passed to us from (x)inetd.
	 *
	 * Use getsockname() to determine if descriptor 0 is indeed a socket
	 * (and thus we are probably a child of (x)inetd) or if it is instead
	 * something else and we are running standalone.
	 */
	ba_len = sizeof(bind_addr);
	if (getsockname(0, (struct sockaddr *)&bind_addr, &ba_len) == 0)
		return (0);	/* under (x)inetd */
	if (errno != ENOTSOCK)	/* strange... */
		syslog(LOGOPTS, "getsockname: %m\n");
	return (1);
}

int main(int argc, char *argv[])
{
	int c, lpnumber;
	char *p;

	if (argc <= 0)		/* in case not provided in (x)inetd config */
		progname = "p910nd";
	else {
		progname = argv[0];
		if ((p = strrchr(progname, '/')) != 0)
			progname = p + 1;
	}
	lpnumber = '0';
	while ((c = getopt(argc, argv, "bi:f:v")) != EOF) {
		switch (c) {
		case 'b':
			bidir = 1;
			break;
		case 'f':
			device = optarg;
			break;
		case 'i':
			bindaddr = optarg;
			break;
		case 'v':
			show_version();
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0) {
		if (isdigit(argv[0][0]))
			lpnumber = argv[0][0];
	}
	/* change the n in argv[0] to match the port so ps will show that */
	if ((p = strstr(progname, "p910n")) != NULL)
		p[4] = lpnumber;

	/* We used to pass (LOG_PERROR|LOG_PID|LOG_LPR|LOG_ERR) to syslog, but
	 * syslog ignored the LOG_PID and LOG_PERROR option.  I.e. the intention
	 * was to add both options but the effect was to have neither.
	 * I disagree with the intention to add PERROR.  --Stef  */
	openlog(p, LOG_PID, LOG_LPR);
	if (is_standalone())
		server(lpnumber);
	else
		one_job(lpnumber);
	return (0);
}
