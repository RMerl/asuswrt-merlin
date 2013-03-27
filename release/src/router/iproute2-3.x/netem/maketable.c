/*
 * Experimental data  distribution table generator
 * Taken from the uncopyrighted NISTnet code (public domain).
 *
 * Read in a series of "random" data values, either
 * experimentally or generated from some probability distribution.
 * From this, create the inverse distribution table used to approximate
 * the distribution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>


double *
readdoubles(FILE *fp, int *number)
{
	struct stat info;
	double *x;
	int limit;
	int n=0, i;

	fstat(fileno(fp), &info);
	if (info.st_size > 0) {
		limit = 2*info.st_size/sizeof(double);	/* @@ approximate */
	} else {
		limit = 10000;
	}

	x = calloc(limit, sizeof(double));
	if (!x) {
		perror("double alloc");
		exit(3);
	}

	for (i=0; i<limit; ++i){
		fscanf(fp, "%lf", &x[i]);
		if (feof(fp))
			break;
		++n;
	}
	*number = n;
	return x;
}

void
arraystats(double *x, int limit, double *mu, double *sigma, double *rho)
{
	int n=0, i;
	double sumsquare=0.0, sum=0.0, top=0.0;
	double sigma2=0.0;

	for (i=0; i<limit; ++i){
		sumsquare += x[i]*x[i];
		sum += x[i];
		++n;
	}
	*mu = sum/(double)n;
	*sigma = sqrt((sumsquare - (double)n*(*mu)*(*mu))/(double)(n-1));

	for (i=1; i < n; ++i){
		top += ((double)x[i]- *mu)*((double)x[i-1]- *mu);
		sigma2 += ((double)x[i-1] - *mu)*((double)x[i-1] - *mu);

	}
	*rho = top/sigma2;
}

/* Create a (normalized) distribution table from a set of observed
 * values.  The table is fixed to run from (as it happens) -4 to +4,
 * with granularity .00002.
 */

#define TABLESIZE	16384/4
#define TABLEFACTOR	8192
#ifndef MINSHORT
#define MINSHORT	-32768
#define MAXSHORT	32767
#endif

/* Since entries in the inverse are scaled by TABLEFACTOR, and can't be bigger
 * than MAXSHORT, we don't bother looking at a larger domain than this:
 */
#define DISTTABLEDOMAIN ((MAXSHORT/TABLEFACTOR)+1)
#define DISTTABLEGRANULARITY 50000
#define DISTTABLESIZE (DISTTABLEDOMAIN*DISTTABLEGRANULARITY*2)

static int *
makedist(double *x, int limit, double mu, double sigma)
{
	int *table;
	int i, index, first=DISTTABLESIZE, last=0;
	double input;

	table = calloc(DISTTABLESIZE, sizeof(int));
	if (!table) {
		perror("table alloc");
		exit(3);
	}

	for (i=0; i < limit; ++i) {
		/* Normalize value */
		input = (x[i]-mu)/sigma;

		index = (int)rint((input+DISTTABLEDOMAIN)*DISTTABLEGRANULARITY);
		if (index < 0) index = 0;
		if (index >= DISTTABLESIZE) index = DISTTABLESIZE-1;
		++table[index];
		if (index > last)
			last = index +1;
		if (index < first)
			first = index;
	}
	return table;
}

/* replace an array by its cumulative distribution */
static void
cumulativedist(int *table, int limit, int *total)
{
	int accum=0;

	while (--limit >= 0) {
		accum += *table;
		*table++ = accum;
	}
	*total = accum;
}

static short *
inverttable(int *table, int inversesize, int tablesize, int cumulative)
{
	int i, inverseindex, inversevalue;
	short *inverse;
	double findex, fvalue;

	inverse = (short *)malloc(inversesize*sizeof(short));
	for (i=0; i < inversesize; ++i) {
		inverse[i] = MINSHORT;
	}
	for (i=0; i < tablesize; ++i) {
		findex = ((double)i/(double)DISTTABLEGRANULARITY) - DISTTABLEDOMAIN;
		fvalue = (double)table[i]/(double)cumulative;
		inverseindex = (int)rint(fvalue*inversesize);
		inversevalue = (int)rint(findex*TABLEFACTOR);
		if (inversevalue <= MINSHORT) inversevalue = MINSHORT+1;
		if (inversevalue > MAXSHORT) inversevalue = MAXSHORT;
		inverse[inverseindex] = inversevalue;
	}
	return inverse;

}

/* Run simple linear interpolation over the table to fill in missing entries */
static void
interpolatetable(short *table, int limit)
{
	int i, j, last, lasti = -1;

	last = MINSHORT;
	for (i=0; i < limit; ++i) {
		if (table[i] == MINSHORT) {
			for (j=i; j < limit; ++j)
				if (table[j] != MINSHORT)
					break;
			if (j < limit) {
				table[i] = last + (i-lasti)*(table[j]-last)/(j-lasti);
			} else {
				table[i] = last + (i-lasti)*(MAXSHORT-last)/(limit-lasti);
			}
		} else {
			last = table[i];
			lasti = i;
		}
	}
}

static void
printtable(const short *table, int limit)
{
	int i;

	printf("# This is the distribution table for the experimental distribution.\n");

	for (i=0 ; i < limit; ++i) {
		printf("%d%c", table[i],
		       (i % 8) == 7 ? '\n' : ' ');
	}
}

int
main(int argc, char **argv)
{
	FILE *fp;
	double *x;
	double mu, sigma, rho;
	int limit;
	int *table;
	short *inverse;
	int total;

	if (argc > 1) {
		if (!(fp = fopen(argv[1], "r"))) {
			perror(argv[1]);
			exit(1);
		}
	} else {
		fp = stdin;
	}				
	x = readdoubles(fp, &limit);
	if (limit <= 0) {
		fprintf(stderr, "Nothing much read!\n");
		exit(2);
	}
	arraystats(x, limit, &mu, &sigma, &rho);
#ifdef DEBUG
	fprintf(stderr, "%d values, mu %10.4f, sigma %10.4f, rho %10.4f\n",
		limit, mu, sigma, rho);
#endif
	
	table = makedist(x, limit, mu, sigma);
	free((void *) x);
	cumulativedist(table, DISTTABLESIZE, &total);
	inverse = inverttable(table, TABLESIZE, DISTTABLESIZE, total);
	interpolatetable(inverse, TABLESIZE);
	printtable(inverse, TABLESIZE);
	return 0;
}
