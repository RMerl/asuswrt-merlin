#include "network_backends.h"

#ifdef USE_LINUX_SENDFILE

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
#include <fcntl.h>
#include <assert.h>

#include <pthread.h>
#include <libsmbclient.h>
/* on linux 2.4.29 + debian/ubuntu we have crashes if this is enabled */
#undef HAVE_POSIX_FADVISE

#define DBE 1
//#define USE_SMB_THREAD
//#define BUFF_SIZE (1460*4)
//#define BUFF_SIZE 131072
#define BUFF_SIZE (1460*10)

#ifdef USE_SMB_THREAD
#include <dlinklist.h>

typedef struct _ChunkInfo{
	server *srv;
	connection *con;
	chunk *c;
	int fd;
	chunkqueue *cq;
}ChunkInfo;

static void *thread_do_smb_transition(void* chunkInfo)
{	
	ChunkInfo *pchunkInfo = chunkInfo;
			
	if(pchunkInfo==NULL)
		goto error;
	
	server *srv = pchunkInfo->srv;
	connection *con = pchunkInfo->con;
	chunk *c = pchunkInfo->c;
	//int fd = pchunkInfo->fd;
	//chunkqueue *cq = pchunkInfo->cq;
	
	//data_string *ds_userAgent = (data_string *)array_get_element(con->request.headers, "user-Agent");	
	//Cdbg(1, " fn =[%s], ds_userAgent=[%s]", pchunkInfo->file_name->ptr, ds_userAgent->value->ptr);
	
	ssize_t r, w;
	off_t offset;
	size_t toSend;
	stat_cache_entry *sce = NULL;
	int count = 0;
	
	do{
		if(c->listcount>100){
			Cdbg(1, "sleep...");
			sleep(10);
			continue;
		}
		
		char buff[BUFF_SIZE]={0};
		Cdbg(1, "00, c->file.start=%d, c->file.length=[%d]", c->file.start, c->file.length);
		offset = c->file.start + c->roffset;
		toSend = ( c->file.length - c->roffset > BUFF_SIZE)?
				   BUFF_SIZE : c->file.length - c->roffset ;
		
		if (-1 == c->file.fd) {
			Cdbg(1, "thread_do_smb_transition: start to open file...");
			if (-1 == (c->file.fd = smbc_wrapper_open(con,c->file.name->ptr, O_RDONLY, 0755))) {
				Cdbg(1, "fail to open file...");
				log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));
				goto error;
			}
		}
		
		smbc_wrapper_lseek(con, c->file.fd, offset, SEEK_SET );		

		Cdbg(1, "11, c->file.fd=%d, smbc_wrapper_lseek=%d, toSend=%zu", c->file.fd, offset, toSend);
		r = smbc_wrapper_read(con, c->file.fd, buff, toSend);
		Cdbg(1, "22, r=%d", r);
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
					Cdbg(1, "1");
					goto error;
				default:
					Cdbg(1, "2");
					goto error;
			}
		} 
		else if (r == 0) {
			int oerrno = errno;
			/* We got an event to write but we wrote nothing
			 *
			 * - the file shrinked -> error
			 * - the remote side closed inbetween -> remote-close */
				
			if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
				/* file is gone ? */
				Cdbg(1, "3");
				goto error;
			}
			
			if (offset > sce->st.st_size) {
				/* file shrinked, close the connection */
				errno = oerrno;
				Cdbg(1, "4");
				goto error;
			}

			errno = oerrno;
			Cdbg(1, "5");
			goto error;
		}
		
		smb_file_buffer_t *smb_file_info;
		smb_file_info = (smb_file_buffer_t *)calloc(1, sizeof(smb_file_buffer_t));
		smb_file_info->id = count;
		smb_file_info->buffer = (char*)malloc(BUFF_SIZE);
		smb_file_info->toSend = toSend;
		memset(smb_file_info->buffer, '\0', BUFF_SIZE);
		memcpy(smb_file_info->buffer, buff, BUFF_SIZE);
		
		DLIST_ADD_END(c->smb_buffer_list, smb_file_info, smb_file_buffer_t*);
		c->listcount++;
		
		c->roffset += r;
		
		Cdbg(1, "toSend=[%d], errno=[%d], r=[%d], c->roffset=[%d], c->listcount=[%d]", toSend, errno, r, c->roffset, c->listcount);

		count++;
				
		if (c->roffset == c->file.length) {
			//chunk_finished = 1;
			
			Cdbg(1, "file finish, c->roffset=[%d]", c->roffset);
			
			if (c->file.fd != -1) {
				smbc_wrapper_close(con,c->file.fd);
				c->file.fd = -1;
				Cdbg(1, "smbc_wrapper_close");
				goto error;
			}
			
			Cdbg(1, "break.........................................");
			break;
		}
		//sleep(1);
	}while(1);

error:
	Cdbg(1, "pthread_exit.........................................");
	c->open_thread = 2;
	//free(pchunkInfo);
	//pthread_exit(NULL);
}

#endif

int network_write_chunkqueue_linuxsendfile(server *srv, connection *con, int fd, chunkqueue *cq) {
	chunk *c;
	size_t chunks_written = 0;

	for(c = cq->first; c; c = c->next, chunks_written++) {
		int chunk_finished = 0;

		//Cdbg(DBE, "\t c->type=[%d]", c->type);

		switch(c->type) {
		case MEM_CHUNK: {
			char * offset;
			size_t toSend;
			ssize_t r;
			//Cdbg(DBE, "MEM_CHUNK...");

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
			for (num_chunks = 0, tc = c;
			     tc && tc->type == MEM_CHUNK && num_chunks < UIO_MAXIOV;
			     tc = tc->next, num_chunks++);

			for (tc = c, i = 0; i < num_chunks; tc = tc->next, i++) {
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
//Cdbg(DBE, "num_chunks = %d", num_chunks);

//Cdbg(DBE, "MEMORY = %s", chunks[0].iov_base);
			if ((r = writev(fd, chunks, num_chunks)) < 0) {
				Cdbg(DBE, "errno =%s, r=%d", strerror(errno),r);
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
							"writev failed:", strerror(errno), fd);

					return -1;
				}
			}

			/* check which chunks have been written */
			cq->bytes_out += r;
//Cdbg(DBE, "bytesout=%lli",cq->bytes_out );
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
			ssize_t r;
			off_t offset;
			size_t toSend;
			Cdbg(DBE, "FILE_CHUNK...");
			
			stat_cache_entry *sce = NULL;
Cdbg(DBE, "\t\tfilename=[%s]", c->file.name->ptr);
			offset = c->file.start + c->offset;
			/* limit the toSend to 2^31-1 bytes in a chunk */
			toSend = c->file.length - c->offset > ((1 << 30) - 1) ?
				((1 << 30) - 1) : c->file.length - c->offset;

			Cdbg(DBE, "to send=%d, c->file.length=%d", 
				toSend, c->file.length);
			
			/* open file if not already opened */
			if (-1 == c->file.fd) {
				if (-1 == (c->file.fd = open(c->file.name->ptr, O_RDONLY))) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));

					return -1;
				}
#ifdef FD_CLOEXEC				
				fcntl(c->file.fd, F_SETFD, FD_CLOEXEC);
#endif
#ifdef HAVE_POSIX_FADVISE				
				/* tell the kernel that we want to stream the file */
				if (-1 == posix_fadvise(c->file.fd, 0, 0, POSIX_FADV_SEQUENTIAL)) {
					if (ENOSYS != errno) {
						log_error_write(srv, __FILE__, __LINE__, "ssd",
							"posix_fadvise failed:", strerror(errno), c->file.fd);
					}
				}
#endif
			}

			Cdbg(DBE, "c->file.fd=[%d], offset =%lli, to send=%d, c->file.length=%d", 
				c->file.fd, offset, toSend, c->file.length);			
			
			if (-1 == (r = sendfile(fd, c->file.fd, &offset, toSend))) {
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
					log_error_write(srv, __FILE__, __LINE__, "ssd",
							"sendfile failed:", strerror(errno), fd);
					return -1;
				}
			} 
			else if (r == 0) {
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

#ifdef HAVE_POSIX_FADVISE
#if 0
#define K * 1024
#define M * 1024 K
#define READ_AHEAD 4 M
			/* check if we need a new chunk */
			if ((c->offset & ~(READ_AHEAD - 1)) != ((c->offset + r) & ~(READ_AHEAD - 1))) {
				/* tell the kernel that we want to stream the file */
				if (-1 == posix_fadvise(c->file.fd, (c->offset + r) & ~(READ_AHEAD - 1), READ_AHEAD, POSIX_FADV_NOREUSE)) {
					log_error_write(srv, __FILE__, __LINE__, "ssd",
						"posix_fadvise failed:", strerror(errno), c->file.fd);
				}
			}
#endif
#endif

			c->offset += r;
			cq->bytes_out += r;

			if (c->offset == c->file.length) {
				chunk_finished = 1;

				/* chunk_free() / chunk_reset() will cleanup for us but it is a ok to be faster :) */

				if (c->file.fd != -1) {
					close(c->file.fd);
					c->file.fd = -1;
				}
			}

			break;
		}

#ifdef USE_SMB_THREAD
		case SMB_CHUNK: {

			if(c->open_thread!=1&&c->open_thread!=2){

				ChunkInfo* chunkinfo = (ChunkInfo*)malloc(sizeof(ChunkInfo));
				chunkinfo->srv = srv;
				chunkinfo->con = con;
				chunkinfo->c = c;
				chunkinfo->fd = fd;
				chunkinfo->cq = cq;
	
				int res;
				pthread_t t;
				pthread_attr_t thread_attr;
				res = pthread_attr_init(&thread_attr);
					
				if(res!=0){
					Cdbg(1, "Attribute creation failed!\n");
					continue;
				}

				res = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
				if(res!=0){
					Cdbg(1, "Setting detached attribute failed!\n");
					continue;
				}

				c->listcount = 0;
				c->open_thread = 1;

				Cdbg(1, "Create thread!\n");
				res = pthread_create( &t, 
								      &thread_attr,
								      thread_do_smb_transition,
								      (void*)chunkinfo);
			}

			ssize_t r;
			smb_file_buffer_t *p;
			for (p = c->smb_buffer_list; p; p = p->next) {

				r = write(fd, p->buffer, p->toSend);

				if(r<=0) {
					//write error??
				   	//Cdbg(DBE,"errno =%d -> %s",errno,strerror(errno));
					//smbc_wrapper_close(con,c->file.fd);
					return -1;
				}

				c->offset += r;
				cq->bytes_out += r;
				
				free(p->buffer);
				DLIST_REMOVE(c->smb_buffer_list, p);
				c->listcount--;

				//Cdbg(1, "%d, write [%d] bytes, fd=%d, c->offset=%d, c->listcount=[%d]", p->id, r, fd, c->offset, c->listcount);
				
				break;
			}
			
			if (c->offset == c->file.length) {
				chunk_finished = 1;
				Cdbg(1, "11111111111111111");

				
				/* chunk_free() / chunk_reset() will cleanup for us but it is a ok to be faster :) */
				
				if (c->file.fd != -1) {
					smbc_wrapper_close(con,c->file.fd);
					c->file.fd = -1;
					Cdbg(1, "xxxxxfile finish, c->roffset=[%d]", c->offset);
				}

				c->open_thread=-1;
			}
			
			break;
		}
#else
		case SMB_CHUNK: {

			ssize_t r, w;
			off_t offset;
			size_t toSend;
			stat_cache_entry *sce = NULL;

			//- 128K
			//#define BUFF_SIZE 128*1024
			//#define BUFF_SIZE 131072
			
			char buff[BUFF_SIZE]={0};
	
			offset = c->file.start + c->offset;
			toSend = ( c->file.length - c->offset > BUFF_SIZE)?
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
			} 
			else if (r == 0) {
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
#endif
		
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
#if 0
network_linuxsendfile_init(void) {
	p->write = network_linuxsendfile_write_chunkset;
}
#endif
