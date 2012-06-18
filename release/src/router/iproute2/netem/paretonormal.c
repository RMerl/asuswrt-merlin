/*
 * Paretoormal distribution table generator
 *
 * This distribution is simply .25*normal + .75*pareto; a combination
 * which seems to match experimentally observed distributions reasonably
 *  well, but is computationally easy to handle.
 * The entries represent a scaled inverse of the cumulative distribution
 * function.
 *
 * Taken from the uncopyrighted NISTnet code.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <malloc.h>

#include <linux/types.h>
#include <linux/pkt_sched.h>

#define TABLESIZE	16384
#define TABLEFACTOR	NETEM_DIST_SCALE

static double
normal(double x, double mu, double sigma)
{
	return .5 + .5*erf((x-mu)/(sqrt(2.0)*sigma));
}

static const double a=3.0;

static int
paretovalue(int i)
{
	double dvalue;

	i = 65536-4*i;
	dvalue = (double)i/(double)65536;
	dvalue = 1.0/pow(dvalue, 1.0/a);
	dvalue -= 1.5;
	dvalue *= (4.0/3.0)*(double)TABLEFACTOR;
	if (dvalue > 32767)
		dvalue = 32767;
	return (int)rint(dvalue);
}	

int
main(int argc, char **argv)
{
	int i,n;
	double x;
	double table[TABLESIZE+1];

	for (x = -10.0; x < 10.05; x += .00005) {
		i = rint(TABLESIZE*normal(x, 0.0, 1.0));
		table[i] = x;
	}
	printf(
"# This is the distribution table for the paretonormal distribution.\n"
	);

	for (i = n = 0; i < TABLESIZE; i += 4) {
		int normvalue, parvalue, value;

		normvalue = (int) rint(table[i]*TABLEFACTOR);
		parvalue = paretovalue(i);

		value = (normvalue+3*parvalue)/4;
		if (value < SHRT_MIN) value = SHRT_MIN;
		if (value > SHRT_MAX) value = SHRT_MAX;

		printf(" %d", value);
		if (++n == 8) {
			putchar('\n');
			n = 0;
		}
	}

	return 0;
}
