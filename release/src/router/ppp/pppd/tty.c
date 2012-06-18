/*
 * tty.c - code for handling serial ports in pppd.
 *
 * Copyright (C) 2000 Paul Mackerras.
 * All rights reserved.
 *
 * Portions Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define RCSID	"$Id: tty.c,v 1.1.1.4 2003/10/14 08:09:53 sparq Exp $"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
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
bool	modem = 1;		/* Use modem control lines */
int	inspeed = 0;		/* Input/Output speed requested */
bool	lockflag = 0;		/* Create lock file to lock the serial dev */
char	*initializer = NULL;	/* Script to initialize physical link */
char	*connect_script = NULL;	/* Script to establish physical link */
char	*disconnect_script = NULL; /* Script to disestablish physical link */
char	*welcomer = NULL;	/* Script to run after phys link estab. */
char	*ptycommand = NULL;	/* Command to run on other side of pty */
bool	notty = 0;		/* Stdin/out is not a tty */
char	*record_file = NULL;	/* File to record chars sent/received */
int	max_data_rate;		/* max bytes/sec through charshunt */
bool	sync_serial = 0;	/* Device is synchronous serial device */
char	*pty_socket = NULL;	/* Socket to connect to pty */
int	using_pty = 0;		/* we're allocating a pty as the device */

extern uid_t uid;
extern int kill_link;

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

    { "record", o_string, &record_file,
      "Record characters sent/received to file", OPT_PRIO },

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

	if (strncmp("/dev/", cp, 5) != 0) {
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

	if (demand && connect_script == 0) {
		option_error("connect script is required for demand-dialling\n");
		exit(EXIT_OPTION_ERROR);
	}
	/* default holdoff to 0 if no connect script has been given */
	if (connect_script == 0 && !holdoff_specified)
		holdoff = 0;

	if (using_pty) {
		if (!default_device) {
			option_error("%s option precludes specifying device name",
				     notty? "notty": "pty");
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
	struct stat statbuf;
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
			return -1;
		locked = 1;
	}

	/*
	 * Open the serial device and set it up to be the ppp interface.
	 * First we open it in non-blocking mode so we can set the
	 * various termios flags appropriately.  If we aren't dialling
	 * out and we want to use the modem lines, we reopen it later
	 * in order to wait for the carrier detect signal from the modem.
	 */
	hungup = 0;
	kill_link = 0;
	connector = doing_callback? callback_script: connect_script;
	if (devnam[0] != 0) {
		for (;;) {
			/* If the user specified the device name, become the
			   user before opening it. */
			int err, prio;

			prio = privopen? OPRIO_ROOT: tty_options[0].priority;
			if (prio < OPRIO_ROOT)
				seteuid(uid);
			ttyfd = open(devnam, O_NONBLOCK | O_RDWR, 0);
			err = errno;
			if (prio < OPRIO_ROOT)
				seteuid(0);
			if (ttyfd >= 0)
				break;
			errno = err;
			if (err != EINTR) {
				error("Failed to open %s: %m", devnam);
				status = EXIT_OPEN_FAILED;
			}
			if (!persist || err != EINTR)
				return -1;
		}
		real_ttyfd = ttyfd;
		if ((fdflags = fcntl(ttyfd, F_GETFL)) == -1
		    || fcntl(ttyfd, F_SETFL, fdflags & ~O_NONBLOCK) < 0)
			warn("Couldn't reset non-blocking mode on device: %m");

		/*
		 * Do the equivalent of `mesg n' to stop broadcast messages.
		 */
		if (fstat(ttyfd, &statbuf) < 0
		    || fchmod(ttyfd, statbuf.st_mode & ~(S_IWGRP | S_IWOTH)) < 0) {
			warn("Couldn't restrict write permissions to %s: %m", devnam);
		} else
			tty_mode = statbuf.st_mode;

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
		if (record_file != NULL) {
			int ipipe[2], opipe[2], ok;

			if (pipe(ipipe) < 0 || pipe(opipe) < 0)
				fatal("Couldn't create pipes for record option: %m");
			ok = device_script(ptycommand, opipe[0], ipipe[1], 1) == 0
				&& start_charshunt(ipipe[0], opipe[1]);
			close(ipipe[0]);
			close(ipipe[1]);
			close(opipe[0]);
			close(opipe[1]);
			if (!ok)
				return -1;
		} else {
			if (device_script(ptycommand, pty_master, pty_master, 1) < 0)
				return -1;
			ttyfd = pty_slave;
			close(pty_master);
			pty_master = -1;
		}
	} else if (pty_socket != NULL) {
		int fd = open_socket(pty_socket);
		if (fd < 0)
			return -1;
		if (!start_charshunt(fd, fd))
			return -1;
	} else if (notty) {
		if (!start_charshunt(0, 1))
			return -1;
	} else if (record_file != NULL) {
		if (!start_charshunt(ttyfd, ttyfd))
			return -1;
	}

	/* run connection script */
	if ((connector && connector[0]) || initializer) {
		if (real_ttyfd != -1) {
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
				return -1;
			}
			if (kill_link) {
				disconnect_tty();
				return -1;
			}
			info("Serial port initialized.");
		}

		if (connector && connector[0]) {
			if (device_script(connector, ttyfd, ttyfd, 0) < 0) {
				error("Connect script failed");
				status = EXIT_CONNECT_FAILED;
				return -1;
			}
			if (kill_link) {
				disconnect_tty();
				return -1;
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
			if (!persist || errno != EINTR || hungup || kill_link)
				return -1;
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
	if (connector != NULL || ptycommand != NULL)
		listen_time = connect_delay;

	return ttyfd;
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
}

void tty_close_fds()
{
	if (pty_master >= 0)
		close(pty_master);
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

	if (tty_mode != (mode_t) -1) {
		if (fchmod(real_ttyfd, tty_mode) != 0) {
			chmod(devnam, tty_mode);
		}
	}

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

    cpid = fork();
    if (cpid == -1) {
	error("Can't fork process for character shunt: %m");
	return 0;
    }
    if (cpid == 0) {
	/* child */
	close(pty_slave);
	setuid(uid);
	if (getuid() != uid)
	    fatal("setuid failed");
	setgid(getgid());
	if (!nodetach)
	    log_to_fd = -1;
	charshunt(ifd, ofd, record_file);
	exit(0);
    }
    charshunt_pid = cpid;
    add_notifier(&sigreceived, stop_charshunt, 0);
    close(pty_master);
    pty_master = -1;
    ttyfd = pty_slave;
    record_child(cpid, "pppd (charshunt)", charshunt_done, NULL);
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
     * Open the record file if required.
     */
    if (record_file != NULL) {
	recordf = fopen(record_file, "a");
	if (recordf == NULL)
	    error("Couldn't create record file %s: %m", record_file);
    }

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
		/* do a 0-length write, hopefully this will generate
		   an EOF (hangup) on the slave side. */
		write(pty_master, inpacket_buf, 0);
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
	}
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
