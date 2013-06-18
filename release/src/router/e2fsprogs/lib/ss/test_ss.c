/*
 * test_ss.c
 *
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose is hereby granted, provided that
 * the names of M.I.T. and the M.I.T. S.I.P.B. not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  M.I.T. and the
 * M.I.T. S.I.P.B. make no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without
 * express or implied warranty.

 */

#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <string.h>
#include "ss.h"

extern ss_request_table test_cmds;

#define TRUE 1
#define FALSE 0

static char subsystem_name[] = "test_ss";
static char version[] = "1.0";

static int source_file(const char *cmd_file, int sci_idx)
{
	FILE		*f;
	char		buf[256];
	char		*cp;
	int		exit_status = 0;
	int		retval;
	int 		noecho;

	if (strcmp(cmd_file, "-") == 0)
		f = stdin;
	else {
		f = fopen(cmd_file, "r");
		if (!f) {
			perror(cmd_file);
			exit(1);
		}
	}
	fflush(stdout);
	fflush(stderr);
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	while (!feof(f)) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			break;
		if (buf[0] == '#')
			continue;
		noecho = 0;
		if (buf[0] == '-') {
			noecho = 1;
			buf[0] = ' ';
		}
		cp = strchr(buf, '\n');
		if (cp)
			*cp = 0;
		cp = strchr(buf, '\r');
		if (cp)
			*cp = 0;
		if (!noecho)
			printf("test_icount: %s\n", buf);
		retval = ss_execute_line(sci_idx, buf);
		if (retval) {
			ss_perror(sci_idx, retval, buf);
			exit_status++;
		}
	}
	return exit_status;
}

int main(int argc, char **argv)
{
	int c, code;
	char *request = (char *)NULL;
	char		*cmd_file = 0;
	int sci_idx;
	int exit_status = 0;

	while ((c = getopt (argc, argv, "wR:f:")) != EOF) {
		switch (c) {
		case 'R':
			request = optarg;
			break;
		case 'f':
			cmd_file = optarg;
			break;
		default:
			com_err(argv[0], 0, "Usage: test_ss [-R request] "
				"[-f cmd_file]");
			exit(1);
		}
	}

	sci_idx = ss_create_invocation(subsystem_name, version,
				       (char *)NULL, &test_cmds, &code);
	if (code) {
		ss_perror(sci_idx, code, "creating invocation");
		exit(1);
	}

	(void) ss_add_request_table (sci_idx, &ss_std_requests, 1, &code);
	if (code) {
		ss_perror (sci_idx, code, "adding standard requests");
		exit (1);
	}

	printf("test_ss %s.  Type '?' for a list of commands.\n\n",
	       version);

	if (request) {
		code = ss_execute_line(sci_idx, request);
		if (code) {
			ss_perror(sci_idx, code, request);
			exit_status++;
		}
	} else if (cmd_file) {
		exit_status = source_file(cmd_file, sci_idx);
	} else {
		ss_listen(sci_idx);
	}

	exit(exit_status);
}


void test_cmd (argc, argv)
    int argc;
    char **argv;
{
    printf("Hello, world!\n");
    printf("Args: ");
    while (++argv, --argc) {
	printf("'%s'", *argv);
	if (argc > 1)
	    fputs(", ", stdout);
    }
    putchar ('\n');
}
