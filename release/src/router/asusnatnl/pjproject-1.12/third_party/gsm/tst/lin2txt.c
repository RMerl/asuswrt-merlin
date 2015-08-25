/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/*$Header: /tmp_amd/presto/export/kbs/jutta/src/gsm/RCS/lin2txt.c,v 1.1 1994/10/21 20:52:11 jutta Exp $*/

#include <stdio.h>

#include "gsm.h"
#include "proto.h"

char  * pname;

int	debug      = 0;
int	verbosity  = 0;
int	error      = 0;

usage P0()
{
	fprintf(stderr, "Usage: %s [-v] [files...]\n", pname);
	exit(1);
}

void process P2((f, filename), FILE * f, char * filename)
{
	short		source[160];
	int		cc, j, k;
	gsm		r;

	if (!(r = gsm_create())) {
		perror("gsm_create");
		error = 1;
		return ;
	}
	gsm_option(r, GSM_OPT_VERBOSE, &verbosity);
	for (;;) {

		if ((cc = fread((char *)source, 1, sizeof(source), f)) == 0) {
			gsm_destroy(r);
#ifdef	COUNT_OVERFLOW
			dump_overflow(stderr);
#endif
			return;
		}
		
		printf("{\t");
		for (j = 0; j < 4; j++) {
			printf("{\t");
			for (k = 0; k < 40; k++) {
				printf("%d", (int)source[ j * 40 + k ]);
				if (k < 39) {
					printf(", ");
					if (k % 4 == 3) printf("\n\t\t");
				} else {
					printf("\t}");
					if (j == 3) printf("\t},\n");
					else printf(",\n\t");
				}
			}
		}
	}
}

main P2((ac, av), int ac, char ** av)
{
	int 		opt;
	extern char   * optarg;
	extern int	optind;

	FILE		* f;

	if (!(pname = av[0])) pname = "inp2txt";

	while ((opt = getopt(ac, av, "v")) != EOF) switch (opt) {
	case 'v': verbosity++;    break;
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
