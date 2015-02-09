/*
 * Architecture specific debugfs files
 *
 * Copyright (C) 2007, Intel Corp.
 *	Huang Ying <ying.huang@intel.com>
 *
 * This file is released under the GPLv2.
 */
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/io.h>
#include <linux/mm.h>

#include <asm/setup.h>

struct dentry *arch_debugfs_dir;
EXPORT_SYMBOL(arch_debugfs_dir);

#ifdef CONFIG_DEBUG_BOOT_PARAMS
struct setup_data_node {
	u64 paddr;
	u32 type;
	u32 len;
};

static ssize_t setup_data_read(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	struct setup_data_node *node = file->private_data;
	unsigned long remain;
	loff_t pos = *ppos;
	struct page *pg;
	void *p;
	u64 pa;

	if (pos < 0)
		return -EINVAL;

	if (pos >= node->len)
		return 0;

	if (count > node->len - pos)
		count = node->len - pos;

	pa = node->paddr + sizeof(struct setup_data) + pos;
	pg = pfn_to_page((pa + count - 1) >> PAGE_SHIFT);
	if (PageHighMem(pg)) {
		p = ioremap_cache(pa, count);
		if (!p)
			return -ENXIO;
	} else
		p = __va(pa);

	remain = copy_to_user(user_buf, p, count);

	if (PageHighMem(pg))
		iounmap(p);

	if (remain)
		return -EFAULT;

	*ppos = pos + count;

	return count;
}

static int setup_data_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static const struct file_operations fops_setup_data = {
	.read		= setup_data_read,
	.open		= setup_data_open,
};

static int __init
create_setup_data_node(struct dentry *parent, int no,
		       struct setup_data_node *node)
{
	struct dentry *d, *type, *data;
	char buf[16];

	sprintf(buf, "%d", no);
	d = debugfs_create_dir(buf, parent);
	if (!d)
		return -ENOMEM;

	type = debugfs_create_x32("type", S_IRUGO, d, &node->type);
	if (!type)
		goto err_dir;

	data = debugfs_create_file("data", S_IRUGO, d, node, &fops_setup_data);
	if (!data)
		goto err_type;

	return 0;

err_type:
	debugfs_remove(type);
err_dir:
	debugfs_remove(d);
	return -ENOMEM;
}

static int __init create_setup_data_nodes(struct dentry *parent)
{
	struct setup_data_node *node;
	struct setup_data *data;
	int error = -ENOMEM;
	struct dentry *d;
	struct page *pg;
	u64 pa_data;
	int no = 0;

	d = debugfs_create_dir("setup_data", parent);
	if (!d)
		return -ENOMEM;

	pa_data = boot_params.hdr.setup_data;

	while (pa_data) {
		node = kmalloc(sizeof(*node), GFP_KERNEL);
		if (!node)
			goto err_dir;

		pg = pfn_to_page((pa_data+sizeof(*data)-1) >> PAGE_SHIFT);
		if (PageHighMem(pg)) {
			data = ioremap_cache(pa_data, sizeof(*data));
			if (!data) {
				kfree(node);
				error = -ENXIO;
				goto err_dir;
			}
		} else
			data = __va(pa_data);

		node->paddr = pa_data;
		node->type = data->type;
		node->len = data->len;
		error = create_setup_data_node(d, no, node);
		pa_data = data->next;

		if (PageHighMem(pg))
			iounmap(data);
		if (error)
			goto err_dir;
		no++;
	}

	return 0;

err_dir:
	debugfs_remove(d);
	return error;
}

static struct debugfs_blob_wrapper boot_params_blob = {
	.data		= &boot_params,
	.size		= sizeof(boot_params),
};

static int __init boot_params_kdebugfs_init(void)
{
	struct dentry *dbp, *version, *data;
	int error = -ENOMEM;

	dbp = debugfs_create_dir("boot_params", NULL);
	if (!dbp)
		return -ENOMEM;

	version = debugfs_create_x16("version", S_IRUGO, dbp,
				     &boot_params.hdr.version);
	if (!version)
		goto err_dir;

	data = debugfs_create_blob("data", S_IRUGO, dbp,
				   &boot_params_blob);
	if (!data)
		goto err_version;

	error = create_setup_data_nodes(dbp);
	if (error)
		goto err_data;

	return 0;

err_data:
	debugfs_remove(data);
err_version:
	debugfs_remove(version);
err_dir:
	debugfs_remove(dbp);
	return error;
}
#endif /* CONFIG_DEBUG_BOOT_PARAMS */

static int __init arch_kdebugfs_init(void)
{
	int error = 0;

	arch_debugfs_dir = debugfs_create_dir("x86", NULL);
	if (!arch_debugfs_dir)
		return -ENOMEM;

#ifdef CONFIG_DEBUG_BOOT_PARAMS
	error = boot_params_kdebugfs_init();
#endif

	return error;
}
arch_initcall(arch_kdebugfs_init);
