#include "network_backends.h"

#include "network.h"
#include "fdevent.h"
#include "log.h"
#include "stat_cache.h"

#include "sys-socket.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#include <libsmbclient.h>
//#include <assert.h>
#define DBE 1

int network_write_chunkqueue_write(server *srv, connection *con, int fd, chunkqueue *cq) {
	chunk *c;
	size_t chunks_written = 0;
	fprintf(stderr,"network_write_chunkqueue_write");

	for(c = cq->first; c; c = c->next) {
		int chunk_finished = 0;

		switch(c->type) {
		case MEM_CHUNK: {
			char * offset;
			size_t toSend;
			ssize_t r;

			if (c->mem->used == 0) {
				chunk_finished = 1;
				break;
			}

			offset = c->mem->ptr + c->offset;
			toSend = c->mem->used - 1 - c->offset;
#ifdef __WIN32
			if ((r = send(fd, offset, toSend, 0)) < 0) {
				/* no error handling for windows... */
				log_error_write(srv, __FILE__, __LINE__, "ssd", "send failed: ", strerror(errno), fd);

				return -1;
			}
#else
			if ((r = write(fd, offset, toSend)) < 0) {
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
#endif

			c->offset += r;
			cq->bytes_out += r;

			if (c->offset == (off_t)c->mem->used - 1) {
				chunk_finished = 1;
			}

			break;
		}


		case FILE_CHUNK: {
#ifdef USE_MMAP
			char *p = NULL;
#endif
			ssize_t r;
			off_t offset;
			size_t toSend;
			stat_cache_entry *sce = NULL;
			int ifd;
			
			if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						strerror(errno), c->file.name);
				return -1;
			}

			offset = c->file.start + c->offset;
			toSend = c->file.length - c->offset;

			if (offset > sce->st.st_size) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "file was shrinked:", c->file.name);

				return -1;
			}

			if (-1 == (ifd = open(c->file.name->ptr, O_RDONLY))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));

				return -1;
			}

#ifdef USE_MMAP
			if (MAP_FAILED == (p = mmap(0, sce->st.st_size, PROT_READ, MAP_SHARED, ifd, 0))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "mmap failed: ", strerror(errno));

				close(ifd);

				return -1;
			}
			close(ifd);

			if ((r = write(fd, p + offset, toSend)) <= 0) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					r = 0;
					break;
				case EPIPE:
				case ECONNRESET:
					munmap(p, sce->st.st_size);
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
						"write failed:", strerror(errno), fd);
					munmap(p, sce->st.st_size);

					return -1;
				}
			}

			munmap(p, sce->st.st_size);
#else /* USE_MMAP */
			buffer_prepare_copy(srv->tmp_buf, toSend);

			lseek(ifd, offset, SEEK_SET);
			if (-1 == (toSend = read(ifd, srv->tmp_buf->ptr, toSend))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "read: ", strerror(errno));
				close(ifd);

				return -1;
			}
			close(ifd);

#ifdef __WIN32
			if ((r = send(fd, srv->tmp_buf->ptr, toSend, 0)) < 0) {
				/* no error handling for windows... */
				log_error_write(srv, __FILE__, __LINE__, "ssd", "send failed: ", strerror(errno), fd);

				return -1;
			}
#else /* __WIN32 */
			if ((r = write(fd, srv->tmp_buf->ptr, toSend)) < 0) {
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
#endif /* __WIN32 */
#endif /* USE_MMAP */

			c->offset += r;
			cq->bytes_out += r;

			if (c->offset == c->file.length) {
				chunk_finished = 1;
			}

			break;
		}

#if 0
		case SMB_CHUNK: {
			ssize_t r;
			off_t offset;
			size_t toSend;
			off_t rest_len;
			stat_cache_entry *sce = NULL;

#define BUFF_SIZE 256*1024

			char buff[BUFF_SIZE];
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

			Cdbg(DBE,"offset =%lli, toSend=%d, sce->st.st_size=%lli", offset, toSend, sce->st.st_size);

			if (offset > sce->st.st_size) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "file was shrinked:", c->file.name);
				Cdbg(DBE,"offset > size");
				if(buff) free(buff);
				return -1;
			}
						
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
#endif
		case SMB_CHUNK: {

			ssize_t r, w;
			off_t offset;
			size_t toSend;
			stat_cache_entry *sce = NULL;
			
			//#define BUFF_SIZE 2*1024

			//- 128K
			#define BUFF_SIZE 131072
			//#define BUFF_SIZE (1460*4)
			
			char buff[BUFF_SIZE]={0};
			offset = c->file.start + c->offset;
			toSend = (c->file.length - c->offset>BUFF_SIZE)?
			   BUFF_SIZE : c->file.length - c->offset ;

			/* open file if not already opened */
			Cdbg(DBE, " fn =[%s]", c->file.name->ptr);	
			
			if (-1 == c->file.fd) {
				Cdbg(DBE, "start to open file...");
				if (-1 == (c->file.fd = smbc_wrapper_open(con,c->file.name->ptr, O_RDONLY, 0755))) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));
					return -1;
				}
			}

			Cdbg(DBE, "c->file.fd=[%d], offset =%lli, to send=%d, c->file.length=%d", c->file.fd, offset, toSend, c->file.length);			
			
			smbc_wrapper_lseek(con, c->file.fd, offset, SEEK_SET );		

			r = smbc_wrapper_read(con, c->file.fd, buff, toSend);

			Cdbg(DBE, "toSend=[%d], errno=[%d]", toSend, errno);			

			if (toSend == -1||r<0) {
				smbc_wrapper_close(con, c->file.fd);
				switch (errno) {
				case EAGAIN:
				case EINTR:
					/* ok, we can't send more, let's try later again */
					r = 0;
					break;
				case EPIPE:
				case ECONNRESET:
					return -2;
				default:
					Cdbg(DBE, "11111 %s", strerror(errno));
					log_error_write(srv, __FILE__, __LINE__, "ssd",
							"sendfile failed:", strerror(errno), fd);
					return -1;
				}
			} else if (r == 0) {
				int oerrno = errno;
				/* We got an event to write but we wrote nothing
				 *
				 * - the file shrinked -> error
				 * - the remote side closed inbetween -> remote-close */
				
				if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
					/* file is gone ? */
					return -1;
				}

				if (offset > sce->st.st_size) {
					/* file shrinked, close the connection */
					errno = oerrno;

					return -1;
				}

				errno = oerrno;
				return -2;
			}

			r = write(fd, buff, toSend);

			Cdbg(DBE, "write [%d] bytes, fd=%d", r, fd);
			
			if(r<=0) {
				//write error??
			   	Cdbg(DBE,"errno =%d -> %s",errno,strerror(errno));
				smbc_wrapper_close(con,c->file.fd);
				return -1;
			}

			c->offset += r;
			cq->bytes_out += r;

			Cdbg(DBE, "c->offset=[%d], c->file.length=[%d]", c->offset, c->file.length);
			
			if (c->offset == c->file.length) {
				chunk_finished = 1;

				Cdbg(DBE, "file finish");
				/* chunk_free() / chunk_reset() will cleanup for us but it is a ok to be faster :) */

				if (c->file.fd != -1) {
					smbc_wrapper_close(con,c->file.fd);
					c->file.fd = -1;
				}
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

#if 0
network_write_init(void) {
	p->write = network_write_write_chunkset;
}
#endif
