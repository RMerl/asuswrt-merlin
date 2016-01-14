/* jhash.h: Jenkins hash support.
 *
 * Copyright (C) 1996 Bob Jenkins (bob_jenkins@burtleburtle.net)
 *
 * http://burtleburtle.net/bob/hash/
 *
 * These are the credits from Bob's sources:
 *
 * lookup2.c, by Bob Jenkins, December 1996, Public Domain.
 * hash(), hash2(), hash3, and mix() are externally useful functions.
 * Routines to test the hash are included if SELF_TEST is defined.
 * You can use this free for any purpose.  It has no warranty.
 *
 * Copyright (C) 2003 David S. Miller (davem@redhat.com)
 *
 * I've modified Bob's hash to be useful in the Linux kernel, and
 * any bugs present are surely my fault.  -DaveM
 */

#ifndef _QUAGGA_JHASH_H
#define _QUAGGA_JHASH_H

/* The most generic version, hashes an arbitrary sequence
 * of bytes.  No alignment or length assumptions are made about
 * the input key.
 */
extern u_int32_t jhash(const void *key, u_int32_t length, u_int32_t initval);

/* A special optimized version that handles 1 or more of u_int32_ts.
 * The length parameter here is the number of u_int32_ts in the key.
 */
extern u_int32_t jhash2(const u_int32_t *k, u_int32_t length, u_int32_t initval);

/* A special ultra-optimized versions that knows they are hashing exactly
 * 3, 2 or 1 word(s).
 *
 * NOTE: In partilar the "c += length; __jhash_mix(a,b,c);" normally
 *       done at the end is not done here.
 */
extern u_int32_t jhash_3words(u_int32_t a, u_int32_t b, u_int32_t c, u_int32_t initval);
extern u_int32_t jhash_2words(u_int32_t a, u_int32_t b, u_int32_t initval);
extern u_int32_t jhash_1word(u_int32_t a, u_int32_t initval);

#endif /* _QUAGGA_JHASH_H */
