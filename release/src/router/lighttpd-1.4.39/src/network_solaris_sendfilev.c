#include "network_backends.h"

#if defined(USE_SOLARIS_SENDFILEV)

#include "network.h"
#include "log.h"

#include <sys/sendfile.h>

#include <errno.h>
#include <string.h>

/**
 * a very simple sendfilev() interface for solaris which can be optimised a lot more
 * as solaris sendfilev() supports 'sending everythin in one syscall()'
 */

int network_write_file_chunk_sendfile(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	chunk* const c = cq->first;
	off_t offset;
	off_t toSend;
	size_t written = 0;
	int r;
	sendfilevec_t fvec;

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

	fvec.sfv_fd = c->file.fd;
	fvec.sfv_flag = 0;
	fvec.sfv_off = offset;
	fvec.sfv_len = toSend;

	/* Solaris sendfilev() */

	if (-1 == (r = sendfilev(fd, &fvec, 1, &written))) {
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

	return (r >= 0 && (off_t) written == toSend) ? 0 : -3;
}

#endif /* USE_SOLARIS_SENDFILEV */
