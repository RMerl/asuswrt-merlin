/** libkeccak-tiny
 *
 * A single-file implementation of SHA-3 and SHAKE.
 *
 * Implementor: David Leon Gil
 * License: CC0, attribution kindly requested. Blame taken too,
 * but not liability.
 */
#include "keccak-tiny.h"

#include <string.h>
#include "crypto.h"

/******** Endianness conversion helpers ********/

static inline uint64_t
loadu64le(const unsigned char *x) {
  uint64_t r = 0;
  size_t i;

  for (i = 0; i < 8; ++i) {
    r |= (uint64_t)x[i] << 8 * i;
  }
  return r;
}

static inline void
storeu64le(uint8_t *x, uint64_t u) {
  size_t i;

  for(i=0; i<8; ++i) {
    x[i] = u;
    u >>= 8;
  }
}

/******** The Keccak-f[1600] permutation ********/

/*** Constants. ***/
static const uint8_t rho[24] = \
  { 1,  3,   6, 10, 15, 21,
    28, 36, 45, 55,  2, 14,
    27, 41, 56,  8, 25, 43,
    62, 18, 39, 61, 20, 44};
static const uint8_t pi[24] = \
  {10,  7, 11, 17, 18, 3,
    5, 16,  8, 21, 24, 4,
   15, 23, 19, 13, 12, 2,
   20, 14, 22,  9, 6,  1};
static const uint64_t RC[24] = \
  {1ULL, 0x8082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
   0x808bULL, 0x80000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
   0x8aULL, 0x88ULL, 0x80008009ULL, 0x8000000aULL,
   0x8000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
   0x8000000000008002ULL, 0x8000000000000080ULL, 0x800aULL, 0x800000008000000aULL,
   0x8000000080008081ULL, 0x8000000000008080ULL, 0x80000001ULL, 0x8000000080008008ULL};

/*** Helper macros to unroll the permutation. ***/
#define rol(x, s) (((x) << s) | ((x) >> (64 - s)))
#define REPEAT6(e) e e e e e e
#define REPEAT24(e) REPEAT6(e e e e)
#define REPEAT5(e) e e e e e
#define FOR5(v, s, e) \
  v = 0;            \
  REPEAT5(e; v += s;)

/*** Keccak-f[1600] ***/
static inline void keccakf(void* state) {
  uint64_t* a = (uint64_t*)state;
  uint64_t b[5] = {0};
  uint64_t t = 0;
  uint8_t x, y, i = 0;

  REPEAT24(
      // Theta
      FOR5(x, 1,
           b[x] = 0;
           FOR5(y, 5,
                b[x] ^= a[x + y]; ))
      FOR5(x, 1,
           FOR5(y, 5,
                a[y + x] ^= b[(x + 4) % 5] ^ rol(b[(x + 1) % 5], 1); ))
      // Rho and pi
      t = a[1];
      x = 0;
      REPEAT24(b[0] = a[pi[x]];
               a[pi[x]] = rol(t, rho[x]);
               t = b[0];
               x++; )
      // Chi
      FOR5(y,
         5,
         FOR5(x, 1,
              b[x] = a[y + x];)
         FOR5(x, 1,
              a[y + x] = b[x] ^ ((~b[(x + 1) % 5]) & b[(x + 2) % 5]); ))
      // Iota
      a[0] ^= RC[i];
      i++; )
}

/******** The FIPS202-defined functions. ********/

/*** Some helper macros. ***/

// `xorin` modified to handle Big Endian systems, `buf` being unaligned on
// systems that care about such things.  Assumes that len is a multiple of 8,
// which is always true for the rates we use, and the modified finalize.
static inline void
xorin8(uint8_t *dst, const uint8_t *src, size_t len) {
  uint64_t* a = (uint64_t*)dst; // Always aligned.
  for (size_t i = 0; i < len; i += 8) {
    a[i/8] ^= loadu64le(src + i);
  }
}

// `setout` likewise modified to handle Big Endian systems.  Assumes that len
// is a multiple of 8, which is true for every rate we use.
static inline void
setout8(const uint8_t *src, uint8_t *dst, size_t len) {
  const uint64_t *si = (const uint64_t*)src; // Always aligned.
  for (size_t i = 0; i < len; i+= 8) {
    storeu64le(dst+i, si[i/8]);
  }
}

#define P keccakf
#define Plen KECCAK_MAX_RATE

#define KECCAK_DELIM_DIGEST 0x06
#define KECCAK_DELIM_XOF 0x1f

// Fold P*F over the full blocks of an input.
#define foldP(I, L, F) \
  while (L >= s->rate) {  \
    F(s->a, I, s->rate);  \
    P(s->a);              \
    I += s->rate;         \
    L -= s->rate;         \
  }

static inline void
keccak_absorb_blocks(keccak_state *s, const uint8_t *buf, size_t nr_blocks)
{
  size_t blen = nr_blocks * s->rate;
  foldP(buf, blen, xorin8);
}

static int
keccak_update(keccak_state *s, const uint8_t *buf, size_t len)
{
  if (s->finalized)
    return -1;
  if ((buf == NULL) && len != 0)
    return -1;

  size_t remaining = len;
  while (remaining > 0) {
    if (s->offset == 0) {
      const size_t blocks = remaining / s->rate;
      size_t direct_bytes = blocks * s->rate;
      if (direct_bytes > 0) {
        keccak_absorb_blocks(s, buf, blocks);
        remaining -= direct_bytes;
        buf += direct_bytes;
      }
    }

    const size_t buf_avail = s->rate - s->offset;
    const size_t buf_bytes = (buf_avail > remaining) ? remaining : buf_avail;
    if (buf_bytes > 0) {
      memcpy(&s->block[s->offset], buf, buf_bytes);
      s->offset += buf_bytes;
      remaining -= buf_bytes;
      buf += buf_bytes;
    }
    if (s->offset == s->rate) {
      keccak_absorb_blocks(s, s->block, 1);
      s->offset = 0;
    }
  }
  return 0;
}

static void
keccak_finalize(keccak_state *s)
{
  // Xor in the DS and pad frame.
  s->block[s->offset++] = s->delim; // DS.
  for (size_t i = s->offset; i < s->rate; i++) {
    s->block[i] = 0;
  }
  s->block[s->rate - 1] |= 0x80; // Pad frame.

  // Xor in the last block.
  xorin8(s->a, s->block, s->rate);

  memwipe(s->block, 0, sizeof(s->block));
  s->finalized = 1;
  s->offset = s->rate;
}

static inline void
keccak_squeeze_blocks(keccak_state *s, uint8_t *out, size_t nr_blocks)
{
  for (size_t n = 0; n < nr_blocks; n++) {
    keccakf(s->a);
    setout8(s->a, out, s->rate);
    out += s->rate;
  }
}

static int
keccak_squeeze(keccak_state *s, uint8_t *out, size_t outlen)
{
  if (!s->finalized)
    return -1;

  size_t remaining = outlen;
  while (remaining > 0) {
    if (s->offset == s->rate) {
      const size_t blocks = remaining / s->rate;
      const size_t direct_bytes = blocks * s->rate;
      if (blocks > 0) {
        keccak_squeeze_blocks(s, out, blocks);
        out += direct_bytes;
        remaining -= direct_bytes;
      }

      if (remaining > 0) {
        keccak_squeeze_blocks(s, s->block, 1);
        s->offset = 0;
      }
    }

    const size_t buf_bytes = s->rate - s->offset;
    const size_t indirect_bytes = (buf_bytes > remaining) ? remaining : buf_bytes;
    if (indirect_bytes > 0) {
      memcpy(out, &s->block[s->offset], indirect_bytes);
      out += indirect_bytes;
      s->offset += indirect_bytes;
      remaining -= indirect_bytes;
    }
  }
  return 0;
}

int
keccak_digest_init(keccak_state *s, size_t bits)
{
  if (s == NULL)
    return -1;
  if (bits != 224 && bits != 256 && bits != 384 && bits != 512)
    return -1;

  keccak_cleanse(s);
  s->rate = KECCAK_RATE(bits);
  s->delim = KECCAK_DELIM_DIGEST;
  return 0;
}

int
keccak_digest_update(keccak_state *s, const uint8_t *buf, size_t len)
{
  if (s == NULL)
    return -1;
  if (s->delim != KECCAK_DELIM_DIGEST)
    return -1;

  return keccak_update(s, buf, len);
}

int
keccak_digest_sum(const keccak_state *s, uint8_t *out, size_t outlen)
{
  if (s == NULL)
    return -1;
  if (s->delim != KECCAK_DELIM_DIGEST)
    return -1;
  if (out == NULL || outlen > 4 * (KECCAK_MAX_RATE - s->rate) / 8)
    return -1;

  // Work in a copy so that incremental/rolling hashes are easy.
  keccak_state s_tmp;
  keccak_clone(&s_tmp, s);
  keccak_finalize(&s_tmp);
  int ret = keccak_squeeze(&s_tmp, out, outlen);
  keccak_cleanse(&s_tmp);
  return ret;
}

int
keccak_xof_init(keccak_state *s, size_t bits)
{
  if (s == NULL)
    return -1;
  if (bits != 128 && bits != 256)
    return -1;

  keccak_cleanse(s);
  s->rate = KECCAK_RATE(bits);
  s->delim = KECCAK_DELIM_XOF;
  return 0;
}

int
keccak_xof_absorb(keccak_state *s, const uint8_t *buf, size_t len)
{
  if (s == NULL)
    return -1;
  if (s->delim != KECCAK_DELIM_XOF)
    return -1;

  return keccak_update(s, buf, len);
}

int
keccak_xof_squeeze(keccak_state *s, uint8_t *out, size_t outlen)
{
  if (s == NULL)
    return -1;
  if (s->delim != KECCAK_DELIM_XOF)
    return -1;

  if (!s->finalized)
    keccak_finalize(s);

  return keccak_squeeze(s, out, outlen);
}

void
keccak_clone(keccak_state *out, const keccak_state *in)
{
  memcpy(out, in, sizeof(keccak_state));
}

void
keccak_cleanse(keccak_state *s)
{
  memwipe(s, 0, sizeof(keccak_state));
}

/** The sponge-based hash construction. **/
static inline int hash(uint8_t* out, size_t outlen,
                       const uint8_t* in, size_t inlen,
                       size_t bits, uint8_t delim) {
  if ((out == NULL) || ((in == NULL) && inlen != 0)) {
    return -1;
  }

  int ret = 0;
  keccak_state s;
  keccak_cleanse(&s);

  switch (delim) {
    case KECCAK_DELIM_DIGEST:
      ret |= keccak_digest_init(&s, bits);
      ret |= keccak_digest_update(&s, in, inlen);
      // Use the internal API instead of sum to avoid the memcpy.
      keccak_finalize(&s);
      ret |= keccak_squeeze(&s, out, outlen);
      break;
    case KECCAK_DELIM_XOF:
      ret |= keccak_xof_init(&s, bits);
      ret |= keccak_xof_absorb(&s, in, inlen);
      ret |= keccak_xof_squeeze(&s, out, outlen);
      break;
    default:
      return -1;
  }
  keccak_cleanse(&s);
  return ret;
}

/*** Helper macros to define SHA3 and SHAKE instances. ***/
#define defshake(bits)                                            \
  int shake##bits(uint8_t* out, size_t outlen,                    \
                  const uint8_t* in, size_t inlen) {              \
    return hash(out, outlen, in, inlen, bits, KECCAK_DELIM_XOF);  \
  }
#define defsha3(bits)                                             \
  int sha3_##bits(uint8_t* out, size_t outlen,                    \
                  const uint8_t* in, size_t inlen) {              \
    if (outlen > (bits/8)) {                                      \
      return -1;                                                  \
    }                                                             \
    return hash(out, outlen, in, inlen, bits, KECCAK_DELIM_DIGEST);  \
  }

/*** FIPS202 SHAKE VOFs ***/
defshake(128)
defshake(256)

/*** FIPS202 SHA3 FOFs ***/
defsha3(224)
defsha3(256)
defsha3(384)
defsha3(512)
