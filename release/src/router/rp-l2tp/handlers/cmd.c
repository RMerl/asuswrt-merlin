/***********************************************************************
*
* l2tp/handlers/cmd.c
*
* Simple (VERY simple) command-processor for the L2TP daemon.
*
* Copyright (C) 2002 Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: cmd.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "l2tp.h"
#include "dstring.h"
#include "event_tcp.h"
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <signal.h>

#define HANDLER_NAME "cmd"

static int process_option(EventSelector *, char const *, char const *);
static void cmd_acceptor(EventSelector *es, int fd);
static void cmd_handler(EventSelector *es,
			int fd, char *buf, int len, int flag, void *data);


static void cmd_exit(EventSelector *es, int fd);
static void cmd_start_session(EventSelector *es, int fd, char *buf);
static void cmd_stop_session(EventSelector *es, int fd, char *buf);
static void cmd_dump_sessions(EventSelector *es, int fd, char *buf);
static void cmd_reply(EventSelector *es, int fd, char const *msg);

static option_handler my_option_handler = {
    NULL, HANDLER_NAME, process_option
};

/* Socket name for commands */
static char const *sockname = NULL;

static l2tp_opt_descriptor my_opts[] = {
    { "socket-path",     OPT_TYPE_STRING, &sockname },
    { NULL,              OPT_TYPE_BOOL,   NULL}
};


/**********************************************************************
* %FUNCTION: describe_session
* %ARGUMENTS:
*  ses -- an L2TP session
*  str -- a dynamic string to which description is appended
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Dumps session description into str
***********************************************************************/
static void
describe_session(l2tp_session *ses,
		 dynstring *str)
{
    char buf[1024];

    sprintf(buf, "Session %s MyID %d AssignedID %d",
	    (ses->we_are_lac ? "LAC" : "LNS"),
	    (int) ses->my_id, (int) ses->assigned_id);
    dynstring_append(str, buf);
    sprintf(buf, " State %s\n",
	    l2tp_session_state_name(ses));
    dynstring_append(str, buf);
}

/**********************************************************************
* %FUNCTION: describe_tunnel
* %ARGUMENTS:
*  tunnel -- an L2TP tunnel
*  str -- a dynamic string to which description is appended
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Dumps tunnel description into str
***********************************************************************/
static void
describe_tunnel(l2tp_tunnel *tunnel,
		dynstring *str)
{
    char buf[1024];
    l2tp_session *ses;
    void *cursor;

    sprintf(buf, "Tunnel MyID %d AssignedID %d",
	    (int) tunnel->my_id, (int) tunnel->assigned_id);
    dynstring_append(str, buf);
    sprintf(buf, " NumSessions %d", (int) hash_num_entries(&tunnel->sessions_by_my_id));
    dynstring_append(str, buf);
    sprintf(buf, " PeerIP %s State %s\n", inet_ntoa(tunnel->peer_addr.sin_addr),
	    l2tp_tunnel_state_name(tunnel));

    dynstring_append(str, buf);

    /* Describe each session */
    for (ses = l2tp_tunnel_first_session(tunnel, &cursor);
	 ses;
	 ses = l2tp_tunnel_next_session(tunnel, &cursor)) {
	describe_session(ses, str);
    }
}

/**********************************************************************
* %FUNCTION: handler_init
* %ARGUMENTS:
*  es -- event selector
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Initializes the command processor's option handler.  We do not
*  actually start a command processor until last option has been parsed.
***********************************************************************/
void
handler_init(EventSelector *es)
{
    l2tp_option_register_section(&my_option_handler);
}

/**********************************************************************
* %FUNCTION: process_option
* %ARGUMENTS:
*  es -- event selector
*  name, value -- name and value of option
* %RETURNS:
*  0 on success; -1 on failure.
* %DESCRIPTION:
*  Processes options.  When last option has been processed, begins
*  command handler.
***********************************************************************/
static int
process_option(EventSelector *es,
	       char const *name,
	       char const *value)
{
    struct sockaddr_un addr;
    socklen_t len;
    int fd;

    if (!strcmp(name, "*begin*")) return 0;
    if (strcmp(name, "*end*")) {
	return l2tp_option_set(es, name, value, my_opts);
    }

    /* We have hit the end of our options.  Open command socket */
    if (!sockname) {
	sockname = "/var/run/l2tpctrl";
    }

    (void) remove(sockname);
    fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0) {
	l2tp_set_errmsg("cmd: process_option: socket: %s", strerror(errno));
	return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, sockname, sizeof(addr.sun_path) - 1);

    len = sizeof(addr);
    if (bind(fd, (struct sockaddr *) &addr, SUN_LEN(&addr)) < 0) {
	l2tp_set_errmsg("cmd: process_option: bind: %s", strerror(errno));
	close(fd);
	return -1;
    }
    (void) chmod(sockname, 0600);
    if (listen(fd, 5) < 0) {
	l2tp_set_errmsg("cmd: process_option: listen: %s", strerror(errno));
	close(fd);
	return -1;
    }

    /* Ignore sigpipe */
    signal(SIGPIPE, SIG_IGN);

    /* Add an accept handler */
    if (!EventTcp_CreateAcceptor(es, fd, cmd_acceptor)) {
	l2tp_set_errmsg("cmd: process_option: EventTcp_CreateAcceptor: %s", strerror(errno));
	close(fd);
	return -1;
    }
    return 0;
}

/**********************************************************************
* %FUNCTION: cmd_acceptor
* %ARGUMENTS:
*  es -- event selector
*  fd -- file descriptor of accepted connection
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Accepts a control connection and sets up read event.
***********************************************************************/
static void
cmd_acceptor(EventSelector *es, int fd)
{
    EventTcp_ReadBuf(es, fd, 512, '\n', cmd_handler, 5, NULL);
}

/**********************************************************************
* %FUNCTION: cmd_handler
* %ARGUMENTS:
*  es -- event selector
*  fd -- file descriptor
*  buf -- buffer which was read
*  len -- length of data
*  flag -- flags
*  data -- not used
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Processes a command from the user
***********************************************************************/
static void
cmd_handler(EventSelector *es,
	    int fd,
	    char *buf,
	    int len,
	    int flag,
	    void *data)
{
    char word[512];

    if (flag == EVENT_TCP_FLAG_IOERROR || flag == EVENT_TCP_FLAG_TIMEOUT) {
	close(fd);
	return;
    }
    if (len < 511) {
	buf[len+1] = 0;
    } else {
	buf[len] = 0;
    }

    /* Chop off newline */
    if (len && (buf[len-1] == '\n')) {
	buf[len-1] = 0;
    }

    /* Get first word */
    buf = (char *) l2tp_chomp_word(buf, word);

    if (!strcmp(word, "exit")) {
	cmd_exit(es, fd);
    } else if (!strcmp(word, "start-session")) {
	cmd_start_session(es, fd, buf);
    } else if (!strcmp(word, "stop-session")) {
	cmd_stop_session(es, fd, buf);
    } else if (!strcmp(word, "dump-sessions")) {
	cmd_dump_sessions(es, fd, buf);
    } else {
	cmd_reply(es, fd, "ERR Unknown command");
    }
}

/**********************************************************************
* %FUNCTION: cmd_reply
* %ARGUMENTS:
*  es -- event selector
*  fd -- file descriptor
*  msg -- message
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Schedules reply to be shot back to user
***********************************************************************/
static void
cmd_reply(EventSelector *es,
	  int fd,
	  char const *msg)
{
    EventTcp_WriteBuf(es, fd, (char *) msg, strlen(msg), NULL, 10, NULL);
}

/**********************************************************************
* %FUNCTION: cmd_exit
* %ARGUMENTS:
*  es -- Event selector
*  fd -- command file descriptor
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Tears down tunnels and quits
***********************************************************************/
static void
cmd_exit(EventSelector *es,
	 int fd)
{
    cmd_reply(es, fd, "OK Shutting down");
    l2tp_tunnel_stop_all("Stopped by system administrator");
    l2tp_cleanup();
    exit(0);
}

/**********************************************************************
* %FUNCTION: cmd_start_session
* %ARGUMENTS:
*  es -- event selector
*  fd -- command file descriptor
*  buf -- rest of command from user
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Starts an L2TP session, if possible
***********************************************************************/
static void
cmd_start_session(EventSelector *es,
		  int fd,
		  char *buf)
{
    char peer[512];
    struct hostent *he;
    struct sockaddr_in haddr;
    l2tp_peer *p;
    l2tp_session *sess;

    buf = (char *) l2tp_chomp_word(buf, peer);
    he = gethostbyname(peer);
    if (!he) {
	cmd_reply(es, fd, "ERR Unknown peer - gethostbyname failed");
	return;
    }
    memcpy(&haddr.sin_addr, he->h_addr, sizeof(haddr.sin_addr));
    p = l2tp_peer_find(&haddr, NULL);
    if (!p) {
	cmd_reply(es, fd, "ERR Unknown peer");
	return;
    }
    sess = l2tp_session_call_lns(p, "foobar", es, NULL);
    if (!sess) {
	cmd_reply(es, fd, l2tp_get_errmsg());
	return;
    }

    /* Form reply */
    sprintf(peer, "OK %d %d",
	    (int) sess->tunnel->my_id,
	    (int) sess->my_id);
    cmd_reply(es, fd, peer);
}

/**********************************************************************
* %FUNCTION: cmd_stop_session
* %ARGUMENTS:
*  es -- event selector
*  fd -- command file descriptor
*  buf -- rest of command from user
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Stops an L2TP session, identified by (Tunnel, Session) pair.
***********************************************************************/
static void
cmd_stop_session(EventSelector *es,
		  int fd,
		  char *buf)
{
    char junk[512];
    l2tp_tunnel *tunnel;
    l2tp_session *sess;
    unsigned int x;

    buf = (char *) l2tp_chomp_word(buf, junk);
    if (sscanf(junk, "%u", &x) != 1) {
	cmd_reply(es, fd, "ERR Syntax error: stop-session tid sid");
	return;
    }
    tunnel = l2tp_tunnel_find_by_my_id((uint16_t) x);
    if (!tunnel) {
	cmd_reply(es, fd, "ERR No such tunnel");
	return;
    }


    buf = (char *) l2tp_chomp_word(buf, junk);
    if (sscanf(junk, "%u", &x) != 1) {
	cmd_reply(es, fd, "ERR Syntax error: stop-session tid sid");
	return;
    }
    sess = l2tp_tunnel_find_session(tunnel, (uint16_t) x);

    if (!sess) {
	cmd_reply(es, fd, "ERR No such session");
	return;
    }

    /* Stop the session */
    l2tp_session_send_CDN(sess, RESULT_GENERAL_REQUEST, ERROR_OK,
			  "Call terminated by operator");
    cmd_reply(es, fd, "OK Session stopped");
}

/**********************************************************************
* %FUNCTION: cmd_dump_sessions
* %ARGUMENTS:
*  es -- event selector
*  fd -- command file descriptor
*  buf -- rest of command from user
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Dumps info about all currently-active tunnels and sessions
***********************************************************************/
static void
cmd_dump_sessions(EventSelector *es,
		  int fd,
		  char *buf)
{
    dynstring str;
    char tmp[256];
    void *cursor;
    char const *ans;
    l2tp_tunnel *tunnel;

    dynstring_init(&str);

    dynstring_append(&str, "OK\n");

    /* Print info about each tunnel */
    sprintf(tmp, "NumL2TPTunnels %d\n", l2tp_num_tunnels());
    dynstring_append(&str, tmp);

    for (tunnel = l2tp_first_tunnel(&cursor);
	 tunnel;
	 tunnel = l2tp_next_tunnel(&cursor)) {
	describe_tunnel(tunnel, &str);
    }

    /* If something went wrong, say so... */
    ans = dynstring_data(&str);
    if (!ans) {
	cmd_reply(es, fd, "ERR Out of memory");
	return;
    }

    cmd_reply(es, fd, ans);
    dynstring_free(&str);
}

