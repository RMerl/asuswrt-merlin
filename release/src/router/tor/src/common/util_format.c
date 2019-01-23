/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file util_format.c
 *
 * \brief Miscellaneous functions for encoding and decoding various things
 *   in base{16,32,64}.
 */

#include "orconfig.h"
#include "torlog.h"
#include "util.h"
#include "util_format.h"
#include "torint.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* Return the base32 encoded size in bytes using the source length srclen.
 * The NUL terminated byte is added as well since every base32 encoding
 * requires enough space for it. */
size_t
base32_encoded_size(size_t srclen)
{
  size_t enclen;
  enclen = CEIL_DIV(srclen*8, 5) + 1;
  tor_assert(enclen < INT_MAX && enclen > srclen);
  return enclen;
}

/** Implements base32 encoding as in RFC 4648. */
void
base32_encode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  unsigned int i, v, u;
  size_t nbits = srclen * 8;
  size_t bit;

  tor_assert(srclen < SIZE_T_CEILING/8);
  /* We need enough space for the encoded data and the extra NUL byte. */
  tor_assert(base32_encoded_size(srclen) <= destlen);
  tor_assert(destlen < SIZE_T_CEILING);

  /* Make sure we leave no uninitialized data in the destination buffer. */
  memset(dest, 0, destlen);

  for (i=0,bit=0; bit < nbits; ++i, bit+=5) {
    /* set v to the 16-bit value starting at src[bits/8], 0-padded. */
    v = ((uint8_t)src[bit/8]) << 8;
    if (bit+5<nbits)
      v += (uint8_t)src[(bit/8)+1];
    /* set u to the 5-bit value at the bit'th bit of buf. */
    u = (v >> (11-(bit%8))) & 0x1F;
    dest[i] = BASE32_CHARS[u];
  }
  dest[i] = '\0';
}

/** Implements base32 decoding as in RFC 4648.
 * Returns 0 if successful, -1 otherwise.
 */
int
base32_decode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  /* XXXX we might want to rewrite this along the lines of base64_decode, if
   * it ever shows up in the profile. */
  unsigned int i;
  size_t nbits, j, bit;
  char *tmp;
  nbits = ((srclen * 5) / 8) * 8;

  tor_assert(srclen < SIZE_T_CEILING / 5);
  tor_assert((nbits/8) <= destlen); /* We need enough space. */
  tor_assert(destlen < SIZE_T_CEILING);

  /* Make sure we leave no uninitialized data in the destination buffer. */
  memset(dest, 0, destlen);

  /* Convert base32 encoded chars to the 5-bit values that they represent. */
  tmp = tor_malloc_zero(srclen);
  for (j = 0; j < srclen; ++j) {
    if (src[j] > 0x60 && src[j] < 0x7B) tmp[j] = src[j] - 0x61;
    else if (src[j] > 0x31 && src[j] < 0x38) tmp[j] = src[j] - 0x18;
    else if (src[j] > 0x40 && src[j] < 0x5B) tmp[j] = src[j] - 0x41;
    else {
      log_warn(LD_GENERAL, "illegal character in base32 encoded string");
      tor_free(tmp);
      return -1;
    }
  }

  /* Assemble result byte-wise by applying five possible cases. */
  for (i = 0, bit = 0; bit < nbits; ++i, bit += 8) {
    switch (bit % 40) {
    case 0:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 3) +
                (((uint8_t)tmp[(bit/5)+1]) >> 2);
      break;
    case 8:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 6) +
                (((uint8_t)tmp[(bit/5)+1]) << 1) +
                (((uint8_t)tmp[(bit/5)+2]) >> 4);
      break;
    case 16:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 4) +
                (((uint8_t)tmp[(bit/5)+1]) >> 1);
      break;
    case 24:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 7) +
                (((uint8_t)tmp[(bit/5)+1]) << 2) +
                (((uint8_t)tmp[(bit/5)+2]) >> 3);
      break;
    case 32:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 5) +
                ((uint8_t)tmp[(bit/5)+1]);
      break;
    }
  }

  memset(tmp, 0, srclen); /* on the heap, this should be safe */
  tor_free(tmp);
  tmp = NULL;
  return 0;
}

#define BASE64_OPENSSL_LINELEN 64

/** Return the Base64 encoded size of <b>srclen</b> bytes of data in
 * bytes.
 *
 * If <b>flags</b>&amp;BASE64_ENCODE_MULTILINE is true, return the size
 * of the encoded output as multiline output (64 character, `\n' terminated
 * lines).
 */
size_t
base64_encode_size(size_t srclen, int flags)
{
  size_t enclen;
  tor_assert(srclen < INT_MAX);

  if (srclen == 0)
    return 0;

  enclen = ((srclen - 1) / 3) * 4 + 4;
  if (flags & BASE64_ENCODE_MULTILINE) {
    size_t remainder = enclen % BASE64_OPENSSL_LINELEN;
    enclen += enclen / BASE64_OPENSSL_LINELEN;
    if (remainder)
      enclen++;
  }
  tor_assert(enclen < INT_MAX && enclen > srclen);
  return enclen;
}

/** Internal table mapping 6 bit values to the Base64 alphabet. */
static const char base64_encode_table[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/'
};

/** Base64 encode <b>srclen</b> bytes of data from <b>src</b>.  Write
 * the result into <b>dest</b>, if it will fit within <b>destlen</b>
 * bytes. Return the number of bytes written on success; -1 if
 * destlen is too short, or other failure.
 *
 * If <b>flags</b>&amp;BASE64_ENCODE_MULTILINE is true, return encoded
 * output in multiline format (64 character, `\n' terminated lines).
 */
int
base64_encode(char *dest, size_t destlen, const char *src, size_t srclen,
              int flags)
{
  const unsigned char *usrc = (unsigned char *)src;
  const unsigned char *eous = usrc + srclen;
  char *d = dest;
  uint32_t n = 0;
  size_t linelen = 0;
  size_t enclen;
  int n_idx = 0;

  if (!src || !dest)
    return -1;

  /* Ensure that there is sufficient space, including the NUL. */
  enclen = base64_encode_size(srclen, flags);
  if (destlen < enclen + 1)
    return -1;
  if (destlen > SIZE_T_CEILING)
    return -1;
  if (enclen > INT_MAX)
    return -1;

  /* Make sure we leave no uninitialized data in the destination buffer. */
  memset(dest, 0, destlen);

  /* XXX/Yawning: If this ends up being too slow, this can be sped up
   * by separating the multiline format case and the normal case, and
   * processing 48 bytes of input at a time when newlines are desired.
   */
#define ENCODE_CHAR(ch) \
  STMT_BEGIN                                                    \
    *d++ = ch;                                                  \
    if (flags & BASE64_ENCODE_MULTILINE) {                      \
      if (++linelen % BASE64_OPENSSL_LINELEN == 0) {            \
        linelen = 0;                                            \
        *d++ = '\n';                                            \
      }                                                         \
    }                                                           \
  STMT_END

#define ENCODE_N(idx) \
  ENCODE_CHAR(base64_encode_table[(n >> ((3 - idx) * 6)) & 0x3f])

#define ENCODE_PAD() ENCODE_CHAR('=')

  /* Iterate over all the bytes in src.  Each one will add 8 bits to the
   * value we're encoding.  Accumulate bits in <b>n</b>, and whenever we
   * have 24 bits, batch them into 4 bytes and flush those bytes to dest.
   */
  for ( ; usrc < eous; ++usrc) {
    n = (n << 8) | *usrc;
    if ((++n_idx) == 3) {
      ENCODE_N(0);
      ENCODE_N(1);
      ENCODE_N(2);
      ENCODE_N(3);
      n_idx = 0;
      n = 0;
    }
  }
  switch (n_idx) {
  case 0:
    /* 0 leftover bits, no pading to add. */
    break;
  case 1:
    /* 8 leftover bits, pad to 12 bits, write the 2 6-bit values followed
     * by 2 padding characters.
     */
    n <<= 4;
    ENCODE_N(2);
    ENCODE_N(3);
    ENCODE_PAD();
    ENCODE_PAD();
    break;
  case 2:
    /* 16 leftover bits, pad to 18 bits, write the 3 6-bit values followed
     * by 1 padding character.
     */
    n <<= 2;
    ENCODE_N(1);
    ENCODE_N(2);
    ENCODE_N(3);
    ENCODE_PAD();
    break;
  default:
    /* Something went catastrophically wrong. */
    tor_fragile_assert(); // LCOV_EXCL_LINE
    return -1;
  }

#undef ENCODE_N
#undef ENCODE_PAD
#undef ENCODE_CHAR

  /* Multiline output always includes at least one newline. */
  if (flags & BASE64_ENCODE_MULTILINE && linelen != 0)
    *d++ = '\n';

  tor_assert(d - dest == (ptrdiff_t)enclen);

  *d++ = '\0'; /* NUL terminate the output. */

  return (int) enclen;
}

/** As base64_encode, but do not add any internal spaces or external padding
 * to the output stream. */
int
base64_encode_nopad(char *dest, size_t destlen,
                    const uint8_t *src, size_t srclen)
{
  int n = base64_encode(dest, destlen, (const char*) src, srclen, 0);
  if (n <= 0)
    return n;
  tor_assert((size_t)n < destlen && dest[n] == 0);
  char *in, *out;
  in = out = dest;
  while (*in) {
    if (*in == '=' || *in == '\n') {
      ++in;
    } else {
      *out++ = *in++;
    }
  }
  *out = 0;

  tor_assert(out - dest <= INT_MAX);

  return (int)(out - dest);
}

/** As base64_decode, but do not require any padding on the input */
int
base64_decode_nopad(uint8_t *dest, size_t destlen,
                    const char *src, size_t srclen)
{
  if (srclen > SIZE_T_CEILING - 4)
    return -1;
  char *buf = tor_malloc(srclen + 4);
  memcpy(buf, src, srclen+1);
  size_t buflen;
  switch (srclen % 4)
    {
    case 0:
    default:
      buflen = srclen;
      break;
    case 1:
      tor_free(buf);
      return -1;
    case 2:
      memcpy(buf+srclen, "==", 3);
      buflen = srclen + 2;
      break;
    case 3:
      memcpy(buf+srclen, "=", 2);
      buflen = srclen + 1;
      break;
  }
  int n = base64_decode((char*)dest, destlen, buf, buflen);
  tor_free(buf);
  return n;
}

#undef BASE64_OPENSSL_LINELEN

/** @{ */
/** Special values used for the base64_decode_table */
#define X 255
#define SP 64
#define PAD 65
/** @} */
/** Internal table mapping byte values to what they represent in base64.
 * Numbers 0..63 are 6-bit integers.  SPs are spaces, and should be
 * skipped.  Xs are invalid and must not appear in base64. PAD indicates
 * end-of-string. */
static const uint8_t base64_decode_table[256] = {
  X, X, X, X, X, X, X, X, X, SP, SP, SP, X, SP, X, X, /* */
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  SP, X, X, X, X, X, X, X, X, X, X, 62, X, X, X, 63,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, X, X, X, PAD, X, X,
  X, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, X, X, X, X, X,
  X, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
};

/** Base64 decode <b>srclen</b> bytes of data from <b>src</b>.  Write
 * the result into <b>dest</b>, if it will fit within <b>destlen</b>
 * bytes.  Return the number of bytes written on success; -1 if
 * destlen is too short, or other failure.
 *
 * NOTE 1: destlen is checked conservatively, as though srclen contained no
 * spaces or padding.
 *
 * NOTE 2: This implementation does not check for the correct number of
 * padding "=" characters at the end of the string, and does not check
 * for internal padding characters.
 */
int
base64_decode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  const char *eos = src+srclen;
  uint32_t n=0;
  int n_idx=0;
  char *dest_orig = dest;

  /* Max number of bits == srclen*6.
   * Number of bytes required to hold all bits == (srclen*6)/8.
   * Yes, we want to round down: anything that hangs over the end of a
   * byte is padding. */
  if (destlen < (srclen*3)/4)
    return -1;
  if (destlen > SIZE_T_CEILING)
    return -1;

  /* Make sure we leave no uninitialized data in the destination buffer. */
  memset(dest, 0, destlen);

  /* Iterate over all the bytes in src.  Each one will add 0 or 6 bits to the
   * value we're decoding.  Accumulate bits in <b>n</b>, and whenever we have
   * 24 bits, batch them into 3 bytes and flush those bytes to dest.
   */
  for ( ; src < eos; ++src) {
    unsigned char c = (unsigned char) *src;
    uint8_t v = base64_decode_table[c];
    switch (v) {
      case X:
        /* This character isn't allowed in base64. */
        return -1;
      case SP:
        /* This character is whitespace, and has no effect. */
        continue;
      case PAD:
        /* We've hit an = character: the data is over. */
        goto end_of_loop;
      default:
        /* We have an actual 6-bit value.  Append it to the bits in n. */
        n = (n<<6) | v;
        if ((++n_idx) == 4) {
          /* We've accumulated 24 bits in n. Flush them. */
          *dest++ = (n>>16);
          *dest++ = (n>>8) & 0xff;
          *dest++ = (n) & 0xff;
          n_idx = 0;
          n = 0;
        }
    }
  }
 end_of_loop:
  /* If we have leftover bits, we need to cope. */
  switch (n_idx) {
    case 0:
    default:
      /* No leftover bits.  We win. */
      break;
    case 1:
      /* 6 leftover bits. That's invalid; we can't form a byte out of that. */
      return -1;
    case 2:
      /* 12 leftover bits: The last 4 are padding and the first 8 are data. */
      *dest++ = n >> 4;
      break;
    case 3:
      /* 18 leftover bits: The last 2 are padding and the first 16 are data. */
      *dest++ = n >> 10;
      *dest++ = n >> 2;
  }

  tor_assert((dest-dest_orig) <= (ssize_t)destlen);
  tor_assert((dest-dest_orig) <= INT_MAX);

  return (int)(dest-dest_orig);
}
#undef X
#undef SP
#undef PAD

/** Encode the <b>srclen</b> bytes at <b>src</b> in a NUL-terminated,
 * uppercase hexadecimal string; store it in the <b>destlen</b>-byte buffer
 * <b>dest</b>.
 */
void
base16_encode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  const char *end;
  char *cp;

  tor_assert(destlen >= srclen*2+1);
  tor_assert(destlen < SIZE_T_CEILING);

  /* Make sure we leave no uninitialized data in the destination buffer. */
  memset(dest, 0, destlen);

  cp = dest;
  end = src+srclen;
  while (src<end) {
    *cp++ = "0123456789ABCDEF"[ (*(const uint8_t*)src) >> 4 ];
    *cp++ = "0123456789ABCDEF"[ (*(const uint8_t*)src) & 0xf ];
    ++src;
  }
  *cp = '\0';
}

/** Helper: given a hex digit, return its value, or -1 if it isn't hex. */
static inline int
hex_decode_digit_(char c)
{
  switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'A': case 'a': return 10;
    case 'B': case 'b': return 11;
    case 'C': case 'c': return 12;
    case 'D': case 'd': return 13;
    case 'E': case 'e': return 14;
    case 'F': case 'f': return 15;
    default:
      return -1;
  }
}

/** Helper: given a hex digit, return its value, or -1 if it isn't hex. */
int
hex_decode_digit(char c)
{
  return hex_decode_digit_(c);
}

/** Given a hexadecimal string of <b>srclen</b> bytes in <b>src</b>, decode
 * it and store the result in the <b>destlen</b>-byte buffer at <b>dest</b>.
 * Return the number of bytes decoded on success, -1 on failure. If
 * <b>destlen</b> is greater than INT_MAX or less than half of
 * <b>srclen</b>, -1 is returned. */
int
base16_decode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  const char *end;
  char *dest_orig = dest;
  int v1,v2;

  if ((srclen % 2) != 0)
    return -1;
  if (destlen < srclen/2 || destlen > INT_MAX)
    return -1;

  /* Make sure we leave no uninitialized data in the destination buffer. */
  memset(dest, 0, destlen);

  end = src+srclen;
  while (src<end) {
    v1 = hex_decode_digit_(*src);
    v2 = hex_decode_digit_(*(src+1));
    if (v1<0||v2<0)
      return -1;
    *(uint8_t*)dest = (v1<<4)|v2;
    ++dest;
    src+=2;
  }

  tor_assert((dest-dest_orig) <= (ptrdiff_t) destlen);

  return (int) (dest-dest_orig);
}

