/*
 * aes_calc.c
 * 
 * A simple AES calculator for generating AES encryption values
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */

/*
  
 Example usage (with first NIST FIPS 197 test case):
 
[sh]$ test/aes_calc 000102030405060708090a0b0c0d0e0f 00112233445566778899aabbccddeeff -v
 plaintext:      00112233445566778899aabbccddeeff
 key:            000102030405060708090a0b0c0d0e0f
 ciphertext:     69c4e0d86a7b0430d8cdb78070b4c55a

 */

#include "aes.h"
#include <stdio.h>
#include <string.h>

void
usage(char *prog_name) {
  printf("usage: %s <key> <plaintext> [-v]\n", prog_name);
  exit(255);
}

#define AES_KEY_LEN 16

int
main (int argc, char *argv[]) {
  v128_t data, key;
  aes_expanded_key_t exp_key;
  int len;
  int verbose;

  if (argc == 3) {
    /* we're not in verbose mode */
    verbose = 0;
  } else if (argc == 4) {
    if (strncmp(argv[3], "-v", 2) == 0) {
      /* we're in verbose mode */
      verbose = 1;
    } else {
      /* unrecognized flag, complain and exit */
      usage(argv[0]);
    }
  } else {
    /* we've been fed the wrong number of arguments - compain and exit */
    usage(argv[0]);
  }
  
  /* read in key, checking length */
  if (strlen(argv[1]) > AES_KEY_LEN*2) {
    fprintf(stderr, 
	    "error: too many digits in key "
	    "(should be %d hexadecimal digits, found %u)\n",
	    AES_KEY_LEN*2, (unsigned)strlen(argv[1]));
    exit(1);    
  }
  len = hex_string_to_octet_string((char *)&key, argv[1], AES_KEY_LEN*2);
  /* check that hex string is the right length */
  if (len < AES_KEY_LEN*2) {
    fprintf(stderr, 
	    "error: too few digits in key "
	    "(should be %d hexadecimal digits, found %d)\n",
	    AES_KEY_LEN*2, len);
    exit(1);    
  } 
      
  /* read in plaintext, checking length */
  if (strlen(argv[2]) > 16*2) {
    fprintf(stderr, 
	    "error: too many digits in plaintext "
	    "(should be %d hexadecimal digits, found %u)\n",
	    16*2, (unsigned)strlen(argv[2]));
    exit(1);    
  }
  len = hex_string_to_octet_string((char *)(&data), argv[2], 16*2);
  /* check that hex string is the right length */
  if (len < 16*2) {
    fprintf(stderr, 
	    "error: too few digits in plaintext "
	    "(should be %d hexadecimal digits, found %d)\n",
	    16*2, len);
    exit(1);    
  }

  if (verbose) {
    /* print out plaintext */
    printf("plaintext:\t%s\n", octet_string_hex_string((uint8_t *)&data, 16));
  }

  /* encrypt plaintext */
  aes_expand_encryption_key(&key, exp_key);

  aes_encrypt(&data, exp_key);

  /* write ciphertext to output */
  if (verbose) {
    printf("key:\t\t%s\n", v128_hex_string(&key));
    printf("ciphertext:\t");
  }
  printf("%s\n", v128_hex_string(&data));

  return 0;
}

