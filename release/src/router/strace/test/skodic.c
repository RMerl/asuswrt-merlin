/* This demonstrates races: kernel may actually open other file then
 * you read at strace output. Create /tmp/delme with 10K of zeros and
 * 666 mode, then run this under strace. If you see open successfull
 * open of /etc/shadow, you know you've seen a race.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	/* XXX: x86 specific stuff? */
	char *c = (char*) 0x94000000;
	int fd;

	open("/tmp/delme", O_RDWR);
	mmap(c, 4096, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, 3, 0);
	*c = 0;

	if (fork()) {
		while(1) {
			strcpy(c, "/etc/passwd");
			strcpy(c, "/etc/shadow");
		}
	} else {
		while (1)
			if ((fd = open(c, 0)) != -1)
				close(fd);
	}

	return 0;
}
