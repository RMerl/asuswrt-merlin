/*
 * pptpctrl.c
 *
 * PPTP control connection between PAC-PNS pair
 *
 * $Id: pptpctrl.c,v 1.20 2006/12/08 00:01:40 quozl Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __linux__
#define _GNU_SOURCE 1		/* kill() prototype, broken arpa/inet.h */
#endif

#include "our_syslog.h"

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_OPENPTY
#ifdef HAVE_PTY_H
#include <pty.h>
#include <termios.h>
#endif
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif
#endif

#ifdef __UCLIBC__
#define socklen_t int
#endif

#include "compat.h"
#include "pptpctrl.h"
#include "pptpgre.h"
#include "pptpdefs.h"
#include "ctrlpacket.h"
#include "defaults.h"
// placing net/if.h here fixes build on Solaris
#include <net/if.h>

static char *ppp_binary = PPP_BINARY;
static int pptp_logwtmp;
static int noipparam;			/* if true, don't send ipparam to ppp */
static char speed[32];
static char pppdxfig[256];
static pid_t pppfork;                   /* so we can kill it after disconnect */

/*
 * Global to handle dying
 *
 * I'd be nice if someone could figure out a way to do it 
 * without the global, but i don't think you can.. -tmk
 */
#define clientSocket 0		/* in case it changes back to a variable */
static u_int32_t call_id_pair;	/* call id (to terminate call) */

/* Needed by this and ctrlpacket.c */
int pptpctrl_debug = 0;		/* specifies if debugging is on or off */
uint16_t unique_call_id = 0xFFFF;	/* Start value for our call IDs on this TCP link */

int gargc;                     /* Command line argument count */
char **gargv;                  /* Command line argument vector */

/* Local function prototypes */
static void bail(int sigraised);
static void pptp_handle_ctrl_connection(char **pppaddrs, struct in_addr *inetaddrs);

static int startCall(char **pppaddrs, struct in_addr *inetaddrs);
static void launch_pppd(char **pppaddrs, struct in_addr *inetaddrs);

/* Oh the horror.. lets hope this covers all the ones we have to handle */
#if defined(O_NONBLOCK) && !defined(__sun__) && !defined(__sun)
#define OUR_NB_MODE O_NONBLOCK
#else
#define OUR_NB_MODE O_NDELAY
#endif

/* read a command line argument, a flag alone */
#define GETARG_INT(X) \
	X = atoi(argv[arg++])

/* read a command line argument, a string alone */
#define GETARG_STRING(X) \
	X = strdup(argv[arg++])

/* read a command line argument, a presence flag followed by string */
#define GETARG_VALUE(X) \
	if(atoi(argv[arg++]) != 0) \
		strlcpy(X, argv[arg++], sizeof(X)); \
	else \
		*X = '\0'

int main(int argc, char **argv)
{
	char pppLocal[16];		/* local IP to pass to pppd */
	char pppRemote[16];		/* remote IP address to pass to pppd */
	struct sockaddr_in addr;	/* client address */
	socklen_t addrlen;
	int arg = 1;
	int flags;
	struct in_addr inetaddrs[2];
	char *pppaddrs[2] = { pppLocal, pppRemote };

        gargc = argc;
        gargv = argv;

	/* fail if argument count invalid */
	if (argc < 7) {
		fprintf(stderr, "pptpctrl: insufficient arguments, see man pptpctrl\n");
		exit(2);
	}

	/* open a connection to the syslog daemon */
	openlog("pptpd", LOG_PID, PPTP_FACILITY);

	/* autoreap if supported */
	signal(SIGCHLD, SIG_IGN);

	/* note: update pptpctrl.8 if the argument list format is changed */
	GETARG_INT(pptpctrl_debug);
	GETARG_INT(noipparam);
	GETARG_VALUE(pppdxfig);
	GETARG_VALUE(speed);
	GETARG_VALUE(pppLocal);
	GETARG_VALUE(pppRemote);
	if (arg < argc) GETARG_INT(unique_call_id);
	if (arg < argc) GETARG_STRING(ppp_binary);
	if (arg < argc) GETARG_INT(pptp_logwtmp);
	
	if (pptpctrl_debug) {
		if (*pppLocal)
			syslog(LOG_DEBUG, "CTRL: local address = %s", pppLocal);
		if (*pppRemote)
			syslog(LOG_DEBUG, "CTRL: remote address = %s", pppRemote);
		if (*speed)
			syslog(LOG_DEBUG, "CTRL: pppd speed = %s", speed);
		if (*pppdxfig)
			syslog(LOG_DEBUG, "CTRL: pppd options file = %s", pppdxfig);
	}

	addrlen = sizeof(addr);
	if (getsockname(clientSocket, (struct sockaddr *) &addr, &addrlen) != 0) {
		syslog(LOG_ERR, "CTRL: getsockname() failed");
		syslog_perror("getsockname");
		close(clientSocket);
		bail(0);	/* NORETURN */
	}
	inetaddrs[0] = addr.sin_addr;

	addrlen = sizeof(addr);
	if (getpeername(clientSocket, (struct sockaddr *) &addr, &addrlen) != 0) {
		syslog(LOG_ERR, "CTRL: getpeername() failed");
		syslog_perror("getpeername");
		close(clientSocket);
		bail(0);	/* NORETURN */
	}
	inetaddrs[1] = addr.sin_addr;

	/* Set non-blocking */
	if ((flags = fcntl(clientSocket, F_GETFL, arg /* ignored */)) == -1 ||
	    fcntl(clientSocket, F_SETFL, flags|OUR_NB_MODE) == -1) {
		syslog(LOG_ERR, "CTRL: Failed to set client socket non-blocking");
		syslog_perror("fcntl");
		close(clientSocket);
		bail(0);	/* NORETURN */
	}

	
	/* Fiddle with argv */
        my_setproctitle(gargc, gargv, "pptpd [%s]%20c",
            inet_ntoa(addr.sin_addr), ' ');

	/* be ready for a grisly death */
	sigpipe_create();
	sigpipe_assign(SIGTERM);
	NOTE_VALUE(PAC, call_id_pair, htons(-1));
	NOTE_VALUE(PNS, call_id_pair, htons(-1));

	syslog(LOG_INFO, "CTRL: Client %s control connection started", inet_ntoa(addr.sin_addr));
	pptp_handle_ctrl_connection(pppaddrs, inetaddrs);
	syslog(LOG_DEBUG, "CTRL: Reaping child PPP[%i]", pppfork);
	if (pppfork > 0)
		waitpid(pppfork, NULL, 0);
	syslog(LOG_INFO, "CTRL: Client %s control connection finished", inet_ntoa(addr.sin_addr));

	bail(0);		/* NORETURN */
	return 1;		/* make gcc happy */
}


/*
 * Local functions only below
 */

/*
 * pptp_handle_ctrl_connection
 *
 * 1. read a packet (should be start_ctrl_conn_rqst)
 * 2. reply to packet (send a start_ctrl_conn_rply)
 * 3. proceed with GRE and CTRL connections
 *
 * args: pppaddrs - ppp local and remote addresses (strings)
 *       inetaddrs - local and client socket address
 * retn: 0 success, -1 failure
 */
static void pptp_handle_ctrl_connection(char **pppaddrs, struct in_addr *inetaddrs)
{

	/* For echo requests used to check link is alive */
	int echo_wait = FALSE;		/* Waiting for echo? */
	u_int32_t echo_count = 0;	/* Sequence # of echo */
	time_t echo_time = 0;		/* Time last echo req sent */
	struct timeval idleTime;	/* How long to select() */

	/* General local variables */
	ssize_t rply_size;		/* Reply packet size */
	fd_set fds;			/* For select() */
	int maxfd = clientSocket;	/* For select() */
	int send_packet;		/* Send a packet this time? */
#if BSDUSER_PPP || SLIRP
/* not needed by stuff which uses socketpair() in startCall() */
#define init 1
#else
	int init = 0;			/* Has pppd initialized the pty? */
#endif
	int pty_fd = -1;		/* File descriptor of pty */
	int gre_fd = -1;		/* Network file descriptor */
	int sig_fd = sigpipe_fd();	/* Signal pipe descriptor	*/

	unsigned char packet[PPTP_MAX_CTRL_PCKT_SIZE];
	unsigned char rply_packet[PPTP_MAX_CTRL_PCKT_SIZE];

	for (;;) {

		FD_ZERO(&fds);
		FD_SET(sig_fd, &fds);
		FD_SET(clientSocket, &fds);
		if (pty_fd != -1)
			FD_SET(pty_fd, &fds);
		if (gre_fd != -1 && init)
			FD_SET(gre_fd, &fds);

		/* set timeout */
		if (encaps_gre(-1, NULL, 0) || decaps_hdlc(-1, NULL, 0)) {
			idleTime.tv_sec = 0;
			idleTime.tv_usec = 50000; /* don't ack immediately */
		} else {
			idleTime.tv_sec = IDLE_WAIT;
			idleTime.tv_usec = 0;
		}

		/* default: do nothing */
		send_packet = FALSE;

		switch (select(maxfd + 1, &fds, NULL, NULL, &idleTime)) {
		case -1:	/* Error with select() */
			if (errno != EINTR)
				syslog(LOG_ERR, "CTRL: Error with select(), quitting");
			goto leave_clear_call;

		case 0:
			if (decaps_hdlc(-1, NULL, 0)) {
				if(decaps_hdlc(-1, encaps_gre, gre_fd))
					syslog(LOG_ERR, "CTRL: GRE re-xmit failed");
			} else if (encaps_gre(-1, NULL, 0))
				/* Pending ack and nothing else to do */
				encaps_gre(gre_fd, NULL, 0);	/* send ack with no payload */
			else if (echo_wait != TRUE) {
				/* Timeout. Start idle link detection. */
				echo_count++;
				if (pptpctrl_debug)
					syslog(LOG_DEBUG, "CTRL: Sending ECHO REQ id %d", echo_count);
				time(&echo_time);
				make_echo_req_packet(rply_packet, &rply_size, echo_count);
				echo_wait = TRUE;
				send_packet = TRUE;
			}
			break;

		default:
			break;
		}

		/* check for pending SIGTERM delivery */
		if (FD_ISSET(sig_fd, &fds)) {
			if (sigpipe_read() == SIGTERM)
				bail(SIGTERM);
		}

		/* detect startup of pppd */
#ifndef init
		if (!init && pty_fd != -1 && FD_ISSET(pty_fd, &fds))
			init = 1;
#endif

		/* handle actual packets */

		/* send from pty off via GRE */
		if (pty_fd != -1 && FD_ISSET(pty_fd, &fds) && decaps_hdlc(pty_fd, encaps_gre, gre_fd) < 0) {
			syslog(LOG_ERR, "CTRL: PTY read or GRE write failed (pty,gre)=(%d,%d)", pty_fd, gre_fd);
			break;
		}
		/* send from GRE off to pty */
		if (gre_fd != -1 && FD_ISSET(gre_fd, &fds) && decaps_gre(gre_fd, encaps_hdlc, pty_fd) < 0) {
			if (gre_fd == 6 && pty_fd == 5) {
				syslog(LOG_ERR, "CTRL: GRE-tunnel has collapsed (GRE read or PTY write failed (gre,pty)=(%d,%d))", gre_fd, pty_fd);
			} else {
				syslog(LOG_ERR, "CTRL: GRE read or PTY write failed (gre,pty)=(%d,%d)", gre_fd, pty_fd);
			}
			break;
		}
		/* handle control messages */

		if (FD_ISSET(clientSocket, &fds)) {
			send_packet = TRUE;
			switch (read_pptp_packet(clientSocket, packet, rply_packet, &rply_size)) {
			case 0:
				syslog(LOG_ERR, "CTRL: CTRL read failed");
				goto leave_drop_call;

			case -1:
				send_packet = FALSE;
				break;

			case STOP_CTRL_CONN_RQST:
				if (pptpctrl_debug)
					syslog(LOG_DEBUG, "CTRL: Received STOP CTRL CONN request (disconnecting)");
				if (gre_fd != -1 || pty_fd != -1)
					syslog(LOG_WARNING, "CTRL: Request to close control connection when call is open, closing");
				send_pptp_packet(clientSocket, rply_packet, rply_size);
				goto leave_drop_call;

			case CALL_CLR_RQST:
				if(pptpctrl_debug)
					syslog(LOG_DEBUG, "CTRL: Received CALL CLR request (closing call)");
				if (gre_fd == -1 || pty_fd == -1)
					syslog(LOG_WARNING, "CTRL: Request to close call but call not open");
				if (gre_fd != -1) {
					FD_CLR(gre_fd, &fds);
					close(gre_fd);
					gre_fd = -1;
				}
				if (pty_fd != -1) {
					FD_CLR(pty_fd, &fds);
					close(pty_fd);
					pty_fd = -1;
				}
				/* violating RFC */
                                goto leave_drop_call;

			case OUT_CALL_RQST:
				/* for killing off the link later (ugly) */
				NOTE_VALUE(PAC, call_id_pair, ((struct pptp_out_call_rply *) (rply_packet))->call_id);
				NOTE_VALUE(PNS, call_id_pair, ((struct pptp_out_call_rply *) (rply_packet))->call_id_peer);
				if (gre_fd != -1 || pty_fd != -1) {
					syslog(LOG_WARNING, "CTRL: Request to open call when call is already open, closing");
					if (gre_fd != -1) {
						FD_CLR(gre_fd, &fds);
						close(gre_fd);
						gre_fd = -1;
					}
					if (pty_fd != -1) {
						FD_CLR(pty_fd, &fds);
						close(pty_fd);
						pty_fd = -1;
					}
				}
                                /* change process title for accounting and status scripts */
                                my_setproctitle(gargc, gargv,
                                      "pptpd [%s:%04X - %04X]",
                                      inet_ntoa(inetaddrs[1]),
                                      ntohs(((struct pptp_out_call_rply *) (rply_packet))->call_id_peer),
                                      ntohs(((struct pptp_out_call_rply *) (rply_packet))->call_id));
				/* start the call, by launching pppd */
				syslog(LOG_INFO, "CTRL: Starting call (launching pppd, opening GRE)");
				pty_fd = startCall(pppaddrs, inetaddrs);
				if (pty_fd > maxfd) maxfd = pty_fd;
				if ((gre_fd = pptp_gre_init(call_id_pair, pty_fd, inetaddrs)) > maxfd)
					maxfd = gre_fd;
				break;

			case ECHO_RPLY:
				if (echo_wait == TRUE && ((struct pptp_echo_rply *) (packet))->identifier == echo_count)
					echo_wait = FALSE;
				else
					syslog(LOG_WARNING, "CTRL: Unexpected ECHO REPLY packet");
				/* FALLTHRU */
			case SET_LINK_INFO:
				send_packet = FALSE;
				break;

#ifdef PNS_MODE
			case IN_CALL_RQST:
			case IN_CALL_RPLY:
			case IN_CALL_CONN:
#endif

			case CALL_DISCONN_NTFY:
			case STOP_CTRL_CONN_RPLY:
				/* These don't generate replies.  Also they come from things we don't send in this section. */
				syslog(LOG_WARNING, "CTRL: Got a reply to a packet we didn't send");
				send_packet = FALSE;
				break;

			/* Otherwise, the already-formed reply will do fine, so send it */
			}
		}

		/* send reply packet - this may block, but it should be very rare */
		if (send_packet == TRUE && send_pptp_packet(clientSocket, rply_packet, rply_size) < 0) {
			syslog(LOG_ERR, "CTRL: Error sending GRE, aborting call");
			goto leave_clear_call;
		}

		/* waiting for echo reply and curtime - echo_time > max wait */
		if (echo_wait == TRUE && (time(NULL) - echo_time) > MAX_ECHO_WAIT) {
			syslog(LOG_INFO, "CTRL: Session timed out, ending call");
			goto leave_clear_call;
		}
	}
	/* Finished! :-) */
leave_drop_call:
	NOTE_VALUE(PAC, call_id_pair, htons(-1));
	NOTE_VALUE(PNS, call_id_pair, htons(-1));
	close(clientSocket);
leave_clear_call:
	/* leave clientSocket and call_id_pair alone for bail() */
	if (gre_fd != -1)
		close(gre_fd);
	gre_fd = -1;
	if (pty_fd != -1)
		close(pty_fd);
	pty_fd = -1;
	return;
#ifdef init
#undef init
#endif
}


/*
 * This is the custom exit() for this program.
 *
 * Updated to also be the default SIGTERM handler, and if
 * the link is going down for unnatural reasons, we will close it
 * right now, it's only been tested for win98, other tests would be nice
 * -tmk
 */
static void bail(int sigraised)
{
	if (sigraised)
		syslog(LOG_INFO, "CTRL: Exiting on signal %d", sigraised);

	/* send a disconnect to the other end */
	/* ignore any errors */
	if (GET_VALUE(PAC, call_id_pair) != htons(-1)) {
		fd_set connSet;		/* fd_set for select() */
		struct timeval tv;	/* time to wait for reply */
		unsigned char packet[PPTP_MAX_CTRL_PCKT_SIZE];
		unsigned char rply_packet[PPTP_MAX_CTRL_PCKT_SIZE];
		ssize_t rply_size;	/* reply packet size */
		int pkt;
		int retry = 0;

		if (pptpctrl_debug)
			syslog(LOG_DEBUG, "CTRL: Exiting with active call");

		make_call_admin_shutdown(rply_packet, &rply_size);
		if(send_pptp_packet(clientSocket, rply_packet, rply_size) < 0)
			goto skip;

		make_stop_ctrl_req(rply_packet, &rply_size);
		if(send_pptp_packet(clientSocket, rply_packet, rply_size) < 0)
			goto skip;

		FD_ZERO(&connSet);
		FD_SET(clientSocket, &connSet);
		tv.tv_sec = 5;	/* wait 5 secs for a reply then quit */
		tv.tv_usec = 0;

		/* Wait for STOP CTRL CONN RQST or RPLY */
		while (select(clientSocket + 1, &connSet, NULL, NULL, &tv) == 1) {
			switch((pkt = read_pptp_packet(clientSocket, packet, rply_packet, &rply_size))) {
			case STOP_CTRL_CONN_RQST:
				send_pptp_packet(clientSocket, rply_packet, rply_size);
				goto skip;
			case CALL_CLR_RQST:
				syslog(LOG_WARNING, "CTRL: Got call clear request after call manually shutdown - buggy client");
				break;
			case STOP_CTRL_CONN_RPLY:
				goto skip;
			case -1:
				syslog(LOG_WARNING, "CTRL: Retryable error in disconnect sequence");
				retry++;
				break;
			case 0:
				syslog(LOG_WARNING, "CTRL: Fatal error reading control message in disconnect sequence");
				goto skip;
			default:
				syslog(LOG_WARNING, "CTRL: Unexpected control message %d in disconnect sequence", pkt);
				retry++;
				break;
			}
			tv.tv_sec = 5;	/* wait 5 secs for another reply then quit */
			tv.tv_usec = 0;
			if (retry > 100) {
				syslog(LOG_WARNING, "CTRL: Too many retries (%d) - giving up", retry);
				break;
			}
		}

	skip:
		close(clientSocket);
	}

	if (pptpctrl_debug)
		syslog(LOG_DEBUG, "CTRL: Exiting now");
}

/*
 * startCall
 *
 * Launches PPPD for the call.
 *
 * args:        pppaddrs - local/remote IPs or "" for either/both if none
 * retn:        pty file descriptor
 *
 */
static int startCall(char **pppaddrs, struct in_addr *inetaddrs)
{
	/* PTY/TTY pair for talking to PPPd */
	int pty_fd, tty_fd;
	/* register pids of children */
#if BSDUSER_PPP || SLIRP
	int sockfd[2];

#ifndef AF_LOCAL
#ifdef AF_UNIX
#define AF_LOCAL AF_UNIX /* Old BSD */
#else
#define AF_LOCAL AF_FILE /* POSIX */
#endif
#endif

	/* userspace ppp doesn't need to waste a real pty/tty pair */
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd)) {
		syslog(LOG_ERR, "CTRL: socketpair() error");
		syslog_perror("socketpair");
		exit(1);
	}
	tty_fd = sockfd[0];
	pty_fd = sockfd[1];
#else
	/* Finds an open pty/tty pair */
	if (openpty(&pty_fd, &tty_fd, NULL, NULL, NULL) != 0) {
		syslog(LOG_ERR, "CTRL: openpty() error");
		syslog_perror("openpty");
		exit(1);
	} else {
		struct termios tios;

		/* Turn off echo in the slave - to prevent loopback.
		   pppd will do this, but might not do it before we
		   try to send data. */
		if (tcgetattr(tty_fd, &tios) < 0) {
			syslog(LOG_ERR, "CTRL: tcgetattr() error");
			syslog_perror("tcgetattr");
			exit(1);
		}
		tios.c_lflag &= ~(ECHO | ECHONL);
		if (tcsetattr(tty_fd, TCSAFLUSH, &tios) < 0) {
			syslog(LOG_ERR, "CTRL: tcsetattr() error");
			syslog_perror("tcsetattr");
			exit(1);
		}
	}
#endif
	if (pptpctrl_debug) {
		syslog(LOG_DEBUG, "CTRL: pty_fd = %d", pty_fd);
		syslog(LOG_DEBUG, "CTRL: tty_fd = %d", tty_fd);
	}
	/* Launch the PPPD  */
#ifndef HAVE_FORK
        switch(pppfork=vfork()){
#else
        switch(pppfork=fork()){
#endif
	case -1:	/* fork() error */
		syslog(LOG_ERR, "CTRL: Error forking to exec pppd");
		_exit(1);

	case 0:		/* child */
		if (dup2(tty_fd, 0) == -1) {
		  syslog(LOG_ERR, "CTRL: child tty_fd dup2 to stdin, %s",
			 strerror(errno));
		  exit(1);
		}
		if (dup2(tty_fd, 1) == -1) {
		  syslog(LOG_ERR, "CTRL: child tty_fd dup2 to stdout, %s",
			 strerror(errno));
		  exit(1);
		}
#if 0
		/* This must never be used if !HAVE_SYSLOG since that logs to stderr.
		 * Trying just never using it to see if it causes anyone else problems.
		 * It may let people see the pppd errors, which would be good.
		 */
		dup2(tty_fd, 2);
#endif
		if (tty_fd > 1)
			close(tty_fd);
		if (pty_fd > 1)
			close(pty_fd);
/* In case we move clientSocket back off stdin */
#ifndef clientSocket
		if (clientSocket > 1)
			close(clientSocket);
#elif clientSocket > 1
		close(clientSocket);
#endif
		launch_pppd(pppaddrs, inetaddrs);
		syslog(LOG_ERR, "CTRL: PPPD launch failed! (launch_pppd did not fork)");
		_exit(1);
	}
	
	close(tty_fd);
	return pty_fd;
}

/*
 * launch_pppd
 *
 * Launches the PPP daemon. The PPP daemon is responsible for assigning the
 * PPTP client its IP address.. These values are assigned via the command
 * line.
 *
 * Add return of connected ppp interface
 *
 * retn: 0 on success, -1 on failure.
 *
 */
static void launch_pppd(char **pppaddrs, struct in_addr *inetaddrs)
{
	char *pppd_argv[14];
	int an = 0;
	sigset_t sigs;

	pppd_argv[an++] = ppp_binary;

	if (pptpctrl_debug) {
		syslog(LOG_DEBUG, 
		       "CTRL (PPPD Launcher): program binary = %s", 
		       pppd_argv[an - 1]);
	}

#if BSDUSER_PPP

	/* The way that Brian Somers' user-land ppp works is to use the
	 * system name as a reference for most of the useful options. Hence
	 * most things can't be defined on the command line. On OpenBSD at
	 * least the file used for the systems is /etc/ppp/ppp.conf, where
	 * the pptp stanza should look something like:

	 pptp:
	 set speed sync
	 enable pap
	 enable chap
	 set dns a.a.a.a b.b.b.b
	 set ndbs x.x.x.x y.y.y.y
	 accept dns
	 add 10.0.0/24

	 * To be honest, at the time of writing, I haven't had the thing
	 * working enough to understand :) I will update this comment and
	 * make a sample config available when I get there.
	 */

	/* options for BSDUSER_PPP
	 *
	 * ignores IP addresses, config file option, speed
	 * fix usage info in pptpd.c and configure script if this changes
	 *
	 * IP addresses can be specified in /etc/ppp/ppp.secret per user
	 */
	pppd_argv[an++] = "-direct";
	pppd_argv[an++] = "pptp";	/* XXX this is the system name */
	/* should be dynamic - PMG */

#elif SLIRP

	/* options for SLIRP
	 *
	 * ignores IP addresses from config - SLIRP handles this
	 */
	pppd_argv[an++] = "-P";
	pppd_argv[an++] = "+chap";
	pppd_argv[an++] = "-b";

	/* If a speed has been specified, use it
	 * if not, use "smart" default (defaults.h)
	 */
	if (*speed) {
		pppd_argv[an++] = speed;
	} else {
		pppd_argv[an++] = PPP_SPEED_DEFAULT;
	}

	if (*pppdxfig) {
		pppd_argv[an++] = "-f";
		pppd_argv[an++] = pppdxfig;
	}

	if (pptpctrl_debug) {
		syslog(LOG_DEBUG, "CTRL (PPPD Launcher): Connection speed = %s", pppd_argv[an - 1]);
	}
#else

	/* options for 'normal' pppd */

	pppd_argv[an++] = "local";

	/* If a pppd option file is specified, use it
	 * if not, pppd will default to /etc/ppp/options
	 */
	if (*pppdxfig) {
		pppd_argv[an++] = "file";
		pppd_argv[an++] = pppdxfig;
	}
	
	/* If a speed has been specified, use it
	 * if not, use "smart" default (defaults.h)
	 */
	if (*speed) {
		pppd_argv[an++] = speed;
	} else {
		pppd_argv[an++] = PPP_SPEED_DEFAULT;
	}

	if (pptpctrl_debug) {
		if (*pppaddrs[0])
			syslog(LOG_DEBUG, "CTRL (PPPD Launcher): local address = %s", pppaddrs[0]);
		if (*pppaddrs[1])
			syslog(LOG_DEBUG, "CTRL (PPPD Launcher): remote address = %s", pppaddrs[1]);
	}
	
	if (*pppaddrs[0] || *pppaddrs[1]) {
		char pppInterfaceIPs[33];
		sprintf(pppInterfaceIPs, "%s:%s", pppaddrs[0], pppaddrs[1]);
		pppd_argv[an++] = pppInterfaceIPs;
	}
#endif

        if (!noipparam) {
                 pppd_argv[an++] = "ipparam";
                 pppd_argv[an++] = inet_ntoa(inetaddrs[1]);
        }

        if (pptp_logwtmp) {
                 pppd_argv[an++] = "plugin";
                 pppd_argv[an++] = "/usr/lib/pptpd/pptpd-logwtmp.so";
                 pppd_argv[an++] = "pptpd-original-ip";
                 pppd_argv[an++] = inet_ntoa(inetaddrs[1]);
        }

	/* argv arrays must always be NULL terminated */
	pppd_argv[an++] = NULL;
	/* make sure SIGCHLD is unblocked, pppd does not expect it */
	sigfillset(&sigs);
	sigprocmask(SIG_UNBLOCK, &sigs, NULL);
	/* run pppd now */
	execvp(pppd_argv[0], pppd_argv);
	/* execvp() failed */
	syslog(LOG_ERR, 
	       "CTRL (PPPD Launcher): Failed to launch PPP daemon. %s",
	       strerror(errno));
}

