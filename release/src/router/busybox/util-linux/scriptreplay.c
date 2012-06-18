/* vi: set sw=4 ts=4: */
/*
 * scriptreplay - play back typescripts, using timing information
 *
 * pascal.bellard@ads-lu.com
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 *
 */
#include "libbb.h"

int scriptreplay_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int scriptreplay_main(int argc UNUSED_PARAM, char **argv)
{
	const char *script = "typescript";
	double delay, factor = 1000000.0;
	int fd;
	unsigned long count;
	FILE *tfp;

	if (!argv[1])
		bb_show_usage();

	if (argv[2]) {
		script = argv[2];
		if (argv[3])
			factor /= atof(argv[3]);
	}

	tfp = xfopen_for_read(argv[1]);
	fd = xopen(script, O_RDONLY);
	while (fscanf(tfp, "%lf %lu\n", &delay, &count) == 2) {
		usleep(delay * factor);
		bb_copyfd_exact_size(fd, STDOUT_FILENO, count);
	}
	if (ENABLE_FEATURE_CLEAN_UP) {
		close(fd);
		fclose(tfp);
	}
	return EXIT_SUCCESS;
}
