/*
 * Experimental data  distribution table generator
 * Taken from the uncopyrighted NISTnet code (public domain).
 *
 * Rread in a series of "random" data values, either
 * experimentally or generated from some probability distribution.
 * From this, report statistics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>

void
stats(FILE *fp)
{
	struct stat info;
	double *x;
	int limit;
	int n=0, i;
	double mu=0.0, sigma=0.0, sumsquare=0.0, sum=0.0, top=0.0, rho=0.0;
	double sigma2=0.0;

	fstat(fileno(fp), &info);
	if (info.st_size > 0) {
		limit = 2*info.st_size/sizeof(double);	/* @@ approximate */
	} else {
		limit = 10000;
	}
	x = (double *)malloc(limit*sizeof(double));

	for (i=0; i<limit; ++i){
		fscanf(fp, "%lf", &x[i]);
		if (feof(fp))
			break;
		sumsquare += x[i]*x[i];
		sum += x[i];
		++n;
	}
	mu = sum/(double)n;
	sigma = sqrt((sumsquare - (double)n*mu*mu)/(double)(n-1));

	for (i=1; i < n; ++i){
		top += ((double)x[i]-mu)*((double)x[i-1]-mu);
		sigma2 += ((double)x[i-1] - mu)*((double)x[i-1] - mu);

	}
	rho = top/sigma2;

	printf("mu =    %12.6f\n", mu);
	printf("sigma = %12.6f\n", sigma);
	printf("rho =   %12.6f\n", rho);
	/*printf("sigma2 = %10.4f\n", sqrt(sigma2/(double)(n-1)));*/
	/*printf("correlation rho = %10.6f\n", top/((double)(n-1)*sigma*sigma));*/
}


int
main(int argc, char **argv)
{
	FILE *fp;

	if (argc > 1) {
		fp = fopen(argv[1], "r");
		if (!fp) {
			perror(argv[1]);
			exit(1);
		}
	} else {
		fp = stdin;
	}
	stats(fp);
	return 0;
}
