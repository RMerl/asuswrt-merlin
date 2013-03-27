/*
 * Copyright (c) 2009, 2010 Jeff Mahoney <jeffm@suse.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "defs.h"
#ifdef LINUX
#include <stdint.h>
#include <inttypes.h>
#include <linux/blkpg.h>
#include <linux/fs.h>
#include <linux/hdreg.h>

/* ioctls <= 114 are present in Linux 2.4. The following ones have been
 * added since then and headers containing them may not be available on
 * every system. */

#define BLKTRACE_BDEV_SIZE      32
struct blk_user_trace_setup {
	char name[BLKTRACE_BDEV_SIZE];	/* output */
	uint16_t act_mask;		/* input */
	uint32_t buf_size;		/* input */
	uint32_t buf_nr;		/* input */
	uint64_t start_lba;
	uint64_t end_lba;
	uint32_t pid;
};

#ifndef BLKTRACESETUP
#define BLKTRACESETUP _IOWR(0x12,115,struct blk_user_trace_setup)
#endif
#ifndef BLKTRACESTART
#define BLKTRACESTART _IO(0x12,116)
#endif
#ifndef BLKTRACESTOP
#define BLKTRACESTOP _IO(0x12,117)
#endif
#ifndef BLKTRACETEARDOWN
#define BLKTRACETEARDOWN _IO(0x12,118)
#endif
#ifndef BLKDISCARD
#define BLKDISCARD _IO(0x12,119)
#endif
#ifndef BLKIOMIN
#define BLKIOMIN _IO(0x12,120)
#endif
#ifndef BLKIOOPT
#define BLKIOOPT _IO(0x12,121)
#endif
#ifndef BLKALIGNOFF
#define BLKALIGNOFF _IO(0x12,122)
#endif
#ifndef BLKPBSZGET
#define BLKPBSZGET _IO(0x12,123)
#endif
#ifndef BLKDISCARDZEROES
#define BLKDISCARDZEROES _IO(0x12,124)
#endif
#ifndef BLKSECDISCARD
#define BLKSECDISCARD _IO(0x12,125)
#endif

static const struct xlat blkpg_ops[] = {
	{ BLKPG_ADD_PARTITION,	"BLKPG_ADD_PARTITION", },
	{ BLKPG_DEL_PARTITION,	"BLKPG_DEL_PARTITION", },
	{ 0,			NULL },
};

static void
print_blkpg_req(struct tcb *tcp, struct blkpg_ioctl_arg *blkpg)
{
	struct blkpg_partition p;

	tprintf("{");
	printxval(blkpg_ops, blkpg->op, "BLKPG_???");

	tprintf(", flags=%d, datalen=%d, ",
		blkpg->flags, blkpg->datalen);

	if (umove(tcp, (long) blkpg->data, &p) < 0)
		tprintf("%#lx}", (long) blkpg->data);
	else
		tprintf("{start=%lld, length=%lld, pno=%d, "
			"devname=\"%.*s\", volname=\"%.*s\"}}",
			p.start, p.length, p.pno,
			(int) sizeof(p.devname), p.devname,
			(int) sizeof(p.volname), p.volname);
}

int
block_ioctl(struct tcb *tcp, long code, long arg)
{
	switch (code) {
	/* take arg as a value, not as a pointer */
	case BLKRASET:
	case BLKFRASET:
		if (entering(tcp))
			tprintf(", %ld", arg);
		break;

	/* take a signed int */
	case BLKROSET:
	case BLKBSZSET:
		if (entering(tcp)) {
			int val;
			if (umove(tcp, arg, &val) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", %d", val);
		}
		break;

	/* returns an unsigned short */
	case BLKSECTGET:
		if (exiting(tcp)) {
			unsigned short val;
			if (syserror(tcp) || umove(tcp, arg, &val) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", %hu", val);
		}
		break;

	/* return a signed int */
	case BLKROGET:
	case BLKBSZGET:
	case BLKSSZGET:
	case BLKALIGNOFF:
		if (exiting(tcp)) {
			int val;
			if (syserror(tcp) || umove(tcp, arg, &val) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", %d", val);
		}
		break;

	/* return an unsigned int */
	case BLKPBSZGET:
	case BLKIOMIN:
	case BLKIOOPT:
	case BLKDISCARDZEROES:
		if (exiting(tcp)) {
			unsigned int val;
			if (syserror(tcp) || umove(tcp, arg, &val) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", %u", val);
		}
		break;

	/* return a signed long */
	case BLKRAGET:
	case BLKFRAGET:
		if (exiting(tcp)) {
			long val;
			if (syserror(tcp) || umove(tcp, arg, &val) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", %ld", val);
		}
		break;

	/* returns an unsigned long */
	case BLKGETSIZE:
		if (exiting(tcp)) {
			unsigned long val;
			if (syserror(tcp) || umove(tcp, arg, &val) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", %lu", val);
			}
		break;

	/* return an uint64_t */
	case BLKGETSIZE64:
		if (exiting(tcp)) {
			uint64_t val;
			if (syserror(tcp) || umove(tcp, arg, &val) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", %" PRIu64, val);
		}
		break;

	/* More complex types */
	case BLKDISCARD:
	case BLKSECDISCARD:
		if (entering(tcp)) {
			uint64_t range[2];
			if (umove(tcp, arg, range) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", {%" PRIx64 ", %" PRIx64 "}",
					range[0], range[1]);
		}
		break;

	case HDIO_GETGEO:
		if (exiting(tcp)) {
			struct hd_geometry geo;
			if (syserror(tcp) || umove(tcp, arg, &geo) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", {heads=%hhu, sectors=%hhu, "
					"cylinders=%hu, start=%lu}",
					geo.heads, geo.sectors,
					geo.cylinders, geo.start);
		}
		break;

	case BLKPG:
		if (entering(tcp)) {
			struct blkpg_ioctl_arg blkpg;
			if (umove(tcp, arg, &blkpg) < 0)
				tprintf(", %#lx", arg);
			else {
				tprintf(", ");
				print_blkpg_req(tcp, &blkpg);
			}
		}
		break;

	case BLKTRACESETUP:
		if (entering(tcp)) {
			struct blk_user_trace_setup buts;
			if (umove(tcp, arg, &buts) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", {act_mask=%hu, buf_size=%u, "
					"buf_nr=%u, start_lba=%" PRIu64 ", "
					"end_lba=%" PRIu64 ", pid=%u}",
					buts.act_mask, buts.buf_size,
					buts.buf_nr, buts.start_lba,
					buts.end_lba, buts.pid);
		}
		if (exiting(tcp)) {
			struct blk_user_trace_setup buts;
			if (syserror(tcp) || umove(tcp, arg, &buts) < 0)
				tprintf(", %#lx", arg);
			else
				tprintf(", {name=\"%.*s\"}",
					(int) sizeof(buts.name), buts.name);
		}
		break;

	/* No arguments or unhandled */
	case BLKTRACESTART:
	case BLKTRACESTOP:
	case BLKTRACETEARDOWN:
	case BLKFLSBUF: /* Requires driver knowlege */
	case BLKRRPART: /* No args */
	default:
		if (entering(tcp))
			tprintf(", %#lx", arg);
		break;

	};
	return 1;
}
#endif /* LINUX */
