/* -*- linux-c -*-
 *  drivers/char/viotape.c
 *
 *  iSeries Virtual Tape
 *
 *  Authors: Dave Boutcher <boutcher@us.ibm.com>
 *           Ryan Arnold <ryanarn@us.ibm.com>
 *           Colin Devilbiss <devilbis@us.ibm.com>
 *           Stephen Rothwell
 *
 * (C) Copyright 2000-2004 IBM Corporation
 *
 * This program is free software;  you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) anyu later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * This routine provides access to tape drives owned and managed by an OS/400
 * partition running on the same box as this Linux partition.
 *
 * All tape operations are performed by sending messages back and forth to
 * the OS/400 partition.  The format of the messages is defined in
 * iseries/vio.h
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/mtio.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/major.h>
#include <linux/completion.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include <asm/ioctls.h>
#include <asm/firmware.h>
#include <asm/vio.h>
#include <asm/iseries/vio.h>
#include <asm/iseries/hv_lp_event.h>
#include <asm/iseries/hv_call_event.h>
#include <asm/iseries/hv_lp_config.h>

#define VIOTAPE_VERSION		"1.2"
#define VIOTAPE_MAXREQ		1

#define VIOTAPE_KERN_WARN	KERN_WARNING "viotape: "
#define VIOTAPE_KERN_INFO	KERN_INFO "viotape: "

static DEFINE_MUTEX(proc_viotape_mutex);
static int viotape_numdev;

/*
 * The minor number follows the conventions of the SCSI tape drives.  The
 * rewind and mode are encoded in the minor #.  We use this struct to break
 * them out
 */
struct viot_devinfo_struct {
	int devno;
	int mode;
	int rewind;
};

#define VIOTAPOP_RESET          0
#define VIOTAPOP_FSF	        1
#define VIOTAPOP_BSF	        2
#define VIOTAPOP_FSR	        3
#define VIOTAPOP_BSR	        4
#define VIOTAPOP_WEOF	        5
#define VIOTAPOP_REW	        6
#define VIOTAPOP_NOP	        7
#define VIOTAPOP_EOM	        8
#define VIOTAPOP_ERASE          9
#define VIOTAPOP_SETBLK        10
#define VIOTAPOP_SETDENSITY    11
#define VIOTAPOP_SETPOS	       12
#define VIOTAPOP_GETPOS	       13
#define VIOTAPOP_SETPART       14
#define VIOTAPOP_UNLOAD        15

enum viotaperc {
	viotape_InvalidRange = 0x0601,
	viotape_InvalidToken = 0x0602,
	viotape_DMAError = 0x0603,
	viotape_UseError = 0x0604,
	viotape_ReleaseError = 0x0605,
	viotape_InvalidTape = 0x0606,
	viotape_InvalidOp = 0x0607,
	viotape_TapeErr = 0x0608,

	viotape_AllocTimedOut = 0x0640,
	viotape_BOTEnc = 0x0641,
	viotape_BlankTape = 0x0642,
	viotape_BufferEmpty = 0x0643,
	viotape_CleanCartFound = 0x0644,
	viotape_CmdNotAllowed = 0x0645,
	viotape_CmdNotSupported = 0x0646,
	viotape_DataCheck = 0x0647,
	viotape_DecompressErr = 0x0648,
	viotape_DeviceTimeout = 0x0649,
	viotape_DeviceUnavail = 0x064a,
	viotape_DeviceBusy = 0x064b,
	viotape_EndOfMedia = 0x064c,
	viotape_EndOfTape = 0x064d,
	viotape_EquipCheck = 0x064e,
	viotape_InsufficientRs = 0x064f,
	viotape_InvalidLogBlk = 0x0650,
	viotape_LengthError = 0x0651,
	viotape_LibDoorOpen = 0x0652,
	viotape_LoadFailure = 0x0653,
	viotape_NotCapable = 0x0654,
	viotape_NotOperational = 0x0655,
	viotape_NotReady = 0x0656,
	viotape_OpCancelled = 0x0657,
	viotape_PhyLinkErr = 0x0658,
	viotape_RdyNotBOT = 0x0659,
	viotape_TapeMark = 0x065a,
	viotape_WriteProt = 0x065b
};

static const struct vio_error_entry viotape_err_table[] = {
	{ viotape_InvalidRange, EIO, "Internal error" },
	{ viotape_InvalidToken, EIO, "Internal error" },
	{ viotape_DMAError, EIO, "DMA error" },
	{ viotape_UseError, EIO, "Internal error" },
	{ viotape_ReleaseError, EIO, "Internal error" },
	{ viotape_InvalidTape, EIO, "Invalid tape device" },
	{ viotape_InvalidOp, EIO, "Invalid operation" },
	{ viotape_TapeErr, EIO, "Tape error" },
	{ viotape_AllocTimedOut, EBUSY, "Allocate timed out" },
	{ viotape_BOTEnc, EIO, "Beginning of tape encountered" },
	{ viotape_BlankTape, EIO, "Blank tape" },
	{ viotape_BufferEmpty, EIO, "Buffer empty" },
	{ viotape_CleanCartFound, ENOMEDIUM, "Cleaning cartridge found" },
	{ viotape_CmdNotAllowed, EIO, "Command not allowed" },
	{ viotape_CmdNotSupported, EIO, "Command not supported" },
	{ viotape_DataCheck, EIO, "Data check" },
	{ viotape_DecompressErr, EIO, "Decompression error" },
	{ viotape_DeviceTimeout, EBUSY, "Device timeout" },
	{ viotape_DeviceUnavail, EIO, "Device unavailable" },
	{ viotape_DeviceBusy, EBUSY, "Device busy" },
	{ viotape_EndOfMedia, ENOSPC, "End of media" },
	{ viotape_EndOfTape, ENOSPC, "End of tape" },
	{ viotape_EquipCheck, EIO, "Equipment check" },
	{ viotape_InsufficientRs, EOVERFLOW, "Insufficient tape resources" },
	{ viotape_InvalidLogBlk, EIO, "Invalid logical block location" },
	{ viotape_LengthError, EOVERFLOW, "Length error" },
	{ viotape_LibDoorOpen, EBUSY, "Door open" },
	{ viotape_LoadFailure, ENOMEDIUM, "Load failure" },
	{ viotape_NotCapable, EIO, "Not capable" },
	{ viotape_NotOperational, EIO, "Not operational" },
	{ viotape_NotReady, EIO, "Not ready" },
	{ viotape_OpCancelled, EIO, "Operation cancelled" },
	{ viotape_PhyLinkErr, EIO, "Physical link error" },
	{ viotape_RdyNotBOT, EIO, "Ready but not beginning of tape" },
	{ viotape_TapeMark, EIO, "Tape mark" },
	{ viotape_WriteProt, EROFS, "Write protection error" },
	{ 0, 0, NULL },
};

/* Maximum number of tapes we support */
#define VIOTAPE_MAX_TAPE	HVMAXARCHITECTEDVIRTUALTAPES
#define MAX_PARTITIONS		4

/* defines for current tape state */
#define VIOT_IDLE		0
#define VIOT_READING		1
#define VIOT_WRITING		2

/* Our info on the tapes */
static struct {
	const char *rsrcname;
	const char *type;
	const char *model;
} viotape_unitinfo[VIOTAPE_MAX_TAPE];

static struct mtget viomtget[VIOTAPE_MAX_TAPE];

static struct class *tape_class;

static struct device *tape_device[VIOTAPE_MAX_TAPE];

/*
 * maintain the current state of each tape (and partition)
 * so that we know when to write EOF marks.
 */
static struct {
	unsigned char	cur_part;
	unsigned char	part_stat_rwi[MAX_PARTITIONS];
} state[VIOTAPE_MAX_TAPE];

/* We single-thread */
static struct semaphore reqSem;

/*
 * When we send a request, we use this struct to get the response back
 * from the interrupt handler
 */
struct op_struct {
	void			*buffer;
	dma_addr_t		dmaaddr;
	size_t			count;
	int			rc;
	int			non_blocking;
	struct completion	com;
	struct device		*dev;
	struct op_struct	*next;
};

static spinlock_t	op_struct_list_lock;
static struct op_struct	*op_struct_list;

/* forward declaration to resolve interdependence */
static int chg_state(int index, unsigned char new_state, struct file *file);

/* procfs support */
static int proc_viotape_show(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "viotape driver version " VIOTAPE_VERSION "\n");
	for (i = 0; i < viotape_numdev; i++) {
		seq_printf(m, "viotape device %d is iSeries resource %10.10s"
				"type %4.4s, model %3.3s\n",
				i, viotape_unitinfo[i].rsrcname,
				viotape_unitinfo[i].type,
				viotape_unitinfo[i].model);
	}
	return 0;
}

static int proc_viotape_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_viotape_show, NULL);
}

static const struct file_operations proc_viotape_operations = {
	.owner		= THIS_MODULE,
	.open		= proc_viotape_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* Decode the device minor number into its parts */
void get_dev_info(struct inode *ino, struct viot_devinfo_struct *devi)
{
	devi->devno = iminor(ino) & 0x1F;
	devi->mode = (iminor(ino) & 0x60) >> 5;
	/* if bit is set in the minor, do _not_ rewind automatically */
	devi->rewind = (iminor(ino) & 0x80) == 0;
}

/* This is called only from the exit and init paths, so no need for locking */
static void clear_op_struct_pool(void)
{
	while (op_struct_list) {
		struct op_struct *toFree = op_struct_list;
		op_struct_list = op_struct_list->next;
		kfree(toFree);
	}
}

/* Likewise, this is only called from the init path */
static int add_op_structs(int structs)
{
	int i;

	for (i = 0; i < structs; ++i) {
		struct op_struct *new_struct =
			kmalloc(sizeof(*new_struct), GFP_KERNEL);
		if (!new_struct) {
			clear_op_struct_pool();
			return -ENOMEM;
		}
		new_struct->next = op_struct_list;
		op_struct_list = new_struct;
	}
	return 0;
}

/* Allocate an op structure from our pool */
static struct op_struct *get_op_struct(void)
{
	struct op_struct *retval;
	unsigned long flags;

	spin_lock_irqsave(&op_struct_list_lock, flags);
	retval = op_struct_list;
	if (retval)
		op_struct_list = retval->next;
	spin_unlock_irqrestore(&op_struct_list_lock, flags);
	if (retval) {
		memset(retval, 0, sizeof(*retval));
		init_completion(&retval->com);
	}

	return retval;
}

/* Return an op structure to our pool */
static void free_op_struct(struct op_struct *op_struct)
{
	unsigned long flags;

	spin_lock_irqsave(&op_struct_list_lock, flags);
	op_struct->next = op_struct_list;
	op_struct_list = op_struct;
	spin_unlock_irqrestore(&op_struct_list_lock, flags);
}

/* Map our tape return codes to errno values */
int tape_rc_to_errno(int tape_rc, char *operation, int tapeno)
{
	const struct vio_error_entry *err;

	if (tape_rc == 0)
		return 0;

	err = vio_lookup_rc(viotape_err_table, tape_rc);
	printk(VIOTAPE_KERN_WARN "error(%s) 0x%04x on Device %d (%-10s): %s\n",
			operation, tape_rc, tapeno,
			viotape_unitinfo[tapeno].rsrcname, err->msg);
	return -err->errno;
}

/* Write */
static ssize_t viotap_write(struct file *file, const char *buf,
		size_t count, loff_t * ppos)
{
	HvLpEvent_Rc hvrc;
	unsigned short flags = file->f_flags;
	int noblock = ((flags & O_NONBLOCK) != 0);
	ssize_t ret;
	struct viot_devinfo_struct devi;
	struct op_struct *op = get_op_struct();

	if (op == NULL)
		return -ENOMEM;

	get_dev_info(file->f_path.dentry->d_inode, &devi);

	/*
	 * We need to make sure we can send a request.  We use
	 * a semaphore to keep track of # requests in use.  If
	 * we are non-blocking, make sure we don't block on the
	 * semaphore
	 */
	if (noblock) {
		if (down_trylock(&reqSem)) {
			ret = -EWOULDBLOCK;
			goto free_op;
		}
	} else
		down(&reqSem);

	/* Allocate a DMA buffer */
	op->dev = tape_device[devi.devno];
	op->buffer = dma_alloc_coherent(op->dev, count, &op->dmaaddr,
			GFP_ATOMIC);

	if (op->buffer == NULL) {
		printk(VIOTAPE_KERN_WARN
				"error allocating dma buffer for len %ld\n",
				count);
		ret = -EFAULT;
		goto up_sem;
	}

	/* Copy the data into the buffer */
	if (copy_from_user(op->buffer, buf, count)) {
		printk(VIOTAPE_KERN_WARN "tape: error on copy from user\n");
		ret = -EFAULT;
		goto free_dma;
	}

	op->non_blocking = noblock;
	init_completion(&op->com);
	op->count = count;

	hvrc = HvCallEvent_signalLpEventFast(viopath_hostLp,
			HvLpEvent_Type_VirtualIo,
			viomajorsubtype_tape | viotapewrite,
			HvLpEvent_AckInd_DoAck, HvLpEvent_AckType_ImmediateAck,
			viopath_sourceinst(viopath_hostLp),
			viopath_targetinst(viopath_hostLp),
			(u64)(unsigned long)op, VIOVERSION << 16,
			((u64)devi.devno << 48) | op->dmaaddr, count, 0, 0);
	if (hvrc != HvLpEvent_Rc_Good) {
		printk(VIOTAPE_KERN_WARN "hv error on op %d\n",
				(int)hvrc);
		ret = -EIO;
		goto free_dma;
	}

	if (noblock)
		return count;

	wait_for_completion(&op->com);

	if (op->rc)
		ret = tape_rc_to_errno(op->rc, "write", devi.devno);
	else {
		chg_state(devi.devno, VIOT_WRITING, file);
		ret = op->count;
	}

free_dma:
	dma_free_coherent(op->dev, count, op->buffer, op->dmaaddr);
up_sem:
	up(&reqSem);
free_op:
	free_op_struct(op);
	return ret;
}

/* read */
static ssize_t viotap_read(struct file *file, char *buf, size_t count,
		loff_t *ptr)
{
	HvLpEvent_Rc hvrc;
	unsigned short flags = file->f_flags;
	struct op_struct *op = get_op_struct();
	int noblock = ((flags & O_NONBLOCK) != 0);
	ssize_t ret;
	struct viot_devinfo_struct devi;

	if (op == NULL)
		return -ENOMEM;

	get_dev_info(file->f_path.dentry->d_inode, &devi);

	/*
	 * We need to make sure we can send a request.  We use
	 * a semaphore to keep track of # requests in use.  If
	 * we are non-blocking, make sure we don't block on the
	 * semaphore
	 */
	if (noblock) {
		if (down_trylock(&reqSem)) {
			ret = -EWOULDBLOCK;
			goto free_op;
		}
	} else
		down(&reqSem);

	chg_state(devi.devno, VIOT_READING, file);

	/* Allocate a DMA buffer */
	op->dev = tape_device[devi.devno];
	op->buffer = dma_alloc_coherent(op->dev, count, &op->dmaaddr,
			GFP_ATOMIC);
	if (op->buffer == NULL) {
		ret = -EFAULT;
		goto up_sem;
	}

	op->count = count;
	init_completion(&op->com);

	hvrc = HvCallEvent_signalLpEventFast(viopath_hostLp,
			HvLpEvent_Type_VirtualIo,
			viomajorsubtype_tape | viotaperead,
			HvLpEvent_AckInd_DoAck, HvLpEvent_AckType_ImmediateAck,
			viopath_sourceinst(viopath_hostLp),
			viopath_targetinst(viopath_hostLp),
			(u64)(unsigned long)op, VIOVERSION << 16,
			((u64)devi.devno << 48) | op->dmaaddr, count, 0, 0);
	if (hvrc != HvLpEvent_Rc_Good) {
		printk(VIOTAPE_KERN_WARN "tape hv error on op %d\n",
				(int)hvrc);
		ret = -EIO;
		goto free_dma;
	}

	wait_for_completion(&op->com);

	if (op->rc)
		ret = tape_rc_to_errno(op->rc, "read", devi.devno);
	else {
		ret = op->count;
		if (ret && copy_to_user(buf, op->buffer, ret)) {
			printk(VIOTAPE_KERN_WARN "error on copy_to_user\n");
			ret = -EFAULT;
		}
	}

free_dma:
	dma_free_coherent(op->dev, count, op->buffer, op->dmaaddr);
up_sem:
	up(&reqSem);
free_op:
	free_op_struct(op);
	return ret;
}

/* ioctl */
static int viotap_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	HvLpEvent_Rc hvrc;
	int ret;
	struct viot_devinfo_struct devi;
	struct mtop mtc;
	u32 myOp;
	struct op_struct *op = get_op_struct();

	if (op == NULL)
		return -ENOMEM;

	get_dev_info(file->f_path.dentry->d_inode, &devi);

	down(&reqSem);

	ret = -EINVAL;

	switch (cmd) {
	case MTIOCTOP:
		ret = -EFAULT;
		/*
		 * inode is null if and only if we (the kernel)
		 * made the request
		 */
		if (inode == NULL)
			memcpy(&mtc, (void *) arg, sizeof(struct mtop));
		else if (copy_from_user((char *)&mtc, (char *)arg,
					sizeof(struct mtop)))
			goto free_op;

		ret = -EIO;
		switch (mtc.mt_op) {
		case MTRESET:
			myOp = VIOTAPOP_RESET;
			break;
		case MTFSF:
			myOp = VIOTAPOP_FSF;
			break;
		case MTBSF:
			myOp = VIOTAPOP_BSF;
			break;
		case MTFSR:
			myOp = VIOTAPOP_FSR;
			break;
		case MTBSR:
			myOp = VIOTAPOP_BSR;
			break;
		case MTWEOF:
			myOp = VIOTAPOP_WEOF;
			break;
		case MTREW:
			myOp = VIOTAPOP_REW;
			break;
		case MTNOP:
			myOp = VIOTAPOP_NOP;
			break;
		case MTEOM:
			myOp = VIOTAPOP_EOM;
			break;
		case MTERASE:
			myOp = VIOTAPOP_ERASE;
			break;
		case MTSETBLK:
			myOp = VIOTAPOP_SETBLK;
			break;
		case MTSETDENSITY:
			myOp = VIOTAPOP_SETDENSITY;
			break;
		case MTTELL:
			myOp = VIOTAPOP_GETPOS;
			break;
		case MTSEEK:
			myOp = VIOTAPOP_SETPOS;
			break;
		case MTSETPART:
			myOp = VIOTAPOP_SETPART;
			break;
		case MTOFFL:
			myOp = VIOTAPOP_UNLOAD;
			break;
		default:
			printk(VIOTAPE_KERN_WARN "MTIOCTOP called "
					"with invalid op 0x%x\n", mtc.mt_op);
			goto free_op;
		}

		/*
		 * if we moved the head, we are no longer
		 * reading or writing
		 */
		switch (mtc.mt_op) {
		case MTFSF:
		case MTBSF:
		case MTFSR:
		case MTBSR:
		case MTTELL:
		case MTSEEK:
		case MTREW:
			chg_state(devi.devno, VIOT_IDLE, file);
		}

		init_completion(&op->com);
		hvrc = HvCallEvent_signalLpEventFast(viopath_hostLp,
				HvLpEvent_Type_VirtualIo,
				viomajorsubtype_tape | viotapeop,
				HvLpEvent_AckInd_DoAck,
				HvLpEvent_AckType_ImmediateAck,
				viopath_sourceinst(viopath_hostLp),
				viopath_targetinst(viopath_hostLp),
				(u64)(unsigned long)op,
				VIOVERSION << 16,
				((u64)devi.devno << 48), 0,
				(((u64)myOp) << 32) | mtc.mt_count, 0);
		if (hvrc != HvLpEvent_Rc_Good) {
			printk(VIOTAPE_KERN_WARN "hv error on op %d\n",
					(int)hvrc);
			goto free_op;
		}
		wait_for_completion(&op->com);
		ret = tape_rc_to_errno(op->rc, "tape operation", devi.devno);
		goto free_op;

	case MTIOCGET:
		ret = -EIO;
		init_completion(&op->com);
		hvrc = HvCallEvent_signalLpEventFast(viopath_hostLp,
				HvLpEvent_Type_VirtualIo,
				viomajorsubtype_tape | viotapegetstatus,
				HvLpEvent_AckInd_DoAck,
				HvLpEvent_AckType_ImmediateAck,
				viopath_sourceinst(viopath_hostLp),
				viopath_targetinst(viopath_hostLp),
				(u64)(unsigned long)op, VIOVERSION << 16,
				((u64)devi.devno << 48), 0, 0, 0);
		if (hvrc != HvLpEvent_Rc_Good) {
			printk(VIOTAPE_KERN_WARN "hv error on op %d\n",
					(int)hvrc);
			goto free_op;
		}
		wait_for_completion(&op->com);

		/* Operation is complete - grab the error code */
		ret = tape_rc_to_errno(op->rc, "get status", devi.devno);
		free_op_struct(op);
		up(&reqSem);

		if ((ret == 0) && copy_to_user((void *)arg,
					&viomtget[devi.devno],
					sizeof(viomtget[0])))
			ret = -EFAULT;
		return ret;
	case MTIOCPOS:
		printk(VIOTAPE_KERN_WARN "Got an (unsupported) MTIOCPOS\n");
		break;
	default:
		printk(VIOTAPE_KERN_WARN "got an unsupported ioctl 0x%0x\n",
				cmd);
		break;
	}

free_op:
	free_op_struct(op);
	up(&reqSem);
	return ret;
}

static long viotap_unlocked_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	long rc;

	mutex_lock(&proc_viotape_mutex);
	rc = viotap_ioctl(file->f_path.dentry->d_inode, file, cmd, arg);
	mutex_unlock(&proc_viotape_mutex);
	return rc;
}

static int viotap_open(struct inode *inode, struct file *file)
{
	HvLpEvent_Rc hvrc;
	struct viot_devinfo_struct devi;
	int ret;
	struct op_struct *op = get_op_struct();

	if (op == NULL)
		return -ENOMEM;

	mutex_lock(&proc_viotape_mutex);
	get_dev_info(file->f_path.dentry->d_inode, &devi);

	/* Note: We currently only support one mode! */
	if ((devi.devno >= viotape_numdev) || (devi.mode)) {
		ret = -ENODEV;
		goto free_op;
	}

	init_completion(&op->com);

	hvrc = HvCallEvent_signalLpEventFast(viopath_hostLp,
			HvLpEvent_Type_VirtualIo,
			viomajorsubtype_tape | viotapeopen,
			HvLpEvent_AckInd_DoAck, HvLpEvent_AckType_ImmediateAck,
			viopath_sourceinst(viopath_hostLp),
			viopath_targetinst(viopath_hostLp),
			(u64)(unsigned long)op, VIOVERSION << 16,
			((u64)devi.devno << 48), 0, 0, 0);
	if (hvrc != 0) {
		printk(VIOTAPE_KERN_WARN "bad rc on signalLpEvent %d\n",
				(int) hvrc);
		ret = -EIO;
		goto free_op;
	}

	wait_for_completion(&op->com);
	ret = tape_rc_to_errno(op->rc, "open", devi.devno);

free_op:
	free_op_struct(op);
	mutex_unlock(&proc_viotape_mutex);
	return ret;
}


static int viotap_release(struct inode *inode, struct file *file)
{
	HvLpEvent_Rc hvrc;
	struct viot_devinfo_struct devi;
	int ret = 0;
	struct op_struct *op = get_op_struct();

	if (op == NULL)
		return -ENOMEM;
	init_completion(&op->com);

	get_dev_info(file->f_path.dentry->d_inode, &devi);

	if (devi.devno >= viotape_numdev) {
		ret = -ENODEV;
		goto free_op;
	}

	chg_state(devi.devno, VIOT_IDLE, file);

	if (devi.rewind) {
		hvrc = HvCallEvent_signalLpEventFast(viopath_hostLp,
				HvLpEvent_Type_VirtualIo,
				viomajorsubtype_tape | viotapeop,
				HvLpEvent_AckInd_DoAck,
				HvLpEvent_AckType_ImmediateAck,
				viopath_sourceinst(viopath_hostLp),
				viopath_targetinst(viopath_hostLp),
				(u64)(unsigned long)op, VIOVERSION << 16,
				((u64)devi.devno << 48), 0,
				((u64)VIOTAPOP_REW) << 32, 0);
		wait_for_completion(&op->com);

		tape_rc_to_errno(op->rc, "rewind", devi.devno);
	}

	hvrc = HvCallEvent_signalLpEventFast(viopath_hostLp,
			HvLpEvent_Type_VirtualIo,
			viomajorsubtype_tape | viotapeclose,
			HvLpEvent_AckInd_DoAck, HvLpEvent_AckType_ImmediateAck,
			viopath_sourceinst(viopath_hostLp),
			viopath_targetinst(viopath_hostLp),
			(u64)(unsigned long)op, VIOVERSION << 16,
			((u64)devi.devno << 48), 0, 0, 0);
	if (hvrc != 0) {
		printk(VIOTAPE_KERN_WARN "bad rc on signalLpEvent %d\n",
				(int) hvrc);
		ret = -EIO;
		goto free_op;
	}

	wait_for_completion(&op->com);

	if (op->rc)
		printk(VIOTAPE_KERN_WARN "close failed\n");

free_op:
	free_op_struct(op);
	return ret;
}

const struct file_operations viotap_fops = {
	.owner =		THIS_MODULE,
	.read =			viotap_read,
	.write =		viotap_write,
	.unlocked_ioctl =	viotap_unlocked_ioctl,
	.open =			viotap_open,
	.release =		viotap_release,
	.llseek = 		noop_llseek,
};

/* Handle interrupt events for tape */
static void vioHandleTapeEvent(struct HvLpEvent *event)
{
	int tapeminor;
	struct op_struct *op;
	struct viotapelpevent *tevent = (struct viotapelpevent *)event;

	if (event == NULL) {
		/* Notification that a partition went away! */
		if (!viopath_isactive(viopath_hostLp)) {
			/* TODO! Clean up */
		}
		return;
	}

	tapeminor = event->xSubtype & VIOMINOR_SUBTYPE_MASK;
	op = (struct op_struct *)event->xCorrelationToken;
	switch (tapeminor) {
	case viotapeopen:
	case viotapeclose:
		op->rc = tevent->sub_type_result;
		complete(&op->com);
		break;
	case viotaperead:
		op->rc = tevent->sub_type_result;
		op->count = tevent->len;
		complete(&op->com);
		break;
	case viotapewrite:
		if (op->non_blocking) {
			dma_free_coherent(op->dev, op->count,
					op->buffer, op->dmaaddr);
			free_op_struct(op);
			up(&reqSem);
		} else {
			op->rc = tevent->sub_type_result;
			op->count = tevent->len;
			complete(&op->com);
		}
		break;
	case viotapeop:
	case viotapegetpos:
	case viotapesetpos:
	case viotapegetstatus:
		if (op) {
			op->count = tevent->u.op.count;
			op->rc = tevent->sub_type_result;
			if (!op->non_blocking)
				complete(&op->com);
		}
		break;
	default:
		printk(VIOTAPE_KERN_WARN "weird ack\n");
	}
}

static int viotape_probe(struct vio_dev *vdev, const struct vio_device_id *id)
{
	int i = vdev->unit_address;
	int j;
	struct device_node *node = vdev->dev.of_node;

	if (i >= VIOTAPE_MAX_TAPE)
		return -ENODEV;
	if (!node)
		return -ENODEV;

	if (i >= viotape_numdev)
		viotape_numdev = i + 1;

	tape_device[i] = &vdev->dev;
	viotape_unitinfo[i].rsrcname = of_get_property(node,
					"linux,vio_rsrcname", NULL);
	viotape_unitinfo[i].type = of_get_property(node, "linux,vio_type",
					NULL);
	viotape_unitinfo[i].model = of_get_property(node, "linux,vio_model",
					NULL);

	state[i].cur_part = 0;
	for (j = 0; j < MAX_PARTITIONS; ++j)
		state[i].part_stat_rwi[j] = VIOT_IDLE;
	device_create(tape_class, NULL, MKDEV(VIOTAPE_MAJOR, i), NULL,
		      "iseries!vt%d", i);
	device_create(tape_class, NULL, MKDEV(VIOTAPE_MAJOR, i | 0x80), NULL,
		      "iseries!nvt%d", i);
	printk(VIOTAPE_KERN_INFO "tape iseries/vt%d is iSeries "
			"resource %10.10s type %4.4s, model %3.3s\n",
			i, viotape_unitinfo[i].rsrcname,
			viotape_unitinfo[i].type, viotape_unitinfo[i].model);
	return 0;
}

static int viotape_remove(struct vio_dev *vdev)
{
	int i = vdev->unit_address;

	device_destroy(tape_class, MKDEV(VIOTAPE_MAJOR, i | 0x80));
	device_destroy(tape_class, MKDEV(VIOTAPE_MAJOR, i));
	return 0;
}

/**
 * viotape_device_table: Used by vio.c to match devices that we
 * support.
 */
static struct vio_device_id viotape_device_table[] __devinitdata = {
	{ "byte", "IBM,iSeries-viotape" },
	{ "", "" }
};
MODULE_DEVICE_TABLE(vio, viotape_device_table);

static struct vio_driver viotape_driver = {
	.id_table = viotape_device_table,
	.probe = viotape_probe,
	.remove = viotape_remove,
	.driver = {
		.name = "viotape",
		.owner = THIS_MODULE,
	}
};


int __init viotap_init(void)
{
	int ret;

	if (!firmware_has_feature(FW_FEATURE_ISERIES))
		return -ENODEV;

	op_struct_list = NULL;
	if ((ret = add_op_structs(VIOTAPE_MAXREQ)) < 0) {
		printk(VIOTAPE_KERN_WARN "couldn't allocate op structs\n");
		return ret;
	}
	spin_lock_init(&op_struct_list_lock);

	sema_init(&reqSem, VIOTAPE_MAXREQ);

	if (viopath_hostLp == HvLpIndexInvalid) {
		vio_set_hostlp();
		if (viopath_hostLp == HvLpIndexInvalid) {
			ret = -ENODEV;
			goto clear_op;
		}
	}

	ret = viopath_open(viopath_hostLp, viomajorsubtype_tape,
			VIOTAPE_MAXREQ + 2);
	if (ret) {
		printk(VIOTAPE_KERN_WARN
				"error on viopath_open to hostlp %d\n", ret);
		ret = -EIO;
		goto clear_op;
	}

	printk(VIOTAPE_KERN_INFO "vers " VIOTAPE_VERSION
			", hosting partition %d\n", viopath_hostLp);

	vio_setHandler(viomajorsubtype_tape, vioHandleTapeEvent);

	ret = register_chrdev(VIOTAPE_MAJOR, "viotape", &viotap_fops);
	if (ret < 0) {
		printk(VIOTAPE_KERN_WARN "Error registering viotape device\n");
		goto clear_handler;
	}

	tape_class = class_create(THIS_MODULE, "tape");
	if (IS_ERR(tape_class)) {
		printk(VIOTAPE_KERN_WARN "Unable to allocat class\n");
		ret = PTR_ERR(tape_class);
		goto unreg_chrdev;
	}

	ret = vio_register_driver(&viotape_driver);
	if (ret)
		goto unreg_class;

	proc_create("iSeries/viotape", S_IFREG|S_IRUGO, NULL,
		    &proc_viotape_operations);

	return 0;

unreg_class:
	class_destroy(tape_class);
unreg_chrdev:
	unregister_chrdev(VIOTAPE_MAJOR, "viotape");
clear_handler:
	vio_clearHandler(viomajorsubtype_tape);
	viopath_close(viopath_hostLp, viomajorsubtype_tape, VIOTAPE_MAXREQ + 2);
clear_op:
	clear_op_struct_pool();
	return ret;
}

/* Give a new state to the tape object */
static int chg_state(int index, unsigned char new_state, struct file *file)
{
	unsigned char *cur_state =
	    &state[index].part_stat_rwi[state[index].cur_part];
	int rc = 0;

	/* if the same state, don't bother */
	if (*cur_state == new_state)
		return 0;

	/* write an EOF if changing from writing to some other state */
	if (*cur_state == VIOT_WRITING) {
		struct mtop write_eof = { MTWEOF, 1 };

		rc = viotap_ioctl(NULL, file, MTIOCTOP,
				  (unsigned long)&write_eof);
	}
	*cur_state = new_state;
	return rc;
}

/* Cleanup */
static void __exit viotap_exit(void)
{
	remove_proc_entry("iSeries/viotape", NULL);
	vio_unregister_driver(&viotape_driver);
	class_destroy(tape_class);
	unregister_chrdev(VIOTAPE_MAJOR, "viotape");
	viopath_close(viopath_hostLp, viomajorsubtype_tape, VIOTAPE_MAXREQ + 2);
	vio_clearHandler(viomajorsubtype_tape);
	clear_op_struct_pool();
}

MODULE_LICENSE("GPL");
module_init(viotap_init);
module_exit(viotap_exit);
