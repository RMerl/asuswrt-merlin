/*
 * linux/drivers/scsi/scsi_proc.c
 *
 * The functions in this file provide an interface between
 * the PROC file system and the SCSI device drivers
 * It is mainly used for debugging, statistics and to pass 
 * information directly to the lowlevel driver.
 *
 * (c) 1995 Michael Neuffer neuffer@goofy.zdv.uni-mainz.de 
 * Version: 0.99.8   last change: 95/09/13
 * 
 * generic command parser provided by: 
 * Andreas Heilwagen <crashcar@informatik.uni-koblenz.de>
 *
 * generic_proc_info() support of xxxx_info() by:
 * Michael A. Griffith <grif@acm.org>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/blkdev.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/gfp.h>
#include <asm/uaccess.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport.h>

#include "scsi_priv.h"
#include "scsi_logging.h"


/* 4K page size, but our output routines, use some slack for overruns */
#define PROC_BLOCK_SIZE (3*1024)

static struct proc_dir_entry *proc_scsi;

/* Protect sht->present and sht->proc_dir */
static DEFINE_MUTEX(global_host_template_mutex);

/**
 * proc_scsi_read - handle read from /proc by calling host's proc_info() command
 * @buffer: passed to proc_info
 * @start: passed to proc_info
 * @offset: passed to proc_info
 * @length: passed to proc_info
 * @eof: returns whether length read was less than requested
 * @data: pointer to a &struct Scsi_Host
 */

static int proc_scsi_read(char *buffer, char **start, off_t offset,
			  int length, int *eof, void *data)
{
	struct Scsi_Host *shost = data;
	int n;

	n = shost->hostt->proc_info(shost, buffer, start, offset, length, 0);
	*eof = (n < length);

	return n;
}

/**
 * proc_scsi_write_proc - Handle write to /proc by calling host's proc_info()
 * @file: not used
 * @buf: source of data to write.
 * @count: number of bytes (at most PROC_BLOCK_SIZE) to write.
 * @data: pointer to &struct Scsi_Host
 */
static int proc_scsi_write_proc(struct file *file, const char __user *buf,
                           unsigned long count, void *data)
{
	struct Scsi_Host *shost = data;
	ssize_t ret = -ENOMEM;
	char *page;
	char *start;
    
	if (count > PROC_BLOCK_SIZE)
		return -EOVERFLOW;

	page = (char *)__get_free_page(GFP_KERNEL);
	if (page) {
		ret = -EFAULT;
		if (copy_from_user(page, buf, count))
			goto out;
		ret = shost->hostt->proc_info(shost, page, &start, 0, count, 1);
	}
out:
	free_page((unsigned long)page);
	return ret;
}

/**
 * scsi_proc_hostdir_add - Create directory in /proc for a scsi host
 * @sht: owner of this directory
 *
 * Sets sht->proc_dir to the new directory.
 */

void scsi_proc_hostdir_add(struct scsi_host_template *sht)
{
	if (!sht->proc_info)
		return;

	mutex_lock(&global_host_template_mutex);
	if (!sht->present++) {
		sht->proc_dir = proc_mkdir(sht->proc_name, proc_scsi);
        	if (!sht->proc_dir)
			printk(KERN_ERR "%s: proc_mkdir failed for %s\n",
			       __func__, sht->proc_name);
	}
	mutex_unlock(&global_host_template_mutex);
}

/**
 * scsi_proc_hostdir_rm - remove directory in /proc for a scsi host
 * @sht: owner of directory
 */
void scsi_proc_hostdir_rm(struct scsi_host_template *sht)
{
	if (!sht->proc_info)
		return;

	mutex_lock(&global_host_template_mutex);
	if (!--sht->present && sht->proc_dir) {
		remove_proc_entry(sht->proc_name, proc_scsi);
		sht->proc_dir = NULL;
	}
	mutex_unlock(&global_host_template_mutex);
}


/**
 * scsi_proc_host_add - Add entry for this host to appropriate /proc dir
 * @shost: host to add
 */
void scsi_proc_host_add(struct Scsi_Host *shost)
{
	struct scsi_host_template *sht = shost->hostt;
	struct proc_dir_entry *p;
	char name[10];

	if (!sht->proc_dir)
		return;

	sprintf(name,"%d", shost->host_no);
	p = create_proc_read_entry(name, S_IFREG | S_IRUGO | S_IWUSR,
			sht->proc_dir, proc_scsi_read, shost);
	if (!p) {
		printk(KERN_ERR "%s: Failed to register host %d in"
		       "%s\n", __func__, shost->host_no,
		       sht->proc_name);
		return;
	} 

	p->write_proc = proc_scsi_write_proc;
}

/**
 * scsi_proc_host_rm - remove this host's entry from /proc
 * @shost: which host
 */
void scsi_proc_host_rm(struct Scsi_Host *shost)
{
	char name[10];

	if (!shost->hostt->proc_dir)
		return;

	sprintf(name,"%d", shost->host_no);
	remove_proc_entry(name, shost->hostt->proc_dir);
}
/**
 * proc_print_scsidevice - return data about this host
 * @dev: A scsi device
 * @data: &struct seq_file to output to.
 *
 * Description: prints Host, Channel, Id, Lun, Vendor, Model, Rev, Type,
 * and revision.
 */
static int proc_print_scsidevice(struct device *dev, void *data)
{
	struct scsi_device *sdev;
	struct seq_file *s = data;
	int i;

	if (!scsi_is_sdev_device(dev))
		goto out;

	sdev = to_scsi_device(dev);
	seq_printf(s,
		"Host: scsi%d Channel: %02d Id: %02d Lun: %02d\n  Vendor: ",
		sdev->host->host_no, sdev->channel, sdev->id, sdev->lun);
	for (i = 0; i < 8; i++) {
		if (sdev->vendor[i] >= 0x20)
			seq_printf(s, "%c", sdev->vendor[i]);
		else
			seq_printf(s, " ");
	}

	seq_printf(s, " Model: ");
	for (i = 0; i < 16; i++) {
		if (sdev->model[i] >= 0x20)
			seq_printf(s, "%c", sdev->model[i]);
		else
			seq_printf(s, " ");
	}

	seq_printf(s, " Rev: ");
	for (i = 0; i < 4; i++) {
		if (sdev->rev[i] >= 0x20)
			seq_printf(s, "%c", sdev->rev[i]);
		else
			seq_printf(s, " ");
	}

	seq_printf(s, "\n");

	seq_printf(s, "  Type:   %s ", scsi_device_type(sdev->type));
	seq_printf(s, "               ANSI  SCSI revision: %02x",
			sdev->scsi_level - (sdev->scsi_level > 1));
	if (sdev->scsi_level == 2)
		seq_printf(s, " CCS\n");
	else
		seq_printf(s, "\n");

out:
	return 0;
}

/**
 * scsi_add_single_device - Respond to user request to probe for/add device
 * @host: user-supplied decimal integer
 * @channel: user-supplied decimal integer
 * @id: user-supplied decimal integer
 * @lun: user-supplied decimal integer
 *
 * Description: called by writing "scsi add-single-device" to /proc/scsi/scsi.
 *
 * does scsi_host_lookup() and either user_scan() if that transport
 * type supports it, or else scsi_scan_host_selected()
 *
 * Note: this seems to be aimed exclusively at SCSI parallel busses.
 */

static int scsi_add_single_device(uint host, uint channel, uint id, uint lun)
{
	struct Scsi_Host *shost;
	int error = -ENXIO;

	shost = scsi_host_lookup(host);
	if (!shost)
		return error;

	if (shost->transportt->user_scan)
		error = shost->transportt->user_scan(shost, channel, id, lun);
	else
		error = scsi_scan_host_selected(shost, channel, id, lun, 1);
	scsi_host_put(shost);
	return error;
}

/**
 * scsi_remove_single_device - Respond to user request to remove a device
 * @host: user-supplied decimal integer
 * @channel: user-supplied decimal integer
 * @id: user-supplied decimal integer
 * @lun: user-supplied decimal integer
 *
 * Description: called by writing "scsi remove-single-device" to
 * /proc/scsi/scsi.  Does a scsi_device_lookup() and scsi_remove_device()
 */
static int scsi_remove_single_device(uint host, uint channel, uint id, uint lun)
{
	struct scsi_device *sdev;
	struct Scsi_Host *shost;
	int error = -ENXIO;

	shost = scsi_host_lookup(host);
	if (!shost)
		return error;
	sdev = scsi_device_lookup(shost, channel, id, lun);
	if (sdev) {
		scsi_remove_device(sdev);
		scsi_device_put(sdev);
		error = 0;
	}

	scsi_host_put(shost);
	return error;
}

/**
 * proc_scsi_write - handle writes to /proc/scsi/scsi
 * @file: not used
 * @buf: buffer to write
 * @length: length of buf, at most PAGE_SIZE
 * @ppos: not used
 *
 * Description: this provides a legacy mechanism to add or remove devices by
 * Host, Channel, ID, and Lun.  To use,
 * "echo 'scsi add-single-device 0 1 2 3' > /proc/scsi/scsi" or
 * "echo 'scsi remove-single-device 0 1 2 3' > /proc/scsi/scsi" with
 * "0 1 2 3" replaced by the Host, Channel, Id, and Lun.
 *
 * Note: this seems to be aimed at parallel SCSI. Most modern busses (USB,
 * SATA, Firewire, Fibre Channel, etc) dynamically assign these values to
 * provide a unique identifier and nothing more.
 */


static ssize_t proc_scsi_write(struct file *file, const char __user *buf,
			       size_t length, loff_t *ppos)
{
	int host, channel, id, lun;
	char *buffer, *p;
	int err;

	if (!buf || length > PAGE_SIZE)
		return -EINVAL;

	buffer = (char *)__get_free_page(GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	err = -EFAULT;
	if (copy_from_user(buffer, buf, length))
		goto out;

	err = -EINVAL;
	if (length < PAGE_SIZE)
		buffer[length] = '\0';
	else if (buffer[PAGE_SIZE-1])
		goto out;

	/*
	 * Usage: echo "scsi add-single-device 0 1 2 3" >/proc/scsi/scsi
	 * with  "0 1 2 3" replaced by your "Host Channel Id Lun".
	 */
	if (!strncmp("scsi add-single-device", buffer, 22)) {
		p = buffer + 23;

		host = simple_strtoul(p, &p, 0);
		channel = simple_strtoul(p + 1, &p, 0);
		id = simple_strtoul(p + 1, &p, 0);
		lun = simple_strtoul(p + 1, &p, 0);

		err = scsi_add_single_device(host, channel, id, lun);

	/*
	 * Usage: echo "scsi remove-single-device 0 1 2 3" >/proc/scsi/scsi
	 * with  "0 1 2 3" replaced by your "Host Channel Id Lun".
	 */
	} else if (!strncmp("scsi remove-single-device", buffer, 25)) {
		p = buffer + 26;

		host = simple_strtoul(p, &p, 0);
		channel = simple_strtoul(p + 1, &p, 0);
		id = simple_strtoul(p + 1, &p, 0);
		lun = simple_strtoul(p + 1, &p, 0);

		err = scsi_remove_single_device(host, channel, id, lun);
	}

	/*
	 * convert success returns so that we return the 
	 * number of bytes consumed.
	 */
	if (!err)
		err = length;

 out:
	free_page((unsigned long)buffer);
	return err;
}

/**
 * proc_scsi_show - show contents of /proc/scsi/scsi (attached devices)
 * @s: output goes here
 * @p: not used
 */
static int proc_scsi_show(struct seq_file *s, void *p)
{
	seq_printf(s, "Attached devices:\n");
	bus_for_each_dev(&scsi_bus_type, NULL, s, proc_print_scsidevice);
	return 0;
}

/**
 * proc_scsi_open - glue function
 * @inode: not used
 * @file: passed to single_open()
 *
 * Associates proc_scsi_show with this file
 */
static int proc_scsi_open(struct inode *inode, struct file *file)
{
	/*
	 * We don't really need this for the write case but it doesn't
	 * harm either.
	 */
	return single_open(file, proc_scsi_show, NULL);
}

static const struct file_operations proc_scsi_operations = {
	.owner		= THIS_MODULE,
	.open		= proc_scsi_open,
	.read		= seq_read,
	.write		= proc_scsi_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/**
 * scsi_init_procfs - create scsi and scsi/scsi in procfs
 */
int __init scsi_init_procfs(void)
{
	struct proc_dir_entry *pde;

	proc_scsi = proc_mkdir("scsi", NULL);
	if (!proc_scsi)
		goto err1;

	pde = proc_create("scsi/scsi", 0, NULL, &proc_scsi_operations);
	if (!pde)
		goto err2;

	return 0;

err2:
	remove_proc_entry("scsi", NULL);
err1:
	return -ENOMEM;
}

/**
 * scsi_exit_procfs - Remove scsi/scsi and scsi from procfs
 */
void scsi_exit_procfs(void)
{
	remove_proc_entry("scsi/scsi", NULL);
	remove_proc_entry("scsi", NULL);
}
