/*
 * Copyright (c) 1999 Adrian Sun (asun@zoology.washington.edu)
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 *
 * modified from main.c. this handles afp over tcp.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <sys/socket.h>
#include <sys/time.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <setjmp.h>
#include <time.h>

#include <atalk/logger.h>
#include <atalk/dsi.h>
#include <atalk/compat.h>
#include <atalk/util.h>
#include <atalk/uuid.h>
#include <atalk/paths.h>
#include <atalk/server_ipc.h>
#include <atalk/fce_api.h>
#include <atalk/globals.h>
#include <atalk/netatalk_conf.h>

#include "switch.h"
#include "auth.h"
#include "fork.h"
#include "dircache.h"

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

/* 
 * We generally pass this from afp_over_dsi to all afp_* funcs, so it should already be
 * available everywhere. Unfortunately some funcs (eg acltoownermode) need acces to it
 * but are deeply nested in the function chain with the caller already without acces to it.
 * Changing this would require adding a reference to the caller which itself might be
 * called in many places (eg acltoownermode is called from accessmode).
 * The only sane way out is providing a copy of it here:
 */
AFPObj *AFPobj = NULL;

typedef struct {
    uint16_t DSIreqID;
    uint8_t  AFPcommand;
    uint32_t result;
} rc_elem_t;

/*
 * AFP replay cache:
 * - fix sized array
 * - indexed just by taking DSIreqID mod REPLAYCACHE_SIZE
 */
static rc_elem_t replaycache[REPLAYCACHE_SIZE];

static sigjmp_buf recon_jmp;
static void afp_dsi_close(AFPObj *obj)
{
    DSI *dsi = obj->dsi;
    sigset_t sigs;
    
    close(obj->ipc_fd);
    obj->ipc_fd = -1;

    /* we may have been called from a signal handler caught when afpd was running
     * as uid 0, that's the wrong user for volume's prexec_close scripts if any,
     * restore our login user
     */
    if (geteuid() != obj->uid) {
        if (seteuid( obj->uid ) < 0) {
            LOG(log_error, logtype_afpd, "can't seteuid(%u) back %s: uid: %u, euid: %u", 
                obj->uid, strerror(errno), getuid(), geteuid());
            exit(EXITERR_SYS);
        }
    }

    close_all_vol(obj);
    if (obj->logout) {
        /* Block sigs, PAM/systemd/whoever might send us a SIG??? in (*obj->logout)() -> pam_close_session() */
        sigfillset(&sigs);
        pthread_sigmask(SIG_BLOCK, &sigs, NULL);
        (*obj->logout)();
    }

    LOG(log_note, logtype_afpd, "AFP statistics: %.2f KB read, %.2f KB written",
        dsi->read_count/1024.0, dsi->write_count/1024.0);
    log_dircache_stat();

    dsi_close(dsi);
}

/* -------------------------------
 * SIGTERM
 * a little bit of code duplication. 
 */
static void afp_dsi_die(int sig)
{
    DSI *dsi = (DSI *)AFPobj->dsi;

    if (dsi->flags & DSI_RECONINPROG) {
        /* Primary reconnect succeeded, got SIGTERM from afpd parent */
        dsi->flags &= ~DSI_RECONINPROG;
        return; /* this returns to afp_disconnect */
    }

    if (dsi->flags & DSI_DISCONNECTED) {
        LOG(log_note, logtype_afpd, "Disconnected session terminating");
        exit(0);
    }

    dsi_attention(AFPobj->dsi, AFPATTN_SHUTDOWN);
    afp_dsi_close(AFPobj);
   if (sig) /* if no signal, assume dieing because logins are disabled &
                don't log it (maintenance mode)*/
        LOG(log_info, logtype_afpd, "Connection terminated");
    if (sig == SIGTERM || sig == SIGALRM) {
        exit( 0 );
    }
    else {
        exit(sig);
    }
}

/* SIGURG handler (primary reconnect) */
static void afp_dsi_transfer_session(int sig _U_)
{
    uint16_t dsiID;
    int socket;
    DSI *dsi = (DSI *)AFPobj->dsi;

    LOG(log_debug, logtype_afpd, "afp_dsi_transfer_session: got SIGURG, trying to receive session");

    if (readt(AFPobj->ipc_fd, &dsiID, 2, 0, 2) != 2) {
        LOG(log_error, logtype_afpd, "afp_dsi_transfer_session: couldn't receive DSI id, goodbye");
        afp_dsi_close(AFPobj);
        exit(EXITERR_SYS);
    }

    if ((socket = recv_fd(AFPobj->ipc_fd, 1)) == -1) {
        LOG(log_error, logtype_afpd, "afp_dsi_transfer_session: couldn't receive session fd, goodbye");
        afp_dsi_close(AFPobj);
        exit(EXITERR_SYS);
    }

    LOG(log_debug, logtype_afpd, "afp_dsi_transfer_session: received socket fd: %i", socket);

    dsi->proto_close(dsi);
    dsi->socket = socket;
    dsi->flags = DSI_RECONSOCKET;
    dsi->datalen = 0;
    dsi->eof = dsi->start = dsi->buffer;
    dsi->in_write = 0;
    dsi->header.dsi_requestID = dsiID;
    dsi->header.dsi_command = DSIFUNC_CMD;

    /*
     * The session transfer happens in the middle of FPDisconnect old session, thus we
     * have to send the reply now.
     */
    if (!dsi_cmdreply(dsi, AFP_OK)) {
        LOG(log_error, logtype_afpd, "dsi_cmdreply: %s", strerror(errno) );
        afp_dsi_close(AFPobj);
        exit(EXITERR_CLNT);
    }

    LOG(log_note, logtype_afpd, "afp_dsi_transfer_session: succesfull primary reconnect");
    /* 
     * Now returning from this signal handler return to dsi_receive which should start
     * reading/continuing from the connected socket that was passed via the parent from
     * another session. The parent will terminate that session.
     */
    siglongjmp(recon_jmp, 1);
}

/* ------------------- */
static void afp_dsi_timedown(int sig _U_)
{
    struct sigaction	sv;
    struct itimerval	it;
    DSI                 *dsi = (DSI *)AFPobj->dsi;
    dsi->flags |= DSI_DIE;
    /* shutdown and don't reconnect. server going down in 5 minutes. */
    setmessage("The server is going down for maintenance.");
    if (dsi_attention(AFPobj->dsi, AFPATTN_SHUTDOWN | AFPATTN_NORECONNECT |
                  AFPATTN_MESG | AFPATTN_TIME(5)) < 0) {
        DSI *dsi = (DSI *)AFPobj->dsi;
        dsi->down_request = 1;
    }                  

    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = 300;
    it.it_value.tv_usec = 0;

    if ( setitimer( ITIMER_REAL, &it, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_timedown: setitimer: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }
    memset(&sv, 0, sizeof(sv));
    sv.sa_handler = afp_dsi_die;
    sigemptyset( &sv.sa_mask );
    sigaddset(&sv.sa_mask, SIGHUP);
    sigaddset(&sv.sa_mask, SIGTERM);
    sv.sa_flags = SA_RESTART;
    if ( sigaction( SIGALRM, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_timedown: sigaction: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }

    /* ignore myself */
    sv.sa_handler = SIG_IGN;
    sigemptyset( &sv.sa_mask );
    sv.sa_flags = SA_RESTART;
    if ( sigaction( SIGUSR1, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_timedown: sigaction SIGHUP: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }
}

/* ---------------------------------
 * SIGHUP reload configuration file
 */
volatile int reload_request = 0;

static void afp_dsi_reload(int sig _U_)
{
    reload_request = 1;
}

/* ---------------------------------
 * SIGINT: enable max_debug LOGging
 */
static volatile sig_atomic_t debug_request = 0;

static void afp_dsi_debug(int sig _U_)
{
    debug_request = 1;
}

/* ---------------------- */
static void afp_dsi_getmesg (int sig _U_)
{
    DSI *dsi = (DSI *)AFPobj->dsi;

    dsi->msg_request = 1;
    if (dsi_attention(AFPobj->dsi, AFPATTN_MESG | AFPATTN_TIME(5)) < 0)
        dsi->msg_request = 2;
}

static void alarm_handler(int sig _U_)
{
    int err;
    DSI *dsi = (DSI *)AFPobj->dsi;

    /* we have to restart the timer because some libraries may use alarm() */
    setitimer(ITIMER_REAL, &dsi->timer, NULL);

    /* we got some traffic from the client since the previous timer tick. */
    if ((dsi->flags & DSI_DATA)) {
        dsi->flags &= ~DSI_DATA;
        return;
    }

    dsi->tickle++;
    LOG(log_maxdebug, logtype_afpd, "alarm: tickles: %u, flags: %s|%s|%s|%s|%s|%s|%s|%s|%s",
        dsi->tickle,
        (dsi->flags & DSI_DATA) ?         "DSI_DATA" : "-",
        (dsi->flags & DSI_RUNNING) ?      "DSI_RUNNING" : "-",
        (dsi->flags & DSI_SLEEPING) ?     "DSI_SLEEPING" : "-",
        (dsi->flags & DSI_EXTSLEEP) ?     "DSI_EXTSLEEP" : "-",
        (dsi->flags & DSI_DISCONNECTED) ? "DSI_DISCONNECTED" : "-",
        (dsi->flags & DSI_DIE) ?          "DSI_DIE" : "-",
        (dsi->flags & DSI_NOREPLY) ?      "DSI_NOREPLY" : "-",
        (dsi->flags & DSI_RECONSOCKET) ?  "DSI_RECONSOCKET" : "-",
        (dsi->flags & DSI_RECONINPROG) ?  "DSI_RECONINPROG" : "-");

    if (dsi->flags & DSI_SLEEPING) {
        if (dsi->tickle > AFPobj->options.sleep) {
            LOG(log_note, logtype_afpd, "afp_alarm: sleep time ended");
            afp_dsi_die(EXITERR_CLNT);
        }
        return;
    } 

    if (dsi->flags & DSI_DISCONNECTED) {
        if (geteuid() == 0) {
            LOG(log_note, logtype_afpd, "afp_alarm: unauthenticated user, connection problem");
            afp_dsi_die(EXITERR_CLNT);
        }
        if (dsi->tickle > AFPobj->options.disconnected) {
            LOG(log_error, logtype_afpd, "afp_alarm: reconnect timer expired, goodbye");
            afp_dsi_die(EXITERR_CLNT);
        }
        return;
    }

    /* if we're in the midst of processing something, don't die. */        
    if (dsi->tickle >= AFPobj->options.timeout) {
        LOG(log_error, logtype_afpd, "afp_alarm: child timed out, entering disconnected state");
        if (dsi_disconnect(dsi) != 0)
            afp_dsi_die(EXITERR_CLNT);
        return;
    }

    if ((err = pollvoltime(AFPobj)) == 0)
        LOG(log_debug, logtype_afpd, "afp_alarm: sending DSI tickle");
        err = dsi_tickle(AFPobj->dsi);
    if (err <= 0) {
        if (geteuid() == 0) {
            LOG(log_note, logtype_afpd, "afp_alarm: unauthenticated user, connection problem");
            afp_dsi_die(EXITERR_CLNT);
        }
        LOG(log_error, logtype_afpd, "afp_alarm: connection problem, entering disconnected state");
        if (dsi_disconnect(dsi) != 0)
            afp_dsi_die(EXITERR_CLNT);
    }
}

/* ----------------- 
   if dsi->in_write is set attention, tickle (and close?) msg
   aren't sent. We don't care about tickle 
*/
static void pending_request(DSI *dsi)
{
    /* send pending attention */

    /* read msg if any, it could be done in afp_getsrvrmesg */
    if (dsi->msg_request) {
        if (dsi->msg_request == 2) {
            /* didn't send it in signal handler */
            dsi_attention(AFPobj->dsi, AFPATTN_MESG | AFPATTN_TIME(5));
        }
        dsi->msg_request = 0;
        readmessage(AFPobj);
    }
    if (dsi->down_request) {
        dsi->down_request = 0;
        dsi_attention(AFPobj->dsi, AFPATTN_SHUTDOWN | AFPATTN_NORECONNECT |
                  AFPATTN_MESG | AFPATTN_TIME(5));
    }
}

void afp_over_dsi_sighandlers(AFPObj *obj)
{
    DSI *dsi = (DSI *) obj->dsi;
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    sigfillset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    /* install SIGHUP */
    action.sa_handler = afp_dsi_reload;
    if ( sigaction( SIGHUP, &action, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_over_dsi: sigaction: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }

    /* install SIGURG */
    action.sa_handler = afp_dsi_transfer_session;
    if ( sigaction( SIGURG, &action, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_over_dsi: sigaction: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }

    /* install SIGTERM */
    action.sa_handler = afp_dsi_die;
    if ( sigaction( SIGTERM, &action, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_over_dsi: sigaction: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }

    /* install SIGQUIT */
    action.sa_handler = afp_dsi_die;
    if ( sigaction(SIGQUIT, &action, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_over_dsi: sigaction: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }

    /* SIGUSR2 - server message support */
    action.sa_handler = afp_dsi_getmesg;
    if ( sigaction( SIGUSR2, &action, NULL) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_over_dsi: sigaction: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }

    /*  SIGUSR1 - set down in 5 minutes  */
    action.sa_handler = afp_dsi_timedown;
    action.sa_flags = SA_RESTART;
    if ( sigaction( SIGUSR1, &action, NULL) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_over_dsi: sigaction: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }

    /*  SIGINT - enable max_debug LOGging to /tmp/afpd.PID.XXXXXX */
    action.sa_handler = afp_dsi_debug;
    if ( sigaction( SIGINT, &action, NULL) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_over_dsi: sigaction: %s", strerror(errno) );
        afp_dsi_die(EXITERR_SYS);
    }

#ifndef DEBUGGING
    /* SIGALRM - tickle handler */
    action.sa_handler = alarm_handler;
    if ((sigaction(SIGALRM, &action, NULL) < 0) ||
            (setitimer(ITIMER_REAL, &dsi->timer, NULL) < 0)) {
        afp_dsi_die(EXITERR_SYS);
    }
#endif /* DEBUGGING */
}

/* -------------------------------------------
 afp over dsi. this never returns. 
*/
void afp_over_dsi(AFPObj *obj)
{
    DSI *dsi = (DSI *) obj->dsi;
    int rc_idx;
    uint32_t err, cmd;
    uint8_t function;

    AFPobj = obj;
    obj->exit = afp_dsi_die;
    obj->reply = (int (*)()) dsi_cmdreply;
    obj->attention = (int (*)(void *, AFPUserBytes)) dsi_attention;
    dsi->tickle = 0;

    afp_over_dsi_sighandlers(obj);

    if (dircache_init(obj->options.dircachesize) != 0)
        afp_dsi_die(EXITERR_SYS);

    /* set TCP snd/rcv buf */
    if (obj->options.tcp_rcvbuf) {
        if (setsockopt(dsi->socket,
                       SOL_SOCKET,
                       SO_RCVBUF,
                       &obj->options.tcp_rcvbuf,
                       sizeof(obj->options.tcp_rcvbuf)) != 0) {
            LOG(log_error, logtype_dsi, "afp_over_dsi: setsockopt(SO_RCVBUF): %s", strerror(errno));
        }
    }
    if (obj->options.tcp_sndbuf) {
        if (setsockopt(dsi->socket,
                       SOL_SOCKET,
                       SO_SNDBUF,
                       &obj->options.tcp_sndbuf,
                       sizeof(obj->options.tcp_sndbuf)) != 0) {
            LOG(log_error, logtype_dsi, "afp_over_dsi: setsockopt(SO_SNDBUF): %s", strerror(errno));
        }
    }

    /* set TCP_NODELAY */
    int flag = 1;
    setsockopt(dsi->socket, SOL_TCP, TCP_NODELAY, &flag, sizeof(flag));

    ipc_child_state(obj, DSI_RUNNING);

    /* get stuck here until the end */
    while (1) {
        if (sigsetjmp(recon_jmp, 1) != 0)
            /* returning from SIGALARM handler for a primary reconnect */
            continue;

        /* Blocking read on the network socket */
        cmd = dsi_stream_receive(dsi);

        if (cmd == 0) {
            /* cmd == 0 is the error condition */
            if (dsi->flags & DSI_RECONSOCKET) {
                /* we just got a reconnect so we immediately try again to receive on the new fd */
                dsi->flags &= ~DSI_RECONSOCKET;
                continue;
            }

            /* the client sometimes logs out (afp_logout) but doesn't close the DSI session */
            if (dsi->flags & DSI_AFP_LOGGED_OUT) {
                LOG(log_note, logtype_afpd, "afp_over_dsi: client logged out, terminating DSI session");
                afp_dsi_close(obj);
                exit(0);
            }

            if (dsi->flags & DSI_RECONINPROG) {
                LOG(log_note, logtype_afpd, "afp_over_dsi: failed reconnect");
                afp_dsi_close(obj);
                exit(0);
            }

            /* Some error on the client connection, enter disconnected state */
            if (dsi_disconnect(dsi) != 0)
                afp_dsi_die(EXITERR_CLNT);

            ipc_child_state(obj, DSI_DISCONNECTED);

            while (dsi->flags & DSI_DISCONNECTED)
                pause(); /* gets interrupted by SIGALARM or SIGURG tickle */
            ipc_child_state(obj, DSI_RUNNING);
            continue; /* continue receiving until disconnect timer expires
                       * or a primary reconnect succeeds  */
        }

        if (!(dsi->flags & DSI_EXTSLEEP) && (dsi->flags & DSI_SLEEPING)) {
            LOG(log_debug, logtype_afpd, "afp_over_dsi: got data, ending normal sleep");
            dsi->flags &= ~DSI_SLEEPING;
            dsi->tickle = 0;
            ipc_child_state(obj, DSI_RUNNING);
        }

        if (reload_request) {
            reload_request = 0;
            load_volumes(AFPobj);
        }

        /* The first SIGINT enables debugging, the next restores the config */
        if (debug_request) {
            static int debugging = 0;
            debug_request = 0;

            dircache_dump();
            uuidcache_dump();

            if (debugging) {
                if (obj->options.logconfig)
                    setuplog(obj->options.logconfig, obj->options.logfile);
                else
                    setuplog("default:note", NULL);
                debugging = 0;
            } else {
                char logstr[50];
                debugging = 1;
                sprintf(logstr, "/tmp/afpd.%u.XXXXXX", getpid());
                setuplog("default:maxdebug", logstr);
            }
        }


        dsi->flags |= DSI_DATA;
        dsi->tickle = 0;

        switch(cmd) {

        case DSIFUNC_CLOSE:
            LOG(log_debug, logtype_afpd, "DSI: close session request");
            afp_dsi_close(obj);
            LOG(log_note, logtype_afpd, "done");
            exit(0);

        case DSIFUNC_TICKLE:
            dsi->flags &= ~DSI_DATA; /* thats no data in the sense we use it in alarm_handler */
            LOG(log_debug, logtype_afpd, "DSI: client tickle");
            /* timer is not every 30 seconds anymore, so we don't get killed on the client side. */
            if ((dsi->flags & DSI_DIE))
                dsi_tickle(dsi);
            break;

        case DSIFUNC_CMD:
#ifdef AFS
            if ( writtenfork ) {
                if ( flushfork( writtenfork ) < 0 ) {
                    LOG(log_error, logtype_afpd, "main flushfork: %s", strerror(errno) );
                }
                writtenfork = NULL;
            }
#endif /* AFS */

            function = (u_char) dsi->commands[0];

            /* AFP replay cache */
            rc_idx = dsi->clientID % REPLAYCACHE_SIZE;
            LOG(log_debug, logtype_dsi, "DSI request ID: %u", dsi->clientID);

            if (replaycache[rc_idx].DSIreqID == dsi->clientID
                && replaycache[rc_idx].AFPcommand == function) {
                LOG(log_note, logtype_afpd, "AFP Replay Cache match: id: %u / cmd: %s",
                    dsi->clientID, AfpNum2name(function));
                err = replaycache[rc_idx].result;
            /* AFP replay cache end */
            } else {
                /* send off an afp command. in a couple cases, we take advantage
                 * of the fact that we're a stream-based protocol. */
                if (afp_switch[function]) {
                    dsi->datalen = DSI_DATASIZ;
                    dsi->flags |= DSI_RUNNING;

                    LOG(log_debug, logtype_afpd, "<== Start AFP command: %s", AfpNum2name(function));

                    AFP_AFPFUNC_START(function, (char *)AfpNum2name(function));
                    err = (*afp_switch[function])(obj,
                                                  (char *)dsi->commands, dsi->cmdlen,
                                                  (char *)&dsi->data, &dsi->datalen);

                    AFP_AFPFUNC_DONE(function, (char *)AfpNum2name(function));
                    LOG(log_debug, logtype_afpd, "==> Finished AFP command: %s -> %s",
                        AfpNum2name(function), AfpErr2name(err));

                    dir_free_invalid_q();

                    dsi->flags &= ~DSI_RUNNING;

                    /* Add result to the AFP replay cache */
                    replaycache[rc_idx].DSIreqID = dsi->clientID;
                    replaycache[rc_idx].AFPcommand = function;
                    replaycache[rc_idx].result = err;
                } else {
                    LOG(log_error, logtype_afpd, "bad function %X", function);
                    dsi->datalen = 0;
                    err = AFPERR_NOOP;
                }
            }

            /* single shot toggle that gets set by dsi_readinit. */
            if (dsi->flags & DSI_NOREPLY) {
                dsi->flags &= ~DSI_NOREPLY;
                break;
            } else if (!dsi_cmdreply(dsi, err)) {
                LOG(log_error, logtype_afpd, "dsi_cmdreply(%d): %s", dsi->socket, strerror(errno) );
                if (dsi_disconnect(dsi) != 0)
                    afp_dsi_die(EXITERR_CLNT);
            }
            break;

        case DSIFUNC_WRITE: /* FPWrite and FPAddIcon */
            function = (u_char) dsi->commands[0];
            if ( afp_switch[ function ] != NULL ) {
                dsi->datalen = DSI_DATASIZ;
                dsi->flags |= DSI_RUNNING;

                LOG(log_debug, logtype_afpd, "<== Start AFP command: %s", AfpNum2name(function));

                AFP_AFPFUNC_START(function, (char *)AfpNum2name(function));

                err = (*afp_switch[function])(obj,
                                              (char *)dsi->commands, dsi->cmdlen,
                                              (char *)&dsi->data, &dsi->datalen);

                AFP_AFPFUNC_DONE(function, (char *)AfpNum2name(function));

                LOG(log_debug, logtype_afpd, "==> Finished AFP command: %s -> %s",
                    AfpNum2name(function), AfpErr2name(err));

                dsi->flags &= ~DSI_RUNNING;
            } else {
                LOG(log_error, logtype_afpd, "(write) bad function %x", function);
                dsi->datalen = 0;
                err = AFPERR_NOOP;
            }

            if (!dsi_wrtreply(dsi, err)) {
                LOG(log_error, logtype_afpd, "dsi_wrtreply: %s", strerror(errno) );
                if (dsi_disconnect(dsi) != 0)
                    afp_dsi_die(EXITERR_CLNT);
            }
            break;

        case DSIFUNC_ATTN: /* attention replies */
            break;

            /* error. this usually implies a mismatch of some kind
             * between server and client. if things are correct,
             * we need to flush the rest of the packet if necessary. */
        default:
            LOG(log_info, logtype_afpd,"afp_dsi: spurious command %d", cmd);
            dsi_writeinit(dsi, dsi->data, DSI_DATASIZ);
            dsi_writeflush(dsi);
            break;
        }
        pending_request(dsi);

        fce_pending_events(obj);
    }

    /* error */
    afp_dsi_die(EXITERR_CLNT);
}
