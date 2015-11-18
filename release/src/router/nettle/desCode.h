/* desCode.h
 *
 */

/*	des - fast & portable DES encryption & decryption.
 *	Copyright (C) 1992  Dana L. How
 *	Please see the file `descore.README' for the complete copyright notice.
 */

#include "des.h"

/* optional customization:
 * the idea here is to alter the code so it will still run correctly
 * on any machine,  but the quickest on the specific machine in mind.
 * note that these silly tweaks can give you a 15%-20% speed improvement
 * on the sparc -- it's probably even more significant on the 68000. */

/* take care of machines with incredibly few registers */
#if	defined(i386)
#define	REGISTER		/* only x, y, z will be declared register */
#else
#define	REGISTER	register
#endif	/* i386 */

/* is auto inc/dec faster than 7bit unsigned indexing? */
#if	defined(vax) || defined(mc68000)
#define	FIXR		r += 32;
#define	FIXS		s +=  8;
#define	PREV(v,o)	*--v
#define	NEXT(v,o)	*v++
#else
#define	FIXR
#define	FIXS
#define	PREV(v,o)	v[o]
#define	NEXT(v,o)	v[o]
#endif

/* if no machine type, default is indexing, 6 registers and cheap literals */
#if	!defined(i386) && !defined(vax) && !defined(mc68000) && !defined(sparc)
#define	vax
#endif


/* handle a compiler which can't reallocate registers */
/* The BYTE type is used as parameter for the encrypt/decrypt functions.
 * It's pretty bad to have the function prototypes depend on
 * a macro definition that the users of the function doesn't
 * know about. /Niels */
#if	0			/* didn't feel like deleting */
#define	SREGFREE	; s = (uint8_t *) D
#define	DEST		s
#define	D		m0
#define	BYTE		uint32_t
#else
#define	SREGFREE
#define	DEST		d
#define	D		d
#define	BYTE		uint8_t
#endif

/* handle constants in the optimal way for 386 & vax */
/* 386: we declare 3 register variables (see above) and use 3 more variables;
 * vax: we use 6 variables, all declared register;
 * we assume address literals are cheap & unrestricted;
 * we assume immediate constants are cheap & unrestricted. */
#if	defined(i386) || defined(vax)
#define	MQ0	 des_bigmap
#define	MQ1	(des_bigmap +  64)
#define	MQ2	(des_bigmap + 128)
#define	MQ3	(des_bigmap + 192)
#define	HQ0(z)				/*	z |= 0x01000000L; */
#define	HQ2(z)				/*	z |= 0x03000200L; */
#define	LQ0(z)	0xFCFC & z
#define	LQ1(z)	0xFCFC & z
#define	LQ2(z)	0xFCFC & z
#define	LQ3(z)	0xFCFC & z
#define	SQ	16
#define	MS0	 des_keymap 
#define	MS1	(des_keymap +  64)
#define	MS2	(des_keymap + 128)
#define	MS3	(des_keymap + 192)
#define	MS4	(des_keymap + 256)
#define	MS5	(des_keymap + 320)
#define	MS6	(des_keymap + 384)
#define	MS7	(des_keymap + 448)
#define	HS(z)
#define	LS0(z)	0xFC & z
#define	LS1(z)	0xFC & z
#define	LS2(z)	0xFC & z
#define	LS3(z)	0xFC & z
#define	REGQUICK
#define	SETQUICK
#define	REGSMALL
#define	SETSMALL
#endif	/* defined(i386) || defined(vax) */

/* handle constants in the optimal way for mc68000 */
/* in addition to the core 6 variables, we declare 3 registers holding constants
 * and 4 registers holding address literals.
 * at most 6 data values and 5 address values are actively used at once.
 * we assume address literals are so expensive we never use them;
 * we assume constant index offsets > 127 are expensive, so they are not used.
 * we assume all constants are expensive and put them in registers,
 * including shift counts greater than 8. */
#if	defined(mc68000)
#define	MQ0	m0
#define	MQ1	m1
#define	MQ2	m2
#define	MQ3	m3
#define	HQ0(z)
#define	HQ2(z)
#define	LQ0(z)	k0 & z
#define	LQ1(z)	k0 & z
#define	LQ2(z)	k0 & z
#define	LQ3(z)	k0 & z
#define	SQ	k1
#define	MS0	m0
#define	MS1	m0
#define	MS2	m1
#define	MS3	m1
#define	MS4	m2
#define	MS5	m2
#define	MS6	m3
#define	MS7	m3
#define	HS(z)	z |= k0;
#define	LS0(z)	k1 & z
#define	LS1(z)	k2 & z
#define	LS2(z)	k1 & z
#define	LS3(z)	k2 & z
#define	REGQUICK				\
	register uint32_t k0, k1;		\
	register uint32_t *m0, *m1, *m2, *m3;
#define	SETQUICK				\
	; k0 = 0xFCFC				\
	; k1 = 16				\
	/*k2 = 28 to speed up ROL */		\
	; m0 = des_bigmap			\
	; m1 = m0 + 64				\
	; m2 = m1 + 64				\
	; m3 = m2 + 64
#define	REGSMALL				\
	register uint32_t k0, k1, k2;		\
	register uint32_t *m0, *m1, *m2, *m3;
#define	SETSMALL				\
	; k0 = 0x01000100L			\
	; k1 = 0x0FC				\
	; k2 = 0x1FC				\
	; m0 = des_keymap			\
	; m1 = m0 + 128				\
	; m2 = m1 + 128				\
	; m3 = m2 + 128
#endif	/* defined(mc68000) */

/* handle constants in the optimal way for sparc */
/* in addition to the core 6 variables, we either declare:
 * 4 registers holding address literals and 1 register holding a constant, or
 * 8 registers holding address literals.
 * up to 14 register variables are declared (sparc has %i0-%i5, %l0-%l7).
 * we assume address literals are so expensive we never use them;
 * we assume any constant with >10 bits is expensive and put it in a register,
 * and any other is cheap and is coded in-line. */
#if	defined(sparc)
#define	MQ0	m0
#define	MQ1	m1
#define	MQ2	m2
#define	MQ3	m3
#define	HQ0(z)
#define	HQ2(z)
#define	LQ0(z)	k0 & z
#define	LQ1(z)	k0 & z
#define	LQ2(z)	k0 & z
#define	LQ3(z)	k0 & z
#define	SQ	16
#define	MS0	m0
#define	MS1	m1
#define	MS2	m2
#define	MS3	m3
#define	MS4	m4
#define	MS5	m5
#define	MS6	m6
#define	MS7	m7
#define	HS(z)
#define	LS0(z)	0xFC & z
#define	LS1(z)	0xFC & z
#define	LS2(z)	0xFC & z
#define	LS3(z)	0xFC & z
#define	REGQUICK				\
	register uint32_t k0;			\
	register uint32_t *m0, *m1, *m2, *m3;
#define	SETQUICK				\
	; k0 = 0xFCFC				\
	; m0 = des_bigmap			\
	; m1 = m0 + 64				\
	; m2 = m1 + 64				\
	; m3 = m2 + 64
#define	REGSMALL				\
	register uint32_t *m0, *m1, *m2, *m3, *m4, *m5, *m6, *m7;
#define	SETSMALL				\
	; m0 = des_keymap			\
	; m1 = m0 + 64				\
	; m2 = m1 + 64				\
	; m3 = m2 + 64				\
	; m4 = m3 + 64				\
	; m5 = m4 + 64				\
	; m6 = m5 + 64				\
	; m7 = m6 + 64
#endif	/* defined(sparc) */


/* some basic stuff */

/* generate addresses from a base and an index */
/* FIXME: This is used only as *ADD(msi,lsi(z)) or *ADD(mqi,lqi(z)).
 * Why not use plain indexing instead? /Niels */
#define	ADD(b,x)	(uint32_t *) ((uint8_t *)b + (x))

/* low level rotate operations */
#define	NOP(d,c,o)
#define	ROL(d,c,o)	d = d << c | d >> o
#define	ROR(d,c,o)	d = d >> c | d << o
#define	ROL1(d)		ROL(d, 1, 31)
#define	ROR1(d)		ROR(d, 1, 31)

/* elementary swap for doing IP/FP */
#define	SWAP(x,y,m,b)				\
	z  = ((x >> b) ^ y) & m;		\
	x ^= z << b;				\
	y ^= z


/* the following macros contain all the important code fragments */

/* load input data, then setup special registers holding constants */
#define	TEMPQUICK(LOAD)				\
	REGQUICK				\
	LOAD()					\
	SETQUICK
#define	TEMPSMALL(LOAD)				\
	REGSMALL				\
	LOAD()					\
	SETSMALL

/* load data */
#define	LOADDATA(x,y)				\
	FIXS					\
	y  = PREV(s, 7); y<<= 8;		\
	y |= PREV(s, 6); y<<= 8;		\
	y |= PREV(s, 5); y<<= 8;		\
	y |= PREV(s, 4);			\
	x  = PREV(s, 3); x<<= 8;		\
	x |= PREV(s, 2); x<<= 8;		\
	x |= PREV(s, 1); x<<= 8;		\
	x |= PREV(s, 0)				\
	SREGFREE
/* load data without initial permutation and put into efficient position */
#define	LOADCORE()				\
	LOADDATA(x, y);				\
	ROR1(x);				\
	ROR1(y)
/* load data, do the initial permutation and put into efficient position */
#define	LOADFIPS()				\
	LOADDATA(y, x);				\
	SWAP(x, y, 0x0F0F0F0FL, 004);		\
	SWAP(y, x, 0x0000FFFFL, 020);		\
	SWAP(x, y, 0x33333333L, 002);		\
	SWAP(y, x, 0x00FF00FFL, 010);		\
	ROR1(x);				\
	z  = (x ^ y) & 0x55555555L;		\
	y ^= z;					\
	x ^= z;					\
	ROR1(y)


/* core encryption/decryption operations */
/* S box mapping and P perm */
#define	KEYMAPSMALL(x,z,mq0,mq1,hq,lq0,lq1,sq,ms0,ms1,ms2,ms3,hs,ls0,ls1,ls2,ls3)\
	hs(z)					\
	x ^= *ADD(ms3, ls3(z));			\
	z>>= 8;					\
	x ^= *ADD(ms2, ls2(z));			\
	z>>= 8;					\
	x ^= *ADD(ms1, ls1(z));			\
	z>>= 8;					\
	x ^= *ADD(ms0, ls0(z))
/* alternate version: use 64k of tables */
#define	KEYMAPQUICK(x,z,mq0,mq1,hq,lq0,lq1,sq,ms0,ms1,ms2,ms3,hs,ls0,ls1,ls2,ls3)\
	hq(z)					\
	x ^= *ADD(mq0, lq0(z));			\
	z>>= sq;				\
	x ^= *ADD(mq1, lq1(z))
/* apply 24 key bits and do the odd  s boxes */
#define	S7S1(x,y,z,r,m,KEYMAP,LOAD)		\
	z  = LOAD(r, m);			\
	z ^= y;					\
	KEYMAP(x,z,MQ0,MQ1,HQ0,LQ0,LQ1,SQ,MS0,MS1,MS2,MS3,HS,LS0,LS1,LS2,LS3)
/* apply 24 key bits and do the even s boxes */
#define	S6S0(x,y,z,r,m,KEYMAP,LOAD)		\
	z  = LOAD(r, m);			\
	z ^= y;					\
	ROL(z, 4, 28);				\
	KEYMAP(x,z,MQ2,MQ3,HQ2,LQ2,LQ3,SQ,MS4,MS5,MS6,MS7,HS,LS0,LS1,LS2,LS3)
/* actual iterations.  equivalent except for UPDATE & swapping m and n */
#define	ENCR(x,y,z,r,m,n,KEYMAP)		\
	S7S1(x,y,z,r,m,KEYMAP,NEXT);		\
	S6S0(x,y,z,r,n,KEYMAP,NEXT)
#define	DECR(x,y,z,r,m,n,KEYMAP)		\
	S6S0(x,y,z,r,m,KEYMAP,PREV);		\
	S7S1(x,y,z,r,n,KEYMAP,PREV)

/* write out result in correct byte order */
#define	SAVEDATA(x,y)				\
	NEXT(DEST, 0) = x; x>>= 8;		\
	NEXT(DEST, 1) = x; x>>= 8;		\
	NEXT(DEST, 2) = x; x>>= 8;		\
	NEXT(DEST, 3) = x;			\
	NEXT(DEST, 4) = y; y>>= 8;		\
	NEXT(DEST, 5) = y; y>>= 8;		\
	NEXT(DEST, 6) = y; y>>= 8;		\
	NEXT(DEST, 7) = y
/* write out result */
#define	SAVECORE()				\
	ROL1(x);				\
	ROL1(y);				\
	SAVEDATA(y, x)
/* do final permutation and write out result */
#define	SAVEFIPS()				\
	ROL1(x);				\
	z  = (x ^ y) & 0x55555555L;		\
	y ^= z;					\
	x ^= z;					\
	ROL1(y);				\
	SWAP(x, y, 0x00FF00FFL, 010);		\
	SWAP(y, x, 0x33333333L, 002);		\
	SWAP(x, y, 0x0000FFFFL, 020);		\
	SWAP(y, x, 0x0F0F0F0FL, 004);		\
	SAVEDATA(x, y)


/* the following macros contain the encryption/decryption skeletons */

#define	ENCRYPT(NAME, TEMP, LOAD, KEYMAP, SAVE)	\
						\
void						\
NAME(REGISTER BYTE *D,				\
     REGISTER const uint32_t *r,		\
     REGISTER const uint8_t *s)			\
{						\
	register uint32_t x, y, z;		\
						\
	/* declare temps & load data */		\
	TEMP(LOAD);				\
						\
	/* do the 16 iterations */		\
	ENCR(x,y,z,r, 0, 1,KEYMAP);		\
	ENCR(y,x,z,r, 2, 3,KEYMAP);		\
	ENCR(x,y,z,r, 4, 5,KEYMAP);		\
	ENCR(y,x,z,r, 6, 7,KEYMAP);		\
	ENCR(x,y,z,r, 8, 9,KEYMAP);		\
	ENCR(y,x,z,r,10,11,KEYMAP);		\
	ENCR(x,y,z,r,12,13,KEYMAP);		\
	ENCR(y,x,z,r,14,15,KEYMAP);		\
	ENCR(x,y,z,r,16,17,KEYMAP);		\
	ENCR(y,x,z,r,18,19,KEYMAP);		\
	ENCR(x,y,z,r,20,21,KEYMAP);		\
	ENCR(y,x,z,r,22,23,KEYMAP);		\
	ENCR(x,y,z,r,24,25,KEYMAP);		\
	ENCR(y,x,z,r,26,27,KEYMAP);		\
	ENCR(x,y,z,r,28,29,KEYMAP);		\
	ENCR(y,x,z,r,30,31,KEYMAP);		\
						\
	/* save result */			\
	SAVE();					\
						\
	return;					\
}

#define	DECRYPT(NAME, TEMP, LOAD, KEYMAP, SAVE)	\
						\
void						\
NAME(REGISTER BYTE *D,				\
     REGISTER const uint32_t *r,		\
     REGISTER const uint8_t *s)			\
{						\
	register uint32_t x, y, z;		\
						\
	/* declare temps & load data */		\
	TEMP(LOAD);				\
						\
	/* do the 16 iterations */		\
	FIXR					\
	DECR(x,y,z,r,31,30,KEYMAP);		\
	DECR(y,x,z,r,29,28,KEYMAP);		\
	DECR(x,y,z,r,27,26,KEYMAP);		\
	DECR(y,x,z,r,25,24,KEYMAP);		\
	DECR(x,y,z,r,23,22,KEYMAP);		\
	DECR(y,x,z,r,21,20,KEYMAP);		\
	DECR(x,y,z,r,19,18,KEYMAP);		\
	DECR(y,x,z,r,17,16,KEYMAP);		\
	DECR(x,y,z,r,15,14,KEYMAP);		\
	DECR(y,x,z,r,13,12,KEYMAP);		\
	DECR(x,y,z,r,11,10,KEYMAP);		\
	DECR(y,x,z,r, 9, 8,KEYMAP);		\
	DECR(x,y,z,r, 7, 6,KEYMAP);		\
	DECR(y,x,z,r, 5, 4,KEYMAP);		\
	DECR(x,y,z,r, 3, 2,KEYMAP);		\
	DECR(y,x,z,r, 1, 0,KEYMAP);		\
						\
	/* save result */			\
	SAVE();					\
						\
	return;					\
}
