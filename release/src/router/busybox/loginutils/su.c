/* vi: set sw=4 ts=4: */
/*
 * Mini su implementation for busybox
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include <syslog.h>

//usage:#define su_trivial_usage
//usage:       "[OPTIONS] [-] [USER]"
//usage:#define su_full_usage "\n\n"
//usage:       "Run shell under USER (by default, root)\n"
//usage:     "\n	-,-l	Clear environment, run shell as login shell"
//usage:     "\n	-p,-m	Do not set new $HOME, $SHELL, $USER, $LOGNAME"
//usage:     "\n	-c CMD	Command to pass to 'sh -c'"
//usage:     "\n	-s SH	Shell to use instead of user's default"

#if ENABLE_FEATURE_SU_CHECKS_SHELLS
/* Return 1 if SHELL is a restricted shell (one not returned by
 * getusershell), else 0, meaning it is a standard shell.  */
static int restricted_shell(const char *shell)
{
	char *line;
	int result = 1;

	/*setusershell(); - getusershell does it itself*/
	while ((line = getusershell()) != NULL) {
		if (/* *line != '#' && */ strcmp(line, shell) == 0) {
			result = 0;
			break;
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		endusershell();
	return result;
}
#endif

#define SU_OPT_mp (3)
#define SU_OPT_l  (4)

int su_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int su_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned flags;
	char *opt_shell = NULL;
	char *opt_command = NULL;
	const char *opt_username = "root";
	struct passwd *pw;
	uid_t cur_uid = getuid();
	const char *tty;
#if ENABLE_FEATURE_UTMP
	char user_buf[64];
#endif
	const char *old_user;

	flags = getopt32(argv, "mplc:s:", &opt_command, &opt_shell);
	//argc -= optind;
	argv += optind;

	if (argv[0] && LONE_DASH(argv[0])) {
		flags |= SU_OPT_l;
		argv++;
	}

	/* get user if specified */
	if (argv[0]) {
		opt_username = argv[0];
		argv++;
	}

	if (ENABLE_FEATURE_SU_SYSLOG) {
		/* The utmp entry (via getlogin) is probably the best way to
		 * identify the user, especially if someone su's from a su-shell.
		 * But getlogin can fail -- usually due to lack of utmp entry.
		 * in this case resort to getpwuid.  */
#if ENABLE_FEATURE_UTMP
		old_user = user_buf;
		if (getlogin_r(user_buf, sizeof(user_buf)) != 0)
#endif
		{
			pw = getpwuid(cur_uid);
			old_user = pw ? xstrdup(pw->pw_name) : "";
		}
		tty = xmalloc_ttyname(2);
		if (!tty) {
			tty = "none";
		}
		openlog(applet_name, 0, LOG_AUTH);
	}

	pw = xgetpwnam(opt_username);

	if (cur_uid == 0 || correct_password(pw)) {
		if (ENABLE_FEATURE_SU_SYSLOG)
			syslog(LOG_NOTICE, "%c %s %s:%s",
				'+', tty, old_user, opt_username);
	} else {
		if (ENABLE_FEATURE_SU_SYSLOG)
			syslog(LOG_NOTICE, "%c %s %s:%s",
				'-', tty, old_user, opt_username);
		bb_error_msg_and_die("incorrect password");
	}

	if (ENABLE_FEATURE_CLEAN_UP && ENABLE_FEATURE_SU_SYSLOG) {
		closelog();
	}

	if (!opt_shell && (flags & SU_OPT_mp)) {
		/* -s SHELL is not given, but "preserve env" opt is */
		opt_shell = getenv("SHELL");
	}

#if ENABLE_FEATURE_SU_CHECKS_SHELLS
	if (opt_shell && cur_uid != 0 && pw->pw_shell && restricted_shell(pw->pw_shell)) {
		/* The user being su'd to has a nonstandard shell, and so is
		 * probably a uucp account or has restricted access.  Don't
		 * compromise the account by allowing access with a standard
		 * shell.  */
		bb_error_msg("using restricted shell");
		opt_shell = NULL; /* ignore -s PROG */
	}
	/* else: user can run whatever he wants via "su -s PROG USER".
	 * This is safe since PROG is run under user's uid/gid. */
#endif
	if (!opt_shell)
		opt_shell = pw->pw_shell;

	change_identity(pw);
	setup_environment(opt_shell,
			((flags & SU_OPT_l) / SU_OPT_l * SETUP_ENV_CLEARENV)
			+ (!(flags & SU_OPT_mp) * SETUP_ENV_CHANGEENV),
			pw);
	IF_SELINUX(set_current_security_context(NULL);)

	/* Never returns */
	run_shell(opt_shell, flags & SU_OPT_l, opt_command, (const char**)argv);

	/* return EXIT_FAILURE; - not reached */
}
