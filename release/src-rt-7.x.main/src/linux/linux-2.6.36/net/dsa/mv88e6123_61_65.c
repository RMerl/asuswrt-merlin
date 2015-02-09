/*
 * net/dsa/mv88e6123_61_65.c - Marvell 88e6123/6161/6165 switch chip support
 * Copyright (c) 2008-2009 Marvell Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include "dsa_priv.h"
#include "mv88e6xxx.h"

static char *mv88e6123_61_65_probe(struct mii_bus *bus, int sw_addr)
{
	int ret;

	ret = __mv88e6xxx_reg_read(bus, sw_addr, REG_PORT(0), 0x03);
	if (ret >= 0) {
		ret &= 0xfff0;
		if (ret == 0x1210)
			return "Marvell 88E6123";
		if (ret == 0x1610)
			return "Marvell 88E6161";
		if (ret == 0x1650)
			return "Marvell 88E6165";
	}

	return NULL;
}

static int mv88e6123_61_65_switch_reset(struct dsa_switch *ds)
{
	int i;
	int ret;

	/*
	 * Set all ports to the disabled state.
	 */
	for (i = 0; i < 8; i++) {
		ret = REG_READ(REG_PORT(i), 0x04);
		REG_WRITE(REG_PORT(i), 0x04, ret & 0xfffc);
	}

	/*
	 * Wait for transmit queues to drain.
	 */
	msleep(2);

	/*
	 * Reset the switch.
	 */
	REG_WRITE(REG_GLOBAL, 0x04, 0xc400);

	/*
	 * Wait up to one second for reset to complete.
	 */
	for (i = 0; i < 1000; i++) {
		ret = REG_READ(REG_GLOBAL, 0x00);
		if ((ret & 0xc800) == 0xc800)
			break;

		msleep(1);
	}
	if (i == 1000)
		return -ETIMEDOUT;

	return 0;
}

static int mv88e6123_61_65_setup_global(struct dsa_switch *ds)
{
	int ret;
	int i;

	/*
	 * Disable the PHY polling unit (since there won't be any
	 * external PHYs to poll), don't discard packets with
	 * excessive collisions, and mask all interrupt sources.
	 */
	REG_WRITE(REG_GLOBAL, 0x04, 0x0000);

	/*
	 * Set the default address aging time to 5 minutes, and
	 * enable address learn messages to be sent to all message
	 * ports.
	 */
	REG_WRITE(REG_GLOBAL, 0x0a, 0x0148);

	/*
	 * Configure the priority mapping registers.
	 */
	ret = mv88e6xxx_config_prio(ds);
	if (ret < 0)
		return ret;

	/*
	 * Configure the upstream port, and configure the upstream
	 * port as the port to which ingress and egress monitor frames
	 * are to be sent.
	 */
	REG_WRITE(REG_GLOBAL, 0x1a, (dsa_upstream_port(ds) * 0x1110));

	/*
	 * Disable remote management for now, and set the switch's
	 * DSA device number.
	 */
	REG_WRITE(REG_GLOBAL, 0x1c, ds->index & 0x1f);

	/*
	 * Send all frames with destination addresses matching
	 * 01:80:c2:00:00:2x to the CPU port.
	 */
	REG_WRITE(REG_GLOBAL2, 0x02, 0xffff);

	/*
	 * Send all frames with destination addresses matching
	 * 01:80:c2:00:00:0x to the CPU port.
	 */
	REG_WRITE(REG_GLOBAL2, 0x03, 0xffff);

	/*
	 * Disable the loopback filter, disable flow control
	 * messages, disable flood broadcast override, disable
	 * removing of provider tags, disable ATU age violation
	 * interrupts, disable tag flow control, force flow
	 * control priority to the highest, and send all special
	 * multicast frames to the CPU at the highest priority.
	 */
	REG_WRITE(REG_GLOBAL2, 0x05, 0x00ff);

	/*
	 * Program the DSA routing table.
	 */
	for (i = 0; i < 32; i++) {
		int nexthop;

		nexthop = 0x1f;
		if (i != ds->index && i < ds->dst->pd->nr_chips)
			nexthop = ds->pd->rtable[i] & 0x1f;

		REG_WRITE(REG_GLOBAL2, 0x06, 0x8000 | (i << 8) | nexthop);
	}

	/*
	 * Clear all trunk masks.
	 */
	for (i = 0; i < 8; i++)
		REG_WRITE(REG_GLOBAL2, 0x07, 0x8000 | (i << 12) | 0xff);

	/*
	 * Clear all trunk mappings.
	 */
	for (i = 0; i < 16; i++)
		REG_WRITE(REG_GLOBAL2, 0x08, 0x8000 | (i << 11));

	/*
	 * Disable ingress rate limiting by resetting all ingress
	 * rate limit registers to their initial state.
	 */
	for (i = 0; i < 6; i++)
		REG_WRITE(REG_GLOBAL2, 0x09, 0x9000 | (i << 8));

	/*
	 * Initialise cross-chip port VLAN table to reset defaults.
	 */
	REG_WRITE(REG_GLOBAL2, 0x0b, 0x9000);

	/*
	 * Clear the priority override table.
	 */
	for (i = 0; i < 16; i++)
		REG_WRITE(REG_GLOBAL2, 0x0f, 0x8000 | (i << 8));

	/* @@@ initialise AVB (22/23) watchdog (27) sdet (29) registers */

	return 0;
}

static int mv88e6123_61_65_setup_port(struct dsa_switch *ds, int p)
{
	int addr = REG_PORT(p);
	u16 val;

	/*
	 * MAC Forcing register: don't force link, speed, duplex
	 * or flow control state to any particular values on physical
	 * ports, but force the CPU port and all DSA ports to 1000 Mb/s
	 * full duplex.
	 */
	if (dsa_is_cpu_port(ds, p) || ds->dsa_port_mask & (1 << p))
		REG_WRITE(addr, 0x01, 0x003e);
	else
		REG_WRITE(addr, 0x01, 0x0003);

	/*
	 * Do not limit the period of time that this port can be
	 * paused for by the remote end or the period of time that
	 * this port can pause the remote end.
	 */
	REG_WRITE(addr, 0x02, 0x0000);

	/*
	 * Port Control: disable Drop-on-Unlock, disable Drop-on-Lock,
	 * disable Header mode, enable IGMP/MLD snooping, disable VLAN
	 * tunneling, determine priority by looking at 802.1p and IP
	 * priority fields (IP prio has precedence), and set STP state
	 * to Forwarding.
	 *
	 * If this is the CPU link, use DSA or EDSA tagging depending
	 * on which tagging mode was configured.
	 *
	 * If this is a link to another switch, use DSA tagging mode.
	 *
	 * If this is the upstream port for this switch, enable
	 * forwarding of unknown unicasts and multicasts.
	 */
	val = 0x0433;
	if (dsa_is_cpu_port(ds, p)) {
		if (ds->dst->tag_protocol == htons(ETH_P_EDSA))
			val |= 0x3300;
		else
			val |= 0x0100;
	}
	if (ds->dsa_port_mask & (1 << p))
		val |= 0x0100;
	if (p == dsa_upstream_port(ds))
		val |= 0x000c;
	REG_WRITE(addr, 0x04, val);

	/*
	 * Port Control 1: disable trunking.  Also, if this is the
	 * CPU port, enable learn messages to be sent to this port.
	 */
	REG_WRITE(addr, 0x05, dsa_is_cpu_port(ds, p) ? 0x8000 : 0x0000);

	/*
	 * Port based VLAN map: give each port its own address
	 * database, allow the CPU port to talk to each of the 'real'
	 * ports, and allow each of the 'real' ports to only talk to
	 * the upstream port.
	 */
	val = (p & 0xf) << 12;
	if (dsa_is_cpu_port(ds, p))
		val |= ds->phys_port_mask;
	else
		val |= 1 << dsa_upstream_port(ds);
	REG_WRITE(addr, 0x06, val);

	/*
	 * Default VLAN ID and priority: don't set a default VLAN
	 * ID, and set the default packet priority to zero.
	 */
	REG_WRITE(addr, 0x07, 0x0000);

	/*
	 * Port Control 2: don't force a good FCS, set the maximum
	 * frame size to 10240 bytes, don't let the switch add or
	 * strip 802.1q tags, don't discard tagged or untagged frames
	 * on this port, do a destination address lookup on all
	 * received packets as usual, disable ARP mirroring and don't
	 * send a copy of all transmitted/received frames on this port
	 * to the CPU.
	 */
	REG_WRITE(addr, 0x08, 0x2080);

	/*
	 * Egress rate control: disable egress rate control.
	 */
	REG_WRITE(addr, 0x09, 0x0001);

	/*
	 * Egress rate control 2: disable egress rate control.
	 */
	REG_WRITE(addr, 0x0a, 0x0000);

	/*
	 * Port Association Vector: when learning source addresses
	 * of packets, add the address to the address database using
	 * a port bitmap that has only the bit for this port set and
	 * the other bits clear.
	 */
	REG_WRITE(addr, 0x0b, 1 << p);

	/*
	 * Port ATU control: disable limiting the number of address
	 * database entries that this port is allowed to use.
	 */
	REG_WRITE(addr, 0x0c, 0x0000);

	/*
	 * Priorit Override: disable DA, SA and VTU priority override.
	 */
	REG_WRITE(addr, 0x0d, 0x0000);

	/*
	 * Port Ethertype: use the Ethertype DSA Ethertype value.
	 */
	REG_WRITE(addr, 0x0f, ETH_P_EDSA);

	/*
	 * Tag Remap: use an identity 802.1p prio -> switch prio
	 * mapping.
	 */
	REG_WRITE(addr, 0x18, 0x3210);

	/*
	 * Tag Remap 2: use an identity 802.1p prio -> switch prio
	 * mapping.
	 */
	REG_WRITE(addr, 0x19, 0x7654);

	return 0;
}

static int mv88e6123_61_65_setup(struct dsa_switch *ds)
{
	struct mv88e6xxx_priv_state *ps = (void *)(ds + 1);
	int i;
	int ret;

	mutex_init(&ps->smi_mutex);
	mutex_init(&ps->stats_mutex);

	ret = mv88e6123_61_65_switch_reset(ds);
	if (ret < 0)
		return ret;

	/* @@@ initialise vtu and atu */

	ret = mv88e6123_61_65_setup_global(ds);
	if (ret < 0)
		return ret;

	for (i = 0; i < 6; i++) {
		ret = mv88e6123_61_65_setup_port(ds, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int mv88e6123_61_65_port_to_phy_addr(int port)
{
	if (port >= 0 && port <= 4)
		return port;
	return -1;
}

static int
mv88e6123_61_65_phy_read(struct dsa_switch *ds, int port, int regnum)
{
	int addr = mv88e6123_61_65_port_to_phy_addr(port);
	return mv88e6xxx_phy_read(ds, addr, regnum);
}

static int
mv88e6123_61_65_phy_write(struct dsa_switch *ds,
			      int port, int regnum, u16 val)
{
	int addr = mv88e6123_61_65_port_to_phy_addr(port);
	return mv88e6xxx_phy_write(ds, addr, regnum, val);
}

static struct mv88e6xxx_hw_stat mv88e6123_61_65_hw_stats[] = {
	{ "in_good_octets", 8, 0x00, },
	{ "in_bad_octets", 4, 0x02, },
	{ "in_unicast", 4, 0x04, },
	{ "in_broadcasts", 4, 0x06, },
	{ "in_multicasts", 4, 0x07, },
	{ "in_pause", 4, 0x16, },
	{ "in_undersize", 4, 0x18, },
	{ "in_fragments", 4, 0x19, },
	{ "in_oversize", 4, 0x1a, },
	{ "in_jabber", 4, 0x1b, },
	{ "in_rx_error", 4, 0x1c, },
	{ "in_fcs_error", 4, 0x1d, },
	{ "out_octets", 8, 0x0e, },
	{ "out_unicast", 4, 0x10, },
	{ "out_broadcasts", 4, 0x13, },
	{ "out_multicasts", 4, 0x12, },
	{ "out_pause", 4, 0x15, },
	{ "excessive", 4, 0x11, },
	{ "collisions", 4, 0x1e, },
	{ "deferred", 4, 0x05, },
	{ "single", 4, 0x14, },
	{ "multiple", 4, 0x17, },
	{ "out_fcs_error", 4, 0x03, },
	{ "late", 4, 0x1f, },
	{ "hist_64bytes", 4, 0x08, },
	{ "hist_65_127bytes", 4, 0x09, },
	{ "hist_128_255bytes", 4, 0x0a, },
	{ "hist_256_511bytes", 4, 0x0b, },
	{ "hist_512_1023bytes", 4, 0x0c, },
	{ "hist_1024_max_bytes", 4, 0x0d, },
};

static void
mv88e6123_61_65_get_strings(struct dsa_switch *ds, int port, uint8_t *data)
{
	mv88e6xxx_get_strings(ds, ARRAY_SIZE(mv88e6123_61_65_hw_stats),
			      mv88e6123_61_65_hw_stats, port, data);
}

static void
mv88e6123_61_65_get_ethtool_stats(struct dsa_switch *ds,
				  int port, uint64_t *data)
{
	mv88e6xxx_get_ethtool_stats(ds, ARRAY_SIZE(mv88e6123_61_65_hw_stats),
				    mv88e6123_61_65_hw_stats, port, data);
}

static int mv88e6123_61_65_get_sset_count(struct dsa_switch *ds)
{
	return ARRAY_SIZE(mv88e6123_61_65_hw_stats);
}

static struct dsa_switch_driver mv88e6123_61_65_switch_driver = {
	.tag_protocol		= cpu_to_be16(ETH_P_EDSA),
	.priv_size		= sizeof(struct mv88e6xxx_priv_state),
	.probe			= mv88e6123_61_65_probe,
	.setup			= mv88e6123_61_65_setup,
	.set_addr		= mv88e6xxx_set_addr_indirect,
	.phy_read		= mv88e6123_61_65_phy_read,
	.phy_write		= mv88e6123_61_65_phy_write,
	.poll_link		= mv88e6xxx_poll_link,
	.get_strings		= mv88e6123_61_65_get_strings,
	.get_ethtool_stats	= mv88e6123_61_65_get_ethtool_stats,
	.get_sset_count		= mv88e6123_61_65_get_sset_count,
};

static int __init mv88e6123_61_65_init(void)
{
	register_switch_driver(&mv88e6123_61_65_switch_driver);
	return 0;
}
module_init(mv88e6123_61_65_init);

static void __exit mv88e6123_61_65_cleanup(void)
{
	unregister_switch_driver(&mv88e6123_61_65_switch_driver);
}
module_exit(mv88e6123_61_65_cleanup);
