/*	$KAME: dhcp6_ctl.c,v 1.4 2004/09/07 05:03:03 jinmei Exp $	*/

/*
 * Copyright (C) 2004 WIDE Project.
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
#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <netdb.h>
#include <errno.h>

#include <dhcp6.h>
#include <config.h>
#include <common.h>
#include <auth.h>
#include <base64.h>
#include <control.h>
#include <dhcp6_ctl.h>

TAILQ_HEAD(dhcp6_commandqueue, dhcp6_commandctx);

static struct dhcp6_commandqueue commandqueue_head;
static int max_commands;
static int commands = 0;

struct dhcp6_commandctx {
	TAILQ_ENTRY(dhcp6_commandctx) link;

	int s;			/* communication socket */
	char inputbuf[1024];	/* input buffer */
	ssize_t input_len;
	ssize_t input_filled;
	int (*callback) __P((char *, ssize_t));
};

int
dhcp6_ctl_init(addr, port, max, sockp)
	char *addr, *port;
	int max, *sockp;
{
	struct addrinfo hints, *res = NULL;
	int on;
	int error;
	int ctlsock = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo(addr, port, &hints, &res);
	if (error) {
		dprintf(LOG_ERR, FNAME, "getaddrinfo: %s",
		    gai_strerror(error));
		return (-1);
	}
	ctlsock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (ctlsock < 0) {
		dprintf(LOG_ERR, FNAME, "socket(control sock): %s",
		    strerror(errno));
		goto fail;
	}
	on = 1;
	if (setsockopt(ctlsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))
	    < 0) {
		dprintf(LOG_ERR, FNAME,
		    "setsockopt(control sock, SO_REUSEADDR: %s",
		    strerror(errno));
		goto fail;
	}
	if (bind(ctlsock, res->ai_addr, res->ai_addrlen) < 0) {
		dprintf(LOG_ERR, FNAME, "bind(control sock): %s",
		    strerror(errno));
		goto fail;
	}
	freeaddrinfo(res);
	if (listen(ctlsock, 1)) {
		dprintf(LOG_ERR, FNAME, "listen(control sock): %s",
		    strerror(errno));
		goto fail;
	}

	TAILQ_INIT(&commandqueue_head);

	if (max <= 0) {
		dprintf(LOG_ERR, FNAME,
		    "invalid maximum number of commands (%d)", max_commands);
		goto fail;
	}
	max_commands = max;

	*sockp = ctlsock;
	return (0);

  fail:
	if (res != NULL)
		freeaddrinfo(res);
	if  (ctlsock >= 0)
		close(ctlsock);

	return (-1);
}

int
dhcp6_ctl_authinit(keyfile, keyinfop, digestlenp)
	char *keyfile;
	struct keyinfo **keyinfop;
	int *digestlenp;
{
	FILE *fp = NULL;
	struct keyinfo *ctlkey = NULL;
	char line[1024], secret[1024];
	int secretlen;

	/* Currently, we only support HMAC-MD5 for authentication. */
	*digestlenp = MD5_DIGESTLENGTH;

	if ((fp = fopen(keyfile, "r")) == NULL) {
		dprintf(LOG_ERR, FNAME, "failed to open %s: %s", keyfile,
		    strerror(errno));
		return (-1);
	}
	if (fgets(line, sizeof(line), fp) == NULL && ferror(fp)) {
		dprintf(LOG_ERR, FNAME, "failed to read key file: %s",
		    strerror(errno));
		goto fail;
	}
	if ((secretlen = base64_decodestring(line, secret, sizeof(secret)))
	    < 0) {
		dprintf(LOG_ERR, FNAME, "failed to decode base64 string");
		goto fail;
	}
	if ((ctlkey = malloc(sizeof(*ctlkey))) == NULL) {
		dprintf(LOG_WARNING, FNAME, "failed to allocate control key");
		goto fail;
	}
	memset(ctlkey, 0, sizeof(*ctlkey));
	if ((ctlkey->secret = malloc(secretlen)) == NULL) {
		dprintf(LOG_WARNING, FNAME, "failed to allocate secret key");
		goto fail;
	}
	ctlkey->secretlen = (size_t)secretlen;
	memcpy(ctlkey->secret, secret, secretlen);

	fclose(fp);

	*keyinfop = ctlkey;
	return (0);

  fail:
	if (fp != NULL)
		fclose(fp);
	if (ctlkey != NULL && ctlkey->secret != NULL)
		free(ctlkey->secret);
	if (ctlkey != NULL)
		free(ctlkey);

	return (-1);
}

int
dhcp6_ctl_acceptcommand(sl, callback)
	int sl;
	int (*callback) __P((char *, ssize_t));
{
	int s;
	struct sockaddr_storage from_ss;
	struct sockaddr *from = (struct sockaddr *)&from_ss;
	socklen_t fromlen;
	struct dhcp6_commandctx *ctx, *new;

	fromlen = sizeof(from_ss);
	if ((s = accept(sl, from, &fromlen)) < 0) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to accept control connection: %s",
		    strerror(errno));
		return (-1);
	}

	dprintf(LOG_DEBUG, FNAME, "accept control connection from %s",
	    addr2str(from));

	if (max_commands <= 0) {
		dprintf(LOG_ERR, FNAME, "command queue is not initialized");
		close(s);
		return (-1);
	}

	new = malloc(sizeof(*new));
	if (new == NULL) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to allocate new command context");
		goto fail;
	}

	/* if the command queue is full, purge the oldest one */
	if (commands == max_commands) {
		ctx = TAILQ_FIRST(&commandqueue_head);

		dprintf(LOG_INFO, FNAME, "command queue is full. "
		    "drop the oldest one (fd=%d)", ctx->s);

		TAILQ_REMOVE(&commandqueue_head, ctx, link);
		dhcp6_ctl_closecommand(ctx);
	}

	/* insert the next context to the queue */
	memset(new, 0, sizeof(*new));
	new->s = s;
	new->callback = callback;
	new->input_len = sizeof(struct dhcp6ctl);
	TAILQ_INSERT_TAIL(&commandqueue_head, new, link);
	commands++;

	return (0);

  fail:
	close(s);

	return (-1);
}

void
dhcp6_ctl_closecommand(ctx)
	struct dhcp6_commandctx *ctx;
{
	close(ctx->s);
	free(ctx);

	if (commands == 0) {
		dprintf(LOG_ERR, FNAME, "assumption error: "
		    "command queue is empty?");
		exit(1);	/* XXX */
	}
	commands--;

	return;
}

int
dhcp6_ctl_readcommand(read_fds)
	fd_set *read_fds;
{
	struct dhcp6_commandctx *ctx, *ctx_next;
	char *cp;
	int cc, resid, result;
	struct dhcp6ctl *ctlhead;

	for (ctx = TAILQ_FIRST(&commandqueue_head); ctx != NULL;
	    ctx = ctx_next) {
		ctx_next = TAILQ_NEXT(ctx, link);

		if (FD_ISSET(ctx->s, read_fds)) {
			cp = ctx->inputbuf + ctx->input_filled;
			resid = ctx->input_len - ctx->input_filled;

			cc = read(ctx->s, cp, resid);
			if (cc < 0) {
				dprintf(LOG_WARNING, FNAME, "read failed: %s",
				    strerror(errno));
				goto closecommand;
			}
			if (cc == 0) {
				dprintf(LOG_INFO, FNAME,
				    "control channel was reset by peer");
				goto closecommand;
			}

			ctx->input_filled += cc;
			if (ctx->input_filled < ctx->input_len)
				continue; /* we need more data */
			else if (ctx->input_filled == sizeof(*ctlhead)) { 
				ctlhead = (struct dhcp6ctl *)ctx->inputbuf;
				ctx->input_len += ntohs(ctlhead->len);
			}

			if (ctx->input_filled == ctx->input_len) {
				/* we're done.  execute the command. */
				result = (ctx->callback)(ctx->inputbuf,
				    ctx->input_len);

				switch (result) {
				case DHCP6CTL_R_DONE:
				case DHCP6CTL_R_FAILURE:
					goto closecommand;
				default:
					break;
				}
			} else if (ctx->input_len > sizeof(ctx->inputbuf)) {
				dprintf(LOG_INFO, FNAME,
				    "too large command (%d bytes)",
				    ctx->input_len);
				goto closecommand;
			}

			continue;

		  closecommand:
			TAILQ_REMOVE(&commandqueue_head, ctx, link);
			dhcp6_ctl_closecommand(ctx);
		}
	}

	return (0);
}

int
dhcp6_ctl_setreadfds(read_fds, maxfdp)
	fd_set *read_fds;
	int *maxfdp;
{
	int maxfd = *maxfdp;
	struct dhcp6_commandctx *ctx;

	for (ctx = TAILQ_FIRST(&commandqueue_head); ctx != NULL;
	    ctx = TAILQ_NEXT(ctx, link)) {
		FD_SET(ctx->s, read_fds);
		if (ctx->s > maxfd)
			maxfd = ctx->s;
	}

	*maxfdp = maxfd;

	return (0);
}
