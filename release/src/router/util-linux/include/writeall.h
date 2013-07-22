#ifndef UTIL_LINUX_WRITEALL_H
#define UTIL_LINUX_WRITEALL_H

#include <string.h>
#include <unistd.h>
#include <errno.h>

static inline int write_all(int fd, const void *buf, size_t count)
{
	while (count) {
		ssize_t tmp;

		errno = 0;
		tmp = write(fd, buf, count);
		if (tmp > 0) {
			count -= tmp;
			if (count)
				buf += tmp;
		} else if (errno != EINTR && errno != EAGAIN)
			return -1;
		if (errno == EAGAIN)	/* Try later, *sigh* */
			usleep(10000);
	}
	return 0;
}

static inline int fwrite_all(const void *ptr, size_t size,
			     size_t nmemb, FILE *stream)
{
	while (nmemb) {
		size_t tmp;

		errno = 0;
		tmp = fwrite(ptr, size, nmemb, stream);
		if (tmp > 0) {
			nmemb -= tmp;
			if (nmemb)
				ptr += (tmp * size);
		} else if (errno != EINTR && errno != EAGAIN)
			return -1;
		if (errno == EAGAIN)	/* Try later, *sigh* */
			usleep(10000);
	}
	return 0;
}

#endif /* UTIL_LINUX_WRITEALL_H */
