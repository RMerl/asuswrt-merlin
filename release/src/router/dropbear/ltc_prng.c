/* Copied from libtomcrypt/src/prngs/sprng.c and modified to
 * use Dropbear's genrandom(). */

/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtomcrypt.com
 */
#include "options.h"
#include "includes.h"
#include "dbrandom.h"
#include "ltc_prng.h"

/**
   @file sprng.c
   Secure PRNG, Tom St Denis
*/
   
/* A secure PRNG using the RNG functions.  Basically this is a
 * wrapper that allows you to use a secure RNG as a PRNG
 * in the various other functions.
 */

#ifdef DROPBEAR_LTC_PRNG

/**
  Start the PRNG
  @param prng     [out] The PRNG state to initialize
  @return CRYPT_OK if successful
*/  
int dropbear_prng_start(prng_state* UNUSED(prng))
{
   return CRYPT_OK;  
}

/**
  Add entropy to the PRNG state
  @param in       The data to add
  @param inlen    Length of the data to add
  @param prng     PRNG state to update
  @return CRYPT_OK if successful
*/  
int dropbear_prng_add_entropy(const unsigned char* UNUSED(in), unsigned long UNUSED(inlen), prng_state* UNUSED(prng))
{
   return CRYPT_OK;
}

/**
  Make the PRNG ready to read from
  @param prng   The PRNG to make active
  @return CRYPT_OK if successful
*/  
int dropbear_prng_ready(prng_state* UNUSED(prng))
{
   return CRYPT_OK;
}

/**
  Read from the PRNG
  @param out      Destination
  @param outlen   Length of output
  @param prng     The active PRNG to read from
  @return Number of octets read
*/  
unsigned long dropbear_prng_read(unsigned char* out, unsigned long outlen, prng_state* UNUSED(prng))
{
   LTC_ARGCHK(out != NULL);
   genrandom(out, outlen);
   return outlen;
}

/**
  Terminate the PRNG
  @param prng   The PRNG to terminate
  @return CRYPT_OK if successful
*/  
int dropbear_prng_done(prng_state* UNUSED(prng))
{
   return CRYPT_OK;
}

/**
  Export the PRNG state
  @param out       [out] Destination
  @param outlen    [in/out] Max size and resulting size of the state
  @param prng      The PRNG to export
  @return CRYPT_OK if successful
*/  
int dropbear_prng_export(unsigned char* UNUSED(out), unsigned long* outlen, prng_state* UNUSED(prng))
{
   LTC_ARGCHK(outlen != NULL);

   *outlen = 0;
   return CRYPT_OK;
}
 
/**
  Import a PRNG state
  @param in       The PRNG state
  @param inlen    Size of the state
  @param prng     The PRNG to import
  @return CRYPT_OK if successful
*/  
int dropbear_prng_import(const unsigned char* UNUSED(in), unsigned long UNUSED(inlen), prng_state* UNUSED(prng))
{
   return CRYPT_OK;
}

/**
  PRNG self-test
  @return CRYPT_OK if successful, CRYPT_NOP if self-testing has been disabled
*/  
int dropbear_prng_test(void)
{
   return CRYPT_OK;
}

const struct ltc_prng_descriptor dropbear_prng_desc =
{
    "dropbear_prng", 0,
    &dropbear_prng_start,
    &dropbear_prng_add_entropy,
    &dropbear_prng_ready,
    &dropbear_prng_read,
    &dropbear_prng_done,
    &dropbear_prng_export,
    &dropbear_prng_import,
    &dropbear_prng_test
};


#endif /* DROPBEAR_LTC_PRNG */
