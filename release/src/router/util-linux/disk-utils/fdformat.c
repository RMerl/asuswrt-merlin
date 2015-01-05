/*
 * fdformat.c  -  Low-level formats a floppy disk - Werner Almesberger
 */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/fd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"
#include "xalloc.h"

struct floppy_struct param;

#define SECTOR_SIZE 512

static void format_disk(int ctrl)
{
	struct format_descr descr;
	unsigned int track;

	printf(_("Formatting ... "));
	fflush(stdout);
	if (ioctl(ctrl, FDFMTBEG, NULL) < 0)
		err(EXIT_FAILURE, "ioctl: FDFMTBEG");
	for (track = 0; track < param.track; track++) {
		descr.track = track;
		descr.head = 0;
		if (ioctl(ctrl, FDFMTTRK, (long) &descr) < 0)
			err(EXIT_FAILURE, "ioctl: FDFMTTRK");

		printf("%3d\b\b\b", track);
		fflush(stdout);
		if (param.head == 2) {
			descr.head = 1;
			if (ioctl(ctrl, FDFMTTRK, (long)&descr) < 0)
				err(EXIT_FAILURE, "ioctl: FDFMTTRK");
		}
	}
	if (ioctl(ctrl, FDFMTEND, NULL) < 0)
		err(EXIT_FAILURE, "ioctl: FDFMTEND");
	printf(_("done\n"));
}

static void verify_disk(char *name)
{
	unsigned char *data;
	unsigned int cyl;
	int fd, cyl_size, count;

	cyl_size = param.sect * param.head * 512;
	data = xmalloc(cyl_size);
	printf(_("Verifying ... "));
	fflush(stdout);
	if ((fd = open(name, O_RDONLY)) < 0)
		err(EXIT_FAILURE, _("cannot open file %s"), name);
	for (cyl = 0; cyl < param.track; cyl++) {
		int read_bytes;

		printf("%3d\b\b\b", cyl);
		fflush(stdout);
		read_bytes = read(fd, data, cyl_size);
		if (read_bytes != cyl_size) {
			if (read_bytes < 0)
				perror(_("Read: "));
			fprintf(stderr,
				_("Problem reading cylinder %d,"
				  " expected %d, read %d\n"),
				cyl, cyl_size, read_bytes);
			free(data);
			exit(EXIT_FAILURE);
		}
		for (count = 0; count < cyl_size; count++)
			if (data[count] != FD_FILL_BYTE) {
				printf(_("bad data in cyl %d\n"
					 "Continuing ... "), cyl);
				fflush(stdout);
				break;
			}
	}
	free(data);
	printf(_("done\n"));
	if (close(fd) < 0)
		err(EXIT_FAILURE, "close");
}

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fprintf(out, _("Usage: %s [options] device\n"),
		program_invocation_short_name);

	fprintf(out, _("\nOptions:\n"
		       " -n, --no-verify  disable the verification after the format\n"
		       " -V, --version    output version information and exit\n"
		       " -h, --help       display this help and exit\n\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int ch;
	int ctrl;
	int verify = 1;
	struct stat st;

	static const struct option longopts[] = {
		{"no-verify", no_argument, NULL, 'n'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((ch = getopt_long(argc, argv, "nVh", longopts, NULL)) != -1)
		switch (ch) {
		case 'n':
			verify = 0;
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
			       PACKAGE_STRING);
			exit(EXIT_SUCCESS);
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage(stderr);
	if (stat(argv[0], &st) < 0)
		err(EXIT_FAILURE, _("cannot stat file %s"), argv[0]);
	if (!S_ISBLK(st.st_mode))
		/* do not test major - perhaps this was an USB floppy */
		errx(EXIT_FAILURE, _("%s: not a block device"), argv[0]);
	if (access(argv[0], W_OK) < 0)
		err(EXIT_FAILURE, _("cannot access file %s"), argv[0]);

	ctrl = open(argv[0], O_WRONLY);
	if (ctrl < 0)
		err(EXIT_FAILURE, _("cannot open file %s"), argv[0]);
	if (ioctl(ctrl, FDGETPRM, (long)&param) < 0)
		err(EXIT_FAILURE, _("Could not determine current format type"));

	printf(_("%s-sided, %d tracks, %d sec/track. Total capacity %d kB.\n"),
	       (param.head == 2) ? _("Double") : _("Single"),
	       param.track, param.sect, param.size >> 1);
	format_disk(ctrl);
	close(ctrl);

	if (verify)
		verify_disk(argv[0]);
	return EXIT_SUCCESS;
}
