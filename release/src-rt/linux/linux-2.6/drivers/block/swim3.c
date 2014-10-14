/*
 * Driver for the SWIM3 (Super Woz Integrated Machine 3)
 * floppy controller found on Power Macintoshes.
 *
 * Copyright (C) 1996 Paul Mackerras.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

/*
 * TODO:
 * handle 2 drives
 * handle GCR disks
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/fd.h>
#include <linux/ioctl.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/dbdma.h>
#include <asm/prom.h>
#include <asm/uaccess.h>
#include <asm/mediabay.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>

static DEFINE_MUTEX(swim3_mutex);
static struct request_queue *swim3_queue;
static struct gendisk *disks[2];
static struct request *fd_req;

#define MAX_FLOPPIES	2

enum swim_state {
	idle,
	locating,
	seeking,
	settling,
	do_transfer,
	jogging,
	available,
	revalidating,
	ejecting
};

#define REG(x)	unsigned char x; char x ## _pad[15];

/*
 * The names for these registers mostly represent speculation on my part.
 * It will be interesting to see how close they are to the names Apple uses.
 */
struct swim3 {
	REG(data);
	REG(timer);		/* counts down at 1MHz */
	REG(error);
	REG(mode);
	REG(select);		/* controls CA0, CA1, CA2 and LSTRB signals */
	REG(setup);
	REG(control);		/* writing bits clears them */
	REG(status);		/* writing bits sets them in control */
	REG(intr);
	REG(nseek);		/* # tracks to seek */
	REG(ctrack);		/* current track number */
	REG(csect);		/* current sector number */
	REG(gap3);		/* size of gap 3 in track format */
	REG(sector);		/* sector # to read or write */
	REG(nsect);		/* # sectors to read or write */
	REG(intr_enable);
};

#define control_bic	control
#define control_bis	status

/* Bits in select register */
#define CA_MASK		7
#define LSTRB		8

/* Bits in control register */
#define DO_SEEK		0x80
#define FORMAT		0x40
#define SELECT		0x20
#define WRITE_SECTORS	0x10
#define DO_ACTION	0x08
#define DRIVE2_ENABLE	0x04
#define DRIVE_ENABLE	0x02
#define INTR_ENABLE	0x01

/* Bits in status register */
#define FIFO_1BYTE	0x80
#define FIFO_2BYTE	0x40
#define ERROR		0x20
#define DATA		0x08
#define RDDATA		0x04
#define INTR_PENDING	0x02
#define MARK_BYTE	0x01

/* Bits in intr and intr_enable registers */
#define ERROR_INTR	0x20
#define DATA_CHANGED	0x10
#define TRANSFER_DONE	0x08
#define SEEN_SECTOR	0x04
#define SEEK_DONE	0x02
#define TIMER_DONE	0x01

/* Bits in error register */
#define ERR_DATA_CRC	0x80
#define ERR_ADDR_CRC	0x40
#define ERR_OVERRUN	0x04
#define ERR_UNDERRUN	0x01

/* Bits in setup register */
#define S_SW_RESET	0x80
#define S_GCR_WRITE	0x40
#define S_IBM_DRIVE	0x20
#define S_TEST_MODE	0x10
#define S_FCLK_DIV2	0x08
#define S_GCR		0x04
#define S_COPY_PROT	0x02
#define S_INV_WDATA	0x01

/* Select values for swim3_action */
#define SEEK_POSITIVE	0
#define SEEK_NEGATIVE	4
#define STEP		1
#define MOTOR_ON	2
#define MOTOR_OFF	6
#define INDEX		3
#define EJECT		7
#define SETMFM		9
#define SETGCR		13

/* Select values for swim3_select and swim3_readbit */
#define STEP_DIR	0
#define STEPPING	1
#define MOTOR_ON	2
#define RELAX		3	/* also eject in progress */
#define READ_DATA_0	4
#define TWOMEG_DRIVE	5
#define SINGLE_SIDED	6	/* drive or diskette is 4MB type? */
#define DRIVE_PRESENT	7
#define DISK_IN		8
#define WRITE_PROT	9
#define TRACK_ZERO	10
#define TACHO		11
#define READ_DATA_1	12
#define MFM_MODE	13
#define SEEK_COMPLETE	14
#define ONEMEG_MEDIA	15

/* Definitions of values used in writing and formatting */
#define DATA_ESCAPE	0x99
#define GCR_SYNC_EXC	0x3f
#define GCR_SYNC_CONV	0x80
#define GCR_FIRST_MARK	0xd5
#define GCR_SECOND_MARK	0xaa
#define GCR_ADDR_MARK	"\xd5\xaa\x00"
#define GCR_DATA_MARK	"\xd5\xaa\x0b"
#define GCR_SLIP_BYTE	"\x27\xaa"
#define GCR_SELF_SYNC	"\x3f\xbf\x1e\x34\x3c\x3f"

#define DATA_99		"\x99\x99"
#define MFM_ADDR_MARK	"\x99\xa1\x99\xa1\x99\xa1\x99\xfe"
#define MFM_INDEX_MARK	"\x99\xc2\x99\xc2\x99\xc2\x99\xfc"
#define MFM_GAP_LEN	12

struct floppy_state {
	enum swim_state	state;
	spinlock_t lock;
	struct swim3 __iomem *swim3;	/* hardware registers */
	struct dbdma_regs __iomem *dma;	/* DMA controller registers */
	int	swim3_intr;	/* interrupt number for SWIM3 */
	int	dma_intr;	/* interrupt number for DMA channel */
	int	cur_cyl;	/* cylinder head is on, or -1 */
	int	cur_sector;	/* last sector we saw go past */
	int	req_cyl;	/* the cylinder for the current r/w request */
	int	head;		/* head number ditto */
	int	req_sector;	/* sector number ditto */
	int	scount;		/* # sectors we're transferring at present */
	int	retries;
	int	settle_time;
	int	secpercyl;	/* disk geometry information */
	int	secpertrack;
	int	total_secs;
	int	write_prot;	/* 1 if write-protected, 0 if not, -1 dunno */
	struct dbdma_cmd *dma_cmd;
	int	ref_count;
	int	expect_cyl;
	struct timer_list timeout;
	int	timeout_pending;
	int	ejected;
	wait_queue_head_t wait;
	int	wanted;
	struct macio_dev *mdev;
	char	dbdma_cmd_space[5 * sizeof(struct dbdma_cmd)];
};

static struct floppy_state floppy_states[MAX_FLOPPIES];
static int floppy_count = 0;
static DEFINE_SPINLOCK(swim3_lock);

static unsigned short write_preamble[] = {
	0x4e4e, 0x4e4e, 0x4e4e, 0x4e4e, 0x4e4e,	/* gap field */
	0, 0, 0, 0, 0, 0,			/* sync field */
	0x99a1, 0x99a1, 0x99a1, 0x99fb,		/* data address mark */
	0x990f					/* no escape for 512 bytes */
};

static unsigned short write_postamble[] = {
	0x9904,					/* insert CRC */
	0x4e4e, 0x4e4e,
	0x9908,					/* stop writing */
	0, 0, 0, 0, 0, 0
};

static void swim3_select(struct floppy_state *fs, int sel);
static void swim3_action(struct floppy_state *fs, int action);
static int swim3_readbit(struct floppy_state *fs, int bit);
static void do_fd_request(struct request_queue * q);
static void start_request(struct floppy_state *fs);
static void set_timeout(struct floppy_state *fs, int nticks,
			void (*proc)(unsigned long));
static void scan_track(struct floppy_state *fs);
static void seek_track(struct floppy_state *fs, int n);
static void init_dma(struct dbdma_cmd *cp, int cmd, void *buf, int count);
static void setup_transfer(struct floppy_state *fs);
static void act(struct floppy_state *fs);
static void scan_timeout(unsigned long data);
static void seek_timeout(unsigned long data);
static void settle_timeout(unsigned long data);
static void xfer_timeout(unsigned long data);
static irqreturn_t swim3_interrupt(int irq, void *dev_id);
/*static void fd_dma_interrupt(int irq, void *dev_id);*/
static int grab_drive(struct floppy_state *fs, enum swim_state state,
		      int interruptible);
static void release_drive(struct floppy_state *fs);
static int fd_eject(struct floppy_state *fs);
static int floppy_ioctl(struct block_device *bdev, fmode_t mode,
			unsigned int cmd, unsigned long param);
static int floppy_open(struct block_device *bdev, fmode_t mode);
static int floppy_release(struct gendisk *disk, fmode_t mode);
static unsigned int floppy_check_events(struct gendisk *disk,
					unsigned int clearing);
static int floppy_revalidate(struct gendisk *disk);

static bool swim3_end_request(int err, unsigned int nr_bytes)
{
	if (__blk_end_request(fd_req, err, nr_bytes))
		return true;

	fd_req = NULL;
	return false;
}

static bool swim3_end_request_cur(int err)
{
	return swim3_end_request(err, blk_rq_cur_bytes(fd_req));
}

static void swim3_select(struct floppy_state *fs, int sel)
{
	struct swim3 __iomem *sw = fs->swim3;

	out_8(&sw->select, RELAX);
	if (sel & 8)
		out_8(&sw->control_bis, SELECT);
	else
		out_8(&sw->control_bic, SELECT);
	out_8(&sw->select, sel & CA_MASK);
}

static void swim3_action(struct floppy_state *fs, int action)
{
	struct swim3 __iomem *sw = fs->swim3;

	swim3_select(fs, action);
	udelay(1);
	out_8(&sw->select, sw->select | LSTRB);
	udelay(2);
	out_8(&sw->select, sw->select & ~LSTRB);
	udelay(1);
}

static int swim3_readbit(struct floppy_state *fs, int bit)
{
	struct swim3 __iomem *sw = fs->swim3;
	int stat;

	swim3_select(fs, bit);
	udelay(1);
	stat = in_8(&sw->status);
	return (stat & DATA) == 0;
}

static void do_fd_request(struct request_queue * q)
{
	int i;

	for(i=0; i<floppy_count; i++) {
		struct floppy_state *fs = &floppy_states[i];
		if (fs->mdev->media_bay &&
		    check_media_bay(fs->mdev->media_bay) != MB_FD)
			continue;
		start_request(fs);
	}
}

static void start_request(struct floppy_state *fs)
{
	struct request *req;
	unsigned long x;

	if (fs->state == idle && fs->wanted) {
		fs->state = available;
		wake_up(&fs->wait);
		return;
	}
	while (fs->state == idle) {
		if (!fd_req) {
			fd_req = blk_fetch_request(swim3_queue);
			if (!fd_req)
				break;
		}
		req = fd_req;
#if 0
		printk("do_fd_req: dev=%s cmd=%d sec=%ld nr_sec=%u buf=%p\n",
		       req->rq_disk->disk_name, req->cmd,
		       (long)blk_rq_pos(req), blk_rq_sectors(req), req->buffer);
		printk("           errors=%d current_nr_sectors=%u\n",
		       req->errors, blk_rq_cur_sectors(req));
#endif

		if (blk_rq_pos(req) >= fs->total_secs) {
			swim3_end_request_cur(-EIO);
			continue;
		}
		if (fs->ejected) {
			swim3_end_request_cur(-EIO);
			continue;
		}

		if (rq_data_dir(req) == WRITE) {
			if (fs->write_prot < 0)
				fs->write_prot = swim3_readbit(fs, WRITE_PROT);
			if (fs->write_prot) {
				swim3_end_request_cur(-EIO);
				continue;
			}
		}

		/* Do not remove the cast. blk_rq_pos(req) is now a
		 * sector_t and can be 64 bits, but it will never go
		 * past 32 bits for this driver anyway, so we can
		 * safely cast it down and not have to do a 64/32
		 * division
		 */
		fs->req_cyl = ((long)blk_rq_pos(req)) / fs->secpercyl;
		x = ((long)blk_rq_pos(req)) % fs->secpercyl;
		fs->head = x / fs->secpertrack;
		fs->req_sector = x % fs->secpertrack + 1;
		fd_req = req;
		fs->state = do_transfer;
		fs->retries = 0;

		act(fs);
	}
}

static void set_timeout(struct floppy_state *fs, int nticks,
			void (*proc)(unsigned long))
{
	unsigned long flags;

	spin_lock_irqsave(&fs->lock, flags);
	if (fs->timeout_pending)
		del_timer(&fs->timeout);
	fs->timeout.expires = jiffies + nticks;
	fs->timeout.function = proc;
	fs->timeout.data = (unsigned long) fs;
	add_timer(&fs->timeout);
	fs->timeout_pending = 1;
	spin_unlock_irqrestore(&fs->lock, flags);
}

static inline void scan_track(struct floppy_state *fs)
{
	struct swim3 __iomem *sw = fs->swim3;

	swim3_select(fs, READ_DATA_0);
	in_8(&sw->intr);		/* clear SEEN_SECTOR bit */
	in_8(&sw->error);
	out_8(&sw->intr_enable, SEEN_SECTOR);
	out_8(&sw->control_bis, DO_ACTION);
	/* enable intr when track found */
	set_timeout(fs, HZ, scan_timeout);	/* enable timeout */
}

static inline void seek_track(struct floppy_state *fs, int n)
{
	struct swim3 __iomem *sw = fs->swim3;

	if (n >= 0) {
		swim3_action(fs, SEEK_POSITIVE);
		sw->nseek = n;
	} else {
		swim3_action(fs, SEEK_NEGATIVE);
		sw->nseek = -n;
	}
	fs->expect_cyl = (fs->cur_cyl >= 0)? fs->cur_cyl + n: -1;
	swim3_select(fs, STEP);
	in_8(&sw->error);
	/* enable intr when seek finished */
	out_8(&sw->intr_enable, SEEK_DONE);
	out_8(&sw->control_bis, DO_SEEK);
	set_timeout(fs, 3*HZ, seek_timeout);	/* enable timeout */
	fs->settle_time = 0;
}

static inline void init_dma(struct dbdma_cmd *cp, int cmd,
			    void *buf, int count)
{
	st_le16(&cp->req_count, count);
	st_le16(&cp->command, cmd);
	st_le32(&cp->phy_addr, virt_to_bus(buf));
	cp->xfer_status = 0;
}

static inline void setup_transfer(struct floppy_state *fs)
{
	int n;
	struct swim3 __iomem *sw = fs->swim3;
	struct dbdma_cmd *cp = fs->dma_cmd;
	struct dbdma_regs __iomem *dr = fs->dma;

	if (blk_rq_cur_sectors(fd_req) <= 0) {
		printk(KERN_ERR "swim3: transfer 0 sectors?\n");
		return;
	}
	if (rq_data_dir(fd_req) == WRITE)
		n = 1;
	else {
		n = fs->secpertrack - fs->req_sector + 1;
		if (n > blk_rq_cur_sectors(fd_req))
			n = blk_rq_cur_sectors(fd_req);
	}
	fs->scount = n;
	swim3_select(fs, fs->head? READ_DATA_1: READ_DATA_0);
	out_8(&sw->sector, fs->req_sector);
	out_8(&sw->nsect, n);
	out_8(&sw->gap3, 0);
	out_le32(&dr->cmdptr, virt_to_bus(cp));
	if (rq_data_dir(fd_req) == WRITE) {
		/* Set up 3 dma commands: write preamble, data, postamble */
		init_dma(cp, OUTPUT_MORE, write_preamble, sizeof(write_preamble));
		++cp;
		init_dma(cp, OUTPUT_MORE, fd_req->buffer, 512);
		++cp;
		init_dma(cp, OUTPUT_LAST, write_postamble, sizeof(write_postamble));
	} else {
		init_dma(cp, INPUT_LAST, fd_req->buffer, n * 512);
	}
	++cp;
	out_le16(&cp->command, DBDMA_STOP);
	out_8(&sw->control_bic, DO_ACTION | WRITE_SECTORS);
	in_8(&sw->error);
	out_8(&sw->control_bic, DO_ACTION | WRITE_SECTORS);
	if (rq_data_dir(fd_req) == WRITE)
		out_8(&sw->control_bis, WRITE_SECTORS);
	in_8(&sw->intr);
	out_le32(&dr->control, (RUN << 16) | RUN);
	/* enable intr when transfer complete */
	out_8(&sw->intr_enable, TRANSFER_DONE);
	out_8(&sw->control_bis, DO_ACTION);
	set_timeout(fs, 2*HZ, xfer_timeout);	/* enable timeout */
}

static void act(struct floppy_state *fs)
{
	for (;;) {
		switch (fs->state) {
		case idle:
			return;		/* XXX shouldn't get here */

		case locating:
			if (swim3_readbit(fs, TRACK_ZERO)) {
				fs->cur_cyl = 0;
				if (fs->req_cyl == 0)
					fs->state = do_transfer;
				else
					fs->state = seeking;
				break;
			}
			scan_track(fs);
			return;

		case seeking:
			if (fs->cur_cyl < 0) {
				fs->expect_cyl = -1;
				fs->state = locating;
				break;
			}
			if (fs->req_cyl == fs->cur_cyl) {
				printk("whoops, seeking 0\n");
				fs->state = do_transfer;
				break;
			}
			seek_track(fs, fs->req_cyl - fs->cur_cyl);
			return;

		case settling:
			/* check for SEEK_COMPLETE after 30ms */
			fs->settle_time = (HZ + 32) / 33;
			set_timeout(fs, fs->settle_time, settle_timeout);
			return;

		case do_transfer:
			if (fs->cur_cyl != fs->req_cyl) {
				if (fs->retries > 5) {
					swim3_end_request_cur(-EIO);
					fs->state = idle;
					return;
				}
				fs->state = seeking;
				break;
			}
			setup_transfer(fs);
			return;

		case jogging:
			seek_track(fs, -5);
			return;

		default:
			printk(KERN_ERR"swim3: unknown state %d\n", fs->state);
			return;
		}
	}
}

static void scan_timeout(unsigned long data)
{
	struct floppy_state *fs = (struct floppy_state *) data;
	struct swim3 __iomem *sw = fs->swim3;

	fs->timeout_pending = 0;
	out_8(&sw->control_bic, DO_ACTION | WRITE_SECTORS);
	out_8(&sw->select, RELAX);
	out_8(&sw->intr_enable, 0);
	fs->cur_cyl = -1;
	if (fs->retries > 5) {
		swim3_end_request_cur(-EIO);
		fs->state = idle;
		start_request(fs);
	} else {
		fs->state = jogging;
		act(fs);
	}
}

static void seek_timeout(unsigned long data)
{
	struct floppy_state *fs = (struct floppy_state *) data;
	struct swim3 __iomem *sw = fs->swim3;

	fs->timeout_pending = 0;
	out_8(&sw->control_bic, DO_SEEK);
	out_8(&sw->select, RELAX);
	out_8(&sw->intr_enable, 0);
	printk(KERN_ERR "swim3: seek timeout\n");
	swim3_end_request_cur(-EIO);
	fs->state = idle;
	start_request(fs);
}

static void settle_timeout(unsigned long data)
{
	struct floppy_state *fs = (struct floppy_state *) data;
	struct swim3 __iomem *sw = fs->swim3;

	fs->timeout_pending = 0;
	if (swim3_readbit(fs, SEEK_COMPLETE)) {
		out_8(&sw->select, RELAX);
		fs->state = locating;
		act(fs);
		return;
	}
	out_8(&sw->select, RELAX);
	if (fs->settle_time < 2*HZ) {
		++fs->settle_time;
		set_timeout(fs, 1, settle_timeout);
		return;
	}
	printk(KERN_ERR "swim3: seek settle timeout\n");
	swim3_end_request_cur(-EIO);
	fs->state = idle;
	start_request(fs);
}

static void xfer_timeout(unsigned long data)
{
	struct floppy_state *fs = (struct floppy_state *) data;
	struct swim3 __iomem *sw = fs->swim3;
	struct dbdma_regs __iomem *dr = fs->dma;
	int n;

	fs->timeout_pending = 0;
	out_le32(&dr->control, RUN << 16);
	/* We must wait a bit for dbdma to stop */
	for (n = 0; (in_le32(&dr->status) & ACTIVE) && n < 1000; n++)
		udelay(1);
	out_8(&sw->intr_enable, 0);
	out_8(&sw->control_bic, WRITE_SECTORS | DO_ACTION);
	out_8(&sw->select, RELAX);
	printk(KERN_ERR "swim3: timeout %sing sector %ld\n",
	       (rq_data_dir(fd_req)==WRITE? "writ": "read"),
	       (long)blk_rq_pos(fd_req));
	swim3_end_request_cur(-EIO);
	fs->state = idle;
	start_request(fs);
}

static irqreturn_t swim3_interrupt(int irq, void *dev_id)
{
	struct floppy_state *fs = (struct floppy_state *) dev_id;
	struct swim3 __iomem *sw = fs->swim3;
	int intr, err, n;
	int stat, resid;
	struct dbdma_regs __iomem *dr;
	struct dbdma_cmd *cp;

	intr = in_8(&sw->intr);
	err = (intr & ERROR_INTR)? in_8(&sw->error): 0;
	if ((intr & ERROR_INTR) && fs->state != do_transfer)
		printk(KERN_ERR "swim3_interrupt, state=%d, dir=%x, intr=%x, err=%x\n",
		       fs->state, rq_data_dir(fd_req), intr, err);
	switch (fs->state) {
	case locating:
		if (intr & SEEN_SECTOR) {
			out_8(&sw->control_bic, DO_ACTION | WRITE_SECTORS);
			out_8(&sw->select, RELAX);
			out_8(&sw->intr_enable, 0);
			del_timer(&fs->timeout);
			fs->timeout_pending = 0;
			if (sw->ctrack == 0xff) {
				printk(KERN_ERR "swim3: seen sector but cyl=ff?\n");
				fs->cur_cyl = -1;
				if (fs->retries > 5) {
					swim3_end_request_cur(-EIO);
					fs->state = idle;
					start_request(fs);
				} else {
					fs->state = jogging;
					act(fs);
				}
				break;
			}
			fs->cur_cyl = sw->ctrack;
			fs->cur_sector = sw->csect;
			if (fs->expect_cyl != -1 && fs->expect_cyl != fs->cur_cyl)
				printk(KERN_ERR "swim3: expected cyl %d, got %d\n",
				       fs->expect_cyl, fs->cur_cyl);
			fs->state = do_transfer;
			act(fs);
		}
		break;
	case seeking:
	case jogging:
		if (sw->nseek == 0) {
			out_8(&sw->control_bic, DO_SEEK);
			out_8(&sw->select, RELAX);
			out_8(&sw->intr_enable, 0);
			del_timer(&fs->timeout);
			fs->timeout_pending = 0;
			if (fs->state == seeking)
				++fs->retries;
			fs->state = settling;
			act(fs);
		}
		break;
	case settling:
		out_8(&sw->intr_enable, 0);
		del_timer(&fs->timeout);
		fs->timeout_pending = 0;
		act(fs);
		break;
	case do_transfer:
		if ((intr & (ERROR_INTR | TRANSFER_DONE)) == 0)
			break;
		out_8(&sw->intr_enable, 0);
		out_8(&sw->control_bic, WRITE_SECTORS | DO_ACTION);
		out_8(&sw->select, RELAX);
		del_timer(&fs->timeout);
		fs->timeout_pending = 0;
		dr = fs->dma;
		cp = fs->dma_cmd;
		if (rq_data_dir(fd_req) == WRITE)
			++cp;
		/*
		 * Check that the main data transfer has finished.
		 * On writing, the swim3 sometimes doesn't use
		 * up all the bytes of the postamble, so we can still
		 * see DMA active here.  That doesn't matter as long
		 * as all the sector data has been transferred.
		 */
		if ((intr & ERROR_INTR) == 0 && cp->xfer_status == 0) {
			/* wait a little while for DMA to complete */
			for (n = 0; n < 100; ++n) {
				if (cp->xfer_status != 0)
					break;
				udelay(1);
				barrier();
			}
		}
		/* turn off DMA */
		out_le32(&dr->control, (RUN | PAUSE) << 16);
		stat = ld_le16(&cp->xfer_status);
		resid = ld_le16(&cp->res_count);
		if (intr & ERROR_INTR) {
			n = fs->scount - 1 - resid / 512;
			if (n > 0) {
				blk_update_request(fd_req, 0, n << 9);
				fs->req_sector += n;
			}
			if (fs->retries < 5) {
				++fs->retries;
				act(fs);
			} else {
				printk("swim3: error %sing block %ld (err=%x)\n",
				       rq_data_dir(fd_req) == WRITE? "writ": "read",
				       (long)blk_rq_pos(fd_req), err);
				swim3_end_request_cur(-EIO);
				fs->state = idle;
			}
		} else {
			if ((stat & ACTIVE) == 0 || resid != 0) {
				/* musta been an error */
				printk(KERN_ERR "swim3: fd dma: stat=%x resid=%d\n", stat, resid);
				printk(KERN_ERR "  state=%d, dir=%x, intr=%x, err=%x\n",
				       fs->state, rq_data_dir(fd_req), intr, err);
				swim3_end_request_cur(-EIO);
				fs->state = idle;
				start_request(fs);
				break;
			}
			if (swim3_end_request(0, fs->scount << 9)) {
				fs->req_sector += fs->scount;
				if (fs->req_sector > fs->secpertrack) {
					fs->req_sector -= fs->secpertrack;
					if (++fs->head > 1) {
						fs->head = 0;
						++fs->req_cyl;
					}
				}
				act(fs);
			} else
				fs->state = idle;
		}
		if (fs->state == idle)
			start_request(fs);
		break;
	default:
		printk(KERN_ERR "swim3: don't know what to do in state %d\n", fs->state);
	}
	return IRQ_HANDLED;
}

/*
static void fd_dma_interrupt(int irq, void *dev_id)
{
}
*/

static int grab_drive(struct floppy_state *fs, enum swim_state state,
		      int interruptible)
{
	unsigned long flags;

	spin_lock_irqsave(&fs->lock, flags);
	if (fs->state != idle) {
		++fs->wanted;
		while (fs->state != available) {
			if (interruptible && signal_pending(current)) {
				--fs->wanted;
				spin_unlock_irqrestore(&fs->lock, flags);
				return -EINTR;
			}
			interruptible_sleep_on(&fs->wait);
		}
		--fs->wanted;
	}
	fs->state = state;
	spin_unlock_irqrestore(&fs->lock, flags);
	return 0;
}

static void release_drive(struct floppy_state *fs)
{
	unsigned long flags;

	spin_lock_irqsave(&fs->lock, flags);
	fs->state = idle;
	start_request(fs);
	spin_unlock_irqrestore(&fs->lock, flags);
}

static int fd_eject(struct floppy_state *fs)
{
	int err, n;

	err = grab_drive(fs, ejecting, 1);
	if (err)
		return err;
	swim3_action(fs, EJECT);
	for (n = 20; n > 0; --n) {
		if (signal_pending(current)) {
			err = -EINTR;
			break;
		}
		swim3_select(fs, RELAX);
		schedule_timeout_interruptible(1);
		if (swim3_readbit(fs, DISK_IN) == 0)
			break;
	}
	swim3_select(fs, RELAX);
	udelay(150);
	fs->ejected = 1;
	release_drive(fs);
	return err;
}

static struct floppy_struct floppy_type =
	{ 2880,18,2,80,0,0x1B,0x00,0xCF,0x6C,NULL };	/*  7 1.44MB 3.5"   */

static int floppy_locked_ioctl(struct block_device *bdev, fmode_t mode,
			unsigned int cmd, unsigned long param)
{
	struct floppy_state *fs = bdev->bd_disk->private_data;
	int err;
		
	if ((cmd & 0x80) && !capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (fs->mdev->media_bay &&
	    check_media_bay(fs->mdev->media_bay) != MB_FD)
		return -ENXIO;

	switch (cmd) {
	case FDEJECT:
		if (fs->ref_count != 1)
			return -EBUSY;
		err = fd_eject(fs);
		return err;
	case FDGETPRM:
	        if (copy_to_user((void __user *) param, &floppy_type,
				 sizeof(struct floppy_struct)))
			return -EFAULT;
		return 0;
	}
	return -ENOTTY;
}

static int floppy_ioctl(struct block_device *bdev, fmode_t mode,
				 unsigned int cmd, unsigned long param)
{
	int ret;

	mutex_lock(&swim3_mutex);
	ret = floppy_locked_ioctl(bdev, mode, cmd, param);
	mutex_unlock(&swim3_mutex);

	return ret;
}

static int floppy_open(struct block_device *bdev, fmode_t mode)
{
	struct floppy_state *fs = bdev->bd_disk->private_data;
	struct swim3 __iomem *sw = fs->swim3;
	int n, err = 0;

	if (fs->ref_count == 0) {
		if (fs->mdev->media_bay &&
		    check_media_bay(fs->mdev->media_bay) != MB_FD)
			return -ENXIO;
		out_8(&sw->setup, S_IBM_DRIVE | S_FCLK_DIV2);
		out_8(&sw->control_bic, 0xff);
		out_8(&sw->mode, 0x95);
		udelay(10);
		out_8(&sw->intr_enable, 0);
		out_8(&sw->control_bis, DRIVE_ENABLE | INTR_ENABLE);
		swim3_action(fs, MOTOR_ON);
		fs->write_prot = -1;
		fs->cur_cyl = -1;
		for (n = 0; n < 2 * HZ; ++n) {
			if (n >= HZ/30 && swim3_readbit(fs, SEEK_COMPLETE))
				break;
			if (signal_pending(current)) {
				err = -EINTR;
				break;
			}
			swim3_select(fs, RELAX);
			schedule_timeout_interruptible(1);
		}
		if (err == 0 && (swim3_readbit(fs, SEEK_COMPLETE) == 0
				 || swim3_readbit(fs, DISK_IN) == 0))
			err = -ENXIO;
		swim3_action(fs, SETMFM);
		swim3_select(fs, RELAX);

	} else if (fs->ref_count == -1 || mode & FMODE_EXCL)
		return -EBUSY;

	if (err == 0 && (mode & FMODE_NDELAY) == 0
	    && (mode & (FMODE_READ|FMODE_WRITE))) {
		check_disk_change(bdev);
		if (fs->ejected)
			err = -ENXIO;
	}

	if (err == 0 && (mode & FMODE_WRITE)) {
		if (fs->write_prot < 0)
			fs->write_prot = swim3_readbit(fs, WRITE_PROT);
		if (fs->write_prot)
			err = -EROFS;
	}

	if (err) {
		if (fs->ref_count == 0) {
			swim3_action(fs, MOTOR_OFF);
			out_8(&sw->control_bic, DRIVE_ENABLE | INTR_ENABLE);
			swim3_select(fs, RELAX);
		}
		return err;
	}

	if (mode & FMODE_EXCL)
		fs->ref_count = -1;
	else
		++fs->ref_count;

	return 0;
}

static int floppy_unlocked_open(struct block_device *bdev, fmode_t mode)
{
	int ret;

	mutex_lock(&swim3_mutex);
	ret = floppy_open(bdev, mode);
	mutex_unlock(&swim3_mutex);

	return ret;
}

static int floppy_release(struct gendisk *disk, fmode_t mode)
{
	struct floppy_state *fs = disk->private_data;
	struct swim3 __iomem *sw = fs->swim3;
	mutex_lock(&swim3_mutex);
	if (fs->ref_count > 0 && --fs->ref_count == 0) {
		swim3_action(fs, MOTOR_OFF);
		out_8(&sw->control_bic, 0xff);
		swim3_select(fs, RELAX);
	}
	mutex_unlock(&swim3_mutex);
	return 0;
}

static unsigned int floppy_check_events(struct gendisk *disk,
					unsigned int clearing)
{
	struct floppy_state *fs = disk->private_data;
	return fs->ejected ? DISK_EVENT_MEDIA_CHANGE : 0;
}

static int floppy_revalidate(struct gendisk *disk)
{
	struct floppy_state *fs = disk->private_data;
	struct swim3 __iomem *sw;
	int ret, n;

	if (fs->mdev->media_bay &&
	    check_media_bay(fs->mdev->media_bay) != MB_FD)
		return -ENXIO;

	sw = fs->swim3;
	grab_drive(fs, revalidating, 0);
	out_8(&sw->intr_enable, 0);
	out_8(&sw->control_bis, DRIVE_ENABLE);
	swim3_action(fs, MOTOR_ON);	/* necessary? */
	fs->write_prot = -1;
	fs->cur_cyl = -1;
	mdelay(1);
	for (n = HZ; n > 0; --n) {
		if (swim3_readbit(fs, SEEK_COMPLETE))
			break;
		if (signal_pending(current))
			break;
		swim3_select(fs, RELAX);
		schedule_timeout_interruptible(1);
	}
	ret = swim3_readbit(fs, SEEK_COMPLETE) == 0
		|| swim3_readbit(fs, DISK_IN) == 0;
	if (ret)
		swim3_action(fs, MOTOR_OFF);
	else {
		fs->ejected = 0;
		swim3_action(fs, SETMFM);
	}
	swim3_select(fs, RELAX);

	release_drive(fs);
	return ret;
}

static const struct block_device_operations floppy_fops = {
	.open		= floppy_unlocked_open,
	.release	= floppy_release,
	.ioctl		= floppy_ioctl,
	.check_events	= floppy_check_events,
	.revalidate_disk= floppy_revalidate,
};

static int swim3_add_device(struct macio_dev *mdev, int index)
{
	struct device_node *swim = mdev->ofdev.dev.of_node;
	struct floppy_state *fs = &floppy_states[index];
	int rc = -EBUSY;

	/* Check & Request resources */
	if (macio_resource_count(mdev) < 2) {
		printk(KERN_WARNING "ifd%d: no address for %s\n",
		       index, swim->full_name);
		return -ENXIO;
	}
	if (macio_irq_count(mdev) < 2) {
		printk(KERN_WARNING "fd%d: no intrs for device %s\n",
			index, swim->full_name);
	}
	if (macio_request_resource(mdev, 0, "swim3 (mmio)")) {
		printk(KERN_ERR "fd%d: can't request mmio resource for %s\n",
		       index, swim->full_name);
		return -EBUSY;
	}
	if (macio_request_resource(mdev, 1, "swim3 (dma)")) {
		printk(KERN_ERR "fd%d: can't request dma resource for %s\n",
		       index, swim->full_name);
		macio_release_resource(mdev, 0);
		return -EBUSY;
	}
	dev_set_drvdata(&mdev->ofdev.dev, fs);

	if (mdev->media_bay == NULL)
		pmac_call_feature(PMAC_FTR_SWIM3_ENABLE, swim, 0, 1);
	
	memset(fs, 0, sizeof(*fs));
	spin_lock_init(&fs->lock);
	fs->state = idle;
	fs->swim3 = (struct swim3 __iomem *)
		ioremap(macio_resource_start(mdev, 0), 0x200);
	if (fs->swim3 == NULL) {
		printk("fd%d: couldn't map registers for %s\n",
		       index, swim->full_name);
		rc = -ENOMEM;
		goto out_release;
	}
	fs->dma = (struct dbdma_regs __iomem *)
		ioremap(macio_resource_start(mdev, 1), 0x200);
	if (fs->dma == NULL) {
		printk("fd%d: couldn't map DMA for %s\n",
		       index, swim->full_name);
		iounmap(fs->swim3);
		rc = -ENOMEM;
		goto out_release;
	}
	fs->swim3_intr = macio_irq(mdev, 0);
	fs->dma_intr = macio_irq(mdev, 1);
	fs->cur_cyl = -1;
	fs->cur_sector = -1;
	fs->secpercyl = 36;
	fs->secpertrack = 18;
	fs->total_secs = 2880;
	fs->mdev = mdev;
	init_waitqueue_head(&fs->wait);

	fs->dma_cmd = (struct dbdma_cmd *) DBDMA_ALIGN(fs->dbdma_cmd_space);
	memset(fs->dma_cmd, 0, 2 * sizeof(struct dbdma_cmd));
	st_le16(&fs->dma_cmd[1].command, DBDMA_STOP);

	if (request_irq(fs->swim3_intr, swim3_interrupt, 0, "SWIM3", fs)) {
		printk(KERN_ERR "fd%d: couldn't request irq %d for %s\n",
		       index, fs->swim3_intr, swim->full_name);
		pmac_call_feature(PMAC_FTR_SWIM3_ENABLE, swim, 0, 0);
		goto out_unmap;
		return -EBUSY;
	}
/*
	if (request_irq(fs->dma_intr, fd_dma_interrupt, 0, "SWIM3-dma", fs)) {
		printk(KERN_ERR "Couldn't get irq %d for SWIM3 DMA",
		       fs->dma_intr);
		return -EBUSY;
	}
*/

	init_timer(&fs->timeout);

	printk(KERN_INFO "fd%d: SWIM3 floppy controller %s\n", floppy_count,
		mdev->media_bay ? "in media bay" : "");

	return 0;

 out_unmap:
	iounmap(fs->dma);
	iounmap(fs->swim3);

 out_release:
	macio_release_resource(mdev, 0);
	macio_release_resource(mdev, 1);

	return rc;
}

static int __devinit swim3_attach(struct macio_dev *mdev, const struct of_device_id *match)
{
	int i, rc;
	struct gendisk *disk;

	/* Add the drive */
	rc = swim3_add_device(mdev, floppy_count);
	if (rc)
		return rc;

	/* Now create the queue if not there yet */
	if (swim3_queue == NULL) {
		/* If we failed, there isn't much we can do as the driver is still
		 * too dumb to remove the device, just bail out
		 */
		if (register_blkdev(FLOPPY_MAJOR, "fd"))
			return 0;
		swim3_queue = blk_init_queue(do_fd_request, &swim3_lock);
		if (swim3_queue == NULL) {
			unregister_blkdev(FLOPPY_MAJOR, "fd");
			return 0;
		}
	}

	/* Now register that disk. Same comment about failure handling */
	i = floppy_count++;
	disk = disks[i] = alloc_disk(1);
	if (disk == NULL)
		return 0;

	disk->major = FLOPPY_MAJOR;
	disk->first_minor = i;
	disk->fops = &floppy_fops;
	disk->private_data = &floppy_states[i];
	disk->queue = swim3_queue;
	disk->flags |= GENHD_FL_REMOVABLE;
	sprintf(disk->disk_name, "fd%d", i);
	set_capacity(disk, 2880);
	add_disk(disk);

	return 0;
}

static struct of_device_id swim3_match[] =
{
	{
	.name		= "swim3",
	},
	{
	.compatible	= "ohare-swim3"
	},
	{
	.compatible	= "swim3"
	},
};

static struct macio_driver swim3_driver =
{
	.driver = {
		.name 		= "swim3",
		.of_match_table	= swim3_match,
	},
	.probe		= swim3_attach,
#if 0
	.suspend	= swim3_suspend,
	.resume		= swim3_resume,
#endif
};


int swim3_init(void)
{
	macio_register_driver(&swim3_driver);
	return 0;
}

module_init(swim3_init)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Paul Mackerras");
MODULE_ALIAS_BLOCKDEV_MAJOR(FLOPPY_MAJOR);
