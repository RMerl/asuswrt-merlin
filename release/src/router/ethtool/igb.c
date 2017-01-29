/* Copyright (c) 2007 Intel Corporation */
#include <stdio.h>
#include "internal.h"

/* Register Bit Masks */
/* Device Control */
#define E1000_CTRL_FD         0x00000001  /* Full duplex.0=half; 1=full */
#define E1000_CTRL_PRIOR      0x00000004  /* Priority on PCI. 0=rx,1=fair */
#define E1000_CTRL_GIOMASTERD 0x00000008  /* GIO Master Disable*/
#define E1000_CTRL_TME        0x00000010  /* Test mode. 0=normal,1=test */
#define E1000_CTRL_SLE        0x00000020  /* Serial Link on 0=dis,1=en */
#define E1000_CTRL_ASDE       0x00000020  /* Auto-speed detect enable */
#define E1000_CTRL_SLU        0x00000040  /* Set link up (Force Link) */
#define E1000_CTRL_ILOS       0x00000080  /* Invert Loss-Of Signal */
#define E1000_CTRL_SPD_SEL    0x00000300  /* Speed Select Mask */
#define E1000_CTRL_SPD_10     0x00000000  /* Force 10Mb */
#define E1000_CTRL_SPD_100    0x00000100  /* Force 100Mb */
#define E1000_CTRL_SPD_1000   0x00000200  /* Force 1Gb */
#define E1000_CTRL_FRCSPD     0x00000800  /* Force Speed */
#define E1000_CTRL_FRCDPX     0x00001000  /* Force Duplex */
#define E1000_CTRL_SDP0_GPIEN 0x00010000  /* General Purpose Interrupt Detection Enable for SDP0 */
#define E1000_CTRL_SDP1_GPIEN 0x00020000  /* General Purpose Interrupt Detection Enable for SDP1 */
#define E1000_CTRL_SDP0_DATA  0x00040000  /* SDP0 Data */
#define E1000_CTRL_SDP1_DATA  0x00080000  /* SDP1 Data */
#define E1000_CTRL_ADVD3WUC   0x00100000  /* D3Cold WakeUp Capability Advertisement Enable */
#define E1000_CTRL_SDP0_WDE   0x00200000  /* Watchdog Indication for SDP0 */
#define E1000_CTRL_SDP1_IODIR 0x00400000  /* SDP1 Directionality */
#define E1000_CTRL_RST        0x04000000  /* Global reset */
#define E1000_CTRL_RFCE       0x08000000  /* Receive Flow Control enable */
#define E1000_CTRL_TFCE       0x10000000  /* Transmit flow control enable */
#define E1000_CTRL_VME        0x40000000  /* IEEE VLAN mode enable */
#define E1000_CTRL_PHY_RST    0x80000000  /* PHY Reset */

/* Device Status */
#define E1000_STATUS_FD          0x00000001      /* Full duplex.0=half,1=full */
#define E1000_STATUS_LU          0x00000002      /* Link up.0=no,1=link */
#define E1000_STATUS_LANID       0x00000008      /* LAN ID */
#define E1000_STATUS_TXOFF       0x00000010      /* transmission paused */
#define E1000_STATUS_TBIMODE     0x00000020      /* TBI mode */
#define E1000_STATUS_SPEED_MASK  0x000000C0      /* Speed Mask */
#define E1000_STATUS_SPEED_10    0x00000000      /* Speed 10Mb/s */
#define E1000_STATUS_SPEED_100   0x00000040      /* Speed 100Mb/s */
#define E1000_STATUS_SPEED_1000  0x00000080      /* Speed 1000Mb/s */
#define E1000_STATUS_ASDV        0x00000300      /* Auto speed detect value */
#define E1000_STATUS_PHYRA       0x00000400      /* PHY Reset Asserted */
#define E1000_STATUS_GIOMASTEREN 0x00080000      /* GIO Master Enable Status */
#define E1000_STATUS_DMA_CGEN    0x80000000      /* DMA clock gating Enable */

/* Receive Control */
#define E1000_RCTL_EN           0x00000002      /* enable */
#define E1000_RCTL_SBP          0x00000004      /* store bad packet */
#define E1000_RCTL_UPE          0x00000008      /* unicast promiscuous enable */
#define E1000_RCTL_MPE          0x00000010      /* multicast promiscuous enab */
#define E1000_RCTL_LPE          0x00000020      /* long packet enable */
#define E1000_RCTL_LBM_MASK     0x000000C0      /* Loopback mode mask */
#define E1000_RCTL_LBM_NORM     0x00000000      /* normal loopback mode */
#define E1000_RCTL_LBM_MAC      0x00000040      /* MAC loopback mode */
#define E1000_RCTL_LBM_SERDES   0x000000C0      /* SERDES loopback mode */
#define E1000_RCTL_RDMTS        0x00000300      /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_HALF   0x00000000      /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT   0x00000100      /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH  0x00000200      /* rx desc min threshold size */
#define E1000_RCTL_MO           0x00003000      /* multicast offset shift */
#define E1000_RCTL_MO_0         0x00000000      /* multicast offset 47:36 */
#define E1000_RCTL_MO_1         0x00001000      /* multicast offset 46:35 */
#define E1000_RCTL_MO_2         0x00002000      /* multicast offset 45:34 */
#define E1000_RCTL_MO_3         0x00003000      /* multicast offset 43:32 */
#define E1000_RCTL_BAM          0x00008000      /* broadcast enable */
#define E1000_RCTL_BSIZE        0x00030000      /* rx buffer size */
#define E1000_RCTL_BSIZE_2048   0x00000000      /* rx buffer size 2048 */
#define E1000_RCTL_BSIZE_1024   0x00010000      /* rx buffer size 1024 */
#define E1000_RCTL_BSIZE_512    0x00020000      /* rx buffer size 512 */
#define E1000_RCTL_BSIZE_256    0x00030000      /* rx buffer size 256 */
#define E1000_RCTL_VFE          0x00040000      /* vlan filter enable */
#define E1000_RCTL_CFIEN        0x00080000      /* canonical form enable */
#define E1000_RCTL_CFI          0x00100000      /* canonical form indicator */
#define E1000_RCTL_DPF          0x00400000      /* discard pause frames */
#define E1000_RCTL_PMCF         0x00800000      /* pass MAC control frames */
#define E1000_RCTL_SECRC        0x04000000      /* Strip Ethernet CRC from packet.0=No strip;1=strip */

/* Transmit Control */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_BST    0x003ff000    /* Backoff Slot time */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */

int
igb_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 *regs_buff = (u32 *)regs->data;
	u32 reg;
	u8 i;
	u8 version = (u8)(regs->version >> 24);

	if (version != 1)
		return -1;

	/* Device control register */
	reg = regs_buff[0];
	fprintf(stdout,
		"0x00000: CTRL (Device control register)               0x%08X\n"
		"       Invert Loss-Of-Signal:                         %s\n"
		"       Receive flow control:                          %s\n"
		"       Transmit flow control:                         %s\n"
		"       VLAN mode:                                     %s\n"
		"       Set link up:                                   %s\n"
		"       D3COLD WakeUp capability advertisement:        %s\n",
		reg,
		reg & E1000_CTRL_ILOS     ? "yes"      : "no",
		reg & E1000_CTRL_RFCE     ? "enabled"  : "disabled",
		reg & E1000_CTRL_TFCE     ? "enabled"  : "disabled",
		reg & E1000_CTRL_VME      ? "enabled"  : "disabled",
		reg & E1000_CTRL_SLU      ? "1"        : "0",
		reg & E1000_CTRL_ADVD3WUC ? "enabled"  : "disabled");
	fprintf(stdout,
		"       Auto speed detect:                             %s\n"
		"       Speed select:                                  %s\n"
		"       Force speed:                                   %s\n"
		"       Force duplex:                                  %s\n",
		reg & E1000_CTRL_ASDE   ? "enabled"  : "disabled",
		(reg & E1000_CTRL_SPD_SEL) == E1000_CTRL_SPD_10   ? "10Mb/s"   :
		(reg & E1000_CTRL_SPD_SEL) == E1000_CTRL_SPD_100  ? "100Mb/s"  :
		(reg & E1000_CTRL_SPD_SEL) == E1000_CTRL_SPD_1000 ? "1000Mb/s" :
		"not used",
		reg & E1000_CTRL_FRCSPD ? "yes"      : "no",
		reg & E1000_CTRL_FRCDPX ? "yes"      : "no");

	/* Device status register */
	reg = regs_buff[1];
	fprintf(stdout,
		"0x00008: STATUS (Device status register)              0x%08X\n"
		"       Duplex:                                        %s\n"
		"       Link up:                                       %s\n"
		"       Transmission:                                  %s\n"
		"       DMA clock gating:                              %s\n",
		reg,
		reg & E1000_STATUS_FD       ? "full"        : "half",
		reg & E1000_STATUS_LU       ? "link config" : "no link config",
		reg & E1000_STATUS_TXOFF    ? "paused"      : "on",
		reg & E1000_STATUS_DMA_CGEN ? "enabled"     : "disabled");
	fprintf(stdout,
		"       TBI mode:                                      %s\n"
		"       Link speed:                                    %s\n"
		"       Bus type:                                      %s\n",
		reg & E1000_STATUS_TBIMODE ? "enabled"     : "disabled",
		(reg & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_10   ?
		"10Mb/s" :
		(reg & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_100  ?
		"100Mb/s" :
		(reg & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_1000 ?
		"1000Mb/s" : "not used",
		"PCI Express");

	/* Receive control register */
	reg = regs_buff[32];
	fprintf(stdout,
		"0x00100: RCTL (Receive control register)              0x%08X\n"
		"       Receiver:                                      %s\n"
		"       Store bad packets:                             %s\n"
		"       Unicast promiscuous:                           %s\n"
		"       Multicast promiscuous:                         %s\n"
		"       Long packet:                                   %s\n"
		"       Descriptor minimum threshold size:             %s\n"
		"       Broadcast accept mode:                         %s\n"
		"       VLAN filter:                                   %s\n"
		"       Cononical form indicator:                      %s\n"
		"       Discard pause frames:                          %s\n"
		"       Pass MAC control frames:                       %s\n"
		"       Loopback mode:                                 %s\n",
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
		reg & E1000_RCTL_BAM      ? "accept"   : "ignore",
		reg & E1000_RCTL_VFE      ? "enabled"  : "disabled",
		reg & E1000_RCTL_CFIEN    ? "enabled"  : "disabled",
		reg & E1000_RCTL_DPF      ? "ignored"  : "filtered",
		reg & E1000_RCTL_PMCF     ? "pass"     : "don't pass",
		(reg & E1000_RCTL_LBM_MASK) == E1000_RCTL_LBM_NORM   ? "normal" :
		(reg & E1000_RCTL_LBM_MASK) == E1000_RCTL_LBM_MAC    ? "MAC":
		(reg & E1000_RCTL_LBM_MASK) == E1000_RCTL_LBM_SERDES ? "SERDES":
		"undefined");
	fprintf(stdout,
		"       Receive buffer size:                           %s\n",
		(reg & E1000_RCTL_BSIZE)==E1000_RCTL_BSIZE_2048  ? "2048"  :
		(reg & E1000_RCTL_BSIZE)==E1000_RCTL_BSIZE_1024  ? "1024"  :
		(reg & E1000_RCTL_BSIZE)==E1000_RCTL_BSIZE_512   ? "512"   :
		"256");

	/* Receive descriptor registers */
	fprintf(stdout,
		"0x02808: RDLEN  (Receive desc length)                 0x%08X\n",
		regs_buff[137]);
	fprintf(stdout,
		"0x02810: RDH    (Receive desc head)                   0x%08X\n",
		regs_buff[141]);
	fprintf(stdout,
		"0x02818: RDT    (Receive desc tail)                   0x%08X\n",
		regs_buff[145]);

	/* Transmit control register */
	reg = regs_buff[38];
	fprintf(stdout,
		"0x00400: TCTL (Transmit ctrl register)                0x%08X\n"
		"       Transmitter:                                   %s\n"
		"       Pad short packets:                             %s\n"
		"       Software XOFF Transmission:                    %s\n",
		reg,
		reg & E1000_TCTL_EN      ? "enabled"  : "disabled",
		reg & E1000_TCTL_PSP     ? "enabled"  : "disabled",
		reg & E1000_TCTL_SWXOFF  ? "enabled"  : "disabled");
	fprintf(stdout,
		"       Re-transmit on late collision:                 %s\n",
		reg & E1000_TCTL_RTLC    ? "enabled"  : "disabled");

	/* Transmit descriptor registers */
	fprintf(stdout,
		"0x03808: TDLEN       (Transmit desc length)           0x%08X\n",
		regs_buff[219]);
	fprintf(stdout,
		"0x03810: TDH         (Transmit desc head)             0x%08X\n",
		regs_buff[223]);
	fprintf(stdout,
		"0x03818: TDT         (Transmit desc tail)             0x%08X\n",
		regs_buff[227]);


	fprintf(stdout,
		"0x00018: CTRL_EXT    (Extended device control)        0x%08X\n",
		regs_buff[2]);

	fprintf(stdout,
		"0x00018: MDIC        (MDI control)                    0x%08X\n",
		regs_buff[3]);

	fprintf(stdout,
		"0x00024: SCTL        (SERDES ANA)                     0x%08X\n",
		regs_buff[4]);

	fprintf(stdout,
		"0x00034: CONNSW      (Copper/Fiber switch control)    0x%08X\n",
		regs_buff[5]);

	fprintf(stdout,
		"0x00038: VET         (VLAN Ether type)                0x%08X\n",
		regs_buff[6]);

	fprintf(stdout,
		"0x00E00: LEDCTL      (LED control)                    0x%08X\n",
		regs_buff[7]);

	fprintf(stdout,
		"0x01000: PBA         (Packet buffer allocation)       0x%08X\n",
		regs_buff[8]);

	fprintf(stdout,
		"0x01008: PBS         (Packet buffer size)             0x%08X\n",
		regs_buff[9]);

	fprintf(stdout,
		"0x01048: FRTIMER     (Free running timer)             0x%08X\n",
		regs_buff[10]);

	fprintf(stdout,
		"0x0104C: TCPTIMER    (TCP timer)                      0x%08X\n",
		regs_buff[11]);

	fprintf(stdout,
		"0x00010: EEC         (EEPROM/FLASH control)           0x%08X\n",
		regs_buff[12]);

	fprintf(stdout,
		"0x01580: EICR        (Extended interrupt cause)       0x%08X\n",
		regs_buff[13]);

	fprintf(stdout,
		"0x01520: EICS        (Extended interrupt cause set)   0x%08X\n",
		regs_buff[14]);

	fprintf(stdout,
		"0x01524: EIMS        (Extended interrup set/read)     0x%08X\n",
		regs_buff[15]);

	fprintf(stdout,
		"0x01528: EIMC        (Extended interrupt mask clear)  0x%08X\n",
		regs_buff[16]);

	fprintf(stdout,
		"0x0152C: EIAC        (Extended interrupt auto clear)  0x%08X\n",
		regs_buff[17]);

	fprintf(stdout,
		"0x01530: EIAM        (Extended interrupt auto mask)   0x%08X\n",
		regs_buff[18]);

	fprintf(stdout,
		"0x01500: ICR         (Interrupt cause read)           0x%08X\n",
		regs_buff[19]);

	fprintf(stdout,
		"0x01504: ICS         (Interrupt cause set)            0x%08X\n",
		regs_buff[20]);

	fprintf(stdout,
		"0x01508: IMS         (Interrupt mask set/read)        0x%08X\n",
		regs_buff[21]);

	fprintf(stdout,
		"0x0150C: IMC         (Interrupt mask clear)           0x%08X\n",
		regs_buff[22]);

	fprintf(stdout,
		"0x04100: IAC         (Interrupt assertion count)      0x%08X\n",
		regs_buff[23]);

	fprintf(stdout,
		"0x01510: IAM         (Interr acknowledge auto-mask)   0x%08X\n",
		regs_buff[24]);

	fprintf(stdout,
		"0x05AC0: IMIRVP      (Immed interr rx VLAN priority)  0x%08X\n",
		regs_buff[25]);

	fprintf(stdout,
		"0x00028: FCAL        (Flow control address low)       0x%08X\n",
		regs_buff[26]);

	fprintf(stdout,
		"0x0002C: FCAH        (Flow control address high)      0x%08X\n",
		regs_buff[27]);

	fprintf(stdout,
		"0x00170: FCTTV       (Flow control tx timer value)    0x%08X\n",
		regs_buff[28]);

	fprintf(stdout,
		"0x02160: FCRTL       (Flow control rx threshold low)  0x%08X\n",
		regs_buff[29]);

	fprintf(stdout,
		"0x02168: FCRTH       (Flow control rx threshold high) 0x%08X\n",
		regs_buff[30]);

	fprintf(stdout,
		"0x02460: FCRTV       (Flow control refresh threshold) 0x%08X\n",
		regs_buff[31]);

	fprintf(stdout,
		"0x05000: RXCSUM      (Receive checksum control)       0x%08X\n",
		regs_buff[33]);

	fprintf(stdout,
		"0x05004: RLPML       (Receive long packet max length) 0x%08X\n",
		regs_buff[34]);

	fprintf(stdout,
		"0x05008: RFCTL       (Receive filter control)         0x%08X\n",
		regs_buff[35]);

	fprintf(stdout,
		"0x05818: MRQC        (Multiple rx queues command)     0x%08X\n",
		regs_buff[36]);

	fprintf(stdout,
		"0x0581C: VMD_CTL     (VMDq control)                   0x%08X\n",
		regs_buff[37]);

	fprintf(stdout,
		"0x00404: TCTL_EXT    (Transmit control extended)      0x%08X\n",
		regs_buff[39]);

	fprintf(stdout,
		"0x00410: TIPG        (Transmit IPG)                   0x%08X\n",
		regs_buff[40]);

	fprintf(stdout,
		"0x03590: DTXCTL      (DMA tx control)                 0x%08X\n",
		regs_buff[41]);

	fprintf(stdout,
		"0x05800: WUC         (Wake up control)                0x%08X\n",
		regs_buff[42]);

	fprintf(stdout,
		"0x05808: WUFC        (Wake up filter control)         0x%08X\n",
		regs_buff[43]);

	fprintf(stdout,
		"0x05810: WUS         (Wake up status)                 0x%08X\n",
		regs_buff[44]);

	fprintf(stdout,
		"0x05838: IPAV        (IP address valid)               0x%08X\n",
		regs_buff[45]);

	fprintf(stdout,
		"0x05900: WUPL        (Wake up packet length)          0x%08X\n",
		regs_buff[46]);

	fprintf(stdout,
		"0x04200: PCS_CFG     (PCS configuration 0)            0x%08X\n",
		regs_buff[47]);

	fprintf(stdout,
		"0x04208: PCS_LCTL    (PCS link control)               0x%08X\n",
		regs_buff[48]);

	fprintf(stdout,
		"0x0420C: PCS_LSTS    (PCS link status)                0x%08X\n",
		regs_buff[49]);

	fprintf(stdout,
		"0x04218: PCS_ANADV   (AN advertisement)               0x%08X\n",
		regs_buff[50]);

	fprintf(stdout,
		"0x0421C: PCS_LPAB    (Link partner ability)           0x%08X\n",
		regs_buff[51]);

	fprintf(stdout,
		"0x04220: PCS_NPTX    (Next Page transmit)             0x%08X\n",
		regs_buff[52]);

	fprintf(stdout,
		"0x04224: PCS_LPABNP  (Link partner ability Next Page) 0x%08X\n",
		regs_buff[53]);

	fprintf(stdout,
		"0x04000: CRCERRS     (CRC error count)                0x%08X\n",
		regs_buff[54]);

	fprintf(stdout,
		"0x04004: ALGNERRC    (Alignment error count)          0x%08X\n",
		regs_buff[55]);

	fprintf(stdout,
		"0x04008: SYMERRS     (Symbol error count)             0x%08X\n",
		regs_buff[56]);

	fprintf(stdout,
		"0x0400C: RXERRC      (RX error count)                 0x%08X\n",
		regs_buff[57]);

	fprintf(stdout,
		"0x04010: MPC         (Missed packets count)           0x%08X\n",
		regs_buff[58]);

	fprintf(stdout,
		"0x04014: SCC         (Single collision count)         0x%08X\n",
		regs_buff[59]);

	fprintf(stdout,
		"0x04018: ECOL        (Excessive collisions count)     0x%08X\n",
		regs_buff[60]);

	fprintf(stdout,
		"0x0401C: MCC         (Multiple collision count)       0x%08X\n",
		regs_buff[61]);

	fprintf(stdout,
		"0x04020: LATECOL     (Late collisions count)          0x%08X\n",
		regs_buff[62]);

	fprintf(stdout,
		"0x04028: COLC        (Collision count)                0x%08X\n",
		regs_buff[63]);

	fprintf(stdout,
		"0x04030: DC          (Defer count)                    0x%08X\n",
		regs_buff[64]);

	fprintf(stdout,
		"0x04034: TNCRS       (Transmit with no CRS)           0x%08X\n",
		regs_buff[65]);

	fprintf(stdout,
		"0x04038: SEC         (Sequence error count)           0x%08X\n",
		regs_buff[66]);

	fprintf(stdout,
		"0x0403C: HTDPMC      (Host tx discrd pkts MAC count)  0x%08X\n",
		regs_buff[67]);

	fprintf(stdout,
		"0x04040: RLEC        (Receive length error count)     0x%08X\n",
		regs_buff[68]);

	fprintf(stdout,
		"0x04048: XONRXC      (XON received count)             0x%08X\n",
		regs_buff[69]);

	fprintf(stdout,
		"0x0404C: XONTXC      (XON transmitted count)          0x%08X\n",
		regs_buff[70]);

	fprintf(stdout,
		"0x04050: XOFFRXC     (XOFF received count)            0x%08X\n",
		regs_buff[71]);

	fprintf(stdout,
		"0x04054: XOFFTXC     (XOFF transmitted count)         0x%08X\n",
		regs_buff[72]);

	fprintf(stdout,
		"0x04058: FCRUC       (FC received unsupported count)  0x%08X\n",
		regs_buff[73]);

	fprintf(stdout,
		"0x0405C: PRC64       (Packets rx (64 B) count)        0x%08X\n",
		regs_buff[74]);

	fprintf(stdout,
		"0x04060: PRC127      (Packets rx (65-127 B) count)    0x%08X\n",
		regs_buff[75]);

	fprintf(stdout,
		"0x04064: PRC255      (Packets rx (128-255 B) count)   0x%08X\n",
		regs_buff[76]);

	fprintf(stdout,
		"0x04068: PRC511      (Packets rx (256-511 B) count)   0x%08X\n",
		regs_buff[77]);

	fprintf(stdout,
		"0x0406C: PRC1023     (Packets rx (512-1023 B) count)  0x%08X\n",
		regs_buff[78]);

	fprintf(stdout,
		"0x04070: PRC1522     (Packets rx (1024-max B) count)  0x%08X\n",
		regs_buff[79]);

	fprintf(stdout,
		"0x04074: GPRC        (Good packets received count)    0x%08X\n",
		regs_buff[80]);

	fprintf(stdout,
		"0x04078: BPRC        (Broadcast packets rx count)     0x%08X\n",
		regs_buff[81]);

	fprintf(stdout,
		"0x0407C: MPRC        (Multicast packets rx count)     0x%08X\n",
		regs_buff[82]);

	fprintf(stdout,
		"0x04080: GPTC        (Good packets tx count)          0x%08X\n",
		regs_buff[83]);

	fprintf(stdout,
		"0x04088: GORCL       (Good octets rx count lower)     0x%08X\n",
		regs_buff[84]);

	fprintf(stdout,
		"0x0408C: GORCH       (Good octets rx count upper)     0x%08X\n",
		regs_buff[85]);

	fprintf(stdout,
		"0x04090: GOTCL       (Good octets tx count lower)     0x%08X\n",
		regs_buff[86]);

	fprintf(stdout,
		"0x04094: GOTCH       (Good octets tx count upper)     0x%08X\n",
		regs_buff[87]);

	fprintf(stdout,
		"0x040A0: RNBC        (Receive no buffers count)       0x%08X\n",
		regs_buff[88]);

	fprintf(stdout,
		"0x040A4: RUC         (Receive undersize count)        0x%08X\n",
		regs_buff[89]);

	fprintf(stdout,
		"0x040A8: RFC         (Receive fragment count)         0x%08X\n",
		regs_buff[90]);

	fprintf(stdout,
		"0x040AC: ROC         (Receive oversize count)         0x%08X\n",
		regs_buff[91]);

	fprintf(stdout,
		"0x040B0: RJC         (Receive jabber count)           0x%08X\n",
		regs_buff[92]);

	fprintf(stdout,
		"0x040B4: MGPRC       (Management packets rx count)    0x%08X\n",
		regs_buff[93]);

	fprintf(stdout,
		"0x040B8: MGPDC       (Management pkts dropped count)  0x%08X\n",
		regs_buff[94]);

	fprintf(stdout,
		"0x040BC: MGPTC       (Management packets tx count)    0x%08X\n",
		regs_buff[95]);

	fprintf(stdout,
		"0x040C0: TORL        (Total octets received lower)    0x%08X\n",
		regs_buff[96]);

	fprintf(stdout,
		"0x040C4: TORH        (Total octets received upper)    0x%08X\n",
		regs_buff[97]);

	fprintf(stdout,
		"0x040C8: TOTL        (Total octets transmitted lower) 0x%08X\n",
		regs_buff[98]);

	fprintf(stdout,
		"0x040CC: TOTH        (Total octets transmitted upper) 0x%08X\n",
		regs_buff[99]);

	fprintf(stdout,
		"0x040D0: TPR         (Total packets received)         0x%08X\n",
		regs_buff[100]);

	fprintf(stdout,
		"0x040D4: TPT         (Total packets transmitted)      0x%08X\n",
		regs_buff[101]);

	fprintf(stdout,
		"0x040D8: PTC64       (Packets tx (64 B) count)        0x%08X\n",
		regs_buff[102]);

	fprintf(stdout,
		"0x040DC: PTC127      (Packets tx (65-127 B) count)    0x%08X\n",
		regs_buff[103]);

	fprintf(stdout,
		"0x040E0: PTC255      (Packets tx (128-255 B) count)   0x%08X\n",
		regs_buff[104]);

	fprintf(stdout,
		"0x040E4: PTC511      (Packets tx (256-511 B) count)   0x%08X\n",
		regs_buff[105]);

	fprintf(stdout,
		"0x040E8: PTC1023     (Packets tx (512-1023 B) count)  0x%08X\n",
		regs_buff[106]);

	fprintf(stdout,
		"0x040EC: PTC1522     (Packets tx (> 1024 B) count)    0x%08X\n",
		regs_buff[107]);

	fprintf(stdout,
		"0x040F0: MPTC        (Multicast packets tx count)     0x%08X\n",
		regs_buff[108]);

	fprintf(stdout,
		"0x040F4: BPTC        (Broadcast packets tx count)     0x%08X\n",
		regs_buff[109]);

	fprintf(stdout,
		"0x040F8: TSCTC       (TCP segment context tx count)   0x%08X\n",
		regs_buff[110]);

	fprintf(stdout,
		"0x04100: IAC         (Interrupt assertion count)      0x%08X\n",
		regs_buff[111]);

	fprintf(stdout,
		"0x04104: RPTHC       (Rx packets to host count)       0x%08X\n",
		regs_buff[112]);

	fprintf(stdout,
		"0x04118: HGPTC       (Host good packets tx count)     0x%08X\n",
		regs_buff[113]);

	fprintf(stdout,
		"0x04128: HGORCL      (Host good octets rx cnt lower)  0x%08X\n",
		regs_buff[114]);

	fprintf(stdout,
		"0x0412C: HGORCH      (Host good octets rx cnt upper)  0x%08X\n",
		regs_buff[115]);

	fprintf(stdout,
		"0x04130: HGOTCL      (Host good octets tx cnt lower)  0x%08X\n",
		regs_buff[116]);

	fprintf(stdout,
		"0x04134: HGOTCH      (Host good octets tx cnt upper)  0x%08X\n",
		regs_buff[117]);

	fprintf(stdout,
		"0x04138: LENNERS     (Length error count)             0x%08X\n",
		regs_buff[118]);

	fprintf(stdout,
		"0x04228: SCVPC       (SerDes/SGMII code viol pkt cnt) 0x%08X\n",
		regs_buff[119]);

	fprintf(stdout,
		"0x0A018: HRMPC       (Header redir missed pkt count)  0x%08X\n",
		regs_buff[120]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: SRRCTL%d     (Split and replic rx ctl%d)       0x%08X\n",
		0x0280C + (0x100 * i), i, i, regs_buff[121 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: PSRTYPE%d    (Packet split receive type%d)     0x%08X\n",
		0x05480 + (0x4 * i), i, i, regs_buff[125 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: RDBAL%d      (Rx desc base addr low%d)         0x%08X\n",
		0x02800 + (0x100 * i), i, i, regs_buff[129 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: RDBAH%d      (Rx desc base addr high%d)        0x%08X\n",
		0x02804 + (0x100 * i), i, i, regs_buff[133 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: RDLEN%d      (Rx descriptor length%d)          0x%08X\n",
		0x02808 + (0x100 * i), i, i, regs_buff[137 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: RDH%d        (Rx descriptor head%d)            0x%08X\n",
		0x02810 + (0x100 * i), i, i, regs_buff[141 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: RDT%d        (Rx descriptor tail%d)            0x%08X\n",
		0x02818 + (0x100 * i), i, i, regs_buff[145 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: RXDCTL%d     (Rx descriptor control%d)         0x%08X\n",
		0x02828 + (0x100 * i), i, i, regs_buff[149 + i]);

	for (i = 0; i < 10; i++)
		fprintf(stdout,
		"0x0%02X: EITR%d       (Interrupt throttle%d)            0x%08X\n",
		0x01680 + (0x4 * i), i, i, regs_buff[153 + i]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x0%02X: IMIR%d       (Immediate interrupt Rx%d)        0x%08X\n",
		0x05A80 + (0x4 * i), i, i, regs_buff[163 + i]);

	for (i = 0; i < 8; i++)
		fprintf(stdout,
		"0x0%02X: IMIREXT%d    (Immediate interr Rx extended%d)  0x%08X\n",
		0x05AA0 + (0x4 * i), i, i, regs_buff[171 + i]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x0%02X: RAL%02d       (Receive address low%02d)          0x%08X\n",
		0x05400 + (0x8 * i), i,i, regs_buff[179 + i]);

	for (i = 0; i < 16; i++)
		fprintf(stdout,
		"0x0%02X: RAH%02d       (Receive address high%02d)         0x%08X\n",
		0x05404 + (0x8 * i), i, i, regs_buff[195 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: TDBAL%d      (Tx desc base address low%d)      0x%08X\n",
		0x03800 + (0x100 * i), i, i, regs_buff[211 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: TDBAH%d      (Tx desc base address high%d)     0x%08X\n",
		0x03804 + (0x100 * i), i, i, regs_buff[215 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: TDLEN%d      (Tx descriptor length%d)          0x%08X\n",
		0x03808 + (0x100 * i), i, i, regs_buff[219 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: TDH%d        (Transmit descriptor head%d)      0x%08X\n",
		0x03810 + (0x100 * i), i, i, regs_buff[223 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: TDT%d        (Transmit descriptor tail%d)      0x%08X\n",
		0x03818 + (0x100 * i), i, i, regs_buff[227 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: TXDCTL%d     (Transmit descriptor control%d)   0x%08X\n",
		0x03828 + (0x100 * i), i, i, regs_buff[231 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: TDWBAL%d     (Tx desc complete wb addr low%d)  0x%08X\n",
		0x03838 + (0x100 * i), i, i, regs_buff[235 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: TDWBAH%d     (Tx desc complete wb addr hi%d)   0x%08X\n",
		0x0383C + (0x100 * i), i, i, regs_buff[239 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: DCA_TXCTRL%d (Tx DCA control%d)                0x%08X\n",
		0x03814 + (0x100 * i), i, i, regs_buff[243 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: IP4AT%d      (IPv4 address table%d)            0x%08X\n",
		0x05840 + (0x8 * i), i, i, regs_buff[247 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: IP6AT%d      (IPv6 address table%d)            0x%08X\n",
		0x05880 + (0x4 * i), i, i, regs_buff[251 + i]);

	for (i = 0; i < 32; i++)
		fprintf(stdout,
		"0x0%02X: WUPM%02d      (Wake up packet memory%02d)        0x%08X\n",
		0x05A00 + (0x4 * i), i, i, regs_buff[255 + i]);

	for (i = 0; i < 128; i++)
		fprintf(stdout,
		"0x0%02X: FFMT%03d     (Flexible filter mask table%03d)  0x%08X\n",
		0x09000 + (0x8 * i), i, i, regs_buff[287 + i]);

	for (i = 0; i < 128; i++)
		fprintf(stdout,
		"0x0%02X: FFVT%03d     (Flexible filter value table%03d) 0x%08X\n",
		0x09800 + (0x8 * i), i, i, regs_buff[415 + i]);

	for (i = 0; i < 4; i++)
		fprintf(stdout,
		"0x0%02X: FFLT%d       (Flexible filter length table%d)  0x%08X\n",
		0x05F00 + (0x8 * i), i, i, regs_buff[543 + i]);

	fprintf(stdout,
		"0x03410: TDFH        (Tx data FIFO head)              0x%08X\n",
		regs_buff[547]);

	fprintf(stdout,
		"0x03418: TDFT        (Tx data FIFO tail)              0x%08X\n",
		regs_buff[548]);

	fprintf(stdout,
		"0x03420: TDFHS       (Tx data FIFO head saved)        0x%08X\n",
		regs_buff[549]);

	fprintf(stdout,
		"0x03430: TDFPC       (Tx data FIFO packet count)      0x%08X\n",
		regs_buff[550]);

	return 0;
}

