/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file buffers.c
 * \brief Implements a generic buffer interface.
 *
 * A buf_t is a (fairly) opaque byte-oriented FIFO that can read to or flush
 * from memory, sockets, file descriptors, TLS connections, or another buf_t.
 * Buffers are implemented as linked lists of memory chunks.
 *
 * All socket-backed and TLS-based connection_t objects have a pair of
 * buffers: one for incoming data, and one for outcoming data.  These are fed
 * and drained from functions in connection.c, trigged by events that are
 * monitored in main.c.
 *
 * This module has basic support for reading and writing on buf_t objects. It
 * also contains specialized functions for handling particular protocols
 * on a buf_t backend, including SOCKS (used in connection_edge.c), Tor cells
 * (used in connection_or.c and channeltls.c), HTTP (used in directory.c), and
 * line-oriented communication (used in control.c).
 **/
#define BUFFERS_PRIVATE
#include "or.h"
#include "addressmap.h"
#include "buffers.h"
#include "config.h"
#include "connection_edge.h"
#include "connection_or.h"
#include "control.h"
#include "reasons.h"
#include "ext_orport.h"
#include "util.h"
#include "torlog.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

//#define PARANOIA

#ifdef PARANOIA
/** Helper: If PARANOIA is defined, assert that the buffer in local variable
 * <b>buf</b> is well-formed. */
#define check() STMT_BEGIN assert_buf_ok(buf); STMT_END
#else
#define check() STMT_NIL
#endif

/* Implementation notes:
 *
 * After flirting with memmove, and dallying with ring-buffers, we're finally
 * getting up to speed with the 1970s and implementing buffers as a linked
 * list of small chunks.  Each buffer has such a list; data is removed from
 * the head of the list, and added at the tail.  The list is singly linked,
 * and the buffer keeps a pointer to the head and the tail.
 *
 * Every chunk, except the tail, contains at least one byte of data.  Data in
 * each chunk is contiguous.
 *
 * When you need to treat the first N characters on a buffer as a contiguous
 * string, use the buf_pullup function to make them so.  Don't do this more
 * than necessary.
 *
 * The major free Unix kernels have handled buffers like this since, like,
 * forever.
 */

static void socks_request_set_socks5_error(socks_request_t *req,
                              socks5_reply_status_t reason);

static int parse_socks(const char *data, size_t datalen, socks_request_t *req,
                       int log_sockstype, int safe_socks, ssize_t *drain_out,
                       size_t *want_length_out);
static int parse_socks_client(const uint8_t *data, size_t datalen,
                              int state, char **reason,
                              ssize_t *drain_out);

/* Chunk manipulation functions */

#define CHUNK_HEADER_LEN STRUCT_OFFSET(chunk_t, mem[0])

/* We leave this many NUL bytes at the end of the buffer. */
#define SENTINEL_LEN 4

/* Header size plus NUL bytes at the end */
#define CHUNK_OVERHEAD (CHUNK_HEADER_LEN + SENTINEL_LEN)

/** Return the number of bytes needed to allocate a chunk to hold
 * <b>memlen</b> bytes. */
#define CHUNK_ALLOC_SIZE(memlen) (CHUNK_OVERHEAD + (memlen))
/** Return the number of usable bytes in a chunk allocated with
 * malloc(<b>memlen</b>). */
#define CHUNK_SIZE_WITH_ALLOC(memlen) ((memlen) - CHUNK_OVERHEAD)

#define DEBUG_SENTINEL

#ifdef DEBUG_SENTINEL
#define DBG_S(s) s
#else
#define DBG_S(s) (void)0
#endif

#define CHUNK_SET_SENTINEL(chunk, alloclen) do {                        \
    uint8_t *a = (uint8_t*) &(chunk)->mem[(chunk)->memlen];             \
    DBG_S(uint8_t *b = &((uint8_t*)(chunk))[(alloclen)-SENTINEL_LEN]);  \
    DBG_S(tor_assert(a == b));                                          \
    memset(a,0,SENTINEL_LEN);                                           \
  } while (0)

/** Return the next character in <b>chunk</b> onto which data can be appended.
 * If the chunk is full, this might be off the end of chunk->mem. */
static inline char *
CHUNK_WRITE_PTR(chunk_t *chunk)
{
  return chunk->data + chunk->datalen;
}

/** Return the number of bytes that can be written onto <b>chunk</b> without
 * running out of space. */
static inline size_t
CHUNK_REMAINING_CAPACITY(const chunk_t *chunk)
{
  return (chunk->mem + chunk->memlen) - (chunk->data + chunk->datalen);
}

/** Move all bytes stored in <b>chunk</b> to the front of <b>chunk</b>->mem,
 * to free up space at the end. */
static inline void
chunk_repack(chunk_t *chunk)
{
  if (chunk->datalen && chunk->data != &chunk->mem[0]) {
    memmove(chunk->mem, chunk->data, chunk->datalen);
  }
  chunk->data = &chunk->mem[0];
}

/** Keep track of total size of allocated chunks for consistency asserts */
static size_t total_bytes_allocated_in_chunks = 0;
static void
buf_chunk_free_unchecked(chunk_t *chunk)
{
  if (!chunk)
    return;
#ifdef DEBUG_CHUNK_ALLOC
  tor_assert(CHUNK_ALLOC_SIZE(chunk->memlen) == chunk->DBG_alloc);
#endif
  tor_assert(total_bytes_allocated_in_chunks >=
             CHUNK_ALLOC_SIZE(chunk->memlen));
  total_bytes_allocated_in_chunks -= CHUNK_ALLOC_SIZE(chunk->memlen);
  tor_free(chunk);
}
static inline chunk_t *
chunk_new_with_alloc_size(size_t alloc)
{
  chunk_t *ch;
  ch = tor_malloc(alloc);
  ch->next = NULL;
  ch->datalen = 0;
#ifdef DEBUG_CHUNK_ALLOC
  ch->DBG_alloc = alloc;
#endif
  ch->memlen = CHUNK_SIZE_WITH_ALLOC(alloc);
  total_bytes_allocated_in_chunks += alloc;
  ch->data = &ch->mem[0];
  CHUNK_SET_SENTINEL(ch, alloc);
  return ch;
}

/** Expand <b>chunk</b> until it can hold <b>sz</b> bytes, and return a
 * new pointer to <b>chunk</b>.  Old pointers are no longer valid. */
static inline chunk_t *
chunk_grow(chunk_t *chunk, size_t sz)
{
  off_t offset;
  const size_t memlen_orig = chunk->memlen;
  const size_t orig_alloc = CHUNK_ALLOC_SIZE(memlen_orig);
  const size_t new_alloc = CHUNK_ALLOC_SIZE(sz);
  tor_assert(sz > chunk->memlen);
  offset = chunk->data - chunk->mem;
  chunk = tor_realloc(chunk, new_alloc);
  chunk->memlen = sz;
  chunk->data = chunk->mem + offset;
#ifdef DEBUG_CHUNK_ALLOC
  tor_assert(chunk->DBG_alloc == orig_alloc);
  chunk->DBG_alloc = new_alloc;
#endif
  total_bytes_allocated_in_chunks += new_alloc - orig_alloc;
  CHUNK_SET_SENTINEL(chunk, new_alloc);
  return chunk;
}

/** If a read onto the end of a chunk would be smaller than this number, then
 * just start a new chunk. */
#define MIN_READ_LEN 8
/** Every chunk should take up at least this many bytes. */
#define MIN_CHUNK_ALLOC 256
/** No chunk should take up more than this many bytes. */
#define MAX_CHUNK_ALLOC 65536

/** Return the allocation size we'd like to use to hold <b>target</b>
 * bytes. */
STATIC size_t
preferred_chunk_size(size_t target)
{
  tor_assert(target <= SIZE_T_CEILING - CHUNK_OVERHEAD);
  if (CHUNK_ALLOC_SIZE(target) >= MAX_CHUNK_ALLOC)
    return CHUNK_ALLOC_SIZE(target);
  size_t sz = MIN_CHUNK_ALLOC;
  while (CHUNK_SIZE_WITH_ALLOC(sz) < target) {
    sz <<= 1;
  }
  return sz;
}

/** Collapse data from the first N chunks from <b>buf</b> into buf->head,
 * growing it as necessary, until buf->head has the first <b>bytes</b> bytes
 * of data from the buffer, or until buf->head has all the data in <b>buf</b>.
 */
STATIC void
buf_pullup(buf_t *buf, size_t bytes)
{
  chunk_t *dest, *src;
  size_t capacity;
  if (!buf->head)
    return;

  check();
  if (buf->datalen < bytes)
    bytes = buf->datalen;

  capacity = bytes;
  if (buf->head->datalen >= bytes)
    return;

  if (buf->head->memlen >= capacity) {
    /* We don't need to grow the first chunk, but we might need to repack it.*/
    size_t needed = capacity - buf->head->datalen;
    if (CHUNK_REMAINING_CAPACITY(buf->head) < needed)
      chunk_repack(buf->head);
    tor_assert(CHUNK_REMAINING_CAPACITY(buf->head) >= needed);
  } else {
    chunk_t *newhead;
    size_t newsize;
    /* We need to grow the chunk. */
    chunk_repack(buf->head);
    newsize = CHUNK_SIZE_WITH_ALLOC(preferred_chunk_size(capacity));
    newhead = chunk_grow(buf->head, newsize);
    tor_assert(newhead->memlen >= capacity);
    if (newhead != buf->head) {
      if (buf->tail == buf->head)
        buf->tail = newhead;
      buf->head = newhead;
    }
  }

  dest = buf->head;
  while (dest->datalen < bytes) {
    size_t n = bytes - dest->datalen;
    src = dest->next;
    tor_assert(src);
    if (n >= src->datalen) {
      memcpy(CHUNK_WRITE_PTR(dest), src->data, src->datalen);
      dest->datalen += src->datalen;
      dest->next = src->next;
      if (buf->tail == src)
        buf->tail = dest;
      buf_chunk_free_unchecked(src);
    } else {
      memcpy(CHUNK_WRITE_PTR(dest), src->data, n);
      dest->datalen += n;
      src->data += n;
      src->datalen -= n;
      tor_assert(dest->datalen == bytes);
    }
  }

  check();
}

#ifdef TOR_UNIT_TESTS
void
buf_get_first_chunk_data(const buf_t *buf, const char **cp, size_t *sz)
{
  if (!buf || !buf->head) {
    *cp = NULL;
    *sz = 0;
  } else {
    *cp = buf->head->data;
    *sz = buf->head->datalen;
  }
}
#endif

/** Remove the first <b>n</b> bytes from buf. */
static inline void
buf_remove_from_front(buf_t *buf, size_t n)
{
  tor_assert(buf->datalen >= n);
  while (n) {
    tor_assert(buf->head);
    if (buf->head->datalen > n) {
      buf->head->datalen -= n;
      buf->head->data += n;
      buf->datalen -= n;
      return;
    } else {
      chunk_t *victim = buf->head;
      n -= victim->datalen;
      buf->datalen -= victim->datalen;
      buf->head = victim->next;
      if (buf->tail == victim)
        buf->tail = NULL;
      buf_chunk_free_unchecked(victim);
    }
  }
  check();
}

/** Create and return a new buf with default chunk capacity <b>size</b>.
 */
buf_t *
buf_new_with_capacity(size_t size)
{
  buf_t *b = buf_new();
  b->default_chunk_size = preferred_chunk_size(size);
  return b;
}

/** Allocate and return a new buffer with default capacity. */
buf_t *
buf_new(void)
{
  buf_t *buf = tor_malloc_zero(sizeof(buf_t));
  buf->magic = BUFFER_MAGIC;
  buf->default_chunk_size = 4096;
  return buf;
}

size_t
buf_get_default_chunk_size(const buf_t *buf)
{
  return buf->default_chunk_size;
}

/** Remove all data from <b>buf</b>. */
void
buf_clear(buf_t *buf)
{
  chunk_t *chunk, *next;
  buf->datalen = 0;
  for (chunk = buf->head; chunk; chunk = next) {
    next = chunk->next;
    buf_chunk_free_unchecked(chunk);
  }
  buf->head = buf->tail = NULL;
}

/** Return the number of bytes stored in <b>buf</b> */
MOCK_IMPL(size_t,
buf_datalen, (const buf_t *buf))
{
  return buf->datalen;
}

/** Return the total length of all chunks used in <b>buf</b>. */
size_t
buf_allocation(const buf_t *buf)
{
  size_t total = 0;
  const chunk_t *chunk;
  for (chunk = buf->head; chunk; chunk = chunk->next) {
    total += CHUNK_ALLOC_SIZE(chunk->memlen);
  }
  return total;
}

/** Return the number of bytes that can be added to <b>buf</b> without
 * performing any additional allocation. */
size_t
buf_slack(const buf_t *buf)
{
  if (!buf->tail)
    return 0;
  else
    return CHUNK_REMAINING_CAPACITY(buf->tail);
}

/** Release storage held by <b>buf</b>. */
void
buf_free(buf_t *buf)
{
  if (!buf)
    return;

  buf_clear(buf);
  buf->magic = 0xdeadbeef;
  tor_free(buf);
}

/** Return a new copy of <b>in_chunk</b> */
static chunk_t *
chunk_copy(const chunk_t *in_chunk)
{
  chunk_t *newch = tor_memdup(in_chunk, CHUNK_ALLOC_SIZE(in_chunk->memlen));
  total_bytes_allocated_in_chunks += CHUNK_ALLOC_SIZE(in_chunk->memlen);
#ifdef DEBUG_CHUNK_ALLOC
  newch->DBG_alloc = CHUNK_ALLOC_SIZE(in_chunk->memlen);
#endif
  newch->next = NULL;
  if (in_chunk->data) {
    off_t offset = in_chunk->data - in_chunk->mem;
    newch->data = newch->mem + offset;
  }
  return newch;
}

/** Return a new copy of <b>buf</b> */
buf_t *
buf_copy(const buf_t *buf)
{
  chunk_t *ch;
  buf_t *out = buf_new();
  out->default_chunk_size = buf->default_chunk_size;
  for (ch = buf->head; ch; ch = ch->next) {
    chunk_t *newch = chunk_copy(ch);
    if (out->tail) {
      out->tail->next = newch;
      out->tail = newch;
    } else {
      out->head = out->tail = newch;
    }
  }
  out->datalen = buf->datalen;
  return out;
}

/** Append a new chunk with enough capacity to hold <b>capacity</b> bytes to
 * the tail of <b>buf</b>.  If <b>capped</b>, don't allocate a chunk bigger
 * than MAX_CHUNK_ALLOC. */
static chunk_t *
buf_add_chunk_with_capacity(buf_t *buf, size_t capacity, int capped)
{
  chunk_t *chunk;

  if (CHUNK_ALLOC_SIZE(capacity) < buf->default_chunk_size) {
    chunk = chunk_new_with_alloc_size(buf->default_chunk_size);
  } else if (capped && CHUNK_ALLOC_SIZE(capacity) > MAX_CHUNK_ALLOC) {
    chunk = chunk_new_with_alloc_size(MAX_CHUNK_ALLOC);
  } else {
    chunk = chunk_new_with_alloc_size(preferred_chunk_size(capacity));
  }

  chunk->inserted_time = (uint32_t)monotime_coarse_absolute_msec();

  if (buf->tail) {
    tor_assert(buf->head);
    buf->tail->next = chunk;
    buf->tail = chunk;
  } else {
    tor_assert(!buf->head);
    buf->head = buf->tail = chunk;
  }
  check();
  return chunk;
}

/** Return the age of the oldest chunk in the buffer <b>buf</b>, in
 * milliseconds.  Requires the current monotonic time, in truncated msec,
 * as its input <b>now</b>.
 */
uint32_t
buf_get_oldest_chunk_timestamp(const buf_t *buf, uint32_t now)
{
  if (buf->head) {
    return now - buf->head->inserted_time;
  } else {
    return 0;
  }
}

size_t
buf_get_total_allocation(void)
{
  return total_bytes_allocated_in_chunks;
}

/** Read up to <b>at_most</b> bytes from the socket <b>fd</b> into
 * <b>chunk</b> (which must be on <b>buf</b>). If we get an EOF, set
 * *<b>reached_eof</b> to 1.  Return -1 on error, 0 on eof or blocking,
 * and the number of bytes read otherwise. */
static inline int
read_to_chunk(buf_t *buf, chunk_t *chunk, tor_socket_t fd, size_t at_most,
              int *reached_eof, int *socket_error)
{
  ssize_t read_result;
  if (at_most > CHUNK_REMAINING_CAPACITY(chunk))
    at_most = CHUNK_REMAINING_CAPACITY(chunk);
  read_result = tor_socket_recv(fd, CHUNK_WRITE_PTR(chunk), at_most, 0);

  if (read_result < 0) {
    int e = tor_socket_errno(fd);
    if (!ERRNO_IS_EAGAIN(e)) { /* it's a real error */
#ifdef _WIN32
      if (e == WSAENOBUFS)
        log_warn(LD_NET,"recv() failed: WSAENOBUFS. Not enough ram?");
#endif
      *socket_error = e;
      return -1;
    }
    return 0; /* would block. */
  } else if (read_result == 0) {
    log_debug(LD_NET,"Encountered eof on fd %d", (int)fd);
    *reached_eof = 1;
    return 0;
  } else { /* actually got bytes. */
    buf->datalen += read_result;
    chunk->datalen += read_result;
    log_debug(LD_NET,"Read %ld bytes. %d on inbuf.", (long)read_result,
              (int)buf->datalen);
    tor_assert(read_result < INT_MAX);
    return (int)read_result;
  }
}

/** As read_to_chunk(), but return (negative) error code on error, blocking,
 * or TLS, and the number of bytes read otherwise. */
static inline int
read_to_chunk_tls(buf_t *buf, chunk_t *chunk, tor_tls_t *tls,
                  size_t at_most)
{
  int read_result;

  tor_assert(CHUNK_REMAINING_CAPACITY(chunk) >= at_most);
  read_result = tor_tls_read(tls, CHUNK_WRITE_PTR(chunk), at_most);
  if (read_result < 0)
    return read_result;
  buf->datalen += read_result;
  chunk->datalen += read_result;
  return read_result;
}

/** Read from socket <b>s</b>, writing onto end of <b>buf</b>.  Read at most
 * <b>at_most</b> bytes, growing the buffer as necessary.  If recv() returns 0
 * (because of EOF), set *<b>reached_eof</b> to 1 and return 0. Return -1 on
 * error; else return the number of bytes read.
 */
/* XXXX indicate "read blocked" somehow? */
int
read_to_buf(tor_socket_t s, size_t at_most, buf_t *buf, int *reached_eof,
            int *socket_error)
{
  /* XXXX It's stupid to overload the return values for these functions:
   * "error status" and "number of bytes read" are not mutually exclusive.
   */
  int r = 0;
  size_t total_read = 0;

  check();
  tor_assert(reached_eof);
  tor_assert(SOCKET_OK(s));

  while (at_most > total_read) {
    size_t readlen = at_most - total_read;
    chunk_t *chunk;
    if (!buf->tail || CHUNK_REMAINING_CAPACITY(buf->tail) < MIN_READ_LEN) {
      chunk = buf_add_chunk_with_capacity(buf, at_most, 1);
      if (readlen > chunk->memlen)
        readlen = chunk->memlen;
    } else {
      size_t cap = CHUNK_REMAINING_CAPACITY(buf->tail);
      chunk = buf->tail;
      if (cap < readlen)
        readlen = cap;
    }

    r = read_to_chunk(buf, chunk, s, readlen, reached_eof, socket_error);
    check();
    if (r < 0)
      return r; /* Error */
    tor_assert(total_read+r < INT_MAX);
    total_read += r;
    if ((size_t)r < readlen) { /* eof, block, or no more to read. */
      break;
    }
  }
  return (int)total_read;
}

/** As read_to_buf, but reads from a TLS connection, and returns a TLS
 * status value rather than the number of bytes read.
 *
 * Using TLS on OR connections complicates matters in two ways.
 *
 * First, a TLS stream has its own read buffer independent of the
 * connection's read buffer.  (TLS needs to read an entire frame from
 * the network before it can decrypt any data.  Thus, trying to read 1
 * byte from TLS can require that several KB be read from the network
 * and decrypted.  The extra data is stored in TLS's decrypt buffer.)
 * Because the data hasn't been read by Tor (it's still inside the TLS),
 * this means that sometimes a connection "has stuff to read" even when
 * poll() didn't return POLLIN. The tor_tls_get_pending_bytes function is
 * used in connection.c to detect TLS objects with non-empty internal
 * buffers and read from them again.
 *
 * Second, the TLS stream's events do not correspond directly to network
 * events: sometimes, before a TLS stream can read, the network must be
 * ready to write -- or vice versa.
 */
int
read_to_buf_tls(tor_tls_t *tls, size_t at_most, buf_t *buf)
{
  int r = 0;
  size_t total_read = 0;

  check_no_tls_errors();

  check();

  while (at_most > total_read) {
    size_t readlen = at_most - total_read;
    chunk_t *chunk;
    if (!buf->tail || CHUNK_REMAINING_CAPACITY(buf->tail) < MIN_READ_LEN) {
      chunk = buf_add_chunk_with_capacity(buf, at_most, 1);
      if (readlen > chunk->memlen)
        readlen = chunk->memlen;
    } else {
      size_t cap = CHUNK_REMAINING_CAPACITY(buf->tail);
      chunk = buf->tail;
      if (cap < readlen)
        readlen = cap;
    }

    r = read_to_chunk_tls(buf, chunk, tls, readlen);
    check();
    if (r < 0)
      return r; /* Error */
    tor_assert(total_read+r < INT_MAX);
    total_read += r;
    if ((size_t)r < readlen) /* eof, block, or no more to read. */
      break;
  }
  return (int)total_read;
}

/** Helper for flush_buf(): try to write <b>sz</b> bytes from chunk
 * <b>chunk</b> of buffer <b>buf</b> onto socket <b>s</b>.  On success, deduct
 * the bytes written from *<b>buf_flushlen</b>.  Return the number of bytes
 * written on success, 0 on blocking, -1 on failure.
 */
static inline int
flush_chunk(tor_socket_t s, buf_t *buf, chunk_t *chunk, size_t sz,
            size_t *buf_flushlen)
{
  ssize_t write_result;

  if (sz > chunk->datalen)
    sz = chunk->datalen;
  write_result = tor_socket_send(s, chunk->data, sz, 0);

  if (write_result < 0) {
    int e = tor_socket_errno(s);
    if (!ERRNO_IS_EAGAIN(e)) { /* it's a real error */
#ifdef _WIN32
      if (e == WSAENOBUFS)
        log_warn(LD_NET,"write() failed: WSAENOBUFS. Not enough ram?");
#endif
      return -1;
    }
    log_debug(LD_NET,"write() would block, returning.");
    return 0;
  } else {
    *buf_flushlen -= write_result;
    buf_remove_from_front(buf, write_result);
    tor_assert(write_result < INT_MAX);
    return (int)write_result;
  }
}

/** Helper for flush_buf_tls(): try to write <b>sz</b> bytes from chunk
 * <b>chunk</b> of buffer <b>buf</b> onto socket <b>s</b>.  (Tries to write
 * more if there is a forced pending write size.)  On success, deduct the
 * bytes written from *<b>buf_flushlen</b>.  Return the number of bytes
 * written on success, and a TOR_TLS error code on failure or blocking.
 */
static inline int
flush_chunk_tls(tor_tls_t *tls, buf_t *buf, chunk_t *chunk,
                size_t sz, size_t *buf_flushlen)
{
  int r;
  size_t forced;
  char *data;

  forced = tor_tls_get_forced_write_size(tls);
  if (forced > sz)
    sz = forced;
  if (chunk) {
    data = chunk->data;
    tor_assert(sz <= chunk->datalen);
  } else {
    data = NULL;
    tor_assert(sz == 0);
  }
  r = tor_tls_write(tls, data, sz);
  if (r < 0)
    return r;
  if (*buf_flushlen > (size_t)r)
    *buf_flushlen -= r;
  else
    *buf_flushlen = 0;
  buf_remove_from_front(buf, r);
  log_debug(LD_NET,"flushed %d bytes, %d ready to flush, %d remain.",
            r,(int)*buf_flushlen,(int)buf->datalen);
  return r;
}

/** Write data from <b>buf</b> to the socket <b>s</b>.  Write at most
 * <b>sz</b> bytes, decrement *<b>buf_flushlen</b> by
 * the number of bytes actually written, and remove the written bytes
 * from the buffer.  Return the number of bytes written on success,
 * -1 on failure.  Return 0 if write() would block.
 */
int
flush_buf(tor_socket_t s, buf_t *buf, size_t sz, size_t *buf_flushlen)
{
  /* XXXX It's stupid to overload the return values for these functions:
   * "error status" and "number of bytes flushed" are not mutually exclusive.
   */
  int r;
  size_t flushed = 0;
  tor_assert(buf_flushlen);
  tor_assert(SOCKET_OK(s));
  tor_assert(*buf_flushlen <= buf->datalen);
  tor_assert(sz <= *buf_flushlen);

  check();
  while (sz) {
    size_t flushlen0;
    tor_assert(buf->head);
    if (buf->head->datalen >= sz)
      flushlen0 = sz;
    else
      flushlen0 = buf->head->datalen;

    r = flush_chunk(s, buf, buf->head, flushlen0, buf_flushlen);
    check();
    if (r < 0)
      return r;
    flushed += r;
    sz -= r;
    if (r == 0 || (size_t)r < flushlen0) /* can't flush any more now. */
      break;
  }
  tor_assert(flushed < INT_MAX);
  return (int)flushed;
}

/** As flush_buf(), but writes data to a TLS connection.  Can write more than
 * <b>flushlen</b> bytes.
 */
int
flush_buf_tls(tor_tls_t *tls, buf_t *buf, size_t flushlen,
              size_t *buf_flushlen)
{
  int r;
  size_t flushed = 0;
  ssize_t sz;
  tor_assert(buf_flushlen);
  tor_assert(*buf_flushlen <= buf->datalen);
  tor_assert(flushlen <= *buf_flushlen);
  sz = (ssize_t) flushlen;

  /* we want to let tls write even if flushlen is zero, because it might
   * have a partial record pending */
  check_no_tls_errors();

  check();
  do {
    size_t flushlen0;
    if (buf->head) {
      if ((ssize_t)buf->head->datalen >= sz)
        flushlen0 = sz;
      else
        flushlen0 = buf->head->datalen;
    } else {
      flushlen0 = 0;
    }

    r = flush_chunk_tls(tls, buf, buf->head, flushlen0, buf_flushlen);
    check();
    if (r < 0)
      return r;
    flushed += r;
    sz -= r;
    if (r == 0) /* Can't flush any more now. */
      break;
  } while (sz > 0);
  tor_assert(flushed < INT_MAX);
  return (int)flushed;
}

/** Append <b>string_len</b> bytes from <b>string</b> to the end of
 * <b>buf</b>.
 *
 * Return the new length of the buffer on success, -1 on failure.
 */
int
write_to_buf(const char *string, size_t string_len, buf_t *buf)
{
  if (!string_len)
    return (int)buf->datalen;
  check();

  while (string_len) {
    size_t copy;
    if (!buf->tail || !CHUNK_REMAINING_CAPACITY(buf->tail))
      buf_add_chunk_with_capacity(buf, string_len, 1);

    copy = CHUNK_REMAINING_CAPACITY(buf->tail);
    if (copy > string_len)
      copy = string_len;
    memcpy(CHUNK_WRITE_PTR(buf->tail), string, copy);
    string_len -= copy;
    string += copy;
    buf->datalen += copy;
    buf->tail->datalen += copy;
  }

  check();
  tor_assert(buf->datalen < INT_MAX);
  return (int)buf->datalen;
}

/** Helper: copy the first <b>string_len</b> bytes from <b>buf</b>
 * onto <b>string</b>.
 */
static inline void
peek_from_buf(char *string, size_t string_len, const buf_t *buf)
{
  chunk_t *chunk;

  tor_assert(string);
  /* make sure we don't ask for too much */
  tor_assert(string_len <= buf->datalen);
  /* assert_buf_ok(buf); */

  chunk = buf->head;
  while (string_len) {
    size_t copy = string_len;
    tor_assert(chunk);
    if (chunk->datalen < copy)
      copy = chunk->datalen;
    memcpy(string, chunk->data, copy);
    string_len -= copy;
    string += copy;
    chunk = chunk->next;
  }
}

/** Remove <b>string_len</b> bytes from the front of <b>buf</b>, and store
 * them into <b>string</b>.  Return the new buffer size.  <b>string_len</b>
 * must be \<= the number of bytes on the buffer.
 */
int
fetch_from_buf(char *string, size_t string_len, buf_t *buf)
{
  /* There must be string_len bytes in buf; write them onto string,
   * then memmove buf back (that is, remove them from buf).
   *
   * Return the number of bytes still on the buffer. */

  check();
  peek_from_buf(string, string_len, buf);
  buf_remove_from_front(buf, string_len);
  check();
  tor_assert(buf->datalen < INT_MAX);
  return (int)buf->datalen;
}

/** True iff the cell command <b>command</b> is one that implies a
 * variable-length cell in Tor link protocol <b>linkproto</b>. */
static inline int
cell_command_is_var_length(uint8_t command, int linkproto)
{
  /* If linkproto is v2 (2), CELL_VERSIONS is the only variable-length cells
   * work as implemented here. If it's 1, there are no variable-length cells.
   * Tor does not support other versions right now, and so can't negotiate
   * them.
   */
  switch (linkproto) {
  case 1:
    /* Link protocol version 1 has no variable-length cells. */
    return 0;
  case 2:
    /* In link protocol version 2, VERSIONS is the only variable-length cell */
    return command == CELL_VERSIONS;
  case 0:
  case 3:
  default:
    /* In link protocol version 3 and later, and in version "unknown",
     * commands 128 and higher indicate variable-length. VERSIONS is
     * grandfathered in. */
    return command == CELL_VERSIONS || command >= 128;
  }
}

/** Check <b>buf</b> for a variable-length cell according to the rules of link
 * protocol version <b>linkproto</b>.  If one is found, pull it off the buffer
 * and assign a newly allocated var_cell_t to *<b>out</b>, and return 1.
 * Return 0 if whatever is on the start of buf_t is not a variable-length
 * cell.  Return 1 and set *<b>out</b> to NULL if there seems to be the start
 * of a variable-length cell on <b>buf</b>, but the whole thing isn't there
 * yet. */
int
fetch_var_cell_from_buf(buf_t *buf, var_cell_t **out, int linkproto)
{
  char hdr[VAR_CELL_MAX_HEADER_SIZE];
  var_cell_t *result;
  uint8_t command;
  uint16_t length;
  const int wide_circ_ids = linkproto >= MIN_LINK_PROTO_FOR_WIDE_CIRC_IDS;
  const int circ_id_len = get_circ_id_size(wide_circ_ids);
  const unsigned header_len = get_var_cell_header_size(wide_circ_ids);
  check();
  *out = NULL;
  if (buf->datalen < header_len)
    return 0;
  peek_from_buf(hdr, header_len, buf);

  command = get_uint8(hdr + circ_id_len);
  if (!(cell_command_is_var_length(command, linkproto)))
    return 0;

  length = ntohs(get_uint16(hdr + circ_id_len + 1));
  if (buf->datalen < (size_t)(header_len+length))
    return 1;
  result = var_cell_new(length);
  result->command = command;
  if (wide_circ_ids)
    result->circ_id = ntohl(get_uint32(hdr));
  else
    result->circ_id = ntohs(get_uint16(hdr));

  buf_remove_from_front(buf, header_len);
  peek_from_buf((char*) result->payload, length, buf);
  buf_remove_from_front(buf, length);
  check();

  *out = result;
  return 1;
}

/** Move up to *<b>buf_flushlen</b> bytes from <b>buf_in</b> to
 * <b>buf_out</b>, and modify *<b>buf_flushlen</b> appropriately.
 * Return the number of bytes actually copied.
 */
int
move_buf_to_buf(buf_t *buf_out, buf_t *buf_in, size_t *buf_flushlen)
{
  /* We can do way better here, but this doesn't turn up in any profiles. */
  char b[4096];
  size_t cp, len;
  len = *buf_flushlen;
  if (len > buf_in->datalen)
    len = buf_in->datalen;

  cp = len; /* Remember the number of bytes we intend to copy. */
  tor_assert(cp < INT_MAX);
  while (len) {
    /* This isn't the most efficient implementation one could imagine, since
     * it does two copies instead of 1, but I kinda doubt that this will be
     * critical path. */
    size_t n = len > sizeof(b) ? sizeof(b) : len;
    fetch_from_buf(b, n, buf_in);
    write_to_buf(b, n, buf_out);
    len -= n;
  }
  *buf_flushlen -= cp;
  return (int)cp;
}

/** Internal structure: represents a position in a buffer. */
typedef struct buf_pos_t {
  const chunk_t *chunk; /**< Which chunk are we pointing to? */
  int pos;/**< Which character inside the chunk's data are we pointing to? */
  size_t chunk_pos; /**< Total length of all previous chunks. */
} buf_pos_t;

/** Initialize <b>out</b> to point to the first character of <b>buf</b>.*/
static void
buf_pos_init(const buf_t *buf, buf_pos_t *out)
{
  out->chunk = buf->head;
  out->pos = 0;
  out->chunk_pos = 0;
}

/** Advance <b>out</b> to the first appearance of <b>ch</b> at the current
 * position of <b>out</b>, or later.  Return -1 if no instances are found;
 * otherwise returns the absolute position of the character. */
static off_t
buf_find_pos_of_char(char ch, buf_pos_t *out)
{
  const chunk_t *chunk;
  int pos;
  tor_assert(out);
  if (out->chunk) {
    if (out->chunk->datalen) {
      tor_assert(out->pos < (off_t)out->chunk->datalen);
    } else {
      tor_assert(out->pos == 0);
    }
  }
  pos = out->pos;
  for (chunk = out->chunk; chunk; chunk = chunk->next) {
    char *cp = memchr(chunk->data+pos, ch, chunk->datalen - pos);
    if (cp) {
      out->chunk = chunk;
      tor_assert(cp - chunk->data < INT_MAX);
      out->pos = (int)(cp - chunk->data);
      return out->chunk_pos + out->pos;
    } else {
      out->chunk_pos += chunk->datalen;
      pos = 0;
    }
  }
  return -1;
}

/** Advance <b>pos</b> by a single character, if there are any more characters
 * in the buffer.  Returns 0 on success, -1 on failure. */
static inline int
buf_pos_inc(buf_pos_t *pos)
{
  ++pos->pos;
  if (pos->pos == (off_t)pos->chunk->datalen) {
    if (!pos->chunk->next)
      return -1;
    pos->chunk_pos += pos->chunk->datalen;
    pos->chunk = pos->chunk->next;
    pos->pos = 0;
  }
  return 0;
}

/** Return true iff the <b>n</b>-character string in <b>s</b> appears
 * (verbatim) at <b>pos</b>. */
static int
buf_matches_at_pos(const buf_pos_t *pos, const char *s, size_t n)
{
  buf_pos_t p;
  if (!n)
    return 1;

  memcpy(&p, pos, sizeof(p));

  while (1) {
    char ch = p.chunk->data[p.pos];
    if (ch != *s)
      return 0;
    ++s;
    /* If we're out of characters that don't match, we match.  Check this
     * _before_ we test incrementing pos, in case we're at the end of the
     * string. */
    if (--n == 0)
      return 1;
    if (buf_pos_inc(&p)<0)
      return 0;
  }
}

/** Return the first position in <b>buf</b> at which the <b>n</b>-character
 * string <b>s</b> occurs, or -1 if it does not occur. */
STATIC int
buf_find_string_offset(const buf_t *buf, const char *s, size_t n)
{
  buf_pos_t pos;
  buf_pos_init(buf, &pos);
  while (buf_find_pos_of_char(*s, &pos) >= 0) {
    if (buf_matches_at_pos(&pos, s, n)) {
      tor_assert(pos.chunk_pos + pos.pos < INT_MAX);
      return (int)(pos.chunk_pos + pos.pos);
    } else {
      if (buf_pos_inc(&pos)<0)
        return -1;
    }
  }
  return -1;
}

/** There is a (possibly incomplete) http statement on <b>buf</b>, of the
 * form "\%s\\r\\n\\r\\n\%s", headers, body. (body may contain NULs.)
 * If a) the headers include a Content-Length field and all bytes in
 * the body are present, or b) there's no Content-Length field and
 * all headers are present, then:
 *
 *  - strdup headers into <b>*headers_out</b>, and NUL-terminate it.
 *  - memdup body into <b>*body_out</b>, and NUL-terminate it.
 *  - Then remove them from <b>buf</b>, and return 1.
 *
 *  - If headers or body is NULL, discard that part of the buf.
 *  - If a headers or body doesn't fit in the arg, return -1.
 *  (We ensure that the headers or body don't exceed max len,
 *   _even if_ we're planning to discard them.)
 *  - If force_complete is true, then succeed even if not all of the
 *    content has arrived.
 *
 * Else, change nothing and return 0.
 */
int
fetch_from_buf_http(buf_t *buf,
                    char **headers_out, size_t max_headerlen,
                    char **body_out, size_t *body_used, size_t max_bodylen,
                    int force_complete)
{
  char *headers, *p;
  size_t headerlen, bodylen, contentlen;
  int crlf_offset;

  check();
  if (!buf->head)
    return 0;

  crlf_offset = buf_find_string_offset(buf, "\r\n\r\n", 4);
  if (crlf_offset > (int)max_headerlen ||
      (crlf_offset < 0 && buf->datalen > max_headerlen)) {
    log_debug(LD_HTTP,"headers too long.");
    return -1;
  } else if (crlf_offset < 0) {
    log_debug(LD_HTTP,"headers not all here yet.");
    return 0;
  }
  /* Okay, we have a full header.  Make sure it all appears in the first
   * chunk. */
  if ((int)buf->head->datalen < crlf_offset + 4)
    buf_pullup(buf, crlf_offset+4);
  headerlen = crlf_offset + 4;

  headers = buf->head->data;
  bodylen = buf->datalen - headerlen;
  log_debug(LD_HTTP,"headerlen %d, bodylen %d.", (int)headerlen, (int)bodylen);

  if (max_headerlen <= headerlen) {
    log_warn(LD_HTTP,"headerlen %d larger than %d. Failing.",
             (int)headerlen, (int)max_headerlen-1);
    return -1;
  }
  if (max_bodylen <= bodylen) {
    log_warn(LD_HTTP,"bodylen %d larger than %d. Failing.",
             (int)bodylen, (int)max_bodylen-1);
    return -1;
  }

#define CONTENT_LENGTH "\r\nContent-Length: "
  p = (char*) tor_memstr(headers, headerlen, CONTENT_LENGTH);
  if (p) {
    int i;
    i = atoi(p+strlen(CONTENT_LENGTH));
    if (i < 0) {
      log_warn(LD_PROTOCOL, "Content-Length is less than zero; it looks like "
               "someone is trying to crash us.");
      return -1;
    }
    contentlen = i;
    /* if content-length is malformed, then our body length is 0. fine. */
    log_debug(LD_HTTP,"Got a contentlen of %d.",(int)contentlen);
    if (bodylen < contentlen) {
      if (!force_complete) {
        log_debug(LD_HTTP,"body not all here yet.");
        return 0; /* not all there yet */
      }
    }
    if (bodylen > contentlen) {
      bodylen = contentlen;
      log_debug(LD_HTTP,"bodylen reduced to %d.",(int)bodylen);
    }
  }
  /* all happy. copy into the appropriate places, and return 1 */
  if (headers_out) {
    *headers_out = tor_malloc(headerlen+1);
    fetch_from_buf(*headers_out, headerlen, buf);
    (*headers_out)[headerlen] = 0; /* NUL terminate it */
  }
  if (body_out) {
    tor_assert(body_used);
    *body_used = bodylen;
    *body_out = tor_malloc(bodylen+1);
    fetch_from_buf(*body_out, bodylen, buf);
    (*body_out)[bodylen] = 0; /* NUL terminate it */
  }
  check();
  return 1;
}

/**
 * Wait this many seconds before warning the user about using SOCKS unsafely
 * again (requires that WarnUnsafeSocks is turned on). */
#define SOCKS_WARN_INTERVAL 5

/** Warn that the user application has made an unsafe socks request using
 * protocol <b>socks_protocol</b> on port <b>port</b>.  Don't warn more than
 * once per SOCKS_WARN_INTERVAL, unless <b>safe_socks</b> is set. */
static void
log_unsafe_socks_warning(int socks_protocol, const char *address,
                         uint16_t port, int safe_socks)
{
  static ratelim_t socks_ratelim = RATELIM_INIT(SOCKS_WARN_INTERVAL);

  const or_options_t *options = get_options();
  if (! options->WarnUnsafeSocks)
    return;
  if (safe_socks) {
    log_fn_ratelim(&socks_ratelim, LOG_WARN, LD_APP,
             "Your application (using socks%d to port %d) is giving "
             "Tor only an IP address. Applications that do DNS resolves "
             "themselves may leak information. Consider using Socks4A "
             "(e.g. via privoxy or socat) instead. For more information, "
             "please see https://wiki.torproject.org/TheOnionRouter/"
             "TorFAQ#SOCKSAndDNS.%s",
             socks_protocol,
             (int)port,
             safe_socks ? " Rejecting." : "");
  }
  control_event_client_status(LOG_WARN,
                              "DANGEROUS_SOCKS PROTOCOL=SOCKS%d ADDRESS=%s:%d",
                              socks_protocol, address, (int)port);
}

/** Do not attempt to parse socks messages longer than this.  This value is
 * actually significantly higher than the longest possible socks message. */
#define MAX_SOCKS_MESSAGE_LEN 512

/** Return a new socks_request_t. */
socks_request_t *
socks_request_new(void)
{
  return tor_malloc_zero(sizeof(socks_request_t));
}

/** Free all storage held in the socks_request_t <b>req</b>. */
void
socks_request_free(socks_request_t *req)
{
  if (!req)
    return;
  if (req->username) {
    memwipe(req->username, 0x10, req->usernamelen);
    tor_free(req->username);
  }
  if (req->password) {
    memwipe(req->password, 0x04, req->passwordlen);
    tor_free(req->password);
  }
  memwipe(req, 0xCC, sizeof(socks_request_t));
  tor_free(req);
}

/** There is a (possibly incomplete) socks handshake on <b>buf</b>, of one
 * of the forms
 *  - socks4: "socksheader username\\0"
 *  - socks4a: "socksheader username\\0 destaddr\\0"
 *  - socks5 phase one: "version #methods methods"
 *  - socks5 phase two: "version command 0 addresstype..."
 * If it's a complete and valid handshake, and destaddr fits in
 *   MAX_SOCKS_ADDR_LEN bytes, then pull the handshake off the buf,
 *   assign to <b>req</b>, and return 1.
 *
 * If it's invalid or too big, return -1.
 *
 * Else it's not all there yet, leave buf alone and return 0.
 *
 * If you want to specify the socks reply, write it into <b>req->reply</b>
 *   and set <b>req->replylen</b>, else leave <b>req->replylen</b> alone.
 *
 * If <b>log_sockstype</b> is non-zero, then do a notice-level log of whether
 * the connection is possibly leaking DNS requests locally or not.
 *
 * If <b>safe_socks</b> is true, then reject unsafe socks protocols.
 *
 * If returning 0 or -1, <b>req->address</b> and <b>req->port</b> are
 * undefined.
 */
int
fetch_from_buf_socks(buf_t *buf, socks_request_t *req,
                     int log_sockstype, int safe_socks)
{
  int res;
  ssize_t n_drain;
  size_t want_length = 128;

  if (buf->datalen < 2) /* version and another byte */
    return 0;

  do {
    n_drain = 0;
    buf_pullup(buf, want_length);
    tor_assert(buf->head && buf->head->datalen >= 2);
    want_length = 0;

    res = parse_socks(buf->head->data, buf->head->datalen, req, log_sockstype,
                      safe_socks, &n_drain, &want_length);

    if (n_drain < 0)
      buf_clear(buf);
    else if (n_drain > 0)
      buf_remove_from_front(buf, n_drain);

  } while (res == 0 && buf->head && want_length < buf->datalen &&
           buf->datalen >= 2);

  return res;
}

/** The size of the header of an Extended ORPort message: 2 bytes for
 *  COMMAND, 2 bytes for BODYLEN */
#define EXT_OR_CMD_HEADER_SIZE 4

/** Read <b>buf</b>, which should contain an Extended ORPort message
 *  from a transport proxy. If well-formed, create and populate
 *  <b>out</b> with the Extended ORport message. Return 0 if the
 *  buffer was incomplete, 1 if it was well-formed and -1 if we
 *  encountered an error while parsing it.  */
int
fetch_ext_or_command_from_buf(buf_t *buf, ext_or_cmd_t **out)
{
  char hdr[EXT_OR_CMD_HEADER_SIZE];
  uint16_t len;

  check();
  if (buf->datalen < EXT_OR_CMD_HEADER_SIZE)
    return 0;
  peek_from_buf(hdr, sizeof(hdr), buf);
  len = ntohs(get_uint16(hdr+2));
  if (buf->datalen < (unsigned)len + EXT_OR_CMD_HEADER_SIZE)
    return 0;
  *out = ext_or_cmd_new(len);
  (*out)->cmd = ntohs(get_uint16(hdr));
  (*out)->len = len;
  buf_remove_from_front(buf, EXT_OR_CMD_HEADER_SIZE);
  fetch_from_buf((*out)->body, len, buf);
  return 1;
}

/** Create a SOCKS5 reply message with <b>reason</b> in its REP field and
 * have Tor send it as error response to <b>req</b>.
 */
static void
socks_request_set_socks5_error(socks_request_t *req,
                  socks5_reply_status_t reason)
{
   req->replylen = 10;
   memset(req->reply,0,10);

   req->reply[0] = 0x05;   // VER field.
   req->reply[1] = reason; // REP field.
   req->reply[3] = 0x01;   // ATYP field.
}

/** Implementation helper to implement fetch_from_*_socks.  Instead of looking
 * at a buffer's contents, we look at the <b>datalen</b> bytes of data in
 * <b>data</b>. Instead of removing data from the buffer, we set
 * <b>drain_out</b> to the amount of data that should be removed (or -1 if the
 * buffer should be cleared).  Instead of pulling more data into the first
 * chunk of the buffer, we set *<b>want_length_out</b> to the number of bytes
 * we'd like to see in the input buffer, if they're available. */
static int
parse_socks(const char *data, size_t datalen, socks_request_t *req,
            int log_sockstype, int safe_socks, ssize_t *drain_out,
            size_t *want_length_out)
{
  unsigned int len;
  char tmpbuf[TOR_ADDR_BUF_LEN+1];
  tor_addr_t destaddr;
  uint32_t destip;
  uint8_t socksver;
  char *next, *startaddr;
  unsigned char usernamelen, passlen;
  struct in_addr in;

  if (datalen < 2) {
    /* We always need at least 2 bytes. */
    *want_length_out = 2;
    return 0;
  }

  if (req->socks_version == 5 && !req->got_auth) {
    /* See if we have received authentication.  Strictly speaking, we should
       also check whether we actually negotiated username/password
       authentication.  But some broken clients will send us authentication
       even if we negotiated SOCKS_NO_AUTH. */
    if (*data == 1) { /* username/pass version 1 */
      /* Format is: authversion [1 byte] == 1
                    usernamelen [1 byte]
                    username    [usernamelen bytes]
                    passlen     [1 byte]
                    password    [passlen bytes] */
      usernamelen = (unsigned char)*(data + 1);
      if (datalen < 2u + usernamelen + 1u) {
        *want_length_out = 2u + usernamelen + 1u;
        return 0;
      }
      passlen = (unsigned char)*(data + 2u + usernamelen);
      if (datalen < 2u + usernamelen + 1u + passlen) {
        *want_length_out = 2u + usernamelen + 1u + passlen;
        return 0;
      }
      req->replylen = 2; /* 2 bytes of response */
      req->reply[0] = 1; /* authversion == 1 */
      req->reply[1] = 0; /* authentication successful */
      log_debug(LD_APP,
               "socks5: Accepted username/password without checking.");
      if (usernamelen) {
        req->username = tor_memdup(data+2u, usernamelen);
        req->usernamelen = usernamelen;
      }
      if (passlen) {
        req->password = tor_memdup(data+3u+usernamelen, passlen);
        req->passwordlen = passlen;
      }
      *drain_out = 2u + usernamelen + 1u + passlen;
      req->got_auth = 1;
      *want_length_out = 7; /* Minimal socks5 command. */
      return 0;
    } else if (req->auth_type == SOCKS_USER_PASS) {
      /* unknown version byte */
      log_warn(LD_APP, "Socks5 username/password version %d not recognized; "
               "rejecting.", (int)*data);
      return -1;
    }
  }

  socksver = *data;

  switch (socksver) { /* which version of socks? */
    case 5: /* socks5 */

      if (req->socks_version != 5) { /* we need to negotiate a method */
        unsigned char nummethods = (unsigned char)*(data+1);
        int have_user_pass, have_no_auth;
        int r=0;
        tor_assert(!req->socks_version);
        if (datalen < 2u+nummethods) {
          *want_length_out = 2u+nummethods;
          return 0;
        }
        if (!nummethods)
          return -1;
        req->replylen = 2; /* 2 bytes of response */
        req->reply[0] = 5; /* socks5 reply */
        have_user_pass = (memchr(data+2, SOCKS_USER_PASS, nummethods) !=NULL);
        have_no_auth   = (memchr(data+2, SOCKS_NO_AUTH,   nummethods) !=NULL);
        if (have_user_pass && !(have_no_auth && req->socks_prefer_no_auth)) {
          req->auth_type = SOCKS_USER_PASS;
          req->reply[1] = SOCKS_USER_PASS; /* tell client to use "user/pass"
                                              auth method */
          req->socks_version = 5; /* remember we've already negotiated auth */
          log_debug(LD_APP,"socks5: accepted method 2 (username/password)");
          r=0;
        } else if (have_no_auth) {
          req->reply[1] = SOCKS_NO_AUTH; /* tell client to use "none" auth
                                            method */
          req->socks_version = 5; /* remember we've already negotiated auth */
          log_debug(LD_APP,"socks5: accepted method 0 (no authentication)");
          r=0;
        } else {
          log_warn(LD_APP,
                    "socks5: offered methods don't include 'no auth' or "
                    "username/password. Rejecting.");
          req->reply[1] = '\xFF'; /* reject all methods */
          r=-1;
        }
        /* Remove packet from buf. Some SOCKS clients will have sent extra
         * junk at this point; let's hope it's an authentication message. */
        *drain_out = 2u + nummethods;

        return r;
      }
      if (req->auth_type != SOCKS_NO_AUTH && !req->got_auth) {
        log_warn(LD_APP,
                 "socks5: negotiated authentication, but none provided");
        return -1;
      }
      /* we know the method; read in the request */
      log_debug(LD_APP,"socks5: checking request");
      if (datalen < 7) {/* basic info plus >=1 for addr plus 2 for port */
        *want_length_out = 7;
        return 0; /* not yet */
      }
      req->command = (unsigned char) *(data+1);
      if (req->command != SOCKS_COMMAND_CONNECT &&
          req->command != SOCKS_COMMAND_RESOLVE &&
          req->command != SOCKS_COMMAND_RESOLVE_PTR) {
        /* not a connect or resolve or a resolve_ptr? we don't support it. */
        socks_request_set_socks5_error(req,SOCKS5_COMMAND_NOT_SUPPORTED);

        log_warn(LD_APP,"socks5: command %d not recognized. Rejecting.",
                 req->command);
        return -1;
      }
      switch (*(data+3)) { /* address type */
        case 1: /* IPv4 address */
        case 4: /* IPv6 address */ {
          const int is_v6 = *(data+3) == 4;
          const unsigned addrlen = is_v6 ? 16 : 4;
          log_debug(LD_APP,"socks5: ipv4 address type");
          if (datalen < 6+addrlen) {/* ip/port there? */
            *want_length_out = 6+addrlen;
            return 0; /* not yet */
          }

          if (is_v6)
            tor_addr_from_ipv6_bytes(&destaddr, data+4);
          else
            tor_addr_from_ipv4n(&destaddr, get_uint32(data+4));

          tor_addr_to_str(tmpbuf, &destaddr, sizeof(tmpbuf), 1);

          if (strlen(tmpbuf)+1 > MAX_SOCKS_ADDR_LEN) {
            socks_request_set_socks5_error(req, SOCKS5_GENERAL_ERROR);
            log_warn(LD_APP,
                     "socks5 IP takes %d bytes, which doesn't fit in %d. "
                     "Rejecting.",
                     (int)strlen(tmpbuf)+1,(int)MAX_SOCKS_ADDR_LEN);
            return -1;
          }
          strlcpy(req->address,tmpbuf,sizeof(req->address));
          req->port = ntohs(get_uint16(data+4+addrlen));
          *drain_out = 6+addrlen;
          if (req->command != SOCKS_COMMAND_RESOLVE_PTR &&
              !addressmap_have_mapping(req->address,0)) {
            log_unsafe_socks_warning(5, req->address, req->port, safe_socks);
            if (safe_socks) {
              socks_request_set_socks5_error(req, SOCKS5_NOT_ALLOWED);
              return -1;
            }
          }
          return 1;
        }
        case 3: /* fqdn */
          log_debug(LD_APP,"socks5: fqdn address type");
          if (req->command == SOCKS_COMMAND_RESOLVE_PTR) {
            socks_request_set_socks5_error(req,
                                           SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED);
            log_warn(LD_APP, "socks5 received RESOLVE_PTR command with "
                     "hostname type. Rejecting.");
            return -1;
          }
          len = (unsigned char)*(data+4);
          if (datalen < 7+len) { /* addr/port there? */
            *want_length_out = 7+len;
            return 0; /* not yet */
          }
          if (len+1 > MAX_SOCKS_ADDR_LEN) {
            socks_request_set_socks5_error(req, SOCKS5_GENERAL_ERROR);
            log_warn(LD_APP,
                     "socks5 hostname is %d bytes, which doesn't fit in "
                     "%d. Rejecting.", len+1,MAX_SOCKS_ADDR_LEN);
            return -1;
          }
          memcpy(req->address,data+5,len);
          req->address[len] = 0;
          req->port = ntohs(get_uint16(data+5+len));
          *drain_out = 5+len+2;

          if (string_is_valid_ipv4_address(req->address) ||
              string_is_valid_ipv6_address(req->address)) {
            log_unsafe_socks_warning(5,req->address,req->port,safe_socks);

            if (safe_socks) {
              socks_request_set_socks5_error(req, SOCKS5_NOT_ALLOWED);
              return -1;
            }
          } else if (!string_is_valid_hostname(req->address)) {
            socks_request_set_socks5_error(req, SOCKS5_GENERAL_ERROR);

            log_warn(LD_PROTOCOL,
                     "Your application (using socks5 to port %d) gave Tor "
                     "a malformed hostname: %s. Rejecting the connection.",
                     req->port, escaped_safe_str_client(req->address));
            return -1;
          }
          if (log_sockstype)
            log_notice(LD_APP,
                  "Your application (using socks5 to port %d) instructed "
                  "Tor to take care of the DNS resolution itself if "
                  "necessary. This is good.", req->port);
          return 1;
        default: /* unsupported */
          socks_request_set_socks5_error(req,
                                         SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED);
          log_warn(LD_APP,"socks5: unsupported address type %d. Rejecting.",
                   (int) *(data+3));
          return -1;
      }
      tor_assert(0);
    case 4: { /* socks4 */
      enum {socks4, socks4a} socks4_prot = socks4a;
      const char *authstart, *authend;
      /* http://ss5.sourceforge.net/socks4.protocol.txt */
      /* http://ss5.sourceforge.net/socks4A.protocol.txt */

      req->socks_version = 4;
      if (datalen < SOCKS4_NETWORK_LEN) {/* basic info available? */
        *want_length_out = SOCKS4_NETWORK_LEN;
        return 0; /* not yet */
      }
      // buf_pullup(buf, 1280);
      req->command = (unsigned char) *(data+1);
      if (req->command != SOCKS_COMMAND_CONNECT &&
          req->command != SOCKS_COMMAND_RESOLVE) {
        /* not a connect or resolve? we don't support it. (No resolve_ptr with
         * socks4.) */
        log_warn(LD_APP,"socks4: command %d not recognized. Rejecting.",
                 req->command);
        return -1;
      }

      req->port = ntohs(get_uint16(data+2));
      destip = ntohl(get_uint32(data+4));
      if ((!req->port && req->command!=SOCKS_COMMAND_RESOLVE) || !destip) {
        log_warn(LD_APP,"socks4: Port or DestIP is zero. Rejecting.");
        return -1;
      }
      if (destip >> 8) {
        log_debug(LD_APP,"socks4: destip not in form 0.0.0.x.");
        in.s_addr = htonl(destip);
        tor_inet_ntoa(&in,tmpbuf,sizeof(tmpbuf));
        if (strlen(tmpbuf)+1 > MAX_SOCKS_ADDR_LEN) {
          log_debug(LD_APP,"socks4 addr (%d bytes) too long. Rejecting.",
                    (int)strlen(tmpbuf));
          return -1;
        }
        log_debug(LD_APP,
                  "socks4: successfully read destip (%s)",
                  safe_str_client(tmpbuf));
        socks4_prot = socks4;
      }

      authstart = data + SOCKS4_NETWORK_LEN;
      next = memchr(authstart, 0,
                    datalen-SOCKS4_NETWORK_LEN);
      if (!next) {
        if (datalen >= 1024) {
          log_debug(LD_APP, "Socks4 user name too long; rejecting.");
          return -1;
        }
        log_debug(LD_APP,"socks4: Username not here yet.");
        *want_length_out = datalen+1024; /* More than we need, but safe */
        return 0;
      }
      authend = next;
      tor_assert(next < data+datalen);

      startaddr = NULL;
      if (socks4_prot != socks4a &&
          !addressmap_have_mapping(tmpbuf,0)) {
        log_unsafe_socks_warning(4, tmpbuf, req->port, safe_socks);

        if (safe_socks)
          return -1;
      }
      if (socks4_prot == socks4a) {
        if (next+1 == data+datalen) {
          log_debug(LD_APP,"socks4: No part of destaddr here yet.");
          *want_length_out = datalen + 1024; /* More than we need, but safe */
          return 0;
        }
        startaddr = next+1;
        next = memchr(startaddr, 0, data + datalen - startaddr);
        if (!next) {
          if (datalen >= 1024) {
            log_debug(LD_APP,"socks4: Destaddr too long.");
            return -1;
          }
          log_debug(LD_APP,"socks4: Destaddr not all here yet.");
          *want_length_out = datalen + 1024; /* More than we need, but safe */
          return 0;
        }
        if (MAX_SOCKS_ADDR_LEN <= next-startaddr) {
          log_warn(LD_APP,"socks4: Destaddr too long. Rejecting.");
          return -1;
        }
        // tor_assert(next < buf->cur+buf->datalen);

        if (log_sockstype)
          log_notice(LD_APP,
                     "Your application (using socks4a to port %d) instructed "
                     "Tor to take care of the DNS resolution itself if "
                     "necessary. This is good.", req->port);
      }
      log_debug(LD_APP,"socks4: Everything is here. Success.");
      strlcpy(req->address, startaddr ? startaddr : tmpbuf,
              sizeof(req->address));
      if (!tor_strisprint(req->address) || strchr(req->address,'\"')) {
        log_warn(LD_PROTOCOL,
                 "Your application (using socks4 to port %d) gave Tor "
                 "a malformed hostname: %s. Rejecting the connection.",
                 req->port, escaped_safe_str_client(req->address));
        return -1;
      }
      if (authend != authstart) {
        req->got_auth = 1;
        req->usernamelen = authend - authstart;
        req->username = tor_memdup(authstart, authend - authstart);
      }
      /* next points to the final \0 on inbuf */
      *drain_out = next - data + 1;
      return 1;
    }
    case 'G': /* get */
    case 'H': /* head */
    case 'P': /* put/post */
    case 'C': /* connect */
      strlcpy((char*)req->reply,
"HTTP/1.0 501 Tor is not an HTTP Proxy\r\n"
"Content-Type: text/html; charset=iso-8859-1\r\n\r\n"
"<html>\n"
"<head>\n"
"<title>Tor is not an HTTP Proxy</title>\n"
"</head>\n"
"<body>\n"
"<h1>Tor is not an HTTP Proxy</h1>\n"
"<p>\n"
"It appears you have configured your web browser to use Tor as an HTTP proxy."
"\n"
"This is not correct: Tor is a SOCKS proxy, not an HTTP proxy.\n"
"Please configure your client accordingly.\n"
"</p>\n"
"<p>\n"
"See <a href=\"https://www.torproject.org/documentation.html\">"
           "https://www.torproject.org/documentation.html</a> for more "
           "information.\n"
"<!-- Plus this comment, to make the body response more than 512 bytes, so "
"     IE will be willing to display it. Comment comment comment comment "
"     comment comment comment comment comment comment comment comment.-->\n"
"</p>\n"
"</body>\n"
"</html>\n"
             , MAX_SOCKS_REPLY_LEN);
      req->replylen = strlen((char*)req->reply)+1;
      /* fall through */
    default: /* version is not socks4 or socks5 */
      log_warn(LD_APP,
               "Socks version %d not recognized. (Tor is not an http proxy.)",
               *(data));
      {
        /* Tell the controller the first 8 bytes. */
        char *tmp = tor_strndup(data, datalen < 8 ? datalen : 8);
        control_event_client_status(LOG_WARN,
                                    "SOCKS_UNKNOWN_PROTOCOL DATA=\"%s\"",
                                    escaped(tmp));
        tor_free(tmp);
      }
      return -1;
  }
}

/** Inspect a reply from SOCKS server stored in <b>buf</b> according
 * to <b>state</b>, removing the protocol data upon success. Return 0 on
 * incomplete response, 1 on success and -1 on error, in which case
 * <b>reason</b> is set to a descriptive message (free() when finished
 * with it).
 *
 * As a special case, 2 is returned when user/pass is required
 * during SOCKS5 handshake and user/pass is configured.
 */
int
fetch_from_buf_socks_client(buf_t *buf, int state, char **reason)
{
  ssize_t drain = 0;
  int r;
  if (buf->datalen < 2)
    return 0;

  buf_pullup(buf, MAX_SOCKS_MESSAGE_LEN);
  tor_assert(buf->head && buf->head->datalen >= 2);

  r = parse_socks_client((uint8_t*)buf->head->data, buf->head->datalen,
                         state, reason, &drain);
  if (drain > 0)
    buf_remove_from_front(buf, drain);
  else if (drain < 0)
    buf_clear(buf);

  return r;
}

/** Implementation logic for fetch_from_*_socks_client. */
static int
parse_socks_client(const uint8_t *data, size_t datalen,
                   int state, char **reason,
                   ssize_t *drain_out)
{
  unsigned int addrlen;
  *drain_out = 0;
  if (datalen < 2)
    return 0;

  switch (state) {
    case PROXY_SOCKS4_WANT_CONNECT_OK:
      /* Wait for the complete response */
      if (datalen < 8)
        return 0;

      if (data[1] != 0x5a) {
        *reason = tor_strdup(socks4_response_code_to_string(data[1]));
        return -1;
      }

      /* Success */
      *drain_out = 8;
      return 1;

    case PROXY_SOCKS5_WANT_AUTH_METHOD_NONE:
      /* we don't have any credentials */
      if (data[1] != 0x00) {
        *reason = tor_strdup("server doesn't support any of our "
                             "available authentication methods");
        return -1;
      }

      log_info(LD_NET, "SOCKS 5 client: continuing without authentication");
      *drain_out = -1;
      return 1;

    case PROXY_SOCKS5_WANT_AUTH_METHOD_RFC1929:
      /* we have a username and password. return 1 if we can proceed without
       * providing authentication, or 2 otherwise. */
      switch (data[1]) {
        case 0x00:
          log_info(LD_NET, "SOCKS 5 client: we have auth details but server "
                            "doesn't require authentication.");
          *drain_out = -1;
          return 1;
        case 0x02:
          log_info(LD_NET, "SOCKS 5 client: need authentication.");
          *drain_out = -1;
          return 2;
        /* fall through */
      }

      *reason = tor_strdup("server doesn't support any of our available "
                           "authentication methods");
      return -1;

    case PROXY_SOCKS5_WANT_AUTH_RFC1929_OK:
      /* handle server reply to rfc1929 authentication */
      if (data[1] != 0x00) {
        *reason = tor_strdup("authentication failed");
        return -1;
      }

      log_info(LD_NET, "SOCKS 5 client: authentication successful.");
      *drain_out = -1;
      return 1;

    case PROXY_SOCKS5_WANT_CONNECT_OK:
      /* response is variable length. BND.ADDR, etc, isn't needed
       * (don't bother with buf_pullup()), but make sure to eat all
       * the data used */

      /* wait for address type field to arrive */
      if (datalen < 4)
        return 0;

      switch (data[3]) {
        case 0x01: /* ip4 */
          addrlen = 4;
          break;
        case 0x04: /* ip6 */
          addrlen = 16;
          break;
        case 0x03: /* fqdn (can this happen here?) */
          if (datalen < 5)
            return 0;
          addrlen = 1 + data[4];
          break;
        default:
          *reason = tor_strdup("invalid response to connect request");
          return -1;
      }

      /* wait for address and port */
      if (datalen < 6 + addrlen)
        return 0;

      if (data[1] != 0x00) {
        *reason = tor_strdup(socks5_response_code_to_string(data[1]));
        return -1;
      }

      *drain_out = 6 + addrlen;
      return 1;
  }

  /* shouldn't get here... */
  tor_assert(0);

  return -1;
}

/** Return 1 iff buf looks more like it has an (obsolete) v0 controller
 * command on it than any valid v1 controller command. */
int
peek_buf_has_control0_command(buf_t *buf)
{
  if (buf->datalen >= 4) {
    char header[4];
    uint16_t cmd;
    peek_from_buf(header, sizeof(header), buf);
    cmd = ntohs(get_uint16(header+2));
    if (cmd <= 0x14)
      return 1; /* This is definitely not a v1 control command. */
  }
  return 0;
}

/** Return the index within <b>buf</b> at which <b>ch</b> first appears,
 * or -1 if <b>ch</b> does not appear on buf. */
static off_t
buf_find_offset_of_char(buf_t *buf, char ch)
{
  chunk_t *chunk;
  off_t offset = 0;
  for (chunk = buf->head; chunk; chunk = chunk->next) {
    char *cp = memchr(chunk->data, ch, chunk->datalen);
    if (cp)
      return offset + (cp - chunk->data);
    else
      offset += chunk->datalen;
  }
  return -1;
}

/** Try to read a single LF-terminated line from <b>buf</b>, and write it
 * (including the LF), NUL-terminated, into the *<b>data_len</b> byte buffer
 * at <b>data_out</b>.  Set *<b>data_len</b> to the number of bytes in the
 * line, not counting the terminating NUL.  Return 1 if we read a whole line,
 * return 0 if we don't have a whole line yet, and return -1 if the line
 * length exceeds *<b>data_len</b>.
 */
int
fetch_from_buf_line(buf_t *buf, char *data_out, size_t *data_len)
{
  size_t sz;
  off_t offset;

  if (!buf->head)
    return 0;

  offset = buf_find_offset_of_char(buf, '\n');
  if (offset < 0)
    return 0;
  sz = (size_t) offset;
  if (sz+2 > *data_len) {
    *data_len = sz + 2;
    return -1;
  }
  fetch_from_buf(data_out, sz+1, buf);
  data_out[sz+1] = '\0';
  *data_len = sz+1;
  return 1;
}

/** Compress on uncompress the <b>data_len</b> bytes in <b>data</b> using the
 * zlib state <b>state</b>, appending the result to <b>buf</b>.  If
 * <b>done</b> is true, flush the data in the state and finish the
 * compression/uncompression.  Return -1 on failure, 0 on success. */
int
write_to_buf_zlib(buf_t *buf, tor_zlib_state_t *state,
                  const char *data, size_t data_len,
                  int done)
{
  char *next;
  size_t old_avail, avail;
  int over = 0;

  do {
    int need_new_chunk = 0;
    if (!buf->tail || ! CHUNK_REMAINING_CAPACITY(buf->tail)) {
      size_t cap = data_len / 4;
      buf_add_chunk_with_capacity(buf, cap, 1);
    }
    next = CHUNK_WRITE_PTR(buf->tail);
    avail = old_avail = CHUNK_REMAINING_CAPACITY(buf->tail);
    switch (tor_zlib_process(state, &next, &avail, &data, &data_len, done)) {
      case TOR_ZLIB_DONE:
        over = 1;
        break;
      case TOR_ZLIB_ERR:
        return -1;
      case TOR_ZLIB_OK:
        if (data_len == 0)
          over = 1;
        break;
      case TOR_ZLIB_BUF_FULL:
        if (avail) {
          /* Zlib says we need more room (ZLIB_BUF_FULL).  Start a new chunk
           * automatically, whether were going to or not. */
          need_new_chunk = 1;
        }
        break;
    }
    buf->datalen += old_avail - avail;
    buf->tail->datalen += old_avail - avail;
    if (need_new_chunk) {
      buf_add_chunk_with_capacity(buf, data_len/4, 1);
    }

  } while (!over);
  check();
  return 0;
}

/** Set *<b>output</b> to contain a copy of the data in *<b>input</b> */
int
buf_set_to_copy(buf_t **output,
                const buf_t *input)
{
  if (*output)
    buf_free(*output);
  *output = buf_copy(input);
  return 0;
}

/** Log an error and exit if <b>buf</b> is corrupted.
 */
void
assert_buf_ok(buf_t *buf)
{
  tor_assert(buf);
  tor_assert(buf->magic == BUFFER_MAGIC);

  if (! buf->head) {
    tor_assert(!buf->tail);
    tor_assert(buf->datalen == 0);
  } else {
    chunk_t *ch;
    size_t total = 0;
    tor_assert(buf->tail);
    for (ch = buf->head; ch; ch = ch->next) {
      total += ch->datalen;
      tor_assert(ch->datalen <= ch->memlen);
      tor_assert(ch->data >= &ch->mem[0]);
      tor_assert(ch->data <= &ch->mem[0]+ch->memlen);
      if (ch->data == &ch->mem[0]+ch->memlen) {
        static int warned = 0;
        if (! warned) {
          log_warn(LD_BUG, "Invariant violation in buf.c related to #15083");
          warned = 1;
        }
      }
      tor_assert(ch->data+ch->datalen <= &ch->mem[0] + ch->memlen);
      if (!ch->next)
        tor_assert(ch == buf->tail);
    }
    tor_assert(buf->datalen == total);
  }
}

