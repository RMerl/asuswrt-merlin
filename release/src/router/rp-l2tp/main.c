/***********************************************************************
*
* main.c
*
* Main program of l2tp
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: main.c 4262 2012-05-24 15:41:48Z themiron.ru $";

#include "l2tp.h"
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>

static void
usage(int argc, char *argv[], int exitcode)
{
    fprintf(stderr, "\nl2tpd Version %s Copyright 2002 Roaring Penguin Software Inc.\n", VERSION);
    fprintf(stderr, "http://www.roaringpenguin.com/\n\n");
    fprintf(stderr, "Usage: %s [options]\n", argv[0]);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "-d level               -- Set debugging to 'level'\n");
    fprintf(stderr, "-c file                -- Set config file\n");
    fprintf(stderr, "-p                     -- Set pid file\n");
    fprintf(stderr, "-f                     -- Do not fork\n");
    fprintf(stderr, "-h                     -- Print usage\n");
    fprintf(stderr, "\nThis program is licensed under the terms of\nthe GNU General Public License, Version 2.\n");
    exit(exitcode);
}

static void
sighandler(int signum)
{
    static int count = 0;

    count++;
    fprintf(stderr, "Caught signal %d times\n", count);
    if (count < 5) {
	l2tp_cleanup();
    }
    exit(EXIT_FAILURE);
}

static void
pidfile_cleanup(void *data)
{
    char *fname = (char *) data;

    unlink(fname);
}

static int
pidfile_check(char const *fname)
{
    FILE *fp;
    pid_t pid;

    fp = fopen(fname, "r");
    if (fp) {
	/* Check if the previous server process is still running */
	if (fscanf(fp, "%d", &pid) == 1 &&
	    pid && pid != getpid() && kill(pid, 0) == 0) {
	    l2tp_set_errmsg("pidfile_check: there's already a l2tpd running.");
	    fclose(fp);
	    return -1;
	}
	fclose(fp);
    }

    return 0;
}

static void
pidfile_init(char const *fname)
{
    FILE *fp;

    unlink(fname);
    fp = fopen(fname, "w");
    if (!fp) {
	l2tp_set_errmsg("pidfile_init: can't create pid file '%s'", fname);
	return;
    }

    fprintf(fp, "%d\n", getpid());
    fclose(fp);

    l2tp_register_shutdown_handler(pidfile_cleanup, (void *) fname);
}

int
main(int argc, char *argv[])
{
    EventSelector *es = Event_CreateSelector();
    int i;
    int opt;
    int do_fork = 1;
    int debugmask = 0;
    /* ASUS char *config_file = SYSCONFDIR"/l2tp/l2tp.conf"; */
    char *config_file = SYSCONFDIR"/l2tp.conf";
    char *pidfile = "/var/run/l2tpd.pid";

    while((opt = getopt(argc, argv, "d:c:p:fh")) != -1) {
	switch(opt) {
	case 'h':
	    usage(argc, argv, EXIT_SUCCESS);
	    break;
	case 'f':
	    do_fork = 0;
	    break;
	case 'p':
	    pidfile = optarg;
	    break;
	case 'd':
	    sscanf(optarg, "%d", &debugmask);
	    break;
        case 'c':
            config_file = optarg;
            break;
	default:
	    usage(argc, argv, EXIT_FAILURE);
	}
    }

    openlog(argv[0], LOG_PID, LOG_DAEMON);
    l2tp_random_init();
    l2tp_tunnel_init(es);
    l2tp_peer_init();
    l2tp_debug_set_bitmask(debugmask);

    if (pidfile_check(pidfile) < 0) {
	l2tp_die();
    }

    if (l2tp_parse_config_file(es, config_file) < 0) {
	l2tp_die();
    }

    if (l2tp_network_init(es) < 0) {
	l2tp_die();
    }

    /* Daemonize */
    if (do_fork) {
	i = fork();
	if (i < 0) {
	    perror("fork");
	    exit(EXIT_FAILURE);
	} else if (i != 0) {
	    /* Parent */
	    exit(EXIT_SUCCESS);
	}

	setsid();
	signal(SIGHUP, SIG_IGN);
	i = fork();
	if (i < 0) {
	    perror("fork");
	    exit(EXIT_FAILURE);
	} else if (i != 0) {
	    exit(EXIT_SUCCESS);
	}

	chdir("/");

	/* Point stdin/stdout/stderr to /dev/null */
	for (i=0; i<3; i++) {
	    close(i);
	}
	i = open("/dev/null", O_RDWR);
	if (i >= 0) {
	    dup2(i, 0);
	    dup2(i, 1);
	    dup2(i, 2);
	    if (i > 2) close(i);
	}
    }

    pidfile_init(pidfile);

    Event_HandleSignal(es, SIGINT, sighandler);
    Event_HandleSignal(es, SIGTERM, sighandler);

    while(1) {
	i = Event_HandleEvent(es);
	if (i < 0) {
	    fprintf(stderr, "Event_HandleEvent returned %d\n", i);
	    l2tp_cleanup();
	    exit(EXIT_FAILURE);
	}
    }
    return 0;
}
