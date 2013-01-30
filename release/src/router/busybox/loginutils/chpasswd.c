/* vi: set sw=4 ts=4: */
/*
 * chpasswd.c
 *
 * Written for SLIND (from passwd.c) by Alexander Shishkin <virtuoso@slind.org>
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

#if ENABLE_LONG_OPTS
static const char chpasswd_longopts[] ALIGN1 =
	"encrypted\0" No_argument "e"
	"md5\0"       No_argument "m"
	;
#endif

#define OPT_ENC		1
#define OPT_MD5		2

int chpasswd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chpasswd_main(int argc UNUSED_PARAM, char **argv)
{
	char *name, *pass;
	char salt[sizeof("$N$XXXXXXXX")];
	int opt, rc;
	int rnd = rnd; /* we *want* it to be non-initialized! */

	if (getuid())
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

	opt_complementary = "m--e:e--m";
	IF_LONG_OPTS(applet_long_options = chpasswd_longopts;)
	opt = getopt32(argv, "em");

	while ((name = xmalloc_fgetline(stdin)) != NULL) {
		pass = strchr(name, ':');
		if (!pass)
			bb_error_msg_and_die("missing new password");
		*pass++ = '\0';

		xuname2uid(name); /* dies if there is no such user */

		if (!(opt & OPT_ENC)) {
			rnd = crypt_make_salt(salt, 1, rnd);
			if (opt & OPT_MD5) {
				strcpy(salt, "$1$");
				rnd = crypt_make_salt(salt + 3, 4, rnd);
			}
			pass = pw_encrypt(pass, salt, 0);
		}

		/* This is rather complex: if user is not found in /etc/shadow,
		 * we try to find & change his passwd in /etc/passwd */
#if ENABLE_FEATURE_SHADOWPASSWDS
		rc = update_passwd(bb_path_shadow_file, name, pass, NULL);
		if (rc == 0) /* no lines updated, no errors detected */
#endif
			rc = update_passwd(bb_path_passwd_file, name, pass, NULL);
		/* LOGMODE_BOTH logs to syslog also */
		logmode = LOGMODE_BOTH;
		if (rc < 0)
			bb_error_msg_and_die("an error occurred updating password for %s", name);
		if (rc)
			bb_info_msg("Password for '%s' changed", name);
		logmode = LOGMODE_STDIO;
		free(name);
		if (!(opt & OPT_ENC))
			free(pass);
	}
	return EXIT_SUCCESS;
}
