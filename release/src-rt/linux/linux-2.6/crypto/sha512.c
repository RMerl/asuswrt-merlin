/* SHA-512 code by Jean-Luc Cooke <jlcooke@certainkey.com>
 *
 * Copyright (c) Jean-Luc Cooke <jlcooke@certainkey.com>
 * Copyright (c) Andrew McDonald <andrew@mcdonald.org.uk>
 * Copyright (c) 2003 Kyle McMartin <kyle@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/crypto.h>
#include <linux/types.h>

#include <asm/scatterlist.h>
#include <asm/byteorder.h>

#define SHA384_DIGEST_SIZE 48
#define SHA512_DIGEST_SIZE 64
#define SHA384_HMAC_BLOCK_SIZE 128
#define SHA512_HMAC_BLOCK_SIZE 128

struct sha512_ctx {
	u64 state[8];
	u32 count[4];
	u8 buf[128];
	u64 W[80];
};

static inline u64 Ch(u64 x, u64 y, u64 z)
{
        return z ^ (x & (y ^ z));
}

static inline u64 Maj(u64 x, u64 y, u64 z)
{
        return (x & y) | (z & (x | y));
}

static inline u64 RORu64(u64 x, u64 y)
{
        return (x >> y) | (x << (64 - y));
}

static const u64 sha512_K[80] = {
        0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
        0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
        0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
        0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
        0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
        0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
        0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
        0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
        0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
        0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
        0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
        0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
        0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
        0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
        0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
        0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
        0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
        0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
        0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
        0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
        0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
        0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
        0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
        0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
        0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
        0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
        0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

#define e0(x)       (RORu64(x,28) ^ RORu64(x,34) ^ RORu64(x,39))
#define e1(x)       (RORu64(x,14) ^ RORu64(x,18) ^ RORu64(x,41))
#define s0(x)       (RORu64(x, 1) ^ RORu64(x, 8) ^ (x >> 7))
#define s1(x)       (RORu64(x,19) ^ RORu64(x,61) ^ (x >> 6))

/* H* initial state for SHA-512 */
#define H0         0x6a09e667f3bcc908ULL
#define H1         0xbb67ae8584caa73bULL
#define H2         0x3c6ef372fe94f82bULL
#define H3         0xa54ff53a5f1d36f1ULL
#define H4         0x510e527fade682d1ULL
#define H5         0x9b05688c2b3e6c1fULL
#define H6         0x1f83d9abfb41bd6bULL
#define H7         0x5be0cd19137e2179ULL

/* H'* initial state for SHA-384 */
#define HP0 0xcbbb9d5dc1059ed8ULL
#define HP1 0x629a292a367cd507ULL
#define HP2 0x9159015a3070dd17ULL
#define HP3 0x152fecd8f70e5939ULL
#define HP4 0x67332667ffc00b31ULL
#define HP5 0x8eb44a8768581511ULL
#define HP6 0xdb0c2e0d64f98fa7ULL
#define HP7 0x47b5481dbefa4fa4ULL

static inline void LOAD_OP(int I, u64 *W, const u8 *input)
{
	W[I] = __be64_to_cpu( ((__be64*)(input))[I] );
}

static inline void BLEND_OP(int I, u64 *W)
{
	W[I] = s1(W[I-2]) + W[I-7] + s0(W[I-15]) + W[I-16];
}

static void
sha512_transform(u64 *state, u64 *W, const u8 *input)
{
	u64 a, b, c, d, e, f, g, h, t1, t2;

	int i;

	/* load the input */
        for (i = 0; i < 16; i++)
                LOAD_OP(i, W, input);

        for (i = 16; i < 80; i++) {
                BLEND_OP(i, W);
        }

	/* load the state into our registers */
	a=state[0];   b=state[1];   c=state[2];   d=state[3];  
	e=state[4];   f=state[5];   g=state[6];   h=state[7];  
  
	/* now iterate */
	for (i=0; i<80; i+=8) {
		t1 = h + e1(e) + Ch(e,f,g) + sha512_K[i  ] + W[i  ];
		t2 = e0(a) + Maj(a,b,c);    d+=t1;    h=t1+t2;
		t1 = g + e1(d) + Ch(d,e,f) + sha512_K[i+1] + W[i+1];
		t2 = e0(h) + Maj(h,a,b);    c+=t1;    g=t1+t2;
		t1 = f + e1(c) + Ch(c,d,e) + sha512_K[i+2] + W[i+2];
		t2 = e0(g) + Maj(g,h,a);    b+=t1;    f=t1+t2;
		t1 = e + e1(b) + Ch(b,c,d) + sha512_K[i+3] + W[i+3];
		t2 = e0(f) + Maj(f,g,h);    a+=t1;    e=t1+t2;
		t1 = d + e1(a) + Ch(a,b,c) + sha512_K[i+4] + W[i+4];
		t2 = e0(e) + Maj(e,f,g);    h+=t1;    d=t1+t2;
		t1 = c + e1(h) + Ch(h,a,b) + sha512_K[i+5] + W[i+5];
		t2 = e0(d) + Maj(d,e,f);    g+=t1;    c=t1+t2;
		t1 = b + e1(g) + Ch(g,h,a) + sha512_K[i+6] + W[i+6];
		t2 = e0(c) + Maj(c,d,e);    f+=t1;    b=t1+t2;
		t1 = a + e1(f) + Ch(f,g,h) + sha512_K[i+7] + W[i+7];
		t2 = e0(b) + Maj(b,c,d);    e+=t1;    a=t1+t2;
	}
  
	state[0] += a; state[1] += b; state[2] += c; state[3] += d;  
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;  

	/* erase our data */
	a = b = c = d = e = f = g = h = t1 = t2 = 0;
}

static void
sha512_init(struct crypto_tfm *tfm)
{
	struct sha512_ctx *sctx = crypto_tfm_ctx(tfm);
	sctx->state[0] = H0;
	sctx->state[1] = H1;
	sctx->state[2] = H2;
	sctx->state[3] = H3;
	sctx->state[4] = H4;
	sctx->state[5] = H5;
	sctx->state[6] = H6;
	sctx->state[7] = H7;
	sctx->count[0] = sctx->count[1] = sctx->count[2] = sctx->count[3] = 0;
}

static void
sha384_init(struct crypto_tfm *tfm)
{
	struct sha512_ctx *sctx = crypto_tfm_ctx(tfm);
        sctx->state[0] = HP0;
        sctx->state[1] = HP1;
        sctx->state[2] = HP2;
        sctx->state[3] = HP3;
        sctx->state[4] = HP4;
        sctx->state[5] = HP5;
        sctx->state[6] = HP6;
        sctx->state[7] = HP7;
        sctx->count[0] = sctx->count[1] = sctx->count[2] = sctx->count[3] = 0;
}

static void
sha512_update(struct crypto_tfm *tfm, const u8 *data, unsigned int len)
{
	struct sha512_ctx *sctx = crypto_tfm_ctx(tfm);

	unsigned int i, index, part_len;

	/* Compute number of bytes mod 128 */
	index = (unsigned int)((sctx->count[0] >> 3) & 0x7F);
	
	/* Update number of bits */
	if ((sctx->count[0] += (len << 3)) < (len << 3)) {
		if ((sctx->count[1] += 1) < 1)
			if ((sctx->count[2] += 1) < 1)
				sctx->count[3]++;
		sctx->count[1] += (len >> 29);
	}
	
        part_len = 128 - index;
	
	/* Transform as many times as possible. */
	if (len >= part_len) {
		memcpy(&sctx->buf[index], data, part_len);
		sha512_transform(sctx->state, sctx->W, sctx->buf);

		for (i = part_len; i + 127 < len; i+=128)
			sha512_transform(sctx->state, sctx->W, &data[i]);

		index = 0;
	} else {
		i = 0;
	}

	/* Buffer remaining input */
	memcpy(&sctx->buf[index], &data[i], len - i);

	/* erase our data */
	memset(sctx->W, 0, sizeof(sctx->W));
}

static void
sha512_final(struct crypto_tfm *tfm, u8 *hash)
{
	struct sha512_ctx *sctx = crypto_tfm_ctx(tfm);
        static u8 padding[128] = { 0x80, };
	__be64 *dst = (__be64 *)hash;
	__be32 bits[4];
	unsigned int index, pad_len;
	int i;

	/* Save number of bits */
	bits[3] = cpu_to_be32(sctx->count[0]);
	bits[2] = cpu_to_be32(sctx->count[1]);
	bits[1] = cpu_to_be32(sctx->count[2]);
	bits[0] = cpu_to_be32(sctx->count[3]);

	/* Pad out to 112 mod 128. */
	index = (sctx->count[0] >> 3) & 0x7f;
	pad_len = (index < 112) ? (112 - index) : ((128+112) - index);
	sha512_update(tfm, padding, pad_len);

	/* Append length (before padding) */
	sha512_update(tfm, (const u8 *)bits, sizeof(bits));

	/* Store state in digest */
	for (i = 0; i < 8; i++)
		dst[i] = cpu_to_be64(sctx->state[i]);

	/* Zeroize sensitive information. */
	memset(sctx, 0, sizeof(struct sha512_ctx));
}

static void sha384_final(struct crypto_tfm *tfm, u8 *hash)
{
        u8 D[64];

	sha512_final(tfm, D);

        memcpy(hash, D, 48);
        memset(D, 0, 64);
}

static struct crypto_alg sha512 = {
        .cra_name       = "sha512",
        .cra_flags      = CRYPTO_ALG_TYPE_DIGEST,
        .cra_blocksize  = SHA512_HMAC_BLOCK_SIZE,
        .cra_ctxsize    = sizeof(struct sha512_ctx),
        .cra_module     = THIS_MODULE,
	.cra_alignmask	= 3,
        .cra_list       = LIST_HEAD_INIT(sha512.cra_list),
        .cra_u          = { .digest = {
                                .dia_digestsize = SHA512_DIGEST_SIZE,
                                .dia_init       = sha512_init,
                                .dia_update     = sha512_update,
                                .dia_final      = sha512_final }
        }
};

static struct crypto_alg sha384 = {
        .cra_name       = "sha384",
        .cra_flags      = CRYPTO_ALG_TYPE_DIGEST,
        .cra_blocksize  = SHA384_HMAC_BLOCK_SIZE,
        .cra_ctxsize    = sizeof(struct sha512_ctx),
	.cra_alignmask	= 3,
        .cra_module     = THIS_MODULE,
        .cra_list       = LIST_HEAD_INIT(sha384.cra_list),
        .cra_u          = { .digest = {
                                .dia_digestsize = SHA384_DIGEST_SIZE,
                                .dia_init       = sha384_init,
                                .dia_update     = sha512_update,
                                .dia_final      = sha384_final }
        }
};

MODULE_ALIAS("sha384");

static int __init init(void)
{
        int ret = 0;

        if ((ret = crypto_register_alg(&sha384)) < 0)
                goto out;
        if ((ret = crypto_register_alg(&sha512)) < 0)
                crypto_unregister_alg(&sha384);
out:
        return ret;
}

static void __exit fini(void)
{
        crypto_unregister_alg(&sha384);
        crypto_unregister_alg(&sha512);
}

module_init(init);
module_exit(fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SHA-512 and SHA-384 Secure Hash Algorithms");
