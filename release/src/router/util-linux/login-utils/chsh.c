/*
 *   chsh.c -- change your login shell
 *   (c) 1994 by salvatore valente <svalente@athena.mit.edu>
 *
 *   this program is free software.  you can redistribute it and
 *   modify it under the terms of the gnu general public license.
 *   there is no warranty.
 *
 *   $Author: aebr $
 *   $Revision: 1.19 $
 *   $Date: 1998/06/11 22:30:14 $
 *
 * Updated Thu Oct 12 09:33:15 1995 by faith@cs.unc.edu with security
 *   patches from Zefram <A.Main@dcs.warwick.ac.uk>
 *
 * Updated Mon Jul  1 18:46:22 1996 by janl@math.uio.no with security
 *   suggestion from Zefram.  Disallowing users with shells not in /etc/shells
 *   from changing their shell.
 *
 *   1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 *   - added Native Language Support
 *
 *
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include "c.h"
#include "islocal.h"
#include "setpwnam.h"
#include "nls.h"
#include "env.h"
#include "pathnames.h"
#include "xalloc.h"

#if defined(REQUIRE_PASSWORD) && defined(HAVE_SECURITY_PAM_MISC_H)
#include <security/pam_appl.h>
#include <security/pam_misc.h>

#define PAM_FAIL_CHECK(_ph, _rc) \
    do { \
	if ((_rc) != PAM_SUCCESS) { \
	    fprintf(stderr, "\n%s\n", pam_strerror((_ph), (_rc))); \
	    pam_end((_ph), (_rc)); \
	    exit(EXIT_FAILURE); \
	} \
    } while(0)

#endif /* PAM */

#ifdef HAVE_LIBSELINUX
#include <selinux/selinux.h>
#include <selinux/av_permissions.h>
#include "selinux_utils.h"
#endif

typedef unsigned char boolean;
#define false 0
#define true 1

/* Only root is allowed to assign a luser a non-listed shell, by default */
#define ONLY_LISTED_SHELLS 1

static char *whoami;

struct sinfo {
    char *username;
    char *shell;
};

static void parse_argv (int argc, char *argv[], struct sinfo *pinfo);
static char *prompt (char *question, char *def_val);
static int check_shell (char *shell);
static boolean get_shell_list (char *shell);

static void __attribute__((__noreturn__)) usage (FILE *fp)
{
    fprintf (fp,
	     _("Usage: %1$s [-s shell] [username]\n"
	       "   or: %1$s (--list-shells | --help | --version)\n"),
	     whoami);

    exit(fp == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main (int argc, char *argv[]) {
    char *cp, *shell, *oldshell;
    uid_t uid;
    struct sinfo info;
    struct passwd *pw;

    sanitize_env();
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    /* whoami is the program name for error messages */
    whoami = argv[0];
    if (! whoami) whoami = "chsh";
    for (cp = whoami; *cp; cp++)
	if (*cp == '/') whoami = cp + 1;

    uid = getuid ();
    memset(&info, 0, sizeof(info));

    parse_argv (argc, argv, &info);
    pw = NULL;
    if (! info.username) {
	pw = getpwuid (uid);
	if (! pw)
	    errx(EXIT_FAILURE, _("%s: you (user %d) don't exist."), whoami, uid);
    }
    else {
	pw = getpwnam (info.username);
	if (! pw)
	    errx(EXIT_FAILURE, _("%s: user \"%s\" does not exist."),
			    whoami, info.username);
    }

    if (!(is_local(pw->pw_name)))
       errx(EXIT_FAILURE, _("%s: can only change local entries."), whoami);

#ifdef HAVE_LIBSELINUX
    if (is_selinux_enabled() > 0) {
      if(uid == 0) {
	if (checkAccess(pw->pw_name,PASSWD__CHSH)!=0) {
	  security_context_t user_context;
	  if (getprevcon(&user_context) < 0)
	    user_context = (security_context_t) NULL;

	  errx(EXIT_FAILURE, _("%s: %s is not authorized to change the shell of %s"),
		  whoami, user_context ? : _("Unknown user context"),
		  pw->pw_name);
	}
      }
      if (setupDefaultContext("/etc/passwd") != 0)
	errx(EXIT_FAILURE, _("%s: can't set default context for /etc/passwd"),
		whoami);
    }
#endif

    oldshell = pw->pw_shell;
    if (oldshell == NULL || *oldshell == '\0')
	    oldshell = _PATH_BSHELL;			/* default */

    /* reality check */
    if (uid != 0 && uid != pw->pw_uid) {
	errno = EACCES;
	err(EXIT_FAILURE, _("%s: running UID doesn't match UID of user we're "
			 "altering, shell change denied"), whoami);
    }
    if (uid != 0 && !get_shell_list(oldshell)) {
	errno = EACCES;
	err(EXIT_FAILURE, _("%s: your shell is not in /etc/shells, "
			    "shell change denied"), whoami);
    }

    shell = info.shell;

    printf( _("Changing shell for %s.\n"), pw->pw_name );

#ifdef REQUIRE_PASSWORD
#ifdef HAVE_SECURITY_PAM_MISC_H
    if (uid != 0) {
	pam_handle_t *pamh = NULL;
	struct pam_conv conv = { misc_conv, NULL };
	int retcode;

	retcode = pam_start("chsh", pw->pw_name, &conv, &pamh);
	if(retcode != PAM_SUCCESS)
	    errx(EXIT_FAILURE, _("%s: PAM failure, aborting: %s"),
		    whoami, pam_strerror(pamh, retcode));

	retcode = pam_authenticate(pamh, 0);
	PAM_FAIL_CHECK(pamh, retcode);

	retcode = pam_acct_mgmt(pamh, 0);
	if (retcode == PAM_NEW_AUTHTOK_REQD)
	    retcode = pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
	PAM_FAIL_CHECK(pamh, retcode);

	retcode = pam_setcred(pamh, 0);
	PAM_FAIL_CHECK(pamh, retcode);

	pam_end(pamh, 0);
	/* no need to establish a session; this isn't a session-oriented
	 * activity... */
    }
#else /* HAVE_SECURITY_PAM_MISC_H */
    /* require password, unless root */
    if(uid != 0 && pw->pw_passwd && pw->pw_passwd[0]) {
	char *pwdstr = getpass(_("Password: "));
	if(strncmp(pw->pw_passwd,
		   crypt(pwdstr, pw->pw_passwd), 13)) {
	    puts(_("Incorrect password."));
	    return EXIT_FAILURE;
	}
    }
#endif /* HAVE_SECURITY_PAM_MISC_H */
#endif /* REQUIRE_PASSWORD */

    if (! shell) {
	shell = prompt (_("New shell"), oldshell);
	if (! shell)
	    return EXIT_SUCCESS;
    }

    if (check_shell (shell) < 0)
        return EXIT_FAILURE;

    if (! strcmp (pw->pw_shell, shell)) {
	printf (_("Shell not changed.\n"));
	return EXIT_SUCCESS;
    }
    pw->pw_shell = shell;
    if (setpwnam (pw) < 0) {
	warn(_("setpwnam failed"));
	printf( _("Shell *NOT* changed.  Try again later.\n") );
	return EXIT_FAILURE;
    }
    printf (_("Shell changed.\n"));
    return EXIT_SUCCESS;
}

/*
 *  parse_argv () --
 *	parse the command line arguments, and fill in "pinfo" with any
 *	information from the command line.
 */
static void
parse_argv (int argc, char *argv[], struct sinfo *pinfo) {
    int index, c;

    static struct option long_options[] = {
	{ "shell",	 required_argument, 0, 's' },
	{ "list-shells", no_argument,	    0, 'l' },
	{ "help",	 no_argument,	    0, 'u' },
	{ "version",	 no_argument,	    0, 'v' },
	{ NULL,		 no_argument,	    0, '0' },
    };

    optind = c = 0;
    while (c != EOF) {
	c = getopt_long (argc, argv, "s:luv", long_options, &index);
	switch (c) {
	case -1:
	    break;
	case 'v':
	    printf ("%s\n", PACKAGE_STRING);
	    exit (EXIT_SUCCESS);
	case 'u':
	    usage (stdout);
	case 'l':
	    get_shell_list (NULL);
	    exit (EXIT_SUCCESS);
	case 's':
	    if (! optarg)
		usage (stderr);
	    pinfo->shell = optarg;
	    break;
	default:
	    usage (stderr);
	}
    }
    /* done parsing arguments.	check for a username. */
    if (optind < argc) {
	if (optind + 1 < argc)
	    usage (stderr);
	pinfo->username = argv[optind];
    }
}


/*
 *  prompt () --
 *	ask the user for a given field and return it.
 */
static char *
prompt (char *question, char *def_val) {
    int len;
    char *ans, *cp;
    char buf[BUFSIZ];

    if (! def_val) def_val = "";
    printf("%s [%s]: ", question, def_val);
    *buf = 0;
    if (fgets (buf, sizeof (buf), stdin) == NULL) {
	printf (_("\nAborted.\n"));
	exit (EXIT_FAILURE);
    }
    /* remove the newline at the end of buf. */
    ans = buf;
    while (isspace (*ans)) ans++;
    len = strlen (ans);
    while (len > 0 && isspace (ans[len-1])) len--;
    if (len <= 0) return NULL;
    ans[len] = 0;
    cp = (char *) xmalloc (len + 1);
    strcpy (cp, ans);
    return cp;
}

/*
 *  check_shell () -- if the shell is completely invalid, print
 *	an error and return (-1).
 *	if the shell is a bad idea, print a warning.
 */
static int
check_shell (char *shell) {
    unsigned int i, c;

    if (!shell)
	return -1;

    if (*shell != '/') {
	printf (_("%s: shell must be a full path name.\n"), whoami);
	return -1;
    }
    if (access (shell, F_OK) < 0) {
	printf (_("%s: \"%s\" does not exist.\n"), whoami, shell);
	return -1;
    }
    if (access (shell, X_OK) < 0) {
	printf (_("%s: \"%s\" is not executable.\n"), whoami, shell);
	return -1;
    }
    /* keep /etc/passwd clean. */
    for (i = 0; i < strlen (shell); i++) {
	c = shell[i];
	if (c == ',' || c == ':' || c == '=' || c == '"' || c == '\n') {
	    printf (_("%s: '%c' is not allowed.\n"), whoami, c);
	    return -1;
	}
	if (iscntrl (c)) {
	    printf (_("%s: Control characters are not allowed.\n"), whoami);
	    return -1;
	}
    }
#ifdef ONLY_LISTED_SHELLS
    if (! get_shell_list (shell)) {
       if (!getuid())
	  printf (_("Warning: \"%s\" is not listed in /etc/shells.\n"), shell);
       else {
	  printf (_("%s: \"%s\" is not listed in /etc/shells.\n"),
		  whoami, shell);
	  printf( _("%s: Use -l option to see list.\n"), whoami );
	  exit(EXIT_FAILURE);
       }
    }
#else
    if (! get_shell_list (shell)) {
       printf (_("Warning: \"%s\" is not listed in /etc/shells.\n"), shell);
       printf( _("Use %s -l to see list.\n"), whoami );
    }
#endif
    return 0;
}

/*
 *  get_shell_list () -- if the given shell appears in /etc/shells,
 *	return true.  if not, return false.
 *	if the given shell is NULL, /etc/shells is outputted to stdout.
 */
static boolean
get_shell_list (char *shell_name) {
    FILE *fp;
    boolean found;
    int len;
    char buf[PATH_MAX];

    found = false;
    fp = fopen ("/etc/shells", "r");
    if (! fp) {
	if (! shell_name) printf (_("No known shells.\n"));
	return true;
    }
    while (fgets (buf, sizeof (buf), fp) != NULL) {
	/* ignore comments */
	if (*buf == '#') continue;
	len = strlen (buf);
	/* strip the ending newline */
	if (buf[len - 1] == '\n') buf[len - 1] = 0;
	/* ignore lines that are too damn long */
	else continue;
	/* check or output the shell */
	if (shell_name) {
	    if (! strcmp (shell_name, buf)) {
		found = true;
		break;
	    }
	}
	else printf ("%s\n", buf);
    }
    fclose (fp);
    return found;
}
