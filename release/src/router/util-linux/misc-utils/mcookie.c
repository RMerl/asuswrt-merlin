/* mcookie.c -- Generates random numbers for xauth
 * Created: Fri Feb  3 10:42:48 1995 by faith@cs.unc.edu
 * Revised: Fri Mar 19 07:48:01 1999 by faith@acm.org
 * Public Domain 1995, 1999 Rickard E. Faith (faith@acm.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * This program gathers some random bits of data and used the MD5
 * message-digest algorithm to generate a 128-bit hexadecimal number for
 * use with xauth(1).
 *
 * NOTE: Unless /dev/random is available, this program does not actually
 * gather 128 bits of random information, so the magic cookie generated
 * will be considerably easier to guess than one might expect.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * 1999-03-21 aeb: Added some fragments of code from Colin Plumb.
 *
 */

#include "c.h"
#include "md5.h"
#include "nls.h"
#include <fcntl.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#define BUFFERSIZE	4096

struct rngs {
	const char *path;
	int minlength, maxlength;
} rngs[] = {
	{"/dev/random",		16, 16},  /* 16 bytes = 128 bits suffice */
	{"/proc/interrupts",	 0,  0},
	{"/proc/slabinfo",	 0,  0},
	{"/proc/stat",		 0,  0},
	{"/dev/urandom",	32, 64},
};

#define RNGS (sizeof(rngs)/sizeof(struct rngs))

/* The basic function to hash a file */
static off_t hash_file(struct MD5Context *ctx, int fd)
{
	off_t count = 0;
	ssize_t r;
	unsigned char buf[BUFFERSIZE];

	while ((r = read(fd, buf, sizeof(buf))) > 0) {
		MD5Update(ctx, buf, r);
		count += r;
	}
	/* Separate files with a null byte */
	buf[0] = '\0';
	MD5Update(ctx, buf, 1);
	return count;
}

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options]\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -f, --file <file> use file as a cookie seed\n"
		" -v, --verbose     explain what is being done\n"
		" -V, --version     output version information and exit\n"
		" -h, --help        display this help and exit\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	size_t i;
	struct MD5Context ctx;
	unsigned char digest[MD5LENGTH];
	unsigned char buf[BUFFERSIZE];
	int fd;
	int c;
	pid_t pid;
	char *file = NULL;
	int verbose = 0;
	int r;
	struct timeval tv;
	struct timezone tz;

	static const struct option longopts[] = {
		{"file", required_argument, NULL, 'f'},
		{"verbose", no_argument, NULL, 'v'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c =
		getopt_long(argc, argv, "f:vVh", longopts, NULL)) != -1)
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'f':
			file = optarg;
			break;
		case 'V':
			printf(_("%s from %s\n"),
			       program_invocation_short_name,
			       PACKAGE_STRING);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	MD5Init(&ctx);
	gettimeofday(&tv, &tz);
	MD5Update(&ctx, (unsigned char *) &tv, sizeof(tv));

	pid = getppid();
	MD5Update(&ctx, (unsigned char *) &pid, sizeof(pid));
	pid = getpid();
	MD5Update(&ctx, (unsigned char *) &pid, sizeof(pid));

	if (file) {
		int count = 0;

		if (file[0] == '-' && !file[1])
			fd = STDIN_FILENO;
		else
			fd = open(file, O_RDONLY);

		if (fd < 0) {
			warn(_("Could not open %s"), file);
		} else {
			count = hash_file(&ctx, fd);
			if (verbose)
				fprintf(stderr,
					_("Got %d bytes from %s\n"), count,
					file);

			if (fd != STDIN_FILENO)
				if (close(fd))
					err(EXIT_FAILURE,
					    _("closing %s failed"), file);
		}
	}

	for (i = 0; i < RNGS; i++) {
		if ((fd = open(rngs[i].path, O_RDONLY | O_NONBLOCK)) >= 0) {
			int count = sizeof(buf);

			if (rngs[i].maxlength && count > rngs[i].maxlength)
				count = rngs[i].maxlength;
			r = read(fd, buf, count);
			if (r > 0)
				MD5Update(&ctx, buf, r);
			else
				r = 0;
			close(fd);
			if (verbose)
				fprintf(stderr,
					_("Got %d bytes from %s\n"), r,
					rngs[i].path);
			if (rngs[i].minlength && r >= rngs[i].minlength)
				break;
		} else if (verbose)
			warn(_("Could not open %s"), rngs[i].path);
	}

	MD5Final(digest, &ctx);
	for (i = 0; i < MD5LENGTH; i++)
		printf("%02x", digest[i]);
	putchar('\n');

	/*
	 * The following is important for cases like disk full,
	 * so shell scripts can bomb out properly rather than
	 * think they succeeded.
	 */
	if (fflush(stdout) < 0 || fclose(stdout) < 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
