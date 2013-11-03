/*
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * Copyright (c) 2013 Frank Lahm <franklahm@gmail.com
 * All rights reserved. See COPYRIGHT.
 *
 * handle inserting, removing, and freeing of children.
 * this does it via a hash table. it incurs some overhead over
 * a linear append/remove in total removal and kills, but it makes
 * single-entry removals a fast operation. as total removals occur during
 * child initialization and kills during server shutdown, this is
 * probably a win for a lot of connections and unimportant for a small
 * number of connections.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pthread.h>

#include <atalk/logger.h>
#include <atalk/errchk.h>
#include <atalk/util.h>
#include <atalk/server_child.h>

#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif /* ! WEXITSTATUS */
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif /* ! WIFEXITED */
#ifndef WIFSTOPPED
#define WIFSTOPPED(status) (((status) & 0xff) == 0x7f)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(status) (!WIFSTOPPED(status) && !WIFEXITED(status))
#endif
#ifndef WTERMSIG
#define WTERMSIG(status)      ((status) & 0x7f)
#endif

/* hash/child functions: hash OR's pid */
#define HASH(i) ((((i) >> 8) ^ (i)) & (CHILD_HASHSIZE - 1))

static inline void hash_child(afp_child_t **htable, afp_child_t *child)
{
    afp_child_t **table;

    table = &htable[HASH(child->afpch_pid)];
    if ((child->afpch_next = *table) != NULL)
        (*table)->afpch_prevp = &child->afpch_next;
    *table = child;
    child->afpch_prevp = table;
}

static inline void unhash_child(afp_child_t *child)
{
    if (child->afpch_prevp) {
        if (child->afpch_next)
            child->afpch_next->afpch_prevp = child->afpch_prevp;
        *(child->afpch_prevp) = child->afpch_next;
    }
}

afp_child_t *server_child_resolve(server_child_t *childs, id_t pid)
{
    afp_child_t *child;

    for (child = childs->servch_table[HASH(pid)]; child; child = child->afpch_next) {
        if (child->afpch_pid == pid)
            break;
    }

    return child;
}

/* initialize server_child structure */
server_child_t *server_child_alloc(int connections)
{
    server_child_t *children;

    if (!(children = (server_child_t *)calloc(1, sizeof(server_child_t))))
        return NULL;

    children->servch_nsessions = connections;
    pthread_mutex_init(&children->servch_lock, NULL);
    return children;
}

/*!
 * add a child
 * @return pointer to struct server_child_data on success, NULL on error
 */
afp_child_t *server_child_add(server_child_t *children, pid_t pid, int ipc_fd)
{
    afp_child_t *child = NULL;

    pthread_mutex_lock(&children->servch_lock);

    /* it's possible that the child could have already died before the
     * pthread_sigmask. we need to check for this. */
    if (kill(pid, 0) < 0) {
        LOG(log_error, logtype_default, "server_child_add: no such process pid [%d]", pid);
        goto exit;
    }

    /* if we already have an entry. just return. */
    if ((child = server_child_resolve(children, pid)))
        goto exit;

    if ((child = calloc(1, sizeof(afp_child_t))) == NULL)
        goto exit;

    child->afpch_pid = pid;
    child->afpch_ipc_fd = ipc_fd;
    child->afpch_logintime = time(NULL);

    hash_child(children->servch_table, child);
    children->servch_count++;

exit:
    pthread_mutex_unlock(&children->servch_lock);
    return child;
}

/* remove a child and free it */
int server_child_remove(server_child_t *children, pid_t pid)
{
    int fd;
    afp_child_t *child;

    if (!(child = server_child_resolve(children, pid)))
        return -1;

    pthread_mutex_lock(&children->servch_lock);

    unhash_child(child);
    if (child->afpch_clientid) {
        free(child->afpch_clientid);
        child->afpch_clientid = NULL;
    }

    /* In main:child_handler() we need the fd in order to remove it from the pollfd set */
    fd = child->afpch_ipc_fd;
    if (fd != -1)
        close(fd);

    free(child);
    children->servch_count--;

    pthread_mutex_unlock(&children->servch_lock);

    return fd;
}

/* free everything: by using a hash table, this increases the cost of
 * this part over a linked list by the size of the hash table */
void server_child_free(server_child_t *children)
{
    afp_child_t *child, *tmp;
    int j;

    for (j = 0; j < CHILD_HASHSIZE; j++) {
        child = children->servch_table[j]; /* start at the beginning */
        while (child) {
            tmp = child->afpch_next;
            close(child->afpch_ipc_fd);
            if (child->afpch_clientid)
                free(child->afpch_clientid);
            if (child->afpch_volumes)
                free(child->afpch_volumes);
            free(child);
            child = tmp;
        }
    }

    free(children);
}

/* send signal to all child processes */
void server_child_kill(server_child_t *children, int sig)
{
    afp_child_t *child, *tmp;
    int i;

    for (i = 0; i < CHILD_HASHSIZE; i++) {
        child = children->servch_table[i];
        while (child) {
            tmp = child->afpch_next;
            kill(child->afpch_pid, sig);
            child = tmp;
        }
    }
}

/* send kill to a child processes */
static int kill_child(afp_child_t *child)
{
    if (!child->afpch_killed) {
        kill(child->afpch_pid, SIGTERM);
        /* we don't wait because there's no guarantee that we can really kill it */
        child->afpch_killed = 1;
        return 1;
    } else {
        LOG(log_info, logtype_default, "Unresponsive child[%d], sending SIGKILL", child->afpch_pid);
        kill(child->afpch_pid, SIGKILL);
    }
    return 1;
}

/*!
 * Try to find an old session and pass socket
 * @returns -1 on error, 0 if no matching session was found, 1 if session was found and socket passed
 */
int server_child_transfer_session(server_child_t *children,
                                  pid_t pid,
                                  uid_t uid,
                                  int afp_socket,
                                  uint16_t DSI_requestID)
{
    EC_INIT;
    afp_child_t *child;

    if ((child = server_child_resolve(children, pid)) == NULL) {
        LOG(log_note, logtype_default, "Reconnect: no child[%u]", pid);
        if (kill(pid, 0) == 0) {
            LOG(log_note, logtype_default, "Reconnect: terminating old session[%u]", pid);
            kill(pid, SIGTERM);
            sleep(2);
            if (kill(pid, 0) == 0) {
                LOG(log_error, logtype_default, "Reconnect: killing old session[%u]", pid);
                kill(pid, SIGKILL);
                sleep(2);
            }
        }
        return 0;
    }

    if (!child->afpch_valid) {
        /* hmm, client 'guess' the pid, rogue? */
        LOG(log_error, logtype_default, "Reconnect: invalidated child[%u]", pid);
        return 0;
    } else if (child->afpch_uid != uid) {
        LOG(log_error, logtype_default, "Reconnect: child[%u] not the same user", pid);
        return 0;
    }

    LOG(log_note, logtype_default, "Reconnect: transfering session to child[%u]", pid);
    
    if (writet(child->afpch_ipc_fd, &DSI_requestID, 2, 0, 2) != 2) {
        LOG(log_error, logtype_default, "Reconnect: error sending DSI id to child[%u]", pid);
        EC_STATUS(-1);
        goto EC_CLEANUP;
    }
    EC_ZERO_LOG(send_fd(child->afpch_ipc_fd, afp_socket));
    EC_ZERO_LOG(kill(pid, SIGURG));

    EC_STATUS(1);

EC_CLEANUP:
    EC_EXIT;
}


/* see if there is a process for the same mac     */
/* if the times don't match mac has been rebooted */
void server_child_kill_one_by_id(server_child_t *children, pid_t pid,
                                 uid_t uid, uint32_t idlen, char *id, uint32_t boottime)
{
    afp_child_t *child, *tmp;
    int i;

    pthread_mutex_lock(&children->servch_lock);
    
    for (i = 0; i < CHILD_HASHSIZE; i++) {
        child = children->servch_table[i];
        while (child) {
            tmp = child->afpch_next;
            if (child->afpch_pid != pid) {
                if (child->afpch_idlen == idlen && memcmp(child->afpch_clientid, id, idlen) == 0) {
                    if ( child->afpch_boottime != boottime ) {
                        /* Client rebooted */
                        if (uid == child->afpch_uid) {
                            kill_child(child);
                            LOG(log_warning, logtype_default,
                                "Terminated disconnected child[%u], client rebooted.",
                                child->afpch_pid);
                        } else {
                            LOG(log_warning, logtype_default,
                                "Session with different pid[%u]", child->afpch_pid);
                        }
                    } else {
                        /* One client with multiple sessions */
                        LOG(log_debug, logtype_default,
                            "Found another session[%u] for client[%u]", child->afpch_pid, pid);
                    }
                }
            } else {
                /* update childs own slot */
                child->afpch_boottime = boottime;
                if (child->afpch_clientid)
                    free(child->afpch_clientid);
                LOG(log_debug, logtype_default, "Setting client ID for %u", child->afpch_pid);
                child->afpch_uid = uid;
                child->afpch_valid = 1;
                child->afpch_idlen = idlen;
                child->afpch_clientid = id;
            }
            child = tmp;
        }
    }

    pthread_mutex_unlock(&children->servch_lock);
}

/* ---------------------------
 * reset children signals
 */
void server_reset_signal(void)
{
    struct sigaction    sv;
    sigset_t            sigs;
    const struct itimerval none = {{0, 0}, {0, 0}};

    setitimer(ITIMER_REAL, &none, NULL);
    memset(&sv, 0, sizeof(sv));
    sv.sa_handler =  SIG_DFL;
    sigemptyset( &sv.sa_mask );

    sigaction(SIGALRM, &sv, NULL );
    sigaction(SIGHUP,  &sv, NULL );
    sigaction(SIGTERM, &sv, NULL );
    sigaction(SIGUSR1, &sv, NULL );
    sigaction(SIGCHLD, &sv, NULL );

    sigemptyset(&sigs);
    sigaddset(&sigs, SIGALRM);
    sigaddset(&sigs, SIGHUP);
    sigaddset(&sigs, SIGUSR1);
    sigaddset(&sigs, SIGCHLD);
    pthread_sigmask(SIG_UNBLOCK, &sigs, NULL);

}
