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

#include "includes.h"
#include "packet.h"
#include "buffer.h"
#include "session.h"
#include "dbutil.h"
#include "channel.h"
#include "chansession.h"
#include "sshpty.h"
#include "termcodes.h"
#include "ssh.h"
#include "dbrandom.h"
#include "x11fwd.h"
#include "agentfwd.h"
#include "runopts.h"
#include "auth.h"

/* Handles sessions (either shells or programs) requested by the client */

static int sessioncommand(struct Channel *channel, struct ChanSess *chansess,
		int iscmd, int issubsys);
static int sessionpty(struct ChanSess * chansess);
static int sessionsignal(struct ChanSess *chansess);
static int noptycommand(struct Channel *channel, struct ChanSess *chansess);
static int ptycommand(struct Channel *channel, struct ChanSess *chansess);
static int sessionwinchange(struct ChanSess *chansess);
static void execchild(void *user_data_chansess);
static void addchildpid(struct ChanSess *chansess, pid_t pid);
static void sesssigchild_handler(int val);
static void closechansess(struct Channel *channel);
static int newchansess(struct Channel *channel);
static void chansessionrequest(struct Channel *channel);
static int sesscheckclose(struct Channel *channel);

static void send_exitsignalstatus(struct Channel *channel);
static void send_msg_chansess_exitstatus(struct Channel * channel,
		struct ChanSess * chansess);
static void send_msg_chansess_exitsignal(struct Channel * channel,
		struct ChanSess * chansess);
static void get_termmodes(struct ChanSess *chansess);

const struct ChanType svrchansess = {
	0, /* sepfds */
	"session", /* name */
	newchansess, /* inithandler */
	sesscheckclose, /* checkclosehandler */
	chansessionrequest, /* reqhandler */
	closechansess, /* closehandler */
};

/* required to clear environment */
extern char** environ;

static int sesscheckclose(struct Channel *channel) {
	struct ChanSess *chansess = (struct ChanSess*)channel->typedata;
	TRACE(("sesscheckclose, pid is %d", chansess->exit.exitpid))
	return chansess->exit.exitpid != -1;
}

/* Handler for childs exiting, store the state for return to the client */

/* There's a particular race we have to watch out for: if the forked child
 * executes, exits, and this signal-handler is called, all before the parent
 * gets to run, then the childpids[] array won't have the pid in it. Hence we
 * use the svr_ses.lastexit struct to hold the exit, which is then compared by
 * the parent when it runs. This work correctly at least in the case of a
 * single shell spawned (ie the usual case) */
static void sesssigchild_handler(int UNUSED(dummy)) {

	int status;
	pid_t pid;
	unsigned int i;
	struct sigaction sa_chld;
	struct exitinfo *exit = NULL;

	const int saved_errno = errno;

	/* Make channel handling code look for closed channels */
	ses.channel_signal_pending = 1;

	TRACE(("enter sigchld handler"))
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		TRACE(("sigchld handler: pid %d", pid))

		exit = NULL;
		/* find the corresponding chansess */
		for (i = 0; i < svr_ses.childpidsize; i++) {
			if (svr_ses.childpids[i].pid == pid) {
				TRACE(("found match session"));
				exit = &svr_ses.childpids[i].chansess->exit;
				break;
			}
		}

		/* If the pid wasn't matched, then we might have hit the race mentioned
		 * above. So we just store the info for the parent to deal with */
		if (exit == NULL) {
			TRACE(("using lastexit"));
			exit = &svr_ses.lastexit;
		}

		exit->exitpid = pid;
		if (WIFEXITED(status)) {
			exit->exitstatus = WEXITSTATUS(status);
		}
		if (WIFSIGNALED(status)) {
			exit->exitsignal = WTERMSIG(status);
#if !defined(AIX) && defined(WCOREDUMP)
			exit->exitcore = WCOREDUMP(status);
#else
			exit->exitcore = 0;
#endif
		} else {
			/* we use this to determine how pid exited */
			exit->exitsignal = -1;
		}
		
		/* Make sure that the main select() loop wakes up */
		while (1) {
			/* isserver is just a random byte to write. We can't do anything
			about an error so should just ignore it */
			if (write(ses.signal_pipe[1], &ses.isserver, 1) == 1
					|| errno != EINTR) {
				break;
			}
		}
	}

	sa_chld.sa_handler = sesssigchild_handler;
	sa_chld.sa_flags = SA_NOCLDSTOP;
	sigemptyset(&sa_chld.sa_mask);
	sigaction(SIGCHLD, &sa_chld, NULL);
	TRACE(("leave sigchld handler"))

	errno = saved_errno;
}

/* send the exit status or the signal causing termination for a session */
static void send_exitsignalstatus(struct Channel *channel) {

	struct ChanSess *chansess = (struct ChanSess*)channel->typedata;

	if (chansess->exit.exitpid >= 0) {
		if (chansess->exit.exitsignal > 0) {
			send_msg_chansess_exitsignal(channel, chansess);
		} else {
			send_msg_chansess_exitstatus(channel, chansess);
		}
	}
}

/* send the exitstatus to the client */
static void send_msg_chansess_exitstatus(struct Channel * channel,
		struct ChanSess * chansess) {

	dropbear_assert(chansess->exit.exitpid != -1);
	dropbear_assert(chansess->exit.exitsignal == -1);

	CHECKCLEARTOWRITE();

	buf_putbyte(ses.writepayload, SSH_MSG_CHANNEL_REQUEST);
	buf_putint(ses.writepayload, channel->remotechan);
	buf_putstring(ses.writepayload, "exit-status", 11);
	buf_putbyte(ses.writepayload, 0); /* boolean FALSE */
	buf_putint(ses.writepayload, chansess->exit.exitstatus);

	encrypt_packet();

}

/* send the signal causing the exit to the client */
static void send_msg_chansess_exitsignal(struct Channel * channel,
		struct ChanSess * chansess) {

	int i;
	char* signame = NULL;
	dropbear_assert(chansess->exit.exitpid != -1);
	dropbear_assert(chansess->exit.exitsignal > 0);

	TRACE(("send_msg_chansess_exitsignal %d", chansess->exit.exitsignal))

	CHECKCLEARTOWRITE();

	/* we check that we can match a signal name, otherwise
	 * don't send anything */
	for (i = 0; signames[i].name != NULL; i++) {
		if (signames[i].signal == chansess->exit.exitsignal) {
			signame = signames[i].name;
			break;
		}
	}

	if (signame == NULL) {
		return;
	}

	buf_putbyte(ses.writepayload, SSH_MSG_CHANNEL_REQUEST);
	buf_putint(ses.writepayload, channel->remotechan);
	buf_putstring(ses.writepayload, "exit-signal", 11);
	buf_putbyte(ses.writepayload, 0); /* boolean FALSE */
	buf_putstring(ses.writepayload, signame, strlen(signame));
	buf_putbyte(ses.writepayload, chansess->exit.exitcore);
	buf_putstring(ses.writepayload, "", 0); /* error msg */
	buf_putstring(ses.writepayload, "", 0); /* lang */

	encrypt_packet();
}

/* set up a session channel */
static int newchansess(struct Channel *channel) {

	struct ChanSess *chansess;

	TRACE(("new chansess %p", (void*)channel))

	dropbear_assert(channel->typedata == NULL);

	chansess = (struct ChanSess*)m_malloc(sizeof(struct ChanSess));
	chansess->cmd = NULL;
	chansess->connection_string = NULL;
	chansess->client_string = NULL;
	chansess->pid = 0;

	/* pty details */
	chansess->master = -1;
	chansess->slave = -1;
	chansess->tty = NULL;
	chansess->term = NULL;

	chansess->exit.exitpid = -1;

	channel->typedata = chansess;

#ifndef DISABLE_X11FWD
	chansess->x11listener = NULL;
	chansess->x11authprot = NULL;
	chansess->x11authcookie = NULL;
#endif

#ifdef ENABLE_SVR_AGENTFWD
	chansess->agentlistener = NULL;
	chansess->agentfile = NULL;
	chansess->agentdir = NULL;
#endif

	channel->prio = DROPBEAR_CHANNEL_PRIO_INTERACTIVE;

	return 0;

}

static struct logininfo* 
chansess_login_alloc(struct ChanSess *chansess) {
	struct logininfo * li;
	li = login_alloc_entry(chansess->pid, ses.authstate.username,
			svr_ses.remotehost, chansess->tty);
	return li;
}

/* clean a session channel */
static void closechansess(struct Channel *channel) {

	struct ChanSess *chansess;
	unsigned int i;
	struct logininfo *li;

	TRACE(("enter closechansess"))

	chansess = (struct ChanSess*)channel->typedata;

	if (chansess == NULL) {
		TRACE(("leave closechansess: chansess == NULL"))
		return;
	}

	send_exitsignalstatus(channel);

	m_free(chansess->cmd);
	m_free(chansess->term);

#ifdef ENABLE_SVR_PUBKEY_OPTIONS
	m_free(chansess->original_command);
#endif

	if (chansess->tty) {
		/* write the utmp/wtmp login record */
		li = chansess_login_alloc(chansess);
		login_logout(li);
		login_free_entry(li);

		pty_release(chansess->tty);
		m_free(chansess->tty);
	}

#ifndef DISABLE_X11FWD
	x11cleanup(chansess);
#endif

#ifdef ENABLE_SVR_AGENTFWD
	svr_agentcleanup(chansess);
#endif

	/* clear child pid entries */
	for (i = 0; i < svr_ses.childpidsize; i++) {
		if (svr_ses.childpids[i].chansess == chansess) {
			dropbear_assert(svr_ses.childpids[i].pid > 0);
			TRACE(("closing pid %d", svr_ses.childpids[i].pid))
			TRACE(("exitpid is %d", chansess->exit.exitpid))
			svr_ses.childpids[i].pid = -1;
			svr_ses.childpids[i].chansess = NULL;
		}
	}
				
	m_free(chansess);

	TRACE(("leave closechansess"))
}

/* Handle requests for a channel. These can be execution requests,
 * or x11/authagent forwarding. These are passed to appropriate handlers */
static void chansessionrequest(struct Channel *channel) {

	char * type = NULL;
	unsigned int typelen;
	unsigned char wantreply;
	int ret = 1;
	struct ChanSess *chansess;

	TRACE(("enter chansessionrequest"))

	type = buf_getstring(ses.payload, &typelen);
	wantreply = buf_getbool(ses.payload);

	if (typelen > MAX_NAME_LEN) {
		TRACE(("leave chansessionrequest: type too long")) /* XXX send error?*/
		goto out;
	}

	chansess = (struct ChanSess*)channel->typedata;
	dropbear_assert(chansess != NULL);
	TRACE(("type is %s", type))

	if (strcmp(type, "window-change") == 0) {
		ret = sessionwinchange(chansess);
	} else if (strcmp(type, "shell") == 0) {
		ret = sessioncommand(channel, chansess, 0, 0);
	} else if (strcmp(type, "pty-req") == 0) {
		ret = sessionpty(chansess);
	} else if (strcmp(type, "exec") == 0) {
		ret = sessioncommand(channel, chansess, 1, 0);
	} else if (strcmp(type, "subsystem") == 0) {
		ret = sessioncommand(channel, chansess, 1, 1);
#ifndef DISABLE_X11FWD
	} else if (strcmp(type, "x11-req") == 0) {
		ret = x11req(chansess);
#endif
#ifdef ENABLE_SVR_AGENTFWD
	} else if (strcmp(type, "auth-agent-req@openssh.com") == 0) {
		ret = svr_agentreq(chansess);
#endif
	} else if (strcmp(type, "signal") == 0) {
		ret = sessionsignal(chansess);
	} else {
		/* etc, todo "env", "subsystem" */
	}

out:

	if (wantreply) {
		if (ret == DROPBEAR_SUCCESS) {
			send_msg_channel_success(channel);
		} else {
			send_msg_channel_failure(channel);
		}
	}

	m_free(type);
	TRACE(("leave chansessionrequest"))
}


/* Send a signal to a session's process as requested by the client*/
static int sessionsignal(struct ChanSess *chansess) {

	int sig = 0;
	char* signame = NULL;
	int i;

	if (chansess->pid == 0) {
		/* haven't got a process pid yet */
		return DROPBEAR_FAILURE;
	}

	signame = buf_getstring(ses.payload, NULL);

	i = 0;
	while (signames[i].name != 0) {
		if (strcmp(signames[i].name, signame) == 0) {
			sig = signames[i].signal;
			break;
		}
		i++;
	}

	m_free(signame);

	if (sig == 0) {
		/* failed */
		return DROPBEAR_FAILURE;
	}
			
	if (kill(chansess->pid, sig) < 0) {
		return DROPBEAR_FAILURE;
	} 

	return DROPBEAR_SUCCESS;
}

/* Let the process know that the window size has changed, as notified from the
 * client. Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int sessionwinchange(struct ChanSess *chansess) {

	int termc, termr, termw, termh;

	if (chansess->master < 0) {
		/* haven't got a pty yet */
		return DROPBEAR_FAILURE;
	}
			
	termc = buf_getint(ses.payload);
	termr = buf_getint(ses.payload);
	termw = buf_getint(ses.payload);
	termh = buf_getint(ses.payload);
	
	pty_change_window_size(chansess->master, termr, termc, termw, termh);

	return DROPBEAR_SUCCESS;
}

static void get_termmodes(struct ChanSess *chansess) {

	struct termios termio;
	unsigned char opcode;
	unsigned int value;
	const struct TermCode * termcode;
	unsigned int len;

	TRACE(("enter get_termmodes"))

	/* Term modes */
	/* We'll ignore errors and continue if we can't set modes.
	 * We're ignoring baud rates since they seem evil */
	if (tcgetattr(chansess->master, &termio) == -1) {
		return;
	}

	len = buf_getint(ses.payload);
	TRACE(("term mode str %d p->l %d p->p %d", 
				len, ses.payload->len , ses.payload->pos));
	if (len != ses.payload->len - ses.payload->pos) {
		dropbear_exit("Bad term mode string");
	}

	if (len == 0) {
		TRACE(("leave get_termmodes: empty terminal modes string"))
		return;
	}

	while (((opcode = buf_getbyte(ses.payload)) != 0x00) && opcode <= 159) {

		/* must be before checking type, so that value is consumed even if
		 * we don't use it */
		value = buf_getint(ses.payload);

		/* handle types of code */
		if (opcode > MAX_TERMCODE) {
			continue;
		}
		termcode = &termcodes[(unsigned int)opcode];
		

		switch (termcode->type) {

			case TERMCODE_NONE:
				break;

			case TERMCODE_CONTROLCHAR:
				termio.c_cc[termcode->mapcode] = value;
				break;

			case TERMCODE_INPUT:
				if (value) {
					termio.c_iflag |= termcode->mapcode;
				} else {
					termio.c_iflag &= ~(termcode->mapcode);
				}
				break;

			case TERMCODE_OUTPUT:
				if (value) {
					termio.c_oflag |= termcode->mapcode;
				} else {
					termio.c_oflag &= ~(termcode->mapcode);
				}
				break;

			case TERMCODE_LOCAL:
				if (value) {
					termio.c_lflag |= termcode->mapcode;
				} else {
					termio.c_lflag &= ~(termcode->mapcode);
				}
				break;

			case TERMCODE_CONTROL:
				if (value) {
					termio.c_cflag |= termcode->mapcode;
				} else {
					termio.c_cflag &= ~(termcode->mapcode);
				}
				break;
				
		}
	}
	if (tcsetattr(chansess->master, TCSANOW, &termio) < 0) {
		dropbear_log(LOG_INFO, "Error setting terminal attributes");
	}
	TRACE(("leave get_termmodes"))
}

/* Set up a session pty which will be used to execute the shell or program.
 * The pty is allocated now, and kept for when the shell/program executes.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int sessionpty(struct ChanSess * chansess) {

	unsigned int termlen;
	char namebuf[65];
	struct passwd * pw = NULL;

	TRACE(("enter sessionpty"))

	if (!svr_pubkey_allows_pty()) {
		TRACE(("leave sessionpty : pty forbidden by public key option"))
		return DROPBEAR_FAILURE;
	}

	chansess->term = buf_getstring(ses.payload, &termlen);
	if (termlen > MAX_TERM_LEN) {
		/* TODO send disconnect ? */
		TRACE(("leave sessionpty: term len too long"))
		return DROPBEAR_FAILURE;
	}

	/* allocate the pty */
	if (chansess->master != -1) {
		dropbear_exit("Multiple pty requests");
	}
	if (pty_allocate(&chansess->master, &chansess->slave, namebuf, 64) == 0) {
		TRACE(("leave sessionpty: failed to allocate pty"))
		return DROPBEAR_FAILURE;
	}
	
	chansess->tty = m_strdup(namebuf);
	if (!chansess->tty) {
		dropbear_exit("Out of memory"); /* TODO disconnect */
	}

	pw = getpwnam(ses.authstate.pw_name);
	if (!pw)
		dropbear_exit("getpwnam failed after succeeding previously");
	pty_setowner(pw, chansess->tty);

	/* Set up the rows/col counts */
	sessionwinchange(chansess);

	/* Read the terminal modes */
	get_termmodes(chansess);

	TRACE(("leave sessionpty"))
	return DROPBEAR_SUCCESS;
}

#ifndef USE_VFORK
static void make_connection_string(struct ChanSess *chansess) {
	char *local_ip, *local_port, *remote_ip, *remote_port;
	size_t len;
	get_socket_address(ses.sock_in, &local_ip, &local_port, &remote_ip, &remote_port, 0);

	/* "remoteip remoteport localip localport" */
	len = strlen(local_ip) + strlen(remote_ip) + 20;
	chansess->connection_string = m_malloc(len);
	snprintf(chansess->connection_string, len, "%s %s %s %s", remote_ip, remote_port, local_ip, local_port);

	/* deprecated but bash only loads .bashrc if SSH_CLIENT is set */ 
	/* "remoteip remoteport localport" */
	len = strlen(remote_ip) + 20;
	chansess->client_string = m_malloc(len);
	snprintf(chansess->client_string, len, "%s %s %s", remote_ip, remote_port, local_port);

	m_free(local_ip);
	m_free(local_port);
	m_free(remote_ip);
	m_free(remote_port);
}
#endif

/* Handle a command request from the client. This is used for both shell
 * and command-execution requests, and passes the command to
 * noptycommand or ptycommand as appropriate.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int sessioncommand(struct Channel *channel, struct ChanSess *chansess,
		int iscmd, int issubsys) {

	unsigned int cmdlen;
	int ret;

	TRACE(("enter sessioncommand"))

	if (chansess->cmd != NULL) {
		/* Note that only one command can _succeed_. The client might try
		 * one command (which fails), then try another. Ie fallback
		 * from sftp to scp */
		return DROPBEAR_FAILURE;
	}

	if (iscmd) {
		/* "exec" */
		if (chansess->cmd == NULL) {
			chansess->cmd = buf_getstring(ses.payload, &cmdlen);

			if (cmdlen > MAX_CMD_LEN) {
				m_free(chansess->cmd);
				/* TODO - send error - too long ? */
				return DROPBEAR_FAILURE;
			}
		}
		if (issubsys) {
#ifdef SFTPSERVER_PATH
			if ((cmdlen == 4) && strncmp(chansess->cmd, "sftp", 4) == 0) {
				m_free(chansess->cmd);
				chansess->cmd = m_strdup(SFTPSERVER_PATH);
			} else 
#endif
			{
				m_free(chansess->cmd);
				return DROPBEAR_FAILURE;
			}
		}
	}
	
	/* take public key option 'command' into account */
	svr_pubkey_set_forced_command(chansess);

#ifdef LOG_COMMANDS
	if (chansess->cmd) {
		dropbear_log(LOG_INFO, "User %s executing '%s'", 
						ses.authstate.pw_name, chansess->cmd);
	} else {
		dropbear_log(LOG_INFO, "User %s executing login shell", 
						ses.authstate.pw_name);
	}
#endif

	/* uClinux will vfork(), so there'll be a race as 
	connection_string is freed below. */
#ifndef USE_VFORK
	make_connection_string(chansess);
#endif

	if (chansess->term == NULL) {
		/* no pty */
		ret = noptycommand(channel, chansess);
		if (ret == DROPBEAR_SUCCESS) {
			channel->prio = DROPBEAR_CHANNEL_PRIO_BULK;
			update_channel_prio();
		}
	} else {
		/* want pty */
		ret = ptycommand(channel, chansess);
	}

#ifndef USE_VFORK
	m_free(chansess->connection_string);
	m_free(chansess->client_string);
#endif

	if (ret == DROPBEAR_FAILURE) {
		m_free(chansess->cmd);
	}
	return ret;
}

/* Execute a command and set up redirection of stdin/stdout/stderr without a
 * pty.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int noptycommand(struct Channel *channel, struct ChanSess *chansess) {
	int ret;

	TRACE(("enter noptycommand"))
	ret = spawn_command(execchild, chansess, 
			&channel->writefd, &channel->readfd, &channel->errfd,
			&chansess->pid);

	if (ret == DROPBEAR_FAILURE) {
		return ret;
	}

	ses.maxfd = MAX(ses.maxfd, channel->writefd);
	ses.maxfd = MAX(ses.maxfd, channel->readfd);
	ses.maxfd = MAX(ses.maxfd, channel->errfd);

	addchildpid(chansess, chansess->pid);

	if (svr_ses.lastexit.exitpid != -1) {
		unsigned int i;
		TRACE(("parent side: lastexitpid is %d", svr_ses.lastexit.exitpid))
		/* The child probably exited and the signal handler triggered
		 * possibly before we got around to adding the childpid. So we fill
		 * out its data manually */
		for (i = 0; i < svr_ses.childpidsize; i++) {
			if (svr_ses.childpids[i].pid == svr_ses.lastexit.exitpid) {
				TRACE(("found match for lastexitpid"))
				svr_ses.childpids[i].chansess->exit = svr_ses.lastexit;
				svr_ses.lastexit.exitpid = -1;
				break;
			}
		}
	}

	TRACE(("leave noptycommand"))
	return DROPBEAR_SUCCESS;
}

/* Execute a command or shell within a pty environment, and set up
 * redirection as appropriate.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int ptycommand(struct Channel *channel, struct ChanSess *chansess) {

	pid_t pid;
	struct logininfo *li = NULL;
#ifdef DO_MOTD
	buffer * motdbuf = NULL;
	int len;
	struct stat sb;
	char *hushpath = NULL;
#endif

	TRACE(("enter ptycommand"))

	/* we need to have a pty allocated */
	if (chansess->master == -1 || chansess->tty == NULL) {
		dropbear_log(LOG_WARNING, "No pty was allocated, couldn't execute");
		return DROPBEAR_FAILURE;
	}
	
#ifdef USE_VFORK
	pid = vfork();
#else
	pid = fork();
#endif
	if (pid < 0)
		return DROPBEAR_FAILURE;

	if (pid == 0) {
		/* child */
		
		TRACE(("back to normal sigchld"))
		/* Revert to normal sigchld handling */
		if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
			dropbear_exit("signal() error");
		}
		
		/* redirect stdin/stdout/stderr */
		close(chansess->master);

		pty_make_controlling_tty(&chansess->slave, chansess->tty);
		
		if ((dup2(chansess->slave, STDIN_FILENO) < 0) ||
			(dup2(chansess->slave, STDERR_FILENO) < 0) ||
			(dup2(chansess->slave, STDOUT_FILENO) < 0)) {
			TRACE(("leave ptycommand: error redirecting filedesc"))
			return DROPBEAR_FAILURE;
		}

		close(chansess->slave);

		/* write the utmp/wtmp login record - must be after changing the
		 * terminal used for stdout with the dup2 above */
		li = chansess_login_alloc(chansess);
		login_login(li);
		login_free_entry(li);

#ifdef DO_MOTD
		if (svr_opts.domotd && !chansess->cmd) {
			/* don't show the motd if ~/.hushlogin exists */

			/* 12 == strlen("/.hushlogin\0") */
			len = strlen(ses.authstate.pw_dir) + 12; 

			hushpath = m_malloc(len);
			snprintf(hushpath, len, "%s/.hushlogin", ses.authstate.pw_dir);

			if (stat(hushpath, &sb) < 0) {
				/* more than a screenful is stupid IMHO */
				motdbuf = buf_new(80 * 25);
				if (buf_readfile(motdbuf, MOTD_FILENAME) == DROPBEAR_SUCCESS) {
					buf_setpos(motdbuf, 0);
					while (motdbuf->pos != motdbuf->len) {
						len = motdbuf->len - motdbuf->pos;
						len = write(STDOUT_FILENO, 
								buf_getptr(motdbuf, len), len);
						buf_incrpos(motdbuf, len);
					}
				}
				buf_free(motdbuf);
			}
			m_free(hushpath);
		}
#endif /* DO_MOTD */

		execchild(chansess);
		/* not reached */

	} else {
		/* parent */
		TRACE(("continue ptycommand: parent"))
		chansess->pid = pid;

		/* add a child pid */
		addchildpid(chansess, pid);

		close(chansess->slave);
		channel->writefd = chansess->master;
		channel->readfd = chansess->master;
		/* don't need to set stderr here */
		ses.maxfd = MAX(ses.maxfd, chansess->master);

		setnonblocking(chansess->master);

	}

	TRACE(("leave ptycommand"))
	return DROPBEAR_SUCCESS;
}

/* Add the pid of a child to the list for exit-handling */
static void addchildpid(struct ChanSess *chansess, pid_t pid) {

	unsigned int i;
	for (i = 0; i < svr_ses.childpidsize; i++) {
		if (svr_ses.childpids[i].pid == -1) {
			break;
		}
	}

	/* need to increase size */
	if (i == svr_ses.childpidsize) {
		svr_ses.childpids = (struct ChildPid*)m_realloc(svr_ses.childpids,
				sizeof(struct ChildPid) * (svr_ses.childpidsize+1));
		svr_ses.childpidsize++;
	}
	
	svr_ses.childpids[i].pid = pid;
	svr_ses.childpids[i].chansess = chansess;

}

/* Clean up, drop to user privileges, set up the environment and execute
 * the command/shell. This function does not return. */
static void execchild(void *user_data) {
	struct ChanSess *chansess = user_data;
	char *usershell = NULL;

	/* with uClinux we'll have vfork()ed, so don't want to overwrite the
	 * hostkey. can't think of a workaround to clear it */
#ifndef USE_VFORK
	/* wipe the hostkey */
	sign_key_free(svr_opts.hostkey);
	svr_opts.hostkey = NULL;

	/* overwrite the prng state */
	seedrandom();
#endif

	/* clear environment */
	/* if we're debugging using valgrind etc, we need to keep the LD_PRELOAD
	 * etc. This is hazardous, so should only be used for debugging. */
#ifndef DEBUG_VALGRIND
#ifdef HAVE_CLEARENV
	clearenv();
#else /* don't HAVE_CLEARENV */
	/* Yay for posix. */
	if (environ) {
		environ[0] = NULL;
	}
#endif /* HAVE_CLEARENV */
#endif /* DEBUG_VALGRIND */

	/* We can only change uid/gid as root ... */
	if (getuid() == 0) {

		if ((setgid(ses.authstate.pw_gid) < 0) ||
			(initgroups(ses.authstate.pw_name, 
						ses.authstate.pw_gid) < 0)) {
			dropbear_exit("Error changing user group");
		}
		if (setuid(ses.authstate.pw_uid) < 0) {
			dropbear_exit("Error changing user");
		}
	} else {
		/* ... but if the daemon is the same uid as the requested uid, we don't
		 * need to */

		/* XXX - there is a minor issue here, in that if there are multiple
		 * usernames with the same uid, but differing groups, then the
		 * differing groups won't be set (as with initgroups()). The solution
		 * is for the sysadmin not to give out the UID twice */
		if (getuid() != ses.authstate.pw_uid) {
			dropbear_exit("Couldn't	change user as non-root");
		}
	}

	/* set env vars */
	addnewvar("USER", ses.authstate.pw_name);
	addnewvar("LOGNAME", ses.authstate.pw_name);
	addnewvar("HOME", ses.authstate.pw_dir);
	addnewvar("SHELL", get_user_shell());
	addnewvar("PATH", DEFAULT_PATH);
	if (chansess->term != NULL) {
		addnewvar("TERM", chansess->term);
	}

	if (chansess->tty) {
		addnewvar("SSH_TTY", chansess->tty);
	}
	
	if (chansess->connection_string) {
		addnewvar("SSH_CONNECTION", chansess->connection_string);
	}

	if (chansess->client_string) {
		addnewvar("SSH_CLIENT", chansess->client_string);
	}
	
#ifdef ENABLE_SVR_PUBKEY_OPTIONS
	if (chansess->original_command) {
		addnewvar("SSH_ORIGINAL_COMMAND", chansess->original_command);
	}
#endif

	/* change directory */
	if (chdir(ses.authstate.pw_dir) < 0) {
		dropbear_exit("Error changing directory");
	}

#ifndef DISABLE_X11FWD
	/* set up X11 forwarding if enabled */
	x11setauth(chansess);
#endif
#ifdef ENABLE_SVR_AGENTFWD
	/* set up agent env variable */
	svr_agentset(chansess);
#endif

	usershell = m_strdup(get_user_shell());
	run_shell_command(chansess->cmd, ses.maxfd, usershell);

	/* only reached on error */
	dropbear_exit("Child failed");
}

/* Set up the general chansession environment, in particular child-exit
 * handling */
void svr_chansessinitialise() {

	struct sigaction sa_chld;

	/* single child process intially */
	svr_ses.childpids = (struct ChildPid*)m_malloc(sizeof(struct ChildPid));
	svr_ses.childpids[0].pid = -1; /* unused */
	svr_ses.childpids[0].chansess = NULL;
	svr_ses.childpidsize = 1;
	svr_ses.lastexit.exitpid = -1; /* Nothing has exited yet */
	sa_chld.sa_handler = sesssigchild_handler;
	sa_chld.sa_flags = SA_NOCLDSTOP;
	sigemptyset(&sa_chld.sa_mask);
	if (sigaction(SIGCHLD, &sa_chld, NULL) < 0) {
		dropbear_exit("signal() error");
	}
	
}

/* add a new environment variable, allocating space for the entry */
void addnewvar(const char* param, const char* var) {

	char* newvar = NULL;
	int plen, vlen;

	plen = strlen(param);
	vlen = strlen(var);

	newvar = m_malloc(plen + vlen + 2); /* 2 is for '=' and '\0' */
	memcpy(newvar, param, plen);
	newvar[plen] = '=';
	memcpy(&newvar[plen+1], var, vlen);
	newvar[plen+vlen+1] = '\0';
	/* newvar is leaked here, but that's part of putenv()'s semantics */
	if (putenv(newvar) < 0) {
		dropbear_exit("environ error");
	}
}
