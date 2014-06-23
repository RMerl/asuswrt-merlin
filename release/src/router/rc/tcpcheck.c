/*
 * tcpcheck.c - checks to see if a TCP connection can be established
 *              to the specified host/port.  Prints a success message
 *              or a failure message for each host.
 *
 * Think of it as 'fping' for TCP connections.  Maximum
 * parallelism.
 *
 * Designed to be as non-blocking as possible, but DNS FAILURES WILL
 * CAUSE IT TO PAUSE.  You have been warned.
 *
 * Timeout handling:
 * 
 * Uses the depricated alarm() syntax for simple compatability between
 * BSD systems.
 *
 * Copyright (C) 1998 David G. Andersen <angio@aros.net>
 *
 * This information is subject to change without notice and does not
 * represent a commitment on the part of David G. Andersen
 * This software is furnished under a license and a nondisclosure agreement.
 * This software may be used or copied only in accordance with the terms of
 * this agreement.  It is against Federal Law to copy this software on magnetic
 * tape, disk, or any other medium for any purpose other than the purchaser's
 * use.  David G. Andersen makes no warranty of any kind, expressed
 * or implied, with regard to these programs or documentation.  David G. 
 * Andersen shall not be liable in any event for incidental or consequential
 * damages in connection with, or arising out of, the furnishing, performance,
 * or use of these programs.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#define ERR_SUCCESS 0
#define ERR_REFUSED 1
#define ERR_TIMEOUT 2
#define ERR_OTHER   3

#define MAXTRIES 220;  /* Max number of connection attempts we can
			  try at once.  Must be lower than the
			  max number of sockets we can have open... */

#define DEFAULT_TIMEOUT 20
static int numcons;
static int consused; /* Number actually used */
static int timeout = DEFAULT_TIMEOUT; /* 20 second default timeout */
static struct connectinfo **cons;

struct connectinfo {
  char *hostname;
  char *portname;
  short port;
  int status;
  int socket;
  struct sockaddr_in *sockaddr;
  int socklen;
};

/*
 * setupsocket:  Configures a socket to make it go away quickly:
 *
 * SO_REUSEADDR - allow immediate reuse of the address
 * not SO_LINGER - Don't wait for data to be delivered.
 */
static int setupsocket(int sock) {
  int flags;
  struct linger nolinger;

  nolinger.l_onoff = 0;
  if (setsockopt(sock, SOL_SOCKET,
	     SO_LINGER, (char *) &nolinger, sizeof(nolinger)) == -1)
    return -1;

  flags = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&flags,
		 sizeof(flags)) == -1)
    return -1;

  /* Last thing we do:  Set it nonblocking */

  flags = O_NONBLOCK | fcntl(sock, F_GETFL);
  fcntl(sock, F_SETFL, flags);
  
  return 0;
}

/*
 * setuphost:  Configures a struct connectinfo for
 *             connecting to a host.  Opens a socket for it.
 *
 * returns 0 on success, -1 on failure
 */
static int setuphost(struct connectinfo *cinfo, char *hostport)
{
  struct hostent *tmp;
  char *port;
  struct servent *serv;

  if (!hostport) {
    fprintf(stderr, "null hostname passed to setuphost.  bailing\n");
    exit(-1);
  }

  port = strchr(hostport, ':');
  if (!port || *(++port) == 0) {
    return -1;
  }

  if (port[0] < '0' || port[0] > '9') {
    serv = getservbyname(port, "tcp");
    if (!serv) {
      return -1;
    }
    cinfo->port = ntohs(serv->s_port);
  } else {
    cinfo->port = atoi(port);
  }
  cinfo->portname = strdup(port);

  cinfo->hostname = strdup(hostport);
  port = strchr(cinfo->hostname, ':');
  *port = 0;
  
  tmp = gethostbyname(cinfo->hostname);

  /* If DNS fails, skip this puppy */
  if (tmp == NULL) { 
    cinfo->status = -1;
    return -1;
  }

  cinfo->status = 0;

  /* Groovy.  DNS stuff worked, now open a socket for it */
  cinfo->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (cinfo->socket == -1) {
    return -1;
  }

  /* Set the socket stuff */
  setupsocket(cinfo->socket);
  
  cinfo->socklen = sizeof(struct sockaddr_in);
  cinfo->sockaddr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
  bzero(cinfo->sockaddr, cinfo->socklen);

  bcopy(tmp->h_addr_list[0],
	&(cinfo->sockaddr->sin_addr),
	sizeof(cinfo->sockaddr->sin_addr));
#ifdef DA_HAS_STRUCT_SINLEN
  cinfo->sockaddr->sin_len = cinfo->socklen;
#endif
  cinfo->sockaddr->sin_family = AF_INET;
  cinfo->sockaddr->sin_port = htons(cinfo->port);
  
  return 0;
}

static void usage()
{
  fprintf(stderr, "usage:  tcpcheck <timeout> <host:port> [host:port]\n");
  exit(1);
}

static void wakeme(int sig)
{
  if (sig == SIGTERM)
		exit(0);

  printf("Got a timeout.  Will fail all outstanding connections.\n");
  exit(ERR_TIMEOUT);
}

static void goodconnect(int i) {
  printf("%s:%s is alive\n", cons[i]->hostname, cons[i]->portname);
  cons[i]->status = 1;
  close(cons[i]->socket);
}

static void failedconnect(int i) {
  printf("%s:%s failed\n", cons[i]->hostname, cons[i]->portname);
  cons[i]->status = -1;
}

/*
 * utility function to set res = a - b
 * for timeval structs
 * Assumes that a > b
 */
static void subtime(struct timeval *a, struct timeval *b, struct timeval *res)
{
  res->tv_sec = a->tv_sec - b->tv_sec;
  if (b->tv_usec > a->tv_usec) {
    a->tv_usec += 1000000;
    res->tv_sec -= 1;
  }
  res->tv_usec = a->tv_usec - b->tv_usec;
}

/*
 * set up our fd sets
 */

static void setupset(fd_set *theset, int *numfds)
{
  int i;
  int fds_used = 0;
  
  *numfds = 0;
  for (i = 0; i < numcons; i++) {
    if (cons[i] == NULL) continue;
    if (cons[i]->status == 0 && cons[i]->socket != -1) {
      FD_SET(cons[i]->socket, theset);
      fds_used++;
      if (cons[i]->socket > *numfds)
	*numfds = cons[i]->socket;
    }
  }
  if (!fds_used) {
    exit(0);			/* Success! */
  }
}

/*
 * We've initiated all connection attempts.
 * Here we sit and wait for them to complete.
 */
static void waitforconnects()
{
  fd_set writefds, exceptfds;
  struct timeval timeleft;
  struct timeval starttime;
  struct timeval curtime;
  struct timeval timeoutval;
  struct timeval timeused;
  struct sockaddr_in dummysock;
  int dummyint = sizeof(dummysock);
  int numfds;
  int res;
  int i;

  gettimeofday(&starttime, NULL);
  
  timeoutval.tv_sec = timeout;
  timeoutval.tv_usec = 0;

  timeleft = timeoutval;
 
  while (1)
  {
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    setupset(&writefds, &numfds);
    setupset(&exceptfds, &numfds);
    res = select(numfds+1, NULL, &writefds, &exceptfds, &timeleft);

    if (res == -1) {
      perror("select barfed, bailing");
      exit(-1);
    }

    if (res == 0)		/* We timed out */
      break;

    /* Oooh.  We have some successes */
    /* First test the exceptions */

    for (i = 0; i < numcons; i++)
    {
      if (cons[i] == NULL) continue;
      if (FD_ISSET(cons[i]->socket, &exceptfds)) {
				failedconnect(i);
      }
      else if (FD_ISSET(cons[i]->socket, &writefds)) {
				/* Boggle.  It's not always good.  select() is weird. */
				if (getpeername(cons[i]->socket, (struct sockaddr *)&dummysock, (socklen_t *)&dummyint))
					failedconnect(i);
				else
					goodconnect(i);
			}
		}

		/* now, timeleft = timeoutval - timeused */

		gettimeofday(&curtime, NULL);
		subtime(&curtime, &starttime, &timeused);
		subtime(&timeoutval, &timeused, &timeleft);
	}

  /* Now clean up the remainder... they timed out. */
  for (i = 0; i < numcons; i++) {
    if (cons[i]->status == 0) {
      printf("%s:%d failed:  timed out\n",
	     cons[i]->hostname, cons[i]->port);
    }
  }
}

int tcpcheck_main(int argc, char *argv[])
{
  struct connectinfo *pending;
  int i;
  int res;
  
  signal(SIGALRM, wakeme);
  signal(SIGTERM, wakeme);

  if (argc <= 2) usage();
  if (argv == NULL || argv[1] == NULL || argv[2] == NULL) usage();

  timeout = atoi(argv[1]);

  numcons = argc-2;
  cons = malloc(sizeof(struct connectinfo *) * (numcons+1));
  bzero((char *)cons, sizeof(struct connectinfo *) * (numcons+1));

  pending = NULL;
  consused = 0;
  
  /* Create a bunch of connection management structs */
  for (i = 2; i < argc; i++) {
    if (pending == NULL)
      pending = malloc(sizeof(struct connectinfo));
    if (setuphost(pending, (char *)argv[i])) {
      printf("%s failed.  could not resolve address\n", pending->hostname);
    } else {
      cons[consused++]  = pending;
      pending = NULL;
    }
  }

  for (i = 0; i < consused; i++) {
    if (cons[i] == NULL) continue;
    res = connect(cons[i]->socket, (struct sockaddr *)(cons[i]->sockaddr),
		  cons[i]->socklen);

    if (res && errno != EINPROGRESS) {
      failedconnect(i);
    }
  }

  /* Okay, we've initiated all of our connection attempts.
   * Now we just have to wait for them to timeout or complete
   */

  waitforconnects();
 
  exit(0);
}
