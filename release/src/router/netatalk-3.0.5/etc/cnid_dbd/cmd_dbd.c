/* 
   Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <atalk/logger.h>
#include <atalk/globals.h>
#include <atalk/netatalk_conf.h>
#include <atalk/util.h>
#include <atalk/errchk.h>

#include "cmd_dbd.h"

enum dbd_cmd {dbd_scan, dbd_rebuild};

/* Global variables */
volatile sig_atomic_t alarmed;  /* flags for signals */

/* Local variables */
static dbd_flags_t flags;

/***************************************************************************
 * Local functions
 ***************************************************************************/

/*
 * SIGNAL handling:
 * catch SIGINT and SIGTERM which cause clean exit. Ignore anything else.
 */
static void sig_handler(int signo)
{
    alarmed = 1;
    return;
}

static void set_signal(void)
{
    struct sigaction sv;

    sv.sa_handler = sig_handler;
    sv.sa_flags = SA_RESTART;
    sigemptyset(&sv.sa_mask);
    if (sigaction(SIGTERM, &sv, NULL) < 0) {
        dbd_log( LOGSTD, "error in sigaction(SIGTERM): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }        
    if (sigaction(SIGINT, &sv, NULL) < 0) {
        dbd_log( LOGSTD, "error in sigaction(SIGINT): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }        

    memset(&sv, 0, sizeof(struct sigaction));
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask);

    if (sigaction(SIGABRT, &sv, NULL) < 0) {
        dbd_log( LOGSTD, "error in sigaction(SIGABRT): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }        
    if (sigaction(SIGHUP, &sv, NULL) < 0) {
        dbd_log( LOGSTD, "error in sigaction(SIGHUP): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }        
    if (sigaction(SIGQUIT, &sv, NULL) < 0) {
        dbd_log( LOGSTD, "error in sigaction(SIGQUIT): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }        
}

static void usage (void)
{
    printf("Usage: dbd [-cfFstvV] <path to netatalk volume>\n\n"
           "dbd scans all file and directories of AFP volumes, updating the\n"
           "CNID database of the volume. dbd must be run with appropiate\n"
           "permissions i.e. as root.\n\n"
           "Options:\n"
           "   -s scan volume: treat the volume as read only and don't\n"
           "      perform any filesystem modifications\n"
           "   -c convert from adouble:v2 to adouble:ea\n"
           "   -F location of the afp.conf config file\n"
           "   -f delete and recreate CNID database\n"
           "   -t show statistics while running\n"
           "   -v verbose\n"
           "   -V show version info\n\n"
        );
}

/***************************************************************************
 * Global functions
 ***************************************************************************/

void dbd_log(enum logtype lt, char *fmt, ...)
{
    int len;
    static char logbuffer[1024];
    va_list args;

    if ( (lt == LOGSTD) || (flags & DBD_FLAGS_VERBOSE)) {
        va_start(args, fmt);
        len = vsnprintf(logbuffer, 1023, fmt, args);
        va_end(args);
        logbuffer[1023] = 0;

        printf("%s\n", logbuffer);
    }
}

int main(int argc, char **argv)
{
    EC_INIT;
    int dbd_cmd = dbd_rebuild;
    int cdir = -1;
    AFPObj obj = { 0 };
    struct vol *vol = NULL;
    const char *volpath = NULL;

    int c;
    while ((c = getopt(argc, argv, ":cfF:rstvV")) != -1) {
        switch(c) {
        case 'c':
            flags |= DBD_FLAGS_V2TOEA;
            break;
        case 'f':
            flags |= DBD_FLAGS_FORCE;
            break;
        case 'F':
            obj.cmdlineconfigfile = strdup(optarg);
            break;
        case 'r':
            /* the default */
            break;
        case 's':
            dbd_cmd = dbd_scan;
            flags |= DBD_FLAGS_SCAN;
            break;
        case 't':
            flags |= DBD_FLAGS_STATS;
            break;
        case 'v':
            flags |= DBD_FLAGS_VERBOSE;
            break;
        case 'V':
            printf("dbd %s\n", VERSION);
            exit(0);
        case ':':
        case '?':
            usage();
            exit(EXIT_FAILURE);
            break;
        }
    }

    if ( (optind + 1) != argc ) {
        usage();
        exit(EXIT_FAILURE);
    }
    volpath = argv[optind];

    if (geteuid() != 0) {
        usage();
        exit(EXIT_FAILURE);
    }
    /* Inhereting perms in ad_mkdir etc requires this */
    ad_setfuid(0);

    setvbuf(stdout, (char *) NULL, _IONBF, 0);

    /* Remember cwd */
    if ((cdir = open(".", O_RDONLY)) < 0) {
        dbd_log( LOGSTD, "Can't open dir: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
        
    /* Setup signal handling */
    set_signal();

    /* Load config */
    if (afp_config_parse(&obj, "dbd") != 0) {
        dbd_log( LOGSTD, "Couldn't load afp.conf");
        exit(EXIT_FAILURE);
    }

    /* Initialize CNID subsystem */
    cnid_init();

    /* Setup logging. Should be portable among *NIXes */
    if (flags & DBD_FLAGS_VERBOSE)
        setuplog("default:note, cnid:debug", "/dev/tty");
    else
        setuplog("default:note", "/dev/tty");

    if (load_volumes(&obj) != 0) {
        dbd_log( LOGSTD, "Couldn't load volumes");
        exit(EXIT_FAILURE);
    }

    if ((vol = getvolbypath(&obj, volpath)) == NULL) {
        dbd_log( LOGSTD, "Couldn't find volume for '%s'", volpath);
        exit(EXIT_FAILURE);
    }

    if (load_charset(vol) != 0) {
        dbd_log( LOGSTD, "Couldn't load charsets for '%s'", volpath);
        exit(EXIT_FAILURE);
    }

    /* open volume */
    if (STRCMP(vol->v_cnidscheme, != , "dbd")) {
        dbd_log(LOGSTD, "\"%s\" isn't a \"dbd\" CNID volume", vol->v_path);
        exit(EXIT_FAILURE);
    }
    if ((vol->v_cdb = cnid_open(vol->v_path,
                                0000,
                                "dbd",
                                vol->v_flags & AFPVOL_NODEV ? CNID_FLAG_NODEV : 0,
                                vol->v_cnidserver,
                                vol->v_cnidport)) == NULL) {
        dbd_log(LOGSTD, "Cant initialize CNID database connection for %s", vol->v_path);
        exit(EXIT_FAILURE);
    }

    if (vol->v_adouble == AD_VERSION_EA)
        dbd_log( LOGDEBUG, "adouble:ea volume");
    else if (vol->v_adouble == AD_VERSION2)
        dbd_log( LOGDEBUG, "adouble:v2 volume");
    else {
        dbd_log( LOGSTD, "unknown adouble volume");
        exit(EXIT_FAILURE);
    }

    /* -C v2 to ea conversion only on adouble:ea volumes */
    if ((flags & DBD_FLAGS_V2TOEA) && (vol->v_adouble!= AD_VERSION_EA)) {
        dbd_log( LOGSTD, "Can't run adouble:v2 to adouble:ea conversion because not an adouble:ea volume");
        exit(EXIT_FAILURE);
    }

    /* Sanity checks to ensure we can touch this volume */
    if (vol->v_vfs_ea != AFPVOL_EA_AD && vol->v_vfs_ea != AFPVOL_EA_SYS) {
        dbd_log( LOGSTD, "Unknown Extended Attributes option: %u", vol->v_vfs_ea);
        exit(EXIT_FAILURE);        
    }

    if (flags & DBD_FLAGS_FORCE) {
        if (cnid_wipe(vol->v_cdb) != 0) {
            dbd_log( LOGSTD, "Failed to wipe CNID db");
            EC_FAIL;
        }
    }

    /* Now execute given command scan|rebuild|dump */
    switch (dbd_cmd) {
    case dbd_scan:
    case dbd_rebuild:
        if (cmd_dbd_scanvol(vol, flags) < 0) {
            dbd_log( LOGSTD, "Error repairing database.");
        }
        break;
    }

EC_CLEANUP:
    if (vol)
        cnid_close(vol->v_cdb);

    if (cdir != -1 && (fchdir(cdir) < 0))
        dbd_log(LOGSTD, "fchdir: %s", strerror(errno));

    if (ret == 0)
        exit(EXIT_SUCCESS);
    else
        exit(EXIT_FAILURE);
}
