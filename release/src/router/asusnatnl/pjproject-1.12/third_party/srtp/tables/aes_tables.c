/*
 * aes_tables.c
 * 
 * generate tables for the AES cipher
 * 
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *	
 * Copyright(c) 2001-2006 Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include "gf2_8.h"
#include "crypto_math.h"


unsigned char aes_sbox[256];

unsigned char aes_inv_sbox[256];

uint32_t T0[256], T1[256], T2[256], T3[256], T4[256]; 


#define AES_INVERSE_TEST 0  /* set to 1 to test forward/backwards aes */

/* functions for precomputing AES values */

/*
 * A[] is the 8 x 8 binary matrix (represented as an array of columns,
 * where each column is an octet) which defines the affine
 * transformation used in the AES substitution table (Section
 * 4.2.1 of the spec).
 */

uint8_t A[8] = { 31, 62, 124, 248, 241, 227, 199, 143 };

/*
 * b is the 8 bit vector (represented as an octet) used in the affine
 * transform described above.
 */

uint8_t b = 99;


void
aes_init_sbox(void) {
  unsigned int i;
  uint8_t x;
  
  for (i=0; i < 256; i++) {
    x = gf2_8_compute_inverse((gf2_8)i);
    x = A_times_x_plus_b(A, x, b);
    aes_sbox[i] = x;
    aes_inv_sbox[x] = i;
  }
}

void
aes_compute_tables(void) {
  int i;
  uint32_t x1, x2, x3;
  v32_t tmp;

  /* initialize substitution table */
  aes_init_sbox();

  /* combine sbox with linear operations to form 8-bit to 32-bit tables */
  for (i=0; i < 256; i++) {
    x1 = aes_sbox[i];
    x2 = gf2_8_shift(x1);
    x3 = x2 ^ x1;

    tmp.v8[0] = x2;
    tmp.v8[1] = x1;
    tmp.v8[2] = x1;
    tmp.v8[3] = x3;
    T0[i] = tmp.value;

    tmp.v8[0] = x3;
    tmp.v8[1] = x2;
    tmp.v8[2] = x1;
    tmp.v8[3] = x1;
    T1[i] = tmp.value;
     
    tmp.v8[0] = x1;
    tmp.v8[1] = x3;
    tmp.v8[2] = x2;
    tmp.v8[3] = x1;
    T2[i] = tmp.value;

    tmp.v8[0] = x1;
    tmp.v8[1] = x1;
    tmp.v8[2] = x3;
    tmp.v8[3] = x2;
    T3[i] = tmp.value;
     
  }
}


/*
 * the tables U0, U1, U2, U3 implement the aes operations invSubBytes,
 * invMixColumns, and invShiftRows 
 */

uint32_t U0[256], U1[256], U2[256], U3[256], U4[256];

extern uint8_t aes_inv_sbox[256];

void
aes_compute_inv_tables(void) {
  int i;
  uint8_t x, xe, x9, xd, xb;
  v32_t tmp;

  /* combine sbox with linear operations to form 8-bit to 32-bit tables */
  for (i=0; i < 256; i++) {
     x = aes_inv_sbox[i];

     xe = gf2_8_multiply(0x0e, x);     
     x9 = gf2_8_multiply(0x09, x);     
     xd = gf2_8_multiply(0x0d, x);     
     xb = gf2_8_multiply(0x0b, x);     

     tmp.v8[0] = xe;
     tmp.v8[1] = x9;
     tmp.v8[2] = xd;
     tmp.v8[3] = xb;
     U0[i] = tmp.value;

     tmp.v8[0] = xb;
     tmp.v8[1] = xe;
     tmp.v8[2] = x9;
     tmp.v8[3] = xd;
     U1[i] = tmp.value;
     
     tmp.v8[0] = xd;
     tmp.v8[1] = xb;
     tmp.v8[2] = xe;
     tmp.v8[3] = x9;
     U2[i] = tmp.value;

     tmp.v8[0] = x9;
     tmp.v8[1] = xd;
     tmp.v8[2] = xb;
     tmp.v8[3] = xe;
     U3[i] = tmp.value;

     tmp.v8[0] = tmp.v8[1] = tmp.v8[2] = tmp.v8[3] = x;
     U4[i] = tmp.value;
   }
}


/*
 * aes_test_inverse() returns err_status_ok if aes
 * encryption and decryption are true inverses of each other, and
 * returns err_status_algo_fail otherwise
 */

#include "err.h"

err_status_t
aes_test_inverse(void);

#define TABLES_32BIT 1

int
main(void) {
  int i;

  aes_init_sbox();
  aes_compute_inv_tables();

#if TABLES_32BIT
  printf("uint32_t U0 = {");
  for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%0x, ", U0[i]);
  }
  printf("\n}\n");

 printf("uint32_t U1 = {");
  for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%x, ", U1[i]);
  }
  printf("\n}\n");

 printf("uint32_t U2 = {");
  for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%x, ", U2[i]);
  }
  printf("\n}\n");

 printf("uint32_t U3 = {");
  for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%x, ", U3[i]);
  }
  printf("\n}\n");

 printf("uint32_t U4 = {");
 for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%x, ", U4[i]);
  }
  printf("\n}\n");

#else

  printf("uint32_t U0 = {");
  for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%lx, ", U0[i]);
  }
  printf("\n}\n");

 printf("uint32_t U1 = {");
  for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%lx, ", U1[i]);
  }
  printf("\n}\n");

 printf("uint32_t U2 = {");
  for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%lx, ", U2[i]);
  }
  printf("\n}\n");

 printf("uint32_t U3 = {");
  for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%lx, ", U3[i]);
  }
  printf("\n}\n");

 printf("uint32_t U4 = {");
 for (i=0; i < 256; i++) {
    if ((i % 4) == 0)
      printf("\n");
    printf("0x%lx, ", U4[i]);
  }
  printf("\n}\n");


#endif /* TABLES_32BIT */


#if AES_INVERSE_TEST
  /* 
   * test that aes_encrypt and aes_decrypt are actually
   * inverses of each other 
   */
    
  printf("aes inverse test: ");
  if (aes_test_inverse() == err_status_ok)
    printf("passed\n");
  else {
    printf("failed\n");
    exit(1);
  }
#endif
  
  return 0;
}

#if AES_INVERSE_TEST

err_status_t
aes_test_inverse(void) {
  v128_t x, y;
  aes_expanded_key_t expanded_key, decrypt_key;
  uint8_t plaintext[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff 
  };
  uint8_t key[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
  };
  v128_t k;
  v128_set_to_zero(&x);
  
  v128_copy_octet_string(&k, key);
  v128_copy_octet_string(&x, plaintext);
  aes_expand_encryption_key(k, expanded_key);
  aes_expand_decryption_key(k, decrypt_key);
  aes_encrypt(&x, expanded_key);
  aes_decrypt(&x, decrypt_key);
  
  /* compare to expected value then report */
  v128_copy_octet_string(&y, plaintext);

  if (v128_is_eq(&x, &y))
    return err_status_ok;
  return err_status_algo_fail;
  
}
 
#endif 
