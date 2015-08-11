/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/*$Header: /tmp_amd/presto/export/kbs/jutta/src/gsm/RCS/gsm2cod.c,v 1.1 1994/10/21 20:52:11 jutta Exp $*/

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
	gsm_signal	source[76];

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
		cc = fread((char *)buf, sizeof(buf), 1, f);
		if (cc == 0) {
			gsm_destroy(r);
			return;
		}
		if (cc != 1) {
			error = 1;
			fprintf(stderr,
				"%s: %s -- trailing bytes ignored\n",
				pname, filename);
			gsm_destroy(r);
			return;
		}

		gsm_explode(r, buf, source);
		if (write(1, (char *)source, sizeof(source))!= sizeof(source)) {

			perror("write");
			error = 1;
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

	if (!(pname = av[0])) pname = "gsm2cod";

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
