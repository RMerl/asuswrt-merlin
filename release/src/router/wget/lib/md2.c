/* Functions to compute MD2 message digest of files or memory blocks.
   according to the definition of MD2 in RFC 1319 from April 1992.
   Copyright (C) 1995-1997, 1999-2003, 2005-2006, 2008-2017 Free Software
   Foundation, Inc.

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

/* Adapted by Simon Josefsson from public domain Libtomcrypt 1.06 by
   Tom St Denis. */

#include <config.h>

#include "md2.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <minmax.h>

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

#define BLOCKSIZE 32768
#if BLOCKSIZE % 64 != 0
# error "invalid BLOCKSIZE"
#endif

static void md2_update_chksum (struct md2_ctx *md);
static void md2_compress (struct md2_ctx *md);

/* Initialize structure containing state of computation.
   (RFC 1319, 3.3: Step 3)  */
void
md2_init_ctx (struct md2_ctx *ctx)
{
  memset (ctx->X, 0, sizeof (ctx->X));
  memset (ctx->chksum, 0, sizeof (ctx->chksum));
  memset (ctx->buf, 0, sizeof (ctx->buf));
  ctx->curlen = 0;
}

/* Put result from CTX in first 16 bytes following RESBUF.  The result
   must be in little endian byte order.  */
void *
md2_read_ctx (const struct md2_ctx *ctx, void *resbuf)
{
  memcpy (resbuf, ctx->X, 16);

  return resbuf;
}

/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.  */
void *
md2_finish_ctx (struct md2_ctx *ctx, void *resbuf)
{
  unsigned long i, k;

  /* pad the message */
  k = 16 - ctx->curlen;
  for (i = ctx->curlen; i < 16; i++)
    {
      ctx->buf[i] = (unsigned char) k;
    }

  /* hash and update */
  md2_compress (ctx);
  md2_update_chksum (ctx);

  /* hash checksum */
  memcpy (ctx->buf, ctx->chksum, 16);
  md2_compress (ctx);

  return md2_read_ctx (ctx, resbuf);
}

/* Compute MD2 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
int
md2_stream (FILE *stream, void *resblock)
{
  struct md2_ctx ctx;
  size_t sum;

  char *buffer = malloc (BLOCKSIZE + 72);
  if (!buffer)
    return 1;

  /* Initialize the computation context.  */
  md2_init_ctx (&ctx);

  /* Iterate over full file contents.  */
  while (1)
    {
      /* We read the file in blocks of BLOCKSIZE bytes.  One call of the
         computation function processes the whole buffer so that with the
         next round of the loop another block can be read.  */
      size_t n;
      sum = 0;

      /* Read block.  Take care for partial reads.  */
      while (1)
        {
          n = fread (buffer + sum, 1, BLOCKSIZE - sum, stream);

          sum += n;

          if (sum == BLOCKSIZE)
            break;

          if (n == 0)
            {
              /* Check for the error flag IFF N == 0, so that we don't
                 exit the loop after a partial read due to e.g., EAGAIN
                 or EWOULDBLOCK.  */
              if (ferror (stream))
                {
                  free (buffer);
                  return 1;
                }
              goto process_partial_block;
            }

          /* We've read at least one byte, so ignore errors.  But always
             check for EOF, since feof may be true even though N > 0.
             Otherwise, we could end up calling fread after EOF.  */
          if (feof (stream))
            goto process_partial_block;
        }

      /* Process buffer with BLOCKSIZE bytes.  Note that
         BLOCKSIZE % 64 == 0
       */
      md2_process_block (buffer, BLOCKSIZE, &ctx);
    }

process_partial_block:;

  /* Process any remaining bytes.  */
  if (sum > 0)
    md2_process_bytes (buffer, sum, &ctx);

  /* Construct result in desired memory.  */
  md2_finish_ctx (&ctx, resblock);
  free (buffer);
  return 0;
}

/* Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
void *
md2_buffer (const char *buffer, size_t len, void *resblock)
{
  struct md2_ctx ctx;

  /* Initialize the computation context.  */
  md2_init_ctx (&ctx);

  /* Process whole buffer but last len % 64 bytes.  */
  md2_process_block (buffer, len, &ctx);

  /* Put result in desired memory area.  */
  return md2_finish_ctx (&ctx, resblock);
}

void
md2_process_bytes (const void *buffer, size_t len, struct md2_ctx *ctx)
{
  const char *in = buffer;
  unsigned long n;

  while (len > 0)
    {
      n = MIN (len, (16 - ctx->curlen));
      memcpy (ctx->buf + ctx->curlen, in, (size_t) n);
      ctx->curlen += n;
      in += n;
      len -= n;

      /* is 16 bytes full? */
      if (ctx->curlen == 16)
        {
          md2_compress (ctx);
          md2_update_chksum (ctx);
          ctx->curlen = 0;
        }
    }
}

static const unsigned char PI_SUBST[256] = {
  41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6,
  19, 98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217, 188,
  76, 130, 202, 30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24,
  138, 23, 229, 18, 190, 78, 196, 214, 218, 158, 222, 73, 160, 251,
  245, 142, 187, 47, 238, 122, 169, 104, 121, 145, 21, 178, 7, 63,
  148, 194, 16, 137, 11, 34, 95, 33, 128, 127, 93, 154, 90, 144, 50,
  39, 53, 62, 204, 231, 191, 247, 151, 3, 255, 25, 48, 179, 72, 165,
  181, 209, 215, 94, 146, 42, 172, 86, 170, 198, 79, 184, 56, 210,
  150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241, 69, 157,
  112, 89, 100, 113, 135, 32, 134, 91, 207, 101, 230, 45, 168, 2, 27,
  96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15,
  85, 71, 163, 35, 221, 81, 175, 58, 195, 92, 249, 206, 186, 197,
  234, 38, 44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65,
  129, 77, 82, 106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123,
  8, 12, 189, 177, 74, 120, 136, 149, 139, 227, 99, 232, 109, 233,
  203, 213, 254, 59, 0, 29, 57, 242, 239, 183, 14, 102, 88, 208, 228,
  166, 119, 114, 248, 235, 117, 75, 10, 49, 68, 80, 180, 143, 237,
  31, 26, 219, 153, 141, 51, 159, 17, 131, 20
};

/* adds 16 bytes to the checksum */
static void
md2_update_chksum (struct md2_ctx *ctx)
{
  int j;
  unsigned char L;

  L = ctx->chksum[15];
  for (j = 0; j < 16; j++)
    {
      /* caution, the RFC says its "C[j] = S[M[i*16+j] xor L]" but the
         reference source code [and test vectors] say otherwise. */
      L = (ctx->chksum[j] ^= PI_SUBST[(int) (ctx->buf[j] ^ L)] & 255);
    }
}

static void
md2_compress (struct md2_ctx *ctx)
{
  size_t j, k;
  unsigned char t;

  /* copy block */
  for (j = 0; j < 16; j++)
    {
      ctx->X[16 + j] = ctx->buf[j];
      ctx->X[32 + j] = ctx->X[j] ^ ctx->X[16 + j];
    }

  t = (unsigned char) 0;

  /* do 18 rounds */
  for (j = 0; j < 18; j++)
    {
      for (k = 0; k < 48; k++)
        {
          t = (ctx->X[k] ^= PI_SUBST[(int) (t & 255)]);
        }
      t = (t + (unsigned char) j) & 255;
    }
}

/* Process LEN bytes of BUFFER, accumulating context into CTX.  */
void
md2_process_block (const void *buffer, size_t len, struct md2_ctx *ctx)
{
  md2_process_bytes (buffer, len, ctx);
}
