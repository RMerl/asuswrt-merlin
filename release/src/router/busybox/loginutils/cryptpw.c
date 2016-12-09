/* vi: set sw=4 ts=4: */
/*
 * cryptpw.c - output a crypt(3)ed password to stdout.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * Cooked from passwd.c by Thomas Lundquist <thomasez@zelow.no>
 * mkpasswd compatible options added by Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config CRYPTPW
//config:	bool "cryptpw"
//config:	default y
//config:	help
//config:	  Encrypts the given password with the crypt(3) libc function
//config:	  using the given salt.
//config:
//config:config MKPASSWD
//config:	bool "mkpasswd"
//config:	default y
//config:	help
//config:	  Encrypts the given password with the crypt(3) libc function
//config:	  using the given salt. Debian has this utility under mkpasswd
//config:	  name. Busybox provides mkpasswd as an alias for cryptpw.

//applet:IF_CRYPTPW(APPLET(cryptpw, BB_DIR_USR_BIN, BB_SUID_DROP))
//                   APPLET_ODDNAME:name      main     location        suid_type     help
//applet:IF_MKPASSWD(APPLET_ODDNAME(mkpasswd, cryptpw, BB_DIR_USR_BIN, BB_SUID_DROP, cryptpw))

//kbuild:lib-$(CONFIG_CRYPTPW) += cryptpw.o
//kbuild:lib-$(CONFIG_MKPASSWD) += cryptpw.o

//usage:#define cryptpw_trivial_usage
//usage:       "[OPTIONS] [PASSWORD] [SALT]"
/* We do support -s, we just don't mention it */
//usage:#define cryptpw_full_usage "\n\n"
//usage:       "Crypt PASSWORD using crypt(3)\n"
//usage:	IF_LONG_OPTS(
//usage:     "\n	-P,--password-fd=N	Read password from fd N"
/* //usage:  "\n	-s,--stdin		Use stdin; like -P0" */
//usage:     "\n	-m,--method=TYPE	Encryption method"
//usage:     "\n	-S,--salt=SALT"
//usage:	)
//usage:	IF_NOT_LONG_OPTS(
//usage:     "\n	-P N	Read password from fd N"
/* //usage:  "\n	-s	Use stdin; like -P0" */
//usage:     "\n	-m TYPE	Encryption method TYPE"
//usage:     "\n	-S SALT"
//usage:	)

#include "libbb.h"

/* Debian has 'mkpasswd' utility, manpage says:

NAME
    mkpasswd - Overfeatured front end to crypt(3)
SYNOPSIS
    mkpasswd PASSWORD SALT
...
OPTIONS
-S, --salt=STRING
    Use the STRING as salt. It must not  contain  prefixes  such  as
    $1$.
-R, --rounds=NUMBER
    Use NUMBER rounds. This argument is ignored if the method
    choosen does not support variable rounds. For the OpenBSD Blowfish
    method this is the logarithm of the number of rounds.
-m, --method=TYPE
    Compute the password using the TYPE method. If TYPE is 'help'
    then the available methods are printed.
-P, --password-fd=NUM
    Read the password from file descriptor NUM instead of using getpass(3).
    If the file descriptor is not connected to a tty then
    no other message than the hashed password is printed on stdout.
-s, --stdin
    Like --password-fd=0.
ENVIRONMENT
    $MKPASSWD_OPTIONS
    A list of options which will be evaluated before the ones
    specified on the command line.
BUGS
    This programs suffers of a bad case of featuritis.
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Very true...

cryptpw was in bbox before this gem, so we retain it, and alias mkpasswd
to cryptpw. -a option (alias for -m) came from cryptpw.
*/

int cryptpw_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cryptpw_main(int argc UNUSED_PARAM, char **argv)
{
	char salt[MAX_PW_SALT_LEN];
	char *salt_ptr;
	char *password;
	const char *opt_m, *opt_S;
	int fd;

#if ENABLE_LONG_OPTS
	static const char mkpasswd_longopts[] ALIGN1 =
		"stdin\0"       No_argument       "s"
		"password-fd\0" Required_argument "P"
		"salt\0"        Required_argument "S"
		"method\0"      Required_argument "m"
	;
	applet_long_options = mkpasswd_longopts;
#endif
	fd = STDIN_FILENO;
	opt_m = CONFIG_FEATURE_DEFAULT_PASSWD_ALGO;
	opt_S = NULL;
	/* at most two non-option arguments; -P NUM */
	opt_complementary = "?2:P+";
	getopt32(argv, "sP:S:m:a:", &fd, &opt_S, &opt_m, &opt_m);
	argv += optind;

	/* have no idea how to handle -s... */

	if (argv[0] && !opt_S)
		opt_S = argv[1];

	salt_ptr = crypt_make_pw_salt(salt, opt_m);
	if (opt_S)
		safe_strncpy(salt_ptr, opt_S, sizeof(salt) - (sizeof("$N$")-1));

	xmove_fd(fd, STDIN_FILENO);

	password = argv[0];
	if (!password) {
		/* Only mkpasswd, and only from tty, prompts.
		 * Otherwise it is a plain read. */
		password = (ENABLE_MKPASSWD && isatty(STDIN_FILENO) && applet_name[0] == 'm')
			? bb_ask_stdin("Password: ")
			: xmalloc_fgetline(stdin)
		;
		/* may still be NULL on EOF/error */
	}

	if (password)
		puts(pw_encrypt(password, salt, 1));

	return EXIT_SUCCESS;
}
