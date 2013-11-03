/*
 * generic_cjk
 * Copyright (C) TSUBAKIMOTO Hiroya <zorac@4000do.co.jp> 2004
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#if HAVE_USABLE_ICONV

#include "generic_cjk.h"
#include <string.h>

static size_t cjk_iconv(void *cd, char **inbuf, char *end,
			char **outbuf, size_t *outbytesleft)
{
  size_t n = end - *inbuf;
  if (iconv(cd, (ICONV_CONST char**)inbuf, &n, outbuf, outbytesleft) == (size_t)-1) {
    iconv(cd, NULL, NULL, NULL, NULL);
  }
  return n;
}

size_t cjk_generic_push(size_t (*char_func)(uint8_t*, const ucs2_t*, size_t*),
			void *cd, char **inbuf, size_t *inbytesleft,
			char **outbuf, size_t *outbytesleft)
{
  char *in = *inbuf;

  while (*inbytesleft >= sizeof(ucs2_t) && *outbytesleft > 0) {
    uint8_t buf[CJK_PUSH_BUFFER];
    size_t size = *inbytesleft / sizeof(ucs2_t);
    size_t n = (char_func)(buf, (const ucs2_t*)in, &size);
    if (n == 0) {
      in += size * sizeof(ucs2_t);
      *inbytesleft -= size * sizeof(ucs2_t);
      continue;
    }
    if (in != *inbuf) {
      int err = errno;

      *inbytesleft += cjk_iconv(cd, inbuf, in, outbuf, outbytesleft);
      if (in != *inbuf) return -1;
      errno = err;
    }
    if (n == (size_t)-1) return -1;
    if (*outbytesleft < n) break;
    memcpy(*outbuf, buf, n);
    *outbuf += n;
    *outbytesleft -= n;
    in += size * sizeof(ucs2_t);
    *inbytesleft -= size * sizeof(ucs2_t);
    *inbuf = in;
  }
  if (in != *inbuf) {
    *inbytesleft += cjk_iconv(cd, inbuf, in, outbuf, outbytesleft);
    if (in != *inbuf) return -1;
  }
  if (*inbytesleft > 0) {
    errno = (*inbytesleft < sizeof(ucs2_t) ? EINVAL : E2BIG);
    return -1;
  }
  return 0;
}

size_t cjk_generic_pull(size_t (*char_func)(ucs2_t*, const uint8_t*, size_t*),
			void *cd, char **inbuf, size_t *inbytesleft,
			char **outbuf, size_t *outbytesleft)
{
  char *in = *inbuf;

  while (*inbytesleft > 0 && *outbytesleft >= sizeof(ucs2_t)) {
    ucs2_t buf[CJK_PULL_BUFFER];
    size_t size = *inbytesleft;
    size_t n = (char_func)(buf, (const uint8_t*)in, &size);
    if (n == 0) {
      in += size;
      *inbytesleft -= size;
      continue;
    }
    if (in != *inbuf) {
      int err = errno;

      *inbytesleft += cjk_iconv(cd, inbuf, in, outbuf, outbytesleft);
      if (in != *inbuf) return -1;
      errno = err;
    }
    if (n == (size_t)-1) return -1;
    if (*outbytesleft < n * sizeof(ucs2_t)) break;
    memcpy(*outbuf, buf, n * sizeof(ucs2_t));
    *outbuf += n * sizeof(ucs2_t);
    *outbytesleft -= n * sizeof(ucs2_t);
    in += size;
    *inbytesleft -= size;
    *inbuf = in;
  }
  if (in != *inbuf) {
    *inbytesleft += cjk_iconv(cd, inbuf, in, outbuf, outbytesleft);
    if (in != *inbuf) return -1;
  }
  if (*inbytesleft > 0) {
    errno = E2BIG;
    return -1;
  }
  return 0;
}

size_t cjk_char_push(uint16_t c, uint8_t *out)
{
  if (!c) return 0;
  if (c == (uint16_t)-1) {
    errno = EILSEQ;
    return (size_t)-1;
  }
  if (c <= 0xff) {
    out[0] = (uint8_t)c;
    return 1;
  }
  out[0] = (uint8_t)(c >> 8);
  out[1] = (uint8_t)c;
  return 2;
}

size_t cjk_char_pull(ucs2_t wc, ucs2_t* out, const uint32_t* compose)
{
  if (!wc) return 0;
  if ((wc & 0xf000) == 0xe000) {
    ucs2_t buf[CJK_PULL_BUFFER];
    size_t i = sizeof(buf) / sizeof(*buf) - 1;
    do {
      uint32_t v = compose[wc & 0xfff];
      buf[i] = (ucs2_t)v;
      wc = (ucs2_t)(v >> 16);
    } while (--i && (wc & 0xf000) == 0xe000);
    buf[i] = wc;
    memcpy(out, buf + i, sizeof(buf) - sizeof(*buf) * i);
    return sizeof(buf) / sizeof(*buf) - i;
  }
  *out = wc;
  return 1;
}

uint16_t cjk_lookup(uint16_t c, const cjk_index_t *index, const uint16_t *charset)
{
  while (index->summary && c >= index->range[0]) {
    if (c <= index->range[1]) {
      const uint16_t* summary = index->summary[(c - index->range[0]) >> 4];
      uint16_t used = 1 << (c & 15);

      if (summary[0] & used) {
	used = summary[0] & (used - 1);
	charset += summary[1];
	while (used) used &= used - 1, ++charset;
	return *charset;
      }
      return 0;
    }
    ++index;
  }
  return 0;
}

ucs2_t cjk_compose(ucs2_t base, ucs2_t comb, const uint32_t* table, size_t size)
{
  uint32_t v = ((uint32_t)base << 16) | comb;
  size_t low = 0;
  while (size > low) {
    size_t n = (low + size) / 2;
    if (table[n] == v) return 0xe000 + n;
    if (table[n] < v) {
      low = n + 1;
    } else {
      size = n;
    }
  }
  return 0;
}

ucs2_t cjk_compose_seq(const ucs2_t* in, size_t* len, const uint32_t* table, size_t size)
{
  static uint8_t sz[] = { 3, 4, 5, 5, 5, 5, 5, 3 };
  ucs2_t wc = in[0];
  size_t n = sz[wc & 7];
  size_t i = 0;

  if (n > *len) {
    errno = EINVAL;
    return 0;
  }
  while (++i < n) {
    wc = cjk_compose(wc, in[i], table, size);
    if (!wc) {
      errno = EILSEQ;
      return 0;
    }
  }
  *len = n;
  return wc;
}
#endif
