#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/netdevice.h>

#include "bonding.h"
#include "bond_alb.h"

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static struct dentry *bonding_debug_root;

/*
 *  Show RLB hash table
 */
static int bond_debug_rlb_hash_show(struct seq_file *m, void *v)
{
	struct bonding *bond = m->private;
	struct alb_bond_info *bond_info = &(BOND_ALB_INFO(bond));
	struct rlb_client_info *client_info;
	u32 hash_index;

	if (bond->params.mode != BOND_MODE_ALB)
		return 0;

	seq_printf(m, "SourceIP        DestinationIP   "
			"Destination MAC   DEV\n");

	spin_lock_bh(&(BOND_ALB_INFO(bond).rx_hashtbl_lock));

	hash_index = bond_info->rx_hashtbl_head;
	for (; hash_index != RLB_NULL_INDEX; hash_index = client_info->next) {
		client_info = &(bond_info->rx_hashtbl[hash_index]);
		seq_printf(m, "%-15pI4 %-15pI4 %-17pM %s\n",
			&client_info->ip_src,
			&client_info->ip_dst,
			&client_info->mac_dst,
			client_info->slave->dev->name);
	}

	spin_unlock_bh(&(BOND_ALB_INFO(bond).rx_hashtbl_lock));

	return 0;
}

static int bond_debug_rlb_hash_open(struct inode *inode, struct file *file)
{
	return single_open(file, bond_debug_rlb_hash_show, inode->i_private);
}

static const struct file_operations bond_debug_rlb_hash_fops = {
	.owner		= THIS_MODULE,
	.open		= bond_debug_rlb_hash_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void bond_debug_register(struct bonding *bond)
{
	if (!bonding_debug_root)
		return;

	bond->debug_dir =
		debugfs_create_dir(bond->dev->name, bonding_debug_root);

	if (!bond->debug_dir) {
		pr_warning("%s: Warning: failed to register to debugfs\n",
			bond->dev->name);
		return;
	}

	debugfs_create_file("rlb_hash_table", 0400, bond->debug_dir,
				bond, &bond_debug_rlb_hash_fops);
}

void bond_debug_unregister(struct bonding *bond)
{
	if (!bonding_debug_root)
		return;

	debugfs_remove_recursive(bond->debug_dir);
}

void bond_debug_reregister(struct bonding *bond)
{
	struct dentry *d;

	if (!bonding_debug_root)
		return;

	d = debugfs_rename(bonding_debug_root, bond->debug_dir,
			   bonding_debug_root, bond->dev->name);
	if (d) {
		bond->debug_dir = d;
	} else {
		pr_warning("%s: Warning: failed to reregister, "
				"so just unregister old one\n",
				bond->dev->name);
		bond_debug_unregister(bond);
	}
}

void bond_create_debugfs(void)
{
	bonding_debug_root = debugfs_create_dir("bonding", NULL);

	if (!bonding_debug_root) {
		pr_warning("Warning: Cannot create bonding directory"
				" in debugfs\n");
	}
}

void bond_destroy_debugfs(void)
{
	debugfs_remove_recursive(bonding_debug_root);
	bonding_debug_root = NULL;
}


#else /* !CONFIG_DEBUG_FS */

void bond_debug_register(struct bonding *bond)
{
}

void bond_debug_unregister(struct bonding *bond)
{
}

void bond_debug_reregister(struct bonding *bond)
{
}

void bond_create_debugfs(void)
{
}

void bond_destroy_debugfs(void)
{
}

#endif /* CONFIG_DEBUG_FS */
