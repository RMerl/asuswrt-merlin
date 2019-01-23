/* Declarations of functions and data types used for MD2 sum
   library functions.
   Copyright (C) 2000-2001, 2003, 2005, 2008-2017 Free Software Foundation,
   Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

#ifndef MD2_H
# define MD2_H 1

# include <stdio.h>
# include <stddef.h>

# ifdef __cplusplus
extern "C" {
# endif

# define MD2_DIGEST_SIZE 16

/* Structure to save state of computation between the single steps.  */
struct md2_ctx
{
  unsigned char chksum[16], X[48], buf[16];
  size_t curlen;
};


/* Initialize structure containing state of computation. */
extern void md2_init_ctx (struct md2_ctx *ctx);

/* Starting with the result of former calls of this function (or the
   initialization function update the context for the next LEN bytes
   starting at BUFFER.
   It is NOT required that LEN is a multiple of 64.  */
extern void md2_process_block (const void *buffer, size_t len,
                               struct md2_ctx *ctx);

/* Starting with the result of former calls of this function (or the
   initialization function update the context for the next LEN bytes
   starting at BUFFER.
   It is NOT required that LEN is a multiple of 64.  */
extern void md2_process_bytes (const void *buffer, size_t len,
                               struct md2_ctx *ctx);

/* Process the remaining bytes in the buffer and put result from CTX
   in first 16 bytes following RESBUF.  The result is always in little
   endian byte order, so that a byte-wise output yields to the wanted
   ASCII representation of the message digest.  */
extern void *md2_finish_ctx (struct md2_ctx *ctx, void *resbuf);


/* Put result from CTX in first 16 bytes following RESBUF.  The result is
   always in little endian byte order, so that a byte-wise output yields
   to the wanted ASCII representation of the message digest.  */
extern void *md2_read_ctx (const struct md2_ctx *ctx, void *resbuf);


/* Compute MD2 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
extern int md2_stream (FILE *stream, void *resblock);

/* Compute MD2 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
extern void *md2_buffer (const char *buffer, size_t len, void *resblock);

# ifdef __cplusplus
}
# endif

#endif
