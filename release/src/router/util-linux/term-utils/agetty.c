/*
 * Alternate Getty (agetty) 'agetty' is a versatile, portable, easy to use
 * replacement for getty on SunOS 4.1.x or the SAC ttymon/ttyadm/sacadm/pmadm
 * suite on Solaris and other SVR4 systems. 'agetty' was written by Wietse
 * Venema, enhanced by John DiMarco, and further enhanced by Dennis Cronin.
 *
 * Ported to Linux by Peter Orbaek <poe@daimi.aau.dk>
 * Adopt the mingetty features for a better support
 * of virtual consoles by Werner Fink <werner@suse.de>
 *
 * This program is freely distributable.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <ctype.h>
#include <utmp.h>
#include <getopt.h>
#include <time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netdb.h>
#include <langinfo.h>
#include <grp.h>

#include "strutils.h"
#include "writeall.h"
#include "nls.h"
#include "pathnames.h"
#include "c.h"
#include "widechar.h"

#ifdef __linux__
#  include <sys/kd.h>
#  include <sys/param.h>
#  define USE_SYSLOG
#  ifndef DEFAULT_VCTERM
#    define DEFAULT_VCTERM "linux"
#  endif
#  ifndef DEFAULT_STERM
#    define DEFAULT_STERM  "vt102"
#  endif
#elif defined(__GNU__)
#  define USE_SYSLOG
#  ifndef DEFAULT_VCTERM
#    define DEFAULT_VCTERM "hurd"
#  endif
#  ifndef DEFAULT_STERM
#    define DEFAULT_STERM  "vt102"
#  endif
#else
#  ifndef DEFAULT_VCTERM
#    define DEFAULT_VCTERM "vt100"
#  endif
#  ifndef DEFAULT_STERM
#    define DEFAULT_STERM  "vt100"
#  endif
#endif

/* If USE_SYSLOG is undefined all diagnostics go to /dev/console. */
#ifdef	USE_SYSLOG
#  include <syslog.h>
#endif

/*
 * Some heuristics to find out what environment we are in: if it is not
 * System V, assume it is SunOS 4. The LOGIN_PROCESS is defined in System V
 * utmp.h, which will select System V style getty.
 */
#ifdef LOGIN_PROCESS
#  define SYSV_STYLE
#endif

/*
 * Things you may want to modify.
 *
 * If ISSUE is not defined, agetty will never display the contents of the
 * /etc/issue file. You will not want to spit out large "issue" files at the
 * wrong baud rate. Relevant for System V only.
 *
 * You may disagree with the default line-editing etc. characters defined
 * below. Note, however, that DEL cannot be used for interrupt generation
 * and for line editing at the same time.
 */

/* Displayed before the login prompt. */
#ifdef	SYSV_STYLE
#  define ISSUE _PATH_ISSUE
#  include <sys/utsname.h>
#endif

/* Login prompt. */
#define LOGIN		"login: "
#define LOGIN_ARGV_MAX	16		/* Numbers of args for login */

/* Some shorthands for control characters. */
#define CTL(x)		(x ^ 0100)	/* Assumes ASCII dialect */
#define	CR		CTL('M')	/* carriage return */
#define	NL		CTL('J')	/* line feed */
#define	BS		CTL('H')	/* back space */
#define	DEL		CTL('?')	/* delete */

/* Defaults for line-editing etc. characters; you may want to change these. */
#define DEF_ERASE	DEL		/* default erase character */
#define DEF_INTR	CTL('C')	/* default interrupt character */
#define DEF_QUIT	CTL('\\')	/* default quit char */
#define DEF_KILL	CTL('U')	/* default kill char */
#define DEF_EOF		CTL('D')	/* default EOF char */
#define DEF_EOL		0
#define DEF_SWITCH	0		/* default switch char */

#ifndef MAXHOSTNAMELEN
#  ifdef HOST_NAME_MAX
#    define MAXHOSTNAMELEN HOST_NAME_MAX
#  else
#    define MAXHOSTNAMELEN 64
#  endif			/* HOST_NAME_MAX */
#endif				/* MAXHOSTNAMELEN */

/*
 * When multiple baud rates are specified on the command line, the first one
 * we will try is the first one specified.
 */
#define	FIRST_SPEED	0

/* Storage for command-line options. */
#define	MAX_SPEED	10	/* max. nr. of baud rates */

struct options {
	int flags;			/* toggle switches, see below */
	int timeout;			/* time-out period */
	char *autolog;			/* login the user automatically */
	char *chdir;			/* Chdir before the login */
	char *chroot;			/* Chroot before the login */
	char *login;			/* login program */
	char *logopt;			/* options for login program */
	char *tty;			/* name of tty */
	char *vcline;			/* line of virtual console */
	char *term;			/* terminal type */
	char *initstring;		/* modem init string */
	char *issue;			/* alternative issue file */
	int delay;			/* Sleep seconds before prompt */
	int nice;			/* Run login with this priority */
	int numspeed;			/* number of baud rates to try */
	speed_t speeds[MAX_SPEED];	/* baud rates to be tried */
};

#define	F_PARSE		(1<<0)	/* process modem status messages */
#define	F_ISSUE		(1<<1)	/* display /etc/issue */
#define	F_RTSCTS	(1<<2)	/* enable RTS/CTS flow control */
#define F_LOCAL		(1<<3)	/* force local */
#define F_INITSTRING    (1<<4)	/* initstring is set */
#define F_WAITCRLF	(1<<5)	/* wait for CR or LF */
#define F_CUSTISSUE	(1<<6)	/* give alternative issue file */
#define F_NOPROMPT	(1<<7)	/* do not ask for login name! */
#define F_LCUC		(1<<8)	/* support for *LCUC stty modes */
#define F_KEEPSPEED	(1<<9)	/* follow baud rate from kernel */
#define F_KEEPCFLAGS	(1<<10)	/* reuse c_cflags setup from kernel */
#define F_EIGHTBITS	(1<<11)	/* Assume 8bit-clean tty */
#define F_VCONSOLE	(1<<12)	/* This is a virtual console */
#define F_HANGUP	(1<<13)	/* Do call vhangup(2) */
#define F_UTF8		(1<<14)	/* We can do UTF8 */
#define F_LOGINPAUSE	(1<<15)	/* Wait for any key before dropping login prompt */
#define F_NOCLEAR	(1<<16) /* Do not clear the screen before prompting */
#define F_NONL		(1<<17) /* No newline before issue */
#define F_NOHOSTNAME	(1<<18) /* Do not show the hostname */
#define F_LONGHNAME	(1<<19) /* Show Full qualified hostname */
#define F_NOHINTS	(1<<20) /* Don't print hints */
#define F_REMOTE	(1<<21) /* Add '-h fakehost' to login(1) command line */

#define serial_tty_option(opt, flag)	\
	(((opt)->flags & (F_VCONSOLE|(flag))) == (flag))

/* Storage for things detected while the login name was read. */
struct chardata {
	int erase;		/* erase character */
	int kill;		/* kill character */
	int eol;		/* end-of-line character */
	int parity;		/* what parity did we see */
	int capslock;		/* upper case without lower case */
};

/* Initial values for the above. */
static const struct chardata init_chardata = {
	DEF_ERASE,		/* default erase character */
	DEF_KILL,		/* default kill character */
	13,			/* default eol char */
	0,			/* space parity */
	0,			/* no capslock */
};

struct Speedtab {
	long speed;
	speed_t code;
};

static const struct Speedtab speedtab[] = {
	{50, B50},
	{75, B75},
	{110, B110},
	{134, B134},
	{150, B150},
	{200, B200},
	{300, B300},
	{600, B600},
	{1200, B1200},
	{1800, B1800},
	{2400, B2400},
	{4800, B4800},
	{9600, B9600},
#ifdef	B19200
	{19200, B19200},
#endif
#ifdef	B38400
	{38400, B38400},
#endif
#ifdef	EXTA
	{19200, EXTA},
#endif
#ifdef	EXTB
	{38400, EXTB},
#endif
#ifdef B57600
	{57600, B57600},
#endif
#ifdef B115200
	{115200, B115200},
#endif
#ifdef B230400
	{230400, B230400},
#endif
	{0, 0},
};

static void init_special_char(char* arg, struct options *op);
static void parse_args(int argc, char **argv, struct options *op);
static void parse_speeds(struct options *op, char *arg);
static void update_utmp(struct options *op);
static void open_tty(char *tty, struct termios *tp, struct options *op);
static void termio_init(struct options *op, struct termios *tp);
static void reset_vc (const struct options *op, struct termios *tp);
static void auto_baud(struct termios *tp);
static void output_special_char (unsigned char c, struct options *op, struct termios *tp);
static void do_prompt(struct options *op, struct termios *tp);
static void next_speed(struct options *op, struct termios *tp);
static char *get_logname(struct options *op,
			 struct termios *tp, struct chardata *cp);
static void termio_final(struct options *op,
			 struct termios *tp, struct chardata *cp);
static int caps_lock(char *s);
static speed_t bcode(char *s);
static void usage(FILE * out) __attribute__((__noreturn__));
static void log_err(const char *, ...) __attribute__((__noreturn__))
			       __attribute__((__format__(printf, 1, 2)));
static void log_warn (const char *, ...)
				__attribute__((__format__(printf, 1, 2)));
static ssize_t append(char *dest, size_t len, const char  *sep, const char *src);
static void check_username (const char* nm);
static void login_options_to_argv(char *argv[], int *argc, char *str, char *username);

/* Fake hostname for ut_host specified on command line. */
static char *fakehost;

#ifdef DEBUGGING
#define debug(s) fprintf(dbf,s); fflush(dbf)
FILE *dbf;
#else
#define debug(s)
#endif				/* DEBUGGING */

int main(int argc, char **argv)
{
	char *username = NULL;			/* login name, given to /bin/login */
	struct chardata chardata;		/* will be set by get_logname() */
	struct termios termios;			/* terminal mode bits */
	struct options options = {
		.flags  =  F_ISSUE,		/* show /etc/issue (SYSV_STYLE) */
		.login  =  _PATH_LOGIN,		/* default login program */
		.tty    = "tty1",		/* default tty line */
		.term   =  DEFAULT_VCTERM,	/* terminal type */
		.issue  =  ISSUE		/* default issue file */
	};
	char *login_argv[LOGIN_ARGV_MAX + 1];
	int login_argc = 0;
	struct sigaction sa, sa_hup, sa_quit, sa_int;
	sigset_t set;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* In case vhangup(2) has to called */
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_RESTART;
	sigemptyset (&sa.sa_mask);
	sigaction(SIGHUP, &sa, &sa_hup);
	sigaction(SIGQUIT, &sa, &sa_quit);
	sigaction(SIGINT, &sa, &sa_int);

#ifdef DEBUGGING
	dbf = fopen("/dev/ttyp0", "w");
	for (int i = 1; i < argc; i++)
		debug(argv[i]);
#endif				/* DEBUGGING */

	/* Parse command-line arguments. */
	parse_args(argc, argv, &options);

	login_argv[login_argc++] = options.login;	/* set login program name */

	/* Update the utmp file. */
#ifdef	SYSV_STYLE
	update_utmp(&options);
#endif
	if (options.delay)
	    sleep(options.delay);

	debug("calling open_tty\n");

	/* Open the tty as standard { input, output, error }. */
	open_tty(options.tty, &termios, &options);

	/* Unmask SIGHUP if inherited */
	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	sigaction(SIGHUP, &sa_hup, NULL);

	tcsetpgrp(STDIN_FILENO, getpid());
	/* Initialize the termios settings (raw mode, eight-bit, blocking i/o). */
	debug("calling termio_init\n");
	termio_init(&options, &termios);

	/* Write the modem init string and DO NOT flush the buffers. */
	if (serial_tty_option(&options, F_INITSTRING) &&
	    options.initstring && *options.initstring != '\0') {
		debug("writing init string\n");
		write_all(STDOUT_FILENO, options.initstring,
			   strlen(options.initstring));
	}

	if (!serial_tty_option(&options, F_LOCAL))
		/* Go to blocking write mode unless -L is specified. */
		fcntl(STDOUT_FILENO, F_SETFL,
		      fcntl(STDOUT_FILENO, F_GETFL, 0) & ~O_NONBLOCK);

	/* Optionally detect the baud rate from the modem status message. */
	debug("before autobaud\n");
	if (serial_tty_option(&options, F_PARSE))
		auto_baud(&termios);

	/* Set the optional timer. */
	if (options.timeout)
		alarm((unsigned) options.timeout);

	/* Optionally wait for CR or LF before writing /etc/issue */
	if (serial_tty_option(&options, F_WAITCRLF)) {
		char ch;

		debug("waiting for cr-lf\n");
		while (read(STDIN_FILENO, &ch, 1) == 1) {
			/* Strip "parity bit". */
			ch &= 0x7f;
#ifdef DEBUGGING
			fprintf(dbf, "read %c\n", ch);
#endif
			if (ch == '\n' || ch == '\r')
				break;
		}
	}

	chardata = init_chardata;
	if ((options.flags & F_NOPROMPT) == 0) {
		if (options.autolog) {
			/* Do the auto login. */
			debug("doing auto login\n");
			do_prompt(&options, &termios);
			printf("%s%s (automatic login)\n", LOGIN, options.autolog);
			username = options.autolog;
		} else {
			/* Read the login name. */
			debug("reading login name\n");
			while ((username =
				get_logname(&options, &termios, &chardata)) == 0)
				if ((options.flags & F_VCONSOLE) == 0)
					next_speed(&options, &termios);
		}
	}

	/* Disable timer. */
	if (options.timeout)
		alarm(0);

	if ((options.flags & F_VCONSOLE) == 0) {
		/* Finalize the termios settings. */
		termio_final(&options, &termios, &chardata);

		/* Now the newline character should be properly written. */
		write_all(STDOUT_FILENO, "\r\n", 2);
	}

	sigaction(SIGQUIT, &sa_quit, NULL);
	sigaction(SIGINT, &sa_int, NULL);

	if (username)
		check_username(username);

	if (options.logopt) {
		/*
		 * The --login-options completely overwrites the default
		 * way how agetty composes login(1) command line.
		 */
		login_options_to_argv(login_argv, &login_argc,
				      options.logopt, username);
	} else {
		if (fakehost && (options.flags & F_REMOTE)) {
			login_argv[login_argc++] = "-h";
			login_argv[login_argc++] = fakehost;
		}
		if (username) {
			if (options.autolog)
				login_argv[login_argc++] = "-f";
			else
				login_argv[login_argc++] = "--";
			login_argv[login_argc++] = username;
		}
	}

	login_argv[login_argc] = NULL;	/* last login argv */

	if (options.chroot) {
		if (chroot(options.chroot) < 0)
			log_err(_("%s: can't change root directory %s: %m"),
				options.tty, options.chroot);
	}
	if (options.chdir) {
		if (chdir(options.chdir) < 0)
			log_err(_("%s: can't change working directory %s: %m"),
				options.tty, options.chdir);
	}
	if (options.nice) {
		if (nice(options.nice) < 0)
			log_warn(_("%s: can't change process priority: %m"),
				options.tty);
	}

	/* Let the login program take care of password validation. */
	execv(options.login, login_argv);
	log_err(_("%s: can't exec %s: %m"), options.tty, login_argv[0]);
}

/*
 * Returns : @str if \u not found
 *         : @username if @str equal to "\u"
 *         : newly allocated string if \u mixed with something other
 */
static char *replace_u(char *str, char *username)
{
	char *entry = NULL, *p = str;
	size_t usz = username ? strlen(username) : 0;

	while (*p) {
		size_t sz;
		char *tp, *old = entry;

		if (memcmp(p, "\\u", 2)) {
			p++;
			continue;	/* no \u */
		}
		sz = strlen(str);

		if (p == str && sz == 2)
			/* 'str' contains only '\u' */
			return username;

		tp = entry = malloc(sz + usz);
		if (!tp)
			log_err(_("failed to allocate memory: %m"));

		if (p != str) {
			/* copy chars befor \u */
			memcpy(tp, str, p - str);
			tp += p - str;
		}
		if (usz) {
			/* copy username */
			memcpy(tp, username, usz);
			tp += usz;
		}
		if (*(p + 2))
			/* copy chars after \u + \0 */
			memcpy(tp, p + 2, sz - (p - str) - 1);
		else
			*tp = '\0';

		p = tp;
		str = entry;
		free(old);
	}

	return entry ? entry : str;
}

static void login_options_to_argv(char *argv[], int *argc,
				  char *str, char *username)
{
	char *p;
	int i = *argc;

	while (str && isspace(*str))
		str++;
	p = str;

	while (p && *p && i < LOGIN_ARGV_MAX) {
		if (isspace(*p)) {
			*p = '\0';
			while (isspace(*++p))
				;
			if (*p) {
				argv[i++] = replace_u(str, username);
				str = p;
			}
		} else
			p++;
	}
	if (str && *str && i < LOGIN_ARGV_MAX)
		argv[i++] = replace_u(str, username);
	*argc = i;
}

/* Parse command-line arguments. */
static void parse_args(int argc, char **argv, struct options *op)
{
	extern char *optarg;
	extern int optind;
	int c;

	enum {
		VERSION_OPTION = CHAR_MAX + 1,
		NOHINTS_OPTION,
		NOHOSTNAME_OPTION,
		LONGHOSTNAME_OPTION,
		HELP_OPTION
	};
	const struct option longopts[] = {
		{  "8bits",	     no_argument,	 0,  '8'  },
		{  "autologin",	     required_argument,	 0,  'a'  },
		{  "noreset",	     no_argument,	 0,  'c'  },
		{  "chdir",	     required_argument,	 0,  'C'  },
		{  "delay",	     required_argument,	 0,  'd'  },
		{  "remote",         no_argument,        0,  'E'  },
		{  "issue-file",     required_argument,  0,  'f'  },
		{  "flow-control",   no_argument,	 0,  'h'  },
		{  "host",	     required_argument,  0,  'H'  },
		{  "noissue",	     no_argument,	 0,  'i'  },
		{  "init-string",    required_argument,  0,  'I'  },
		{  "noclear",	     no_argument,	 0,  'J'  },
		{  "login-program",  required_argument,  0,  'l'  },
		{  "local-line",     no_argument,	 0,  'L'  },
		{  "extract-baud",   no_argument,	 0,  'm'  },
		{  "skip-login",     no_argument,	 0,  'n'  },
		{  "nonewline",	     no_argument,	 0,  'N'  },
		{  "login-options",  required_argument,  0,  'o'  },
		{  "login-pause",    no_argument,        0,  'p'  },
		{  "nice",	     required_argument,  0,  'P'  },
		{  "chroot",	     required_argument,	 0,  'r'  },
		{  "hangup",	     no_argument,	 0,  'R'  },
		{  "keep-baud",      no_argument,	 0,  's'  },
		{  "timeout",	     required_argument,  0,  't'  },
		{  "detect-case",    no_argument,	 0,  'U'  },
		{  "wait-cr",	     no_argument,	 0,  'w'  },
		{  "nohints",        no_argument,        0,  NOHINTS_OPTION },
		{  "nohostname",     no_argument,	 0,  NOHOSTNAME_OPTION },
		{  "long-hostname",  no_argument,	 0,  LONGHOSTNAME_OPTION },
		{  "version",	     no_argument,	 0,  VERSION_OPTION  },
		{  "help",	     no_argument,	 0,  HELP_OPTION     },
		{ NULL, 0, 0, 0 }
	};

	while ((c = getopt_long(argc, argv,
			   "8a:cC:d:Ef:hH:iI:Jl:LmnNo:pP:r:Rst:Uw", longopts,
			    NULL)) != -1) {
		switch (c) {
		case '8':
			op->flags |= F_EIGHTBITS;
			break;
		case 'a':
			op->autolog = optarg;
			break;
		case 'c':
			op->flags |= F_KEEPCFLAGS;
			break;
		case 'C':
			op->chdir = optarg;
			break;
		case 'd':
			op->delay = atoi(optarg);
			break;
		case 'E':
			op->flags |= F_REMOTE;
			break;
		case 'f':
			op->flags |= F_CUSTISSUE;
			op->issue = optarg;
			break;
		case 'h':
			op->flags |= F_RTSCTS;
			break;
		case 'H':
			fakehost = optarg;
			break;
		case 'i':
			op->flags &= ~F_ISSUE;
			break;
		case 'I':
			init_special_char(optarg, op);
			op->flags |= F_INITSTRING;
			break;
		case 'J':
			op->flags |= F_NOCLEAR;
			break;
		case 'l':
			op->login = optarg;
			break;
		case 'L':
			op->flags |= F_LOCAL;
			break;
		case 'm':
			op->flags |= F_PARSE;
			break;
		case 'n':
			op->flags |= F_NOPROMPT;
			break;
		case 'o':
			op->logopt = optarg;
			break;
		case 'p':
			op->flags |= F_LOGINPAUSE;
			break;
		case 'P':
			op->nice = atoi(optarg);
			break;
		case 'r':
			op->chroot = optarg;
			break;
		case 'R':
			op->flags |= F_HANGUP;
			break;
		case 's':
			op->flags |= F_KEEPSPEED;
			break;
		case 't':
			if ((op->timeout = atoi(optarg)) <= 0)
				log_err(_("bad timeout value: %s"), optarg);
			break;
		case 'U':
			op->flags |= F_LCUC;
			break;
		case 'w':
			op->flags |= F_WAITCRLF;
			break;
		case NOHINTS_OPTION:
			op->flags |= F_NOHINTS;
			break;
		case NOHOSTNAME_OPTION:
			op->flags |= F_NOHOSTNAME;
			break;
		case LONGHOSTNAME_OPTION:
			op->flags |= F_LONGHNAME;
			break;
		case VERSION_OPTION:
			printf(_("%s from %s\n"), program_invocation_short_name,
			       PACKAGE_STRING);
			exit(EXIT_SUCCESS);
		case HELP_OPTION:
			usage(stdout);
		default:
			usage(stderr);
		}
	}

	debug("after getopt loop\n");

	if (argc < optind + 1) {
		log_warn(_("not enough arguments"));
		usage(stderr);
	}

	/* Accept "tty", "baudrate tty", and "tty baudrate". */
	if ('0' <= argv[optind][0] && argv[optind][0] <= '9') {
		/* Assume BSD style speed. */
		parse_speeds(op, argv[optind++]);
		if (argc < optind + 1) {
			warn(_("not enough arguments"));
			usage(stderr);
		}
		op->tty = argv[optind++];
	} else {
		op->tty = argv[optind++];
		if (argc > optind) {
			char *v = argv[optind++];
			if ('0' <= *v && *v <= '9')
				parse_speeds(op, v);
			else
				op->speeds[op->numspeed++] = bcode("9600");
		}
	}

	/* On virtual console remember the line which is used for */
	if (strncmp(op->tty, "tty", 3) == 0 &&
	    strspn(op->tty + 3, "0123456789") == strlen(op->tty+3))
		op->vcline = op->tty+3;

	if (argc > optind && argv[optind])
		op->term = argv[optind];

#ifdef DO_DEVFS_FIDDLING
	/*
	 * Some devfs junk, following Goswin Brederlow:
	 *   turn ttyS<n> into tts/<n>
	 *   turn tty<n> into vc/<n>
	 * http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=72241
	 */
	if (op->tty && strlen(op->tty) < 90) {
		char dev_name[100];
		struct stat st;

		if (strncmp(op->tty, "ttyS", 4) == 0) {
			strcpy(dev_name, "/dev/");
			strcat(dev_name, op->tty);
			if (stat(dev_name, &st) < 0) {
				strcpy(dev_name, "/dev/tts/");
				strcat(dev_name, op->tty + 4);
				if (stat(dev_name, &st) == 0) {
					op->tty = strdup(dev_name + 5);
					if (!op->tty)
						log_err(_("failed to allocate memory: %m"));
				}
			}
		} else if (strncmp(op->tty, "tty", 3) == 0) {
			strcpy(dev_name, "/dev/");
			strncat(dev_name, op->tty, 90);
			if (stat(dev_name, &st) < 0) {
				strcpy(dev_name, "/dev/vc/");
				strcat(dev_name, op->tty + 3);
				if (stat(dev_name, &st) == 0) {
					op->tty = strdup(dev_name + 5);
					if (!op->tty)
						log_err(_("failed to allocate memory: %m"));
				}
			}
		}
	}
#endif				/* DO_DEVFS_FIDDLING */

	debug("exiting parseargs\n");
}

/* Parse alternate baud rates. */
static void parse_speeds(struct options *op, char *arg)
{
	char *cp;

	debug("entered parse_speeds\n");
	for (cp = strtok(arg, ","); cp != 0; cp = strtok((char *)0, ",")) {
		if ((op->speeds[op->numspeed++] = bcode(cp)) <= 0)
			log_err(_("bad speed: %s"), cp);
		if (op->numspeed >= MAX_SPEED)
			log_err(_("too many alternate speeds"));
	}
	debug("exiting parsespeeds\n");
}

#ifdef	SYSV_STYLE

/* Update our utmp entry. */
static void update_utmp(struct options *op)
{
	struct utmp ut;
	time_t t;
	pid_t pid = getpid();
	pid_t sid = getsid(0);
	char *vcline = op->vcline;
	char *line   = op->tty;
	struct utmp *utp;

	/*
	 * The utmp file holds miscellaneous information about things started by
	 * /sbin/init and other system-related events. Our purpose is to update
	 * the utmp entry for the current process, in particular the process type
	 * and the tty line we are listening to. Return successfully only if the
	 * utmp file can be opened for update, and if we are able to find our
	 * entry in the utmp file.
	 */
	utmpname(_PATH_UTMP);
	setutent();

	/*
	 * Find my pid in utmp.
	 *
	 * FIXME: Earlier (when was that?) code here tested only utp->ut_type !=
	 * INIT_PROCESS, so maybe the >= here should be >.
	 *
	 * FIXME: The present code is taken from login.c, so if this is changed,
	 * maybe login has to be changed as well (is this true?).
	 */
	while ((utp = getutent()))
		if (utp->ut_pid == pid
				&& utp->ut_type >= INIT_PROCESS
				&& utp->ut_type <= DEAD_PROCESS)
			break;

	if (utp) {
		memcpy(&ut, utp, sizeof(ut));
	} else {
		/* Some inits do not initialize utmp. */
		memset(&ut, 0, sizeof(ut));
		if (vcline && *vcline)
			/* Standard virtual console devices */
			strncpy(ut.ut_id, vcline, sizeof(ut.ut_id));
		else {
			size_t len = strlen(line);
			char * ptr;
			if (len >= sizeof(ut.ut_id))
				ptr = line + len - sizeof(ut.ut_id);
			else
				ptr = line;
			strncpy(ut.ut_id, ptr, sizeof(ut.ut_id));
		}
	}

	strncpy(ut.ut_user, "LOGIN", sizeof(ut.ut_user));
	strncpy(ut.ut_line, line, sizeof(ut.ut_line));
	if (fakehost)
		strncpy(ut.ut_host, fakehost, sizeof(ut.ut_host));
	time(&t);
	ut.ut_time = t;
	ut.ut_type = LOGIN_PROCESS;
	ut.ut_pid = pid;
	ut.ut_session = sid;

	pututline(&ut);
	endutent();

	{
#ifdef HAVE_UPDWTMP
		updwtmp(_PATH_WTMP, &ut);
#else
		int ut_fd;
		int lf;

		if ((lf = open(_PATH_WTMPLOCK, O_CREAT | O_WRONLY, 0660)) >= 0) {
			flock(lf, LOCK_EX);
			if ((ut_fd =
			     open(_PATH_WTMP, O_APPEND | O_WRONLY)) >= 0) {
				write_all(ut_fd, &ut, sizeof(ut));
				close(ut_fd);
			}
			flock(lf, LOCK_UN);
			close(lf);
		}
#endif				/* HAVE_UPDWTMP */
	}
}

#endif				/* SYSV_STYLE */

/* Set up tty as stdin, stdout & stderr. */
static void open_tty(char *tty, struct termios *tp, struct options *op)
{
	const pid_t pid = getpid();
	int serial;

	/* Set up new standard input, unless we are given an already opened port. */

	if (strcmp(tty, "-") != 0) {
		char buf[PATH_MAX+1];
		struct group *gr = NULL;
		struct stat st;
		int fd, len;
		pid_t tid;
		gid_t gid = 0;

		/* Use tty group if available */
		if ((gr = getgrnam("tty")))
			gid = gr->gr_gid;

		if (((len = snprintf(buf, sizeof(buf), "/dev/%s", tty)) >=
		     (int)sizeof(buf)) || (len < 0))
			log_err(_("/dev/%s: cannot open as standard input: %m"), tty);

		/*
		 * There is always a race between this reset and the call to
		 * vhangup() that s.o. can use to get access to your tty.
		 * Linux login(1) will change tty permissions. Use root owner and group
		 * with permission -rw------- for the period between getty and login.
		 */
		if (chown(buf, 0, gid) || chmod(buf, (gid ? 0660 : 0600))) {
			if (errno == EROFS)
				log_warn("%s: %m", buf);
			else
				log_err("%s: %m", buf);
		}

		/* Open the tty as standard input. */
		if ((fd = open(buf, O_RDWR|O_NOCTTY|O_NONBLOCK, 0)) < 0)
			log_err(_("/dev/%s: cannot open as standard input: %m"), tty);

		/* Sanity checks... */
		if (!isatty(fd))
			log_err(_("/dev/%s: not a character device"), tty);
		if (fstat(fd, &st) < 0)
			log_err("%s: %m", buf);
		if ((st.st_mode & S_IFMT) != S_IFCHR)
			log_err(_("/dev/%s: not a character device"), tty);

		if (((tid = tcgetsid(fd)) < 0) || (pid != tid)) {
			if (ioctl(fd, TIOCSCTTY, 1) == -1)
				log_warn("/dev/%s: cannot get controlling tty: %m", tty);
		}

		if (op->flags & F_HANGUP) {
			/*
			 * vhangup() will replace all open file descriptors in the kernel
			 * that point to our controlling tty by a dummy that will deny
			 * further reading/writing to our device. It will also reset the
			 * tty to sane defaults, so we don't have to modify the tty device
			 * for sane settings. We also get a SIGHUP/SIGCONT.
			 */
			if (vhangup())
				log_err("/dev/%s: vhangup() failed: %m", tty);
			ioctl(fd, TIOCNOTTY);
		}

		close(fd);
		close(STDIN_FILENO);
		errno = 0;

		debug("open(2)\n");
		if (open(buf, O_RDWR|O_NOCTTY|O_NONBLOCK, 0) != 0)
			log_err(_("/dev/%s: cannot open as standard input: %m"), tty);
		if (((tid = tcgetsid(STDIN_FILENO)) < 0) || (pid != tid)) {
			if (ioctl(STDIN_FILENO, TIOCSCTTY, 1) == -1)
				log_warn("/dev/%s: cannot get controlling tty: %m", tty);
		}

	} else {

		/*
		 * Standard input should already be connected to an open port. Make
		 * sure it is open for read/write.
		 */

		if ((fcntl(STDIN_FILENO, F_GETFL, 0) & O_RDWR) != O_RDWR)
			log_err(_("%s: not open for read/write"), tty);

	}

	if (tcsetpgrp(STDIN_FILENO, pid))
		log_err("/dev/%s: cannot set process group: %m", tty);

	/* Get rid of the present outputs. */
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	errno = 0;

	/* Set up standard output and standard error file descriptors. */
	debug("duping\n");

	/* set up stdout and stderr */
	if (dup(STDIN_FILENO) != 1 || dup(STDIN_FILENO) != 2)
		log_err(_("%s: dup problem: %m"), tty);

	/* make stdio unbuffered for slow modem lines */
	setvbuf(stdout, NULL, _IONBF, 0);

	/*
	 * The following ioctl will fail if stdin is not a tty, but also when
	 * there is noise on the modem control lines. In the latter case, the
	 * common course of action is (1) fix your cables (2) give the modem
	 * more time to properly reset after hanging up.
	 *
	 * SunOS users can achieve (2) by patching the SunOS kernel variable
	 * "zsadtrlow" to a larger value; 5 seconds seems to be a good value.
	 * http://www.sunmanagers.org/archives/1993/0574.html
	 */
	memset(tp, 0, sizeof(struct termios));
	if (tcgetattr(STDIN_FILENO, tp) < 0)
		log_err("%s: tcgetattr: %m", tty);

	/*
	 * Detect if this is a virtual console or serial/modem line.
	 * In case of a virtual console the ioctl TIOCMGET fails and
	 * the error number will be set to EINVAL.
	 */
	if (ioctl(STDIN_FILENO, TIOCMGET, &serial) < 0 && (errno == EINVAL)) {
		op->flags |= F_VCONSOLE;
		if (!op->term)
			op->term = DEFAULT_VCTERM;
	} else if (!op->term)
		op->term = DEFAULT_STERM;

	setenv("TERM", op->term, 1);
}

/* Initialize termios settings. */
static void termio_init(struct options *op, struct termios *tp)
{
	speed_t ispeed, ospeed;
	struct winsize ws;

	if (op->flags & F_VCONSOLE) {
#if defined(IUTF8) && defined(KDGKBMODE)
		int mode;

		/* Detect mode of current keyboard setup, e.g. for UTF-8 */
		if (ioctl(STDIN_FILENO, KDGKBMODE, &mode) < 0)
			mode = K_RAW;
		switch(mode) {
		case K_UNICODE:
			setlocale(LC_CTYPE, "C.UTF-8");
			op->flags |= F_UTF8;
			break;
		case K_RAW:
		case K_MEDIUMRAW:
		case K_XLATE:
		default:
			setlocale(LC_CTYPE, "POSIX");
			op->flags &= ~F_UTF8;
			break;
		}
#else
		setlocale(LC_CTYPE, "POSIX");
		op->flags &= ~F_UTF8;
#endif
		reset_vc(op, tp);

		if ((tp->c_cflag & (CS8|PARODD|PARENB)) == CS8)
			op->flags |= F_EIGHTBITS;

		if ((op->flags & F_NOCLEAR) == 0) {
			/*
			 * Do not write a full reset (ESC c) because this destroys
			 * the unicode mode again if the terminal was in unicode
			 * mode.  Also it clears the CONSOLE_MAGIC features which
			 * are required for some languages/console-fonts.
			 * Just put the cursor to the home position (ESC [ H),
			 * erase everything below the cursor (ESC [ J), and set the
			 * scrolling region to the full window (ESC [ r)
			 */
			write_all(STDOUT_FILENO, "\033[r\033[H\033[J", 9);
		}
		return;
	}

	if (op->flags & F_KEEPSPEED) {
		/* Save the original setting. */
		ispeed = cfgetispeed(tp);
		ospeed = cfgetospeed(tp);

		if (!ispeed) ispeed = TTYDEF_SPEED;
		if (!ospeed) ospeed = TTYDEF_SPEED;

	} else {
		ospeed = ispeed = op->speeds[FIRST_SPEED];
	}

	/*
	 * Initial termios settings: 8-bit characters, raw-mode, blocking i/o.
	 * Special characters are set after we have read the login name; all
	 * reads will be done in raw mode anyway. Errors will be dealt with
	 * later on.
	 */

	 /* Flush input and output queues, important for modems! */
	tcflush(STDIN_FILENO, TCIOFLUSH);

#ifdef IUTF8
	tp->c_iflag = tp->c_iflag & IUTF8;
	if (tp->c_iflag & IUTF8)
		op->flags |= F_UTF8;
#else
	tp->c_iflag = 0;
#endif
	tp->c_lflag = 0;
	tp->c_oflag &= OPOST | ONLCR;

	if ((op->flags & F_KEEPCFLAGS) == 0)
		tp->c_cflag = CS8 | HUPCL | CREAD | (tp->c_cflag & CLOCAL);

	/*
	 * Note that the speed is stored in the c_cflag termios field, so we have
	 * set the speed always when the cflag se reseted.
	 */
	cfsetispeed(tp, ispeed);
	cfsetospeed(tp, ospeed);

	if (op->flags & F_LOCAL)
		tp->c_cflag |= CLOCAL;
#ifdef HAVE_STRUCT_TERMIOS_C_LINE
	tp->c_line = 0;
#endif
	tp->c_cc[VMIN] = 1;
	tp->c_cc[VTIME] = 0;

	/* Check for terminal size and if not found set default */
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
		int set = 0;
		if (ws.ws_row == 0) {
			ws.ws_row = 24;
			set++;
		}
		if (ws.ws_col == 0) {
			ws.ws_col = 80;
			set++;
		}
		ioctl(STDIN_FILENO, TIOCSWINSZ, &ws);
	}

	/* Optionally enable hardware flow control. */
#ifdef	CRTSCTS
	if (op->flags & F_RTSCTS)
		tp->c_cflag |= CRTSCTS;
#endif

	tcsetattr(STDIN_FILENO, TCSANOW, tp);

	/* Go to blocking input even in local mode. */
	fcntl(STDIN_FILENO, F_SETFL,
	      fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);

	debug("term_io 2\n");
}

/* Reset virtual console on stdin to its defaults */
static void reset_vc(const struct options *op, struct termios *tp)
{
	/* Use defaults of <sys/ttydefaults.h> for base settings */
	tp->c_iflag |= TTYDEF_IFLAG;
	tp->c_oflag |= TTYDEF_OFLAG;
	tp->c_lflag |= TTYDEF_LFLAG;

	if ((op->flags & F_KEEPCFLAGS) == 0) {
#ifdef CBAUD
		tp->c_lflag &= ~CBAUD;
#endif
		tp->c_cflag |= (B38400 | TTYDEF_CFLAG);
	}

	/* Sane setting, allow eight bit characters, no carriage return delay
	 * the same result as `stty sane cr0 pass8'
	 */
	tp->c_iflag |=  (BRKINT | ICRNL | IMAXBEL);
	tp->c_iflag &= ~(IGNBRK | INLCR | IGNCR | IXOFF | IUCLC | IXANY | ISTRIP);
	tp->c_oflag |=  (OPOST | ONLCR | NL0 | CR0 | TAB0 | BS0 | VT0 | FF0);
	tp->c_oflag &= ~(OLCUC | OCRNL | ONOCR | ONLRET | OFILL | \
			    NLDLY|CRDLY|TABDLY|BSDLY|VTDLY|FFDLY);
	tp->c_lflag |=  (ISIG | ICANON | IEXTEN | ECHO|ECHOE|ECHOK|ECHOKE);
	tp->c_lflag &= ~(ECHONL|ECHOCTL|ECHOPRT | NOFLSH | TOSTOP);

	if ((op->flags & F_KEEPCFLAGS) == 0) {
		tp->c_cflag |=  (CREAD | CS8 | HUPCL);
		tp->c_cflag &= ~(PARODD | PARENB);
	}
#ifdef OFDEL
	tp->c_oflag &= ~OFDEL;
#endif
#ifdef XCASE
	tp->c_lflag &= ~XCASE;
#endif
#ifdef IUTF8
	if (op->flags & F_UTF8)
		tp->c_iflag |= IUTF8;	    /* Set UTF-8 input flag */
	else
		tp->c_iflag &= ~IUTF8;
#endif
	/* VTIME and VMIN can overlap with VEOF and VEOL since they are
	 * only used for non-canonical mode. We just set the at the
	 * beginning, so nothing bad should happen.
	 */
	tp->c_cc[VTIME]    = 0;
	tp->c_cc[VMIN]     = 1;
	tp->c_cc[VINTR]    = CINTR;
	tp->c_cc[VQUIT]    = CQUIT;
	tp->c_cc[VERASE]   = CERASE; /* ASCII DEL (0177) */
	tp->c_cc[VKILL]    = CKILL;
	tp->c_cc[VEOF]     = CEOF;
#ifdef VSWTC
	tp->c_cc[VSWTC]    = _POSIX_VDISABLE;
#elif defined(VSWTCH)
	tp->c_cc[VSWTCH]   = _POSIX_VDISABLE;
#endif
	tp->c_cc[VSTART]   = CSTART;
	tp->c_cc[VSTOP]    = CSTOP;
	tp->c_cc[VSUSP]    = CSUSP;
	tp->c_cc[VEOL]     = _POSIX_VDISABLE;
	tp->c_cc[VREPRINT] = CREPRINT;
	tp->c_cc[VDISCARD] = CDISCARD;
	tp->c_cc[VWERASE]  = CWERASE;
	tp->c_cc[VLNEXT]   = CLNEXT;
	tp->c_cc[VEOL2]    = _POSIX_VDISABLE;
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, tp))
		log_warn("tcsetattr problem: %m");
}

/* Extract baud rate from modem status message. */
static void auto_baud(struct termios *tp)
{
	speed_t speed;
	int vmin;
	unsigned iflag;
	char buf[BUFSIZ];
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
	/* Enable 8th-bit stripping. */
	tp->c_iflag |= ISTRIP;
	vmin = tp->c_cc[VMIN];
	/* Do not block when queue is empty. */
	tp->c_cc[VMIN] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, tp);

	/*
	 * Wait for a while, then read everything the modem has said so far and
	 * try to extract the speed of the dial-in call.
	 */
	sleep(1);
	if ((nread = read(STDIN_FILENO, buf, sizeof(buf) - 1)) > 0) {
		buf[nread] = '\0';
		for (bp = buf; bp < buf + nread; bp++)
			if (isascii(*bp) && isdigit(*bp)) {
				if ((speed = bcode(bp))) {
					cfsetispeed(tp, speed);
					cfsetospeed(tp, speed);
				}
				break;
			}
	}

	/* Restore terminal settings. Errors will be dealt with later on. */
	tp->c_iflag = iflag;
	tp->c_cc[VMIN] = vmin;
	tcsetattr(STDIN_FILENO, TCSANOW, tp);
}

/* Show login prompt, optionally preceded by /etc/issue contents. */
static void do_prompt(struct options *op, struct termios *tp)
{
#ifdef	ISSUE
	FILE *fd;
#endif				/* ISSUE */

	if ((op->flags & F_NONL) == 0) {
		/* Issue not in use, start with a new line. */
		write_all(STDOUT_FILENO, "\r\n", 2);
	}

#ifdef	ISSUE
	if ((op->flags & F_ISSUE) && (fd = fopen(op->issue, "r"))) {
		int c, oflag = tp->c_oflag;	    /* Save current setting. */

		if ((op->flags & F_VCONSOLE) == 0) {
			/* Map new line in output to carriage return & new line. */
			tp->c_oflag |= (ONLCR | OPOST);
			tcsetattr(STDIN_FILENO, TCSADRAIN, tp);
		}

		while ((c = getc(fd)) != EOF) {
			if (c == '\\')
				output_special_char(getc(fd), op, tp);
			else
				putchar(c);
		}
		fflush(stdout);

		if ((op->flags & F_VCONSOLE) == 0) {
			/* Restore settings. */
			tp->c_oflag = oflag;
			/* Wait till output is gone. */
			tcsetattr(STDIN_FILENO, TCSADRAIN, tp);
		}
		fclose(fd);
	}
#endif	/* ISSUE */
	if (op->flags & F_LOGINPAUSE) {
		puts("[press ENTER to login]");
		getc(stdin);
	}
#ifdef KDGKBLED
	if (!(op->flags & F_NOHINTS) && !op->autolog &&
	    (op->flags & F_VCONSOLE)) {
		int kb = 0;

		if (ioctl(STDIN_FILENO, KDGKBLED, &kb) == 0) {
			char hint[256] = { '\0' };
			int nl = 0;
			struct stat st;

			if (stat("/var/run/numlock-on", &st) == 0)
				nl = 1;

			if (nl && (kb & 0x02) == 0)
				append(hint, sizeof(hint), NULL, _("Num Lock off"));

			else if (nl == 0 && (kb & 2) && (kb & 0x20) == 0)
				append(hint, sizeof(hint), NULL, _("Num Lock on"));

			if ((kb & 0x04) && (kb & 0x40) == 0)
				append(hint, sizeof(hint), ", ", _("Caps Lock on"));

			if ((kb & 0x01) && (kb & 0x10) == 0)
				append(hint, sizeof(hint), ", ",  _("Scroll Lock on"));

			if (*hint)
				printf(_("Hint: %s\n\n"), hint);
		}
	}
#endif /* KDGKBLED */
	if ((op->flags & F_NOHOSTNAME) == 0) {
		char hn[MAXHOSTNAMELEN + 1];
		if (gethostname(hn, sizeof(hn)) == 0) {
			struct hostent *ht;
			char *dot = strchr(hn, '.');

			hn[MAXHOSTNAMELEN] = '\0';
			if ((op->flags & F_LONGHNAME) == 0) {
				if (dot)
					*dot = '\0';
				write_all(STDOUT_FILENO, hn, strlen(hn));
			} else if (dot == NULL && (ht = gethostbyname(hn)))
				write_all(STDOUT_FILENO, ht->h_name, strlen(ht->h_name));
			else
				write_all(STDOUT_FILENO, hn, strlen(hn));
			write_all(STDOUT_FILENO, " ", 1);
		}
	}
	if (op->autolog == (char*)0) {
		/* Always show login prompt. */
		write_all(STDOUT_FILENO, LOGIN, sizeof(LOGIN) - 1);
	}
}

/* Select next baud rate. */
static void next_speed(struct options *op, struct termios *tp)
{
	static int baud_index = -1;

	if (baud_index == -1)
		/*
		 * If the F_KEEPSPEED flags is set then the FIRST_SPEED is not
		 * tested yet (see termio_init()).
		 */
		baud_index =
		    (op->flags & F_KEEPSPEED) ? FIRST_SPEED : 1 % op->numspeed;
	else
		baud_index = (baud_index + 1) % op->numspeed;

	cfsetispeed(tp, op->speeds[baud_index]);
	cfsetospeed(tp, op->speeds[baud_index]);
	tcsetattr(STDIN_FILENO, TCSANOW, tp);
}

/* Get user name, establish parity, speed, erase, kill & eol. */
static char *get_logname(struct options *op, struct termios *tp, struct chardata *cp)
{
	static char logname[BUFSIZ];
	char *bp;
	char c;			/* input character, full eight bits */
	char ascval;		/* low 7 bits of input character */
	int eightbit;
	static char *erase[] = {	/* backspace-space-backspace */
		"\010\040\010",		/* space parity */
		"\010\040\010",		/* odd parity */
		"\210\240\210",		/* even parity */
		"\210\240\210",		/* no parity */
	};

	/* Initialize kill, erase, parity etc. (also after switching speeds). */
	*cp = init_chardata;

	/*
	 * Flush pending input (especially important after parsing or switching
	 * the baud rate).
	 */
	if ((op->flags & F_VCONSOLE) == 0)
		sleep(1);
	tcflush(STDIN_FILENO, TCIFLUSH);

	eightbit = (op->flags & F_EIGHTBITS);
	bp = logname;
	*bp = '\0';

	while (*logname == '\0') {

		/* Write issue file and prompt */
		do_prompt(op, tp);

		cp->eol = '\0';

		/* Read name, watch for break and end-of-line. */
		while (cp->eol == '\0') {

			if (read(STDIN_FILENO, &c, 1) < 1) {

				/* Do not report trivial like EINTR/EIO errors. */
				if (errno == EINTR || errno == EAGAIN) {
					usleep(1000);
					continue;
				}
				switch (errno) {
				case 0:
				case EIO:
				case ESRCH:
				case EINVAL:
				case ENOENT:
					break;
				default:
					log_err(_("%s: read: %m"), op->tty);
				}
			}

			/* Do parity bit handling. */
			if (eightbit)
				ascval = c;
			else if (c != (ascval = (c & 0177))) {
				uint32_t bits;			/* # of "1" bits per character */
				uint32_t mask;			/* mask with 1 bit up */
				for (bits = 1, mask = 1; mask & 0177; mask <<= 1) {
					if (mask & ascval)
						bits++;
				}
				cp->parity |= ((bits & 1) ? 1 : 2);
			}

			/* Do erase, kill and end-of-line processing. */
			switch (ascval) {
			case 0:
				*bp = 0;
				if (op->numspeed > 1)
					return NULL;
				break;
			case CR:
			case NL:
				*bp = 0;			/* terminate logname */
				cp->eol = ascval;		/* set end-of-line char */
				break;
			case BS:
			case DEL:
			case '#':
				cp->erase = ascval; /* set erase character */
				if (bp > logname) {
					if ((tp->c_lflag & ECHO) == 0)
						write_all(1, erase[cp->parity], 3);
					bp--;
				}
				break;
			case CTL('U'):
			case '@':
				cp->kill = ascval;		/* set kill character */
				while (bp > logname) {
					if ((tp->c_lflag & ECHO) == 0)
						write_all(1, erase[cp->parity], 3);
					bp--;
				}
				break;
			case CTL('D'):
				exit(EXIT_SUCCESS);
			default:
				if (!isascii(ascval) || !isprint(ascval))
					break;
				if ((size_t)(bp - logname) >= sizeof(logname) - 1)
					log_err(_("%s: input overrun"), op->tty);
				if ((tp->c_lflag & ECHO) == 0)
					write_all(1, &c, 1);	/* echo the character */
				*bp++ = ascval;			/* and store it */
				break;
			}
		}
	}
#ifdef HAVE_WIDECHAR
	if ((op->flags & (F_EIGHTBITS|F_UTF8)) == (F_EIGHTBITS|F_UTF8)) {
		/* Check out UTF-8 multibyte characters */
		ssize_t len;
		wchar_t *wcs, *wcp;

		len = mbstowcs((wchar_t *)0, logname, 0);
		if (len < 0)
			log_err("%s: invalid character conversion for login name", op->tty);

		wcs = (wchar_t *) malloc((len + 1) * sizeof(wchar_t));
		if (!wcs)
			log_err(_("failed to allocate memory: %m"));

		len = mbstowcs(wcs, logname, len + 1);
		if (len < 0)
			log_err("%s: invalid character conversion for login name", op->tty);

		wcp = wcs;
		while (*wcp) {
			const wint_t wc = *wcp++;
			if (!iswprint(wc))
				log_err("%s: invalid character 0x%x in login name", op->tty, wc);
		}
		free(wcs);
	} else
#endif
	if ((op->flags & F_LCUC) && (cp->capslock = caps_lock(logname))) {

		/* Handle names with upper case and no lower case. */
		for (bp = logname; *bp; bp++)
			if (isupper(*bp))
				*bp = tolower(*bp);		/* map name to lower case */
	}

	return logname;
}

/* Set the final tty mode bits. */
static void termio_final(struct options *op, struct termios *tp, struct chardata *cp)
{
	/* General terminal-independent stuff. */

	/* 2-way flow control */
	tp->c_iflag |= IXON | IXOFF;
	tp->c_lflag |= ICANON | ISIG | ECHO | ECHOE | ECHOK | ECHOKE;
	/* no longer| ECHOCTL | ECHOPRT */
	tp->c_oflag |= OPOST;
	/* tp->c_cflag = 0; */
	tp->c_cc[VINTR] = DEF_INTR;
	tp->c_cc[VQUIT] = DEF_QUIT;
	tp->c_cc[VEOF] = DEF_EOF;
	tp->c_cc[VEOL] = DEF_EOL;
#ifdef __linux__
	tp->c_cc[VSWTC] = DEF_SWITCH;
#elif defined(VSWTCH)
	tp->c_cc[VSWTCH] = DEF_SWITCH;
#endif				/* __linux__ */

	/* Account for special characters seen in input. */
	if (cp->eol == CR) {
		tp->c_iflag |= ICRNL;
		tp->c_oflag |= ONLCR;
	}
	tp->c_cc[VERASE] = cp->erase;
	tp->c_cc[VKILL] = cp->kill;

	/* Account for the presence or absence of parity bits in input. */
	switch (cp->parity) {
	case 0:
		/* space (always 0) parity */
		break;
	case 1:
		/* odd parity */
		tp->c_cflag |= PARODD;
		/* do not break */
	case 2:
		/* even parity */
		tp->c_cflag |= PARENB;
		tp->c_iflag |= INPCK | ISTRIP;
		/* do not break */
	case (1 | 2):
		/* no parity bit */
		tp->c_cflag &= ~CSIZE;
		tp->c_cflag |= CS7;
		break;
	}
	/* Account for upper case without lower case. */
	if (cp->capslock) {
#ifdef IUCLC
		tp->c_iflag |= IUCLC;
#endif
#ifdef XCASE
		tp->c_lflag |= XCASE;
#endif
#ifdef OLCUC
		tp->c_oflag |= OLCUC;
#endif
	}
	/* Optionally enable hardware flow control. */
#ifdef	CRTSCTS
	if (op->flags & F_RTSCTS)
		tp->c_cflag |= CRTSCTS;
#endif

	/* Finally, make the new settings effective. */
	if (tcsetattr(STDIN_FILENO, TCSANOW, tp) < 0)
		log_err("%s: tcsetattr: TCSANOW: %m", op->tty);
}

/*
 * String contains upper case without lower case.
 * http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=52940
 * http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=156242
 */
static int caps_lock(char *s)
{
	int capslock;

	for (capslock = 0; *s; s++) {
		if (islower(*s))
			return EXIT_SUCCESS;
		if (capslock == 0)
			capslock = isupper(*s);
	}
	return capslock;
}

/* Convert speed string to speed code; return 0 on failure. */
static speed_t bcode(char *s)
{
	const struct Speedtab *sp;
	long speed = atol(s);

	for (sp = speedtab; sp->speed; sp++)
		if (sp->speed == speed)
			return sp->code;
	return 0;
}

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fprintf(out, _("\nUsage:\n"
		       " %1$s [options] line baud_rate,... [termtype]\n"
		       " %1$s [options] baud_rate,... line [termtype]\n"),
		program_invocation_short_name);

	fprintf(out, _("\nOptions:\n"
		       " -8, --8bits                assume 8-bit tty\n"
		       " -a, --autologin <user>     login the specified user automatically\n"
		       " -c, --noreset              do not reset control mode\n"
		       " -f, --issue-file <file>    display issue file\n"
		       " -h, --flow-control         enable hardware flow control\n"
		       " -H, --host <hostname>      specify login host\n"
		       " -i, --noissue              do not display issue file\n"
		       " -I, --init-string <string> set init string\n"
		       " -l, --login-program <file> specify login program\n"
		       " -L, --local-line           force local line\n"
		       " -m, --extract-baud         extract baud rate during connect\n"
		       " -n, --skip-login           do not prompt for login\n"
		       " -o, --login-options <opts> options that are passed to login\n"
		       " -p, --loginpause           wait for any key before the login\n"
		       " -R, --hangup               do virtually hangup on the tty\n"
		       " -s, --keep-baud            try to keep baud rate after break\n"
		       " -t, --timeout <number>     login process timeout\n"
		       " -U, --detect-case          detect uppercase terminal\n"
		       " -w, --wait-cr              wait carriage-return\n"
		       "     --noclear              do not clear the screen before prompt\n"
		       "     --nohints              do not print hints\n"
		       "     --nonewline            do not print a newline before issue\n"
		       "     --no-hostname          no hostname at all will be shown\n"
		       "     --long-hostname        show full qualified hostname\n"
		       "     --version              output version information and exit\n"
		       "     --help                 display this help and exit\n\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/*
 * Helper function reports errors to console or syslog.
 * Will be used by log_err() and log_warn() therefore
 * it takes a format as well as va_list.
 */
#define	str2cpy(b,s1,s2)	strcat(strcpy(b,s1),s2)

static void dolog(int priority, const char *fmt, va_list ap)
{
#ifndef	USE_SYSLOG
	int fd;
#endif
	char buf[BUFSIZ];
	char *bp;

	/*
	 * If the diagnostic is reported via syslog(3), the process name is
	 * automatically prepended to the message. If we write directly to
	 * /dev/console, we must prepend the process name ourselves.
	 */
#ifdef USE_SYSLOG
	buf[0] = '\0';
	bp = buf;
#else
	str2cpy(buf, program_invocation_short_name, ": ");
	bp = buf + strlen(buf);
#endif				/* USE_SYSLOG */
	vsnprintf(bp, sizeof(buf)-strlen(buf), fmt, ap);

	/*
	 * Write the diagnostic directly to /dev/console if we do not use the
	 * syslog(3) facility.
	 */
#ifdef	USE_SYSLOG
	openlog(program_invocation_short_name, LOG_PID, LOG_AUTHPRIV);
	syslog(priority, "%s", buf);
	closelog();
#else
	/* Terminate with CR-LF since the console mode is unknown. */
	strcat(bp, "\r\n");
	if ((fd = open("/dev/console", 1)) >= 0) {
		write_all(fd, buf, strlen(buf));
		close(fd);
	}
#endif				/* USE_SYSLOG */
}

static void log_err(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	dolog(LOG_ERR, fmt, ap);
	va_end(ap);

	/* Be kind to init(8). */
	sleep(10);
	exit(EXIT_FAILURE);
}

static void log_warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	dolog(LOG_WARNING, fmt, ap);
	va_end(ap);
}

static void output_special_char(unsigned char c, struct options *op,
				struct termios *tp)
{
	struct utsname uts;

	uname(&uts);

	switch (c) {
	case 's':
		printf("%s", uts.sysname);
		break;
	case 'n':
		printf("%s", uts.nodename);
		break;
	case 'r':
		printf("%s", uts.release);
		break;
	case 'v':
		printf("%s", uts.version);
		break;
	case 'm':
		printf("%s", uts.machine);
		break;
	case 'o':
	{
		char domainname[MAXHOSTNAMELEN+1];
#ifdef HAVE_GETDOMAINNAME
		if (getdomainname(domainname, sizeof(domainname)))
#endif
		strcpy(domainname, "unknown_domain");
		domainname[sizeof(domainname)-1] = '\0';
		printf("%s", domainname);
		break;
	}
	case 'O':
	{
		char *dom = "unknown_domain";
		char host[MAXHOSTNAMELEN+1];
		struct addrinfo hints, *info = NULL;

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_CANONNAME;

		if (gethostname(host, sizeof(host)) ||
		    getaddrinfo(host, NULL, &hints, &info) ||
		    info == NULL)
			fputs(dom, stdout);
		else {
			char *canon;
			if (info->ai_canonname &&
			    (canon = strchr(info->ai_canonname, '.')))
				dom = canon + 1;
			fputs(dom, stdout);
			freeaddrinfo(info);
		}
		break;
	}
	case 'd':
	case 't':
	{
		time_t now;
		struct tm *tm;

		time(&now);
		tm = localtime(&now);

		if (!tm)
			break;

		if (c == 'd') /* ISO 8601 */
			printf("%s %s %d  %d",
				      nl_langinfo(ABDAY_1 + tm->tm_wday),
				      nl_langinfo(ABMON_1 + tm->tm_mon),
				      tm->tm_mday,
				      tm->tm_year < 70 ? tm->tm_year + 2000 :
				      tm->tm_year + 1900);
		else
			printf("%02d:%02d:%02d",
				      tm->tm_hour, tm->tm_min, tm->tm_sec);
		break;
	}
	case 'l':
		printf ("%s", op->tty);
		break;
	case 'b':
	{
		const speed_t speed = cfgetispeed(tp);
		int i;

		for (i = 0; speedtab[i].speed; i++) {
			if (speedtab[i].code == speed) {
				printf("%ld", speedtab[i].speed);
				break;
			}
		}
		break;
	}
	case 'u':
	case 'U':
	{
		int users = 0;
		struct utmp *ut;
		setutent();
		while ((ut = getutent()))
			if (ut->ut_type == USER_PROCESS)
				users++;
		endutent();
		printf ("%d ", users);
		if (c == 'U')
			printf((users == 1) ? _("user") : _("users"));
		break;
	}
	default:
		putchar(c);
		break;
	}
}

static void init_special_char(char* arg, struct options *op)
{
	char ch, *p, *q;
	int i;

	op->initstring = malloc(strlen(arg) + 1);
	if (!op->initstring)
		log_err(_("failed to allocate memory: %m"));

	/*
	 * Copy optarg into op->initstring decoding \ddd octal
	 * codes into chars.
	 */
	q = op->initstring;
	p = arg;
	while (*p) {
		/* The \\ is converted to \ */
		if (*p == '\\') {
			p++;
			if (*p == '\\') {
				ch = '\\';
				p++;
			} else {
				/* Handle \000 - \177. */
				ch = 0;
				for (i = 1; i <= 3; i++) {
					if (*p >= '0' && *p <= '7') {
						ch <<= 3;
						ch += *p - '0';
						p++;
					} else {
						break;
					}
				}
			}
			*q++ = ch;
		} else
			*q++ = *p++;
	}
	*q = '\0';
}

/*
 * Appends @str to @dest and if @dest is not empty then use use @sep as a
 * separator. The maximal final length of the @dest is @len.
 *
 * Returns the final @dest length or -1 in case of error.
 */
static ssize_t append(char *dest, size_t len, const char  *sep, const char *src)
{
	size_t dsz = 0, ssz = 0, sz;
	char *p;

	if (!dest || !len || !src)
		return -1;

	if (*dest)
		dsz = strlen(dest);
	if (dsz && sep)
		ssz = strlen(sep);
	sz = strlen(src);

	if (dsz + ssz + sz + 1 > len)
		return -1;

	p = dest + dsz;
	if (ssz) {
		memcpy(p, sep, ssz);
		p += ssz;
	}
	memcpy(p, src, sz);
	*(p + sz) = '\0';

	return dsz + ssz + sz;
}

/*
 * Do not allow the user to pass an option as a user name
 * To be more safe: Use `--' to make sure the rest is
 * interpreted as non-options by the program, if it supports it.
 */
static void check_username(const char* nm)
{
	const char *p = nm;
	if (!nm)
		goto err;
	if (strlen(nm) > 42)
		goto err;
	while (isspace(*p))
		p++;
	if (*p == '-')
		goto err;
	return;
err:
	errno = EPERM;
	log_err("checkname: %m");
}

