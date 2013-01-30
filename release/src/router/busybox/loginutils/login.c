/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <syslog.h>
#if ENABLE_FEATURE_UTMP
# include <utmp.h> /* USER_PROCESS */
#endif
#include <sys/resource.h>

#if ENABLE_SELINUX
# include <selinux/selinux.h>  /* for is_selinux_enabled()  */
# include <selinux/get_context_list.h> /* for get_default_context() */
# include <selinux/flask.h> /* for security class definitions  */
#endif

#if ENABLE_PAM
/* PAM may include <locale.h>. We may need to undefine bbox's stub define: */
# undef setlocale
/* For some obscure reason, PAM is not in pam/xxx, but in security/xxx.
 * Apparently they like to confuse people. */
# include <security/pam_appl.h>
# include <security/pam_misc.h>
static const struct pam_conv conv = {
	misc_conv,
	NULL
};
#endif

enum {
	TIMEOUT = 60,
	EMPTY_USERNAME_COUNT = 10,
	USERNAME_SIZE = 32,
	TTYNAME_SIZE = 32,
};

static char* short_tty;

#if ENABLE_FEATURE_NOLOGIN
static void die_if_nologin(void)
{
	FILE *fp;
	int c;
	int empty = 1;

	fp = fopen_for_read("/etc/nologin");
	if (!fp) /* assuming it does not exist */
		return;

	while ((c = getc(fp)) != EOF) {
		if (c == '\n')
			bb_putchar('\r');
		bb_putchar(c);
		empty = 0;
	}
	if (empty)
		puts("\r\nSystem closed for routine maintenance\r");

	fclose(fp);
	fflush_all();
	/* Users say that they do need this prior to exit: */
	tcdrain(STDOUT_FILENO);
	exit(EXIT_FAILURE);
}
#else
# define die_if_nologin() ((void)0)
#endif

#if ENABLE_FEATURE_SECURETTY && !ENABLE_PAM
static int check_securetty(void)
{
	char *buf = (char*)"/etc/securetty"; /* any non-NULL is ok */
	parser_t *parser = config_open2("/etc/securetty", fopen_for_read);
	while (config_read(parser, &buf, 1, 1, "# \t", PARSE_NORMAL)) {
		if (strcmp(buf, short_tty) == 0)
			break;
		buf = NULL;
	}
	config_close(parser);
	/* buf != NULL here if config file was not found, empty
	 * or line was found which equals short_tty */
	return buf != NULL;
}
#else
static ALWAYS_INLINE int check_securetty(void) { return 1; }
#endif

#if ENABLE_SELINUX
static void initselinux(char *username, char *full_tty,
						security_context_t *user_sid)
{
	security_context_t old_tty_sid, new_tty_sid;

	if (!is_selinux_enabled())
		return;

	if (get_default_context(username, NULL, user_sid)) {
		bb_error_msg_and_die("can't get SID for %s", username);
	}
	if (getfilecon(full_tty, &old_tty_sid) < 0) {
		bb_perror_msg_and_die("getfilecon(%s) failed", full_tty);
	}
	if (security_compute_relabel(*user_sid, old_tty_sid,
				SECCLASS_CHR_FILE, &new_tty_sid) != 0) {
		bb_perror_msg_and_die("security_change_sid(%s) failed", full_tty);
	}
	if (setfilecon(full_tty, new_tty_sid) != 0) {
		bb_perror_msg_and_die("chsid(%s, %s) failed", full_tty, new_tty_sid);
	}
}
#endif

#if ENABLE_LOGIN_SCRIPTS
static void run_login_script(struct passwd *pw, char *full_tty)
{
	char *t_argv[2];

	t_argv[0] = getenv("LOGIN_PRE_SUID_SCRIPT");
	if (t_argv[0]) {
		t_argv[1] = NULL;
		xsetenv("LOGIN_TTY", full_tty);
		xsetenv("LOGIN_USER", pw->pw_name);
		xsetenv("LOGIN_UID", utoa(pw->pw_uid));
		xsetenv("LOGIN_GID", utoa(pw->pw_gid));
		xsetenv("LOGIN_SHELL", pw->pw_shell);
		spawn_and_wait(t_argv);	/* NOMMU-friendly */
		unsetenv("LOGIN_TTY");
		unsetenv("LOGIN_USER");
		unsetenv("LOGIN_UID");
		unsetenv("LOGIN_GID");
		unsetenv("LOGIN_SHELL");
	}
}
#else
void run_login_script(struct passwd *pw, char *full_tty);
#endif

static void get_username_or_die(char *buf, int size_buf)
{
	int c, cntdown;

	cntdown = EMPTY_USERNAME_COUNT;
 prompt:
	print_login_prompt();
	/* skip whitespace */
	do {
		c = getchar();
		if (c == EOF)
			exit(EXIT_FAILURE);
		if (c == '\n') {
			if (!--cntdown)
				exit(EXIT_FAILURE);
			goto prompt;
		}
	} while (isspace(c)); /* maybe isblank? */

	*buf++ = c;
	if (!fgets(buf, size_buf-2, stdin))
		exit(EXIT_FAILURE);
	if (!strchr(buf, '\n'))
		exit(EXIT_FAILURE);
	while ((unsigned char)*buf > ' ')
		buf++;
	*buf = '\0';
}

static void motd(void)
{
	int fd;

	fd = open(bb_path_motd_file, O_RDONLY);
	if (fd >= 0) {
		fflush_all();
		bb_copyfd_eof(fd, STDOUT_FILENO);
		close(fd);
	}
}

static void alarm_handler(int sig UNUSED_PARAM)
{
	/* This is the escape hatch!  Poor serial line users and the like
	 * arrive here when their connection is broken.
	 * We don't want to block here */
	ndelay_on(1);
	printf("\r\nLogin timed out after %d seconds\r\n", TIMEOUT);
	fflush_all();
	/* unix API is brain damaged regarding O_NONBLOCK,
	 * we should undo it, or else we can affect other processes */
	ndelay_off(1);
	_exit(EXIT_SUCCESS);
}

int login_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int login_main(int argc UNUSED_PARAM, char **argv)
{
	enum {
		LOGIN_OPT_f = (1<<0),
		LOGIN_OPT_h = (1<<1),
		LOGIN_OPT_p = (1<<2),
	};
	char *fromhost;
	char username[USERNAME_SIZE];
	const char *shell;
	int run_by_root;
	unsigned opt;
	int count = 0;
	struct passwd *pw;
	char *opt_host = NULL;
	char *opt_user = opt_user; /* for compiler */
	char *full_tty;
	IF_SELINUX(security_context_t user_sid = NULL;)
#if ENABLE_PAM
	int pamret;
	pam_handle_t *pamh;
	const char *pamuser;
	const char *failed_msg;
	struct passwd pwdstruct;
	char pwdbuf[256];
#endif

	username[0] = '\0';
	signal(SIGALRM, alarm_handler);
	alarm(TIMEOUT);

	/* More of suid paranoia if called by non-root: */
	/* Clear dangerous stuff, set PATH */
	run_by_root = !sanitize_env_if_suid();

	/* Mandatory paranoia for suid applet:
	 * ensure that fd# 0,1,2 are opened (at least to /dev/null)
	 * and any extra open fd's are closed.
	 * (The name of the function is misleading. Not daemonizing here.) */
	bb_daemonize_or_rexec(DAEMON_ONLY_SANITIZE | DAEMON_CLOSE_EXTRA_FDS, NULL);

	opt = getopt32(argv, "f:h:p", &opt_user, &opt_host);
	if (opt & LOGIN_OPT_f) {
		if (!run_by_root)
			bb_error_msg_and_die("-f is for root only");
		safe_strncpy(username, opt_user, sizeof(username));
	}
	argv += optind;
	if (argv[0]) /* user from command line (getty) */
		safe_strncpy(username, argv[0], sizeof(username));

	/* Let's find out and memorize our tty */
	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO))
		return EXIT_FAILURE;		/* Must be a terminal */
	full_tty = xmalloc_ttyname(STDIN_FILENO);
	if (!full_tty)
		full_tty = xstrdup("UNKNOWN");
	short_tty = skip_dev_pfx(full_tty);

	if (opt_host) {
		fromhost = xasprintf(" on '%s' from '%s'", short_tty, opt_host);
	} else {
		fromhost = xasprintf(" on '%s'", short_tty);
	}

	/* Was breaking "login <username>" from shell command line: */
	/*bb_setpgrp();*/

	openlog(applet_name, LOG_PID | LOG_CONS, LOG_AUTH);

	while (1) {
		/* flush away any type-ahead (as getty does) */
		ioctl(0, TCFLSH, TCIFLUSH);

		if (!username[0])
			get_username_or_die(username, sizeof(username));

#if ENABLE_PAM
		pamret = pam_start("login", username, &conv, &pamh);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "start";
			goto pam_auth_failed;
		}
		/* set TTY (so things like securetty work) */
		pamret = pam_set_item(pamh, PAM_TTY, short_tty);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "set_item(TTY)";
			goto pam_auth_failed;
		}
		pamret = pam_authenticate(pamh, 0);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "authenticate";
			goto pam_auth_failed;
			/* TODO: or just "goto auth_failed"
			 * since user seems to enter wrong password
			 * (in this case pamret == 7)
			 */
		}
		/* check that the account is healthy */
		pamret = pam_acct_mgmt(pamh, 0);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "acct_mgmt";
			goto pam_auth_failed;
		}
		/* read user back */
		pamuser = NULL;
		/* gcc: "dereferencing type-punned pointer breaks aliasing rules..."
		 * thus we cast to (void*) */
		if (pam_get_item(pamh, PAM_USER, (void*)&pamuser) != PAM_SUCCESS) {
			failed_msg = "get_item(USER)";
			goto pam_auth_failed;
		}
		if (!pamuser || !pamuser[0])
			goto auth_failed;
		safe_strncpy(username, pamuser, sizeof(username));
		/* Don't use "pw = getpwnam(username);",
		 * PAM is said to be capable of destroying static storage
		 * used by getpwnam(). We are using safe(r) function */
		pw = NULL;
		getpwnam_r(username, &pwdstruct, pwdbuf, sizeof(pwdbuf), &pw);
		if (!pw)
			goto auth_failed;
		pamret = pam_open_session(pamh, 0);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "open_session";
			goto pam_auth_failed;
		}
		pamret = pam_setcred(pamh, PAM_ESTABLISH_CRED);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "setcred";
			goto pam_auth_failed;
		}
		break; /* success, continue login process */

 pam_auth_failed:
		/* syslog, because we don't want potential attacker
		 * to know _why_ login failed */
		syslog(LOG_WARNING, "pam_%s call failed: %s (%d)", failed_msg,
					pam_strerror(pamh, pamret), pamret);
		safe_strncpy(username, "UNKNOWN", sizeof(username));
#else /* not PAM */
		pw = getpwnam(username);
		if (!pw) {
			strcpy(username, "UNKNOWN");
			goto fake_it;
		}

		if (pw->pw_passwd[0] == '!' || pw->pw_passwd[0] == '*')
			goto auth_failed;

		if (opt & LOGIN_OPT_f)
			break; /* -f USER: success without asking passwd */

		if (pw->pw_uid == 0 && !check_securetty())
			goto auth_failed;

		/* Don't check the password if password entry is empty (!) */
		if (!pw->pw_passwd[0])
			break;
 fake_it:
		/* authorization takes place here */
		if (correct_password(pw))
			break;
#endif /* ENABLE_PAM */
 auth_failed:
		opt &= ~LOGIN_OPT_f;
		bb_do_delay(FAIL_DELAY);
		/* TODO: doesn't sound like correct English phrase to me */
		puts("Login incorrect");
		if (++count == 3) {
			syslog(LOG_WARNING, "invalid password for '%s'%s",
						username, fromhost);
			return EXIT_FAILURE;
		}
		username[0] = '\0';
	} /* while (1) */

	alarm(0);
	/* We can ignore /etc/nologin if we are logging in as root,
	 * it doesn't matter whether we are run by root or not */
	if (pw->pw_uid != 0)
		die_if_nologin();

	IF_SELINUX(initselinux(username, full_tty, &user_sid));

	/* Try these, but don't complain if they fail.
	 * _f_chown is safe wrt race t=ttyname(0);...;chown(t); */
	fchown(0, pw->pw_uid, pw->pw_gid);
	fchmod(0, 0600);

	update_utmp(getpid(), USER_PROCESS, short_tty, username, run_by_root ? opt_host : NULL);

	/* We trust environment only if we run by root */
	if (ENABLE_LOGIN_SCRIPTS && run_by_root)
		run_login_script(pw, full_tty);

	change_identity(pw);
	shell = pw->pw_shell;
	if (!shell || !shell[0])
		shell = DEFAULT_SHELL;
	setup_environment(shell,
			(!(opt & LOGIN_OPT_p) * SETUP_ENV_CLEARENV) + SETUP_ENV_CHANGEENV,
			pw);

	motd();

	if (pw->pw_uid == 0)
		syslog(LOG_INFO, "root login%s", fromhost);

	/* well, a simple setexeccon() here would do the job as well,
	 * but let's play the game for now */
	IF_SELINUX(set_current_security_context(user_sid);)

	// util-linux login also does:
	// /* start new session */
	// setsid();
	// /* TIOCSCTTY: steal tty from other process group */
	// if (ioctl(0, TIOCSCTTY, 1)) error_msg...
	// BBox login used to do this (see above):
	// bb_setpgrp();
	// If this stuff is really needed, add it and explain why!

	/* Set signals to defaults */
	/* Non-ignored signals revert to SIG_DFL on exec anyway */
	/*signal(SIGALRM, SIG_DFL);*/

	/* Is this correct? This way user can ctrl-c out of /etc/profile,
	 * potentially creating security breach (tested with bash 3.0).
	 * But without this, bash 3.0 will not enable ctrl-c either.
	 * Maybe bash is buggy?
	 * Need to find out what standards say about /bin/login -
	 * should we leave SIGINT etc enabled or disabled? */
	signal(SIGINT, SIG_DFL);

	/* Exec login shell with no additional parameters */
	run_shell(shell, 1, NULL, NULL);

	/* return EXIT_FAILURE; - not reached */
}
