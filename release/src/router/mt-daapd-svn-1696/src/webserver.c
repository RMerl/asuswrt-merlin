/*
 * $Id: webserver.c 1680 2007-10-21 22:51:01Z rpedde $
 * Webserver library
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef WIN32
# include <netdb.h>
# include <sys/param.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#include "daapd.h"
#include "webserver.h"
#include "io.h"

/*
 * Defines
 */

#ifndef PACKAGE
# define PACKAGE "Firefly Web Server"
# define VERSION "1.0"
#endif

#define MAX_HOSTNAME 256
#define MAX_LINEBUFFER 2048
#define BLKSIZE PIPE_BUF

#ifdef DEBUG
#  ifndef ASSERT
#    define ASSERT(f)         \
        if(f)                 \
            {}                \
        else                  \
            ws_dprintf(0,"Assert error in %s, line %d\n",__FILE__,__LINE__)
#  endif /* ndef ASSERT */
#  define WS_ENTER()          ws_dprintf(10,"Entering %s",__func__)
#  define WS_EXIT()           ws_printf(10,"Exiting %s",__func__)
#else /* ndef DEBUG */
#  ifndef ASSERT
#    define ASSERT(f)
#  endif
#  define WS_ENTER()
#  define WS_EXIT()
#endif

/*
 * Local (private) typedefs
 */

typedef struct tag_ws_handler {
    char *stem;
    void (*req_handler)(WS_CONNINFO*);
    int(*auth_handler)(WS_CONNINFO*, char *, char *);
    int flags;
    int addheaders;
    struct tag_ws_handler *next;
} WS_HANDLER;

typedef struct tag_ws_connlist {
    WS_CONNINFO *pwsc;
    struct tag_ws_connlist *next;
} WS_CONNLIST;

typedef struct tag_ws_private {
    WSCONFIG wsconfig;
    WS_HANDLER handlers;
    WS_CONNLIST connlist;

    IOHANDLE hserver;
    int stop;
    int running;
    int threadno;
    int dispatch_threads;
    pthread_t server_tid;
    pthread_cond_t exit_cond;
    pthread_mutex_t exit_mutex;
} WS_PRIVATE;


/*
 * Forwards
 */
void *ws_mainthread(void*);
void *ws_dispatcher(void*);
int ws_lock_unsafe(void);
int ws_unlock_unsafe(void);
void ws_defaulthandler(WS_PRIVATE *pwsp, WS_CONNINFO *pwsc);
int ws_addarg(ARGLIST *root, char *key, char *fmt, ...);
void ws_freearglist(ARGLIST *root);
char *ws_urldecode(char *string, int space_as_plus);
int ws_getheaders(WS_CONNINFO *pwsc);
int ws_getpostvars(WS_CONNINFO *pwsc);
int ws_getgetvars(WS_CONNINFO *pwsc, char *string);
char *ws_getarg(ARGLIST *root, char *key);
int ws_testarg(ARGLIST *root, char *key, char *value);
int ws_findhandler(WS_PRIVATE *pwsp, WS_CONNINFO *pwsc,
                   void(**preq)(WS_CONNINFO*),
                   int(**pauth)(WS_CONNINFO*, char *, char *),
                   int *addheaders);
int ws_registerhandler(WSHANDLE ws, char *stem,
                       void(*handler)(WS_CONNINFO*),
                       int(*auth)(WS_CONNINFO*, char *, char *),
                       int flags,
                       int addheaders);
int ws_decodepassword(char *header, char **username, char **password);
int ws_testrequestheader(WS_CONNINFO *pwsc, char *header, char *value);
char *ws_getrequestheader(WS_CONNINFO *pwsc, char *header);
static void ws_add_dispatch_thread(WS_PRIVATE *pwsp, WS_CONNINFO *pwsc);
static void ws_remove_dispatch_thread(WS_PRIVATE *pwsp, WS_CONNINFO *pwsc);
static int ws_encoding_hack(WS_CONNINFO *pwsc);

static void ws_default_errhandler(int level, char *msg);
static void(*ws_err_handler)(int, char*) = ws_default_errhandler;

/*
 * Globals
 */
pthread_mutex_t ws_unsafe=PTHREAD_MUTEX_INITIALIZER;

char *ws_dow[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char *ws_moy[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                   "Aug", "Sep", "Oct", "Nov", "Dec" };

char *ws_errors[] = {
    "Success",                 /**< E_WS_SUCCESS     */
    "Native Error",            /**< E_WS_NATIVE      */
    "Out of memory",           /**< E_WS_MEMORY      */
    "pthreads error",          /**< E_WS_PTHREADS    */
    "ports exhausted",         /**< E_WS_EXHAUSTED   */
    "Can't listen on port",    /**< E_WS_LISTEN      */
    "Invalid content length",  /**< E_WS_CONTENTLEN  */
    "Read error",              /**< E_WS_READ        */
    "Couldn't parse GET vars", /**< E_WS_GETVARS     */
    "Timeout error",           /**< E_WS_TIMEOUT     */
    "Invalid reqeust type",    /**< E_WS_REQTYPE     */
    "Bad path - out of root"   /**< E_WS_BADPATH     */
};

/**
 * ws_dprintf
 *
 * Do debugging printfs.  This will use the registered errhandler
 */
void ws_dprintf(int level, char *fmt, ...) {  /* FIXME: dynamic buf len */
    char errbuf[4096];
    va_list ap;

    va_start(ap,fmt);
    vsnprintf(errbuf,sizeof(errbuf),fmt,ap);
    va_end(ap);

    ws_err_handler(level,errbuf);
}

/**
 * set the error handler to something other than default
 *
 * @param err_handler new error handler to use
 */
void ws_set_errhandler(void(*err_handler)(int, char*)) {
    ws_err_handler = err_handler;
}


/**
 * by default, just dump to stdout
 */
void ws_default_errhandler(int level, char *msg) {
    fprintf(stderr,"%d: %s", level, msg);
    if(!level) { /* fatal! */
        exit(0);
    }
}

/*
 * ws_lock_unsafe
 *
 * Lock non-thread-safe functions
 *
 * returns TRUE on success
 */
int ws_lock_unsafe(void) {
    int err;
    int retval=TRUE;

    WS_ENTER();

    if((err=pthread_mutex_lock(&ws_unsafe))) {
        ws_dprintf(L_WS_FATAL,"Cannot lock mutex: %s\n",strerror(err));
        retval=FALSE;
    }

    WS_EXIT();
    return retval;
}

/*
 * ws_unlock_unsafe
 *
 * Lock non-thread-safe functions
 *
 * returns TRUE on success
 */
int ws_unlock_unsafe(void) {
    int err;
    int retval=TRUE;

    WS_ENTER();

    if((err=pthread_mutex_unlock(&ws_unsafe))) {
        ws_dprintf(L_WS_FATAL,"Cannot unlock mutex: %s\n",strerror(err));
        retval=FALSE;
    }

    WS_EXIT();
    return retval;
}

/**
 * lock the connection list
 */
void ws_lock_connlist(WS_PRIVATE *pwsp) {
    if(pthread_mutex_lock(&pwsp->exit_mutex))
        ws_dprintf(L_WS_FATAL,"Cannot lock condition mutex\n");
}

/**
 * Unlock the connection list
 */
void ws_unlock_connlist(WS_PRIVATE *pwsp) {
    if(pthread_mutex_unlock(&pwsp->exit_mutex))
        ws_dprintf(L_WS_FATAL,"Cannot unlock condition mutex\n");
}

/**
 * Create a new webserver object, with a specific config.
 *
 * @param config config to use for newly created web server
 * @returns WSHANDLE on success, NULL on failure
 */
WSHANDLE ws_init(WSCONFIG *config) {
    int err;
    WS_PRIVATE *pwsp;

    WS_ENTER();
    if((pwsp=(WS_PRIVATE*)malloc(sizeof(WS_PRIVATE))) == NULL) {
        ws_dprintf(L_WS_SPAM,"Malloc error: %s\n",strerror(errno));
        return NULL;
    }

    memcpy(&pwsp->wsconfig,config,sizeof(WS_PRIVATE));
    pwsp->connlist.next=NULL;
    pwsp->running=0;
    pwsp->threadno=0;
    pwsp->stop=0;
    pwsp->dispatch_threads=0;
    pwsp->handlers.next=NULL;

    if((err=pthread_cond_init(&pwsp->exit_cond, NULL))) {
        ws_dprintf(L_WS_LOG,"Error in pthread_cond_init: %s\n",strerror(err));
        return NULL;
    }

    if((err=pthread_mutex_init(&pwsp->exit_mutex,NULL))) {
        ws_dprintf(L_WS_LOG,"Error in pthread_mutex_init: %s\n",strerror(err));
        return NULL;
    }

    WS_EXIT();
    return (WSHANDLE)pwsp;
}


/*
 * ws_start
 *
 * Start the main webserver thread.  Should really
 * bind and listen to the port before spawning the thread,
 * since it will be hard to detect and return the error unless
 * we listen first
 *
 * RETURNS
 *   Success: E_WS_SUCCESS
 *   Failure: Appropriate E_WS_ error code
 *
 */
int ws_start(WSHANDLE ws) {
    int err;
    int ephemeral = 0;
    WS_PRIVATE *pwsp = (WS_PRIVATE*)ws;

    WS_ENTER();

    /* this is kind of stupid, but is easier to fix once here
     * rather than mucking with differences in sockets on windows/unix
     */

    if(pwsp->wsconfig.port == 0) {
        pwsp->wsconfig.port = 1024; /* start with default port */
        ephemeral = 1;
    }

    while(1) {
        ws_dprintf(L_WS_INF,"Listening on port %d\n",pwsp->wsconfig.port);
        pwsp->hserver = io_new();
        if(!pwsp->hserver)
            ws_dprintf(L_WS_FATAL,"Cannot create new IO object");

        if(!io_open(pwsp->hserver,"listen://%d",pwsp->wsconfig.port)) {
            if((!ephemeral) || (io_errcode(pwsp->hserver) != IO_E_SOCKET_INUSE)) {
                ws_dprintf(L_WS_LOG,"Listen port: %s\n",io_errstr(pwsp->hserver));
                WS_EXIT();
                return E_WS_LISTEN;
            }
        } else {
            break;
        }

        pwsp->wsconfig.port++;
        if(!pwsp->wsconfig.port) {
            ws_dprintf(L_WS_LOG,"Exhausted ports\n");
            io_dispose(pwsp->hserver);
            pwsp->hserver = NULL;
            WS_EXIT();
            return E_WS_EXHAUSTED;
        }
    }

    ws_dprintf(L_WS_INF,"Starting server thread\n");
    if((err=pthread_create(&pwsp->server_tid,NULL,ws_mainthread,(void*)pwsp))) {
        ws_dprintf(L_WS_LOG,"Could not spawn thread: %s\n",strerror(err));
        io_close(pwsp->hserver);
        io_dispose(pwsp->hserver);
        WS_EXIT();
        return E_WS_PTHREADS;
    }

    /* we're really running */
    pwsp->running=1;

    WS_EXIT();
    return E_WS_SUCCESS;
}


/*
 * ws_remove_dispatch_thread
 *
 * remove a dispatch thread from the thread list
 */
void ws_remove_dispatch_thread(WS_PRIVATE *pwsp, WS_CONNINFO *pwsc) {
    WS_CONNLIST *pHead, *pTail;

    WS_ENTER();

    ws_lock_connlist(pwsp);

    pTail=&(pwsp->connlist);
    pHead=pwsp->connlist.next;

    while((pHead) && (pHead->pwsc != pwsc)) {
        pTail=pHead;
        pHead=pHead->next;
    }

    if(pHead) {
        pwsp->dispatch_threads--;
        ws_dprintf(L_WS_DBG,"With thread %d exiting, %d are still running\n",
                pwsc->threadno,pwsp->dispatch_threads);

        pTail->next = pHead->next;

        free(pHead);

        /* signal condition in case something is waiting */
        pthread_cond_signal(&pwsp->exit_cond);
    }

    ws_unlock_connlist(pwsp);
    WS_EXIT();
}


/*
 * ws_add_dispatch_thread
 *
 * Add a thread to the dispatch thread list
 */
void ws_add_dispatch_thread(WS_PRIVATE *pwsp, WS_CONNINFO *pwsc) {
    WS_CONNLIST *pNew;

    WS_ENTER();

    pNew=(WS_CONNLIST*)malloc(sizeof(WS_CONNLIST));
    pNew->next=NULL;
    pNew->pwsc=pwsc;

    if(!pNew) {
        ws_dprintf(L_WS_FATAL,"Malloc: %s\n",strerror(errno));
        exit(1); /* shouldn't strictly be necessary unless paren doesn't
                  * honor L_WS_FATAL */
    }

    ws_lock_connlist(pwsp);

    /* list is locked... */
    pwsp->dispatch_threads++;
    pNew->next = pwsp->connlist.next;
    pwsp->connlist.next = pNew;

    ws_unlock_connlist(pwsp);
    WS_EXIT();
}

/**
 * Stop the web server and all the child threads.  If there are any
 * active sessions, the web server ungraciously closes the underlying
 * socket, in an effort to make the child stop.
 *
 * @param ws handle of webserver to stop
 * @returns TRUE on success
 */
int ws_stop(WSHANDLE ws) {
    WS_PRIVATE *pwsp = (WS_PRIVATE*)ws;
    WS_HANDLER *current;
    WS_CONNLIST *pcl;
    void *result;

    WS_ENTER();

    /* free the ws_handlers */
    while(pwsp->handlers.next) {
        current=pwsp->handlers.next;
        pwsp->handlers.next=current->next;
        free(&current->stem);
        free(current);
    }

    pwsp->stop=1;
    pwsp->running=0;

    ws_dprintf(L_WS_DBG,"ws_stop: closing the server fd\n");
    io_close(pwsp->hserver);
    io_dispose(pwsp->hserver);

    /* wait for the server thread to terminate.  SHould be quick! */
    pthread_join(pwsp->server_tid,&result);

    /* Give the threads an extra push */
    ws_lock_connlist(pwsp);

    pcl=pwsp->connlist.next;

    /* Closing the client sockets out from under the dispatch threads
     * should cause the dispatch threads to exit out with an error.
     */
    while(pcl) {
        if(pcl->pwsc->hclient) {
            io_close(pcl->pwsc->hclient);
            io_dispose(pcl->pwsc->hclient);
            pcl->pwsc->hclient = NULL;
        }
        pcl=pcl->next;
    }

    /* wait for the threads to be done */
    while(pwsp->dispatch_threads) {
        ws_dprintf(L_WS_DBG,"ws_stop: I still see %d threads\n",pwsp->dispatch_threads);
        pthread_cond_wait(&pwsp->exit_cond, &pwsp->exit_mutex);
    }

    ws_unlock_connlist(pwsp);

    free(pwsp);

    WS_EXIT();
    return TRUE;
}

/**
 * Main thread for webserver - this accepts connections
 * and spawns a handler thread for each incoming connection.
 *
 * For a persistant connection, these threads will be
 * long-lived, otherwise, they will terminate as soon as
 * the request has been honored.
 *
 * These client threads will, of course, be detached
 *
 * @param arg this is actually a pointer to the web server private session
 */
void *ws_mainthread(void *arg) {
    int err;
    IOHANDLE hnew;
    WS_PRIVATE *pwsp = (WS_PRIVATE*)arg;
    WS_CONNINFO *pwsc;
    pthread_t tid;
    /* FIXME: endpoint from io_socket */
    char hostname[MAX_HOSTNAME+1];
    struct in_addr hostaddr;

    WS_ENTER();

    while(1) {
        pwsc=(WS_CONNINFO*)malloc(sizeof(WS_CONNINFO));
        if(!pwsc) {
            /* can't very well service any more threads! */
            ws_dprintf(L_WS_FATAL,"Error: %s\n",strerror(errno));
            pwsp->running=0;
            WS_EXIT();
            exit(1); /* should be unnecessary */
            return NULL;
        }

        memset(pwsc,0,sizeof(WS_CONNINFO));

        hnew = io_new();
        if(!hnew)
            ws_dprintf(L_WS_FATAL,"Malloc error in io_new()");

        if(!io_listen_accept(pwsp->hserver,hnew,&hostaddr)) {
            ws_dprintf(L_WS_LOG,"Dispatcher: accept failed: %s\n",
                io_errstr(pwsp->hserver));
            io_close(pwsp->hserver);
            io_dispose(pwsp->hserver);
            pwsp->running=0;
            free(pwsc);

            ws_dprintf(L_WS_FATAL,"Dispatcher: Aborting\n");
            WS_EXIT();
            return NULL;
        }

        /* FIXME: Get remote endpoint */
        strncpy(hostname,inet_ntoa(hostaddr),MAX_HOSTNAME);

        pwsc->hostname=strdup(hostname);
        pwsc->hclient = hnew;
        pwsc->pwsp = pwsp;

        /* Spawn off a dispatcher to decide what to do with
         * the request
         */

        /* don't really care if it locks or not */
        ws_lock_unsafe();
        pwsc->threadno=pwsp->threadno;
        pwsp->threadno++;
        /* we'll hold the unsafe until the dispatch thread is registered */

        /* now, throw off a dispatch thread */
        if((err=pthread_create(&tid,NULL,ws_dispatcher,(void*)pwsc))) {
            ws_set_err(pwsc,E_WS_PTHREADS);
            ws_dprintf(L_WS_FATAL,"Could not spawn thread: %s\n",strerror(err));
            ws_close(pwsc);
        } else {
            ws_add_dispatch_thread(pwsp,pwsc);
            pthread_detach(tid);
        }
        ws_unlock_unsafe();
    }

    WS_EXIT();
}


/**
 * Close the connection.  This might be called when things
 * are already in bad shape, so we'll ignore errors and let
 * them be detected back in either the dispatch
 * thread or the main server thread
 *
 * Mainly, we just want to make sure that any
 * allocated memory has been freed
 *
 * @param pwsc connection to close
 * @returns TRUE on success
 */
void ws_close(WS_CONNINFO *pwsc) {
    WS_PRIVATE *pwsp = (WS_PRIVATE *)(pwsc->pwsp);

    WS_ENTER();

    ws_dprintf(L_WS_DBG,"Thread %d: Terminating\n",pwsc->threadno);
    ws_dprintf(L_WS_DBG,"Thread %d: Freeing request headers\n",pwsc->threadno);
    ws_freearglist(&pwsc->request_headers);
    ws_dprintf(L_WS_DBG,"Thread %d: Freeing response headers\n",pwsc->threadno);
    ws_freearglist(&pwsc->response_headers);
    ws_dprintf(L_WS_DBG,"Thread %d: Freeing request vars\n",pwsc->threadno);
    ws_freearglist(&pwsc->request_vars);
    if(pwsc->uri) {
        free(pwsc->uri);
        pwsc->uri=NULL;
    }

    if((pwsc->close)||(pwsc->error)) {
        ws_dprintf(L_WS_DBG,"Thread %d: Closing fd\n",pwsc->threadno);
        io_close(pwsc->hclient);
        io_dispose(pwsc->hclient);
        /* this thread is done */

        ws_remove_dispatch_thread(pwsp, pwsc);

        /* Get rid of the local storage */
        if(pwsc->local_storage) {
            if(pwsc->storage_callback) {
                pwsc->storage_callback(pwsc->local_storage);
                pwsc->local_storage=NULL;
                pwsc->storage_callback=NULL;
            }
        }

        free(pwsc->hostname);
        memset(pwsc,0x00,sizeof(WS_CONNINFO));
        free(pwsc);
        WS_EXIT();
        pthread_exit(NULL);
    }
    WS_EXIT();
}

/*
 * ws_freearglist
 *
 * Walks through an arg list freeing as it goes
 */
void ws_freearglist(ARGLIST *root) {
    ARGLIST *current;

    WS_ENTER();

    while(root->next) {
        free(root->next->key);
        free(root->next->value);
        current=root->next;
        root->next=current->next;
        free(current);
    }

    WS_EXIT();
}


void ws_emitheaders(WS_CONNINFO *pwsc) {
    ARGLIST *pcurrent=pwsc->response_headers.next;

    WS_ENTER();
    while(pcurrent) {
        ws_dprintf(L_WS_DBG,"Emitting reponse header %s: %s\n",pcurrent->key,
                pcurrent->value);
        ws_writefd(pwsc,"%s: %s\r\n",pcurrent->key,pcurrent->value);
        pcurrent=pcurrent->next;
    }

    ws_writefd(pwsc,"\r\n");
    WS_EXIT();
}


/**
 * Receive and parse headers.  These will trump
 * get headers.  This should really only be called if
 * the method if post.  This will fill in the request_headers
 * linked list.
 *
 * @param pwsc session to parse post vars for
 * @returns TRUE on success
 */
int ws_getpostvars(WS_CONNINFO *pwsc) {
    char *content_length;
    unsigned char *buffer;
    uint32_t length;
    uint32_t ms;

    WS_ENTER();

    content_length = ws_getarg(&pwsc->request_headers,"Content-Length");
    if(!content_length) {
        ws_set_err(pwsc,E_WS_CONTENTLEN);
        WS_EXIT();
        return FALSE;
    }

    length=atoi(content_length);
    ws_dprintf(L_WS_DBG,"Thread %d: Post var length: %d\n",
            pwsc->threadno,length);

    buffer=(unsigned char*)malloc(length+1);

    if(!buffer) {
        ws_set_err(pwsc,E_WS_MEMORY);
        ws_dprintf(L_WS_INF,"Thread %d: Could not malloc %d bytes\n",
            pwsc->threadno, length);
        WS_EXIT();
        return FALSE;
    }

    // make the read time out 30 minutes like we said in the
    // /server-info response
    ms = 1800 * 1000;
    if(!io_read_timeout(pwsc->hclient, buffer, &length, &ms)) {
        if(0 == ms) {
            ws_dprintf(L_WS_INF,"Thread %d: Timeout reading post vars\n",
                pwsc->threadno);
            ws_set_err(pwsc,E_WS_TIMEOUT);
            WS_EXIT();
            free(buffer);
            return FALSE;
        }
        /* FIXME: Need to pass real errors back from the webserver */
        ws_dprintf(L_WS_LOG,"Thread %d: Read error: %s",
            pwsc->threadno,io_errstr(pwsc->hclient));
        ws_set_err(pwsc,E_WS_READ);
        free(buffer);
        return FALSE;
    }

    ws_dprintf(L_WS_DBG,"Thread %d: Read post vars: %s\n",pwsc->threadno,buffer);

    if(!ws_getgetvars(pwsc,(char*)buffer)) {
        /* assume error was set already */
        free(buffer);
        ws_dprintf(L_WS_LOG,"Could not parse get vars\n");
        return FALSE;
    }

    free(buffer);

    WS_EXIT();
    return TRUE;
}


/**
 * Receive and parse headers.  This is called from
 * ws_dispatcher
 *
 * @param pwsc session to get headers for
 * @returns TRUE on success
 */
int ws_getheaders(WS_CONNINFO *pwsc) {
    char *first, *last;
    int done;
    char buffer[MAX_LINEBUFFER];
    uint32_t len;

    WS_ENTER();

    /* Break down the headers into some kind of header list */
    done=0;
    while(!done) {
        len = sizeof(buffer);
        if(!io_readline(pwsc->hclient,(unsigned char *)buffer,&len)) {
            ws_set_err(pwsc,E_WS_READ);
            pwsc->error=errno;
            ws_dprintf(L_WS_INF,"Thread %d: read error: %s\n",pwsc->threadno,
                io_errstr(pwsc->hclient));
            WS_EXIT();
            return FALSE;
        }

        ws_dprintf(L_WS_DBG,"Thread %d: Read: %s",pwsc->threadno,buffer);

        first=buffer;
        if(buffer[0] == '\r')
            first=&buffer[1];

        /* trim the trailing \n */
        if(first[strlen(first)-1] == '\n')
            first[strlen(first)-1] = '\0';

        if(strlen(first) == 0) {
            ws_dprintf(L_WS_DBG,"Thread %d: Headers parsed!\n",pwsc->threadno);
            done=1;
        } else {
            /* we have a header! */
            last=first;
            strsep(&last,":");

            if(!last) {
                ws_dprintf(L_WS_WARN,"Thread %d: Invalid header: %s\n",
                        pwsc->threadno,first);
            } else {
                while(*last==' ')
                    last++;

                while(last[strlen(last)-1] == '\r')
                    last[strlen(last)-1] = '\0';

                ws_dprintf(L_WS_DBG,"Thread %d: Adding header *%s=%s*\n",
                        pwsc->threadno,first,last);

                if(!ws_addarg(&pwsc->request_headers,first,"%s",last)) {
                    ws_dprintf(L_WS_FATAL,"Thread %d: Out of memory\n",
                        pwsc->threadno);
                    ws_set_err(pwsc,E_WS_MEMORY);
                    WS_EXIT();
                    return FALSE;
                }
            }
        }
    }

    WS_EXIT();
    return TRUE;
}


/**
 * Some client choose to encode a space as +, others dont.  This
 * causes problems when the literal text to be passed is really a
 * '+' and not a ' '.  This attempts to fix broken browsers by
 * setting the appropriate flag for whether or not the client encodes
 * a space as plus.
 *
 * This is indeed a hack, as the url encoding spec makes it clear
 * that spaces should be encoded as a +.  Firefix and IE do this,
 * but the SoundBridge and iTunes don't.  (But Safari does, go figure)
 *
 * This could be used to set other browser-specific quirks, like the
 * Olive/HiFidelio issue with not honoring "Connection: close", as is
 * *REQUIRED* by RFC for HTTP 1.1 browsers, and ignored by HiFidelio.
 *
 * HiFidelio programmers, take note -- if you fixed your HTTP
 * implementation to be RFC compliant, I could provide transcoding
 * for your clients.  (And they want it, judging by my forums)
 *
 * @param pwsc session to check for encoding style
 * @returns TRUE if the remote user agent encodes space as a plus.
 */
int ws_encoding_hack(WS_CONNINFO *pwsc) {
    char *user_agent;
    int space_as_plus=1;

    WS_ENTER();
    user_agent=ws_getrequestheader(pwsc, "user-agent");
    if(user_agent) {
        if(strncasecmp(user_agent,"Roku",4) == 0)
            space_as_plus=0;
        if(strncasecmp(user_agent,"iTunes",6) == 0)
            space_as_plus=0;
    }
    WS_EXIT();
    return space_as_plus;
}


/**
 * parse a GET string of variables, filling in the request_vars
 * linked list.
 *
 * @params pwsc the session to parse vars for
 * @params string the argument part of the URI as requested by the client
 * @returns
 */
int ws_getgetvars(WS_CONNINFO *pwsc, char *string) {
    char *first, *last, *middle;
    char *key, *value;
    int done;

    int space_as_plus;

    WS_ENTER();

    space_as_plus=ws_encoding_hack(pwsc);

    done=0;

    first=string;

    while((!done) && (first)) {
        last=middle=first;
        strsep(&last,"&");
        strsep(&middle,"=");

        if(!middle) {
            ws_dprintf(L_WS_WARN,"Thread %d: Bad arg: %s\n",
                    pwsc->threadno,first);
        } else {
            key=ws_urldecode(first,space_as_plus);
            value=ws_urldecode(middle,space_as_plus);

            ws_dprintf(L_WS_DBG,"Thread %d: Adding arg %s = %s\n",
                    pwsc->threadno,key,value);
            ws_addarg(&pwsc->request_vars,key,"%s",value);

            free(key);
            free(value);
        }

        if(!last) {
            ws_dprintf(L_WS_DBG,"Thread %d: Done parsing GET/POST args!\n",
                    pwsc->threadno);
            done=1;
        } else {
            first=last;
        }
    }

    WS_EXIT();
    return TRUE;
}


/**
 * Main dispatch thread.  This gets the request, reads the
 * headers, decodes the GET'd or POST'd variables,
 * then decides what function should service the request
 *
 * @param arg a pointer to the WS_CONNINFO struct we are servicing
 */
void *ws_dispatcher(void *arg) {
    WS_CONNINFO *pwsc=(WS_CONNINFO*)arg;
    WS_PRIVATE *pwsp=pwsc->pwsp;
    char buffer[MAX_LINEBUFFER];
    char *first,*last;
    int connection_done=0;
    int can_dispatch;
    char *auth, *username, *password;
    int hdrs,handler;
    time_t now;
    struct tm now_tm;
    void (*req_handler)(WS_CONNINFO*);
    int(*auth_handler)(WS_CONNINFO*, char *, char *);
    uint32_t ms, len;

    WS_ENTER()

    /* quick fence to ensure that we have been registered on the thread list */
    ws_lock_unsafe();
    ws_unlock_unsafe();

    while(!connection_done) {
        // Now, get the request from the other end
        // and decide where to dispatch it
        ms = 1800 * 1000;
        len = sizeof(buffer);
        if(!io_readline_timeout(pwsc->hclient,(unsigned char *)buffer,&len,&ms) || (!len)) {
            ws_set_err(pwsc,E_WS_TIMEOUT);
            pwsc->error=errno;
            pwsc->close=1;
            ws_dprintf(L_WS_WARN,"Thread %d:  could not read: %s\n",
                pwsc->threadno,io_errstr(pwsc->hclient));
            ws_close(pwsc);
            WS_EXIT();
            return NULL;
        }

        ws_dprintf(L_WS_DBG,"Thread %d: \n",pwsc->threadno);
        ws_dprintf(L_WS_DBG - 1, "Request: %s", buffer);

        first=last=buffer;
        strsep(&last," ");
        if(!last) {
            pwsc->close=1;
            ws_returnerror(pwsc,400,"Bad request");
            ws_close(pwsc);
            ws_dprintf(L_WS_SPAM,"Error: bad request.  Exiting ws_dispatcher\n");
            WS_EXIT();
            return NULL;
        }

        if(!strcasecmp(first,"get")) {
            pwsc->request_type = RT_GET;
        } else if(!strcasecmp(first,"post")) {
            pwsc->request_type = RT_POST;
        } else {
            /* TODO: pass these to the underlying client, as they
             * might be interested in them (UPnP, maybe) */
            ws_set_err(pwsc,E_WS_REQTYPE);
            ws_returnerror(pwsc,501,"Not implemented");
            ws_close(pwsc);
            ws_dprintf(L_WS_SPAM,"Error: not get or post.  Exiting ws_dispatcher\n");
            WS_EXIT();
            return NULL;
        }

        first=last;
        strsep(&last," ");
        pwsc->uri=strdup(first);

        /* Get headers */
        if((!ws_getheaders(pwsc)) || (!last)) { /* didn't provide a HTTP/1.x */
            /* error already set */
            ws_dprintf(L_WS_LOG,"Thread %d: Couldn't parse headers - aborting\n",pwsc->threadno);
            ws_should_close(pwsc,TRUE);
            ws_close(pwsc);
            WS_EXIT();
            return NULL;
        }


        /* Now that we have the headers, we can
         * decide whether or not this is a persistant
         * connection */
        if(strncasecmp(last,"HTTP/1.0",8)==0) { /* defaults to non-persistant */
            pwsc->close=!ws_testarg(&pwsc->request_headers,"connection","keep-alive");
        } else { /* default to persistant for HTTP/1.1 and above */
            pwsc->close=ws_testarg(&pwsc->request_headers,"connection","close");
        }

        ws_dprintf(L_WS_DBG,"Thread %d: Connection type %s: Connection: %s\n",
                pwsc->threadno, last, pwsc->close ? "non-persist" : "persist");

        if(!pwsc->uri) {
            ws_set_err(pwsc,E_WS_MEMORY);
            ws_dprintf(L_WS_LOG,"Thread %d: Error allocation URI\n",
                    pwsc->threadno);
            ws_returnerror(pwsc,500,"Internal server error");
            ws_close(pwsc);
            WS_EXIT();
            return NULL;
        }

        /* trim the URI */
        first=pwsc->uri;
        strsep(&first,"?");

        if(first) { /* got some GET args */
            ws_dprintf(L_WS_DBG,"Thread %d: parsing GET args\n",pwsc->threadno);
            ws_getgetvars(pwsc,first);
        }

        /* fix the URI by un urldecoding it */

        ws_dprintf(L_WS_DBG,"Thread %d: Original URI: %s\n",
                pwsc->threadno,pwsc->uri);

        first=ws_urldecode(pwsc->uri,ws_encoding_hack(pwsc));
        free(pwsc->uri);
        pwsc->uri=first;

        /* Strip out the proxy stuff - iTunes 4.5 */
        first=strstr(pwsc->uri,"://");
        if(first) {
            first += 3;
            first=strchr(first,'/');
            if(first) {
                first=strdup(first);
                free(pwsc->uri);
                pwsc->uri=first;
            }
        }


        ws_dprintf(L_WS_DBG,"Thread %d: Translated URI: %s\n",pwsc->threadno,
                pwsc->uri);

        /* now, parse POST args */
        if(pwsc->request_type == RT_POST)
            ws_getpostvars(pwsc);

        hdrs=1;

        handler=ws_findhandler(pwsp,pwsc,&req_handler,&auth_handler,&hdrs);

        time(&now);
        ws_dprintf(L_WS_DBG,"Thread %d: Time is %d seconds after epoch\n",
                pwsc->threadno,now);
        gmtime_r(&now,&now_tm);
        ws_dprintf(L_WS_DBG,"Thread %d: Setting time header\n",pwsc->threadno);
        ws_addarg(&pwsc->response_headers,"Date",
                  "%s, %d %s %d %02d:%02d:%02d GMT",
                  ws_dow[now_tm.tm_wday],now_tm.tm_mday,
                  ws_moy[now_tm.tm_mon],now_tm.tm_year + 1900,
                  now_tm.tm_hour,now_tm.tm_min,now_tm.tm_sec);

        if(hdrs) {
            ws_addarg(&pwsc->response_headers,"Connection",
                      pwsc->close ? "close" : "keep-alive");

            ws_addarg(&pwsc->response_headers,"Server",
                      PACKAGE "/" VERSION);

            ws_addarg(&pwsc->response_headers,"Content-Type","text/html");
            ws_addarg(&pwsc->response_headers,"Content-Language","en_us");
        }

        /* Find the appropriate handler and dispatch it */
        if(handler == -1) {
            ws_dprintf(L_WS_DBG,"Thread %d: Using default handler.\n",
                    pwsc->threadno);
            ws_defaulthandler(pwsp,pwsc);
        } else {
            ws_dprintf(L_WS_DBG,"Thread %d: Using non-default handler\n",
                    pwsc->threadno);

            can_dispatch=0;
            /* If an auth handler is registered, but it accepts a
             * username and password of NULL, then don't bother
             * authing.
             */
            if((auth_handler) && (auth_handler(pwsc,NULL,NULL)==0)) {
                /* do the auth thing */
                auth=ws_getarg(&pwsc->request_headers,"Authorization");
                if((auth) && (ws_decodepassword(auth,&username, &password))) {
                    if(auth_handler(pwsc,username,password))
                        can_dispatch=1;
                    ws_addarg(&pwsc->request_vars,"HTTP_USER","%s",username);
                    ws_addarg(&pwsc->request_vars,"HTTP_PASSWD","%s",password);
                    free(username); /* this frees password too */
                }

                if(!can_dispatch) { /* auth failed, or need auth */
                    //ws_addarg(&pwsc->response_headers,"Connection","close");
                    ws_addarg(&pwsc->response_headers,"WWW-Authenticate",
                              "Basic realm=\"webserver\"");
                    ws_returnerror(pwsc,401,"Unauthorized");
                    ws_set_err(pwsc,E_WS_SUCCESS); /* don't close! */
                }
            } else {
                can_dispatch=1;
            }

            if(can_dispatch) {
                if(req_handler)
                    req_handler(pwsc);
                else
                    ws_defaulthandler(pwsp,pwsc);
            }
        }

        if((pwsc->close) || (pwsc->error) || (pwsp->stop)) {
            ws_should_close(pwsc,TRUE);
            connection_done=1;
        }
        ws_close(pwsc);
    }

    WS_EXIT();
    return NULL;
}


/**
 * Write a printf-style output to a connection.
 *
 * FIXME: This has a max buffer length of 1024, and shold
 * be fixed to use a variable-length buffer.  Note to self:
 * use io_printf to write a platform-indepentent aprintf, or
 * make an io_vprintf function...
 *
 * Note also that the return value is particularly useless.
 *
 * @param pwsc session to print output to
 * @param fmt format of output string
 * @returns number of bytes in the formatted string, not bytes written
 */
int ws_writefd(WS_CONNINFO *pwsc, char *fmt, ...) {
    char buffer[1024];
    va_list ap;
    uint32_t len;

    WS_ENTER();

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    len = (uint32_t)strlen(buffer);
    if(!io_write(pwsc->hclient,(unsigned char *)buffer,&len)) {
        ws_dprintf(L_WS_DBG,"Error writing to client socket: %s\n",
            io_errstr(pwsc->hclient));
    }

    WS_EXIT();
    return (int)len;
}

/**
* write a binary block of data to the connection
*
* @params pwsc connection to write data to
* @params data data to write
* @params len length of data to write
* @returns bytes actually written
*/
int ws_writebinary(WS_CONNINFO *pwsc, char *data, int len) {
    uint32_t bytes_written;

    WS_ENTER();
    bytes_written = (uint32_t) len;
    if(!io_write(pwsc->hclient, (unsigned char *)data, &bytes_written)) {
        ws_dprintf(L_WS_LOG,"Error writing to client socket: %s\n",
            io_errstr(pwsc->hclient));
    }

    WS_EXIT();
    return (int)bytes_written;
}

/**
 * return a particular error code to the requesting
 * agent
 *
 * This will always succeed.  If it cannot, it will
 * just close the connection with prejudice.
 *
 * @param pwsc connection to write error to
 * @param int error to return (500, 302, etc)
 * @param description (internal server error, content moved, etc)
 * @return TRUE on success
 */
int ws_returnerror(WS_CONNINFO *pwsc,int error, char *description) {
    char *useragent;
    int err_code;
    char *err_str;
    int keep_alive = 0;

    WS_ENTER();

    ws_dprintf(L_WS_WARN,"Thread %d: Entering ws_returnerror (%d: %s)\n",
            pwsc->threadno,error,description);
    ws_writefd(pwsc,"HTTP/1.1 %d %s\r\n",error,description);

    /* we'll force a close here unless the user agent is
       iTunes, which seems to get pissy about it */

    useragent = ws_getarg(&pwsc->request_headers,"User-Agent");
    if((useragent) &&
       (((strncmp(useragent,"iTunes",6) == 0) && (error == 401)) ||
        ((strncmp(useragent,"Java",4) == 0)))) {
        keep_alive = 1;
    }

    if(error == 302) {
        keep_alive = 1;
    }

    if(keep_alive) {
        ws_addarg(&pwsc->response_headers,"Connection","keep-alive");
        ws_addarg(&pwsc->response_headers,"Content-Length","2");
        ws_emitheaders(pwsc);
        ws_writefd(pwsc,"\r\n");
        WS_EXIT();
        return TRUE;
    }

    ws_should_close(pwsc,TRUE);
    ws_addarg(&pwsc->response_headers,"Connection","close");

    ws_emitheaders(pwsc);

    ws_writefd(pwsc,"<HTML>\r\n<TITLE>");
    ws_writefd(pwsc,"%d %s</TITLE>\r\n<BODY>",error,description);
    ws_writefd(pwsc,"\r\n<H1>%s</H1>\r\n",description);
    ws_writefd(pwsc,"Error %d\r\n<hr>\r\n",error);
    ws_writefd(pwsc,"<i>" PACKAGE ": %s\r\n<br>",VERSION);

    ws_get_err(pwsc,&err_code, &err_str);
    if(E_WS_SUCCESS != err_code)
        ws_writefd(pwsc,"Error: %s\r\n",strerror(errno));

    ws_writefd(pwsc,"</i></BODY>\r\n</HTML>\r\n");

    WS_EXIT();
    return TRUE;
}

/**
 * default URI handler.  This simply finds the file
 * and serves it up
 *
 * @param pwsp webserver private struct
 * @param pwsc connection to serve over
 */
void ws_defaulthandler(WS_PRIVATE *pwsp, WS_CONNINFO *pwsc) {
    char path[PATH_MAX];
    char resolved_path[PATH_MAX];
    IOHANDLE hfile;
    uint64_t len;

    WS_ENTER();

    snprintf(path,PATH_MAX,"%s/%s",pwsp->wsconfig.web_root,pwsc->uri);
    if(!realpath(path,resolved_path)) {
        ws_set_err(pwsc,E_WS_NATIVE);
        ws_dprintf(L_WS_WARN,"Exiting ws_defaulthandler: Cannot resolve %s\n",path);
        ws_returnerror(pwsc,404,"Not found");
        ws_close(pwsc);
        WS_EXIT();
        return;
    }

    ws_dprintf(L_WS_DBG,"Thread %d: Preparing to serve %s\n",
            pwsc->threadno, resolved_path);

    if(strncmp(resolved_path,pwsp->wsconfig.web_root,
               strlen(pwsp->wsconfig.web_root))) {
        ws_set_err(pwsc,E_WS_BADPATH);
        ws_dprintf(L_WS_WARN,"Exiting ws_defaulthandler: Thread %d: "
                "Requested file %s out of root\n",
                pwsc->threadno,resolved_path);
        ws_returnerror(pwsc,403,"Forbidden");
        ws_close(pwsc);
        WS_EXIT();
        return;
    }

    hfile = io_new();
    if(!hfile) {
        ws_set_err(pwsc,E_WS_NATIVE);
        ws_dprintf(L_WS_LOG,"Error creating file handle\n");
        ws_returnerror(pwsc,500,"Internal Server Error");
        ws_close(pwsc);
        WS_EXIT();
        return;
    }

    if(!io_open(hfile,"file://%U",resolved_path)) { /* default is O_RDONLY */
        ws_set_err(pwsc,E_WS_NATIVE); /* FIXME: ws_set_errstr */
        ws_dprintf(L_WS_LOG,"Error opening %s: %s",resolved_path,
            io_errstr(hfile));
        ws_dprintf(L_WS_WARN,"Exiting ws_defaulthandler: Thread %d: "
                "Error opening %s: %s\n",
                pwsc->threadno,resolved_path,strerror(errno));
        ws_returnerror(pwsc,404,"Not found");
        ws_close(pwsc);

        io_dispose(hfile);
        WS_EXIT();
        return;
    }

    /* set the Content-Length response header */
    io_size(hfile,&len);

    ws_dprintf(L_WS_DBG,"Length of file is %lld\n",len);
    ws_addarg(&pwsc->response_headers,"Content-Length","%lld",len);

    ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
    ws_emitheaders(pwsc);

    /* now throw out the file */
    ws_copyfile(pwsc,hfile,NULL);

    io_close(hfile);
    io_dispose(hfile);

    WS_EXIT();
    return;
}


/**
 * Check to see if a request header is a particular value.  This is
 * a thin wrapper of the interal funcion ws_testarg
 *
 * Example:
 *
 * if(ws_testrequestheader(pwsc,"Connection","keep-alive")) {}
 *
 * @param pwsc session to test headers of
 * @param header request header to test
 * @param value value to compare against
 * @returns TRUE if request header is set to checked value
 */
int ws_testrequestheader(WS_CONNINFO *pwsc, char *header, char *value) {
    return ws_testarg(&pwsc->request_headers,header,value);
}

/**
 * Check an arg for a particular value
 *
 * Example:
 *
 * pwsc->close=ws_queryarg(&pwsc->request_headers,"Connection","close");
 *
 */
int ws_testarg(ARGLIST *root, char *key, char *value) {
    char *retval;
    int result;

    WS_ENTER();
    ws_dprintf(L_WS_DBG,"Checking to see if %s matches %s\n",key,value);

    retval=ws_getarg(root,key);
    if(!retval) {
        ws_dprintf(L_WS_DBG,"Nope!\n");
        WS_EXIT();
        return FALSE;
    }

    result=!strcasecmp(value,retval);
    ws_dprintf(L_WS_DBG,"And it %s\n",result ? "DOES!" : "does NOT");
    WS_EXIT();
    return result;
}

/**
 * Find an argument in an argument list
 *
 * returns a pointer to the value if successful,
 * NULL otherwise.
 *
 * This should be passed the pointer to the
 * stub in the pwsc.
 *
 * Ex: ws_getarg(pwsc->request_headers,"Connection");
 *
 * @param root root of linked ARGLIST list
 * @param key key to find
 * @returns pointer to value
 */
char *ws_getarg(ARGLIST *root, char *key) {
    ARGLIST *pcurrent=root->next;

    WS_ENTER();
    while((pcurrent)&&(strcasecmp(pcurrent->key,key)))
        pcurrent=pcurrent->next;

    if(pcurrent) {
        WS_EXIT();
        return pcurrent->value;
    }

    WS_EXIT();
    return NULL;
}


/**
 * Add an argument to an arg list
 *
 * This will strdup the passed key and value.
 * The arglist will then have to be freed.  This should
 * be done in ws_close, and handler functions will
 * have to remember to ws_close them.
 *
 * FIXME: This uses a fixed-size buffer.  Again, need to make
 * an aprintf function like io_printf.
 *
 * @param root root of the ARGLIST list ot add key/value to
 * @param key key to add
 * @param fmt printf-style format to add
 * @returns TRUE on success
 */
int ws_addarg(ARGLIST *root, char *key, char *fmt, ...) {
    char *newkey;
    char *newvalue;
    ARGLIST *pnew;
    ARGLIST *current;
    va_list ap;
    char value[MAX_LINEBUFFER];

    WS_ENTER();

    va_start(ap,fmt);
    vsnprintf(value,sizeof(value),fmt,ap);
    va_end(ap);

    newkey=strdup(key);
    newvalue=strdup(value);
    pnew=(ARGLIST*)malloc(sizeof(ARGLIST));

    if((!pnew)||(!newkey)||(!newvalue)) {
        WS_EXIT();
        return FALSE;
    }

    pnew->key=newkey;
    pnew->value=newvalue;

    /* first, see if the key exists... if it does, simply
     * replace it rather than adding a duplicate key
     */

    current=root->next;
    while(current) {
        if(!strcasecmp(current->key,key)) {
            /* got a match! */
            ws_dprintf(L_WS_DBG,"Updating %s from %s to %s\n",
                    key,current->value,value);
            free(current->value);
            current->value = newvalue;
            free(newkey);
            free(pnew);
            WS_EXIT();
            return 0;
        }
        current=current->next;
    }


    pnew->next=root->next;
    ws_dprintf(L_WS_DBG,"Added *%s=%s*\n",newkey,newvalue);
    root->next=pnew;

    WS_EXIT();
    return TRUE;
}

/**
 * decode a urlencoded string
 *
 * the returned char will be malloced -- it must be
 * freed by the caller
 *
 * @param string string to decode
 * @param space_as_plus whether the client encodes ' ' as '+'
 * @returns NULL on error, else the decoded URI
 */
char *ws_urldecode(char *string, int space_as_plus) {
    char *pnew;
    char *src,*dst;
    int val=0;

    WS_ENTER();

    pnew=(char*)malloc(strlen(string)+1);
    if(!pnew) {
        WS_EXIT();
        return NULL;
    }

    src=string;
    dst=pnew;

    while(*src) {
        switch(*src) {
        case '+':
            if(space_as_plus) {
                *dst++=' ';
            } else {
                *dst++=*src;
            }
            src++;
            break;
        case '%':
            /* this is hideous */
            src++;
            if(*src) {
                if((*src <= '9') && (*src >='0'))
                    val=(*src - '0');
                else if((tolower(*src) <= 'f')&&(tolower(*src) >= 'a'))
                    val=10+(tolower(*src) - 'a');
                src++;
            }
            if(*src) {
                val *= 16;
                if((*src <= '9') && (*src >='0'))
                    val+=(*src - '0');
                else if((tolower(*src) <= 'f')&&(tolower(*src) >= 'a'))
                    val+=(10+(tolower(*src) - 'a'));
                src++;
            }
            *dst++=val;
            break;
        default:
            *dst++=*src++;
            break;
        }
    }

    *dst='\0';
    WS_EXIT();
    return pnew;
}


/**
 * Register a page and auth handler.
 *
 * @param ws pointer to private webserver struct
 * @param stem the prefix of the handled web namespace ("/upnp", etc)
 * @param handler page handler
 * @param auth auth handler
 * @param flags currently unused
 * @param addheaders whether default headers should be added (date, etc)
 * @returns TRUE on success, FALSE otherwise
 */
int ws_registerhandler(WSHANDLE ws, char *stem,
                       void(*handler)(WS_CONNINFO*),
                       int(*auth)(WS_CONNINFO *, char *, char *),
                       int flags,
                       int addheaders) {
    WS_HANDLER *phandler;
    WS_PRIVATE *pwsp = (WS_PRIVATE *)ws;

    WS_ENTER();
    phandler=(WS_HANDLER *)malloc(sizeof(WS_HANDLER));
    if(!phandler)
        ws_dprintf(L_WS_FATAL,"Malloc error in ws_registerhandler\n");

    phandler->stem=strdup(stem);
    phandler->req_handler=handler;
    phandler->auth_handler=auth;
    phandler->addheaders=addheaders;

    ws_lock_unsafe();
    phandler->next=pwsp->handlers.next;
    pwsp->handlers.next=phandler;
    ws_unlock_unsafe();

    WS_EXIT();
    return TRUE;
}

/**
 * Given a URI, determine the appropriate handler.
 *
 * If a handler is found, it returns TRUE, else FALSE
 *
 * @param pwsp pointer to internal webserver struct
 * @param pwsc session (from which URI is gained)
 * @param preq filled iwth request handler
 * @param pauth filled with auth handler
 * @param addheaders whether or not to add headers
 * @returns TRUE on success, FALSE otherwise
 */
int ws_findhandler(WS_PRIVATE *pwsp, WS_CONNINFO *pwsc,
                   void(**preq)(WS_CONNINFO*),
                   int(**pauth)(WS_CONNINFO *, char *, char *),
                   int *addheaders) {
    WS_HANDLER *phandler;

    WS_ENTER();
    ws_lock_unsafe();

    phandler = pwsp->handlers.next;
    *preq=NULL;

    ws_dprintf(L_WS_DBG,"Thread %d: Preparing to find handler\n",
            pwsc->threadno);

    while(phandler) {
        ws_dprintf(L_WS_DBG,"Checking %s against handler for %s\n",
                pwsc->uri, phandler->stem);
        if(!strncasecmp(phandler->stem,pwsc->uri,strlen(phandler->stem))) {
            /* that's a match */
            ws_dprintf(L_WS_DBG,"Thread %d: URI Match!\n",pwsc->threadno);
            *preq=phandler->req_handler;
            *pauth=phandler->auth_handler;
            *addheaders=phandler->addheaders;
            ws_unlock_unsafe();
            WS_EXIT();
            return 0;
        }
        phandler=phandler->next;
    }

    ws_unlock_unsafe();
    ws_dprintf(L_WS_DBG,"Didn't find one!\n");
    WS_EXIT();
    return -1;
}

/**
 * Given a base64 encoded Authentication request header,
 * decode it into the username and password.  The memory for
 * the decoding is allocated, but the username and password
 * share the same allocated chunk of memory.  The caller is
 * responsible for freeing username when the username and
 * password are no longer needed.
 *
 * @param header the base64 encoded source
 * @param username username to be filled in
 * @param password to be filled in
 * @returns TRUE on success
 */
int ws_decodepassword(char *header, char **username, char **password) {
    static char ws_xlat[256];
    static int ws_xlat_init=0;
    int index;
    int len;
    int rack=0;
    int pads=0;
    unsigned char *decodebuffer;
    unsigned char *pin, *pout;
    char *type,*base64;
    int lookup;

    *username=NULL;
    *password=NULL;

    WS_ENTER();

    ws_lock_unsafe();

    if(!ws_xlat_init) {
        ws_xlat_init=1;

        memset((char*)&ws_xlat,0xFF,sizeof(ws_xlat));
        for(index=0; index < 26; index++) {
            ws_xlat['A' + index] = index;
            ws_xlat['a' + index] = index + 26;
        }

        for(index=0; index < 10; index++) {
            ws_xlat['0' + index] = index + 52;
        }

        ws_xlat['+'] = 62;
        ws_xlat['/'] = 63;
    }

    ws_unlock_unsafe();

    /* xlat table is initialized */

    // Trim leading spaces
    while((*header) && (*header == ' '))
        header++;

    // Should be in the form "Basic <base-64 enc username/pw>"
    type=header;
    base64 = strchr(header,' ');
    if(!base64) {
        // invalid auth header 
        ws_dprintf(L_WS_DBG,"Bad authentication header: %s\n",header);
        WS_EXIT();
        return FALSE;
    }
    
    *base64 = '\0';
    base64++;

    decodebuffer=(unsigned char *)malloc(strlen(base64));
    if(!decodebuffer) {
        WS_EXIT();
        return FALSE;
    }

    ws_dprintf(L_WS_DBG,"Preparing to decode %s\n",base64);

    memset(decodebuffer,0,strlen(base64));
    len=0;
    pout=decodebuffer;
    pin=(unsigned char *)base64;

    /* this is more than a little sloppy */
    while(pin[rack]) {
        if(pin[rack] != '=') {
            lookup=ws_xlat[pin[rack]];
            if(lookup == 0xFF) {
                ws_dprintf(L_WS_WARN,"Got garbage Authenticate header\n");
                free(decodebuffer);
                *username = NULL;
                *password = NULL;
                return FALSE;
            }

            /* valid character */
            switch(rack) {
            case 0:
                pout[0]=(lookup << 2);
                break;
            case 1:
                pout[0] |= (lookup >> 4);
                pout[1] = (lookup << 4);
                break;
            case 2:
                pout[1] |= (lookup >> 2);
                pout[2] = (lookup << 6);
                break;
            case 3:
                pout[2] |= lookup;
                break;
            }
            rack++;
        } else {
            /* padding char */
            pads++;
            rack++;
        }

        if(rack == 4) {
            pin += 4;
            pout += 3;

            len += (3-pads);
            rack=0;
        }
    }

    /* we now have the decoded string */
    ws_dprintf(L_WS_DBG,"Decoded %s\n",decodebuffer);

    *username = (char*)decodebuffer;
    *password = *username;

    strsep(password,":");

    ws_dprintf(L_WS_DBG,"Decoded user=%s, pw=%s\n",*username,*password);
    WS_EXIT();
    return TRUE;
}

/**
 * Simple wrapper around the CONNINFO response headers
 *
 * FIXME: See comments on other printf style functions
 *
 * @param pwsc session to add a response header to
 * @param header header to add
 * @param fmt format of value to add
 * @return TRUE on success
 */
int ws_addresponseheader(WS_CONNINFO *pwsc, char *header, char *fmt, ...) {
    va_list ap;
    char value[MAX_LINEBUFFER];

    WS_ENTER();

    va_start(ap,fmt);
    vsnprintf(value,sizeof(value),fmt,ap);
    va_end(ap);

    WS_EXIT(); /* FIXME: Put this after the ws_addarg */
    return ws_addarg(&pwsc->response_headers,header,value);
}

/**
 * Fetch a get/post variable (case insensitive)
 *
 * @param pwsc session to get variable for
 * @param var get/post variable to retrieve
 * @returns pointer to value of variable, or NULL
 */
char *ws_getvar(WS_CONNINFO *pwsc, char *var) {
    return ws_getarg(&(pwsc->request_vars),var);
}

/**
 * Fetch a http request header (case insenstive)
 *
 * @param pwsc session to get http request header for
 * @param header header value to retrieve
 * @returns pointer to value of header variable, or NULL
 */
char *ws_getrequestheader(WS_CONNINFO *pwsc, char *header) {
    return ws_getarg(&pwsc->request_headers,header);
}

/**
 * get the local storage pointer.
 *
 * @param pwsc connection to get local storage for
 */
void *ws_get_local_storage(WS_CONNINFO *pwsc) {
    return pwsc->local_storage;
}


/**
 * lock the local storage pointer.  This is sort of wrong, as
 * the all operations manipulating local storage are locked,
 * not just the one you are working on.
 *
 * @param pwsc connection to lock storage pointer for
 */
void ws_lock_local_storage(WS_CONNINFO *pwsc) {
    WS_PRIVATE *pwsp;

    pwsp = (WS_PRIVATE *)pwsc->pwsp;
    ws_lock_connlist(pwsp);
}


/**
 * unlock the local storage pointer.  Again, this suffers (?)
 * from the same lack of fine-grained semaphores described above
 *
 * @param pwsc connection to unlock storage for
 */
void ws_unlock_local_storage(WS_CONNINFO *pwsc) {
    WS_PRIVATE *pwsp;

    pwsp = (WS_PRIVATE *)pwsc->pwsp;
    ws_unlock_connlist(pwsp);
}

/**
 * set the local storage pointer (and callback).  If there is no
 * callback, the storage pointer will be free'd when the session is
 * terminated.  If the callback is set, then the application can
 * choose a better session deallocation routine.
 *
 * @param pwsc session to set ptr and callback for
 * @param ptr new local storage pointer
 * @param callback callback function to call (passes the ptr)
 */
void ws_set_local_storage(WS_CONNINFO *pwsc, void *ptr, void (*callback)(void *)) {
    if(!pwsc)
        return;

    if(pwsc->local_storage) {
        ws_dprintf(L_WS_FATAL,"ls already allocated");
    }

    pwsc->storage_callback = callback;
    pwsc->local_storage = ptr;
}

/**
 * Walk through the connection list and enumerate all the items.
 * vpp should be a pointer to a WSTHREADENUM type.  This is opaque,
 * and used by the enumerator functions.  The same WSTHREADENUM must
 * be passed to the ws_thread_enum_ functions.  Also, note that as soon
 * as the enum is started, the connlist is locked, so the enumeration
 * must be completed, or new connections will be blocked.
 *
 * FIXME: should this be a session connection?
 *
 * @param wsh server handle
 * @param vpp opaque WSTHREADENUM structure
 * @returns first WS_CONNINFO, or NULL, if no connections
 */
WS_CONNINFO *ws_thread_enum_first(WSHANDLE wsh, WSTHREADENUM *vpp) {
    WS_PRIVATE *pwsp = (WS_PRIVATE *)wsh;
    WS_CONNINFO *pwsc = NULL;
    WS_CONNLIST *pconlist;

    ws_lock_connlist(pwsp);

    pconlist = pwsp->connlist.next;
    *vpp = (WSTHREADENUM)pconlist;
    if(pconlist) {
        pwsc = pconlist->pwsc;
    } else {
        ws_unlock_connlist(pwsp);
    }

    return pwsc;
}

/**
 * continue enumerating the connection list.  This should only
 * be called if ws_thread_enum_first returns a non-null value.
 * again, this must be called repeatedly until a NULL result is
 * returned, or the connection list will remain locked
 *
 * @param wsh web server handle
 * @param vpp opaque WSTHREADENUM structure
 * @returns next connection handle, or NULL
 */
WS_CONNINFO *ws_thread_enum_next(WSHANDLE wsh, WSTHREADENUM *vpp) {
    WS_PRIVATE *pwsp = (WS_PRIVATE *)wsh;
    WS_CONNINFO *pwsc = NULL;
    WS_CONNLIST *pconlist;

    pconlist = (WS_CONNLIST*)*vpp;
    if((!pconlist) || (!pconlist->next)) {
        ws_unlock_connlist(pwsp);
    } else {
        pconlist=pconlist->next;
        *vpp = (WSTHREADENUM)pconlist;
        pwsc = pconlist->pwsc;
    }

    return pwsc;
}

/**
 * Enumerate a key list (could be either request headers or get/post vars).
 * To start the enumeration, pass NULL to last.
 *
 * @param pwsc session to enumerate vars for
 * @param key filled in with next key
 * @param value filled in with next value
 * @param last NULL to start the enumeration, or return value from last call
 * @returns value o be passed into last for next round.
 */
void *ws_enum_var(WS_CONNINFO *pwsc, char **key, char **value, void *last) {
    ARGLIST *plist = (ARGLIST *)last;

    if(!plist) {
        plist = pwsc->request_vars.next;
    } else {
        plist = plist->next;
    }

    if(plist) {
        if(key) *key = plist->key;
        if(value) *value = plist->value;
    }

    return plist;
}

/**
 * copy a file (given a fd) to the output socket
 *
 * FIXME: Move this to something like io_readwrite
 *
 * @param pwsc connection to stream output to
 * @param fromfd fd to read and strem from
 * @returns number of bytes copied, or -1  FIXME: pass copy bytes, return T/F
 */
int ws_copyfile(WS_CONNINFO *pwsc, IOHANDLE hfile, uint64_t *bytes_copied) {
    int retval = FALSE;
    unsigned char buf[BLKSIZE];

    uint64_t total_bytes = 0;
    uint32_t bytes_read = 0;
    uint32_t bytes_written = 0;

    ASSERT(pwsc);
    if(!pwsc)
        return -1; /* error handling! */

    bytes_read = BLKSIZE;
    while(io_read(hfile,buf,&bytes_read) && bytes_read) {
        bytes_written = bytes_read;
        if(!io_write(pwsc->hclient,buf,&bytes_written)) {
            ws_dprintf(L_WS_LOG,"Write error: %s\n",io_errstr(pwsc->hclient));
            *bytes_copied = total_bytes;
            return FALSE;
        }

        if(bytes_written != bytes_read) {
            ws_dprintf(L_WS_LOG,"Internal error in ws_copyfile\n");
            break;
        }

        total_bytes += bytes_read;
    }

    if(!bytes_read) {
        retval = TRUE;
    } else {
        ws_dprintf(L_WS_LOG,"Read error %s\n",io_errstr(hfile));
    }

    if(bytes_copied)
        *bytes_copied = total_bytes;

    return retval;
}

/**
 * return the URI requested
 *
 * @param pwsc connection handle
 * @returns the URI requested
 */
char *ws_uri(WS_CONNINFO *pwsc) {
    return pwsc->uri;
}

/**
 * mark this thread as one to be closed
 *
 * @param pwsc connection handle to close
 * @param should_close whether the connection should be closed at finish
 */
void ws_should_close(WS_CONNINFO *pwsc, int should_close) {
    ASSERT(pwsc);

    pwsc->close = should_close;
}

/**
 * retrive the thread number, internal id
 *
 * @param pwsc connection to get thread number for
 * @returns thread number (int)
 */
extern int ws_threadno(WS_CONNINFO *pwsc) {
    ASSERT(pwsc);

    if(pwsc)
        return pwsc->threadno;
    return 0;
}

/**
 * return the hostname of the connected endpoint
 *
 * @param pwsc connection to get endpoint for
 */
extern char *ws_hostname(WS_CONNINFO *pwsc) {
    ASSERT(pwsc);
    ASSERT(pwsc->hostname);

    if((pwsc) && (pwsc->hostname)) {
        return pwsc->hostname;
    }

    return NULL;
}

/**
 * set the local error for the selected socket session.  This looks
 * up and populates the error message at the time of failure, presuming
 * that the application will subsequently just print the error message
 * anyway.
 *
 * @param pwsc session to set error for
 * @param error code to set
 * @return TRUE on success
 */
int ws_set_err(WS_CONNINFO *pwsc, int ws_error) {
#ifdef WIN32
    char lpErrorBuf[256];
#endif

    ASSERT(pwsc);

    if(!pwsc)
        return FALSE;

    if(pwsc->err_msg)
        free(pwsc->err_msg);

    pwsc->err_code = ws_error;
    if(E_WS_SUCCESS == ws_error)
        return TRUE;

    ws_should_close(pwsc,TRUE); /* close the session on error */

    if(E_WS_NATIVE == ws_error) {
#ifdef WIN32
        pwsc->err_native = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,pwsc->err_native,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
            (LPTSTR)lpErrorBuf,sizeof(lpErrorBuf),NULL);
        pwsc->err_msg = strdup(lpErrorBuf);

#else
        pwsc->err_native = errno;
        pwsc->err_msg = strdup(strerror(pwsc->err_native));
#endif
    }

    return TRUE;
}

/**
 * get the local error and error code for a selected socket session.
 * Note that the error message is a pointer to an internal string.
 * The error isn't valid of other socket operations take place.  The
 * error should be dup'ed or used immediately.
 *
 * Sample:
 *
 * char *errstr;
 * int errcode;
 *
 * if(!ws_writebinary(pwsc,"foo",3)) {
 *     ws_get_err(pwsc,&errcode, &errstr);
 *     fprintf(stderr,"Error: %s",errstr);
 * }
 * @param pwsc session to get error of
 * @param errcode to be filled with error number
 * @param err_msg to be filled with non-freeable pointer to error msg
 */
int ws_get_err(WS_CONNINFO *pwsc, int *errcode, char **err_msg) {
    ASSERT(pwsc);

    if(!pwsc)
        return FALSE;

    if(errcode) {
        *errcode = pwsc->err_code;
    }

    if(err_msg) {
        if(E_WS_NATIVE != pwsc->err_code) {
            *err_msg = ws_errors[pwsc->err_code];
        } else {
            *err_msg = pwsc->err_msg;
        }
    }

    return TRUE;
}

