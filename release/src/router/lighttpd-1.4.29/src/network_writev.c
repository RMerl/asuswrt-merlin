#include "network_backends.h"

#ifdef USE_WRITEV

#include "network.h"
#include "fdevent.h"
#include "log.h"
#include "stat_cache.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
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
#include <limits.h>
#include <stdio.h>
#include <assert.h>

#if 0
#define LOCAL_BUFFERING 1
#endif

#define DBE 1

int network_write_chunkqueue_writev(server *srv, connection *con, int fd, chunkqueue *cq) {
	chunk *c;
	size_t chunks_written = 0;

	for(c = cq->first; c; c = c->next) {
		int chunk_finished = 0;

		switch(c->type) {
		case MEM_CHUNK: {
			char * offset;
			size_t toSend;
			ssize_t r;

			size_t num_chunks, i;
			struct iovec *chunks;
			chunk *tc;
			size_t num_bytes = 0;
#if defined(_SC_IOV_MAX) /* IRIX, MacOS X, FreeBSD, Solaris, ... */
			const size_t max_chunks = sysconf(_SC_IOV_MAX);
#elif defined(IOV_MAX) /* Linux x86 (glibc-2.3.6-3) */
			const size_t max_chunks = IOV_MAX;
#elif defined(MAX_IOVEC) /* Linux ia64 (glibc-2.3.3-98.28) */
			const size_t max_chunks = MAX_IOVEC;
#elif defined(UIO_MAXIOV) /* Linux x86 (glibc-2.2.5-233) */
			const size_t max_chunks = UIO_MAXIOV;
#elif (defined(__FreeBSD__) && __FreeBSD_version < 500000) || defined(__DragonFly__) || defined(__APPLE__) 
			/* - FreeBSD 4.x
			 * - MacOS X 10.3.x
			 *   (covered in -DKERNEL)
			 *  */
			const size_t max_chunks = 1024; /* UIO_MAXIOV value from sys/uio.h */
#else
#error "sysconf() doesnt return _SC_IOV_MAX ..., check the output of 'man writev' for the EINVAL error and send the output to jan@kneschke.de"
#endif

			/* we can't send more then SSIZE_MAX bytes in one chunk */

			/* build writev list
			 *
			 * 1. limit: num_chunks < max_chunks
			 * 2. limit: num_bytes < SSIZE_MAX
			 */
			for (num_chunks = 0, tc = c; tc && tc->type == MEM_CHUNK && num_chunks < max_chunks; num_chunks++, tc = tc->next);

			chunks = calloc(num_chunks, sizeof(*chunks));

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
				case EPIPE:
				case ECONNRESET:
					free(chunks);
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
							"writev failed:", strerror(errno), fd);

					free(chunks);
					return -1;
				}
			}

			cq->bytes_out += r;

			/* check which chunks have been written */

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
			free(chunks);

			break;
		}
		case FILE_CHUNK: {
			ssize_t r;
			off_t abs_offset;
			off_t toSend;
			stat_cache_entry *sce = NULL;

#define KByte * 1024
#define MByte * 1024 KByte
#define GByte * 1024 MByte
			const off_t we_want_to_mmap = 512 KByte;
			char *start = NULL;
			
			if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						strerror(errno), c->file.name);
				return -1;
			}

			abs_offset = c->file.start + c->offset;

			if (abs_offset > sce->st.st_size) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"file was shrinked:", c->file.name);

				return -1;
			}

			/* mmap the buffer
			 * - first mmap
			 * - new mmap as the we are at the end of the last one */
			if (c->file.mmap.start == MAP_FAILED ||
			    abs_offset == (off_t)(c->file.mmap.offset + c->file.mmap.length)) {

				/* Optimizations for the future:
				 *
				 * adaptive mem-mapping
				 *   the problem:
				 *     we mmap() the whole file. If someone has alot large files and 32bit
				 *     machine the virtual address area will be unrun and we will have a failing
				 *     mmap() call.
				 *   solution:
				 *     only mmap 16M in one chunk and move the window as soon as we have finished
				 *     the first 8M
				 *
				 * read-ahead buffering
				 *   the problem:
				 *     sending out several large files in parallel trashes the read-ahead of the
				 *     kernel leading to long wait-for-seek times.
				 *   solutions: (increasing complexity)
				 *     1. use madvise
				 *     2. use a internal read-ahead buffer in the chunk-structure
				 *     3. use non-blocking IO for file-transfers
				 *   */

				/* all mmap()ed areas are 512kb expect the last which might be smaller */
				off_t we_want_to_send;
				size_t to_mmap;

				/* this is a remap, move the mmap-offset */
				if (c->file.mmap.start != MAP_FAILED) {
					munmap(c->file.mmap.start, c->file.mmap.length);
					c->file.mmap.offset += we_want_to_mmap;
				} else {
					/* in case the range-offset is after the first mmap()ed area we skip the area */
					c->file.mmap.offset = 0;

					while (c->file.mmap.offset + we_want_to_mmap < c->file.start) {
						c->file.mmap.offset += we_want_to_mmap;
					}
				}

				/* length is rel, c->offset too, assume there is no limit at the mmap-boundaries */
				we_want_to_send = c->file.length - c->offset;
				to_mmap = (c->file.start + c->file.length) - c->file.mmap.offset;

				/* we have more to send than we can mmap() at once */
				if (abs_offset + we_want_to_send > c->file.mmap.offset + we_want_to_mmap) {
					we_want_to_send = (c->file.mmap.offset + we_want_to_mmap) - abs_offset;
					to_mmap = we_want_to_mmap;
				}

				if (-1 == c->file.fd) {  /* open the file if not already open */
					if (-1 == (c->file.fd = open(c->file.name->ptr, O_RDONLY))) {
						log_error_write(srv, __FILE__, __LINE__, "sbs", "open failed for:", c->file.name, strerror(errno));

						return -1;
					}
#ifdef FD_CLOEXEC
					fcntl(c->file.fd, F_SETFD, FD_CLOEXEC);
#endif
				}

				if (MAP_FAILED == (c->file.mmap.start = mmap(0, to_mmap, PROT_READ, MAP_SHARED, c->file.fd, c->file.mmap.offset))) {
					/* close it here, otherwise we'd have to set FD_CLOEXEC */

					log_error_write(srv, __FILE__, __LINE__, "ssbd", "mmap failed:",
							strerror(errno), c->file.name, c->file.fd);

					return -1;
				}

				c->file.mmap.length = to_mmap;
#ifdef LOCAL_BUFFERING
				buffer_copy_string_len(c->mem, c->file.mmap.start, c->file.mmap.length);
#else
#ifdef HAVE_MADVISE
				/* don't advise files < 64Kb */
				if (c->file.mmap.length > (64 KByte)) {
					/* darwin 7 is returning EINVAL all the time and I don't know how to
					 * detect this at runtime.i
					 *
					 * ignore the return value for now */
					madvise(c->file.mmap.start, c->file.mmap.length, MADV_WILLNEED);
				}
#endif
#endif

				/* chunk_reset() or chunk_free() will cleanup for us */
			}

			/* to_send = abs_mmap_end - abs_offset */
			toSend = (c->file.mmap.offset + c->file.mmap.length) - (abs_offset);

			if (toSend < 0) {
				log_error_write(srv, __FILE__, __LINE__, "soooo",
						"toSend is negative:",
						toSend,
						c->file.mmap.length,
						abs_offset,
						c->file.mmap.offset);
				assert(toSend < 0);
			}

#ifdef LOCAL_BUFFERING
			start = c->mem->ptr;
#else
			start = c->file.mmap.start;
#endif

			if ((r = write(fd, start + (abs_offset - c->file.mmap.offset), toSend)) < 0) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					r = 0;
					break;
				case EPIPE:
				case ECONNRESET:
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
							"write failed:", strerror(errno), fd);

					return -1;
				}
			}

			c->offset += r;
			cq->bytes_out += r;

			if (c->offset == c->file.length) {
				chunk_finished = 1;

				/* we don't need the mmaping anymore */
				if (c->file.mmap.start != MAP_FAILED) {
					munmap(c->file.mmap.start, c->file.mmap.length);
					c->file.mmap.start = MAP_FAILED;
				}
			}

			break;
		}
		case SMB_CHUNK:
		{
			ssize_t r;
			off_t offset;
			size_t toSend;
			off_t rest_len;
			stat_cache_entry *sce = NULL;
//#define BUFF_SIZE 2048

//- 256K
#define BUFF_SIZE 256*1024


		
			char buff[BUFF_SIZE]={0};
//			memset(buff,0,BUFF_SIZE);
//			char *buff=NULL;
			int ifd;
			
			if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						strerror(errno), c->file.name);
				Cdbg(DBE,"stat cache get entry failed");
				return -1;
			}

			offset = c->file.start + c->offset;
			toSend = (c->file.length - c->offset>BUFF_SIZE)?
			   BUFF_SIZE : c->file.length - c->offset ;

//			rest_len = c->file.length - c->offset; 
//			toSend =  
			Cdbg(DBE,"offset =%lli, toSend=%d, sce->st.st_size=%lli", offset, toSend, sce->st.st_size);

			if (offset > sce->st.st_size) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "file was shrinked:", c->file.name);
				Cdbg(DBE,"offset > size");
				return -1;
			}

//			if (-1 == (ifd = open(c->file.name->ptr, O_RDONLY))) {
				if (-1 == (ifd = smbc_wrapper_open(con,c->file.name->ptr, O_RDONLY, 0755))) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));
					Cdbg(DBE,"wrapper open failed,ifd=%d,  fn =%s, open failed =%s, errno =%d",ifd, c->file.name->ptr,strerror(errno),errno);
					return -1;
				}
			Cdbg(DBE,"ifd =%d, toSend=%d",ifd, toSend);
			smbc_wrapper_lseek(con, ifd, offset, SEEK_SET );		
			if (-1 == (toSend = smbc_wrapper_read(con, ifd, buff, toSend ))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "read: ", strerror(errno));
				smbc_wrapper_close(con, ifd);
				Cdbg(DBE,"ifd =%d,toSend =%d, errno=%s",ifd,toSend, strerror(errno));
				return -1;
			}
			Cdbg(DBE,"close ifd=%d, toSend=%d",ifd,toSend);
			smbc_wrapper_close(con, ifd);

			Cdbg(DBE,"write socket fd=%d",fd);
			if ((r = write(fd, buff, toSend)) < 0) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					r = 0;
					break;
				case EPIPE:
				case ECONNRESET:
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
						"write failed:", strerror(errno), fd);

					return -1;
				}
			}

			c->offset += r;
			cq->bytes_out += r;
			Cdbg(DBE,"r =%d",r);
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

		chunks_written++;
	}

	return chunks_written;
}

#endif
