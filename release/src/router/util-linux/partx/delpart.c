#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "partx.h"

int
main(int argc, char **argv){
	int fd;

	if (argc != 3) {
		fprintf(stderr,
			"usage: %s diskdevice partitionnr\n",
			argv[0]);
		exit(1);
	}
	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		perror(argv[1]);
		exit(1);
	}

	if (partx_del_partition(fd, atoi(argv[2])) == -1) {
		perror("BLKPG");
		exit(1);
	}
	return 0;
}
