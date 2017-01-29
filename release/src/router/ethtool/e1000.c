/* Copyright (c) 2002 Intel Corporation */
#include <stdio.h>
#include "internal.h"

/* Register Bit Masks */
/* Device Control */
#define E1000_CTRL_FD       0x00000001  /* Full duplex.0=half; 1=full */
#define E1000_CTRL_BEM      0x00000002  /* Endian Mode.0=little,1=big */
#define E1000_CTRL_PRIOR    0x00000004  /* Priority on PCI. 0=rx,1=fair */
#define E1000_CTRL_LRST     0x00000008  /* Link reset. 0=normal,1=reset */
#define E1000_CTRL_TME      0x00000010  /* Test mode. 0=normal,1=test */
#define E1000_CTRL_SLE      0x00000020  /* Serial Link on 0=dis,1=en */
#define E1000_CTRL_ASDE     0x00000020  /* Auto-speed detect enable */
#define E1000_CTRL_SLU      0x00000040  /* Set link up (Force Link) */
#define E1000_CTRL_ILOS     0x00000080  /* Invert Loss-Of Signal */
#define E1000_CTRL_SPD_SEL  0x00000300  /* Speed Select Mask */
#define E1000_CTRL_SPD_10   0x00000000  /* Force 10Mb */
#define E1000_CTRL_SPD_100  0x00000100  /* Force 100Mb */
#define E1000_CTRL_SPD_1000 0x00000200  /* Force 1Gb */
#define E1000_CTRL_BEM32    0x00000400  /* Big Endian 32 mode */
#define E1000_CTRL_FRCSPD   0x00000800  /* Force Speed */
#define E1000_CTRL_FRCDPX   0x00001000  /* Force Duplex */
#define E1000_CTRL_SWDPIN0  0x00040000  /* SWDPIN 0 value */
#define E1000_CTRL_SWDPIN1  0x00080000  /* SWDPIN 1 value */
#define E1000_CTRL_SWDPIN2  0x00100000  /* SWDPIN 2 value */
#define E1000_CTRL_SWDPIN3  0x00200000  /* SWDPIN 3 value */
#define E1000_CTRL_SWDPIO0  0x00400000  /* SWDPIN 0 Input or output */
#define E1000_CTRL_SWDPIO1  0x00800000  /* SWDPIN 1 input or output */
#define E1000_CTRL_SWDPIO2  0x01000000  /* SWDPIN 2 input or output */
#define E1000_CTRL_SWDPIO3  0x02000000  /* SWDPIN 3 input or output */
#define E1000_CTRL_RST      0x04000000  /* Global reset */
#define E1000_CTRL_RFCE     0x08000000  /* Receive Flow Control enable */
#define E1000_CTRL_TFCE     0x10000000  /* Transmit flow control enable */
#define E1000_CTRL_RTE      0x20000000  /* Routing tag enable */
#define E1000_CTRL_VME      0x40000000  /* IEEE VLAN mode enable */
#define E1000_CTRL_PHY_RST  0x80000000  /* PHY Reset */

/* Device Status */
#define E1000_STATUS_FD         0x00000001      /* Full duplex.0=half,1=full */
#define E1000_STATUS_LU         0x00000002      /* Link up.0=no,1=link */
#define E1000_STATUS_FUNC_MASK  0x0000000C      /* PCI Function Mask */
#define E1000_STATUS_FUNC_0     0x00000000      /* Function 0 */
#define E1000_STATUS_FUNC_1     0x00000004      /* Function 1 */
#define E1000_STATUS_TXOFF      0x00000010      /* transmission paused */
#define E1000_STATUS_TBIMODE    0x00000020      /* TBI mode */
#define E1000_STATUS_SPEED_MASK 0x000000C0
#define E1000_STATUS_SPEED_10   0x00000000      /* Speed 10Mb/s */
#define E1000_STATUS_SPEED_100  0x00000040      /* Speed 100Mb/s */
#define E1000_STATUS_SPEED_1000 0x00000080      /* Speed 1000Mb/s */
#define E1000_STATUS_ASDV       0x00000300      /* Auto speed detect value */
#define E1000_STATUS_MTXCKOK    0x00000400      /* MTX clock running OK */
#define E1000_STATUS_PCI66      0x00000800      /* In 66Mhz slot */
#define E1000_STATUS_BUS64      0x00001000      /* In 64 bit slot */
#define E1000_STATUS_PCIX_MODE  0x00002000      /* PCI-X mode */
#define E1000_STATUS_PCIX_SPEED 0x0000C000      /* PCI-X bus speed */

/* Constants used to intrepret the masked PCI-X bus speed. */
#define E1000_STATUS_PCIX_SPEED_66  0x00000000 /* PCI-X bus speed  50-66 MHz */
#define E1000_STATUS_PCIX_SPEED_100 0x00004000 /* PCI-X bus speed  66-100 MHz */
#define E1000_STATUS_PCIX_SPEED_133 0x00008000 /* PCI-X bus speed 100-133 MHz */

/* Receive Control */
#define E1000_RCTL_RST          0x00000001      /* Software reset */
#define E1000_RCTL_EN           0x00000002      /* enable */
#define E1000_RCTL_SBP          0x00000004      /* store bad packet */
#define E1000_RCTL_UPE          0x00000008      /* unicast promiscuous enable */
#define E1000_RCTL_MPE          0x00000010      /* multicast promiscuous enab */
#define E1000_RCTL_LPE          0x00000020      /* long packet enable */
#define E1000_RCTL_LBM_NO       0x00000000      /* no loopback mode */
#define E1000_RCTL_LBM_MAC      0x00000040      /* MAC loopback mode */
#define E1000_RCTL_LBM_SLP      0x00000080      /* serial link loopback mode */
#define E1000_RCTL_LBM_TCVR     0x000000C0      /* tcvr loopback mode */
#define E1000_RCTL_RDMTS        0x00000300      /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_HALF   0x00000000      /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT   0x00000100      /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH  0x00000200      /* rx desc min threshold size */
#define E1000_RCTL_MO_SHIFT     12              /* multicast offset shift */
#define E1000_RCTL_MO_0         0x00000000      /* multicast offset 11:0 */
#define E1000_RCTL_MO_1         0x00001000      /* multicast offset 12:1 */
#define E1000_RCTL_MO_2         0x00002000      /* multicast offset 13:2 */
#define E1000_RCTL_MO_3         0x00003000      /* multicast offset 15:4 */
#define E1000_RCTL_MDR          0x00004000      /* multicast desc ring 0 */
#define E1000_RCTL_BAM          0x00008000      /* broadcast enable */
#define E1000_RCTL_SZ           0x00030000      /* rx buffer size */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048      0x00000000      /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024      0x00010000      /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512       0x00020000      /* rx buffer size 512 */
#define E1000_RCTL_SZ_256       0x00030000      /* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384     0x00010000      /* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192      0x00020000      /* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096      0x00030000      /* rx buffer size 4096 */
#define E1000_RCTL_VFE          0x00040000      /* vlan filter enable */
#define E1000_RCTL_CFIEN        0x00080000      /* canonical form enable */
#define E1000_RCTL_CFI          0x00100000      /* canonical form indicator */
#define E1000_RCTL_DPF          0x00400000      /* discard pause frames */
#define E1000_RCTL_PMCF         0x00800000      /* pass MAC control frames */
#define E1000_RCTL_BSEX         0x02000000      /* Buffer size extension */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */

/* M88E1000 PHY Specific Status Register */
#define M88_PSSR_JABBER             0x0001 /* 1=Jabber */
#define M88_PSSR_REV_POLARITY       0x0002 /* 1=Polarity reversed */
#define M88_PSSR_DOWNSHIFT          0x0020 /* 1=Downshifted */
#define M88_PSSR_MDIX               0x0040 /* 1=MDIX; 0=MDI */
#define M88_PSSR_CABLE_LENGTH       0x0380 /* 0=<50M;1=50-80M;2=80-110M;
                                            * 3=110-140M;4=>140M */
#define M88_PSSR_LINK               0x0400 /* 1=Link up, 0=Link down */
#define M88_PSSR_SPD_DPLX_RESOLVED  0x0800 /* 1=Speed & Duplex resolved */
#define M88_PSSR_PAGE_RCVD          0x1000 /* 1=Page received */
#define M88_PSSR_DPLX               0x2000 /* 1=Duplex 0=Half Duplex */
#define M88_PSSR_SPEED              0xC000 /* Speed, bits 14:15 */
#define M88_PSSR_10MBS              0x0000 /* 00=10Mbs */
#define M88_PSSR_100MBS             0x4000 /* 01=100Mbs */
#define M88_PSSR_1000MBS            0x8000 /* 10=1000Mbs */

#define M88_PSSR_CL_0_50     (0<<7)
#define M88_PSSR_CL_50_80    (1<<7)
#define M88_PSSR_CL_80_110   (2<<7)
#define M88_PSSR_CL_110_140  (3<<7)
#define M88_PSSR_CL_140_PLUS (4<<7)

/* M88E1000 PHY Specific Control Register */
#define M88_PSCR_JABBER_DISABLE    0x0001  /* 1=Jabber Function disabled */
#define M88_PSCR_POLARITY_REVERSAL 0x0002  /* 1=Polarity Reversal enabled */
#define M88_PSCR_SQE_TEST          0x0004  /* 1=SQE Test enabled */
#define M88_PSCR_CLK125_DISABLE    0x0010  /* 1=CLK125 low,
                                            * 0=CLK125 toggling
                                            */
#define M88_PSCR_MDI_MASK         0x0060
#define M88_PSCR_MDI_MANUAL_MODE  0x0000   /* MDI Crossover Mode bits 6:5 */
                                          /* Manual MDI configuration */
#define M88_PSCR_MDIX_MANUAL_MODE 0x0020   /* Manual MDIX configuration */
#define M88_PSCR_AUTO_X_1000T     0x0040   /* 1000BASE-T: Auto crossover,
                                            *  100BASE-TX/10BASE-T:
                                            *  MDI Mode
                                            */
#define M88_PSCR_AUTO_X_MODE      0x0060   /* Auto crossover enabled
                                            * all speeds.
                                            */
#define M88_PSCR_10BT_EXT_DIST_ENABLE 0x0080
                                   /* 1=Enable Extended 10BASE-T distance
                                    * (Lower 10BASE-T RX Threshold)
                                    * 0=Normal 10BASE-T RX Threshold */
#define M88_PSCR_MII_5BIT_ENABLE      0x0100
                                   /* 1=5-Bit interface in 100BASE-TX
                                    * 0=MII interface in 100BASE-TX */
#define M88_PSCR_SCRAMBLER_DISABLE    0x0200       /* 1=Scrambler disable */
#define M88_PSCR_FORCE_LINK_GOOD      0x0400       /* 1=Force link good */
#define M88_PSCR_ASSERT_CRS_ON_TX     0x0800       /* 1=Assert CRS on Transmit */

#define M88_PSCR_POLARITY_REVERSAL_SHIFT    1
#define M88_PSCR_AUTO_X_MODE_SHIFT          5
#define M88_PSCR_10BT_EXT_DIST_ENABLE_SHIFT 7

/* PCI Device IDs */
#define E1000_DEV_ID_82542                    0x1000
#define E1000_DEV_ID_82543GC_FIBER            0x1001
#define E1000_DEV_ID_82543GC_COPPER           0x1004
#define E1000_DEV_ID_82544EI_COPPER           0x1008
#define E1000_DEV_ID_82544EI_FIBER            0x1009
#define E1000_DEV_ID_82544GC_COPPER           0x100C
#define E1000_DEV_ID_82544GC_LOM              0x100D
#define E1000_DEV_ID_82540EM                  0x100E
#define E1000_DEV_ID_82540EM_LOM              0x1015
#define E1000_DEV_ID_82540EP_LOM              0x1016
#define E1000_DEV_ID_82540EP                  0x1017
#define E1000_DEV_ID_82540EP_LP               0x101E
#define E1000_DEV_ID_82545EM_COPPER           0x100F
#define E1000_DEV_ID_82545EM_FIBER            0x1011
#define E1000_DEV_ID_82545GM_COPPER           0x1026
#define E1000_DEV_ID_82545GM_FIBER            0x1027
#define E1000_DEV_ID_82545GM_SERDES           0x1028
#define E1000_DEV_ID_82546EB_COPPER           0x1010
#define E1000_DEV_ID_82546EB_FIBER            0x1012
#define E1000_DEV_ID_82546EB_QUAD_COPPER      0x101D
#define E1000_DEV_ID_82546GB_COPPER           0x1079
#define E1000_DEV_ID_82546GB_FIBER            0x107A
#define E1000_DEV_ID_82546GB_SERDES           0x107B
#define E1000_DEV_ID_82546GB_PCIE             0x108A
#define E1000_DEV_ID_82546GB_QUAD_COPPER      0x1099
#define E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3 0x10B5
#define E1000_DEV_ID_82541EI                  0x1013
#define E1000_DEV_ID_82541EI_MOBILE           0x1018
#define E1000_DEV_ID_82541ER_LOM              0x1014
#define E1000_DEV_ID_82541ER                  0x1078
#define E1000_DEV_ID_82541GI                  0x1076
#define E1000_DEV_ID_82541GI_LF               0x107C
#define E1000_DEV_ID_82541GI_MOBILE           0x1077
#define E1000_DEV_ID_82547EI                  0x1019
#define E1000_DEV_ID_82547EI_MOBILE           0x101A
#define E1000_DEV_ID_82547GI                  0x1075
#define E1000_DEV_ID_82571EB_COPPER           0x105E
#define E1000_DEV_ID_82571EB_FIBER            0x105F
#define E1000_DEV_ID_82571EB_SERDES           0x1060
#define E1000_DEV_ID_82571EB_QUAD_COPPER      0x10A4
#define E1000_DEV_ID_82571EB_QUAD_FIBER       0x10A5
#define E1000_DEV_ID_82571EB_QUAD_COPPER_LP   0x10BC
#define E1000_DEV_ID_82572EI_COPPER           0x107D
#define E1000_DEV_ID_82572EI_FIBER            0x107E
#define E1000_DEV_ID_82572EI_SERDES           0x107F
#define E1000_DEV_ID_82572EI                  0x10B9
#define E1000_DEV_ID_82573E                   0x108B
#define E1000_DEV_ID_82573E_IAMT              0x108C
#define E1000_DEV_ID_82573L                   0x109A
#define E1000_DEV_ID_80003ES2LAN_COPPER_DPT   0x1096
#define E1000_DEV_ID_80003ES2LAN_SERDES_DPT   0x1098
#define E1000_DEV_ID_80003ES2LAN_COPPER_SPT   0x10BA
#define E1000_DEV_ID_80003ES2LAN_SERDES_SPT   0x10BB
#define E1000_DEV_ID_ICH8_IGP_M_AMT           0x1049
#define E1000_DEV_ID_ICH8_IGP_AMT             0x104A
#define E1000_DEV_ID_ICH8_IGP_C               0x104B
#define E1000_DEV_ID_ICH8_IFE                 0x104C
#define E1000_DEV_ID_ICH8_IFE_GT              0x10C4
#define E1000_DEV_ID_ICH8_IFE_G               0x10C5
#define E1000_DEV_ID_ICH8_IGP_M               0x104D

#define E1000_82542_2_0_REV_ID 2
#define E1000_82542_2_1_REV_ID 3

/* Enumerated types specific to the e1000 hardware */
/* Media Access Controlers */
enum e1000_mac_type {
	e1000_undefined = 0,
	e1000_82542,
	e1000_82543,
	e1000_82544,
	e1000_82540,
	e1000_82545,
	e1000_82545_rev_3,
	e1000_82546,
	e1000_82546_rev_3,
	e1000_82541,
	e1000_82541_rev_2,
	e1000_82547,
	e1000_82547_rev_2,
	e1000_82571,
	e1000_82572,
	e1000_82573,
	e1000_80003es2lan,
	e1000_ich8lan,
	e1000_num_macs
};

static enum e1000_mac_type
e1000_get_mac_type(u16 device_id, u8 revision_id)
{
	enum e1000_mac_type mac_type = e1000_undefined;

	switch (device_id) {
	case E1000_DEV_ID_82542:
		mac_type = e1000_82542;
		break;
	case E1000_DEV_ID_82543GC_FIBER:
	case E1000_DEV_ID_82543GC_COPPER:
		mac_type = e1000_82543;
		break;
	case E1000_DEV_ID_82544EI_COPPER:
	case E1000_DEV_ID_82544EI_FIBER:
	case E1000_DEV_ID_82544GC_COPPER:
	case E1000_DEV_ID_82544GC_LOM:
		mac_type = e1000_82544;
		break;
	case E1000_DEV_ID_82540EM:
	case E1000_DEV_ID_82540EM_LOM:
	case E1000_DEV_ID_82540EP:
	case E1000_DEV_ID_82540EP_LOM:
	case E1000_DEV_ID_82540EP_LP:
		mac_type = e1000_82540;
		break;
	case E1000_DEV_ID_82545EM_COPPER:
	case E1000_DEV_ID_82545EM_FIBER:
		mac_type = e1000_82545;
		break;
	case E1000_DEV_ID_82545GM_COPPER:
	case E1000_DEV_ID_82545GM_FIBER:
	case E1000_DEV_ID_82545GM_SERDES:
		mac_type = e1000_82545_rev_3;
		break;
	case E1000_DEV_ID_82546EB_COPPER:
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546EB_QUAD_COPPER:
		mac_type = e1000_82546;
		break;
	case E1000_DEV_ID_82546GB_COPPER:
	case E1000_DEV_ID_82546GB_FIBER:
	case E1000_DEV_ID_82546GB_SERDES:
	case E1000_DEV_ID_82546GB_PCIE:
	case E1000_DEV_ID_82546GB_QUAD_COPPER:
	case E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3:
		mac_type = e1000_82546_rev_3;
		break;
	case E1000_DEV_ID_82541EI:
	case E1000_DEV_ID_82541EI_MOBILE:
	case E1000_DEV_ID_82541ER_LOM:
		mac_type = e1000_82541;
		break;
	case E1000_DEV_ID_82541ER:
	case E1000_DEV_ID_82541GI:
	case E1000_DEV_ID_82541GI_LF:
	case E1000_DEV_ID_82541GI_MOBILE:
		mac_type = e1000_82541_rev_2;
		break;
	case E1000_DEV_ID_82547EI:
	case E1000_DEV_ID_82547EI_MOBILE:
		mac_type = e1000_82547;
		break;
	case E1000_DEV_ID_82547GI:
		mac_type = e1000_82547_rev_2;
		break;
	case E1000_DEV_ID_82571EB_COPPER:
	case E1000_DEV_ID_82571EB_FIBER:
	case E1000_DEV_ID_82571EB_SERDES:
	case E1000_DEV_ID_82571EB_QUAD_COPPER:
	case E1000_DEV_ID_82571EB_QUAD_FIBER:
	case E1000_DEV_ID_82571EB_QUAD_COPPER_LP:
		mac_type = e1000_82571;
		break;
	case E1000_DEV_ID_82572EI:
	case E1000_DEV_ID_82572EI_COPPER:
	case E1000_DEV_ID_82572EI_FIBER:
	case E1000_DEV_ID_82572EI_SERDES:
		mac_type = e1000_82572;
		break;
	case E1000_DEV_ID_82573E:
	case E1000_DEV_ID_82573E_IAMT:
	case E1000_DEV_ID_82573L:
		mac_type = e1000_82573;
		break;
	case E1000_DEV_ID_80003ES2LAN_COPPER_DPT:
	case E1000_DEV_ID_80003ES2LAN_SERDES_DPT:
	case E1000_DEV_ID_80003ES2LAN_COPPER_SPT:
	case E1000_DEV_ID_80003ES2LAN_SERDES_SPT:
		mac_type = e1000_80003es2lan;
		break;
	case E1000_DEV_ID_ICH8_IFE:
	case E1000_DEV_ID_ICH8_IFE_GT:
	case E1000_DEV_ID_ICH8_IFE_G:
	case E1000_DEV_ID_ICH8_IGP_M:
	case E1000_DEV_ID_ICH8_IGP_M_AMT:
	case E1000_DEV_ID_ICH8_IGP_AMT:
	case E1000_DEV_ID_ICH8_IGP_C:
		mac_type = e1000_ich8lan;
		break;
	default:
		/* assume old nic and attempt so user can get limited
		 * functionality */
		mac_type = e1000_82543;
		break;
	}

	return mac_type;
}

int
e1000_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 *regs_buff = (u32 *)regs->data;
	u16 hw_device_id = (u16)regs->version;
	u8 hw_revision_id = (u8)(regs->version >> 16);
	u8 version = (u8)(regs->version >> 24);
	enum e1000_mac_type mac_type;
	u32 reg;

	if (version != 1)
		return -1;

	mac_type = e1000_get_mac_type(hw_device_id, hw_revision_id);

	if(mac_type == e1000_undefined)
		return -1;

	fprintf(stdout, "MAC Registers\n");
	fprintf(stdout, "-------------\n");

	/* Device control register */
	reg = regs_buff[0];
	fprintf(stdout,
		"0x00000: CTRL (Device control register)  0x%08X\n"
		"      Endian mode (buffers):             %s\n"
		"      Link reset:                        %s\n"
		"      Set link up:                       %s\n"
		"      Invert Loss-Of-Signal:             %s\n"
		"      Receive flow control:              %s\n"
		"      Transmit flow control:             %s\n"
		"      VLAN mode:                         %s\n",
		reg,
		reg & E1000_CTRL_BEM    ? "big"      : "little",
		reg & E1000_CTRL_LRST   ? "reset"    : "normal",
		reg & E1000_CTRL_SLU    ? "1"        : "0",
		reg & E1000_CTRL_ILOS   ? "yes"      : "no",
		reg & E1000_CTRL_RFCE   ? "enabled"  : "disabled",
		reg & E1000_CTRL_TFCE   ? "enabled"  : "disabled",
		reg & E1000_CTRL_VME    ? "enabled"  : "disabled");
	if(mac_type >= e1000_82543) {
	fprintf(stdout,
		"      Auto speed detect:                 %s\n"
		"      Speed select:                      %s\n"
		"      Force speed:                       %s\n"
		"      Force duplex:                      %s\n",
		reg & E1000_CTRL_ASDE   ? "enabled"  : "disabled",
		(reg & E1000_CTRL_SPD_SEL) == E1000_CTRL_SPD_10   ? "10Mb/s"   :
		(reg & E1000_CTRL_SPD_SEL) == E1000_CTRL_SPD_100  ? "100Mb/s"  :
		(reg & E1000_CTRL_SPD_SEL) == E1000_CTRL_SPD_1000 ? "1000Mb/s" :
		"not used",
		reg & E1000_CTRL_FRCSPD ? "yes"      : "no",
		reg & E1000_CTRL_FRCDPX ? "yes"      : "no");
	}

	/* Device status register */
	reg = regs_buff[1];
	fprintf(stdout,
		"0x00008: STATUS (Device status register) 0x%08X\n"
		"      Duplex:                            %s\n"
		"      Link up:                           %s\n",
		reg,
		reg & E1000_STATUS_FD      ? "full"        : "half",
		reg & E1000_STATUS_LU      ? "link config" : "no link config");
	if (mac_type >= e1000_82571) {
	fprintf(stdout,
		"      TBI mode:                          %s\n"
		"      Link speed:                        %s\n"
		"      Bus type:                          %s\n"
		"      Port number:                       %s\n",
		reg & E1000_STATUS_TBIMODE ? "enabled"     : "disabled",
		(reg & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_10   ?
		"10Mb/s" :
		(reg & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_100  ?
		"100Mb/s" :
		(reg & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_1000 ?
		"1000Mb/s" : "not used",
		"PCI Express",
		(reg & E1000_STATUS_FUNC_MASK) == 0 ? "0" : "1");
	}
	else if (mac_type >= e1000_82543) {
	fprintf(stdout,
		"      TBI mode:                          %s\n"
		"      Link speed:                        %s\n"
		"      Bus type:                          %s\n"
		"      Bus speed:                         %s\n"
		"      Bus width:                         %s\n",
		reg & E1000_STATUS_TBIMODE ? "enabled"     : "disabled",
		(reg & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_10   ?
		"10Mb/s" :
		(reg & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_100  ?
		"100Mb/s" :
		(reg & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_1000 ?
		"1000Mb/s" : "not used",
		(reg & E1000_STATUS_PCIX_MODE) ? "PCI-X" : "PCI",
		(reg & E1000_STATUS_PCIX_MODE) ?
			((reg & E1000_STATUS_PCIX_SPEED_133) ? "133MHz" :
			(reg & E1000_STATUS_PCIX_SPEED_100) ? "100MHz" :
			"66MHz")	       :
			((reg & E1000_STATUS_PCI66) ? "66MHz" : "33MHz"),
		(reg & E1000_STATUS_BUS64) ? "64-bit" : "32-bit");
	}

	/* Receive control register */
	reg = regs_buff[2];
	fprintf(stdout,
		"0x00100: RCTL (Receive control register) 0x%08X\n"
		"      Receiver:                          %s\n"
		"      Store bad packets:                 %s\n"
		"      Unicast promiscuous:               %s\n"
		"      Multicast promiscuous:             %s\n"
		"      Long packet:                       %s\n"
		"      Descriptor minimum threshold size: %s\n"
		"      Broadcast accept mode:             %s\n"
		"      VLAN filter:                       %s\n"
		"      Canonical form indicator:          %s\n"
		"      Discard pause frames:              %s\n"
		"      Pass MAC control frames:           %s\n",
		reg,
		reg & E1000_RCTL_EN      ? "enabled"  : "disabled",
		reg & E1000_RCTL_SBP     ? "enabled"  : "disabled",
		reg & E1000_RCTL_UPE     ? "enabled"  : "disabled",
		reg & E1000_RCTL_MPE     ? "enabled"  : "disabled",
		reg & E1000_RCTL_LPE     ? "enabled"  : "disabled",
		(reg & E1000_RCTL_RDMTS) == E1000_RCTL_RDMTS_HALF  ? "1/2" :
		(reg & E1000_RCTL_RDMTS) == E1000_RCTL_RDMTS_QUAT  ? "1/4" :
		(reg & E1000_RCTL_RDMTS) == E1000_RCTL_RDMTS_EIGTH ? "1/8" :
		"reserved",
		reg & E1000_RCTL_BAM     ? "accept"   : "ignore",
		reg & E1000_RCTL_VFE     ? "enabled"  : "disabled",
		reg & E1000_RCTL_CFIEN   ? "enabled"  : "disabled",
		reg & E1000_RCTL_DPF     ? "ignored"  : "filtered",
		reg & E1000_RCTL_PMCF    ? "pass"     : "don't pass");
	if(mac_type >= e1000_82543) {
	fprintf(stdout,
		"      Receive buffer size:               %s\n",
		reg & E1000_RCTL_BSEX    ?
			((reg & E1000_RCTL_SZ)==E1000_RCTL_SZ_16384 ? "16384" :
			 (reg & E1000_RCTL_SZ)==E1000_RCTL_SZ_8192  ? "8192"  :
			 (reg & E1000_RCTL_SZ)==E1000_RCTL_SZ_4096  ? "4096"  :
			 "reserved")     :
			((reg & E1000_RCTL_SZ)==E1000_RCTL_SZ_2048  ? "2048"  :
			 (reg & E1000_RCTL_SZ)==E1000_RCTL_SZ_1024  ? "1024"  :
			 (reg & E1000_RCTL_SZ)==E1000_RCTL_SZ_512   ? "512"   :
			 "256"));
	} else {
	fprintf(stdout,
		"      Receive buffer size:               %s\n",
		(reg & E1000_RCTL_SZ) == E1000_RCTL_SZ_2048  ? "2048"  :
		(reg & E1000_RCTL_SZ) == E1000_RCTL_SZ_1024  ? "1024"  :
		(reg & E1000_RCTL_SZ) == E1000_RCTL_SZ_512   ? "512"   :
		"256");
	}

	/* Receive descriptor registers */
	fprintf(stdout,
		"0x02808: RDLEN (Receive desc length)     0x%08X\n",
		regs_buff[3]);
	fprintf(stdout,
		"0x02810: RDH   (Receive desc head)       0x%08X\n",
		regs_buff[4]);
	fprintf(stdout,
		"0x02818: RDT   (Receive desc tail)       0x%08X\n",
		regs_buff[5]);
	fprintf(stdout,
		"0x02820: RDTR  (Receive delay timer)     0x%08X\n",
		regs_buff[6]);

	/* Transmit control register */
	reg = regs_buff[7];
	fprintf(stdout,
		"0x00400: TCTL (Transmit ctrl register)   0x%08X\n"
		"      Transmitter:                       %s\n"
		"      Pad short packets:                 %s\n"
		"      Software XOFF Transmission:        %s\n",
		reg,
		reg & E1000_TCTL_EN      ? "enabled"  : "disabled",
		reg & E1000_TCTL_PSP     ? "enabled"  : "disabled",
		reg & E1000_TCTL_SWXOFF  ? "enabled"  : "disabled");
	if(mac_type >= e1000_82543) {
	fprintf(stdout,
		"      Re-transmit on late collision:     %s\n",
		reg & E1000_TCTL_RTLC    ? "enabled"  : "disabled");
	}

	/* Transmit descriptor registers */
	fprintf(stdout,
		"0x03808: TDLEN (Transmit desc length)    0x%08X\n",
		regs_buff[8]);
	fprintf(stdout,
		"0x03810: TDH   (Transmit desc head)      0x%08X\n",
		regs_buff[9]);
	fprintf(stdout,
		"0x03818: TDT   (Transmit desc tail)      0x%08X\n",
		regs_buff[10]);
	fprintf(stdout,
		"0x03820: TIDV  (Transmit delay timer)    0x%08X\n",
		regs_buff[11]);

	/* PHY type */
	fprintf(stdout,
		"PHY type:                                %s\n",
		regs_buff[12] == 0 ? "M88" :
		regs_buff[12] == 1 ? "IGP" :
		regs_buff[12] == 2 ? "IGP2" : "unknown" );

	if (0 == regs_buff[12]) {
		reg = regs_buff[13];
		fprintf(stdout,
			"M88 PHY STATUS REGISTER:                 0x%08X\n"
			"      Jabber:                            %s\n"
			"      Polarity:                          %s\n"
			"      Downshifted:                       %s\n"
			"      MDI/MDIX:                          %s\n"
			"      Cable Length Estimate:             %s meters\n"
			"      Link State:                        %s\n"
			"      Speed & Duplex Resolved:           %s\n"
			"      Page Received:                     %s\n"
			"      Duplex:                            %s\n"
			"      Speed:                             %s mbps\n",
			reg,
			reg & M88_PSSR_JABBER       ? "yes"     : "no",
			reg & M88_PSSR_REV_POLARITY ? "reverse" : "normal",
			reg & M88_PSSR_DOWNSHIFT    ? "yes"     : "no",
			reg & M88_PSSR_MDIX         ? "MDIX"    : "MDI",
			((reg & M88_PSSR_CABLE_LENGTH)==M88_PSSR_CL_0_50 ? "0-50"
				: (reg & M88_PSSR_CABLE_LENGTH)==M88_PSSR_CL_50_80 ? "50-80"
				: (reg & M88_PSSR_CABLE_LENGTH)==M88_PSSR_CL_80_110 ? "80-110"
				: (reg & M88_PSSR_CABLE_LENGTH)==M88_PSSR_CL_110_140? "110-140"
				: (reg & M88_PSSR_CABLE_LENGTH)==M88_PSSR_CL_140_PLUS ? "140+"
				: "unknown"),
			reg & M88_PSSR_LINK              ? "Up"      : "Down",
			reg & M88_PSSR_SPD_DPLX_RESOLVED ? "Yes"     : "No",
			reg & M88_PSSR_PAGE_RCVD         ? "Yes"     : "No",
			reg & M88_PSSR_DPLX              ? "Full"    : "Half",
			((reg & M88_PSSR_SPEED)==M88_PSSR_10MBS        ? "10"
				: (reg & M88_PSSR_SPEED)==M88_PSSR_100MBS  ? "100"
				: (reg & M88_PSSR_SPEED)==M88_PSSR_1000MBS ? "1000"
				: "unknown")
		);

		reg = regs_buff[17];
		fprintf(stdout,
			"M88 PHY CONTROL REGISTER:                0x%08X\n"
			"      Jabber function:                   %s\n"
			"      Auto-polarity:                     %s\n"
			"      SQE Test:                          %s\n"
			"      CLK125:                            %s\n"
			"      Auto-MDIX:                         %s\n"
			"      Extended 10Base-T Distance:        %s\n"
			"      100Base-TX Interface:              %s\n"
			"      Scrambler:                         %s\n"
			"      Force Link Good:                   %s\n"
			"      Assert CRS on Transmit:            %s\n",
			reg,
			reg & M88_PSCR_JABBER_DISABLE    ? "disabled" : "enabled",
			reg & M88_PSCR_POLARITY_REVERSAL ? "enabled"  : "disabled",
			reg & M88_PSCR_SQE_TEST          ? "enabled"  : "disabled",
			reg & M88_PSCR_CLK125_DISABLE    ? "disabled" : "enabled",
			((reg & M88_PSCR_MDI_MASK)==M88_PSCR_MDI_MANUAL_MODE ? "force MDI"
				: (reg & M88_PSCR_MDI_MASK)==M88_PSCR_MDIX_MANUAL_MODE ? "force MDIX"
				: (reg & M88_PSCR_MDI_MASK)==M88_PSCR_AUTO_X_1000T ? "1000 auto, 10/100 MDI"
				: (reg & M88_PSCR_MDI_MASK)==M88_PSCR_AUTO_X_MODE ? "auto"
				: "wtf"),
			reg & M88_PSCR_10BT_EXT_DIST_ENABLE ? "enabled" : "disabled",
			reg & M88_PSCR_MII_5BIT_ENABLE ? "5-bit" : "MII",
			reg & M88_PSCR_SCRAMBLER_DISABLE ? "disabled" : "enabled",
			reg & M88_PSCR_FORCE_LINK_GOOD ? "forced" : "disabled",
			reg & M88_PSCR_ASSERT_CRS_ON_TX ? "enabled" : "disabled"
		);
	}

	return 0;
}

