/* vi: set sw=4 ts=4: */
/*
 * cryptpw.c - output a crypt(3)ed password to stdout.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Cooked from passwd.c by Thomas Lundquist <thomasez@zelow.no>
 * mkpasswd compatible options added by Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

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
	/* $N$ + sha_salt_16_bytes + NUL */
	char salt[3 + 16 + 1];
	char *salt_ptr;
	const char *opt_m, *opt_S;
	int len;
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
	opt_m = "d";
	opt_S = NULL;
	/* at most two non-option arguments; -P NUM */
	opt_complementary = "?2:P+";
	getopt32(argv, "sP:S:m:a:", &fd, &opt_S, &opt_m, &opt_m);
	argv += optind;

	/* have no idea how to handle -s... */

	if (argv[0] && !opt_S)
		opt_S = argv[1];

	len = 2/2;
	salt_ptr = salt;
	if (opt_m[0] != 'd') { /* not des */
		len = 8/2; /* so far assuming md5 */
		*salt_ptr++ = '$';
		*salt_ptr++ = '1';
		*salt_ptr++ = '$';
#if !ENABLE_USE_BB_CRYPT || ENABLE_USE_BB_CRYPT_SHA
		if (opt_m[0] == 's') { /* sha */
			salt[1] = '5' + (strcmp(opt_m, "sha512") == 0);
			len = 16/2;
		}
#endif
	}
	if (opt_S)
		safe_strncpy(salt_ptr, opt_S, sizeof(salt) - 3);
	else
		crypt_make_salt(salt_ptr, len, 0);

	xmove_fd(fd, STDIN_FILENO);

	puts(pw_encrypt(
		argv[0]	? argv[0] : (
			/* Only mkpasswd, and only from tty, prompts.
			 * Otherwise it is a plain read. */
			(isatty(STDIN_FILENO) && applet_name[0] == 'm')
			? bb_ask_stdin("Password: ")
			: xmalloc_fgetline(stdin)
		),
		salt, 1));

	return EXIT_SUCCESS;
}
