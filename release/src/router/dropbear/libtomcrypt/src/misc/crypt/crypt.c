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
#include "tomcrypt.h"

/**
  @file crypt.c
  Build strings, Tom St Denis
*/  

/*
const char *crypt_build_settings =
   "LibTomCrypt " SCRYPT " (Tom St Denis, tomstdenis@gmail.com)\n"
   "LibTomCrypt is public domain software.\n"
   "Built on " __DATE__ " at " __TIME__ "\n\n\n"
   "Endianess: "
#if defined(ENDIAN_NEUTRAL)
   "neutral\n"
#elif defined(ENDIAN_LITTLE)
   "little"
   #if defined(ENDIAN_32BITWORD)
   " (32-bit words)\n"
   #else
   " (64-bit words)\n"
   #endif
#elif defined(ENDIAN_BIG)
   "big"
   #if defined(ENDIAN_32BITWORD)
   " (32-bit words)\n"
   #else
   " (64-bit words)\n"
   #endif
#endif
   "Clean stack: "
#if defined(LTC_CLEAN_STACK)
   "enabled\n"
#else
   "disabled\n"
#endif
   "Ciphers built-in:\n"
#if defined(BLOWFISH)
   "   Blowfish\n"
#endif
#if defined(RC2)
   "   RC2\n"
#endif
#if defined(RC5)
   "   RC5\n"
#endif
#if defined(RC6)
   "   RC6\n"
#endif
#if defined(SAFERP)
   "   Safer+\n"
#endif
#if defined(SAFER)
   "   Safer\n"
#endif
#if defined(RIJNDAEL)
   "   Rijndael\n"
#endif
#if defined(XTEA)
   "   XTEA\n"
#endif
#if defined(TWOFISH)
   "   Twofish "
   #if defined(TWOFISH_SMALL) && defined(TWOFISH_TABLES) && defined(TWOFISH_ALL_TABLES)
       "(small, tables, all_tables)\n"
   #elif defined(TWOFISH_SMALL) && defined(TWOFISH_TABLES)
       "(small, tables)\n"
   #elif defined(TWOFISH_SMALL) && defined(TWOFISH_ALL_TABLES)
       "(small, all_tables)\n"
   #elif defined(TWOFISH_TABLES) && defined(TWOFISH_ALL_TABLES)
       "(tables, all_tables)\n"
   #elif defined(TWOFISH_SMALL)
       "(small)\n"
   #elif defined(TWOFISH_TABLES)
       "(tables)\n"
   #elif defined(TWOFISH_ALL_TABLES)
       "(all_tables)\n"
   #else
       "\n"
   #endif
#endif
#if defined(DES)
   "   DES\n"
#endif
#if defined(CAST5)
   "   CAST5\n"
#endif
#if defined(NOEKEON)
   "   Noekeon\n"
#endif
#if defined(SKIPJACK)
   "   Skipjack\n"
#endif
#if defined(KHAZAD)
   "   Khazad\n"
#endif
#if defined(ANUBIS)
   "   Anubis "
#endif
#if defined(ANUBIS_TWEAK)
   " (tweaked)"
#endif
   "\n"
#if defined(KSEED)
   "   KSEED\n"
#endif
#if defined(LTC_KASUMI)
   "   KASUMI\n"
#endif

    "\nHashes built-in:\n"
#if defined(SHA512)
   "   SHA-512\n"
#endif
#if defined(SHA384)
   "   SHA-384\n"
#endif
#if defined(SHA256)
   "   SHA-256\n"
#endif
#if defined(SHA224)
   "   SHA-224\n"
#endif
#if defined(TIGER)
   "   TIGER\n"
#endif
#if defined(SHA1)
   "   SHA1\n"
#endif
#if defined(MD5)
   "   MD5\n"
#endif
#if defined(MD4)
   "   MD4\n"
#endif
#if defined(MD2)
   "   MD2\n"
#endif
#if defined(RIPEMD128)
   "   RIPEMD128\n"
#endif
#if defined(RIPEMD160)
   "   RIPEMD160\n"
#endif
#if defined(WHIRLPOOL)
   "   WHIRLPOOL\n"
#endif
#if defined(CHC_HASH)
   "   CHC_HASH \n"
#endif

    "\nBlock Chaining Modes:\n"
#if defined(LTC_CFB_MODE)
    "   CFB\n"
#endif
#if defined(LTC_OFB_MODE)
    "   OFB\n"
#endif
#if defined(LTC_ECB_MODE)
    "   ECB\n"
#endif
#if defined(LTC_CBC_MODE)
    "   CBC\n"
#endif
#if defined(LTC_CTR_MODE)
    "   CTR "
#endif
#if defined(LTC_CTR_OLD)
    " (CTR_OLD) "
#endif
    "\n"
#if defined(LRW_MODE)
    "   LRW_MODE"
#if defined(LRW_TABLES)
    " (LRW_TABLES) "
#endif
    "\n"
#endif
#if defined(LTC_F8_MODE)
    "   F8 MODE\n"
#endif    

    "\nMACs:\n"
#if defined(LTC_HMAC)
    "   HMAC\n"
#endif
#if defined(LTC_OMAC)
    "   OMAC\n"
#endif
#if defined(LTC_PMAC)
    "   PMAC\n"
#endif
#if defined(PELICAN)
    "   PELICAN\n"
#endif
#if defined(LTC_XCBC)
    "   XCBC-MAC\n"
#endif
#if defined(LTC_F9_MODE)
    "   F9-MAC\n"
#endif

    "\nENC + AUTH modes:\n"
#if defined(EAX_MODE)
    "   EAX_MODE\n"
#endif
#if defined(OCB_MODE)
    "   OCB_MODE\n"
#endif
#if defined(CCM_MODE)
    "   CCM_MODE\n"
#endif
#if defined(GCM_MODE)
    "   GCM_MODE "
#endif
#if defined(GCM_TABLES)
    " (GCM_TABLES) "
#endif
   "\n"

    "\nPRNG:\n"
#if defined(YARROW)
    "   Yarrow\n"
#endif
#if defined(SPRNG)
    "   SPRNG\n"
#endif
#if defined(RC4)
    "   RC4\n"
#endif
#if defined(FORTUNA)
    "   Fortuna\n"
#endif
#if defined(SOBER128)
    "   SOBER128\n"
#endif

    "\nPK Algs:\n"
#if defined(MRSA)
    "   RSA \n"
#endif
#if defined(MECC)
    "   ECC\n"
#endif
#if defined(MDSA)
    "   DSA\n"
#endif
#if defined(MKAT)
    "   Katja\n"
#endif    

    "\nCompiler:\n"
#if defined(WIN32)
    "   WIN32 platform detected.\n"
#endif
#if defined(__CYGWIN__)
    "   CYGWIN Detected.\n"
#endif
#if defined(__DJGPP__)
    "   DJGPP Detected.\n"
#endif
#if defined(_MSC_VER)
    "   MSVC compiler detected.\n"
#endif
#if defined(__GNUC__)
    "   GCC compiler detected.\n"
#endif
#if defined(INTEL_CC)
    "   Intel C Compiler detected.\n"
#endif
#if defined(__x86_64__)
    "   x86-64 detected.\n"
#endif
#if defined(LTC_PPC32)
    "   LTC_PPC32 defined \n"
#endif    

    "\nVarious others: "
#if defined(BASE64)
    " BASE64 "
#endif
#if defined(MPI)
    " MPI "
#endif
#if defined(TRY_UNRANDOM_FIRST)
    " TRY_UNRANDOM_FIRST "
#endif
#if defined(LTC_TEST)
    " LTC_TEST "
#endif
#if defined(PKCS_1)
    " PKCS#1 "
#endif
#if defined(PKCS_5)
    " PKCS#5 "
#endif
#if defined(LTC_SMALL_CODE)
    " LTC_SMALL_CODE "
#endif
#if defined(LTC_NO_FILE)
    " LTC_NO_FILE "
#endif
#if defined(LTC_DER)
    " LTC_DER "
#endif
#if defined(LTC_FAST)
    " LTC_FAST "
#endif
#if defined(LTC_NO_FAST)
    " LTC_NO_FAST "
#endif
#if defined(LTC_NO_BSWAP)
    " LTC_NO_BSWAP "
#endif
#if defined(LTC_NO_ASM)
    " LTC_NO_ASM "
#endif
#if defined(LTC_NO_TEST)
    " LTC_NO_TEST "
#endif
#if defined(LTC_NO_TABLES)
    " LTC_NO_TABLES "
#endif
#if defined(LTC_PTHREAD)
    " LTC_PTHREAD "
#endif
#if defined(LTM_DESC)
    " LTM_DESC "
#endif
#if defined(TFM_DESC)
    " TFM_DESC "
#endif
#if defined(MECC_ACCEL)
    " MECC_ACCEL "
#endif
#if defined(GMP_DESC)
    " GMP_DESC "
#endif
#if defined(LTC_EASY)
    " (easy) "
#endif    
#if defined(MECC_FP)
   " MECC_FP "
#endif
#if defined(LTC_ECC_SHAMIR)
   " LTC_ECC_SHAMIR "
#endif
    "\n"
    "\n\n\n"
    ;
	*/


/* $Source: /cvs/libtom/libtomcrypt/src/misc/crypt/crypt.c,v $ */
/* $Revision: 1.27 $ */
/* $Date: 2006/12/03 03:50:45 $ */
