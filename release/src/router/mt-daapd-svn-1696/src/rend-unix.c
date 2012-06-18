/*
 * $Id: rend-unix.c 1311 2006-07-19 05:55:05Z rpedde $
 * General unix rendezvous routines
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <restart.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "daapd.h"
#include "err.h"
#include "rend-unix.h"

int rend_pipe_to[2];
int rend_pipe_from[2];
int rend_pid;

#define RD_SIDE 0
#define WR_SIDE 1

/*
 * rend_init
 *
 * Fork and set up message passing system 
 */
int rend_init(char *user) {
    int err;
    int fd;

    if(pipe((int*)&rend_pipe_to) == -1)
        return -1;

    if(pipe((int*)&rend_pipe_from) == -1) {
        err=errno;
        close(rend_pipe_to[RD_SIDE]);
        close(rend_pipe_to[WR_SIDE]);
        errno=err;
        return -1;
    }

    rend_pid=fork();
    if(rend_pid==-1) {
        err=errno;
        close(rend_pipe_to[RD_SIDE]);
        close(rend_pipe_to[WR_SIDE]);
        close(rend_pipe_from[RD_SIDE]);
        close(rend_pipe_from[WR_SIDE]);
        errno=err;
        return -1;
    }

    if(rend_pid) { /* parent */
        close(rend_pipe_to[RD_SIDE]);
        close(rend_pipe_from[WR_SIDE]);
        return 0;
    }

    /* child */
    close(rend_pipe_to[WR_SIDE]);
    close(rend_pipe_from[RD_SIDE]);
    
    /* Depending on the backend, this might not get done on the
     * rendezvous server-specific side
     */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

#ifdef SETPGRP_VOID
    setpgrp();
#else
    setpgrp(0,0);
#endif

#ifdef TIOCNOTTY
    if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
        ioctl(fd, TIOCNOTTY, (char *) NULL);
        close(fd);
    }
#endif

    if((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        if(!config.foreground)
            dup2(fd, STDERR_FILENO); 

        if (fd > 2)
            close(fd);
    }

    errno = 0;

    chdir("/");
    umask(0);

    /* something bad here... should really signal the parent, rather
     * than just zombieizing
     */
    rend_private_init(user); /* should only return when terminated */
    exit(0);
}

/*
 * rend_running
 *
 * See if the rendezvous daemon is runnig
 */
int rend_running(void) {
    REND_MESSAGE msg;
    int result;

    DPRINTF(E_DBG,L_REND,"Status inquiry\n");
    memset((void*)&msg,0x00,sizeof(msg));
    msg.cmd=REND_MSG_TYPE_STATUS;
    result=rend_send_message(&msg);
    DPRINTF(E_DBG,L_REND,"Returning status %d\n",result);
    return result;
}

/*
 *rend_stop
 *
 * Stop the rendezvous server
 */
int rend_stop(void) {
    REND_MESSAGE msg;

    memset((void*)&msg,0x0,sizeof(msg));
    msg.cmd=REND_MSG_TYPE_STOP;
    return rend_send_message(&msg);
}

/*
 * rend_register
 *
 * register a rendezvous name
 */
int rend_register(char *name, char *type, int port, char *iface, char *txt) {
    REND_MESSAGE msg;

    if((strlen(name)+1 > MAX_NAME_LEN) || (strlen(type)+1 > MAX_NAME_LEN) ||
        (strlen(txt)+1 > MAX_TEXT_LEN)) {
        DPRINTF(E_FATAL,L_REND,"Registration failed: name or type too long\n");
        return -1;
    }

    memset((void*)&msg,0x00,sizeof(msg)); /* shut valgrind up */
    msg.cmd=REND_MSG_TYPE_REGISTER;
    strncpy(msg.name,name,MAX_NAME_LEN-1);
    strncpy(msg.type,type,MAX_NAME_LEN-1);
    if(iface)
        strncpy(msg.iface,iface,MAX_IFACE_NAME_LEN-1);
    strncpy(msg.txt,txt,MAX_NAME_LEN-1);
    msg.port=port;

    return rend_send_message(&msg);
}

/*
 * rend_unregister
 *
 * Stop advertising a rendezvous name
 */
int rend_unregister(char *name, char *type, int port) {
    return -1; /* not implemented */
}

/*
 * rend_send_message
 *
 * Send a rendezvous message
 */
int rend_send_message(REND_MESSAGE *pmsg) {
    int retval;

    if(r_write(rend_pipe_to[WR_SIDE],pmsg,sizeof(REND_MESSAGE)) == -1)
        return -1;

    if((retval=r_read(rend_pipe_from[RD_SIDE],&retval,sizeof(int)) == -1))
        return -1;

    return retval;
}

/*
 * rend_read_message
 *
 * read the message passed to the rend daemon
 */
int rend_read_message(REND_MESSAGE *pmsg) {
    return r_read(rend_pipe_to[RD_SIDE],pmsg,sizeof(REND_MESSAGE));
}

/*
 * rend_send_response
 *
 * Let the rendezvous daemon return a result
 */
int rend_send_response(int value) {
    return r_write(rend_pipe_from[WR_SIDE],&value,sizeof(int));
}
