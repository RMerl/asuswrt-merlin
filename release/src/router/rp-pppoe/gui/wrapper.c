/* -*-Mode: C;-*- */

/***********************************************************************
*
* wrapper.c
*
* C wrapper designed to run SUID root for controlling PPPoE connections.
*
* Copyright (C) 2005 by Roaring Penguin Software Inc.
*
* LIC: GPL
*
* This program may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
***********************************************************************/

#define _SVID_SOURCE 1 /* For putenv */
#define _POSIX_SOURCE 1 /* For fileno */
#define _BSD_SOURCE 1 /* For setreuid */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONN_NAME_LEN 64
#define LINELEN 512

static char const *pppoe_start = PPPOE_START_PATH;
static char const *pppoe_stop = PPPOE_STOP_PATH;
static char const *pppoe_status = PPPOE_STATUS_PATH;

/**********************************************************************
 *%FUNCTION: PathOK
 *%ARGUMENTS:
 * fname -- a file name.
 *%RETURNS:
 * 1 if path to fname is secure; 0 otherwise.
 *%DESCRIPTION:
 * Makes sure ownership/permissions of file and parent directories
 * are safe.
 **********************************************************************/
static int
PathOK(char const *fname)
{
    char path[LINELEN];
    struct stat buf;
    char const *slash;

    if (strlen(fname) > LINELEN) {
	fprintf(stderr, "Pathname '%s' too long\n", fname);
	return 0;
    }

    /* Must be absolute path */
    if (*fname != '/') {
	fprintf(stderr, "Unsafe path '%s' not absolute\n", fname);
	return 0;
    }

    /* Check root directory */
    if (stat("/", &buf) < 0) {
	perror("stat");
	return 0;
    }
    if (buf.st_uid) {
	fprintf(stderr, "SECURITY ALERT: Root directory (/) not owned by root\n");
	return 0;
    }
    if (buf.st_mode & (S_IWGRP | S_IWOTH)) {
	fprintf(stderr, "SECURITY ALERT: Root directory (/) writable by group or other\n");
	return 0;
    }

    /* Check each component */
    slash = fname;

    while(*slash) {
	slash = strchr(slash+1, '/');
	if (!slash) {
	    slash = fname + strlen(fname);
	}
	memcpy(path, fname, slash-fname);
	path[slash-fname] = 0;
	if (stat(path, &buf) < 0) {
	    perror("stat");
	    return 0;
	}
	if (buf.st_uid) {
	    fprintf(stderr, "SECURITY ALERT: '%s' not owned by root\n", path);
	    return 0;
	}

	if (buf.st_mode & (S_IWGRP | S_IWOTH)) {
	    fprintf(stderr, "SECURITY ALERT: '%s' writable by group or other\n",
		    path);
	    return 0;
	}
    }
    return 1;
}

/**********************************************************************
 *%FUNCTION: CleanEnvironment
 *%ARGUMENTS:
 * envp -- environment passed to main
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Deletes all environment variables; makes safe environment
 **********************************************************************/
static void
CleanEnvironment(char *envp[])
{
    envp[0] = NULL;
    putenv("PATH=/bin:/usr/bin:/sbin:/usr/sbin");
}

/**********************************************************************
 *%FUNCTION: main
 *%ARGUMENTS:
 * argc, argv -- usual suspects
 * Usage: pppoe-wrapper {start|stop|status} {connection_name}
 *%RETURNS:
 * Whatever pppoe-start, pppoe-stop or pppoe-status returns.
 *%DESCRIPTION:
 * Runs pppoe-start, pppoe-stop or pppoe-status on given connection if
 * non-root users are allowed to do it.
 **********************************************************************/
int
main(int argc, char *argv[])
{
    int amRoot;
    char *cp;
    char fname[64+CONN_NAME_LEN];
    char line[LINELEN+1];
    int allowed = 0;

    FILE *fp;

    extern char **environ;

    /* Clean out environment */
    CleanEnvironment(environ);

    /* Are we root? */
    amRoot = (getuid() == 0);

    /* Validate arguments */
    if (argc != 3) {
	fprintf(stderr, "Usage: %s {start|stop|status} connection_name\n",
		argv[0]);
	exit(1);
    }

    if (strcmp(argv[1], "start") &&
	strcmp(argv[1], "stop") &&
	strcmp(argv[1], "status")) {
	fprintf(stderr, "Usage: %s {start|stop|status} connection_name\n",
		argv[0]);
	exit(1);
    }

    /* Connection name can be at most CONN_NAME_LEN chars; alpha, num, underscore */
    if (strlen(argv[2]) > CONN_NAME_LEN) {
	fprintf(stderr, "%s: Connection name '%s' too long.\n",
		argv[0], argv[2]);
	exit(1);
    }

    for (cp = argv[2]; *cp; cp++) {
	if (!strchr("abcdefghijklmnopqrstuvwxyz"
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		    "0123456789_-", *cp)) {
	    fprintf(stderr, "%s: Connection name '%s' contains illegal character '%c'\n", argv[0], argv[2], *cp);
	    exit(1);
	}
    }

    /* Open the connection file */
    sprintf(fname, "/etc/ppp/rp-pppoe-gui/conf.%s", argv[2]);
    /* Check path sanity */
    if (!PathOK(fname)) {
	exit(1);
    }

    fp = fopen(fname, "r");
    if (!fp) {
	fprintf(stderr, "%s: Could not open '%s': %s\n",
		argv[0], fname, strerror(errno));
	exit(1);
    }

    /* Check if non-root users can control it */
    if (amRoot) {
	allowed = 1;
    } else {
	while (!feof(fp)) {
	    if (!fgets(line, LINELEN, fp)) {
		break;
	    }
	    if (!strcmp(line, "NONROOT=OK\n")) {
		allowed = 1;
		break;
	    }
	}
    }
    fclose(fp);

    if (!allowed) {
	fprintf(stderr, "%s: Non-root users are not permitted to control connection '%s'\n", argv[0], argv[2]);
	exit(1);
    }

    /* Become root with setuid() to defeat is-root checks in shell scripts */
    if (setreuid(0, 0) < 0) {
	perror("setreuid");
	exit(1);
    }

    /* It's OK -- do it.  */
    if (!strcmp(argv[1], "start")) {
	if (!PathOK(pppoe_start)) exit(1);
	execl(pppoe_start, "pppoe-start", fname, NULL);
    } else if (!strcmp(argv[1], "stop")) {
	if (!PathOK(pppoe_stop)) exit(1);
	execl(pppoe_stop, "pppoe-stop", fname, NULL);
    } else {
	if (!PathOK(pppoe_status)) exit(1);
	execl(pppoe_status, "pppoe-status", fname, NULL);
    }
    fprintf(stderr, "%s: execl: %s\n", argv[0], strerror(errno));
    exit(1);
}
