/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/*$Header: /tmp_amd/presto/export/kbs/jutta/src/gsm/RCS/bitter.c,v 1.1 1992/10/28 00:28:39 jutta Exp $*/

/* Generate code to pack a bit array from a name:#bits description */

#include	<stdio.h>
#include	"taste.h"
#include	"proto.h"

void write_code P2((s_spex, n_spex), struct spex * s_spex, int n_spex)
{
	struct spex	* sp = s_spex;
	int		bits = 8;
	int		vars;

	if (!n_spex) return;

	vars = sp->varsize;

	while (n_spex) {

		if (bits == 8) printf("\t*c++ =   ");
		else printf("\t       | ");

		if (vars == bits) {
	
			printf( (bits==8? "%s & 0x%lX;\n" : "(%s & 0x%lX);\n"),
				sp->var, 
				~(0xfffffffe << (bits - 1)));
			if (!-- n_spex) break;
			sp++;

			vars = sp->varsize;
			bits = 8;

		} else if (vars < bits) {

			printf( "((%s & 0x%lX) << %d)",
				sp->var,
				~(0xfffffffe << (vars - 1)),
				bits - vars);
			bits -= vars;
			if (!--n_spex) {
				puts(";");
				break;
			}
			else putchar('\n');
			sp++;
			vars = sp->varsize;

		} else {
			printf("((%s >> %d) & 0x%X);\n",
				sp->var, 
				vars - bits,
				~(0xfffffffe << (bits - 1)));
			
			vars -= bits;
			bits = 8;
		}
	}
}
