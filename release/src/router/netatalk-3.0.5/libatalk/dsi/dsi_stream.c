/*
 * Copyright (c) 1998 Adrian Sun (asun@zoology.washington.edu)
 * Copyright (c) 2010,2011,2012 Frank Lahm <franklahm@googlemail.com>
 * All rights reserved. See COPYRIGHT.
 *
 * this file provides the following functions:
 * dsi_stream_write:    just write a bunch of bytes.
 * dsi_stream_read:     just read a bunch of bytes.
 * dsi_stream_send:     send a DSI header + data.
 * dsi_stream_receive:  read a DSI header + data.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#ifdef HAVE_SENDFILEV
#include <sys/sendfile.h>
#endif

#include <atalk/logger.h>
#include <atalk/dsi.h>
#include <atalk/util.h>

#ifndef MSG_MORE
#define MSG_MORE 0x8000
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0x40
#endif

/* Pack a DSI header in wire format */
static void dsi_header_pack_reply(const DSI *dsi, char *buf)
{
    buf[0] = dsi->header.dsi_flags;
    buf[1] = dsi->header.dsi_command;
    memcpy(buf + 2, &dsi->header.dsi_requestID, sizeof(dsi->header.dsi_requestID));           
    memcpy(buf + 4, &dsi->header.dsi_data.dsi_code, sizeof(dsi->header.dsi_data.dsi_code));
    memcpy(buf + 8, &dsi->header.dsi_len, sizeof(dsi->header.dsi_len));
    memcpy(buf + 12, &dsi->header.dsi_reserved, sizeof(dsi->header.dsi_reserved));
}

/*
 * afpd is sleeping too much while trying to send something.
 * May be there's no reader or the reader is also sleeping in write,
 * look if there's some data for us to read, hopefully it will wake up
 * the reader so we can write again.
 *
 * @returns 0 when is possible to send again, -1 on error
 */
static int dsi_peek(DSI *dsi)
{
    static int warned = 0;
    fd_set readfds, writefds;
    int    len;
    int    maxfd;
    int    ret;

    LOG(log_debug, logtype_dsi, "dsi_peek");

    maxfd = dsi->socket + 1;

    while (1) {
        if (dsi->socket == -1)
            /* eg dsi_disconnect() might have disconnected us */
            return -1;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        if (dsi->eof < dsi->end) {
            /* space in read buffer */
            FD_SET( dsi->socket, &readfds);
        } else {
            if (!warned) {
                warned = 1;
                LOG(log_note, logtype_dsi, "dsi_peek: readahead buffer is full, possibly increase -dsireadbuf option");
                LOG(log_note, logtype_dsi, "dsi_peek: dsireadbuf: %d, DSI quantum: %d, effective buffer size: %d",
                    dsi->dsireadbuf,
                    dsi->server_quantum ? dsi->server_quantum : DSI_SERVQUANT_DEF,
                    dsi->end - dsi->buffer);
            }
        }

        FD_SET( dsi->socket, &writefds);

        /* No timeout: if there's nothing to read nor nothing to write,
         * we've got nothing to do at all */
        if ((ret = select( maxfd, &readfds, &writefds, NULL, NULL)) <= 0) {
            if (ret == -1 && errno == EINTR)
                /* we might have been interrupted by out timer, so restart select */
                continue;
            /* give up */
            LOG(log_error, logtype_dsi, "dsi_peek: unexpected select return: %d %s",
                ret, ret < 0 ? strerror(errno) : "");
            return -1;
        }

        if (FD_ISSET(dsi->socket, &writefds)) {
            /* we can write again */
            LOG(log_debug, logtype_dsi, "dsi_peek: can write again");
            break;
        }

        /* Check if there's sth to read, hopefully reading that will unblock the client */
        if (FD_ISSET(dsi->socket, &readfds)) {
            len = dsi->end - dsi->eof; /* it's ensured above that there's space */

            if ((len = recv(dsi->socket, dsi->eof, len, 0)) <= 0) {
                if (len == 0) {
                    LOG(log_error, logtype_dsi, "dsi_peek: EOF");
                    return -1;
                }
                LOG(log_error, logtype_dsi, "dsi_peek: read: %s", strerror(errno));
                if (errno == EAGAIN)
                    continue;
                return -1;
            }
            LOG(log_debug, logtype_dsi, "dsi_peek: read %d bytes", len);

            dsi->eof += len;
        }
    }

    return 0;
}

/* 
 * Return all bytes up to count from dsi->buffer if there are any buffered there
 */
static size_t from_buf(DSI *dsi, uint8_t *buf, size_t count)
{
    size_t nbe = 0;

    if (dsi->buffer == NULL)
        /* afpd master has no DSI buffering */
        return 0;

    LOG(log_maxdebug, logtype_dsi, "from_buf: %u bytes", count);
    
    nbe = dsi->eof - dsi->start;

    if (nbe > 0) {
        nbe = MIN((size_t)nbe, count);
        memcpy(buf, dsi->start, nbe);
        dsi->start += nbe;

        if (dsi->eof == dsi->start)
            dsi->start = dsi->eof = dsi->buffer;
    }

    LOG(log_debug, logtype_dsi, "from_buf(read: %u, unread:%u , space left: %u): returning %u",
        dsi->start - dsi->buffer, dsi->eof - dsi->start, dsi->end - dsi->eof, nbe);

    return nbe;
}

/*
 * Get bytes from buffer dsi->buffer or read from socket
 *
 * 1. Check if there are bytes in the the dsi->buffer buffer.
 * 2. Return bytes from (1) if yes.
 *    Note: this may return fewer bytes then requested in count !!
 * 3. If the buffer was empty, read from the socket.
 */
static ssize_t buf_read(DSI *dsi, uint8_t *buf, size_t count)
{
    ssize_t len;

    LOG(log_maxdebug, logtype_dsi, "buf_read(%u bytes)", count);

    if (!count)
        return 0;

    len = from_buf(dsi, buf, count); /* 1. */
    if (len)
        return len;             /* 2. */
  
    len = readt(dsi->socket, buf, count, 0, 0); /* 3. */

    LOG(log_maxdebug, logtype_dsi, "buf_read(%u bytes): got: %d", count, len);

    return len;
}

/*
 * Get "length" bytes from buffer and/or socket. In order to avoid frequent small reads
 * this tries to read larger chunks (8192 bytes) into a buffer.
 */
static size_t dsi_buffered_stream_read(DSI *dsi, uint8_t *data, const size_t length)
{
  size_t len;
  size_t buflen;

  LOG(log_maxdebug, logtype_dsi, "dsi_buffered_stream_read: %u bytes", length);
  
  len = from_buf(dsi, data, length); /* read from buffer dsi->buffer */
  dsi->read_count += len;
  if (len == length) {          /* got enough bytes from there ? */
      return len;               /* yes */
  }

  /* fill the buffer with 8192 bytes or until buffer is full */
  buflen = MIN(8192, dsi->end - dsi->eof);
  if (buflen > 0) {
      ssize_t ret;
      ret = recv(dsi->socket, dsi->eof, buflen, 0);
      if (ret > 0)
          dsi->eof += ret;
  }

  /* now get the remaining data */
  if ((buflen = dsi_stream_read(dsi, data + len, length - len)) != length - len)
      return 0;
  len += buflen;

  return len;
}

/* ---------------------------------------
*/
static void block_sig(DSI *dsi)
{
  dsi->in_write++;
}

/* ---------------------------------------
*/
static void unblock_sig(DSI *dsi)
{
  dsi->in_write--;
}

/*********************************************************************************
 * Public functions
 *********************************************************************************/

/*!
 * Communication error with the client, enter disconnected state
 *
 * 1. close the socket
 * 2. set the DSI_DISCONNECTED flag, remove possible sleep flags
 *
 * @returns  0 if successfully entered disconnected state
 *          -1 if ppid is 1 which means afpd master died
 *             or euid == 0 ie where still running as root (unauthenticated session)
 */
int dsi_disconnect(DSI *dsi)
{
    LOG(log_note, logtype_dsi, "dsi_disconnect: entering disconnected state");
    dsi->proto_close(dsi);          /* 1 */
    dsi->flags &= ~(DSI_SLEEPING | DSI_EXTSLEEP); /* 2 */
    dsi->flags |= DSI_DISCONNECTED;
    if (geteuid() == 0)
        return -1;
    return 0;
}

/* ------------------------------
 * write raw data. return actual bytes read. checks against EINTR
 * aren't necessary if all of the signals have SA_RESTART
 * specified. */
ssize_t dsi_stream_write(DSI *dsi, void *data, const size_t length, int mode)
{
  size_t written;
  ssize_t len;
  unsigned int flags;

  dsi->in_write++;
  written = 0;

  LOG(log_maxdebug, logtype_dsi, "dsi_stream_write(send: %zd bytes): START", length);

  if (dsi->flags & DSI_DISCONNECTED)
      return -1;

  if (mode & DSI_MSG_MORE)
      flags = MSG_MORE;
  else
      flags = 0;

  while (written < length) {
      len = send(dsi->socket, (uint8_t *) data + written, length - written, flags);
      if (len >= 0) {
          written += len;
          continue;
      }

      if (errno == EINTR)
          continue;

      if (errno == EAGAIN || errno == EWOULDBLOCK) {
          LOG(log_debug, logtype_dsi, "dsi_stream_write: send: %s", strerror(errno));

          if (mode == DSI_NOWAIT && written == 0) {
              /* DSI_NOWAIT is used by attention give up in this case. */
              written = -1;
              goto exit;
          }

          /* Try to read sth. in order to break up possible deadlock */
          if (dsi_peek(dsi) != 0) {
              written = -1;
              goto exit;
          }
          /* Now try writing again */
          continue;
      }

      LOG(log_error, logtype_dsi, "dsi_stream_write: %s", strerror(errno));
      written = -1;
      goto exit;
  }

  dsi->write_count += written;
  LOG(log_maxdebug, logtype_dsi, "dsi_stream_write(send: %zd bytes): END", length);

exit:
  dsi->in_write--;
  return written;
}

/* ---------------------------------
*/
#ifdef WITH_SENDFILE
ssize_t dsi_stream_read_file(DSI *dsi, const int fromfd, off_t offset, const size_t length, const int err)
{
    int ret = 0;
    size_t written = 0;
    size_t total = length;
    ssize_t len;
    off_t pos = offset;
    char block[DSI_BLOCKSIZ];
#ifdef HAVE_SENDFILEV
    int sfvcnt;
    struct sendfilevec vec[2];
    ssize_t nwritten;
#elif defined(FREEBSD)
    ssize_t nwritten;
    void *hdrp;
    struct sf_hdtr hdr;
    struct iovec iovec;
    hdr.headers = &iovec;
    hdr.hdr_cnt = 1;
    hdr.trailers = NULL;
    hdr.trl_cnt = 0;
    hdrp = &hdr;
#endif

    LOG(log_maxdebug, logtype_dsi, "dsi_stream_read_file(off: %jd, len: %zu)", (intmax_t)offset, length);

    if (dsi->flags & DSI_DISCONNECTED)
        return -1;

    dsi->in_write++;

    dsi->flags |= DSI_NOREPLY;
    dsi->header.dsi_flags = DSIFL_REPLY;
    dsi->header.dsi_len = htonl(length);
    dsi->header.dsi_data.dsi_code = htonl(err);
    dsi_header_pack_reply(dsi, block);

#ifdef HAVE_SENDFILEV
    total += DSI_BLOCKSIZ;
    sfvcnt = 2;
    vec[0].sfv_fd = SFV_FD_SELF;
    vec[0].sfv_flag = 0;
    /* Cast to unsigned long to prevent sign extension of the
     * pointer value for the LFS case; see Apache PR 39463. */
    vec[0].sfv_off = (unsigned long)block;
    vec[0].sfv_len = DSI_BLOCKSIZ;
    vec[1].sfv_fd = fromfd;
    vec[1].sfv_flag = 0;
    vec[1].sfv_off = offset;
    vec[1].sfv_len = length;
#elif defined(FREEBSD)
    iovec.iov_base = block;
    iovec.iov_len = DSI_BLOCKSIZ;
#else
    dsi_stream_write(dsi, block, sizeof(block), DSI_MSG_MORE);
#endif

    while (written < total) {
#ifdef HAVE_SENDFILEV
        nwritten = 0;
        len = sendfilev(dsi->socket, vec, sfvcnt, &nwritten);
#elif defined(FREEBSD)
        len = sendfile(fromfd, dsi->socket, pos, total - written, hdrp, &nwritten, 0);
        if (len == 0)
            len = nwritten;
#else
        len = sys_sendfile(dsi->socket, fromfd, &pos, total - written);
#endif
        if (len < 0) {
            switch (errno) {
            case EINTR:
            case EAGAIN:
                len = 0;
#if defined(HAVE_SENDFILEV) || defined(FREEBSD)
                len = (size_t)nwritten;
#elif defined(SOLARIS)
                if (pos > offset) {
                    /* we actually have sent sth., adjust counters and keep trying */
                    len = pos - offset;
                    offset = pos;
                }
#endif /* HAVE_SENDFILEV */

                if (dsi_peek(dsi) != 0) {
                    ret = -1;
                    goto exit;
                }
                break;
            default:
                LOG(log_error, logtype_dsi, "dsi_stream_read_file: %s", strerror(errno));
                ret = -1;
                goto exit;
            }
        } else if (len == 0) {
            /* afpd is going to exit */
            ret = -1;
            goto exit;
        }
#ifdef HAVE_SENDFILEV
        if (sfvcnt == 2 && len >= vec[0].sfv_len) {
            vec[1].sfv_off += len - vec[0].sfv_len;
            vec[1].sfv_len -= len - vec[0].sfv_len;

            vec[0] = vec[1];
            sfvcnt = 1;
        } else {
            vec[0].sfv_off += len;
            vec[0].sfv_len -= len;
        }
#elif defined(FREEBSD)
        if (hdrp) {
            if (len >= iovec.iov_len) {
                hdrp = NULL;
                len -= iovec.iov_len;   /* len now contains how much sendfile() actually sent from the file */
            } else {
                iovec.iov_len -= len;
                iovec.iov_base += len;
                len = 0;
            }
        }
        pos += len;
#endif  /* HAVE_SENDFILEV */
        LOG(log_maxdebug, logtype_dsi, "dsi_stream_read_file: wrote: %zd", len);
        written += len;
    }
#ifdef HAVE_SENDFILEV
    written -= DSI_BLOCKSIZ;
#endif
    dsi->write_count += written;

exit:
    dsi->in_write--;
    LOG(log_maxdebug, logtype_dsi, "dsi_stream_read_file: written: %zd", written);
    if (ret != 0)
        return -1;
    return written;
}
#endif


/*
 * Essentially a loop around buf_read() to ensure "length" bytes are read
 * from dsi->buffer and/or the socket.
 *
 * @returns length on success, some value smaller then length indicates an error
 */
size_t dsi_stream_read(DSI *dsi, void *data, const size_t length)
{
  size_t stored;
  ssize_t len;

  if (dsi->flags & DSI_DISCONNECTED)
      return 0;

  LOG(log_maxdebug, logtype_dsi, "dsi_stream_read(%u bytes)", length);

  stored = 0;
  while (stored < length) {
      len = buf_read(dsi, (uint8_t *) data + stored, length - stored);
      if (len == -1 && (errno == EINTR || errno == EAGAIN)) {
          LOG(log_maxdebug, logtype_dsi, "dsi_stream_read: select read loop");
          continue;
      } else if (len > 0) {
          stored += len;
      } else { /* eof or error */
          /* don't log EOF error if it's just after connect (OSX 10.3 probe) */
          if (len || stored || dsi->read_count) {
              if (! (dsi->flags & DSI_DISCONNECTED)) {
                  LOG(log_error, logtype_dsi, "dsi_stream_read: len:%d, %s",
                      len, (len < 0) ? strerror(errno) : "unexpected EOF");
              }
              return 0;
          }
          break;
      }
  }

  dsi->read_count += stored;

  LOG(log_maxdebug, logtype_dsi, "dsi_stream_read(%u bytes): got: %u", length, stored);
  return stored;
}

/* ---------------------------------------
 * write data. 0 on failure. this assumes that dsi_len will never
 * cause an overflow in the data buffer. 
 */
int dsi_stream_send(DSI *dsi, void *buf, size_t length)
{
  char block[DSI_BLOCKSIZ];
  struct iovec iov[2];
  int iovecs = 2;
  size_t towrite;
  ssize_t len;

  LOG(log_maxdebug, logtype_dsi, "dsi_stream_send(%u bytes): START", length);

  if (dsi->flags & DSI_DISCONNECTED)
      return 0;

  dsi_header_pack_reply(dsi, block);

  if (!length) { /* just write the header */
      LOG(log_maxdebug, logtype_dsi, "dsi_stream_send(%u bytes): DSI header, no data", sizeof(block));
    length = (dsi_stream_write(dsi, block, sizeof(block), 0) == sizeof(block));
    return length; /* really 0 on failure, 1 on success */
  }
  
  /* block signals */
  block_sig(dsi);
  iov[0].iov_base = block;
  iov[0].iov_len = sizeof(block);
  iov[1].iov_base = buf;
  iov[1].iov_len = length;
  
  towrite = sizeof(block) + length;
  dsi->write_count += towrite;
  while (towrite > 0) {
      if (((len = writev(dsi->socket, iov, iovecs)) == -1 && errno == EINTR) || (len == 0))
          continue;
    
      if ((size_t)len == towrite) /* wrote everything out */
          break;
      else if (len < 0) { /* error */
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
              if (dsi_peek(dsi) == 0) {
                  continue;
              }
          }
          LOG(log_error, logtype_dsi, "dsi_stream_send: %s", strerror(errno));
          unblock_sig(dsi);
          return 0;
      }
    
      towrite -= len;
      if (towrite > length) { /* skip part of header */
          iov[0].iov_base = (char *) iov[0].iov_base + len;
          iov[0].iov_len -= len;
      } else { /* skip to data */
          if (iovecs == 2) {
              iovecs = 1;
              len -= iov[0].iov_len;
              iov[0] = iov[1];
          }
          iov[0].iov_base = (char *) iov[0].iov_base + len;
          iov[0].iov_len -= len;
      }
  }

  LOG(log_maxdebug, logtype_dsi, "dsi_stream_send(%u bytes): END", length);
  
  unblock_sig(dsi);
  return 1;
}


/*!
 * Read DSI command and data
 *
 * @param  dsi   (rw) DSI handle
 *
 * @return    DSI function on success, 0 on failure
 */
int dsi_stream_receive(DSI *dsi)
{
  char block[DSI_BLOCKSIZ];

  LOG(log_maxdebug, logtype_dsi, "dsi_stream_receive: START");

  if (dsi->flags & DSI_DISCONNECTED)
      return 0;

  /* read in the header */
  if (dsi_buffered_stream_read(dsi, (uint8_t *)block, sizeof(block)) != sizeof(block)) 
    return 0;

  dsi->header.dsi_flags = block[0];
  dsi->header.dsi_command = block[1];

  if (dsi->header.dsi_command == 0)
      return 0;

  memcpy(&dsi->header.dsi_requestID, block + 2, sizeof(dsi->header.dsi_requestID));
  memcpy(&dsi->header.dsi_data.dsi_code, block + 4, sizeof(dsi->header.dsi_data.dsi_code));
  memcpy(&dsi->header.dsi_len, block + 8, sizeof(dsi->header.dsi_len));
  memcpy(&dsi->header.dsi_reserved, block + 12, sizeof(dsi->header.dsi_reserved));
  dsi->clientID = ntohs(dsi->header.dsi_requestID);
  
  /* make sure we don't over-write our buffers. */
  dsi->cmdlen = MIN(ntohl(dsi->header.dsi_len), dsi->server_quantum);
  if (dsi_stream_read(dsi, dsi->commands, dsi->cmdlen) != dsi->cmdlen) 
    return 0;

  LOG(log_debug, logtype_dsi, "dsi_stream_receive: DSI cmdlen: %zd", dsi->cmdlen);

  return block[1];
}
