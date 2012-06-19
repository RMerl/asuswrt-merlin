#ifndef TOMCRYPT_CUSTOM_H_
#define TOMCRYPT_CUSTOM_H_

/* this will sort out which stuff based on the user-config in options.h */
#include "options.h"

/* macros for various libc functions you can change for embedded targets */
#ifndef XMALLOC
   #ifdef malloc 
   #define LTC_NO_PROTOTYPES
   #endif
#define XMALLOC  malloc
#endif
#ifndef XREALLOC
   #ifdef realloc 
   #define LTC_NO_PROTOTYPES
   #endif
#define XREALLOC realloc
#endif
#ifndef XCALLOC
   #ifdef calloc 
   #define LTC_NO_PROTOTYPES
   #endif
#define XCALLOC  calloc
#endif
#ifndef XFREE
   #ifdef free
   #define LTC_NO_PROTOTYPES
   #endif
#define XFREE    free
#endif

#ifndef XMEMSET
   #ifdef memset
   #define LTC_NO_PROTOTYPES
   #endif
#define XMEMSET  memset
#endif
#ifndef XMEMCPY
   #ifdef memcpy
   #define LTC_NO_PROTOTYPES
   #endif
#define XMEMCPY  memcpy
#endif
#ifndef XMEMCMP
   #ifdef memcmp 
   #define LTC_NO_PROTOTYPES
   #endif
#define XMEMCMP  memcmp
#endif
#ifndef XSTRCMP
   #ifdef strcmp
   #define LTC_NO_PROTOTYPES
   #endif
#define XSTRCMP strcmp
#endif

#ifndef XCLOCK
#define XCLOCK   clock
#endif
#ifndef XCLOCKS_PER_SEC
#define XCLOCKS_PER_SEC CLOCKS_PER_SEC
#endif

   #define LTC_NO_PRNGS
   #define LTC_NO_PK
#ifdef DROPBEAR_SMALL_CODE
#define LTC_SMALL_CODE
#endif
/* These spit out warnings etc */
#define LTC_NO_ROLC

/* Enable self-test test vector checking */
/* Not for dropbear */
/*#define LTC_TEST*/

/* clean the stack of functions which put private information on stack */
/* #define LTC_CLEAN_STACK */

/* disable all file related functions */
#define LTC_NO_FILE

/* disable all forms of ASM */
/* #define LTC_NO_ASM */

/* disable FAST mode */
/* #define LTC_NO_FAST */

/* disable BSWAP on x86 */
/* #define LTC_NO_BSWAP */


#ifdef DROPBEAR_BLOWFISH
#define BLOWFISH
#endif

#ifdef DROPBEAR_AES
#define RIJNDAEL
#endif

#ifdef DROPBEAR_TWOFISH
#define TWOFISH

/* enabling just TWOFISH_SMALL will make the binary ~1kB smaller, turning on
 * TWOFISH_TABLES will make it a few kB bigger, but perhaps reduces runtime
 * memory usage? */
#define TWOFISH_SMALL
/*#define TWOFISH_TABLES*/
#endif

#ifdef DROPBEAR_3DES
#define DES
#endif

#define LTC_CBC_MODE

#ifdef DROPBEAR_ENABLE_CTR_MODE
#define LTC_CTR_MODE
#endif

#if defined(DROPBEAR_DSS) && defined(DSS_PROTOK)
#define SHA512
#endif

#define SHA1

#ifdef DROPBEAR_MD5_HMAC
#define MD5
#endif

#define LTC_HMAC

/* Various tidbits of modern neatoness */
#define BASE64

/* default no pthread functions */
#define LTC_MUTEX_GLOBAL(x)
#define LTC_MUTEX_PROTO(x)
#define LTC_MUTEX_TYPE(x)
#define LTC_MUTEX_INIT(x)
#define LTC_MUTEX_LOCK(x)
#define LTC_MUTEX_UNLOCK(x)
#define FORTUNA_POOLS 0

/* Debuggers */

/* define this if you use Valgrind, note: it CHANGES the way SOBER-128 and RC4 work (see the code) */
/* #define LTC_VALGRIND */

#endif



/* $Source: /cvs/libtom/libtomcrypt/src/headers/tomcrypt_custom.h,v $ */
/* $Revision: 1.66 $ */
/* $Date: 2006/12/04 02:50:11 $ */
