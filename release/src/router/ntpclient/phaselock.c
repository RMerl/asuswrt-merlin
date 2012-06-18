/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * phaselock.c - Phase locking for NTP client
 *
 * Copyright 2000  Larry Doolittle  <LRDoolittle@lbl.gov>
 * Last hack: 28 November, 2000
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License (Version 2,
 *  June 1991) as published by the Free Software Foundation.  At the
 *  time of writing, that license was published by the FSF with the URL
 *  http://www.gnu.org/copyleft/gpl.html, and is incorporated herein by
 *  reference.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  Possible future improvements:
 *      - Write and test live mode
 *      - Subtract configurable amount from errorbar
 *      - Sculpt code so it's legible, this version is out of control
 *      - Write documentation  :-(
 */

#include <stdio.h>

//#define ENABLE_DEBUG

#define RING_SIZE 16
struct datum {
	unsigned int absolute;
	double skew;
	double errorbar;
	int freq;
	/* s.s.min and s.s.max (skews) are "corrected" to what they would
	 * have been if freq had been constant at its current value during
	 * the measurements.
	 */
	union {
		struct { double min; double max; } s;
		double ss[2];
	} s;
	/*
	double smin;
	double smax;
	 */
} d_ring[RING_SIZE];

struct _seg {
	double slope;
	double offset;
} maxseg[RING_SIZE+1], minseg[RING_SIZE+1];

#ifdef ENABLE_DEBUG
extern int debug;
#define DEBUG_OPTION "d"
#else
#define debug 0
#define DEBUG_OPTION
#endif

/* draw a line from a to c, what the offset is of that line
 * where that line matches b's slope coordinate.
 */
double interpolate(struct _seg *a, struct _seg *b, struct _seg *c)
{
	double x, y;
	x = (b->slope - a->slope) / (c->slope  - a->slope) ;
	y =         a->offset + x * (c->offset - a->offset);
	return y;
}

int next_up(int i) { int r = i+1; if (r>=RING_SIZE) r=0; return r;}
int next_dn(int i) { int r = i-1; if (r<0) r=RING_SIZE-1; return r;}

/* Looks at the line segments that start at point j, that end at
 * all following points (ending at index rp).  The initial point
 * is on curve s0, the ending point is on curve s1.  The curve choice
 * (s.min vs. s.max) is based on the index in ss[].  The scan
 * looks for the largest (sign=0) or smallest (sign=1) slope.
 */
int search(int rp, int j, int s0, int s1, int sign, struct _seg *answer)
{
	double dt, slope;
	int n, nextj=0, cinit=1;
	for (n=next_up(j); n!=next_up(rp); n=next_up(n)) {
		if (0 && debug) fprintf(stderr,"d_ring[%d].s.ss[%d]=%f d_ring[%d].s.ss[%d]=%f\n",
			n, s0, d_ring[n].s.ss[s0], j, s1, d_ring[j].s.ss[s1]);
		dt = d_ring[n].absolute - d_ring[j].absolute;
		slope = (d_ring[n].s.ss[s0] - d_ring[j].s.ss[s1]) / dt;
		if (0 && debug) fprintf(stderr,"slope %d%d%d [%d,%d] = %f\n",
			s0, s1, sign, j, n, slope);
		if (cinit || (slope < answer->slope) ^ sign) {
			answer->slope = slope;
			answer->offset = d_ring[n].s.ss[s0] +
				slope*(d_ring[rp].absolute - d_ring[n].absolute);
			cinit = 0;
			nextj = n;
		}
	}
	return nextj;
}

/* Pseudo-class for finding consistent frequency shift */
#define MIN_INIT 20
struct _polygon {
	double l_min;
	double r_min;
} df;

void polygon_reset()
{
	df.l_min = MIN_INIT;
	df.r_min = MIN_INIT;
}

double find_df(int *flag)
{
	if (df.l_min == 0.0) {
		if (df.r_min == 0.0) {
			return 0.0;   /* every point was OK */
		} else {
			return -df.r_min;
		}
	} else {
		if (df.r_min == 0.0) {
			return df.l_min;
		} else {
			if (flag) *flag=1;
			return 0.0;   /* some points on each side,
			               * or no data at all */
		}
	}
}

/* Finds the amount of delta-f required to move a point onto a
 * target line in delta-f/delta-t phase space.  Any line is OK
 * as long as it's not convex and never returns greater than
 * MIN_INIT. */
double find_shift(double slope, double offset)
{
	double shift  = slope - offset/600.0;
	double shift2 = slope + 0.3 - offset/6000.0;
	if (shift2 < shift) shift = shift2;
	if (debug) fprintf(stderr,"find_shift %f %f -> %f\n", slope, offset, shift);
	if (shift  < 0) return 0.0;
	return shift;
}

void polygon_point(struct _seg *s)
{
	double l, r;
	if (debug) fprintf(stderr,"loop %f %f\n", s->slope, s->offset);
	l = find_shift(- s->slope,   s->offset);
	r = find_shift(  s->slope, - s->offset);
	if (l < df.l_min) df.l_min = l;
	if (r < df.r_min) df.r_min = r;
	if (debug) fprintf(stderr,"constraint left:  %f %f \n", l, df.l_min);
	if (debug) fprintf(stderr,"constraint right: %f %f \n", r, df.r_min);
}

/* Something like linear feedback to be used when we are "close" to
 * phase lock.  Not really used at the moment:  the logic in find_df()
 * never sets the flag. */
double find_df_center(struct _seg *min, struct _seg *max)
{
	double slope  = 0.5 * (max->slope  + min->slope);
	double dslope =       (max->slope  - min->slope);
	double offset = 0.5 * (max->offset + min->offset);
	double doffset =      (max->offset - min->offset);
	double delta = -offset/600.0 - slope;
	double factor = 60.0/(doffset+dslope*600.0);
	if (debug) fprintf(stderr,"find_df_center %f %f\n", delta, factor);
	return factor*delta;
}

int contemplate_data(unsigned int absolute, double skew, double errorbar, int freq)
{
	/*  Here is the actual phase lock loop.
	 *  Need to keep a ring buffer of points to make a rational
	 *  decision how to proceed.  if (debug) print a lot.
	 */
	static int rp=0, valid=0;
	int both_sides_now=0;
	int j, n, c, max_avail, min_avail, dinit;
	int nextj=0;	/* initialization not needed; but gcc can't figure out my logic */
	double cum;
	struct _seg check, save_min, save_max;
	double last_slope;
	int delta_freq;
	double delta_f;
	int inconsistent=0, max_imax, max_imin=0, min_imax, min_imin=0;
	int computed_freq=freq;

	if (debug) fprintf(stderr,"xontemplate %u %.1f %.1f %d\n",absolute,skew,errorbar,freq);
	d_ring[rp].absolute = absolute;
	d_ring[rp].skew     = skew;
	d_ring[rp].errorbar = errorbar - 800.0;   /* quick hack to speed things up */
	d_ring[rp].freq     = freq;

	if (valid<RING_SIZE) ++valid;
	if (valid==RING_SIZE) {
		/*
		 * Pass 1: correct for wandering freq's */
		cum = 0.0;
		if (debug) fprintf(stderr,"\n");
		for (j=rp; ; j=n) {
			d_ring[j].s.s.max = d_ring[j].skew - cum + d_ring[j].errorbar;
			d_ring[j].s.s.min = d_ring[j].skew - cum - d_ring[j].errorbar;
			if (debug) fprintf(stderr,"hist %d %d %f %f %f\n",j,d_ring[j].absolute-absolute,
				cum,d_ring[j].s.s.min,d_ring[j].s.s.max);
			n=next_dn(j);
			if (n == rp) break;
			/* Assume the freq change took place immediately after
			 * the data was taken; this is valid for the case where
			 * this program was responsible for the change.
			 */
			cum = cum + (d_ring[j].absolute-d_ring[n].absolute) *
				(double)(d_ring[j].freq-freq)/65536;
		}
		/*
		 * Pass 2: find the convex down envelope of s.max, composed of
		 * line segments in s.max vs. absolute space, which are
		 * points in freq vs. dt space.  Find points in order of increasing
		 * slope == freq */
		dinit=1; last_slope=-100;
		for (c=1, j=next_up(rp); ; j=nextj) {
			nextj = search(rp, j, 1, 1, 0, &maxseg[c]);
			        search(rp, j, 0, 1, 1, &check);
			if (check.slope < maxseg[c].slope && check.slope > last_slope &&
			    (dinit || check.slope < save_min.slope)) {dinit=0; save_min=check; }
			if (debug) fprintf(stderr,"maxseg[%d] = %f *x+ %f\n",
				 c, maxseg[c].slope, maxseg[c].offset);
			last_slope = maxseg[c].slope;
			c++;
			if (nextj == rp) break;
		}
		if (dinit==1) inconsistent=1;
		if (debug && dinit==0) printf ("mincross %f *x+ %f\n", save_min.slope, save_min.offset);
		max_avail=c;
		/*
		 * Pass 3: find the convex up envelope of s.min, composed of
		 * line segments in s.min vs. absolute space, which are
		 * points in freq vs. dt space.  These points are found in
		 * order of decreasing slope. */
		dinit=1; last_slope=+100.0;
		for (c=1, j=next_up(rp); ; j=nextj) {
			nextj = search(rp, j, 0, 0, 1, &minseg[c]);
			        search(rp, j, 1, 0, 0, &check);
			if (check.slope > minseg[c].slope && check.slope < last_slope &&
			    (dinit || check.slope < save_max.slope)) {dinit=0; save_max=check; }
			if (debug) fprintf(stderr,"minseg[%d] = %f *x+ %f\n",
				 c, minseg[c].slope, minseg[c].offset);
			last_slope = minseg[c].slope;
			c++;
			if (nextj == rp) break;
		}
		if (dinit==1) inconsistent=1;
		if (debug && dinit==0) printf ("maxcross %f *x+ %f\n", save_max.slope, save_max.offset);
		min_avail=c;
		/*
		 * Pass 4: splice together the convex polygon that forms
		 * the envelope of slope/offset coordinates that are consistent
		 * with the observed data.  The order of calls to polygon_point
		 * doesn't matter for the frequency shift determination, but
		 * the order chosen is nice for visual display. */
		polygon_reset();
		polygon_point(&save_min);
		for (dinit=1, c=1; c<max_avail; c++) {
			if (dinit && maxseg[c].slope > save_min.slope) {
				max_imin = c-1;
				maxseg[max_imin] = save_min;
				dinit = 0;
			}
			if (maxseg[c].slope > save_max.slope)
				break;
			if (dinit==0) polygon_point(&maxseg[c]);
		}
		if (dinit && debug) fprintf(stderr,"found maxseg vs. save_min inconsistency\n");
		if (dinit) inconsistent=1;
		max_imax = c;
		maxseg[max_imax] = save_max;

		polygon_point(&save_max);
		for (dinit=1, c=1; c<min_avail; c++) {
			if (dinit && minseg[c].slope < save_max.slope) {
				max_imin = c-1;
				minseg[min_imin] = save_max;
				dinit = 0;
			}
			if (minseg[c].slope < save_min.slope)
				break;
			if (dinit==0) polygon_point(&minseg[c]);
		}
		if (dinit && debug) fprintf(stderr,"found minseg vs. save_max inconsistency\n");
		if (dinit) inconsistent=1;
		min_imax = c;
		minseg[min_imax] = save_max;

		/* not needed for analysis, but shouldn't hurt either */
		if (debug) polygon_point(&save_min);

		/*
		 * Pass 5: decide on a new freq */
		if (inconsistent) {
			fprintf(stderr,"# inconsistent\n");
		} else {
			delta_f = find_df(&both_sides_now);
			if (debug) fprintf(stderr,"find_df() = %e\n", delta_f);
			if (both_sides_now)
				delta_f = find_df_center(&save_min,&save_max);
			delta_freq = delta_f*65536+.5;
			if (debug) fprintf(stderr,"delta_f %f  delta_freq %d  bsn %d\n", delta_f, delta_freq, both_sides_now);
			computed_freq -= delta_freq;
			printf ("# box [( %.3f , %.1f ) ",  save_min.slope, save_min.offset);
			printf (      " ( %.3f , %.1f )] ", save_max.slope, save_max.offset);
			printf (" delta_f %.3f  computed_freq %d\n", delta_f, computed_freq);

			if (computed_freq < -6000000) computed_freq=-6000000;
			if (computed_freq >  6000000) computed_freq= 6000000;
		}
	}
	rp = (rp+1)%RING_SIZE;
	return computed_freq;
}
