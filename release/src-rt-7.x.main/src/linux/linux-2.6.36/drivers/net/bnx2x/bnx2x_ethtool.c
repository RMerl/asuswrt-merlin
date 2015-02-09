
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/crc32.h>


#include "bnx2x.h"
#include "bnx2x_cmn.h"
#include "bnx2x_dump.h"


static int bnx2x_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct bnx2x *bp = netdev_priv(dev);

	cmd->supported = bp->port.supported;
	cmd->advertising = bp->port.advertising;

	if ((bp->state == BNX2X_STATE_OPEN) &&
	    !(bp->flags & MF_FUNC_DIS) &&
	    (bp->link_vars.link_up)) {
		cmd->speed = bp->link_vars.line_speed;
		cmd->duplex = bp->link_vars.duplex;
		if (IS_E1HMF(bp)) {
			u16 vn_max_rate;

			vn_max_rate =
				((bp->mf_config & FUNC_MF_CFG_MAX_BW_MASK) >>
				FUNC_MF_CFG_MAX_BW_SHIFT) * 100;
			if (vn_max_rate < cmd->speed)
				cmd->speed = vn_max_rate;
		}
	} else {
		cmd->speed = -1;
		cmd->duplex = -1;
	}

	if (bp->link_params.switch_cfg == SWITCH_CFG_10G) {
		u32 ext_phy_type =
			XGXS_EXT_PHY_TYPE(bp->link_params.ext_phy_config);

		switch (ext_phy_type) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			cmd->port = PORT_FIBRE;
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
			cmd->port = PORT_TP;
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE:
			BNX2X_ERR("XGXS PHY Failure detected 0x%x\n",
				  bp->link_params.ext_phy_config);
			break;

		default:
			DP(NETIF_MSG_LINK, "BAD XGXS ext_phy_config 0x%x\n",
			   bp->link_params.ext_phy_config);
			break;
		}
	} else
		cmd->port = PORT_TP;

	cmd->phy_address = bp->mdio.prtad;
	cmd->transceiver = XCVR_INTERNAL;

	if (bp->link_params.req_line_speed == SPEED_AUTO_NEG)
		cmd->autoneg = AUTONEG_ENABLE;
	else
		cmd->autoneg = AUTONEG_DISABLE;

	cmd->maxtxpkt = 0;
	cmd->maxrxpkt = 0;

	DP(NETIF_MSG_LINK, "ethtool_cmd: cmd %d\n"
	   DP_LEVEL "  supported 0x%x  advertising 0x%x  speed %d\n"
	   DP_LEVEL "  duplex %d  port %d  phy_address %d  transceiver %d\n"
	   DP_LEVEL "  autoneg %d  maxtxpkt %d  maxrxpkt %d\n",
	   cmd->cmd, cmd->supported, cmd->advertising, cmd->speed,
	   cmd->duplex, cmd->port, cmd->phy_address, cmd->transceiver,
	   cmd->autoneg, cmd->maxtxpkt, cmd->maxrxpkt);

	return 0;
}

static int bnx2x_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct bnx2x *bp = netdev_priv(dev);
	u32 advertising;

	if (IS_E1HMF(bp))
		return 0;

	DP(NETIF_MSG_LINK, "ethtool_cmd: cmd %d\n"
	   DP_LEVEL "  supported 0x%x  advertising 0x%x  speed %d\n"
	   DP_LEVEL "  duplex %d  port %d  phy_address %d  transceiver %d\n"
	   DP_LEVEL "  autoneg %d  maxtxpkt %d  maxrxpkt %d\n",
	   cmd->cmd, cmd->supported, cmd->advertising, cmd->speed,
	   cmd->duplex, cmd->port, cmd->phy_address, cmd->transceiver,
	   cmd->autoneg, cmd->maxtxpkt, cmd->maxrxpkt);

	if (cmd->autoneg == AUTONEG_ENABLE) {
		if (!(bp->port.supported & SUPPORTED_Autoneg)) {
			DP(NETIF_MSG_LINK, "Autoneg not supported\n");
			return -EINVAL;
		}

		/* advertise the requested speed and duplex if supported */
		cmd->advertising &= bp->port.supported;

		bp->link_params.req_line_speed = SPEED_AUTO_NEG;
		bp->link_params.req_duplex = DUPLEX_FULL;
		bp->port.advertising |= (ADVERTISED_Autoneg |
					 cmd->advertising);

	} else { /* forced speed */
		/* advertise the requested speed and duplex if supported */
		switch (cmd->speed) {
		case SPEED_10:
			if (cmd->duplex == DUPLEX_FULL) {
				if (!(bp->port.supported &
				      SUPPORTED_10baseT_Full)) {
					DP(NETIF_MSG_LINK,
					   "10M full not supported\n");
					return -EINVAL;
				}

				advertising = (ADVERTISED_10baseT_Full |
					       ADVERTISED_TP);
			} else {
				if (!(bp->port.supported &
				      SUPPORTED_10baseT_Half)) {
					DP(NETIF_MSG_LINK,
					   "10M half not supported\n");
					return -EINVAL;
				}

				advertising = (ADVERTISED_10baseT_Half |
					       ADVERTISED_TP);
			}
			break;

		case SPEED_100:
			if (cmd->duplex == DUPLEX_FULL) {
				if (!(bp->port.supported &
						SUPPORTED_100baseT_Full)) {
					DP(NETIF_MSG_LINK,
					   "100M full not supported\n");
					return -EINVAL;
				}

				advertising = (ADVERTISED_100baseT_Full |
					       ADVERTISED_TP);
			} else {
				if (!(bp->port.supported &
						SUPPORTED_100baseT_Half)) {
					DP(NETIF_MSG_LINK,
					   "100M half not supported\n");
					return -EINVAL;
				}

				advertising = (ADVERTISED_100baseT_Half |
					       ADVERTISED_TP);
			}
			break;

		case SPEED_1000:
			if (cmd->duplex != DUPLEX_FULL) {
				DP(NETIF_MSG_LINK, "1G half not supported\n");
				return -EINVAL;
			}

			if (!(bp->port.supported & SUPPORTED_1000baseT_Full)) {
				DP(NETIF_MSG_LINK, "1G full not supported\n");
				return -EINVAL;
			}

			advertising = (ADVERTISED_1000baseT_Full |
				       ADVERTISED_TP);
			break;

		case SPEED_2500:
			if (cmd->duplex != DUPLEX_FULL) {
				DP(NETIF_MSG_LINK,
				   "2.5G half not supported\n");
				return -EINVAL;
			}

			if (!(bp->port.supported & SUPPORTED_2500baseX_Full)) {
				DP(NETIF_MSG_LINK,
				   "2.5G full not supported\n");
				return -EINVAL;
			}

			advertising = (ADVERTISED_2500baseX_Full |
				       ADVERTISED_TP);
			break;

		case SPEED_10000:
			if (cmd->duplex != DUPLEX_FULL) {
				DP(NETIF_MSG_LINK, "10G half not supported\n");
				return -EINVAL;
			}

			if (!(bp->port.supported & SUPPORTED_10000baseT_Full)) {
				DP(NETIF_MSG_LINK, "10G full not supported\n");
				return -EINVAL;
			}

			advertising = (ADVERTISED_10000baseT_Full |
				       ADVERTISED_FIBRE);
			break;

		default:
			DP(NETIF_MSG_LINK, "Unsupported speed\n");
			return -EINVAL;
		}

		bp->link_params.req_line_speed = cmd->speed;
		bp->link_params.req_duplex = cmd->duplex;
		bp->port.advertising = advertising;
	}

	DP(NETIF_MSG_LINK, "req_line_speed %d\n"
	   DP_LEVEL "  req_duplex %d  advertising 0x%x\n",
	   bp->link_params.req_line_speed, bp->link_params.req_duplex,
	   bp->port.advertising);

	if (netif_running(dev)) {
		bnx2x_stats_handle(bp, STATS_EVENT_STOP);
		bnx2x_link_set(bp);
	}

	return 0;
}

#define IS_E1_ONLINE(info)	(((info) & RI_E1_ONLINE) == RI_E1_ONLINE)
#define IS_E1H_ONLINE(info)	(((info) & RI_E1H_ONLINE) == RI_E1H_ONLINE)

static int bnx2x_get_regs_len(struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);
	int regdump_len = 0;
	int i;

	if (CHIP_IS_E1(bp)) {
		for (i = 0; i < REGS_COUNT; i++)
			if (IS_E1_ONLINE(reg_addrs[i].info))
				regdump_len += reg_addrs[i].size;

		for (i = 0; i < WREGS_COUNT_E1; i++)
			if (IS_E1_ONLINE(wreg_addrs_e1[i].info))
				regdump_len += wreg_addrs_e1[i].size *
					(1 + wreg_addrs_e1[i].read_regs_count);

	} else { /* E1H */
		for (i = 0; i < REGS_COUNT; i++)
			if (IS_E1H_ONLINE(reg_addrs[i].info))
				regdump_len += reg_addrs[i].size;

		for (i = 0; i < WREGS_COUNT_E1H; i++)
			if (IS_E1H_ONLINE(wreg_addrs_e1h[i].info))
				regdump_len += wreg_addrs_e1h[i].size *
					(1 + wreg_addrs_e1h[i].read_regs_count);
	}
	regdump_len *= 4;
	regdump_len += sizeof(struct dump_hdr);

	return regdump_len;
}

static void bnx2x_get_regs(struct net_device *dev,
			   struct ethtool_regs *regs, void *_p)
{
	u32 *p = _p, i, j;
	struct bnx2x *bp = netdev_priv(dev);
	struct dump_hdr dump_hdr = {0};

	regs->version = 0;
	memset(p, 0, regs->len);

	if (!netif_running(bp->dev))
		return;

	dump_hdr.hdr_size = (sizeof(struct dump_hdr) / 4) - 1;
	dump_hdr.dump_sign = dump_sign_all;
	dump_hdr.xstorm_waitp = REG_RD(bp, XSTORM_WAITP_ADDR);
	dump_hdr.tstorm_waitp = REG_RD(bp, TSTORM_WAITP_ADDR);
	dump_hdr.ustorm_waitp = REG_RD(bp, USTORM_WAITP_ADDR);
	dump_hdr.cstorm_waitp = REG_RD(bp, CSTORM_WAITP_ADDR);
	dump_hdr.info = CHIP_IS_E1(bp) ? RI_E1_ONLINE : RI_E1H_ONLINE;

	memcpy(p, &dump_hdr, sizeof(struct dump_hdr));
	p += dump_hdr.hdr_size + 1;

	if (CHIP_IS_E1(bp)) {
		for (i = 0; i < REGS_COUNT; i++)
			if (IS_E1_ONLINE(reg_addrs[i].info))
				for (j = 0; j < reg_addrs[i].size; j++)
					*p++ = REG_RD(bp,
						      reg_addrs[i].addr + j*4);

	} else { /* E1H */
		for (i = 0; i < REGS_COUNT; i++)
			if (IS_E1H_ONLINE(reg_addrs[i].info))
				for (j = 0; j < reg_addrs[i].size; j++)
					*p++ = REG_RD(bp,
						      reg_addrs[i].addr + j*4);
	}
}

#define PHY_FW_VER_LEN			10

static void bnx2x_get_drvinfo(struct net_device *dev,
			      struct ethtool_drvinfo *info)
{
	struct bnx2x *bp = netdev_priv(dev);
	u8 phy_fw_ver[PHY_FW_VER_LEN];

	strcpy(info->driver, DRV_MODULE_NAME);
	strcpy(info->version, DRV_MODULE_VERSION);

	phy_fw_ver[0] = '\0';
	if (bp->port.pmf) {
		bnx2x_acquire_phy_lock(bp);
		bnx2x_get_ext_phy_fw_version(&bp->link_params,
					     (bp->state != BNX2X_STATE_CLOSED),
					     phy_fw_ver, PHY_FW_VER_LEN);
		bnx2x_release_phy_lock(bp);
	}

	strncpy(info->fw_version, bp->fw_ver, 32);
	snprintf(info->fw_version + strlen(bp->fw_ver), 32 - strlen(bp->fw_ver),
		 "bc %d.%d.%d%s%s",
		 (bp->common.bc_ver & 0xff0000) >> 16,
		 (bp->common.bc_ver & 0xff00) >> 8,
		 (bp->common.bc_ver & 0xff),
		 ((phy_fw_ver[0] != '\0') ? " phy " : ""), phy_fw_ver);
	strcpy(info->bus_info, pci_name(bp->pdev));
	info->n_stats = BNX2X_NUM_STATS;
	info->testinfo_len = BNX2X_NUM_TESTS;
	info->eedump_len = bp->common.flash_size;
	info->regdump_len = bnx2x_get_regs_len(dev);
}

static void bnx2x_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct bnx2x *bp = netdev_priv(dev);

	if (bp->flags & NO_WOL_FLAG) {
		wol->supported = 0;
		wol->wolopts = 0;
	} else {
		wol->supported = WAKE_MAGIC;
		if (bp->wol)
			wol->wolopts = WAKE_MAGIC;
		else
			wol->wolopts = 0;
	}
	memset(&wol->sopass, 0, sizeof(wol->sopass));
}

static int bnx2x_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct bnx2x *bp = netdev_priv(dev);

	if (wol->wolopts & ~WAKE_MAGIC)
		return -EINVAL;

	if (wol->wolopts & WAKE_MAGIC) {
		if (bp->flags & NO_WOL_FLAG)
			return -EINVAL;

		bp->wol = 1;
	} else
		bp->wol = 0;

	return 0;
}

static u32 bnx2x_get_msglevel(struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);

	return bp->msg_enable;
}

static void bnx2x_set_msglevel(struct net_device *dev, u32 level)
{
	struct bnx2x *bp = netdev_priv(dev);

	if (capable(CAP_NET_ADMIN))
		bp->msg_enable = level;
}

static int bnx2x_nway_reset(struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);

	if (!bp->port.pmf)
		return 0;

	if (netif_running(dev)) {
		bnx2x_stats_handle(bp, STATS_EVENT_STOP);
		bnx2x_link_set(bp);
	}

	return 0;
}

static u32 bnx2x_get_link(struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);

	if (bp->flags & MF_FUNC_DIS)
		return 0;

	return bp->link_vars.link_up;
}

static int bnx2x_get_eeprom_len(struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);

	return bp->common.flash_size;
}

static int bnx2x_acquire_nvram_lock(struct bnx2x *bp)
{
	int port = BP_PORT(bp);
	int count, i;
	u32 val = 0;

	/* adjust timeout for emulation/FPGA */
	count = NVRAM_TIMEOUT_COUNT;
	if (CHIP_REV_IS_SLOW(bp))
		count *= 100;

	/* request access to nvram interface */
	REG_WR(bp, MCP_REG_MCPR_NVM_SW_ARB,
	       (MCPR_NVM_SW_ARB_ARB_REQ_SET1 << port));

	for (i = 0; i < count*10; i++) {
		val = REG_RD(bp, MCP_REG_MCPR_NVM_SW_ARB);
		if (val & (MCPR_NVM_SW_ARB_ARB_ARB1 << port))
			break;

		udelay(5);
	}

	if (!(val & (MCPR_NVM_SW_ARB_ARB_ARB1 << port))) {
		DP(BNX2X_MSG_NVM, "cannot get access to nvram interface\n");
		return -EBUSY;
	}

	return 0;
}

static int bnx2x_release_nvram_lock(struct bnx2x *bp)
{
	int port = BP_PORT(bp);
	int count, i;
	u32 val = 0;

	/* adjust timeout for emulation/FPGA */
	count = NVRAM_TIMEOUT_COUNT;
	if (CHIP_REV_IS_SLOW(bp))
		count *= 100;

	/* relinquish nvram interface */
	REG_WR(bp, MCP_REG_MCPR_NVM_SW_ARB,
	       (MCPR_NVM_SW_ARB_ARB_REQ_CLR1 << port));

	for (i = 0; i < count*10; i++) {
		val = REG_RD(bp, MCP_REG_MCPR_NVM_SW_ARB);
		if (!(val & (MCPR_NVM_SW_ARB_ARB_ARB1 << port)))
			break;

		udelay(5);
	}

	if (val & (MCPR_NVM_SW_ARB_ARB_ARB1 << port)) {
		DP(BNX2X_MSG_NVM, "cannot free access to nvram interface\n");
		return -EBUSY;
	}

	return 0;
}

static void bnx2x_enable_nvram_access(struct bnx2x *bp)
{
	u32 val;

	val = REG_RD(bp, MCP_REG_MCPR_NVM_ACCESS_ENABLE);

	/* enable both bits, even on read */
	REG_WR(bp, MCP_REG_MCPR_NVM_ACCESS_ENABLE,
	       (val | MCPR_NVM_ACCESS_ENABLE_EN |
		      MCPR_NVM_ACCESS_ENABLE_WR_EN));
}

static void bnx2x_disable_nvram_access(struct bnx2x *bp)
{
	u32 val;

	val = REG_RD(bp, MCP_REG_MCPR_NVM_ACCESS_ENABLE);

	/* disable both bits, even after read */
	REG_WR(bp, MCP_REG_MCPR_NVM_ACCESS_ENABLE,
	       (val & ~(MCPR_NVM_ACCESS_ENABLE_EN |
			MCPR_NVM_ACCESS_ENABLE_WR_EN)));
}

static int bnx2x_nvram_read_dword(struct bnx2x *bp, u32 offset, __be32 *ret_val,
				  u32 cmd_flags)
{
	int count, i, rc;
	u32 val;

	/* build the command word */
	cmd_flags |= MCPR_NVM_COMMAND_DOIT;

	/* need to clear DONE bit separately */
	REG_WR(bp, MCP_REG_MCPR_NVM_COMMAND, MCPR_NVM_COMMAND_DONE);

	/* address of the NVRAM to read from */
	REG_WR(bp, MCP_REG_MCPR_NVM_ADDR,
	       (offset & MCPR_NVM_ADDR_NVM_ADDR_VALUE));

	/* issue a read command */
	REG_WR(bp, MCP_REG_MCPR_NVM_COMMAND, cmd_flags);

	/* adjust timeout for emulation/FPGA */
	count = NVRAM_TIMEOUT_COUNT;
	if (CHIP_REV_IS_SLOW(bp))
		count *= 100;

	/* wait for completion */
	*ret_val = 0;
	rc = -EBUSY;
	for (i = 0; i < count; i++) {
		udelay(5);
		val = REG_RD(bp, MCP_REG_MCPR_NVM_COMMAND);

		if (val & MCPR_NVM_COMMAND_DONE) {
			val = REG_RD(bp, MCP_REG_MCPR_NVM_READ);
			/* we read nvram data in cpu order
			 * but ethtool sees it as an array of bytes
			 * converting to big-endian will do the work */
			*ret_val = cpu_to_be32(val);
			rc = 0;
			break;
		}
	}

	return rc;
}

static int bnx2x_nvram_read(struct bnx2x *bp, u32 offset, u8 *ret_buf,
			    int buf_size)
{
	int rc;
	u32 cmd_flags;
	__be32 val;

	if ((offset & 0x03) || (buf_size & 0x03) || (buf_size == 0)) {
		DP(BNX2X_MSG_NVM,
		   "Invalid parameter: offset 0x%x  buf_size 0x%x\n",
		   offset, buf_size);
		return -EINVAL;
	}

	if (offset + buf_size > bp->common.flash_size) {
		DP(BNX2X_MSG_NVM, "Invalid parameter: offset (0x%x) +"
				  " buf_size (0x%x) > flash_size (0x%x)\n",
		   offset, buf_size, bp->common.flash_size);
		return -EINVAL;
	}

	/* request access to nvram interface */
	rc = bnx2x_acquire_nvram_lock(bp);
	if (rc)
		return rc;

	/* enable access to nvram interface */
	bnx2x_enable_nvram_access(bp);

	/* read the first word(s) */
	cmd_flags = MCPR_NVM_COMMAND_FIRST;
	while ((buf_size > sizeof(u32)) && (rc == 0)) {
		rc = bnx2x_nvram_read_dword(bp, offset, &val, cmd_flags);
		memcpy(ret_buf, &val, 4);

		/* advance to the next dword */
		offset += sizeof(u32);
		ret_buf += sizeof(u32);
		buf_size -= sizeof(u32);
		cmd_flags = 0;
	}

	if (rc == 0) {
		cmd_flags |= MCPR_NVM_COMMAND_LAST;
		rc = bnx2x_nvram_read_dword(bp, offset, &val, cmd_flags);
		memcpy(ret_buf, &val, 4);
	}

	/* disable access to nvram interface */
	bnx2x_disable_nvram_access(bp);
	bnx2x_release_nvram_lock(bp);

	return rc;
}

static int bnx2x_get_eeprom(struct net_device *dev,
			    struct ethtool_eeprom *eeprom, u8 *eebuf)
{
	struct bnx2x *bp = netdev_priv(dev);
	int rc;

	if (!netif_running(dev))
		return -EAGAIN;

	DP(BNX2X_MSG_NVM, "ethtool_eeprom: cmd %d\n"
	   DP_LEVEL "  magic 0x%x  offset 0x%x (%d)  len 0x%x (%d)\n",
	   eeprom->cmd, eeprom->magic, eeprom->offset, eeprom->offset,
	   eeprom->len, eeprom->len);

	/* parameters already validated in ethtool_get_eeprom */

	rc = bnx2x_nvram_read(bp, eeprom->offset, eebuf, eeprom->len);

	return rc;
}

static int bnx2x_nvram_write_dword(struct bnx2x *bp, u32 offset, u32 val,
				   u32 cmd_flags)
{
	int count, i, rc;

	/* build the command word */
	cmd_flags |= MCPR_NVM_COMMAND_DOIT | MCPR_NVM_COMMAND_WR;

	/* need to clear DONE bit separately */
	REG_WR(bp, MCP_REG_MCPR_NVM_COMMAND, MCPR_NVM_COMMAND_DONE);

	/* write the data */
	REG_WR(bp, MCP_REG_MCPR_NVM_WRITE, val);

	/* address of the NVRAM to write to */
	REG_WR(bp, MCP_REG_MCPR_NVM_ADDR,
	       (offset & MCPR_NVM_ADDR_NVM_ADDR_VALUE));

	/* issue the write command */
	REG_WR(bp, MCP_REG_MCPR_NVM_COMMAND, cmd_flags);

	/* adjust timeout for emulation/FPGA */
	count = NVRAM_TIMEOUT_COUNT;
	if (CHIP_REV_IS_SLOW(bp))
		count *= 100;

	/* wait for completion */
	rc = -EBUSY;
	for (i = 0; i < count; i++) {
		udelay(5);
		val = REG_RD(bp, MCP_REG_MCPR_NVM_COMMAND);
		if (val & MCPR_NVM_COMMAND_DONE) {
			rc = 0;
			break;
		}
	}

	return rc;
}

#define BYTE_OFFSET(offset)		(8 * (offset & 0x03))

static int bnx2x_nvram_write1(struct bnx2x *bp, u32 offset, u8 *data_buf,
			      int buf_size)
{
	int rc;
	u32 cmd_flags;
	u32 align_offset;
	__be32 val;

	if (offset + buf_size > bp->common.flash_size) {
		DP(BNX2X_MSG_NVM, "Invalid parameter: offset (0x%x) +"
				  " buf_size (0x%x) > flash_size (0x%x)\n",
		   offset, buf_size, bp->common.flash_size);
		return -EINVAL;
	}

	/* request access to nvram interface */
	rc = bnx2x_acquire_nvram_lock(bp);
	if (rc)
		return rc;

	/* enable access to nvram interface */
	bnx2x_enable_nvram_access(bp);

	cmd_flags = (MCPR_NVM_COMMAND_FIRST | MCPR_NVM_COMMAND_LAST);
	align_offset = (offset & ~0x03);
	rc = bnx2x_nvram_read_dword(bp, align_offset, &val, cmd_flags);

	if (rc == 0) {
		val &= ~(0xff << BYTE_OFFSET(offset));
		val |= (*data_buf << BYTE_OFFSET(offset));

		/* nvram data is returned as an array of bytes
		 * convert it back to cpu order */
		val = be32_to_cpu(val);

		rc = bnx2x_nvram_write_dword(bp, align_offset, val,
					     cmd_flags);
	}

	/* disable access to nvram interface */
	bnx2x_disable_nvram_access(bp);
	bnx2x_release_nvram_lock(bp);

	return rc;
}

static int bnx2x_nvram_write(struct bnx2x *bp, u32 offset, u8 *data_buf,
			     int buf_size)
{
	int rc;
	u32 cmd_flags;
	u32 val;
	u32 written_so_far;

	if (buf_size == 1)	/* ethtool */
		return bnx2x_nvram_write1(bp, offset, data_buf, buf_size);

	if ((offset & 0x03) || (buf_size & 0x03) || (buf_size == 0)) {
		DP(BNX2X_MSG_NVM,
		   "Invalid parameter: offset 0x%x  buf_size 0x%x\n",
		   offset, buf_size);
		return -EINVAL;
	}

	if (offset + buf_size > bp->common.flash_size) {
		DP(BNX2X_MSG_NVM, "Invalid parameter: offset (0x%x) +"
				  " buf_size (0x%x) > flash_size (0x%x)\n",
		   offset, buf_size, bp->common.flash_size);
		return -EINVAL;
	}

	/* request access to nvram interface */
	rc = bnx2x_acquire_nvram_lock(bp);
	if (rc)
		return rc;

	/* enable access to nvram interface */
	bnx2x_enable_nvram_access(bp);

	written_so_far = 0;
	cmd_flags = MCPR_NVM_COMMAND_FIRST;
	while ((written_so_far < buf_size) && (rc == 0)) {
		if (written_so_far == (buf_size - sizeof(u32)))
			cmd_flags |= MCPR_NVM_COMMAND_LAST;
		else if (((offset + 4) % NVRAM_PAGE_SIZE) == 0)
			cmd_flags |= MCPR_NVM_COMMAND_LAST;
		else if ((offset % NVRAM_PAGE_SIZE) == 0)
			cmd_flags |= MCPR_NVM_COMMAND_FIRST;

		memcpy(&val, data_buf, 4);

		rc = bnx2x_nvram_write_dword(bp, offset, val, cmd_flags);

		/* advance to the next dword */
		offset += sizeof(u32);
		data_buf += sizeof(u32);
		written_so_far += sizeof(u32);
		cmd_flags = 0;
	}

	/* disable access to nvram interface */
	bnx2x_disable_nvram_access(bp);
	bnx2x_release_nvram_lock(bp);

	return rc;
}

static int bnx2x_set_eeprom(struct net_device *dev,
			    struct ethtool_eeprom *eeprom, u8 *eebuf)
{
	struct bnx2x *bp = netdev_priv(dev);
	int port = BP_PORT(bp);
	int rc = 0;

	if (!netif_running(dev))
		return -EAGAIN;

	DP(BNX2X_MSG_NVM, "ethtool_eeprom: cmd %d\n"
	   DP_LEVEL "  magic 0x%x  offset 0x%x (%d)  len 0x%x (%d)\n",
	   eeprom->cmd, eeprom->magic, eeprom->offset, eeprom->offset,
	   eeprom->len, eeprom->len);

	/* parameters already validated in ethtool_set_eeprom */

	/* PHY eeprom can be accessed only by the PMF */
	if ((eeprom->magic >= 0x50485900) && (eeprom->magic <= 0x504859FF) &&
	    !bp->port.pmf)
		return -EINVAL;

	if (eeprom->magic == 0x50485950) {
		/* 'PHYP' (0x50485950): prepare phy for FW upgrade */
		bnx2x_stats_handle(bp, STATS_EVENT_STOP);

		bnx2x_acquire_phy_lock(bp);
		rc |= bnx2x_link_reset(&bp->link_params,
				       &bp->link_vars, 0);
		if (XGXS_EXT_PHY_TYPE(bp->link_params.ext_phy_config) ==
					PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101)
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_0,
				       MISC_REGISTERS_GPIO_HIGH, port);
		bnx2x_release_phy_lock(bp);
		bnx2x_link_report(bp);

	} else if (eeprom->magic == 0x50485952) {
		/* 'PHYR' (0x50485952): re-init link after FW upgrade */
		if (bp->state == BNX2X_STATE_OPEN) {
			bnx2x_acquire_phy_lock(bp);
			rc |= bnx2x_link_reset(&bp->link_params,
					       &bp->link_vars, 1);

			rc |= bnx2x_phy_init(&bp->link_params,
					     &bp->link_vars);
			bnx2x_release_phy_lock(bp);
			bnx2x_calc_fc_adv(bp);
		}
	} else if (eeprom->magic == 0x53985943) {
		/* 'PHYC' (0x53985943): PHY FW upgrade completed */
		if (XGXS_EXT_PHY_TYPE(bp->link_params.ext_phy_config) ==
				       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101) {
			u8 ext_phy_addr =
			     XGXS_EXT_PHY_ADDR(bp->link_params.ext_phy_config);

			/* DSP Remove Download Mode */
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_0,
				       MISC_REGISTERS_GPIO_LOW, port);

			bnx2x_acquire_phy_lock(bp);

			bnx2x_sfx7101_sp_sw_reset(bp, port, ext_phy_addr);

			/* wait 0.5 sec to allow it to run */
			msleep(500);
			bnx2x_ext_phy_hw_reset(bp, port);
			msleep(500);
			bnx2x_release_phy_lock(bp);
		}
	} else
		rc = bnx2x_nvram_write(bp, eeprom->offset, eebuf, eeprom->len);

	return rc;
}
static int bnx2x_get_coalesce(struct net_device *dev,
			      struct ethtool_coalesce *coal)
{
	struct bnx2x *bp = netdev_priv(dev);

	memset(coal, 0, sizeof(struct ethtool_coalesce));

	coal->rx_coalesce_usecs = bp->rx_ticks;
	coal->tx_coalesce_usecs = bp->tx_ticks;

	return 0;
}

static int bnx2x_set_coalesce(struct net_device *dev,
			      struct ethtool_coalesce *coal)
{
	struct bnx2x *bp = netdev_priv(dev);

	bp->rx_ticks = (u16)coal->rx_coalesce_usecs;
	if (bp->rx_ticks > BNX2X_MAX_COALESCE_TOUT)
		bp->rx_ticks = BNX2X_MAX_COALESCE_TOUT;

	bp->tx_ticks = (u16)coal->tx_coalesce_usecs;
	if (bp->tx_ticks > BNX2X_MAX_COALESCE_TOUT)
		bp->tx_ticks = BNX2X_MAX_COALESCE_TOUT;

	if (netif_running(dev))
		bnx2x_update_coalesce(bp);

	return 0;
}

static void bnx2x_get_ringparam(struct net_device *dev,
				struct ethtool_ringparam *ering)
{
	struct bnx2x *bp = netdev_priv(dev);

	ering->rx_max_pending = MAX_RX_AVAIL;
	ering->rx_mini_max_pending = 0;
	ering->rx_jumbo_max_pending = 0;

	ering->rx_pending = bp->rx_ring_size;
	ering->rx_mini_pending = 0;
	ering->rx_jumbo_pending = 0;

	ering->tx_max_pending = MAX_TX_AVAIL;
	ering->tx_pending = bp->tx_ring_size;
}

static int bnx2x_set_ringparam(struct net_device *dev,
			       struct ethtool_ringparam *ering)
{
	struct bnx2x *bp = netdev_priv(dev);
	int rc = 0;

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		printk(KERN_ERR "Handling parity error recovery. Try again later\n");
		return -EAGAIN;
	}

	if ((ering->rx_pending > MAX_RX_AVAIL) ||
	    (ering->tx_pending > MAX_TX_AVAIL) ||
	    (ering->tx_pending <= MAX_SKB_FRAGS + 4))
		return -EINVAL;

	bp->rx_ring_size = ering->rx_pending;
	bp->tx_ring_size = ering->tx_pending;

	if (netif_running(dev)) {
		bnx2x_nic_unload(bp, UNLOAD_NORMAL);
		rc = bnx2x_nic_load(bp, LOAD_NORMAL);
	}

	return rc;
}

static void bnx2x_get_pauseparam(struct net_device *dev,
				 struct ethtool_pauseparam *epause)
{
	struct bnx2x *bp = netdev_priv(dev);

	epause->autoneg = (bp->link_params.req_flow_ctrl ==
			   BNX2X_FLOW_CTRL_AUTO) &&
			  (bp->link_params.req_line_speed == SPEED_AUTO_NEG);

	epause->rx_pause = ((bp->link_vars.flow_ctrl & BNX2X_FLOW_CTRL_RX) ==
			    BNX2X_FLOW_CTRL_RX);
	epause->tx_pause = ((bp->link_vars.flow_ctrl & BNX2X_FLOW_CTRL_TX) ==
			    BNX2X_FLOW_CTRL_TX);

	DP(NETIF_MSG_LINK, "ethtool_pauseparam: cmd %d\n"
	   DP_LEVEL "  autoneg %d  rx_pause %d  tx_pause %d\n",
	   epause->cmd, epause->autoneg, epause->rx_pause, epause->tx_pause);
}

static int bnx2x_set_pauseparam(struct net_device *dev,
				struct ethtool_pauseparam *epause)
{
	struct bnx2x *bp = netdev_priv(dev);

	if (IS_E1HMF(bp))
		return 0;

	DP(NETIF_MSG_LINK, "ethtool_pauseparam: cmd %d\n"
	   DP_LEVEL "  autoneg %d  rx_pause %d  tx_pause %d\n",
	   epause->cmd, epause->autoneg, epause->rx_pause, epause->tx_pause);

	bp->link_params.req_flow_ctrl = BNX2X_FLOW_CTRL_AUTO;

	if (epause->rx_pause)
		bp->link_params.req_flow_ctrl |= BNX2X_FLOW_CTRL_RX;

	if (epause->tx_pause)
		bp->link_params.req_flow_ctrl |= BNX2X_FLOW_CTRL_TX;

	if (bp->link_params.req_flow_ctrl == BNX2X_FLOW_CTRL_AUTO)
		bp->link_params.req_flow_ctrl = BNX2X_FLOW_CTRL_NONE;

	if (epause->autoneg) {
		if (!(bp->port.supported & SUPPORTED_Autoneg)) {
			DP(NETIF_MSG_LINK, "autoneg not supported\n");
			return -EINVAL;
		}

		if (bp->link_params.req_line_speed == SPEED_AUTO_NEG)
			bp->link_params.req_flow_ctrl = BNX2X_FLOW_CTRL_AUTO;
	}

	DP(NETIF_MSG_LINK,
	   "req_flow_ctrl 0x%x\n", bp->link_params.req_flow_ctrl);

	if (netif_running(dev)) {
		bnx2x_stats_handle(bp, STATS_EVENT_STOP);
		bnx2x_link_set(bp);
	}

	return 0;
}

static int bnx2x_set_flags(struct net_device *dev, u32 data)
{
	struct bnx2x *bp = netdev_priv(dev);
	int changed = 0;
	int rc = 0;

	if (data & ~(ETH_FLAG_LRO | ETH_FLAG_RXHASH))
		return -EINVAL;

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		printk(KERN_ERR "Handling parity error recovery. Try again later\n");
		return -EAGAIN;
	}

	/* TPA requires Rx CSUM offloading */
	if ((data & ETH_FLAG_LRO) && bp->rx_csum) {
		if (!bp->disable_tpa) {
			if (!(dev->features & NETIF_F_LRO)) {
				dev->features |= NETIF_F_LRO;
				bp->flags |= TPA_ENABLE_FLAG;
				changed = 1;
			}
		} else
			rc = -EINVAL;
	} else if (dev->features & NETIF_F_LRO) {
		dev->features &= ~NETIF_F_LRO;
		bp->flags &= ~TPA_ENABLE_FLAG;
		changed = 1;
	}

	if (data & ETH_FLAG_RXHASH)
		dev->features |= NETIF_F_RXHASH;
	else
		dev->features &= ~NETIF_F_RXHASH;

	if (changed && netif_running(dev)) {
		bnx2x_nic_unload(bp, UNLOAD_NORMAL);
		rc = bnx2x_nic_load(bp, LOAD_NORMAL);
	}

	return rc;
}

static u32 bnx2x_get_rx_csum(struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);

	return bp->rx_csum;
}

static int bnx2x_set_rx_csum(struct net_device *dev, u32 data)
{
	struct bnx2x *bp = netdev_priv(dev);
	int rc = 0;

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		printk(KERN_ERR "Handling parity error recovery. Try again later\n");
		return -EAGAIN;
	}

	bp->rx_csum = data;

	/* Disable TPA, when Rx CSUM is disabled. Otherwise all
	   TPA'ed packets will be discarded due to wrong TCP CSUM */
	if (!data) {
		u32 flags = ethtool_op_get_flags(dev);

		rc = bnx2x_set_flags(dev, (flags & ~ETH_FLAG_LRO));
	}

	return rc;
}

static int bnx2x_set_tso(struct net_device *dev, u32 data)
{
	if (data) {
		dev->features |= (NETIF_F_TSO | NETIF_F_TSO_ECN);
		dev->features |= NETIF_F_TSO6;
	} else {
		dev->features &= ~(NETIF_F_TSO | NETIF_F_TSO_ECN);
		dev->features &= ~NETIF_F_TSO6;
	}

	return 0;
}

static const struct {
	char string[ETH_GSTRING_LEN];
} bnx2x_tests_str_arr[BNX2X_NUM_TESTS] = {
	{ "register_test (offline)" },
	{ "memory_test (offline)" },
	{ "loopback_test (offline)" },
	{ "nvram_test (online)" },
	{ "interrupt_test (online)" },
	{ "link_test (online)" },
	{ "idle check (online)" }
};

static int bnx2x_test_registers(struct bnx2x *bp)
{
	int idx, i, rc = -ENODEV;
	u32 wr_val = 0;
	int port = BP_PORT(bp);
	static const struct {
		u32 offset0;
		u32 offset1;
		u32 mask;
	} reg_tbl[] = {
/* 0 */		{ BRB1_REG_PAUSE_LOW_THRESHOLD_0,      4, 0x000003ff },
		{ DORQ_REG_DB_ADDR0,                   4, 0xffffffff },
		{ HC_REG_AGG_INT_0,                    4, 0x000003ff },
		{ PBF_REG_MAC_IF0_ENABLE,              4, 0x00000001 },
		{ PBF_REG_P0_INIT_CRD,                 4, 0x000007ff },
		{ PRS_REG_CID_PORT_0,                  4, 0x00ffffff },
		{ PXP2_REG_PSWRQ_CDU0_L2P,             4, 0x000fffff },
		{ PXP2_REG_RQ_CDU0_EFIRST_MEM_ADDR,    8, 0x0003ffff },
		{ PXP2_REG_PSWRQ_TM0_L2P,              4, 0x000fffff },
		{ PXP2_REG_RQ_USDM0_EFIRST_MEM_ADDR,   8, 0x0003ffff },
/* 10 */	{ PXP2_REG_PSWRQ_TSDM0_L2P,            4, 0x000fffff },
		{ QM_REG_CONNNUM_0,                    4, 0x000fffff },
		{ TM_REG_LIN0_MAX_ACTIVE_CID,          4, 0x0003ffff },
		{ SRC_REG_KEYRSS0_0,                  40, 0xffffffff },
		{ SRC_REG_KEYRSS0_7,                  40, 0xffffffff },
		{ XCM_REG_WU_DA_SET_TMR_CNT_FLG_CMD00, 4, 0x00000001 },
		{ XCM_REG_WU_DA_CNT_CMD00,             4, 0x00000003 },
		{ XCM_REG_GLB_DEL_ACK_MAX_CNT_0,       4, 0x000000ff },
		{ NIG_REG_LLH0_T_BIT,                  4, 0x00000001 },
		{ NIG_REG_EMAC0_IN_EN,                 4, 0x00000001 },
/* 20 */	{ NIG_REG_BMAC0_IN_EN,                 4, 0x00000001 },
		{ NIG_REG_XCM0_OUT_EN,                 4, 0x00000001 },
		{ NIG_REG_BRB0_OUT_EN,                 4, 0x00000001 },
		{ NIG_REG_LLH0_XCM_MASK,               4, 0x00000007 },
		{ NIG_REG_LLH0_ACPI_PAT_6_LEN,        68, 0x000000ff },
		{ NIG_REG_LLH0_ACPI_PAT_0_CRC,        68, 0xffffffff },
		{ NIG_REG_LLH0_DEST_MAC_0_0,         160, 0xffffffff },
		{ NIG_REG_LLH0_DEST_IP_0_1,          160, 0xffffffff },
		{ NIG_REG_LLH0_IPV4_IPV6_0,          160, 0x00000001 },
		{ NIG_REG_LLH0_DEST_UDP_0,           160, 0x0000ffff },
/* 30 */	{ NIG_REG_LLH0_DEST_TCP_0,           160, 0x0000ffff },
		{ NIG_REG_LLH0_VLAN_ID_0,            160, 0x00000fff },
		{ NIG_REG_XGXS_SERDES0_MODE_SEL,       4, 0x00000001 },
		{ NIG_REG_LED_CONTROL_OVERRIDE_TRAFFIC_P0, 4, 0x00000001 },
		{ NIG_REG_STATUS_INTERRUPT_PORT0,      4, 0x07ffffff },
		{ NIG_REG_XGXS0_CTRL_EXTREMOTEMDIOST, 24, 0x00000001 },
		{ NIG_REG_SERDES0_CTRL_PHY_ADDR,      16, 0x0000001f },

		{ 0xffffffff, 0, 0x00000000 }
	};

	if (!netif_running(bp->dev))
		return rc;

	/* Repeat the test twice:
	   First by writing 0x00000000, second by writing 0xffffffff */
	for (idx = 0; idx < 2; idx++) {

		switch (idx) {
		case 0:
			wr_val = 0;
			break;
		case 1:
			wr_val = 0xffffffff;
			break;
		}

		for (i = 0; reg_tbl[i].offset0 != 0xffffffff; i++) {
			u32 offset, mask, save_val, val;

			offset = reg_tbl[i].offset0 + port*reg_tbl[i].offset1;
			mask = reg_tbl[i].mask;

			save_val = REG_RD(bp, offset);

			REG_WR(bp, offset, (wr_val & mask));
			val = REG_RD(bp, offset);

			/* Restore the original register's value */
			REG_WR(bp, offset, save_val);

			/* verify value is as expected */
			if ((val & mask) != (wr_val & mask)) {
				DP(NETIF_MSG_PROBE,
				   "offset 0x%x: val 0x%x != 0x%x mask 0x%x\n",
				   offset, val, wr_val, mask);
				goto test_reg_exit;
			}
		}
	}

	rc = 0;

test_reg_exit:
	return rc;
}

static int bnx2x_test_memory(struct bnx2x *bp)
{
	int i, j, rc = -ENODEV;
	u32 val;
	static const struct {
		u32 offset;
		int size;
	} mem_tbl[] = {
		{ CCM_REG_XX_DESCR_TABLE,   CCM_REG_XX_DESCR_TABLE_SIZE },
		{ CFC_REG_ACTIVITY_COUNTER, CFC_REG_ACTIVITY_COUNTER_SIZE },
		{ CFC_REG_LINK_LIST,        CFC_REG_LINK_LIST_SIZE },
		{ DMAE_REG_CMD_MEM,         DMAE_REG_CMD_MEM_SIZE },
		{ TCM_REG_XX_DESCR_TABLE,   TCM_REG_XX_DESCR_TABLE_SIZE },
		{ UCM_REG_XX_DESCR_TABLE,   UCM_REG_XX_DESCR_TABLE_SIZE },
		{ XCM_REG_XX_DESCR_TABLE,   XCM_REG_XX_DESCR_TABLE_SIZE },

		{ 0xffffffff, 0 }
	};
	static const struct {
		char *name;
		u32 offset;
		u32 e1_mask;
		u32 e1h_mask;
	} prty_tbl[] = {
		{ "CCM_PRTY_STS",  CCM_REG_CCM_PRTY_STS,   0x3ffc0, 0 },
		{ "CFC_PRTY_STS",  CFC_REG_CFC_PRTY_STS,   0x2,     0x2 },
		{ "DMAE_PRTY_STS", DMAE_REG_DMAE_PRTY_STS, 0,       0 },
		{ "TCM_PRTY_STS",  TCM_REG_TCM_PRTY_STS,   0x3ffc0, 0 },
		{ "UCM_PRTY_STS",  UCM_REG_UCM_PRTY_STS,   0x3ffc0, 0 },
		{ "XCM_PRTY_STS",  XCM_REG_XCM_PRTY_STS,   0x3ffc1, 0 },

		{ NULL, 0xffffffff, 0, 0 }
	};

	if (!netif_running(bp->dev))
		return rc;

	/* Go through all the memories */
	for (i = 0; mem_tbl[i].offset != 0xffffffff; i++)
		for (j = 0; j < mem_tbl[i].size; j++)
			REG_RD(bp, mem_tbl[i].offset + j*4);

	/* Check the parity status */
	for (i = 0; prty_tbl[i].offset != 0xffffffff; i++) {
		val = REG_RD(bp, prty_tbl[i].offset);
		if ((CHIP_IS_E1(bp) && (val & ~(prty_tbl[i].e1_mask))) ||
		    (CHIP_IS_E1H(bp) && (val & ~(prty_tbl[i].e1h_mask)))) {
			DP(NETIF_MSG_HW,
			   "%s is 0x%x\n", prty_tbl[i].name, val);
			goto test_mem_exit;
		}
	}

	rc = 0;

test_mem_exit:
	return rc;
}

static void bnx2x_wait_for_link(struct bnx2x *bp, u8 link_up)
{
	int cnt = 1000;

	if (link_up)
		while (bnx2x_link_test(bp) && cnt--)
			msleep(10);
}

static int bnx2x_run_loopback(struct bnx2x *bp, int loopback_mode, u8 link_up)
{
	unsigned int pkt_size, num_pkts, i;
	struct sk_buff *skb;
	unsigned char *packet;
	struct bnx2x_fastpath *fp_rx = &bp->fp[0];
	struct bnx2x_fastpath *fp_tx = &bp->fp[0];
	u16 tx_start_idx, tx_idx;
	u16 rx_start_idx, rx_idx;
	u16 pkt_prod, bd_prod;
	struct sw_tx_bd *tx_buf;
	struct eth_tx_start_bd *tx_start_bd;
	struct eth_tx_parse_bd *pbd = NULL;
	dma_addr_t mapping;
	union eth_rx_cqe *cqe;
	u8 cqe_fp_flags;
	struct sw_rx_bd *rx_buf;
	u16 len;
	int rc = -ENODEV;

	/* check the loopback mode */
	switch (loopback_mode) {
	case BNX2X_PHY_LOOPBACK:
		if (bp->link_params.loopback_mode != LOOPBACK_XGXS_10)
			return -EINVAL;
		break;
	case BNX2X_MAC_LOOPBACK:
		bp->link_params.loopback_mode = LOOPBACK_BMAC;
		bnx2x_phy_init(&bp->link_params, &bp->link_vars);
		break;
	default:
		return -EINVAL;
	}

	/* prepare the loopback packet */
	pkt_size = (((bp->dev->mtu < ETH_MAX_PACKET_SIZE) ?
		     bp->dev->mtu : ETH_MAX_PACKET_SIZE) + ETH_HLEN);
	skb = netdev_alloc_skb(bp->dev, bp->rx_buf_size);
	if (!skb) {
		rc = -ENOMEM;
		goto test_loopback_exit;
	}
	packet = skb_put(skb, pkt_size);
	memcpy(packet, bp->dev->dev_addr, ETH_ALEN);
	memset(packet + ETH_ALEN, 0, ETH_ALEN);
	memset(packet + 2*ETH_ALEN, 0x77, (ETH_HLEN - 2*ETH_ALEN));
	for (i = ETH_HLEN; i < pkt_size; i++)
		packet[i] = (unsigned char) (i & 0xff);

	/* send the loopback packet */
	num_pkts = 0;
	tx_start_idx = le16_to_cpu(*fp_tx->tx_cons_sb);
	rx_start_idx = le16_to_cpu(*fp_rx->rx_cons_sb);

	pkt_prod = fp_tx->tx_pkt_prod++;
	tx_buf = &fp_tx->tx_buf_ring[TX_BD(pkt_prod)];
	tx_buf->first_bd = fp_tx->tx_bd_prod;
	tx_buf->skb = skb;
	tx_buf->flags = 0;

	bd_prod = TX_BD(fp_tx->tx_bd_prod);
	tx_start_bd = &fp_tx->tx_desc_ring[bd_prod].start_bd;
	mapping = dma_map_single(&bp->pdev->dev, skb->data,
				 skb_headlen(skb), DMA_TO_DEVICE);
	tx_start_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	tx_start_bd->addr_lo = cpu_to_le32(U64_LO(mapping));
	tx_start_bd->nbd = cpu_to_le16(2); /* start + pbd */
	tx_start_bd->nbytes = cpu_to_le16(skb_headlen(skb));
	tx_start_bd->vlan = cpu_to_le16(pkt_prod);
	tx_start_bd->bd_flags.as_bitfield = ETH_TX_BD_FLAGS_START_BD;
	tx_start_bd->general_data = ((UNICAST_ADDRESS <<
				ETH_TX_START_BD_ETH_ADDR_TYPE_SHIFT) | 1);

	/* turn on parsing and get a BD */
	bd_prod = TX_BD(NEXT_TX_IDX(bd_prod));
	pbd = &fp_tx->tx_desc_ring[bd_prod].parse_bd;

	memset(pbd, 0, sizeof(struct eth_tx_parse_bd));

	wmb();

	fp_tx->tx_db.data.prod += 2;
	barrier();
	DOORBELL(bp, fp_tx->index, fp_tx->tx_db.raw);

	mmiowb();

	num_pkts++;
	fp_tx->tx_bd_prod += 2; /* start + pbd */

	udelay(100);

	tx_idx = le16_to_cpu(*fp_tx->tx_cons_sb);
	if (tx_idx != tx_start_idx + num_pkts)
		goto test_loopback_exit;

	rx_idx = le16_to_cpu(*fp_rx->rx_cons_sb);
	if (rx_idx != rx_start_idx + num_pkts)
		goto test_loopback_exit;

	cqe = &fp_rx->rx_comp_ring[RCQ_BD(fp_rx->rx_comp_cons)];
	cqe_fp_flags = cqe->fast_path_cqe.type_error_flags;
	if (CQE_TYPE(cqe_fp_flags) || (cqe_fp_flags & ETH_RX_ERROR_FALGS))
		goto test_loopback_rx_exit;

	len = le16_to_cpu(cqe->fast_path_cqe.pkt_len);
	if (len != pkt_size)
		goto test_loopback_rx_exit;

	rx_buf = &fp_rx->rx_buf_ring[RX_BD(fp_rx->rx_bd_cons)];
	skb = rx_buf->skb;
	skb_reserve(skb, cqe->fast_path_cqe.placement_offset);
	for (i = ETH_HLEN; i < pkt_size; i++)
		if (*(skb->data + i) != (unsigned char) (i & 0xff))
			goto test_loopback_rx_exit;

	rc = 0;

test_loopback_rx_exit:

	fp_rx->rx_bd_cons = NEXT_RX_IDX(fp_rx->rx_bd_cons);
	fp_rx->rx_bd_prod = NEXT_RX_IDX(fp_rx->rx_bd_prod);
	fp_rx->rx_comp_cons = NEXT_RCQ_IDX(fp_rx->rx_comp_cons);
	fp_rx->rx_comp_prod = NEXT_RCQ_IDX(fp_rx->rx_comp_prod);

	/* Update producers */
	bnx2x_update_rx_prod(bp, fp_rx, fp_rx->rx_bd_prod, fp_rx->rx_comp_prod,
			     fp_rx->rx_sge_prod);

test_loopback_exit:
	bp->link_params.loopback_mode = LOOPBACK_NONE;

	return rc;
}

static int bnx2x_test_loopback(struct bnx2x *bp, u8 link_up)
{
	int rc = 0, res;

	if (BP_NOMCP(bp))
		return rc;

	if (!netif_running(bp->dev))
		return BNX2X_LOOPBACK_FAILED;

	bnx2x_netif_stop(bp, 1);
	bnx2x_acquire_phy_lock(bp);

	res = bnx2x_run_loopback(bp, BNX2X_PHY_LOOPBACK, link_up);
	if (res) {
		DP(NETIF_MSG_PROBE, "  PHY loopback failed  (res %d)\n", res);
		rc |= BNX2X_PHY_LOOPBACK_FAILED;
	}

	res = bnx2x_run_loopback(bp, BNX2X_MAC_LOOPBACK, link_up);
	if (res) {
		DP(NETIF_MSG_PROBE, "  MAC loopback failed  (res %d)\n", res);
		rc |= BNX2X_MAC_LOOPBACK_FAILED;
	}

	bnx2x_release_phy_lock(bp);
	bnx2x_netif_start(bp);

	return rc;
}

#define CRC32_RESIDUAL			0xdebb20e3

static int bnx2x_test_nvram(struct bnx2x *bp)
{
	static const struct {
		int offset;
		int size;
	} nvram_tbl[] = {
		{     0,  0x14 }, /* bootstrap */
		{  0x14,  0xec }, /* dir */
		{ 0x100, 0x350 }, /* manuf_info */
		{ 0x450,  0xf0 }, /* feature_info */
		{ 0x640,  0x64 }, /* upgrade_key_info */
		{ 0x6a4,  0x64 },
		{ 0x708,  0x70 }, /* manuf_key_info */
		{ 0x778,  0x70 },
		{     0,     0 }
	};
	__be32 buf[0x350 / 4];
	u8 *data = (u8 *)buf;
	int i, rc;
	u32 magic, crc;

	if (BP_NOMCP(bp))
		return 0;

	rc = bnx2x_nvram_read(bp, 0, data, 4);
	if (rc) {
		DP(NETIF_MSG_PROBE, "magic value read (rc %d)\n", rc);
		goto test_nvram_exit;
	}

	magic = be32_to_cpu(buf[0]);
	if (magic != 0x669955aa) {
		DP(NETIF_MSG_PROBE, "magic value (0x%08x)\n", magic);
		rc = -ENODEV;
		goto test_nvram_exit;
	}

	for (i = 0; nvram_tbl[i].size; i++) {

		rc = bnx2x_nvram_read(bp, nvram_tbl[i].offset, data,
				      nvram_tbl[i].size);
		if (rc) {
			DP(NETIF_MSG_PROBE,
			   "nvram_tbl[%d] read data (rc %d)\n", i, rc);
			goto test_nvram_exit;
		}

		crc = ether_crc_le(nvram_tbl[i].size, data);
		if (crc != CRC32_RESIDUAL) {
			DP(NETIF_MSG_PROBE,
			   "nvram_tbl[%d] crc value (0x%08x)\n", i, crc);
			rc = -ENODEV;
			goto test_nvram_exit;
		}
	}

test_nvram_exit:
	return rc;
}

static int bnx2x_test_intr(struct bnx2x *bp)
{
	struct mac_configuration_cmd *config = bnx2x_sp(bp, mac_config);
	int i, rc;

	if (!netif_running(bp->dev))
		return -ENODEV;

	config->hdr.length = 0;
	if (CHIP_IS_E1(bp))
		/* use last unicast entries */
		config->hdr.offset = (BP_PORT(bp) ? 63 : 31);
	else
		config->hdr.offset = BP_FUNC(bp);
	config->hdr.client_id = bp->fp->cl_id;
	config->hdr.reserved1 = 0;

	bp->set_mac_pending++;
	smp_wmb();
	rc = bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_SET_MAC, 0,
			   U64_HI(bnx2x_sp_mapping(bp, mac_config)),
			   U64_LO(bnx2x_sp_mapping(bp, mac_config)), 0);
	if (rc == 0) {
		for (i = 0; i < 10; i++) {
			if (!bp->set_mac_pending)
				break;
			smp_rmb();
			msleep_interruptible(10);
		}
		if (i == 10)
			rc = -ENODEV;
	}

	return rc;
}

static void bnx2x_self_test(struct net_device *dev,
			    struct ethtool_test *etest, u64 *buf)
{
	struct bnx2x *bp = netdev_priv(dev);

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		printk(KERN_ERR "Handling parity error recovery. Try again later\n");
		etest->flags |= ETH_TEST_FL_FAILED;
		return;
	}

	memset(buf, 0, sizeof(u64) * BNX2X_NUM_TESTS);

	if (!netif_running(dev))
		return;

	/* offline tests are not supported in MF mode */
	if (IS_E1HMF(bp))
		etest->flags &= ~ETH_TEST_FL_OFFLINE;

	if (etest->flags & ETH_TEST_FL_OFFLINE) {
		int port = BP_PORT(bp);
		u32 val;
		u8 link_up;

		/* save current value of input enable for TX port IF */
		val = REG_RD(bp, NIG_REG_EGRESS_UMP0_IN_EN + port*4);
		/* disable input for TX port IF */
		REG_WR(bp, NIG_REG_EGRESS_UMP0_IN_EN + port*4, 0);

		link_up = (bnx2x_link_test(bp) == 0);
		bnx2x_nic_unload(bp, UNLOAD_NORMAL);
		bnx2x_nic_load(bp, LOAD_DIAG);
		/* wait until link state is restored */
		bnx2x_wait_for_link(bp, link_up);

		if (bnx2x_test_registers(bp) != 0) {
			buf[0] = 1;
			etest->flags |= ETH_TEST_FL_FAILED;
		}
		if (bnx2x_test_memory(bp) != 0) {
			buf[1] = 1;
			etest->flags |= ETH_TEST_FL_FAILED;
		}
		buf[2] = bnx2x_test_loopback(bp, link_up);
		if (buf[2] != 0)
			etest->flags |= ETH_TEST_FL_FAILED;

		bnx2x_nic_unload(bp, UNLOAD_NORMAL);

		/* restore input for TX port IF */
		REG_WR(bp, NIG_REG_EGRESS_UMP0_IN_EN + port*4, val);

		bnx2x_nic_load(bp, LOAD_NORMAL);
		/* wait until link state is restored */
		bnx2x_wait_for_link(bp, link_up);
	}
	if (bnx2x_test_nvram(bp) != 0) {
		buf[3] = 1;
		etest->flags |= ETH_TEST_FL_FAILED;
	}
	if (bnx2x_test_intr(bp) != 0) {
		buf[4] = 1;
		etest->flags |= ETH_TEST_FL_FAILED;
	}
	if (bp->port.pmf)
		if (bnx2x_link_test(bp) != 0) {
			buf[5] = 1;
			etest->flags |= ETH_TEST_FL_FAILED;
		}

#ifdef BNX2X_EXTRA_DEBUG
	bnx2x_panic_dump(bp);
#endif
}

static const struct {
	long offset;
	int size;
	u8 string[ETH_GSTRING_LEN];
} bnx2x_q_stats_arr[BNX2X_NUM_Q_STATS] = {
/* 1 */	{ Q_STATS_OFFSET32(total_bytes_received_hi), 8, "[%d]: rx_bytes" },
	{ Q_STATS_OFFSET32(error_bytes_received_hi),
						8, "[%d]: rx_error_bytes" },
	{ Q_STATS_OFFSET32(total_unicast_packets_received_hi),
						8, "[%d]: rx_ucast_packets" },
	{ Q_STATS_OFFSET32(total_multicast_packets_received_hi),
						8, "[%d]: rx_mcast_packets" },
	{ Q_STATS_OFFSET32(total_broadcast_packets_received_hi),
						8, "[%d]: rx_bcast_packets" },
	{ Q_STATS_OFFSET32(no_buff_discard_hi),	8, "[%d]: rx_discards" },
	{ Q_STATS_OFFSET32(rx_err_discard_pkt),
					 4, "[%d]: rx_phy_ip_err_discards"},
	{ Q_STATS_OFFSET32(rx_skb_alloc_failed),
					 4, "[%d]: rx_skb_alloc_discard" },
	{ Q_STATS_OFFSET32(hw_csum_err), 4, "[%d]: rx_csum_offload_errors" },

/* 10 */{ Q_STATS_OFFSET32(total_bytes_transmitted_hi),	8, "[%d]: tx_bytes" },
	{ Q_STATS_OFFSET32(total_unicast_packets_transmitted_hi),
						8, "[%d]: tx_ucast_packets" },
	{ Q_STATS_OFFSET32(total_multicast_packets_transmitted_hi),
						8, "[%d]: tx_mcast_packets" },
	{ Q_STATS_OFFSET32(total_broadcast_packets_transmitted_hi),
						8, "[%d]: tx_bcast_packets" }
};

static const struct {
	long offset;
	int size;
	u32 flags;
#define STATS_FLAGS_PORT		1
#define STATS_FLAGS_FUNC		2
#define STATS_FLAGS_BOTH		(STATS_FLAGS_FUNC | STATS_FLAGS_PORT)
	u8 string[ETH_GSTRING_LEN];
} bnx2x_stats_arr[BNX2X_NUM_STATS] = {
/* 1 */	{ STATS_OFFSET32(total_bytes_received_hi),
				8, STATS_FLAGS_BOTH, "rx_bytes" },
	{ STATS_OFFSET32(error_bytes_received_hi),
				8, STATS_FLAGS_BOTH, "rx_error_bytes" },
	{ STATS_OFFSET32(total_unicast_packets_received_hi),
				8, STATS_FLAGS_BOTH, "rx_ucast_packets" },
	{ STATS_OFFSET32(total_multicast_packets_received_hi),
				8, STATS_FLAGS_BOTH, "rx_mcast_packets" },
	{ STATS_OFFSET32(total_broadcast_packets_received_hi),
				8, STATS_FLAGS_BOTH, "rx_bcast_packets" },
	{ STATS_OFFSET32(rx_stat_dot3statsfcserrors_hi),
				8, STATS_FLAGS_PORT, "rx_crc_errors" },
	{ STATS_OFFSET32(rx_stat_dot3statsalignmenterrors_hi),
				8, STATS_FLAGS_PORT, "rx_align_errors" },
	{ STATS_OFFSET32(rx_stat_etherstatsundersizepkts_hi),
				8, STATS_FLAGS_PORT, "rx_undersize_packets" },
	{ STATS_OFFSET32(etherstatsoverrsizepkts_hi),
				8, STATS_FLAGS_PORT, "rx_oversize_packets" },
/* 10 */{ STATS_OFFSET32(rx_stat_etherstatsfragments_hi),
				8, STATS_FLAGS_PORT, "rx_fragments" },
	{ STATS_OFFSET32(rx_stat_etherstatsjabbers_hi),
				8, STATS_FLAGS_PORT, "rx_jabbers" },
	{ STATS_OFFSET32(no_buff_discard_hi),
				8, STATS_FLAGS_BOTH, "rx_discards" },
	{ STATS_OFFSET32(mac_filter_discard),
				4, STATS_FLAGS_PORT, "rx_filtered_packets" },
	{ STATS_OFFSET32(xxoverflow_discard),
				4, STATS_FLAGS_PORT, "rx_fw_discards" },
	{ STATS_OFFSET32(brb_drop_hi),
				8, STATS_FLAGS_PORT, "rx_brb_discard" },
	{ STATS_OFFSET32(brb_truncate_hi),
				8, STATS_FLAGS_PORT, "rx_brb_truncate" },
	{ STATS_OFFSET32(pause_frames_received_hi),
				8, STATS_FLAGS_PORT, "rx_pause_frames" },
	{ STATS_OFFSET32(rx_stat_maccontrolframesreceived_hi),
				8, STATS_FLAGS_PORT, "rx_mac_ctrl_frames" },
	{ STATS_OFFSET32(nig_timer_max),
			4, STATS_FLAGS_PORT, "rx_constant_pause_events" },
/* 20 */{ STATS_OFFSET32(rx_err_discard_pkt),
				4, STATS_FLAGS_BOTH, "rx_phy_ip_err_discards"},
	{ STATS_OFFSET32(rx_skb_alloc_failed),
				4, STATS_FLAGS_BOTH, "rx_skb_alloc_discard" },
	{ STATS_OFFSET32(hw_csum_err),
				4, STATS_FLAGS_BOTH, "rx_csum_offload_errors" },

	{ STATS_OFFSET32(total_bytes_transmitted_hi),
				8, STATS_FLAGS_BOTH, "tx_bytes" },
	{ STATS_OFFSET32(tx_stat_ifhcoutbadoctets_hi),
				8, STATS_FLAGS_PORT, "tx_error_bytes" },
	{ STATS_OFFSET32(total_unicast_packets_transmitted_hi),
				8, STATS_FLAGS_BOTH, "tx_ucast_packets" },
	{ STATS_OFFSET32(total_multicast_packets_transmitted_hi),
				8, STATS_FLAGS_BOTH, "tx_mcast_packets" },
	{ STATS_OFFSET32(total_broadcast_packets_transmitted_hi),
				8, STATS_FLAGS_BOTH, "tx_bcast_packets" },
	{ STATS_OFFSET32(tx_stat_dot3statsinternalmactransmiterrors_hi),
				8, STATS_FLAGS_PORT, "tx_mac_errors" },
	{ STATS_OFFSET32(rx_stat_dot3statscarriersenseerrors_hi),
				8, STATS_FLAGS_PORT, "tx_carrier_errors" },
/* 30 */{ STATS_OFFSET32(tx_stat_dot3statssinglecollisionframes_hi),
				8, STATS_FLAGS_PORT, "tx_single_collisions" },
	{ STATS_OFFSET32(tx_stat_dot3statsmultiplecollisionframes_hi),
				8, STATS_FLAGS_PORT, "tx_multi_collisions" },
	{ STATS_OFFSET32(tx_stat_dot3statsdeferredtransmissions_hi),
				8, STATS_FLAGS_PORT, "tx_deferred" },
	{ STATS_OFFSET32(tx_stat_dot3statsexcessivecollisions_hi),
				8, STATS_FLAGS_PORT, "tx_excess_collisions" },
	{ STATS_OFFSET32(tx_stat_dot3statslatecollisions_hi),
				8, STATS_FLAGS_PORT, "tx_late_collisions" },
	{ STATS_OFFSET32(tx_stat_etherstatscollisions_hi),
				8, STATS_FLAGS_PORT, "tx_total_collisions" },
	{ STATS_OFFSET32(tx_stat_etherstatspkts64octets_hi),
				8, STATS_FLAGS_PORT, "tx_64_byte_packets" },
	{ STATS_OFFSET32(tx_stat_etherstatspkts65octetsto127octets_hi),
			8, STATS_FLAGS_PORT, "tx_65_to_127_byte_packets" },
	{ STATS_OFFSET32(tx_stat_etherstatspkts128octetsto255octets_hi),
			8, STATS_FLAGS_PORT, "tx_128_to_255_byte_packets" },
	{ STATS_OFFSET32(tx_stat_etherstatspkts256octetsto511octets_hi),
			8, STATS_FLAGS_PORT, "tx_256_to_511_byte_packets" },
/* 40 */{ STATS_OFFSET32(tx_stat_etherstatspkts512octetsto1023octets_hi),
			8, STATS_FLAGS_PORT, "tx_512_to_1023_byte_packets" },
	{ STATS_OFFSET32(etherstatspkts1024octetsto1522octets_hi),
			8, STATS_FLAGS_PORT, "tx_1024_to_1522_byte_packets" },
	{ STATS_OFFSET32(etherstatspktsover1522octets_hi),
			8, STATS_FLAGS_PORT, "tx_1523_to_9022_byte_packets" },
	{ STATS_OFFSET32(pause_frames_sent_hi),
				8, STATS_FLAGS_PORT, "tx_pause_frames" }
};

#define IS_PORT_STAT(i) \
	((bnx2x_stats_arr[i].flags & STATS_FLAGS_BOTH) == STATS_FLAGS_PORT)
#define IS_FUNC_STAT(i)		(bnx2x_stats_arr[i].flags & STATS_FLAGS_FUNC)
#define IS_E1HMF_MODE_STAT(bp) \
			(IS_E1HMF(bp) && !(bp->msg_enable & BNX2X_MSG_STATS))

static int bnx2x_get_sset_count(struct net_device *dev, int stringset)
{
	struct bnx2x *bp = netdev_priv(dev);
	int i, num_stats;

	switch (stringset) {
	case ETH_SS_STATS:
		if (is_multi(bp)) {
			num_stats = BNX2X_NUM_Q_STATS * bp->num_queues;
			if (!IS_E1HMF_MODE_STAT(bp))
				num_stats += BNX2X_NUM_STATS;
		} else {
			if (IS_E1HMF_MODE_STAT(bp)) {
				num_stats = 0;
				for (i = 0; i < BNX2X_NUM_STATS; i++)
					if (IS_FUNC_STAT(i))
						num_stats++;
			} else
				num_stats = BNX2X_NUM_STATS;
		}
		return num_stats;

	case ETH_SS_TEST:
		return BNX2X_NUM_TESTS;

	default:
		return -EINVAL;
	}
}

static void bnx2x_get_strings(struct net_device *dev, u32 stringset, u8 *buf)
{
	struct bnx2x *bp = netdev_priv(dev);
	int i, j, k;

	switch (stringset) {
	case ETH_SS_STATS:
		if (is_multi(bp)) {
			k = 0;
			for_each_queue(bp, i) {
				for (j = 0; j < BNX2X_NUM_Q_STATS; j++)
					sprintf(buf + (k + j)*ETH_GSTRING_LEN,
						bnx2x_q_stats_arr[j].string, i);
				k += BNX2X_NUM_Q_STATS;
			}
			if (IS_E1HMF_MODE_STAT(bp))
				break;
			for (j = 0; j < BNX2X_NUM_STATS; j++)
				strcpy(buf + (k + j)*ETH_GSTRING_LEN,
				       bnx2x_stats_arr[j].string);
		} else {
			for (i = 0, j = 0; i < BNX2X_NUM_STATS; i++) {
				if (IS_E1HMF_MODE_STAT(bp) && IS_PORT_STAT(i))
					continue;
				strcpy(buf + j*ETH_GSTRING_LEN,
				       bnx2x_stats_arr[i].string);
				j++;
			}
		}
		break;

	case ETH_SS_TEST:
		memcpy(buf, bnx2x_tests_str_arr, sizeof(bnx2x_tests_str_arr));
		break;
	}
}

static void bnx2x_get_ethtool_stats(struct net_device *dev,
				    struct ethtool_stats *stats, u64 *buf)
{
	struct bnx2x *bp = netdev_priv(dev);
	u32 *hw_stats, *offset;
	int i, j, k;

	if (is_multi(bp)) {
		k = 0;
		for_each_queue(bp, i) {
			hw_stats = (u32 *)&bp->fp[i].eth_q_stats;
			for (j = 0; j < BNX2X_NUM_Q_STATS; j++) {
				if (bnx2x_q_stats_arr[j].size == 0) {
					/* skip this counter */
					buf[k + j] = 0;
					continue;
				}
				offset = (hw_stats +
					  bnx2x_q_stats_arr[j].offset);
				if (bnx2x_q_stats_arr[j].size == 4) {
					/* 4-byte counter */
					buf[k + j] = (u64) *offset;
					continue;
				}
				/* 8-byte counter */
				buf[k + j] = HILO_U64(*offset, *(offset + 1));
			}
			k += BNX2X_NUM_Q_STATS;
		}
		if (IS_E1HMF_MODE_STAT(bp))
			return;
		hw_stats = (u32 *)&bp->eth_stats;
		for (j = 0; j < BNX2X_NUM_STATS; j++) {
			if (bnx2x_stats_arr[j].size == 0) {
				/* skip this counter */
				buf[k + j] = 0;
				continue;
			}
			offset = (hw_stats + bnx2x_stats_arr[j].offset);
			if (bnx2x_stats_arr[j].size == 4) {
				/* 4-byte counter */
				buf[k + j] = (u64) *offset;
				continue;
			}
			/* 8-byte counter */
			buf[k + j] = HILO_U64(*offset, *(offset + 1));
		}
	} else {
		hw_stats = (u32 *)&bp->eth_stats;
		for (i = 0, j = 0; i < BNX2X_NUM_STATS; i++) {
			if (IS_E1HMF_MODE_STAT(bp) && IS_PORT_STAT(i))
				continue;
			if (bnx2x_stats_arr[i].size == 0) {
				/* skip this counter */
				buf[j] = 0;
				j++;
				continue;
			}
			offset = (hw_stats + bnx2x_stats_arr[i].offset);
			if (bnx2x_stats_arr[i].size == 4) {
				/* 4-byte counter */
				buf[j] = (u64) *offset;
				j++;
				continue;
			}
			/* 8-byte counter */
			buf[j] = HILO_U64(*offset, *(offset + 1));
			j++;
		}
	}
}

static int bnx2x_phys_id(struct net_device *dev, u32 data)
{
	struct bnx2x *bp = netdev_priv(dev);
	int i;

	if (!netif_running(dev))
		return 0;

	if (!bp->port.pmf)
		return 0;

	if (data == 0)
		data = 2;

	for (i = 0; i < (data * 2); i++) {
		if ((i % 2) == 0)
			bnx2x_set_led(&bp->link_params, LED_MODE_OPER,
				      SPEED_1000);
		else
			bnx2x_set_led(&bp->link_params, LED_MODE_OFF, 0);

		msleep_interruptible(500);
		if (signal_pending(current))
			break;
	}

	if (bp->link_vars.link_up)
		bnx2x_set_led(&bp->link_params, LED_MODE_OPER,
			      bp->link_vars.line_speed);

	return 0;
}

static const struct ethtool_ops bnx2x_ethtool_ops = {
	.get_settings		= bnx2x_get_settings,
	.set_settings		= bnx2x_set_settings,
	.get_drvinfo		= bnx2x_get_drvinfo,
	.get_regs_len		= bnx2x_get_regs_len,
	.get_regs		= bnx2x_get_regs,
	.get_wol		= bnx2x_get_wol,
	.set_wol		= bnx2x_set_wol,
	.get_msglevel		= bnx2x_get_msglevel,
	.set_msglevel		= bnx2x_set_msglevel,
	.nway_reset		= bnx2x_nway_reset,
	.get_link		= bnx2x_get_link,
	.get_eeprom_len		= bnx2x_get_eeprom_len,
	.get_eeprom		= bnx2x_get_eeprom,
	.set_eeprom		= bnx2x_set_eeprom,
	.get_coalesce		= bnx2x_get_coalesce,
	.set_coalesce		= bnx2x_set_coalesce,
	.get_ringparam		= bnx2x_get_ringparam,
	.set_ringparam		= bnx2x_set_ringparam,
	.get_pauseparam		= bnx2x_get_pauseparam,
	.set_pauseparam		= bnx2x_set_pauseparam,
	.get_rx_csum		= bnx2x_get_rx_csum,
	.set_rx_csum		= bnx2x_set_rx_csum,
	.get_tx_csum		= ethtool_op_get_tx_csum,
	.set_tx_csum		= ethtool_op_set_tx_hw_csum,
	.set_flags		= bnx2x_set_flags,
	.get_flags		= ethtool_op_get_flags,
	.get_sg			= ethtool_op_get_sg,
	.set_sg			= ethtool_op_set_sg,
	.get_tso		= ethtool_op_get_tso,
	.set_tso		= bnx2x_set_tso,
	.self_test		= bnx2x_self_test,
	.get_sset_count		= bnx2x_get_sset_count,
	.get_strings		= bnx2x_get_strings,
	.phys_id		= bnx2x_phys_id,
	.get_ethtool_stats	= bnx2x_get_ethtool_stats,
};

void bnx2x_set_ethtool_ops(struct net_device *netdev)
{
	SET_ETHTOOL_OPS(netdev, &bnx2x_ethtool_ops);
}
