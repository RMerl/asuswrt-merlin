#ifndef __NET_SYSFS_H__
#define __NET_SYSFS_H__

int netdev_kobject_init(void);
int netdev_register_kobject(struct net_device *);
void netdev_unregister_kobject(struct net_device *);
int net_rx_queue_update_kobjects(struct net_device *, int old_num, int new_num);
int netdev_queue_update_kobjects(struct net_device *net,
				 int old_num, int new_num);

#endif
