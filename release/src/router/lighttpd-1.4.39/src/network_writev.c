#include "network_backends.h"

#if defined(USE_WRITEV)

#include "network.h"
#include "log.h"

#if defined(HAVE_SYS_UIO_H)
# include <sys/uio.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#if defined(UIO_MAXIOV)
# define SYS_MAX_CHUNKS UIO_MAXIOV
#elif defined(IOV_MAX)
/* new name for UIO_MAXIOV since IEEE Std 1003.1-2001 */
# define SYS_MAX_CHUNKS IOV_MAX
#elif defined(_XOPEN_IOV_MAX)
/* minimum value for sysconf(_SC_IOV_MAX); posix requires this to be at least 16, which is good enough - no need to call sysconf() */
# define SYS_MAX_CHUNKS _XOPEN_IOV_MAX
#else
# error neither UIO_MAXIOV nor IOV_MAX nor _XOPEN_IOV_MAX are defined
#endif

/* allocate iovec[MAX_CHUNKS] on stack, so pick a sane limit:
 * - each entry will use 1 pointer + 1 size_t
 * - 32 chunks -> 256 / 512 bytes (32-bit/64-bit pointers)
 */
#define STACK_MAX_ALLOC_CHUNKS 32
#if SYS_MAX_CHUNKS > STACK_MAX_ALLOC_CHUNKS
# define MAX_CHUNKS STACK_MAX_ALLOC_CHUNKS
#else
# define MAX_CHUNKS SYS_MAX_CHUNKS
#endif

#define DBE 0

int network_writev_mem_chunks(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	struct iovec chunks[MAX_CHUNKS];
	size_t num_chunks;
	off_t max_bytes = *p_max_bytes;
	off_t toSend;
	ssize_t r;
	UNUSED(con);

	force_assert(NULL != cq->first);
	force_assert(MEM_CHUNK == cq->first->type);

	{
		chunk const *c;

		toSend = 0;
		num_chunks = 0;
		for (c = cq->first; NULL != c && MEM_CHUNK == c->type && num_chunks < MAX_CHUNKS && toSend < max_bytes; c = c->next) {
			size_t c_len;

			force_assert(c->offset >= 0 && c->offset <= (off_t)buffer_string_length(c->mem));
			c_len = buffer_string_length(c->mem) - c->offset;
			if (c_len > 0) {
				toSend += c_len;

				chunks[num_chunks].iov_base = c->mem->ptr + c->offset;
				chunks[num_chunks].iov_len = c_len;

				++num_chunks;
			}
		}
	}

	if (0 == num_chunks) {
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}

	r = writev(fd, chunks, num_chunks);

	if (r < 0) switch (errno) {
	case EAGAIN:
	case EINTR:
		break;
	case EPIPE:
	case ECONNRESET:
		return -2;
	default:
		log_error_write(srv, __FILE__, __LINE__, "ssd",
				"writev failed:", strerror(errno), fd);
		return -1;
	}

	if (r >= 0) {
		*p_max_bytes -= r;
		chunkqueue_mark_written(cq, r);
	}

	return (r > 0 && r == toSend) ? 0 : -3;
}

#endif /* USE_WRITEV */

int network_write_chunkqueue_writev(server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes) {
	while (max_bytes > 0 && NULL != cq->first) {
		int r = -1;

		switch (cq->first->type) {
		case MEM_CHUNK:
			r = network_writev_mem_chunks(srv, con, fd, cq, &max_bytes);
			break;
		case FILE_CHUNK:
			r = network_write_file_chunk_mmap(srv, con, fd, cq, &max_bytes);
			break;
		case SMB_CHUNK:
			r = network_write_smbfile_chunk_mmap(srv, con, fd, cq, &max_bytes);			
			break;
		}

		if (-3 == r) return 0;
		if (0 != r) return r;
	}

	return 0;
}
