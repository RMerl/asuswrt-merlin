/* desdata.c
 *
 * Generate tables used by des.c and desCode.h.
 *
 */

/*
 *	des - fast & portable DES encryption & decryption.
 *	Copyright (C) 1992  Dana L. How
 *	Please see the file `descore.README' for the complete copyright notice.
 *
 */

#include <stdio.h>

#include	"desinfo.h"


/* list of weak and semi-weak keys

	 +0   +1   +2   +3   +4   +5   +6   +7
	0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01
	0x01 0x1f 0x01 0x1f 0x01 0x0e 0x01 0x0e
	0x01 0xe0 0x01 0xe0 0x01 0xf1 0x01 0xf1
	0x01 0xfe 0x01 0xfe 0x01 0xfe 0x01 0xfe
	0x1f 0x01 0x1f 0x01 0x0e 0x01 0x0e 0x01
	0x1f 0x1f 0x1f 0x1f 0x0e 0x0e 0x0e 0x0e
	0x1f 0xe0 0x1f 0xe0 0x0e 0xf1 0x0e 0xf1
	0x1f 0xfe 0x1f 0xfe 0x0e 0xfe 0x0e 0xfe
	0xe0 0x01 0xe0 0x01 0xf1 0x01 0xf1 0x01
	0xe0 0x1f 0xe0 0x1f 0xf1 0x0e 0xf1 0x0e
	0xe0 0xe0 0xe0 0xe0 0xf1 0xf1 0xf1 0xf1
	0xe0 0xfe 0xe0 0xfe 0xf1 0xfe 0xf1 0xfe
	0xfe 0x01 0xfe 0x01 0xfe 0x01 0xfe 0x01
	0xfe 0x1f 0xfe 0x1f 0xfe 0x0e 0xfe 0x0e
	0xfe 0xe0 0xfe 0xe0 0xfe 0xf1 0xfe 0xf1
	0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe
 */

/* key bit order in each method pair: bits 31->00 of 1st, bits 31->00 of 2nd */
/* this does not reflect the rotate of the 2nd word */

#define	S(box,bit)	(box*6+bit)
int korder[] = {
	S(7, 5), S(7, 4), S(7, 3), S(7, 2), S(7, 1), S(7, 0),
	S(5, 5), S(5, 4), S(5, 3), S(5, 2), S(5, 1), S(5, 0),
	S(3, 5), S(3, 4), S(3, 3), S(3, 2), S(3, 1), S(3, 0),
	S(1, 5), S(1, 4), S(1, 3), S(1, 2), S(1, 1), S(1, 0),
	S(6, 5), S(6, 4), S(6, 3), S(6, 2), S(6, 1), S(6, 0),
	S(4, 5), S(4, 4), S(4, 3), S(4, 2), S(4, 1), S(4, 0),
	S(2, 5), S(2, 4), S(2, 3), S(2, 2), S(2, 1), S(2, 0),
	S(0, 5), S(0, 4), S(0, 3), S(0, 2), S(0, 1), S(0, 0),
};

/* the order in which the algorithm accesses the s boxes */

int sorder[] = {
	7, 5, 3, 1, 6, 4, 2, 0,
};

int
main(int argc, char **argv)
{
	unsigned long d, i, j, k, l, m, n, s; /* Always at least 32 bits */
	char b[256], ksr[56];

	if (argc <= 1)
		return 1;

	switch ( argv[1][0] ) {

default: 
	return 1;
	/*
	 * <<< make the key parity table >>>
	 */

case 'p':
	printf(
"/* automagically produced - do not fuss with this information */\n\n");

	/* store parity information */
	for ( i = 0; i < 256; i++ ) {
		j  = i;
		j ^= j >> 4;	/* bits 3-0 have pairs */
		j ^= j << 2;	/* bits 3-2 have quads */
		j ^= j << 1;	/* bit  3 has the entire eight (no cox) */
		b[i] = 8 & ~j;	/* 0 is okay and 8 is bad parity */
	}

	/* only these characters can appear in a weak key */
	b[0x01] = 1;
	b[0x0e] = 2;
	b[0x1f] = 3;
	b[0xe0] = 4;
	b[0xf1] = 5;
	b[0xfe] = 6;

	/* print it out */
	for ( i = 0; i < 256; i++ ) {
		printf("%d,", b[i]);
		if ( (i & 31) == 31 )
			printf("\n");
	}

	break;


	/*
	 * <<< make the key usage table >>>
	 */

case 'r':
	printf("/* automagically made - do not fuss with this */\n\n");

	/* KL specifies the initial key bit positions */
	for (i = 0; i < 56; i++)
		ksr[i] = (KL[i] - 1) ^ 7;

	for (i = 0; i < 16; i++) {

		/* apply the appropriate number of left shifts */
		for (j = 0; j < KS[i]; j++) {
			m = ksr[ 0];
			n = ksr[28];
			for (k = 0; k < 27; k++)
				ksr[k     ] = ksr[k +  1],
				ksr[k + 28] = ksr[k + 29];
			ksr[27] = m;
			ksr[55] = n;
		}

		/* output the key bit numbers */
		for (j = 0; j < 48; j++) {
			m = ksr[KC[korder[j]] - 1];
			m = (m / 8) * 7 + (m % 8) - 1;
			m = 55 - m;
			printf(" %2ld,", (long) m);
			if ((j % 12) == 11)
				printf("\n");
		}
		printf("\n");
	}

	break;


	/*
	 * <<< make the keymap table >>>
	 */

case 'k':
	printf("/* automagically made - do not fuss with this */\n\n");

	for ( i = 0; i <= 7 ; i++ ) {
		s = sorder[i];
		for ( d = 0; d <= 63; d++ ) {
			/* flip bits */
			k =	((d << 5) & 32) |
				((d << 3) & 16) |
				((d << 1) &  8) |
				((d >> 1) &  4) |
				((d >> 3) &  2) |
				((d >> 5) &  1) ;
			/* more bit twiddling */
			l =	((k << 0) & 32) |	/* overlap bit */
				((k << 4) & 16) |	/* overlap bit */
				((k >> 1) & 15) ;	/* unique bits */
			/* look up s box value */
			m = SB[s][l];
			/* flip bits */
			n =	((m << 3) &  8) |
				((m << 1) &  4) |
				((m >> 1) &  2) |
				((m >> 3) &  1) ;
			/* put in correct nybble */
			n <<= (s << 2);
			/* perform p permutation */
			for ( m = j = 0; j < 32; j++ )
				if ( n & (1 << (SP[j] - 1)) )
					m |= (1UL << j);
			/* rotate right (alg keeps everything rotated by 1) */
			m = (m >> 1) | ((m & 1) << 31);
			/* print it out */
			printf(" 0x%08lx,", m);
			if ( ( d & 3 ) == 3 )
				printf("\n");
		}
		printf("\n");
	}

	break;

	}

	return 0;
}
