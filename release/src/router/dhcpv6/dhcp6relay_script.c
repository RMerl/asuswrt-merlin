/*from	$KAME: dhcp6c_script.c,v 1.11 2004/11/28 10:48:38 jinmei Exp $	*/

/*
 * Copyright (C) 2003 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/stat.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <netinet/in.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "dhcp6.h"
#include "config.h"
#include "common.h"

static char client_str[] = "client";
static char buf[BUFSIZ];

static char *iapd2str __P((int, struct dhcp6_listval *));
static char *iana2str __P((int, struct dhcp6_listval *));

int
relay6_script(scriptpath, client, dh6, len)
	char *scriptpath;
	struct sockaddr_in6 *client;
	struct dhcp6 *dh6;
	int len;
{
	struct dhcp6_optinfo optinfo;
	struct dhcp6opt *optend;
	int i, j, iapds, ianas, envc, elen, ret = 0;
	char **envp, *s, *t;
	struct dhcp6_listval *v;
	pid_t pid, wpid;

	/* if a script is not specified, do nothing */
	if (scriptpath == NULL || strlen(scriptpath) == 0)
		return -1;

	/* only replies are interesting */
	if (dh6->dh6_msgtype != DH6_REPLY) {
		if (dh6->dh6_msgtype != DH6_ADVERTISE) {
			dprintf(LOG_INFO, FNAME, "forward msg#%d to client?",
			    dh6->dh6_msgtype);
			return -1;
		}
		return 0;
	}

	/* parse options */
	optend = (struct dhcp6opt *)((caddr_t) dh6 + len);
	dhcp6_init_options(&optinfo);
	if (dhcp6_get_options((struct dhcp6opt *)(dh6 + 1), optend,
	    &optinfo) < 0) {
		dprintf(LOG_INFO, FNAME, "failed to parse options");
		return -1;
	}

	/* initialize counters */
	iapds = 0;
	ianas = 0;
	envc = 2;     /* we at least include the address and the terminator */

	/* count the number of variables */
	for (v = TAILQ_FIRST(&optinfo.iapd_list); v; v = TAILQ_NEXT(v, link))
		iapds++;
	envc += iapds;
	for (v = TAILQ_FIRST(&optinfo.iana_list); v; v = TAILQ_NEXT(v, link))
		ianas++;
	envc += ianas;

	/* allocate an environments array */
	if ((envp = malloc(sizeof (char *) * envc)) == NULL) {
		dprintf(LOG_NOTICE, FNAME,
		    "failed to allocate environment buffer");
		dhcp6_clear_options(&optinfo);
		return -1;
	}
	memset(envp, 0, sizeof (char *) * envc);

	/*
	 * Copy the parameters as environment variables
	 */
	i = 0;
	/* address */
	t = addr2str((struct sockaddr *) client);
	if (t == NULL) {
		dprintf(LOG_NOTICE, FNAME,
		    "failed to get address of client");
		ret = -1;
		goto clean;
	}
	elen = sizeof (client_str) + 1 + strlen(t) + 1;
	if ((s = envp[i++] = malloc(elen)) == NULL) {
		dprintf(LOG_NOTICE, FNAME,
		    "failed to allocate string for client");
		ret = -1;
		goto clean;
	}
	memset(s, 0, elen);
	snprintf(s, elen, "%s=%s", client_str, t);
	/* IAs */
	j = 0;
	for (v = TAILQ_FIRST(&optinfo.iapd_list); v;
	    v = TAILQ_NEXT(v, link)) {
		if ((s = envp[i++] = iapd2str(j++, v)) == NULL) {
			ret = -1;
			goto clean;
		}
	}
	j = 0;
	for (v = TAILQ_FIRST(&optinfo.iana_list); v;
	    v = TAILQ_NEXT(v, link)) {
		if ((s = envp[i++] = iana2str(j++, v)) == NULL) {
			ret = -1;
			goto clean;
		}
	}

	/* launch the script */
	pid = fork();
	if (pid < 0) {
		dprintf(LOG_ERR, FNAME, "failed to fork: %s", strerror(errno));
		ret = -1;
		goto clean;
	} else if (pid) {
		int wstatus;

		do {
			wpid = wait(&wstatus);
		} while (wpid != pid && wpid > 0);

		if (wpid < 0)
			dprintf(LOG_ERR, FNAME, "wait: %s", strerror(errno));
		else {
			dprintf(LOG_DEBUG, FNAME,
			    "script \"%s\" terminated", scriptpath);
		}
	} else {
		char *argv[2];
		int fd;

		argv[0] = scriptpath;
		argv[1] = NULL;

		if (safefile(scriptpath)) {
			dprintf(LOG_ERR, FNAME,
			    "script \"%s\" cannot be executed safely",
			    scriptpath);
			exit(1);
		}

		if (foreground == 0 &&
		    (fd = open("/dev/null", O_RDWR)) != -1) {
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO)
				close(fd);
		}

		execve(scriptpath, argv, envp);

		dprintf(LOG_ERR, FNAME, "child: exec failed: %s",
		    strerror(errno));
		exit(0);
	}

  clean:
	for (i = 0; i < envc; i++)
		free(envp[i]);
	free(envp);
	dhcp6_clear_options(&optinfo);

	return ret;
}

static char *
iapd2str(num, iav)
	int num;
	struct dhcp6_listval *iav;
{
	struct dhcp6_listval *siav;
	char *s, *r, *comma;

	s = buf;
	memset(s, 0, BUFSIZ);

	snprintf(s, BUFSIZ, "iapd_%d=", num);
	comma = "";

	for (siav = TAILQ_FIRST(&iav->sublist); siav;
	    siav = TAILQ_NEXT(siav, link)) {
		switch (siav->type) {
		case DHCP6_LISTVAL_PREFIX6:
			snprintf(s + strlen(s), BUFSIZ - strlen(s),
			    "%s%s/%d", comma,
			    in6addr2str(&siav->val_prefix6.addr, 0),
			    siav->val_prefix6.plen);
			comma = ",";
			break;

		case DHCP6_LISTVAL_STCODE:
			snprintf(s + strlen(s), BUFSIZ - strlen(s),
			    "%s#%d", comma, siav->val_num16);
			comma = ",";
			break;

		default:
			dprintf(LOG_ERR, FNAME, "impossible subopt");
		}
	}

	if ((r = strdup(s)) == NULL)
		dprintf(LOG_ERR, FNAME, "failed to allocate iapd_%d", num);
	return r;
}

static char *
iana2str(num, iav)
	int num;
	struct dhcp6_listval *iav;
{
	struct dhcp6_listval *siav;
	char *s, *r, *comma;

	s = buf;
	memset(s, 0, BUFSIZ);

	snprintf(s, BUFSIZ, "iana_%d=", num);
	comma = "";

	for (siav = TAILQ_FIRST(&iav->sublist); siav;
	    siav = TAILQ_NEXT(siav, link)) {
		switch (siav->type) {
		case DHCP6_LISTVAL_STATEFULADDR6:
			snprintf(s + strlen(s), BUFSIZ - strlen(s),
			    "%s%s", comma,
			    in6addr2str(&siav->val_statefuladdr6.addr, 0));
			comma = ",";
			break;

		case DHCP6_LISTVAL_STCODE:
			snprintf(s + strlen(s), BUFSIZ - strlen(s),
			    "%s#%d", comma, siav->val_num16);
			comma = ",";
			break;

		default:
			dprintf(LOG_ERR, FNAME, "impossible subopt");
		}
	}

	if ((r = strdup(s)) == NULL)
		dprintf(LOG_ERR, FNAME, "failed to allocate iana_%d", num);
	return r;
}
