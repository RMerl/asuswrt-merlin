/* Process handling
 *
 * Copyright © 2006, Thomas Bernard
 * Copyright © 2013, Benoît Knecht <benoit.knecht@fsfe.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#include "upnpglobalvars.h"
#include "process.h"
#include "config.h"
#include "log.h"

struct child *children = NULL;
int number_of_children = 0;

static void
add_process_info(pid_t pid, struct client_cache_s *client)
{
	struct child *child;
	int i;

	for (i = 0; i < runtime_vars.max_connections; i++)
	{
		child = children+i;
		if (child->pid)
			continue;
		child->pid = pid;
		child->client = client;
		child->age = time(NULL);
		break;
	}
}

static inline void
remove_process_info(pid_t pid)
{
	struct child *child;
	int i;

	for (i = 0; i < runtime_vars.max_connections; i++)
	{
		child = children+i;
		if (child->pid != pid)
			continue;
		child->pid = 0;
		if (child->client)
			child->client->connections--;
		break;
	}
}

pid_t
process_fork(struct client_cache_s *client)
{
	if (number_of_children >= runtime_vars.max_connections)
	{
		DPRINTF(E_WARN, L_GENERAL, "Exceeded max connections [%d], not forking\n",
			runtime_vars.max_connections);
		errno = EAGAIN;
		return -1;
	}

	pid_t pid = fork();
	if (pid > 0)
	{
		if (client)
			client->connections++;
		add_process_info(pid, client);
		number_of_children++;
	}

	return pid;
}

void
process_handle_child_termination(int signal)
{
	pid_t pid;

	while ((pid = waitpid(-1, NULL, WNOHANG)))
	{
		if (pid == -1)
		{
			if (errno == EINTR)
				continue;
			else
				break;
		}
		number_of_children--;
		remove_process_info(pid);
	}
}

int
process_daemonize(void)
{
	int pid;
#ifndef USE_DAEMON
	int i;

	switch(fork())
	{
		/* fork error */
		case -1:
			perror("fork()");
			exit(1);

		/* child process */
		case 0:
		/* obtain a new process group */
			if( (pid = setsid()) < 0)
			{
				perror("setsid()");
				exit(1);
			}

			/* close all descriptors */
			for (i=getdtablesize();i>=0;--i) close(i);		

			i = open("/dev/null",O_RDWR); /* open stdin */
			dup(i); /* stdout */
			dup(i); /* stderr */

			umask(027);
			chdir("/");

			break;
		/* parent process */
		default:
			exit(0);
	}
#else
	if( daemon(0, 0) < 0 )
		perror("daemon()");
	pid = getpid();
#endif
	return pid;
}

int
process_check_if_running(const char *fname)
{
	char buffer[64];
	int pidfile;
	pid_t pid;

	if(!fname || *fname == '\0')
		return -1;

	if( (pidfile = open(fname, O_RDONLY)) < 0)
		return 0;

	memset(buffer, 0, 64);

	if(read(pidfile, buffer, 63) > 0)
	{
		if( (pid = atol(buffer)) > 0)
		{
			if(!kill(pid, 0))
			{
				close(pidfile);
				return -2;
			}
		}
	}

	close(pidfile);

	return 0;
}

void
process_reap_children(void)
{
	struct child *child;
	int i;

	for (i = 0; i < runtime_vars.max_connections; i++)
	{
		child = children+i;
		if (child->pid)
			kill(child->pid, SIGKILL);
	}
}
