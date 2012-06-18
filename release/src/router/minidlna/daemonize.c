/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 *
 * Copyright (c) 2006, Thomas Bernard
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

#include "daemonize.h"
#include "config.h"
#include "log.h"

int
daemonize(void)
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
writepidfile(const char * fname, int pid)
{
	char pidstring[16];
	int pidstringlen;
	int pidfile;

	if(!fname || (strlen(fname) == 0))
		return -1;
	
	if( (pidfile = open(fname, O_WRONLY|O_CREAT, 0644)) < 0)
	{
		DPRINTF(E_ERROR, L_GENERAL, "Unable to open pidfile for writing %s: %s\n", fname, strerror(errno));
		return -1;
	}

	pidstringlen = snprintf(pidstring, sizeof(pidstring), "%d\n", pid);
	if(pidstringlen <= 0)
	{
		DPRINTF(E_ERROR, L_GENERAL, 
			"Unable to write to pidfile %s: snprintf(): FAILED\n", fname);
		close(pidfile);
		return -1;
	}
	else
	{
		if(write(pidfile, pidstring, pidstringlen) < 0)
			DPRINTF(E_ERROR, L_GENERAL, "Unable to write to pidfile %s: %s\n", fname, strerror(errno));
	}

	close(pidfile);

	return 0;
}

int
checkforrunning(const char * fname)
{
	char buffer[64];
	int pidfile;
	pid_t pid;

	if(!fname || (strlen(fname) == 0))
		return -1;

	if( (pidfile = open(fname, O_RDONLY)) < 0)
		return 0;
	
	memset(buffer, 0, 64);
	
	if(read(pidfile, buffer, 63))
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

