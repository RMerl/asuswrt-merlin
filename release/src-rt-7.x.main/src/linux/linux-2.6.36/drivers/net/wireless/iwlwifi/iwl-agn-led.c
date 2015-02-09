/******************************************************************************
 *
 * Copyright(c) 2003 - 2010 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/mac80211.h>
#include <linux/etherdevice.h>
#include <asm/unaligned.h>

#include "iwl-commands.h"
#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-io.h"
#include "iwl-agn-led.h"

/* Send led command */
static int iwl_send_led_cmd(struct iwl_priv *priv, struct iwl_led_cmd *led_cmd)
{
	struct iwl_host_cmd cmd = {
		.id = REPLY_LEDS_CMD,
		.len = sizeof(struct iwl_led_cmd),
		.data = led_cmd,
		.flags = CMD_ASYNC,
		.callback = NULL,
	};
	u32 reg;

	reg = iwl_read32(priv, CSR_LED_REG);
	if (reg != (reg & CSR_LED_BSM_CTRL_MSK))
		iwl_write32(priv, CSR_LED_REG, reg & CSR_LED_BSM_CTRL_MSK);

	return iwl_send_cmd(priv, &cmd);
}

/* Set led register off */
static int iwl_led_on_reg(struct iwl_priv *priv)
{
	IWL_DEBUG_LED(priv, "led on\n");
	iwl_write32(priv, CSR_LED_REG, CSR_LED_REG_TRUN_ON);
	return 0;
}

/* Set led register off */
static int iwl_led_off_reg(struct iwl_priv *priv)
{
	IWL_DEBUG_LED(priv, "LED Reg off\n");
	iwl_write32(priv, CSR_LED_REG, CSR_LED_REG_TRUN_OFF);
	return 0;
}

const struct iwl_led_ops iwlagn_led_ops = {
	.cmd = iwl_send_led_cmd,
	.on = iwl_led_on_reg,
	.off = iwl_led_off_reg,
};
