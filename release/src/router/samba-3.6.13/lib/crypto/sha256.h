/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: sha.h 17450 2006-05-05 11:11:43Z lha $ */

#ifndef HEIM_SHA_H
/*
  based on heildal lib/hcrypto/sha.h. Copied to lib/crypto to avoid a link
  problem. Hopefully will be removed once we solve this link problem

   (tridge)
 */
#define HEIM_SHA_H 1

#if 0
/* symbol renaming */
#define SHA1_Init hc_SHA1_Init
#define SHA1_Update hc_SHA1_Update
#define SHA1_Final hc_SHA1_Final
#define SHA256_Init hc_SHA256_Init
#define SHA256_Update hc_SHA256_Update
#define SHA256_Final hc_SHA256_Final
#endif

/*
 * SHA-1
 */

#define SHA_DIGEST_LENGTH 20

struct sha {
  unsigned int sz[2];
  uint32_t counter[5];
  unsigned char save[64];
};

typedef struct sha SHA_CTX;

void SHA1_Init (struct sha *m);
void SHA1_Update (struct sha *m, const void *v, size_t len);
void SHA1_Final (void *res, struct sha *m);

/*
 * SHA-2 256
 */

#define SHA256_DIGEST_LENGTH 32

struct hc_sha256state {
  unsigned int sz[2];
  uint32_t counter[8];
  unsigned char save[64];
};

typedef struct hc_sha256state SHA256_CTX;

void SHA256_Init (SHA256_CTX *);
void SHA256_Update (SHA256_CTX *, const void *, size_t);
void SHA256_Final (void *, SHA256_CTX *);

#endif /* HEIM_SHA_H */
