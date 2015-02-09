/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 *
 * Linux driver for Brocade Fibre Channel Host Bus Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/debugfs.h>

#include <bfad_drv.h>
#include <bfad_im.h>

/*
 * BFA debufs interface
 *
 * To access the interface, debugfs file system should be mounted
 * if not already mounted using:
 * mount -t debugfs none /sys/kernel/debug
 *
 * BFA Hierarchy:
 * 	- bfa/host#
 * where the host number corresponds to the one under /sys/class/scsi_host/host#
 *
 * Debugging service available per host:
 * fwtrc:  To collect current firmware trace.
 * drvtrc: To collect current driver trace
 * fwsave: To collect last saved fw trace as a result of firmware crash.
 * regwr:  To write one word to chip register
 * regrd:  To read one or more words from chip register.
 */

struct bfad_debug_info {
	char *debug_buffer;
	void *i_private;
	int buffer_len;
};

static int
bfad_debugfs_open_drvtrc(struct inode *inode, struct file *file)
{
	struct bfad_port_s *port = inode->i_private;
	struct bfad_s *bfad = port->bfad;
	struct bfad_debug_info *debug;

	debug = kzalloc(sizeof(struct bfad_debug_info), GFP_KERNEL);
	if (!debug)
		return -ENOMEM;

	debug->debug_buffer = (void *) bfad->trcmod;
	debug->buffer_len = sizeof(struct bfa_trc_mod_s);

	file->private_data = debug;

	return 0;
}

static int
bfad_debugfs_open_fwtrc(struct inode *inode, struct file *file)
{
	struct bfad_port_s *port = inode->i_private;
	struct bfad_s *bfad = port->bfad;
	struct bfad_debug_info *fw_debug;
	unsigned long flags;
	int rc;

	fw_debug = kzalloc(sizeof(struct bfad_debug_info), GFP_KERNEL);
	if (!fw_debug)
		return -ENOMEM;

	fw_debug->buffer_len = sizeof(struct bfa_trc_mod_s);

	fw_debug->debug_buffer = vmalloc(fw_debug->buffer_len);
	if (!fw_debug->debug_buffer) {
		kfree(fw_debug);
		printk(KERN_INFO "bfad[%d]: Failed to allocate fwtrc buffer\n",
				bfad->inst_no);
		return -ENOMEM;
	}

	memset(fw_debug->debug_buffer, 0, fw_debug->buffer_len);

	spin_lock_irqsave(&bfad->bfad_lock, flags);
	rc = bfa_debug_fwtrc(&bfad->bfa,
			fw_debug->debug_buffer,
			&fw_debug->buffer_len);
	spin_unlock_irqrestore(&bfad->bfad_lock, flags);
	if (rc != BFA_STATUS_OK) {
		vfree(fw_debug->debug_buffer);
		fw_debug->debug_buffer = NULL;
		kfree(fw_debug);
		printk(KERN_INFO "bfad[%d]: Failed to collect fwtrc\n",
				bfad->inst_no);
		return -ENOMEM;
	}

	file->private_data = fw_debug;

	return 0;
}

static int
bfad_debugfs_open_fwsave(struct inode *inode, struct file *file)
{
	struct bfad_port_s *port = inode->i_private;
	struct bfad_s *bfad = port->bfad;
	struct bfad_debug_info *fw_debug;
	unsigned long flags;
	int rc;

	fw_debug = kzalloc(sizeof(struct bfad_debug_info), GFP_KERNEL);
	if (!fw_debug)
		return -ENOMEM;

	fw_debug->buffer_len = sizeof(struct bfa_trc_mod_s);

	fw_debug->debug_buffer = vmalloc(fw_debug->buffer_len);
	if (!fw_debug->debug_buffer) {
		kfree(fw_debug);
		printk(KERN_INFO "bfad[%d]: Failed to allocate fwsave buffer\n",
				bfad->inst_no);
		return -ENOMEM;
	}

	memset(fw_debug->debug_buffer, 0, fw_debug->buffer_len);

	spin_lock_irqsave(&bfad->bfad_lock, flags);
	rc = bfa_debug_fwsave(&bfad->bfa,
			fw_debug->debug_buffer,
			&fw_debug->buffer_len);
	spin_unlock_irqrestore(&bfad->bfad_lock, flags);
	if (rc != BFA_STATUS_OK) {
		vfree(fw_debug->debug_buffer);
		fw_debug->debug_buffer = NULL;
		kfree(fw_debug);
		printk(KERN_INFO "bfad[%d]: Failed to collect fwsave\n",
				bfad->inst_no);
		return -ENOMEM;
	}

	file->private_data = fw_debug;

	return 0;
}

static int
bfad_debugfs_open_reg(struct inode *inode, struct file *file)
{
	struct bfad_debug_info *reg_debug;

	reg_debug = kzalloc(sizeof(struct bfad_debug_info), GFP_KERNEL);
	if (!reg_debug)
		return -ENOMEM;

	reg_debug->i_private = inode->i_private;

	file->private_data = reg_debug;

	return 0;
}

/* Changes the current file position */
static loff_t
bfad_debugfs_lseek(struct file *file, loff_t offset, int orig)
{
	struct bfad_debug_info *debug;
	loff_t pos = file->f_pos;

	debug = file->private_data;

	switch (orig) {
	case 0:
		file->f_pos = offset;
		break;
	case 1:
		file->f_pos += offset;
		break;
	case 2:
		file->f_pos = debug->buffer_len - offset;
		break;
	default:
		return -EINVAL;
	}

	if (file->f_pos < 0 || file->f_pos > debug->buffer_len) {
		file->f_pos = pos;
		return -EINVAL;
	}

	return file->f_pos;
}

static ssize_t
bfad_debugfs_read(struct file *file, char __user *buf,
			size_t nbytes, loff_t *pos)
{
	struct bfad_debug_info *debug = file->private_data;

	if (!debug || !debug->debug_buffer)
		return 0;

	return memory_read_from_buffer(buf, nbytes, pos,
				debug->debug_buffer, debug->buffer_len);
}

#define BFA_REG_CT_ADDRSZ	(0x40000)
#define BFA_REG_CB_ADDRSZ	(0x20000)
#define BFA_REG_ADDRSZ(__bfa)	\
	((bfa_ioc_devid(&(__bfa)->ioc) == BFA_PCI_DEVICE_ID_CT) ?	\
		BFA_REG_CT_ADDRSZ : BFA_REG_CB_ADDRSZ)
#define BFA_REG_ADDRMSK(__bfa)  ((uint32_t)(BFA_REG_ADDRSZ(__bfa) - 1))

static bfa_status_t
bfad_reg_offset_check(struct bfa_s *bfa, u32 offset, u32 len)
{
	u8	area;

	/* check [16:15] */
	area = (offset >> 15) & 0x7;
	if (area == 0) {
		/* PCIe core register */
		if ((offset + (len<<2)) > 0x8000)    /* 8k dwords or 32KB */
			return BFA_STATUS_EINVAL;
	} else if (area == 0x1) {
		/* CB 32 KB memory page */
		if ((offset + (len<<2)) > 0x10000)    /* 8k dwords or 32KB */
			return BFA_STATUS_EINVAL;
	} else {
		/* CB register space 64KB */
		if ((offset + (len<<2)) > BFA_REG_ADDRMSK(bfa))
			return BFA_STATUS_EINVAL;
	}
	return BFA_STATUS_OK;
}

static ssize_t
bfad_debugfs_read_regrd(struct file *file, char __user *buf,
		size_t nbytes, loff_t *pos)
{
	struct bfad_debug_info *regrd_debug = file->private_data;
	struct bfad_port_s *port = (struct bfad_port_s *)regrd_debug->i_private;
	struct bfad_s *bfad = port->bfad;
	ssize_t rc;

	if (!bfad->regdata)
		return 0;

	rc = memory_read_from_buffer(buf, nbytes, pos,
			bfad->regdata, bfad->reglen);

	if ((*pos + nbytes) >= bfad->reglen) {
		kfree(bfad->regdata);
		bfad->regdata = NULL;
		bfad->reglen = 0;
	}

	return rc;
}

static ssize_t
bfad_debugfs_write_regrd(struct file *file, const char __user *buf,
		size_t nbytes, loff_t *ppos)
{
	struct bfad_debug_info *regrd_debug = file->private_data;
	struct bfad_port_s *port = (struct bfad_port_s *)regrd_debug->i_private;
	struct bfad_s *bfad = port->bfad;
	struct bfa_s *bfa = &bfad->bfa;
	struct bfa_ioc_s *ioc = &bfa->ioc;
	int addr, len, rc, i;
	u32 *regbuf;
	void __iomem *rb, *reg_addr;
	unsigned long flags;

	rc = sscanf(buf, "%x:%x", &addr, &len);
	if (rc < 2) {
		printk(KERN_INFO
			"bfad[%d]: %s failed to read user buf\n",
			bfad->inst_no, __func__);
		return -EINVAL;
	}

	kfree(bfad->regdata);
	bfad->regdata = NULL;
	bfad->reglen = 0;

	bfad->regdata = kzalloc(len << 2, GFP_KERNEL);
	if (!bfad->regdata) {
		printk(KERN_INFO "bfad[%d]: Failed to allocate regrd buffer\n",
				bfad->inst_no);
		return -ENOMEM;
	}

	bfad->reglen = len << 2;
	rb = bfa_ioc_bar0(ioc);
	addr &= BFA_REG_ADDRMSK(bfa);

	/* offset and len sanity check */
	rc = bfad_reg_offset_check(bfa, addr, len);
	if (rc) {
		printk(KERN_INFO "bfad[%d]: Failed reg offset check\n",
				bfad->inst_no);
		kfree(bfad->regdata);
		bfad->regdata = NULL;
		bfad->reglen = 0;
		return -EINVAL;
	}

	reg_addr = rb + addr;
	regbuf =  (u32 *)bfad->regdata;
	spin_lock_irqsave(&bfad->bfad_lock, flags);
	for (i = 0; i < len; i++) {
		*regbuf = bfa_reg_read(reg_addr);
		regbuf++;
		reg_addr += sizeof(u32);
	}
	spin_unlock_irqrestore(&bfad->bfad_lock, flags);

	return nbytes;
}

static ssize_t
bfad_debugfs_write_regwr(struct file *file, const char __user *buf,
		size_t nbytes, loff_t *ppos)
{
	struct bfad_debug_info *debug = file->private_data;
	struct bfad_port_s *port = (struct bfad_port_s *)debug->i_private;
	struct bfad_s *bfad = port->bfad;
	struct bfa_s *bfa = &bfad->bfa;
	struct bfa_ioc_s *ioc = &bfa->ioc;
	int addr, val, rc;
	void __iomem *reg_addr;
	unsigned long flags;

	rc = sscanf(buf, "%x:%x", &addr, &val);
	if (rc < 2) {
		printk(KERN_INFO
			"bfad[%d]: %s failed to read user buf\n",
			bfad->inst_no, __func__);
		return -EINVAL;
	}

	addr &= BFA_REG_ADDRMSK(bfa); /* offset only 17 bit and word align */

	/* offset and len sanity check */
	rc = bfad_reg_offset_check(bfa, addr, 1);
	if (rc) {
		printk(KERN_INFO
			"bfad[%d]: Failed reg offset check\n",
			bfad->inst_no);
		return -EINVAL;
	}

	reg_addr = (uint32_t *) ((uint8_t *) bfa_ioc_bar0(ioc) + addr);
	spin_lock_irqsave(&bfad->bfad_lock, flags);
	bfa_reg_write(reg_addr, val);
	spin_unlock_irqrestore(&bfad->bfad_lock, flags);

	return nbytes;
}

static int
bfad_debugfs_release(struct inode *inode, struct file *file)
{
	struct bfad_debug_info *debug = file->private_data;

	if (!debug)
		return 0;

	file->private_data = NULL;
	kfree(debug);
	return 0;
}

static int
bfad_debugfs_release_fwtrc(struct inode *inode, struct file *file)
{
	struct bfad_debug_info *fw_debug = file->private_data;

	if (!fw_debug)
		return 0;

	if (fw_debug->debug_buffer)
		vfree(fw_debug->debug_buffer);

	file->private_data = NULL;
	kfree(fw_debug);
	return 0;
}

static const struct file_operations bfad_debugfs_op_drvtrc = {
	.owner		=	THIS_MODULE,
	.open		=	bfad_debugfs_open_drvtrc,
	.llseek		=	bfad_debugfs_lseek,
	.read		=	bfad_debugfs_read,
	.release	=	bfad_debugfs_release,
};

static const struct file_operations bfad_debugfs_op_fwtrc = {
	.owner		=	THIS_MODULE,
	.open		=	bfad_debugfs_open_fwtrc,
	.llseek		=	bfad_debugfs_lseek,
	.read		=	bfad_debugfs_read,
	.release	=	bfad_debugfs_release_fwtrc,
};

static const struct file_operations bfad_debugfs_op_fwsave = {
	.owner		=	THIS_MODULE,
	.open		=	bfad_debugfs_open_fwsave,
	.llseek		=	bfad_debugfs_lseek,
	.read		=	bfad_debugfs_read,
	.release	=	bfad_debugfs_release_fwtrc,
};

static const struct file_operations bfad_debugfs_op_regrd = {
	.owner		=	THIS_MODULE,
	.open		=	bfad_debugfs_open_reg,
	.llseek		=	bfad_debugfs_lseek,
	.read		=	bfad_debugfs_read_regrd,
	.write		=	bfad_debugfs_write_regrd,
	.release	=	bfad_debugfs_release,
};

static const struct file_operations bfad_debugfs_op_regwr = {
	.owner		=	THIS_MODULE,
	.open		=	bfad_debugfs_open_reg,
	.llseek		=	bfad_debugfs_lseek,
	.write		=	bfad_debugfs_write_regwr,
	.release	=	bfad_debugfs_release,
};

struct bfad_debugfs_entry {
	const char *name;
	mode_t	mode;
	const struct file_operations *fops;
};

static const struct bfad_debugfs_entry bfad_debugfs_files[] = {
	{ "drvtrc", S_IFREG|S_IRUGO, &bfad_debugfs_op_drvtrc, },
	{ "fwtrc",  S_IFREG|S_IRUGO, &bfad_debugfs_op_fwtrc,  },
	{ "fwsave", S_IFREG|S_IRUGO, &bfad_debugfs_op_fwsave, },
	{ "regrd",  S_IFREG|S_IRUGO|S_IWUSR, &bfad_debugfs_op_regrd,  },
	{ "regwr",  S_IFREG|S_IWUSR, &bfad_debugfs_op_regwr,  },
};

static struct dentry *bfa_debugfs_root;
static atomic_t bfa_debugfs_port_count;

inline void
bfad_debugfs_init(struct bfad_port_s *port)
{
	struct bfad_im_port_s *im_port = port->im_port;
	struct bfad_s *bfad = im_port->bfad;
	struct Scsi_Host *shost = im_port->shost;
	const struct bfad_debugfs_entry *file;
	char name[16];
	int i;

	if (!bfa_debugfs_enable)
		return;

	/* Setup the BFA debugfs root directory*/
	if (!bfa_debugfs_root) {
		bfa_debugfs_root = debugfs_create_dir("bfa", NULL);
		atomic_set(&bfa_debugfs_port_count, 0);
		if (!bfa_debugfs_root) {
			printk(KERN_WARNING
				"BFA debugfs root dir creation failed\n");
			goto err;
		}
	}

	/*
	 * Setup the host# directory for the port,
	 * corresponds to the scsi_host num of this port.
	 */
	snprintf(name, sizeof(name), "host%d", shost->host_no);
	if (!port->port_debugfs_root) {
		port->port_debugfs_root =
			debugfs_create_dir(name, bfa_debugfs_root);
		if (!port->port_debugfs_root) {
			printk(KERN_WARNING
				"BFA host root dir creation failed\n");
			goto err;
		}

		atomic_inc(&bfa_debugfs_port_count);

		for (i = 0; i < ARRAY_SIZE(bfad_debugfs_files); i++) {
			file = &bfad_debugfs_files[i];
			bfad->bfad_dentry_files[i] =
					debugfs_create_file(file->name,
							file->mode,
							port->port_debugfs_root,
							port,
							file->fops);
			if (!bfad->bfad_dentry_files[i]) {
				printk(KERN_WARNING
					"BFA host%d: create %s entry failed\n",
					shost->host_no, file->name);
				goto err;
			}
		}
	}

err:
	return;
}

inline void
bfad_debugfs_exit(struct bfad_port_s *port)
{
	struct bfad_im_port_s *im_port = port->im_port;
	struct bfad_s *bfad = im_port->bfad;
	int i;

	for (i = 0; i < ARRAY_SIZE(bfad_debugfs_files); i++) {
		if (bfad->bfad_dentry_files[i]) {
			debugfs_remove(bfad->bfad_dentry_files[i]);
			bfad->bfad_dentry_files[i] = NULL;
		}
	}

	/*
	 * Remove the host# directory for the port,
	 * corresponds to the scsi_host num of this port.
	*/
	if (port->port_debugfs_root) {
		debugfs_remove(port->port_debugfs_root);
		port->port_debugfs_root = NULL;
		atomic_dec(&bfa_debugfs_port_count);
	}

	/* Remove the BFA debugfs root directory */
	if (atomic_read(&bfa_debugfs_port_count) == 0) {
		debugfs_remove(bfa_debugfs_root);
		bfa_debugfs_root = NULL;
	}
}
