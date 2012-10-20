/**
 * the network chunk-API
 *
 *
 */

#include "chunk.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "log.h"
#include "connections.h"
#include <assert.h>

#define DBE 0

chunkqueue *chunkqueue_init(void) {
	chunkqueue *cq;

	cq = calloc(1, sizeof(*cq));

	cq->first = NULL;
	cq->last = NULL;

	cq->unused = NULL;

	return cq;
}

static chunk *chunk_init(void) {
	chunk *c;

	c = calloc(1, sizeof(*c));

	c->mem = buffer_init();
	c->file.name = buffer_init();
	c->file.fd = -1;
	c->file.mmap.start = MAP_FAILED;
	c->next = NULL;

	return c;
}

static void chunk_free(chunk *c) {
	if (!c) return;

	buffer_free(c->mem);
	buffer_free(c->file.name);

	free(c);
}

static void chunk_reset(chunk *c) {
	if (!c) return;

	buffer_reset(c->mem);

	if (c->file.is_temp && !buffer_is_empty(c->file.name)) {
		unlink(c->file.name->ptr);
	}

	buffer_reset(c->file.name);

	if(c->type == SMB_CHUNK){
		if (c->file.fd != -1) {
			smbc_close(c->file.fd);
			Cdbg(DBE,"close smb file-------------------------------->remote computer");
			c->file.fd = -1;
		}
	}
	else{
		if (c->file.fd != -1) {
			close(c->file.fd);
			Cdbg(DBE,"close usbdisk file-------------------------------->usb disk");
			c->file.fd = -1;
		}
		if (MAP_FAILED != c->file.mmap.start) {
			munmap(c->file.mmap.start, c->file.mmap.length);
			c->file.mmap.start = MAP_FAILED;
		}
	}
}


void chunkqueue_free(chunkqueue *cq) {
	chunk *c, *pc;

	if (!cq) return;

	for (c = cq->first; c; ) {
		pc = c;
		c = c->next;
		chunk_free(pc);
	}

	for (c = cq->unused; c; ) {
		pc = c;
		c = c->next;
		chunk_free(pc);
	}

	free(cq);
}

static chunk *chunkqueue_get_unused_chunk(chunkqueue *cq) {
	chunk *c;

	/* check if we have a unused chunk */
	if (!cq->unused) {
		c = chunk_init();
	} else {
		/* take the first element from the list (a stack) */
		c = cq->unused;
		cq->unused = c->next;
		c->next = NULL;
		cq->unused_chunks--;
	}

	return c;
}

static int chunkqueue_prepend_chunk(chunkqueue *cq, chunk *c) {
	c->next = cq->first;
	cq->first = c;

	if (cq->last == NULL) {
		cq->last = c;
	}

	return 0;
}

static int chunkqueue_append_chunk(chunkqueue *cq, chunk *c) {
	if (cq->last) {
		cq->last->next = c;
	}
	cq->last = c;

	if (cq->first == NULL) {
		cq->first = c;
	}

	return 0;
}

void chunkqueue_reset(chunkqueue *cq) {
	chunk *c;
	/* move everything to the unused queue */

	/* mark all read written */
	for (c = cq->first; c; c = c->next) {
		switch(c->type) {
		case MEM_CHUNK:
			c->offset = c->mem->used - 1;
			break;
		case FILE_CHUNK:
			c->offset = c->file.length;
			break;
		case SMB_CHUNK:
			c->offset = c->file.length;
			break;
		default:
			break;
		}
	}

	chunkqueue_remove_finished_chunks(cq);
	cq->bytes_in = 0;
	cq->bytes_out = 0;
}

int chunkqueue_append_smb_file(chunkqueue *cq, buffer *fn, off_t offset, off_t len) {
	chunk *c;

	if (len == 0) return 0;

	c = chunkqueue_get_unused_chunk(cq);

	c->type = SMB_CHUNK;

	buffer_copy_string_buffer(c->file.name, fn);
	c->file.start = offset;
	c->file.length = len;
	c->offset = 0;

	//- JerryLin add
	c->roffset = 0;
	c->open_thread = -1;
	c->listcount = 0;
	Cdbg(1,"chunkqueue_append_smb_file.....fn=[%s]", fn->ptr);

	chunkqueue_append_chunk(cq, c);

	return 0;
}

int chunkqueue_append_file(chunkqueue *cq, buffer *fn, off_t offset, off_t len) {
	chunk *c;
	
	if (len == 0) return 0;

	c = chunkqueue_get_unused_chunk(cq);

	c->type = FILE_CHUNK;

	buffer_copy_string_buffer(c->file.name, fn);
	c->file.start = offset;
	c->file.length = len;
	c->offset = 0;
	
	chunkqueue_append_chunk(cq, c);

	return 0;
}

int chunkqueue_append_buffer(chunkqueue *cq, buffer *mem) {
	chunk *c;

	if (mem->used == 0) return 0;

	c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_copy_string_buffer(c->mem, mem);

	chunkqueue_append_chunk(cq, c);

	return 0;
}

int chunkqueue_append_buffer_weak(chunkqueue *cq, buffer *mem) {
	chunk *c;

	c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	c->offset = 0;
	if (c->mem) buffer_free(c->mem);
	c->mem = mem;

	chunkqueue_append_chunk(cq, c);

	return 0;
}

int chunkqueue_prepend_buffer(chunkqueue *cq, buffer *mem) {
	chunk *c;

	if (mem->used == 0) return 0;

	c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_copy_string_buffer(c->mem, mem);

	chunkqueue_prepend_chunk(cq, c);

	return 0;
}


int chunkqueue_append_mem(chunkqueue *cq, const char * mem, size_t len) {
	chunk *c;

	if (len == 0) return 0;

	c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_copy_string_len(c->mem, mem, len - 1);

	chunkqueue_append_chunk(cq, c);

	return 0;
}

buffer * chunkqueue_get_prepend_buffer(chunkqueue *cq) {
	chunk *c;

	c = chunkqueue_get_unused_chunk(cq);

	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_reset(c->mem);

	chunkqueue_prepend_chunk(cq, c);

	return c->mem;
}

buffer *chunkqueue_get_append_buffer(chunkqueue *cq) {
	chunk *c;

	c = chunkqueue_get_unused_chunk(cq);

	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_reset(c->mem);

	chunkqueue_append_chunk(cq, c);

	return c->mem;
}

int chunkqueue_set_tempdirs(chunkqueue *cq, array *tempdirs) {
	if (!cq) return -1;

	cq->tempdirs = tempdirs;

	return 0;
}

chunk *chunkqueue_get_append_tempfile(chunkqueue *cq) {
	chunk *c;
	buffer *template = buffer_init_string("/var/tmp/lighttpd-upload-XXXXXX");

	c = chunkqueue_get_unused_chunk(cq);

	c->type = FILE_CHUNK;
	c->offset = 0;

	if (cq->tempdirs && cq->tempdirs->used) {
		size_t i;

		/* we have several tempdirs, only if all of them fail we jump out */

		for (i = 0; i < cq->tempdirs->used; i++) {
			data_string *ds = (data_string *)cq->tempdirs->data[i];

			buffer_copy_string_buffer(template, ds->value);
			BUFFER_APPEND_SLASH(template);
			buffer_append_string_len(template, CONST_STR_LEN("lighttpd-upload-XXXXXX"));

			if (-1 != (c->file.fd = mkstemp(template->ptr))) {
				/* only trigger the unlink if we created the temp-file successfully */
				c->file.is_temp = 1;
				break;
			}
		}
	} else {
		if (-1 != (c->file.fd = mkstemp(template->ptr))) {
			/* only trigger the unlink if we created the temp-file successfully */
			c->file.is_temp = 1;
		}
	}

	buffer_copy_string_buffer(c->file.name, template);
	c->file.length = 0;

	chunkqueue_append_chunk(cq, c);

	buffer_free(template);

	return c;
}

chunk *chunkqueue_get_append_smbfile(chunkqueue *cq, const char* smb_fname, connection* con, off_t chunk_len, off_t offset ) {
	chunk *c;
	buffer *template = buffer_init_string(smb_fname);

	c = chunkqueue_get_unused_chunk(cq);

	c->type = SMB_CHUNK;
	c->file.start = offset;// starting offset of file
	c->offset = chunk_len;
	c->file.length= 0;
	Cdbg(DBE,"c->file.start=%lli, c->file.length=%lli, c->offset=%lli", c->file.start, c->file.length, c->offset);

//	if(con->smb_info->cur_fd <= 0){
//		con->smb_info->cur_fd = smbc_wrapper_open(con, smb_fname,O_WRONLY|O_TRUNC, 0);
	if(con->cur_fd <= 0){
//		con->cur_fd = smbc_wrapper_open(con, smb_fname,O_WRONLY|O_TRUNC, 0);
		con->cur_fd = smbc_wrapper_open(con, smb_fname,O_WRONLY|O_APPEND, 0);

	//	if ( con->smb_info->cur_fd <= 0) {
		if ( con->cur_fd <= 0) {
			Cdbg(DBE,"smbc open failed err=%d, smb_fname=%s, ",errno,smb_fname);
		//	con->smb_info->cur_fd = smbc_wrapper_create(con, smb_fname,
			con->cur_fd = smbc_wrapper_create(con, smb_fname,
				  FILE_READ_DATA | FILE_WRITE_DATA,0);
			if(con->cur_fd <=0) Cdbg(DBE,"smbc create failed err=%d, smb_fname=%s, ",errno,smb_fname);
		}else{
#if 1
	struct stat st; 
	off_t partial_file_sz=0;
	int err;
				
	err = smbc_wrapper_stat(con, con->url.path->ptr, &st);
	if(err != -1){

		partial_file_sz = st.st_size;
		Cdbg(DBE, " partial size = ===========================================%lli  ",partial_file_sz);
		off_t t_end;
		if (-1 == (t_end = smbc_wrapper_lseek(con, con->cur_fd, partial_file_sz, SEEK_SET))) {
			Cdbg(DBE, " seeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeek failed");
		}else{
			Cdbg(DBE, "t_end =================================================%lli", t_end);
		}
	}else {
	   Cdbg(DBE,"======================================> get stat failed");
	}	
	// Set file pointer later
#endif

		}
	}
//	c->file.fd = con->smb_info->cur_fd;
	c->file.fd = con->cur_fd;
	Cdbg(DBE,"con =============================================================================%p",con);
	Cdbg(DBE,"smb_info ========================================================================%p",con->smb_info);
	
	Cdbg(DBE,"smbc open success fd=%d", c->file.fd);
	
	buffer_copy_string_buffer(c->file.name, template);
	Cdbg(DBE,"file name =%s",c->file.name->ptr);
	chunkqueue_append_chunk(cq, c);
	
	buffer_free(template);

	return c;
}

off_t chunkqueue_length(chunkqueue *cq) {
	off_t len = 0;
	chunk *c;
	for (c = cq->first; c; c = c->next) {
		switch (c->type) {
		case MEM_CHUNK:
			len += c->mem->used ? c->mem->used - 1 : 0;
			break;
		case FILE_CHUNK:
			len += c->file.length;
			break;
		case SMB_CHUNK:
			len += c->file.length;
			break;
		default:
		   Cdbg(DBE,"default");
			break;
		}
	}
	return len;
}

off_t chunkqueue_written(chunkqueue *cq) {
	off_t len = 0;
	chunk *c;

	for (c = cq->first; c; c = c->next) {
		switch (c->type) {
		case MEM_CHUNK:
		case FILE_CHUNK:
		case SMB_CHUNK:
			len += c->offset;
			break;
		default:
			break;
		}
	}

	return len;
}

int chunkqueue_is_empty(chunkqueue *cq) {
	return cq->first ? 0 : 1;
}

int chunkqueue_remove_finished_chunks(chunkqueue *cq) {
	chunk *c;

	for (c = cq->first; c; c = cq->first) {
		int is_finished = 0;

		switch (c->type) {
		case MEM_CHUNK:
			if (c->mem->used == 0 || (c->offset == (off_t)c->mem->used - 1)) is_finished = 1;
			break;
		case FILE_CHUNK:
			if (c->offset == c->file.length) is_finished = 1;
			break;
		case SMB_CHUNK:
			Cdbg(DBE,"c->offset =%lli, c->file.length=%lli",c->offset,c->file.length);
			if (c->offset == c->file.length) is_finished = 1;
			break;
		default:
			break;
		}

		if (!is_finished) break;

		chunk_reset(c);

		cq->first = c->next;
		if (c == cq->last) cq->last = NULL;

		/* keep at max 4 chunks in the 'unused'-cache */
		if (cq->unused_chunks > 4) {
			chunk_free(c);
		} else {
			c->next = cq->unused;
			cq->unused = c;
			cq->unused_chunks++;
		}
	}
	//Cdbg(1," ends");
	return 0;
}
