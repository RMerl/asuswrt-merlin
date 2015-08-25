/*
 * Copyright 1996 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/*$Header*/

/* Generate code to pack a bit array from a name:#bits description */

#include	<stdio.h>
#include	"taste.h"
#include	"proto.h"
#include	<limits.h>

/* This module is the opposite of sour.   Sweet was already taken,
 * that's why it's called ginger.  (Add one point if that reminds
 * you of Gary Larson.)
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

		while (n_in < sp->varsize) {
			if (n_in) printf("sr |= (uword)*c++ << %d;\n", n_in);
			else printf("sr = *c++;\n");
			n_in += BYTE_BITS;
		}

		printf("%s = sr & %#x;  sr >>= %d;\n",
			sp->var, ~(~0U << sp->varsize), sp->varsize);

		n_in -= sp->varsize;
	}

	if (n_in > 0) {
		fprintf(stderr, "%d bits left over\n", n_in);
	}
}
