/*
 * passprompt.c - pppd plugin to invoke an external PAP password prompter
 *
 * Copyright 1999 Paul Mackerras, Alan Curry.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <syslog.h>
#include "pppd.h"

char pppd_version[] = VERSION;

static char promptprog[PATH_MAX+1];

static option_t options[] = {
    { "promptprog", o_string, promptprog,
      "External PAP password prompting program",
      OPT_STATIC, NULL, PATH_MAX },
    { NULL }
};

static int promptpass(char *user, char *passwd)
{
    int p[2];
    pid_t kid;
    int readgood, wstat;
    size_t red;

    if (promptprog[0] == 0 || access(promptprog, X_OK) < 0)
	return -1;	/* sorry, can't help */

    if (!passwd)
	return 1;

    if (pipe(p)) {
	warn("Can't make a pipe for %s", promptprog);
	return 0;
    }
    if ((kid = fork()) == (pid_t) -1) {
	warn("Can't fork to run %s", promptprog);
	close(p[0]);
	close(p[1]);
	return 0;
    }
    if (!kid) {
	/* we are the child, exec the program */
	char *argv[4], fdstr[32];
	sys_close();
	closelog();
	close(p[0]);
	seteuid(getuid());
	setegid(getgid());
	argv[0] = promptprog;
	argv[1] = user;
	argv[2] = remote_name;
	sprintf(fdstr, "%d", p[1]);
	argv[3] = fdstr;
	argv[4] = 0;
	execv(*argv, argv);
	_exit(127);
    }

    /* we are the parent, read the password from the pipe */
    close(p[1]);
    readgood = 0;
    do {
	red = read(p[0], passwd + readgood, MAXSECRETLEN-1 - readgood);
	if (red == 0)
	    break;
	if (red < 0) {
	    error("Can't read secret from %s: %m", promptprog);
	    readgood = -1;
	    break;
	}
	readgood += red;
    } while (readgood < MAXSECRETLEN - 1);
    passwd[readgood] = 0;
    close(p[0]);

    /* now wait for child to exit */
    while (waitpid(kid, &wstat, 0) < 0) {
	if (errno != EINTR) {
	    warn("error waiting for %s: %m", promptprog);
	    break;
	}
    }

    if (readgood < 0)
	return 0;
    if (!WIFEXITED(wstat))
	warn("%s terminated abnormally", promptprog);
    if (WEXITSTATUS(wstat))
	warn("%s exited with code %d", promptprog, WEXITSTATUS(status));

    return 1;
}

void plugin_init(void)
{
    add_options(options);
    pap_passwd_hook = promptpass;
}
