/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2009 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

/* e1000_82575
 * e1000_82576
 */

#include <linux/types.h>
#include <linux/if_ether.h>

#include "e1000_mac.h"
#include "e1000_82575.h"

static s32  igb_get_invariants_82575(struct e1000_hw *);
static s32  igb_acquire_phy_82575(struct e1000_hw *);
static void igb_release_phy_82575(struct e1000_hw *);
static s32  igb_acquire_nvm_82575(struct e1000_hw *);
static void igb_release_nvm_82575(struct e1000_hw *);
static s32  igb_check_for_link_82575(struct e1000_hw *);
static s32  igb_get_cfg_done_82575(struct e1000_hw *);
static s32  igb_init_hw_82575(struct e1000_hw *);
static s32  igb_phy_hw_reset_sgmii_82575(struct e1000_hw *);
static s32  igb_read_phy_reg_sgmii_82575(struct e1000_hw *, u32, u16 *);
static s32  igb_read_phy_reg_82580(struct e1000_hw *, u32, u16 *);
static s32  igb_write_phy_reg_82580(struct e1000_hw *, u32, u16);
static s32  igb_reset_hw_82575(struct e1000_hw *);
static s32  igb_reset_hw_82580(struct e1000_hw *);
static s32  igb_set_d0_lplu_state_82575(struct e1000_hw *, bool);
static s32  igb_setup_copper_link_82575(struct e1000_hw *);
static s32  igb_setup_serdes_link_82575(struct e1000_hw *);
static s32  igb_write_phy_reg_sgmii_82575(struct e1000_hw *, u32, u16);
static void igb_clear_hw_cntrs_82575(struct e1000_hw *);
static s32  igb_acquire_swfw_sync_82575(struct e1000_hw *, u16);
static s32  igb_get_pcs_speed_and_duplex_82575(struct e1000_hw *, u16 *,
						 u16 *);
static s32  igb_get_phy_id_82575(struct e1000_hw *);
static void igb_release_swfw_sync_82575(struct e1000_hw *, u16);
static bool igb_sgmii_active_82575(struct e1000_hw *);
static s32  igb_reset_init_script_82575(struct e1000_hw *);
static s32  igb_read_mac_addr_82575(struct e1000_hw *);
static s32  igb_set_pcie_completion_timeout(struct e1000_hw *hw);
static s32  igb_reset_mdicnfg_82580(struct e1000_hw *hw);

static const u16 e1000_82580_rxpbs_table[] =
	{ 36, 72, 144, 1, 2, 4, 8, 16,
	  35, 70, 140 };
#define E1000_82580_RXPBS_TABLE_SIZE \
	(sizeof(e1000_82580_rxpbs_table)/sizeof(u16))

/**
 *  igb_sgmii_uses_mdio_82575 - Determine if I2C pins are for external MDIO
 *  @hw: pointer to the HW structure
 *
 *  Called to determine if the I2C pins are being used for I2C or as an
 *  external MDIO interface since the two options are mutually exclusive.
 **/
static bool igb_sgmii_uses_mdio_82575(struct e1000_hw *hw)
{
	u32 reg = 0;
	bool ext_mdio = false;

	switch (hw->mac.type) {
	case e1000_82575:
	case e1000_82576:
		reg = rd32(E1000_MDIC);
		ext_mdio = !!(reg & E1000_MDIC_DEST);
		break;
	case e1000_82580:
	case e1000_i350:
		reg = rd32(E1000_MDICNFG);
		ext_mdio = !!(reg & E1000_MDICNFG_EXT_MDIO);
		break;
	default:
		break;
	}
	return ext_mdio;
}

static s32 igb_get_invariants_82575(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	struct e1000_nvm_info *nvm = &hw->nvm;
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_dev_spec_82575 * dev_spec = &hw->dev_spec._82575;
	u32 eecd;
	s32 ret_val;
	u16 size;
	u32 ctrl_ext = 0;

	switch (hw->device_id) {
	case E1000_DEV_ID_82575EB_COPPER:
	case E1000_DEV_ID_82575EB_FIBER_SERDES:
	case E1000_DEV_ID_82575GB_QUAD_COPPER:
		mac->type = e1000_82575;
		break;
	case E1000_DEV_ID_82576:
	case E1000_DEV_ID_82576_NS:
	case E1000_DEV_ID_82576_NS_SERDES:
	case E1000_DEV_ID_82576_FIBER:
	case E1000_DEV_ID_82576_SERDES:
	case E1000_DEV_ID_82576_QUAD_COPPER:
	case E1000_DEV_ID_82576_QUAD_COPPER_ET2:
	case E1000_DEV_ID_82576_SERDES_QUAD:
		mac->type = e1000_82576;
		break;
	case E1000_DEV_ID_82580_COPPER:
	case E1000_DEV_ID_82580_FIBER:
	case E1000_DEV_ID_82580_SERDES:
	case E1000_DEV_ID_82580_SGMII:
	case E1000_DEV_ID_82580_COPPER_DUAL:
		mac->type = e1000_82580;
		break;
	case E1000_DEV_ID_I350_COPPER:
	case E1000_DEV_ID_I350_FIBER:
	case E1000_DEV_ID_I350_SERDES:
	case E1000_DEV_ID_I350_SGMII:
		mac->type = e1000_i350;
		break;
	default:
		return -E1000_ERR_MAC_INIT;
		break;
	}

	/* Set media type */
	/*
	 * The 82575 uses bits 22:23 for link mode. The mode can be changed
	 * based on the EEPROM. We cannot rely upon device ID. There
	 * is no distinguishable difference between fiber and internal
	 * SerDes mode on the 82575. There can be an external PHY attached
	 * on the SGMII interface. For this, we'll set sgmii_active to true.
	 */
	phy->media_type = e1000_media_type_copper;
	dev_spec->sgmii_active = false;

	ctrl_ext = rd32(E1000_CTRL_EXT);
	switch (ctrl_ext & E1000_CTRL_EXT_LINK_MODE_MASK) {
	case E1000_CTRL_EXT_LINK_MODE_SGMII:
		dev_spec->sgmii_active = true;
		break;
	case E1000_CTRL_EXT_LINK_MODE_1000BASE_KX:
	case E1000_CTRL_EXT_LINK_MODE_PCIE_SERDES:
		hw->phy.media_type = e1000_media_type_internal_serdes;
		break;
	default:
		break;
	}

	/* Set mta register count */
	mac->mta_reg_count = 128;
	/* Set rar entry count */
	mac->rar_entry_count = E1000_RAR_ENTRIES_82575;
	if (mac->type == e1000_82576)
		mac->rar_entry_count = E1000_RAR_ENTRIES_82576;
	if (mac->type == e1000_82580)
		mac->rar_entry_count = E1000_RAR_ENTRIES_82580;
	if (mac->type == e1000_i350)
		mac->rar_entry_count = E1000_RAR_ENTRIES_I350;
	/* reset */
	if (mac->type >= e1000_82580)
		mac->ops.reset_hw = igb_reset_hw_82580;
	else
		mac->ops.reset_hw = igb_reset_hw_82575;
	/* Set if part includes ASF firmware */
	mac->asf_firmware_present = true;
	/* Set if manageability features are enabled. */
	mac->arc_subsystem_valid =
		(rd32(E1000_FWSM) & E1000_FWSM_MODE_MASK)
			? true : false;

	/* physical interface link setup */
	mac->ops.setup_physical_interface =
		(hw->phy.media_type == e1000_media_type_copper)
			? igb_setup_copper_link_82575
			: igb_setup_serdes_link_82575;

	/* NVM initialization */
	eecd = rd32(E1000_EECD);

	nvm->opcode_bits        = 8;
	nvm->delay_usec         = 1;
	switch (nvm->override) {
	case e1000_nvm_override_spi_large:
		nvm->page_size    = 32;
		nvm->address_bits = 16;
		break;
	case e1000_nvm_override_spi_small:
		nvm->page_size    = 8;
		nvm->address_bits = 8;
		break;
	default:
		nvm->page_size    = eecd & E1000_EECD_ADDR_BITS ? 32 : 8;
		nvm->address_bits = eecd & E1000_EECD_ADDR_BITS ? 16 : 8;
		break;
	}

	nvm->type = e1000_nvm_eeprom_spi;

	size = (u16)((eecd & E1000_EECD_SIZE_EX_MASK) >>
		     E1000_EECD_SIZE_EX_SHIFT);

	/*
	 * Added to a constant, "size" becomes the left-shift value
	 * for setting word_size.
	 */
	size += NVM_WORD_SIZE_BASE_SHIFT;

	/* EEPROM access above 16k is unsupported */
	if (size > 14)
		size = 14;
	nvm->word_size = 1 << size;

	/* if 82576 then initialize mailbox parameters */
	if (mac->type == e1000_82576)
		igb_init_mbx_params_pf(hw);

	/* setup PHY parameters */
	if (phy->media_type != e1000_media_type_copper) {
		phy->type = e1000_phy_none;
		return 0;
	}

	phy->autoneg_mask        = AUTONEG_ADVERTISE_SPEED_DEFAULT;
	phy->reset_delay_us      = 100;

	ctrl_ext = rd32(E1000_CTRL_EXT);

	/* PHY function pointers */
	if (igb_sgmii_active_82575(hw)) {
		phy->ops.reset      = igb_phy_hw_reset_sgmii_82575;
		ctrl_ext |= E1000_CTRL_I2C_ENA;
	} else {
		phy->ops.reset      = igb_phy_hw_reset;
		ctrl_ext &= ~E1000_CTRL_I2C_ENA;
	}

	wr32(E1000_CTRL_EXT, ctrl_ext);
	igb_reset_mdicnfg_82580(hw);

	if (igb_sgmii_active_82575(hw) && !igb_sgmii_uses_mdio_82575(hw)) {
		phy->ops.read_reg   = igb_read_phy_reg_sgmii_82575;
		phy->ops.write_reg  = igb_write_phy_reg_sgmii_82575;
	} else if (hw->mac.type >= e1000_82580) {
		phy->ops.read_reg   = igb_read_phy_reg_82580;
		phy->ops.write_reg  = igb_write_phy_reg_82580;
	} else {
		phy->ops.read_reg   = igb_read_phy_reg_igp;
		phy->ops.write_reg  = igb_write_phy_reg_igp;
	}

	/* set lan id */
	hw->bus.func = (rd32(E1000_STATUS) & E1000_STATUS_FUNC_MASK) >>
	               E1000_STATUS_FUNC_SHIFT;

	/* Set phy->phy_addr and phy->id. */
	ret_val = igb_get_phy_id_82575(hw);
	if (ret_val)
		return ret_val;

	/* Verify phy id and set remaining function pointers */
	switch (phy->id) {
	case M88E1111_I_PHY_ID:
		phy->type                   = e1000_phy_m88;
		phy->ops.get_phy_info       = igb_get_phy_info_m88;
		phy->ops.get_cable_length   = igb_get_cable_length_m88;
		phy->ops.force_speed_duplex = igb_phy_force_speed_duplex_m88;
		break;
	case IGP03E1000_E_PHY_ID:
		phy->type                   = e1000_phy_igp_3;
		phy->ops.get_phy_info       = igb_get_phy_info_igp;
		phy->ops.get_cable_length   = igb_get_cable_length_igp_2;
		phy->ops.force_speed_duplex = igb_phy_force_speed_duplex_igp;
		phy->ops.set_d0_lplu_state  = igb_set_d0_lplu_state_82575;
		phy->ops.set_d3_lplu_state  = igb_set_d3_lplu_state;
		break;
	case I82580_I_PHY_ID:
	case I350_I_PHY_ID:
		phy->type                   = e1000_phy_82580;
		phy->ops.force_speed_duplex = igb_phy_force_speed_duplex_82580;
		phy->ops.get_cable_length   = igb_get_cable_length_82580;
		phy->ops.get_phy_info       = igb_get_phy_info_82580;
		break;
	default:
		return -E1000_ERR_PHY;
	}

	return 0;
}

/**
 *  igb_acquire_phy_82575 - Acquire rights to access PHY
 *  @hw: pointer to the HW structure
 *
 *  Acquire access rights to the correct PHY.  This is a
 *  function pointer entry point called by the api module.
 **/
static s32 igb_acquire_phy_82575(struct e1000_hw *hw)
{
	u16 mask = E1000_SWFW_PHY0_SM;

	if (hw->bus.func == E1000_FUNC_1)
		mask = E1000_SWFW_PHY1_SM;
	else if (hw->bus.func == E1000_FUNC_2)
		mask = E1000_SWFW_PHY2_SM;
	else if (hw->bus.func == E1000_FUNC_3)
		mask = E1000_SWFW_PHY3_SM;

	return igb_acquire_swfw_sync_82575(hw, mask);
}

/**
 *  igb_release_phy_82575 - Release rights to access PHY
 *  @hw: pointer to the HW structure
 *
 *  A wrapper to release access rights to the correct PHY.  This is a
 *  function pointer entry point called by the api module.
 **/
static void igb_release_phy_82575(struct e1000_hw *hw)
{
	u16 mask = E1000_SWFW_PHY0_SM;

	if (hw->bus.func == E1000_FUNC_1)
		mask = E1000_SWFW_PHY1_SM;
	else if (hw->bus.func == E1000_FUNC_2)
		mask = E1000_SWFW_PHY2_SM;
	else if (hw->bus.func == E1000_FUNC_3)
		mask = E1000_SWFW_PHY3_SM;

	igb_release_swfw_sync_82575(hw, mask);
}

/**
 *  igb_read_phy_reg_sgmii_82575 - Read PHY register using sgmii
 *  @hw: pointer to the HW structure
 *  @offset: register offset to be read
 *  @data: pointer to the read data
 *
 *  Reads the PHY register at offset using the serial gigabit media independent
 *  interface and stores the retrieved information in data.
 **/
static s32 igb_read_phy_reg_sgmii_82575(struct e1000_hw *hw, u32 offset,
					  u16 *data)
{
	s32 ret_val = -E1000_ERR_PARAM;

	if (offset > E1000_MAX_SGMII_PHY_REG_ADDR) {
		hw_dbg("PHY Address %u is out of range\n", offset);
		goto out;
	}

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	ret_val = igb_read_phy_reg_i2c(hw, offset, data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

/**
 *  igb_write_phy_reg_sgmii_82575 - Write PHY register using sgmii
 *  @hw: pointer to the HW structure
 *  @offset: register offset to write to
 *  @data: data to write at register offset
 *
 *  Writes the data to PHY register at the offset using the serial gigabit
 *  media independent interface.
 **/
static s32 igb_write_phy_reg_sgmii_82575(struct e1000_hw *hw, u32 offset,
					   u16 data)
{
	s32 ret_val = -E1000_ERR_PARAM;


	if (offset > E1000_MAX_SGMII_PHY_REG_ADDR) {
		hw_dbg("PHY Address %d is out of range\n", offset);
		goto out;
	}

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	ret_val = igb_write_phy_reg_i2c(hw, offset, data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

/**
 *  igb_get_phy_id_82575 - Retrieve PHY addr and id
 *  @hw: pointer to the HW structure
 *
 *  Retrieves the PHY address and ID for both PHY's which do and do not use
 *  sgmi interface.
 **/
static s32 igb_get_phy_id_82575(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32  ret_val = 0;
	u16 phy_id;
	u32 ctrl_ext;
	u32 mdic;

	/*
	 * For SGMII PHYs, we try the list of possible addresses until
	 * we find one that works.  For non-SGMII PHYs
	 * (e.g. integrated copper PHYs), an address of 1 should
	 * work.  The result of this function should mean phy->phy_addr
	 * and phy->id are set correctly.
	 */
	if (!(igb_sgmii_active_82575(hw))) {
		phy->addr = 1;
		ret_val = igb_get_phy_id(hw);
		goto out;
	}

	if (igb_sgmii_uses_mdio_82575(hw)) {
		switch (hw->mac.type) {
		case e1000_82575:
		case e1000_82576:
			mdic = rd32(E1000_MDIC);
			mdic &= E1000_MDIC_PHY_MASK;
			phy->addr = mdic >> E1000_MDIC_PHY_SHIFT;
			break;
		case e1000_82580:
		case e1000_i350:
			mdic = rd32(E1000_MDICNFG);
			mdic &= E1000_MDICNFG_PHY_MASK;
			phy->addr = mdic >> E1000_MDICNFG_PHY_SHIFT;
			break;
		default:
			ret_val = -E1000_ERR_PHY;
			goto out;
			break;
		}
		ret_val = igb_get_phy_id(hw);
		goto out;
	}

	/* Power on sgmii phy if it is disabled */
	ctrl_ext = rd32(E1000_CTRL_EXT);
	wr32(E1000_CTRL_EXT, ctrl_ext & ~E1000_CTRL_EXT_SDP3_DATA);
	wrfl();
	msleep(300);

	/*
	 * The address field in the I2CCMD register is 3 bits and 0 is invalid.
	 * Therefore, we need to test 1-7
	 */
	for (phy->addr = 1; phy->addr < 8; phy->addr++) {
		ret_val = igb_read_phy_reg_sgmii_82575(hw, PHY_ID1, &phy_id);
		if (ret_val == 0) {
			hw_dbg("Vendor ID 0x%08X read at address %u\n",
			       phy_id, phy->addr);
			/*
			 * At the time of this writing, The M88 part is
			 * the only supported SGMII PHY product.
			 */
			if (phy_id == M88_VENDOR)
				break;
		} else {
			hw_dbg("PHY address %u was unreadable\n", phy->addr);
		}
	}

	/* A valid PHY type couldn't be found. */
	if (phy->addr == 8) {
		phy->addr = 0;
		ret_val = -E1000_ERR_PHY;
		goto out;
	} else {
		ret_val = igb_get_phy_id(hw);
	}

	/* restore previous sfp cage power state */
	wr32(E1000_CTRL_EXT, ctrl_ext);

out:
	return ret_val;
}

/**
 *  igb_phy_hw_reset_sgmii_82575 - Performs a PHY reset
 *  @hw: pointer to the HW structure
 *
 *  Resets the PHY using the serial gigabit media independent interface.
 **/
static s32 igb_phy_hw_reset_sgmii_82575(struct e1000_hw *hw)
{
	s32 ret_val;

	/*
	 * This isn't a true "hard" reset, but is the only reset
	 * available to us at this time.
	 */

	hw_dbg("Soft resetting SGMII attached PHY...\n");

	/*
	 * SFP documentation requires the following to configure the SPF module
	 * to work on SGMII.  No further documentation is given.
	 */
	ret_val = hw->phy.ops.write_reg(hw, 0x1B, 0x8084);
	if (ret_val)
		goto out;

	ret_val = igb_phy_sw_reset(hw);

out:
	return ret_val;
}

/**
 *  igb_set_d0_lplu_state_82575 - Set Low Power Linkup D0 state
 *  @hw: pointer to the HW structure
 *  @active: true to enable LPLU, false to disable
 *
 *  Sets the LPLU D0 state according to the active flag.  When
 *  activating LPLU this function also disables smart speed
 *  and vice versa.  LPLU will not be activated unless the
 *  device autonegotiation advertisement meets standards of
 *  either 10 or 10/100 or 10/100/1000 at all duplexes.
 *  This is a function pointer entry point only called by
 *  PHY setup routines.
 **/
static s32 igb_set_d0_lplu_state_82575(struct e1000_hw *hw, bool active)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = phy->ops.read_reg(hw, IGP02E1000_PHY_POWER_MGMT, &data);
	if (ret_val)
		goto out;

	if (active) {
		data |= IGP02E1000_PM_D0_LPLU;
		ret_val = phy->ops.write_reg(hw, IGP02E1000_PHY_POWER_MGMT,
						 data);
		if (ret_val)
			goto out;

		/* When LPLU is enabled, we should disable SmartSpeed */
		ret_val = phy->ops.read_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
						&data);
		data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val = phy->ops.write_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
						 data);
		if (ret_val)
			goto out;
	} else {
		data &= ~IGP02E1000_PM_D0_LPLU;
		ret_val = phy->ops.write_reg(hw, IGP02E1000_PHY_POWER_MGMT,
						 data);
		/*
		 * LPLU and SmartSpeed are mutually exclusive.  LPLU is used
		 * during Dx states where the power conservation is most
		 * important.  During driver activity we should enable
		 * SmartSpeed, so performance is maintained.
		 */
		if (phy->smart_speed == e1000_smart_speed_on) {
			ret_val = phy->ops.read_reg(hw,
					IGP01E1000_PHY_PORT_CONFIG, &data);
			if (ret_val)
				goto out;

			data |= IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
					IGP01E1000_PHY_PORT_CONFIG, data);
			if (ret_val)
				goto out;
		} else if (phy->smart_speed == e1000_smart_speed_off) {
			ret_val = phy->ops.read_reg(hw,
					IGP01E1000_PHY_PORT_CONFIG, &data);
			if (ret_val)
				goto out;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
					IGP01E1000_PHY_PORT_CONFIG, data);
			if (ret_val)
				goto out;
		}
	}

out:
	return ret_val;
}

/**
 *  igb_acquire_nvm_82575 - Request for access to EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Acquire the necessary semaphores for exclusive access to the EEPROM.
 *  Set the EEPROM access request bit and wait for EEPROM access grant bit.
 *  Return successful if access grant bit set, else clear the request for
 *  EEPROM access and return -E1000_ERR_NVM (-1).
 **/
static s32 igb_acquire_nvm_82575(struct e1000_hw *hw)
{
	s32 ret_val;

	ret_val = igb_acquire_swfw_sync_82575(hw, E1000_SWFW_EEP_SM);
	if (ret_val)
		goto out;

	ret_val = igb_acquire_nvm(hw);

	if (ret_val)
		igb_release_swfw_sync_82575(hw, E1000_SWFW_EEP_SM);

out:
	return ret_val;
}

/**
 *  igb_release_nvm_82575 - Release exclusive access to EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Stop any current commands to the EEPROM and clear the EEPROM request bit,
 *  then release the semaphores acquired.
 **/
static void igb_release_nvm_82575(struct e1000_hw *hw)
{
	igb_release_nvm(hw);
	igb_release_swfw_sync_82575(hw, E1000_SWFW_EEP_SM);
}

/**
 *  igb_acquire_swfw_sync_82575 - Acquire SW/FW semaphore
 *  @hw: pointer to the HW structure
 *  @mask: specifies which semaphore to acquire
 *
 *  Acquire the SW/FW semaphore to access the PHY or NVM.  The mask
 *  will also specify which port we're acquiring the lock for.
 **/
static s32 igb_acquire_swfw_sync_82575(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;
	u32 swmask = mask;
	u32 fwmask = mask << 16;
	s32 ret_val = 0;
	s32 i = 0, timeout = 200;

	while (i < timeout) {
		if (igb_get_hw_semaphore(hw)) {
			ret_val = -E1000_ERR_SWFW_SYNC;
			goto out;
		}

		swfw_sync = rd32(E1000_SW_FW_SYNC);
		if (!(swfw_sync & (fwmask | swmask)))
			break;

		/*
		 * Firmware currently using resource (fwmask)
		 * or other software thread using resource (swmask)
		 */
		igb_put_hw_semaphore(hw);
		mdelay(5);
		i++;
	}

	if (i == timeout) {
		hw_dbg("Driver can't access resource, SW_FW_SYNC timeout.\n");
		ret_val = -E1000_ERR_SWFW_SYNC;
		goto out;
	}

	swfw_sync |= swmask;
	wr32(E1000_SW_FW_SYNC, swfw_sync);

	igb_put_hw_semaphore(hw);

out:
	return ret_val;
}

/**
 *  igb_release_swfw_sync_82575 - Release SW/FW semaphore
 *  @hw: pointer to the HW structure
 *  @mask: specifies which semaphore to acquire
 *
 *  Release the SW/FW semaphore used to access the PHY or NVM.  The mask
 *  will also specify which port we're releasing the lock for.
 **/
static void igb_release_swfw_sync_82575(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;

	while (igb_get_hw_semaphore(hw) != 0);
	/* Empty */

	swfw_sync = rd32(E1000_SW_FW_SYNC);
	swfw_sync &= ~mask;
	wr32(E1000_SW_FW_SYNC, swfw_sync);

	igb_put_hw_semaphore(hw);
}

/**
 *  igb_get_cfg_done_82575 - Read config done bit
 *  @hw: pointer to the HW structure
 *
 *  Read the management control register for the config done bit for
 *  completion status.  NOTE: silicon which is EEPROM-less will fail trying
 *  to read the config done bit, so an error is *ONLY* logged and returns
 *  0.  If we were to return with error, EEPROM-less silicon
 *  would not be able to be reset or change link.
 **/
static s32 igb_get_cfg_done_82575(struct e1000_hw *hw)
{
	s32 timeout = PHY_CFG_TIMEOUT;
	s32 ret_val = 0;
	u32 mask = E1000_NVM_CFG_DONE_PORT_0;

	if (hw->bus.func == 1)
		mask = E1000_NVM_CFG_DONE_PORT_1;
	else if (hw->bus.func == E1000_FUNC_2)
		mask = E1000_NVM_CFG_DONE_PORT_2;
	else if (hw->bus.func == E1000_FUNC_3)
		mask = E1000_NVM_CFG_DONE_PORT_3;

	while (timeout) {
		if (rd32(E1000_EEMNGCTL) & mask)
			break;
		msleep(1);
		timeout--;
	}
	if (!timeout)
		hw_dbg("MNG configuration cycle has not completed.\n");

	/* If EEPROM is not marked present, init the PHY manually */
	if (((rd32(E1000_EECD) & E1000_EECD_PRES) == 0) &&
	    (hw->phy.type == e1000_phy_igp_3))
		igb_phy_init_script_igp3(hw);

	return ret_val;
}

/**
 *  igb_check_for_link_82575 - Check for link
 *  @hw: pointer to the HW structure
 *
 *  If sgmii is enabled, then use the pcs register to determine link, otherwise
 *  use the generic interface for determining link.
 **/
static s32 igb_check_for_link_82575(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 speed, duplex;

	if (hw->phy.media_type != e1000_media_type_copper) {
		ret_val = igb_get_pcs_speed_and_duplex_82575(hw, &speed,
		                                             &duplex);
		/*
		 * Use this flag to determine if link needs to be checked or
		 * not.  If  we have link clear the flag so that we do not
		 * continue to check for link.
		 */
		hw->mac.get_link_status = !hw->mac.serdes_has_link;
	} else {
		ret_val = igb_check_for_copper_link(hw);
	}

	return ret_val;
}

/**
 *  igb_power_up_serdes_link_82575 - Power up the serdes link after shutdown
 *  @hw: pointer to the HW structure
 **/
void igb_power_up_serdes_link_82575(struct e1000_hw *hw)
{
	u32 reg;


	if ((hw->phy.media_type != e1000_media_type_internal_serdes) &&
	    !igb_sgmii_active_82575(hw))
		return;

	/* Enable PCS to turn on link */
	reg = rd32(E1000_PCS_CFG0);
	reg |= E1000_PCS_CFG_PCS_EN;
	wr32(E1000_PCS_CFG0, reg);

	/* Power up the laser */
	reg = rd32(E1000_CTRL_EXT);
	reg &= ~E1000_CTRL_EXT_SDP3_DATA;
	wr32(E1000_CTRL_EXT, reg);

	/* flush the write to verify completion */
	wrfl();
	msleep(1);
}

/**
 *  igb_get_pcs_speed_and_duplex_82575 - Retrieve current speed/duplex
 *  @hw: pointer to the HW structure
 *  @speed: stores the current speed
 *  @duplex: stores the current duplex
 *
 *  Using the physical coding sub-layer (PCS), retrieve the current speed and
 *  duplex, then store the values in the pointers provided.
 **/
static s32 igb_get_pcs_speed_and_duplex_82575(struct e1000_hw *hw, u16 *speed,
						u16 *duplex)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 pcs;

	/* Set up defaults for the return values of this function */
	mac->serdes_has_link = false;
	*speed = 0;
	*duplex = 0;

	/*
	 * Read the PCS Status register for link state. For non-copper mode,
	 * the status register is not accurate. The PCS status register is
	 * used instead.
	 */
	pcs = rd32(E1000_PCS_LSTAT);

	/*
	 * The link up bit determines when link is up on autoneg. The sync ok
	 * gets set once both sides sync up and agree upon link. Stable link
	 * can be determined by checking for both link up and link sync ok
	 */
	if ((pcs & E1000_PCS_LSTS_LINK_OK) && (pcs & E1000_PCS_LSTS_SYNK_OK)) {
		mac->serdes_has_link = true;

		/* Detect and store PCS speed */
		if (pcs & E1000_PCS_LSTS_SPEED_1000) {
			*speed = SPEED_1000;
		} else if (pcs & E1000_PCS_LSTS_SPEED_100) {
			*speed = SPEED_100;
		} else {
			*speed = SPEED_10;
		}

		/* Detect and store PCS duplex */
		if (pcs & E1000_PCS_LSTS_DUPLEX_FULL) {
			*duplex = FULL_DUPLEX;
		} else {
			*duplex = HALF_DUPLEX;
		}
	}

	return 0;
}

/**
 *  igb_shutdown_serdes_link_82575 - Remove link during power down
 *  @hw: pointer to the HW structure
 *
 *  In the case of fiber serdes, shut down optics and PCS on driver unload
 *  when management pass thru is not enabled.
 **/
void igb_shutdown_serdes_link_82575(struct e1000_hw *hw)
{
	u32 reg;

	if (hw->phy.media_type != e1000_media_type_internal_serdes &&
	    igb_sgmii_active_82575(hw))
		return;

	if (!igb_enable_mng_pass_thru(hw)) {
		/* Disable PCS to turn off link */
		reg = rd32(E1000_PCS_CFG0);
		reg &= ~E1000_PCS_CFG_PCS_EN;
		wr32(E1000_PCS_CFG0, reg);

		/* shutdown the laser */
		reg = rd32(E1000_CTRL_EXT);
		reg |= E1000_CTRL_EXT_SDP3_DATA;
		wr32(E1000_CTRL_EXT, reg);

		/* flush the write to verify completion */
		wrfl();
		msleep(1);
	}
}

/**
 *  igb_reset_hw_82575 - Reset hardware
 *  @hw: pointer to the HW structure
 *
 *  This resets the hardware into a known state.  This is a
 *  function pointer entry point called by the api module.
 **/
static s32 igb_reset_hw_82575(struct e1000_hw *hw)
{
	u32 ctrl, icr;
	s32 ret_val;

	/*
	 * Prevent the PCI-E bus from sticking if there is no TLP connection
	 * on the last TLP read/write transaction when MAC is reset.
	 */
	ret_val = igb_disable_pcie_master(hw);
	if (ret_val)
		hw_dbg("PCI-E Master disable polling has failed.\n");

	/* set the completion timeout for interface */
	ret_val = igb_set_pcie_completion_timeout(hw);
	if (ret_val) {
		hw_dbg("PCI-E Set completion timeout has failed.\n");
	}

	hw_dbg("Masking off all interrupts\n");
	wr32(E1000_IMC, 0xffffffff);

	wr32(E1000_RCTL, 0);
	wr32(E1000_TCTL, E1000_TCTL_PSP);
	wrfl();

	msleep(10);

	ctrl = rd32(E1000_CTRL);

	hw_dbg("Issuing a global reset to MAC\n");
	wr32(E1000_CTRL, ctrl | E1000_CTRL_RST);

	ret_val = igb_get_auto_rd_done(hw);
	if (ret_val) {
		/*
		 * When auto config read does not complete, do not
		 * return with an error. This can happen in situations
		 * where there is no eeprom and prevents getting link.
		 */
		hw_dbg("Auto Read Done did not complete\n");
	}

	/* If EEPROM is not present, run manual init scripts */
	if ((rd32(E1000_EECD) & E1000_EECD_PRES) == 0)
		igb_reset_init_script_82575(hw);

	/* Clear any pending interrupt events. */
	wr32(E1000_IMC, 0xffffffff);
	icr = rd32(E1000_ICR);

	/* Install any alternate MAC address into RAR0 */
	ret_val = igb_check_alt_mac_addr(hw);

	return ret_val;
}

/**
 *  igb_init_hw_82575 - Initialize hardware
 *  @hw: pointer to the HW structure
 *
 *  This inits the hardware readying it for operation.
 **/
static s32 igb_init_hw_82575(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	u16 i, rar_count = mac->rar_entry_count;

	/* Initialize identification LED */
	ret_val = igb_id_led_init(hw);
	if (ret_val) {
		hw_dbg("Error initializing identification LED\n");
		/* This is not fatal and we should not stop init due to this */
	}

	/* Disabling VLAN filtering */
	hw_dbg("Initializing the IEEE VLAN\n");
	igb_clear_vfta(hw);

	/* Setup the receive address */
	igb_init_rx_addrs(hw, rar_count);

	/* Zero out the Multicast HASH table */
	hw_dbg("Zeroing the MTA\n");
	for (i = 0; i < mac->mta_reg_count; i++)
		array_wr32(E1000_MTA, i, 0);

	/* Zero out the Unicast HASH table */
	hw_dbg("Zeroing the UTA\n");
	for (i = 0; i < mac->uta_reg_count; i++)
		array_wr32(E1000_UTA, i, 0);

	/* Setup link and flow control */
	ret_val = igb_setup_link(hw);

	/*
	 * Clear all of the statistics registers (clear on read).  It is
	 * important that we do this after we have tried to establish link
	 * because the symbol error count will increment wildly if there
	 * is no link.
	 */
	igb_clear_hw_cntrs_82575(hw);

	return ret_val;
}

/**
 *  igb_setup_copper_link_82575 - Configure copper link settings
 *  @hw: pointer to the HW structure
 *
 *  Configures the link for auto-neg or forced speed and duplex.  Then we check
 *  for link, once link is established calls to configure collision distance
 *  and flow control are called.
 **/
static s32 igb_setup_copper_link_82575(struct e1000_hw *hw)
{
	u32 ctrl;
	s32  ret_val;

	ctrl = rd32(E1000_CTRL);
	ctrl |= E1000_CTRL_SLU;
	ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	wr32(E1000_CTRL, ctrl);

	ret_val = igb_setup_serdes_link_82575(hw);
	if (ret_val)
		goto out;

	if (igb_sgmii_active_82575(hw) && !hw->phy.reset_disable) {
		/* allow time for SFP cage time to power up phy */
		msleep(300);

		ret_val = hw->phy.ops.reset(hw);
		if (ret_val) {
			hw_dbg("Error resetting the PHY.\n");
			goto out;
		}
	}
	switch (hw->phy.type) {
	case e1000_phy_m88:
		ret_val = igb_copper_link_setup_m88(hw);
		break;
	case e1000_phy_igp_3:
		ret_val = igb_copper_link_setup_igp(hw);
		break;
	case e1000_phy_82580:
		ret_val = igb_copper_link_setup_82580(hw);
		break;
	default:
		ret_val = -E1000_ERR_PHY;
		break;
	}

	if (ret_val)
		goto out;

	ret_val = igb_setup_copper_link(hw);
out:
	return ret_val;
}

/**
 *  igb_setup_serdes_link_82575 - Setup link for serdes
 *  @hw: pointer to the HW structure
 *
 *  Configure the physical coding sub-layer (PCS) link.  The PCS link is
 *  used on copper connections where the serialized gigabit media independent
 *  interface (sgmii), or serdes fiber is being used.  Configures the link
 *  for auto-negotiation or forces speed/duplex.
 **/
static s32 igb_setup_serdes_link_82575(struct e1000_hw *hw)
{
	u32 ctrl_ext, ctrl_reg, reg;
	bool pcs_autoneg;

	if ((hw->phy.media_type != e1000_media_type_internal_serdes) &&
	    !igb_sgmii_active_82575(hw))
		return 0;

	/*
	 * On the 82575, SerDes loopback mode persists until it is
	 * explicitly turned off or a power cycle is performed.  A read to
	 * the register does not indicate its status.  Therefore, we ensure
	 * loopback mode is disabled during initialization.
	 */
	wr32(E1000_SCTL, E1000_SCTL_DISABLE_SERDES_LOOPBACK);

	/* power on the sfp cage if present */
	ctrl_ext = rd32(E1000_CTRL_EXT);
	ctrl_ext &= ~E1000_CTRL_EXT_SDP3_DATA;
	wr32(E1000_CTRL_EXT, ctrl_ext);

	ctrl_reg = rd32(E1000_CTRL);
	ctrl_reg |= E1000_CTRL_SLU;

	if (hw->mac.type == e1000_82575 || hw->mac.type == e1000_82576) {
		/* set both sw defined pins */
		ctrl_reg |= E1000_CTRL_SWDPIN0 | E1000_CTRL_SWDPIN1;

		/* Set switch control to serdes energy detect */
		reg = rd32(E1000_CONNSW);
		reg |= E1000_CONNSW_ENRGSRC;
		wr32(E1000_CONNSW, reg);
	}

	reg = rd32(E1000_PCS_LCTL);

	/* default pcs_autoneg to the same setting as mac autoneg */
	pcs_autoneg = hw->mac.autoneg;

	switch (ctrl_ext & E1000_CTRL_EXT_LINK_MODE_MASK) {
	case E1000_CTRL_EXT_LINK_MODE_SGMII:
		/* sgmii mode lets the phy handle forcing speed/duplex */
		pcs_autoneg = true;
		/* autoneg time out should be disabled for SGMII mode */
		reg &= ~(E1000_PCS_LCTL_AN_TIMEOUT);
		break;
	case E1000_CTRL_EXT_LINK_MODE_1000BASE_KX:
		/* disable PCS autoneg and support parallel detect only */
		pcs_autoneg = false;
	default:
		/*
		 * non-SGMII modes only supports a speed of 1000/Full for the
		 * link so it is best to just force the MAC and let the pcs
		 * link either autoneg or be forced to 1000/Full
		 */
		ctrl_reg |= E1000_CTRL_SPD_1000 | E1000_CTRL_FRCSPD |
		            E1000_CTRL_FD | E1000_CTRL_FRCDPX;

		/* set speed of 1000/Full if speed/duplex is forced */
		reg |= E1000_PCS_LCTL_FSV_1000 | E1000_PCS_LCTL_FDV_FULL;
		break;
	}

	wr32(E1000_CTRL, ctrl_reg);

	/*
	 * New SerDes mode allows for forcing speed or autonegotiating speed
	 * at 1gb. Autoneg should be default set by most drivers. This is the
	 * mode that will be compatible with older link partners and switches.
	 * However, both are supported by the hardware and some drivers/tools.
	 */
	reg &= ~(E1000_PCS_LCTL_AN_ENABLE | E1000_PCS_LCTL_FLV_LINK_UP |
		E1000_PCS_LCTL_FSD | E1000_PCS_LCTL_FORCE_LINK);

	/*
	 * We force flow control to prevent the CTRL register values from being
	 * overwritten by the autonegotiated flow control values
	 */
	reg |= E1000_PCS_LCTL_FORCE_FCTRL;

	if (pcs_autoneg) {
		/* Set PCS register for autoneg */
		reg |= E1000_PCS_LCTL_AN_ENABLE | /* Enable Autoneg */
		       E1000_PCS_LCTL_AN_RESTART; /* Restart autoneg */
		hw_dbg("Configuring Autoneg:PCS_LCTL=0x%08X\n", reg);
	} else {
		/* Set PCS register for forced link */
		reg |= E1000_PCS_LCTL_FSD;        /* Force Speed */

		hw_dbg("Configuring Forced Link:PCS_LCTL=0x%08X\n", reg);
	}

	wr32(E1000_PCS_LCTL, reg);

	if (!igb_sgmii_active_82575(hw))
		igb_force_mac_fc(hw);

	return 0;
}

/**
 *  igb_sgmii_active_82575 - Return sgmii state
 *  @hw: pointer to the HW structure
 *
 *  82575 silicon has a serialized gigabit media independent interface (sgmii)
 *  which can be enabled for use in the embedded applications.  Simply
 *  return the current state of the sgmii interface.
 **/
static bool igb_sgmii_active_82575(struct e1000_hw *hw)
{
	struct e1000_dev_spec_82575 *dev_spec = &hw->dev_spec._82575;
	return dev_spec->sgmii_active;
}

/**
 *  igb_reset_init_script_82575 - Inits HW defaults after reset
 *  @hw: pointer to the HW structure
 *
 *  Inits recommended HW defaults after a reset when there is no EEPROM
 *  detected. This is only for the 82575.
 **/
static s32 igb_reset_init_script_82575(struct e1000_hw *hw)
{
	if (hw->mac.type == e1000_82575) {
		hw_dbg("Running reset init script for 82575\n");
		/* SerDes configuration via SERDESCTRL */
		igb_write_8bit_ctrl_reg(hw, E1000_SCTL, 0x00, 0x0C);
		igb_write_8bit_ctrl_reg(hw, E1000_SCTL, 0x01, 0x78);
		igb_write_8bit_ctrl_reg(hw, E1000_SCTL, 0x1B, 0x23);
		igb_write_8bit_ctrl_reg(hw, E1000_SCTL, 0x23, 0x15);

		/* CCM configuration via CCMCTL register */
		igb_write_8bit_ctrl_reg(hw, E1000_CCMCTL, 0x14, 0x00);
		igb_write_8bit_ctrl_reg(hw, E1000_CCMCTL, 0x10, 0x00);

		/* PCIe lanes configuration */
		igb_write_8bit_ctrl_reg(hw, E1000_GIOCTL, 0x00, 0xEC);
		igb_write_8bit_ctrl_reg(hw, E1000_GIOCTL, 0x61, 0xDF);
		igb_write_8bit_ctrl_reg(hw, E1000_GIOCTL, 0x34, 0x05);
		igb_write_8bit_ctrl_reg(hw, E1000_GIOCTL, 0x2F, 0x81);

		/* PCIe PLL Configuration */
		igb_write_8bit_ctrl_reg(hw, E1000_SCCTL, 0x02, 0x47);
		igb_write_8bit_ctrl_reg(hw, E1000_SCCTL, 0x14, 0x00);
		igb_write_8bit_ctrl_reg(hw, E1000_SCCTL, 0x10, 0x00);
	}

	return 0;
}

/**
 *  igb_read_mac_addr_82575 - Read device MAC address
 *  @hw: pointer to the HW structure
 **/
static s32 igb_read_mac_addr_82575(struct e1000_hw *hw)
{
	s32 ret_val = 0;

	/*
	 * If there's an alternate MAC address place it in RAR0
	 * so that it will override the Si installed default perm
	 * address.
	 */
	ret_val = igb_check_alt_mac_addr(hw);
	if (ret_val)
		goto out;

	ret_val = igb_read_mac_addr(hw);

out:
	return ret_val;
}

/**
 * igb_power_down_phy_copper_82575 - Remove link during PHY power down
 * @hw: pointer to the HW structure
 *
 * In the case of a PHY power down to save power, or to turn off link during a
 * driver unload, or wake on lan is not enabled, remove the link.
 **/
void igb_power_down_phy_copper_82575(struct e1000_hw *hw)
{
	/* If the management interface is not enabled, then power down */
	if (!(igb_enable_mng_pass_thru(hw) || igb_check_reset_block(hw)))
		igb_power_down_phy_copper(hw);
}

/**
 *  igb_clear_hw_cntrs_82575 - Clear device specific hardware counters
 *  @hw: pointer to the HW structure
 *
 *  Clears the hardware counters by reading the counter registers.
 **/
static void igb_clear_hw_cntrs_82575(struct e1000_hw *hw)
{
	igb_clear_hw_cntrs_base(hw);

	rd32(E1000_PRC64);
	rd32(E1000_PRC127);
	rd32(E1000_PRC255);
	rd32(E1000_PRC511);
	rd32(E1000_PRC1023);
	rd32(E1000_PRC1522);
	rd32(E1000_PTC64);
	rd32(E1000_PTC127);
	rd32(E1000_PTC255);
	rd32(E1000_PTC511);
	rd32(E1000_PTC1023);
	rd32(E1000_PTC1522);

	rd32(E1000_ALGNERRC);
	rd32(E1000_RXERRC);
	rd32(E1000_TNCRS);
	rd32(E1000_CEXTERR);
	rd32(E1000_TSCTC);
	rd32(E1000_TSCTFC);

	rd32(E1000_MGTPRC);
	rd32(E1000_MGTPDC);
	rd32(E1000_MGTPTC);

	rd32(E1000_IAC);
	rd32(E1000_ICRXOC);

	rd32(E1000_ICRXPTC);
	rd32(E1000_ICRXATC);
	rd32(E1000_ICTXPTC);
	rd32(E1000_ICTXATC);
	rd32(E1000_ICTXQEC);
	rd32(E1000_ICTXQMTC);
	rd32(E1000_ICRXDMTC);

	rd32(E1000_CBTMPC);
	rd32(E1000_HTDPMC);
	rd32(E1000_CBRMPC);
	rd32(E1000_RPTHC);
	rd32(E1000_HGPTC);
	rd32(E1000_HTCBDPC);
	rd32(E1000_HGORCL);
	rd32(E1000_HGORCH);
	rd32(E1000_HGOTCL);
	rd32(E1000_HGOTCH);
	rd32(E1000_LENERRS);

	/* This register should not be read in copper configurations */
	if (hw->phy.media_type == e1000_media_type_internal_serdes ||
	    igb_sgmii_active_82575(hw))
		rd32(E1000_SCVPC);
}

/**
 *  igb_rx_fifo_flush_82575 - Clean rx fifo after RX enable
 *  @hw: pointer to the HW structure
 *
 *  After rx enable if managability is enabled then there is likely some
 *  bad data at the start of the fifo and possibly in the DMA fifo.  This
 *  function clears the fifos and flushes any packets that came in as rx was
 *  being enabled.
 **/
void igb_rx_fifo_flush_82575(struct e1000_hw *hw)
{
	u32 rctl, rlpml, rxdctl[4], rfctl, temp_rctl, rx_enabled;
	int i, ms_wait;

	if (hw->mac.type != e1000_82575 ||
	    !(rd32(E1000_MANC) & E1000_MANC_RCV_TCO_EN))
		return;

	/* Disable all RX queues */
	for (i = 0; i < 4; i++) {
		rxdctl[i] = rd32(E1000_RXDCTL(i));
		wr32(E1000_RXDCTL(i),
		     rxdctl[i] & ~E1000_RXDCTL_QUEUE_ENABLE);
	}
	/* Poll all queues to verify they have shut down */
	for (ms_wait = 0; ms_wait < 10; ms_wait++) {
		msleep(1);
		rx_enabled = 0;
		for (i = 0; i < 4; i++)
			rx_enabled |= rd32(E1000_RXDCTL(i));
		if (!(rx_enabled & E1000_RXDCTL_QUEUE_ENABLE))
			break;
	}

	if (ms_wait == 10)
		hw_dbg("Queue disable timed out after 10ms\n");

	/* Clear RLPML, RCTL.SBP, RFCTL.LEF, and set RCTL.LPE so that all
	 * incoming packets are rejected.  Set enable and wait 2ms so that
	 * any packet that was coming in as RCTL.EN was set is flushed
	 */
	rfctl = rd32(E1000_RFCTL);
	wr32(E1000_RFCTL, rfctl & ~E1000_RFCTL_LEF);

	rlpml = rd32(E1000_RLPML);
	wr32(E1000_RLPML, 0);

	rctl = rd32(E1000_RCTL);
	temp_rctl = rctl & ~(E1000_RCTL_EN | E1000_RCTL_SBP);
	temp_rctl |= E1000_RCTL_LPE;

	wr32(E1000_RCTL, temp_rctl);
	wr32(E1000_RCTL, temp_rctl | E1000_RCTL_EN);
	wrfl();
	msleep(2);

	/* Enable RX queues that were previously enabled and restore our
	 * previous state
	 */
	for (i = 0; i < 4; i++)
		wr32(E1000_RXDCTL(i), rxdctl[i]);
	wr32(E1000_RCTL, rctl);
	wrfl();

	wr32(E1000_RLPML, rlpml);
	wr32(E1000_RFCTL, rfctl);

	rd32(E1000_ROC);
	rd32(E1000_RNBC);
	rd32(E1000_MPC);
}

/**
 *  igb_set_pcie_completion_timeout - set pci-e completion timeout
 *  @hw: pointer to the HW structure
 *
 *  The defaults for 82575 and 82576 should be in the range of 50us to 50ms,
 *  however the hardware default for these parts is 500us to 1ms which is less
 *  than the 10ms recommended by the pci-e spec.  To address this we need to
 *  increase the value to either 10ms to 200ms for capability version 1 config,
 *  or 16ms to 55ms for version 2.
 **/
static s32 igb_set_pcie_completion_timeout(struct e1000_hw *hw)
{
	u32 gcr = rd32(E1000_GCR);
	s32 ret_val = 0;
	u16 pcie_devctl2;

	/* only take action if timeout value is defaulted to 0 */
	if (gcr & E1000_GCR_CMPL_TMOUT_MASK)
		goto out;

	/*
	 * if capababilities version is type 1 we can write the
	 * timeout of 10ms to 200ms through the GCR register
	 */
	if (!(gcr & E1000_GCR_CAP_VER2)) {
		gcr |= E1000_GCR_CMPL_TMOUT_10ms;
		goto out;
	}

	/*
	 * for version 2 capabilities we need to write the config space
	 * directly in order to set the completion timeout value for
	 * 16ms to 55ms
	 */
	ret_val = igb_read_pcie_cap_reg(hw, PCIE_DEVICE_CONTROL2,
	                                &pcie_devctl2);
	if (ret_val)
		goto out;

	pcie_devctl2 |= PCIE_DEVICE_CONTROL2_16ms;

	ret_val = igb_write_pcie_cap_reg(hw, PCIE_DEVICE_CONTROL2,
	                                 &pcie_devctl2);
out:
	/* disable completion timeout resend */
	gcr &= ~E1000_GCR_CMPL_TMOUT_RESEND;

	wr32(E1000_GCR, gcr);
	return ret_val;
}

/**
 *  igb_vmdq_set_loopback_pf - enable or disable vmdq loopback
 *  @hw: pointer to the hardware struct
 *  @enable: state to enter, either enabled or disabled
 *
 *  enables/disables L2 switch loopback functionality.
 **/
void igb_vmdq_set_loopback_pf(struct e1000_hw *hw, bool enable)
{
	u32 dtxswc = rd32(E1000_DTXSWC);

	if (enable)
		dtxswc |= E1000_DTXSWC_VMDQ_LOOPBACK_EN;
	else
		dtxswc &= ~E1000_DTXSWC_VMDQ_LOOPBACK_EN;

	wr32(E1000_DTXSWC, dtxswc);
}

/**
 *  igb_vmdq_set_replication_pf - enable or disable vmdq replication
 *  @hw: pointer to the hardware struct
 *  @enable: state to enter, either enabled or disabled
 *
 *  enables/disables replication of packets across multiple pools.
 **/
void igb_vmdq_set_replication_pf(struct e1000_hw *hw, bool enable)
{
	u32 vt_ctl = rd32(E1000_VT_CTL);

	if (enable)
		vt_ctl |= E1000_VT_CTL_VM_REPL_EN;
	else
		vt_ctl &= ~E1000_VT_CTL_VM_REPL_EN;

	wr32(E1000_VT_CTL, vt_ctl);
}

/**
 *  igb_read_phy_reg_82580 - Read 82580 MDI control register
 *  @hw: pointer to the HW structure
 *  @offset: register offset to be read
 *  @data: pointer to the read data
 *
 *  Reads the MDI control register in the PHY at offset and stores the
 *  information read to data.
 **/
static s32 igb_read_phy_reg_82580(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val;


	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	ret_val = igb_read_phy_reg_mdic(hw, offset, data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

/**
 *  igb_write_phy_reg_82580 - Write 82580 MDI control register
 *  @hw: pointer to the HW structure
 *  @offset: register offset to write to
 *  @data: data to write to register at offset
 *
 *  Writes data to MDI control register in the PHY at offset.
 **/
static s32 igb_write_phy_reg_82580(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val;


	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	ret_val = igb_write_phy_reg_mdic(hw, offset, data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

/**
 *  igb_reset_mdicnfg_82580 - Reset MDICNFG destination and com_mdio bits
 *  @hw: pointer to the HW structure
 *
 *  This resets the the MDICNFG.Destination and MDICNFG.Com_MDIO bits based on
 *  the values found in the EEPROM.  This addresses an issue in which these
 *  bits are not restored from EEPROM after reset.
 **/
static s32 igb_reset_mdicnfg_82580(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u32 mdicnfg;
	u16 nvm_data;

	if (hw->mac.type != e1000_82580)
		goto out;
	if (!igb_sgmii_active_82575(hw))
		goto out;

	ret_val = hw->nvm.ops.read(hw, NVM_INIT_CONTROL3_PORT_A +
				   NVM_82580_LAN_FUNC_OFFSET(hw->bus.func), 1,
				   &nvm_data);
	if (ret_val) {
		hw_dbg("NVM Read Error\n");
		goto out;
	}

	mdicnfg = rd32(E1000_MDICNFG);
	if (nvm_data & NVM_WORD24_EXT_MDIO)
		mdicnfg |= E1000_MDICNFG_EXT_MDIO;
	if (nvm_data & NVM_WORD24_COM_MDIO)
		mdicnfg |= E1000_MDICNFG_COM_MDIO;
	wr32(E1000_MDICNFG, mdicnfg);
out:
	return ret_val;
}

/**
 *  igb_reset_hw_82580 - Reset hardware
 *  @hw: pointer to the HW structure
 *
 *  This resets function or entire device (all ports, etc.)
 *  to a known state.
 **/
static s32 igb_reset_hw_82580(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	/* BH SW mailbox bit in SW_FW_SYNC */
	u16 swmbsw_mask = E1000_SW_SYNCH_MB;
	u32 ctrl, icr;
	bool global_device_reset = hw->dev_spec._82575.global_device_reset;


	hw->dev_spec._82575.global_device_reset = false;

	/* Get current control state. */
	ctrl = rd32(E1000_CTRL);

	/*
	 * Prevent the PCI-E bus from sticking if there is no TLP connection
	 * on the last TLP read/write transaction when MAC is reset.
	 */
	ret_val = igb_disable_pcie_master(hw);
	if (ret_val)
		hw_dbg("PCI-E Master disable polling has failed.\n");

	hw_dbg("Masking off all interrupts\n");
	wr32(E1000_IMC, 0xffffffff);
	wr32(E1000_RCTL, 0);
	wr32(E1000_TCTL, E1000_TCTL_PSP);
	wrfl();

	msleep(10);

	/* Determine whether or not a global dev reset is requested */
	if (global_device_reset &&
		igb_acquire_swfw_sync_82575(hw, swmbsw_mask))
			global_device_reset = false;

	if (global_device_reset &&
		!(rd32(E1000_STATUS) & E1000_STAT_DEV_RST_SET))
		ctrl |= E1000_CTRL_DEV_RST;
	else
		ctrl |= E1000_CTRL_RST;

	wr32(E1000_CTRL, ctrl);

	/* Add delay to insure DEV_RST has time to complete */
	if (global_device_reset)
		msleep(5);

	ret_val = igb_get_auto_rd_done(hw);
	if (ret_val) {
		/*
		 * When auto config read does not complete, do not
		 * return with an error. This can happen in situations
		 * where there is no eeprom and prevents getting link.
		 */
		hw_dbg("Auto Read Done did not complete\n");
	}

	/* If EEPROM is not present, run manual init scripts */
	if ((rd32(E1000_EECD) & E1000_EECD_PRES) == 0)
		igb_reset_init_script_82575(hw);

	/* clear global device reset status bit */
	wr32(E1000_STATUS, E1000_STAT_DEV_RST_SET);

	/* Clear any pending interrupt events. */
	wr32(E1000_IMC, 0xffffffff);
	icr = rd32(E1000_ICR);

	ret_val = igb_reset_mdicnfg_82580(hw);
	if (ret_val)
		hw_dbg("Could not reset MDICNFG based on EEPROM\n");

	/* Install any alternate MAC address into RAR0 */
	ret_val = igb_check_alt_mac_addr(hw);

	/* Release semaphore */
	if (global_device_reset)
		igb_release_swfw_sync_82575(hw, swmbsw_mask);

	return ret_val;
}

/**
 *  igb_rxpbs_adjust_82580 - adjust RXPBS value to reflect actual RX PBA size
 *  @data: data received by reading RXPBS register
 *
 *  The 82580 uses a table based approach for packet buffer allocation sizes.
 *  This function converts the retrieved value into the correct table value
 *     0x0 0x1 0x2 0x3 0x4 0x5 0x6 0x7
 *  0x0 36  72 144   1   2   4   8  16
 *  0x8 35  70 140 rsv rsv rsv rsv rsv
 */
u16 igb_rxpbs_adjust_82580(u32 data)
{
	u16 ret_val = 0;

	if (data < E1000_82580_RXPBS_TABLE_SIZE)
		ret_val = e1000_82580_rxpbs_table[data];

	return ret_val;
}

static struct e1000_mac_operations e1000_mac_ops_82575 = {
	.init_hw              = igb_init_hw_82575,
	.check_for_link       = igb_check_for_link_82575,
	.rar_set              = igb_rar_set,
	.read_mac_addr        = igb_read_mac_addr_82575,
	.get_speed_and_duplex = igb_get_speed_and_duplex_copper,
};

static struct e1000_phy_operations e1000_phy_ops_82575 = {
	.acquire              = igb_acquire_phy_82575,
	.get_cfg_done         = igb_get_cfg_done_82575,
	.release              = igb_release_phy_82575,
};

static struct e1000_nvm_operations e1000_nvm_ops_82575 = {
	.acquire              = igb_acquire_nvm_82575,
	.read                 = igb_read_nvm_eerd,
	.release              = igb_release_nvm_82575,
	.write                = igb_write_nvm_spi,
};

const struct e1000_info e1000_82575_info = {
	.get_invariants = igb_get_invariants_82575,
	.mac_ops = &e1000_mac_ops_82575,
	.phy_ops = &e1000_phy_ops_82575,
	.nvm_ops = &e1000_nvm_ops_82575,
};
