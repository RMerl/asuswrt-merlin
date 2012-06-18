/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#ifndef _RUNOPTS_H_
#define _RUNOPTS_H_

#include "includes.h"
#include "signkey.h"
#include "buffer.h"
#include "auth.h"
#include "tcpfwd.h"

typedef struct runopts {

#if defined(ENABLE_SVR_REMOTETCPFWD) || defined(ENABLE_CLI_LOCALTCPFWD)
	int listen_fwd_all;
#endif
	unsigned int recv_window;
	unsigned int keepalive_secs;
	unsigned int idle_timeout_secs;

} runopts;

extern runopts opts;

int readhostkey(const char * filename, sign_key * hostkey, int *type);

typedef struct svr_runopts {

	char * rsakeyfile;
	char * dsskeyfile;
	char * bannerfile;

	int forkbg;
	int usingsyslog;

	/* ports is an array of the portcount listening ports */
	char *ports[DROPBEAR_MAX_PORTS];
	unsigned int portcount;
	char *addresses[DROPBEAR_MAX_PORTS];

	int inetdmode;

	/* Flags indicating whether to use ipv4 and ipv6 */
	/* not used yet
	int ipv4;
	int ipv6;
	*/

#ifdef DO_MOTD
	/* whether to print the MOTD */
	int domotd;
#endif

	int norootlogin;

	int noauthpass;
	int norootpass;

#ifdef ENABLE_SVR_REMOTETCPFWD
	int noremotetcp;
#endif
#ifdef ENABLE_SVR_LOCALTCPFWD
	int nolocaltcp;
#endif

	sign_key *hostkey;
	buffer * banner;
	char * pidfile;

} svr_runopts;

extern svr_runopts svr_opts;

void svr_getopts(int argc, char ** argv);
void loadhostkeys();

typedef struct cli_runopts {

	char *progname;
	char *remotehost;
	char *remoteport;

	char *own_user;
	char *username;

	char *cmd;
	int wantpty;
	int always_accept_key;
	int no_cmd;
	int backgrounded;
	int is_subsystem;
#ifdef ENABLE_CLI_PUBKEY_AUTH
	struct SignKeyList *privkeys; /* Keys to use for public-key auth */
#endif
#ifdef ENABLE_CLI_REMOTETCPFWD
	struct TCPFwdList * remotefwds;
#endif
#ifdef ENABLE_CLI_LOCALTCPFWD
	struct TCPFwdList * localfwds;
#endif

#ifdef ENABLE_CLI_NETCAT
	char *netcat_host;
	unsigned int netcat_port;
#endif
#ifdef ENABLE_CLI_PROXYCMD
	char *proxycmd;
#endif

} cli_runopts;

extern cli_runopts cli_opts;
void cli_getopts(int argc, char ** argv);

#endif /* _RUNOPTS_H_ */
