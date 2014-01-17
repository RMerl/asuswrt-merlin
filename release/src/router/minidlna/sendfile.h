#if defined(HAVE_LINUX_SENDFILE_API)

#include <sys/sendfile.h>

int sys_sendfile(int sock, int sendfd, off_t *offset, off_t len)
{
	return sendfile(sock, sendfd, offset, len);
}

#elif defined(HAVE_DARWIN_SENDFILE_API)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

int sys_sendfile(int sock, int sendfd, off_t *offset, off_t len)
{
	int ret;

	ret = sendfile(sendfd, sock, *offset, &len, NULL, 0);
	*offset += len;

	return ret;
}

#elif defined(HAVE_FREEBSD_SENDFILE_API)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

int sys_sendfile(int sock, int sendfd, off_t *offset, off_t len)
{
	int ret;
	size_t nbytes = len;

	ret = sendfile(sendfd, sock, *offset, nbytes, NULL, &len, SF_MNOWAIT);
	*offset += len;

	return ret;
}

#else

#include <errno.h>

int sys_sendfile(int sock, int sendfd, off_t *offset, off_t len)
{
	errno = EINVAL;
	return -1;
}

#endif
