#ifndef _HTTP_CHUNK_H_
#define _HTTP_CHUNK_H_

#include "server.h"
#include <sys/types.h>

int http_chunk_append_mem(server *srv, connection *con, const char * mem, size_t len);
int http_chunk_append_buffer(server *srv, connection *con, buffer *mem);
int http_chunk_append_file(server *srv, connection *con, buffer *fn, off_t offset, off_t len);
off_t http_chunkqueue_length(server *srv, connection *con);
int http_chunk_append_smb_file(server *srv, connection *con, buffer *fn, off_t offset, off_t len);

#endif
