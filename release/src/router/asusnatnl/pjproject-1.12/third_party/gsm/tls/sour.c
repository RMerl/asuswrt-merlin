/*
 * Copyright 1996 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/*$Header*/

/* Generate code to pack a bit array from a name:#bits description,
 * WAV #49 style.
 */

#include	<stdio.h>
#include	"taste.h"
#include	"proto.h"
#include	<limits.h>

/* This module goes back to one Jeff Chilton used for his implementation
 * of the #49 WAV GSM format.  (In his original patch 8, it replaced
 * bitter.c.)
 *
 * In Microsoft's WAV #49 version of the GSM format, two 32 1/2
 * byte GSM frames are packed together to make one WAV frame, and
 * the GSM parameters are packed into bytes right-to-left rather
 * than left-to-right.
 *
 * That is, where toast's GSM format writes
 *
 * 	aaaaaabb bbbbcccc cdddddee ...
 *	___1____ ___2____ ___3____
 *
 *  for parameters a (6 bits), b (6 bits), c (5 bits), d (5 bits), e ..
 *  the WAV format has
 *
 * 	bbaaaaaa ccccbbbb eedddddc ...
 *	___1____ ___2____ ___3____
 *
 *  (This format looks a lot prettier if one pictures octets coming
 *  in through a fifo queue from the left, rather than waiting in the
 *  right-hand remainder of a C array.)
 */

#define WORD_BITS	16	/* sizeof(uword) * CHAR_BIT on the 
				 * target architecture---if this isn't 16,
				 * you're in trouble with this library anyway.
				 */

#define BYTE_BITS	 8	/* CHAR_BIT on the target architecture---
				 * if this isn't 8, you're in *deep* trouble.
				 */

void write_code P2((s_spex, n_spex), struct spex * s_spex, int n_spex)
{
	struct spex	* sp = s_spex;
	int		  n_in = 0;

	printf("uword sr = 0;\n");

	for (; n_spex > 0; n_spex--, sp++) {

		/*	insert       old 
		 *	new var	     value     unused
		 *	here  
		 *
		 *	[____________xxxxxx**********]
		 *
		 *	<----- n_in ------>
		 */
		printf("sr = sr >> %d | %s << %d;\n",
			sp->varsize,
			sp->var, 
			WORD_BITS - sp->varsize);

		n_in += sp->varsize;

		while (n_in >= BYTE_BITS) {
			printf("*c++ = sr >> %d;\n",
				WORD_BITS - n_in);
			n_in -= BYTE_BITS;
		}
	}

	while (n_in >= BYTE_BITS) {
		printf("*c++ = sr >> %d;\n", WORD_BITS - n_in);
		n_in -= BYTE_BITS;
	}

	if (n_in > 0) {
		fprintf(stderr, "warning: %d bits left over\n", n_in);
	}
}
