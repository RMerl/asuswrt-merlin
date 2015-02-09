/*
 * Copyright (C) 2007-2010 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#include "main.h"
#include <linux/debugfs.h>
#include <linux/slab.h>
#include "icmp_socket.h"
#include "send.h"
#include "types.h"
#include "hash.h"
#include "hard-interface.h"


static struct socket_client *socket_client_hash[256];

static void bat_socket_add_packet(struct socket_client *socket_client,
				  struct icmp_packet_rr *icmp_packet,
				  size_t icmp_len);

void bat_socket_init(void)
{
	memset(socket_client_hash, 0, sizeof(socket_client_hash));
}

static int bat_socket_open(struct inode *inode, struct file *file)
{
	unsigned int i;
	struct socket_client *socket_client;

	socket_client = kmalloc(sizeof(struct socket_client), GFP_KERNEL);

	if (!socket_client)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(socket_client_hash); i++) {
		if (!socket_client_hash[i]) {
			socket_client_hash[i] = socket_client;
			break;
		}
	}

	if (i == ARRAY_SIZE(socket_client_hash)) {
		pr_err("Error - can't add another packet client: "
		       "maximum number of clients reached\n");
		kfree(socket_client);
		return -EXFULL;
	}

	INIT_LIST_HEAD(&socket_client->queue_list);
	socket_client->queue_len = 0;
	socket_client->index = i;
	socket_client->bat_priv = inode->i_private;
	spin_lock_init(&socket_client->lock);
	init_waitqueue_head(&socket_client->queue_wait);

	file->private_data = socket_client;

	inc_module_count();
	return 0;
}

static int bat_socket_release(struct inode *inode, struct file *file)
{
	struct socket_client *socket_client = file->private_data;
	struct socket_packet *socket_packet;
	struct list_head *list_pos, *list_pos_tmp;
	unsigned long flags;

	spin_lock_irqsave(&socket_client->lock, flags);

	/* for all packets in the queue ... */
	list_for_each_safe(list_pos, list_pos_tmp, &socket_client->queue_list) {
		socket_packet = list_entry(list_pos,
					   struct socket_packet, list);

		list_del(list_pos);
		kfree(socket_packet);
	}

	socket_client_hash[socket_client->index] = NULL;
	spin_unlock_irqrestore(&socket_client->lock, flags);

	kfree(socket_client);
	dec_module_count();

	return 0;
}

static ssize_t bat_socket_read(struct file *file, char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct socket_client *socket_client = file->private_data;
	struct socket_packet *socket_packet;
	size_t packet_len;
	int error;
	unsigned long flags;

	if ((file->f_flags & O_NONBLOCK) && (socket_client->queue_len == 0))
		return -EAGAIN;

	if ((!buf) || (count < sizeof(struct icmp_packet)))
		return -EINVAL;

	if (!access_ok(VERIFY_WRITE, buf, count))
		return -EFAULT;

	error = wait_event_interruptible(socket_client->queue_wait,
					 socket_client->queue_len);

	if (error)
		return error;

	spin_lock_irqsave(&socket_client->lock, flags);

	socket_packet = list_first_entry(&socket_client->queue_list,
					 struct socket_packet, list);
	list_del(&socket_packet->list);
	socket_client->queue_len--;

	spin_unlock_irqrestore(&socket_client->lock, flags);

	error = __copy_to_user(buf, &socket_packet->icmp_packet,
			       socket_packet->icmp_len);

	packet_len = socket_packet->icmp_len;
	kfree(socket_packet);

	if (error)
		return -EFAULT;

	return packet_len;
}

static ssize_t bat_socket_write(struct file *file, const char __user *buff,
				size_t len, loff_t *off)
{
	struct socket_client *socket_client = file->private_data;
	struct bat_priv *bat_priv = socket_client->bat_priv;
	struct icmp_packet_rr icmp_packet;
	struct orig_node *orig_node;
	struct batman_if *batman_if;
	size_t packet_len = sizeof(struct icmp_packet);
	uint8_t dstaddr[ETH_ALEN];
	unsigned long flags;

	if (len < sizeof(struct icmp_packet)) {
		bat_dbg(DBG_BATMAN, bat_priv,
			"Error - can't send packet from char device: "
			"invalid packet size\n");
		return -EINVAL;
	}

	if (!bat_priv->primary_if)
		return -EFAULT;

	if (len >= sizeof(struct icmp_packet_rr))
		packet_len = sizeof(struct icmp_packet_rr);

	if (!access_ok(VERIFY_READ, buff, packet_len))
		return -EFAULT;

	if (__copy_from_user(&icmp_packet, buff, packet_len))
		return -EFAULT;

	if (icmp_packet.packet_type != BAT_ICMP) {
		bat_dbg(DBG_BATMAN, bat_priv,
			"Error - can't send packet from char device: "
			"got bogus packet type (expected: BAT_ICMP)\n");
		return -EINVAL;
	}

	if (icmp_packet.msg_type != ECHO_REQUEST) {
		bat_dbg(DBG_BATMAN, bat_priv,
			"Error - can't send packet from char device: "
			"got bogus message type (expected: ECHO_REQUEST)\n");
		return -EINVAL;
	}

	icmp_packet.uid = socket_client->index;

	if (icmp_packet.version != COMPAT_VERSION) {
		icmp_packet.msg_type = PARAMETER_PROBLEM;
		icmp_packet.ttl = COMPAT_VERSION;
		bat_socket_add_packet(socket_client, &icmp_packet, packet_len);
		goto out;
	}

	if (atomic_read(&module_state) != MODULE_ACTIVE)
		goto dst_unreach;

	spin_lock_irqsave(&orig_hash_lock, flags);
	orig_node = ((struct orig_node *)hash_find(orig_hash, icmp_packet.dst));

	if (!orig_node)
		goto unlock;

	if (!orig_node->router)
		goto unlock;

	batman_if = orig_node->router->if_incoming;
	memcpy(dstaddr, orig_node->router->addr, ETH_ALEN);

	spin_unlock_irqrestore(&orig_hash_lock, flags);

	if (!batman_if)
		goto dst_unreach;

	if (batman_if->if_status != IF_ACTIVE)
		goto dst_unreach;

	memcpy(icmp_packet.orig,
	       bat_priv->primary_if->net_dev->dev_addr, ETH_ALEN);

	if (packet_len == sizeof(struct icmp_packet_rr))
		memcpy(icmp_packet.rr, batman_if->net_dev->dev_addr, ETH_ALEN);

	send_raw_packet((unsigned char *)&icmp_packet,
			packet_len, batman_if, dstaddr);

	goto out;

unlock:
	spin_unlock_irqrestore(&orig_hash_lock, flags);
dst_unreach:
	icmp_packet.msg_type = DESTINATION_UNREACHABLE;
	bat_socket_add_packet(socket_client, &icmp_packet, packet_len);
out:
	return len;
}

static unsigned int bat_socket_poll(struct file *file, poll_table *wait)
{
	struct socket_client *socket_client = file->private_data;

	poll_wait(file, &socket_client->queue_wait, wait);

	if (socket_client->queue_len > 0)
		return POLLIN | POLLRDNORM;

	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = bat_socket_open,
	.release = bat_socket_release,
	.read = bat_socket_read,
	.write = bat_socket_write,
	.poll = bat_socket_poll,
};

int bat_socket_setup(struct bat_priv *bat_priv)
{
	struct dentry *d;

	if (!bat_priv->debug_dir)
		goto err;

	d = debugfs_create_file(ICMP_SOCKET, S_IFREG | S_IWUSR | S_IRUSR,
				bat_priv->debug_dir, bat_priv, &fops);
	if (d)
		goto err;

	return 0;

err:
	return 1;
}

static void bat_socket_add_packet(struct socket_client *socket_client,
				  struct icmp_packet_rr *icmp_packet,
				  size_t icmp_len)
{
	struct socket_packet *socket_packet;
	unsigned long flags;

	socket_packet = kmalloc(sizeof(struct socket_packet), GFP_ATOMIC);

	if (!socket_packet)
		return;

	INIT_LIST_HEAD(&socket_packet->list);
	memcpy(&socket_packet->icmp_packet, icmp_packet, icmp_len);
	socket_packet->icmp_len = icmp_len;

	spin_lock_irqsave(&socket_client->lock, flags);

	/* while waiting for the lock the socket_client could have been
	 * deleted */
	if (!socket_client_hash[icmp_packet->uid]) {
		spin_unlock_irqrestore(&socket_client->lock, flags);
		kfree(socket_packet);
		return;
	}

	list_add_tail(&socket_packet->list, &socket_client->queue_list);
	socket_client->queue_len++;

	if (socket_client->queue_len > 100) {
		socket_packet = list_first_entry(&socket_client->queue_list,
						 struct socket_packet, list);

		list_del(&socket_packet->list);
		kfree(socket_packet);
		socket_client->queue_len--;
	}

	spin_unlock_irqrestore(&socket_client->lock, flags);

	wake_up(&socket_client->queue_wait);
}

void bat_socket_receive_packet(struct icmp_packet_rr *icmp_packet,
			       size_t icmp_len)
{
	struct socket_client *hash = socket_client_hash[icmp_packet->uid];

	if (hash)
		bat_socket_add_packet(hash, icmp_packet, icmp_len);
}
