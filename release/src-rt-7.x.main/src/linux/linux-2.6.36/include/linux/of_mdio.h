/*
 * OF helpers for the MDIO (Ethernet PHY) API
 *
 * Copyright (c) 2009 Secret Lab Technologies, Ltd.
 *
 * This file is released under the GPLv2
 */

#ifndef __LINUX_OF_MDIO_H
#define __LINUX_OF_MDIO_H

#include <linux/phy.h>
#include <linux/of.h>

extern int of_mdiobus_register(struct mii_bus *mdio, struct device_node *np);
extern struct phy_device *of_phy_find_device(struct device_node *phy_np);
extern struct phy_device *of_phy_connect(struct net_device *dev,
					 struct device_node *phy_np,
					 void (*hndlr)(struct net_device *),
					 u32 flags, phy_interface_t iface);
extern struct phy_device *of_phy_connect_fixed_link(struct net_device *dev,
					 void (*hndlr)(struct net_device *),
					 phy_interface_t iface);

#endif /* __LINUX_OF_MDIO_H */
