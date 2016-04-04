#include "network_backends.h"

#if defined(USE_FREEBSD_SENDFILE)

#include "network.h"
#include "log.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <errno.h>
#include <string.h>

int network_write_file_chunk_sendfile(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	chunk* const c = cq->first;
	off_t offset, written = 0;
	off_t toSend;
	int r;

	force_assert(NULL != c);
	force_assert(FILE_CHUNK == c->type);
	force_assert(c->offset >= 0 && c->offset <= c->file.length);

	offset = c->file.start + c->offset;
	toSend = c->file.length - c->offset;
	if (toSend > *p_max_bytes) toSend = *p_max_bytes;

	if (0 == toSend) {
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}

	if (0 != network_open_file_chunk(srv, con, cq)) return -1;

	/* FreeBSD sendfile() */
	if (-1 == (r = sendfile(c->file.fd, fd, offset, toSend, NULL, &written, 0))) {
		switch(errno) {
		case EAGAIN:
		case EINTR:
			/* for EAGAIN/EINTR written still contains the sent bytes */
			break; /* try again later */
		case EPIPE:
		case ENOTCONN:
			return -2;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd", "sendfile: ", strerror(errno), errno);
			return -1;
		}
	}

	if (written >= 0) {
		chunkqueue_mark_written(cq, written);
		*p_max_bytes -= written;
	}

	return (r >= 0 && written == toSend) ? 0 : -3;
}

#endif /* USE_FREEBSD_SENDFILE */
