/*
 * MacChineseTrad
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
 *
 * Reference
 * http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdlib.h>

#if HAVE_USABLE_ICONV

#include "generic_cjk.h"
#include "mac_chinese_trad.h"

static size_t mac_chinese_trad_pull(void *,char **, size_t *, char **, size_t *);
static size_t mac_chinese_trad_push(void *,char **, size_t *, char **, size_t *);

struct charset_functions charset_mac_chinese_trad = {
  "MAC_CHINESE_TRAD",
  2,
  mac_chinese_trad_pull,
  mac_chinese_trad_push,
  CHARSET_ICONV | CHARSET_MULTIBYTE | CHARSET_PRECOMPOSED | CHARSET_CLIENT,
  "BIG-5",
  NULL, NULL
};

static size_t mac_chinese_trad_char_push(uint8_t* out, const ucs2_t* in, size_t* size)
{
  ucs2_t wc = in[0];

  if (wc <= 0x7f) {
    if (wc == 0x5c && *size >= 2 && in[1] == 0xf87f) {
      *size = 2;
      out[0] = 0x80;
    } else {
      *size = 1;
      out[0] = (uint8_t)wc;
    }
    return 1;
  } else if ((wc & 0xf000) == 0xe000) {
    *size = 1;
    return 0;
  } else if (*size >= 2 && (in[1] & ~15) == 0xf870) {
    ucs2_t comp = cjk_compose(wc, in[1], mac_chinese_trad_compose,
			      sizeof(mac_chinese_trad_compose) / sizeof(uint32_t));
    if (comp) {
      wc = comp;
      *size = 2;
    } else {
      *size = 1;
    }
  } else {
    *size = 1;
  }
  return cjk_char_push(cjk_lookup(wc, mac_chinese_trad_uni2_index,
				  mac_chinese_trad_uni2_charset), out);
}

static size_t mac_chinese_trad_push(void *cd, char **inbuf, size_t *inbytesleft,
				    char **outbuf, size_t *outbytesleft)
{
  return cjk_generic_push(mac_chinese_trad_char_push,
			  cd, inbuf, inbytesleft, outbuf, outbytesleft);
}

static size_t mac_chinese_trad_char_pull(ucs2_t* out, const uint8_t* in, size_t* size)
{
  uint16_t c = in[0];

  if (c <= 0x7f) {
    *size = 1;
    *out = c;
    return 1;
  } else if (c >= 0xa1 && c <= 0xfc) {
    if (*size >= 2) {
      uint8_t c2 = in[1];

      if ((c2 >= 0x40 && c2 <= 0x7e) || (c2 >= 0xa1 && c2 <= 0xfe)) {
	*size = 2;
	c = (c << 8) + c2;
      } else {
	errno = EILSEQ;
	return (size_t)-1;
      }
    } else {
      errno = EINVAL;
      return (size_t)-1;
    }
  } else {
    *size = 1;
  }
  return cjk_char_pull(cjk_lookup(c, mac_chinese_trad_2uni_index,
				  mac_chinese_trad_2uni_charset),
		       out, mac_chinese_trad_compose);
}

static size_t mac_chinese_trad_pull(void *cd, char **inbuf, size_t *inbytesleft,
				    char **outbuf, size_t *outbytesleft)
{
  return cjk_generic_pull(mac_chinese_trad_char_pull,
			  cd, inbuf, inbytesleft, outbuf, outbytesleft);
}
#endif
