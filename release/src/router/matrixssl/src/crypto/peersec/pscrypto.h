/*
 *	pscrypto.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Internal definitions for PeerSec Networks MatrixSSL cryptography provider
 */
/*
 *	Copyright (c) PeerSec Networks, 2002-2009. All Rights Reserved.
 *	The latest version of this code is available at http://www.matrixssl.org
 *
 *	This software is open source; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This General Public License does NOT permit incorporating this software 
 *	into proprietary programs.  If you are unable to comply with the GPL, a 
 *	commercial license for this software may be purchased from PeerSec Networks
 *	at http://www.peersec.com
 *	
 *	This program is distributed in WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	See the GNU General Public License for more details.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *	http://www.gnu.org/copyleft/gpl.html
 */
/******************************************************************************/

#ifndef _h_PSCRYPTO
#define _h_PSCRYPTO

#ifdef __cplusplus
extern "C" {
#endif

/*
	PeerSec crypto-specific defines.
 */
#define SMALL_CODE
#define CLEAN_STACK
/*
	If Native 64 bit integers are not supported, we must set the 16 bit flag
	to produce 32 bit mp_words in mpi.h
	We must also include the slow MPI functions because the fast ones only
	work with larger (28 bit) digit sizes.
*/
#ifndef USE_INT64
#define MP_16BIT
#define USE_SMALL_WORD
#endif /* USE_INT64 */

/******************************************************************************/

#ifdef USE_RSA

#include "mpi.h"

#if LINUX
	#define _stat stat
#endif

/* this is the "32-bit at least" data type 
 * Re-define it to suit your platform but it must be at least 32-bits 
 */
typedef unsigned long ulong32;

/*
	Primary RSA Key struct.  Define here for crypto
*/
typedef struct {
	mp_int		e, d, N, qP, dP, dQ, p, q;
	int32			size;	/* Size of the key in bytes */
	int32			optimized; /* 1 for optimized */
} sslRsaKey_t;

#endif /* USE_RSA */



/*
 *	Private
 */
extern int32 ps_base64_decode(const unsigned char *in, uint32 len, 
							unsigned char *out, uint32 *outlen);

/*
 *	Memory routines
 */
extern void psZeromem(void *dst, size_t len);
extern void psBurnStack(unsigned long len);


/* max size of either a cipher/hash block or symmetric key [largest of the two] */
#define MAXBLOCKSIZE			24

/* ch1-01-1 */
/* error codes [will be expanded in future releases] */
enum {
	CRYPT_OK=0,					/* Result OK */
	CRYPT_ERROR,				/* Generic Error */
	CRYPT_NOP,					/* Not a failure but no operation was performed */

	CRYPT_INVALID_KEYSIZE,		/* Invalid key size given */
	CRYPT_INVALID_ROUNDS,		/* Invalid number of rounds */
	CRYPT_FAIL_TESTVECTOR,		/* Algorithm failed test vectors */

	CRYPT_BUFFER_OVERFLOW,		/* Not enough space for output */
	CRYPT_INVALID_PACKET,		/* Invalid input packet given */

	CRYPT_INVALID_PRNGSIZE,		/* Invalid number of bits for a PRNG */
	CRYPT_ERROR_READPRNG,		/* Could not read enough from PRNG */

	CRYPT_INVALID_CIPHER,		/* Invalid cipher specified */
	CRYPT_INVALID_HASH,			/* Invalid hash specified */
	CRYPT_INVALID_PRNG,			/* Invalid PRNG specified */

	CRYPT_MEM,					/* Out of memory */

	CRYPT_PK_TYPE_MISMATCH,		/* Not equivalent types of PK keys */
	CRYPT_PK_NOT_PRIVATE,		/* Requires a private PK key */

	CRYPT_INVALID_ARG,			/* Generic invalid argument */
	CRYPT_FILE_NOTFOUND,		/* File Not Found */

	CRYPT_PK_INVALID_TYPE,		/* Invalid type of PK key */
	CRYPT_PK_INVALID_SYSTEM,	/* Invalid PK system specified */
	CRYPT_PK_DUP,				/* Duplicate key already in key ring */
	CRYPT_PK_NOT_FOUND,			/* Key not found in keyring */
	CRYPT_PK_INVALID_SIZE,		/* Invalid size input for PK parameters */

	CRYPT_INVALID_PRIME_SIZE	/* Invalid size of prime requested */
};

/******************************************************************************/
/*
	hash defines
 */
struct sha1_state {
#ifdef USE_INT64
	ulong64		length;
#else
	ulong32		lengthHi;
	ulong32		lengthLo;
#endif /* USE_INT64 */
	ulong32 state[5], curlen;
	unsigned char	buf[64];
};

struct md5_state {
#ifdef USE_INT64
	ulong64 length;
#else
	ulong32	lengthHi;
	ulong32	lengthLo;
#endif /* USE_INT64 */
	ulong32 state[4], curlen;
	unsigned char buf[64];
};

#ifdef USE_MD2
struct md2_state {
	unsigned char chksum[16], X[48], buf[16];
	unsigned long curlen;
};
#endif /* USE_MD2 */



typedef union {
	struct sha1_state	sha1;
	struct md5_state	md5;
#ifdef USE_MD2
	struct md2_state	md2;
#endif /* USE_MD2 */
} hash_state;

typedef hash_state sslSha1Context_t;
typedef hash_state sslMd5Context_t;
#ifdef USE_MD2
typedef hash_state sslMd2Context_t;
#endif /* USE_MD2 */

typedef struct {
	unsigned char	pad[64];
	union {
		sslMd5Context_t		md5;
		sslSha1Context_t	sha1;
	} u;
} sslHmacContext_t;

/******************************************************************************/
/*
	RC4
 */
#ifdef USE_ARC4
typedef struct {
	unsigned char	state[256];
	uint32	byteCount;
	unsigned char	x;
	unsigned char	y;
} rc4_key;
#endif /* USE_ARC4 */


#define SSL_DES3_KEY_LEN	24
#define SSL_DES3_IV_LEN		8
#define SSL_DES_KEY_LEN		8

#ifdef USE_3DES

typedef struct {
	ulong32 ek[3][32], dk[3][32];
} des3_key;

/*
	A block cipher CBC structure
 */
typedef struct {
	int32				blocklen;
	unsigned char		IV[8];
	des3_key			key;
	int32				explicitIV; /* 1 if yes */
} des3_CBC;

extern int32 des3_setup(const unsigned char *key, int32 keylen, int32 num_rounds,
		 des3_CBC *skey);
extern void des3_ecb_encrypt(const unsigned char *pt, unsigned char *ct,
		 des3_CBC *key);
extern void des3_ecb_decrypt(const unsigned char *ct, unsigned char *pt,
		 des3_CBC *key);
extern int32 des3_keysize(int32 *desired_keysize);

extern int32 des_setup(const unsigned char *key, int32 keylen, int32 num_rounds,
		 des3_CBC *skey);
extern void des_ecb_encrypt(const unsigned char *pt, unsigned char *ct,
		 des3_CBC *key);
extern void des_ecb_decrypt(const unsigned char *ct, unsigned char *pt,
		 des3_CBC *key);

#endif /* USE_3DES */



typedef union {
#ifdef USE_ARC4
	rc4_key		arc4;
#endif
#ifdef USE_3DES
	des3_CBC	des3;
#endif
} sslCipherContext_t;


/*
	Controls endianess and size of registers.  Leave uncommented to get
	platform neutral [slower] code detect x86-32 machines somewhat
 */
#if (defined(_MSC_VER) && defined(WIN32)) || (defined(__GNUC__) && (defined(__DJGPP__) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__i386__)))
	#define ENDIAN_LITTLE
	#define ENDIAN_32BITWORD
#endif


/* #define ENDIAN_LITTLE */
/* #define ENDIAN_BIG */

/* #define ENDIAN_32BITWORD */
/* #define ENDIAN_64BITWORD */

#if (defined(ENDIAN_BIG) || defined(ENDIAN_LITTLE)) && !(defined(ENDIAN_32BITWORD) || defined(ENDIAN_64BITWORD))
		#error You must specify a word size as well as endianess
#endif

#if !(defined(ENDIAN_BIG) || defined(ENDIAN_LITTLE))
	#define ENDIAN_NEUTRAL
#endif

/*
	helper macros
 */
#if defined (ENDIAN_NEUTRAL)

#define STORE32L(x, y)                                                                     \
     { (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); }

#define LOAD32L(x, y)                            \
     { x = ((unsigned long)((y)[3] & 255)<<24) | \
           ((unsigned long)((y)[2] & 255)<<16) | \
           ((unsigned long)((y)[1] & 255)<<8)  | \
           ((unsigned long)((y)[0] & 255)); }

#define STORE64L(x, y)                                                                     \
     { (y)[7] = (unsigned char)(((x)>>56)&255); (y)[6] = (unsigned char)(((x)>>48)&255);   \
       (y)[5] = (unsigned char)(((x)>>40)&255); (y)[4] = (unsigned char)(((x)>>32)&255);   \
       (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); }

#define LOAD64L(x, y)                                                       \
     { x = (((ulong64)((y)[7] & 255))<<56)|(((ulong64)((y)[6] & 255))<<48)| \
           (((ulong64)((y)[5] & 255))<<40)|(((ulong64)((y)[4] & 255))<<32)| \
           (((ulong64)((y)[3] & 255))<<24)|(((ulong64)((y)[2] & 255))<<16)| \
           (((ulong64)((y)[1] & 255))<<8)|(((ulong64)((y)[0] & 255))); }

#define STORE32H(x, y)                                                                     \
     { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
       (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); }

#define LOAD32H(x, y)                            \
     { x = ((unsigned long)((y)[0] & 255)<<24) | \
           ((unsigned long)((y)[1] & 255)<<16) | \
           ((unsigned long)((y)[2] & 255)<<8)  | \
           ((unsigned long)((y)[3] & 255)); }

#define STORE64H(x, y)                                                                     \
   { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);     \
     (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);     \
     (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);     \
     (y)[6] = (unsigned char)(((x)>>8)&255); (y)[7] = (unsigned char)((x)&255); }

#define LOAD64H(x, y)                                                      \
   { x = (((ulong64)((y)[0] & 255))<<56)|(((ulong64)((y)[1] & 255))<<48) | \
         (((ulong64)((y)[2] & 255))<<40)|(((ulong64)((y)[3] & 255))<<32) | \
         (((ulong64)((y)[4] & 255))<<24)|(((ulong64)((y)[5] & 255))<<16) | \
         (((ulong64)((y)[6] & 255))<<8)|(((ulong64)((y)[7] & 255))); }

#endif /* ENDIAN_NEUTRAL */

#ifdef ENDIAN_LITTLE

#define STORE32H(x, y)                                                                     \
     { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
       (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); }

#define LOAD32H(x, y)                            \
     { x = ((unsigned long)((y)[0] & 255)<<24) | \
           ((unsigned long)((y)[1] & 255)<<16) | \
           ((unsigned long)((y)[2] & 255)<<8)  | \
           ((unsigned long)((y)[3] & 255)); }

#define STORE64H(x, y)                                                                     \
   { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);     \
     (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);     \
     (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);     \
     (y)[6] = (unsigned char)(((x)>>8)&255); (y)[7] = (unsigned char)((x)&255); }

#define LOAD64H(x, y)                                                      \
   { x = (((ulong64)((y)[0] & 255))<<56)|(((ulong64)((y)[1] & 255))<<48) | \
         (((ulong64)((y)[2] & 255))<<40)|(((ulong64)((y)[3] & 255))<<32) | \
         (((ulong64)((y)[4] & 255))<<24)|(((ulong64)((y)[5] & 255))<<16) | \
         (((ulong64)((y)[6] & 255))<<8)|(((ulong64)((y)[7] & 255))); }

#ifdef ENDIAN_32BITWORD 

#define STORE32L(x, y)        \
     { unsigned long __t = (x); memcpy(y, &__t, 4); }

#define LOAD32L(x, y)         \
     memcpy(&(x), y, 4);

#define STORE64L(x, y)                                                                     \
     { (y)[7] = (unsigned char)(((x)>>56)&255); (y)[6] = (unsigned char)(((x)>>48)&255);   \
       (y)[5] = (unsigned char)(((x)>>40)&255); (y)[4] = (unsigned char)(((x)>>32)&255);   \
       (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); }

#define LOAD64L(x, y)                                                       \
     { x = (((ulong64)((y)[7] & 255))<<56)|(((ulong64)((y)[6] & 255))<<48)| \
           (((ulong64)((y)[5] & 255))<<40)|(((ulong64)((y)[4] & 255))<<32)| \
           (((ulong64)((y)[3] & 255))<<24)|(((ulong64)((y)[2] & 255))<<16)| \
           (((ulong64)((y)[1] & 255))<<8)|(((ulong64)((y)[0] & 255))); }

#else /* 64-bit words then  */

#define STORE32L(x, y)        \
     { unsigned long __t = (x); memcpy(y, &__t, 4); }

#define LOAD32L(x, y)         \
     { memcpy(&(x), y, 4); x &= 0xFFFFFFFF; }

#define STORE64L(x, y)        \
     { ulong64 __t = (x); memcpy(y, &__t, 8); }

#define LOAD64L(x, y)         \
    { memcpy(&(x), y, 8); }

#endif /* ENDIAN_64BITWORD */
#endif /* ENDIAN_LITTLE */

#ifdef ENDIAN_BIG
#define STORE32L(x, y)                                                                     \
     { (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); }

#define LOAD32L(x, y)                            \
     { x = ((unsigned long)((y)[3] & 255)<<24) | \
           ((unsigned long)((y)[2] & 255)<<16) | \
           ((unsigned long)((y)[1] & 255)<<8)  | \
           ((unsigned long)((y)[0] & 255)); }

#define STORE64L(x, y)                                                                     \
   { (y)[7] = (unsigned char)(((x)>>56)&255); (y)[6] = (unsigned char)(((x)>>48)&255);     \
     (y)[5] = (unsigned char)(((x)>>40)&255); (y)[4] = (unsigned char)(((x)>>32)&255);     \
     (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);     \
     (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); }

#define LOAD64L(x, y)                                                      \
   { x = (((ulong64)((y)[7] & 255))<<56)|(((ulong64)((y)[6] & 255))<<48) | \
         (((ulong64)((y)[5] & 255))<<40)|(((ulong64)((y)[4] & 255))<<32) | \
         (((ulong64)((y)[3] & 255))<<24)|(((ulong64)((y)[2] & 255))<<16) | \
         (((ulong64)((y)[1] & 255))<<8)|(((ulong64)((y)[0] & 255))); }

#ifdef ENDIAN_32BITWORD 

#define STORE32H(x, y)        \
     { unsigned long __t = (x); memcpy(y, &__t, 4); }

#define LOAD32H(x, y)         \
     memcpy(&(x), y, 4);

#define STORE64H(x, y)                                                                     \
     { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);   \
       (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);   \
       (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);   \
       (y)[6] = (unsigned char)(((x)>>8)&255);  (y)[7] = (unsigned char)((x)&255); }

#define LOAD64H(x, y)                                                       \
     { x = (((ulong64)((y)[0] & 255))<<56)|(((ulong64)((y)[1] & 255))<<48)| \
           (((ulong64)((y)[2] & 255))<<40)|(((ulong64)((y)[3] & 255))<<32)| \
           (((ulong64)((y)[4] & 255))<<24)|(((ulong64)((y)[5] & 255))<<16)| \
           (((ulong64)((y)[6] & 255))<<8)| (((ulong64)((y)[7] & 255))); }

#else /* 64-bit words then  */

#define STORE32H(x, y)        \
     { unsigned long __t = (x); memcpy(y, &__t, 4); }

#define LOAD32H(x, y)         \
     { memcpy(&(x), y, 4); x &= 0xFFFFFFFF; }

#define STORE64H(x, y)        \
     { ulong64 __t = (x); memcpy(y, &__t, 8); }

#define LOAD64H(x, y)         \
    { memcpy(&(x), y, 8); }

#endif /* ENDIAN_64BITWORD */
#endif /* ENDIAN_BIG */

/*
	packet code */
#if defined(USE_RSA) || defined(MDH) || defined(MECC)
	#define PACKET

/*
	size of a packet header in bytes */
	#define PACKET_SIZE				4

/*
	Section tags
 */
	#define PACKET_SECT_RSA			0
	#define PACKET_SECT_DH			1
	#define PACKET_SECT_ECC			2
	#define PACKET_SECT_DSA			3

/*
	Subsection Tags for the first three sections
 */
	#define PACKET_SUB_KEY			0
	#define PACKET_SUB_ENCRYPTED	1
	#define PACKET_SUB_SIGNED		2
	#define PACKET_SUB_ENC_KEY		3
#endif

/*
	fix for MSVC ...evil!
 */
#ifdef WIN32
#ifdef _MSC_VER
	#define CONST64(n) n ## ui64
	typedef unsigned __int64 ulong64;
#else
	#define CONST64(n) n ## ULL
	typedef unsigned long long ulong64;
#endif
#endif /* WIN32 */


#define BSWAP(x)  ( ((x>>24)&0x000000FFUL) | ((x<<24)&0xFF000000UL)  | \
                    ((x>>8)&0x0000FF00UL)  | ((x<<8)&0x00FF0000UL) )

#ifdef _MSC_VER

/*
	instrinsic rotate
 */
#include <stdlib.h>
#pragma intrinsic(_lrotr,_lrotl)
#define ROR(x,n) _lrotr(x,n)
#define ROL(x,n) _lrotl(x,n)
#define RORc(x,n) _lrotr(x,n)
#define ROLc(x,n) _lrotl(x,n)

/*
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && !defined(INTEL_CC) && !defined(PS_NO_ASM)

static inline unsigned ROL(unsigned word, int32 i)
{
	asm ("roll %%cl,%0"
		:"0" (word),"c" (i));
	return word;
}

static inline unsigned ROR(unsigned word, int32 i)
{
	asm ("rorl %%cl,%0"
		:"=r" (word)
		:"0" (word),"c" (i));
	return word;
}
*/
/*
#ifndef PS_NO_ROLC

static inline unsigned ROLc(unsigned word, const int32 i)
{
   asm ("roll %2,%0"
      :"=r" (word)
      :"0" (word),"I" (i));
   return word;
}

static inline unsigned RORc(unsigned word, const int32 i)
{
   asm ("rorl %2,%0"
      :"=r" (word)
      :"0" (word),"I" (i));
   return word;
}

#else

#define ROLc ROL
#define RORc ROR

#endif
*/

#else /* _MSC_VER */

/*
	rotates the hard way
 */
#define ROL(x, y) ( (((unsigned long)(x)<<(unsigned long)((y)&31)) | (((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)
#define ROR(x, y) ( ((((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)((y)&31)) | ((unsigned long)(x)<<(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)
#define ROLc(x, y) ( (((unsigned long)(x)<<(unsigned long)((y)&31)) | (((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)
#define RORc(x, y) ( ((((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)((y)&31)) | ((unsigned long)(x)<<(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)

#endif /* _MSC_VER */

/* 64-bit Rotates */
#if 0

#if defined(__GNUC__) && defined(__x86_64__) && !defined(PS_NO_ASM)

static inline unsigned long ROL64(unsigned long word, int32 i)
{
   asm("rolq %%cl,%0"
      :"=r" (word)
      :"0" (word),"c" (i));
   return word;
}

static inline unsigned long ROR64(unsigned long word, int32 i)
{
   asm("rorq %%cl,%0"
      :"=r" (word)
      :"0" (word),"c" (i));
   return word;
}

#ifndef PS_NO_ROLC

static inline unsigned long ROL64c(unsigned long word, const int32 i)
{
   asm("rolq %2,%0"
      :"=r" (word)
      :"0" (word),"J" (i));
   return word;
}

static inline unsigned long ROR64c(unsigned long word, const int32 i)
{
   asm("rorq %2,%0"
      :"=r" (word)
      :"0" (word),"J" (i));
   return word;
}

#else /* PS_NO_ROLC */

#define ROL64c ROL
#define ROR64c ROR

#endif /* PS_NO_ROLC */
#endif
#endif /* commented out */

#define ROL64(x, y) \
    ( (((x)<<((ulong64)(y)&63)) | \
      (((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((ulong64)64-((y)&63)))) & CONST64(0xFFFFFFFFFFFFFFFF))

#define ROR64(x, y) \
    ( ((((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((ulong64)(y)&CONST64(63))) | \
      ((x)<<((ulong64)(64-((y)&CONST64(63)))))) & CONST64(0xFFFFFFFFFFFFFFFF))

#define ROL64c(x, y) \
    ( (((x)<<((ulong64)(y)&63)) | \
      (((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((ulong64)64-((y)&63)))) & CONST64(0xFFFFFFFFFFFFFFFF))

#define ROR64c(x, y) \
    ( ((((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((ulong64)(y)&CONST64(63))) | \
      ((x)<<((ulong64)(64-((y)&CONST64(63)))))) & CONST64(0xFFFFFFFFFFFFFFFF))

#undef MAX
#undef MIN
#define MAX(x, y) ( ((x)>(y))?(x):(y) )
#define MIN(x, y) ( ((x)<(y))?(x):(y) )

/*
	extract a byte portably This MSC code causes runtime errors in VS.NET,
	always use the other
 */
/*
#ifdef _MSC_VER
   #define byte(x, n) ((unsigned char)((x) >> (8 * (n))))
#else
*/
   #define byte(x, n) (((x) >> (8 * (n))) & 255)
/*
#endif
*/
#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* _h_PSCRYPTO */

/******************************************************************************/

