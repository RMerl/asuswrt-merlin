/* Copyright (c) 2007 Intel Corporation */
#include <stdio.h>
#include "internal.h"

/* Register Bit Masks */
#define IXGBE_FCTRL_SBP            0x00000002
#define IXGBE_FCTRL_MPE            0x00000100
#define IXGBE_FCTRL_UPE            0x00000200
#define IXGBE_FCTRL_BAM            0x00000400
#define IXGBE_FCTRL_PMCF           0x00001000
#define IXGBE_FCTRL_DPF            0x00002000
#define IXGBE_FCTRL_RPFCE          0x00004000
#define IXGBE_FCTRL_RFCE           0x00008000
#define IXGBE_VLNCTRL_VET          0x0000FFFF
#define IXGBE_VLNCTRL_CFI          0x10000000
#define IXGBE_VLNCTRL_CFIEN        0x20000000
#define IXGBE_VLNCTRL_VFE          0x40000000
#define IXGBE_VLNCTRL_VME          0x80000000
#define IXGBE_LINKS_UP             0x40000000
#define IXGBE_LINKS_SPEED          0x20000000
#define IXGBE_SRRCTL_BSIZEPKT_MASK 0x0000007F
#define IXGBE_HLREG0_TXCRCEN       0x00000001
#define IXGBE_HLREG0_RXCRCSTRP     0x00000002
#define IXGBE_HLREG0_JUMBOEN       0x00000004
#define IXGBE_HLREG0_TXPADEN       0x00000400
#define IXGBE_HLREG0_LPBK          0x00008000
#define IXGBE_RMCS_TFCE_802_3X     0x00000008
#define IXGBE_RMCS_TFCE_PRIORITY   0x00000010
#define IXGBE_FCCFG_TFCE_802_3X    0x00000008
#define IXGBE_FCCFG_TFCE_PRIORITY  0x00000010
#define IXGBE_MFLCN_PMCF           0x00000001 /* Pass MAC Control Frames */
#define IXGBE_MFLCN_DPF            0x00000002 /* Discard Pause Frame */
#define IXGBE_MFLCN_RPFCE          0x00000004 /* Receive Priority FC Enable */
#define IXGBE_MFLCN_RFCE           0x00000008 /* Receive FC Enable */

/* Device IDs */
#define IXGBE_DEV_ID_82598               0x10B6
#define IXGBE_DEV_ID_82598_BX            0x1508
#define IXGBE_DEV_ID_82598AF_DUAL_PORT   0x10C6
#define IXGBE_DEV_ID_82598AF_SINGLE_PORT 0x10C7
#define IXGBE_DEV_ID_82598EB_SFP_LOM     0x10DB
#define IXGBE_DEV_ID_82598AT             0x10C8
#define IXGBE_DEV_ID_82598AT2            0x150B
#define IXGBE_DEV_ID_82598EB_CX4         0x10DD
#define IXGBE_DEV_ID_82598_CX4_DUAL_PORT 0x10EC
#define IXGBE_DEV_ID_82598_DA_DUAL_PORT  0x10F1
#define IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM      0x10E1
#define IXGBE_DEV_ID_82598EB_XF_LR       0x10F4
#define IXGBE_DEV_ID_82599_KX4           0x10F7
#define IXGBE_DEV_ID_82599_KX4_MEZZ      0x1514
#define IXGBE_DEV_ID_82599_KR            0x1517
#define IXGBE_DEV_ID_82599_T3_LOM        0x151C
#define IXGBE_DEV_ID_82599_CX4           0x10F9
#define IXGBE_DEV_ID_82599_SFP           0x10FB
#define IXGBE_DEV_ID_82599_BACKPLANE_FCOE       0x152a
#define IXGBE_DEV_ID_82599_SFP_FCOE      0x1529
#define IXGBE_SUBDEV_ID_82599_SFP        0x11A9
#define IXGBE_DEV_ID_82599_SFP_EM        0x1507
#define IXGBE_DEV_ID_82599_SFP_SF2       0x154D
#define IXGBE_DEV_ID_82599EN_SFP         0x1557
#define IXGBE_DEV_ID_82599_XAUI_LOM      0x10FC
#define IXGBE_DEV_ID_82599_COMBO_BACKPLANE 0x10F8
#define IXGBE_SUBDEV_ID_82599_KX4_KR_MEZZ  0x000C
#define IXGBE_DEV_ID_82599_LS            0x154F
#define IXGBE_DEV_ID_X540T               0x1528
#define IXGBE_DEV_ID_82599_SFP_SF_QP     0x154A
#define IXGBE_DEV_ID_82599_QSFP_SF_QP    0x1558
#define IXGBE_DEV_ID_X540T1              0x1560

#define IXGBE_DEV_ID_X550T		0x1563
#define IXGBE_DEV_ID_X550T1		0x15D1
#define IXGBE_DEV_ID_X550EM_X_KX4	0x15AA
#define IXGBE_DEV_ID_X550EM_X_KR	0x15AB
#define IXGBE_DEV_ID_X550EM_X_SFP	0x15AC
#define IXGBE_DEV_ID_X550EM_X_10G_T	0x15AD
#define IXGBE_DEV_ID_X550EM_X_1G_T	0x15AE
#define IXGBE_DEV_ID_X550EM_A_KR	0x15C2
#define IXGBE_DEV_ID_X550EM_A_KR_L	0x15C3
#define IXGBE_DEV_ID_X550EM_A_SFP_N	0x15C4
#define IXGBE_DEV_ID_X550EM_A_SGMII	0x15C6
#define IXGBE_DEV_ID_X550EM_A_SGMII_L	0x15C7
#define IXGBE_DEV_ID_X550EM_A_SFP	0x15CE

/*
 * Enumerated types specific to the ixgbe hardware
 * Media Access Controlers
 */
enum ixgbe_mac_type {
	ixgbe_mac_unknown = 0,
	ixgbe_mac_82598EB,
	ixgbe_mac_82599EB,
	ixgbe_mac_X540,
	ixgbe_mac_x550,
	ixgbe_mac_x550em_x,
	ixgbe_mac_x550em_a,
	ixgbe_num_macs
};

static enum ixgbe_mac_type
ixgbe_get_mac_type(u16 device_id)
{
	enum ixgbe_mac_type mac_type = ixgbe_mac_unknown;

	switch (device_id) {
	case IXGBE_DEV_ID_82598:
	case IXGBE_DEV_ID_82598_BX:
	case IXGBE_DEV_ID_82598AF_DUAL_PORT:
	case IXGBE_DEV_ID_82598AF_SINGLE_PORT:
	case IXGBE_DEV_ID_82598EB_SFP_LOM:
	case IXGBE_DEV_ID_82598AT:
	case IXGBE_DEV_ID_82598AT2:
	case IXGBE_DEV_ID_82598EB_CX4:
	case IXGBE_DEV_ID_82598_CX4_DUAL_PORT:
	case IXGBE_DEV_ID_82598_DA_DUAL_PORT:
	case IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM:
	case IXGBE_DEV_ID_82598EB_XF_LR:
		mac_type = ixgbe_mac_82598EB;
		break;
	case IXGBE_DEV_ID_82599_KX4:
	case IXGBE_DEV_ID_82599_KX4_MEZZ:
	case IXGBE_DEV_ID_82599_KR:
	case IXGBE_DEV_ID_82599_T3_LOM:
	case IXGBE_DEV_ID_82599_CX4:
	case IXGBE_DEV_ID_82599_SFP:
	case IXGBE_DEV_ID_82599_BACKPLANE_FCOE:
	case IXGBE_DEV_ID_82599_SFP_FCOE:
	case IXGBE_SUBDEV_ID_82599_SFP:
	case IXGBE_DEV_ID_82599_SFP_EM:
	case IXGBE_DEV_ID_82599_SFP_SF2:
	case IXGBE_DEV_ID_82599EN_SFP:
	case IXGBE_DEV_ID_82599_XAUI_LOM:
	case IXGBE_DEV_ID_82599_COMBO_BACKPLANE:
	case IXGBE_SUBDEV_ID_82599_KX4_KR_MEZZ:
	case IXGBE_DEV_ID_82599_LS:
	case IXGBE_DEV_ID_82599_SFP_SF_QP:
	case IXGBE_DEV_ID_82599_QSFP_SF_QP:
		mac_type = ixgbe_mac_82599EB;
		break;
	case IXGBE_DEV_ID_X540T:
	case IXGBE_DEV_ID_X540T1:
		mac_type = ixgbe_mac_X540;
		break;
	case IXGBE_DEV_ID_X550T:
	case IXGBE_DEV_ID_X550T1:
		mac_type = ixgbe_mac_x550;
		break;
	case IXGBE_DEV_ID_X550EM_X_KX4:
	case IXGBE_DEV_ID_X550EM_X_KR:
	case IXGBE_DEV_ID_X550EM_X_SFP:
	case IXGBE_DEV_ID_X550EM_X_10G_T:
	case IXGBE_DEV_ID_X550EM_X_1G_T:
		mac_type = ixgbe_mac_x550em_x;
		break;
	case IXGBE_DEV_ID_X550EM_A_KR:
	case IXGBE_DEV_ID_X550EM_A_KR_L:
	case IXGBE_DEV_ID_X550EM_A_SFP_N:
	case IXGBE_DEV_ID_X550EM_A_SGMII:
	case IXGBE_DEV_ID_X550EM_A_SGMII_L:
	case IXGBE_DEV_ID_X550EM_A_SFP:
		mac_type = ixgbe_mac_x550em_a;
		break;
	default:
		mac_type = ixgbe_mac_82598EB;
		break;
	}

	return mac_type;
}

int
ixgbe_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 *regs_buff = (u32 *)regs->data;
	u32 regs_buff_len = regs->len / sizeof(*regs_buff);
	u32 reg;
	u32 offset;
	u16 hw_device_id = (u16) regs->version;
	u8 version = (u8)(regs->version >> 24);
	u8 i;
	enum ixgbe_mac_type mac_type;

	if (version == 0)
		return -1;

	/* The current driver reports the MAC type, but older versions
	 * only report the device ID so we have to infer the MAC type.
	 */
	mac_type = version > 1 ? version : ixgbe_get_mac_type(hw_device_id);

	reg = regs_buff[1065];
	fprintf(stdout,
	"0x042A4: LINKS (Link Status register)                 0x%08X\n"
	"       Link Status:                                   %s\n"
	"       Link Speed:                                    %s\n",
	reg,
	reg & IXGBE_LINKS_UP      ? "up"       : "down",
	reg & IXGBE_LINKS_SPEED   ? "10G"      : "1G");

	reg = regs_buff[515];
	fprintf(stdout,
	"0x05080: FCTRL (Filter Control register)              0x%08X\n"
	"       Broadcast Accept:                              %s\n"
	"       Unicast Promiscuous:                           %s\n"
	"       Multicast Promiscuous:                         %s\n"
	"       Store Bad Packets:                             %s\n",
	reg,
	reg & IXGBE_FCTRL_BAM     ? "enabled"  : "disabled",
	reg & IXGBE_FCTRL_UPE     ? "enabled"  : "disabled",
	reg & IXGBE_FCTRL_MPE     ? "enabled"  : "disabled",
	reg & IXGBE_FCTRL_SBP     ? "enabled"  : "disabled");

	/* Some FCTRL bits are valid only on 82598 */
	if (mac_type == ixgbe_mac_82598EB) {
		fprintf(stdout,
		"       Receive Flow Control Packets:                  %s\n"
		"       Receive Priority Flow Control Packets:         %s\n"
		"       Discard Pause Frames:                          %s\n"
		"       Pass MAC Control Frames:                       %s\n",
		reg & IXGBE_FCTRL_RFCE    ? "enabled"  : "disabled",
		reg & IXGBE_FCTRL_RPFCE   ? "enabled"  : "disabled",
		reg & IXGBE_FCTRL_DPF     ? "enabled"  : "disabled",
		reg & IXGBE_FCTRL_PMCF    ? "enabled"  : "disabled");
	 }

	reg = regs_buff[1128];
	if (mac_type >= ixgbe_mac_82599EB) {
		fprintf(stdout,
		"0x04294: MFLCN (TabMAC Flow Control register)         0x%08X\n"
		"       Receive Flow Control Packets:                  %s\n"
		"       Discard Pause Frames:                          %s\n"
		"       Pass MAC Control Frames:                       %s\n"
		"       Receive Priority Flow Control Packets:         %s\n",
		reg,
		reg & IXGBE_MFLCN_RFCE    ? "enabled"  : "disabled",
		reg & IXGBE_FCTRL_DPF     ? "enabled"  : "disabled",
		reg & IXGBE_FCTRL_PMCF    ? "enabled"  : "disabled",
		reg & IXGBE_FCTRL_RPFCE   ? "enabled"  : "disabled");
	}

	reg = regs_buff[516];
	fprintf(stdout,
	"0x05088: VLNCTRL (VLAN Control register)              0x%08X\n"
	"       VLAN Mode:                                     %s\n"
	"       VLAN Filter:                                   %s\n",
	reg,
	reg & IXGBE_VLNCTRL_VME   ? "enabled"  : "disabled",
	reg & IXGBE_VLNCTRL_VFE   ? "enabled"  : "disabled");

	reg = regs_buff[437];
	fprintf(stdout,
	"0x02100: SRRCTL0 (Split and Replic Rx Control 0)      0x%08X\n"
	"       Receive Buffer Size:                           %uKB\n",
	reg,
	(reg & IXGBE_SRRCTL_BSIZEPKT_MASK) <= 0x10 ? (reg & IXGBE_SRRCTL_BSIZEPKT_MASK) : 0x10);

	reg = regs_buff[829];
	if (mac_type == ixgbe_mac_82598EB) {
		fprintf(stdout,
		"0x03D00: RMCS (Receive Music Control register)        0x%08X\n"
		"       Transmit Flow Control:                         %s\n"
		"       Priority Flow Control:                         %s\n",
		reg,
		reg & IXGBE_RMCS_TFCE_802_3X     ? "enabled"  : "disabled",
		reg & IXGBE_RMCS_TFCE_PRIORITY   ? "enabled"  : "disabled");
	} else if (mac_type >= ixgbe_mac_82599EB) {
		fprintf(stdout,
		"0x03D00: FCCFG (Flow Control Configuration)           0x%08X\n"
		"       Transmit Flow Control:                         %s\n"
		"       Priority Flow Control:                         %s\n",
		reg,
		reg & IXGBE_FCCFG_TFCE_802_3X     ? "enabled"  : "disabled",
		reg & IXGBE_FCCFG_TFCE_PRIORITY   ? "enabled"  : "disabled");
	}

	reg = regs_buff[1047];
	fprintf(stdout,
	"0x04240: HLREG0 (Highlander Control 0 register)       0x%08X\n"
	"       Transmit CRC:                                  %s\n"
	"       Receive CRC Strip:                             %s\n"
	"       Jumbo Frames:                                  %s\n"
	"       Pad Short Frames:                              %s\n"
	"       Loopback:                                      %s\n",
	reg,
	reg & IXGBE_HLREG0_TXCRCEN   ? "enabled"  : "disabled",
	reg & IXGBE_HLREG0_RXCRCSTRP ? "enabled"  : "disabled",
	reg & IXGBE_HLREG0_JUMBOEN   ? "enabled"  : "disabled",
	reg & IXGBE_HLREG0_TXPADEN   ? "enabled"  : "disabled",
	reg & IXGBE_HLREG0_LPBK      ? "enabled"  : "disabled");

	/* General Registers */
	fprintf(stdout,
		"0x00000: CTRL        (Device Control)                 0x%08X\n",
		regs_buff[0]);

	fprintf(stdout,
		"0x00008: STATUS      (Device Status)                  0x%08X\n",
		regs_buff[1]);

	fprintf(stdout,
		"0x00018: CTRL_EXT    (Extended Device Control)        0x%08X\n",
		regs_buff[2]);

	fprintf(stdout,
		"0x00020: ESDP        (Extended SDP Control)           0x%08X\n",
		regs_buff[3]);

	fprintf(stdout,
		"0x00028: EODSDP      (Extended OD SDP Control)        0x%08X\n",
		regs_buff[4]);

	fprintf(stdout,
		"0x00200: LEDCTL      (LED Control)                    0x%08X\n",
		regs_buff[5]);

	fprintf(stdout,
		"0x00048: FRTIMER     (Free Running Timer)             0x%08X\n",
		regs_buff[6]);

	fprintf(stdout,
		"0x0004C: TCPTIMER    (TCP Timer)                      0x%08X\n",
		regs_buff[7]);

	/* NVM Register */
	offset = mac_type == ixgbe_mac_x550em_a ? 0x15FF8 : 0x10010;
	fprintf(stdout,
		"0x%05X: EEC         (EEPROM/Flash Control)           0x%08X\n",
		offset, regs_buff[8]);

	fprintf(stdout,
		"0x10014: EERD        (EEPROM Read)                    0x%08X\n",
		regs_buff[9]);

	offset = mac_type == ixgbe_mac_x550em_a ? 0x15F6C : 0x1001C;
	fprintf(stdout,
		"0x%05X: FLA         (Flash Access)                   0x%08X\n",
		offset, regs_buff[10]);

	fprintf(stdout,
		"0x10110: EEMNGCTL    (Manageability EEPROM Control)   0x%08X\n",
		regs_buff[11]);

	fprintf(stdout,
		"0x10114: EEMNGDATA   (Manageability EEPROM R/W Data)  0x%08X\n",
		regs_buff[12]);

	fprintf(stdout,
		"0x10118: FLMNGCTL    (Manageability Flash Control)    0x%08X\n",
		regs_buff[13]);

	fprintf(stdout,
		"0x1011C: FLMNGDATA   (Manageability Flash Read Data)  0x%08X\n",
		regs_buff[14]);

	fprintf(stdout,
		"0x10120: FLMNGCNT    (Manageability Flash Read Count) 0x%08X\n",
		regs_buff[15]);

	fprintf(stdout,
		"0x1013C: FLOP        (Flash Opcode)                   0x%08X\n",
		regs_buff[16]);

	offset = mac_type == ixgbe_mac_x550em_a ? 0x15F64 : 0x10200;
	fprintf(stdout,
		"0x%05X: GRC         (General Receive Control)        0x%08X\n",
		offset, regs_buff[17]);

	/* Interrupt */
	fprintf(stdout,
		"0x00800: EICR        (Extended Interrupt Cause)       0x%08X\n",
		regs_buff[18]);

	fprintf(stdout,
		"0x00808: EICS        (Extended Interrupt Cause Set)   0x%08X\n",
		regs_buff[19]);

	fprintf(stdout,
		"0x00880: EIMS        (Extended Interr. Mask Set/Read) 0x%08X\n",
		regs_buff[20]);

	fprintf(stdout,
		"0x00888: EIMC        (Extended Interrupt Mask Clear)  0x%08X\n",
		regs_buff[21]);

	fprintf(stdout,
		"0x00810: EIAC        (Extended Interrupt Auto Clear)  0x%08X\n",
		regs_buff[22]);

	fprintf(stdout,
		"0x00890: EIAM        (Extended Interr. Auto Mask EN)  0x%08X\n",
		regs_buff[23]);

	fprintf(stdout,
		"0x00820: EITR0       (Extended Interrupt Throttle 0)  0x%08X\n",
		regs_buff[24]);

	fprintf(stdout,
		"0x00900: IVAR0       (Interrupt Vector Allocation 0)  0x%08X\n",
		regs_buff[25]);

	fprintf(stdout,
		"0x00000: MSIXT       (MSI-X Table)                    0x%08X\n",
		regs_buff[26]);

	if (mac_type == ixgbe_mac_82598EB)
		fprintf(stdout,
			"0x02000: MSIXPBA     (MSI-X Pending Bit Array)        0x%08X\n",
			regs_buff[27]);

	fprintf(stdout,
		"0x11068: PBACL       (MSI-X PBA Clear)                0x%08X\n",
		regs_buff[28]);

	fprintf(stdout,
		"0x00898: GPIE        (General Purpose Interrupt EN)   0x%08X\n",
		regs_buff[29]);

	/* Flow Control */
	fprintf(stdout,
		"0x03008: PFCTOP      (Priority Flow Ctrl Type Opcode) 0x%08X\n",
		regs_buff[30]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x%05X: FCCTV%d      (Flow Ctrl Tx Timer Value %d)     0x%08X\n",
		0x03200 + (4 * i), i, i, regs_buff[31 + i]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: FCRTL%d      (Flow Ctrl Rx Threshold low %d)   0x%08X\n",
		0x3220 + (8 * i), i, i, regs_buff[35 + i]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: FCRTH%d      (Flow Ctrl Rx Threshold High %d)  0x%08X\n",
		0x3260 + (8 * i), i, i, regs_buff[43 + i]);

	fprintf(stdout,
		"0x032A0: FCRTV       (Flow Control Refresh Threshold) 0x%08X\n",
		regs_buff[51]);

	fprintf(stdout,
		"0x0CE00: TFCS        (Transmit Flow Control Status)   0x%08X\n",
		regs_buff[52]);

	/* Receive DMA */
	for (i = 0; i < 64; i++)
		fprintf(stdout,
		"0x%05X: RDBAL%02d     (Rx Desc Base Addr Low %02d)       0x%08X\n",
		0x01000 + (0x40 * i), i, i, regs_buff[53 + i]);

	for (i = 0; i < 64; i++)
		fprintf(stdout,
		"0x%05X: RDBAH%02d     (Rx Desc Base Addr High %02d)      0x%08X\n",
		0x01004 + (0x40 * i), i, i, regs_buff[117 + i]);

	for (i = 0; i < 64; i++)
		fprintf(stdout,
		"0x%05X: RDLEN%02d     (Receive Descriptor Length %02d)   0x%08X\n",
		0x01008 + (0x40 * i), i, i, regs_buff[181 + i]);

	for (i = 0; i < 64; i++)
		fprintf(stdout,
		"0x%05X: RDH%02d       (Receive Descriptor Head %02d)     0x%08X\n",
		0x01010 + (0x40 * i), i, i, regs_buff[245 + i]);

	for (i = 0; i < 64; i++)
		fprintf(stdout,
		"0x%05X: RDT%02d       (Receive Descriptor Tail %02d)     0x%08X\n",
		0x01018 + (0x40 * i), i, i, regs_buff[309 + i]);

	for (i = 0; i < 64; i++)
		fprintf(stdout,
		"0x%05X: RXDCTL%02d    (Receive Descriptor Control %02d)  0x%08X\n",
		0x01028 + (0x40 * i), i, i, regs_buff[373 + i]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x%05X: SRRCTL%02d    (Split and Replic Rx Control %02d) 0x%08X\n",
		0x02100 + (4 * i), i, i, regs_buff[437 + i]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x%05X: DCA_RXCTRL%02d (Rx DCA Control %02d)             0x%08X\n",
		0x02200 + (4 * i), i, i, regs_buff[453 + i]);

	fprintf(stdout,
		"0x02F00: RDRXCTL     (Receive DMA Control)            0x%08X\n",
		regs_buff[469]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: RXPBSIZE%d   (Receive Packet Buffer Size %d)   0x%08X\n",
		0x3C00 + (4 * i), i, i, regs_buff[470 + i]);

	fprintf(stdout,
		"0x03000: RXCTRL      (Receive Control)                0x%08X\n",
		regs_buff[478]);

	if (mac_type == ixgbe_mac_82598EB)
		fprintf(stdout,
			"0x03D04: DROPEN      (Drop Enable Control)            0x%08X\n",
			regs_buff[479]);

	/* Receive */
	fprintf(stdout,
		"0x05000: RXCSUM      (Receive Checksum Control)       0x%08X\n",
		regs_buff[480]);

	fprintf(stdout,
		"0x05008: RFCTL       (Receive Filter Control)         0x%08X\n",
		regs_buff[481]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x%05X: RAL%02d       (Receive Address Low%02d)          0x%08X\n",
		0x05400 + (8 * i), i, i, regs_buff[482 + i]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x%05X: RAH%02d       (Receive Address High %02d)        0x%08X\n",
		0x05404 + (8 * i), i, i, regs_buff[498 + i]);

	fprintf(stdout,
		"0x05480: PSRTYPE     (Packet Split Receive Type)      0x%08X\n",
		regs_buff[514]);

	fprintf(stdout,
		"0x05090: MCSTCTRL    (Multicast Control)              0x%08X\n",
		regs_buff[517]);

	fprintf(stdout,
		"0x05818: MRQC        (Multiple Rx Queues Command)     0x%08X\n",
		regs_buff[518]);

	fprintf(stdout,
		"0x0581C: VMD_CTL     (VMDq Control)                   0x%08X\n",
		regs_buff[519]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: IMIR%d       (Immediate Interrupt Rx %d)       0x%08X\n",
		0x05A80 + (4 * i), i, i, regs_buff[520 + i]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: IMIREXT%d    (Immed. Interr. Rx Extended %d)   0x%08X\n",
		0x05AA0 + (4 * i), i, i, regs_buff[528 + i]);

	fprintf(stdout,
		"0x05AC0: IMIRVP      (Immed. Interr. Rx VLAN Prior.)  0x%08X\n",
		regs_buff[536]);

	/* Transmit */
	for (i = 0; i < 32; i++)
		fprintf(stdout,
		"0x%05X: TDBAL%02d     (Tx Desc Base Addr Low %02d)       0x%08X\n",
		0x06000 + (0x40 * i), i, i, regs_buff[537 + i]);

	for (i = 0; i < 32; i++)
		fprintf(stdout,
		"0x%05X: TDBAH%02d     (Tx Desc Base Addr High %02d)      0x%08X\n",
		0x06004 + (0x40 * i), i, i, regs_buff[569 + i]);

	for (i = 0; i < 32; i++)
		fprintf(stdout,
		"0x%05X: TDLEN%02d     (Tx Descriptor Length %02d)        0x%08X\n",
		0x06008 + (0x40 * i), i, i, regs_buff[601 + i]);

	for (i = 0; i < 32; i++)
		fprintf(stdout,
		"0x%05X: TDH%02d       (Transmit Descriptor Head %02d)    0x%08X\n",
		0x06010 + (0x40 * i), i, i, regs_buff[633 + i]);

	for (i = 0; i < 32; i++)
		fprintf(stdout,
		"0x%05X: TDT%02d       (Transmit Descriptor Tail %02d)    0x%08X\n",
		0x06018 + (0x40 * i), i, i, regs_buff[665 + i]);

	for (i = 0; i < 32; i++)
		fprintf(stdout,
		"0x%05X: TXDCTL%02d    (Tx Descriptor Control %02d)       0x%08X\n",
		0x06028 + (0x40 * i), i, i, regs_buff[697 + i]);

	for (i = 0; i < 32; i++)
		fprintf(stdout,
		"0x%05X: TDWBAL%02d    (Tx Desc Compl. WB Addr low %02d)  0x%08X\n",
		0x06038 + (0x40 * i), i, i, regs_buff[729 + i]);

	for (i = 0; i < 32; i++)
		fprintf(stdout,
		"0x%05X: TDWBAH%02d    (Tx Desc Compl. WB Addr High %02d) 0x%08X\n",
		0x0603C + (0x40 * i), i, i, regs_buff[761 + i]);

	fprintf(stdout,
		"0x07E00: DTXCTL      (DMA Tx Control)                 0x%08X\n",
		regs_buff[793]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x%05X: DCA_TXCTRL%02d (Tx DCA Control %02d)             0x%08X\n",
		0x07200 + (4 * i), i, i, regs_buff[794 + i]);

	if (mac_type == ixgbe_mac_82598EB)
		fprintf(stdout,
			"0x0CB00: TIPG        (Transmit IPG Control)           0x%08X\n",
			regs_buff[810]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: TXPBSIZE%d   (Transmit Packet Buffer Size %d)  0x%08X\n",
		0x0CC00 + (4 * i), i, i, regs_buff[811 + i]);

	fprintf(stdout,
		"0x0CD10: MNGTXMAP    (Manageability Tx TC Mapping)    0x%08X\n",
		regs_buff[819]);

	/* Wake Up */
	fprintf(stdout,
		"0x05800: WUC         (Wake up Control)                0x%08X\n",
		regs_buff[820]);

	fprintf(stdout,
		"0x05808: WUFC        (Wake Up Filter Control)         0x%08X\n",
		regs_buff[821]);

	fprintf(stdout,
		"0x05810: WUS         (Wake Up Status)                 0x%08X\n",
		regs_buff[822]);

	fprintf(stdout,
		"0x05838: IPAV        (IP Address Valid)               0x%08X\n",
		regs_buff[823]);

	fprintf(stdout,
		"0x05840: IP4AT       (IPv4 Address Table)             0x%08X\n",
		regs_buff[824]);

	fprintf(stdout,
		"0x05880: IP6AT       (IPv6 Address Table)             0x%08X\n",
		regs_buff[825]);

	fprintf(stdout,
		"0x05900: WUPL        (Wake Up Packet Length)          0x%08X\n",
		regs_buff[826]);

	fprintf(stdout,
		"0x05A00: WUPM        (Wake Up Packet Memory)          0x%08X\n",
		regs_buff[827]);

	fprintf(stdout,
		"0x09000: FHFT        (Flexible Host Filter Table)     0x%08X\n",
		regs_buff[828]);

	/* DCB */
	if (mac_type == ixgbe_mac_82598EB) {
		fprintf(stdout,
		"0x07F40: DPMCS       (Desc. Plan Music Ctrl Status)   0x%08X\n",
		regs_buff[830]);

		fprintf(stdout,
		"0x0CD00: PDPMCS      (Pkt Data Plan Music ctrl Stat)  0x%08X\n",
		regs_buff[831]);

		fprintf(stdout,
		"0x050A0: RUPPBMR     (Rx User Prior to Pkt Buff Map)  0x%08X\n",
		regs_buff[832]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: RT2CR%d      (Receive T2 Configure %d)         0x%08X\n",
			0x03C20 + (4 * i), i, i, regs_buff[833 + i]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: RT2SR%d      (Receive T2 Status %d)            0x%08X\n",
			0x03C40 + (4 * i), i, i, regs_buff[841 + i]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: TDTQ2TCCR%d  (Tx Desc TQ2 TC Config %d)        0x%08X\n",
			0x0602C + (0x40 * i), i, i, regs_buff[849 + i]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: TDTQ2TCSR%d  (Tx Desc TQ2 TC Status %d)        0x%08X\n",
			0x0622C + (0x40 * i), i, i, regs_buff[857 + i]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: TDPT2TCCR%d  (Tx Data Plane T2 TC Config %d)   0x%08X\n",
			0x0CD20 + (4 * i), i, i, regs_buff[865 + i]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: TDPT2TCSR%d  (Tx Data Plane T2 TC Status %d)   0x%08X\n",
			0x0CD40 + (4 * i), i, i, regs_buff[873 + i]);
	} else if (mac_type >= ixgbe_mac_82599EB && mac_type <= ixgbe_mac_x550) {
		fprintf(stdout,
			"0x04900: RTTDCS      (Tx Descr Plane Ctrl&Status)     0x%08X\n",
			regs_buff[830]);

		fprintf(stdout,
			"0x0CD00: RTTPCS      (Tx Pkt Plane Ctrl&Status)       0x%08X\n",
			regs_buff[831]);

		fprintf(stdout,
			"0x02430: RTRPCS      (Rx Packet Plane Ctrl&Status)    0x%08X\n",
			regs_buff[832]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: RTRPT4C%d    (Rx Packet Plane T4 Config %d)    0x%08X\n",
			0x02140 + (4 * i), i, i, regs_buff[833 + i]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: RTRPT4S%d    (Rx Packet Plane T4 Status %d)    0x%08X\n",
			0x02160 + (4 * i), i, i, regs_buff[841 + i]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: RTTDT2C%d    (Tx Descr Plane T2 Config %d)     0x%08X\n",
			0x04910 + (4 * i), i, i, regs_buff[849 + i]);

		if (mac_type < ixgbe_mac_x550)
			for (i = 0; i < 8; i++)
				fprintf(stdout,
					"0x%05X: RTTDT2S%d    (Tx Descr Plane T2 Status %d)     0x%08X\n",
					0x04930 + (4 * i), i, i, regs_buff[857 + i]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
			"0x%05X: RTTPT2C%d    (Tx Packet Plane T2 Config %d)    0x%08X\n",
			0x0CD20 + (4 * i), i, i, regs_buff[865]);

		if (mac_type < ixgbe_mac_x550)
			for (i = 0; i < 8; i++)
				fprintf(stdout,
					"0x%05X: RTTPT2S%d    (Tx Packet Plane T2 Status %d)    0x%08X\n",
					0x0CD40 + (4 * i), i, i, regs_buff[873 + i]);
	}

	if (regs_buff_len > 1129 && mac_type != ixgbe_mac_82598EB) {
		fprintf(stdout,
			"0x03020: RTRUP2TC    (Rx User Prio to Traffic Classes)0x%08X\n",
			regs_buff[1129]);

		fprintf(stdout,
			"0x0C800: RTTUP2TC    (Tx User Prio to Traffic Classes)0x%08X\n",
			regs_buff[1130]);

		if (mac_type <= ixgbe_mac_x550)
			for (i = 0; i < 4; i++)
				fprintf(stdout,
					"0x%05X: TXLLQ%d      (Strict Low Lat Tx Queues %d)     0x%08X\n",
					0x082E0 + (4 * i), i, i, regs_buff[1131 + i]);

		if (mac_type == ixgbe_mac_82599EB) {
			fprintf(stdout,
				"0x04980: RTTBCNRM    (DCB TX Rate Sched MMW)          0x%08X\n",
				regs_buff[1135]);

			fprintf(stdout,
				"0x0498C: RTTBCNRD    (DCB TX Rate-Scheduler Drift)    0x%08X\n",
				regs_buff[1136]);
		} else if (mac_type <= ixgbe_mac_x550) {
			fprintf(stdout,
				"0x04980: RTTQCNRM    (DCB TX QCN Rate Sched MMW)      0x%08X\n",
				regs_buff[1135]);

			fprintf(stdout,
				"0x0498C: RTTQCNRR    (DCB TX QCN Rate Reset)          0x%08X\n",
				regs_buff[1136]);

			if (mac_type < ixgbe_mac_x550)
				fprintf(stdout,
					"0x08B00: RTTQCNCR    (DCB TX QCN Control)             0x%08X\n",
					regs_buff[1137]);

			fprintf(stdout,
				"0x04A90: RTTQCNTG    (DCB TX QCN Tagging)             0x%08X\n",
				regs_buff[1138]);
		}
	}

	/* Statistics */
	fprintf(stdout,
		"0x04000: crcerrs     (CRC Error Count)                0x%08X\n",
		regs_buff[881]);

	fprintf(stdout,
		"0x04004: illerrc     (Illegal Byte Error Count)       0x%08X\n",
		regs_buff[882]);

	fprintf(stdout,
		"0x04008: errbc       (Error Byte Count)               0x%08X\n",
		regs_buff[883]);

	fprintf(stdout,
		"0x04010: mspdc       (MAC Short Packet Discard Count) 0x%08X\n",
		regs_buff[884]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: mpc%d        (Missed Packets Count %d)         0x%08X\n",
		0x03FA0 + (4 * i), i, i, regs_buff[885 + i]);

	fprintf(stdout,
		"0x04034: mlfc        (MAC Local Fault Count)          0x%08X\n",
		regs_buff[893]);

	fprintf(stdout,
		"0x04038: mrfc        (MAC Remote Fault Count)         0x%08X\n",
		regs_buff[894]);

	fprintf(stdout,
		"0x04040: rlec        (Receive Length Error Count)     0x%08X\n",
		regs_buff[895]);

	fprintf(stdout,
		"0x03F60: lxontxc     (Link XON Transmitted Count)     0x%08X\n",
		regs_buff[896]);

	fprintf(stdout,
		"0x0CF60: lxonrxc     (Link XON Received Count)        0x%08X\n",
		regs_buff[897]);

	fprintf(stdout,
		"0x03F68: lxofftxc    (Link XOFF Transmitted Count)    0x%08X\n",
		regs_buff[898]);

	fprintf(stdout,
		"0x0CF68: lxoffrxc    (Link XOFF Received Count)       0x%08X\n",
		regs_buff[899]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: pxontxc%d    (Priority XON Tx Count %d)        0x%08X\n",
		0x03F00 + (4 * i), i, i, regs_buff[900 + i]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: pxonrxc%d    (Priority XON Received Count %d)  0x%08X\n",
		0x0CF00 + (4 * i), i, i, regs_buff[908 + i]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: pxofftxc%d   (Priority XOFF Tx Count %d)       0x%08X\n",
		0x03F20 + (4 * i), i, i, regs_buff[916 + i]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: pxoffrxc%d   (Priority XOFF Received Count %d) 0x%08X\n",
		0x0CF20 + (4 * i), i, i, regs_buff[924 + i]);

	fprintf(stdout,
		"0x0405C: prc64       (Packets Received (64B) Count)   0x%08X\n",
		regs_buff[932]);

	fprintf(stdout,
		"0x04060: prc127      (Packets Rx (65-127B) Count)     0x%08X\n",
		regs_buff[933]);

	fprintf(stdout,
		"0x04064: prc255      (Packets Rx (128-255B) Count)    0x%08X\n",
		regs_buff[934]);

	fprintf(stdout,
		"0x04068: prc511      (Packets Rx (256-511B) Count)    0x%08X\n",
		regs_buff[935]);

	fprintf(stdout,
		"0x0406C: prc1023     (Packets Rx (512-1023B) Count)   0x%08X\n",
		regs_buff[936]);

	fprintf(stdout,
		"0x04070: prc1522     (Packets Rx (1024-Max) Count)    0x%08X\n",
		regs_buff[937]);

	fprintf(stdout,
		"0x04074: gprc        (Good Packets Received Count)    0x%08X\n",
		regs_buff[938]);

	fprintf(stdout,
		"0x04078: bprc        (Broadcast Packets Rx Count)     0x%08X\n",
		regs_buff[939]);

	fprintf(stdout,
		"0x0407C: mprc        (Multicast Packets Rx Count)     0x%08X\n",
		regs_buff[940]);

	fprintf(stdout,
		"0x04080: gptc        (Good Packets Transmitted Count) 0x%08X\n",
		regs_buff[941]);

	fprintf(stdout,
		"0x04088: gorcl       (Good Octets Rx Count Low)       0x%08X\n",
		regs_buff[942]);

	fprintf(stdout,
		"0x0408C: gorch       (Good Octets Rx Count High)      0x%08X\n",
		regs_buff[943]);

	fprintf(stdout,
		"0x04090: gotcl       (Good Octets Tx Count Low)       0x%08X\n",
		regs_buff[944]);

	fprintf(stdout,
		"0x04094: gotch       (Good Octets Tx Count High)      0x%08X\n",
		regs_buff[945]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: rnbc%d       (Receive No Buffers Count %d)     0x%08X\n",
		0x03FC0 + (4 * i), i, i, regs_buff[946 + i]);

	fprintf(stdout,
		"0x040A4: ruc         (Receive Undersize count)        0x%08X\n",
		regs_buff[954]);

	fprintf(stdout,
		"0x040A8: rfc         (Receive Fragment Count)         0x%08X\n",
		regs_buff[955]);

	fprintf(stdout,
		"0x040AC: roc         (Receive Oversize Count)         0x%08X\n",
		regs_buff[956]);

	fprintf(stdout,
		"0x040B0: rjc         (Receive Jabber Count)           0x%08X\n",
		regs_buff[957]);

	fprintf(stdout,
		"0x040B4: mngprc      (Management Packets Rx Count)    0x%08X\n",
		regs_buff[958]);

	fprintf(stdout,
		"0x040B8: mngpdc      (Management Pkts Dropped Count)  0x%08X\n",
		regs_buff[959]);

	fprintf(stdout,
		"0x0CF90: mngptc      (Management Packets Tx Count)    0x%08X\n",
		regs_buff[960]);

	fprintf(stdout,
		"0x040C0: torl        (Total Octets Rx Count Low)      0x%08X\n",
		regs_buff[961]);

	fprintf(stdout,
		"0x040C4: torh        (Total Octets Rx Count High)     0x%08X\n",
		regs_buff[962]);

	fprintf(stdout,
		"0x040D0: tpr         (Total Packets Received)         0x%08X\n",
		regs_buff[963]);

	fprintf(stdout,
		"0x040D4: tpt         (Total Packets Transmitted)      0x%08X\n",
		regs_buff[964]);

	fprintf(stdout,
		"0x040D8: ptc64       (Packets Tx (64B) Count)         0x%08X\n",
		regs_buff[965]);

	fprintf(stdout,
		"0x040DC: ptc127      (Packets Tx (65-127B) Count)     0x%08X\n",
		regs_buff[966]);

	fprintf(stdout,
		"0x040E0: ptc255      (Packets Tx (128-255B) Count)    0x%08X\n",
		regs_buff[967]);

	fprintf(stdout,
		"0x040E4: ptc511      (Packets Tx (256-511B) Count)    0x%08X\n",
		regs_buff[968]);

	fprintf(stdout,
		"0x040E8: ptc1023     (Packets Tx (512-1023B) Count)   0x%08X\n",
		regs_buff[969]);

	fprintf(stdout,
		"0x040EC: ptc1522     (Packets Tx (1024-Max) Count)    0x%08X\n",
		regs_buff[970]);

	fprintf(stdout,
		"0x040F0: mptc        (Multicast Packets Tx Count)     0x%08X\n",
		regs_buff[971]);

	fprintf(stdout,
		"0x040F4: bptc        (Broadcast Packets Tx Count)     0x%08X\n",
		regs_buff[972]);

	fprintf(stdout,
		"0x04120: xec         (XSUM Error Count)               0x%08X\n",
		regs_buff[973]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x%05X: qprc%02d      (Queue Packets Rx Count %02d)      0x%08X\n",
		0x01030 + (0x40 * i), i, i, regs_buff[974 + i]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x%05X: qptc%02d      (Queue Packets Tx Count %02d)      0x%08X\n",
		0x06030 + (0x40 * i), i, i, regs_buff[990 + i]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x%05X: qbrc%02d      (Queue Bytes Rx Count %02d)        0x%08X\n",
		0x01034 + (0x40 * i), i, i, regs_buff[1006 + i]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x%05X: qbtc%02d      (Queue Bytes Tx Count %02d)        0x%08X\n",
		0x06034 + (0x40 * i), i, i, regs_buff[1022 + i]);

	/* MAC */
	if (mac_type < ixgbe_mac_X540) {
		fprintf(stdout,
			"0x04200: PCS1GCFIG   (PCS_1G Gloabal Config 1)        0x%08X\n",
			regs_buff[1038]);

		fprintf(stdout,
			"0x04208: PCS1GLCTL   (PCS_1G Link Control)            0x%08X\n",
			regs_buff[1039]);

		fprintf(stdout,
			"0x0420C: PCS1GLSTA   (PCS_1G Link Status)             0x%08X\n",
			regs_buff[1040]);

		fprintf(stdout,
			"0x04210: PCS1GDBG0   (PCS_1G Debug 0)                 0x%08X\n",
			regs_buff[1041]);

		fprintf(stdout,
			"0x04214: PCS1GDBG1   (PCS_1G Debug 1)                 0x%08X\n",
			regs_buff[1042]);

		fprintf(stdout,
			"0x04218: PCS1GANA    (PCS-1G Auto Neg. Adv.)          0x%08X\n",
			regs_buff[1043]);

		fprintf(stdout,
			"0x0421C: PCS1GANLP   (PCS-1G AN LP Ability)           0x%08X\n",
			regs_buff[1044]);

		fprintf(stdout,
			"0x04220: PCS1GANNP   (PCS_1G Auto Neg Next Page Tx)   0x%08X\n",
			regs_buff[1045]);

		fprintf(stdout,
			"0x04224: PCS1GANLPNP (PCS_1G Auto Neg LPs Next Page)  0x%08X\n",
			regs_buff[1046]);
	}

	fprintf(stdout,
		"0x04244: HLREG1      (Highlander Status 1)            0x%08X\n",
		regs_buff[1048]);

	fprintf(stdout,
		"0x04248: PAP         (Pause and Pace)                 0x%08X\n",
		regs_buff[1049]);

	fprintf(stdout,
		"0x0424C: MACA        (MDI Auto-Scan Command and Addr) 0x%08X\n",
		regs_buff[1050]);

	fprintf(stdout,
		"0x04250: APAE        (Auto-Scan PHY Address Enable)   0x%08X\n",
		regs_buff[1051]);

	fprintf(stdout,
		"0x04254: ARD         (Auto-Scan Read Data)            0x%08X\n",
		regs_buff[1052]);

	fprintf(stdout,
		"0x04258: AIS         (Auto-Scan Interrupt Status)     0x%08X\n",
		regs_buff[1053]);

	fprintf(stdout,
		"0x0425C: MSCA        (MDI Single Command and Addr)    0x%08X\n",
		regs_buff[1054]);

	fprintf(stdout,
		"0x04260: MSRWD       (MDI Single Read and Write Data) 0x%08X\n",
		regs_buff[1055]);

	fprintf(stdout,
		"0x04264: MLADD       (MAC Address Low)                0x%08X\n",
		regs_buff[1056]);

	fprintf(stdout,
		"0x04268: MHADD       (MAC Addr High/Max Frame size)   0x%08X\n",
		regs_buff[1057]);

	fprintf(stdout,
		"0x0426C: TREG        (Test Register)                  0x%08X\n",
		regs_buff[1058]);

	if (mac_type < ixgbe_mac_X540) {
		fprintf(stdout,
			"0x04288: PCSS1       (XGXS Status 1)                  0x%08X\n",
			regs_buff[1059]);

		fprintf(stdout,
			"0x0428C: PCSS2       (XGXS Status 2)                  0x%08X\n",
			regs_buff[1060]);

		fprintf(stdout,
			"0x04290: XPCSS       (10GBASE-X PCS Status)           0x%08X\n",
			regs_buff[1061]);

		fprintf(stdout,
			"0x04298: SERDESC     (SERDES Interface Control)       0x%08X\n",
			regs_buff[1062]);

		fprintf(stdout,
			"0x0429C: MACS        (FIFO Status/CNTL Report)        0x%08X\n",
			regs_buff[1063]);

		fprintf(stdout,
			"0x042A0: AUTOC       (Auto Negotiation Control)       0x%08X\n",
			regs_buff[1064]);

		fprintf(stdout,
			"0x042A8: AUTOC2      (Auto Negotiation Control 2)     0x%08X\n",
			regs_buff[1066]);

		fprintf(stdout,
			"0x042AC: AUTOC3      (Auto Negotiation Control 3)     0x%08X\n",
			regs_buff[1067]);

		fprintf(stdout,
			"0x042B0: ANLP1       (Auto Neg Lnk Part. Ctrl Word 1) 0x%08X\n",
			regs_buff[1068]);

		fprintf(stdout,
			"0x042B4: ANLP2       (Auto Neg Lnk Part. Ctrl Word 2) 0x%08X\n",
			regs_buff[1069]);
	}

	if (mac_type == ixgbe_mac_82598EB) {
		fprintf(stdout,
			"0x04800: ATLASCTL    (Atlas Analog Configuration)     0x%08X\n",
			regs_buff[1070]);

		/* Diagnostic */
		fprintf(stdout,
			"0x02C20: RDSTATCTL   (Rx DMA Statistic Control)       0x%08X\n",
			regs_buff[1071]);

		for (i = 0; i < 8; i++)
			fprintf(stdout,
				"0x%05X: RDSTAT%d     (Rx DMA Statistics %d)            0x%08X\n",
				0x02C00 + (4 * i), i, i, regs_buff[1072 + i]);

		fprintf(stdout,
			"0x02F08: RDHMPN      (Rx Desc Handler Mem Page num)   0x%08X\n",
			regs_buff[1080]);

		fprintf(stdout,
			"0x02F10: RIC_DW0     (Rx Desc Hand. Mem Read Data 0)  0x%08X\n",
			regs_buff[1081]);

		fprintf(stdout,
			"0x02F14: RIC_DW1     (Rx Desc Hand. Mem Read Data 1)  0x%08X\n",
			regs_buff[1082]);

		fprintf(stdout,
			"0x02F18: RIC_DW2     (Rx Desc Hand. Mem Read Data 2)  0x%08X\n",
			regs_buff[1083]);

		fprintf(stdout,
			"0x02F1C: RIC_DW3     (Rx Desc Hand. Mem Read Data 3)  0x%08X\n",
			regs_buff[1084]);
	}

	if (mac_type < ixgbe_mac_X540)
		fprintf(stdout,
			"0x02F20: RDPROBE     (Rx Probe Mode Status)           0x%08X\n",
			regs_buff[1085]);

	fprintf(stdout,
		"0x07C20: TDSTATCTL   (Tx DMA Statistic Control)       0x%08X\n",
		regs_buff[1086]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: TDSTAT%d     (Tx DMA Statistics %d)            0x%08X\n",
		0x07C00 + (4 * i), i, i, regs_buff[1087 + i]);

	fprintf(stdout,
		"0x07F08: TDHMPN      (Tx Desc Handler Mem Page Num)   0x%08X\n",
		regs_buff[1095]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
			"0x%05X: TIC_DW%d     (Tx Desc Hand. Mem Read Data %d)  0x%08X\n",
			0x07F10 + (4 * i), i, i, regs_buff[1096 + i]);

	fprintf(stdout,
		"0x07F20: TDPROBE     (Tx Probe Mode Status)           0x%08X\n",
		regs_buff[1100]);

	fprintf(stdout,
		"0x0C600: TXBUFCTRL   (TX Buffer Access Control)       0x%08X\n",
		regs_buff[1101]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
			"0x%05X: TXBUFDATA%d  (TX Buffer DATA %d)               0x%08X\n",
			0x0C610 + (4 * i), i, i, regs_buff[1102 + i]);

	fprintf(stdout,
		"0x03600: RXBUFCTRL   (RX Buffer Access Control)       0x%08X\n",
		regs_buff[1106]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
			"0x%05X: RXBUFDATA%d  (RX Buffer DATA %d)               0x%08X\n",
			0x03610 + (4 * i), i, i, regs_buff[1107 + i]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x%05X: PCIE_DIAG%d  (PCIe Diagnostic %d)              0x%08X\n",
		0x11090 + (4 * i), i, i, regs_buff[1111 + i]);

	fprintf(stdout,
		"0x050A4: RFVAL       (Receive Filter Validation)      0x%08X\n",
		regs_buff[1119]);

	if (mac_type < ixgbe_mac_X540) {
		fprintf(stdout,
			"0x042B8: MDFTC1      (MAC DFT Control 1)              0x%08X\n",
			regs_buff[1120]);

		fprintf(stdout,
			"0x042C0: MDFTC2      (MAC DFT Control 2)              0x%08X\n",
			regs_buff[1121]);

		fprintf(stdout,
			"0x042C4: MDFTFIFO1   (MAC DFT FIFO 1)                 0x%08X\n",
			regs_buff[1122]);

		fprintf(stdout,
			"0x042C8: MDFTFIFO2   (MAC DFT FIFO 2)                 0x%08X\n",
			regs_buff[1123]);

		fprintf(stdout,
			"0x042CC: MDFTS       (MAC DFT Status)                 0x%08X\n",
			regs_buff[1124]);
	}

	if (mac_type == ixgbe_mac_82598EB) {
		fprintf(stdout,
			"0x1106C: PCIEECCCTL  (PCIe ECC Control)               0x%08X\n",
			regs_buff[1125]);

		fprintf(stdout,
			"0x0C300: PBTXECC     (Packet Buffer Tx ECC)           0x%08X\n",
			regs_buff[1126]);

		fprintf(stdout,
			"0x03300: PBRXECC     (Packet Buffer Rx ECC)           0x%08X\n",
			regs_buff[1127]);
	}

	return 0;
}
