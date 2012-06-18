/* vi: set sw=4 ts=4: */
/* agetty.c - another getty program for Linux. By W. Z. Venema 1989
 * Ported to Linux by Peter Orbaek <poe@daimi.aau.dk>
 * This program is freely distributable. The entire man-page used to
 * be here. Now read the real man-page agetty.8 instead.
 *
 * option added by Eric Rasmussen <ear@usfirst.org> - 12/28/95
 *
 * 1999-02-22 Arkadiusz Mickiewicz <misiek@misiek.eu.org>
 * - added Native Language Support
 *
 * 1999-05-05 Thorsten Kranzkowski <dl8bcu@gmx.net>
 * - enable hardware flow control before displaying /etc/issue
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <syslog.h>

#if ENABLE_FEATURE_UTMP
# include <utmp.h> /* LOGIN_PROCESS */
#endif

#ifndef IUCLC
# define IUCLC 0
#endif

/*
 * Some heuristics to find out what environment we are in: if it is not
 * System V, assume it is SunOS 4.
 */
#ifdef LOGIN_PROCESS                    /* defined in System V utmp.h */
#include <sys/utsname.h>
#else /* if !sysV style, wtmp/utmp code is off */
#undef ENABLE_FEATURE_UTMP
#undef ENABLE_FEATURE_WTMP
#define ENABLE_FEATURE_UTMP 0
#define ENABLE_FEATURE_WTMP 0
#endif  /* LOGIN_PROCESS */

/*
 * Things you may want to modify.
 *
 * You may disagree with the default line-editing etc. characters defined
 * below. Note, however, that DEL cannot be used for interrupt generation
 * and for line editing at the same time.
 */

/* I doubt there are systems which still need this */
#undef HANDLE_ALLCAPS
#undef ANCIENT_BS_KILL_CHARS

#define _PATH_LOGIN "/bin/login"

/* If ISSUE is not defined, getty will never display the contents of the
 * /etc/issue file. You will not want to spit out large "issue" files at the
 * wrong baud rate.
 */
#define ISSUE "/etc/issue"              /* displayed before the login prompt */

/* Some shorthands for control characters. */
#define CTL(x)          ((x) ^ 0100)    /* Assumes ASCII dialect */
#define CR              CTL('M')        /* carriage return */
#define NL              CTL('J')        /* line feed */
#define BS              CTL('H')        /* back space */
#define DEL             CTL('?')        /* delete */

/* Defaults for line-editing etc. characters; you may want to change this. */
#define DEF_ERASE       DEL             /* default erase character */
#define DEF_INTR        CTL('C')        /* default interrupt character */
#define DEF_QUIT        CTL('\\')       /* default quit char */
#define DEF_KILL        CTL('U')        /* default kill char */
#define DEF_EOF         CTL('D')        /* default EOF char */
#define DEF_EOL         '\n'
#define DEF_SWITCH      0               /* default switch char */

/*
 * When multiple baud rates are specified on the command line, the first one
 * we will try is the first one specified.
 */
#define MAX_SPEED       10              /* max. nr. of baud rates */

/* Storage for command-line options. */
struct options {
	int flags;                      /* toggle switches, see below */
	unsigned timeout;               /* time-out period */
	const char *login;              /* login program */
	const char *tty;                /* name of tty */
	const char *initstring;         /* modem init string */
	const char *issue;              /* alternative issue file */
	int numspeed;                   /* number of baud rates to try */
	int speeds[MAX_SPEED];          /* baud rates to be tried */
};

/* Storage for things detected while the login name was read. */
struct chardata {
	unsigned char erase;    /* erase character */
	unsigned char kill;     /* kill character */
	unsigned char eol;      /* end-of-line character */
	unsigned char parity;   /* what parity did we see */
	/* (parity & 1): saw odd parity char with 7th bit set */
	/* (parity & 2): saw even parity char with 7th bit set */
	/* parity == 0: probably 7-bit, space parity? */
	/* parity == 1: probably 7-bit, odd parity? */
	/* parity == 2: probably 7-bit, even parity? */
	/* parity == 3: definitely 8 bit, no parity! */
	/* Hmm... with any value of "parity" 8 bit, no parity is possible */
#ifdef HANDLE_ALLCAPS
	unsigned char capslock; /* upper case without lower case */
#endif
};


/* Initial values for the above. */
static const struct chardata init_chardata = {
	DEF_ERASE,                              /* default erase character */
	DEF_KILL,                               /* default kill character */
	13,                                     /* default eol char */
	0,                                      /* space parity */
#ifdef HANDLE_ALLCAPS
	0,                                      /* no capslock */
#endif
};

static const char opt_string[] ALIGN1 = "I:LH:f:hil:mt:wn";
#define F_INITSTRING    (1 << 0)        /* -I initstring is set */
#define F_LOCAL         (1 << 1)        /* -L force local */
#define F_FAKEHOST      (1 << 2)        /* -H fake hostname */
#define F_CUSTISSUE     (1 << 3)        /* -f give alternative issue file */
#define F_RTSCTS        (1 << 4)        /* -h enable RTS/CTS flow control */
#define F_ISSUE         (1 << 5)        /* -i display /etc/issue */
#define F_LOGIN         (1 << 6)        /* -l non-default login program */
#define F_PARSE         (1 << 7)        /* -m process modem status messages */
#define F_TIMEOUT       (1 << 8)        /* -t time out */
#define F_WAITCRLF      (1 << 9)        /* -w wait for CR or LF */
#define F_NOPROMPT      (1 << 10)       /* -n don't ask for login name */


#define line_buf bb_common_bufsiz1

/* The following is used for understandable diagnostics. */
#ifdef DEBUGGING
static FILE *dbf;
#define DEBUGTERM "/dev/ttyp0"
#define debug(...) do { fprintf(dbf, __VA_ARGS__); fflush(dbf); } while (0)
#else
#define debug(...) ((void)0)
#endif


/* bcode - convert speed string to speed code; return <= 0 on failure */
static int bcode(const char *s)
{
	int value = bb_strtou(s, NULL, 10); /* yes, int is intended! */
	if (value < 0) /* bad terminating char, overflow, etc */
		return value;
	return tty_value_to_baud(value);
}

/* parse_speeds - parse alternate baud rates */
static void parse_speeds(struct options *op, char *arg)
{
	char *cp;

	/* NB: at least one iteration is always done */
	debug("entered parse_speeds\n");
	while ((cp = strsep(&arg, ",")) != NULL) {
		op->speeds[op->numspeed] = bcode(cp);
		if (op->speeds[op->numspeed] < 0)
			bb_error_msg_and_die("bad speed: %s", cp);
		/* note: arg "0" turns into speed B0 */
		op->numspeed++;
		if (op->numspeed > MAX_SPEED)
			bb_error_msg_and_die("too many alternate speeds");
	}
	debug("exiting parse_speeds\n");
}

/* parse_args - parse command-line arguments */
static void parse_args(char **argv, struct options *op, char **fakehost_p)
{
	char *ts;

	opt_complementary = "-2:t+"; /* at least 2 args; -t N */
	op->flags = getopt32(argv, opt_string,
		&(op->initstring), fakehost_p, &(op->issue),
		&(op->login), &op->timeout);
	argv += optind;
	if (op->flags & F_INITSTRING) {
		const char *p = op->initstring;
		char *q;

		op->initstring = q = xstrdup(p);
		/* copy optarg into op->initstring decoding \ddd
		   octal codes into chars */
		while (*p) {
			if (*p == '\\') {
				p++;
				*q++ = bb_process_escape_sequence(&p);
			} else {
				*q++ = *p++;
			}
		}
		*q = '\0';
	}
	op->flags ^= F_ISSUE;           /* invert flag "show /etc/issue" */
	debug("after getopt\n");

	/* we loosen up a bit and accept both "baudrate tty" and "tty baudrate" */
	op->tty = argv[0];      /* tty name */
	ts = argv[1];           /* baud rate(s) */
	if (isdigit(argv[0][0])) {
		/* a number first, assume it's a speed (BSD style) */
		op->tty = ts;   /* tty name is in argv[1] */
		ts = argv[0];   /* baud rate(s) */
	}
	parse_speeds(op, ts);

// TODO: if applet_name is set to "getty: TTY", bb_error_msg's get simpler!
// grep for "%s:"

	if (argv[2])
		xsetenv("TERM", argv[2]);

	debug("exiting parse_args\n");
}

/* open_tty - set up tty as standard { input, output, error } */
static void open_tty(const char *tty)
{
	/* Set up new standard input, unless we are given an already opened port. */
	if (NOT_LONE_DASH(tty)) {
//		struct stat st;
//		int cur_dir_fd;
//		int fd;

		/* Sanity checks... */
//		cur_dir_fd = xopen(".", O_DIRECTORY | O_NONBLOCK);
//		xchdir("/dev");
//		xstat(tty, &st);
//		if (!S_ISCHR(st.st_mode))
//			bb_error_msg_and_die("%s: not a character device", tty);

		if (tty[0] != '/')
			tty = xasprintf("/dev/%s", tty); /* will leak it */

		/* Open the tty as standard input. */
		debug("open(2)\n");
		close(0);
		/*fd =*/ xopen(tty, O_RDWR | O_NONBLOCK); /* uses fd 0 */

//		/* Restore current directory */
//		fchdir(cur_dir_fd);

		/* Open the tty as standard input, continued */
//		xmove_fd(fd, 0);
//		/* fd is >= cur_dir_fd, and cur_dir_fd gets closed too here: */
//		while (fd > 2)
//			close(fd--);

		/* Set proper protections and ownership. */
		fchown(0, 0, 0);        /* 0:0 */
		fchmod(0, 0620);        /* crw--w---- */
	} else {
		/*
		 * Standard input should already be connected to an open port. Make
		 * sure it is open for read/write.
		 */
		if ((fcntl(0, F_GETFL) & O_RDWR) != O_RDWR)
			bb_error_msg_and_die("stdin is not open for read/write");
	}
}

/* termios_init - initialize termios settings */
static void termios_init(struct termios *tp, int speed, struct options *op)
{
	speed_t ispeed, ospeed;
	/*
	 * Initial termios settings: 8-bit characters, raw-mode, blocking i/o.
	 * Special characters are set after we have read the login name; all
	 * reads will be done in raw mode anyway. Errors will be dealt with
	 * later on.
	 */
#ifdef __linux__
	/* flush input and output queues, important for modems! */
	ioctl(0, TCFLSH, TCIOFLUSH); /* tcflush(0, TCIOFLUSH)? - same */
#endif
	ispeed = ospeed = speed;
	if (speed == B0) {
		/* Speed was specified as "0" on command line.
		 * Just leave it unchanged */
		ispeed = cfgetispeed(tp);
		ospeed = cfgetospeed(tp);
	}
	tp->c_cflag = CS8 | HUPCL | CREAD;
	if (op->flags & F_LOCAL)
		tp->c_cflag |= CLOCAL;
	cfsetispeed(tp, ispeed);
	cfsetospeed(tp, ospeed);

	tp->c_iflag = tp->c_lflag = tp->c_line = 0;
	tp->c_oflag = OPOST | ONLCR;
	tp->c_cc[VMIN] = 1;
	tp->c_cc[VTIME] = 0;

	/* Optionally enable hardware flow control */
#ifdef CRTSCTS
	if (op->flags & F_RTSCTS)
		tp->c_cflag |= CRTSCTS;
#endif

	tcsetattr_stdin_TCSANOW(tp);

	debug("term_io 2\n");
}

/* auto_baud - extract baud rate from modem status message */
static void auto_baud(char *buf, unsigned size_buf, struct termios *tp)
{
	int speed;
	int vmin;
	unsigned iflag;
	char *bp;
	int nread;

	/*
	 * This works only if the modem produces its status code AFTER raising
	 * the DCD line, and if the computer is fast enough to set the proper
	 * baud rate before the message has gone by. We expect a message of the
	 * following format:
	 *
	 * <junk><number><junk>
	 *
	 * The number is interpreted as the baud rate of the incoming call. If the
	 * modem does not tell us the baud rate within one second, we will keep
	 * using the current baud rate. It is advisable to enable BREAK
	 * processing (comma-separated list of baud rates) if the processing of
	 * modem status messages is enabled.
	 */

	/*
	 * Use 7-bit characters, don't block if input queue is empty. Errors will
	 * be dealt with later on.
	 */
	iflag = tp->c_iflag;
	tp->c_iflag |= ISTRIP;          /* enable 8th-bit stripping */
	vmin = tp->c_cc[VMIN];
	tp->c_cc[VMIN] = 0;             /* don't block if queue empty */
	tcsetattr_stdin_TCSANOW(tp);

	/*
	 * Wait for a while, then read everything the modem has said so far and
	 * try to extract the speed of the dial-in call.
	 */
	sleep(1);
	nread = safe_read(STDIN_FILENO, buf, size_buf - 1);
	if (nread > 0) {
		buf[nread] = '\0';
		for (bp = buf; bp < buf + nread; bp++) {
			if (isdigit(*bp)) {
				speed = bcode(bp);
				if (speed > 0) {
					tp->c_cflag &= ~CBAUD;
					tp->c_cflag |= speed;
				}
				break;
			}
		}
	}

	/* Restore terminal settings. Errors will be dealt with later on. */
	tp->c_iflag = iflag;
	tp->c_cc[VMIN] = vmin;
	tcsetattr_stdin_TCSANOW(tp);
}

/* do_prompt - show login prompt, optionally preceded by /etc/issue contents */
static void do_prompt(struct options *op)
{
#ifdef ISSUE
	print_login_issue(op->issue, op->tty);
#endif
	print_login_prompt();
}

#ifdef HANDLE_ALLCAPS
/* all_is_upcase - string contains upper case without lower case */
/* returns 1 if true, 0 if false */
static int all_is_upcase(const char *s)
{
	while (*s)
		if (islower(*s++))
			return 0;
	return 1;
}
#endif

/* get_logname - get user name, establish parity, speed, erase, kill, eol;
 * return NULL on BREAK, logname on success */
static char *get_logname(char *logname, unsigned size_logname,
		struct options *op, struct chardata *cp)
{
	char *bp;
	char c;				/* input character, full eight bits */
	char ascval;                    /* low 7 bits of input character */
	int bits;                       /* # of "1" bits per character */
	int mask;                       /* mask with 1 bit up */
	static const char erase[][3] = {/* backspace-space-backspace */
		"\010\040\010",                 /* space parity */
		"\010\040\010",                 /* odd parity */
		"\210\240\210",                 /* even parity */
		"\010\040\010",                 /* 8 bit no parity */
	};

	/* NB: *cp is pre-initialized with init_chardata */

	/* Flush pending input (esp. after parsing or switching the baud rate). */
	sleep(1);
	ioctl(0, TCFLSH, TCIFLUSH); /* tcflush(0, TCIOFLUSH)? - same */

	/* Prompt for and read a login name. */
	logname[0] = '\0';
	while (!logname[0]) {
		/* Write issue file and prompt, with "parity" bit == 0. */
		do_prompt(op);

		/* Read name, watch for break, parity, erase, kill, end-of-line. */
		bp = logname;
		cp->eol = '\0';
		while (cp->eol == '\0') {

			/* Do not report trivial EINTR/EIO errors. */
			errno = EINTR; /* make read of 0 bytes be silent too */
			if (read(STDIN_FILENO, &c, 1) < 1) {
				if (errno == EINTR || errno == EIO)
					exit(EXIT_SUCCESS);
				bb_perror_msg_and_die("%s: read", op->tty);
			}

			/* BREAK. If we have speeds to try,
			 * return NULL (will switch speeds and return here) */
			if (c == '\0' && op->numspeed > 1)
				return NULL;

			/* Do parity bit handling. */
			if (!(op->flags & F_LOCAL) && (c & 0x80)) {       /* "parity" bit on? */
				bits = 1;
				mask = 1;
				while (mask & 0x7f) {
					if (mask & c)
						bits++; /* count "1" bits */
					mask <<= 1;
				}
				/* ... |= 2 - even, 1 - odd */
				cp->parity |= 2 - (bits & 1);
			}

			/* Do erase, kill and end-of-line processing. */
			ascval = c & 0x7f;
			switch (ascval) {
			case CR:
			case NL:
				*bp = '\0';             /* terminate logname */
				cp->eol = ascval;       /* set end-of-line char */
				break;
			case BS:
			case DEL:
#ifdef ANCIENT_BS_KILL_CHARS
			case '#':
#endif
				cp->erase = ascval;     /* set erase character */
				if (bp > logname) {
					full_write(STDOUT_FILENO, erase[cp->parity], 3);
					bp--;
				}
				break;
			case CTL('U'):
#ifdef ANCIENT_BS_KILL_CHARS
			case '@':
#endif
				cp->kill = ascval;      /* set kill character */
				while (bp > logname) {
					full_write(STDOUT_FILENO, erase[cp->parity], 3);
					bp--;
				}
				break;
			case CTL('D'):
				exit(EXIT_SUCCESS);
			default:
				if (ascval < ' ') {
					/* ignore garbage characters */
				} else if ((int)(bp - logname) >= size_logname - 1) {
					bb_error_msg_and_die("%s: input overrun", op->tty);
				} else {
					full_write(STDOUT_FILENO, &c, 1); /* echo the character */
					*bp++ = ascval; /* and store it */
				}
				break;
			}
		}
	}
	/* Handle names with upper case and no lower case. */

#ifdef HANDLE_ALLCAPS
	cp->capslock = all_is_upcase(logname);
	if (cp->capslock) {
		for (bp = logname; *bp; bp++)
			if (isupper(*bp))
				*bp = tolower(*bp);     /* map name to lower case */
	}
#endif
	return logname;
}

/* termios_final - set the final tty mode bits */
static void termios_final(struct options *op, struct termios *tp, struct chardata *cp)
{
	/* General terminal-independent stuff. */
	tp->c_iflag |= IXON | IXOFF;    /* 2-way flow control */
	tp->c_lflag |= ICANON | ISIG | ECHO | ECHOE | ECHOK | ECHOKE;
	/* no longer| ECHOCTL | ECHOPRT */
	tp->c_oflag |= OPOST;
	/* tp->c_cflag = 0; */
	tp->c_cc[VINTR] = DEF_INTR;     /* default interrupt */
	tp->c_cc[VQUIT] = DEF_QUIT;     /* default quit */
	tp->c_cc[VEOF] = DEF_EOF;       /* default EOF character */
	tp->c_cc[VEOL] = DEF_EOL;
	tp->c_cc[VSWTC] = DEF_SWITCH;   /* default switch character */

	/* Account for special characters seen in input. */
	if (cp->eol == CR) {
		tp->c_iflag |= ICRNL;   /* map CR in input to NL */
		tp->c_oflag |= ONLCR;   /* map NL in output to CR-NL */
	}
	tp->c_cc[VERASE] = cp->erase;   /* set erase character */
	tp->c_cc[VKILL] = cp->kill;     /* set kill character */

	/* Account for the presence or absence of parity bits in input. */
	switch (cp->parity) {
	case 0:                                 /* space (always 0) parity */
// I bet most people go here - they use only 7-bit chars in usernames....
		break;
	case 1:                                 /* odd parity */
		tp->c_cflag |= PARODD;
		/* FALLTHROUGH */
	case 2:                                 /* even parity */
		tp->c_cflag |= PARENB;
		tp->c_iflag |= INPCK | ISTRIP;
		/* FALLTHROUGH */
	case (1 | 2):                           /* no parity bit */
		tp->c_cflag &= ~CSIZE;
		tp->c_cflag |= CS7;
// FIXME: wtf? case 3: we saw both even and odd 8-bit bytes -
// it's probably some umlauts etc, but definitely NOT 7-bit!!!
// Entire parity detection madness here just begs for deletion...
		break;
	}

	/* Account for upper case without lower case. */
#ifdef HANDLE_ALLCAPS
	if (cp->capslock) {
		tp->c_iflag |= IUCLC;
		tp->c_lflag |= XCASE;
		tp->c_oflag |= OLCUC;
	}
#endif
	/* Optionally enable hardware flow control */
#ifdef CRTSCTS
	if (op->flags & F_RTSCTS)
		tp->c_cflag |= CRTSCTS;
#endif

	/* Finally, make the new settings effective */
	/* It's tcsetattr_stdin_TCSANOW() + error check */
	ioctl_or_perror_and_die(0, TCSETS, tp, "%s: TCSETS", op->tty);
}

int getty_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int getty_main(int argc UNUSED_PARAM, char **argv)
{
	int n;
	pid_t pid;
	char *fakehost = NULL;          /* Fake hostname for ut_host */
	char *logname;                  /* login name, given to /bin/login */
	/* Merging these into "struct local" may _seem_ to reduce
	 * parameter passing, but today's gcc will inline
	 * statics which are called once anyway, so don't do that */
	struct chardata chardata;       /* set by get_logname() */
	struct termios termios;         /* terminal mode bits */
	struct options options;

	chardata = init_chardata;

	memset(&options, 0, sizeof(options));
	options.login =	_PATH_LOGIN;    /* default login program */
	options.tty = "tty1";           /* default tty line */
	options.initstring = "";        /* modem init string */
#ifdef ISSUE
	options.issue = ISSUE;          /* default issue file */
#endif

	/* Parse command-line arguments. */
	parse_args(argv, &options, &fakehost);

	logmode = LOGMODE_NONE;

	/* Create new session, lose controlling tty, if any */
	/* docs/ctty.htm says:
	 * "This is allowed only when the current process
	 *  is not a process group leader" - is this a problem? */
	setsid();
	/* close stdio, and stray descriptors, just in case */
	n = xopen(bb_dev_null, O_RDWR);
	/* dup2(n, 0); - no, we need to handle "getty - 9600" too */
	xdup2(n, 1);
	xdup2(n, 2);
	while (n > 2)
		close(n--);

	/* Logging. We want special flavor of error_msg_and_die */
	die_sleep = 10;
	msg_eol = "\r\n";
	/* most likely will internally use fd #3 in CLOEXEC mode: */
	openlog(applet_name, LOG_PID, LOG_AUTH);
	logmode = LOGMODE_BOTH;

#ifdef DEBUGGING
	dbf = xfopen_for_write(DEBUGTERM);
	for (n = 1; argv[n]; n++) {
		debug(argv[n]);
		debug("\n");
	}
#endif

	/* Open the tty as standard input, if it is not "-" */
	/* If it's not "-" and not taken yet, it will become our ctty */
	debug("calling open_tty\n");
	open_tty(options.tty);
	ndelay_off(0);
	debug("duping\n");
	xdup2(0, 1);
	xdup2(0, 2);

	/*
	 * The following ioctl will fail if stdin is not a tty, but also when
	 * there is noise on the modem control lines. In the latter case, the
	 * common course of action is (1) fix your cables (2) give the modem more
	 * time to properly reset after hanging up. SunOS users can achieve (2)
	 * by patching the SunOS kernel variable "zsadtrlow" to a larger value;
	 * 5 seconds seems to be a good value.
	 */
	/* tcgetattr() + error check */
	ioctl_or_perror_and_die(0, TCGETS, &termios, "%s: TCGETS", options.tty);

	pid = getpid();
#ifdef __linux__
// FIXME: do we need this? Otherwise "-" case seems to be broken...
	// /* Forcibly make fd 0 our controlling tty, even if another session
	//  * has it as a ctty. (Another session loses ctty). */
	// ioctl(0, TIOCSCTTY, (void*)1);
	/* Make ourself a foreground process group within our session */
	tcsetpgrp(0, pid);
#endif

	/* Update the utmp file. This tty is ours now! */
	update_utmp(pid, LOGIN_PROCESS, options.tty, "LOGIN", fakehost);

	/* Initialize the termios settings (raw mode, eight-bit, blocking i/o). */
	debug("calling termios_init\n");
	termios_init(&termios, options.speeds[0], &options);

	/* Write the modem init string and DON'T flush the buffers */
	if (options.flags & F_INITSTRING) {
		debug("writing init string\n");
		full_write1_str(options.initstring);
	}

	/* Optionally detect the baud rate from the modem status message */
	debug("before autobaud\n");
	if (options.flags & F_PARSE)
		auto_baud(line_buf, sizeof(line_buf), &termios);

	/* Set the optional timer */
	alarm(options.timeout); /* if 0, alarm is not set */

	/* Optionally wait for CR or LF before writing /etc/issue */
	if (options.flags & F_WAITCRLF) {
		char ch;

		debug("waiting for cr-lf\n");
		while (safe_read(STDIN_FILENO, &ch, 1) == 1) {
			debug("read %x\n", (unsigned char)ch);
			ch &= 0x7f;                     /* strip "parity bit" */
			if (ch == '\n' || ch == '\r')
				break;
		}
	}

	logname = NULL;
	if (!(options.flags & F_NOPROMPT)) {
		/* NB:termios_init already set line speed
		 * to options.speeds[0] */
		int baud_index = 0;

		while (1) {
			/* Read the login name. */
			debug("reading login name\n");
			logname = get_logname(line_buf, sizeof(line_buf),
					&options, &chardata);
			if (logname)
				break;
			/* we are here only if options.numspeed > 1 */
			baud_index = (baud_index + 1) % options.numspeed;
			cfsetispeed(&termios, options.speeds[baud_index]);
			cfsetospeed(&termios, options.speeds[baud_index]);
			tcsetattr_stdin_TCSANOW(&termios);
		}
	}

	/* Disable timer. */
	alarm(0);

	/* Finalize the termios settings. */
	termios_final(&options, &termios, &chardata);

	/* Now the newline character should be properly written. */
	full_write(STDOUT_FILENO, "\n", 1);

	/* Let the login program take care of password validation. */
	/* We use PATH because we trust that root doesn't set "bad" PATH,
	 * and getty is not suid-root applet. */
	/* With -n, logname == NULL, and login will ask for username instead */
	BB_EXECLP(options.login, options.login, "--", logname, NULL);
	bb_error_msg_and_die("%s: can't exec %s", options.tty, options.login);
}
