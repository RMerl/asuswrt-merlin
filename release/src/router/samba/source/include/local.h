/* Copyright (C) 1995-1998 Samba-Team */
/* Copyright (C) 1998 John H Terpstra <jht@aquasoft.com.au> */

/* local definitions for file server */
#ifndef _LOCAL_H
#define _LOCAL_H

/* The default workgroup - usually overridden in smb.conf */
#ifndef WORKGROUP
#define WORKGROUP "WORKGROUP"
#endif

/* This defines the section name in the configuration file that will contain */
/* global parameters - that is, parameters relating to the whole server, not */
/* just services. This name is then reserved, and may not be used as a       */
/* a service name. It will default to "global" if not defined here.          */
#define GLOBAL_NAME "global"
#define GLOBAL_NAME2 "globals"

/* This defines the section name in the configuration file that will
   refer to the special "homes" service */
#define HOMES_NAME "homes"

/* This defines the section name in the configuration file that will
   refer to the special "printers" service */
#define PRINTERS_NAME "printers"

/* Yves Gaige <yvesg@hptnodur.grenoble.hp.com> requested this set this 	     */
/* to a maximum of 8 if old smb clients break because of long printer names. */
#define MAXPRINTERLEN 15

/* max number of directories open at once */
/* note that with the new directory code this no longer requires a
   file handle per directory, but large numbers do use more memory */
#define MAX_OPEN_DIRECTORIES 256

/* max number of directory handles */
/* As this now uses the bitmap code this can be
   quite large. */
#define MAX_DIRECTORY_HANDLES 2048

/* maximum number of file caches per smbd */
#define MAX_WRITE_CACHES 10

/* define what facility to use for syslog */
#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_DAEMON
#endif

/* Default size of shared memory used for share mode locking */
#ifndef SHMEM_SIZE
#define SHMEM_SIZE (1024*1024)
#endif

/* 
 * Default number of maximum open files per smbd. This is
 * also limited by the maximum available file descriptors
 * per process and can also be set in smb.conf as "max open files"
 * in the [global] section.
 */

#ifndef MAX_OPEN_FILES
#define MAX_OPEN_FILES 10000
#endif
 
/* the max number of simultanous connections to the server by all clients */
#define MAXSTATUS 100000

#define WORDMAX 0xFFFF

/* the maximum password length before we declare a likely attack */
#define MAX_PASS_LEN 200

/* separators for lists */
#define LIST_SEP " \t,;:\n\r"

#ifndef LOCKDIR
/* this should have been set in the Makefile */
#define LOCKDIR "/tmp/samba"
#endif

/* this is where browse lists are kept in the lock dir */
#define SERVER_LIST "browse.dat"

/* shall guest entries in printer queues get changed to user entries,
   so they can be deleted using the windows print manager? */
#define LPQ_GUEST_TO_USER

/* shall filenames with illegal chars in them get mangled in long
   filename listings? */
#define MANGLE_LONG_FILENAMES 

/* define this if you want to stop spoofing with .. and soft links
   NOTE: This also slows down the server considerably */
#define REDUCE_PATHS

/* the size of the directory cache */
#define DIRCACHESIZE 20

/* what default type of filesystem do we want this to show up as in a
   NT file manager window? */
#define FSTYPE_STRING "NTFS"

/* the default guest account - normally set in the Makefile or smb.conf */
#ifndef GUEST_ACCOUNT
#define GUEST_ACCOUNT "nobody"
#endif

/* the default pager to use for the client "more" command. Users can
   override this with the PAGER environment variable */
#ifndef PAGER
#define PAGER "more"
#endif

/* the size of the uid cache used to reduce valid user checks */
#define UID_CACHE_SIZE 4

/* if mmap is enabled, then this is the maximum size of file to use
   the mmap code on. We don't want to mmap huge files as virtual
   address spaces are limited */
#define MAX_MMAP_SIZE (100*0x100000)

/* the following control timings of various actions. Don't change 
   them unless you know what you are doing. These are all in seconds */
#define DEFAULT_SMBD_TIMEOUT (60*60*24*7)
#define SMBD_RELOAD_CHECK (180)
#define IDLE_CLOSED_TIMEOUT (60)
#define DPTR_IDLE_TIMEOUT (120)
#define SMBD_SELECT_TIMEOUT (60)
#define SMBD_SELECT_TIMEOUT_WITH_PENDING_LOCKS (10)
#define NMBD_SELECT_LOOP (10)
#define BROWSE_INTERVAL (60)
#define REGISTRATION_INTERVAL (10*60)
#define NMBD_INETD_TIMEOUT (120)
#define NMBD_MAX_TTL (24*60*60)
#define LPQ_LOCK_TIMEOUT (5)
#define NMBD_INTERFACES_RELOAD (120)

/* the following are in milliseconds */
#define LOCK_RETRY_TIMEOUT (100)

/* do you want to dump core (carefully!) when an internal error is
   encountered? Samba will be careful to make the core file only
   accessible to root */
#define DUMP_CORE 1

#define SMB_ALIGNMENT 1


/* shall we support browse requests via a FIFO to nmbd? */
#define ENABLE_FIFO 1

/* how long (in miliseconds) to wait for a socket connect to happen */
#define LONG_CONNECT_TIMEOUT 30000
#define SHORT_CONNECT_TIMEOUT 5000

/* the default netbios keepalive timeout */
#define DEFAULT_KEEPALIVE 300

/* the directory to sit in when idle */
/* #define IDLE_DIR "/" */

/* Timout (in seconds) to wait for an oplock break
   message to return from the client. */

#define OPLOCK_BREAK_TIMEOUT 30

/* Timout (in seconds) to add to the oplock break timeout
   to wait for the smbd to smbd message to return. */

#define OPLOCK_BREAK_TIMEOUT_FUDGEFACTOR 2

/* the read preciction code has been disabled until some problems with
   it are worked out */
#define USE_READ_PREDICTION 0

/* name of directory that netatalk uses to store macintosh resource forks */
#define APPLEDOUBLE ".AppleDouble/"

/*
 * Default passwd chat script.
 */

#define DEFAULT_PASSWD_CHAT "*new*password* %n\\n *new*password* %n\\n *changed*"

/* Minimum length of allowed password when changing UNIX password. */
#define MINPASSWDLENGTH 5

#endif
