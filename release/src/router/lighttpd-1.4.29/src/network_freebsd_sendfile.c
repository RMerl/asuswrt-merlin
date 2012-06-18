#include "network_backends.h"

#ifdef USE_FREEBSD_SENDFILE

#include "network.h"
#include "fdevent.h"
#include "log.h"
#include "stat_cache.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>


#ifndef UIO_MAXIOV
# if defined(__FreeBSD__) || defined(__DragonFly__)
/* FreeBSD 4.7, 4.9 defined it in sys/uio.h only if _KERNEL is specified */
#  define UIO_MAXIOV 1024
# endif
#endif

int network_write_chunkqueue_freebsdsendfile(server *srv, connection *con, int fd, chunkqueue *cq) {
	chunk *c;
	size_t chunks_written = 0;

	for(c = cq->first; c; c = c->next, chunks_written++) {
		int chunk_finished = 0;

		switch(c->type) {
		case MEM_CHUNK: {
			char * offset;
			size_t toSend;
			ssize_t r;

			size_t num_chunks, i;
			struct iovec chunks[UIO_MAXIOV];
			chunk *tc;
			size_t num_bytes = 0;

			/* we can't send more then SSIZE_MAX bytes in one chunk */

			/* build writev list
			 *
			 * 1. limit: num_chunks < UIO_MAXIOV
			 * 2. limit: num_bytes < SSIZE_MAX
			 */
			for(num_chunks = 0, tc = c; tc && tc->type == MEM_CHUNK && num_chunks < UIO_MAXIOV; num_chunks++, tc = tc->next);

			for(tc = c, i = 0; i < num_chunks; tc = tc->next, i++) {
				if (tc->mem->used == 0) {
					chunks[i].iov_base = tc->mem->ptr;
					chunks[i].iov_len  = 0;
				} else {
					offset = tc->mem->ptr + tc->offset;
					toSend = tc->mem->used - 1 - tc->offset;

					chunks[i].iov_base = offset;

					/* protect the return value of writev() */
					if (toSend > SSIZE_MAX ||
					    num_bytes + toSend > SSIZE_MAX) {
						chunks[i].iov_len = SSIZE_MAX - num_bytes;

						num_chunks = i + 1;
						break;
					} else {
						chunks[i].iov_len = toSend;
					}

					num_bytes += toSend;
				}
			}

			if ((r = writev(fd, chunks, num_chunks)) < 0) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					r = 0;
					break;
				case ENOTCONN:
				case EPIPE:
				case ECONNRESET:
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
							"writev failed:", strerror(errno), fd);

					return -1;
				}

				r = 0;
			}

			/* check which chunks have been written */
			cq->bytes_out += r;

			for(i = 0, tc = c; i < num_chunks; i++, tc = tc->next) {
				if (r >= (ssize_t)chunks[i].iov_len) {
					/* written */
					r -= chunks[i].iov_len;
					tc->offset += chunks[i].iov_len;

					if (chunk_finished) {
						/* skip the chunks from further touches */
						chunks_written++;
						c = c->next;
					} else {
						/* chunks_written + c = c->next is done in the for()*/
						chunk_finished++;
					}
				} else {
					/* partially written */

					tc->offset += r;
					chunk_finished = 0;

					break;
				}
			}

			break;
		}
		case FILE_CHUNK: {
			off_t offset, r;
			size_t toSend;
			stat_cache_entry *sce = NULL;
			
			if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						strerror(errno), c->file.name);
				return -1;
			}

			offset = c->file.start + c->offset;
			/* limit the toSend to 2^31-1 bytes in a chunk */
			toSend = c->file.length - c->offset > ((1 << 30) - 1) ?
				((1 << 30) - 1) : c->file.length - c->offset;

			if (-1 == c->file.fd) {
				if (-1 == (c->file.fd = open(c->file.name->ptr, O_RDONLY))) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));

					return -1;
				}

#ifdef FD_CLOEXEC
				fcntl(c->file.fd, F_SETFD, FD_CLOEXEC);
#endif
			}

			r = 0;

			/* FreeBSD sendfile() */
			if (-1 == sendfile(c->file.fd, fd, offset, toSend, NULL, &r, 0)) {
				switch(errno) {
				case EAGAIN:
				case EINTR:
					/* for EAGAIN/EINTR r still contains the sent bytes */
					break; /* try again later */
				case EPIPE:
				case ENOTCONN:
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd", "sendfile: ", strerror(errno), errno);
					return -1;
				}
			} else if (r == 0) {
				/* We got an event to write but we wrote nothing
				 *
				 * - the file shrinked -> error
				 * - the remote side closed inbetween -> remote-close */
				
				if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
					/* file is gone ? */
					return -1;
				}

				if (offset >= sce->st.st_size) {
					/* file shrinked, close the connection */
					return -1;
				}

				return -2;
			}

			c->offset += r;
			cq->bytes_out += r;

			if (c->offset == c->file.length) {
				chunk_finished = 1;
			}

			break;
		}
		default:

			log_error_write(srv, __FILE__, __LINE__, "ds", c, "type not known");

			return -1;
		}

		if (!chunk_finished) {
			/* not finished yet */

			break;
		}
	}

	return chunks_written;
}

#endif
