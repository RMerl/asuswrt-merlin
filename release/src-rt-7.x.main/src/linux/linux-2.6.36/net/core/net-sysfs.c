/*
 * net-sysfs.c - network device class and attributes
 *
 * Copyright (c) 2003 Stephen Hemminger <shemminger@osdl.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/slab.h>
#include <linux/nsproxy.h>
#include <net/sock.h>
#include <net/net_namespace.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <linux/vmalloc.h>
#include <net/wext.h>

#include "net-sysfs.h"

#ifdef CONFIG_SYSFS
static const char fmt_hex[] = "%#x\n";
static const char fmt_long_hex[] = "%#lx\n";
static const char fmt_dec[] = "%d\n";
static const char fmt_ulong[] = "%lu\n";
static const char fmt_u64[] = "%llu\n";

static inline int dev_isalive(const struct net_device *dev)
{
	return dev->reg_state <= NETREG_REGISTERED;
}

/* use same locking rules as GIF* ioctl's */
static ssize_t netdev_show(const struct device *dev,
			   struct device_attribute *attr, char *buf,
			   ssize_t (*format)(const struct net_device *, char *))
{
	struct net_device *net = to_net_dev(dev);
	ssize_t ret = -EINVAL;

	read_lock(&dev_base_lock);
	if (dev_isalive(net))
		ret = (*format)(net, buf);
	read_unlock(&dev_base_lock);

	return ret;
}

/* generate a show function for simple field */
#define NETDEVICE_SHOW(field, format_string)				\
static ssize_t format_##field(const struct net_device *net, char *buf)	\
{									\
	return sprintf(buf, format_string, net->field);			\
}									\
static ssize_t show_##field(struct device *dev,				\
			    struct device_attribute *attr, char *buf)	\
{									\
	return netdev_show(dev, attr, buf, format_##field);		\
}


/* use same locking and permission rules as SIF* ioctl's */
static ssize_t netdev_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len,
			    int (*set)(struct net_device *, unsigned long))
{
	struct net_device *net = to_net_dev(dev);
	char *endp;
	unsigned long new;
	int ret = -EINVAL;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	new = simple_strtoul(buf, &endp, 0);
	if (endp == buf)
		goto err;

	if (!rtnl_trylock())
		return restart_syscall();

	if (dev_isalive(net)) {
		if ((ret = (*set)(net, new)) == 0)
			ret = len;
	}
	rtnl_unlock();
 err:
	return ret;
}

NETDEVICE_SHOW(dev_id, fmt_hex);
NETDEVICE_SHOW(addr_assign_type, fmt_dec);
NETDEVICE_SHOW(addr_len, fmt_dec);
NETDEVICE_SHOW(iflink, fmt_dec);
NETDEVICE_SHOW(ifindex, fmt_dec);
NETDEVICE_SHOW(features, fmt_long_hex);
NETDEVICE_SHOW(type, fmt_dec);
NETDEVICE_SHOW(link_mode, fmt_dec);

/* use same locking rules as GIFHWADDR ioctl's */
static ssize_t show_address(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct net_device *net = to_net_dev(dev);
	ssize_t ret = -EINVAL;

	read_lock(&dev_base_lock);
	if (dev_isalive(net))
		ret = sysfs_format_mac(buf, net->dev_addr, net->addr_len);
	read_unlock(&dev_base_lock);
	return ret;
}

static ssize_t show_broadcast(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct net_device *net = to_net_dev(dev);
	if (dev_isalive(net))
		return sysfs_format_mac(buf, net->broadcast, net->addr_len);
	return -EINVAL;
}

static ssize_t show_carrier(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_dev(dev);
	if (netif_running(netdev)) {
		return sprintf(buf, fmt_dec, !!netif_carrier_ok(netdev));
	}
	return -EINVAL;
}

static ssize_t show_speed(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_dev(dev);
	int ret = -EINVAL;

	if (!rtnl_trylock())
		return restart_syscall();

	if (netif_running(netdev) &&
	    netdev->ethtool_ops &&
	    netdev->ethtool_ops->get_settings) {
		struct ethtool_cmd cmd = { ETHTOOL_GSET };

		if (!netdev->ethtool_ops->get_settings(netdev, &cmd))
			ret = sprintf(buf, fmt_dec, ethtool_cmd_speed(&cmd));
	}
	rtnl_unlock();
	return ret;
}

static ssize_t show_duplex(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_dev(dev);
	int ret = -EINVAL;

	if (!rtnl_trylock())
		return restart_syscall();

	if (netif_running(netdev) &&
	    netdev->ethtool_ops &&
	    netdev->ethtool_ops->get_settings) {
		struct ethtool_cmd cmd = { ETHTOOL_GSET };

		if (!netdev->ethtool_ops->get_settings(netdev, &cmd))
			ret = sprintf(buf, "%s\n", cmd.duplex ? "full" : "half");
	}
	rtnl_unlock();
	return ret;
}

static ssize_t show_dormant(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_dev(dev);

	if (netif_running(netdev))
		return sprintf(buf, fmt_dec, !!netif_dormant(netdev));

	return -EINVAL;
}

static const char *const operstates[] = {
	"unknown",
	"notpresent", /* currently unused */
	"down",
	"lowerlayerdown",
	"testing", /* currently unused */
	"dormant",
	"up"
};

static ssize_t show_operstate(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	const struct net_device *netdev = to_net_dev(dev);
	unsigned char operstate;

	read_lock(&dev_base_lock);
	operstate = netdev->operstate;
	if (!netif_running(netdev))
		operstate = IF_OPER_DOWN;
	read_unlock(&dev_base_lock);

	if (operstate >= ARRAY_SIZE(operstates))
		return -EINVAL; /* should not happen */

	return sprintf(buf, "%s\n", operstates[operstate]);
}

/* read-write attributes */
NETDEVICE_SHOW(mtu, fmt_dec);

static int change_mtu(struct net_device *net, unsigned long new_mtu)
{
	return dev_set_mtu(net, (int) new_mtu);
}

static ssize_t store_mtu(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t len)
{
	return netdev_store(dev, attr, buf, len, change_mtu);
}

NETDEVICE_SHOW(flags, fmt_hex);

static int change_flags(struct net_device *net, unsigned long new_flags)
{
	return dev_change_flags(net, (unsigned) new_flags);
}

static ssize_t store_flags(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t len)
{
	return netdev_store(dev, attr, buf, len, change_flags);
}

NETDEVICE_SHOW(tx_queue_len, fmt_ulong);

static int change_tx_queue_len(struct net_device *net, unsigned long new_len)
{
	net->tx_queue_len = new_len;
	return 0;
}

static ssize_t store_tx_queue_len(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len)
{
	return netdev_store(dev, attr, buf, len, change_tx_queue_len);
}

static ssize_t store_ifalias(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct net_device *netdev = to_net_dev(dev);
	size_t count = len;
	ssize_t ret;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* ignore trailing newline */
	if (len >  0 && buf[len - 1] == '\n')
		--count;

	if (!rtnl_trylock())
		return restart_syscall();
	ret = dev_set_alias(netdev, buf, count);
	rtnl_unlock();

	return ret < 0 ? ret : len;
}

static ssize_t show_ifalias(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	const struct net_device *netdev = to_net_dev(dev);
	ssize_t ret = 0;

	if (!rtnl_trylock())
		return restart_syscall();
	if (netdev->ifalias)
		ret = sprintf(buf, "%s\n", netdev->ifalias);
	rtnl_unlock();
	return ret;
}

static struct device_attribute net_class_attributes[] = {
	__ATTR(addr_assign_type, S_IRUGO, show_addr_assign_type, NULL),
	__ATTR(addr_len, S_IRUGO, show_addr_len, NULL),
	__ATTR(dev_id, S_IRUGO, show_dev_id, NULL),
	__ATTR(ifalias, S_IRUGO | S_IWUSR, show_ifalias, store_ifalias),
	__ATTR(iflink, S_IRUGO, show_iflink, NULL),
	__ATTR(ifindex, S_IRUGO, show_ifindex, NULL),
	__ATTR(features, S_IRUGO, show_features, NULL),
	__ATTR(type, S_IRUGO, show_type, NULL),
	__ATTR(link_mode, S_IRUGO, show_link_mode, NULL),
	__ATTR(address, S_IRUGO, show_address, NULL),
	__ATTR(broadcast, S_IRUGO, show_broadcast, NULL),
	__ATTR(carrier, S_IRUGO, show_carrier, NULL),
	__ATTR(speed, S_IRUGO, show_speed, NULL),
	__ATTR(duplex, S_IRUGO, show_duplex, NULL),
	__ATTR(dormant, S_IRUGO, show_dormant, NULL),
	__ATTR(operstate, S_IRUGO, show_operstate, NULL),
	__ATTR(mtu, S_IRUGO | S_IWUSR, show_mtu, store_mtu),
	__ATTR(flags, S_IRUGO | S_IWUSR, show_flags, store_flags),
	__ATTR(tx_queue_len, S_IRUGO | S_IWUSR, show_tx_queue_len,
	       store_tx_queue_len),
	{}
};

/* Show a given an attribute in the statistics group */
static ssize_t netstat_show(const struct device *d,
			    struct device_attribute *attr, char *buf,
			    unsigned long offset)
{
	struct net_device *dev = to_net_dev(d);
	ssize_t ret = -EINVAL;

	WARN_ON(offset > sizeof(struct rtnl_link_stats64) ||
			offset % sizeof(u64) != 0);

	read_lock(&dev_base_lock);
	if (dev_isalive(dev)) {
		struct rtnl_link_stats64 temp;
		const struct rtnl_link_stats64 *stats = dev_get_stats(dev, &temp);

		ret = sprintf(buf, fmt_u64, *(u64 *)(((u8 *) stats) + offset));
	}
	read_unlock(&dev_base_lock);
	return ret;
}

/* generate a read-only statistics attribute */
#define NETSTAT_ENTRY(name)						\
static ssize_t show_##name(struct device *d,				\
			   struct device_attribute *attr, char *buf) 	\
{									\
	return netstat_show(d, attr, buf,				\
			    offsetof(struct rtnl_link_stats64, name));	\
}									\
static DEVICE_ATTR(name, S_IRUGO, show_##name, NULL)

NETSTAT_ENTRY(rx_packets);
NETSTAT_ENTRY(tx_packets);
NETSTAT_ENTRY(rx_bytes);
NETSTAT_ENTRY(tx_bytes);
NETSTAT_ENTRY(rx_errors);
NETSTAT_ENTRY(tx_errors);
NETSTAT_ENTRY(rx_dropped);
NETSTAT_ENTRY(tx_dropped);
NETSTAT_ENTRY(multicast);
NETSTAT_ENTRY(collisions);
NETSTAT_ENTRY(rx_length_errors);
NETSTAT_ENTRY(rx_over_errors);
NETSTAT_ENTRY(rx_crc_errors);
NETSTAT_ENTRY(rx_frame_errors);
NETSTAT_ENTRY(rx_fifo_errors);
NETSTAT_ENTRY(rx_missed_errors);
NETSTAT_ENTRY(tx_aborted_errors);
NETSTAT_ENTRY(tx_carrier_errors);
NETSTAT_ENTRY(tx_fifo_errors);
NETSTAT_ENTRY(tx_heartbeat_errors);
NETSTAT_ENTRY(tx_window_errors);
NETSTAT_ENTRY(rx_compressed);
NETSTAT_ENTRY(tx_compressed);

static struct attribute *netstat_attrs[] = {
	&dev_attr_rx_packets.attr,
	&dev_attr_tx_packets.attr,
	&dev_attr_rx_bytes.attr,
	&dev_attr_tx_bytes.attr,
	&dev_attr_rx_errors.attr,
	&dev_attr_tx_errors.attr,
	&dev_attr_rx_dropped.attr,
	&dev_attr_tx_dropped.attr,
	&dev_attr_multicast.attr,
	&dev_attr_collisions.attr,
	&dev_attr_rx_length_errors.attr,
	&dev_attr_rx_over_errors.attr,
	&dev_attr_rx_crc_errors.attr,
	&dev_attr_rx_frame_errors.attr,
	&dev_attr_rx_fifo_errors.attr,
	&dev_attr_rx_missed_errors.attr,
	&dev_attr_tx_aborted_errors.attr,
	&dev_attr_tx_carrier_errors.attr,
	&dev_attr_tx_fifo_errors.attr,
	&dev_attr_tx_heartbeat_errors.attr,
	&dev_attr_tx_window_errors.attr,
	&dev_attr_rx_compressed.attr,
	&dev_attr_tx_compressed.attr,
	NULL
};


static struct attribute_group netstat_group = {
	.name  = "statistics",
	.attrs  = netstat_attrs,
};

#ifdef CONFIG_WIRELESS_EXT_SYSFS
/* helper function that does all the locking etc for wireless stats */
static ssize_t wireless_show(struct device *d, char *buf,
			     ssize_t (*format)(const struct iw_statistics *,
					       char *))
{
	struct net_device *dev = to_net_dev(d);
	const struct iw_statistics *iw;
	ssize_t ret = -EINVAL;

	if (!rtnl_trylock())
		return restart_syscall();
	if (dev_isalive(dev)) {
		iw = get_wireless_stats(dev);
		if (iw)
			ret = (*format)(iw, buf);
	}
	rtnl_unlock();

	return ret;
}

/* show function template for wireless fields */
#define WIRELESS_SHOW(name, field, format_string)			\
static ssize_t format_iw_##name(const struct iw_statistics *iw, char *buf) \
{									\
	return sprintf(buf, format_string, iw->field);			\
}									\
static ssize_t show_iw_##name(struct device *d,				\
			      struct device_attribute *attr, char *buf)	\
{									\
	return wireless_show(d, buf, format_iw_##name);			\
}									\
static DEVICE_ATTR(name, S_IRUGO, show_iw_##name, NULL)

WIRELESS_SHOW(status, status, fmt_hex);
WIRELESS_SHOW(link, qual.qual, fmt_dec);
WIRELESS_SHOW(level, qual.level, fmt_dec);
WIRELESS_SHOW(noise, qual.noise, fmt_dec);
WIRELESS_SHOW(nwid, discard.nwid, fmt_dec);
WIRELESS_SHOW(crypt, discard.code, fmt_dec);
WIRELESS_SHOW(fragment, discard.fragment, fmt_dec);
WIRELESS_SHOW(misc, discard.misc, fmt_dec);
WIRELESS_SHOW(retries, discard.retries, fmt_dec);
WIRELESS_SHOW(beacon, miss.beacon, fmt_dec);

static struct attribute *wireless_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_link.attr,
	&dev_attr_level.attr,
	&dev_attr_noise.attr,
	&dev_attr_nwid.attr,
	&dev_attr_crypt.attr,
	&dev_attr_fragment.attr,
	&dev_attr_retries.attr,
	&dev_attr_misc.attr,
	&dev_attr_beacon.attr,
	NULL
};

static struct attribute_group wireless_group = {
	.name = "wireless",
	.attrs = wireless_attrs,
};
#endif
#endif /* CONFIG_SYSFS */

#ifdef CONFIG_RPS
/*
 * RX queue sysfs structures and functions.
 */
struct rx_queue_attribute {
	struct attribute attr;
	ssize_t (*show)(struct netdev_rx_queue *queue,
	    struct rx_queue_attribute *attr, char *buf);
	ssize_t (*store)(struct netdev_rx_queue *queue,
	    struct rx_queue_attribute *attr, const char *buf, size_t len);
};
#define to_rx_queue_attr(_attr) container_of(_attr,		\
    struct rx_queue_attribute, attr)

#define to_rx_queue(obj) container_of(obj, struct netdev_rx_queue, kobj)

static ssize_t rx_queue_attr_show(struct kobject *kobj, struct attribute *attr,
				  char *buf)
{
	struct rx_queue_attribute *attribute = to_rx_queue_attr(attr);
	struct netdev_rx_queue *queue = to_rx_queue(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(queue, attribute, buf);
}

static ssize_t rx_queue_attr_store(struct kobject *kobj, struct attribute *attr,
				   const char *buf, size_t count)
{
	struct rx_queue_attribute *attribute = to_rx_queue_attr(attr);
	struct netdev_rx_queue *queue = to_rx_queue(kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(queue, attribute, buf, count);
}

static struct sysfs_ops rx_queue_sysfs_ops = {
	.show = rx_queue_attr_show,
	.store = rx_queue_attr_store,
};

static ssize_t show_rps_map(struct netdev_rx_queue *queue,
			    struct rx_queue_attribute *attribute, char *buf)
{
	struct rps_map *map;
	cpumask_var_t mask;
	size_t len = 0;
	int i;

	if (!zalloc_cpumask_var(&mask, GFP_KERNEL))
		return -ENOMEM;

	rcu_read_lock();
	map = rcu_dereference(queue->rps_map);
	if (map)
		for (i = 0; i < map->len; i++)
			cpumask_set_cpu(map->cpus[i], mask);

	len += cpumask_scnprintf(buf + len, PAGE_SIZE, mask);
	if (PAGE_SIZE - len < 3) {
		rcu_read_unlock();
		free_cpumask_var(mask);
		return -EINVAL;
	}
	rcu_read_unlock();

	free_cpumask_var(mask);
	len += sprintf(buf + len, "\n");
	return len;
}

static void rps_map_release(struct rcu_head *rcu)
{
	struct rps_map *map = container_of(rcu, struct rps_map, rcu);

	kfree(map);
}

static ssize_t store_rps_map(struct netdev_rx_queue *queue,
		      struct rx_queue_attribute *attribute,
		      const char *buf, size_t len)
{
	struct rps_map *old_map, *map;
	cpumask_var_t mask;
	int err, cpu, i;
	static DEFINE_SPINLOCK(rps_map_lock);

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!alloc_cpumask_var(&mask, GFP_KERNEL))
		return -ENOMEM;

	err = bitmap_parse(buf, len, cpumask_bits(mask), nr_cpumask_bits);
	if (err) {
		free_cpumask_var(mask);
		return err;
	}

	map = kzalloc(max_t(unsigned,
	    RPS_MAP_SIZE(cpumask_weight(mask)), L1_CACHE_BYTES),
	    GFP_KERNEL);
	if (!map) {
		free_cpumask_var(mask);
		return -ENOMEM;
	}

	i = 0;
	for_each_cpu_and(cpu, mask, cpu_online_mask)
		map->cpus[i++] = cpu;

	if (i)
		map->len = i;
	else {
		kfree(map);
		map = NULL;
	}

	spin_lock(&rps_map_lock);
	old_map = queue->rps_map;
	rcu_assign_pointer(queue->rps_map, map);
	spin_unlock(&rps_map_lock);

	if (old_map)
		call_rcu(&old_map->rcu, rps_map_release);

	free_cpumask_var(mask);
	return len;
}

static ssize_t show_rps_dev_flow_table_cnt(struct netdev_rx_queue *queue,
					   struct rx_queue_attribute *attr,
					   char *buf)
{
	struct rps_dev_flow_table *flow_table;
	unsigned int val = 0;

	rcu_read_lock();
	flow_table = rcu_dereference(queue->rps_flow_table);
	if (flow_table)
		val = flow_table->mask + 1;
	rcu_read_unlock();

	return sprintf(buf, "%u\n", val);
}

static void rps_dev_flow_table_release_work(struct work_struct *work)
{
	struct rps_dev_flow_table *table = container_of(work,
	    struct rps_dev_flow_table, free_work);

	vfree(table);
}

static void rps_dev_flow_table_release(struct rcu_head *rcu)
{
	struct rps_dev_flow_table *table = container_of(rcu,
	    struct rps_dev_flow_table, rcu);

	INIT_WORK(&table->free_work, rps_dev_flow_table_release_work);
	schedule_work(&table->free_work);
}

static ssize_t store_rps_dev_flow_table_cnt(struct netdev_rx_queue *queue,
				     struct rx_queue_attribute *attr,
				     const char *buf, size_t len)
{
	unsigned int count;
	char *endp;
	struct rps_dev_flow_table *table, *old_table;
	static DEFINE_SPINLOCK(rps_dev_flow_lock);

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	count = simple_strtoul(buf, &endp, 0);
	if (endp == buf)
		return -EINVAL;

	if (count) {
		int i;

		if (count > 1<<30) {
			/* Enforce a limit to prevent overflow */
			return -EINVAL;
		}
		count = roundup_pow_of_two(count);
		table = vmalloc(RPS_DEV_FLOW_TABLE_SIZE(count));
		if (!table)
			return -ENOMEM;

		table->mask = count - 1;
		for (i = 0; i < count; i++)
			table->flows[i].cpu = RPS_NO_CPU;
	} else
		table = NULL;

	spin_lock(&rps_dev_flow_lock);
	old_table = queue->rps_flow_table;
	rcu_assign_pointer(queue->rps_flow_table, table);
	spin_unlock(&rps_dev_flow_lock);

	if (old_table)
		call_rcu(&old_table->rcu, rps_dev_flow_table_release);

	return len;
}

static struct rx_queue_attribute rps_cpus_attribute =
	__ATTR(rps_cpus, S_IRUGO | S_IWUSR, show_rps_map, store_rps_map);


static struct rx_queue_attribute rps_dev_flow_table_cnt_attribute =
	__ATTR(rps_flow_cnt, S_IRUGO | S_IWUSR,
	    show_rps_dev_flow_table_cnt, store_rps_dev_flow_table_cnt);

static struct attribute *rx_queue_default_attrs[] = {
	&rps_cpus_attribute.attr,
	&rps_dev_flow_table_cnt_attribute.attr,
	NULL
};

static void rx_queue_release(struct kobject *kobj)
{
	struct netdev_rx_queue *queue = to_rx_queue(kobj);
	struct netdev_rx_queue *first = queue->first;

	if (queue->rps_map)
		call_rcu(&queue->rps_map->rcu, rps_map_release);

	if (queue->rps_flow_table)
		call_rcu(&queue->rps_flow_table->rcu,
		    rps_dev_flow_table_release);

	if (atomic_dec_and_test(&first->count))
		kfree(first);
}

static struct kobj_type rx_queue_ktype = {
	.sysfs_ops = &rx_queue_sysfs_ops,
	.release = rx_queue_release,
	.default_attrs = rx_queue_default_attrs,
};

static int rx_queue_add_kobject(struct net_device *net, int index)
{
	struct netdev_rx_queue *queue = net->_rx + index;
	struct kobject *kobj = &queue->kobj;
	int error = 0;

	kobj->kset = net->queues_kset;
	error = kobject_init_and_add(kobj, &rx_queue_ktype, NULL,
	    "rx-%u", index);
	if (error) {
		kobject_put(kobj);
		return error;
	}

	kobject_uevent(kobj, KOBJ_ADD);

	return error;
}

static int rx_queue_register_kobjects(struct net_device *net)
{
	int i;
	int error = 0;

	net->queues_kset = kset_create_and_add("queues",
	    NULL, &net->dev.kobj);
	if (!net->queues_kset)
		return -ENOMEM;
	for (i = 0; i < net->num_rx_queues; i++) {
		error = rx_queue_add_kobject(net, i);
		if (error)
			break;
	}

	if (error)
		while (--i >= 0)
			kobject_put(&net->_rx[i].kobj);

	return error;
}

static void rx_queue_remove_kobjects(struct net_device *net)
{
	int i;

	for (i = 0; i < net->num_rx_queues; i++)
		kobject_put(&net->_rx[i].kobj);
	kset_unregister(net->queues_kset);
}
#endif /* CONFIG_RPS */

static const void *net_current_ns(void)
{
	return current->nsproxy->net_ns;
}

static const void *net_initial_ns(void)
{
	return &init_net;
}

static const void *net_netlink_ns(struct sock *sk)
{
	return sock_net(sk);
}

static struct kobj_ns_type_operations net_ns_type_operations = {
	.type = KOBJ_NS_TYPE_NET,
	.current_ns = net_current_ns,
	.netlink_ns = net_netlink_ns,
	.initial_ns = net_initial_ns,
};

static void net_kobj_ns_exit(struct net *net)
{
	kobj_ns_exit(KOBJ_NS_TYPE_NET, net);
}

static struct pernet_operations kobj_net_ops = {
	.exit = net_kobj_ns_exit,
};


#ifdef CONFIG_HOTPLUG
static int netdev_uevent(struct device *d, struct kobj_uevent_env *env)
{
	struct net_device *dev = to_net_dev(d);
	int retval;

	/* pass interface to uevent. */
	retval = add_uevent_var(env, "INTERFACE=%s", dev->name);
	if (retval)
		goto exit;

	/* pass ifindex to uevent.
	 * ifindex is useful as it won't change (interface name may change)
	 * and is what RtNetlink uses natively. */
	retval = add_uevent_var(env, "IFINDEX=%d", dev->ifindex);

exit:
	return retval;
}
#endif

/*
 *	netdev_release -- destroy and free a dead device.
 *	Called when last reference to device kobject is gone.
 */
static void netdev_release(struct device *d)
{
	struct net_device *dev = to_net_dev(d);

	BUG_ON(dev->reg_state != NETREG_RELEASED);

	kfree(dev->ifalias);
	kfree((char *)dev - dev->padded);
}

static const void *net_namespace(struct device *d)
{
	struct net_device *dev;
	dev = container_of(d, struct net_device, dev);
	return dev_net(dev);
}

static struct class net_class = {
	.name = "net",
	.dev_release = netdev_release,
#ifdef CONFIG_SYSFS
	.dev_attrs = net_class_attributes,
#endif /* CONFIG_SYSFS */
#ifdef CONFIG_HOTPLUG
	.dev_uevent = netdev_uevent,
#endif
	.ns_type = &net_ns_type_operations,
	.namespace = net_namespace,
};

/* Delete sysfs entries but hold kobject reference until after all
 * netdev references are gone.
 */
void netdev_unregister_kobject(struct net_device * net)
{
	struct device *dev = &(net->dev);

	kobject_get(&dev->kobj);

#ifdef CONFIG_RPS
	rx_queue_remove_kobjects(net);
#endif

	device_del(dev);
}

/* Create sysfs entries for network device. */
int netdev_register_kobject(struct net_device *net)
{
	struct device *dev = &(net->dev);
	const struct attribute_group **groups = net->sysfs_groups;
	int error = 0;

	device_initialize(dev);
	dev->class = &net_class;
	dev->platform_data = net;
	dev->groups = groups;

	dev_set_name(dev, "%s", net->name);

#ifdef CONFIG_SYSFS
	/* Allow for a device specific group */
	if (*groups)
		groups++;

	*groups++ = &netstat_group;
#ifdef CONFIG_WIRELESS_EXT_SYSFS
	if (net->ieee80211_ptr)
		*groups++ = &wireless_group;
#ifdef CONFIG_WIRELESS_EXT
	else if (net->wireless_handlers)
		*groups++ = &wireless_group;
#endif
#endif
#endif /* CONFIG_SYSFS */

	error = device_add(dev);
	if (error)
		return error;

#ifdef CONFIG_RPS
	error = rx_queue_register_kobjects(net);
	if (error) {
		device_del(dev);
		return error;
	}
#endif

	return error;
}

int netdev_class_create_file(struct class_attribute *class_attr)
{
	return class_create_file(&net_class, class_attr);
}
EXPORT_SYMBOL(netdev_class_create_file);

void netdev_class_remove_file(struct class_attribute *class_attr)
{
	class_remove_file(&net_class, class_attr);
}
EXPORT_SYMBOL(netdev_class_remove_file);

int netdev_kobject_init(void)
{
	kobj_ns_type_register(&net_ns_type_operations);
	register_pernet_subsys(&kobj_net_ops);
	return class_register(&net_class);
}
