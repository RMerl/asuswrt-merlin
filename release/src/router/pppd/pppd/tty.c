/*
 * tty.c - code for handling serial ports in pppd.
 *
 * Copyright (C) 2000-2004 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Portions derived from main.c, which is:
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define RCSID	"$Id: tty.c,v 1.27 2008/07/01 12:27:56 paulus Exp $"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>
#include <pwd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"

void tty_process_extra_options __P((void));
void tty_check_options __P((void));
int  connect_tty __P((void));
void disconnect_tty __P((void));
void tty_close_fds __P((void));
void cleanup_tty __P((void));
void tty_do_send_config __P((int, u_int32_t, int, int));

static int setdevname __P((char *, char **, int));
static int setspeed __P((char *, char **, int));
static int setxonxoff __P((char **));
static int setescape __P((char **));
static void printescape __P((option_t *, void (*)(void *, char *,...),void *));
static void finish_tty __P((void));
static int start_charshunt __P((int, int));
static void stop_charshunt __P((void *, int));
static void charshunt_done __P((void *));
static void charshunt __P((int, int, char *));
static int record_write __P((FILE *, int code, u_char *buf, int nb,
			     struct timeval *));
static int open_socket __P((char *));
static void maybe_relock __P((void *, int));

static int pty_master;		/* fd for master side of pty */
static int pty_slave;		/* fd for slave side of pty */
static int real_ttyfd;		/* fd for actual serial port (not pty) */
static int ttyfd;		/* Serial port file descriptor */
static char speed_str[16];	/* Serial port speed as string */

mode_t tty_mode = (mode_t)-1;	/* Original access permissions to tty */
int baud_rate;			/* Actual bits/second for serial device */
char *callback_script;		/* script for doing callback */
int charshunt_pid;		/* Process ID for charshunt */
int locked;			/* lock() has succeeded */
struct stat devstat;		/* result of stat() on devnam */

/* option variables */
int	crtscts = 0;		/* Use hardware flow control */
int	stop_bits = 1;		/* Number of serial port stop bits */
bool	modem = 1;		/* Use modem control lines */
int	inspeed = 0;		/* Input/Output speed requested */
bool	lockflag = 0;		/* Create lock file to lock the serial dev */
char	*initializer = NULL;	/* Script to initialize physical link */
char	*connect_script = NULL;	/* Script to establish physical link */
char	*disconnect_script = NULL; /* Script to disestablish physical link */
char	*welcomer = NULL;	/* Script to run after phys link estab. */
char	*ptycommand = NULL;	/* Command to run on other side of pty */
bool	notty = 0;		/* Stdin/out is not a tty */
static char *record_file = NULL;	/* File to record chars sent/received */
int	max_data_rate;		/* max bytes/sec through charshunt */
bool	sync_serial = 0;	/* Device is synchronous serial device */
char	*pty_socket = NULL;	/* Socket to connect to pty */
int	using_pty = 0;		/* we're allocating a pty as the device */

extern uid_t uid;
extern int kill_link;
extern int asked_to_quit;
extern int got_sigterm;

/* XXX */
extern int privopen;		/* don't lock, open device as root */

u_int32_t xmit_accm[8];		/* extended transmit ACCM */

/* option descriptors */
option_t tty_options[] = {
    /* device name must be first, or change connect_tty() below! */
    { "device name", o_wild, (void *) &setdevname,
      "Serial port device name",
      OPT_DEVNAM | OPT_PRIVFIX | OPT_NOARG  | OPT_A2STRVAL | OPT_STATIC,
      devnam},

    { "tty speed", o_wild, (void *) &setspeed,
      "Baud rate for serial port",
      OPT_PRIO | OPT_NOARG | OPT_A2STRVAL | OPT_STATIC, speed_str },

    { "lock", o_bool, &lockflag,
      "Lock serial device with UUCP-style lock file", OPT_PRIO | 1 },
    { "nolock", o_bool, &lockflag,
      "Don't lock serial device", OPT_PRIOSUB | OPT_PRIV },

    { "init", o_string, &initializer,
      "A program to initialize the device", OPT_PRIO | OPT_PRIVFIX },

    { "connect", o_string, &connect_script,
      "A program to set up a connection", OPT_PRIO | OPT_PRIVFIX },

    { "disconnect", o_string, &disconnect_script,
      "Program to disconnect serial device", OPT_PRIO | OPT_PRIVFIX },

    { "welcome", o_string, &welcomer,
      "Script to welcome client", OPT_PRIO | OPT_PRIVFIX },

    { "pty", o_string, &ptycommand,
      "Script to run on pseudo-tty master side",
      OPT_PRIO | OPT_PRIVFIX | OPT_DEVNAM },

    { "notty", o_bool, &notty,
      "Input/output is not a tty", OPT_DEVNAM | 1 },

    { "socket", o_string, &pty_socket,
      "Send and receive over socket, arg is host:port",
      OPT_PRIO | OPT_DEVNAM },

    { "crtscts", o_int, &crtscts,
      "Set hardware (RTS/CTS) flow control",
      OPT_PRIO | OPT_NOARG | OPT_VAL(1) },
    { "cdtrcts", o_int, &crtscts,
      "Set alternate hardware (DTR/CTS) flow control",
      OPT_PRIOSUB | OPT_NOARG | OPT_VAL(2) },
    { "nocrtscts", o_int, &crtscts,
      "Disable hardware flow control",
      OPT_PRIOSUB | OPT_NOARG | OPT_VAL(-1) },
    { "-crtscts", o_int, &crtscts,
      "Disable hardware flow control",
      OPT_PRIOSUB | OPT_ALIAS | OPT_NOARG | OPT_VAL(-1) },
    { "nocdtrcts", o_int, &crtscts,
      "Disable hardware flow control",
      OPT_PRIOSUB | OPT_ALIAS | OPT_NOARG | OPT_VAL(-1) },
    { "xonxoff", o_special_noarg, (void *)setxonxoff,
      "Set software (XON/XOFF) flow control", OPT_PRIOSUB },
    { "stop-bits", o_int, &stop_bits,
      "Number of stop bits in serial port",
      OPT_PRIO | OPT_PRIVFIX | OPT_LIMITS, NULL, 2, 1 },

    { "modem", o_bool, &modem,
      "Use modem control lines", OPT_PRIO | 1 },
    { "local", o_bool, &modem,
      "Don't use modem control lines", OPT_PRIOSUB | 0 },

    { "sync", o_bool, &sync_serial,
      "Use synchronous HDLC serial encoding", 1 },

    { "datarate", o_int, &max_data_rate,
      "Maximum data rate in bytes/sec (with pty, notty or record option)",
      OPT_PRIO },

    { "escape", o_special, (void *)setescape,
      "List of character codes to escape on transmission",
      OPT_A2PRINTER, (void *)printescape },

    { NULL }
};


struct channel tty_channel = {
	tty_options,
	&tty_process_extra_options,
	&tty_check_options,
	&connect_tty,
	&disconnect_tty,
	&tty_establish_ppp,
	&tty_disestablish_ppp,
	&tty_do_send_config,
	&tty_recv_config,
	&cleanup_tty,
	&tty_close_fds
};

/*
 * setspeed - Set the serial port baud rate.
 * If doit is 0, the call is to check whether this option is
 * potentially a speed value.
 */
static int
setspeed(arg, argv, doit)
    char *arg;
    char **argv;
    int doit;
{
	char *ptr;
	int spd;

	spd = strtol(arg, &ptr, 0);
	if (ptr == arg || *ptr != 0 || spd == 0)
		return 0;
	if (doit) {
		inspeed = spd;
		slprintf(speed_str, sizeof(speed_str), "%d", spd);
	}
	return 1;
}


/*
 * setdevname - Set the device name.
 * If doit is 0, the call is to check whether this option is
 * potentially a device name.
 */
static int
setdevname(cp, argv, doit)
    char *cp;
    char **argv;
    int doit;
{
	struct stat statbuf;
	char dev[MAXPATHLEN];

	if (*cp == 0)
		return 0;

	if (*cp != '/') {
		strlcpy(dev, "/dev/", sizeof(dev));
		strlcat(dev, cp, sizeof(dev));
		cp = dev;
	}

	/*
	 * Check if there is a character device by this name.
	 */
	if (stat(cp, &statbuf) < 0) {
		if (!doit)
			return errno != ENOENT;
		option_error("Couldn't stat %s: %m", cp);
		return 0;
	}
	if (!S_ISCHR(statbuf.st_mode)) {
		if (doit)
			option_error("%s is not a character device", cp);
		return 0;
	}

	if (doit) {
		strlcpy(devnam, cp, sizeof(devnam));
		devstat = statbuf;
		default_device = 0;
	}
  
	return 1;
}

static int
setxonxoff(argv)
    char **argv;
{
	lcp_wantoptions[0].asyncmap |= 0x000A0000;	/* escape ^S and ^Q */
	lcp_wantoptions[0].neg_asyncmap = 1;

	crtscts = -2;
	return 1;
}

/*
 * setescape - add chars to the set we escape on transmission.
 */
static int
setescape(argv)
    char **argv;
{
    int n, ret;
    char *p, *endp;

    p = *argv;
    ret = 1;
    while (*p) {
	n = strtol(p, &endp, 16);
	if (p == endp) {
	    option_error("escape parameter contains invalid hex number '%s'",
			 p);
	    return 0;
	}
	p = endp;
	if (n < 0 || n == 0x5E || n > 0xFF) {
	    option_error("can't escape character 0x%x", n);
	    ret = 0;
	} else
	    xmit_accm[n >> 5] |= 1 << (n & 0x1F);
	while (*p == ',' || *p == ' ')
	    ++p;
    }
    lcp_allowoptions[0].asyncmap = xmit_accm[0];
    return ret;
}

static void
printescape(opt, printer, arg)
    option_t *opt;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
	int n;
	int first = 1;

	for (n = 0; n < 256; ++n) {
		if (n == 0x7d)
			n += 2;		/* skip 7d, 7e */
		if (xmit_accm[n >> 5] & (1 << (n & 0x1f))) {
			if (!first)
				printer(arg, ",");
			else
				first = 0;
			printer(arg, "%x", n);
		}
	}
	if (first)
		printer(arg, "oops # nothing escaped");
}

/*
 * tty_init - do various tty-related initializations.
 */
void tty_init()
{
    add_notifier(&pidchange, maybe_relock, 0);
    the_channel = &tty_channel;
    xmit_accm[3] = 0x60000000;
}

/*
 * tty_process_extra_options - work out which tty device we are using
 * and read its options file.
 */
void tty_process_extra_options()
{
	using_pty = notty || ptycommand != NULL || pty_socket != NULL;
	if (using_pty)
		return;
	if (default_device) {
		char *p;
		if (!isatty(0) || (p = ttyname(0)) == NULL) {
			option_error("no device specified and stdin is not a tty");
			exit(EXIT_OPTION_ERROR);
		}
		strlcpy(devnam, p, sizeof(devnam));
		if (stat(devnam, &devstat) < 0)
			fatal("Couldn't stat default device %s: %m", devnam);
	}


	/*
	 * Parse the tty options file.
	 * The per-tty options file should not change
	 * ptycommand, pty_socket, notty or devnam.
	 * options_for_tty doesn't override options set on the command line,
	 * except for some privileged options.
	 */
	if (!options_for_tty())
		exit(EXIT_OPTION_ERROR);
}

/*
 * tty_check_options - do consistency checks on the options we were given.
 */
void
tty_check_options()
{
	struct stat statbuf;
	int fdflags;

	if (demand && notty) {
		option_error("demand-dialling is incompatible with notty");
		exit(EXIT_OPTION_ERROR);
	}
	if (demand && connect_script == 0 && ptycommand == NULL
	    && pty_socket == NULL) {
		option_error("connect script is required for demand-dialling\n");
		exit(EXIT_OPTION_ERROR);
	}
	/* default holdoff to 0 if no connect script has been given */
	if (connect_script == 0 && !holdoff_specified)
		holdoff = 0;

	if (using_pty) {
		if (!default_device) {
			option_error("%s option precludes specifying device name",
				     pty_socket? "socket": notty? "notty": "pty");
			exit(EXIT_OPTION_ERROR);
		}
		if (ptycommand != NULL && notty) {
			option_error("pty option is incompatible with notty option");
			exit(EXIT_OPTION_ERROR);
		}
		if (pty_socket != NULL && (ptycommand != NULL || notty)) {
			option_error("socket option is incompatible with pty and notty");
			exit(EXIT_OPTION_ERROR);
		}
		default_device = notty;
		lockflag = 0;
		modem = 0;
		if (notty && log_to_fd <= 1)
			log_to_fd = -1;
	} else {
		/*
		 * If the user has specified a device which is the same as
		 * the one on stdin, pretend they didn't specify any.
		 * If the device is already open read/write on stdin,
		 * we assume we don't need to lock it, and we can open it
		 * as root.
		 */
		if (fstat(0, &statbuf) >= 0 && S_ISCHR(statbuf.st_mode)
		    && statbuf.st_rdev == devstat.st_rdev) {
			default_device = 1;
			fdflags = fcntl(0, F_GETFL);
			if (fdflags != -1 && (fdflags & O_ACCMODE) == O_RDWR)
				privopen = 1;
		}
	}
	if (default_device)
		nodetach = 1;

	/*
	 * Don't send log messages to the serial port, it tends to
	 * confuse the peer. :-)
	 */
	if (log_to_fd >= 0 && fstat(log_to_fd, &statbuf) >= 0
	    && S_ISCHR(statbuf.st_mode) && statbuf.st_rdev == devstat.st_rdev)
		log_to_fd = -1;
}

/*
 * connect_tty - get the serial port ready to start doing PPP.
 * That is, open the serial port, set its speed and mode, and run
 * the connector and/or welcomer.
 */
int connect_tty()
{
	char *connector;
	int fdflags;
#ifndef __linux__
	struct stat statbuf;
#endif
	char numbuf[16];

	/*
	 * Get a pty master/slave pair if the pty, notty, socket,
	 * or record options were specified.
	 */
	strlcpy(ppp_devnam, devnam, sizeof(ppp_devnam));
	pty_master = -1;
	pty_slave = -1;
	real_ttyfd = -1;
	if (using_pty || record_file != NULL) {
		if (!get_pty(&pty_master, &pty_slave, ppp_devnam, uid)) {
			error("Couldn't allocate pseudo-tty");
			status = EXIT_FATAL_ERROR;
			return -1;
		}
		set_up_tty(pty_slave, 1);
	}

	/*
	 * Lock the device if we've been asked to.
	 */
	status = EXIT_LOCK_FAILED;
	if (lockflag && !privopen) {
		if (lock(devnam) < 0)
			goto errret;
		locked = 1;
	}

	/*
	 * Open the serial device and set it up to be the ppp interface.
	 * First we open it in non-blocking mode so we can set the
	 * various termios flags appropriately.  If we aren't dialling
	 * out and we want to use the modem lines, we reopen it later
	 * in order to wait for the carrier detect signal from the modem.
	 */
	got_sigterm = 0;
	connector = doing_callback? callback_script: connect_script;
	if (devnam[0] != 0) {
		for (;;) {
			/* If the user specified the device name, become the
			   user before opening it. */
			int err, prio;

			prio = privopen? OPRIO_ROOT: tty_options[0].priority;
			if (prio < OPRIO_ROOT && seteuid(uid) == -1) {
				error("Unable to drop privileges before opening %s: %m\n",
				      devnam);
				status = EXIT_OPEN_FAILED;
				goto errret;
			}
			real_ttyfd = open(devnam, O_NONBLOCK | O_RDWR, 0);
			err = errno;
			if (prio < OPRIO_ROOT && seteuid(0) == -1)
				fatal("Unable to regain privileges");
			if (real_ttyfd >= 0)
				break;
			errno = err;
			if (err != EINTR) {
				error("Failed to open %s: %m", devnam);
				status = EXIT_OPEN_FAILED;
			}
			if (!persist || err != EINTR)
				goto errret;
		}
		ttyfd = real_ttyfd;
		if ((fdflags = fcntl(ttyfd, F_GETFL)) == -1
		    || fcntl(ttyfd, F_SETFL, fdflags & ~O_NONBLOCK) < 0)
			warn("Couldn't reset non-blocking mode on device: %m");

#ifndef __linux__
		/*
		 * Linux 2.4 and above blocks normal writes to the tty
		 * when it is in PPP line discipline, so this isn't needed.
		 */
		/*
		 * Do the equivalent of `mesg n' to stop broadcast messages.
		 */
		if (fstat(ttyfd, &statbuf) < 0
		    || fchmod(ttyfd, statbuf.st_mode & ~(S_IWGRP | S_IWOTH)) < 0) {
			warn("Couldn't restrict write permissions to %s: %m", devnam);
		} else
			tty_mode = statbuf.st_mode;
#endif /* __linux__ */

		/*
		 * Set line speed, flow control, etc.
		 * If we have a non-null connection or initializer script,
		 * on most systems we set CLOCAL for now so that we can talk
		 * to the modem before carrier comes up.  But this has the
		 * side effect that we might miss it if CD drops before we
		 * get to clear CLOCAL below.  On systems where we can talk
		 * successfully to the modem with CLOCAL clear and CD down,
		 * we could clear CLOCAL at this point.
		 */
		set_up_tty(ttyfd, ((connector != NULL && connector[0] != 0)
				   || initializer != NULL));
	}

	/*
	 * If the pty, socket, notty and/or record option was specified,
	 * start up the character shunt now.
	 */
	status = EXIT_PTYCMD_FAILED;
	if (ptycommand != NULL) {
			if (device_script(ptycommand, pty_master, pty_master, 1) < 0)
				goto errret;
	} else if (pty_socket != NULL) {
		int fd = open_socket(pty_socket);
		if (fd < 0)
			goto errret;
		if (!start_charshunt(fd, fd))
			goto errret;
		close(fd);
	} else if (notty) {
		if (!start_charshunt(0, 1))
			goto errret;
		dup2(fd_devnull, 0);
		dup2(fd_devnull, 1);
		if (log_to_fd == 1)
			log_to_fd = -1;
		if (log_to_fd != 2)
			dup2(fd_devnull, 2);
	}

	if (using_pty || record_file != NULL) {
		ttyfd = pty_slave;
		close(pty_master);
		pty_master = -1;
	}

	/* run connection script */
	if ((connector && connector[0]) || initializer) {
		if (real_ttyfd != -1) {
			/* XXX do this if doing_callback == CALLBACK_DIALIN? */
			if (!default_device && modem) {
				setdtr(real_ttyfd, 0);	/* in case modem is off hook */
				sleep(1);
				setdtr(real_ttyfd, 1);
			}
		}

		if (initializer && initializer[0]) {
			if (device_script(initializer, ttyfd, ttyfd, 0) < 0) {
				error("Initializer script failed");
				status = EXIT_INIT_FAILED;
				goto errretf;
			}
			if (got_sigterm) {
				disconnect_tty();
				goto errretf;
			}
			info("Serial port initialized.");
		}

		if (connector && connector[0]) {
			if (device_script(connector, ttyfd, ttyfd, 0) < 0) {
				error("Connect script failed");
				status = EXIT_CONNECT_FAILED;
				goto errretf;
			}
			if (got_sigterm) {
				disconnect_tty();
				goto errretf;
			}
			info("Serial connection established.");
		}

		/* set line speed, flow control, etc.;
		   clear CLOCAL if modem option */
		if (real_ttyfd != -1)
			set_up_tty(real_ttyfd, 0);

		if (doing_callback == CALLBACK_DIALIN)
			connector = NULL;
	}

	/* reopen tty if necessary to wait for carrier */
	if (connector == NULL && modem && devnam[0] != 0) {
		int i;
		for (;;) {
			if ((i = open(devnam, O_RDWR)) >= 0)
				break;
			if (errno != EINTR) {
				error("Failed to reopen %s: %m", devnam);
				status = EXIT_OPEN_FAILED;
			}
			if (!persist || errno != EINTR || hungup || got_sigterm)
				goto errret;
		}
		close(i);
	}

	slprintf(numbuf, sizeof(numbuf), "%d", baud_rate);
	script_setenv("SPEED", numbuf, 0);

	/* run welcome script, if any */
	if (welcomer && welcomer[0]) {
		if (device_script(welcomer, ttyfd, ttyfd, 0) < 0)
			warn("Welcome script failed");
	}

	/*
	 * If we are initiating this connection, wait for a short
	 * time for something from the peer.  This can avoid bouncing
	 * our packets off his tty before he has it set up.
	 */
	if (connector != NULL || ptycommand != NULL || pty_socket != NULL)
		listen_time = connect_delay;

	return ttyfd;

 errretf:
	if (real_ttyfd >= 0)
		tcflush(real_ttyfd, TCIOFLUSH);
 errret:
	if (pty_master >= 0) {
		close(pty_master);
		pty_master = -1;
	}
	ttyfd = -1;
	if (got_sigterm)
		asked_to_quit = 1;
	return -1;
}


void disconnect_tty()
{
	if (disconnect_script == NULL || hungup)
		return;
	if (real_ttyfd >= 0)
		set_up_tty(real_ttyfd, 1);
	if (device_script(disconnect_script, ttyfd, ttyfd, 0) < 0) {
		warn("disconnect script failed");
	} else {
		info("Serial link disconnected.");
	}
	stop_charshunt(NULL, 0);
}

void tty_close_fds()
{
	if (pty_slave >= 0)
		close(pty_slave);
	if (real_ttyfd >= 0) {
		close(real_ttyfd);
		real_ttyfd = -1;
	}
	/* N.B. ttyfd will == either pty_slave or real_ttyfd */
}

void cleanup_tty()
{
	if (real_ttyfd >= 0)
		finish_tty();
	tty_close_fds();
	if (locked) {
		unlock();
		locked = 0;
	}
}

/*
 * tty_do_send_config - set transmit-side PPP configuration.
 * We set the extended transmit ACCM here as well.
 */
void
tty_do_send_config(mtu, accm, pcomp, accomp)
    int mtu;
    u_int32_t accm;
    int pcomp, accomp;
{
	tty_set_xaccm(xmit_accm);
	tty_send_config(mtu, accm, pcomp, accomp);
}

/*
 * finish_tty - restore the terminal device to its original settings
 */
static void
finish_tty()
{
	/* drop dtr to hang up */
	if (!default_device && modem) {
		setdtr(real_ttyfd, 0);
		/*
		 * This sleep is in case the serial port has CLOCAL set by default,
		 * and consequently will reassert DTR when we close the device.
		 */
		sleep(1);
	}

	restore_tty(real_ttyfd);

#ifndef __linux__
	if (tty_mode != (mode_t) -1) {
		if (fchmod(real_ttyfd, tty_mode) != 0)
			error("Couldn't restore tty permissions");
	}
#endif /* __linux__ */

	close(real_ttyfd);
	real_ttyfd = -1;
}

/*
 * maybe_relock - our PID has changed, maybe update the lock file.
 */
static void
maybe_relock(arg, pid)
    void *arg;
    int pid;
{
    if (locked)
	relock(pid);
}

/*
 * open_socket - establish a stream socket connection to the nominated
 * host and port.
 */
static int
open_socket(dest)
    char *dest;
{
    char *sep, *endp = NULL;
    int sock, port = -1;
    u_int32_t host;
    struct hostent *hent;
    struct sockaddr_in sad;

    /* parse host:port and resolve host to an IP address */
    sep = strchr(dest, ':');
    if (sep != NULL)
	port = strtol(sep+1, &endp, 10);
    if (port < 0 || endp == sep+1 || sep == dest) {
	error("Can't parse host:port for socket destination");
	return -1;
    }
    *sep = 0;
    host = inet_addr(dest);
    if (host == (u_int32_t) -1) {
	hent = gethostbyname(dest);
	if (hent == NULL) {
	    error("%s: unknown host in socket option", dest);
	    *sep = ':';
	    return -1;
	}
	host = *(u_int32_t *)(hent->h_addr_list[0]);
    }
    *sep = ':';

    /* get a socket and connect it to the other end */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
	error("Can't create socket: %m");
	return -1;
    }
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_port = htons(port);
    sad.sin_addr.s_addr = host;
    if (connect(sock, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
	error("Can't connect to %s: %m", dest);
	close(sock);
	return -1;
    }

    return sock;
}


/*
 * start_charshunt - create a child process to run the character shunt.
 */
static int
start_charshunt(ifd, ofd)
    int ifd, ofd;
{
    int cpid;

    cpid = safe_fork(ifd, ofd, (log_to_fd >= 0? log_to_fd: 2));
    if (cpid == -1) {
	error("Can't fork process for character shunt: %m");
	return 0;
    }
    if (cpid == 0) {
	/* child */
	reopen_log();
	if (!nodetach)
	    log_to_fd = -1;
	else if (log_to_fd >= 0)
	    log_to_fd = 2;
	setgid(getgid());
	setuid(uid);
	if (getuid() != uid)
	    fatal("setuid failed");
	charshunt(0, 1, record_file);
	exit(0);
    }
    charshunt_pid = cpid;
    record_child(cpid, "pppd (charshunt)", charshunt_done, NULL, 1);
    return 1;
}

static void
charshunt_done(arg)
    void *arg;
{
	charshunt_pid = 0;
}

static void
stop_charshunt(arg, sig)
    void *arg;
    int sig;
{
	if (charshunt_pid)
		kill(charshunt_pid, (sig == SIGINT? sig: SIGTERM));
}

/*
 * charshunt - the character shunt, which passes characters between
 * the pty master side and the serial port (or stdin/stdout).
 * This runs as the user (not as root).
 * (We assume ofd >= ifd which is true the way this gets called. :-).
 */
static void
charshunt(ifd, ofd, record_file)
    int ifd, ofd;
    char *record_file;
{
    int n, nfds;
    fd_set ready, writey;
    u_char *ibufp, *obufp;
    int nibuf, nobuf;
    int flags;
    int pty_readable, stdin_readable;
    struct timeval lasttime;
    FILE *recordf = NULL;
    int ilevel, olevel, max_level;
    struct timeval levelt, tout, *top;
    extern u_char inpacket_buf[];

    /*
     * Reset signal handlers.
     */
    signal(SIGHUP, SIG_IGN);		/* Hangup */
    signal(SIGINT, SIG_DFL);		/* Interrupt */
    signal(SIGTERM, SIG_DFL);		/* Terminate */
    signal(SIGCHLD, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
    signal(SIGUSR2, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGALRM, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
#ifdef SIGBUS
    signal(SIGBUS, SIG_DFL);
#endif
#ifdef SIGEMT
    signal(SIGEMT, SIG_DFL);
#endif
#ifdef SIGPOLL
    signal(SIGPOLL, SIG_DFL);
#endif
#ifdef SIGPROF
    signal(SIGPROF, SIG_DFL);
#endif
#ifdef SIGSYS
    signal(SIGSYS, SIG_DFL);
#endif
#ifdef SIGTRAP
    signal(SIGTRAP, SIG_DFL);
#endif
#ifdef SIGVTALRM
    signal(SIGVTALRM, SIG_DFL);
#endif
#ifdef SIGXCPU
    signal(SIGXCPU, SIG_DFL);
#endif
#ifdef SIGXFSZ
    signal(SIGXFSZ, SIG_DFL);
#endif

    /*
     * Check that the fds won't overrun the fd_sets
     */
    if (ifd >= FD_SETSIZE || ofd >= FD_SETSIZE || pty_master >= FD_SETSIZE)
	fatal("internal error: file descriptor too large (%d, %d, %d)",
	      ifd, ofd, pty_master);

    /* set all the fds to non-blocking mode */
    flags = fcntl(pty_master, F_GETFL);
    if (flags == -1
	|| fcntl(pty_master, F_SETFL, flags | O_NONBLOCK) == -1)
	warn("couldn't set pty master to nonblock: %m");
    flags = fcntl(ifd, F_GETFL);
    if (flags == -1
	|| fcntl(ifd, F_SETFL, flags | O_NONBLOCK) == -1)
	warn("couldn't set %s to nonblock: %m", (ifd==0? "stdin": "tty"));
    if (ofd != ifd) {
	flags = fcntl(ofd, F_GETFL);
	if (flags == -1
	    || fcntl(ofd, F_SETFL, flags | O_NONBLOCK) == -1)
	    warn("couldn't set stdout to nonblock: %m");
    }

    nibuf = nobuf = 0;
    ibufp = obufp = NULL;
    pty_readable = stdin_readable = 1;

    ilevel = olevel = 0;
    gettimeofday(&levelt, NULL);
    if (max_data_rate) {
	max_level = max_data_rate / 10;
	if (max_level < 100)
	    max_level = 100;
    } else
	max_level = PPP_MRU + PPP_HDRLEN + 1;

    nfds = (ofd > pty_master? ofd: pty_master) + 1;
    if (recordf != NULL) {
	gettimeofday(&lasttime, NULL);
	putc(7, recordf);	/* put start marker */
	putc(lasttime.tv_sec >> 24, recordf);
	putc(lasttime.tv_sec >> 16, recordf);
	putc(lasttime.tv_sec >> 8, recordf);
	putc(lasttime.tv_sec, recordf);
	lasttime.tv_usec = 0;
    }

    while (nibuf != 0 || nobuf != 0 || pty_readable || stdin_readable) {
	top = 0;
	tout.tv_sec = 0;
	tout.tv_usec = 10000;
	FD_ZERO(&ready);
	FD_ZERO(&writey);
	if (nibuf != 0) {
	    if (ilevel >= max_level)
		top = &tout;
	    else
		FD_SET(pty_master, &writey);
	} else if (stdin_readable)
	    FD_SET(ifd, &ready);
	if (nobuf != 0) {
	    if (olevel >= max_level)
		top = &tout;
	    else
		FD_SET(ofd, &writey);
	} else if (pty_readable)
	    FD_SET(pty_master, &ready);
	if (select(nfds, &ready, &writey, NULL, top) < 0) {
	    if (errno != EINTR)
		fatal("select");
	    continue;
	}
	if (max_data_rate) {
	    double dt;
	    int nbt;
	    struct timeval now;

	    gettimeofday(&now, NULL);
	    dt = (now.tv_sec - levelt.tv_sec
		  + (now.tv_usec - levelt.tv_usec) / 1e6);
	    nbt = (int)(dt * max_data_rate);
	    ilevel = (nbt < 0 || nbt > ilevel)? 0: ilevel - nbt;
	    olevel = (nbt < 0 || nbt > olevel)? 0: olevel - nbt;
	    levelt = now;
	} else
	    ilevel = olevel = 0;
	if (FD_ISSET(ifd, &ready)) {
	    ibufp = inpacket_buf;
	    nibuf = read(ifd, ibufp, PPP_MRU + PPP_HDRLEN);
	    if (nibuf < 0 && errno == EIO)
		nibuf = 0;
	    if (nibuf < 0) {
		if (!(errno == EINTR || errno == EAGAIN)) {
		    error("Error reading standard input: %m");
		    break;
		}
		nibuf = 0;
	    } else if (nibuf == 0) {
		/* end of file from stdin */
		stdin_readable = 0;
		if (recordf)
		    if (!record_write(recordf, 4, NULL, 0, &lasttime))
			recordf = NULL;
	    } else {
		FD_SET(pty_master, &writey);
		if (recordf)
		    if (!record_write(recordf, 2, ibufp, nibuf, &lasttime))
			recordf = NULL;
	    }
	}
	if (FD_ISSET(pty_master, &ready)) {
	    obufp = outpacket_buf;
	    nobuf = read(pty_master, obufp, PPP_MRU + PPP_HDRLEN);
	    if (nobuf < 0 && errno == EIO)
		nobuf = 0;
	    if (nobuf < 0) {
		if (!(errno == EINTR || errno == EAGAIN)) {
		    error("Error reading pseudo-tty master: %m");
		    break;
		}
		nobuf = 0;
	    } else if (nobuf == 0) {
		/* end of file from the pty - slave side has closed */
		pty_readable = 0;
		stdin_readable = 0;	/* pty is not writable now */
		nibuf = 0;
		close(ofd);
		if (recordf)
		    if (!record_write(recordf, 3, NULL, 0, &lasttime))
			recordf = NULL;
	    } else {
		FD_SET(ofd, &writey);
		if (recordf)
		    if (!record_write(recordf, 1, obufp, nobuf, &lasttime))
			recordf = NULL;
	    }
	} else if (!stdin_readable)
	    pty_readable = 0;
	if (FD_ISSET(ofd, &writey)) {
	    n = nobuf;
	    if (olevel + n > max_level)
		n = max_level - olevel;
	    n = write(ofd, obufp, n);
	    if (n < 0) {
		if (errno == EIO) {
		    pty_readable = 0;
		    nobuf = 0;
		} else if (errno != EAGAIN && errno != EINTR) {
		    error("Error writing standard output: %m");
		    break;
		}
	    } else {
		obufp += n;
		nobuf -= n;
		olevel += n;
	    }
	}
	if (FD_ISSET(pty_master, &writey)) {
	    n = nibuf;
	    if (ilevel + n > max_level)
		n = max_level - ilevel;
	    n = write(pty_master, ibufp, n);
	    if (n < 0) {
		if (errno == EIO) {
		    stdin_readable = 0;
		    nibuf = 0;
		} else if (errno != EAGAIN && errno != EINTR) {
		    error("Error writing pseudo-tty master: %m");
		    break;
		}
	    } else {
		ibufp += n;
		nibuf -= n;
		ilevel += n;
	    }
	}
    }
    exit(0);
}

static int
record_write(f, code, buf, nb, tp)
    FILE *f;
    int code;
    u_char *buf;
    int nb;
    struct timeval *tp;
{
    struct timeval now;
    int diff;

    gettimeofday(&now, NULL);
    now.tv_usec /= 100000;	/* actually 1/10 s, not usec now */
    diff = (now.tv_sec - tp->tv_sec) * 10 + (now.tv_usec - tp->tv_usec);
    if (diff > 0) {
	if (diff > 255) {
	    putc(5, f);
	    putc(diff >> 24, f);
	    putc(diff >> 16, f);
	    putc(diff >> 8, f);
	    putc(diff, f);
	} else {
	    putc(6, f);
	    putc(diff, f);
	}
	*tp = now;
    }
    putc(code, f);
    if (buf != NULL) {
	putc(nb >> 8, f);
	putc(nb, f);
	fwrite(buf, nb, 1, f);
    }
    fflush(f);
    if (ferror(f)) {
	error("Error writing record file: %m");
	return 0;
    }
    return 1;
}
