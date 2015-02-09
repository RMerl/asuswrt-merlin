/*
 * ppp_mppe_mppc.c - MPPC/MPPE "compressor/decompressor" module.
 *
 * Copyright (c) 1994 �rp�d Magos�nyi <mag@bunuel.tii.matav.hu>
 * Copyright (c) 1999 Tim Hockin, Cobalt Networks Inc. <thockin@cobaltnet.com>
 * Copyright (c) 2002-2004 Jan Dubiec <jdx@slackware.pl>
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies. This software is provided without any
 * warranty, express or implied.
 *
 * The code is based on MPPE kernel module written by �rp�d Magos�nyi and
 * Tim Hockin which can be found on http://planetmirror.com/pub/mppe/.
 * I have added MPPC and 56 bit session keys support in MPPE.
 *
 * WARNING! Although this is open source code, its usage in some countries
 * (in particular in the USA) may violate Stac Inc. patent for MPPC.
 *
 *  ==FILEVERSION 20041123==
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/scatterlist.h>
#include <linux/vmalloc.h>
#include <linux/crypto.h>

#include <linux/ppp_defs.h>
#include <linux/ppp-comp.h>

/*
 * State for a mppc/mppe "(de)compressor".
 */
struct ppp_mppe_state {
    struct crypto_tfm *arc4_tfm;
    struct crypto_hash *sha1_tfm;
    u8		*sha1_digest;
    u8		master_key[MPPE_MAX_KEY_LEN];
    u8		session_key[MPPE_MAX_KEY_LEN];
    u8		mppc;		/* do we use compression (MPPC)? */
    u8		mppe;		/* do we use encryption (MPPE)? */
    u8		keylen;		/* key length in bytes */
    u8		bitkeylen;	/* key length in bits */
    u16		ccount;		/* coherency counter */
    u16		bits;		/* MPPC/MPPE control bits */
    u8		stateless;	/* do we use stateless mode? */
    u8		nextflushed;	/* set A bit in the next outgoing packet;
				   used only by compressor*/
    u8		flushexpected;	/* drop packets until A bit is received;
				   used only by decompressor*/
    u8		*hist;		/* MPPC history */
    u16		*hash;		/* Hash table; used only by compressor */
    u16		histptr;	/* history "cursor" */
    int		unit;
    int		debug;
    int		mru;
    struct compstat stats;
};

#define MPPE_HIST_LEN		8192	/* MPPC history size */
#define MPPE_MAX_CCOUNT		0x0FFF	/* max. coherency counter value */

#define MPPE_BIT_FLUSHED	0x80	/* bit A */
#define MPPE_BIT_RESET		0x40	/* bit B */
#define MPPE_BIT_COMP		0x20	/* bit C */
#define MPPE_BIT_ENCRYPTED	0x10	/* bit D */

#define MPPE_SALT0		0xD1	/* values used in MPPE key derivation */
#define MPPE_SALT1		0x26	/* according to RFC3079 */
#define MPPE_SALT2		0x9E

#define MPPE_CCOUNT(x)		((((x)[4] & 0x0f) << 8) + (x)[5])
#define MPPE_BITS(x)		((x)[4] & 0xf0)
#define MPPE_CTRLHI(x)		((((x)->ccount & 0xf00)>>8)|((x)->bits))
#define MPPE_CTRLLO(x)		((x)->ccount & 0xff)

/*
 * Kernel Crypto API needs its arguments to be in kmalloc'd memory, not in the
 * module static data area. That means sha_pad needs to be kmalloc'd. It is done
 * in mppe_module_init(). This has been pointed out on 30th July 2004 by Oleg
 * Makarenko on pptpclient-devel mailing list.
 */
#define SHA1_PAD_SIZE		40
struct sha_pad {
    unsigned char sha_pad1[SHA1_PAD_SIZE];
    unsigned char sha_pad2[SHA1_PAD_SIZE];
};
static struct sha_pad *sha_pad;

static unsigned int
setup_sg(struct scatterlist *sg, const void  *address, unsigned int length)
{
    sg[0].page_link = virt_to_page(address);
    sg[0].offset = offset_in_page(address);
    sg[0].length = length;
    return length;
}

static inline void
arc4_setkey(struct ppp_mppe_state *state, const unsigned char *key,
	    const unsigned int keylen)
{
    crypto_cipher_setkey(state->arc4_tfm, key, keylen);
}

static inline void
arc4_encrypt(struct ppp_mppe_state *state, const unsigned char *in,
	     const unsigned int len, unsigned char *out)
{
    int i;
    for (i = 0; i < len; i++)
    {
       crypto_cipher_encrypt_one(state->arc4_tfm, out+i, in+i);
    }
}


#define arc4_decrypt arc4_encrypt

/*
 * Key Derivation, from RFC 3078, RFC 3079.
 * Equivalent to Get_Key() for MS-CHAP as described in RFC 3079.
 */
static void
get_new_key_from_sha(struct ppp_mppe_state *state, unsigned char *interim_key)
{
	struct hash_desc desc;
	struct scatterlist sg[4];
	unsigned int nbytes;


	nbytes = setup_sg(&sg[0], state->master_key, state->keylen);
	nbytes += setup_sg(&sg[1], sha_pad->sha_pad1,
			   sizeof(sha_pad->sha_pad1));
	nbytes += setup_sg(&sg[2], state->session_key, state->keylen);
	nbytes += setup_sg(&sg[3], sha_pad->sha_pad2,
			   sizeof(sha_pad->sha_pad2));

	desc.tfm = state->sha1_tfm;
	desc.flags = 0;

	crypto_hash_digest(&desc, sg, nbytes, state->sha1_digest);
    memcpy(interim_key, state->sha1_digest, state->keylen);
}

static void
mppe_change_key(struct ppp_mppe_state *state, int initialize)
{
    unsigned char interim_key[MPPE_MAX_KEY_LEN];

    get_new_key_from_sha(state, interim_key);
    if (initialize) {
	memcpy(state->session_key, interim_key, state->keylen);
    } else {
	arc4_setkey(state, interim_key, state->keylen);
	arc4_encrypt(state, interim_key, state->keylen, state->session_key);
    }
    if (state->keylen == 8) {
	if (state->bitkeylen == 40) {
	    state->session_key[0] = MPPE_SALT0;
	    state->session_key[1] = MPPE_SALT1;
	    state->session_key[2] = MPPE_SALT2;
	} else {
	    state->session_key[0] = MPPE_SALT0;
	}
    }
    arc4_setkey(state, state->session_key, state->keylen);
}

/* increase 12-bit coherency counter */
static inline void
mppe_increase_ccount(struct ppp_mppe_state *state)
{
    state->ccount = (state->ccount + 1) & MPPE_MAX_CCOUNT;
    if (state->mppe) {
	if (state->stateless) {
	    mppe_change_key(state, 0);
	    state->nextflushed = 1;
	} else {
	    if ((state->ccount & 0xff) == 0xff) {
		mppe_change_key(state, 0);
	    }
	}
    }
}

/* allocate space for a MPPE/MPPC (de)compressor.  */
/*   comp != 0 -> init compressor */
/*   comp = 0 -> init decompressor */
static void *
mppe_alloc(unsigned char *options, int opt_len, int comp)
{
    struct ppp_mppe_state *state;
    unsigned int digestsize;
    u8* fname;

    fname = comp ? "mppe_comp_alloc" : "mppe_decomp_alloc";

    /*  
     * Hack warning - additionally to the standard MPPC/MPPE configuration
     * options, pppd passes to the (de)copressor 8 or 16 byte session key.
     * Therefore options[1] contains MPPC/MPPE configuration option length
     * (CILEN_MPPE = 6), but the real options length, depending on the key
     * length, is 6+8 or 6+16.
     */
    if (opt_len < CILEN_MPPE) {
	printk(KERN_WARNING "%s: wrong options length: %u\n", fname, opt_len);
	return NULL;
    }

    if (options[0] != CI_MPPE || options[1] != CILEN_MPPE ||
	(options[2] & ~MPPE_STATELESS) != 0 ||
	options[3] != 0 || options[4] != 0 ||
	(options[5] & ~(MPPE_128BIT|MPPE_56BIT|MPPE_40BIT|MPPE_MPPC)) != 0 ||
	(options[5] & (MPPE_128BIT|MPPE_56BIT|MPPE_40BIT|MPPE_MPPC)) == 0) {
	printk(KERN_WARNING "%s: options rejected: o[0]=%02x, o[1]=%02x, "
	       "o[2]=%02x, o[3]=%02x, o[4]=%02x, o[5]=%02x\n", fname, options[0],
	       options[1], options[2], options[3], options[4], options[5]);
	return NULL;
    }

    state = (struct ppp_mppe_state *)kmalloc(sizeof(*state), GFP_KERNEL);
    if (state == NULL) {
	printk(KERN_ERR "%s: cannot allocate space for %scompressor\n", fname,
	       comp ? "" : "de");
	return NULL;
    }
    memset(state, 0, sizeof(struct ppp_mppe_state));

    state->mppc = options[5] & MPPE_MPPC;	/* Do we use MPPC? */
    state->mppe = options[5] & (MPPE_128BIT | MPPE_56BIT |
	MPPE_40BIT);				/* Do we use MPPE? */

    if (state->mppc) {
	/* allocate MPPC history */
	state->hist = (u8*)vmalloc(2*MPPE_HIST_LEN*sizeof(u8));
	if (state->hist == NULL) {
	    kfree(state);
	    printk(KERN_ERR "%s: cannot allocate space for MPPC history\n",
		   fname);
	    return NULL;
	}

	/* allocate hashtable for MPPC compressor */
	if (comp) {
	    state->hash = (u16*)vmalloc(MPPE_HIST_LEN*sizeof(u16));
	    if (state->hash == NULL) {
		vfree(state->hist);
		kfree(state);
		printk(KERN_ERR "%s: cannot allocate space for MPPC history\n",
		       fname);
		return NULL;
	    }
	}
    }

    if (state->mppe) { /* specific for MPPE */
	/* Load ARC4 algorithm */
	state->arc4_tfm = crypto_alloc_base("arc4", 0, 0);
	if (state->arc4_tfm == NULL) {
	    if (state->mppc) {
		vfree(state->hash);
		if (comp)
		    vfree(state->hist);
	    }
	    kfree(state);
	    printk(KERN_ERR "%s: cannot load ARC4 module\n", fname);
	    return NULL;
	}

	/* Load SHA1 algorithm */
	state->sha1_tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (state->sha1_tfm == NULL) {
	    crypto_free_tfm(state->arc4_tfm);
	    if (state->mppc) {
		vfree(state->hash);
		if (comp)
		    vfree(state->hist);
	    }
	    kfree(state);
	    printk(KERN_ERR "%s: cannot load SHA1 module\n", fname);
	    return NULL;
	}

	digestsize = crypto_hash_digestsize(state->sha1_tfm);
	if (digestsize < MPPE_MAX_KEY_LEN) {
	    crypto_free_hash(state->sha1_tfm);
	    crypto_free_tfm(state->arc4_tfm);
	    if (state->mppc) {
		vfree(state->hash);
		if (comp)
		    vfree(state->hist);
	    }
	    kfree(state);
	    printk(KERN_ERR "%s: CryptoAPI SHA1 digest size too small\n", fname);
	}

	state->sha1_digest = kmalloc(digestsize, GFP_KERNEL);
	if (!state->sha1_digest) {
	    crypto_free_hash(state->sha1_tfm);
	    crypto_free_tfm(state->arc4_tfm);
	    if (state->mppc) {
		vfree(state->hash);
		if (comp)
		    vfree(state->hist);
	    }
	    kfree(state);
	    printk(KERN_ERR "%s: cannot allocate space for SHA1 digest\n", fname);
	}

	memcpy(state->master_key, options+CILEN_MPPE, MPPE_MAX_KEY_LEN);
	memcpy(state->session_key, state->master_key, MPPE_MAX_KEY_LEN);
	/* initial key generation is done in mppe_init() */
    }

    return (void *) state;
}

static void *
mppe_comp_alloc(unsigned char *options, int opt_len)
{
    return mppe_alloc(options, opt_len, 1);
}

static void *
mppe_decomp_alloc(unsigned char *options, int opt_len)
{
    return mppe_alloc(options, opt_len, 0);
}

/* cleanup the (de)compressor */
static void
mppe_comp_free(void *arg)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *) arg;

    if (state != NULL) {
	if (state->mppe) {
	    if (state->sha1_digest != NULL)
		kfree(state->sha1_digest);
	    if (state->sha1_tfm != NULL)
		crypto_free_hash(state->sha1_tfm);
	    if (state->arc4_tfm != NULL)
		crypto_free_tfm(state->arc4_tfm);
	}
	if (state->hist != NULL)
	    vfree(state->hist);
	if (state->hash != NULL)
	    vfree(state->hash);
	kfree(state);
    }
}

/* init MPPC/MPPE (de)compresor */
/*   comp != 0 -> init compressor */
/*   comp = 0 -> init decompressor */
static int
mppe_init(void *arg, unsigned char *options, int opt_len, int unit,
	  int hdrlen, int mru, int debug, int comp)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *) arg;
    u8* fname;

    fname = comp ? "mppe_comp_init" : "mppe_decomp_init";

    if (opt_len < CILEN_MPPE) {
	if (debug)
	    printk(KERN_WARNING "%s: wrong options length: %u\n",
		   fname, opt_len);
	return 0;
    }

    if (options[0] != CI_MPPE || options[1] != CILEN_MPPE ||
	(options[2] & ~MPPE_STATELESS) != 0 ||
	options[3] != 0 || options[4] != 0 ||
	(options[5] & ~(MPPE_56BIT|MPPE_128BIT|MPPE_40BIT|MPPE_MPPC)) != 0 ||
	(options[5] & (MPPE_56BIT|MPPE_128BIT|MPPE_40BIT|MPPE_MPPC)) == 0) {
	if (debug)
	    printk(KERN_WARNING "%s: options rejected: o[0]=%02x, o[1]=%02x, "
		   "o[2]=%02x, o[3]=%02x, o[4]=%02x, o[5]=%02x\n", fname,
		   options[0], options[1], options[2], options[3], options[4],
		   options[5]);
	return 0;
    }

    if ((options[5] & ~MPPE_MPPC) != MPPE_128BIT &&
	(options[5] & ~MPPE_MPPC) != MPPE_56BIT &&
	(options[5] & ~MPPE_MPPC) != MPPE_40BIT &&
	(options[5] & MPPE_MPPC) != MPPE_MPPC) {
	if (debug)
	    printk(KERN_WARNING "%s: don't know what to do: o[5]=%02x\n",
		   fname, options[5]);
	return 0;
    }

    state->mppc = options[5] & MPPE_MPPC;	/* Do we use MPPC? */
    state->mppe = options[5] & (MPPE_128BIT | MPPE_56BIT |
	MPPE_40BIT);				/* Do we use MPPE? */
    state->stateless = options[2] & MPPE_STATELESS; /* Do we use stateless mode? */

    switch (state->mppe) {
    case MPPE_40BIT:     /* 40 bit key */
	state->keylen = 8;
	state->bitkeylen = 40;
	break;
    case MPPE_56BIT:     /* 56 bit key */
	state->keylen = 8;
	state->bitkeylen = 56;
	break;
    case MPPE_128BIT:    /* 128 bit key */
	state->keylen = 16;
	state->bitkeylen = 128;
	break;
    default:
	state->keylen = 0;
	state->bitkeylen = 0;
    }

    state->ccount = MPPE_MAX_CCOUNT;
    state->bits = 0;
    state->unit  = unit;
    state->debug = debug;
    state->histptr = MPPE_HIST_LEN;
    if (state->mppc) {	/* reset history if MPPC was negotiated */
	memset(state->hist, 0, 2*MPPE_HIST_LEN*sizeof(u8));
    }

    if (state->mppe) { /* generate initial session keys */
	mppe_change_key(state, 1);
    }

    if (comp) { /* specific for compressor */
	state->nextflushed = 1;
    } else { /* specific for decompressor */
	state->mru = mru;
	state->flushexpected = 1;
    }

    return 1;
}

static int
mppe_comp_init(void *arg, unsigned char *options, int opt_len, int unit,
	       int hdrlen, int debug)
{
    return mppe_init(arg, options, opt_len, unit, hdrlen, 0, debug, 1);
}


static int
mppe_decomp_init(void *arg, unsigned char *options, int opt_len, int unit,
		 int hdrlen, int mru, int debug)
{
    return mppe_init(arg, options, opt_len, unit, hdrlen, mru, debug, 0);
}

static void
mppe_comp_reset(void *arg)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *)arg;

    if (state->debug)
	printk(KERN_DEBUG "%s%d: resetting MPPC/MPPE compressor\n",
	       __FUNCTION__, state->unit);

    state->nextflushed = 1;
    if (state->mppe)
	arc4_setkey(state, state->session_key, state->keylen);
}

static void
mppe_decomp_reset(void *arg)
{
    /* When MPPC/MPPE is in use, we shouldn't receive any CCP Reset-Ack.
       But when we receive such a packet, we just ignore it. */
    return;
}

static void
mppe_stats(void *arg, struct compstat *stats)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *)arg;

    *stats = state->stats;
}

/***************************/
/**** Compression stuff ****/
/***************************/
/* inserts 1 to 8 bits into the output buffer */
static inline void putbits8(u8 *buf, u32 val, const u32 n, u32 *i, u32 *l)
{
    buf += *i;
    if (*l >= n) {
	*l = (*l) - n;
	val <<= *l;
	*buf = *buf | (val & 0xff);
	if (*l == 0) {
	    *l = 8;
	    (*i)++;
	    *(++buf) = 0;
	}
    } else {
	(*i)++;
	*l = 8 - n + (*l);
	val <<= *l;
	*buf = *buf | ((val >> 8) & 0xff);
	*(++buf) = val & 0xff;
    }
}

/* inserts 9 to 16 bits into the output buffer */
static inline void putbits16(u8 *buf, u32 val, const u32 n, u32 *i, u32 *l)
{
    buf += *i;
    if (*l >= n - 8) {
	(*i)++;
	*l = 8 - n + (*l);
	val <<= *l;
	*buf = *buf | ((val >> 8) & 0xff);
	*(++buf) = val & 0xff;
	if (*l == 0) {
	    *l = 8;
	    (*i)++;
	    *(++buf) = 0;
	}
    } else {
	(*i)++; (*i)++;
	*l = 16 - n + (*l);
	val <<= *l;
	*buf = *buf | ((val >> 16) & 0xff);
	*(++buf) = (val >> 8) & 0xff;
	*(++buf) = val & 0xff;
    }
}

/* inserts 17 to 24 bits into the output buffer */
static inline void putbits24(u8 *buf, u32 val, const u32 n, u32 *i, u32 *l)
{
    buf += *i;
    if (*l >= n - 16) {
	(*i)++; (*i)++;
	*l = 16 - n + (*l);
	val <<= *l;
	*buf = *buf | ((val >> 16) & 0xff);
	*(++buf) = (val >> 8) & 0xff;
	*(++buf) = val & 0xff;
	if (*l == 0) {
	    *l = 8;
	    (*i)++;
	    *(++buf) = 0;
	}
    } else {
	(*i)++; (*i)++; (*i)++;
	*l = 24 - n + (*l);
	val <<= *l;
	*buf = *buf | ((val >> 24) & 0xff);
	*(++buf) = (val >> 16) & 0xff;
	*(++buf) = (val >> 8) & 0xff;
	*(++buf) = val & 0xff;
    }
}

static int
mppc_compress(struct ppp_mppe_state *state, unsigned char *ibuf,
	      unsigned char *obuf, int isize, int osize)
{
    u32 olen, off, len, idx, i, l;
    u8 *hist, *sbuf, *p, *q, *r, *s;

    /*  
	At this point, to avoid possible buffer overflow caused by packet
	expansion during/after compression,  we should make sure that
	osize >= (((isize*9)/8)+1)+2, but we don't do that because in
	ppp_generic.c we simply allocate bigger obuf.

	Maximum MPPC packet expansion is 12.5%. This is the worst case when
	all octets in the input buffer are >= 0x80 and we cannot find any
	repeated tokens. Additionally we have to reserve 2 bytes for MPPE/MPPC
	status bits and coherency counter.
    */

    hist = state->hist + MPPE_HIST_LEN;
    /* check if there is enough room at the end of the history */
    if (state->histptr + isize >= 2*MPPE_HIST_LEN) {
	state->bits |= MPPE_BIT_RESET;
	state->histptr = MPPE_HIST_LEN;
	memcpy(state->hist, hist, MPPE_HIST_LEN);
    }
    /* add packet to the history; isize must be <= MPPE_HIST_LEN */
    sbuf = state->hist + state->histptr;
    memcpy(sbuf, ibuf, isize);
    state->histptr += isize;

    /* compress data */
    r = sbuf + isize;
    *obuf = olen = i = 0;
    l = 8;
    while (i < isize - 2) {
	s = q = sbuf + i;
	idx = ((40543*((((s[0]<<4)^s[1])<<4)^s[2]))>>4) & 0x1fff;
	p = hist + state->hash[idx];
	state->hash[idx] = (u16) (s - hist);
	off = s - p;
	if (off > MPPE_HIST_LEN - 1 || off < 1 || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++) {
	    /* no match found; encode literal byte */
	    if (ibuf[i] < 0x80) {		/* literal byte < 0x80 */
		putbits8(obuf, (u32) ibuf[i], 8, &olen, &l);
	    } else {				/* literal byte >= 0x80 */
		putbits16(obuf, (u32) (0x100|(ibuf[i]&0x7f)), 9, &olen, &l);
	    }
	    ++i;
	    continue;
	}
	if (r - q >= 64) {
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++ || *p++ != *s++ || *p++ != *s++ || *p++ != *s++ ||
	    *p++ != *s++;
	    if (s - q == 64) {
		p--; s--;
		while((*p++ == *s++) && (s < r) && (p < q));
	    }
	} else {
	    while((*p++ == *s++) && (s < r) && (p < q));
	}
	len = s - q - 1;
	i += len;

	/* at least 3 character match found; code data */
	/* encode offset */
	if (off < 64) {			/* 10-bit offset; 0 <= offset < 64 */
	    putbits16(obuf, 0x3c0|off, 10, &olen, &l);
	} else if (off < 320) {		/* 12-bit offset; 64 <= offset < 320 */
	    putbits16(obuf, 0xe00|(off-64), 12, &olen, &l);
	} else if (off < 8192) {	/* 16-bit offset; 320 <= offset < 8192 */
	    putbits16(obuf, 0xc000|(off-320), 16, &olen, &l);
	} else {
	    /* This shouldn't happen; we return 0 what means "packet expands",
	    and we send packet uncompressed. */
	    if (state->debug)
		printk(KERN_DEBUG "%s%d: wrong offset value: %d\n",
		       __FUNCTION__, state->unit, off);
	    return 0;
	}
	/* encode length of match */
	if (len < 4) {			/* length = 3 */
	    putbits8(obuf, 0, 1, &olen, &l);
	} else if (len < 8) {		/* 4 <= length < 8 */
	    putbits8(obuf, 0x08|(len&0x03), 4, &olen, &l);
	} else if (len < 16) {		/* 8 <= length < 16 */
	    putbits8(obuf, 0x30|(len&0x07), 6, &olen, &l);
	} else if (len < 32) {		/* 16 <= length < 32 */
	    putbits8(obuf, 0xe0|(len&0x0f), 8, &olen, &l);
	} else if (len < 64) {		/* 32 <= length < 64 */
	    putbits16(obuf, 0x3c0|(len&0x1f), 10, &olen, &l);
	} else if (len < 128) {		/* 64 <= length < 128 */
	    putbits16(obuf, 0xf80|(len&0x3f), 12, &olen, &l);
	} else if (len < 256) {		/* 128 <= length < 256 */
	    putbits16(obuf, 0x3f00|(len&0x7f), 14, &olen, &l);
	} else if (len < 512) {		/* 256 <= length < 512 */
	    putbits16(obuf, 0xfe00|(len&0xff), 16, &olen, &l);
	} else if (len < 1024) {	/* 512 <= length < 1024 */
	    putbits24(obuf, 0x3fc00|(len&0x1ff), 18, &olen, &l);
	} else if (len < 2048) {	/* 1024 <= length < 2048 */
	    putbits24(obuf, 0xff800|(len&0x3ff), 20, &olen, &l);
	} else if (len < 4096) {	/* 2048 <= length < 4096 */
	    putbits24(obuf, 0x3ff000|(len&0x7ff), 22, &olen, &l);
	} else if (len < 8192) {	/* 4096 <= length < 8192 */
	    putbits24(obuf, 0xffe000|(len&0xfff), 24, &olen, &l);
	} else {
	    /* This shouldn't happen; we return 0 what means "packet expands",
	    and send packet uncompressed. */
	    if (state->debug)
		printk(KERN_DEBUG "%s%d: wrong length of match value: %d\n",
		       __FUNCTION__, state->unit, len);
	    return 0;
	}
    }

    /* Add remaining octets to the output */
    while(isize - i > 0) {
	if (ibuf[i] < 0x80) {	/* literal byte < 0x80 */
	    putbits8(obuf, (u32) ibuf[i++], 8, &olen, &l);
	} else {		/* literal byte >= 0x80 */
	    putbits16(obuf, (u32) (0x100|(ibuf[i++]&0x7f)), 9, &olen, &l);
	}
    }
    /* Reset unused bits of the last output octet */
    if ((l != 0) && (l != 8)) {
	putbits8(obuf, 0, l, &olen, &l);
    }

    return (int) olen;
}

int
mppe_compress(void *arg, unsigned char *ibuf, unsigned char *obuf,
	      int isize, int osize)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *) arg;
    int proto, olen, complen, off;
    unsigned char *wptr;

    /* Check that the protocol is in the range we handle. */
    proto = PPP_PROTOCOL(ibuf);
    if (proto < 0x0021 || proto > 0x00fa)
	return 0;

    wptr = obuf;
    /* Copy over the PPP header */
    wptr[0] = PPP_ADDRESS(ibuf);
    wptr[1] = PPP_CONTROL(ibuf);
    wptr[2] = PPP_COMP >> 8;
    wptr[3] = PPP_COMP;
    wptr += PPP_HDRLEN + (MPPE_OVHD / 2); /* Leave two octets for MPPE/MPPC bits */

    /* 
     * In ver. 0.99 protocol field was compressed. Deflate and BSD compress
     * do PFC before actual compression, RCF2118 and RFC3078 are not precise
     * on this topic so I decided to do PFC. Unfortunately this change caused
     * incompatibility with older/other MPPE/MPPC modules. I have received
     * a lot of complaints from unexperienced users so I have decided to revert
     * to previous state, i.e. the protocol field is sent uncompressed now.
     * Although this may be changed in the future.
     *
     * Receiving side (mppe_decompress()) still accepts packets with compressed
     * and uncompressed protocol field so you shouldn't get "Unsupported protocol
     * 0x2145 received" messages anymore.
     */
    //off = (proto > 0xff) ? 2 : 3; /* PFC - skip first protocol byte if 0 */
    off = 2;

    ibuf += off;

    mppe_increase_ccount(state);

    if (state->nextflushed) {
	state->bits |= MPPE_BIT_FLUSHED;
	state->nextflushed = 0;
	if (state->mppe && !state->stateless) {
	    /*
	     * If this is the flag packet, the key has been already changed in
	     * mppe_increase_ccount() so we dont't do it once again.
	     */
	    if ((state->ccount & 0xff) != 0xff) {
		arc4_setkey(state, state->session_key, state->keylen);
	    }
	}
	if (state->mppc) { /* reset history */
	    state->bits |= MPPE_BIT_RESET;
	    state->histptr = MPPE_HIST_LEN;
	    memset(state->hist + MPPE_HIST_LEN, 0, MPPE_HIST_LEN*sizeof(u8));
	}
    }

    if (state->mppc && !state->mppe) { /* Do only compression */
	complen = mppc_compress(state, ibuf, wptr, isize - off,
				osize - PPP_HDRLEN - (MPPE_OVHD / 2));
	/*
	 * TODO: Implement an heuristics to handle packet expansion in a smart
	 * way. Now, when a packet expands, we send it as uncompressed and
	 * when next packet is sent we have to reset compressor's history.
	 * Maybe it would be better to send such packet as compressed in order
	 * to keep history's continuity.
	 */
	if ((complen > isize) || (complen > osize - PPP_HDRLEN) ||
	    (complen == 0)) {
	    /* packet expands */
	    state->nextflushed = 1;
	    memcpy(wptr, ibuf, isize - off);
	    olen = isize - (off - 2) + MPPE_OVHD;
	    (state->stats).inc_bytes += olen;
	    (state->stats).inc_packets++;
	} else {
	    state->bits |= MPPE_BIT_COMP;
	    olen = complen + PPP_HDRLEN + (MPPE_OVHD / 2);
	    (state->stats).comp_bytes += olen;
	    (state->stats).comp_packets++;
	}
    } else { /* Do encryption with or without compression */
	state->bits |= MPPE_BIT_ENCRYPTED;
	if (!state->mppc && state->mppe) { /* Do only encryption */
	    /* read from ibuf, write to wptr, adjust for PPP_HDRLEN */
	    arc4_encrypt(state, ibuf, isize - off, wptr);
	    olen = isize - (off - 2) + MPPE_OVHD;
	    (state->stats).inc_bytes += olen;
	    (state->stats).inc_packets++;
	} else { /* Do compression and then encryption - RFC3078 */
	    complen = mppc_compress(state, ibuf, wptr, isize - off,
				    osize - PPP_HDRLEN - (MPPE_OVHD / 2));
	    /*
	     * TODO: Implement an heuristics to handle packet expansion in a smart
	     * way. Now, when a packet expands, we send it as uncompressed and
	     * when next packet is sent we have to reset compressor's history.
	     * Maybe it would be good to send such packet as compressed in order
	     * to keep history's continuity.
	     */
	    if ((complen > isize) || (complen > osize - PPP_HDRLEN) ||
		(complen == 0)) {
		/* packet expands */
		state->nextflushed = 1;
		arc4_encrypt(state, ibuf, isize - off, wptr);
		olen = isize - (off - 2) + MPPE_OVHD;
		(state->stats).inc_bytes += olen;
		(state->stats).inc_packets++;
	    } else {
		state->bits |= MPPE_BIT_COMP;
		/* Hack warning !!! RC4 implementation which we use does
		   encryption "in place" - it means that input and output
		   buffers can be *the same* memory area. Therefore we don't
		   need to use a temporary buffer. But be careful - other
		   implementations don't have to be so nice.
		   I used to use ibuf as temporary buffer here, but it led
		   packet sniffers into error. Thanks to Wilfried Weissmann
		   for pointing that. */
		arc4_encrypt(state, wptr, complen, wptr);
		olen = complen + PPP_HDRLEN + (MPPE_OVHD / 2);
		(state->stats).comp_bytes += olen;
		(state->stats).comp_packets++;
	    }
	}
    }

    /* write status bits and coherency counter into the output buffer */
    wptr = obuf + PPP_HDRLEN;
    wptr[0] = MPPE_CTRLHI(state);
    wptr[1] = MPPE_CTRLLO(state);

    state->bits = 0;

    (state->stats).unc_bytes += isize;
    (state->stats).unc_packets++;

    return olen;
}

/***************************/
/*** Decompression stuff ***/
/***************************/
static inline u32 getbits(const u8 *buf, const u32 n, u32 *i, u32 *l)
{
    static const u32 m[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};
    u32 res, ol;

    ol = *l;
    if (*l >= n) {
	*l = (*l) - n;
	res = (buf[*i] & m[ol]) >> (*l);
	if (*l == 0) {
	    *l = 8;
	    (*i)++;
	}
    } else {
	*l = 8 - n + (*l);
	res = (buf[(*i)++] & m[ol]) << 8;
	res = (res | buf[*i]) >> (*l);
    }

    return res;
}

static inline u32 getbyte(const u8 *buf, const u32 i, const u32 l)
{
    if (l == 8) {
	return buf[i];
    } else {
	return (((buf[i] << 8) | buf[i+1]) >> l) & 0xff;
    }
}

static inline void lamecopy(u8 *dst, u8 *src, u32 len)
{
    while (len--)
	*dst++ = *src++;
}

static int
mppc_decompress(struct ppp_mppe_state *state, unsigned char *ibuf,
		unsigned char *obuf, int isize, int osize)
{
    u32 olen, off, len, bits, val, sig, i, l;
    u8 *history, *s;

    history = state->hist + state->histptr;
    olen = len = i = 0;
    l = 8;
    bits = isize * 8;
    while (bits >= 8) {
	val = getbyte(ibuf, i++, l);
	if (val < 0x80) {		/* literal byte < 0x80 */
	    if (state->histptr < 2*MPPE_HIST_LEN) {
		/* copy uncompressed byte to the history */
		(state->hist)[(state->histptr)++] = (u8) val;
	    } else {
		/* buffer overflow; drop packet */
		if (state->debug)
		    printk(KERN_ERR "%s%d: trying to write outside history "
			   "buffer\n", __FUNCTION__, state->unit);
		return DECOMP_ERROR;
	    }
	    olen++;
	    bits -= 8;
	    continue;
	}

	sig = val & 0xc0;
	if (sig == 0x80) {		/* literal byte >= 0x80 */
	    if (state->histptr < 2*MPPE_HIST_LEN) {
		/* copy uncompressed byte to the history */
		(state->hist)[(state->histptr)++] = 
		    (u8) (0x80|((val&0x3f)<<1)|getbits(ibuf, 1 , &i ,&l));
	    } else {
		/* buffer overflow; drop packet */
		if (state->debug)
		    printk(KERN_ERR "%s%d: trying to write outside history "
			   "buffer\n", __FUNCTION__, state->unit);
		return DECOMP_ERROR;
	    }
	    olen++;
	    bits -= 9;
	    continue;
	}

	/* Not a literal byte so it must be an (offset,length) pair */
	/* decode offset */
	sig = val & 0xf0;
	if (sig == 0xf0) {		/* 10-bit offset; 0 <= offset < 64 */
	    off = (((val&0x0f)<<2)|getbits(ibuf, 2 , &i ,&l));
	    bits -= 10;
	} else {
	    if (sig == 0xe0) {		/* 12-bit offset; 64 <= offset < 320 */
		off = ((((val&0x0f)<<4)|getbits(ibuf, 4 , &i ,&l))+64);
		bits -= 12;
	    } else {
		if ((sig&0xe0) == 0xc0) {/* 16-bit offset; 320 <= offset < 8192 */
		    off = ((((val&0x1f)<<8)|getbyte(ibuf, i++, l))+320);
		    bits -= 16;
		    if (off > MPPE_HIST_LEN - 1) {
			if (state->debug)
			    printk(KERN_DEBUG "%s%d: too big offset value: %d\n",
				   __FUNCTION__, state->unit, off);
			return DECOMP_ERROR;
		    }
		} else {		/* this shouldn't happen */
		    if (state->debug)
			printk(KERN_DEBUG "%s%d: cannot decode offset value\n",
			       __FUNCTION__, state->unit);
		    return DECOMP_ERROR;
		}
	    }
	}
	/* decode length of match */
	val = getbyte(ibuf, i, l);
	if ((val & 0x80) == 0x00) {			/* len = 3 */
	    len = 3;
	    bits--;
	    getbits(ibuf, 1 , &i ,&l);
	} else if ((val & 0xc0) == 0x80) {		/* 4 <= len < 8 */
	    len = 0x04 | ((val>>4) & 0x03);
	    bits -= 4;
	    getbits(ibuf, 4 , &i ,&l);
	} else if ((val & 0xe0) == 0xc0) {		/* 8 <= len < 16 */
	    len = 0x08 | ((val>>2) & 0x07);
	    bits -= 6;
	    getbits(ibuf, 6 , &i ,&l);
	} else if ((val & 0xf0) == 0xe0) {		/* 16 <= len < 32 */
	    len = 0x10 | (val & 0x0f);
	    bits -= 8;
	    i++;
	} else {
	    bits -= 8;
	    val = (val << 8) | getbyte(ibuf, ++i, l);
	    if ((val & 0xf800) == 0xf000) {		/* 32 <= len < 64 */
		len = 0x0020 | ((val >> 6) & 0x001f);
		bits -= 2;
		getbits(ibuf, 2 , &i ,&l);
	    } else if ((val & 0xfc00) == 0xf800) {	/* 64 <= len < 128 */
		len = 0x0040 | ((val >> 4) & 0x003f);
		bits -= 4;
		getbits(ibuf, 4 , &i ,&l);
	    } else if ((val & 0xfe00) == 0xfc00) {	/* 128 <= len < 256 */
		len = 0x0080 | ((val >> 2) & 0x007f);
		bits -= 6;
		getbits(ibuf, 6 , &i ,&l);
	    } else if ((val & 0xff00) == 0xfe00) {	/* 256 <= len < 512 */
		len = 0x0100 | (val & 0x00ff);
		bits -= 8;
		i++;
	    } else {
		bits -= 8;
		val = (val << 8) | getbyte(ibuf, ++i, l);
		if ((val & 0xff8000) == 0xff0000) {	/* 512 <= len < 1024 */
		    len = 0x000200 | ((val >> 6) & 0x0001ff);
		    bits -= 2;
		    getbits(ibuf, 2 , &i ,&l);
		} else if ((val & 0xffc000) == 0xff8000) {/* 1024 <= len < 2048 */
		    len = 0x000400 | ((val >> 4) & 0x0003ff);
		    bits -= 4;
		    getbits(ibuf, 4 , &i ,&l);
		} else if ((val & 0xffe000) == 0xffc000) {/* 2048 <= len < 4096 */
		    len = 0x000800 | ((val >> 2) & 0x0007ff);
		    bits -= 6;
		    getbits(ibuf, 6 , &i ,&l);
		} else if ((val & 0xfff000) == 0xffe000) {/* 4096 <= len < 8192 */
		    len = 0x001000 | (val & 0x000fff);
		    bits -= 8;
		    i++;
		} else {				/* this shouldn't happen */
		    if (state->debug)
			printk(KERN_DEBUG "%s%d: wrong length code: 0x%X\n",
			       __FUNCTION__, state->unit, val);
		    return DECOMP_ERROR;
		}
	    }
	}
	s = state->hist + state->histptr;
	state->histptr += len;
	olen += len;
	if (state->histptr < 2*MPPE_HIST_LEN) {
	    /* copy uncompressed bytes to the history */

	    /* In some cases len may be greater than off. It means that memory
	     * areas pointed by s and s-off overlap. I had used memmove() here
	     * because I thought that it acts as libc's version. Unfortunately,
	     * I was wrong. :-) I got strange errors sometimes. Wilfried suggested
	     * using of byte by byte copying here and strange errors disappeared.
	     */
	    lamecopy(s, s - off, len);
	} else {
	    /* buffer overflow; drop packet */
	    if (state->debug)
		printk(KERN_ERR "%s%d: trying to write outside history "
		       "buffer\n", __FUNCTION__, state->unit);
	    return DECOMP_ERROR;
	}
    }

    /* Do PFC decompression */
    len = olen;
    if ((history[0] & 0x01) != 0) {
	obuf[0] = 0;
	obuf++;
	len++;
    }

    if (len <= osize) {
	/* copy uncompressed packet to the output buffer */
	memcpy(obuf, history, olen);
    } else {
	/* buffer overflow; drop packet */
	if (state->debug)
	    printk(KERN_ERR "%s%d: too big uncompressed packet: %d\n",
		   __FUNCTION__, state->unit, len + (PPP_HDRLEN / 2));
	return DECOMP_ERROR;
    }

    return (int) len;
}

int
mppe_decompress(void *arg, unsigned char *ibuf, int isize,
		unsigned char *obuf, int osize)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *)arg;
    int seq, bits, uncomplen;

    if (isize <= PPP_HDRLEN + MPPE_OVHD) {
	if (state->debug) {
	    printk(KERN_DEBUG "%s%d: short packet (len=%d)\n",  __FUNCTION__,
		   state->unit, isize);
	}
	return DECOMP_ERROR;
    }

    /* Get coherency counter and control bits from input buffer */
    seq = MPPE_CCOUNT(ibuf);
    bits = MPPE_BITS(ibuf);

    if (state->stateless) {
	/* RFC 3078, sec 8.1. */
	mppe_increase_ccount(state);
	if ((seq != state->ccount) && state->debug)
	    printk(KERN_DEBUG "%s%d: bad sequence number: %d, expected: %d\n",
		   __FUNCTION__, state->unit, seq, state->ccount);
	while (seq != state->ccount)
	    mppe_increase_ccount(state);
    } else {
	/* RFC 3078, sec 8.2. */
	if (state->flushexpected) { /* discard state */
	    if ((bits & MPPE_BIT_FLUSHED)) { /* we received expected FLUSH bit */
		while (seq != state->ccount)
		    mppe_increase_ccount(state);
		state->flushexpected = 0;
	    } else /* drop packet*/
		return DECOMP_ERROR;
	} else { /* normal state */
	    mppe_increase_ccount(state);
	    if (seq != state->ccount) {
		/* Packet loss detected, enter the discard state. */
		if (state->debug)
		    printk(KERN_DEBUG "%s%d: bad sequence number: %d, expected: %d\n",
			   __FUNCTION__, state->unit, seq, state->ccount);
		state->flushexpected = 1;
		return DECOMP_ERROR;
	    }
	}
	if (state->mppe && (bits & MPPE_BIT_FLUSHED)) {
	    arc4_setkey(state, state->session_key, state->keylen);
	}
    }

    if (state->mppc && (bits & (MPPE_BIT_FLUSHED | MPPE_BIT_RESET))) {
	state->histptr = MPPE_HIST_LEN;
	if ((bits & MPPE_BIT_FLUSHED)) {
	    memset(state->hist + MPPE_HIST_LEN, 0, MPPE_HIST_LEN*sizeof(u8));
	} else
	    if ((bits & MPPE_BIT_RESET)) {
		memcpy(state->hist, state->hist + MPPE_HIST_LEN, MPPE_HIST_LEN);
	    }
    }

    /* Fill in the first part of the PPP header. The protocol field
       comes from the decompressed data. */
    obuf[0] = PPP_ADDRESS(ibuf);
    obuf[1] = PPP_CONTROL(ibuf);
    obuf += PPP_HDRLEN / 2;

    if (state->mppe) { /* process encrypted packet */
	if ((bits & MPPE_BIT_ENCRYPTED)) {
	    /* OK, packet encrypted, so decrypt it */
	    if (state->mppc && (bits & MPPE_BIT_COMP)) {
		/* Hack warning !!! RC4 implementation which we use does
		   decryption "in place" - it means that input and output
		   buffers can be *the same* memory area. Therefore we don't
		   need to use a temporary buffer. But be careful - other
		   implementations don't have to be so nice. */
		arc4_decrypt(state, ibuf + PPP_HDRLEN +	(MPPE_OVHD / 2), isize -
			     PPP_HDRLEN - (MPPE_OVHD / 2), ibuf + PPP_HDRLEN +
			     (MPPE_OVHD / 2));
		uncomplen = mppc_decompress(state, ibuf + PPP_HDRLEN +
					    (MPPE_OVHD / 2), obuf, isize -
					    PPP_HDRLEN - (MPPE_OVHD / 2),
					    osize - (PPP_HDRLEN / 2));
		if (uncomplen == DECOMP_ERROR) {
		    state->flushexpected = 1;
		    return DECOMP_ERROR;
		}
		uncomplen += PPP_HDRLEN / 2;
		(state->stats).comp_bytes += isize;
		(state->stats).comp_packets++;
	    } else {
		uncomplen = isize - MPPE_OVHD;
		/* Decrypt the first byte in order to check if it is
		   compressed or uncompressed protocol field */
		arc4_decrypt(state, ibuf + PPP_HDRLEN +	(MPPE_OVHD / 2), 1, obuf);
		/* Do PFC decompression */
		if ((obuf[0] & 0x01) != 0) {
		    obuf[1] = obuf[0];
		    obuf[0] = 0;
		    obuf++;
		    uncomplen++;
		}
		/* And finally, decrypt the rest of the frame. */
		arc4_decrypt(state, ibuf + PPP_HDRLEN +	(MPPE_OVHD / 2) + 1,
			     isize - PPP_HDRLEN - (MPPE_OVHD / 2) - 1, obuf + 1);
		(state->stats).inc_bytes += isize;
		(state->stats).inc_packets++;
	    }
	} else { /* this shouldn't happen */
	    if (state->debug)
		printk(KERN_ERR "%s%d: encryption negotiated but not an "
		       "encrypted packet received\n", __FUNCTION__, state->unit);
	    mppe_change_key(state, 0);
	    state->flushexpected = 1;
	    return DECOMP_ERROR;
	}
    } else {
	if (state->mppc) { /* no MPPE, only MPPC */
	    if ((bits & MPPE_BIT_COMP)) {
		uncomplen = mppc_decompress(state, ibuf + PPP_HDRLEN +
					    (MPPE_OVHD / 2), obuf, isize -
					    PPP_HDRLEN - (MPPE_OVHD / 2),
					    osize - (PPP_HDRLEN / 2));
		if (uncomplen == DECOMP_ERROR) {
		    state->flushexpected = 1;
		    return DECOMP_ERROR;
		}
		uncomplen += PPP_HDRLEN / 2;
		(state->stats).comp_bytes += isize;
		(state->stats).comp_packets++;
	    } else {
		memcpy(obuf, ibuf + PPP_HDRLEN + (MPPE_OVHD / 2), isize -
		       PPP_HDRLEN - (MPPE_OVHD / 2));
		uncomplen = isize - MPPE_OVHD;
		(state->stats).inc_bytes += isize;
		(state->stats).inc_packets++;
	    }
	} else { /* this shouldn't happen */
	    if (state->debug)
		printk(KERN_ERR "%s%d: error - not an  MPPC or MPPE frame "
		       "received\n", __FUNCTION__, state->unit);
	    state->flushexpected = 1;
	    return DECOMP_ERROR;
	}
    }

    (state->stats).unc_bytes += uncomplen;
    (state->stats).unc_packets++;

    return uncomplen;
}


/************************************************************
 * Module interface table
 ************************************************************/

/* These are in ppp_generic.c */
extern int  ppp_register_compressor   (struct compressor *cp);
extern void ppp_unregister_compressor (struct compressor *cp);

/*
 * Functions exported to ppp_generic.c.
 *
 * In case of MPPC/MPPE there is no need to process incompressible data
 * because such a data is sent in MPPC/MPPE frame. Therefore the (*incomp)
 * callback function isn't needed.
 */
struct compressor ppp_mppe = {
    .compress_proto =	CI_MPPE,
    .comp_alloc =	mppe_comp_alloc,
    .comp_free =	mppe_comp_free,
    .comp_init =	mppe_comp_init,
    .comp_reset =	mppe_comp_reset,
    .compress =		mppe_compress,
    .comp_stat =	mppe_stats,
    .decomp_alloc =	mppe_decomp_alloc,
    .decomp_free =	mppe_comp_free,
    .decomp_init =	mppe_decomp_init,
    .decomp_reset =	mppe_decomp_reset,
    .decompress =	mppe_decompress,
    .incomp =		NULL,
    .decomp_stat =	mppe_stats,
    .owner =		THIS_MODULE
};

/************************************************************
 * Module support routines
 ************************************************************/

int __init mppe_module_init(void)
{
    int answer;

    if (!(crypto_alg_available("arc4", 0) && crypto_alg_available("sha1", 0))) {
	printk(KERN_ERR "Kernel doesn't provide ARC4 and/or SHA1 algorithms "
	       "required by MPPE/MPPC. Check CryptoAPI configuration.\n");
	return -ENODEV;
    }

    /* Allocate space for SHAPad1, SHAPad2 and ... */
    sha_pad = kmalloc(sizeof(struct sha_pad), GFP_KERNEL);
    if (sha_pad == NULL)
	return -ENOMEM;
    /* ... initialize them */
    memset(sha_pad->sha_pad1, 0x00, sizeof(sha_pad->sha_pad1));
    memset(sha_pad->sha_pad2, 0xf2, sizeof(sha_pad->sha_pad2));

    answer = ppp_register_compressor(&ppp_mppe);
    if (answer == 0) {
	printk(KERN_INFO "MPPE/MPPC encryption/compression module registered\n");
    }
    return answer;
}

void __exit mppe_module_cleanup(void)
{
    kfree(sha_pad);
    ppp_unregister_compressor(&ppp_mppe);
    printk(KERN_INFO "MPPE/MPPC encryption/compression module unregistered\n");
}

module_init(mppe_module_init);
module_exit(mppe_module_cleanup);

MODULE_AUTHOR("Jan Dubiec <jdx@slackware.pl>");
MODULE_DESCRIPTION("MPPE/MPPC encryption/compression module for Linux");
MODULE_VERSION("1.3");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("ppp-compress-" __stringify(CI_MPPE));
