/*
 * tc_core.c		TC core library.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "tc_core.h"
#include <linux/atm.h>

static double tick_in_usec = 1;
static double clock_factor = 1;

int tc_core_time2big(unsigned time)
{
	__u64 t = time;

	t *= tick_in_usec;
	return (t >> 32) != 0;
}


unsigned tc_core_time2tick(unsigned time)
{
	return time*tick_in_usec;
}

unsigned tc_core_tick2time(unsigned tick)
{
	return tick/tick_in_usec;
}

unsigned tc_core_time2ktime(unsigned time)
{
	return time * clock_factor;
}

unsigned tc_core_ktime2time(unsigned ktime)
{
	return ktime / clock_factor;
}

unsigned tc_calc_xmittime(__u64 rate, unsigned size)
{
	return tc_core_time2tick(TIME_UNITS_PER_SEC*((double)size/(double)rate));
}

unsigned tc_calc_xmitsize(__u64 rate, unsigned ticks)
{
	return ((double)rate*tc_core_tick2time(ticks))/TIME_UNITS_PER_SEC;
}

/*
 * The align to ATM cells is used for determining the (ATM) SAR
 * alignment overhead at the ATM layer. (SAR = Segmentation And
 * Reassembly).  This is for example needed when scheduling packet on
 * an ADSL connection.  Note that the extra ATM-AAL overhead is _not_
 * included in this calculation. This overhead is added in the kernel
 * before doing the rate table lookup, as this gives better precision
 * (as the table will always be aligned for 48 bytes).
 *  --Hawk, d.7/11-2004. <hawk@diku.dk>
 */
static unsigned tc_align_to_atm(unsigned size)
{
	int linksize, cells;
	cells = size / ATM_CELL_PAYLOAD;
	if ((size % ATM_CELL_PAYLOAD) > 0)
		cells++;

	linksize = cells * ATM_CELL_SIZE; /* Use full cell size to add ATM tax */
	return linksize;
}

static unsigned tc_adjust_size(unsigned sz, unsigned mpu, enum link_layer linklayer)
{
	if (sz < mpu)
		sz = mpu;

	switch (linklayer) {
	case LINKLAYER_ATM:
		return tc_align_to_atm(sz);
	case LINKLAYER_ETHERNET:
	default:
		// No size adjustments on Ethernet
		return sz;
	}
}

/* Notice, the rate table calculated here, have gotten replaced in the
 * kernel and is no-longer used for lookups.
 *
 * This happened in kernel release v3.8 caused by kernel
 *  - commit 56b765b79 ("htb: improved accuracy at high rates").
 * This change unfortunately caused breakage of tc overhead and
 * linklayer parameters.
 *
 * Kernel overhead handling got fixed in kernel v3.10 by
 * - commit 01cb71d2d47 (net_sched: restore "overhead xxx" handling)
 *
 * Kernel linklayer handling got fixed in kernel v3.11 by
 * - commit 8a8e3d84b17 (net_sched: restore "linklayer atm" handling)
 */

/*
   rtab[pkt_len>>cell_log] = pkt_xmit_time
 */

int tc_calc_rtable(struct tc_ratespec *r, __u32 *rtab,
		   int cell_log, unsigned mtu,
		   enum link_layer linklayer)
{
	int i;
	unsigned sz;
	unsigned bps = r->rate;
	unsigned mpu = r->mpu;

	if (mtu == 0)
		mtu = 2047;

	if (cell_log < 0) {
		cell_log = 0;
		while ((mtu >> cell_log) > 255)
			cell_log++;
	}

	for (i=0; i<256; i++) {
		sz = tc_adjust_size((i + 1) << cell_log, mpu, linklayer);
		rtab[i] = tc_calc_xmittime(bps, sz);
	}

	r->cell_align=-1; // Due to the sz calc
	r->cell_log=cell_log;
	r->linklayer = (linklayer & TC_LINKLAYER_MASK);
	return cell_log;
}

/*
   stab[pkt_len>>cell_log] = pkt_xmit_size>>size_log
 */

int tc_calc_size_table(struct tc_sizespec *s, __u16 **stab)
{
	int i;
	enum link_layer linklayer = s->linklayer;
	unsigned int sz;

	if (linklayer <= LINKLAYER_ETHERNET && s->mpu == 0) {
		/* don't need data table in this case (only overhead set) */
		s->mtu = 0;
		s->tsize = 0;
		s->cell_log = 0;
		s->cell_align = 0;
		*stab = NULL;
		return 0;
	}

	if (s->mtu == 0)
		s->mtu = 2047;
	if (s->tsize == 0)
		s->tsize = 512;

	s->cell_log = 0;
	while ((s->mtu >> s->cell_log) > s->tsize - 1)
		s->cell_log++;

	*stab = malloc(s->tsize * sizeof(__u16));
	if (!*stab)
		return -1;

again:
	for (i = s->tsize - 1; i >= 0; i--) {
		sz = tc_adjust_size((i + 1) << s->cell_log, s->mpu, linklayer);
		if ((sz >> s->size_log) > UINT16_MAX) {
			s->size_log++;
			goto again;
		}
		(*stab)[i] = sz >> s->size_log;
	}

	s->cell_align = -1; // Due to the sz calc
	return 0;
}

int tc_core_init(void)
{
	FILE *fp;
	__u32 clock_res;
	__u32 t2us;
	__u32 us2t;

	fp = fopen("/proc/net/psched", "r");
	if (fp == NULL)
		return -1;

	if (fscanf(fp, "%08x%08x%08x", &t2us, &us2t, &clock_res) != 3) {
		fclose(fp);
		return -1;
	}
	fclose(fp);

	/* compatibility hack: for old iproute binaries (ignoring
	 * the kernel clock resolution) the kernel advertises a
	 * tick multiplier of 1000 in case of nano-second resolution,
	 * which really is 1. */
	if (clock_res == 1000000000)
		t2us = us2t;

	clock_factor  = (double)clock_res / TIME_UNITS_PER_SEC;
	tick_in_usec = (double)t2us / us2t * clock_factor;
	return 0;
}
