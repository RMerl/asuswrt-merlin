/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "dbutil.h"
#include "bignum.h"
#include "dbrandom.h"
#include "rsa.h"
#include "genrsa.h"

#define RSA_E 65537

#ifdef DROPBEAR_RSA

static void getrsaprime(mp_int* prime, mp_int *primeminus, 
		mp_int* rsa_e, unsigned int size_bytes);

/* mostly taken from libtomcrypt's rsa key generation routine */
dropbear_rsa_key * gen_rsa_priv_key(unsigned int size) {

	dropbear_rsa_key * key;
	DEF_MP_INT(pminus);
	DEF_MP_INT(qminus);
	DEF_MP_INT(lcm);

	if (size < 512 || size > 4096 || (size % 8 != 0)) {
		dropbear_exit("Bits must satisfy 512 <= bits <= 4096, and be a"
			" multiple of 8");
	}

	key = m_malloc(sizeof(*key));
	m_mp_alloc_init_multi(&key->e, &key->n, &key->d, &key->p, &key->q, NULL);
	m_mp_init_multi(&pminus, &lcm, &qminus, NULL);

	if (mp_set_int(key->e, RSA_E) != MP_OKAY) {
		fprintf(stderr, "RSA generation failed\n");
		exit(1);
	}

	while (1) {
		getrsaprime(key->p, &pminus, key->e, size/16);
		getrsaprime(key->q, &qminus, key->e, size/16);

		if (mp_mul(key->p, key->q, key->n) != MP_OKAY) {
			fprintf(stderr, "RSA generation failed\n");
			exit(1);
		}

		if ((unsigned int)mp_count_bits(key->n) == size) {
			break;
		}
	}

	/* lcm(p-1, q-1) */
	if (mp_lcm(&pminus, &qminus, &lcm) != MP_OKAY) {
		fprintf(stderr, "RSA generation failed\n");
		exit(1);
	}

	/* de = 1 mod lcm(p-1,q-1) */
	/* therefore d = (e^-1) mod lcm(p-1,q-1) */
	if (mp_invmod(key->e, &lcm, key->d) != MP_OKAY) {
		fprintf(stderr, "RSA generation failed\n");
		exit(1);
	}

	mp_clear_multi(&pminus, &qminus, &lcm, NULL);

	return key;
}	

/* return a prime suitable for p or q */
static void getrsaprime(mp_int* prime, mp_int *primeminus, 
		mp_int* rsa_e, unsigned int size_bytes) {

	unsigned char *buf;
	DEF_MP_INT(temp_gcd);

	buf = (unsigned char*)m_malloc(size_bytes);

	m_mp_init(&temp_gcd);
	do {
		/* generate a random odd number with MSB set, then find the
		   the next prime above it */
		genrandom(buf, size_bytes);
		buf[0] |= 0x80;

		bytes_to_mp(prime, buf, size_bytes);

		/* find the next integer which is prime, 8 round of miller-rabin */
		if (mp_prime_next_prime(prime, 8, 0) != MP_OKAY) {
			fprintf(stderr, "RSA generation failed\n");
			exit(1);
		}

		/* subtract one to get p-1 */
		if (mp_sub_d(prime, 1, primeminus) != MP_OKAY) {
			fprintf(stderr, "RSA generation failed\n");
			exit(1);
		}
		/* check relative primality to e */
		if (mp_gcd(primeminus, rsa_e, &temp_gcd) != MP_OKAY) {
			fprintf(stderr, "RSA generation failed\n");
			exit(1);
		}
	} while (mp_cmp_d(&temp_gcd, 1) != MP_EQ); /* while gcd(p-1, e) != 1 */

	/* now we have a good value for result */
	mp_clear(&temp_gcd);
	m_burn(buf, size_bytes);
	m_free(buf);
}

#endif /* DROPBEAR_RSA */
