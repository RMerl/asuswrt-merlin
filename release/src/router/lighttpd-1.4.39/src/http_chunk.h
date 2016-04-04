#ifndef _HTTP_CHUNK_H_
#define _HTTP_CHUNK_H_

#include "server.h"
#include <sys/types.h>

void http_chunk_append_mem(server *srv, connection *con, const char * mem, size_t len); /* copies memory */
void http_chunk_append_buffer(server *srv, connection *con, buffer *mem); /* may reset "mem" */
void http_chunk_append_file(server *srv, connection *con, buffer *fn, off_t offset, off_t len); /* copies "fn" */
void http_chunk_close(server *srv, connection *con);

#endif
