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

#ifndef _CHANSESSION_H_
#define _CHANSESSION_H_

#include "loginrec.h"
#include "channel.h"
#include "listener.h"

struct exitinfo {

	int exitpid; /* -1 if not exited */
	int exitstatus;
	int exitsignal;
	int exitcore;
};

struct ChanSess {

	unsigned char * cmd; /* command to exec */
	pid_t pid; /* child process pid */

	/* pty details */
	int master; /* the master terminal fd*/
	int slave;
	unsigned char * tty;
	unsigned char * term;

	/* exit details */
	struct exitinfo exit;

	/* Used to set $SSH_CONNECTION in the child session. 
	Is only set temporarily before forking */
	char *connection_string;
	
#ifndef DISABLE_X11FWD
	struct Listener * x11listener;
	int x11port;
	char * x11authprot;
	char * x11authcookie;
	unsigned int x11screennum;
	unsigned char x11singleconn;
#endif

#ifdef ENABLE_SVR_AGENTFWD
	struct Listener * agentlistener;
	char * agentfile;
	char * agentdir;
#endif

#ifdef ENABLE_SVR_PUBKEY_OPTIONS
	char *original_command;
#endif
};

struct ChildPid {
	pid_t pid;
	struct ChanSess * chansess;
};


void addnewvar(const char* param, const char* var);

void cli_send_chansess_request();
void cli_tty_cleanup();
void cli_chansess_winchange();
#ifdef ENABLE_CLI_NETCAT
void cli_send_netcat_request();
#endif
void cli_start_send_channel_request(struct Channel *channel, unsigned char *type);

void svr_chansessinitialise();
extern const struct ChanType svrchansess;

struct SigMap {
	int signal;
	char* name;
};

extern const struct SigMap signames[];

#endif /* _CHANSESSION_H_ */
