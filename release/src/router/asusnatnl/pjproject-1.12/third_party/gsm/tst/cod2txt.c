/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/*$Header: /tmp_amd/presto/export/kbs/jutta/src/gsm/RCS/cod2txt.c,v 1.1 1994/10/21 20:52:11 jutta Exp $*/

#include <stdio.h>
#include <assert.h>

#include	"gsm.h"
#include	"proto.h"

char  * pname;

int	debug      = 0;
int	verbosity  = 0;
int	error      = 0;

usage P0()
{
	fprintf(stderr, "Usage: %s [files...]\n", pname);
	exit(1);
}

void process P2((f, filename), FILE * f, char * filename)
{
	gsm_frame	buf;
	gsm_signal	source[160];

	int		cc;
	gsm		r;
	int		nr=0;

	(void)memset(source, 0, sizeof(source));

	if (!(r = gsm_create())) {
		perror("gsm_create");
		error = 1;
		return ;
	}
	gsm_option(r, GSM_OPT_VERBOSE, &verbosity);
	for (;;) {
		cc = fread((char *)source, sizeof(*source), 76, f);
		if (cc == 0) {
			gsm_destroy(r);
			return;
		}
		if (cc != 76) {
			error = 1;
			fprintf(stderr,
				"%s: %s -- %d trailing bytes ignored\n",
				pname, filename, cc);
			gsm_destroy(r);
			return;
		}

		gsm_implode(r, source, buf);
		printf("[%d] ", ++nr);
		if (gsm_print(stdout, r, buf)) {
			fprintf(stderr,
				"%s: %s: bad magic\n", pname, filename);
			gsm_destroy(r);
			return;

		}
	}
}

main P2((ac, av), int ac, char ** av)
{
	int 		opt;
	extern char   * optarg;
	extern int	optind;

	FILE		* f;

	if (!(pname = av[0])) pname = "cod2txt";

	ac--;
	av++;

	if (!ac) process(stdin, "*stdin*");
	else for (; *av; av++) {
		if (!(f = fopen(*av, "r"))) perror(*av);
		else {
			process(f, *av);
			fclose(f);
		}
	}

	exit(error);
}
