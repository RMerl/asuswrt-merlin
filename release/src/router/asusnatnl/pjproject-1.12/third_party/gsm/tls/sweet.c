 /*
  * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
  * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
  * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
  */

/*$Header: /tmp_amd/presto/export/kbs/jutta/src/gsm/RCS/sweet.c,v 1.2 1996/07/02 10:15:53 jutta Exp $*/
 
/* Generate code to unpack a bit array from name:#bits description */

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

		if (vars == sp->varsize) {
			printf("\t%s  = ", sp->var);
		} else printf("\t%s |= ", sp->var);

		if (vars == bits) {
	
			if (bits == 8) printf( "*c++;\n" );
			else printf( "*c++ & 0x%lX;\n",
				~(0xfffffffe << (bits - 1)) );

			if (!-- n_spex) break;
			sp++;
			vars = sp->varsize;
			bits = 8;

		} else if (vars < bits) {

			printf( "(*c >> %d) & 0x%lX;\n", 
				bits - vars,
				~(0xfffffffe << (vars - 1)));

			bits -= vars;
			if (!--n_spex) break;
			sp++;
			vars = sp->varsize;

		} else {
			/*   vars > bits.  We're eating lower-all of c,
			 *   but we must shift it.
			 */
			printf(	"(*c++ & 0x%X) << %d;\n",
				~(0xfffffffe << (bits - 1)),
				vars - bits );

			vars -= bits;
			bits = 8;
		}
	}
}

