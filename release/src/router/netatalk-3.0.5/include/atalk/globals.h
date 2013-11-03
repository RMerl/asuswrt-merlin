/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifndef AFPD_GLOBALS_H
#define AFPD_GLOBALS_H 1

#include <sys/param.h>
#include <grp.h>
#include <sys/types.h>

#ifdef HAVE_NETDB_H
#include <netdb.h>  /* this isn't header-protected under ultrix */
#endif /* HAVE_NETDB_H */

#include <atalk/afp.h>
#include <atalk/compat.h>
#include <atalk/unicode.h>
#include <atalk/uam.h>
#include <atalk/iniparser.h>
#ifdef WITH_DTRACE
#include <atalk/afp_dtrace.h>
#else
/* List of empty dtrace macros */
#define AFP_AFPFUNC_START(a,b)
#define AFP_AFPFUNC_DONE(a, b)
#define AFP_CNID_START(a)
#define AFP_CNID_DONE()
#define AFP_READ_START(a)
#define AFP_READ_DONE()
#define AFP_WRITE_START(a)
#define AFP_WRITE_DONE()
#endif

/* #define DOSFILELEN 12 */             /* Type1, DOS-compat*/
#define MACFILELEN 31                   /* Type2, HFS-compat */
#define UTF8FILELEN_EARLY 255           /* Type3, early Mac OS X 10.0-10.4.? */
/* #define UTF8FILELEN_NAME_MAX 765 */  /* Type3, 10.4.?- , getconf NAME_MAX */
/* #define UTF8FILELEN_SPEC 0xFFFF */   /* Type3, spec on document */
/* #define HFSPLUSFILELEN 510 */        /* HFS+ spec, 510byte = 255codepoint */

#define MAXUSERLEN 256

#define DEFAULT_MAX_DIRCACHE_SIZE 8192

#define OPTION_DEBUG         (1 << 0)
#define OPTION_CLOSEVOL      (1 << 1)
#define OPTION_SERVERNOTIF   (1 << 2)
#define OPTION_NOSENDFILE    (1 << 3)
#define OPTION_VETOMSG       (1 << 4) /* whether to send an AFP message for veto file access */
#define OPTION_AFP_READ_LOCK (1 << 5) /* whether to do AFP spec conforming read locks (default: no) */
#define OPTION_ANNOUNCESSH   (1 << 6)
#define OPTION_UUID          (1 << 7)
#define OPTION_ACL2MACCESS   (1 << 8)
#define OPTION_NOZEROCONF    (1 << 9)
#define OPTION_ACL2MODE      (1 << 10)
#define OPTION_SHARE_RESERV  (1 << 11) /* whether to use Solaris fcntl F_SHARE locks */
#define OPTION_DBUS_AFPSTATS (1 << 12) /* whether to run dbus thread for afpstats */

#define PASSWD_NONE     0
#define PASSWD_SET     (1 << 0)
#define PASSWD_NOSAVE  (1 << 1)
#define PASSWD_ALL     (PASSWD_SET | PASSWD_NOSAVE)

#define IS_AFP_SESSION(obj) ((obj)->dsi && (obj)->dsi->serversock == -1)

/**********************************************************************************************
 * Ini config sections
 **********************************************************************************************/

#define INISEC_GLOBAL "Global"
#define INISEC_HOMES  "Homes"

struct DSI;
#define AFPOBJ_TMPSIZ (MAXPATHLEN)

struct afp_volume_name {
    time_t     mtime;
    int        loaded;
};

struct afp_options {
    int connections;            /* Maximum number of possible AFP connections */
    int tickleval;
    int timeout;
    int flags;
    int dircachesize;
    int sleep;                  /* Maximum time allowed to sleep (in tickles) */
    int disconnected;           /* Maximum time in disconnected state (in tickles) */
    int fce_fmodwait;           /* number of seconds FCE file mod events are put on hold */
    unsigned int tcp_sndbuf, tcp_rcvbuf;
    unsigned char passwdbits, passwdminlen;
    uint32_t server_quantum;
    int dsireadbuf; /* scale factor for sizefof(dsi->buffer) = server_quantum * dsireadbuf */
    char *hostname;
    char *listen, *interfaces, *port;
    char *Cnid_srv, *Cnid_port;
    char *configfile;
    char *uampath, *fqdn;
    char *sigconffile;
    char *uuidconf;
    char *guest, *loginmesg, *keyfile, *passwdfile, *extmapfile;
    char *uamlist;
    char *signatureopt;
    unsigned char signature[16];
    char *k5service, *k5realm, *k5keytab;
    char *unixcodepage, *maccodepage, *volcodepage;
    charset_t maccharset, unixcharset; 
    mode_t umask;
    mode_t save_mask;
    gid_t admingid;
    int    volnamelen;
    /* default value for winbind authentication */
    char *ntdomain, *ntseparator, *addomain;
    char *logconfig;
    char *logfile;
    char *mimicmodel;
    char *adminauthuser;
    char *ignored_attr;
    struct afp_volume_name volfile;
};

typedef struct AFPObj {
    const char *cmdlineconfigfile;
    int cmdlineflags;
    const void *signature;
    struct DSI *dsi;
    struct afp_options options;
    dictionary *iniconfig;
    char username[MAXUSERLEN];
    /* to prevent confusion, only use these in afp_* calls */
    char oldtmp[AFPOBJ_TMPSIZ + 1], newtmp[AFPOBJ_TMPSIZ + 1];
    void *uam_cookie; /* cookie for uams */
    struct session_info  sinfo;
    uid_t uid; 	/* client running user id */
    int ipc_fd; /* anonymous PF_UNIX socket for IPC with afpd parent */
    gid_t *groups;
    int ngroups;
    int afp_version;
    /* Functions */
    void (*logout)(void);
    void (*exit)(int);
    int (*reply)(void *, int);
    int (*attention)(void *, AFPUserBytes);
} AFPObj;

/* typedef for AFP functions handlers */
typedef int (*AFPCmd)(AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);

/* Global variables */
extern AFPObj             *AFPobj;
extern int                afp_version;
extern int                afp_errno;
extern unsigned char      nologin;
extern struct dir         *curdir;
extern char               getwdbuf[];
extern struct afp_options default_options;
extern const char         *Cnid_srv;
extern const char         *Cnid_port;

extern int  get_afp_errno   (const int param);
extern void afp_options_init (struct afp_options *);
extern void afp_options_parse_cmdline(AFPObj *obj, int ac, char **av);
extern int setmessage (const char *);
extern void readmessage (AFPObj *);

/* afp_util.c */
extern const char *AfpNum2name (int );
extern const char *AfpErr2name(int err);

/* directory.c */
extern struct dir rootParent;

extern void afp_over_dsi (AFPObj *);
extern void afp_over_dsi_sighandlers(AFPObj *obj);
#endif /* globals.h */
