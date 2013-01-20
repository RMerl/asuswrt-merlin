/*
 * partinfo.c
 *
 * Originally written by Alain Knaff, <alknaff@innet.lu>.
 *
 * Cleaned up by Theodore Ts'o, <tytso@mit.edu>.
 *
 */

#include <sys/types.h>
#include <fcntl.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <stdio.h>
#include <linux/hdreg.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "nls-enable.h"

#if defined(__linux__) && defined(_IO) && !defined(BLKGETSIZE)
#define BLKGETSIZE _IO(0x12,96)	/* return device size */
#endif

int main(int argc, char **argv)
{
	struct hd_geometry loc;
	int fd, i;
	unsigned long size;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif
	if (argc == 1) {
		fprintf(stderr, _("Usage:  %s device...\n\nPrints out the "
			"partition information for each given device.\n"
			"For example: %s /dev/hda\n\n"), argv[0], argv[0]);
		exit(1);
	}

	for (i=1; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);

		if (fd < 0) {
			fprintf(stderr, _("Cannot open %s: %s"),
				argv[i], strerror(errno));
			continue;
		}

		if (ioctl(fd, HDIO_GETGEO, &loc) < 0) {
			fprintf(stderr, _("Cannot get geometry of %s: %s"),
				argv[i], strerror(errno));
			close(fd);
			continue;
		}


		if (ioctl(fd, BLKGETSIZE, &size) < 0) {
			fprintf(stderr, _("Cannot get size of %s: %s"),
				argv[i], strerror(errno));
			close(fd);
			continue;
		}

		printf(_("%s: h=%3d s=%3d c=%4d   start=%8d size=%8lu end=%8d\n"),
		       argv[i],
		       loc.heads, (int)loc.sectors, loc.cylinders,
		       (int)loc.start, size, (int) loc.start + size -1);
		close(fd);
	}
	exit(0);
}
