/*
 * pptpctrl.c
 *
 * PPTP control connection between PAC-PNS pair
 *
 * $Id: pptpctrl.c,v 1.19 2005/10/30 22:40:41 quozl Exp $
 *
 * 24.02.2008 Bugs fixed by Serbulov D. (SDY)
 *  + added SIGCHLD control
 *  + added PPPD kill on stop ses
 *  + correct exit on duplicate connection
 *
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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_OPENPTY
#ifdef HAVE_PTY_H
#include <pty.h>
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
#include "pptpdefs.h"
#include "ctrlpacket.h"
#include "defaults.h"
// placing net/if.h here fixes build on Solaris
#include <net/if.h>
#include <net/ethernet.h>
//#include "if_pppox.h" //Yau del
#include <linux/if_pppox.h>

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
int window=10;
int pptp_sock=-1;
struct in_addr inetaddrs[2];

/* Needed by this and ctrlpacket.c */
int pptpctrl_debug = 0;		/* specifies if debugging is on or off */
uint16_t unique_call_id = 0xFFFF;	/* Start value for our call IDs on this TCP link */
int pptp_timeout=1000000;

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
	if (arg < argc) GETARG_INT(pptp_timeout);
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
	sigpipe_assign(SIGCHLD);
	sigpipe_assign(SIGTERM);
	NOTE_VALUE(PAC, call_id_pair, htons(-1));
	NOTE_VALUE(PNS, call_id_pair, htons(-1));

	syslog(LOG_INFO, "CTRL: Client %s control connection started", inet_ntoa(addr.sin_addr));
	pptp_handle_ctrl_connection(pppaddrs, inetaddrs);

	syslog(LOG_DEBUG, "CTRL: Reaping child PPP[%i]", pppfork);
	bail(0);		/* NORETURN */
	syslog(LOG_INFO, "CTRL: Client %s control connection finished", inet_ntoa(addr.sin_addr));
	return 1;		/* make gcc happy */
}

//SDY special for prevent pppd session stay on air then need to stop
void waitclosefork(int sigraised)
{
	//SDY senf term to fork();
	if (pppfork > 0)
	{
		if (sigraised != SIGCHLD) {
			syslog(LOG_INFO, "CTRL: Client pppd TERM sending");
			kill(pppfork,SIGTERM);
		}
		syslog(LOG_INFO, "CTRL: Client pppd finish wait");
		waitpid(pppfork, NULL, 0);
	}
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
	int sig_fd = sigpipe_fd();	/* Signal pipe descriptor	*/

	unsigned char packet[PPTP_MAX_CTRL_PCKT_SIZE];
	unsigned char rply_packet[PPTP_MAX_CTRL_PCKT_SIZE];

	struct sockaddr_pppox dst_addr;

	for (;;) {

		FD_ZERO(&fds);
		FD_SET(sig_fd, &fds);
		FD_SET(clientSocket, &fds);

		/* set timeout */
		idleTime.tv_sec = IDLE_WAIT;
			idleTime.tv_usec = 0;

		/* default: do nothing */
		send_packet = FALSE;

		switch (select(maxfd + 1, &fds, NULL, NULL, &idleTime)) {
		case -1:	/* Error with select() */
			if (errno != EINTR)
				syslog(LOG_ERR, "CTRL: Error with select(), quitting");
			goto leave_clear_call;

		case 0:
			if (echo_wait != TRUE) {
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
			int signum = sigpipe_read();
			if (signum == SIGCHLD)
				bail(SIGCHLD);
			else if (signum == SIGTERM)
				bail(SIGTERM);
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
				send_pptp_packet(clientSocket, rply_packet, rply_size);
				goto leave_drop_call;

			case CALL_CLR_RQST:
				if(pptpctrl_debug)
					syslog(LOG_DEBUG, "CTRL: Received CALL CLR request (closing call)");
				/* violating RFC */
                                goto leave_drop_call;

			case OUT_CALL_RQST:
				/* for killing off the link later (ugly) */
				NOTE_VALUE(PAC, call_id_pair, ((struct pptp_out_call_rply *) (rply_packet))->call_id);
				NOTE_VALUE(PNS, call_id_pair, ((struct pptp_out_call_rply *) (rply_packet))->call_id_peer);

				dst_addr.sa_family=AF_PPPOX;
				dst_addr.sa_protocol=PX_PROTO_PPTP;
				dst_addr.sa_addr.pptp.call_id=htons(((struct pptp_out_call_rply *) (rply_packet))->call_id_peer);
				dst_addr.sa_addr.pptp.sin_addr=inetaddrs[1];

				if (connect(pptp_sock,(struct sockaddr*)&dst_addr,sizeof(dst_addr))){
					syslog(LOG_INFO,"CTRL: failed to connect PPTP socket (%s)\n",strerror(errno));
					goto leave_drop_call; //SDY close on correct!
					break;
				}


                                /* change process title for accounting and status scripts */
                                my_setproctitle(gargc, gargv,
                                      "pptpd [%s:%04X - %04X]",
                                      inet_ntoa(inetaddrs[1]),
                                      ntohs(((struct pptp_out_call_rply *) (rply_packet))->call_id_peer),
                                      ntohs(((struct pptp_out_call_rply *) (rply_packet))->call_id));
				/* start the call, by launching pppd */
				syslog(LOG_INFO, "CTRL: Starting call (launching pppd, opening GRE)");
				startCall(pppaddrs, inetaddrs);
				close(pptp_sock);
				pptp_sock=-1;
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

	waitclosefork(sigraised);

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
		NOTE_VALUE(PAC, call_id_pair, htons(-1));
		NOTE_VALUE(PNS, call_id_pair, htons(-1));
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

	return -1;
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
	char *pppd_argv[25];
	int an = 0;
	sigset_t sigs;
	char tmp[128];

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

	pppd_argv[an++] = "plugin";
	pppd_argv[an++] = "pptp.so";
	pppd_argv[an++] = "pptp_client";
	strcpy(tmp,inet_ntoa(inetaddrs[1]));
	pppd_argv[an++] = strdup(tmp);
	pppd_argv[an++] = "pptp_sock";
	sprintf(tmp,"%u",pptp_sock);
	pppd_argv[an++] = strdup(tmp);
	pppd_argv[an++] = "nodetach";

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
