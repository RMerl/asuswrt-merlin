/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/*$Header: /tmp_amd/presto/export/kbs/jutta/src/gsm/RCS/taste.c,v 1.1 1992/10/28 00:28:39 jutta Exp $*/

#include	<stdio.h>
#include	<string.h>
#include	<memory.h>

#include	"config.h"

#ifdef	HAS_STDLIB_H
#	include	<stdlib.h>
#else
#include "proto.h"
#	ifdef	HAS_MALLOC_H
#	include <malloc.h>
#	else
		extern char	* malloc P((char *)), * realloc P((char *,int));
#	endif
	extern int exit P((int));
#endif

#include "proto.h"

/*
 * common code to sweet.c and bitter.c: read the name:#bits description.
 */

#include	"taste.h"

static struct spex  * s_spex;
static int n_spex, m_spex;

extern void	write_code P((struct spex *, int));

char * strsave P1((str), char * str)		/* strdup() + errors */
{
	int    n = strlen(str) + 1;
	char * s = malloc(n);
	if (!s) {
		fprintf(stderr, "Failed to malloc %d bytes, abort\n",
			strlen(str) + 1);
		exit(1);
	}
	return memcpy(s, str, n);
}

struct spex * new_spex P0()
{
	if (n_spex >= m_spex) {
		m_spex += 500;
		if (!(s_spex = (struct spex *)(n_spex
			? realloc((char *)s_spex, m_spex * sizeof(*s_spex))
			: malloc( m_spex * sizeof(*s_spex))))) {
			fprintf(stderr, "Failed to malloc %d bytes, abort\n",
				m_spex * sizeof(*s_spex));
			exit(1);
		}
	}
	return s_spex + n_spex;
}

char * strtek P2((str, sep), char * str, char * sep) {

	static char     * S = (char *)0;
	char		* c, * base;

	if (str) S = str;

	if (!S || !*S) return (char *)0;

	/*  Skip delimiters.
	 */
	while (*S) {
		for (c = sep; *c && *c != *S; c++) ;
		if (*c) *S++ = 0;
		else break;
	}

	base = S;

	/*   Skip non-delimiters.
	 */
	for (base = S; *S; S++) {

		for (c = sep; *c; c++)
			if (*c == *S) {
				*S++ = 0;
				return base;
			}
	}

	return base == S ? (char *)0 : base;
}

int read_spex P0()
{
	char buf[200];
	char * s, *t;
	struct spex	* sp = s_spex;	

	while (fgets(buf, sizeof buf, stdin)) {

		char 	* nl;

		if (nl = strchr(buf, '\n'))
			*nl = '\0';

		if (!*buf || *buf == ';') continue;
		s = strtek(buf, " \t");
		if (!s) {
			fprintf(stderr, "? %s\n", buf);
			continue;
		}
		sp = new_spex();
		sp->var = strsave(s);
		s = strtek((char*)0, " \t");
		if (!s) {
			fprintf(stderr, "varsize?\n");
			continue;
		}
		sp->varsize = strtol(s, (char **)0, 0);
		n_spex++;
	}

	return sp - s_spex;
}

int main P0()
{
	read_spex();
	write_code(s_spex, n_spex);

	exit(0);
}
