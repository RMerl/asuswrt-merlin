/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/*$Header: /tmp_amd/presto/export/kbs/jutta/src/gsm/RCS/lin2cod.c,v 1.2 1996/07/02 14:33:13 jutta Exp jutta $*/

#include <stdio.h>

#include "gsm.h"
#include "proto.h"

char  * pname;

int	debug      = 0;
int	verbosity  = 0;
int	fast       = 0;
int	wav        = 0;
int	error      = 0;

usage P0()
{
	fprintf(stderr, "Usage: %s [-vwF] [files...]\n", pname);
	exit(1);
}

void process P2((f, filename), FILE * f, char * filename)
{
	gsm_frame	buf;
	short		source[160];
	int		cc;
	gsm		r;

	if (!(r = gsm_create())) {
		perror("gsm_create");
		error = 1;
		return ;
	}
	gsm_option(r, GSM_OPT_VERBOSE, &verbosity);
	gsm_option(r, GSM_OPT_FAST,    &fast);
	gsm_option(r, GSM_OPT_WAV49,   &wav);
	for (;;) {

		if ((cc = fread((char *)source, 1, sizeof(source), f)) == 0) {
			gsm_destroy(r);
#ifdef	COUNT_OVERFLOW
			dump_overflow(stderr);
#endif
			return;
		}

		if (cc != sizeof(source)) {
			error = 1;
			perror(filename);
			fprintf(stderr, "%s: cannot read input from %s\n",
				pname, filename);
			gsm_destroy(r);
			return;
		}

		gsm_encode(r, source, buf);
		gsm_explode(r, buf, source);	/* 76 shorts */
		if (write(1, source, sizeof(*source) * 76)
			!= sizeof(*source) * 76) {

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

	if (!(pname = av[0])) pname = "inp2cod";

	while ((opt = getopt(ac, av, "vwF")) != EOF) switch (opt) {
	case 'v': verbosity++;    break;
	case 'w': wav++;    	  break;
	case 'F': fast++;         break;
	default:  usage();
	}

	ac -= optind;
	av += optind;

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
