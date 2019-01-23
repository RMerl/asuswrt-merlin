/* mszip decompression - based on cabextract.c code from
 * Stuart Caie
 *
 * adapted for Samba by Andrew Tridgell and Stefan Metzmacher 2005
 *
 * (C) 2000-2001 Stuart Caie <kyzer@4u.net>
 * reaktivate-specifics by Malte Starostik <malte@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "../compression/mszip.h"

/*--------------------------------------------------------------------------*/
/* our archiver information / state */

/* MSZIP stuff */
#define ZIPWSIZE 	0x8000  /* window size */
#define ZIPLBITS	9	/* bits in base literal/length lookup table */
#define ZIPDBITS	6	/* bits in base distance lookup table */
#define ZIPBMAX		16      /* maximum bit length of any code */
#define ZIPN_MAX	288     /* maximum number of codes in any set */

struct Ziphuft {
  uint8_t e;                /* number of extra bits or operation */
  uint8_t b;                /* number of bits in this code or subcode */
  union {
    uint16_t n;              /* literal, length base, or distance base */
    struct Ziphuft *t;    /* pointer to next level of table */
  } v;
};

struct ZIPstate {
    uint32_t window_posn;     /* current offset within the window        */
    uint32_t bb;              /* bit buffer */
    uint32_t bk;              /* bits in bit buffer */
    uint32_t ll[288+32];	   /* literal/length and distance code lengths */
    uint32_t c[ZIPBMAX+1];    /* bit length count table */
    int32_t  lx[ZIPBMAX+1];   /* memory for l[-1..ZIPBMAX-1] */
    struct Ziphuft *u[ZIPBMAX];         	/* table stack */
    uint32_t v[ZIPN_MAX];     /* values in order of bit length */
    uint32_t x[ZIPBMAX+1];    /* bit offsets, then code stack */
    uint8_t *inpos;
};

/* generic stuff */
#define CAB(x) (decomp_state->x)
#define ZIP(x) (decomp_state->methods.zip.x)

/* CAB data blocks are <= 32768 bytes in uncompressed form. Uncompressed
 * blocks have zero growth. MSZIP guarantees that it won't grow above
 * uncompressed size by more than 12 bytes. LZX guarantees it won't grow
 * more than 6144 bytes.
 */
#define CAB_BLOCKMAX (32768)
#define CAB_INPUTMAX (CAB_BLOCKMAX+6144)

struct decomp_state {
  struct folder *current; /* current folder we're extracting from  */
  uint32_t offset;           /* uncompressed offset within folder     */
  uint8_t *outpos;          /* (high level) start of data to use up  */
  uint16_t outlen;           /* (high level) amount of data to use up */
  uint16_t split;            /* at which split in current folder?     */
  int (*decompress)(int, int); /* the chosen compression func      */
  uint8_t inbuf[CAB_INPUTMAX+2]; /* +2 for lzx bitbuffer overflows!  */
  uint8_t outbuf[CAB_BLOCKMAX];
  union {
    struct ZIPstate zip;
  } methods;
};


/* MSZIP decruncher */

/* Dirk Stoecker wrote the ZIP decoder, based on the InfoZip deflate code */

/* Tables for deflate from PKZIP's appnote.txt. */
static const uint8_t Zipborder[] = /* Order of the bit length code lengths */
{ 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static const uint16_t Zipcplens[] = /* Copy lengths for literal codes 257..285 */
{ 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
static const uint16_t Zipcplext[] = /* Extra bits for literal codes 257..285 */
{ 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
  4, 5, 5, 5, 5, 0, 99, 99}; /* 99==invalid */
static const uint16_t Zipcpdist[] = /* Copy offsets for distance codes 0..29 */
{ 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint16_t Zipcpdext[] = /* Extra bits for distance codes */
{ 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
10, 11, 11, 12, 12, 13, 13};

/* And'ing with Zipmask[n] masks the lower n bits */
static const uint16_t Zipmask[17] = {
 0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

#define ZIPNEEDBITS(n) {while(k<(n)){int32_t c=*(ZIP(inpos)++);\
    b|=((uint32_t)c)<<k;k+=8;}}
#define ZIPDUMPBITS(n) {b>>=(n);k-=(n);}

static void Ziphuft_free(struct Ziphuft *t)
{
  register struct Ziphuft *p, *q;

  /* Go through linked list, freeing from the allocated (t[-1]) address. */
  p = t;
  while (p != (struct Ziphuft *)NULL)
  {
    q = (--p)->v.t;
    free(p);
    p = q;
  }
}

static int32_t Ziphuft_build(struct decomp_state *decomp_state,
			  uint32_t *b, uint32_t n, uint32_t s, const uint16_t *d, const uint16_t *e,
			  struct Ziphuft **t, int32_t *m)
{
  uint32_t a;                   	/* counter for codes of length k */
  uint32_t el;                  	/* length of EOB code (value 256) */
  uint32_t f;                   	/* i repeats in table every f entries */
  int32_t g;                    	/* maximum code length */
  int32_t h;                    	/* table level */
  register uint32_t i;          	/* counter, current code */
  register uint32_t j;          	/* counter */
  register int32_t k;           	/* number of bits in current code */
  int32_t *l;               	/* stack of bits per table */
  register uint32_t *p;         	/* pointer into ZIP(c)[],ZIP(b)[],ZIP(v)[] */
  register struct Ziphuft *q;   /* points to current table */
  struct Ziphuft r;             /* table entry for structure assignment */
  register int32_t w;              /* bits before this table == (l * h) */
  uint32_t *xp;                 	/* pointer into x */
  int32_t y;                       /* number of dummy codes added */
  uint32_t z;                   	/* number of entries in current table */

  l = ZIP(lx)+1;

  /* Generate counts for each bit length */
  el = n > 256 ? b[256] : ZIPBMAX; /* set length of EOB code, if any */

  for(i = 0; i < ZIPBMAX+1; ++i)
    ZIP(c)[i] = 0;
  p = b;  i = n;
  do
  {
    ZIP(c)[*p]++; p++;               /* assume all entries <= ZIPBMAX */
  } while (--i);
  if (ZIP(c)[0] == n)                /* null input--all zero length codes */
  {
    *t = (struct Ziphuft *)NULL;
    *m = 0;
    return 0;
  }

  /* Find minimum and maximum length, bound *m by those */
  for (j = 1; j <= ZIPBMAX; j++)
    if (ZIP(c)[j])
      break;
  k = j;                        /* minimum code length */
  if ((uint32_t)*m < j)
    *m = j;
  for (i = ZIPBMAX; i; i--)
    if (ZIP(c)[i])
      break;
  g = i;                        /* maximum code length */
  if ((uint32_t)*m > i)
    *m = i;

  /* Adjust last length count to fill out codes, if needed */
  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= ZIP(c)[j]) < 0)
      return 2;                 /* bad input: more codes than bits */
  if ((y -= ZIP(c)[i]) < 0)
    return 2;
  ZIP(c)[i] += y;

  /* Generate starting offsets int32_to the value table for each length */
  ZIP(x)[1] = j = 0;
  p = ZIP(c) + 1;  xp = ZIP(x) + 2;
  while (--i)
  {                 /* note that i == g from above */
    *xp++ = (j += *p++);
  }

  /* Make a table of values in order of bit lengths */
  p = b;  i = 0;
  do{
    if ((j = *p++) != 0)
      ZIP(v)[ZIP(x)[j]++] = i;
  } while (++i < n);


  /* Generate the Huffman codes and for each, make the table entries */
  ZIP(x)[0] = i = 0;                 /* first Huffman code is zero */
  p = ZIP(v);                        /* grab values in bit order */
  h = -1;                       /* no tables yet--level -1 */
  w = l[-1] = 0;                /* no bits decoded yet */
  ZIP(u)[0] = (struct Ziphuft *)NULL;   /* just to keep compilers happy */
  q = (struct Ziphuft *)NULL;      /* ditto */
  z = 0;                        /* ditto */

  /* go through the bit lengths (k already is bits in shortest code) */
  for (; k <= g; k++)
  {
    a = ZIP(c)[k];
    while (a--)
    {
      /* here i is the Huffman code of length k bits for value *p */
      /* make tables up to required level */
      while (k > w + l[h])
      {
        w += l[h++];            /* add bits already decoded */

        /* compute minimum size table less than or equal to *m bits */
        z = (z = g - w) > (uint32_t)*m ? *m : z;        /* upper limit */
        if ((f = 1 << (j = k - w)) > a + 1)     /* try a k-w bit table */
        {                       /* too few codes for k-w bit table */
          f -= a + 1;           /* deduct codes from patterns left */
          xp = ZIP(c) + k;
          while (++j < z)       /* try smaller tables up to z bits */
          {
            if ((f <<= 1) <= *++xp)
              break;            /* enough codes to use up j bits */
            f -= *xp;           /* else deduct codes from patterns */
          }
        }
        if ((uint32_t)w + j > el && (uint32_t)w < el)
          j = el - w;           /* make EOB code end at table */
        z = 1 << j;             /* table entries for j-bit table */
        l[h] = j;               /* set table size in stack */

        /* allocate and link in new table */
        if (!(q = (struct Ziphuft *)SMB_MALLOC((z + 1)*sizeof(struct Ziphuft))))
        {
          if(h)
            Ziphuft_free(ZIP(u)[0]);
          return 3;             /* not enough memory */
        }
        *t = q + 1;             /* link to list for Ziphuft_free() */
        *(t = &(q->v.t)) = (struct Ziphuft *)NULL;
        ZIP(u)[h] = ++q;             /* table starts after link */

        /* connect to last table, if there is one */
        if (h)
        {
          ZIP(x)[h] = i;             /* save pattern for backing up */
          r.b = (uint8_t)l[h-1];    /* bits to dump before this table */
          r.e = (uint8_t)(16 + j);  /* bits in this table */
          r.v.t = q;            /* pointer to this table */
          j = (i & ((1 << w) - 1)) >> (w - l[h-1]);
          ZIP(u)[h-1][j] = r;        /* connect to last table */
        }
      }

      /* set up table entry in r */
      r.b = (uint8_t)(k - w);
      if (p >= ZIP(v) + n)
        r.e = 99;               /* out of values--invalid code */
      else if (*p < s)
      {
        r.e = (uint8_t)(*p < 256 ? 16 : 15);    /* 256 is end-of-block code */
        r.v.n = *p++;           /* simple code is just the value */
      }
      else
      {
        r.e = (uint8_t)e[*p - s];   /* non-simple--look up in lists */
        r.v.n = d[*p++ - s];
      }

      /* fill code-like entries with r */
      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      /* backwards increment the k-bit code i */
      for (j = 1 << (k - 1); i & j; j >>= 1)
        i ^= j;
      i ^= j;

      /* backup over finished tables */
      while ((i & ((1 << w) - 1)) != ZIP(x)[h])
        w -= l[--h];            /* don't need to update q */
    }
  }

  /* return actual size of base table */
  *m = l[0];

  /* Return true (1) if we were given an incomplete table */
  return y != 0 && g != 1;
}

static int32_t Zipinflate_codes(struct decomp_state *decomp_state,
			     struct Ziphuft *tl, struct Ziphuft *td,
			     int32_t bl, int32_t bd)
{
  register uint32_t e;  /* table entry flag/number of extra bits */
  uint32_t n, d;        /* length and index for copy */
  uint32_t w;           /* current window position */
  struct Ziphuft *t; /* pointer to table entry */
  uint32_t ml, md;      /* masks for bl and bd bits */
  register uint32_t b;  /* bit buffer */
  register uint32_t k;  /* number of bits in bit buffer */

  DEBUG(10,("Zipinflate_codes\n"));

  /* make local copies of globals */
  b = ZIP(bb);                       /* initialize bit buffer */
  k = ZIP(bk);
  w = ZIP(window_posn);                       /* initialize window position */

  /* inflate the coded data */
  ml = Zipmask[bl];           	/* precompute masks for speed */
  md = Zipmask[bd];

  for(;;)
  {
    ZIPNEEDBITS((uint32_t)bl)
    if((e = (t = tl + ((uint32_t)b & ml))->e) > 16)
      do
      {
        if (e == 99)
          return 1;
        ZIPDUMPBITS(t->b)
        e -= 16;
        ZIPNEEDBITS(e)
      } while ((e = (t = t->v.t + ((uint32_t)b & Zipmask[e]))->e) > 16);
    ZIPDUMPBITS(t->b)
    if (w >= CAB_BLOCKMAX) break;
    if (e == 16)                /* then it's a literal */
      CAB(outbuf)[w++] = (uint8_t)t->v.n;
    else                        /* it's an EOB or a length */
    {
      /* exit if end of block */
      if(e == 15)
        break;

      /* get length of block to copy */
      ZIPNEEDBITS(e)
      n = t->v.n + ((uint32_t)b & Zipmask[e]);
      ZIPDUMPBITS(e);

      /* decode distance of block to copy */
      ZIPNEEDBITS((uint32_t)bd)
      if ((e = (t = td + ((uint32_t)b & md))->e) > 16)
        do {
          if (e == 99)
            return 1;
          ZIPDUMPBITS(t->b)
          e -= 16;
          ZIPNEEDBITS(e)
        } while ((e = (t = t->v.t + ((uint32_t)b & Zipmask[e]))->e) > 16);
      ZIPDUMPBITS(t->b)
      ZIPNEEDBITS(e)
      d = w - t->v.n - ((uint32_t)b & Zipmask[e]);
      ZIPDUMPBITS(e)
      do
      {
        n -= (e = (e = ZIPWSIZE - ((d &= ZIPWSIZE-1) > w ? d : w)) > n ?n:e);
        do
        {
          CAB(outbuf)[w++] = CAB(outbuf)[d++];
        } while (--e);
      } while (n);
    }
  }

  /* restore the globals from the locals */
  ZIP(window_posn) = w;              /* restore global window pointer */
  ZIP(bb) = b;                       /* restore global bit buffer */
  ZIP(bk) = k;

  /* done */
  return 0;
}

/* "decompress" an inflated type 0 (stored) block. */
static int32_t Zipinflate_stored(struct decomp_state *decomp_state)
{
  uint32_t n;           /* number of bytes in block */
  uint32_t w;           /* current window position */
  register uint32_t b;  /* bit buffer */
  register uint32_t k;  /* number of bits in bit buffer */

  /* make local copies of globals */
  b = ZIP(bb);                       /* initialize bit buffer */
  k = ZIP(bk);
  w = ZIP(window_posn);              /* initialize window position */

  /* go to byte boundary */
  n = k & 7;
  ZIPDUMPBITS(n);

  /* get the length and its complement */
  ZIPNEEDBITS(16)
  n = ((uint32_t)b & 0xffff);
  ZIPDUMPBITS(16)
  ZIPNEEDBITS(16)
  if (n != (uint32_t)((~b) & 0xffff))
    return 1;                   /* error in compressed data */
  ZIPDUMPBITS(16)

  /* read and output the compressed data */
  while(n--)
  {
    ZIPNEEDBITS(8)
    CAB(outbuf)[w++] = (uint8_t)b;
    ZIPDUMPBITS(8)
  }

  /* restore the globals from the locals */
  ZIP(window_posn) = w;              /* restore global window pointer */
  ZIP(bb) = b;                       /* restore global bit buffer */
  ZIP(bk) = k;
  return 0;
}

static int32_t Zipinflate_fixed(struct decomp_state *decomp_state)
{
  struct Ziphuft *fixed_tl;
  struct Ziphuft *fixed_td;
  int32_t fixed_bl, fixed_bd;
  int32_t i;                /* temporary variable */
  uint32_t *l;

  l = ZIP(ll);

  /* literal table */
  for(i = 0; i < 144; i++)
    l[i] = 8;
  for(; i < 256; i++)
    l[i] = 9;
  for(; i < 280; i++)
    l[i] = 7;
  for(; i < 288; i++)          /* make a complete, but wrong code set */
    l[i] = 8;
  fixed_bl = 7;
  if((i = Ziphuft_build(decomp_state, l, 288, 257, Zipcplens, Zipcplext, &fixed_tl, &fixed_bl)))
    return i;

  /* distance table */
  for(i = 0; i < 30; i++)      /* make an incomplete code set */
    l[i] = 5;
  fixed_bd = 5;
  if((i = Ziphuft_build(decomp_state, l, 30, 0, Zipcpdist, Zipcpdext, &fixed_td, &fixed_bd)) > 1)
  {
    Ziphuft_free(fixed_tl);
    return i;
  }

  /* decompress until an end-of-block code */
  i = Zipinflate_codes(decomp_state, fixed_tl, fixed_td, fixed_bl, fixed_bd);

  Ziphuft_free(fixed_td);
  Ziphuft_free(fixed_tl);
  return i;
}

/* decompress an inflated type 2 (dynamic Huffman codes) block. */
static int32_t Zipinflate_dynamic(struct decomp_state *decomp_state)
{
  int32_t i;          	/* temporary variables */
  uint32_t j;
  uint32_t *ll;
  uint32_t l;           	/* last length */
  uint32_t m;           	/* mask for bit lengths table */
  uint32_t n;           	/* number of lengths to get */
  struct Ziphuft *tl;      /* literal/length code table */
  struct Ziphuft *td;      /* distance code table */
  int32_t bl;              /* lookup bits for tl */
  int32_t bd;              /* lookup bits for td */
  uint32_t nb;          	/* number of bit length codes */
  uint32_t nl;          	/* number of literal/length codes */
  uint32_t nd;          	/* number of distance codes */
  register uint32_t b;     /* bit buffer */
  register uint32_t k;	/* number of bits in bit buffer */

  /* make local bit buffer */
  b = ZIP(bb);
  k = ZIP(bk);
  ll = ZIP(ll);

  /* read in table lengths */
  ZIPNEEDBITS(5)
  nl = 257 + ((uint32_t)b & 0x1f);      /* number of literal/length codes */
  ZIPDUMPBITS(5)
  ZIPNEEDBITS(5)
  nd = 1 + ((uint32_t)b & 0x1f);        /* number of distance codes */
  ZIPDUMPBITS(5)
  ZIPNEEDBITS(4)
  nb = 4 + ((uint32_t)b & 0xf);         /* number of bit length codes */
  ZIPDUMPBITS(4)
  if(nl > 288 || nd > 32)
    return 1;                   /* bad lengths */

  /* read in bit-length-code lengths */
  for(j = 0; j < nb; j++)
  {
    ZIPNEEDBITS(3)
    ll[Zipborder[j]] = (uint32_t)b & 7;
    ZIPDUMPBITS(3)
  }
  for(; j < 19; j++)
    ll[Zipborder[j]] = 0;

  /* build decoding table for trees--single level, 7 bit lookup */
  bl = 7;
  if((i = Ziphuft_build(decomp_state, ll, 19, 19, NULL, NULL, &tl, &bl)) != 0)
  {
    if(i == 1)
      Ziphuft_free(tl);
    return i;                   /* incomplete code set */
  }

  /* read in literal and distance code lengths */
  n = nl + nd;
  m = Zipmask[bl];
  i = l = 0;
  while((uint32_t)i < n)
  {
    ZIPNEEDBITS((uint32_t)bl)
    j = (td = tl + ((uint32_t)b & m))->b;
    ZIPDUMPBITS(j)
    j = td->v.n;
    if (j < 16)                 /* length of code in bits (0..15) */
      ll[i++] = l = j;          /* save last length in l */
    else if (j == 16)           /* repeat last length 3 to 6 times */
    {
      ZIPNEEDBITS(2)
      j = 3 + ((uint32_t)b & 3);
      ZIPDUMPBITS(2)
      if((uint32_t)i + j > n)
        return 1;
      while (j--)
        ll[i++] = l;
    }
    else if (j == 17)           /* 3 to 10 zero length codes */
    {
      ZIPNEEDBITS(3)
      j = 3 + ((uint32_t)b & 7);
      ZIPDUMPBITS(3)
      if ((uint32_t)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
    else                        /* j == 18: 11 to 138 zero length codes */
    {
      ZIPNEEDBITS(7)
      j = 11 + ((uint32_t)b & 0x7f);
      ZIPDUMPBITS(7)
      if ((uint32_t)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
  }

  /* free decoding table for trees */
  Ziphuft_free(tl);

  /* restore the global bit buffer */
  ZIP(bb) = b;
  ZIP(bk) = k;

  /* build the decoding tables for literal/length and distance codes */
  bl = ZIPLBITS;
  if((i = Ziphuft_build(decomp_state, ll, nl, 257, Zipcplens, Zipcplext, &tl, &bl)) != 0)
  {
    if(i == 1)
      Ziphuft_free(tl);
    return i;                   /* incomplete code set */
  }
  bd = ZIPDBITS;
  Ziphuft_build(decomp_state, ll + nl, nd, 0, Zipcpdist, Zipcpdext, &td, &bd);

  /* decompress until an end-of-block code */
  if(Zipinflate_codes(decomp_state, tl, td, bl, bd))
    return 1;

  /* free the decoding tables, return */
  Ziphuft_free(tl);
  Ziphuft_free(td);
  return 0;
}

/* e == last block flag */
static int32_t Zipinflate_block(struct decomp_state *decomp_state, int32_t *e)
{ /* decompress an inflated block */
  uint32_t t;           	/* block type */
  register uint32_t b;     /* bit buffer */
  register uint32_t k;     /* number of bits in bit buffer */

  DEBUG(10,("Zipinflate_block\n"));

  /* make local bit buffer */
  b = ZIP(bb);
  k = ZIP(bk);

  /* read in last block bit */
  ZIPNEEDBITS(1)
  *e = (int32_t)b & 1;
  ZIPDUMPBITS(1)

  /* read in block type */
  ZIPNEEDBITS(2)
  t = (uint32_t)b & 3;
  ZIPDUMPBITS(2)

  /* restore the global bit buffer */
  ZIP(bb) = b;
  ZIP(bk) = k;

  DEBUG(10,("inflate type %d\n", t));

  /* inflate that block type */
  if(t == 2)
    return Zipinflate_dynamic(decomp_state);
  if(t == 0)
    return Zipinflate_stored(decomp_state);
  if(t == 1)
    return Zipinflate_fixed(decomp_state);
  /* bad block type */
  return 2;
}

_PUBLIC_ struct decomp_state *ZIPdecomp_state(TALLOC_CTX *mem_ctx)
{
	return talloc_zero(mem_ctx, struct decomp_state);
}

int ZIPdecompress(struct decomp_state *decomp_state, DATA_BLOB *inbuf, DATA_BLOB *outbuf)
{
	int32_t e = 0;/* last block flag */

	ZIP(inpos) = CAB(inbuf);
	ZIP(bb) = ZIP(bk) = ZIP(window_posn) = 0;

	if (inbuf->length > sizeof(decomp_state->inbuf)) return DECR_INPUT;

	if (outbuf->length > sizeof(decomp_state->outbuf)) return DECR_OUTPUT;

	if (outbuf->length > ZIPWSIZE) return DECR_DATAFORMAT;

	memcpy(decomp_state->inbuf, inbuf->data, inbuf->length);

	/* CK = Chris Kirmse, official Microsoft purloiner */
	if (ZIP(inpos)[0] != 'C' || ZIP(inpos)[1] != 'K') return DECR_ILLEGALDATA;
	ZIP(inpos) += 2;

	while (!e) {
		if (Zipinflate_block(decomp_state, &e)) {
			return DECR_ILLEGALDATA;
		}
	}

	memcpy(outbuf->data, decomp_state->outbuf, outbuf->length);

	return DECR_OK;
}
