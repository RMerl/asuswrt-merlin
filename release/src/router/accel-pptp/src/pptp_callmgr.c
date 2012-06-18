/* pptp_callmgr.c ... Call manager for PPTP connections.
 *                    Handles TCP port 1723 protocol.
 *                    C. Scott Ananian <cananian@alumni.princeton.edu>
 *
 * $Id: pptp_callmgr.c,v 1.20 2005/03/31 07:42:39 quozl Exp $
 */
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <errno.h>
#include "pptp_callmgr.h"
#include "pptp_ctrl.h"
#include "pptp_msg.h"
#include "dirutil.h"
#include "vector.h"
#include "util.h"

extern struct in_addr localbind; /* from pptp.c */
extern int call_ID;

int open_inetsock(struct in_addr inetaddr);
int open_unixsock(struct in_addr inetaddr);
void close_inetsock(int fd, struct in_addr inetaddr);
void close_unixsock(int fd, struct in_addr inetaddr);

sigjmp_buf callmgr_env;

void callmgr_sighandler(int sig) {
    /* TODO: according to signal(2), siglongjmp() is unsafe used here */
    siglongjmp (callmgr_env, 1);
}

void callmgr_do_nothing(int sig) {
    /* do nothing signal handler */
}

struct local_callinfo {
    int unix_sock;
    pid_t pid[2];
};

struct local_conninfo {
    VECTOR * call_list;
    fd_set * call_set;
};

/* Call callback */
void call_callback(PPTP_CONN *conn, PPTP_CALL *call, enum call_state state)
{
    struct local_callinfo *lci;
    struct local_conninfo *conninfo;
    u_int16_t call_id[2];
    switch(state) {
        case CALL_OPEN_DONE:
            /* okey dokey.  This means that the call_id and peer_call_id are
             * now valid, so lets send them on to our friends who requested
             * this call.  */
            lci = pptp_call_closure_get(conn, call); assert(lci != NULL);
            pptp_call_get_ids(conn, call, &call_id[0], &call_id[1]);
            write(lci->unix_sock, &call_id, sizeof(call_id));
            /* Our duty to the fatherland is now complete. */
            break;
        case CALL_OPEN_FAIL:
        case CALL_CLOSE_RQST:
        case CALL_CLOSE_DONE:
            /* don't need to do anything here, except make sure tables
             * are sync'ed */
            log("Closing connection (call state)");
            conninfo = pptp_conn_closure_get(conn);
            lci = pptp_call_closure_get(conn, call);
            assert(lci != NULL && conninfo != NULL);
            if (vector_contains(conninfo->call_list, lci->unix_sock)) {
                vector_remove(conninfo->call_list, lci->unix_sock);
                close(lci->unix_sock);
                FD_CLR(lci->unix_sock, conninfo->call_set);
                //if(lci->pid[0] > 1) kill(lci->pid[0], SIGTERM);
                //if(lci->pid[1] > 1) kill(lci->pid[1], SIGTERM);
            }
            break;
        default:
            log("Unhandled call callback state [%d].", (int) state);
            break;
    }
}

/******************************************************************************
 * NOTE ABOUT 'VOLATILE':
 * several variables here get a volatile qualifier to silence warnings
 * from older (before 3.0) gccs. if the longjmp stuff is removed,
 * the volatile qualifiers should be removed as well.
 *****************************************************************************/

/*** Call Manager *************************************************************/
int callmgr_main(int argc, char **argv, char **envp)
{
    struct in_addr inetaddr;
    int inet_sock, unix_sock;
    fd_set call_set;
    PPTP_CONN * conn;
    VECTOR * call_list;
    int max_fd = 0;
    volatile int first = 1;
    int retval;
    int i;
    char * volatile phonenr=NULL;
    int volatile window=10;
    //int volatile call_id=0;
    /* Step 0: Check arguments */
    if (argc < 2)
        fatal("Usage: %s ip.add.ress.here [--phone <phone number>]", argv[0]);
    //phonenr = argc == 3 ? argv[2] : NULL;
    for(i=2; i<argc; i++)
    {
    	//log("%s",argv[i]);
    	if (strcmp(argv[i],"--phone")==0 && i+1<argc) phonenr=argv[++i];
    	else if (strcmp(argv[i],"--window")==0 && i+1<argc) window=atoi(argv[++i]);
    	else if (strcmp(argv[i],"--call_id")==0 && i+1<argc) call_ID=atoi(argv[++i]);
    }
    if (inet_aton(argv[1], &inetaddr) == 0)
        fatal("Invalid IP address: %s", argv[1]);
     log("IP: %s\n",inet_ntoa(inetaddr));
    /* Step 1: Open sockets. */
    if ((inet_sock = open_inetsock(inetaddr)) < 0)
        fatal("Could not open control connection to %s", argv[1]);
    log("control connection");
    if ((unix_sock = open_unixsock(inetaddr)) < 0)
        fatal("Could not open unix socket for %s", argv[1]);
    /* Step 1b: FORK and return status to calling process. */
    log("unix_sock");

    switch (fork()) {
        case 0: /* child. stick around. */
            break;
        case -1: /* failure.  Fatal. */
            fatal("Could not fork.");
        default: /* Parent. Return status to caller. */
            exit(0);
    }
    /* re-open stderr as /dev/null to release it */
    file2fd("/dev/null", "wb", STDERR_FILENO);
    /* Step 1c: Clean up unix socket on TERM */
    if (sigsetjmp(callmgr_env, 1) != 0)
        goto cleanup;
    signal(SIGINT, callmgr_sighandler);
    signal(SIGTERM, callmgr_sighandler);
    signal(SIGPIPE, callmgr_do_nothing);
    signal(SIGUSR1, callmgr_do_nothing); /* signal state change
                                            wake up accept */
    /* Step 2: Open control connection and register callback */
    if ((conn = pptp_conn_open(inet_sock, 1, NULL/* callback */)) == NULL) {
        close(unix_sock); close(inet_sock); fatal("Could not open connection.");
    }
    FD_ZERO(&call_set);
    call_list = vector_create();
    {
        struct local_conninfo *conninfo = malloc(sizeof(*conninfo));
        if (conninfo == NULL) {
            close(unix_sock); close(inet_sock); fatal("No memory.");
        }
        conninfo->call_list = call_list;
        conninfo->call_set  = &call_set;
        pptp_conn_closure_put(conn, conninfo);
    }
    if (sigsetjmp(callmgr_env, 1) != 0) goto shutdown;
    /* Step 3: Get FD_SETs */
    max_fd = unix_sock;
    do {
        int rc;
        fd_set read_set = call_set, write_set;
        if (pptp_conn_dead(conn))
            break;
        FD_ZERO (&write_set);
        if (pptp_conn_established(conn)) {
	  FD_SET (unix_sock, &read_set);
	  if (unix_sock > max_fd) max_fd = unix_sock;
	}
        pptp_fd_set(conn, &read_set, &write_set, &max_fd);
        for (; max_fd > 0 ; max_fd--) {
            if (FD_ISSET (max_fd, &read_set) ||
                    FD_ISSET (max_fd, &write_set))
                break;
        }
        /* Step 4: Wait on INET or UNIX event */
        if ((rc = select(max_fd + 1, &read_set, &write_set, NULL, NULL)) <0) {
	  if (errno == EBADF) break;
	  /* a signal or somesuch. */
	  continue;
	}
        /* Step 5a: Handle INET events */
        rc = pptp_dispatch(conn, &read_set, &write_set);
	if (rc < 0)
	    break;
        /* Step 5b: Handle new connection to UNIX socket */
        if (FD_ISSET(unix_sock, &read_set)) {
            /* New call! */
            struct sockaddr_un from;
            int len = sizeof(from);
            PPTP_CALL * call;
            struct local_callinfo *lci;
            int s;
            /* Accept the socket */
            FD_CLR (unix_sock, &read_set);
            if ((s = accept(unix_sock, (struct sockaddr *) &from, &len)) < 0) {
                warn("Socket not accepted: %s", strerror(errno));
                goto skip_accept;
            }
            /* Allocate memory for local call information structure. */
            if ((lci = malloc(sizeof(*lci))) == NULL) {
                warn("Out of memory."); close(s); goto skip_accept;
            }
            lci->unix_sock = s;
            /* Give the initiator time to write the PIDs while we open
             * the call */
            call = pptp_call_open(conn, call_ID,call_callback, phonenr,window);
            /* Read and store the associated pids */
            read(s, &lci->pid[0], sizeof(lci->pid[0]));
            read(s, &lci->pid[1], sizeof(lci->pid[1]));
            /* associate the local information with the call */
            pptp_call_closure_put(conn, call, (void *) lci);
            /* The rest is done on callback. */
            /* Keep alive; wait for close */
            retval = vector_insert(call_list, s, call); assert(retval);
            if (s > max_fd) max_fd = s;
            FD_SET(s, &call_set);
            first = 0;
        }
skip_accept: /* Step 5c: Handle socket close */
        for (i = 0; i < max_fd + 1; i++)
            if (FD_ISSET(i, &read_set)) {
                /* close it */
                PPTP_CALL * call;
                retval = vector_search(call_list, i, &call);
                if (retval) {
                    struct local_callinfo *lci =
                        pptp_call_closure_get(conn, call);
                    log("Closing connection (unhandled)");
                    //if(lci->pid[0] > 1) kill(lci->pid[0], SIGTERM);
                    //if(lci->pid[1] > 1) kill(lci->pid[1], SIGTERM);
                    free(lci);
                    /* soft shutdown.  Callback will do hard shutdown later */
                    pptp_call_close(conn, call);
                    vector_remove(call_list, i);
                }
                FD_CLR(i, &call_set);
                close(i);
            }
    } while (vector_size(call_list) > 0 || first);
shutdown:
    {
        int rc;
        fd_set read_set, write_set;
        struct timeval tv;
	signal(SIGINT, callmgr_do_nothing);
	signal(SIGTERM, callmgr_do_nothing);
        /* warn("Shutdown"); */
        /* kill all open calls */
        for (i = 0; i < vector_size(call_list); i++) {
            PPTP_CALL *call = vector_get_Nth(call_list, i);
            //struct local_callinfo *lci = pptp_call_closure_get(conn, call);
            log("Closing connection (shutdown)");
            pptp_call_close(conn, call);
            //if(lci->pid[0] > 1) kill(lci->pid[0], SIGTERM);
            //if(lci->pid[1] > 1) kill(lci->pid[1], SIGTERM);
        }
        /* attempt to dispatch these messages */
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        pptp_fd_set(conn, &read_set, &write_set, &max_fd);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	select(max_fd + 1, &read_set, &write_set, NULL, &tv);
        rc = pptp_dispatch(conn, &read_set, &write_set);
	if (rc > 0) {
	  /* wait for a respond, a timeout because there might not be one */
	  FD_ZERO(&read_set);
	  FD_ZERO(&write_set);
	  pptp_fd_set(conn, &read_set, &write_set, &max_fd);
	  tv.tv_sec = 2;
	  tv.tv_usec = 0;
	  select(max_fd + 1, &read_set, &write_set, NULL, &tv);
	  rc = pptp_dispatch(conn, &read_set, &write_set);
	  if (rc > 0) {
	    if (i > 0) sleep(2);
	    /* no more open calls.  Close the connection. */
	    pptp_conn_close(conn, PPTP_STOP_LOCAL_SHUTDOWN);
	    /* wait for a respond, a timeout because there might not be one */
	    FD_ZERO(&read_set);
	    FD_ZERO(&write_set);
	    pptp_fd_set(conn, &read_set, &write_set, &max_fd);
	    tv.tv_sec = 2;
	    tv.tv_usec = 0;
	    select(max_fd + 1, &read_set, &write_set, NULL, &tv);
	    pptp_dispatch(conn, &read_set, &write_set);
	    if (rc > 0) sleep(2);
	  }
	}
        /* with extreme prejudice */
        pptp_conn_destroy(conn);
        pptp_conn_free(conn);
        vector_destroy(call_list);
    }
cleanup:
    signal(SIGINT, callmgr_do_nothing);
    signal(SIGTERM, callmgr_do_nothing);
    close_inetsock(inet_sock, inetaddr);
    close_unixsock(unix_sock, inetaddr);
    return 0;
}

/*** open_inetsock ************************************************************/
int open_inetsock(struct in_addr inetaddr)
{
    struct sockaddr_in dest, src;
    int s;
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(PPTP_PORT);
    dest.sin_addr   = inetaddr;
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        warn("socket: %s", strerror(errno));
        return s;
    }
    if (localbind.s_addr != INADDR_NONE) {
        bzero(&src, sizeof(src));
        src.sin_family = AF_INET;
        src.sin_addr   = localbind;
        if (bind(s, (struct sockaddr *) &src, sizeof(src)) != 0) {
            warn("bind: %s", strerror(errno));
            close(s); return -1;
        }
    }
    if (connect(s, (struct sockaddr *) &dest, sizeof(dest)) < 0) {
        warn("connect: %s", strerror(errno));
        close(s); return -1;
    }
    return s;
}

/*** open_unixsock ************************************************************/
int open_unixsock(struct in_addr inetaddr)
{
    struct sockaddr_un where;
    struct stat st;
    char *dir;
    int s;
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        warn("socket: %s", strerror(errno));
        return s;
    }
    callmgr_name_unixsock( &where, inetaddr, localbind);
    if (stat(where.sun_path, &st) >= 0)
    {
        warn("Call manager for %s is already running.", inet_ntoa(inetaddr));
        close(s); return -1;
    }
   /* Make sure path is valid. */
    dir = dirnamex(where.sun_path);
    if (!make_valid_path(dir, 0770))
        fatal("Could not make path to %s: %s", where.sun_path, strerror(errno));
    free(dir);
    if (bind(s, (struct sockaddr *) &where, sizeof(where)) < 0) {
        warn("bind: %s", strerror(errno));
        close(s); return -1;
    }
    chmod(where.sun_path, 0777);
    listen(s, 127);
    return s;
}

/*** close_inetsock ***********************************************************/
void close_inetsock(int fd, struct in_addr inetaddr)
{
    close(fd);
}

/*** close_unixsock ***********************************************************/
void close_unixsock(int fd, struct in_addr inetaddr)
{
    struct sockaddr_un where;
    close(fd);
    callmgr_name_unixsock(&where, inetaddr, localbind);
    unlink(where.sun_path);
}

/*** make a unix socket address ***********************************************/
void callmgr_name_unixsock(struct sockaddr_un *where,
			   struct in_addr inetaddr,
			   struct in_addr localbind)
{
    char localaddr[16], remoteaddr[16];
    where->sun_family = AF_UNIX;
    strncpy(localaddr,  inet_ntoa(localbind), 16);
    strncpy(remoteaddr, inet_ntoa(inetaddr),  16);
    snprintf(where->sun_path, sizeof(where->sun_path),
            PPTP_SOCKET_PREFIX "%s:%i", remoteaddr,call_ID);
}
