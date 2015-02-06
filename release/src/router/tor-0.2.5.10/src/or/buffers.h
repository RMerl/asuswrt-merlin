/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file buffers.h
 * \brief Header file for buffers.c.
 **/

#ifndef TOR_BUFFERS_H
#define TOR_BUFFERS_H

#include "testsupport.h"

buf_t *buf_new(void);
buf_t *buf_new_with_capacity(size_t size);
size_t buf_get_default_chunk_size(const buf_t *buf);
void buf_free(buf_t *buf);
void buf_clear(buf_t *buf);
buf_t *buf_copy(const buf_t *buf);
void buf_shrink(buf_t *buf);
size_t buf_shrink_freelists(int free_all);
void buf_dump_freelist_sizes(int severity);

size_t buf_datalen(const buf_t *buf);
size_t buf_allocation(const buf_t *buf);
size_t buf_slack(const buf_t *buf);

uint32_t buf_get_oldest_chunk_timestamp(const buf_t *buf, uint32_t now);
size_t buf_get_total_allocation(void);

int read_to_buf(tor_socket_t s, size_t at_most, buf_t *buf, int *reached_eof,
                int *socket_error);
int read_to_buf_tls(tor_tls_t *tls, size_t at_most, buf_t *buf);

int flush_buf(tor_socket_t s, buf_t *buf, size_t sz, size_t *buf_flushlen);
int flush_buf_tls(tor_tls_t *tls, buf_t *buf, size_t sz, size_t *buf_flushlen);

int write_to_buf(const char *string, size_t string_len, buf_t *buf);
int write_to_buf_zlib(buf_t *buf, tor_zlib_state_t *state,
                      const char *data, size_t data_len, int done);
int move_buf_to_buf(buf_t *buf_out, buf_t *buf_in, size_t *buf_flushlen);
int fetch_from_buf(char *string, size_t string_len, buf_t *buf);
int fetch_var_cell_from_buf(buf_t *buf, var_cell_t **out, int linkproto);
int fetch_from_buf_http(buf_t *buf,
                        char **headers_out, size_t max_headerlen,
                        char **body_out, size_t *body_used, size_t max_bodylen,
                        int force_complete);
socks_request_t *socks_request_new(void);
void socks_request_free(socks_request_t *req);
int fetch_from_buf_socks(buf_t *buf, socks_request_t *req,
                         int log_sockstype, int safe_socks);
int fetch_from_buf_socks_client(buf_t *buf, int state, char **reason);
int fetch_from_buf_line(buf_t *buf, char *data_out, size_t *data_len);

int peek_buf_has_control0_command(buf_t *buf);

int fetch_ext_or_command_from_buf(buf_t *buf, ext_or_cmd_t **out);

#ifdef USE_BUFFEREVENTS
int fetch_var_cell_from_evbuffer(struct evbuffer *buf, var_cell_t **out,
                                 int linkproto);
int fetch_from_evbuffer_socks(struct evbuffer *buf, socks_request_t *req,
                              int log_sockstype, int safe_socks);
int fetch_from_evbuffer_socks_client(struct evbuffer *buf, int state,
                                     char **reason);
int fetch_from_evbuffer_http(struct evbuffer *buf,
                        char **headers_out, size_t max_headerlen,
                        char **body_out, size_t *body_used, size_t max_bodylen,
                        int force_complete);
int peek_evbuffer_has_control0_command(struct evbuffer *buf);
int write_to_evbuffer_zlib(struct evbuffer *buf, tor_zlib_state_t *state,
                           const char *data, size_t data_len,
                           int done);
int fetch_ext_or_command_from_evbuffer(struct evbuffer *buf,
                                       ext_or_cmd_t **out);
#endif

#ifdef USE_BUFFEREVENTS
#define generic_buffer_new() evbuffer_new()
#define generic_buffer_len(b) evbuffer_get_length((b))
#define generic_buffer_add(b,dat,len) evbuffer_add((b),(dat),(len))
#define generic_buffer_get(b,buf,buflen) evbuffer_remove((b),(buf),(buflen))
#define generic_buffer_clear(b) evbuffer_drain((b), evbuffer_get_length((b)))
#define generic_buffer_free(b) evbuffer_free((b))
#define generic_buffer_fetch_ext_or_cmd(b, out) \
  fetch_ext_or_command_from_evbuffer((b), (out))
#else
#define generic_buffer_new() buf_new()
#define generic_buffer_len(b) buf_datalen((b))
#define generic_buffer_add(b,dat,len) write_to_buf((dat),(len),(b))
#define generic_buffer_get(b,buf,buflen) fetch_from_buf((buf),(buflen),(b))
#define generic_buffer_clear(b) buf_clear((b))
#define generic_buffer_free(b) buf_free((b))
#define generic_buffer_fetch_ext_or_cmd(b, out) \
  fetch_ext_or_command_from_buf((b), (out))
#endif
int generic_buffer_set_to_copy(generic_buffer_t **output,
                               const generic_buffer_t *input);

void assert_buf_ok(buf_t *buf);

#ifdef BUFFERS_PRIVATE
STATIC int buf_find_string_offset(const buf_t *buf, const char *s, size_t n);
STATIC void buf_pullup(buf_t *buf, size_t bytes, int nulterminate);
void buf_get_first_chunk_data(const buf_t *buf, const char **cp, size_t *sz);

#define DEBUG_CHUNK_ALLOC
/** A single chunk on a buffer or in a freelist. */
typedef struct chunk_t {
  struct chunk_t *next; /**< The next chunk on the buffer or freelist. */
  size_t datalen; /**< The number of bytes stored in this chunk */
  size_t memlen; /**< The number of usable bytes of storage in <b>mem</b>. */
#ifdef DEBUG_CHUNK_ALLOC
  size_t DBG_alloc;
#endif
  char *data; /**< A pointer to the first byte of data stored in <b>mem</b>. */
  uint32_t inserted_time; /**< Timestamp in truncated ms since epoch
                           * when this chunk was inserted. */
  char mem[FLEXIBLE_ARRAY_MEMBER]; /**< The actual memory used for storage in
                * this chunk. */
} chunk_t;

/** Magic value for buf_t.magic, to catch pointer errors. */
#define BUFFER_MAGIC 0xB0FFF312u
/** A resizeable buffer, optimized for reading and writing. */
struct buf_t {
  uint32_t magic; /**< Magic cookie for debugging: Must be set to
                   *   BUFFER_MAGIC. */
  size_t datalen; /**< How many bytes is this buffer holding right now? */
  size_t default_chunk_size; /**< Don't allocate any chunks smaller than
                              * this for this buffer. */
  chunk_t *head; /**< First chunk in the list, or NULL for none. */
  chunk_t *tail; /**< Last chunk in the list, or NULL for none. */
};
#endif

#endif

