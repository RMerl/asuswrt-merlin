/*
 * login(1)
 *
 * This program is derived from 4.3 BSD software and is subject to the
 * copyright notice below.
 *
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 * Rewritten to PAM-only version.
 *
 * Michael Glad (glad@daimi.dk)
 * Computer Science Department, Aarhus University, Denmark
 * 1990-07-04
 *
 * Copyright (c) 1980, 1987, 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <memory.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <utmp.h>
#include <stdlib.h>
#include <sys/syslog.h>
#include <sys/sysmacros.h>
#include <linux/major.h>
#include <netdb.h>
#include <lastlog.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <sys/sendfile.h>

#ifdef HAVE_LIBAUDIT
# include <libaudit.h>
#endif

#include "c.h"
#include "setproctitle.h"
#include "pathnames.h"
#include "strutils.h"
#include "nls.h"
#include "xalloc.h"
#include "writeall.h"

#include "logindefs.h"

#define is_pam_failure(_rc)	((_rc) != PAM_SUCCESS)

#define LOGIN_MAX_TRIES        3
#define LOGIN_EXIT_TIMEOUT     5
#define LOGIN_TIMEOUT          60

#ifdef USE_TTY_GROUP
# define TTY_MODE 0620
#else
# define TTY_MODE 0600
#endif

#define	TTYGRPNAME	"tty"	/* name of group to own ttys */
#define VCS_PATH_MAX	64

/*
 * Login control struct
 */
struct login_context {
	const char	*tty_path;	/* ttyname() return value */
	const char	*tty_name;	/* tty_path without /dev prefix */
	const char	*tty_number;	/* end of the tty_path */
	mode_t		tty_mode;	/* chmod() mode */

	char		*username;	/* from command line or PAM */

	struct passwd	*pwd;		/* user info */

	pam_handle_t	*pamh;		/* PAM handler */
	struct pam_conv	conv;		/* PAM conversation */

#ifdef LOGIN_CHOWN_VCS
	char		vcsn[VCS_PATH_MAX];	/* virtual console name */
	char		vcsan[VCS_PATH_MAX];
#endif

	char		thishost[MAXHOSTNAMELEN + 1];	/* this machine */
	char		*thisdomain;			/* this machine domain */
	char		*hostname;			/* remote machine */
	char		hostaddress[16];		/* remote address */

	pid_t		pid;
	int		quiet;		/* 1 is hush file exists */

	unsigned int	remote:1,	/* login -h */
			nohost:1,	/* login -H */
			noauth:1,	/* login -f */
			keep_env:1;	/* login -p */
};

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
static int timeout = LOGIN_TIMEOUT;
static int child_pid = 0;
static volatile int got_sig = 0;

/*
 * Robert Ambrose writes:
 * A couple of my users have a problem with login processes hanging around
 * soaking up pts's.  What they seem to hung up on is trying to write out the
 * message 'Login timed out after %d seconds' when the connection has already
 * been dropped.
 * What I did was add a second timeout while trying to write the message so
 * the process just exits if the second timeout expires.
 */
static void __attribute__ ((__noreturn__))
timedout2(int sig __attribute__ ((__unused__)))
{
	struct termios ti;

	/* reset echo */
	tcgetattr(0, &ti);
	ti.c_lflag |= ECHO;
	tcsetattr(0, TCSANOW, &ti);
	exit(EXIT_SUCCESS);	/* %% */
}

static void timedout(int sig __attribute__ ((__unused__)))
{
	signal(SIGALRM, timedout2);
	alarm(10);
	/* TRANSLATORS: The standard value for %d is 60. */
	warnx(_("timed out after %d seconds"), timeout);
	signal(SIGALRM, SIG_IGN);
	alarm(0);
	timedout2(0);
}

/*
 * This handler allows to inform a shell about signals to login. If you have
 * (root) permissions you can kill all login childrent by one signal to login
 * process.
 *
 * Also, parent who is session leader is able (before setsid() in child) to
 * inform child when controlling tty goes away (e.g. modem hangup, SIGHUP).
 */
static void sig_handler(int signal)
{
	if (child_pid)
		kill(-child_pid, signal);
	else
		got_sig = 1;
	if (signal == SIGTERM)
		kill(-child_pid, SIGHUP);	/* because the shell often ignores SIGTERM */
}

/*
 * Let use delay for all exit() calls when user is not authenticated or
 * session fully initialized (loginpam_session()).
 */
static void __attribute__ ((__noreturn__)) sleepexit(int eval)
{
	sleep(getlogindefs_num("FAIL_DELAY", LOGIN_EXIT_TIMEOUT));
	exit(eval);
}

static const char *get_thishost(struct login_context *cxt, const char **domain)
{
	if (!*cxt->thishost) {
		if (gethostname(cxt->thishost, sizeof(cxt->thishost))) {
			if (domain)
				*domain = NULL;
			return NULL;
		}
		cxt->thishost[sizeof(cxt->thishost) -1] = '\0';
		cxt->thisdomain = strchr(cxt->thishost, '.');
		if (cxt->thisdomain)
			*cxt->thisdomain++ = '\0';
	}

	if (domain)
		*domain = cxt->thisdomain;
	return cxt->thishost;
}

/*
 * Output the /etc/motd file
 *
 * motd() determines the name of a login announcement file and outputs it to
 * the user's terminal at login time.  The MOTD_FILE configuration option is a
 * colon-delimited list of filenames. The empty MOTD_FILE option disables motd
 * printing at all.
 */
static void motd(void)
{
	char *motdlist, *motdfile;
	const char *mb;

	mb = getlogindefs_str("MOTD_FILE", _PATH_MOTDFILE);
	if (!mb || !*mb)
		return;

	motdlist = xstrdup(mb);

	for (motdfile = strtok(motdlist, ":"); motdfile;
	     motdfile = strtok(NULL, ":")) {

		struct stat st;
		int fd;

		if (stat(motdfile, &st) || !st.st_size)
			continue;
		fd = open(motdfile, O_RDONLY, 0);
		if (fd < 0)
			continue;

		sendfile(fileno(stdout), fd, NULL, st.st_size);
		close(fd);
	}

	free(motdlist);
}

/*
 * Nice and simple code provided by Linus Torvalds 16-Feb-93
 * Nonblocking stuff by Maciej W. Rozycki, macro@ds2.pg.gda.pl, 1999.
 *
 * He writes: "Login performs open() on a tty in a blocking mode.
 * In some cases it may make login wait in open() for carrier infinitely,
 * for example if the line is a simplistic case of a three-wire serial
 * connection. I believe login should open the line in the non-blocking mode
 * leaving the decision to make a connection to getty (where it actually
 * belongs).
 */
static void open_tty(const char *tty)
{
	int i, fd, flags;

	fd = open(tty, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		syslog(LOG_ERR, _("FATAL: can't reopen tty: %m"));
		sleepexit(EXIT_FAILURE);
	}

	if (!isatty(fd)) {
		close(fd);
		syslog(LOG_ERR, _("FATAL: %s is not a terminal"), tty);
		sleepexit(EXIT_FAILURE);
	}

	flags = fcntl(fd, F_GETFL);
	flags &= ~O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);

	for (i = 0; i < fd; i++)
		close(i);
	for (i = 0; i < 3; i++)
		if (fd != i)
			dup2(fd, i);
	if (fd >= 3)
		close(fd);
}

#define chown_err(_what, _uid, _gid) \
		syslog(LOG_ERR, _("chown (%s, %lu, %lu) failed: %m"), \
			(_what), (unsigned long) (_uid), (unsigned long) (_gid))

#define chmod_err(_what, _mode) \
		syslog(LOG_ERR, _("chmod (%s, %u) failed: %m"), (_what), (_mode))

static void chown_tty(struct login_context *cxt)
{
	const char *grname;
	uid_t uid = cxt->pwd->pw_uid;
	gid_t gid = cxt->pwd->pw_gid;

	grname = getlogindefs_str("TTYGROUP", TTYGRPNAME);
	if (grname && *grname) {
		if (*grname >= 0 && *grname <= 9)		/* group by ID */
			gid = getlogindefs_num("TTYGROUP", gid);
		else {						/* group by name */
			struct group *gr = getgrnam(grname);
			if (gr)
				gid = gr->gr_gid;
		}
	}

	if (fchown(0, uid, gid))				/* tty */
		chown_err(cxt->tty_name, uid, gid);
	if (fchmod(0, cxt->tty_mode))
		chmod_err(cxt->tty_name, cxt->tty_mode);

#ifdef LOGIN_CHOWN_VCS
	if (is_consoletty(0)) {
		if (chown(cxt->vcs, uid, gid))			/* vcs */
			chown_err(cxt->vcs, uid, gid);
		if (chmod(cxt->vcs, cxt->tty_mode))
			chmod_err(cxt->vcs, cxt->tty_mode);

		if (chown(cxt->vcsa, uid, gid))			/* vcsa */
			chown_err(cxt->vcsa, uid, gid);
		if (chmod(cxt->vcsa, cxt->tty_mode))
			chmod_err(cxt->vcsa, cxt->tty_mode);
	}
#endif
}

/*
 * Reads the currect terminal path and initialize cxt->tty_* variables.
 */
static void init_tty(struct login_context *cxt)
{
	const char *p;
	struct stat st;
	struct termios tt, ttt;

	cxt->tty_mode = (mode_t) getlogindefs_num("TTYPERM", TTY_MODE);

	cxt->tty_path = ttyname(0);		/* libc calls istty() here */

	/*
	 * In case login is suid it was possible to use a hardlink as stdin
	 * and exploit races for a local root exploit. (Wojciech Purczynski).
	 *
	 * More precisely, the problem is  ttyn := ttyname(0); ...; chown(ttyn);
	 * here ttyname() might return "/tmp/x", a hardlink to a pseudotty.
	 * All of this is a problem only when login is suid, which it isnt.
	 */
	if (!cxt->tty_path || !*cxt->tty_path ||
	    lstat(cxt->tty_path, &st) != 0 || !S_ISCHR(st.st_mode) ||
	    (st.st_nlink > 1 && strncmp(cxt->tty_path, "/dev/", 5)) ||
	    access(cxt->tty_path, R_OK | W_OK) != 0) {

		syslog(LOG_ERR, _("FATAL: bad tty"));
		sleepexit(EXIT_FAILURE);
	}

	if (strncmp(cxt->tty_path, "/dev/", 5) == 0)
		cxt->tty_name = cxt->tty_path + 5;
	else
		cxt->tty_name = cxt->tty_path;

	for (p = cxt->tty_name; p && *p; p++) {
		if (isdigit(*p)) {
			cxt->tty_number = p;
			break;
		}
	}

#ifdef LOGIN_CHOWN_VCS
	/* find names of Virtual Console devices, for later mode change */
	snprintf(cxt->vcsn, sizeof(cxt->vcsn), "/dev/vcs%s", cxt->tty_number);
	snprintf(cxt->vcsan, sizeof(cxt->vcsan), "/dev/vcsa%s", cxt->tty_number);
#endif

	tcgetattr(0, &tt);
	ttt = tt;
	ttt.c_cflag &= ~HUPCL;

	if ((fchown(0, 0, 0) || fchmod(0, cxt->tty_mode)) && errno != EROFS) {

		syslog(LOG_ERR, _("FATAL: %s: change permissions failed: %m"),
				cxt->tty_path);
		sleepexit(EXIT_FAILURE);
	}

	/* Kill processes left on this tty */
	tcsetattr(0, TCSAFLUSH, &ttt);

	signal(SIGHUP, SIG_IGN);	/* so vhangup() wont kill us */
	vhangup();
	signal(SIGHUP, SIG_DFL);

	/* open stdin,stdout,stderr to the tty */
	open_tty(cxt->tty_path);

	/* restore tty modes */
	tcsetattr(0, TCSAFLUSH, &tt);
}


#ifdef LOGIN_CHOWN_VCS
/* true if the filedescriptor fd is a console tty, very Linux specific */
static int is_consoletty(int fd)
{
	struct stat stb;

	if ((fstat(fd, &stb) >= 0)
	    && (major(stb.st_rdev) == TTY_MAJOR)
	    && (minor(stb.st_rdev) < 64)) {
		return 1;
	}
	return 0;
}
#endif

/*
 * Log failed login attempts in _PATH_BTMP if that exists.
 * Must be called only with username the name of an actual user.
 * The most common login failure is to give password instead of username.
 */
static void log_btmp(struct login_context *cxt)
{
	struct utmp ut;
	struct timeval tv;

	memset(&ut, 0, sizeof(ut));

	strncpy(ut.ut_user,
		cxt->username ? cxt->username : "(unknown)",
		sizeof(ut.ut_user));

	if (cxt->tty_number)
		strncpy(ut.ut_id, cxt->tty_number, sizeof(ut.ut_id));
	if (cxt->tty_name)
		xstrncpy(ut.ut_line, cxt->tty_name, sizeof(ut.ut_line));

#if defined(_HAVE_UT_TV)	/* in <utmpbits.h> included by <utmp.h> */
	gettimeofday(&tv, NULL);
	ut.ut_tv.tv_sec = tv.tv_sec;
	ut.ut_tv.tv_usec = tv.tv_usec;
#else
	{
		time_t t;
		time(&t);
		ut.ut_time = t;	/* ut_time is not always a time_t */
	}
#endif

	ut.ut_type = LOGIN_PROCESS;	/* XXX doesn't matter */
	ut.ut_pid = cxt->pid;

	if (cxt->hostname) {
		xstrncpy(ut.ut_host, cxt->hostname, sizeof(ut.ut_host));
		if (*cxt->hostaddress)
			memcpy(&ut.ut_addr_v6, cxt->hostaddress,
			       sizeof(ut.ut_addr_v6));
	}

	updwtmp(_PATH_BTMP, &ut);
}


#ifdef HAVE_LIBAUDIT
static void log_audit(struct login_context *cxt, int status)
{
	int audit_fd;
	struct passwd *pwd = cxt->pwd;

	audit_fd = audit_open();
	if (audit_fd == -1)
		return;
	if (!pwd && cxt->username)
		pwd = getpwnam(cxt->username);

	audit_log_acct_message(audit_fd,
			       AUDIT_USER_LOGIN,
			       NULL,
			       "login",
			       cxt->username ? cxt->username : "(unknown)",
			       pwd ? pwd->pw_uid : (unsigned int) -1,
			       cxt->hostname,
			       NULL,
			       cxt->tty_name,
			       status);

	close(audit_fd);
}
#else				/* !HAVE_LIBAUDIT */
# define log_audit(cxt, status)
#endif				/* HAVE_LIBAUDIT */

static void log_lastlog(struct login_context *cxt)
{
	struct lastlog ll;
	time_t t;
	int fd;

	if (!cxt->pwd)
		return;

	fd = open(_PATH_LASTLOG, O_RDWR, 0);
	if (fd < 0)
		return;

	lseek(fd, (off_t) cxt->pwd->pw_uid * sizeof(ll), SEEK_SET);

	/*
	 * Print last log message
	 */
	if (!cxt->quiet) {
		if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
							ll.ll_time != 0) {
			time_t ll_time = (time_t) ll.ll_time;

			printf(_("Last login: %.*s "), 24 - 5, ctime(&ll_time));
			if (*ll.ll_host != '\0')
				printf(_("from %.*s\n"),
				       (int)sizeof(ll.ll_host), ll.ll_host);
			else
				printf(_("on %.*s\n"),
				       (int)sizeof(ll.ll_line), ll.ll_line);
		}
		lseek(fd, (off_t) cxt->pwd->pw_uid * sizeof(ll), SEEK_SET);
	}

	memset((char *)&ll, 0, sizeof(ll));

	time(&t);
	ll.ll_time = t;		/* ll_time is always 32bit */

	if (cxt->tty_name)
		xstrncpy(ll.ll_line, cxt->tty_name, sizeof(ll.ll_line));
	if (cxt->hostname)
		xstrncpy(ll.ll_host, cxt->hostname, sizeof(ll.ll_host));

	if (write_all(fd, (char *)&ll, sizeof(ll)))
		warn(_("write lastlog failed"));

	close(fd);
}

/*
 * Update wtmp and utmp logs
 */
static void log_utmp(struct login_context *cxt)
{
	struct utmp ut;
	struct utmp *utp;
	struct timeval tv;

	utmpname(_PATH_UTMP);
	setutent();

	/* Find pid in utmp.
	 *
	 * login sometimes overwrites the runlevel entry in /var/run/utmp,
	 * confusing sysvinit. I added a test for the entry type, and the
	 * problem was gone. (In a runlevel entry, st_pid is not really a pid
	 * but some number calculated from the previous and current runlevel).
	 * -- Michael Riepe <michael@stud.uni-hannover.de>
	 */
	while ((utp = getutent()))
		if (utp->ut_pid == cxt->pid
		    && utp->ut_type >= INIT_PROCESS
		    && utp->ut_type <= DEAD_PROCESS)
			break;

	/* If we can't find a pre-existing entry by pid, try by line.
	 * BSD network daemons may rely on this.
	 */
	if (utp == NULL) {
		setutent();
		ut.ut_type = LOGIN_PROCESS;
		strncpy(ut.ut_line, cxt->tty_name, sizeof(ut.ut_line));
		utp = getutline(&ut);
	}

	if (utp)
		memcpy(&ut, utp, sizeof(ut));
	else
		/* some gettys/telnetds don't initialize utmp... */
		memset(&ut, 0, sizeof(ut));

	if (ut.ut_id[0] == 0)
		strncpy(ut.ut_id, cxt->tty_number, sizeof(ut.ut_id));

	strncpy(ut.ut_user, cxt->username, sizeof(ut.ut_user));
	xstrncpy(ut.ut_line, cxt->tty_name, sizeof(ut.ut_line));

#ifdef _HAVE_UT_TV		/* in <utmpbits.h> included by <utmp.h> */
	gettimeofday(&tv, NULL);
	ut.ut_tv.tv_sec = tv.tv_sec;
	ut.ut_tv.tv_usec = tv.tv_usec;
#else
	{
		time_t t;
		time(&t);
		ut.ut_time = t;	/* ut_time is not always a time_t */
				/* glibc2 #defines it as ut_tv.tv_sec */
	}
#endif
	ut.ut_type = USER_PROCESS;
	ut.ut_pid = cxt->pid;
	if (cxt->hostname) {
		xstrncpy(ut.ut_host, cxt->hostname, sizeof(ut.ut_host));
		if (*cxt->hostaddress)
			memcpy(&ut.ut_addr_v6, cxt->hostaddress,
			       sizeof(ut.ut_addr_v6));
	}

	pututline(&ut);
	endutent();

	updwtmp(_PATH_WTMP, &ut);
}

static void log_syslog(struct login_context *cxt)
{
	struct passwd *pwd = cxt->pwd;

	if (!strncmp(cxt->tty_name, "ttyS", 4))
		syslog(LOG_INFO, _("DIALUP AT %s BY %s"),
		       cxt->tty_name, pwd->pw_name);

	if (!pwd->pw_uid) {
		if (cxt->hostname)
			syslog(LOG_NOTICE, _("ROOT LOGIN ON %s FROM %s"),
			       cxt->tty_name, cxt->hostname);
		else
			syslog(LOG_NOTICE, _("ROOT LOGIN ON %s"), cxt->tty_name);
	} else {
		if (cxt->hostname)
			syslog(LOG_INFO, _("LOGIN ON %s BY %s FROM %s"),
			       cxt->tty_name, pwd->pw_name, cxt->hostname);
		else
			syslog(LOG_INFO, _("LOGIN ON %s BY %s"), cxt->tty_name,
			       pwd->pw_name);
	}
}

static struct passwd *get_passwd_entry(const char *username,
					 char **pwdbuf,
					 struct passwd *pwd)
{
	struct passwd *res = NULL;
	size_t sz = 16384;
	int x;

	if (!pwdbuf || !username)
		return NULL;

#ifdef _SC_GETPW_R_SIZE_MAX
	{
		long xsz = sysconf(_SC_GETPW_R_SIZE_MAX);
		if (xsz > 0)
			sz = (size_t) xsz;
	}
#endif
	*pwdbuf = xrealloc(*pwdbuf, sz);

	x = getpwnam_r(username, pwd, *pwdbuf, sz, &res);
	if (!res) {
		errno = x;
		return NULL;
	}
	return res;
}

/* encapsulate stupid "void **" pam_get_item() API */
static int loginpam_get_username(pam_handle_t *pamh, char **name)
{
	const void *item = (void *)*name;
	int rc;
	rc = pam_get_item(pamh, PAM_USER, &item);
	*name = (char *)item;
	return rc;
}

static void loginpam_err(pam_handle_t *pamh, int retcode)
{
	const char *msg = pam_strerror(pamh, retcode);

	if (msg) {
		fprintf(stderr, "\n%s\n", msg);
		syslog(LOG_ERR, "%s", msg);
	}
	pam_end(pamh, retcode);
	sleepexit(EXIT_FAILURE);
}

/*
 * Composes "<host> login: " string; or returns "login: " is -H is given
 */
static const char *loginpam_get_prompt(struct login_context *cxt)
{
	const char *host;
	char *prompt, *dflt_prompt = _("login: ");
	size_t sz;

	if (cxt->nohost || !(host = get_thishost(cxt, NULL)))
		return dflt_prompt;

	sz = strlen(host) + 1 + strlen(dflt_prompt) + 1;

	prompt = xmalloc(sz);
	snprintf(prompt, sz, "%s %s", host, dflt_prompt);

	return prompt;
}

static pam_handle_t *init_loginpam(struct login_context *cxt)
{
	pam_handle_t *pamh = NULL;
	int rc;

	/*
	 * username is initialized to NULL and if specified on the command line
	 * it is set.  Therefore, we are safe not setting it to anything
	 */
	rc = pam_start(cxt->remote ? "remote" : "login",
		       cxt->username, &cxt->conv, &pamh);
	if (rc != PAM_SUCCESS) {
		warnx(_("PAM failure, aborting: %s"), pam_strerror(pamh, rc));
		syslog(LOG_ERR, _("Couldn't initialize PAM: %s"),
		       pam_strerror(pamh, rc));
		sleepexit(EXIT_FAILURE);
	}

	/* hostname & tty are either set to NULL or their correct values,
	 * depending on how much we know
	 */
	rc = pam_set_item(pamh, PAM_RHOST, cxt->hostname);
	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	rc = pam_set_item(pamh, PAM_TTY, cxt->tty_name);
	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	/*
	 * Andrew.Taylor@cal.montage.ca: Provide a user prompt to PAM so that
	 * the "login: " prompt gets localized. Unfortunately, PAM doesn't have
	 * an interface to specify the "Password: " string (yet).
	 */
	rc = pam_set_item(pamh, PAM_USER_PROMPT, loginpam_get_prompt(cxt));
	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	/* we need't the original username. We have to follow PAM. */
	free(cxt->username);
	cxt->username = NULL;
	cxt->pamh = pamh;

	return pamh;
}

static void loginpam_auth(struct login_context *cxt)
{
	int rc, failcount = 0, show_unknown, retries;
	const char *hostname = cxt->hostname ? cxt->hostname :
			       cxt->tty_name ? cxt->tty_name : "<unknown>";
	pam_handle_t *pamh = cxt->pamh;

	/* if we didn't get a user on the command line, set it to NULL */
	loginpam_get_username(pamh, &cxt->username);

	show_unknown = getlogindefs_bool("LOG_UNKFAIL_ENAB", 0);
	retries = getlogindefs_num("LOGIN_RETRIES", LOGIN_MAX_TRIES);

	/*
	 * There may be better ways to deal with some of these conditions, but
	 * at least this way I don't think we'll be giving away information...
	 *
	 * Perhaps someday we can trust that all PAM modules will pay attention
	 * to failure count and get rid of LOGIN_MAX_TRIES?
	 */
	rc = pam_authenticate(pamh, 0);

	while ((++failcount < retries) &&
	       ((rc == PAM_AUTH_ERR) ||
		(rc == PAM_USER_UNKNOWN) ||
		(rc == PAM_CRED_INSUFFICIENT) ||
		(rc == PAM_AUTHINFO_UNAVAIL))) {

		if (rc == PAM_USER_UNKNOWN && !show_unknown)
			/*
			 * logging unknown usernames may be a security issue if
			 * an user enter her password instead of her login name
			 */
			cxt->username = NULL;
		else
			loginpam_get_username(pamh, &cxt->username);

		syslog(LOG_NOTICE,
		       _("FAILED LOGIN %d FROM %s FOR %s, %s"),
		       failcount, hostname,
		       cxt->username ? cxt->username : "(unknown)",
		       pam_strerror(pamh, rc));

		log_btmp(cxt);
		log_audit(cxt, 0);

		fprintf(stderr, _("Login incorrect\n\n"));

		pam_set_item(pamh, PAM_USER, NULL);
		rc = pam_authenticate(pamh, 0);
	}

	if (is_pam_failure(rc)) {

		if (rc == PAM_USER_UNKNOWN && !show_unknown)
			cxt->username = NULL;
		else
			loginpam_get_username(pamh, &cxt->username);

		if (rc == PAM_MAXTRIES)
			syslog(LOG_NOTICE,
			       _("TOO MANY LOGIN TRIES (%d) FROM %s FOR %s, %s"),
			       failcount, hostname,
			       cxt->username ? cxt->username : "(unknown)",
			       pam_strerror(pamh, rc));
		else
			syslog(LOG_NOTICE,
			       _("FAILED LOGIN SESSION FROM %s FOR %s, %s"),
			       hostname,
			       cxt->username ? cxt->username : "(unknown)",
			       pam_strerror(pamh, rc));

		log_btmp(cxt);
		log_audit(cxt, 0);

		fprintf(stderr, _("\nLogin incorrect\n"));
		pam_end(pamh, rc);
		sleepexit(EXIT_SUCCESS);
	}
}

static void loginpam_acct(struct login_context *cxt)
{
	int rc;
	pam_handle_t *pamh = cxt->pamh;

	rc = pam_acct_mgmt(pamh, 0);

	if (rc == PAM_NEW_AUTHTOK_REQD)
		rc = pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);

	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	/*
	 * Grab the user information out of the password file for future usage
	 * First get the username that we are actually using, though.
	 */
	rc = loginpam_get_username(pamh, &cxt->username);
	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	if (!cxt->username || !*cxt->username) {
		warnx(_("\nSession setup problem, abort."));
		syslog(LOG_ERR, _("NULL user name in %s:%d. Abort."),
		       __FUNCTION__, __LINE__);
		pam_end(pamh, PAM_SYSTEM_ERR);
		sleepexit(EXIT_FAILURE);
	}
}

/*
 * Note that position of the pam_setcred() call is discussable:
 *
 *  - the PAM docs recommends pam_setcred() before pam_open_session()
 *  - but the original RFC http://www.opengroup.org/rfc/mirror-rfc/rfc86.0.txt
 *    uses pam_setcred() after pam_open_session()
 *
 * The old login versions (before year 2011) followed the RFC. This is probably
 * not optimal, because there could be dependence between some session modules
 * and user's credentials.
 *
 * The best is probably to follow openssh and call pam_setcred() before and
 * after pam_open_session().                -- kzak@redhat.com (18-Nov-2011)
 *
 */
static void loginpam_session(struct login_context *cxt)
{
	int rc;
	pam_handle_t *pamh = cxt->pamh;

	rc = pam_setcred(pamh, PAM_ESTABLISH_CRED);
	if (is_pam_failure(rc))
		loginpam_err(pamh, rc);

	rc = pam_open_session(pamh, 0);
	if (is_pam_failure(rc)) {
		pam_setcred(cxt->pamh, PAM_DELETE_CRED);
		loginpam_err(pamh, rc);
	}

	rc = pam_setcred(pamh, PAM_REINITIALIZE_CRED);
	if (is_pam_failure(rc)) {
		pam_close_session(pamh, 0);
		loginpam_err(pamh, rc);
	}
}

/*
 * We need to check effective UID/GID. For example $HOME could be on root
 * squashed NFS or on NFS with UID mapping and access(2) uses real UID/GID.
 * The open(2) seems as the surest solution.
 * -- kzak@redhat.com (10-Apr-2009)
 */
static int effective_access(const char *path, int mode)
{
	int fd = open(path, mode);
	if (fd != -1)
		close(fd);
	return fd == -1 ? -1 : 0;
}

/*
 * Check per accout or global hush-login setting.
 *
 * Hushed mode is enabled:
 *
 * a) if global (e.g. /etc/hushlogins) hush file exists:
 *     1) for ALL ACCOUNTS if the file is empty
 *     2) for the current user if the username or shell are found in the file
 *
 * b) if ~/.hushlogin file exists
 *
 * The ~/.hushlogin is ignored if the global hush file exists.
 *
 * The HUSHLOGIN_FILE login.def variable overwrites the default hush filename.
 *
 * Note that shadow-utils login(1) does not support "a1)". The "a1)" is
 * necessary if you want to use PAM for "Last login" message.
 *
 * -- Karel Zak <kzak@redhat.com> (26-Aug-2011)
 *
 *
 * Per-account check requires some explanation: As root we may not be able to
 * read the directory of the user if it is on an NFS mounted filesystem. We
 * temporarily set our effective uid to the user-uid making sure that we keep
 * root privs. in the real uid.
 *
 * A portable solution would require a fork(), but we rely on Linux having the
 * BSD setreuid()
 */
static int get_hushlogin_status(struct passwd *pwd)
{
	const char *files[] = { _PATH_HUSHLOGINS, _PATH_HUSHLOGIN, NULL };
	const char *file;
	char buf[BUFSIZ];
	int i;

	file = getlogindefs_str("HUSHLOGIN_FILE", NULL);
	if (file) {
		if (!*file)
			return 0;	/* empty HUSHLOGIN_FILE defined */

		files[0] = file;
		files[1] = NULL;
	}

	for (i = 0; files[i]; i++) {
		int ok = 0;

		file = files[i];

		/* Global hush-file*/
		if (*file == '/') {
			struct stat st;
			FILE *f;

			if (stat(file, &st) != 0)
				continue;	/* file does not exist */

			if (st.st_size == 0)
				return 1;	/* for all accounts */

			f = fopen(file, "r");
			if (!f)
				continue;	/* ignore errors... */

			while (ok == 0 && fgets(buf, sizeof(buf), f)) {
				buf[strlen(buf) - 1] = '\0';
				ok = !strcmp(buf, *buf == '/' ? pwd->pw_shell :
								pwd->pw_name);
			}
			fclose(f);
			if (ok)
				return 1;	/* found username/shell */

			return 0;		/* ignore per-account files */
		}

		/* Per-account setting */
		if (strlen(pwd->pw_dir) + sizeof(file) + 2 > sizeof(buf))
			continue;
		else {
			uid_t ruid = getuid();
			gid_t egid = getegid();

			sprintf(buf, "%s/%s", pwd->pw_dir, file);
			setregid(-1, pwd->pw_gid);
			setreuid(0, pwd->pw_uid);
			ok = effective_access(buf, O_RDONLY) == 0;
			setuid(0);	/* setreuid doesn't do it alone! */
			setreuid(ruid, 0);
			setregid(-1, egid);

			if (ok)
				return 1;	/* enabled by user */
		}
	}

	return 0;
}

/*
 * Detach the controlling terminal, fork, restore syslog stuff and create a new
 * session.
 */
static void fork_session(struct login_context *cxt)
{
	struct sigaction sa, oldsa_hup, oldsa_term;

	signal(SIGALRM, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTSTP, SIG_IGN);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sa, NULL);

	sigaction(SIGHUP, &sa, &oldsa_hup);	/* ignore when TIOCNOTTY */

	/*
	 * detach the controlling tty
	 * -- we needn't the tty in parent who waits for child only.
	 *    The child calls setsid() that detach from the tty as well.
	 */
	ioctl(0, TIOCNOTTY, NULL);

	/*
	 * We have care about SIGTERM, because leave PAM session without
	 * pam_close_session() is pretty bad thing.
	 */
	sa.sa_handler = sig_handler;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, &oldsa_term);

	closelog();

	/*
	 * We must fork before setuid() because we need to call
	 * pam_close_session() as root.
	 */
	child_pid = fork();
	if (child_pid < 0) {
		/*
		 * fork() error
		 */
		warn(_("fork failed"));

		pam_setcred(cxt->pamh, PAM_DELETE_CRED);
		pam_end(cxt->pamh, pam_close_session(cxt->pamh, 0));
		sleepexit(EXIT_FAILURE);
	}

	if (child_pid) {
		/*
		 * parent - wait for child to finish, then cleanup session
		 */
		close(0);
		close(1);
		close(2);
		sa.sa_handler = SIG_IGN;
		sigaction(SIGQUIT, &sa, NULL);
		sigaction(SIGINT, &sa, NULL);

		/* wait as long as any child is there */
		while (wait(NULL) == -1 && errno == EINTR) ;
		openlog("login", LOG_ODELAY, LOG_AUTHPRIV);

		pam_setcred(cxt->pamh, PAM_DELETE_CRED);
		pam_end(cxt->pamh, pam_close_session(cxt->pamh, 0));
		exit(EXIT_SUCCESS);
	}

	/*
	 * child
	 */
	sigaction(SIGHUP, &oldsa_hup, NULL);		/* restore old state */
	sigaction(SIGTERM, &oldsa_term, NULL);
	if (got_sig)
		exit(EXIT_FAILURE);

	/*
	 * Problem: if the user's shell is a shell like ash that doesnt do
	 * setsid() or setpgrp(), then a ctrl-\, sending SIGQUIT to every
	 * process in the pgrp, will kill us.
	 */

	/* start new session */
	setsid();

	/* make sure we have a controlling tty */
	open_tty(cxt->tty_path);
	openlog("login", LOG_ODELAY, LOG_AUTHPRIV);	/* reopen */

	/*
	 * TIOCSCTTY: steal tty from other process group.
	 */
	if (ioctl(0, TIOCSCTTY, 1))
		syslog(LOG_ERR, _("TIOCSCTTY failed: %m"));
	signal(SIGINT, SIG_DFL);
}

/*
 * Initialize $TERM, $HOME, ...
 */
static void init_environ(struct login_context *cxt)
{
	struct passwd *pwd = cxt->pwd;
	char *termenv = NULL, **env;
	char tmp[PATH_MAX];
	int len, i;

	termenv = getenv("TERM");
	termenv = termenv ? xstrdup(termenv) : "dumb";

	/* destroy environment unless user has requested preservation (-p) */
	if (!cxt->keep_env) {
		environ = (char **) xmalloc(sizeof(char *));
		memset(environ, 0, sizeof(char *));
	}

	setenv("HOME", pwd->pw_dir, 0);	/* legal to override */
	setenv("SHELL", pwd->pw_shell, 1);
	setenv("TERM", termenv, 1);

	if (pwd->pw_uid)
		setenv("PATH", getlogindefs_str("ENV_PATH", _PATH_DEFPATH), 1);
	else {
		const char *x = getlogindefs_str("ENV_ROOTPATH", NULL);
		if (!x)
			x = getlogindefs_str("ENV_SUPATH", _PATH_DEFPATH_ROOT);
		setenv("PATH", x, 1);
	}

	/* mailx will give a funny error msg if you forget this one */
	len = snprintf(tmp, sizeof(tmp), "%s/%s", _PATH_MAILDIR, pwd->pw_name);
	if (len > 0 && (size_t) len + 1 <= sizeof(tmp))
		setenv("MAIL", tmp, 0);

	/* LOGNAME is not documented in login(1) but HP-UX 6.5 does it. We'll
	 * not allow modifying it.
	 */
	setenv("LOGNAME", pwd->pw_name, 1);

	env = pam_getenvlist(cxt->pamh);
	for (i = 0; env && env[i]; i++)
		putenv(env[i]);
}

/*
 * Called for -h option, initialize cxt->{hostname,hostaddress}
 */
static void init_remote_info(struct login_context *cxt, char *remotehost)
{
	const char *domain;
	char *p;
	struct addrinfo hints, *info = NULL;

	cxt->remote = 1;

	get_thishost(cxt, &domain);

	if (domain && (p = strchr(remotehost, '.')) &&
	    strcasecmp(p + 1, domain) == 0)
		*p = '\0';

	cxt->hostname = xstrdup(remotehost);

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	cxt->hostaddress[0] = 0;

	if (getaddrinfo(cxt->hostname, NULL, &hints, &info) == 0 && info) {
		if (info->ai_family == AF_INET) {
			struct sockaddr_in *sa =
				    (struct sockaddr_in *) info->ai_addr;

			memcpy(cxt->hostaddress, &(sa->sin_addr), sizeof(sa->sin_addr));

		} else if (info->ai_family == AF_INET6) {
			struct sockaddr_in6 *sa =
				     (struct sockaddr_in6 *) info->ai_addr;

			memcpy(cxt->hostaddress, &(sa->sin6_addr), sizeof(sa->sin6_addr));
		}
		freeaddrinfo(info);
	}
}

int main(int argc, char **argv)
{
	int c;
	int cnt;
	char *childArgv[10];
	char *buff;
	int childArgc = 0;
	int retcode;

	char *pwdbuf = NULL;
	struct passwd *pwd = NULL, _pwd;

	struct login_context cxt = {
		.tty_mode = TTY_MODE,		/* tty chmod() */
		.pid = getpid(),		/* PID */
		.conv = { misc_conv, NULL }	/* PAM conversation function */
	};

	timeout = getlogindefs_num("LOGIN_TIMEOUT", LOGIN_TIMEOUT);

	signal(SIGALRM, timedout);
	siginterrupt(SIGALRM, 1);	/* we have to interrupt syscalls like ioclt() */
	alarm((unsigned int)timeout);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	setpriority(PRIO_PROCESS, 0, 0);
	initproctitle(argc, argv);

	/*
	 * -p is used by getty to tell login not to destroy the environment
	 * -f is used to skip a second login authentication
	 * -h is used by other servers to pass the name of the remote
	 *    host to login so that it may be placed in utmp and wtmp
	 */
	while ((c = getopt(argc, argv, "fHh:pV")) != -1)
		switch (c) {
		case 'f':
			cxt.noauth = 1;
			break;

		case 'H':
			cxt.nohost = 1;
			break;

		case 'h':
			if (getuid()) {
				fprintf(stderr,
					_("login: -h for super-user only.\n"));
				exit(EXIT_FAILURE);
			}
			init_remote_info(&cxt, optarg);
			break;

		case 'p':
			cxt.keep_env = 1;
			break;

		case 'V':
			printf(UTIL_LINUX_VERSION);
			return EXIT_SUCCESS;
		case '?':
		default:
			fprintf(stderr, _("usage: login [ -p ] [ -h host ] [ -H ] [ -f username | username ]\n"));
			exit(EXIT_FAILURE);
		}
	argc -= optind;
	argv += optind;

	if (*argv) {
		char *p = *argv;
		cxt.username = xstrdup(p);

		/* wipe name - some people mistype their password here */
		/* (of course we are too late, but perhaps this helps a little ..) */
		while (*p)
			*p++ = ' ';
	}

	for (cnt = getdtablesize(); cnt > 2; cnt--)
		close(cnt);

	setpgrp();	 /* set pgid to pid this means that setsid() will fail */

	openlog("login", LOG_ODELAY, LOG_AUTHPRIV);

	init_tty(&cxt);
	init_loginpam(&cxt);

	/* login -f, then the user has already been authenticated */
	cxt.noauth = cxt.noauth && getuid() == 0 ? 1 : 0;

	if (!cxt.noauth)
		loginpam_auth(&cxt);

	/*
	 * Authentication may be skipped (for example, during krlogin, rlogin,
	 * etc...), but it doesn't mean that we can skip other account checks.
	 * The account could be disabled or password expired (althought
	 * kerberos ticket is valid).         -- kzak@redhat.com (22-Feb-2006)
	 */
	loginpam_acct(&cxt);

	if (!(cxt.pwd = get_passwd_entry(cxt.username, &pwdbuf, &_pwd))) {
		warnx(_("\nSession setup problem, abort."));
		syslog(LOG_ERR, _("Invalid user name \"%s\" in %s:%d. Abort."),
		       cxt.username, __FUNCTION__, __LINE__);
		pam_end(cxt.pamh, PAM_SYSTEM_ERR);
		sleepexit(EXIT_FAILURE);
	}

	pwd = cxt.pwd;
	cxt.username = pwd->pw_name;

	/*
	 * Initialize the supplementary group list. This should be done before
	 * pam_setcred because the PAM modules might add groups during
	 * pam_setcred.
	 *
         * For root we don't call initgroups, instead we call setgroups with
	 * group 0. This avoids the need to step through the whole group file,
	 * which can cause problems if NIS, NIS+, LDAP or something similar
	 * is used and the machine has network problems.
	 */
	retcode = pwd->pw_uid ? initgroups(cxt.username, pwd->pw_gid) :	/* user */
			        setgroups(0, NULL);			/* root */
	if (retcode < 0) {
		syslog(LOG_ERR, _("groups initialization failed: %m"));
		warnx(_("\nSession setup problem, abort."));
		pam_end(cxt.pamh, PAM_SYSTEM_ERR);
		sleepexit(EXIT_FAILURE);
	}

	/*
	 * Open PAM session (after successful authentication and account check)
	 */
	loginpam_session(&cxt);

	/* committed to login -- turn off timeout */
	alarm((unsigned int)0);

	endpwent();

	cxt.quiet = get_hushlogin_status(pwd);

	log_utmp(&cxt);
	log_audit(&cxt, 1);
	log_lastlog(&cxt);

	chown_tty(&cxt);

	if (setgid(pwd->pw_gid) < 0 && pwd->pw_gid) {
		syslog(LOG_ALERT, _("setgid() failed"));
		exit(EXIT_FAILURE);
	}

	if (pwd->pw_shell == NULL || *pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;

	init_environ(&cxt);		/* init $HOME, $TERM ... */

	setproctitle("login", cxt.username);

	log_syslog(&cxt);

	if (!cxt.quiet) {
		motd();

#ifdef LOGIN_STAT_MAIL
		/*
		 * This turns out to be a bad idea: when the mail spool
		 * is NFS mounted, and the NFS connection hangs, the
		 * login hangs, even root cannot login.
		 * Checking for mail should be done from the shell.
		 */
		{
			struct stat st;
			char *mail;

			mail = getenv("MAIL");
			if (mail && stat(mail, &st) == 0 && st.st_size != 0) {
				if (st.st_mtime > st.st_atime)
					printf(_("You have new mail.\n"));
				else
					printf(_("You have mail.\n"));
			}
		}
#endif
	}

	/*
	 * Detach the controlling terminal, fork() and create, new session
	 * and reinilizalize syslog stuff.
	 */
	fork_session(&cxt);

	/* discard permissions last so can't get killed and drop core */
	if (setuid(pwd->pw_uid) < 0 && pwd->pw_uid) {
		syslog(LOG_ALERT, _("setuid() failed"));
		exit(EXIT_FAILURE);
	}

	/* wait until here to change directory! */
	if (chdir(pwd->pw_dir) < 0) {
		warn(_("%s: change directory failed"), pwd->pw_dir);

		if (!getlogindefs_bool("DEFAULT_HOME", 1))
			exit(0);
		if (chdir("/"))
			exit(EXIT_FAILURE);
		pwd->pw_dir = "/";
		printf(_("Logging in with home = \"/\".\n"));
	}

	/* if the shell field has a space: treat it like a shell script */
	if (strchr(pwd->pw_shell, ' ')) {
		buff = xmalloc(strlen(pwd->pw_shell) + 6);

		strcpy(buff, "exec ");
		strcat(buff, pwd->pw_shell);
		childArgv[childArgc++] = "/bin/sh";
		childArgv[childArgc++] = "-sh";
		childArgv[childArgc++] = "-c";
		childArgv[childArgc++] = buff;
	} else {
		char tbuf[PATH_MAX + 2], *p;

		tbuf[0] = '-';
		xstrncpy(tbuf + 1, ((p = strrchr(pwd->pw_shell, '/')) ?
				    p + 1 : pwd->pw_shell), sizeof(tbuf) - 1);

		childArgv[childArgc++] = pwd->pw_shell;
		childArgv[childArgc++] = xstrdup(tbuf);
	}

	childArgv[childArgc++] = NULL;

	execvp(childArgv[0], childArgv + 1);

	if (!strcmp(childArgv[0], "/bin/sh"))
		warn(_("couldn't exec shell script"));
	else
		warn(_("no shell"));

	exit(EXIT_SUCCESS);
}


