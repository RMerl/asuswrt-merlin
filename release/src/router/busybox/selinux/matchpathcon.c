/* matchpathcon  -  get the default security context for the specified
 *                  path from the file contexts configuration.
 *                  based on libselinux-1.32
 * Port to busybox: KaiGai Kohei <kaigai@kaigai.gr.jp>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

static int print_matchpathcon(char *path, int noprint)
{
	char *buf;
	int rc = matchpathcon(path, 0, &buf);
	if (rc < 0) {
		bb_perror_msg("matchpathcon(%s) failed", path);
		return 1;
	}
	if (!noprint)
		printf("%s\t%s\n", path, buf);
	else
		puts(buf);

	freecon(buf);
	return 0;
}

#define OPT_NOT_PRINT   (1<<0)  /* -n */
#define OPT_NOT_TRANS   (1<<1)  /* -N */
#define OPT_FCONTEXT    (1<<2)  /* -f */
#define OPT_PREFIX      (1<<3)  /* -p */
#define OPT_VERIFY      (1<<4)  /* -V */

int matchpathcon_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int matchpathcon_main(int argc UNUSED_PARAM, char **argv)
{
	int error = 0;
	unsigned opts;
	char *fcontext, *prefix, *path;

	opt_complementary = "-1" /* at least one param reqd */
		":?:f--p:p--f"; /* mutually exclusive */
	opts = getopt32(argv, "nNf:p:V", &fcontext, &prefix);
	argv += optind;

	if (opts & OPT_NOT_TRANS) {
		set_matchpathcon_flags(MATCHPATHCON_NOTRANS);
	}
	if (opts & OPT_FCONTEXT) {
		if (matchpathcon_init(fcontext))
			bb_perror_msg_and_die("error while processing %s", fcontext);
	}
	if (opts & OPT_PREFIX) {
		if (matchpathcon_init_prefix(NULL, prefix))
			bb_perror_msg_and_die("error while processing %s", prefix);
	}

	while ((path = *argv++) != NULL) {
		security_context_t con;
		int rc;

		if (!(opts & OPT_VERIFY)) {
			error += print_matchpathcon(path, opts & OPT_NOT_PRINT);
			continue;
		}

		if (selinux_file_context_verify(path, 0)) {
			printf("%s verified\n", path);
			continue;
		}

		if (opts & OPT_NOT_TRANS)
			rc = lgetfilecon_raw(path, &con);
		else
			rc = lgetfilecon(path, &con);

		if (rc >= 0) {
			printf("%s has context %s, should be ", path, con);
			error += print_matchpathcon(path, 1);
			freecon(con);
			continue;
		}
		printf("actual context unknown: %s, should be ", strerror(errno));
		error += print_matchpathcon(path, 1);
	}
	matchpathcon_fini();
	return error;
}
