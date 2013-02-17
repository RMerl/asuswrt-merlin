/*
 * support/include/ha-callout.h
 *
 * High Availability NFS Callout support routines
 *
 * Copyright (c) 2004, Paul Clements, SteelEye Technology
 *
 * In order to implement HA NFS, we need several callouts at key
 * points in statd and mountd. These callouts all come to ha_callout(),
 * which, in turn, calls out to an ha-callout script (not part of nfs-utils;
 * defined by -H argument to rpc.statd and rpc.mountd).
 */
#ifndef HA_CALLOUT_H
#define HA_CALLOUT_H

#include <sys/wait.h>
#include <signal.h>

extern char *ha_callout_prog;

static inline void
ha_callout(char *event, char *arg1, char *arg2, int arg3)
{
	char buf[16]; /* should be plenty */
	pid_t pid;
	int ret = -1;
	struct sigaction oldact, newact;

	if (!ha_callout_prog) /* HA callout is not enabled */
		return;

	sprintf(buf, "%d", arg3);

	/* many daemons ignore SIGCHLD as tcpwrappers will
	 * fork a child to do logging.  We need to wait
	 * for a child here, so we need to un-ignore
	 * SIGCHLD temporarily
	 */
	newact.sa_handler = SIG_DFL;
	newact.sa_flags = 0;
	sigemptyset(&newact.sa_mask);
	sigaction(SIGCHLD, &newact, &oldact);
	pid = fork();
	switch (pid) {
		case 0: execl(ha_callout_prog, ha_callout_prog,
				event, arg1, arg2, 
			      arg3 < 0 ? NULL : buf,
			      NULL);
			perror("execl");
			exit(2);
		case -1: perror("fork");
			break;
		default: pid = waitpid(pid, &ret, 0);
  	}
	sigaction(SIGCHLD, &oldact, &newact);
#ifdef dprintf
	dprintf(N_DEBUG, "ha callout returned %d\n", WEXITSTATUS(ret));
#else
	xlog(D_GENERAL, "ha callout returned %d\n", WEXITSTATUS(ret));
#endif
}

#endif
