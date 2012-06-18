/* 
   Unix SMB/CIFS implementation.
   a implementation of MD4 designed for use in the SMB authentication protocol
   Copyright (C) Andrew Tridgell 1997-1998.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "../lib/crypto/md4.h"

/* NOTE: This code makes no attempt to be fast! 

   It assumes that a int is at least 32 bits long
*/

struct mdfour_state {
	uint32_t A, B, C, D;
};

static uint32_t F(uint32_t X, uint32_t Y, uint32_t Z)
{
	return (X&Y) | ((~X)&Z);
}

static uint32_t G(uint32_t X, uint32_t Y, uint32_t Z)
{
	return (X&Y) | (X&Z) | (Y&Z); 
}

static uint32_t H(uint32_t X, uint32_t Y, uint32_t Z)
{
	return X^Y^Z;
}

static uint32_t lshift(uint32_t x, int s)
{
	x &= 0xFFFFFFFF;
	return ((x<<s)&0xFFFFFFFF) | (x>>(32-s));
}

#define ROUND1(a,b,c,d,k,s) a = lshift(a + F(b,c,d) + X[k], s)
#define ROUND2(a,b,c,d,k,s) a = lshift(a + G(b,c,d) + X[k] + (uint32_t)0x5A827999,s)
#define ROUND3(a,b,c,d,k,s) a = lshift(a + H(b,c,d) + X[k] + (uint32_t)0x6ED9EBA1,s)

/* this applies md4 to 64 byte chunks */
static void mdfour64(struct mdfour_state *s, uint32_t *M)
{
	int j;
	uint32_t AA, BB, CC, DD;
	uint32_t X[16];

	for (j=0;j<16;j++)
		X[j] = M[j];

	AA = s->A; BB = s->B; CC = s->C; DD = s->D;

        ROUND1(s->A,s->B,s->C,s->D,  0,  3);  ROUND1(s->D,s->A,s->B,s->C,  1,  7);  
	ROUND1(s->C,s->D,s->A,s->B,  2, 11);  ROUND1(s->B,s->C,s->D,s->A,  3, 19);
        ROUND1(s->A,s->B,s->C,s->D,  4,  3);  ROUND1(s->D,s->A,s->B,s->C,  5,  7);  
	ROUND1(s->C,s->D,s->A,s->B,  6, 11);  ROUND1(s->B,s->C,s->D,s->A,  7, 19);
        ROUND1(s->A,s->B,s->C,s->D,  8,  3);  ROUND1(s->D,s->A,s->B,s->C,  9,  7);  
	ROUND1(s->C,s->D,s->A,s->B, 10, 11);  ROUND1(s->B,s->C,s->D,s->A, 11, 19);
        ROUND1(s->A,s->B,s->C,s->D, 12,  3);  ROUND1(s->D,s->A,s->B,s->C, 13,  7);  
	ROUND1(s->C,s->D,s->A,s->B, 14, 11);  ROUND1(s->B,s->C,s->D,s->A, 15, 19);	

        ROUND2(s->A,s->B,s->C,s->D,  0,  3);  ROUND2(s->D,s->A,s->B,s->C,  4,  5);  
	ROUND2(s->C,s->D,s->A,s->B,  8,  9);  ROUND2(s->B,s->C,s->D,s->A, 12, 13);
        ROUND2(s->A,s->B,s->C,s->D,  1,  3);  ROUND2(s->D,s->A,s->B,s->C,  5,  5);  
	ROUND2(s->C,s->D,s->A,s->B,  9,  9);  ROUND2(s->B,s->C,s->D,s->A, 13, 13);
        ROUND2(s->A,s->B,s->C,s->D,  2,  3);  ROUND2(s->D,s->A,s->B,s->C,  6,  5);  
	ROUND2(s->C,s->D,s->A,s->B, 10,  9);  ROUND2(s->B,s->C,s->D,s->A, 14, 13);
        ROUND2(s->A,s->B,s->C,s->D,  3,  3);  ROUND2(s->D,s->A,s->B,s->C,  7,  5);  
	ROUND2(s->C,s->D,s->A,s->B, 11,  9);  ROUND2(s->B,s->C,s->D,s->A, 15, 13);

	ROUND3(s->A,s->B,s->C,s->D,  0,  3);  ROUND3(s->D,s->A,s->B,s->C,  8,  9);  
	ROUND3(s->C,s->D,s->A,s->B,  4, 11);  ROUND3(s->B,s->C,s->D,s->A, 12, 15);
        ROUND3(s->A,s->B,s->C,s->D,  2,  3);  ROUND3(s->D,s->A,s->B,s->C, 10,  9);  
	ROUND3(s->C,s->D,s->A,s->B,  6, 11);  ROUND3(s->B,s->C,s->D,s->A, 14, 15);
        ROUND3(s->A,s->B,s->C,s->D,  1,  3);  ROUND3(s->D,s->A,s->B,s->C,  9,  9);  
	ROUND3(s->C,s->D,s->A,s->B,  5, 11);  ROUND3(s->B,s->C,s->D,s->A, 13, 15);
        ROUND3(s->A,s->B,s->C,s->D,  3,  3);  ROUND3(s->D,s->A,s->B,s->C, 11,  9);  
	ROUND3(s->C,s->D,s->A,s->B,  7, 11);  ROUND3(s->B,s->C,s->D,s->A, 15, 15);

	s->A += AA; 
	s->B += BB; 
	s->C += CC; 
	s->D += DD;
	
	s->A &= 0xFFFFFFFF; 
	s->B &= 0xFFFFFFFF;
	s->C &= 0xFFFFFFFF; 
	s->D &= 0xFFFFFFFF;

	for (j=0;j<16;j++)
		X[j] = 0;
}

static void copy64(uint32_t *M, const uint8_t *in)
{
	int i;

	for (i=0;i<16;i++)
		M[i] = (in[i*4+3]<<24) | (in[i*4+2]<<16) |
			(in[i*4+1]<<8) | (in[i*4+0]<<0);
}

static void copy4(uint8_t *out, uint32_t x)
{
	out[0] = x&0xFF;
	out[1] = (x>>8)&0xFF;
	out[2] = (x>>16)&0xFF;
	out[3] = (x>>24)&0xFF;
}

/**
 * produce a md4 message digest from data of length n bytes 
 */
_PUBLIC_ void mdfour(uint8_t *out, const uint8_t *in, int n)
{
	uint8_t buf[128];
	uint32_t M[16];
	uint32_t b = n * 8;
	int i;
	struct mdfour_state state;

	state.A = 0x67452301;
	state.B = 0xefcdab89;
	state.C = 0x98badcfe;
	state.D = 0x10325476;

	while (n > 64) {
		copy64(M, in);
		mdfour64(&state, M);
		in += 64;
		n -= 64;
	}

	for (i=0;i<128;i++)
		buf[i] = 0;
	memcpy(buf, in, n);
	buf[n] = 0x80;
	
	if (n <= 55) {
		copy4(buf+56, b);
		copy64(M, buf);
		mdfour64(&state, M);
	} else {
		copy4(buf+120, b); 
		copy64(M, buf);
		mdfour64(&state, M);
		copy64(M, buf+64);
		mdfour64(&state, M);
	}

	for (i=0;i<128;i++)
		buf[i] = 0;
	copy64(M, buf);

	copy4(out, state.A);
	copy4(out+4, state.B);
	copy4(out+8, state.C);
	copy4(out+12, state.D);
}


