/*
 * r8169.c: RealTek 8169/8168/8101 ethernet driver.
 *
 * Copyright (c) 2002 ShuChen <shuchen@realtek.com.tw>
 * Copyright (c) 2003 - 2007 Francois Romieu <romieu@fr.zoreil.com>
 * Copyright (c) a lot of people too. Please respect their work.
 *
 * See MAINTAINERS file for support contact information.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>

#define RTL8169_VERSION "2.3LK-NAPI"
#define MODULENAME "r8169"
#define PFX MODULENAME ": "

#ifdef RTL8169_DEBUG
#define assert(expr) \
	if (!(expr)) {					\
		printk( "Assertion failed! %s,%s,%s,line=%d\n",	\
		#expr,__FILE__,__func__,__LINE__);		\
	}
#define dprintk(fmt, args...) \
	do { printk(KERN_DEBUG PFX fmt, ## args); } while (0)
#else
#define assert(expr) do {} while (0)
#define dprintk(fmt, args...)	do {} while (0)
#endif /* RTL8169_DEBUG */

#define R8169_MSG_DEFAULT \
	(NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_IFUP | NETIF_MSG_IFDOWN)

#define TX_BUFFS_AVAIL(tp) \
	(tp->dirty_tx + NUM_TX_DESC - tp->cur_tx - 1)

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC. */
static const int multicast_filter_limit = 32;

/* MAC address length */
#define MAC_ADDR_LEN	6

#define MAX_READ_REQUEST_SHIFT	12
#define RX_FIFO_THRESH	7	/* 7 means NO threshold, Rx buffer level before first PCI xfer. */
#define RX_DMA_BURST	6	/* Maximum PCI burst, '6' is 1024 */
#define TX_DMA_BURST	6	/* Maximum PCI burst, '6' is 1024 */
#define EarlyTxThld	0x3F	/* 0x3F means NO early transmit */
#define SafeMtu		0x1c20	/* ... actually life sucks beyond ~7k */
#define InterFrameGap	0x03	/* 3 means InterFrameGap = the shortest one */

#define R8169_REGS_SIZE		256
#define R8169_NAPI_WEIGHT	64
#define NUM_TX_DESC	64	/* Number of Tx descriptor registers */
#define NUM_RX_DESC	256	/* Number of Rx descriptor registers */
#define RX_BUF_SIZE	1536	/* Rx Buffer size */
#define R8169_TX_RING_BYTES	(NUM_TX_DESC * sizeof(struct TxDesc))
#define R8169_RX_RING_BYTES	(NUM_RX_DESC * sizeof(struct RxDesc))

#define RTL8169_TX_TIMEOUT	(6*HZ)
#define RTL8169_PHY_TIMEOUT	(10*HZ)

#define RTL_EEPROM_SIG		cpu_to_le32(0x8129)
#define RTL_EEPROM_SIG_MASK	cpu_to_le32(0xffff)
#define RTL_EEPROM_SIG_ADDR	0x0000

/* write/read MMIO register */
#define RTL_W8(reg, val8)	writeb ((val8), ioaddr + (reg))
#define RTL_W16(reg, val16)	writew ((val16), ioaddr + (reg))
#define RTL_W32(reg, val32)	writel ((val32), ioaddr + (reg))
#define RTL_R8(reg)		readb (ioaddr + (reg))
#define RTL_R16(reg)		readw (ioaddr + (reg))
#define RTL_R32(reg)		readl (ioaddr + (reg))

enum mac_version {
	RTL_GIGA_MAC_NONE   = 0x00,
	RTL_GIGA_MAC_VER_01 = 0x01, // 8169
	RTL_GIGA_MAC_VER_02 = 0x02, // 8169S
	RTL_GIGA_MAC_VER_03 = 0x03, // 8110S
	RTL_GIGA_MAC_VER_04 = 0x04, // 8169SB
	RTL_GIGA_MAC_VER_05 = 0x05, // 8110SCd
	RTL_GIGA_MAC_VER_06 = 0x06, // 8110SCe
	RTL_GIGA_MAC_VER_07 = 0x07, // 8102e
	RTL_GIGA_MAC_VER_08 = 0x08, // 8102e
	RTL_GIGA_MAC_VER_09 = 0x09, // 8102e
	RTL_GIGA_MAC_VER_10 = 0x0a, // 8101e
	RTL_GIGA_MAC_VER_11 = 0x0b, // 8168Bb
	RTL_GIGA_MAC_VER_12 = 0x0c, // 8168Be
	RTL_GIGA_MAC_VER_13 = 0x0d, // 8101Eb
	RTL_GIGA_MAC_VER_14 = 0x0e, // 8101 ?
	RTL_GIGA_MAC_VER_15 = 0x0f, // 8101 ?
	RTL_GIGA_MAC_VER_16 = 0x11, // 8101Ec
	RTL_GIGA_MAC_VER_17 = 0x10, // 8168Bf
	RTL_GIGA_MAC_VER_18 = 0x12, // 8168CP
	RTL_GIGA_MAC_VER_19 = 0x13, // 8168C
	RTL_GIGA_MAC_VER_20 = 0x14, // 8168C
	RTL_GIGA_MAC_VER_21 = 0x15, // 8168C
	RTL_GIGA_MAC_VER_22 = 0x16, // 8168C
	RTL_GIGA_MAC_VER_23 = 0x17, // 8168CP
	RTL_GIGA_MAC_VER_24 = 0x18, // 8168CP
	RTL_GIGA_MAC_VER_25 = 0x19, // 8168D
	RTL_GIGA_MAC_VER_26 = 0x1a, // 8168D
	RTL_GIGA_MAC_VER_27 = 0x1b  // 8168DP
};

#define _R(NAME,MAC,MASK) \
	{ .name = NAME, .mac_version = MAC, .RxConfigMask = MASK }

static const struct {
	const char *name;
	u8 mac_version;
	u32 RxConfigMask;	/* Clears the bits supported by this chip */
} rtl_chip_info[] = {
	_R("RTL8169",		RTL_GIGA_MAC_VER_01, 0xff7e1880), // 8169
	_R("RTL8169s",		RTL_GIGA_MAC_VER_02, 0xff7e1880), // 8169S
	_R("RTL8110s",		RTL_GIGA_MAC_VER_03, 0xff7e1880), // 8110S
	_R("RTL8169sb/8110sb",	RTL_GIGA_MAC_VER_04, 0xff7e1880), // 8169SB
	_R("RTL8169sc/8110sc",	RTL_GIGA_MAC_VER_05, 0xff7e1880), // 8110SCd
	_R("RTL8169sc/8110sc",	RTL_GIGA_MAC_VER_06, 0xff7e1880), // 8110SCe
	_R("RTL8102e",		RTL_GIGA_MAC_VER_07, 0xff7e1880), // PCI-E
	_R("RTL8102e",		RTL_GIGA_MAC_VER_08, 0xff7e1880), // PCI-E
	_R("RTL8102e",		RTL_GIGA_MAC_VER_09, 0xff7e1880), // PCI-E
	_R("RTL8101e",		RTL_GIGA_MAC_VER_10, 0xff7e1880), // PCI-E
	_R("RTL8168b/8111b",	RTL_GIGA_MAC_VER_11, 0xff7e1880), // PCI-E
	_R("RTL8168b/8111b",	RTL_GIGA_MAC_VER_12, 0xff7e1880), // PCI-E
	_R("RTL8101e",		RTL_GIGA_MAC_VER_13, 0xff7e1880), // PCI-E 8139
	_R("RTL8100e",		RTL_GIGA_MAC_VER_14, 0xff7e1880), // PCI-E 8139
	_R("RTL8100e",		RTL_GIGA_MAC_VER_15, 0xff7e1880), // PCI-E 8139
	_R("RTL8168b/8111b",	RTL_GIGA_MAC_VER_17, 0xff7e1880), // PCI-E
	_R("RTL8101e",		RTL_GIGA_MAC_VER_16, 0xff7e1880), // PCI-E
	_R("RTL8168cp/8111cp",	RTL_GIGA_MAC_VER_18, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_19, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_20, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_21, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_22, 0xff7e1880), // PCI-E
	_R("RTL8168cp/8111cp",	RTL_GIGA_MAC_VER_23, 0xff7e1880), // PCI-E
	_R("RTL8168cp/8111cp",	RTL_GIGA_MAC_VER_24, 0xff7e1880), // PCI-E
	_R("RTL8168d/8111d",	RTL_GIGA_MAC_VER_25, 0xff7e1880), // PCI-E
	_R("RTL8168d/8111d",	RTL_GIGA_MAC_VER_26, 0xff7e1880), // PCI-E
	_R("RTL8168dp/8111dp",	RTL_GIGA_MAC_VER_27, 0xff7e1880)  // PCI-E
};
#undef _R

enum cfg_version {
	RTL_CFG_0 = 0x00,
	RTL_CFG_1,
	RTL_CFG_2
};

static void rtl_hw_start_8169(struct net_device *);
static void rtl_hw_start_8168(struct net_device *);
static void rtl_hw_start_8101(struct net_device *);

static DEFINE_PCI_DEVICE_TABLE(rtl8169_pci_tbl) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8129), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8136), 0, 0, RTL_CFG_2 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8167), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8168), 0, 0, RTL_CFG_1 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8169), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK,	0x4300), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_AT,		0xc107), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(0x16ec,			0x0116), 0, 0, RTL_CFG_0 },
	{ PCI_VENDOR_ID_LINKSYS,		0x1032,
		PCI_ANY_ID, 0x0024, 0, 0, RTL_CFG_0 },
	{ 0x0001,				0x8168,
		PCI_ANY_ID, 0x2410, 0, 0, RTL_CFG_2 },
	{0,},
};

MODULE_DEVICE_TABLE(pci, rtl8169_pci_tbl);

/*
 * we set our copybreak very high so that we don't have
 * to allocate 16k frames all the time (see note in
 * rtl8169_open()
 */
static int rx_copybreak = 16383;
static int use_dac;
static struct {
	u32 msg_enable;
} debug = { -1 };

enum rtl_registers {
	MAC0		= 0,	/* Ethernet hardware address. */
	MAC4		= 4,
	MAR0		= 8,	/* Multicast filter. */
	CounterAddrLow		= 0x10,
	CounterAddrHigh		= 0x14,
	TxDescStartAddrLow	= 0x20,
	TxDescStartAddrHigh	= 0x24,
	TxHDescStartAddrLow	= 0x28,
	TxHDescStartAddrHigh	= 0x2c,
	FLASH		= 0x30,
	ERSR		= 0x36,
	ChipCmd		= 0x37,
	TxPoll		= 0x38,
	IntrMask	= 0x3c,
	IntrStatus	= 0x3e,
	TxConfig	= 0x40,
	RxConfig	= 0x44,
	RxMissed	= 0x4c,
	Cfg9346		= 0x50,
	Config0		= 0x51,
	Config1		= 0x52,
	Config2		= 0x53,
	Config3		= 0x54,
	Config4		= 0x55,
	Config5		= 0x56,
	MultiIntr	= 0x5c,
	PHYAR		= 0x60,
	PHYstatus	= 0x6c,
	RxMaxSize	= 0xda,
	CPlusCmd	= 0xe0,
	IntrMitigate	= 0xe2,
	RxDescAddrLow	= 0xe4,
	RxDescAddrHigh	= 0xe8,
	EarlyTxThres	= 0xec,
	FuncEvent	= 0xf0,
	FuncEventMask	= 0xf4,
	FuncPresetState	= 0xf8,
	FuncForceEvent	= 0xfc,
};

enum rtl8110_registers {
	TBICSR			= 0x64,
	TBI_ANAR		= 0x68,
	TBI_LPAR		= 0x6a,
};

enum rtl8168_8101_registers {
	CSIDR			= 0x64,
	CSIAR			= 0x68,
#define	CSIAR_FLAG			0x80000000
#define	CSIAR_WRITE_CMD			0x80000000
#define	CSIAR_BYTE_ENABLE		0x0f
#define	CSIAR_BYTE_ENABLE_SHIFT		12
#define	CSIAR_ADDR_MASK			0x0fff

	EPHYAR			= 0x80,
#define	EPHYAR_FLAG			0x80000000
#define	EPHYAR_WRITE_CMD		0x80000000
#define	EPHYAR_REG_MASK			0x1f
#define	EPHYAR_REG_SHIFT		16
#define	EPHYAR_DATA_MASK		0xffff
	DBG_REG			= 0xd1,
#define	FIX_NAK_1			(1 << 4)
#define	FIX_NAK_2			(1 << 3)
	EFUSEAR			= 0xdc,
#define	EFUSEAR_FLAG			0x80000000
#define	EFUSEAR_WRITE_CMD		0x80000000
#define	EFUSEAR_READ_CMD		0x00000000
#define	EFUSEAR_REG_MASK		0x03ff
#define	EFUSEAR_REG_SHIFT		8
#define	EFUSEAR_DATA_MASK		0xff
};

enum rtl_register_content {
	/* InterruptStatusBits */
	SYSErr		= 0x8000,
	PCSTimeout	= 0x4000,
	SWInt		= 0x0100,
	TxDescUnavail	= 0x0080,
	RxFIFOOver	= 0x0040,
	LinkChg		= 0x0020,
	RxOverflow	= 0x0010,
	TxErr		= 0x0008,
	TxOK		= 0x0004,
	RxErr		= 0x0002,
	RxOK		= 0x0001,

	/* RxStatusDesc */
	RxFOVF	= (1 << 23),
	RxRWT	= (1 << 22),
	RxRES	= (1 << 21),
	RxRUNT	= (1 << 20),
	RxCRC	= (1 << 19),

	/* ChipCmdBits */
	CmdReset	= 0x10,
	CmdRxEnb	= 0x08,
	CmdTxEnb	= 0x04,
	RxBufEmpty	= 0x01,

	/* TXPoll register p.5 */
	HPQ		= 0x80,		/* Poll cmd on the high prio queue */
	NPQ		= 0x40,		/* Poll cmd on the low prio queue */
	FSWInt		= 0x01,		/* Forced software interrupt */

	/* Cfg9346Bits */
	Cfg9346_Lock	= 0x00,
	Cfg9346_Unlock	= 0xc0,

	/* rx_mode_bits */
	AcceptErr	= 0x20,
	AcceptRunt	= 0x10,
	AcceptBroadcast	= 0x08,
	AcceptMulticast	= 0x04,
	AcceptMyPhys	= 0x02,
	AcceptAllPhys	= 0x01,

	/* RxConfigBits */
	RxCfgFIFOShift	= 13,
	RxCfgDMAShift	=  8,

	/* TxConfigBits */
	TxInterFrameGapShift = 24,
	TxDMAShift = 8,	/* DMA burst value (0-7) is shift this many bits */

	/* Config1 register p.24 */
	LEDS1		= (1 << 7),
	LEDS0		= (1 << 6),
	MSIEnable	= (1 << 5),	/* Enable Message Signaled Interrupt */
	Speed_down	= (1 << 4),
	MEMMAP		= (1 << 3),
	IOMAP		= (1 << 2),
	VPD		= (1 << 1),
	PMEnable	= (1 << 0),	/* Power Management Enable */

	/* Config2 register p. 25 */
	PCI_Clock_66MHz = 0x01,
	PCI_Clock_33MHz = 0x00,

	/* Config3 register p.25 */
	MagicPacket	= (1 << 5),	/* Wake up when receives a Magic Packet */
	LinkUp		= (1 << 4),	/* Wake up when the cable connection is re-established */
	Beacon_en	= (1 << 0),	/* 8168 only. Reserved in the 8168b */

	/* Config5 register p.27 */
	BWF		= (1 << 6),	/* Accept Broadcast wakeup frame */
	MWF		= (1 << 5),	/* Accept Multicast wakeup frame */
	UWF		= (1 << 4),	/* Accept Unicast wakeup frame */
	LanWake		= (1 << 1),	/* LanWake enable/disable */
	PMEStatus	= (1 << 0),	/* PME status can be reset by PCI RST# */

	/* TBICSR p.28 */
	TBIReset	= 0x80000000,
	TBILoopback	= 0x40000000,
	TBINwEnable	= 0x20000000,
	TBINwRestart	= 0x10000000,
	TBILinkOk	= 0x02000000,
	TBINwComplete	= 0x01000000,

	/* CPlusCmd p.31 */
	EnableBist	= (1 << 15),	// 8168 8101
	Mac_dbgo_oe	= (1 << 14),	// 8168 8101
	Normal_mode	= (1 << 13),	// unused
	Force_half_dup	= (1 << 12),	// 8168 8101
	Force_rxflow_en	= (1 << 11),	// 8168 8101
	Force_txflow_en	= (1 << 10),	// 8168 8101
	Cxpl_dbg_sel	= (1 << 9),	// 8168 8101
	ASF		= (1 << 8),	// 8168 8101
	PktCntrDisable	= (1 << 7),	// 8168 8101
	Mac_dbgo_sel	= 0x001c,	// 8168
	RxVlan		= (1 << 6),
	RxChkSum	= (1 << 5),
	PCIDAC		= (1 << 4),
	PCIMulRW	= (1 << 3),
	INTT_0		= 0x0000,	// 8168
	INTT_1		= 0x0001,	// 8168
	INTT_2		= 0x0002,	// 8168
	INTT_3		= 0x0003,	// 8168

	/* rtl8169_PHYstatus */
	TBI_Enable	= 0x80,
	TxFlowCtrl	= 0x40,
	RxFlowCtrl	= 0x20,
	_1000bpsF	= 0x10,
	_100bps		= 0x08,
	_10bps		= 0x04,
	LinkStatus	= 0x02,
	FullDup		= 0x01,

	/* _TBICSRBit */
	TBILinkOK	= 0x02000000,

	/* DumpCounterCommand */
	CounterDump	= 0x8,
};

enum desc_status_bit {
	DescOwn		= (1 << 31), /* Descriptor is owned by NIC */
	RingEnd		= (1 << 30), /* End of descriptor ring */
	FirstFrag	= (1 << 29), /* First segment of a packet */
	LastFrag	= (1 << 28), /* Final segment of a packet */

	/* Tx private */
	LargeSend	= (1 << 27), /* TCP Large Send Offload (TSO) */
	MSSShift	= 16,        /* MSS value position */
	MSSMask		= 0xfff,     /* MSS value + LargeSend bit: 12 bits */
	IPCS		= (1 << 18), /* Calculate IP checksum */
	UDPCS		= (1 << 17), /* Calculate UDP/IP checksum */
	TCPCS		= (1 << 16), /* Calculate TCP/IP checksum */
	TxVlanTag	= (1 << 17), /* Add VLAN tag */

	/* Rx private */
	PID1		= (1 << 18), /* Protocol ID bit 1/2 */
	PID0		= (1 << 17), /* Protocol ID bit 2/2 */

#define RxProtoUDP	(PID1)
#define RxProtoTCP	(PID0)
#define RxProtoIP	(PID1 | PID0)
#define RxProtoMask	RxProtoIP

	IPFail		= (1 << 16), /* IP checksum failed */
	UDPFail		= (1 << 15), /* UDP/IP checksum failed */
	TCPFail		= (1 << 14), /* TCP/IP checksum failed */
	RxVlanTag	= (1 << 16), /* VLAN tag available */
};

#define RsvdMask	0x3fffc000

struct TxDesc {
	__le32 opts1;
	__le32 opts2;
	__le64 addr;
};

struct RxDesc {
	__le32 opts1;
	__le32 opts2;
	__le64 addr;
};

struct ring_info {
	struct sk_buff	*skb;
	u32		len;
	u8		__pad[sizeof(void *) - sizeof(u32)];
};

enum features {
	RTL_FEATURE_WOL		= (1 << 0),
	RTL_FEATURE_MSI		= (1 << 1),
	RTL_FEATURE_GMII	= (1 << 2),
};

struct rtl8169_counters {
	__le64	tx_packets;
	__le64	rx_packets;
	__le64	tx_errors;
	__le32	rx_errors;
	__le16	rx_missed;
	__le16	align_errors;
	__le32	tx_one_collision;
	__le32	tx_multi_collision;
	__le64	rx_unicast;
	__le64	rx_broadcast;
	__le32	rx_multicast;
	__le16	tx_aborted;
	__le16	tx_underun;
};

struct rtl8169_private {
	void __iomem *mmio_addr;	/* memory map physical address */
	struct pci_dev *pci_dev;	/* Index of PCI device */
	struct net_device *dev;
	struct napi_struct napi;
	spinlock_t lock;		/* spin lock flag */
	u32 msg_enable;
	int chipset;
	int mac_version;
	u32 cur_rx; /* Index into the Rx descriptor buffer of next Rx pkt. */
	u32 cur_tx; /* Index into the Tx descriptor buffer of next Rx pkt. */
	u32 dirty_rx;
	u32 dirty_tx;
	struct TxDesc *TxDescArray;	/* 256-aligned Tx descriptor ring */
	struct RxDesc *RxDescArray;	/* 256-aligned Rx descriptor ring */
	dma_addr_t TxPhyAddr;
	dma_addr_t RxPhyAddr;
	struct sk_buff *Rx_skbuff[NUM_RX_DESC];	/* Rx data buffers */
	struct ring_info tx_skb[NUM_TX_DESC];	/* Tx data buffers */
	unsigned align;
	unsigned rx_buf_sz;
	struct timer_list timer;
	u16 cp_cmd;
	u16 intr_event;
	u16 napi_event;
	u16 intr_mask;
	int phy_1000_ctrl_reg;
#ifdef CONFIG_R8169_VLAN
	struct vlan_group *vlgrp;
#endif
	int (*set_speed)(struct net_device *, u8 autoneg, u16 speed, u8 duplex);
	int (*get_settings)(struct net_device *, struct ethtool_cmd *);
	void (*phy_reset_enable)(void __iomem *);
	void (*hw_start)(struct net_device *);
	unsigned int (*phy_reset_pending)(void __iomem *);
	unsigned int (*link_ok)(void __iomem *);
	int (*do_ioctl)(struct rtl8169_private *tp, struct mii_ioctl_data *data, int cmd);
	int pcie_cap;
	struct delayed_work task;
	unsigned features;

	struct mii_if_info mii;
	struct rtl8169_counters counters;
	u32 saved_wolopts;
};

MODULE_AUTHOR("Realtek and the Linux r8169 crew <netdev@vger.kernel.org>");
MODULE_DESCRIPTION("RealTek RTL-8169 Gigabit Ethernet driver");
module_param(rx_copybreak, int, 0);
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");
module_param(use_dac, int, 0);
MODULE_PARM_DESC(use_dac, "Enable PCI DAC. Unsafe on 32 bit PCI slot.");
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., 16=all)");
MODULE_LICENSE("GPL");
MODULE_VERSION(RTL8169_VERSION);

static int rtl8169_open(struct net_device *dev);
static netdev_tx_t rtl8169_start_xmit(struct sk_buff *skb,
				      struct net_device *dev);
static irqreturn_t rtl8169_interrupt(int irq, void *dev_instance);
static int rtl8169_init_ring(struct net_device *dev);
static void rtl_hw_start(struct net_device *dev);
static int rtl8169_close(struct net_device *dev);
static void rtl_set_rx_mode(struct net_device *dev);
static void rtl8169_tx_timeout(struct net_device *dev);
static struct net_device_stats *rtl8169_get_stats(struct net_device *dev);
static int rtl8169_rx_interrupt(struct net_device *, struct rtl8169_private *,
				void __iomem *, u32 budget);
static int rtl8169_change_mtu(struct net_device *dev, int new_mtu);
static void rtl8169_down(struct net_device *dev);
static void rtl8169_rx_clear(struct rtl8169_private *tp);
static int rtl8169_poll(struct napi_struct *napi, int budget);

static const unsigned int rtl8169_rx_config =
	(RX_FIFO_THRESH << RxCfgFIFOShift) | (RX_DMA_BURST << RxCfgDMAShift);

static void mdio_write(void __iomem *ioaddr, int reg_addr, int value)
{
	int i;

	RTL_W32(PHYAR, 0x80000000 | (reg_addr & 0x1f) << 16 | (value & 0xffff));

	for (i = 20; i > 0; i--) {
		/*
		 * Check if the RTL8169 has completed writing to the specified
		 * MII register.
		 */
		if (!(RTL_R32(PHYAR) & 0x80000000))
			break;
		udelay(25);
	}
	/*
	 * According to hardware specs a 20us delay is required after write
	 * complete indication, but before sending next command.
	 */
	udelay(20);
}

static int mdio_read(void __iomem *ioaddr, int reg_addr)
{
	int i, value = -1;

	RTL_W32(PHYAR, 0x0 | (reg_addr & 0x1f) << 16);

	for (i = 20; i > 0; i--) {
		/*
		 * Check if the RTL8169 has completed retrieving data from
		 * the specified MII register.
		 */
		if (RTL_R32(PHYAR) & 0x80000000) {
			value = RTL_R32(PHYAR) & 0xffff;
			break;
		}
		udelay(25);
	}
	/*
	 * According to hardware specs a 20us delay is required after read
	 * complete indication, but before sending next command.
	 */
	udelay(20);

	return value;
}

static void mdio_patch(void __iomem *ioaddr, int reg_addr, int value)
{
	mdio_write(ioaddr, reg_addr, mdio_read(ioaddr, reg_addr) | value);
}

static void mdio_plus_minus(void __iomem *ioaddr, int reg_addr, int p, int m)
{
	int val;

	val = mdio_read(ioaddr, reg_addr);
	mdio_write(ioaddr, reg_addr, (val | p) & ~m);
}

static void rtl_mdio_write(struct net_device *dev, int phy_id, int location,
			   int val)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	mdio_write(ioaddr, location, val);
}

static int rtl_mdio_read(struct net_device *dev, int phy_id, int location)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	return mdio_read(ioaddr, location);
}

static void rtl_ephy_write(void __iomem *ioaddr, int reg_addr, int value)
{
	unsigned int i;

	RTL_W32(EPHYAR, EPHYAR_WRITE_CMD | (value & EPHYAR_DATA_MASK) |
		(reg_addr & EPHYAR_REG_MASK) << EPHYAR_REG_SHIFT);

	for (i = 0; i < 100; i++) {
		if (!(RTL_R32(EPHYAR) & EPHYAR_FLAG))
			break;
		udelay(10);
	}
}

static u16 rtl_ephy_read(void __iomem *ioaddr, int reg_addr)
{
	u16 value = 0xffff;
	unsigned int i;

	RTL_W32(EPHYAR, (reg_addr & EPHYAR_REG_MASK) << EPHYAR_REG_SHIFT);

	for (i = 0; i < 100; i++) {
		if (RTL_R32(EPHYAR) & EPHYAR_FLAG) {
			value = RTL_R32(EPHYAR) & EPHYAR_DATA_MASK;
			break;
		}
		udelay(10);
	}

	return value;
}

static void rtl_csi_write(void __iomem *ioaddr, int addr, int value)
{
	unsigned int i;

	RTL_W32(CSIDR, value);
	RTL_W32(CSIAR, CSIAR_WRITE_CMD | (addr & CSIAR_ADDR_MASK) |
		CSIAR_BYTE_ENABLE << CSIAR_BYTE_ENABLE_SHIFT);

	for (i = 0; i < 100; i++) {
		if (!(RTL_R32(CSIAR) & CSIAR_FLAG))
			break;
		udelay(10);
	}
}

static u32 rtl_csi_read(void __iomem *ioaddr, int addr)
{
	u32 value = ~0x00;
	unsigned int i;

	RTL_W32(CSIAR, (addr & CSIAR_ADDR_MASK) |
		CSIAR_BYTE_ENABLE << CSIAR_BYTE_ENABLE_SHIFT);

	for (i = 0; i < 100; i++) {
		if (RTL_R32(CSIAR) & CSIAR_FLAG) {
			value = RTL_R32(CSIDR);
			break;
		}
		udelay(10);
	}

	return value;
}

static u8 rtl8168d_efuse_read(void __iomem *ioaddr, int reg_addr)
{
	u8 value = 0xff;
	unsigned int i;

	RTL_W32(EFUSEAR, (reg_addr & EFUSEAR_REG_MASK) << EFUSEAR_REG_SHIFT);

	for (i = 0; i < 300; i++) {
		if (RTL_R32(EFUSEAR) & EFUSEAR_FLAG) {
			value = RTL_R32(EFUSEAR) & EFUSEAR_DATA_MASK;
			break;
		}
		udelay(100);
	}

	return value;
}

static void rtl8169_irq_mask_and_ack(void __iomem *ioaddr)
{
	RTL_W16(IntrMask, 0x0000);

	RTL_W16(IntrStatus, 0xffff);
}

static void rtl8169_asic_down(void __iomem *ioaddr)
{
	RTL_W8(ChipCmd, 0x00);
	rtl8169_irq_mask_and_ack(ioaddr);
	RTL_R16(CPlusCmd);
}

static unsigned int rtl8169_tbi_reset_pending(void __iomem *ioaddr)
{
	return RTL_R32(TBICSR) & TBIReset;
}

static unsigned int rtl8169_xmii_reset_pending(void __iomem *ioaddr)
{
	return mdio_read(ioaddr, MII_BMCR) & BMCR_RESET;
}

static unsigned int rtl8169_tbi_link_ok(void __iomem *ioaddr)
{
	return RTL_R32(TBICSR) & TBILinkOk;
}

static unsigned int rtl8169_xmii_link_ok(void __iomem *ioaddr)
{
	return RTL_R8(PHYstatus) & LinkStatus;
}

static void rtl8169_tbi_reset_enable(void __iomem *ioaddr)
{
	RTL_W32(TBICSR, RTL_R32(TBICSR) | TBIReset);
}

static void rtl8169_xmii_reset_enable(void __iomem *ioaddr)
{
	unsigned int val;

	val = mdio_read(ioaddr, MII_BMCR) | BMCR_RESET;
	mdio_write(ioaddr, MII_BMCR, val & 0xffff);
}

static void rtl8169_check_link_status(struct net_device *dev,
				      struct rtl8169_private *tp,
				      void __iomem *ioaddr)
{
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);
	if (tp->link_ok(ioaddr)) {
		/* This is to cancel a scheduled suspend if there's one. */
		pm_request_resume(&tp->pci_dev->dev);
		netif_carrier_on(dev);
		netif_info(tp, ifup, dev, "link up\n");
	} else {
		netif_carrier_off(dev);
		netif_info(tp, ifdown, dev, "link down\n");
		pm_schedule_suspend(&tp->pci_dev->dev, 100);
	}
	spin_unlock_irqrestore(&tp->lock, flags);
}

#define WAKE_ANY (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_BCAST | WAKE_MCAST)

static u32 __rtl8169_get_wol(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u8 options;
	u32 wolopts = 0;

	options = RTL_R8(Config1);
	if (!(options & PMEnable))
		return 0;

	options = RTL_R8(Config3);
	if (options & LinkUp)
		wolopts |= WAKE_PHY;
	if (options & MagicPacket)
		wolopts |= WAKE_MAGIC;

	options = RTL_R8(Config5);
	if (options & UWF)
		wolopts |= WAKE_UCAST;
	if (options & BWF)
		wolopts |= WAKE_BCAST;
	if (options & MWF)
		wolopts |= WAKE_MCAST;

	return wolopts;
}

static void rtl8169_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	spin_lock_irq(&tp->lock);

	wol->supported = WAKE_ANY;
	wol->wolopts = __rtl8169_get_wol(tp);

	spin_unlock_irq(&tp->lock);
}

static void __rtl8169_set_wol(struct rtl8169_private *tp, u32 wolopts)
{
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int i;
	static const struct {
		u32 opt;
		u16 reg;
		u8  mask;
	} cfg[] = {
		{ WAKE_ANY,   Config1, PMEnable },
		{ WAKE_PHY,   Config3, LinkUp },
		{ WAKE_MAGIC, Config3, MagicPacket },
		{ WAKE_UCAST, Config5, UWF },
		{ WAKE_BCAST, Config5, BWF },
		{ WAKE_MCAST, Config5, MWF },
		{ WAKE_ANY,   Config5, LanWake }
	};

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	for (i = 0; i < ARRAY_SIZE(cfg); i++) {
		u8 options = RTL_R8(cfg[i].reg) & ~cfg[i].mask;
		if (wolopts & cfg[i].opt)
			options |= cfg[i].mask;
		RTL_W8(cfg[i].reg, options);
	}

	RTL_W8(Cfg9346, Cfg9346_Lock);
}

static int rtl8169_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	spin_lock_irq(&tp->lock);

	if (wol->wolopts)
		tp->features |= RTL_FEATURE_WOL;
	else
		tp->features &= ~RTL_FEATURE_WOL;
	__rtl8169_set_wol(tp, wol->wolopts);
	spin_unlock_irq(&tp->lock);

	device_set_wakeup_enable(&tp->pci_dev->dev, wol->wolopts);

	return 0;
}

static void rtl8169_get_drvinfo(struct net_device *dev,
				struct ethtool_drvinfo *info)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	strcpy(info->driver, MODULENAME);
	strcpy(info->version, RTL8169_VERSION);
	strcpy(info->bus_info, pci_name(tp->pci_dev));
}

static int rtl8169_get_regs_len(struct net_device *dev)
{
	return R8169_REGS_SIZE;
}

static int rtl8169_set_speed_tbi(struct net_device *dev,
				 u8 autoneg, u16 speed, u8 duplex)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	int ret = 0;
	u32 reg;

	reg = RTL_R32(TBICSR);
	if ((autoneg == AUTONEG_DISABLE) && (speed == SPEED_1000) &&
	    (duplex == DUPLEX_FULL)) {
		RTL_W32(TBICSR, reg & ~(TBINwEnable | TBINwRestart));
	} else if (autoneg == AUTONEG_ENABLE)
		RTL_W32(TBICSR, reg | TBINwEnable | TBINwRestart);
	else {
		netif_warn(tp, link, dev,
			   "incorrect speed setting refused in TBI mode\n");
		ret = -EOPNOTSUPP;
	}

	return ret;
}

static int rtl8169_set_speed_xmii(struct net_device *dev,
				  u8 autoneg, u16 speed, u8 duplex)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	int giga_ctrl, bmcr;

	if (autoneg == AUTONEG_ENABLE) {
		int auto_nego;

		auto_nego = mdio_read(ioaddr, MII_ADVERTISE);
		auto_nego |= (ADVERTISE_10HALF | ADVERTISE_10FULL |
			      ADVERTISE_100HALF | ADVERTISE_100FULL);
		auto_nego |= ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;

		giga_ctrl = mdio_read(ioaddr, MII_CTRL1000);
		giga_ctrl &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);

		/* The 8100e/8101e/8102e do Fast Ethernet only. */
		if ((tp->mac_version != RTL_GIGA_MAC_VER_07) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_08) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_09) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_10) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_13) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_14) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_15) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_16)) {
			giga_ctrl |= ADVERTISE_1000FULL | ADVERTISE_1000HALF;
		} else {
			netif_info(tp, link, dev,
				   "PHY does not support 1000Mbps\n");
		}

		bmcr = BMCR_ANENABLE | BMCR_ANRESTART;

		if ((tp->mac_version == RTL_GIGA_MAC_VER_11) ||
		    (tp->mac_version == RTL_GIGA_MAC_VER_12) ||
		    (tp->mac_version >= RTL_GIGA_MAC_VER_17)) {
			/*
			 * Wake up the PHY.
			 * Vendor specific (0x1f) and reserved (0x0e) MII
			 * registers.
			 */
			mdio_write(ioaddr, 0x1f, 0x0000);
			mdio_write(ioaddr, 0x0e, 0x0000);
		}

		mdio_write(ioaddr, MII_ADVERTISE, auto_nego);
		mdio_write(ioaddr, MII_CTRL1000, giga_ctrl);
	} else {
		giga_ctrl = 0;

		if (speed == SPEED_10)
			bmcr = 0;
		else if (speed == SPEED_100)
			bmcr = BMCR_SPEED100;
		else
			return -EINVAL;

		if (duplex == DUPLEX_FULL)
			bmcr |= BMCR_FULLDPLX;

		mdio_write(ioaddr, 0x1f, 0x0000);
	}

	tp->phy_1000_ctrl_reg = giga_ctrl;

	mdio_write(ioaddr, MII_BMCR, bmcr);

	if ((tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03)) {
		if ((speed == SPEED_100) && (autoneg != AUTONEG_ENABLE)) {
			mdio_write(ioaddr, 0x17, 0x2138);
			mdio_write(ioaddr, 0x0e, 0x0260);
		} else {
			mdio_write(ioaddr, 0x17, 0x2108);
			mdio_write(ioaddr, 0x0e, 0x0000);
		}
	}

	return 0;
}

static int rtl8169_set_speed(struct net_device *dev,
			     u8 autoneg, u16 speed, u8 duplex)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	int ret;

	ret = tp->set_speed(dev, autoneg, speed, duplex);

	if (netif_running(dev) && (tp->phy_1000_ctrl_reg & ADVERTISE_1000FULL))
		mod_timer(&tp->timer, jiffies + RTL8169_PHY_TIMEOUT);

	return ret;
}

static int rtl8169_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&tp->lock, flags);
	ret = rtl8169_set_speed(dev, cmd->autoneg, cmd->speed, cmd->duplex);
	spin_unlock_irqrestore(&tp->lock, flags);

	return ret;
}

static u32 rtl8169_get_rx_csum(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	return tp->cp_cmd & RxChkSum;
}

static int rtl8169_set_rx_csum(struct net_device *dev, u32 data)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);

	if (data)
		tp->cp_cmd |= RxChkSum;
	else
		tp->cp_cmd &= ~RxChkSum;

	RTL_W16(CPlusCmd, tp->cp_cmd);
	RTL_R16(CPlusCmd);

	spin_unlock_irqrestore(&tp->lock, flags);

	return 0;
}

#ifdef CONFIG_R8169_VLAN

static inline u32 rtl8169_tx_vlan_tag(struct rtl8169_private *tp,
				      struct sk_buff *skb)
{
	return (tp->vlgrp && vlan_tx_tag_present(skb)) ?
		TxVlanTag | swab16(vlan_tx_tag_get(skb)) : 0x00;
}

static void rtl8169_vlan_rx_register(struct net_device *dev,
				     struct vlan_group *grp)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);
	tp->vlgrp = grp;
	/*
	 * Do not disable RxVlan on 8110SCd.
	 */
	if (tp->vlgrp || (tp->mac_version == RTL_GIGA_MAC_VER_05))
		tp->cp_cmd |= RxVlan;
	else
		tp->cp_cmd &= ~RxVlan;
	RTL_W16(CPlusCmd, tp->cp_cmd);
	RTL_R16(CPlusCmd);
	spin_unlock_irqrestore(&tp->lock, flags);
}

static int rtl8169_rx_vlan_skb(struct rtl8169_private *tp, struct RxDesc *desc,
			       struct sk_buff *skb, int polling)
{
	u32 opts2 = le32_to_cpu(desc->opts2);
	struct vlan_group *vlgrp = tp->vlgrp;
	int ret;

	if (vlgrp && (opts2 & RxVlanTag)) {
		__vlan_hwaccel_rx(skb, vlgrp, swab16(opts2 & 0xffff), polling);
		ret = 0;
	} else
		ret = -1;
	desc->opts2 = 0;
	return ret;
}

#else /* !CONFIG_R8169_VLAN */

static inline u32 rtl8169_tx_vlan_tag(struct rtl8169_private *tp,
				      struct sk_buff *skb)
{
	return 0;
}

static int rtl8169_rx_vlan_skb(struct rtl8169_private *tp, struct RxDesc *desc,
			       struct sk_buff *skb, int polling)
{
	return -1;
}

#endif

static int rtl8169_gset_tbi(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u32 status;

	cmd->supported =
		SUPPORTED_1000baseT_Full | SUPPORTED_Autoneg | SUPPORTED_FIBRE;
	cmd->port = PORT_FIBRE;
	cmd->transceiver = XCVR_INTERNAL;

	status = RTL_R32(TBICSR);
	cmd->advertising = (status & TBINwEnable) ?  ADVERTISED_Autoneg : 0;
	cmd->autoneg = !!(status & TBINwEnable);

	cmd->speed = SPEED_1000;
	cmd->duplex = DUPLEX_FULL; /* Always set */

	return 0;
}

static int rtl8169_gset_xmii(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	return mii_ethtool_gset(&tp->mii, cmd);
}

static int rtl8169_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&tp->lock, flags);

	rc = tp->get_settings(dev, cmd);

	spin_unlock_irqrestore(&tp->lock, flags);
	return rc;
}

static void rtl8169_get_regs(struct net_device *dev, struct ethtool_regs *regs,
			     void *p)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned long flags;

	if (regs->len > R8169_REGS_SIZE)
		regs->len = R8169_REGS_SIZE;

	spin_lock_irqsave(&tp->lock, flags);
	memcpy_fromio(p, tp->mmio_addr, regs->len);
	spin_unlock_irqrestore(&tp->lock, flags);
}

static u32 rtl8169_get_msglevel(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	return tp->msg_enable;
}

static void rtl8169_set_msglevel(struct net_device *dev, u32 value)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	tp->msg_enable = value;
}

static const char rtl8169_gstrings[][ETH_GSTRING_LEN] = {
	"tx_packets",
	"rx_packets",
	"tx_errors",
	"rx_errors",
	"rx_missed",
	"align_errors",
	"tx_single_collisions",
	"tx_multi_collisions",
	"unicast",
	"broadcast",
	"multicast",
	"tx_aborted",
	"tx_underrun",
};

static int rtl8169_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(rtl8169_gstrings);
	default:
		return -EOPNOTSUPP;
	}
}

static void rtl8169_update_counters(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct rtl8169_counters *counters;
	dma_addr_t paddr;
	u32 cmd;
	int wait = 1000;

	/*
	 * Some chips are unable to dump tally counters when the receiver
	 * is disabled.
	 */
	if ((RTL_R8(ChipCmd) & CmdRxEnb) == 0)
		return;

	counters = dma_alloc_coherent(&tp->pci_dev->dev, sizeof(*counters),
				      &paddr, GFP_KERNEL);
	if (!counters)
		return;

	RTL_W32(CounterAddrHigh, (u64)paddr >> 32);
	cmd = (u64)paddr & DMA_BIT_MASK(32);
	RTL_W32(CounterAddrLow, cmd);
	RTL_W32(CounterAddrLow, cmd | CounterDump);

	while (wait--) {
		if ((RTL_R32(CounterAddrLow) & CounterDump) == 0) {
			/* copy updated counters */
			memcpy(&tp->counters, counters, sizeof(*counters));
			break;
		}
		udelay(10);
	}

	RTL_W32(CounterAddrLow, 0);
	RTL_W32(CounterAddrHigh, 0);

	dma_free_coherent(&tp->pci_dev->dev, sizeof(*counters), counters,
			  paddr);
}

static void rtl8169_get_ethtool_stats(struct net_device *dev,
				      struct ethtool_stats *stats, u64 *data)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	ASSERT_RTNL();

	rtl8169_update_counters(dev);

	data[0] = le64_to_cpu(tp->counters.tx_packets);
	data[1] = le64_to_cpu(tp->counters.rx_packets);
	data[2] = le64_to_cpu(tp->counters.tx_errors);
	data[3] = le32_to_cpu(tp->counters.rx_errors);
	data[4] = le16_to_cpu(tp->counters.rx_missed);
	data[5] = le16_to_cpu(tp->counters.align_errors);
	data[6] = le32_to_cpu(tp->counters.tx_one_collision);
	data[7] = le32_to_cpu(tp->counters.tx_multi_collision);
	data[8] = le64_to_cpu(tp->counters.rx_unicast);
	data[9] = le64_to_cpu(tp->counters.rx_broadcast);
	data[10] = le32_to_cpu(tp->counters.rx_multicast);
	data[11] = le16_to_cpu(tp->counters.tx_aborted);
	data[12] = le16_to_cpu(tp->counters.tx_underun);
}

static void rtl8169_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	switch(stringset) {
	case ETH_SS_STATS:
		memcpy(data, *rtl8169_gstrings, sizeof(rtl8169_gstrings));
		break;
	}
}

static const struct ethtool_ops rtl8169_ethtool_ops = {
	.get_drvinfo		= rtl8169_get_drvinfo,
	.get_regs_len		= rtl8169_get_regs_len,
	.get_link		= ethtool_op_get_link,
	.get_settings		= rtl8169_get_settings,
	.set_settings		= rtl8169_set_settings,
	.get_msglevel		= rtl8169_get_msglevel,
	.set_msglevel		= rtl8169_set_msglevel,
	.get_rx_csum		= rtl8169_get_rx_csum,
	.set_rx_csum		= rtl8169_set_rx_csum,
	.set_tx_csum		= ethtool_op_set_tx_csum,
	.set_sg			= ethtool_op_set_sg,
	.set_tso		= ethtool_op_set_tso,
	.get_regs		= rtl8169_get_regs,
	.get_wol		= rtl8169_get_wol,
	.set_wol		= rtl8169_set_wol,
	.get_strings		= rtl8169_get_strings,
	.get_sset_count		= rtl8169_get_sset_count,
	.get_ethtool_stats	= rtl8169_get_ethtool_stats,
};

static void rtl8169_get_mac_version(struct rtl8169_private *tp,
				    void __iomem *ioaddr)
{
	/*
	 * The driver currently handles the 8168Bf and the 8168Be identically
	 * but they can be identified more specifically through the test below
	 * if needed:
	 *
	 * (RTL_R32(TxConfig) & 0x700000) == 0x500000 ? 8168Bf : 8168Be
	 *
	 * Same thing for the 8101Eb and the 8101Ec:
	 *
	 * (RTL_R32(TxConfig) & 0x700000) == 0x200000 ? 8101Eb : 8101Ec
	 */
	static const struct {
		u32 mask;
		u32 val;
		int mac_version;
	} mac_info[] = {
		/* 8168D family. */
		{ 0x7cf00000, 0x28300000,	RTL_GIGA_MAC_VER_26 },
		{ 0x7cf00000, 0x28100000,	RTL_GIGA_MAC_VER_25 },
		{ 0x7c800000, 0x28800000,	RTL_GIGA_MAC_VER_27 },
		{ 0x7c800000, 0x28000000,	RTL_GIGA_MAC_VER_26 },

		/* 8168C family. */
		{ 0x7cf00000, 0x3cb00000,	RTL_GIGA_MAC_VER_24 },
		{ 0x7cf00000, 0x3c900000,	RTL_GIGA_MAC_VER_23 },
		{ 0x7cf00000, 0x3c800000,	RTL_GIGA_MAC_VER_18 },
		{ 0x7c800000, 0x3c800000,	RTL_GIGA_MAC_VER_24 },
		{ 0x7cf00000, 0x3c000000,	RTL_GIGA_MAC_VER_19 },
		{ 0x7cf00000, 0x3c200000,	RTL_GIGA_MAC_VER_20 },
		{ 0x7cf00000, 0x3c300000,	RTL_GIGA_MAC_VER_21 },
		{ 0x7cf00000, 0x3c400000,	RTL_GIGA_MAC_VER_22 },
		{ 0x7c800000, 0x3c000000,	RTL_GIGA_MAC_VER_22 },

		/* 8168B family. */
		{ 0x7cf00000, 0x38000000,	RTL_GIGA_MAC_VER_12 },
		{ 0x7cf00000, 0x38500000,	RTL_GIGA_MAC_VER_17 },
		{ 0x7c800000, 0x38000000,	RTL_GIGA_MAC_VER_17 },
		{ 0x7c800000, 0x30000000,	RTL_GIGA_MAC_VER_11 },

		/* 8101 family. */
		{ 0x7cf00000, 0x34a00000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7cf00000, 0x24a00000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7cf00000, 0x34900000,	RTL_GIGA_MAC_VER_08 },
		{ 0x7cf00000, 0x24900000,	RTL_GIGA_MAC_VER_08 },
		{ 0x7cf00000, 0x34800000,	RTL_GIGA_MAC_VER_07 },
		{ 0x7cf00000, 0x24800000,	RTL_GIGA_MAC_VER_07 },
		{ 0x7cf00000, 0x34000000,	RTL_GIGA_MAC_VER_13 },
		{ 0x7cf00000, 0x34300000,	RTL_GIGA_MAC_VER_10 },
		{ 0x7cf00000, 0x34200000,	RTL_GIGA_MAC_VER_16 },
		{ 0x7c800000, 0x34800000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7c800000, 0x24800000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7c800000, 0x34000000,	RTL_GIGA_MAC_VER_16 },
		{ 0xfc800000, 0x38800000,	RTL_GIGA_MAC_VER_15 },
		{ 0xfc800000, 0x30800000,	RTL_GIGA_MAC_VER_14 },

		/* 8110 family. */
		{ 0xfc800000, 0x98000000,	RTL_GIGA_MAC_VER_06 },
		{ 0xfc800000, 0x18000000,	RTL_GIGA_MAC_VER_05 },
		{ 0xfc800000, 0x10000000,	RTL_GIGA_MAC_VER_04 },
		{ 0xfc800000, 0x04000000,	RTL_GIGA_MAC_VER_03 },
		{ 0xfc800000, 0x00800000,	RTL_GIGA_MAC_VER_02 },
		{ 0xfc800000, 0x00000000,	RTL_GIGA_MAC_VER_01 },

		/* Catch-all */
		{ 0x00000000, 0x00000000,	RTL_GIGA_MAC_NONE   }
	}, *p = mac_info;
	u32 reg;

	reg = RTL_R32(TxConfig);
	while ((reg & p->mask) != p->val)
		p++;
	tp->mac_version = p->mac_version;
}

static void rtl8169_print_mac_version(struct rtl8169_private *tp)
{
	dprintk("mac_version = 0x%02x\n", tp->mac_version);
}

struct phy_reg {
	u16 reg;
	u16 val;
};

static void rtl_phy_write(void __iomem *ioaddr, const struct phy_reg *regs, int len)
{
	while (len-- > 0) {
		mdio_write(ioaddr, regs->reg, regs->val);
		regs++;
	}
}

static void rtl8169s_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x06, 0x006e },
		{ 0x08, 0x0708 },
		{ 0x15, 0x4000 },
		{ 0x18, 0x65c7 },

		{ 0x1f, 0x0001 },
		{ 0x03, 0x00a1 },
		{ 0x02, 0x0008 },
		{ 0x01, 0x0120 },
		{ 0x00, 0x1000 },
		{ 0x04, 0x0800 },
		{ 0x04, 0x0000 },

		{ 0x03, 0xff41 },
		{ 0x02, 0xdf60 },
		{ 0x01, 0x0140 },
		{ 0x00, 0x0077 },
		{ 0x04, 0x7800 },
		{ 0x04, 0x7000 },

		{ 0x03, 0x802f },
		{ 0x02, 0x4f02 },
		{ 0x01, 0x0409 },
		{ 0x00, 0xf0f9 },
		{ 0x04, 0x9800 },
		{ 0x04, 0x9000 },

		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0xff95 },
		{ 0x00, 0xba00 },
		{ 0x04, 0xa800 },
		{ 0x04, 0xa000 },

		{ 0x03, 0xff41 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x0140 },
		{ 0x00, 0x00bb },
		{ 0x04, 0xb800 },
		{ 0x04, 0xb000 },

		{ 0x03, 0xdf41 },
		{ 0x02, 0xdc60 },
		{ 0x01, 0x6340 },
		{ 0x00, 0x007d },
		{ 0x04, 0xd800 },
		{ 0x04, 0xd000 },

		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x100a },
		{ 0x00, 0xa0ff },
		{ 0x04, 0xf800 },
		{ 0x04, 0xf000 },

		{ 0x1f, 0x0000 },
		{ 0x0b, 0x0000 },
		{ 0x00, 0x9200 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8169sb_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0002 },
		{ 0x01, 0x90d0 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8169scd_hw_phy_config_quirk(struct rtl8169_private *tp,
					   void __iomem *ioaddr)
{
	struct pci_dev *pdev = tp->pci_dev;
	u16 vendor_id, device_id;

	pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &vendor_id);
	pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, &device_id);

	if ((vendor_id != PCI_VENDOR_ID_GIGABYTE) || (device_id != 0xe000))
		return;

	mdio_write(ioaddr, 0x1f, 0x0001);
	mdio_write(ioaddr, 0x10, 0xf01b);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8169scd_hw_phy_config(struct rtl8169_private *tp,
				     void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x04, 0x0000 },
		{ 0x03, 0x00a1 },
		{ 0x02, 0x0008 },
		{ 0x01, 0x0120 },
		{ 0x00, 0x1000 },
		{ 0x04, 0x0800 },
		{ 0x04, 0x9000 },
		{ 0x03, 0x802f },
		{ 0x02, 0x4f02 },
		{ 0x01, 0x0409 },
		{ 0x00, 0xf099 },
		{ 0x04, 0x9800 },
		{ 0x04, 0xa000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0xff95 },
		{ 0x00, 0xba00 },
		{ 0x04, 0xa800 },
		{ 0x04, 0xf000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x101a },
		{ 0x00, 0xa0ff },
		{ 0x04, 0xf800 },
		{ 0x04, 0x0000 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x10, 0xf41b },
		{ 0x14, 0xfb54 },
		{ 0x18, 0xf5c7 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x17, 0x0cc0 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	rtl8169scd_hw_phy_config_quirk(tp, ioaddr);
}

static void rtl8169sce_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x04, 0x0000 },
		{ 0x03, 0x00a1 },
		{ 0x02, 0x0008 },
		{ 0x01, 0x0120 },
		{ 0x00, 0x1000 },
		{ 0x04, 0x0800 },
		{ 0x04, 0x9000 },
		{ 0x03, 0x802f },
		{ 0x02, 0x4f02 },
		{ 0x01, 0x0409 },
		{ 0x00, 0xf099 },
		{ 0x04, 0x9800 },
		{ 0x04, 0xa000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0xff95 },
		{ 0x00, 0xba00 },
		{ 0x04, 0xa800 },
		{ 0x04, 0xf000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x101a },
		{ 0x00, 0xa0ff },
		{ 0x04, 0xf800 },
		{ 0x04, 0x0000 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x0b, 0x8480 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x18, 0x67c7 },
		{ 0x04, 0x2000 },
		{ 0x03, 0x002f },
		{ 0x02, 0x4360 },
		{ 0x01, 0x0109 },
		{ 0x00, 0x3022 },
		{ 0x04, 0x2800 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x17, 0x0cc0 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168bb_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x10, 0xf41b },
		{ 0x1f, 0x0000 }
	};

	mdio_write(ioaddr, 0x1f, 0x0001);
	mdio_patch(ioaddr, 0x16, 1 << 0);

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168bef_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x10, 0xf41b },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168cp_1_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0000 },
		{ 0x1d, 0x0f00 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x1ec8 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168cp_2_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x1d, 0x3d98 },
		{ 0x1f, 0x0000 }
	};

	mdio_write(ioaddr, 0x1f, 0x0000);
	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168c_1_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x1f, 0x0002 },
		{ 0x00, 0x88d4 },
		{ 0x01, 0x82b1 },
		{ 0x03, 0x7002 },
		{ 0x08, 0x9e30 },
		{ 0x09, 0x01f0 },
		{ 0x0a, 0x5500 },
		{ 0x0c, 0x00c8 },
		{ 0x1f, 0x0003 },
		{ 0x12, 0xc096 },
		{ 0x16, 0x000a },
		{ 0x1f, 0x0000 },
		{ 0x1f, 0x0000 },
		{ 0x09, 0x2000 },
		{ 0x09, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8168c_2_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x03, 0x802f },
		{ 0x02, 0x4f02 },
		{ 0x01, 0x0409 },
		{ 0x00, 0xf099 },
		{ 0x04, 0x9800 },
		{ 0x04, 0x9000 },
		{ 0x1d, 0x3d98 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x7eb8 },
		{ 0x06, 0x0761 },
		{ 0x1f, 0x0003 },
		{ 0x16, 0x0f0a },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	mdio_patch(ioaddr, 0x16, 1 << 0);
	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8168c_3_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x1d, 0x3d98 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x7eb8 },
		{ 0x06, 0x5461 },
		{ 0x1f, 0x0003 },
		{ 0x16, 0x0f0a },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	mdio_patch(ioaddr, 0x16, 1 << 0);
	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8168c_4_hw_phy_config(void __iomem *ioaddr)
{
	rtl8168c_3_hw_phy_config(ioaddr);
}

static void rtl8168d_1_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init_0[] = {
		{ 0x1f, 0x0001 },
		{ 0x06, 0x4064 },
		{ 0x07, 0x2863 },
		{ 0x08, 0x059c },
		{ 0x09, 0x26b4 },
		{ 0x0a, 0x6a19 },
		{ 0x0b, 0xdcc8 },
		{ 0x10, 0xf06d },
		{ 0x14, 0x7f68 },
		{ 0x18, 0x7fd9 },
		{ 0x1c, 0xf0ff },
		{ 0x1d, 0x3d9c },
		{ 0x1f, 0x0003 },
		{ 0x12, 0xf49f },
		{ 0x13, 0x070b },
		{ 0x1a, 0x05ad },
		{ 0x14, 0x94c0 }
	};
	static const struct phy_reg phy_reg_init_1[] = {
		{ 0x1f, 0x0002 },
		{ 0x06, 0x5561 },
		{ 0x1f, 0x0005 },
		{ 0x05, 0x8332 },
		{ 0x06, 0x5561 }
	};
	static const struct phy_reg phy_reg_init_2[] = {
		{ 0x1f, 0x0005 },
		{ 0x05, 0xffc2 },
		{ 0x1f, 0x0005 },
		{ 0x05, 0x8000 },
		{ 0x06, 0xf8f9 },
		{ 0x06, 0xfaef },
		{ 0x06, 0x59ee },
		{ 0x06, 0xf8ea },
		{ 0x06, 0x00ee },
		{ 0x06, 0xf8eb },
		{ 0x06, 0x00e0 },
		{ 0x06, 0xf87c },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x7d59 },
		{ 0x06, 0x0fef },
		{ 0x06, 0x0139 },
		{ 0x06, 0x029e },
		{ 0x06, 0x06ef },
		{ 0x06, 0x1039 },
		{ 0x06, 0x089f },
		{ 0x06, 0x2aee },
		{ 0x06, 0xf8ea },
		{ 0x06, 0x00ee },
		{ 0x06, 0xf8eb },
		{ 0x06, 0x01e0 },
		{ 0x06, 0xf87c },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x7d58 },
		{ 0x06, 0x409e },
		{ 0x06, 0x0f39 },
		{ 0x06, 0x46aa },
		{ 0x06, 0x0bbf },
		{ 0x06, 0x8290 },
		{ 0x06, 0xd682 },
		{ 0x06, 0x9802 },
		{ 0x06, 0x014f },
		{ 0x06, 0xae09 },
		{ 0x06, 0xbf82 },
		{ 0x06, 0x98d6 },
		{ 0x06, 0x82a0 },
		{ 0x06, 0x0201 },
		{ 0x06, 0x4fef },
		{ 0x06, 0x95fe },
		{ 0x06, 0xfdfc },
		{ 0x06, 0x05f8 },
		{ 0x06, 0xf9fa },
		{ 0x06, 0xeef8 },
		{ 0x06, 0xea00 },
		{ 0x06, 0xeef8 },
		{ 0x06, 0xeb00 },
		{ 0x06, 0xe2f8 },
		{ 0x06, 0x7ce3 },
		{ 0x06, 0xf87d },
		{ 0x06, 0xa511 },
		{ 0x06, 0x1112 },
		{ 0x06, 0xd240 },
		{ 0x06, 0xd644 },
		{ 0x06, 0x4402 },
		{ 0x06, 0x8217 },
		{ 0x06, 0xd2a0 },
		{ 0x06, 0xd6aa },
		{ 0x06, 0xaa02 },
		{ 0x06, 0x8217 },
		{ 0x06, 0xae0f },
		{ 0x06, 0xa544 },
		{ 0x06, 0x4402 },
		{ 0x06, 0xae4d },
		{ 0x06, 0xa5aa },
		{ 0x06, 0xaa02 },
		{ 0x06, 0xae47 },
		{ 0x06, 0xaf82 },
		{ 0x06, 0x13ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x00ee },
		{ 0x06, 0x834d },
		{ 0x06, 0x0fee },
		{ 0x06, 0x834c },
		{ 0x06, 0x0fee },
		{ 0x06, 0x834f },
		{ 0x06, 0x00ee },
		{ 0x06, 0x8351 },
		{ 0x06, 0x00ee },
		{ 0x06, 0x834a },
		{ 0x06, 0xffee },
		{ 0x06, 0x834b },
		{ 0x06, 0xffe0 },
		{ 0x06, 0x8330 },
		{ 0x06, 0xe183 },
		{ 0x06, 0x3158 },
		{ 0x06, 0xfee4 },
		{ 0x06, 0xf88a },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x8be0 },
		{ 0x06, 0x8332 },
		{ 0x06, 0xe183 },
		{ 0x06, 0x3359 },
		{ 0x06, 0x0fe2 },
		{ 0x06, 0x834d },
		{ 0x06, 0x0c24 },
		{ 0x06, 0x5af0 },
		{ 0x06, 0x1e12 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x8ce5 },
		{ 0x06, 0xf88d },
		{ 0x06, 0xaf82 },
		{ 0x06, 0x13e0 },
		{ 0x06, 0x834f },
		{ 0x06, 0x10e4 },
		{ 0x06, 0x834f },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x009f },
		{ 0x06, 0x0ae0 },
		{ 0x06, 0x834f },
		{ 0x06, 0xa010 },
		{ 0x06, 0xa5ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x01e0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7805 },
		{ 0x06, 0x9e9a },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x049e },
		{ 0x06, 0x10e0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7803 },
		{ 0x06, 0x9e0f },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x019e },
		{ 0x06, 0x05ae },
		{ 0x06, 0x0caf },
		{ 0x06, 0x81f8 },
		{ 0x06, 0xaf81 },
		{ 0x06, 0xa3af },
		{ 0x06, 0x81dc },
		{ 0x06, 0xaf82 },
		{ 0x06, 0x13ee },
		{ 0x06, 0x8348 },
		{ 0x06, 0x00ee },
		{ 0x06, 0x8349 },
		{ 0x06, 0x00e0 },
		{ 0x06, 0x8351 },
		{ 0x06, 0x10e4 },
		{ 0x06, 0x8351 },
		{ 0x06, 0x5801 },
		{ 0x06, 0x9fea },
		{ 0x06, 0xd000 },
		{ 0x06, 0xd180 },
		{ 0x06, 0x1f66 },
		{ 0x06, 0xe2f8 },
		{ 0x06, 0xeae3 },
		{ 0x06, 0xf8eb },
		{ 0x06, 0x5af8 },
		{ 0x06, 0x1e20 },
		{ 0x06, 0xe6f8 },
		{ 0x06, 0xeae5 },
		{ 0x06, 0xf8eb },
		{ 0x06, 0xd302 },
		{ 0x06, 0xb3fe },
		{ 0x06, 0xe2f8 },
		{ 0x06, 0x7cef },
		{ 0x06, 0x325b },
		{ 0x06, 0x80e3 },
		{ 0x06, 0xf87d },
		{ 0x06, 0x9e03 },
		{ 0x06, 0x7dff },
		{ 0x06, 0xff0d },
		{ 0x06, 0x581c },
		{ 0x06, 0x551a },
		{ 0x06, 0x6511 },
		{ 0x06, 0xa190 },
		{ 0x06, 0xd3e2 },
		{ 0x06, 0x8348 },
		{ 0x06, 0xe383 },
		{ 0x06, 0x491b },
		{ 0x06, 0x56ab },
		{ 0x06, 0x08ef },
		{ 0x06, 0x56e6 },
		{ 0x06, 0x8348 },
		{ 0x06, 0xe783 },
		{ 0x06, 0x4910 },
		{ 0x06, 0xd180 },
		{ 0x06, 0x1f66 },
		{ 0x06, 0xa004 },
		{ 0x06, 0xb9e2 },
		{ 0x06, 0x8348 },
		{ 0x06, 0xe383 },
		{ 0x06, 0x49ef },
		{ 0x06, 0x65e2 },
		{ 0x06, 0x834a },
		{ 0x06, 0xe383 },
		{ 0x06, 0x4b1b },
		{ 0x06, 0x56aa },
		{ 0x06, 0x0eef },
		{ 0x06, 0x56e6 },
		{ 0x06, 0x834a },
		{ 0x06, 0xe783 },
		{ 0x06, 0x4be2 },
		{ 0x06, 0x834d },
		{ 0x06, 0xe683 },
		{ 0x06, 0x4ce0 },
		{ 0x06, 0x834d },
		{ 0x06, 0xa000 },
		{ 0x06, 0x0caf },
		{ 0x06, 0x81dc },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4d10 },
		{ 0x06, 0xe483 },
		{ 0x06, 0x4dae },
		{ 0x06, 0x0480 },
		{ 0x06, 0xe483 },
		{ 0x06, 0x4de0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7803 },
		{ 0x06, 0x9e0b },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x049e },
		{ 0x06, 0x04ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x02e0 },
		{ 0x06, 0x8332 },
		{ 0x06, 0xe183 },
		{ 0x06, 0x3359 },
		{ 0x06, 0x0fe2 },
		{ 0x06, 0x834d },
		{ 0x06, 0x0c24 },
		{ 0x06, 0x5af0 },
		{ 0x06, 0x1e12 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x8ce5 },
		{ 0x06, 0xf88d },
		{ 0x06, 0xe083 },
		{ 0x06, 0x30e1 },
		{ 0x06, 0x8331 },
		{ 0x06, 0x6801 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x8ae5 },
		{ 0x06, 0xf88b },
		{ 0x06, 0xae37 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e03 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4ce1 },
		{ 0x06, 0x834d },
		{ 0x06, 0x1b01 },
		{ 0x06, 0x9e04 },
		{ 0x06, 0xaaa1 },
		{ 0x06, 0xaea8 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e04 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4f00 },
		{ 0x06, 0xaeab },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4f78 },
		{ 0x06, 0x039f },
		{ 0x06, 0x14ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x05d2 },
		{ 0x06, 0x40d6 },
		{ 0x06, 0x5554 },
		{ 0x06, 0x0282 },
		{ 0x06, 0x17d2 },
		{ 0x06, 0xa0d6 },
		{ 0x06, 0xba00 },
		{ 0x06, 0x0282 },
		{ 0x06, 0x17fe },
		{ 0x06, 0xfdfc },
		{ 0x06, 0x05f8 },
		{ 0x06, 0xe0f8 },
		{ 0x06, 0x60e1 },
		{ 0x06, 0xf861 },
		{ 0x06, 0x6802 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x60e5 },
		{ 0x06, 0xf861 },
		{ 0x06, 0xe0f8 },
		{ 0x06, 0x48e1 },
		{ 0x06, 0xf849 },
		{ 0x06, 0x580f },
		{ 0x06, 0x1e02 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x48e5 },
		{ 0x06, 0xf849 },
		{ 0x06, 0xd000 },
		{ 0x06, 0x0282 },
		{ 0x06, 0x5bbf },
		{ 0x06, 0x8350 },
		{ 0x06, 0xef46 },
		{ 0x06, 0xdc19 },
		{ 0x06, 0xddd0 },
		{ 0x06, 0x0102 },
		{ 0x06, 0x825b },
		{ 0x06, 0x0282 },
		{ 0x06, 0x77e0 },
		{ 0x06, 0xf860 },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x6158 },
		{ 0x06, 0xfde4 },
		{ 0x06, 0xf860 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x61fc },
		{ 0x06, 0x04f9 },
		{ 0x06, 0xfafb },
		{ 0x06, 0xc6bf },
		{ 0x06, 0xf840 },
		{ 0x06, 0xbe83 },
		{ 0x06, 0x50a0 },
		{ 0x06, 0x0101 },
		{ 0x06, 0x071b },
		{ 0x06, 0x89cf },
		{ 0x06, 0xd208 },
		{ 0x06, 0xebdb },
		{ 0x06, 0x19b2 },
		{ 0x06, 0xfbff },
		{ 0x06, 0xfefd },
		{ 0x06, 0x04f8 },
		{ 0x06, 0xe0f8 },
		{ 0x06, 0x48e1 },
		{ 0x06, 0xf849 },
		{ 0x06, 0x6808 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x48e5 },
		{ 0x06, 0xf849 },
		{ 0x06, 0x58f7 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x48e5 },
		{ 0x06, 0xf849 },
		{ 0x06, 0xfc04 },
		{ 0x06, 0x4d20 },
		{ 0x06, 0x0002 },
		{ 0x06, 0x4e22 },
		{ 0x06, 0x0002 },
		{ 0x06, 0x4ddf },
		{ 0x06, 0xff01 },
		{ 0x06, 0x4edd },
		{ 0x06, 0xff01 },
		{ 0x05, 0x83d4 },
		{ 0x06, 0x8000 },
		{ 0x05, 0x83d8 },
		{ 0x06, 0x8051 },
		{ 0x02, 0x6010 },
		{ 0x03, 0xdc00 },
		{ 0x05, 0xfff6 },
		{ 0x06, 0x00fc },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0000 },
		{ 0x0d, 0xf880 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init_0, ARRAY_SIZE(phy_reg_init_0));

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_plus_minus(ioaddr, 0x0b, 0x0010, 0x00ef);
	mdio_plus_minus(ioaddr, 0x0c, 0xa200, 0x5d00);

	rtl_phy_write(ioaddr, phy_reg_init_1, ARRAY_SIZE(phy_reg_init_1));

	if (rtl8168d_efuse_read(ioaddr, 0x01) == 0xb1) {
		static const struct phy_reg phy_reg_init[] = {
			{ 0x1f, 0x0002 },
			{ 0x05, 0x669a },
			{ 0x1f, 0x0005 },
			{ 0x05, 0x8330 },
			{ 0x06, 0x669a },
			{ 0x1f, 0x0002 }
		};
		int val;

		rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

		val = mdio_read(ioaddr, 0x0d);

		if ((val & 0x00ff) != 0x006c) {
			static const u32 set[] = {
				0x0065, 0x0066, 0x0067, 0x0068,
				0x0069, 0x006a, 0x006b, 0x006c
			};
			int i;

			mdio_write(ioaddr, 0x1f, 0x0002);

			val &= 0xff00;
			for (i = 0; i < ARRAY_SIZE(set); i++)
				mdio_write(ioaddr, 0x0d, val | set[i]);
		}
	} else {
		static const struct phy_reg phy_reg_init[] = {
			{ 0x1f, 0x0002 },
			{ 0x05, 0x6662 },
			{ 0x1f, 0x0005 },
			{ 0x05, 0x8330 },
			{ 0x06, 0x6662 }
		};

		rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
	}

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_patch(ioaddr, 0x0d, 0x0300);
	mdio_patch(ioaddr, 0x0f, 0x0010);

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_plus_minus(ioaddr, 0x02, 0x0100, 0x0600);
	mdio_plus_minus(ioaddr, 0x03, 0x0000, 0xe000);

	rtl_phy_write(ioaddr, phy_reg_init_2, ARRAY_SIZE(phy_reg_init_2));
}

static void rtl8168d_2_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init_0[] = {
		{ 0x1f, 0x0001 },
		{ 0x06, 0x4064 },
		{ 0x07, 0x2863 },
		{ 0x08, 0x059c },
		{ 0x09, 0x26b4 },
		{ 0x0a, 0x6a19 },
		{ 0x0b, 0xdcc8 },
		{ 0x10, 0xf06d },
		{ 0x14, 0x7f68 },
		{ 0x18, 0x7fd9 },
		{ 0x1c, 0xf0ff },
		{ 0x1d, 0x3d9c },
		{ 0x1f, 0x0003 },
		{ 0x12, 0xf49f },
		{ 0x13, 0x070b },
		{ 0x1a, 0x05ad },
		{ 0x14, 0x94c0 },

		{ 0x1f, 0x0002 },
		{ 0x06, 0x5561 },
		{ 0x1f, 0x0005 },
		{ 0x05, 0x8332 },
		{ 0x06, 0x5561 }
	};
	static const struct phy_reg phy_reg_init_1[] = {
		{ 0x1f, 0x0005 },
		{ 0x05, 0xffc2 },
		{ 0x1f, 0x0005 },
		{ 0x05, 0x8000 },
		{ 0x06, 0xf8f9 },
		{ 0x06, 0xfaee },
		{ 0x06, 0xf8ea },
		{ 0x06, 0x00ee },
		{ 0x06, 0xf8eb },
		{ 0x06, 0x00e2 },
		{ 0x06, 0xf87c },
		{ 0x06, 0xe3f8 },
		{ 0x06, 0x7da5 },
		{ 0x06, 0x1111 },
		{ 0x06, 0x12d2 },
		{ 0x06, 0x40d6 },
		{ 0x06, 0x4444 },
		{ 0x06, 0x0281 },
		{ 0x06, 0xc6d2 },
		{ 0x06, 0xa0d6 },
		{ 0x06, 0xaaaa },
		{ 0x06, 0x0281 },
		{ 0x06, 0xc6ae },
		{ 0x06, 0x0fa5 },
		{ 0x06, 0x4444 },
		{ 0x06, 0x02ae },
		{ 0x06, 0x4da5 },
		{ 0x06, 0xaaaa },
		{ 0x06, 0x02ae },
		{ 0x06, 0x47af },
		{ 0x06, 0x81c2 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e00 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4d0f },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4c0f },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4f00 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x5100 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4aff },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4bff },
		{ 0x06, 0xe083 },
		{ 0x06, 0x30e1 },
		{ 0x06, 0x8331 },
		{ 0x06, 0x58fe },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x8ae5 },
		{ 0x06, 0xf88b },
		{ 0x06, 0xe083 },
		{ 0x06, 0x32e1 },
		{ 0x06, 0x8333 },
		{ 0x06, 0x590f },
		{ 0x06, 0xe283 },
		{ 0x06, 0x4d0c },
		{ 0x06, 0x245a },
		{ 0x06, 0xf01e },
		{ 0x06, 0x12e4 },
		{ 0x06, 0xf88c },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x8daf },
		{ 0x06, 0x81c2 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4f10 },
		{ 0x06, 0xe483 },
		{ 0x06, 0x4fe0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7800 },
		{ 0x06, 0x9f0a },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4fa0 },
		{ 0x06, 0x10a5 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e01 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x059e },
		{ 0x06, 0x9ae0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7804 },
		{ 0x06, 0x9e10 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x039e },
		{ 0x06, 0x0fe0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7801 },
		{ 0x06, 0x9e05 },
		{ 0x06, 0xae0c },
		{ 0x06, 0xaf81 },
		{ 0x06, 0xa7af },
		{ 0x06, 0x8152 },
		{ 0x06, 0xaf81 },
		{ 0x06, 0x8baf },
		{ 0x06, 0x81c2 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4800 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4900 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x5110 },
		{ 0x06, 0xe483 },
		{ 0x06, 0x5158 },
		{ 0x06, 0x019f },
		{ 0x06, 0xead0 },
		{ 0x06, 0x00d1 },
		{ 0x06, 0x801f },
		{ 0x06, 0x66e2 },
		{ 0x06, 0xf8ea },
		{ 0x06, 0xe3f8 },
		{ 0x06, 0xeb5a },
		{ 0x06, 0xf81e },
		{ 0x06, 0x20e6 },
		{ 0x06, 0xf8ea },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0xebd3 },
		{ 0x06, 0x02b3 },
		{ 0x06, 0xfee2 },
		{ 0x06, 0xf87c },
		{ 0x06, 0xef32 },
		{ 0x06, 0x5b80 },
		{ 0x06, 0xe3f8 },
		{ 0x06, 0x7d9e },
		{ 0x06, 0x037d },
		{ 0x06, 0xffff },
		{ 0x06, 0x0d58 },
		{ 0x06, 0x1c55 },
		{ 0x06, 0x1a65 },
		{ 0x06, 0x11a1 },
		{ 0x06, 0x90d3 },
		{ 0x06, 0xe283 },
		{ 0x06, 0x48e3 },
		{ 0x06, 0x8349 },
		{ 0x06, 0x1b56 },
		{ 0x06, 0xab08 },
		{ 0x06, 0xef56 },
		{ 0x06, 0xe683 },
		{ 0x06, 0x48e7 },
		{ 0x06, 0x8349 },
		{ 0x06, 0x10d1 },
		{ 0x06, 0x801f },
		{ 0x06, 0x66a0 },
		{ 0x06, 0x04b9 },
		{ 0x06, 0xe283 },
		{ 0x06, 0x48e3 },
		{ 0x06, 0x8349 },
		{ 0x06, 0xef65 },
		{ 0x06, 0xe283 },
		{ 0x06, 0x4ae3 },
		{ 0x06, 0x834b },
		{ 0x06, 0x1b56 },
		{ 0x06, 0xaa0e },
		{ 0x06, 0xef56 },
		{ 0x06, 0xe683 },
		{ 0x06, 0x4ae7 },
		{ 0x06, 0x834b },
		{ 0x06, 0xe283 },
		{ 0x06, 0x4de6 },
		{ 0x06, 0x834c },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4da0 },
		{ 0x06, 0x000c },
		{ 0x06, 0xaf81 },
		{ 0x06, 0x8be0 },
		{ 0x06, 0x834d },
		{ 0x06, 0x10e4 },
		{ 0x06, 0x834d },
		{ 0x06, 0xae04 },
		{ 0x06, 0x80e4 },
		{ 0x06, 0x834d },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x039e },
		{ 0x06, 0x0be0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7804 },
		{ 0x06, 0x9e04 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e02 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x32e1 },
		{ 0x06, 0x8333 },
		{ 0x06, 0x590f },
		{ 0x06, 0xe283 },
		{ 0x06, 0x4d0c },
		{ 0x06, 0x245a },
		{ 0x06, 0xf01e },
		{ 0x06, 0x12e4 },
		{ 0x06, 0xf88c },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x8de0 },
		{ 0x06, 0x8330 },
		{ 0x06, 0xe183 },
		{ 0x06, 0x3168 },
		{ 0x06, 0x01e4 },
		{ 0x06, 0xf88a },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x8bae },
		{ 0x06, 0x37ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x03e0 },
		{ 0x06, 0x834c },
		{ 0x06, 0xe183 },
		{ 0x06, 0x4d1b },
		{ 0x06, 0x019e },
		{ 0x06, 0x04aa },
		{ 0x06, 0xa1ae },
		{ 0x06, 0xa8ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x04ee },
		{ 0x06, 0x834f },
		{ 0x06, 0x00ae },
		{ 0x06, 0xabe0 },
		{ 0x06, 0x834f },
		{ 0x06, 0x7803 },
		{ 0x06, 0x9f14 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e05 },
		{ 0x06, 0xd240 },
		{ 0x06, 0xd655 },
		{ 0x06, 0x5402 },
		{ 0x06, 0x81c6 },
		{ 0x06, 0xd2a0 },
		{ 0x06, 0xd6ba },
		{ 0x06, 0x0002 },
		{ 0x06, 0x81c6 },
		{ 0x06, 0xfefd },
		{ 0x06, 0xfc05 },
		{ 0x06, 0xf8e0 },
		{ 0x06, 0xf860 },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x6168 },
		{ 0x06, 0x02e4 },
		{ 0x06, 0xf860 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x61e0 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x4958 },
		{ 0x06, 0x0f1e },
		{ 0x06, 0x02e4 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x49d0 },
		{ 0x06, 0x0002 },
		{ 0x06, 0x820a },
		{ 0x06, 0xbf83 },
		{ 0x06, 0x50ef },
		{ 0x06, 0x46dc },
		{ 0x06, 0x19dd },
		{ 0x06, 0xd001 },
		{ 0x06, 0x0282 },
		{ 0x06, 0x0a02 },
		{ 0x06, 0x8226 },
		{ 0x06, 0xe0f8 },
		{ 0x06, 0x60e1 },
		{ 0x06, 0xf861 },
		{ 0x06, 0x58fd },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x60e5 },
		{ 0x06, 0xf861 },
		{ 0x06, 0xfc04 },
		{ 0x06, 0xf9fa },
		{ 0x06, 0xfbc6 },
		{ 0x06, 0xbff8 },
		{ 0x06, 0x40be },
		{ 0x06, 0x8350 },
		{ 0x06, 0xa001 },
		{ 0x06, 0x0107 },
		{ 0x06, 0x1b89 },
		{ 0x06, 0xcfd2 },
		{ 0x06, 0x08eb },
		{ 0x06, 0xdb19 },
		{ 0x06, 0xb2fb },
		{ 0x06, 0xfffe },
		{ 0x06, 0xfd04 },
		{ 0x06, 0xf8e0 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x4968 },
		{ 0x06, 0x08e4 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x4958 },
		{ 0x06, 0xf7e4 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x49fc },
		{ 0x06, 0x044d },
		{ 0x06, 0x2000 },
		{ 0x06, 0x024e },
		{ 0x06, 0x2200 },
		{ 0x06, 0x024d },
		{ 0x06, 0xdfff },
		{ 0x06, 0x014e },
		{ 0x06, 0xddff },
		{ 0x06, 0x0100 },
		{ 0x05, 0x83d8 },
		{ 0x06, 0x8000 },
		{ 0x03, 0xdc00 },
		{ 0x05, 0xfff6 },
		{ 0x06, 0x00fc },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0000 },
		{ 0x0d, 0xf880 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init_0, ARRAY_SIZE(phy_reg_init_0));

	if (rtl8168d_efuse_read(ioaddr, 0x01) == 0xb1) {
		static const struct phy_reg phy_reg_init[] = {
			{ 0x1f, 0x0002 },
			{ 0x05, 0x669a },
			{ 0x1f, 0x0005 },
			{ 0x05, 0x8330 },
			{ 0x06, 0x669a },

			{ 0x1f, 0x0002 }
		};
		int val;

		rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

		val = mdio_read(ioaddr, 0x0d);
		if ((val & 0x00ff) != 0x006c) {
			u32 set[] = {
				0x0065, 0x0066, 0x0067, 0x0068,
				0x0069, 0x006a, 0x006b, 0x006c
			};
			int i;

			mdio_write(ioaddr, 0x1f, 0x0002);

			val &= 0xff00;
			for (i = 0; i < ARRAY_SIZE(set); i++)
				mdio_write(ioaddr, 0x0d, val | set[i]);
		}
	} else {
		static const struct phy_reg phy_reg_init[] = {
			{ 0x1f, 0x0002 },
			{ 0x05, 0x2642 },
			{ 0x1f, 0x0005 },
			{ 0x05, 0x8330 },
			{ 0x06, 0x2642 }
		};

		rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
	}

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_plus_minus(ioaddr, 0x02, 0x0100, 0x0600);
	mdio_plus_minus(ioaddr, 0x03, 0x0000, 0xe000);

	mdio_write(ioaddr, 0x1f, 0x0001);
	mdio_write(ioaddr, 0x17, 0x0cc0);

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_patch(ioaddr, 0x0f, 0x0017);

	rtl_phy_write(ioaddr, phy_reg_init_1, ARRAY_SIZE(phy_reg_init_1));
}

static void rtl8168d_3_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0002 },
		{ 0x10, 0x0008 },
		{ 0x0d, 0x006c },

		{ 0x1f, 0x0000 },
		{ 0x0d, 0xf880 },

		{ 0x1f, 0x0001 },
		{ 0x17, 0x0cc0 },

		{ 0x1f, 0x0001 },
		{ 0x0b, 0xa4d8 },
		{ 0x09, 0x281c },
		{ 0x07, 0x2883 },
		{ 0x0a, 0x6b35 },
		{ 0x1d, 0x3da4 },
		{ 0x1c, 0xeffd },
		{ 0x14, 0x7f52 },
		{ 0x18, 0x7fc6 },
		{ 0x08, 0x0601 },
		{ 0x06, 0x4063 },
		{ 0x10, 0xf074 },
		{ 0x1f, 0x0003 },
		{ 0x13, 0x0789 },
		{ 0x12, 0xf4bd },
		{ 0x1a, 0x04fd },
		{ 0x14, 0x84b0 },
		{ 0x1f, 0x0000 },
		{ 0x00, 0x9200 },

		{ 0x1f, 0x0005 },
		{ 0x01, 0x0340 },
		{ 0x1f, 0x0001 },
		{ 0x04, 0x4000 },
		{ 0x03, 0x1d21 },
		{ 0x02, 0x0c32 },
		{ 0x01, 0x0200 },
		{ 0x00, 0x5554 },
		{ 0x04, 0x4800 },
		{ 0x04, 0x4000 },
		{ 0x04, 0xf000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x101a },
		{ 0x00, 0xa0ff },
		{ 0x04, 0xf800 },
		{ 0x04, 0xf000 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0007 },
		{ 0x1e, 0x0023 },
		{ 0x16, 0x0000 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8102e_hw_phy_config(void __iomem *ioaddr)
{
	static const struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0003 },
		{ 0x08, 0x441d },
		{ 0x01, 0x9100 },
		{ 0x1f, 0x0000 }
	};

	mdio_write(ioaddr, 0x1f, 0x0000);
	mdio_patch(ioaddr, 0x11, 1 << 12);
	mdio_patch(ioaddr, 0x19, 1 << 13);
	mdio_patch(ioaddr, 0x10, 1 << 15);

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl_hw_phy_config(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	rtl8169_print_mac_version(tp);

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_01:
		break;
	case RTL_GIGA_MAC_VER_02:
	case RTL_GIGA_MAC_VER_03:
		rtl8169s_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_04:
		rtl8169sb_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_05:
		rtl8169scd_hw_phy_config(tp, ioaddr);
		break;
	case RTL_GIGA_MAC_VER_06:
		rtl8169sce_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_07:
	case RTL_GIGA_MAC_VER_08:
	case RTL_GIGA_MAC_VER_09:
		rtl8102e_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_11:
		rtl8168bb_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_12:
		rtl8168bef_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_17:
		rtl8168bef_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_18:
		rtl8168cp_1_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_19:
		rtl8168c_1_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_20:
		rtl8168c_2_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_21:
		rtl8168c_3_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_22:
		rtl8168c_4_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_23:
	case RTL_GIGA_MAC_VER_24:
		rtl8168cp_2_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_25:
		rtl8168d_1_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_26:
		rtl8168d_2_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_27:
		rtl8168d_3_hw_phy_config(ioaddr);
		break;

	default:
		break;
	}
}

static void rtl8169_phy_timer(unsigned long __opaque)
{
	struct net_device *dev = (struct net_device *)__opaque;
	struct rtl8169_private *tp = netdev_priv(dev);
	struct timer_list *timer = &tp->timer;
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long timeout = RTL8169_PHY_TIMEOUT;

	assert(tp->mac_version > RTL_GIGA_MAC_VER_01);

	if (!(tp->phy_1000_ctrl_reg & ADVERTISE_1000FULL))
		return;

	spin_lock_irq(&tp->lock);

	if (tp->phy_reset_pending(ioaddr)) {
		/*
		 * A busy loop could burn quite a few cycles on nowadays CPU.
		 * Let's delay the execution of the timer for a few ticks.
		 */
		timeout = HZ/10;
		goto out_mod_timer;
	}

	if (tp->link_ok(ioaddr))
		goto out_unlock;

	netif_warn(tp, link, dev, "PHY reset until link up\n");

	tp->phy_reset_enable(ioaddr);

out_mod_timer:
	mod_timer(timer, jiffies + timeout);
out_unlock:
	spin_unlock_irq(&tp->lock);
}

static inline void rtl8169_delete_timer(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct timer_list *timer = &tp->timer;

	if (tp->mac_version <= RTL_GIGA_MAC_VER_01)
		return;

	del_timer_sync(timer);
}

static inline void rtl8169_request_timer(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct timer_list *timer = &tp->timer;

	if (tp->mac_version <= RTL_GIGA_MAC_VER_01)
		return;

	mod_timer(timer, jiffies + RTL8169_PHY_TIMEOUT);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */
static void rtl8169_netpoll(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct pci_dev *pdev = tp->pci_dev;

	disable_irq(pdev->irq);
	rtl8169_interrupt(pdev->irq, dev);
	enable_irq(pdev->irq);
}
#endif

static void rtl8169_release_board(struct pci_dev *pdev, struct net_device *dev,
				  void __iomem *ioaddr)
{
	iounmap(ioaddr);
	pci_release_regions(pdev);
	pci_clear_mwi(pdev);
	pci_disable_device(pdev);
	free_netdev(dev);
}

static void rtl8169_phy_reset(struct net_device *dev,
			      struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int i;

	tp->phy_reset_enable(ioaddr);
	for (i = 0; i < 100; i++) {
		if (!tp->phy_reset_pending(ioaddr))
			return;
		msleep(1);
	}
	netif_err(tp, link, dev, "PHY reset failed\n");
}

static void rtl8169_init_phy(struct net_device *dev, struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;

	rtl_hw_phy_config(dev);

	if (tp->mac_version <= RTL_GIGA_MAC_VER_06) {
		dprintk("Set MAC Reg C+CR Offset 0x82h = 0x01h\n");
		RTL_W8(0x82, 0x01);
	}

	pci_write_config_byte(tp->pci_dev, PCI_LATENCY_TIMER, 0x40);

	if (tp->mac_version <= RTL_GIGA_MAC_VER_06)
		pci_write_config_byte(tp->pci_dev, PCI_CACHE_LINE_SIZE, 0x08);

	if (tp->mac_version == RTL_GIGA_MAC_VER_02) {
		dprintk("Set MAC Reg C+CR Offset 0x82h = 0x01h\n");
		RTL_W8(0x82, 0x01);
		dprintk("Set PHY Reg 0x0bh = 0x00h\n");
		mdio_write(ioaddr, 0x0b, 0x0000); //w 0x0b 15 0 0
	}

	rtl8169_phy_reset(dev, tp);

	/*
	 * rtl8169_set_speed_xmii takes good care of the Fast Ethernet
	 * only 8101. Don't panic.
	 */
	rtl8169_set_speed(dev, AUTONEG_ENABLE, SPEED_1000, DUPLEX_FULL);

	if (RTL_R8(PHYstatus) & TBI_Enable)
		netif_info(tp, link, dev, "TBI auto-negotiating\n");
}

static void rtl_rar_set(struct rtl8169_private *tp, u8 *addr)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u32 high;
	u32 low;

	low  = addr[0] | (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24);
	high = addr[4] | (addr[5] << 8);

	spin_lock_irq(&tp->lock);

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	RTL_W32(MAC4, high);
	RTL_R32(MAC4);

	RTL_W32(MAC0, low);
	RTL_R32(MAC0);

	RTL_W8(Cfg9346, Cfg9346_Lock);

	spin_unlock_irq(&tp->lock);
}

static int rtl_set_mac_address(struct net_device *dev, void *p)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	rtl_rar_set(tp, dev->dev_addr);

	return 0;
}

static int rtl8169_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct mii_ioctl_data *data = if_mii(ifr);

	return netif_running(dev) ? tp->do_ioctl(tp, data, cmd) : -ENODEV;
}

static int rtl_xmii_ioctl(struct rtl8169_private *tp, struct mii_ioctl_data *data, int cmd)
{
	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = 32; /* Internal PHY */
		return 0;

	case SIOCGMIIREG:
		data->val_out = mdio_read(tp->mmio_addr, data->reg_num & 0x1f);
		return 0;

	case SIOCSMIIREG:
		mdio_write(tp->mmio_addr, data->reg_num & 0x1f, data->val_in);
		return 0;
	}
	return -EOPNOTSUPP;
}

static int rtl_tbi_ioctl(struct rtl8169_private *tp, struct mii_ioctl_data *data, int cmd)
{
	return -EOPNOTSUPP;
}

static const struct rtl_cfg_info {
	void (*hw_start)(struct net_device *);
	unsigned int region;
	unsigned int align;
	u16 intr_event;
	u16 napi_event;
	unsigned features;
	u8 default_ver;
} rtl_cfg_infos [] = {
	[RTL_CFG_0] = {
		.hw_start	= rtl_hw_start_8169,
		.region		= 1,
		.align		= 0,
		.intr_event	= SYSErr | LinkChg | RxOverflow |
				  RxFIFOOver | TxErr | TxOK | RxOK | RxErr,
		.napi_event	= RxFIFOOver | TxErr | TxOK | RxOK | RxOverflow,
		.features	= RTL_FEATURE_GMII,
		.default_ver	= RTL_GIGA_MAC_VER_01,
	},
	[RTL_CFG_1] = {
		.hw_start	= rtl_hw_start_8168,
		.region		= 2,
		.align		= 8,
		.intr_event	= SYSErr | LinkChg | RxOverflow |
				  TxErr | TxOK | RxOK | RxErr,
		.napi_event	= TxErr | TxOK | RxOK | RxOverflow,
		.features	= RTL_FEATURE_GMII | RTL_FEATURE_MSI,
		.default_ver	= RTL_GIGA_MAC_VER_11,
	},
	[RTL_CFG_2] = {
		.hw_start	= rtl_hw_start_8101,
		.region		= 2,
		.align		= 8,
		.intr_event	= SYSErr | LinkChg | RxOverflow | PCSTimeout |
				  RxFIFOOver | TxErr | TxOK | RxOK | RxErr,
		.napi_event	= RxFIFOOver | TxErr | TxOK | RxOK | RxOverflow,
		.features	= RTL_FEATURE_MSI,
		.default_ver	= RTL_GIGA_MAC_VER_13,
	}
};

/* Cfg9346_Unlock assumed. */
static unsigned rtl_try_msi(struct pci_dev *pdev, void __iomem *ioaddr,
			    const struct rtl_cfg_info *cfg)
{
	unsigned msi = 0;
	u8 cfg2;

	cfg2 = RTL_R8(Config2) & ~MSIEnable;
	if (cfg->features & RTL_FEATURE_MSI) {
		if (pci_enable_msi(pdev)) {
			dev_info(&pdev->dev, "no MSI. Back to INTx.\n");
		} else {
			cfg2 |= MSIEnable;
			msi = RTL_FEATURE_MSI;
		}
	}
	RTL_W8(Config2, cfg2);
	return msi;
}

static void rtl_disable_msi(struct pci_dev *pdev, struct rtl8169_private *tp)
{
	if (tp->features & RTL_FEATURE_MSI) {
		pci_disable_msi(pdev);
		tp->features &= ~RTL_FEATURE_MSI;
	}
}

static const struct net_device_ops rtl8169_netdev_ops = {
	.ndo_open		= rtl8169_open,
	.ndo_stop		= rtl8169_close,
	.ndo_get_stats		= rtl8169_get_stats,
	.ndo_start_xmit		= rtl8169_start_xmit,
	.ndo_tx_timeout		= rtl8169_tx_timeout,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= rtl8169_change_mtu,
	.ndo_set_mac_address	= rtl_set_mac_address,
	.ndo_do_ioctl		= rtl8169_ioctl,
	.ndo_set_multicast_list	= rtl_set_rx_mode,
#ifdef CONFIG_R8169_VLAN
	.ndo_vlan_rx_register	= rtl8169_vlan_rx_register,
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= rtl8169_netpoll,
#endif

};

static int __devinit
rtl8169_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	const struct rtl_cfg_info *cfg = rtl_cfg_infos + ent->driver_data;
	const unsigned int region = cfg->region;
	struct rtl8169_private *tp;
	struct mii_if_info *mii;
	struct net_device *dev;
	void __iomem *ioaddr;
	unsigned int i;
	int rc;

	if (netif_msg_drv(&debug)) {
		printk(KERN_INFO "%s Gigabit Ethernet driver %s loaded\n",
		       MODULENAME, RTL8169_VERSION);
	}

	dev = alloc_etherdev(sizeof (*tp));
	if (!dev) {
		if (netif_msg_drv(&debug))
			dev_err(&pdev->dev, "unable to alloc new ethernet\n");
		rc = -ENOMEM;
		goto out;
	}

	SET_NETDEV_DEV(dev, &pdev->dev);
	dev->netdev_ops = &rtl8169_netdev_ops;
	tp = netdev_priv(dev);
	tp->dev = dev;
	tp->pci_dev = pdev;
	tp->msg_enable = netif_msg_init(debug.msg_enable, R8169_MSG_DEFAULT);

	mii = &tp->mii;
	mii->dev = dev;
	mii->mdio_read = rtl_mdio_read;
	mii->mdio_write = rtl_mdio_write;
	mii->phy_id_mask = 0x1f;
	mii->reg_num_mask = 0x1f;
	mii->supports_gmii = !!(cfg->features & RTL_FEATURE_GMII);

	/* enable device (incl. PCI PM wakeup and hotplug setup) */
	rc = pci_enable_device(pdev);
	if (rc < 0) {
		netif_err(tp, probe, dev, "enable failure\n");
		goto err_out_free_dev_1;
	}

	if (pci_set_mwi(pdev) < 0)
		netif_info(tp, probe, dev, "Mem-Wr-Inval unavailable\n");

	/* make sure PCI base addr 1 is MMIO */
	if (!(pci_resource_flags(pdev, region) & IORESOURCE_MEM)) {
		netif_err(tp, probe, dev,
			  "region #%d not an MMIO resource, aborting\n",
			  region);
		rc = -ENODEV;
		goto err_out_mwi_2;
	}

	/* check for weird/broken PCI region reporting */
	if (pci_resource_len(pdev, region) < R8169_REGS_SIZE) {
		netif_err(tp, probe, dev,
			  "Invalid PCI region size(s), aborting\n");
		rc = -ENODEV;
		goto err_out_mwi_2;
	}

	rc = pci_request_regions(pdev, MODULENAME);
	if (rc < 0) {
		netif_err(tp, probe, dev, "could not request regions\n");
		goto err_out_mwi_2;
	}

	tp->cp_cmd = PCIMulRW | RxChkSum;

	if ((sizeof(dma_addr_t) > 4) &&
	    !pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) && use_dac) {
		tp->cp_cmd |= PCIDAC;
		dev->features |= NETIF_F_HIGHDMA;
	} else {
		rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (rc < 0) {
			netif_err(tp, probe, dev, "DMA configuration failed\n");
			goto err_out_free_res_3;
		}
	}

	/* ioremap MMIO region */
	ioaddr = ioremap(pci_resource_start(pdev, region), R8169_REGS_SIZE);
	if (!ioaddr) {
		netif_err(tp, probe, dev, "cannot remap MMIO, aborting\n");
		rc = -EIO;
		goto err_out_free_res_3;
	}

	tp->pcie_cap = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	if (!tp->pcie_cap)
		netif_info(tp, probe, dev, "no PCI Express capability\n");

	RTL_W16(IntrMask, 0x0000);

	/* Soft reset the chip. */
	RTL_W8(ChipCmd, CmdReset);

	/* Check that the chip has finished the reset. */
	for (i = 0; i < 100; i++) {
		if ((RTL_R8(ChipCmd) & CmdReset) == 0)
			break;
		msleep_interruptible(1);
	}

	RTL_W16(IntrStatus, 0xffff);

	pci_set_master(pdev);

	/* Identify chip attached to board */
	rtl8169_get_mac_version(tp, ioaddr);

	/* Use appropriate default if unknown */
	if (tp->mac_version == RTL_GIGA_MAC_NONE) {
		netif_notice(tp, probe, dev,
			     "unknown MAC, using family default\n");
		tp->mac_version = cfg->default_ver;
	}

	rtl8169_print_mac_version(tp);

	for (i = 0; i < ARRAY_SIZE(rtl_chip_info); i++) {
		if (tp->mac_version == rtl_chip_info[i].mac_version)
			break;
	}
	if (i == ARRAY_SIZE(rtl_chip_info)) {
		dev_err(&pdev->dev,
			"driver bug, MAC version not found in rtl_chip_info\n");
		goto err_out_msi_4;
	}
	tp->chipset = i;

	RTL_W8(Cfg9346, Cfg9346_Unlock);
	RTL_W8(Config1, RTL_R8(Config1) | PMEnable);
	RTL_W8(Config5, RTL_R8(Config5) & PMEStatus);
	if ((RTL_R8(Config3) & (LinkUp | MagicPacket)) != 0)
		tp->features |= RTL_FEATURE_WOL;
	if ((RTL_R8(Config5) & (UWF | BWF | MWF)) != 0)
		tp->features |= RTL_FEATURE_WOL;
	tp->features |= rtl_try_msi(pdev, ioaddr, cfg);
	RTL_W8(Cfg9346, Cfg9346_Lock);

	if ((tp->mac_version <= RTL_GIGA_MAC_VER_06) &&
	    (RTL_R8(PHYstatus) & TBI_Enable)) {
		tp->set_speed = rtl8169_set_speed_tbi;
		tp->get_settings = rtl8169_gset_tbi;
		tp->phy_reset_enable = rtl8169_tbi_reset_enable;
		tp->phy_reset_pending = rtl8169_tbi_reset_pending;
		tp->link_ok = rtl8169_tbi_link_ok;
		tp->do_ioctl = rtl_tbi_ioctl;

		tp->phy_1000_ctrl_reg = ADVERTISE_1000FULL; /* Implied by TBI */
	} else {
		tp->set_speed = rtl8169_set_speed_xmii;
		tp->get_settings = rtl8169_gset_xmii;
		tp->phy_reset_enable = rtl8169_xmii_reset_enable;
		tp->phy_reset_pending = rtl8169_xmii_reset_pending;
		tp->link_ok = rtl8169_xmii_link_ok;
		tp->do_ioctl = rtl_xmii_ioctl;
	}

	spin_lock_init(&tp->lock);

	tp->mmio_addr = ioaddr;

	/* Get MAC address */
	for (i = 0; i < MAC_ADDR_LEN; i++)
		dev->dev_addr[i] = RTL_R8(MAC0 + i);
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);

	SET_ETHTOOL_OPS(dev, &rtl8169_ethtool_ops);
	dev->watchdog_timeo = RTL8169_TX_TIMEOUT;
	dev->irq = pdev->irq;
	dev->base_addr = (unsigned long) ioaddr;

	netif_napi_add(dev, &tp->napi, rtl8169_poll, R8169_NAPI_WEIGHT);

#ifdef CONFIG_R8169_VLAN
	dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
#endif

	tp->intr_mask = 0xffff;
	tp->align = cfg->align;
	tp->hw_start = cfg->hw_start;
	tp->intr_event = cfg->intr_event;
	tp->napi_event = cfg->napi_event;

	init_timer(&tp->timer);
	tp->timer.data = (unsigned long) dev;
	tp->timer.function = rtl8169_phy_timer;

	rc = register_netdev(dev);
	if (rc < 0)
		goto err_out_msi_4;

	pci_set_drvdata(pdev, dev);

	netif_info(tp, probe, dev, "%s at 0x%lx, %pM, XID %08x IRQ %d\n",
		   rtl_chip_info[tp->chipset].name,
		   dev->base_addr, dev->dev_addr,
		   (u32)(RTL_R32(TxConfig) & 0x9cf0f8ff), dev->irq);

	rtl8169_init_phy(dev, tp);

	/*
	 * Pretend we are using VLANs; This bypasses a nasty bug where
	 * Interrupts stop flowing on high load on 8110SCd controllers.
	 */
	if (tp->mac_version == RTL_GIGA_MAC_VER_05)
		RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) | RxVlan);

	device_set_wakeup_enable(&pdev->dev, tp->features & RTL_FEATURE_WOL);

	if (pci_dev_run_wake(pdev))
		pm_runtime_put_noidle(&pdev->dev);

out:
	return rc;

err_out_msi_4:
	rtl_disable_msi(pdev, tp);
	iounmap(ioaddr);
err_out_free_res_3:
	pci_release_regions(pdev);
err_out_mwi_2:
	pci_clear_mwi(pdev);
	pci_disable_device(pdev);
err_out_free_dev_1:
	free_netdev(dev);
	goto out;
}

static void __devexit rtl8169_remove_one(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);

	flush_scheduled_work();

	unregister_netdev(dev);

	if (pci_dev_run_wake(pdev))
		pm_runtime_get_noresume(&pdev->dev);

	/* restore original MAC address */
	rtl_rar_set(tp, dev->perm_addr);

	rtl_disable_msi(pdev, tp);
	rtl8169_release_board(pdev, dev, tp->mmio_addr);
	pci_set_drvdata(pdev, NULL);
}

static void rtl8169_set_rxbufsize(struct rtl8169_private *tp,
				  unsigned int mtu)
{
	unsigned int max_frame = mtu + VLAN_ETH_HLEN + ETH_FCS_LEN;

	if (max_frame != 16383)
		printk(KERN_WARNING PFX "WARNING! Changing of MTU on this "
			"NIC may lead to frame reception errors!\n");

	tp->rx_buf_sz = (max_frame > RX_BUF_SIZE) ? max_frame : RX_BUF_SIZE;
}

static int rtl8169_open(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct pci_dev *pdev = tp->pci_dev;
	int retval = -ENOMEM;

	pm_runtime_get_sync(&pdev->dev);

	/*
	 * Note that we use a magic value here, its wierd I know
	 * its done because, some subset of rtl8169 hardware suffers from
	 * a problem in which frames received that are longer than
	 * the size set in RxMaxSize register return garbage sizes
	 * when received.  To avoid this we need to turn off filtering,
	 * which is done by setting a value of 16383 in the RxMaxSize register
	 * and allocating 16k frames to handle the largest possible rx value
	 * thats what the magic math below does.
	 */
	rtl8169_set_rxbufsize(tp, 16383 - VLAN_ETH_HLEN - ETH_FCS_LEN);

	/*
	 * Rx and Tx desscriptors needs 256 bytes alignment.
	 * dma_alloc_coherent provides more.
	 */
	tp->TxDescArray = dma_alloc_coherent(&pdev->dev, R8169_TX_RING_BYTES,
					     &tp->TxPhyAddr, GFP_KERNEL);
	if (!tp->TxDescArray)
		goto err_pm_runtime_put;

	tp->RxDescArray = dma_alloc_coherent(&pdev->dev, R8169_RX_RING_BYTES,
					     &tp->RxPhyAddr, GFP_KERNEL);
	if (!tp->RxDescArray)
		goto err_free_tx_0;

	retval = rtl8169_init_ring(dev);
	if (retval < 0)
		goto err_free_rx_1;

	INIT_DELAYED_WORK(&tp->task, NULL);

	smp_mb();

	retval = request_irq(dev->irq, rtl8169_interrupt,
			     (tp->features & RTL_FEATURE_MSI) ? 0 : IRQF_SHARED,
			     dev->name, dev);
	if (retval < 0)
		goto err_release_ring_2;

	napi_enable(&tp->napi);

	rtl_hw_start(dev);

	rtl8169_request_timer(dev);

	tp->saved_wolopts = 0;
	pm_runtime_put_noidle(&pdev->dev);

	rtl8169_check_link_status(dev, tp, tp->mmio_addr);
out:
	return retval;

err_release_ring_2:
	rtl8169_rx_clear(tp);
err_free_rx_1:
	dma_free_coherent(&pdev->dev, R8169_RX_RING_BYTES, tp->RxDescArray,
			  tp->RxPhyAddr);
	tp->RxDescArray = NULL;
err_free_tx_0:
	dma_free_coherent(&pdev->dev, R8169_TX_RING_BYTES, tp->TxDescArray,
			  tp->TxPhyAddr);
	tp->TxDescArray = NULL;
err_pm_runtime_put:
	pm_runtime_put_noidle(&pdev->dev);
	goto out;
}

static void rtl8169_hw_reset(void __iomem *ioaddr)
{
	/* Disable interrupts */
	rtl8169_irq_mask_and_ack(ioaddr);

	/* Reset the chipset */
	RTL_W8(ChipCmd, CmdReset);

	/* PCI commit */
	RTL_R8(ChipCmd);
}

static void rtl_set_rx_tx_config_registers(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u32 cfg = rtl8169_rx_config;

	cfg |= (RTL_R32(RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);
	RTL_W32(RxConfig, cfg);

	/* Set DMA burst size and Interframe Gap Time */
	RTL_W32(TxConfig, (TX_DMA_BURST << TxDMAShift) |
		(InterFrameGap << TxInterFrameGapShift));
}

static void rtl_hw_start(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int i;

	/* Soft reset the chip. */
	RTL_W8(ChipCmd, CmdReset);

	/* Check that the chip has finished the reset. */
	for (i = 0; i < 100; i++) {
		if ((RTL_R8(ChipCmd) & CmdReset) == 0)
			break;
		msleep_interruptible(1);
	}

	tp->hw_start(dev);

	netif_start_queue(dev);
}


static void rtl_set_rx_tx_desc_registers(struct rtl8169_private *tp,
					 void __iomem *ioaddr)
{
	/*
	 * Magic spell: some iop3xx ARM board needs the TxDescAddrHigh
	 * register to be written before TxDescAddrLow to work.
	 * Switching from MMIO to I/O access fixes the issue as well.
	 */
	RTL_W32(TxDescStartAddrHigh, ((u64) tp->TxPhyAddr) >> 32);
	RTL_W32(TxDescStartAddrLow, ((u64) tp->TxPhyAddr) & DMA_BIT_MASK(32));
	RTL_W32(RxDescAddrHigh, ((u64) tp->RxPhyAddr) >> 32);
	RTL_W32(RxDescAddrLow, ((u64) tp->RxPhyAddr) & DMA_BIT_MASK(32));
}

static u16 rtl_rw_cpluscmd(void __iomem *ioaddr)
{
	u16 cmd;

	cmd = RTL_R16(CPlusCmd);
	RTL_W16(CPlusCmd, cmd);
	return cmd;
}

static void rtl_set_rx_max_size(void __iomem *ioaddr, unsigned int rx_buf_sz)
{
	/* Low hurts. Let's disable the filtering. */
	RTL_W16(RxMaxSize, rx_buf_sz + 1);
}

static void rtl8169_set_magic_reg(void __iomem *ioaddr, unsigned mac_version)
{
	static const struct {
		u32 mac_version;
		u32 clk;
		u32 val;
	} cfg2_info [] = {
		{ RTL_GIGA_MAC_VER_05, PCI_Clock_33MHz, 0x000fff00 }, // 8110SCd
		{ RTL_GIGA_MAC_VER_05, PCI_Clock_66MHz, 0x000fffff },
		{ RTL_GIGA_MAC_VER_06, PCI_Clock_33MHz, 0x00ffff00 }, // 8110SCe
		{ RTL_GIGA_MAC_VER_06, PCI_Clock_66MHz, 0x00ffffff }
	}, *p = cfg2_info;
	unsigned int i;
	u32 clk;

	clk = RTL_R8(Config2) & PCI_Clock_66MHz;
	for (i = 0; i < ARRAY_SIZE(cfg2_info); i++, p++) {
		if ((p->mac_version == mac_version) && (p->clk == clk)) {
			RTL_W32(0x7c, p->val);
			break;
		}
	}
}

static void rtl_hw_start_8169(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct pci_dev *pdev = tp->pci_dev;

	if (tp->mac_version == RTL_GIGA_MAC_VER_05) {
		RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) | PCIMulRW);
		pci_write_config_byte(pdev, PCI_CACHE_LINE_SIZE, 0x08);
	}

	RTL_W8(Cfg9346, Cfg9346_Unlock);
	if ((tp->mac_version == RTL_GIGA_MAC_VER_01) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_04))
		RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_set_rx_max_size(ioaddr, tp->rx_buf_sz);

	if ((tp->mac_version == RTL_GIGA_MAC_VER_01) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_04))
		rtl_set_rx_tx_config_registers(tp);

	tp->cp_cmd |= rtl_rw_cpluscmd(ioaddr) | PCIMulRW;

	if ((tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03)) {
		dprintk("Set MAC Reg C+CR Offset 0xE0. "
			"Bit-3 and bit-14 MUST be 1\n");
		tp->cp_cmd |= (1 << 14);
	}

	RTL_W16(CPlusCmd, tp->cp_cmd);

	rtl8169_set_magic_reg(ioaddr, tp->mac_version);

	/*
	 * Undocumented corner. Supposedly:
	 * (TxTimer << 12) | (TxPackets << 8) | (RxTimer << 4) | RxPackets
	 */
	RTL_W16(IntrMitigate, 0x0000);

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	if ((tp->mac_version != RTL_GIGA_MAC_VER_01) &&
	    (tp->mac_version != RTL_GIGA_MAC_VER_02) &&
	    (tp->mac_version != RTL_GIGA_MAC_VER_03) &&
	    (tp->mac_version != RTL_GIGA_MAC_VER_04)) {
		RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);
		rtl_set_rx_tx_config_registers(tp);
	}

	RTL_W8(Cfg9346, Cfg9346_Lock);

	/* Initially a 10 us delay. Turned it into a PCI commit. - FR */
	RTL_R8(IntrMask);

	RTL_W32(RxMissed, 0);

	rtl_set_rx_mode(dev);

	/* no early-rx interrupts */
	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xF000);

	/* Enable all known interrupts by setting the interrupt mask. */
	RTL_W16(IntrMask, tp->intr_event);
}

static void rtl_tx_performance_tweak(struct pci_dev *pdev, u16 force)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);
	int cap = tp->pcie_cap;

	if (cap) {
		u16 ctl;

		pci_read_config_word(pdev, cap + PCI_EXP_DEVCTL, &ctl);
		ctl = (ctl & ~PCI_EXP_DEVCTL_READRQ) | force;
		pci_write_config_word(pdev, cap + PCI_EXP_DEVCTL, ctl);
	}
}

static void rtl_csi_access_enable(void __iomem *ioaddr)
{
	u32 csi;

	csi = rtl_csi_read(ioaddr, 0x070c) & 0x00ffffff;
	rtl_csi_write(ioaddr, 0x070c, csi | 0x27000000);
}

struct ephy_info {
	unsigned int offset;
	u16 mask;
	u16 bits;
};

static void rtl_ephy_init(void __iomem *ioaddr, const struct ephy_info *e, int len)
{
	u16 w;

	while (len-- > 0) {
		w = (rtl_ephy_read(ioaddr, e->offset) & ~e->mask) | e->bits;
		rtl_ephy_write(ioaddr, e->offset, w);
		e++;
	}
}

static void rtl_disable_clock_request(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);
	int cap = tp->pcie_cap;

	if (cap) {
		u16 ctl;

		pci_read_config_word(pdev, cap + PCI_EXP_LNKCTL, &ctl);
		ctl &= ~PCI_EXP_LNKCTL_CLKREQ_EN;
		pci_write_config_word(pdev, cap + PCI_EXP_LNKCTL, ctl);
	}
}

#define R8168_CPCMD_QUIRK_MASK (\
	EnableBist | \
	Mac_dbgo_oe | \
	Force_half_dup | \
	Force_rxflow_en | \
	Force_txflow_en | \
	Cxpl_dbg_sel | \
	ASF | \
	PktCntrDisable | \
	Mac_dbgo_sel)

static void rtl_hw_start_8168bb(void __iomem *ioaddr, struct pci_dev *pdev)
{
	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);

	rtl_tx_performance_tweak(pdev,
		(0x5 << MAX_READ_REQUEST_SHIFT) | PCI_EXP_DEVCTL_NOSNOOP_EN);
}

static void rtl_hw_start_8168bef(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_hw_start_8168bb(ioaddr, pdev);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	RTL_W8(Config4, RTL_R8(Config4) & ~(1 << 0));
}

static void __rtl_hw_start_8168cp(void __iomem *ioaddr, struct pci_dev *pdev)
{
	RTL_W8(Config1, RTL_R8(Config1) | Speed_down);

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	rtl_disable_clock_request(pdev);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168cp_1(void __iomem *ioaddr, struct pci_dev *pdev)
{
	static const struct ephy_info e_info_8168cp[] = {
		{ 0x01, 0,	0x0001 },
		{ 0x02, 0x0800,	0x1000 },
		{ 0x03, 0,	0x0042 },
		{ 0x06, 0x0080,	0x0000 },
		{ 0x07, 0,	0x2000 }
	};

	rtl_csi_access_enable(ioaddr);

	rtl_ephy_init(ioaddr, e_info_8168cp, ARRAY_SIZE(e_info_8168cp));

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168cp_2(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168cp_3(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	/* Magic. */
	RTL_W8(DBG_REG, 0x20);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168c_1(void __iomem *ioaddr, struct pci_dev *pdev)
{
	static const struct ephy_info e_info_8168c_1[] = {
		{ 0x02, 0x0800,	0x1000 },
		{ 0x03, 0,	0x0002 },
		{ 0x06, 0x0080,	0x0000 }
	};

	rtl_csi_access_enable(ioaddr);

	RTL_W8(DBG_REG, 0x06 | FIX_NAK_1 | FIX_NAK_2);

	rtl_ephy_init(ioaddr, e_info_8168c_1, ARRAY_SIZE(e_info_8168c_1));

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168c_2(void __iomem *ioaddr, struct pci_dev *pdev)
{
	static const struct ephy_info e_info_8168c_2[] = {
		{ 0x01, 0,	0x0001 },
		{ 0x03, 0x0400,	0x0220 }
	};

	rtl_csi_access_enable(ioaddr);

	rtl_ephy_init(ioaddr, e_info_8168c_2, ARRAY_SIZE(e_info_8168c_2));

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168c_3(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_hw_start_8168c_2(ioaddr, pdev);
}

static void rtl_hw_start_8168c_4(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168d(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	rtl_disable_clock_request(pdev);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct pci_dev *pdev = tp->pci_dev;

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_set_rx_max_size(ioaddr, tp->rx_buf_sz);

	tp->cp_cmd |= RTL_R16(CPlusCmd) | PktCntrDisable | INTT_1;

	RTL_W16(CPlusCmd, tp->cp_cmd);

	RTL_W16(IntrMitigate, 0x5151);

	if (tp->mac_version == RTL_GIGA_MAC_VER_11) {
		tp->intr_event |= RxFIFOOver | PCSTimeout;
		tp->intr_event &= ~RxOverflow;
	}

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	rtl_set_rx_mode(dev);

	RTL_W32(TxConfig, (TX_DMA_BURST << TxDMAShift) |
		(InterFrameGap << TxInterFrameGapShift));

	RTL_R8(IntrMask);

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_11:
		rtl_hw_start_8168bb(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_12:
	case RTL_GIGA_MAC_VER_17:
		rtl_hw_start_8168bef(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_18:
		rtl_hw_start_8168cp_1(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_19:
		rtl_hw_start_8168c_1(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_20:
		rtl_hw_start_8168c_2(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_21:
		rtl_hw_start_8168c_3(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_22:
		rtl_hw_start_8168c_4(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_23:
		rtl_hw_start_8168cp_2(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_24:
		rtl_hw_start_8168cp_3(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_25:
	case RTL_GIGA_MAC_VER_26:
	case RTL_GIGA_MAC_VER_27:
		rtl_hw_start_8168d(ioaddr, pdev);
	break;

	default:
		printk(KERN_ERR PFX "%s: unknown chipset (mac_version = %d).\n",
			dev->name, tp->mac_version);
	break;
	}

	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W8(Cfg9346, Cfg9346_Lock);

	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xF000);

	RTL_W16(IntrMask, tp->intr_event);
}

#define R810X_CPCMD_QUIRK_MASK (\
	EnableBist | \
	Mac_dbgo_oe | \
	Force_half_dup | \
	Force_rxflow_en | \
	Force_txflow_en | \
	Cxpl_dbg_sel | \
	ASF | \
	PktCntrDisable | \
	PCIDAC | \
	PCIMulRW)

static void rtl_hw_start_8102e_1(void __iomem *ioaddr, struct pci_dev *pdev)
{
	static const struct ephy_info e_info_8102e_1[] = {
		{ 0x01,	0, 0x6e65 },
		{ 0x02,	0, 0x091f },
		{ 0x03,	0, 0xc2f9 },
		{ 0x06,	0, 0xafb5 },
		{ 0x07,	0, 0x0e00 },
		{ 0x19,	0, 0xec80 },
		{ 0x01,	0, 0x2e65 },
		{ 0x01,	0, 0x6e65 }
	};
	u8 cfg1;

	rtl_csi_access_enable(ioaddr);

	RTL_W8(DBG_REG, FIX_NAK_1);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W8(Config1,
	       LEDS1 | LEDS0 | Speed_down | MEMMAP | IOMAP | VPD | PMEnable);
	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	cfg1 = RTL_R8(Config1);
	if ((cfg1 & LEDS0) && (cfg1 & LEDS1))
		RTL_W8(Config1, cfg1 & ~LEDS0);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R810X_CPCMD_QUIRK_MASK);

	rtl_ephy_init(ioaddr, e_info_8102e_1, ARRAY_SIZE(e_info_8102e_1));
}

static void rtl_hw_start_8102e_2(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W8(Config1, MEMMAP | IOMAP | VPD | PMEnable);
	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R810X_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8102e_3(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_hw_start_8102e_2(ioaddr, pdev);

	rtl_ephy_write(ioaddr, 0x03, 0xc2f9);
}

static void rtl_hw_start_8101(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct pci_dev *pdev = tp->pci_dev;

	if ((tp->mac_version == RTL_GIGA_MAC_VER_13) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_16)) {
		int cap = tp->pcie_cap;

		if (cap) {
			pci_write_config_word(pdev, cap + PCI_EXP_DEVCTL,
					      PCI_EXP_DEVCTL_NOSNOOP_EN);
		}
	}

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_07:
		rtl_hw_start_8102e_1(ioaddr, pdev);
		break;

	case RTL_GIGA_MAC_VER_08:
		rtl_hw_start_8102e_3(ioaddr, pdev);
		break;

	case RTL_GIGA_MAC_VER_09:
		rtl_hw_start_8102e_2(ioaddr, pdev);
		break;
	}

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_set_rx_max_size(ioaddr, tp->rx_buf_sz);

	tp->cp_cmd |= rtl_rw_cpluscmd(ioaddr) | PCIMulRW;

	RTL_W16(CPlusCmd, tp->cp_cmd);

	RTL_W16(IntrMitigate, 0x0000);

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);
	rtl_set_rx_tx_config_registers(tp);

	RTL_W8(Cfg9346, Cfg9346_Lock);

	RTL_R8(IntrMask);

	rtl_set_rx_mode(dev);

	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xf000);

	RTL_W16(IntrMask, tp->intr_event);
}

static int rtl8169_change_mtu(struct net_device *dev, int new_mtu)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	int ret = 0;

	if (new_mtu < ETH_ZLEN || new_mtu > SafeMtu)
		return -EINVAL;

	dev->mtu = new_mtu;

	if (!netif_running(dev))
		goto out;

	rtl8169_down(dev);

	rtl8169_set_rxbufsize(tp, dev->mtu);

	ret = rtl8169_init_ring(dev);
	if (ret < 0)
		goto out;

	napi_enable(&tp->napi);

	rtl_hw_start(dev);

	rtl8169_request_timer(dev);

out:
	return ret;
}

static inline void rtl8169_make_unusable_by_asic(struct RxDesc *desc)
{
	desc->addr = cpu_to_le64(0x0badbadbadbadbadull);
	desc->opts1 &= ~cpu_to_le32(DescOwn | RsvdMask);
}

static void rtl8169_free_rx_skb(struct rtl8169_private *tp,
				struct sk_buff **sk_buff, struct RxDesc *desc)
{
	struct pci_dev *pdev = tp->pci_dev;

	dma_unmap_single(&pdev->dev, le64_to_cpu(desc->addr), tp->rx_buf_sz,
			 PCI_DMA_FROMDEVICE);
	dev_kfree_skb(*sk_buff);
	*sk_buff = NULL;
	rtl8169_make_unusable_by_asic(desc);
}

static inline void rtl8169_mark_to_asic(struct RxDesc *desc, u32 rx_buf_sz)
{
	u32 eor = le32_to_cpu(desc->opts1) & RingEnd;

	desc->opts1 = cpu_to_le32(DescOwn | eor | rx_buf_sz);
}

static inline void rtl8169_map_to_asic(struct RxDesc *desc, dma_addr_t mapping,
				       u32 rx_buf_sz)
{
	desc->addr = cpu_to_le64(mapping);
	wmb();
	rtl8169_mark_to_asic(desc, rx_buf_sz);
}

static struct sk_buff *rtl8169_alloc_rx_skb(struct pci_dev *pdev,
					    struct net_device *dev,
					    struct RxDesc *desc, int rx_buf_sz,
					    unsigned int align, gfp_t gfp)
{
	struct sk_buff *skb;
	dma_addr_t mapping;
	unsigned int pad;

	pad = align ? align : NET_IP_ALIGN;

	skb = __netdev_alloc_skb(dev, rx_buf_sz + pad, gfp);
	if (!skb)
		goto err_out;

	skb_reserve(skb, align ? ((pad - 1) & (unsigned long)skb->data) : pad);

	mapping = dma_map_single(&pdev->dev, skb->data, rx_buf_sz,
				 PCI_DMA_FROMDEVICE);

	rtl8169_map_to_asic(desc, mapping, rx_buf_sz);
out:
	return skb;

err_out:
	rtl8169_make_unusable_by_asic(desc);
	goto out;
}

static void rtl8169_rx_clear(struct rtl8169_private *tp)
{
	unsigned int i;

	for (i = 0; i < NUM_RX_DESC; i++) {
		if (tp->Rx_skbuff[i]) {
			rtl8169_free_rx_skb(tp, tp->Rx_skbuff + i,
					    tp->RxDescArray + i);
		}
	}
}

static u32 rtl8169_rx_fill(struct rtl8169_private *tp, struct net_device *dev,
			   u32 start, u32 end, gfp_t gfp)
{
	u32 cur;

	for (cur = start; end - cur != 0; cur++) {
		struct sk_buff *skb;
		unsigned int i = cur % NUM_RX_DESC;

		WARN_ON((s32)(end - cur) < 0);

		if (tp->Rx_skbuff[i])
			continue;

		skb = rtl8169_alloc_rx_skb(tp->pci_dev, dev,
					   tp->RxDescArray + i,
					   tp->rx_buf_sz, tp->align, gfp);
		if (!skb)
			break;

		tp->Rx_skbuff[i] = skb;
	}
	return cur - start;
}

static inline void rtl8169_mark_as_last_descriptor(struct RxDesc *desc)
{
	desc->opts1 |= cpu_to_le32(RingEnd);
}

static void rtl8169_init_ring_indexes(struct rtl8169_private *tp)
{
	tp->dirty_tx = tp->dirty_rx = tp->cur_tx = tp->cur_rx = 0;
}

static int rtl8169_init_ring(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl8169_init_ring_indexes(tp);

	memset(tp->tx_skb, 0x0, NUM_TX_DESC * sizeof(struct ring_info));
	memset(tp->Rx_skbuff, 0x0, NUM_RX_DESC * sizeof(struct sk_buff *));

	if (rtl8169_rx_fill(tp, dev, 0, NUM_RX_DESC, GFP_KERNEL) != NUM_RX_DESC)
		goto err_out;

	rtl8169_mark_as_last_descriptor(tp->RxDescArray + NUM_RX_DESC - 1);

	return 0;

err_out:
	rtl8169_rx_clear(tp);
	return -ENOMEM;
}

static void rtl8169_unmap_tx_skb(struct pci_dev *pdev, struct ring_info *tx_skb,
				 struct TxDesc *desc)
{
	unsigned int len = tx_skb->len;

	dma_unmap_single(&pdev->dev, le64_to_cpu(desc->addr), len,
			 PCI_DMA_TODEVICE);
	desc->opts1 = 0x00;
	desc->opts2 = 0x00;
	desc->addr = 0x00;
	tx_skb->len = 0;
}

static void rtl8169_tx_clear(struct rtl8169_private *tp)
{
	unsigned int i;

	for (i = tp->dirty_tx; i < tp->dirty_tx + NUM_TX_DESC; i++) {
		unsigned int entry = i % NUM_TX_DESC;
		struct ring_info *tx_skb = tp->tx_skb + entry;
		unsigned int len = tx_skb->len;

		if (len) {
			struct sk_buff *skb = tx_skb->skb;

			rtl8169_unmap_tx_skb(tp->pci_dev, tx_skb,
					     tp->TxDescArray + entry);
			if (skb) {
				dev_kfree_skb(skb);
				tx_skb->skb = NULL;
			}
			tp->dev->stats.tx_dropped++;
		}
	}
	tp->cur_tx = tp->dirty_tx = 0;
}

static void rtl8169_schedule_work(struct net_device *dev, work_func_t task)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	PREPARE_DELAYED_WORK(&tp->task, task);
	schedule_delayed_work(&tp->task, 4);
}

static void rtl8169_wait_for_quiescence(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	synchronize_irq(dev->irq);

	/* Wait for any pending NAPI task to complete */
	napi_disable(&tp->napi);

	rtl8169_irq_mask_and_ack(ioaddr);

	tp->intr_mask = 0xffff;
	RTL_W16(IntrMask, tp->intr_event);
	napi_enable(&tp->napi);
}

static void rtl8169_reinit_task(struct work_struct *work)
{
	struct rtl8169_private *tp =
		container_of(work, struct rtl8169_private, task.work);
	struct net_device *dev = tp->dev;
	int ret;

	rtnl_lock();

	if (!netif_running(dev))
		goto out_unlock;

	rtl8169_wait_for_quiescence(dev);
	rtl8169_close(dev);

	ret = rtl8169_open(dev);
	if (unlikely(ret < 0)) {
		if (net_ratelimit())
			netif_err(tp, drv, dev,
				  "reinit failure (status = %d). Rescheduling\n",
				  ret);
		rtl8169_schedule_work(dev, rtl8169_reinit_task);
	}

out_unlock:
	rtnl_unlock();
}

static void rtl8169_reset_task(struct work_struct *work)
{
	struct rtl8169_private *tp =
		container_of(work, struct rtl8169_private, task.work);
	struct net_device *dev = tp->dev;

	rtnl_lock();

	if (!netif_running(dev))
		goto out_unlock;

	rtl8169_wait_for_quiescence(dev);

	rtl8169_rx_interrupt(dev, tp, tp->mmio_addr, ~(u32)0);
	rtl8169_tx_clear(tp);

	if (tp->dirty_rx == tp->cur_rx) {
		rtl8169_init_ring_indexes(tp);
		rtl_hw_start(dev);
		netif_wake_queue(dev);
		rtl8169_check_link_status(dev, tp, tp->mmio_addr);
	} else {
		if (net_ratelimit())
			netif_emerg(tp, intr, dev, "Rx buffers shortage\n");
		rtl8169_schedule_work(dev, rtl8169_reset_task);
	}

out_unlock:
	rtnl_unlock();
}

static void rtl8169_tx_timeout(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl8169_hw_reset(tp->mmio_addr);

	/* Let's wait a bit while any (async) irq lands on */
	rtl8169_schedule_work(dev, rtl8169_reset_task);
}

static int rtl8169_xmit_frags(struct rtl8169_private *tp, struct sk_buff *skb,
			      u32 opts1)
{
	struct skb_shared_info *info = skb_shinfo(skb);
	unsigned int cur_frag, entry;
	struct TxDesc * uninitialized_var(txd);

	entry = tp->cur_tx;
	for (cur_frag = 0; cur_frag < info->nr_frags; cur_frag++) {
		skb_frag_t *frag = info->frags + cur_frag;
		dma_addr_t mapping;
		u32 status, len;
		void *addr;

		entry = (entry + 1) % NUM_TX_DESC;

		txd = tp->TxDescArray + entry;
		len = frag->size;
		addr = ((void *) page_address(frag->page)) + frag->page_offset;
		mapping = dma_map_single(&tp->pci_dev->dev, addr, len,
					 PCI_DMA_TODEVICE);

		/* anti gcc 2.95.3 bugware (sic) */
		status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));

		txd->opts1 = cpu_to_le32(status);
		txd->addr = cpu_to_le64(mapping);

		tp->tx_skb[entry].len = len;
	}

	if (cur_frag) {
		tp->tx_skb[entry].skb = skb;
		txd->opts1 |= cpu_to_le32(LastFrag);
	}

	return cur_frag;
}

static inline u32 rtl8169_tso_csum(struct sk_buff *skb, struct net_device *dev)
{
	if (dev->features & NETIF_F_TSO) {
		u32 mss = skb_shinfo(skb)->gso_size;

		if (mss)
			return LargeSend | ((mss & MSSMask) << MSSShift);
	}
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		const struct iphdr *ip = ip_hdr(skb);

		if (ip->protocol == IPPROTO_TCP)
			return IPCS | TCPCS;
		else if (ip->protocol == IPPROTO_UDP)
			return IPCS | UDPCS;
		WARN_ON(1);	/* we need a WARN() */
	}
	return 0;
}

static netdev_tx_t rtl8169_start_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned int frags, entry = tp->cur_tx % NUM_TX_DESC;
	struct TxDesc *txd = tp->TxDescArray + entry;
	void __iomem *ioaddr = tp->mmio_addr;
	dma_addr_t mapping;
	u32 status, len;
	u32 opts1;

	if (unlikely(TX_BUFFS_AVAIL(tp) < skb_shinfo(skb)->nr_frags)) {
		netif_err(tp, drv, dev, "BUG! Tx Ring full when queue awake!\n");
		goto err_stop;
	}

	if (unlikely(le32_to_cpu(txd->opts1) & DescOwn))
		goto err_stop;

	opts1 = DescOwn | rtl8169_tso_csum(skb, dev);

	frags = rtl8169_xmit_frags(tp, skb, opts1);
	if (frags) {
		len = skb_headlen(skb);
		opts1 |= FirstFrag;
	} else {
		len = skb->len;
		opts1 |= FirstFrag | LastFrag;
		tp->tx_skb[entry].skb = skb;
	}

	mapping = dma_map_single(&tp->pci_dev->dev, skb->data, len,
				 PCI_DMA_TODEVICE);

	tp->tx_skb[entry].len = len;
	txd->addr = cpu_to_le64(mapping);
	txd->opts2 = cpu_to_le32(rtl8169_tx_vlan_tag(tp, skb));

	wmb();

	/* anti gcc 2.95.3 bugware (sic) */
	status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));
	txd->opts1 = cpu_to_le32(status);

	tp->cur_tx += frags + 1;

	wmb();

	RTL_W8(TxPoll, NPQ);	/* set polling bit */

	if (TX_BUFFS_AVAIL(tp) < MAX_SKB_FRAGS) {
		netif_stop_queue(dev);
		smp_rmb();
		if (TX_BUFFS_AVAIL(tp) >= MAX_SKB_FRAGS)
			netif_wake_queue(dev);
	}

	return NETDEV_TX_OK;

err_stop:
	netif_stop_queue(dev);
	dev->stats.tx_dropped++;
	return NETDEV_TX_BUSY;
}

static void rtl8169_pcierr_interrupt(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct pci_dev *pdev = tp->pci_dev;
	void __iomem *ioaddr = tp->mmio_addr;
	u16 pci_status, pci_cmd;

	pci_read_config_word(pdev, PCI_COMMAND, &pci_cmd);
	pci_read_config_word(pdev, PCI_STATUS, &pci_status);

	netif_err(tp, intr, dev, "PCI error (cmd = 0x%04x, status = 0x%04x)\n",
		  pci_cmd, pci_status);

	/*
	 * The recovery sequence below admits a very elaborated explanation:
	 * - it seems to work;
	 * - I did not see what else could be done;
	 * - it makes iop3xx happy.
	 *
	 * Feel free to adjust to your needs.
	 */
	if (pdev->broken_parity_status)
		pci_cmd &= ~PCI_COMMAND_PARITY;
	else
		pci_cmd |= PCI_COMMAND_SERR | PCI_COMMAND_PARITY;

	pci_write_config_word(pdev, PCI_COMMAND, pci_cmd);

	pci_write_config_word(pdev, PCI_STATUS,
		pci_status & (PCI_STATUS_DETECTED_PARITY |
		PCI_STATUS_SIG_SYSTEM_ERROR | PCI_STATUS_REC_MASTER_ABORT |
		PCI_STATUS_REC_TARGET_ABORT | PCI_STATUS_SIG_TARGET_ABORT));

	/* The infamous DAC f*ckup only happens at boot time */
	if ((tp->cp_cmd & PCIDAC) && !tp->dirty_rx && !tp->cur_rx) {
		netif_info(tp, intr, dev, "disabling PCI DAC\n");
		tp->cp_cmd &= ~PCIDAC;
		RTL_W16(CPlusCmd, tp->cp_cmd);
		dev->features &= ~NETIF_F_HIGHDMA;
	}

	rtl8169_hw_reset(ioaddr);

	rtl8169_schedule_work(dev, rtl8169_reinit_task);
}

static void rtl8169_tx_interrupt(struct net_device *dev,
				 struct rtl8169_private *tp,
				 void __iomem *ioaddr)
{
	unsigned int dirty_tx, tx_left;

	dirty_tx = tp->dirty_tx;
	smp_rmb();
	tx_left = tp->cur_tx - dirty_tx;

	while (tx_left > 0) {
		unsigned int entry = dirty_tx % NUM_TX_DESC;
		struct ring_info *tx_skb = tp->tx_skb + entry;
		u32 len = tx_skb->len;
		u32 status;

		rmb();
		status = le32_to_cpu(tp->TxDescArray[entry].opts1);
		if (status & DescOwn)
			break;

		dev->stats.tx_bytes += len;
		dev->stats.tx_packets++;

		rtl8169_unmap_tx_skb(tp->pci_dev, tx_skb, tp->TxDescArray + entry);

		if (status & LastFrag) {
			dev_kfree_skb(tx_skb->skb);
			tx_skb->skb = NULL;
		}
		dirty_tx++;
		tx_left--;
	}

	if (tp->dirty_tx != dirty_tx) {
		tp->dirty_tx = dirty_tx;
		smp_wmb();
		if (netif_queue_stopped(dev) &&
		    (TX_BUFFS_AVAIL(tp) >= MAX_SKB_FRAGS)) {
			netif_wake_queue(dev);
		}
		/*
		 * 8168 hack: TxPoll requests are lost when the Tx packets are
		 * too close. Let's kick an extra TxPoll request when a burst
		 * of start_xmit activity is detected (if it is not detected,
		 * it is slow enough). -- FR
		 */
		smp_rmb();
		if (tp->cur_tx != dirty_tx)
			RTL_W8(TxPoll, NPQ);
	}
}

static inline int rtl8169_fragmented_frame(u32 status)
{
	return (status & (FirstFrag | LastFrag)) != (FirstFrag | LastFrag);
}

static inline void rtl8169_rx_csum(struct sk_buff *skb, u32 opts1)
{
	u32 status = opts1 & RxProtoMask;

	if (((status == RxProtoTCP) && !(opts1 & TCPFail)) ||
	    ((status == RxProtoUDP) && !(opts1 & UDPFail)))
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	else
		skb->ip_summed = CHECKSUM_NONE;
}

static inline bool rtl8169_try_rx_copy(struct sk_buff **sk_buff,
				       struct rtl8169_private *tp, int pkt_size,
				       dma_addr_t addr)
{
	struct sk_buff *skb;
	bool done = false;

	if (pkt_size >= rx_copybreak)
		goto out;

	skb = netdev_alloc_skb_ip_align(tp->dev, pkt_size);
	if (!skb)
		goto out;

	dma_sync_single_for_cpu(&tp->pci_dev->dev, addr, pkt_size,
				PCI_DMA_FROMDEVICE);
	skb_copy_from_linear_data(*sk_buff, skb->data, pkt_size);
	*sk_buff = skb;
	done = true;
out:
	return done;
}

/*
 * Warning : rtl8169_rx_interrupt() might be called :
 * 1) from NAPI (softirq) context
 *	(polling = 1 : we should call netif_receive_skb())
 * 2) from process context (rtl8169_reset_task())
 *	(polling = 0 : we must call netif_rx() instead)
 */
static int rtl8169_rx_interrupt(struct net_device *dev,
				struct rtl8169_private *tp,
				void __iomem *ioaddr, u32 budget)
{
	unsigned int cur_rx, rx_left;
	unsigned int delta, count;
	int polling = (budget != ~(u32)0) ? 1 : 0;

	cur_rx = tp->cur_rx;
	rx_left = NUM_RX_DESC + tp->dirty_rx - cur_rx;
	rx_left = min(rx_left, budget);

	for (; rx_left > 0; rx_left--, cur_rx++) {
		unsigned int entry = cur_rx % NUM_RX_DESC;
		struct RxDesc *desc = tp->RxDescArray + entry;
		u32 status;

		rmb();
		status = le32_to_cpu(desc->opts1);

		if (status & DescOwn)
			break;
		if (unlikely(status & RxRES)) {
			netif_info(tp, rx_err, dev, "Rx ERROR. status = %08x\n",
				   status);
			dev->stats.rx_errors++;
			if (status & (RxRWT | RxRUNT))
				dev->stats.rx_length_errors++;
			if (status & RxCRC)
				dev->stats.rx_crc_errors++;
			if (status & RxFOVF) {
				rtl8169_schedule_work(dev, rtl8169_reset_task);
				dev->stats.rx_fifo_errors++;
			}
			rtl8169_mark_to_asic(desc, tp->rx_buf_sz);
		} else {
			struct sk_buff *skb = tp->Rx_skbuff[entry];
			dma_addr_t addr = le64_to_cpu(desc->addr);
			int pkt_size = (status & 0x00001FFF) - 4;
			struct pci_dev *pdev = tp->pci_dev;

			/*
			 * The driver does not support incoming fragmented
			 * frames. They are seen as a symptom of over-mtu
			 * sized frames.
			 */
			if (unlikely(rtl8169_fragmented_frame(status))) {
				dev->stats.rx_dropped++;
				dev->stats.rx_length_errors++;
				rtl8169_mark_to_asic(desc, tp->rx_buf_sz);
				continue;
			}

			if (rtl8169_try_rx_copy(&skb, tp, pkt_size, addr)) {
				dma_sync_single_for_device(&pdev->dev, addr,
					pkt_size, PCI_DMA_FROMDEVICE);
				rtl8169_mark_to_asic(desc, tp->rx_buf_sz);
			} else {
				dma_unmap_single(&pdev->dev, addr, tp->rx_buf_sz,
						 PCI_DMA_FROMDEVICE);
				tp->Rx_skbuff[entry] = NULL;
			}

			rtl8169_rx_csum(skb, status);
			skb_put(skb, pkt_size);
			skb->protocol = eth_type_trans(skb, dev);

			if (rtl8169_rx_vlan_skb(tp, desc, skb, polling) < 0) {
				if (likely(polling))
					netif_receive_skb(skb);
				else
					netif_rx(skb);
			}

			dev->stats.rx_bytes += pkt_size;
			dev->stats.rx_packets++;
		}

		if ((desc->opts2 & cpu_to_le32(0xfffe000)) &&
		    (tp->mac_version == RTL_GIGA_MAC_VER_05)) {
			desc->opts2 = 0;
			cur_rx++;
		}
	}

	count = cur_rx - tp->cur_rx;
	tp->cur_rx = cur_rx;

	delta = rtl8169_rx_fill(tp, dev, tp->dirty_rx, tp->cur_rx, GFP_ATOMIC);
	if (!delta && count)
		netif_info(tp, intr, dev, "no Rx buffer allocated\n");
	tp->dirty_rx += delta;

	if (tp->dirty_rx + NUM_RX_DESC == tp->cur_rx)
		netif_emerg(tp, intr, dev, "Rx buffers exhausted\n");

	return count;
}

static irqreturn_t rtl8169_interrupt(int irq, void *dev_instance)
{
	struct net_device *dev = dev_instance;
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	int handled = 0;
	int status;

	/* loop handling interrupts until we have no new ones or
	 * we hit a invalid/hotplug case.
	 */
	status = RTL_R16(IntrStatus);
	while (status && status != 0xffff) {
		handled = 1;

		/* Handle all of the error cases first. These will reset
		 * the chip, so just exit the loop.
		 */
		if (unlikely(!netif_running(dev))) {
			rtl8169_asic_down(ioaddr);
			break;
		}

		if (unlikely(status & RxFIFOOver) &&
		(tp->mac_version == RTL_GIGA_MAC_VER_11)) {
			netif_stop_queue(dev);
			rtl8169_tx_timeout(dev);
			break;
		}

		if (unlikely(status & SYSErr)) {
			rtl8169_pcierr_interrupt(dev);
			break;
		}

		if (status & LinkChg)
			rtl8169_check_link_status(dev, tp, ioaddr);

		/* We need to see the lastest version of tp->intr_mask to
		 * avoid ignoring an MSI interrupt and having to wait for
		 * another event which may never come.
		 */
		smp_rmb();
		if (status & tp->intr_mask & tp->napi_event) {
			RTL_W16(IntrMask, tp->intr_event & ~tp->napi_event);
			tp->intr_mask = ~tp->napi_event;

			if (likely(napi_schedule_prep(&tp->napi)))
				__napi_schedule(&tp->napi);
			else
				netif_info(tp, intr, dev,
					   "interrupt %04x in poll\n", status);
		}

		/* We only get a new MSI interrupt when all active irq
		 * sources on the chip have been acknowledged. So, ack
		 * everything we've seen and check if new sources have become
		 * active to avoid blocking all interrupts from the chip.
		 */
		RTL_W16(IntrStatus,
			(status & RxFIFOOver) ? (status | RxOverflow) : status);
		status = RTL_R16(IntrStatus);
	}

	return IRQ_RETVAL(handled);
}

static int rtl8169_poll(struct napi_struct *napi, int budget)
{
	struct rtl8169_private *tp = container_of(napi, struct rtl8169_private, napi);
	struct net_device *dev = tp->dev;
	void __iomem *ioaddr = tp->mmio_addr;
	int work_done;

	work_done = rtl8169_rx_interrupt(dev, tp, ioaddr, (u32) budget);
	rtl8169_tx_interrupt(dev, tp, ioaddr);

	if (work_done < budget) {
		napi_complete(napi);

		/* We need for force the visibility of tp->intr_mask
		 * for other CPUs, as we can loose an MSI interrupt
		 * and potentially wait for a retransmit timeout if we don't.
		 * The posted write to IntrMask is safe, as it will
		 * eventually make it to the chip and we won't loose anything
		 * until it does.
		 */
		tp->intr_mask = 0xffff;
		wmb();
		RTL_W16(IntrMask, tp->intr_event);
	}

	return work_done;
}

static void rtl8169_rx_missed(struct net_device *dev, void __iomem *ioaddr)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	if (tp->mac_version > RTL_GIGA_MAC_VER_06)
		return;

	dev->stats.rx_missed_errors += (RTL_R32(RxMissed) & 0xffffff);
	RTL_W32(RxMissed, 0);
}

static void rtl8169_down(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int intrmask;

	rtl8169_delete_timer(dev);

	netif_stop_queue(dev);

	napi_disable(&tp->napi);

core_down:
	spin_lock_irq(&tp->lock);

	rtl8169_asic_down(ioaddr);

	rtl8169_rx_missed(dev, ioaddr);

	spin_unlock_irq(&tp->lock);

	synchronize_irq(dev->irq);

	/* Give a racing hard_start_xmit a few cycles to complete. */
	synchronize_sched();

	/*
	 * And now for the 50k$ question: are IRQ disabled or not ?
	 *
	 * Two paths lead here:
	 * 1) dev->close
	 *    -> netif_running() is available to sync the current code and the
	 *       IRQ handler. See rtl8169_interrupt for details.
	 * 2) dev->change_mtu
	 *    -> rtl8169_poll can not be issued again and re-enable the
	 *       interruptions. Let's simply issue the IRQ down sequence again.
	 *
	 * No loop if hotpluged or major error (0xffff).
	 */
	intrmask = RTL_R16(IntrMask);
	if (intrmask && (intrmask != 0xffff))
		goto core_down;

	rtl8169_tx_clear(tp);

	rtl8169_rx_clear(tp);
}

static int rtl8169_close(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct pci_dev *pdev = tp->pci_dev;

	pm_runtime_get_sync(&pdev->dev);

	/* update counters before going down */
	rtl8169_update_counters(dev);

	rtl8169_down(dev);

	free_irq(dev->irq, dev);

	dma_free_coherent(&pdev->dev, R8169_RX_RING_BYTES, tp->RxDescArray,
			  tp->RxPhyAddr);
	dma_free_coherent(&pdev->dev, R8169_TX_RING_BYTES, tp->TxDescArray,
			  tp->TxPhyAddr);
	tp->TxDescArray = NULL;
	tp->RxDescArray = NULL;

	pm_runtime_put_sync(&pdev->dev);

	return 0;
}

static void rtl_set_rx_mode(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long flags;
	u32 mc_filter[2];	/* Multicast hash filter */
	int rx_mode;
	u32 tmp = 0;

	if (dev->flags & IFF_PROMISC) {
		/* Unconditionally log net taps. */
		netif_notice(tp, link, dev, "Promiscuous mode enabled\n");
		rx_mode =
		    AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
		    AcceptAllPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else if ((netdev_mc_count(dev) > multicast_filter_limit) ||
		   (dev->flags & IFF_ALLMULTI)) {
		/* Too many to filter perfectly -- accept all multicasts. */
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else {
		struct netdev_hw_addr *ha;

		rx_mode = AcceptBroadcast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0;
		netdev_for_each_mc_addr(ha, dev) {
			int bit_nr = ether_crc(ETH_ALEN, ha->addr) >> 26;
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
			rx_mode |= AcceptMulticast;
		}
	}

	spin_lock_irqsave(&tp->lock, flags);

	tmp = rtl8169_rx_config | rx_mode |
	      (RTL_R32(RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);

	if (tp->mac_version > RTL_GIGA_MAC_VER_06) {
		u32 data = mc_filter[0];

		mc_filter[0] = swab32(mc_filter[1]);
		mc_filter[1] = swab32(data);
	}

	RTL_W32(MAR0 + 4, mc_filter[1]);
	RTL_W32(MAR0 + 0, mc_filter[0]);

	RTL_W32(RxConfig, tmp);

	spin_unlock_irqrestore(&tp->lock, flags);
}

/**
 *  rtl8169_get_stats - Get rtl8169 read/write statistics
 *  @dev: The Ethernet Device to get statistics for
 *
 *  Get TX/RX statistics for rtl8169
 */
static struct net_device_stats *rtl8169_get_stats(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long flags;

	if (netif_running(dev)) {
		spin_lock_irqsave(&tp->lock, flags);
		rtl8169_rx_missed(dev, ioaddr);
		spin_unlock_irqrestore(&tp->lock, flags);
	}

	return &dev->stats;
}

static void rtl8169_net_suspend(struct net_device *dev)
{
	if (!netif_running(dev))
		return;

	netif_device_detach(dev);
	netif_stop_queue(dev);
}

#ifdef CONFIG_PM

static int rtl8169_suspend(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct net_device *dev = pci_get_drvdata(pdev);

	rtl8169_net_suspend(dev);

	return 0;
}

static void __rtl8169_resume(struct net_device *dev)
{
	netif_device_attach(dev);
	rtl8169_schedule_work(dev, rtl8169_reset_task);
}

static int rtl8169_resume(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl8169_init_phy(dev, tp);

	if (netif_running(dev))
		__rtl8169_resume(dev);

	return 0;
}

static int rtl8169_runtime_suspend(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);

	if (!tp->TxDescArray)
		return 0;

	spin_lock_irq(&tp->lock);
	tp->saved_wolopts = __rtl8169_get_wol(tp);
	__rtl8169_set_wol(tp, WAKE_ANY);
	spin_unlock_irq(&tp->lock);

	rtl8169_net_suspend(dev);

	return 0;
}

static int rtl8169_runtime_resume(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);

	if (!tp->TxDescArray)
		return 0;

	spin_lock_irq(&tp->lock);
	__rtl8169_set_wol(tp, tp->saved_wolopts);
	tp->saved_wolopts = 0;
	spin_unlock_irq(&tp->lock);

	rtl8169_init_phy(dev, tp);

	__rtl8169_resume(dev);

	return 0;
}

static int rtl8169_runtime_idle(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);

	if (!tp->TxDescArray)
		return 0;

	rtl8169_check_link_status(dev, tp, tp->mmio_addr);
	return -EBUSY;
}

static const struct dev_pm_ops rtl8169_pm_ops = {
	.suspend = rtl8169_suspend,
	.resume = rtl8169_resume,
	.freeze = rtl8169_suspend,
	.thaw = rtl8169_resume,
	.poweroff = rtl8169_suspend,
	.restore = rtl8169_resume,
	.runtime_suspend = rtl8169_runtime_suspend,
	.runtime_resume = rtl8169_runtime_resume,
	.runtime_idle = rtl8169_runtime_idle,
};

#define RTL8169_PM_OPS	(&rtl8169_pm_ops)

#else /* !CONFIG_PM */

#define RTL8169_PM_OPS	NULL

#endif /* !CONFIG_PM */

static void rtl_shutdown(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	rtl8169_net_suspend(dev);

	/* restore original MAC address */
	rtl_rar_set(tp, dev->perm_addr);

	spin_lock_irq(&tp->lock);

	rtl8169_asic_down(ioaddr);

	spin_unlock_irq(&tp->lock);

	if (system_state == SYSTEM_POWER_OFF) {
		/* WoL fails with some 8168 when the receiver is disabled. */
		if (tp->features & RTL_FEATURE_WOL) {
			pci_clear_master(pdev);

			RTL_W8(ChipCmd, CmdRxEnb);
			/* PCI commit */
			RTL_R8(ChipCmd);
		}

		pci_wake_from_d3(pdev, true);
		pci_set_power_state(pdev, PCI_D3hot);
	}
}

static struct pci_driver rtl8169_pci_driver = {
	.name		= MODULENAME,
	.id_table	= rtl8169_pci_tbl,
	.probe		= rtl8169_init_one,
	.remove		= __devexit_p(rtl8169_remove_one),
	.shutdown	= rtl_shutdown,
	.driver.pm	= RTL8169_PM_OPS,
};

static int __init rtl8169_init_module(void)
{
	return pci_register_driver(&rtl8169_pci_driver);
}

static void __exit rtl8169_cleanup_module(void)
{
	pci_unregister_driver(&rtl8169_pci_driver);
}

module_init(rtl8169_init_module);
module_exit(rtl8169_cleanup_module);
