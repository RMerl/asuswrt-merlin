#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "partx.h"

int
main(int argc, char **argv)
{
	int fd;

	if (argc != 5) {
		fprintf(stderr,
			"usage: %s diskdevice partitionnr start length\n",
			argv[0]);
		exit(1);
	}
	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		perror(argv[1]);
		exit(1);
	}

	if (partx_add_partition(fd, atoi(argv[2]),
				atoll(argv[3]),
				atoll(argv[4]))) {
		perror("BLKPG");
		exit(1);
	}

	return 0;
}
