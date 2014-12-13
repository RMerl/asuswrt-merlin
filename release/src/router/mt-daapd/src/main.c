/*
 * $Id: main.c,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Driver for multi-threaded daap server
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

/**
 * \file main.c
 *
 * Driver for mt-daapd, including the main() function.  This
 * is responsible for kicking off the initial mp3 scan, starting
 * up the signal handler, starting up the webserver, and waiting
 * around for external events to happen (like a request to rescan,
 * or a background rescan to take place.)
 *
 * It also contains the daap handling callback for the webserver.
 * This should almost certainly be somewhere else, and is in
 * desparate need of refactoring, but somehow continues to be in
 * this file.
 *
 * \todo Refactor daap_handler()
 */

/** \mainpage mt-daapd
 * \section about_section About
 *
 * This is mt-daapd, an attempt to create an iTunes server for
 * linux and other POSIXish systems.  Maybe even Windows with cygwin,
 * eventually.
 *
 * You might check these locations for more info:
 * - <a href="http://sf.net/projects/mt-daapd">Project page on SourceForge</a>
 * - <a href="http://mt-daapd.sf.net">Home page</a>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <restart.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "configfile.h"
#include "db-memory.h"
#include "daap.h"
#include "daap-proto.h"
#include "err.h"
#include "mp3-scanner.h"
#include "webserver.h"
#include "playlist.h"
#include "dynamic-art.h"

#ifndef WITHOUT_MDNS
#include "rend.h"
#endif

/**
 * Where the default configfile is.  On the NSLU2 running unslung,
 * thats in /opt, not /etc. */
#ifndef DEFAULT_CONFIGFILE
#ifdef NSLU2
#define DEFAULT_CONFIGFILE "/opt/etc/mt-daapd/mt-daapd.conf"
#else
#define DEFAULT_CONFIGFILE "/etc/mt-daapd.conf"
#endif
#endif

/** Where to dump the pidfile */
#ifndef PIDFILE
#define PIDFILE "/var/run/mt-daapd.pid"
#endif

/** You say po-tay-to, I say po-tat-o */
#ifndef SIGCLD
# define SIGCLD SIGCHLD
#endif

/** Seconds to sleep before checking for a shutdown or reload */
#define MAIN_SLEEP_INTERVAL  2

/** Let's hope if you have no atoll, you only have 32 bit inodes... */
#if !HAVE_ATOLL
#  define atoll(a) atol(a)
#endif

/*
 * Globals
 */
CONFIG config; /**< Main configuration structure, as read from configfile */

/*
 * Forwards
 */
static int daemon_start(void);
static void write_pid_file(void);
static void usage(char *program);
static void *signal_handler(void *arg);
static int start_signal_handler(pthread_t *handler_tid);
static void daap_handler(WS_CONNINFO *pwsc);
static int daap_auth(char *username, char *password);

/**
 * Handles authentication for the daap server.  This isn't the
 * authenticator for the web admin page, but rather the iTunes
 * authentication when trying to connect to the server.  Note that most
 * of this is actually handled in the web server registration, which
 * decides when to apply the authentication or not.  If you mess with
 * when and where the webserver applies auth or not, you'll likely
 * break something.  It seems that some requests must be authed, and others
 * not.  If you apply authentication somewhere that iTunes doesn't expect
 * it, it happily disconnects.
 *
 * \param username The username passed by iTunes
 * \param password The password passed by iTunes
 * \returns 1 if auth successful, 0 otherwise
 */
int daap_auth(char *username, char *password) {
    if((password == NULL) &&
       ((config.readpassword == NULL) || (strlen(config.readpassword)==0)))
        return 1;

    if(password == NULL)
        return 0;

    return !strcasecmp(password,config.readpassword);
}

/**
 * This handles requests that are daap-related.  For example,
 * /server-info, /login, etc.  This should really be split up
 * into multiple functions, and perhaps moved into daap.c
 *
 * \param pwsc Webserver connection info, passed from the webserver
 *
 * \todo Decomplexify this!
 */
void daap_handler(WS_CONNINFO *pwsc) {
    int close;
    DAAP_BLOCK *root;
    int clientrev;

    /* for the /databases URI */
    char *uri;
    unsigned long int db_index;
    unsigned long int playlist_index;
    unsigned long int item=0;
    char *first, *last;
    char* index = 0;
    int streaming=0;
    int compress =0;
    int start_time;
    int end_time;
    int bytes_written;

    MP3FILE *pmp3;
    int file_fd;
    int session_id=0;

    int img_fd;
    struct stat sb;
    long img_size;

    off_t offset=0;
    off_t real_len;
    off_t file_len;

    int bytes_copied=0;

    GZIP_STREAM *gz;

    close=pwsc->close;
    pwsc->close=1;  /* in case we have any errors */
    root=NULL;

    ws_addresponseheader(pwsc,"Accept-Ranges","bytes");
    ws_addresponseheader(pwsc,"DAAP-Server","mt-daapd/%s",VERSION);
    ws_addresponseheader(pwsc,"Content-Type","application/x-dmap-tagged");

    if(ws_getvar(pwsc,"session-id")) {
        session_id=atoi(ws_getvar(pwsc,"session-id"));
    }

    if(!strcasecmp(pwsc->uri,"/server-info")) {
        config_set_status(pwsc,session_id,"Sending server info");
        root=daap_response_server_info(config.servername,
                                       ws_getrequestheader(pwsc,"Client-DAAP-Version"));
    } else if (!strcasecmp(pwsc->uri,"/content-codes")) {
        config_set_status(pwsc,session_id,"Sending content codes");
        root=daap_response_content_codes();
    } else if (!strcasecmp(pwsc->uri,"/login")) {
        config_set_status(pwsc,session_id,"Logging in");
        root=daap_response_login(pwsc->hostname);
    } else if (!strcasecmp(pwsc->uri,"/update")) {
        if(!ws_getvar(pwsc,"delta")) { /* first check */
            clientrev=db_version() - 1;
            config_set_status(pwsc,session_id,"Sending database");
        } else {
            clientrev=atoi(ws_getvar(pwsc,"delta"));
            config_set_status(pwsc,session_id,"Waiting for DB updates");
        }
        root=daap_response_update(pwsc->fd,clientrev);
        if((ws_getvar(pwsc,"delta")) && (root==NULL)) {
            DPRINTF(E_LOG,L_WS,"Client %s disconnected\n",pwsc->hostname);
            config_set_status(pwsc,session_id,NULL);
            pwsc->close=1;
            return;
        }
    } else if (!strcasecmp(pwsc->uri,"/logout")) {
        config_set_status(pwsc,session_id,NULL);
        ws_returnerror(pwsc,204,"Logout Successful");
        return;
    } else if(strcmp(pwsc->uri,"/databases")==0) {
        config_set_status(pwsc,session_id,"Sending database info");
        root=daap_response_dbinfo(config.servername);
        if(0 != (index = ws_getvar(pwsc, "index")))
            daap_handle_index(root, index);
    } else if(strncmp(pwsc->uri,"/databases/",11) == 0) {

        /* the /databases/ uri will either be:
         *
         * /databases/id/items, which returns items in a db
         * /databases/id/containers, which returns a container
         * /databases/id/containers/id/items, which returns playlist elements
         * /databases/id/items/id.mp3, to spool an mp3
 * /databases/id/browse/category
         */

        uri = strdup(pwsc->uri);
        first=(char*)&uri[11];
        last=first;
        while((*last) && (*last != '/')) {
            last++;
        }

        if(*last) {
            *last='\0';
            db_index=atoll(first);

            last++;

            if(strncasecmp(last,"items/",6)==0) {
                /* streaming */
                first=last+6;
                while((*last) && (*last != '.'))
                    last++;

                if(*last == '.') {
                    *last='\0';
                    item=atoll(first);
                    streaming=1;
                    DPRINTF(E_DBG,L_DAAP|L_WS,"Streaming request for id %lu\n",item);
                }
                free(uri);
            } else if (strncasecmp(last,"items",5)==0) {
                /* songlist */
                free(uri);
                // pass the meta field request for processing
                // pass the query request for processing
                root=daap_response_songlist(ws_getvar(pwsc,"meta"),
                                            ws_getvar(pwsc,"query"));
                config_set_status(pwsc,session_id,"Sending songlist");
            } else if (strncasecmp(last,"containers/",11)==0) {
                /* playlist elements */
                first=last + 11;
                last=first;
                while((*last) && (*last != '/')) {
                    last++;
                }

                if(*last) {
                    *last='\0';
                    playlist_index=atoll(first);
                    // pass the meta list info for processing
                    root=daap_response_playlist_items(playlist_index,
                                                      ws_getvar(pwsc,"meta"),
                                                      ws_getvar(pwsc,"query"));
                }
                free(uri);
                config_set_status(pwsc,session_id,"Sending playlist info");
            } else if (strncasecmp(last,"containers",10)==0) {
                /* list of playlists */
                free(uri);
                root=daap_response_playlists(config.servername);
                config_set_status(pwsc,session_id,"Sending playlist info");
            } else if (strncasecmp(last,"browse/",7)==0) {
                config_set_status(pwsc,session_id,"Compiling browse info");
                root = daap_response_browse(last + 7,
                                            ws_getvar(pwsc, "filter"));
                config_set_status(pwsc,session_id,"Sending browse info");
                free(uri);
            }
        }

        // prune the full list if an index range was specified
        if(0 != (index = ws_getvar(pwsc, "index")))
            daap_handle_index(root, index);
    }

    if((!root)&&(!streaming)) {
        DPRINTF(E_DBG,L_WS|L_DAAP,"Bad request -- root=%x, streaming=%d\n",root,streaming);
        ws_returnerror(pwsc,400,"Invalid Request");
        config_set_status(pwsc,session_id,NULL);
        return;
    }

    pwsc->close=close;

    if(!streaming) {
        DPRINTF(E_DBG,L_WS,"Satisfying request\n");

        if((config.compress) && ws_testrequestheader(pwsc,"Accept-Encoding","gzip") && root->reported_size >= 1000) {
          compress=1;
        }

        DPRINTF(E_DBG,L_WS|L_DAAP,"Serializing\n");
        start_time = time(NULL);
        if (compress) {
          DPRINTF(E_DBG,L_WS|L_DAAP,"Using compression: %s\n", pwsc->uri);
          gz = gzip_alloc();
          daap_serialize(root,pwsc->fd,gz);
          gzip_compress(gz);
          bytes_written = gz->bytes_out;
          ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
          ws_addresponseheader(pwsc,"Content-Length","%d",bytes_written);
          ws_addresponseheader(pwsc,"Content-Encoding","gzip");
          DPRINTF(E_DBG,L_WS,"Emitting headers\n");
          ws_emitheaders(pwsc);
          if (gzip_close(gz,pwsc->fd) != bytes_written) {
            DPRINTF(E_LOG,L_WS|L_DAAP,"Error compressing data\n");
          }
          DPRINTF(E_DBG,L_WS|L_DAAP,"Compression ratio: %f\n",((double) bytes_written)/(8.0 + root->reported_size))
        }
        else {
          bytes_written = root->reported_size + 8;
          ws_addresponseheader(pwsc,"Content-Length","%d",bytes_written);
          ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
          DPRINTF(E_DBG,L_WS,"Emitting headers\n");
          ws_emitheaders(pwsc);
          daap_serialize(root,pwsc->fd,NULL);
        }
        end_time = time(NULL);
        DPRINTF(E_DBG,L_WS|L_DAAP,"Sent %d bytes in %d seconds\n",bytes_written,end_time-start_time);
        DPRINTF(E_DBG,L_WS|L_DAAP,"Done, freeing\n");
        daap_free(root);
    } else {
        /* stream out the song */
        pwsc->close=1;

        if(ws_getrequestheader(pwsc,"range")) {
            offset=(off_t)atol(ws_getrequestheader(pwsc,"range") + 6);
        }

        pmp3=db_find(item);
        if(!pmp3) {
            DPRINTF(E_LOG,L_DAAP|L_WS|L_DB,"Could not find requested item %lu\n",item);
            ws_returnerror(pwsc,404,"File Not Found");
        } else {
            /* got the file, let's open and serve it */
            file_fd=r_open2(pmp3->path,O_RDONLY);
            if(file_fd == -1) {
                pwsc->error=errno;
                DPRINTF(E_WARN,L_WS,"Thread %d: Error opening %s: %s\n",
                        pwsc->threadno,pmp3->path,strerror(errno));
                ws_returnerror(pwsc,404,"Not found");
                config_set_status(pwsc,session_id,NULL);
                db_dispose(pmp3);
                free(pmp3);
            } else {
                real_len=lseek(file_fd,0,SEEK_END);
                lseek(file_fd,0,SEEK_SET);

                /* Re-adjust content length for cover art */
                if((config.artfilename) &&
                   ((img_fd=da_get_image_fd(pmp3->path)) != -1)) {
                  fstat(img_fd, &sb);
                  img_size = sb.st_size;

                  if (strncasecmp(pmp3->type,"mp3",4) ==0) {
                    /*PENDING*/
                  } else if (strncasecmp(pmp3->type, "m4a", 4) == 0) {
                    real_len += img_size + 24;

                    if (offset > img_size + 24) {
                      offset -= img_size + 24;
                    }
                  }
                }

                file_len = real_len - offset;

                DPRINTF(E_DBG,L_WS,"Thread %d: Length of file (remaining) is %ld\n",
                        pwsc->threadno,(long)file_len);

                // DWB:  fix content-type to correctly reflect data
                // content type (dmap tagged) should only be used on
                // dmap protocol requests, not the actually song data
                if(pmp3->type)
                    ws_addresponseheader(pwsc,"Content-Type","audio/%s",pmp3->type);

                ws_addresponseheader(pwsc,"Content-Length","%ld",(long)file_len);
                ws_addresponseheader(pwsc,"Connection","Close");


                if(!offset)
                    ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
                else {
                    ws_addresponseheader(pwsc,"Content-Range","bytes %ld-%ld/%ld",
                                         (long)offset,(long)real_len,
                                         (long)real_len+1);
                    ws_writefd(pwsc,"HTTP/1.1 206 Partial Content\r\n");
                }

                ws_emitheaders(pwsc);

                config_set_status(pwsc,session_id,"Streaming file '%s'",pmp3->fname);
                DPRINTF(E_LOG,L_WS,"Session %d: Streaming file '%s' to %s (offset %d)\n",
                        session_id,pmp3->fname, pwsc->hostname,(long)offset);

                if(!offset)
                    config.stats.songs_served++; /* FIXME: remove stat races */

                if((config.artfilename) &&
                   (!offset) &&
                   ((img_fd=da_get_image_fd(pmp3->path)) != -1)) {
                    if (strncasecmp(pmp3->type,"mp3",4) ==0) {
                        DPRINTF(E_INF,L_WS|L_ART,"Dynamic add artwork to %s (fd %d)\n",
                                pmp3->fname, img_fd);
                        da_attach_image(img_fd, pwsc->fd, file_fd, offset);
                    } else if (strncasecmp(pmp3->type, "m4a", 4) == 0) {
                        DPRINTF(E_INF,L_WS|L_ART,"Dynamic add artwork to %s (fd %d)\n",
                                pmp3->fname, img_fd);
                        da_aac_attach_image(img_fd, pwsc->fd, file_fd, offset);
                    }
                } else if(offset) {
                    DPRINTF(E_INF,L_WS,"Seeking to offset %ld\n",(long)offset);
                    lseek(file_fd,offset,SEEK_SET);
                }

                if((bytes_copied=copyfile(file_fd,pwsc->fd)) == -1) {
                    DPRINTF(E_INF,L_WS,"Error copying file to remote... %s\n",
                            strerror(errno));
                } else {
                    DPRINTF(E_INF,L_WS,"Finished streaming file to remote: %d bytes\n",
                            bytes_copied);
                }

                config_set_status(pwsc,session_id,NULL);
                r_close(file_fd);
                db_dispose(pmp3);
                free(pmp3);
            }
        }
    }

    DPRINTF(E_DBG,L_WS|L_DAAP,"Finished serving DAAP response\n");

    return;
}

/**
 * Fork and exit.  Stolen pretty much straight from Stevens.
 */
int daemon_start(void) {
    int childpid, fd;

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    // Fork and exit
    if ((childpid = fork()) < 0) {
        fprintf(stderr, "Can't fork!\n");
        return -1;
    } else if (childpid > 0)
        exit(0);

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
        dup2(fd, STDERR_FILENO);
        if (fd > 2)
            close(fd);
    }

    /*
    for (fd = 0; fd < FOPEN_MAX; fd++)
        close(fd);
    */

    errno = 0;

    chdir("/");
    umask(0);

    return 0;
}

/**
 * Print usage information to stdout
 *
 * \param program name of program (argv[0])
 */
void usage(char *program) {
    printf("Usage: %s [options]\n\n",program);
    printf("Options:\n");
    printf("  -d <number>    Debuglevel (0-9)\n");
    printf("  -D <mod,mod..> Debug modules\n");
    printf("  -m             Disable mDNS\n");
    printf("  -c <file>      Use configfile specified\n");
    printf("  -p             Parse playlist file\n");
    printf("  -f             Run in foreground\n");
    printf("  -y             Yes, go ahead and run as non-root user\n");
    printf("\n\n");
    printf("Valid debug modules:\n");
    printf(" config,webserver,database,scan,query,index,browse\n");
    printf(" playlist,art,daap,main,rend,misc\n");
    printf("\n\n");
}

/**
 * Drop privs.  This allows mt-daapd to run as a non-privileged user.
 * Hopefully this will limit the damage it could do if exploited
 * remotely.  Note that only the user need be specified.  GID
 * is set to the primary group of the user.
 *
 * \param user user to run as (or UID)
 */
int drop_privs(char *user) {
    int err;
    struct passwd *pw=NULL;

    /* drop privs */
    if(getuid() == (uid_t)0) {
        //if(atoi(user)) {
        //    pw=getpwuid((uid_t)atoi(user)); /* doh! */
        //} else {
            pw=getpwnam(config.runas);
        //}

        if(pw) {
            if(initgroups(user,pw->pw_gid) != 0 ||
               setgid(pw->pw_gid) != 0 ||
               setuid(pw->pw_uid) != 0) {
                err=errno;
                fprintf(stderr,"Couldn't change to %s, gid=%d, uid=%d\n",
                        user,pw->pw_gid, pw->pw_uid);
                errno=err;
                return -1;
            }
        } else {
            err=errno;
            fprintf(stderr,"Couldn't lookup user %s\n",user);
            errno=err;
            return -1;
        }
    }

    return 0;
}

/**
 * Wait for signals and flag the main process.  This is
 * a thread handler for the signal processing thread.  It
 * does absolutely nothing except wait for signals.  The rest
 * of the threads are running with signals blocked, so this thread
 * is guaranteed to catch all the signals.  It sets flags in
 * the config structure that the main thread looks for.  Specifically,
 * the stop flag (from an INT signal), and the reload flag (from HUP).
 * \param arg NULL, but required of a thread procedure
 */
void *signal_handler(void *arg) {
    sigset_t intmask;
    int sig;
    int status;

    config.stop=0;
    config.reload=0;
    config.pid=getpid();

    DPRINTF(E_WARN,L_MAIN,"Signal handler started\n");

    while(!config.stop) {
        if((sigemptyset(&intmask) == -1) ||
           (sigaddset(&intmask, SIGCLD) == -1) ||
           (sigaddset(&intmask, SIGINT) == -1) ||
           (sigaddset(&intmask, SIGHUP) == -1) ||
           (sigwait(&intmask, &sig) == -1)) {
            DPRINTF(E_FATAL,L_MAIN,"Error waiting for signals.  Aborting\n");
        } else {
            /* process the signal */
            switch(sig) {
            case SIGCLD:
                DPRINTF(E_LOG,L_MAIN,"Got CLD signal.  Reaping\n");
                while (wait(&status)) {
                };
                break;
            case SIGINT:
                DPRINTF(E_LOG,L_MAIN,"Got INT signal. Notifying daap server.\n");
                config.stop=1;
                return NULL;
                break;
            case SIGHUP:
                DPRINTF(E_LOG,L_MAIN,"Got HUP signal. Notifying daap server.\n");
                config.reload=1;
                break;
            default:
                DPRINTF(E_LOG,L_MAIN,"What am I doing here?\n");
                break;
            }
        }
    }

    return NULL;
}

/**
 * Block signals, then start the signal handler.  The
 * signal handler started by spawning a new thread on
 * signal_handler().
 *
 * \returns 0 on success, -1 on failure with errno set
 */
int start_signal_handler(pthread_t *handler_tid) {
    int error;
    sigset_t set;

    if((sigemptyset(&set) == -1) ||
       (sigaddset(&set,SIGINT) == -1) ||
       (sigaddset(&set,SIGHUP) == -1) ||
       (sigaddset(&set,SIGCLD) == -1) ||
       (sigprocmask(SIG_BLOCK, &set, NULL) == -1)) {
        DPRINTF(E_LOG,L_MAIN,"Error setting signal set\n");
        return -1;
    }

    if(error=pthread_create(handler_tid, NULL, signal_handler, NULL)) {
        errno=error;
        DPRINTF(E_LOG,L_MAIN,"Error creating signal_handler thread\n");
        return -1;
    }

    /* we'll not detach this... let's join it */
    //pthread_detach(handler_tid);
    return 0;
}

/**
 * Kick off the daap server and wait for events.
 *
 * This starts the initial db scan, sets up the signal
 * handling, starts the webserver, then sits back and waits
 * for events, as notified by the signal handler and the
 * web interface.  These events are communicated via flags
 * in the config structure.
 *
 * \param argc count of command line arguments
 * \param argv command line argument pointers
 * \returns 0 on success, -1 otherwise
 *
 * \todo split out a ws_init and ws_start, so that the
 * web space handlers can be registered before the webserver
 * starts.
 *
 */
int main(int argc, char *argv[]) {
    int option;
    char *configfile=DEFAULT_CONFIGFILE;
    WSCONFIG ws_config;
    WSHANDLE server;
    int parseonly=0;
    int foreground=0;
    int reload=0;
    int start_time;
    int end_time;
    int rescan_counter=0;
    int old_song_count;
    int force_non_root=0;
    pthread_t signal_tid;

    int pid_fd;
    FILE *pid_fp=NULL;

    config.use_mdns=1;
    err_debuglevel=1;

    while((option=getopt(argc,argv,"D:d:c:mpfry")) != -1) {
        switch(option) {
        case 'd':
            err_debuglevel=atoi(optarg);
            break;
        case 'D':
            if(err_setdebugmask(optarg)) {
                usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'f':
            foreground=1;
            break;

        case 'c':
            configfile=optarg;
            break;

        case 'm':
            config.use_mdns=0;
            break;

        case 'p':
            parseonly=1;
            foreground=1;
            break;

        case 'r':
            reload=1;
            break;

        case 'y':
            force_non_root=1;
            break;

        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    if((getuid()) && (!force_non_root)) {
        fprintf(stderr,"You are not root.  This is almost certainly wrong.  If you are\n"
                "sure you want to do this, use the -y command-line switch\n");
        exit(EXIT_FAILURE);
    }

    /* read the configfile, if specified, otherwise
     * try defaults */
    config.stats.start_time=start_time=time(NULL);

    if(config_read(configfile)) {
        fprintf(stderr,"Error reading config file (%s)\n",configfile);
        exit(EXIT_FAILURE);
    }

    if(!foreground) {
        if(config.logfile) {
            err_setdest(config.logfile,LOGDEST_LOGFILE);
        } else {
            err_setdest("mt-daapd",LOGDEST_SYSLOG);
        }
    }

#ifndef WITHOUT_MDNS
    if((config.use_mdns) && (!parseonly)) {
        DPRINTF(E_LOG,L_MAIN,"Starting rendezvous daemon\n");
        if(rend_init(config.runas)) {
            DPRINTF(E_FATAL,L_MAIN|L_REND,"Error in rend_init: %s\n",strerror(errno));
        }
    }
#endif

    /* open the pidfile, so it can be written once we detach */
    if((!foreground) && (!force_non_root)) {
        if(-1 == (pid_fd = open(PIDFILE,O_CREAT | O_WRONLY | O_TRUNC, 0644)))
            DPRINTF(E_FATAL,L_MAIN,"Error opening pidfile (%s): %s\n",PIDFILE,strerror(errno));

        if(0 == (pid_fp = fdopen(pid_fd, "w")))
            DPRINTF(E_FATAL,L_MAIN,"fdopen: %s\n",strerror(errno));

        daemon_start();

        /* just to be on the safe side... */
        config.pid=0;
    }

    /* DWB: shouldn't this be done after dropping privs? */
    if(db_open(config.dbdir, reload))
        DPRINTF(E_FATAL,L_MAIN|L_DB,"Error in db_open: %s\n",strerror(errno));


    // Drop privs here
    if(drop_privs(config.runas)) {
        DPRINTF(E_FATAL,L_MAIN,"Error in drop_privs: %s\n",strerror(errno));
    }

    /* block signals and set up the signal handling thread */
    DPRINTF(E_LOG,L_MAIN,"Starting signal handler\n");
    if(start_signal_handler(&signal_tid)) {
        DPRINTF(E_FATAL,L_MAIN,"Error starting signal handler %s\n",strerror(errno));
    }


    if(pid_fp) {
        /* wait to for config.pid to be set by the signal handler */
        while(!config.pid) {
            sleep(1);
        }

        fprintf(pid_fp,"%d\n",config.pid);
        fclose(pid_fp);
    }

    DPRINTF(E_LOG,L_MAIN|L_PL,"Loading playlists\n");

    if(config.playlist)
        pl_load(config.playlist);

    if(parseonly) {
        if(!pl_error) {
            fprintf(stderr,"Parsed successfully.\n");
            pl_dump();
        }
        exit(EXIT_SUCCESS);
    }

    /* Initialize the database before starting */
    DPRINTF(E_LOG,L_MAIN|L_DB,"Initializing database\n");
    if(db_init()) {
        DPRINTF(E_FATAL,L_MAIN|L_DB,"Error in db_init: %s\n",strerror(errno));
    }

    DPRINTF(E_LOG,L_MAIN|L_SCAN,"Starting mp3 scan\n");
    if(scan_init(config.mp3dir)) {
        DPRINTF(E_FATAL,L_MAIN|L_SCAN,"Error scanning MP3 files: %s\n",strerror(errno));
    }

    /* start up the web server */

    ws_config.web_root=config.web_root;
    ws_config.port=config.port;

    DPRINTF(E_LOG,L_MAIN|L_WS,"Starting web server from %s on port %d\n",
            config.web_root, config.port);

    server=ws_start(&ws_config);
    if(!server) {
        DPRINTF(E_FATAL,L_MAIN|L_WS,"Error staring web server: %s\n",strerror(errno));
    }

    ws_registerhandler(server, "^.*$",config_handler,config_auth,1);
    ws_registerhandler(server, "^/server-info$",daap_handler,NULL,0);
    ws_registerhandler(server, "^/content-codes$",daap_handler,NULL,0);
    ws_registerhandler(server,"^/login$",daap_handler,daap_auth,0);
    ws_registerhandler(server,"^/update$",daap_handler,daap_auth,0);
    ws_registerhandler(server,"^/databases$",daap_handler,daap_auth,0);
    ws_registerhandler(server,"^/logout$",daap_handler,NULL,0);
    ws_registerhandler(server,"^/databases/.*",daap_handler,NULL,0);

#ifndef WITHOUT_MDNS
    if(config.use_mdns) { /* register services */
        DPRINTF(E_LOG,L_MAIN|L_REND,"Registering rendezvous names\n");
        rend_register(config.servername,"_daap._tcp",config.port);
        rend_register(config.servername,"_http._tcp",config.port);
    }
#endif

    end_time=time(NULL);

    DPRINTF(E_LOG,L_MAIN,"Scanned %d songs in  %d seconds\n",db_get_song_count(),
            end_time-start_time);

    config.stop=0;

    while(!config.stop) {
        if((config.rescan_interval) && (rescan_counter > config.rescan_interval)) {
            if((config.always_scan) || (config_get_session_count())) {
                config.reload=1;
            } else {
                DPRINTF(E_DBG,L_MAIN|L_SCAN|L_DB,"Skipped bground scan... no users\n");
            }
            rescan_counter=0;
        }

        if(config.reload) {
            old_song_count = db_get_song_count();
            start_time=time(NULL);

//            DPRINTF(E_LOG,L_MAIN|L_DB|L_SCAN,"Rescanning database\n");	// J++. noisy...
            if(scan_init(config.mp3dir)) {
                DPRINTF(E_LOG,L_MAIN|L_DB|L_SCAN,"Error rescanning... exiting\n");
                config.stop=1;
            }
            config.reload=0;
            DPRINTF(E_INF,L_MAIN|L_DB|L_SCAN,"Scanned %d songs (was %d) in %d seconds\n",
                    db_get_song_count(),old_song_count,time(NULL)-start_time);
        }

        sleep(MAIN_SLEEP_INTERVAL);
        rescan_counter += MAIN_SLEEP_INTERVAL;
    }

    DPRINTF(E_LOG,L_MAIN,"Stopping gracefully\n");

#ifndef WITHOUT_MDNS
    if(config.use_mdns) {
        DPRINTF(E_LOG,L_MAIN|L_REND,"Stopping rendezvous daemon\n");
        rend_stop();
    }
#endif

    DPRINTF(E_LOG,L_MAIN,"Stopping signal handler\n");
    if(!pthread_kill(signal_tid,SIGINT)) {
        pthread_join(signal_tid,NULL);
    }

    /* Got to find a cleaner way to stop the web server.
     * Closing the fd of the socking accepting doesn't necessarily
     * cause the accept to fail on some libcs.
     *
    DPRINTF(E_LOG,L_MAIN|L_WS,"Stopping web server\n");
    ws_stop(server);
    */

    config_close();

    DPRINTF(E_LOG,L_MAIN|L_DB,"Closing database\n");
    db_deinit();

#ifdef DEBUG_MEMORY
    fprintf(stderr,"Leaked memory:\n");
    err_leakcheck();
#endif

    DPRINTF(E_LOG,L_MAIN,"Done!\n");

    err_setdest(NULL,LOGDEST_STDERR);

    return EXIT_SUCCESS;
}

