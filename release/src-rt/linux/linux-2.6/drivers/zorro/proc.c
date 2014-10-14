/*
 *	Procfs interface for the Zorro bus.
 *
 *	Copyright (C) 1998-2003 Geert Uytterhoeven
 *
 *	Heavily based on the procfs interface for the PCI bus, which is
 *
 *	Copyright (C) 1997, 1998 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#include <linux/types.h>
#include <linux/zorro.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/amigahw.h>
#include <asm/setup.h>

static loff_t
proc_bus_zorro_lseek(struct file *file, loff_t off, int whence)
{
	loff_t new = -1;
	struct inode *inode = file->f_path.dentry->d_inode;

	mutex_lock(&inode->i_mutex);
	switch (whence) {
	case 0:
		new = off;
		break;
	case 1:
		new = file->f_pos + off;
		break;
	case 2:
		new = sizeof(struct ConfigDev) + off;
		break;
	}
	if (new < 0 || new > sizeof(struct ConfigDev))
		new = -EINVAL;
	else
		file->f_pos = new;
	mutex_unlock(&inode->i_mutex);
	return new;
}

static ssize_t
proc_bus_zorro_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
	struct inode *ino = file->f_path.dentry->d_inode;
	struct proc_dir_entry *dp = PDE(ino);
	struct zorro_dev *z = dp->data;
	struct ConfigDev cd;
	loff_t pos = *ppos;

	if (pos >= sizeof(struct ConfigDev))
		return 0;
	if (nbytes >= sizeof(struct ConfigDev))
		nbytes = sizeof(struct ConfigDev);
	if (pos + nbytes > sizeof(struct ConfigDev))
		nbytes = sizeof(struct ConfigDev) - pos;

	/* Construct a ConfigDev */
	memset(&cd, 0, sizeof(cd));
	cd.cd_Rom = z->rom;
	cd.cd_SlotAddr = z->slotaddr;
	cd.cd_SlotSize = z->slotsize;
	cd.cd_BoardAddr = (void *)zorro_resource_start(z);
	cd.cd_BoardSize = zorro_resource_len(z);

	if (copy_to_user(buf, (void *)&cd + pos, nbytes))
		return -EFAULT;
	*ppos += nbytes;

	return nbytes;
}

static const struct file_operations proc_bus_zorro_operations = {
	.owner		= THIS_MODULE,
	.llseek		= proc_bus_zorro_lseek,
	.read		= proc_bus_zorro_read,
};

static void * zorro_seq_start(struct seq_file *m, loff_t *pos)
{
	return (*pos < zorro_num_autocon) ? pos : NULL;
}

static void * zorro_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	(*pos)++;
	return (*pos < zorro_num_autocon) ? pos : NULL;
}

static void zorro_seq_stop(struct seq_file *m, void *v)
{
}

static int zorro_seq_show(struct seq_file *m, void *v)
{
	unsigned int slot = *(loff_t *)v;
	struct zorro_dev *z = &zorro_autocon[slot];

	seq_printf(m, "%02x\t%08x\t%08lx\t%08lx\t%02x\n", slot, z->id,
		   (unsigned long)zorro_resource_start(z),
		   (unsigned long)zorro_resource_len(z),
		   z->rom.er_Type);
	return 0;
}

static const struct seq_operations zorro_devices_seq_ops = {
	.start = zorro_seq_start,
	.next  = zorro_seq_next,
	.stop  = zorro_seq_stop,
	.show  = zorro_seq_show,
};

static int zorro_devices_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &zorro_devices_seq_ops);
}

static const struct file_operations zorro_devices_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zorro_devices_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static struct proc_dir_entry *proc_bus_zorro_dir;

static int __init zorro_proc_attach_device(unsigned int slot)
{
	struct proc_dir_entry *entry;
	char name[4];

	sprintf(name, "%02x", slot);
	entry = proc_create_data(name, 0, proc_bus_zorro_dir,
				 &proc_bus_zorro_operations,
				 &zorro_autocon[slot]);
	if (!entry)
		return -ENOMEM;
	entry->size = sizeof(struct zorro_dev);
	return 0;
}

static int __init zorro_proc_init(void)
{
	unsigned int slot;

	if (MACH_IS_AMIGA && AMIGAHW_PRESENT(ZORRO)) {
		proc_bus_zorro_dir = proc_mkdir("bus/zorro", NULL);
		proc_create("devices", 0, proc_bus_zorro_dir,
			    &zorro_devices_proc_fops);
		for (slot = 0; slot < zorro_num_autocon; slot++)
			zorro_proc_attach_device(slot);
	}
	return 0;
}

device_initcall(zorro_proc_init);
