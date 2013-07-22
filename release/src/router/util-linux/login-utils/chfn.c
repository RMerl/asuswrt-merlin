/*
 *   chfn.c -- change your finger information
 *   (c) 1994 by salvatore valente <svalente@athena.mit.edu>
 *
 *   this program is free software.  you can redistribute it and
 *   modify it under the terms of the gnu general public license.
 *   there is no warranty.
 *
 *   $Author: aebr $
 *   $Revision: 1.18 $
 *   $Date: 1998/06/11 22:30:11 $
 *
 * Updated Thu Oct 12 09:19:26 1995 by faith@cs.unc.edu with security
 * patches from Zefram <A.Main@dcs.warwick.ac.uk>
 *
 * Hacked by Peter Breitenlohner, peb@mppmu.mpg.de,
 * to remove trailing empty fields.  Oct 5, 96.
 *
 *  1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 *  - added Native Language Support
 *
 */

#define _BSD_SOURCE           /* for strcasecmp() */

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

#include "islocal.h"
#include "setpwnam.h"
#include "strutils.h"
#include "nls.h"
#include "env.h"
#include "xalloc.h"
#include "c.h"

#ifdef HAVE_LIBSELINUX
#include <selinux/selinux.h>
#include <selinux/av_permissions.h>
#include "selinux_utils.h"
#endif

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

#endif

typedef unsigned char boolean;
#define false 0
#define true 1

static char *whoami;

static char buf[1024];

struct finfo {
    struct passwd *pw;
    char *username;
    char *full_name;
    char *office;
    char *office_phone;
    char *home_phone;
    char *other;
};

static boolean parse_argv (int argc, char *argv[], struct finfo *pinfo);
static void parse_passwd (struct passwd *pw, struct finfo *pinfo);
static void ask_info (struct finfo *oldfp, struct finfo *newfp);
static char *prompt (char *question, char *def_val);
static int check_gecos_string (char *msg, char *gecos);
static boolean set_changed_data (struct finfo *oldfp, struct finfo *newfp);
static int save_new_data (struct finfo *pinfo);

/* we do not accept gecos field sizes longer than MAX_FIELD_SIZE */
#define MAX_FIELD_SIZE		256

static void __attribute__((__noreturn__)) usage(FILE *fp)
{
    fprintf (fp, _("Usage: %s [ -f full-name ] [ -o office ] "), whoami);
    fprintf (fp, _("[ -p office-phone ]\n	[ -h home-phone ] "));
    fprintf (fp, _("[ --help ] [ --version ]\n"));

    exit(fp == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main (int argc, char **argv) {
    char *cp;
    uid_t uid;
    struct finfo oldf, newf;
    boolean interactive;
    int status;

    sanitize_env();
    setlocale(LC_ALL, "");	/* both for messages and for iscntrl() below */
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    /* whoami is the program name for error messages */
    whoami = argv[0];
    if (! whoami) whoami = "chfn";
    for (cp = whoami; *cp; cp++)
	if (*cp == '/') whoami = cp + 1;

    /*
     *	"oldf" contains the users original finger information.
     *	"newf" contains the changed finger information, and contains NULL
     *	       in fields that haven't been changed.
     *	in the end, "newf" is folded into "oldf".
     *	the reason the new finger information is not put _immediately_ into
     *	"oldf" is that on the command line, new finger information can
     *	be specified before we know what user the information is being
     *	specified for.
     */
    uid = getuid ();
    memset(&oldf, 0, sizeof(oldf));
    memset(&newf, 0, sizeof (newf));

    interactive = parse_argv (argc, argv, &newf);
    if (! newf.username) {
	parse_passwd (getpwuid (uid), &oldf);
	if (! oldf.username)
	    errx(EXIT_FAILURE, _("%s: you (user %d) don't exist."), whoami, uid);
    }
    else {
	parse_passwd (getpwnam (newf.username), &oldf);
	if (! oldf.username)
	    errx(EXIT_FAILURE, _("%s: user \"%s\" does not exist."),
			    whoami, newf.username);
    }

    if (!(is_local(oldf.username)))
       errx(EXIT_FAILURE, _("can only change local entries"));

#ifdef HAVE_LIBSELINUX
    if (is_selinux_enabled() > 0) {
      if(uid == 0) {
	if (checkAccess(oldf.username,PASSWD__CHFN)!=0) {
	  security_context_t user_context;
	  if (getprevcon(&user_context) < 0)
	    user_context = NULL;

	  errx(EXIT_FAILURE, _("%s: %s is not authorized to change "
			"the finger info of %s"),
			 whoami, user_context ? : _("Unknown user context"),
			 oldf.username);
	}
      }
      if (setupDefaultContext("/etc/passwd"))
        errx(EXIT_FAILURE, _("can't set default context for /etc/passwd"));
    }
#endif

    /* Reality check */
    if (uid != 0 && uid != oldf.pw->pw_uid) {
	errno = EACCES;
	err(EXIT_FAILURE, NULL);
    }

    printf (_("Changing finger information for %s.\n"), oldf.username);

#ifdef REQUIRE_PASSWORD
#ifdef HAVE_SECURITY_PAM_MISC_H
    if(uid != 0) {
	pam_handle_t *pamh = NULL;
	struct pam_conv conv = { misc_conv, NULL };
	int retcode;

	retcode = pam_start("chfn", oldf.username, &conv, &pamh);
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
# else /* HAVE_SECURITY_PAM_MISC_H */
    /* require password, unless root */
    if(uid != 0 && oldf.pw->pw_passwd && oldf.pw->pw_passwd[0]) {
	char *pwdstr = getpass(_("Password: "));
	if(strncmp(oldf.pw->pw_passwd,
		   crypt(pwdstr, oldf.pw->pw_passwd), 13)) {
	    puts(_("Incorrect password."));
	    exit(EXIT_FAILURE);
	}
    }
# endif /* HAVE_SECURITY_PAM_MISC_H */
#endif /* REQUIRE_PASSWORD */


    if (interactive) ask_info (&oldf, &newf);

    if (! set_changed_data (&oldf, &newf)) {
	printf (_("Finger information not changed.\n"));
	return EXIT_SUCCESS;
    }
    status = save_new_data (&oldf);
    return status;
}

/*
 *  parse_argv () --
 *	parse the command line arguments.
 *	returns true if no information beyond the username was given.
 */
static boolean parse_argv (argc, argv, pinfo)
    int argc;
    char *argv[];
    struct finfo *pinfo;
{
    int index, c, status;
    boolean info_given;

    static struct option long_options[] = {
	{ "full-name",	  required_argument, 0, 'f' },
	{ "office",	  required_argument, 0, 'o' },
	{ "office-phone", required_argument, 0, 'p' },
	{ "home-phone",   required_argument, 0, 'h' },
	{ "help",	  no_argument,       0, 'u' },
	{ "version",	  no_argument,	     0, 'v' },
	{ NULL,		  no_argument,	     0, '0' },
    };

    optind = 0;
    info_given = false;
    while (true) {
	c = getopt_long (argc, argv, "f:r:p:h:o:uv", long_options, &index);
	if (c == -1) break;
	/* version?  output version and exit. */
	if (c == 'v') {
	    printf ("%s\n", PACKAGE_STRING);
	    exit (EXIT_SUCCESS);
	}
	if (c == 'u')
	    usage (stdout);
	/* all other options must have an argument. */
	if (! optarg)
	    usage (stderr);
	/* ok, we were given an argument */
	info_given = true;
	status = 0;

	xstrncpy (buf, whoami, sizeof(buf)-128); 
	strcat (buf, ": ");

	/* now store the argument */
	switch (c) {
	case 'f':
	    pinfo->full_name = optarg;
	    strcat (buf, "full name");
	    status = check_gecos_string (buf, optarg);
	    break;
	case 'o':
	    pinfo->office = optarg;
	    strcat (buf, "office");
	    status = check_gecos_string (buf, optarg);
	    break;
	case 'p':
	    pinfo->office_phone = optarg;
	    strcat (buf, "office phone");
	    status = check_gecos_string (buf, optarg);
	    break;
	case 'h':
	    pinfo->home_phone = optarg;
	    strcat (buf, "home phone");
	    status = check_gecos_string (buf, optarg);
	    break;
	default:
	    usage (stderr);
	}
	if (status < 0) exit (status);
    }
    /* done parsing arguments.	check for a username. */
    if (optind < argc) {
	if (optind + 1 < argc)
	    usage (stderr);
	pinfo->username = argv[optind];
    }
    return (! info_given);
}

/*
 *  parse_passwd () --
 *	take a struct password and fill in the fields of the
 *	struct finfo.
 */
static void parse_passwd (pw, pinfo)
    struct passwd *pw;
    struct finfo *pinfo;
{
    char *gecos;
    char *cp;

    if (pw) {
	pinfo->pw = pw;
	pinfo->username = pw->pw_name;
	/* use pw_gecos - we take a copy since PAM destroys the original */
	gecos = xstrdup(pw->pw_gecos);
	cp = (gecos ? gecos : "");
	pinfo->full_name = cp;
	cp = strchr (cp, ',');
	if (cp) { *cp = 0, cp++; } else return;
	pinfo->office = cp;
	cp = strchr (cp, ',');
	if (cp) { *cp = 0, cp++; } else return;
	pinfo->office_phone = cp;
	cp = strchr (cp, ',');
	if (cp) { *cp = 0, cp++; } else return;
	pinfo->home_phone = cp;
	/*  extra fields contain site-specific information, and
	 *  can not be changed by this version of chfn.	 */
	cp = strchr (cp, ',');
	if (cp) { *cp = 0, cp++; } else return;
	pinfo->other = cp;
    }
}

/*
 *  ask_info () --
 *	prompt the user for the finger information and store it.
 */
static void ask_info (oldfp, newfp)
    struct finfo *oldfp;
    struct finfo *newfp;
{
    newfp->full_name = prompt (_("Name"), oldfp->full_name);
    newfp->office = prompt (_("Office"), oldfp->office);
    newfp->office_phone = prompt (_("Office Phone"), oldfp->office_phone);
    newfp->home_phone = prompt (_("Home Phone"), oldfp->home_phone);
    printf ("\n");
}

/*
 *  prompt () --
 *	ask the user for a given field and check that the string is legal.
 */
static char *prompt (question, def_val)
    char *question;
    char *def_val;
{
    static char *blank = "none";
    int len;
    char *ans, *cp;
  
    while (true) {
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
	if (! strcasecmp (ans, blank)) return "";
	if (check_gecos_string (NULL, ans) >= 0) break;
    }
    cp = (char *) xmalloc (len + 1);
    strcpy (cp, ans);
    return cp;
}

/*
 *  check_gecos_string () --
 *	check that the given gecos string is legal.  if it's not legal,
 *	output "msg" followed by a description of the problem, and
 *	return (-1).
 */
static int check_gecos_string (msg, gecos)
    char *msg;
    char *gecos;
{
    unsigned int i, c;

    if (strlen(gecos) > MAX_FIELD_SIZE) {
	if (msg != NULL)
	    printf("%s: ", msg);
	printf(_("field is too long.\n"));
	return -1;
    }

    for (i = 0; i < strlen (gecos); i++) {
	c = gecos[i];
	if (c == ',' || c == ':' || c == '=' || c == '"' || c == '\n') {
	    if (msg) printf ("%s: ", msg);
	    printf (_("'%c' is not allowed.\n"), c);
	    return -1;
	}
	if (iscntrl (c)) {
	    if (msg) printf ("%s: ", msg);
	    printf (_("Control characters are not allowed.\n"));
	    return -1;
	}
    }
    return 0;
}

/*
 *  set_changed_data () --
 *	incorporate the new data into the old finger info.
 */
static boolean set_changed_data (oldfp, newfp)
    struct finfo *oldfp;
    struct finfo *newfp;
{
    boolean changed = false;

    if (newfp->full_name) {
	oldfp->full_name = newfp->full_name; changed = true; }
    if (newfp->office) {
	oldfp->office = newfp->office; changed = true; }
    if (newfp->office_phone) {
	oldfp->office_phone = newfp->office_phone; changed = true; }
    if (newfp->home_phone) {
	oldfp->home_phone = newfp->home_phone; changed = true; }

    return changed;
}

/*
 *  save_new_data () --
 *	save the given finger info in /etc/passwd.
 *	return zero on success.
 */
static int save_new_data (pinfo)
     struct finfo *pinfo;
{
    char *gecos;
    int len;

    /* null fields will confuse printf(). */
    if (! pinfo->full_name) pinfo->full_name = "";
    if (! pinfo->office) pinfo->office = "";
    if (! pinfo->office_phone) pinfo->office_phone = "";
    if (! pinfo->home_phone) pinfo->home_phone = "";
    if (! pinfo->other) pinfo->other = "";

    /* create the new gecos string */
    len = (strlen (pinfo->full_name) + strlen (pinfo->office) +
	   strlen (pinfo->office_phone) + strlen (pinfo->home_phone) +
	   strlen (pinfo->other) + 4);
    gecos = (char *) xmalloc (len + 1);
    sprintf (gecos, "%s,%s,%s,%s,%s", pinfo->full_name, pinfo->office,
	     pinfo->office_phone, pinfo->home_phone, pinfo->other);

    /* remove trailing empty fields (but not subfields of pinfo->other) */
    if (! pinfo->other[0] ) {
       while (len > 0 && gecos[len-1] == ',') len--;
       gecos[len] = 0;
    }

    /* write the new struct passwd to the passwd file. */
    pinfo->pw->pw_gecos = gecos;
    if (setpwnam (pinfo->pw) < 0) {
	perror ("setpwnam");
	printf( _("Finger information *NOT* changed.  Try again later.\n" ));
	return -1;
    }
    printf (_("Finger information changed.\n"));
    return 0;
}
