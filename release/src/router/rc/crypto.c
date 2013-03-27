/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bcmcrypto/dh.h>

#define _OPENSSL_malloc malloc
#define _OPENSSL_free free

#define DEBUG 0
#define FUNTEST 0
#define BRCM 
#define MAX_PRIME_NUM 50
#define _dbg(fmt, arg...)       //printf(fmt, ##arg)

unsigned long p1536[] = {
 0xfe6adceb, 	
 0xbd3934a4, 	
 0x31139db1, 	
 0x716fc9ec, 	
 0x4f464d0e, 	
 0x96783772, 	
 0x6c22c9d7, 	
 0x1b7e34d5, 	
 0x40d9b1e7, 	
 0x55df9cb1, 	
 0xdfbec820, 	
 0xb053c663, 	
 0x8e4577a6, 	
 0x88d6f5ca, 	
 0x7143063f, 	
 0x09cf3ed6, 	
 0x2afc249a, 	
 0x25a0fdf4, 	
 0x0c939c04, 	
 0x4451ad44, 	
 0xe60a2efc, 	
 0x67f19747, 	
 0x840bb2f7, 	
 0xe31f9830, 	
 0x66d30ad7, 	
 0x36298c7f, 	
 0xb3802af6, 	
 0x9c79b075, 	
 0xc15eb521, 	
 0xc72abad9, 	
 0x9d0ec244, 	
 0x19385d0e, 	
 0x9386226a, 	
 0x93580c99, 	
 0x3983a696, 	
 0xa735c837, 	
 0x974f4e09, 	
 0x91b101ed, 	
 0x623ff671, 	
 0x8c547d0c, 	
 0x6d7cb0ac, 	
 0x1377ff20, 	
 0xf1c50a11, 	
 0xf5071153, 	
 0x849758ba, 	
 0x5f92e05c, 	
 0xe149fa4d, 	
 0xab062592};

void ots_rand(unsigned char *rnd, int rndlen)
{
	time_t tim;
	int i, vrand;

	/* make a random number and set the top and bottom bits */
	time(&tim);
	srand((unsigned long)tim);
	
	for (i=0;i<rndlen;i++)
	{
		vrand = rand();		
		rnd[i] = vrand;
	}
	return;
}

#ifdef FUNTEST
#else
int main(int argc, char *argv)
{
	DH *a;
	DH *b=NULL;
	unsigned char *abuf=NULL,*bbuf=NULL;
	unsigned char *apubbuf=NULL,*bpubbuf=NULL;
	int i,alen,blen,apublen,bpublen,aout,bout,ret=1, size = 0;

	BN_register_RAND(ots_rand);

	a = DH_init(p1536, 192, 5);

	_dbg("\np    =");
	bn_print(a->p);
	_dbg("\ng    =");
	bn_print(a->g);
	_dbg("\n");


	b=DH_new();
	if (b == NULL) goto err;

	b->p=a->p;
	b->g=a->g;
	if ((b->p == NULL) || (b->g == NULL)) goto err;

	apubbuf = (unsigned char *)_OPENSSL_malloc(DH_size(a));
	/* _dbg("%s\n", file2str("/proc/uptime")); */
	if (!(apublen=DH_generate_key(apubbuf,a))) goto err;
	/* _dbg("%s\n", file2str("/proc/uptime")); */
	_dbg("pri 1=");
	bn_print(a->priv_key);
	_dbg("\npub 1:%x=", apublen);
	for (i=0; i<apublen; i++) _dbg("%02X",apubbuf[i]);
	_dbg("\n");

	bpubbuf = (unsigned char *)_OPENSSL_malloc(DH_size(b));
	if (!(bpublen=DH_generate_key(bpubbuf,b))) goto err;
	_dbg("pri 2=");
	bn_print(b->priv_key);
	_dbg("\npub 2:%x=", bpublen);
	for (i=0; i<bpublen; i++) _dbg("%02X",bpubbuf[i]);
	_dbg("\n");

	alen=DH_size(a);
	abuf=(unsigned char *)_OPENSSL_malloc(alen);
	/* _dbg("%s\n", file2str("/proc/uptime")); */
	aout=DH_compute_key(abuf,bpubbuf, bpublen,a);
	/* _dbg("%s\n", file2str("/proc/uptime")); */

	_dbg("key1:%x =", aout);
	for (i=0; i<aout; i++) _dbg("%02X",abuf[i]);
	_dbg("\n");

	blen=DH_size(b);
	bbuf=(unsigned char *)_OPENSSL_malloc(blen);
	bout=DH_compute_key(bbuf,apubbuf,apublen,b);

	_dbg("key2:%x =", bout);
	for (i=0; i<bout; i++) _dbg("%02X",bbuf[i]);
	_dbg("\n");

	if ((aout < 4) || (bout != aout) || (memcmp(abuf,bbuf,aout) != 0))
		{
		fprintf(stderr,"Error in DH routines\n");
		ret=1;
		}
	else
		ret=0;
err:
	if (abuf != NULL) _OPENSSL_free(abuf);
	if (bbuf != NULL) _OPENSSL_free(bbuf);
	if (b != NULL) DH_free(b);
	if (a != NULL) DH_free(a);
	return (ret);
}

int DH_size(const DH *dh)
	{
	return (BN_num_bytes(dh->p));
	}


static const char *Hex="0123456789ABCDEF";

static int bn_print(const BIGNUM *a)
{
	int i,j,v,z=0;
	int ret=0;

	if ((a->neg) && (_dbg("-") != 1)) goto end;
	if ((a->top == 0) && (_dbg("0") != 1)) goto end;
	for (i=a->top-1; i >=0; i--)
		{
		for (j=BN_BITS2-4; j >= 0; j-=4)
			{
			/* strip leading zeros */
			v=((int)(a->d[i]>>(long)j))&0x0f;
			if (z || (v != 0))
				{
				_dbg("%c", Hex[v]);
				z=1;
				}
			}
		}
	ret=1;
end:
	return (ret);
}
#endif
