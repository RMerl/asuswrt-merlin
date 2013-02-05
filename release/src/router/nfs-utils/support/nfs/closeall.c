/*
 * support/nfs/closeall.c
 * Close all file descriptors greater than some limit,
 * Use readdir "/proc/self/fd" to avoid excess close(2) calls.
 */

#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>

void
closeall(int min)
{
	DIR *dir = opendir("/proc/self/fd");
	if (dir != NULL) {
		int dfd = dirfd(dir);
		struct dirent *d;

		while ((d = readdir(dir)) != NULL) {
			char *endp;
			long n = strtol(d->d_name, &endp, 10);
			if (*endp != '\0' && n >= min && n != dfd)
				(void) close(n);
		}
		closedir(dir);
	} else {
		int fd = sysconf(_SC_OPEN_MAX);
		while (--fd >= min)
			(void) close(fd);
	}
}
