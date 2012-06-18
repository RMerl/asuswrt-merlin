/*
 * pptpmanager.c
 *
 * Manages the PoPToP sessions.
 *
 * $Id: pptpmanager.c,v 1.14 2005/12/29 09:59:49 quozl Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __linux__
#define _GNU_SOURCE 1		/* broken arpa/inet.h */
#endif

#include "our_syslog.h"

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>

#ifdef HAVE_LIBWRAP
/* re-include, just in case HAVE_SYSLOG_H wasn't defined */
#include <syslog.h>
#include <tcpd.h>

int allow_severity = LOG_WARNING;
int deny_severity = LOG_WARNING;
#endif

#ifdef __UCLIBC__
#define socklen_t int
#endif

#include "configfile.h"
#include "defaults.h"
#include "pptpctrl.h"
#include "pptpdefs.h"
#include "pptpmanager.h"
#include "compat.h"

/* command line arg variables */
extern char *ppp_binary;
extern char *pppdoptstr;
extern char *speedstr;
extern char *bindaddr;
extern int pptp_debug;
extern int pptp_noipparam;
extern int pptp_logwtmp;
extern int pptp_delegate;

/* option for timeout on starting ctrl connection */
extern int pptp_stimeout;
extern int pptp_ptimeout;

extern int pptp_connections;
extern int keep_connections;

/* local function prototypes */
static void connectCall(int clientSocket, int clientNumber);
static int createHostSocket(int *hostSocket);

/* this end's call identifier */
uint16_t unique_call_id = 1;

/* slots - begin */

/* data about connection slots */
struct slot {
  pid_t pid;
  char *local;
  char *remote;
} *slots;

/* number of connection slots allocated */
int slot_count;

static void slot_iterate(struct slot *slots, int count, void (*callback) (struct slot *slot))
{
  int i;
  for(i=0; i<count; i++)
    (*callback)(&slots[i]);
}

static void slot_slot_init(struct slot *slot)
{
  slot->pid = 0;
  slot->local = NULL;
  slot->remote = NULL;
}

void slot_init(int count)
{
  slot_count = count;
  slots = (struct slot *) calloc(slot_count, sizeof(struct slot));
  slot_iterate(slots, slot_count, slot_slot_init);
}

static void slot_slot_free(struct slot *slot)
{
  slot->pid = 0;
  if (slot->local) free(slot->local);
  slot->local = NULL;
  if (slot->remote) free(slot->remote);
  slot->remote = NULL;
}

void slot_free()
{
  slot_iterate(slots, slot_count, slot_slot_free);
  free(slots);
  slots = NULL;
  slot_count = 0;
}

void slot_set_local(int i, char *ip)
{
  struct slot *slot = &slots[i];
  if (slot->local) free(slot->local);
  slot->local = strdup(ip);
}

void slot_set_remote(int i, char *ip)
{
  struct slot *slot = &slots[i];
  if (slot->remote) free(slot->remote);
  slot->remote = strdup(ip);
}

void slot_set_pid(int i, pid_t pid)
{
  struct slot *slot = &slots[i];
  slot->pid = pid; 
}

int slot_find_by_pid(pid_t pid)
{
  int i;
  for(i=0; i<slot_count; i++) {
    struct slot *slot = &slots[i];
    if (slot->pid == pid) return i;
  }
  return -1;
}

int slot_find_empty()
{
  return slot_find_by_pid(0);
}

char *slot_get_local(int i)
{
  struct slot *slot = &slots[i];
  return slot->local; 
}

char *slot_get_remote(int i)
{
  struct slot *slot = &slots[i];
  return slot->remote; 
}

/* slots - end */

static void sigchld_responder(int sig)
{
  int child, status;

  while ((child = waitpid(-1, &status, WNOHANG)) > 0) {
    if (pptp_delegate) {
      if (pptp_debug) syslog(LOG_DEBUG, "MGR: Reaped child %d", child);
    } else {
      int i;
      i = slot_find_by_pid(child);
      if (i != -1) {
	slot_set_pid(i, 0);
	if (pptp_debug) syslog(LOG_DEBUG, "MGR: Reaped child %d", child);
      } else {
	syslog(LOG_INFO, "MGR: Reaped unknown child %d", child);
      }
    }
  }
}

static void sigterm_responder(void)
{
  int i;
  for(i=0; i<slot_count; i++)
    kill(slots[i].pid,SIGTERM);
}

int pptp_manager(int argc, char **argv)
{
	int firstOpen = -1;
	int ctrl_pid;
	socklen_t addrsize;

	int hostSocket;
	fd_set connSet;

	int rc, sig_fd;

	rc = sigpipe_create();
	if (rc < 0) {
		syslog(LOG_ERR, "MGR: unable to setup sigchld pipe!");
		syslog_perror("sigpipe_create");
		exit(-1);
	}
	
	sigpipe_assign(SIGCHLD);
	sigpipe_assign(SIGTERM);
	sig_fd = sigpipe_fd();

	/* openlog() not required, done in pptpd.c */

	syslog(LOG_INFO, "MGR: Manager process started");

	if (!pptp_delegate) {
		syslog(LOG_INFO, "MGR: Maximum of %d connections available", 
		       pptp_connections);
	}

	/* Connect the host socket and activate it for listening */
	if (createHostSocket(&hostSocket) < 0) {
		syslog(LOG_ERR, "MGR: Couldn't create host socket");
		syslog_perror("createHostSocket");
		exit(-1);
	}

	while (1) {
		int max_fd;
		FD_ZERO(&connSet);
		if (pptp_delegate) {
			FD_SET(hostSocket, &connSet);
		} else {
			firstOpen = slot_find_empty();
			if (firstOpen == -1) {
				syslog(LOG_ERR, "MGR: No free connection slots or IPs - no more clients can connect!");
			} else {
				FD_SET(hostSocket, &connSet);
			}
		}
		max_fd = hostSocket;

		FD_SET(sig_fd, &connSet);
		if (max_fd < sig_fd) max_fd = sig_fd;

		while (1) {
			if (select(max_fd + 1, &connSet, NULL, NULL, NULL) != -1) break;
			if (errno == EINTR) continue;
			syslog(LOG_ERR, "MGR: Error with manager select()!");
			syslog_perror("select");
			exit(-1);
		}

		if (FD_ISSET(sig_fd, &connSet)) {	/* SIGCHLD */
			int signum = sigpipe_read();
			if (signum == SIGCHLD)
				sigchld_responder(signum);
			else if (signum == SIGTERM)
			{
			    if (!keep_connections) sigterm_responder();
				return signum;
			}
		}

		if (FD_ISSET(hostSocket, &connSet)) {	/* A call came! */
			int clientSocket;
			struct sockaddr_in client_addr;

			/* Accept call and launch PPTPCTRL */
			addrsize = sizeof(client_addr);
			clientSocket = accept(hostSocket, (struct sockaddr *) &client_addr, &addrsize);

#ifdef HAVE_LIBWRAP
			if (clientSocket != -1) {
				struct request_info r;
				request_init(&r, RQ_DAEMON, "pptpd", RQ_FILE, clientSocket, NULL);
				fromhost(&r);
				if (!hosts_access(&r)) {
					/* send a permission denied message? this is a tcp wrapper
					 * type deny so probably best to just drop it immediately like
					 * this, as tcp wrappers usually do.
					 */
					close(clientSocket);
					/* this would never be file descriptor 0, so use it as a error
					 * value
					 */
					clientSocket = 0;
				}
			}
#endif
			if (clientSocket == -1) {
				/* accept failed, but life goes on... */
				syslog(LOG_ERR, "MGR: accept() failed");
				syslog_perror("accept");
			} else if (clientSocket != 0) {
				fd_set rfds;
				struct timeval tv;
				struct pptp_header ph;

				/* TODO: this select below prevents
                                   other connections from being
                                   processed during the wait for the
                                   first data packet from the
                                   client. */

				/*
				 * DOS protection: get a peek at the first packet
				 * and do some checks on it before we continue.
				 * A 10 second timeout on the first packet seems reasonable
				 * to me,  if anything looks sus,  throw it away.
				 */

				FD_ZERO(&rfds);
				FD_SET(clientSocket, &rfds);
				tv.tv_sec = pptp_stimeout;
				tv.tv_usec = 0;
				if (select(clientSocket + 1, &rfds, NULL, NULL, &tv) <= 0) {
					syslog(LOG_ERR, "MGR: dropped slow initial connection");
					close(clientSocket);
					continue;
				}

				if (recv(clientSocket, &ph, sizeof(ph), MSG_PEEK) !=
						sizeof(ph)) {
					syslog(LOG_ERR, "MGR: dropped small initial connection");
					close(clientSocket);
					continue;
				}

				ph.length = ntohs(ph.length);
				ph.pptp_type = ntohs(ph.pptp_type);
				ph.magic = ntohl(ph.magic);
				ph.ctrl_type = ntohs(ph.ctrl_type);

				if (ph.length <= 0 || ph.length > PPTP_MAX_CTRL_PCKT_SIZE) {
					syslog(LOG_WARNING, "MGR: initial packet length %d outside "
							"(0 - %d)",  ph.length, PPTP_MAX_CTRL_PCKT_SIZE);
					goto dos_exit;
				}

				if (ph.magic != PPTP_MAGIC_COOKIE) {
					syslog(LOG_WARNING, "MGR: initial packet bad magic");
					goto dos_exit;
				}

				if (ph.pptp_type != PPTP_CTRL_MESSAGE) {
					syslog(LOG_WARNING, "MGR: initial packet has bad type");
					goto dos_exit;
				}

				if (ph.ctrl_type != START_CTRL_CONN_RQST) {
					syslog(LOG_WARNING, "MGR: initial packet has bad ctrl type "
							"0x%x", ph.ctrl_type);
		dos_exit:
					close(clientSocket);
					continue;
				}

#ifndef HAVE_FORK
				switch (ctrl_pid = vfork()) {
#else
				switch (ctrl_pid = fork()) {
#endif
				case -1:	/* error */
					syslog(LOG_ERR, "MGR: fork() failed launching " PPTP_CTRL_BIN);
					close(clientSocket);
					break;

				case 0:	/* child */
					close(hostSocket);
					if (pptp_debug)
						syslog(LOG_DEBUG, "MGR: Launching " PPTP_CTRL_BIN " to handle client");
					connectCall(clientSocket, !pptp_delegate ? firstOpen : 0);
					_exit(1);
					/* NORETURN */
				default:	/* parent */
					close(clientSocket);
					unique_call_id += MAX_CALLS_PER_TCP_LINK;
					if (!pptp_delegate)
						slot_set_pid(firstOpen, ctrl_pid);
					break;
				}
			}
		}		/* FD_ISSET(hostSocket, &connSet) */
	}			/* while (1) */
}				/* pptp_manager() */

/*
 * Author: Kevin Thayer
 * 
 * This creates a socket to listen on, sets the max # of pending connections and 
 * various other options.
 * 
 * Returns the fd of the host socket.
 * 
 * The function return values are:
 * 0 for sucessful
 * -1 for bad socket creation
 * -2 for bad socket options
 * -3 for bad bind
 * -4 for bad listen
 */
static int createHostSocket(int *hostSocket)
{
	int opt = 1;
	struct sockaddr_in address;
#ifdef HAVE_GETSERVBYNAME
	struct servent *serv;
#endif

	/* create the master socket and check it worked */
	if ((*hostSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
		return -1;

	/* set master socket to allow daemon to be restarted with connections active  */
	if (setsockopt(*hostSocket, SOL_SOCKET, SO_REUSEADDR,
		       (char *) &opt, sizeof(opt)) < 0)
		return -2;

	/* set up socket */
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	if(bindaddr)
		address.sin_addr.s_addr = inet_addr(bindaddr);
	else
		address.sin_addr.s_addr = INADDR_ANY;
#ifdef HAVE_GETSERVBYNAME
	if ((serv = getservbyname("pptp", "tcp")) != NULL) {
		address.sin_port = serv->s_port;
	} else
#endif
		address.sin_port = htons(PPTP_PORT);

	/* bind the socket to the pptp port */
	if (bind(*hostSocket, (struct sockaddr *) &address, sizeof(address)) < 0)
		return -3;

	/* minimal backlog to avoid DoS */
	if (listen(*hostSocket, 3) < 0)
		return -4;

	return 0;
}

/*
 * Author: Kevin Thayer
 * 
 * this routine sets up the arguments for the call handler and calls it.
 */

static void connectCall(int clientSocket, int clientNumber)
{

#define NUM2ARRAY(array, num) snprintf(array, sizeof(array), "%d", num)
	
	char *ctrl_argv[16];	/* arguments for launching 'pptpctrl' binary */

	int pptpctrl_argc = 0;	/* count the number of arguments sent to pptpctrl */

	/* lame strings to hold passed args. */
	char ctrl_debug[2];
	char ctrl_noipparam[2];
	char pppdoptfile_argv[2];
	char speedgiven_argv[2];
	extern char **environ;

	char ptimeout_argv[16];

	/*
	 * Launch the CTRL manager binary; we send it some information such as
	 * speed and option file on the command line.
	 */

	ctrl_argv[pptpctrl_argc++] = PPTP_CTRL_BIN "                                ";

	/* Pass socket as stdin */
	if (clientSocket != 0) {
		dup2(clientSocket, 0);
		close(clientSocket);
	}

	/* get argv set up */
	NUM2ARRAY(ctrl_debug, pptp_debug ? 1 : 0);
	ctrl_debug[1] = '\0';
	ctrl_argv[pptpctrl_argc++] = ctrl_debug;

	NUM2ARRAY(ctrl_noipparam, pptp_noipparam ? 1 : 0);
	ctrl_noipparam[1] = '\0';
	ctrl_argv[pptpctrl_argc++] = ctrl_noipparam;

	/* optionfile = TRUE or FALSE; so the CTRL manager knows whether to load a non-standard options file */
	NUM2ARRAY(pppdoptfile_argv, pppdoptstr ? 1 : 0);
	pppdoptfile_argv[1] = '\0';
	ctrl_argv[pptpctrl_argc++] = pppdoptfile_argv;
	if (pppdoptstr) {
		/* send the option filename so the CTRL manager can launch pppd with this alternate file */
		ctrl_argv[pptpctrl_argc++] = pppdoptstr;
	}
	/* tell the ctrl manager whether we were given a speed */
	NUM2ARRAY(speedgiven_argv, speedstr ? 1 : 0);
	speedgiven_argv[1] = '\0';
	ctrl_argv[pptpctrl_argc++] = speedgiven_argv;
	if (speedstr) {
		/* send the CTRL manager the speed of the connection so it can fire pppd at that speed */
		ctrl_argv[pptpctrl_argc++] = speedstr;
	}
	if (pptp_delegate) {
		/* no local or remote address to specify */
		ctrl_argv[pptpctrl_argc++] = "0";
		ctrl_argv[pptpctrl_argc++] = "0";
	} else {
		/* specify local & remote addresses for this call */
		ctrl_argv[pptpctrl_argc++] = "1";
		ctrl_argv[pptpctrl_argc++] = slot_get_local(clientNumber);
		ctrl_argv[pptpctrl_argc++] = "1";
		ctrl_argv[pptpctrl_argc++] = slot_get_remote(clientNumber);
	}

	/* our call id to be included in GRE packets the client
	 * will send to us */
	NUM2ARRAY(ptimeout_argv, pptp_ptimeout);
	ctrl_argv[pptpctrl_argc++] = ptimeout_argv;

	/* pass path to ppp binary */
	ctrl_argv[pptpctrl_argc++] = ppp_binary;

	/* pass logwtmp flag */
	ctrl_argv[pptpctrl_argc++] = pptp_logwtmp ? "1" : "0";

	/* note: update pptpctrl.8 if the argument list format is changed */

	/* terminate argv array with a NULL */
	ctrl_argv[pptpctrl_argc] = NULL;
	pptpctrl_argc++;

	/* ok, args are setup: invoke the call handler */
	execve(PPTP_CTRL_BIN, ctrl_argv, environ);
	syslog(LOG_ERR, "MGR: Failed to exec " PPTP_CTRL_BIN "!");
	_exit(1);
}
