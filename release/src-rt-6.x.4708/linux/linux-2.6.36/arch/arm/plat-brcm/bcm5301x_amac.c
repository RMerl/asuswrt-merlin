/*
 * Northstar AMAC Ethernet driver
 *
 * 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/bug.h>
#include <linux/ioport.h>
#include <linux/dmapool.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>

#undef	_EXTRA_DEBUG

#define	AMAC_MAX_PACKET	(ETH_FRAME_LEN+ETH_FCS_LEN+2*VLAN_HLEN)


/*
 * RESOURCES
 */
static struct resource amac_regs[4] = {
	{
	.name	= "amac0regs", .flags	= IORESOURCE_MEM,
	.start	= 0x18024000, .end	= 0x18024FFF,
	},
	{
	.name	= "amac1regs", .flags	= IORESOURCE_MEM,
	.start	= 0x18025000, .end	= 0x18025FFF,
	},
	{
	.name	= "amac2regs", .flags	= IORESOURCE_MEM,
	.start	= 0x18026000, .end	= 0x18026FFF,
	},
	{
	.name	= "amac3regs", .flags	= IORESOURCE_MEM,
	.start	= 0x18027000, .end	= 0x18027FFF,
	},

};

static struct resource amac_irqs[4] = {
	{
	.name	= "amac0irq",	.flags	= IORESOURCE_IRQ,
	.start	= 179,  .end	= 179,
	},
	{
	.name	= "amac1irq",	.flags	= IORESOURCE_IRQ,
	.start	= 180,	 .end	= 180,
	},
	{
	.name	= "amac2irq",	.flags	= IORESOURCE_IRQ,
	.start	= 181,	 .end	= 181,
	},
	{
	.name	= "amac3irq",	.flags	= IORESOURCE_IRQ,
	.start	= 182,	 .end	= 182,
	},
};

/*
 * REGISTERS
 *
 * Individual bit-fields aof registers are specificed here
 * for clarity, and the rest of the code will access each field
 * as if it was its own register.
 *
 */
#define	REG_BIT_FIELD(r,p,w)	((reg_bit_field_t){(r),(p),(w)})

#define	GMAC_CTL_TX_ARB_MODE		REG_BIT_FIELD(0x0, 0, 1)
#define	GMAC_CTL_RX_OVFLOW_MODE		REG_BIT_FIELD(0x0, 1, 1)
#define	GMAC_CTL_FLOW_CNTLSRC		REG_BIT_FIELD(0x0, 2, 1)
#define	GMAC_CTL_LINKSTAT_SEL		REG_BIT_FIELD(0x0, 3, 1)
#define	GMAC_CTL_MIB_RESET		REG_BIT_FIELD(0x0, 4, 1)
#define	GMAC_CTL_FLOW_CNTL_MODE		REG_BIT_FIELD(0x0, 5, 2)
#define	GMAC_CTL_NWAY_AUTO_POLL		REG_BIT_FIELD(0x0, 7, 1)
#define	GMAC_CTL_TX_FLUSH		REG_BIT_FIELD(0x0, 8, 1)
#define	GMAC_CTL_RXCLK_DMG		REG_BIT_FIELD(0x0, 16, 2)
#define	GMAC_CTL_TXCLK_DMG		REG_BIT_FIELD(0x0, 18, 2)
#define	GMAC_CTL_RXCLK_DLL		REG_BIT_FIELD(0x0, 20, 1)
#define	GMAC_CTL_TXCLK_DLL		REG_BIT_FIELD(0x0, 21, 1)

#define	GMAC_STAT			REG_BIT_FIELD(0x04, 0, 32)
#define	GMAC_STAT_RX_FIFO_FULL		REG_BIT_FIELD(0x04, 0, 1)
#define	GMAC_STAT_RX_DBUF_FULL		REG_BIT_FIELD(0x04, 1, 1)
#define	GMAC_STAT_RX_IBUF_FULL		REG_BIT_FIELD(0x04, 2, 1)
#define	GMAC_STAT_TX_FIFO_FULL		REG_BIT_FIELD(0x04, 3, 1)
#define	GMAC_STAT_TX_DBUF_FULL		REG_BIT_FIELD(0x04, 4, 1)
#define	GMAC_STAT_TX_IBUF_FULL		REG_BIT_FIELD(0x04, 5, 1)
#define	GMAC_STAT_TX_PAUSE		REG_BIT_FIELD(0x04, 6, 1)
#define	GMAC_STAT_TX_IF_MODE		REG_BIT_FIELD(0x04, 7, 2)
#define	GMAC_STAT_RX_Q_SIZE		REG_BIT_FIELD(0x04, 16, 4)
#define	GMAC_STAT_TX_Q_SIZE		REG_BIT_FIELD(0x04, 20, 4)

#define	GMAC_INTSTAT			REG_BIT_FIELD(0x020, 0, 32)
#define	GMAC_INTSTAT_MIB_RX_OVRUN	REG_BIT_FIELD(0x020, 0, 1)
#define	GMAC_INTSTAT_MIB_TX_OVRUN	REG_BIT_FIELD(0x020, 1, 1)
#define	GMAC_INTSTAT_TX_FLUSH_DONE	REG_BIT_FIELD(0x020, 2, 1)
#define	GMAC_INTSTAT_MII_LINK_CHANGE	REG_BIT_FIELD(0x020, 3, 1)
#define	GMAC_INTSTAT_MDIO_DONE		REG_BIT_FIELD(0x020, 4, 1)
#define	GMAC_INTSTAT_MIB_RX_HALF	REG_BIT_FIELD(0x020, 5, 1)
#define	GMAC_INTSTAT_MIB_TX_HALF	REG_BIT_FIELD(0x020, 6, 1)
#define	GMAC_INTSTAT_TIMER_INT		REG_BIT_FIELD(0x020, 7, 1)
#define	GMAC_INTSTAT_SW_LINK_CHANGE	REG_BIT_FIELD(0x020, 8, 1)
#define	GMAC_INTSTAT_DMA_DESC_ERR	REG_BIT_FIELD(0x020, 10, 1)
#define	GMAC_INTSTAT_DMA_DATA_ERR	REG_BIT_FIELD(0x020, 11, 1)
#define	GMAC_INTSTAT_DMA_PROTO_ERR	REG_BIT_FIELD(0x020, 12, 1)
#define	GMAC_INTSTAT_DMA_RX_UNDERFLOW	REG_BIT_FIELD(0x020, 13, 1)
#define	GMAC_INTSTAT_DMA_RX_OVERFLOW	REG_BIT_FIELD(0x020, 14, 1)
#define	GMAC_INTSTAT_DMA_TX_UNDERFLOW	REG_BIT_FIELD(0x020, 15, 1)
#define	GMAC_INTSTAT_RX_INT		REG_BIT_FIELD(0x020, 16, 1)
#define	GMAC_INTSTAT_TX_INT(q)		REG_BIT_FIELD(0x020, 24+(q), 1)
#define	GMAC_INTSTAT_RX_ECC_SOFT	REG_BIT_FIELD(0x020, 28, 1)
#define	GMAC_INTSTAT_RX_ECC_HARD	REG_BIT_FIELD(0x020, 29, 1)
#define	GMAC_INTSTAT_TX_ECC_SOFT	REG_BIT_FIELD(0x020, 30, 1)
#define	GMAC_INTSTAT_TX_ECC_HARD	REG_BIT_FIELD(0x020, 31, 1)

#define	GMAC_INTMASK			REG_BIT_FIELD(0x024, 0, 32)
#define	GMAC_INTMASK_MIB_RX_OVRUN	REG_BIT_FIELD(0x024, 0, 1)
#define	GMAC_INTMASK_MIB_TX_OVRUN	REG_BIT_FIELD(0x024, 1, 1)
#define	GMAC_INTMASK_TX_FLUSH_DONE	REG_BIT_FIELD(0x024, 2, 1)
#define	GMAC_INTMASK_MII_LINK_CHANGE	REG_BIT_FIELD(0x024, 3, 1)
#define	GMAC_INTMASK_MDIO_DONE		REG_BIT_FIELD(0x024, 4, 1)
#define	GMAC_INTMASK_MIB_RX_HALF	REG_BIT_FIELD(0x024, 5, 1)
#define	GMAC_INTMASK_MIB_TX_HALF	REG_BIT_FIELD(0x024, 6, 1)
#define	GMAC_INTMASK_TIMER_INT		REG_BIT_FIELD(0x024, 7, 1)
#define	GMAC_INTMASK_SW_LINK_CHANGE	REG_BIT_FIELD(0x024, 8, 1)
#define	GMAC_INTMASK_DMA_DESC_ERR	REG_BIT_FIELD(0x024, 10, 1)
#define	GMAC_INTMASK_DMA_DATA_ERR	REG_BIT_FIELD(0x024, 11, 1)
#define	GMAC_INTMASK_DMA_PROTO_ERR	REG_BIT_FIELD(0x024, 12, 1)
#define	GMAC_INTMASK_DMA_RX_UNDERFLOW	REG_BIT_FIELD(0x024, 13, 1)
#define	GMAC_INTMASK_DMA_RX_OVERFLOW	REG_BIT_FIELD(0x024, 14, 1)
#define	GMAC_INTMASK_DMA_TX_UNDERFLOW	REG_BIT_FIELD(0x024, 15, 1)
#define	GMAC_INTMASK_RX_INT		REG_BIT_FIELD(0x024, 16, 1)
#define	GMAC_INTMASK_TX_INT(q)		REG_BIT_FIELD(0x024, 24+(q), 1)
#define	GMAC_INTMASK_RX_ECC_SOFT	REG_BIT_FIELD(0x024, 28, 1)
#define	GMAC_INTMASK_RX_ECC_HARD	REG_BIT_FIELD(0x024, 29, 1)
#define	GMAC_INTMASK_TX_ECC_SOFT	REG_BIT_FIELD(0x024, 30, 1)
#define	GMAC_INTMASK_TX_ECC_HARD	REG_BIT_FIELD(0x024, 31, 1)

#define	GMAC_TIMER			REG_BIT_FIELD(0x028, 0, 32)

#define	GMAC_INTRX_LZY_TIMEOUT		REG_BIT_FIELD(0x100,0,24)
#define	GMAC_INTRX_LZY_FRMCNT		REG_BIT_FIELD(0x100,24,8)

#define	GMAC_PHYACC_DATA		REG_BIT_FIELD(0x180, 0, 16)
#define	GMAC_PHYACC_ADDR		REG_BIT_FIELD(0x180, 16, 5)
#define	GMAC_PHYACC_REG			REG_BIT_FIELD(0x180, 24, 5)
#define	GMAC_PHYACC_WRITE		REG_BIT_FIELD(0x180, 29, 1)
#define	GMAC_PHYACC_GO			REG_BIT_FIELD(0x180, 30, 1)

#define	GMAC_PHYCTL_ADDR		REG_BIT_FIELD(0x188, 0, 5)
#define	GMAC_PHYCTL_MDC_CYCLE		REG_BIT_FIELD(0x188, 16, 7)
#define	GMAC_PHYCTL_MDC_TRANS		REG_BIT_FIELD(0x188, 23, 1)

/* GMAC has 4 Tx queues, all bit-fields are defined per <q> number */
#define	GMAC_TXCTL(q)			REG_BIT_FIELD(0x200+((q)<<6),0,32)
#define	GMAC_TXCTL_TX_EN(q)		REG_BIT_FIELD(0x200+((q)<<6),0,1)
#define	GMAC_TXCTL_SUSPEND(q)		REG_BIT_FIELD(0x200+((q)<<6),1,1)
#define	GMAC_TXCTL_DMALOOPBACK(q)	REG_BIT_FIELD(0x200+((q)<<6),2,1)
#define	GMAC_TXCTL_DESC_ALIGN(q)	REG_BIT_FIELD(0x200+((q)<<6),5,1)
#define	GMAC_TXCTL_OUTSTAND_READS(q)	REG_BIT_FIELD(0x200+((q)<<6),6,2)
#define	GMAC_TXCTL_PARITY_DIS(q)	REG_BIT_FIELD(0x200+((q)<<6),11,1)
#define	GMAC_TXCTL_DNA_ACT_INDEX(q)	REG_BIT_FIELD(0x200+((q)<<6),13,1)
#define	GMAC_TXCTL_EXTADDR(q)		REG_BIT_FIELD(0x200+((q)<<6),16,2)
#define	GMAC_TXCTL_BURST_LEN(q)		REG_BIT_FIELD(0x200+((q)<<6),18,3)
#define	GMAC_TXCTL_PFETCH_CTL(q)	REG_BIT_FIELD(0x200+((q)<<6),21,3)
#define	GMAC_TXCTL_PFETCH_TH(q)		REG_BIT_FIELD(0x200+((q)<<6),24,2)

#define	GMAC_TX_PTR(q)			REG_BIT_FIELD(0x204+((q)<<6),0,12)
#define	GMAC_TX_ADDR_LOW(q)		REG_BIT_FIELD(0x208+((q)<<6),0,32)
#define	GMAC_TX_ADDR_HIGH(q)		REG_BIT_FIELD(0x20c+((q)<<6),0,32)
#define	GMAC_TXSTAT_CURR_DESC(q)	REG_BIT_FIELD(0x210+((q)<<6),0,12)
#define	GMAC_TXSTAT_TXSTATE(q)		REG_BIT_FIELD(0x210+((q)<<6),28,4)
#define	GMAC_TXSTAT_ACT_DESC(q)		REG_BIT_FIELD(0x214+((q)<<6),0,12)
#define	GMAC_TXSTAT_TXERR(q)		REG_BIT_FIELD(0x214+((q)<<6),28,4)

#define	GMAC_RXCTL			REG_BIT_FIELD(0x220,0,32)
#define	GMAC_RXCTL_RX_EN		REG_BIT_FIELD(0x220,0,1)
#define	GMAC_RXCTL_RX_OFFSET		REG_BIT_FIELD(0x220,1,7)
#define	GMAC_RXCTL_SEP_HDR_DESC		REG_BIT_FIELD(0x220,9,1)
#define	GMAC_RXCTL_OFLOW_CONT		REG_BIT_FIELD(0x220,10,1)
#define	GMAC_RXCTL_PARITY_DIS		REG_BIT_FIELD(0x220,11,1)
#define	GMAC_RXCTL_WAIT_COMPLETE	REG_BIT_FIELD(0x220,12,1)
#define	GMAC_RXCTL_DMA_ACT_INDEX	REG_BIT_FIELD(0x220,13,1)
#define	GMAC_RXCTL_EXTADDR		REG_BIT_FIELD(0x220,16,2)
#define	GMAC_RXCTL_BURST_LEN		REG_BIT_FIELD(0x220,18,3)
#define	GMAC_RXCTL_PFETCH_CTL		REG_BIT_FIELD(0x220,21,3)
#define	GMAC_RXCTL_PFETCH_TH		REG_BIT_FIELD(0x220,24,2)
#define	GMAC_RX_PTR			REG_BIT_FIELD(0x224,0,12)
#define	GMAC_RX_ADDR_LOW		REG_BIT_FIELD(0x228,0,32)
#define	GMAC_RX_ADDR_HIGH		REG_BIT_FIELD(0x22c,0,32)
#define	GMAC_RXSTAT_CURR_DESC		REG_BIT_FIELD(0x230,0,12)
#define	GMAC_RXSTAT_RXSTATE		REG_BIT_FIELD(0x230,28,4)
#define	GMAC_RXSTAT_ACT_DESC		REG_BIT_FIELD(0x234,0,12)
#define	GMAC_RXSTAT_RXERR		REG_BIT_FIELD(0x234,28,4)

#define	UMAC_CORE_VERSION		REG_BIT_FIELD(0x800,0,32)
#define	UMAC_HD_FC_ENA			REG_BIT_FIELD(0x804,0,1)
#define	UMAC_HD_FC_NKOFF		REG_BIT_FIELD(0x804,1,1)
#define	UMAC_IPG_CONFIG_RX		REG_BIT_FIELD(0x804,2,5)

#define	UMAC_CONFIG			REG_BIT_FIELD(0x808,0,32)
#define	UMAC_CONFIG_TX_EN		REG_BIT_FIELD(0x808,0,1)
#define	UMAC_CONFIG_RX_EN		REG_BIT_FIELD(0x808,1,1)
#define	UMAC_CONFIG_ETH_SPEED		REG_BIT_FIELD(0x808,2,2)
#define	UMAC_CONFIG_PROMISC		REG_BIT_FIELD(0x808,4,1)
#define	UMAC_CONFIG_PAD_EN		REG_BIT_FIELD(0x808,5,1)
#define	UMAC_CONFIG_CRC_FW		REG_BIT_FIELD(0x808,6,1)
#define	UMAC_CONFIG_PAUSE_FW		REG_BIT_FIELD(0x808,7,1)
#define	UMAC_CONFIG_RX_PAUSE_IGN	REG_BIT_FIELD(0x808,8,1)
#define	UMAC_CONFIG_TX_ADDR_INS		REG_BIT_FIELD(0x808,9,1)
#define	UMAC_CONFIG_HD_ENA		REG_BIT_FIELD(0x808,10,1)
#define	UMAC_CONFIG_SW_RESET		REG_BIT_FIELD(0x808,11,1)
#define	UMAC_CONFIG_LCL_LOOP_EN		REG_BIT_FIELD(0x808,15,1)
#define	UMAC_CONFIG_AUTO_EN		REG_BIT_FIELD(0x808,22,1)
#define	UMAC_CONFIG_CNTLFRM_EN		REG_BIT_FIELD(0x808,23,1)
#define	UMAC_CONFIG_LNGTHCHK_DIS	REG_BIT_FIELD(0x808,24,1)
#define	UMAC_CONFIG_RMT_LOOP_EN		REG_BIT_FIELD(0x808,25,1)
#define	UMAC_CONFIG_PREAMB_EN		REG_BIT_FIELD(0x808,27,1)
#define	UMAC_CONFIG_TX_PAUSE_IGN	REG_BIT_FIELD(0x808,28,1)
#define	UMAC_CONFIG_TXRX_AUTO_EN	REG_BIT_FIELD(0x808,29,1)

#define	UMAC_MACADDR_LOW		REG_BIT_FIELD(0x80c,0,32)
#define	UMAC_MACADDR_HIGH		REG_BIT_FIELD(0x810,0,16)
#define	UMAC_FRM_LENGTH			REG_BIT_FIELD(0x814,0,14)
#define	UMAC_PAUSE_QUANT		REG_BIT_FIELD(0x818,0,16)

#define	UMAC_MAC_STAT			REG_BIT_FIELD(0x844,0,32)
#define	UMAC_MAC_SPEED			REG_BIT_FIELD(0x844,0,2)
#define	UMAC_MAC_DUPLEX			REG_BIT_FIELD(0x844,2,1)
#define	UMAC_MAC_RX_PAUSE		REG_BIT_FIELD(0x844,3,1)
#define	UMAC_MAC_TX_PAUSE		REG_BIT_FIELD(0x844,4,1)
#define	UMAC_MAC_LINK			REG_BIT_FIELD(0x844,5,1)

#define	UMAC_FRM_TAG0			REG_BIT_FIELD(0x848,0,16)
#define	UMAC_FRM_TAG1			REG_BIT_FIELD(0x84c,0,16)
#define	UMAC_IPG_CONFIG_TX		REG_BIT_FIELD(0x85c,0,5)

#define	UMAC_PAUSE_TIMER		REG_BIT_FIELD(0xb30,0,17)
#define	UMAC_PAUSE_CONTROL_EN		REG_BIT_FIELD(0xb30,17,1)
#define	UMAC_TXFIFO_FLUSH		REG_BIT_FIELD(0xb34,0,1)
#define	UMAC_RXFIFO_STAT		REG_BIT_FIELD(0xb38,0,2)

#define	GMAC_MIB_COUNTERS_OFFSET	0x300

/* GMAC MIB counters structure, with same names as link_stats_64 if exists */
struct gmac_mib_counters {
	/* TX stats */
	u64	tx_bytes;		/* 0x300-0x304 */
        u32	tx_packets;		/* 0x308 */
	u64	tx_all_bytes;		/* 0x30c-0x310 */
	u32	tx_all_packets;		/* 0x314 */
	u32	tx_bcast_packets;	/* 0x318 */
	u32	tx_mcast_packets;	/* 0x31c */
	u32	tx_64b_packets;		/* 0x320 */
	u32	tx_127b_packets;	/* 0x324 */
	u32	tx_255b_packets;	/* 0x328 */
	u32	tx_511b_packets;	/* 0x32c */
	u32	tx_1kb_packets;		/* 0x330 */
	u32	tx_1_5kb_packets;	/* 0x334 */
	u32	tx_2kb_packets;		/* 0x338 */
	u32	tx_4kb_pakcets;		/* 0x33c */
	u32	tx_8kb_pakcets;		/* 0x340 */
	u32	tx_max_pakcets;		/* 0x344 */
	u32	tx_jabbers;		/* 0x348 */
	u32	tx_oversize;		/* 0x34c */
	u32	tx_runt;		/* 0x350 */
        u32	tx_fifo_errors;		/* 0x354 */
	u32	collisions;		/* 0x358 */
	u32	tx_1_col_packets;	/* 0x35c */
	u32	tx_m_col_packets;	/* 0x360 */
        u32	tx_aborted_errors;	/* 0x364 */
	u32	tx_window_errors;	/* 0x368 */
	u32	tx_deferred_packets;	/* 0x36c */
	u32	tx_carrier_errors;	/* 0x370 */
	u32	tx_pause_packets;	/* 0x374 */
	u32	tx_unicast_packets;	/* 0x378 */
	u32	tx_qos_0_packets;	/* 0x37c */
	u64	tx_qos_0_bytes;		/* 0x380-0x384 */
	u32	tx_qos_1_packets;	/* 0x388 */
	u64	tx_qos_1_bytes;		/* 0x38c-0x390 */
	u32	tx_qos_2_packets;	/* 0x394 */
	u64	tx_qos_2_bytes;		/* 0x398-0x39c */
	u32	tx_qos_3_packets;	/* 0x3a0 */
	u64	tx_qos_3_bytes;		/* 0x3a4-0x3a8 */
	u32	reserved[1];		/* 0x3ac */

	/* RX stats */
        u64	rx_bytes;		/* 0x3b0-0x3b4 */
	u32	rx_packets;		/* 0x3b8 */
	u64	rx_all_bytes;		/* 0x3bc-0x3c0 */
	u32	rx_all_packets;		/* 0x3c4 */
	u32	rx_bcast_packets;	/* 0x3c8 */
	u32	multicast;		/* 0x3cc */
	u32	rx_64b_packets;		/* 0x3d0 */
	u32	rx_127b_packets;	/* 0x3d4 */
	u32	rx_255b_packets;	/* 0x3d8 */
	u32	rx_511b_packets;	/* 0x3dc */
	u32	rx_1kb_packets;		/* 0x3e0 */
	u32	rx_1_5kb_packets;	/* 0x3e4 */
	u32	rx_2kb_packets;		/* 0x3e8 */
	u32	rx_4kb_packets;		/* 0x3ec */
	u32	rx_8kb_packets;		/* 0x3f0 */
	u32	rx_max_packets;		/* 0x3f4 */
	u32	rx_jabbers;		/* 0x3f8 */
	u32	rx_length_errors;	/* 0x3fc */
	u32	rx_runt_bad_packets;	/* 0x400 */
	u32	rx_over_errors;         /* 0x404 */
	u32	rx_crc_or_align_errors;	/* 0x408 */
	u32	rx_runt_good_packets;	/* 0x40c */
	u32	rx_crc_errors;		/* 0x410 */
	u32	rx_frame_errors;	/* 0x414 */
	u32	rx_missed_errors;	/* 0x418 */
	u32	rx_pause_packets;	/* 0x41c */
	u32	rx_control_packets;	/* 0x420 */
	u32	rx_src_mac_chanhges;	/* 0x424 */
	u32	rx_unicast_packets;	/* 0x428 */
}__attribute__((packed)) ;

/*
 * Register bit-field manipulation routines
 * NOTE: These compile to just a few machine instructions in-line
 */

typedef	struct {unsigned reg, pos, width;} reg_bit_field_t ;

static unsigned inline _reg_read( struct net_device *dev,  reg_bit_field_t rbf )
{
	void * __iomem base = (void *) dev->base_addr;
	unsigned val ;

	val =  __raw_readl( base + rbf.reg);
	val >>= rbf.pos;
	val &= (1 << rbf.width)-1;

	return val;
}

static void inline _reg_write( struct net_device *dev,  
	reg_bit_field_t rbf, unsigned newval )
{
	void * __iomem base = (void *) dev->base_addr;
	unsigned val, msk ;

	msk = (1 << rbf.width)-1;
	msk <<= rbf.pos;
	newval <<= rbf.pos;
	newval &= msk ;

	val =  __raw_readl( base + rbf.reg);
	val &= ~msk ;
	val |= newval ;
	__raw_writel( val, base + rbf.reg);
}

/*
 * BUFFER DESCRIPTORS
 */
#define	GMAC_DESC_SIZE_SHIFT	4	/* Descriptors are 16 bytes in size */

/* 
  This will only work in Little-Endian regime.
  Descriptors defined witb bit-fields, for best optimization,
  assuming LSBs get allocated first
 */
typedef struct {
	unsigned
		desc_rsrvd_0	: 20,	/* Reserved, must be 0 */
		desc_flags	: 8,	/* Flags, i.e. CRC generation mode */
		desc_eot	: 1,	/* End-of-Table indicator */
		desc_int_comp	: 1,	/* Interrupt on completion */
		desc_eof	: 1,	/* End-of-Frame */
		desc_sof	: 1,	/* Start-of-Frame */
		desc_buf_sz	: 14,	/* Data buffer length (bytes) */
		desc_resrvd_1	: 1,	/* Reserved, must be 0 */
		desc_addr_ext	: 2,	/* AddrExt, not used, must be 0 */
		desc_parity	: 1,	/* Parity bit for even desc parity */
		desc_resrvd_2	: 12;	/* Reserved, must be 0 */
	u64	desc_data_ptr	: 64;	/* Data buffer 64-bit pointer */
} gmac_desc_t;

typedef	struct {
	unsigned
		rxstat_framelen	: 16,	/* Actual received byte count */
		rxstat_type	: 2,	/* Type: 0:uni, 1:multi, 2:broadcast */
		rxstat_vlan	: 1,	/* VLAN Tag detected */
		rxstat_crc_err	: 1,	/* CRC error */
		rxstat_oversize	: 1,	/* Oversize, truncated packet */
		rxstat_ctf	: 1,	/* Processed by CTF */
		rxstat_ctf_err	: 1,	/* Error in CTF processing */
		rxstat_oflow	: 1,	/* Overflow in packet reception */
		rxstat_desc_cnt	: 4,	/* Number of descriptors - 1 */
		rxstat_datatype	: 4;	/* Data type, not used */
} gmac_rxstat_t;

#define	GMAC_RX_DESC_COUNT	256	/* Number of Rx Descriptors */
#define	GMAC_TX_DESC_COUNT	256	/* Number of Tx Descriptors */

/*
 * PRIVATE DEVICE DATA STRUCTURE
 */

struct amac_ring {
	unsigned	count,		/* Total # of elements */
			ix_in,		/* Producer's index */
			ix_out,		/* Consumer index */
			spare;
};

struct amac_priv {
	spinlock_t		rx_lock;
	struct net_device_stats	counters;
	spinlock_t		tx_lock;
	struct amac_ring	rx_ring, tx_ring;
	dma_addr_t		rx_desc_paddr, tx_desc_paddr ;
	void *			tx_desc_start;
	void *			rx_desc_start;
	struct sk_buff 	**	rx_skb,
			**	tx_skb;
	struct napi_struct	rx_napi, tx_napi;
	unsigned		rx_coal_usec, rx_coal_frames;
	unsigned		tx_coal_usec, tx_coal_frames;
	bool			tx_active;
	u8			unit;
} ;

/*
 * Forward declarations
 */
static int amac_tx_fini( struct net_device * dev, unsigned q, unsigned quota );
static int amac_rx_fill( struct net_device * dev, int quant );
static irqreturn_t  amac_interrupt( int irq,  void * _dev );

/*
 * Ring manipulation routines
 */
static int inline _ring_put( struct amac_ring * r )
{
	int ix, ret;

	ix = r->ix_in + 1;
	if( ix >= r->count )
		ix = 0;
	if( ix == r->ix_out )
		return -1;
	ret = r->ix_in;
	r->ix_in = ix;
	return ret;
}

static int inline _ring_get( struct amac_ring * r, unsigned stop_ix )
{
	int ix;

	if( r->ix_in == r->ix_out )
		return -1;
	if( r->ix_out == stop_ix )
		return -2;
	ix = r->ix_out ;
	r->ix_out = ix + 1;
	if( r->ix_out >= r->count )
		r->ix_out = 0;
	return ix;
}

static int inline _ring_room( struct amac_ring * r )
{
	int i;

	i = r->ix_out;
	if( i <= r->ix_in )
		i += r->count;
	i = i - r->ix_in - 1 ;

	BUG_ON(i > r->count || i <= 0 );
	return i;
}

static int inline _ring_members( struct amac_ring * r )
{
	int i;

	if( r->ix_in >= r->ix_out )
		i = r->ix_in - r->ix_out ;
	else
		i = r->count + r->ix_in - r->ix_out ;

	BUG_ON(i >= r->count || i < 0 );
	return i;
}

static void amac_desc_parity( gmac_desc_t *d )
{
	u32 r, * a = (void *) d;
	
	r = a[0] ^ a[1] ^ a[2] ^ a[3];
	r = 0xFFFF & ( r ^ (r>>16) );
	r = 0xFF & ( r ^ (r>>8) );
	r = 0xF & ( r ^ (r>>4) );
	r = 0x3 & ( r ^ (r>>2) );
	r = 0x1 & ( r ^ (r>>1) );
	d->desc_parity ^= r ;
}

static void amac_tx_show( struct net_device * dev )
{
	struct amac_priv * priv = netdev_priv(dev);
	unsigned q = 0;

	printk("%s: Tx ring in %#x out %#x "
		"Curr %#x Act %#x Last %#x State %#x Err %#x\n"
		,
		dev->name, 
		priv->tx_ring.ix_in, 
		priv->tx_ring.ix_out,
		_reg_read(dev,  GMAC_TXSTAT_CURR_DESC(q)),
		_reg_read(dev,  GMAC_TXSTAT_ACT_DESC(q)),
		_reg_read(dev,  GMAC_TX_PTR(q)),
		_reg_read(dev, GMAC_TXSTAT_TXSTATE(0)),
		_reg_read(dev, GMAC_TXSTAT_TXERR(0))
		);
}

/*
 * Network device method implementation
 */
static void amac_change_rx_flags(struct net_device *dev, int flags)
{
	if( dev->flags & IFF_PROMISC )
		_reg_write( dev, UMAC_CONFIG_PROMISC, 1);
	else
		_reg_write( dev, UMAC_CONFIG_PROMISC, 0);
	/* No MC-filtering in hardware, ignore IFF_ALLMULTI */
}

static void amac_set_hw_addr(struct net_device *dev)
{
	u32 hw_addr[2];

	hw_addr[0] = 	dev->dev_addr[ 3 ] |
			dev->dev_addr[ 2 ] << 8 |
			dev->dev_addr[ 1 ] << 16 |
			dev->dev_addr[ 0 ] << 24;
	hw_addr[1] =	dev->dev_addr[ 5 ] |
			dev->dev_addr[ 4 ] << 8;

	_reg_write( dev, UMAC_MACADDR_LOW, hw_addr[0] );
	_reg_write( dev, UMAC_MACADDR_HIGH, hw_addr[1] );
}

/* SHOULD BE DELETED ? */
static void amac_set_mc_list(struct net_device *dev)
{


}

static struct rtnl_link_stats64 * amac_get_stats64(
		struct net_device *dev,
		struct rtnl_link_stats64 * dest)
{
	struct gmac_mib_counters * mib_regs = (void *) GMAC_MIB_COUNTERS_OFFSET;
	
	/* Validate MIB counter structure definition accuracy */
	WARN_ON( (u32)&mib_regs->rx_unicast_packets != (0x428) );

	/* Get tx_dropped count from <txq> */
	dev_txq_stats_fold( dev, dest );

	/* Linux caused rx drops, we have to count these in the driver */
	dest->rx_dropped 	= dev->stats.rx_dropped ;

	/* there is no register to count these */
	dest->rx_fifo_errors	= dev->stats.rx_fifo_errors ;

	mib_regs = (void *)(dev->base_addr + GMAC_MIB_COUNTERS_OFFSET);

	/* Get the appropriate MIB counters */
	dest->rx_packets	= mib_regs->rx_packets ;
	dest->tx_packets	= mib_regs->tx_packets ;

	dest->multicast 	= mib_regs->multicast ;
	dest->collisions 	= mib_regs->collisions ;

	dest->rx_length_errors	= mib_regs->rx_length_errors ;
	dest->rx_over_errors	= mib_regs->rx_over_errors ;
	dest->rx_crc_errors	= mib_regs->rx_crc_errors ;
	dest->rx_frame_errors	= mib_regs->rx_frame_errors ;
	dest->rx_missed_errors	= mib_regs->rx_missed_errors ;

	dest->tx_aborted_errors	= mib_regs->tx_aborted_errors ;
	dest->tx_fifo_errors	= mib_regs->tx_fifo_errors ;
	dest->tx_window_errors	= mib_regs->tx_window_errors ;
	dest->tx_heartbeat_errors	= mib_regs->tx_m_col_packets ;


	/* These are cummulative error counts for all types of errors */
	dest->rx_errors 	= 
		dest->rx_length_errors	+
		dest->rx_over_errors	+
		dest->rx_crc_errors	+
		dest->rx_frame_errors	+
		dest->rx_fifo_errors	+
		dest->rx_missed_errors	;
	dest->tx_errors 	= 
		dest->tx_aborted_errors	+
		dest->tx_carrier_errors	+
		dest->tx_fifo_errors	+
		dest->tx_window_errors	+
		dest->tx_heartbeat_errors;

	/* These are 64-bit MIB counters */
	dest->rx_bytes		= mib_regs->rx_bytes ;
	dest->tx_bytes		= mib_regs->tx_bytes ;

	return dest ;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void amac_poll_controller(struct net_device *dev)
{
	u32 int_msk ;

	/* Disable device interrupts */
	int_msk = _reg_read( dev, GMAC_INTMASK );
	_reg_write( dev, GMAC_INTMASN, 0 );

	/* Call the interrupt service routine */
	amac_interrupt, dev->irq, dev );

	/* Re-enable interrupts */
	_reg_write( dev, GMAC_INTMASN, int_msk );
}
#endif

static int amac_rx_pkt( struct net_device *dev, struct sk_buff * skb)
{
	struct amac_priv * priv = netdev_priv(dev);
	gmac_rxstat_t rxstat ;
	unsigned rx_len;

	/* Mark ownership */
	skb->dev = dev ;

	/* Fetch <rxstat> from start of data buffer */
	memcpy( &rxstat, skb->data, sizeof(rxstat) );

	/* Adjust valid packet length is <skb> */
	rx_len = rxstat.rxstat_framelen;
	skb_put(skb, rx_len+sizeof(rxstat) );
	skb_pull( skb, sizeof(rxstat) );

	/* If bad packet, count errors, otherwise ingest it */
	if( rxstat.rxstat_crc_err ) {
		priv->counters.rx_crc_errors ++ ;
		goto _pkt_err;
	} else if( rxstat.rxstat_oversize ) {
		priv->counters.rx_over_errors ++ ;
		goto _pkt_err;
	} else if( rxstat.rxstat_oflow ) {
		priv->counters.rx_fifo_errors ++ ;
		goto _pkt_err;
	}

	/* Must be good packet, handle it */
	skb->protocol = eth_type_trans( skb, dev );

	priv->counters.rx_packets ++;
	priv->counters.rx_bytes += skb->len;
	return( netif_receive_skb( skb ) );

_pkt_err:
	priv->counters.rx_errors ++ ;
	dev_kfree_skb_any( skb );
	return NET_RX_SUCCESS ;
}

static int amac_rx_do( struct net_device *dev, int quota, bool * done )
{
	struct amac_priv * priv = netdev_priv(dev);
	struct sk_buff * skb ;
	gmac_desc_t * rx_desc ;
	unsigned curr_rx_ix, npackets = 0 ;
	int ix, res;

	* done = false ;

	/* Get currently used Rx descriptor index */
	curr_rx_ix = _reg_read( dev,  GMAC_RXSTAT_CURR_DESC );
	curr_rx_ix >>= GMAC_DESC_SIZE_SHIFT ;
	BUG_ON( curr_rx_ix >= priv->rx_ring.count );

	while( npackets < quota) {
		if( ! spin_trylock( & priv->rx_lock ) ) {
#ifdef	_EXTRA_DEBUG
			printk("%s: lock busy\n", __FUNCTION__ );
#endif
			return npackets ;
			}
		ix = _ring_get( &priv->rx_ring, curr_rx_ix );
		spin_unlock( & priv->rx_lock );

		if( ix < 0 ) {
			* done = true ;
			break;
			}

#ifdef	_EXTRA_DEBUG
		printk("%s: ix=%#x\n", __FUNCTION__, ix );
#endif

		/* Process the descriptor */
		rx_desc = priv->rx_desc_start + (ix<<GMAC_DESC_SIZE_SHIFT);

		/* Unmap buffer from DMA */
		dma_unmap_single( &dev->dev, rx_desc->desc_data_ptr,
			rx_desc->desc_buf_sz, DMA_FROM_DEVICE );

		/* Extract <skb> from descriptor */
		skb = priv->rx_skb[ ix ];

		/* Clear descriptor */
		rx_desc->desc_data_ptr = 0;
		rx_desc->desc_buf_sz = 0;
		priv->rx_skb[ ix ] = NULL;

		/* Process incoming packet */
		res = amac_rx_pkt( dev, skb );

		/* Stop if kernel is congested */
		if( res == NET_RX_DROP ) {
			dev->stats.rx_dropped ++;
			break;
			}

		/* Count processed packets */
		npackets ++ ;
	} 
	return npackets ;
}

static int amac_rx_fill( struct net_device * dev, int quant )
{
	struct amac_priv * priv = netdev_priv(dev);
	struct sk_buff * skb ;
	gmac_desc_t * rx_desc ;
	unsigned room, size, off, count = 0;
	unsigned saved_ix_in;
	int ix, last_ix= -1 ;
	dma_addr_t paddr;

	/* All Rx buffers over 2K for now */
	size = AMAC_MAX_PACKET + sizeof(gmac_rxstat_t) + L1_CACHE_BYTES ;

	BUG_ON( size <= dev->mtu + sizeof(gmac_rxstat_t));

	do {
		if( ! spin_trylock( &priv->rx_lock )) {
#ifdef	_EXTRA_DEBUG
			printk("%s: lock busy\n", __FUNCTION__ );
#endif
			break;
		}
		saved_ix_in = priv->rx_ring.ix_in;
		ix = _ring_put( &priv->rx_ring );
		spin_unlock( &priv->rx_lock );

		if( ix < 0 )
			break;

#ifdef	_EXTRA_DEBUG
		printk("%s: ix=%#x\n", __FUNCTION__, ix );
#endif

		/* Bail if slot is not empty (should not happen) */
		if( unlikely(priv->rx_skb[ ix ]) )
			continue;

		/* Fill a buffer into empty descriptor */
		rx_desc = priv->rx_desc_start + (ix<<GMAC_DESC_SIZE_SHIFT);

		/* Allocate new buffer */
		skb = dev_alloc_skb( size );
		if( IS_ERR_OR_NULL(skb) )
			break;

		/* Mark ownership */
		skb->dev = dev ;

		/* Save <skb> pointer */
		priv->rx_skb[ ix ] = skb;

		/* Update descriptor with new buffer */
		BUG_ON( rx_desc->desc_data_ptr );

		room = skb_tailroom( skb );

		paddr = dma_map_single( &dev->dev, skb->data, room,
				 DMA_FROM_DEVICE );

		if( dma_mapping_error( &dev->dev, paddr )) {
			printk_ratelimited(KERN_WARNING
				"%s: failed to map Rx buffer\n", dev->name);
			priv->rx_skb[ ix ] = NULL;
			priv->rx_ring.ix_in = saved_ix_in ;
			dev_kfree_skb_any( skb );
			break;
		}

		rx_desc->desc_parity = 0;
		rx_desc->desc_data_ptr =  paddr ;
		rx_desc->desc_buf_sz = room ;
		rx_desc->desc_sof =
		rx_desc->desc_eof = 1;	/* One pakcet per desc */

		/* Mark EoT for last desc */
		if( (ix+1) == priv->tx_ring.count )
			rx_desc->desc_eot = 1;

		/* calculate and set descriptor parity */
		amac_desc_parity( rx_desc );

		count ++ ;
		last_ix = ix;
		} while( --quant > 0 );

	barrier();

	/* Tell DMA where the last valid descriptor is */
	if( last_ix >= 0 ) {
		if( (++ last_ix) >= priv->rx_ring.count )
			last_ix = 0;
		off = last_ix << GMAC_DESC_SIZE_SHIFT ;
		_reg_write( dev, GMAC_RX_PTR, priv->rx_desc_paddr + off);
	}
	return count ;
}

/*
 * NAPI polling functions
 */
static int amac_rx_poll( struct napi_struct *napi, int budget )
{
	struct net_device * dev = napi->dev;
	struct amac_priv * priv = netdev_priv(dev);
	unsigned npackets, nbuffs, i ;
	bool rx_done ;

	npackets = amac_rx_do( dev, budget, & rx_done );

	/* out of budget, come back for more */
	if( npackets >= budget )
		return npackets ;

	/* If too few Rx Buffers, fill up more */
	nbuffs = _ring_members( &priv->rx_ring );

	/* Keep Rx buffs between <budget> and 2*<budget> */
	i = min( (budget << 1) - nbuffs, budget - npackets - 1);
	npackets += amac_rx_fill( dev, i );

	/* Must not call napi_complete() if used up all budget */
	if( npackets >= budget )
		return npackets ;

	if( rx_done ) {
		/* Done for now, re-enable interrupts */
		napi_complete(napi);
		_reg_write( dev, GMAC_INTMASK_RX_INT, 1);
	}

	return npackets ;
}

static int amac_tx_poll( struct napi_struct *napi, int budget )
{
	struct net_device * dev = napi->dev;
	struct amac_priv * priv = netdev_priv(dev);
	unsigned count ;
	const unsigned q = 0 ;

	count = amac_tx_fini( dev, q, budget );

	if( count >= budget )
		return count ;

	if( ! priv->tx_active ) {
		napi_complete(napi);
		_reg_write( dev, GMAC_INTMASK_TX_INT(q), 1);
		}

	return count ;
}

static irqreturn_t  amac_interrupt( int irq,  void * _dev )
{
	struct net_device * dev = _dev ;
	struct amac_priv * priv = netdev_priv(dev);
	const unsigned q = 0;
	u32 reg ;

	reg = _reg_read( dev, GMAC_INTSTAT);

#ifdef	_EXTRA_DEBUG
	if( reg ) {
		char msg[32] = "";
		if( _reg_read( dev, GMAC_INTSTAT_RX_INT) ) 
			strcat( msg, "Rx ");
		if( _reg_read( dev, GMAC_INTSTAT_TX_INT(q)))
			strcat( msg, "Tx ");
		if( _reg_read( dev, GMAC_INTSTAT_TIMER_INT))
			strcat( msg, "W ");
		printk("%s: %s\n", __FUNCTION__, msg );
	}
#endif

	if( reg == 0 )
		return IRQ_NONE;

	/* Decode interrupt causes */
	if( _reg_read( dev, GMAC_INTSTAT_RX_INT) ) {
		/* Disable Rx interrupt */
		_reg_write( dev, GMAC_INTMASK_RX_INT, 0);
		_reg_write( dev, GMAC_INTSTAT_RX_INT, 1);
		/* Schedule Rx processing */
		napi_schedule( &priv->rx_napi );
		}
	if( _reg_read( dev, GMAC_INTSTAT_TX_INT(q)) ||
	    _reg_read( dev, GMAC_INTSTAT_TIMER_INT)) {

		/* Shut-off Tx/Timer interrupts */
		_reg_write( dev, GMAC_TIMER, 0 );
		_reg_write( dev, GMAC_INTMASK_TIMER_INT, 0);
		_reg_write( dev, GMAC_INTSTAT_TIMER_INT, 1);
		_reg_write( dev, GMAC_INTMASK_TX_INT(q), 0);
		_reg_write( dev, GMAC_INTSTAT_TX_INT(q), 1);

		/* Schedule cleanup of Tx buffers */
		napi_schedule( &priv->tx_napi );
		}
	if( _reg_read( dev, GMAC_INTSTAT_MII_LINK_CHANGE)) {
		printk_ratelimited( "%s: MII Link Change\n", dev->name );
		_reg_write( dev, GMAC_INTSTAT_MII_LINK_CHANGE, 1);
		}
	if( _reg_read( dev, GMAC_INTSTAT_SW_LINK_CHANGE)) {
		printk_ratelimited( "%s: Switch Link Change\n", dev->name );
		_reg_write( dev, GMAC_INTSTAT_SW_LINK_CHANGE, 1);
		goto _dma_error;
		}
	if( _reg_read( dev, GMAC_INTSTAT_DMA_DESC_ERR)) {
		printk_ratelimited( "%s: DMA Desciptor Error\n", dev->name );
		napi_schedule( &priv->tx_napi );
		goto _dma_error;
		}
	if( _reg_read( dev, GMAC_INTSTAT_DMA_DATA_ERR)) {
		printk_ratelimited( "%s: DMA Data Error\n", dev->name );
		_reg_write( dev, GMAC_INTSTAT_DMA_DATA_ERR, 1);
		goto _dma_error;
		}
	if( _reg_read( dev, GMAC_INTSTAT_DMA_PROTO_ERR)) {
		printk_ratelimited( "%s: DMA Protocol Error\n", dev->name );
		_reg_write( dev, GMAC_INTSTAT_DMA_PROTO_ERR, 1);
		goto _dma_error;
		}
	if( _reg_read( dev, GMAC_INTSTAT_DMA_RX_UNDERFLOW)) {
		printk_ratelimited( "%s: DMA Rx Undeflow\n", dev->name );
		dev->stats.rx_fifo_errors ++ ;
		_reg_write( dev, GMAC_INTSTAT_DMA_RX_UNDERFLOW, 1);
		goto _dma_error;
		}
	if( _reg_read( dev, GMAC_INTSTAT_DMA_RX_OVERFLOW)) {
		printk_ratelimited( "%s: DMA Rx Overflow\n", dev->name );
		dev->stats.rx_fifo_errors ++ ;
		_reg_write( dev, GMAC_INTSTAT_DMA_RX_OVERFLOW, 1);
		goto _dma_error;
		}
	if( _reg_read( dev, GMAC_INTSTAT_DMA_TX_UNDERFLOW)) {
		printk_ratelimited( "%s: DMA Rx Underflow\n", dev->name );
		_reg_write( dev, GMAC_INTSTAT_DMA_TX_UNDERFLOW, 1);
		goto _dma_error;
		}

	return IRQ_HANDLED;

_dma_error:
	napi_schedule( &priv->tx_napi );
	return IRQ_HANDLED;
}


static u16 amac_select_queue(struct net_device *dev, struct sk_buff *skb)
{
/* Don't know how to do this yet */
return 0;
}

static int amac_tx_fini( struct net_device * dev, unsigned q, unsigned quota )
{
	struct amac_priv * priv = netdev_priv(dev);
	gmac_desc_t * tx_desc ;
	struct sk_buff * skb ;
	unsigned curr_tx_ix, count = 0 ;
	int ix;

	/* Get currently used Tx descriptor index */
	curr_tx_ix = _reg_read( dev,  GMAC_TXSTAT_CURR_DESC(q) );
	curr_tx_ix >>= GMAC_DESC_SIZE_SHIFT ;
	BUG_ON( curr_tx_ix >= priv->tx_ring.count );

	while( count < quota ) {
		if( ! spin_trylock( &priv->tx_lock )) {
#ifdef	_EXTRA_DEBUG
			printk("%s: lock busy\n", __FUNCTION__);
#endif
			break;
		}
		ix = _ring_get( &priv->tx_ring, curr_tx_ix);
		spin_unlock( &priv->tx_lock );

		if( ix < 0 ) {
			priv->tx_active = false ;
			break;
			}

#ifdef	_EXTRA_DEBUG
		printk("%s: ix=%#x curr_ix+%#x\n", __FUNCTION__, ix, curr_tx_ix );
#endif

		if( unlikely( priv->tx_skb[ix] == NULL)) {
			priv->tx_active = false ;
			break;
			}

		tx_desc = priv->tx_desc_start + (ix<<GMAC_DESC_SIZE_SHIFT);

		/* Unmap <skb> from DMA */
		dma_unmap_single( &dev->dev, tx_desc->desc_data_ptr,
			tx_desc->desc_buf_sz, DMA_TO_DEVICE );

		/* get <skb> to release */
		skb = priv->tx_skb[ ix ];
		priv->tx_skb[ ix ] = NULL;

		/* Mark descriptor free */
		memset( tx_desc, 0, sizeof( * tx_desc ));

		/* Release <skb> */
		dev_kfree_skb_any( skb );

		count ++ ;
	}

	/* Resume stalled transmission */
	if( count && netif_queue_stopped( dev ))
		netif_wake_queue( dev );

	return count ;
}

static netdev_tx_t amac_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct amac_priv * priv = netdev_priv(dev);
	dma_addr_t paddr;
	gmac_desc_t *tx_desc, desc ;
	u16 len;
	int ix, room;
	unsigned off;
	unsigned const q=0;

	BUG_ON( skb_shinfo(skb)->nr_frags != 0 );/* S/G not implemented yet */

	/* tx_lock already taken at device level */
	ix = _ring_put( &priv->tx_ring );

#ifdef	_EXTRA_DEBUG
	printk("%s: ix=%#x\n", __FUNCTION__, ix );
#endif

	if( ix < 0 )
		return NETDEV_TX_BUSY;

	if( priv->tx_skb[ ix ] != NULL )
		return NETDEV_TX_BUSY;

	len = skb->len ;

	/* Map <skb> into Tx descriptor */
	paddr = dma_map_single( &dev->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if( dma_mapping_error( &dev->dev, paddr )) {
		printk(KERN_WARNING "%s: Tx DMA map failed\n", dev->name);
		return NETDEV_TX_BUSY ;
	}

	/* Save <skb> pointer */
	priv->tx_skb[ ix ] = skb ;

	/* Fill in the Tx Descriptor */
	tx_desc = priv->tx_desc_start + (ix << GMAC_DESC_SIZE_SHIFT) ;

	/* Prep descriptor */
	memset( &desc, 0, sizeof(desc));

	desc.desc_flags = 0x0;	/* Append CRC */
	desc.desc_parity = 0;
	desc.desc_buf_sz = len;
	desc.desc_data_ptr = paddr;

	/* Be sure the descriptor is in memory before the device reads it */
	desc.desc_eof = 1;		/* One pakcet per desc (for now) */
	desc.desc_sof = 1;
	desc.desc_int_comp = 0;

	/* Mark EoT for last desc */
	if( (ix+1) == priv->tx_ring.count )
		desc.desc_eot = 1;

	/* Interrupt once for every 64 packets transmitted */
	if( (ix & 0x3f) == 0x3f && ! priv->tx_active ) {
		desc.desc_int_comp = 1;
		priv->tx_active = true;
	}

	/* calculate and set descriptor parity */
	amac_desc_parity( &desc );

	/* Get current LastDesc */
	off =_reg_read( dev, GMAC_TX_PTR(q) );
	off >>= GMAC_DESC_SIZE_SHIFT ;
	BUG_ON( off >= priv->tx_ring.count );

	/* Need to suspend Tx DMA if writing into LastDesc */
	if( off == ix )
		_reg_write( dev, GMAC_TXCTL_SUSPEND(q), 1);

	/* Write descriptor at once to memory */
	*tx_desc = desc ;
	barrier();

	dev->trans_start = jiffies ;

	/* Stop transmission if ran out of descriptors */
	room =_ring_room( &priv->tx_ring) ;

	if( room <= 0 )
		netif_stop_queue( dev );

	/* Update stats */
	priv->counters.tx_packets ++ ;
	priv->counters.tx_bytes += len;

	/* Kick the hardware */
	if( off == ix )
		_reg_write( dev, GMAC_TXCTL_SUSPEND(q), 0);

	off = (priv->tx_ring.ix_in) << GMAC_DESC_SIZE_SHIFT ;
	_reg_write( dev, GMAC_TX_PTR(q), priv->tx_desc_paddr + off);

	/* Reset timer */
	if( ! priv->tx_active ) {
		_reg_write( dev, GMAC_TIMER, 500000000 );
		_reg_write( dev, GMAC_INTSTAT_TIMER_INT, 1);
		_reg_write( dev, GMAC_INTMASK_TIMER_INT, 1);
		priv->tx_active = true ;
		}

	return NETDEV_TX_OK;
}

static void amac_tx_timeout(struct net_device *dev)
{
	struct amac_priv * priv = netdev_priv(dev);

	printk(KERN_WARNING "%s: Tx timeout\n", dev->name );

	napi_schedule( &priv->tx_napi );
}


static int amac_open(struct net_device *dev)
{
	struct amac_priv * priv = netdev_priv(dev);
	gmac_desc_t * desc ;
	const unsigned q = 0;
	int res = 0;

	/* Setup interrupt service routine */
	res = request_irq( dev->irq, amac_interrupt, 0, dev->name, dev );

	if( res != 0 ) goto _fail_irq;

	/* GMAC descriptors have full 64-bit address pointers */
	dma_set_coherent_mask( &dev->dev, DMA_BIT_MASK(64));

	/* Set MAC address */
	amac_set_hw_addr( dev );


	/* Initialize rings */
	priv->tx_ring.count = GMAC_TX_DESC_COUNT;
	priv->rx_ring.count = GMAC_RX_DESC_COUNT;
	priv->rx_ring.ix_in = priv->rx_ring.ix_out = 0;
	priv->tx_ring.ix_in = priv->tx_ring.ix_out = 0;

	priv->rx_desc_start = dma_alloc_coherent( & dev->dev, 
				priv->rx_ring.count << GMAC_DESC_SIZE_SHIFT,
				&priv->rx_desc_paddr, GFP_KERNEL );

	if( IS_ERR_OR_NULL( priv->rx_desc_start ))
		goto _fail_desc_alloc;

	/* Verify the descritors are aligned as needed */
	if( priv->rx_ring.count <=256)
		BUG_ON( priv->rx_desc_paddr & (SZ_4K-1));
	else
		BUG_ON( priv->rx_desc_paddr & (SZ_8K-1));

	priv->tx_desc_start = dma_alloc_coherent( & dev->dev, 
				priv->tx_ring.count << GMAC_DESC_SIZE_SHIFT,
				&priv->tx_desc_paddr, GFP_KERNEL );

	if( IS_ERR_OR_NULL( priv->tx_desc_start ))
		goto _fail_desc_alloc;

	/* Verify the descritors are aligned as needed */
	if( priv->tx_ring.count <=256)
		BUG_ON( priv->tx_desc_paddr & (SZ_4K-1));
	else
		BUG_ON( priv->tx_desc_paddr & (SZ_8K-1));

	/* Initialize descriptors */
	memset( priv->tx_desc_start, 0, 
		priv->tx_ring.count << GMAC_DESC_SIZE_SHIFT );
	memset( priv->rx_desc_start, 0, 
		priv->rx_ring.count << GMAC_DESC_SIZE_SHIFT );

	/* Mark last descriptors with EOT */
	desc = priv->tx_desc_start +
		((priv->tx_ring.count-1)<<GMAC_DESC_SIZE_SHIFT);
	desc->desc_eot = 1 ;

	desc = priv->rx_desc_start +
		((priv->rx_ring.count-1)<<GMAC_DESC_SIZE_SHIFT);
	desc->desc_eot = 1 ;

	/* Alloc auxiliary <skb> pointer arrays */
	priv->rx_skb = kmalloc( sizeof(struct skbuff *) * priv->rx_ring.count,
		GFP_KERNEL );
	priv->tx_skb = kmalloc( sizeof(struct skbuff *) * priv->tx_ring.count,
		GFP_KERNEL );
	if( NULL == priv->tx_skb || NULL == priv->rx_skb )
		goto _fail_alloc;
	memset( priv->rx_skb, 0, sizeof(struct skbuff *) * priv->rx_ring.count);
	memset( priv->tx_skb, 0, sizeof(struct skbuff *) * priv->tx_ring.count);

	/* Enable hardware */
	barrier();

	/* Write physical address of descriptor tables */
	_reg_write( dev, GMAC_RX_ADDR_LOW, priv->rx_desc_paddr );
	_reg_write( dev, GMAC_RX_ADDR_HIGH, (u64)priv->rx_desc_paddr >> 32);
	_reg_write( dev, GMAC_TX_ADDR_LOW(q), priv->tx_desc_paddr );
	_reg_write( dev, GMAC_TX_ADDR_HIGH(q), (u64)priv->tx_desc_paddr >> 32);

	/* Set Other Rx control parameters */
	_reg_write( dev, GMAC_RXCTL, 0);
	_reg_write( dev, GMAC_RXCTL_RX_OFFSET, sizeof( gmac_rxstat_t ));
	_reg_write( dev, GMAC_RXCTL_OFLOW_CONT, 1);
	_reg_write( dev, GMAC_RXCTL_PARITY_DIS, 0);
	_reg_write( dev, GMAC_RXCTL_BURST_LEN, 1);	/* 32-bytes */
	
#ifdef	_EXTRA_DEBUG
	printk("UniMAC config=%#x mac stat %#x\n",
		_reg_read(dev, UMAC_CONFIG), _reg_read(dev, UMAC_MAC_STAT));
#endif

	/* Enable UniMAC */
	_reg_write( dev, UMAC_FRM_LENGTH, AMAC_MAX_PACKET);

	_reg_write( dev, UMAC_CONFIG_ETH_SPEED, 2);	/* 1Gbps */
	_reg_write( dev, UMAC_CONFIG_CRC_FW, 0);
	_reg_write( dev, UMAC_CONFIG_LNGTHCHK_DIS, 1);
	_reg_write( dev, UMAC_CONFIG_CNTLFRM_EN, 0);
	_reg_write( dev, UMAC_CONFIG_PROMISC, 0);
	_reg_write( dev, UMAC_CONFIG_LCL_LOOP_EN, 0);
	_reg_write( dev, UMAC_CONFIG_RMT_LOOP_EN, 0);
	_reg_write( dev, UMAC_CONFIG_TXRX_AUTO_EN, 0);
	_reg_write( dev, UMAC_CONFIG_AUTO_EN, 0);
	_reg_write( dev, UMAC_CONFIG_TX_ADDR_INS, 0);
	_reg_write( dev, UMAC_CONFIG_PAD_EN, 0);
	_reg_write( dev, UMAC_CONFIG_PREAMB_EN, 0);
	_reg_write( dev, UMAC_CONFIG_TX_EN, 1);
	_reg_write( dev, UMAC_CONFIG_RX_EN, 1);

	/* Configure GMAC */
	_reg_write( dev, GMAC_CTL_FLOW_CNTLSRC, 0);
	_reg_write( dev, GMAC_CTL_RX_OVFLOW_MODE, 0);
	_reg_write( dev, GMAC_CTL_MIB_RESET, 0);
	_reg_write( dev, GMAC_CTL_LINKSTAT_SEL, 1);
	_reg_write( dev, GMAC_CTL_FLOW_CNTL_MODE, 0);
	_reg_write( dev, GMAC_CTL_NWAY_AUTO_POLL, 1);

	/* Set Tx DMA control bits */
	_reg_write( dev, GMAC_TXCTL(q), 0);
	_reg_write( dev, GMAC_TXCTL_PARITY_DIS(q), 0);	
	_reg_write( dev, GMAC_TXCTL_BURST_LEN(q), 1);	/* 32-bytes */
	_reg_write( dev, GMAC_TXCTL_DNA_ACT_INDEX(q), 1);/* for debug */

	/* Enable Tx DMA */
	_reg_write( dev, GMAC_TXCTL_TX_EN(q), 1);

	/* Enable Rx DMA */
	_reg_write( dev, GMAC_RXCTL_RX_EN, 1);

	/* Fill Rx queue - works only after Rx DMA is enabled */
	amac_rx_fill( dev, 64*2 );

	/* Install NAPI instance */
	netif_napi_add( dev, &priv->rx_napi, amac_rx_poll, 64 );
	netif_napi_add( dev, &priv->tx_napi, amac_tx_poll, 64 );

	/* Enable NAPI right away */
	napi_enable( &priv->rx_napi );
	napi_enable( &priv->tx_napi );

	/* Enable interrupts */
	_reg_write( dev, GMAC_INTMASK_RX_INT, 1);
	_reg_write( dev, GMAC_INTMASK_TX_INT(q), 1);
	_reg_write( dev, GMAC_INTMASK_MII_LINK_CHANGE, 1);
	_reg_write( dev, GMAC_INTMASK_SW_LINK_CHANGE, 1);
	_reg_write( dev, GMAC_INTMASK_DMA_DESC_ERR, 1);
	_reg_write( dev, GMAC_INTMASK_DMA_DATA_ERR, 1);
	_reg_write( dev, GMAC_INTMASK_DMA_PROTO_ERR, 1);
	_reg_write( dev, GMAC_INTMASK_DMA_RX_UNDERFLOW, 1);
	_reg_write( dev, GMAC_INTMASK_DMA_RX_OVERFLOW, 1);
	_reg_write( dev, GMAC_INTMASK_DMA_TX_UNDERFLOW, 1);
	/* _reg_write( dev, GMAC_INTMASK_TIMER_INT, 1); */

	/* Setup lazy Rx interrupts (TBD: ethertool coalesce ?) */
	_reg_write( dev, GMAC_INTRX_LZY_TIMEOUT, 125000 );
	_reg_write( dev, GMAC_INTRX_LZY_FRMCNT, 16 );

	/* Ready to transmit packets */
	netif_start_queue( dev );

	return res;

	/* Various failure exits */
_fail_alloc:
	if( NULL != priv->tx_skb )
		kfree( priv->tx_skb );
	if( NULL != priv->rx_skb )
		kfree( priv->rx_skb );

_fail_desc_alloc:
	printk(KERN_WARNING "%s: failed to allocate memory\n", dev->name );
	if( NULL != priv->rx_desc_start )
		dma_free_coherent( &dev->dev, 
				priv->rx_ring.count << GMAC_DESC_SIZE_SHIFT,
				priv->rx_desc_start, priv->rx_desc_paddr );

	if( NULL != priv->tx_desc_start )
		dma_free_coherent( &dev->dev, 
				priv->tx_ring.count << GMAC_DESC_SIZE_SHIFT,
				priv->tx_desc_start, priv->tx_desc_paddr );

	free_irq( dev->irq, dev );

_fail_irq:
	return -ENODEV;
}

static int amac_stop(struct net_device *dev)
{
	struct amac_priv * priv = netdev_priv(dev);
	const unsigned q = 0;
	unsigned ix;

	/* Stop accepting packets for transmission */
	netif_stop_queue( dev );

	/* Flush Tx FIFO */
	_reg_write( dev, GMAC_CTL_TX_FLUSH, 1);
	ndelay(1);


	/* Stop hardware */

	/* Disable Rx DMA */
	_reg_write( dev, GMAC_RXCTL_RX_EN, 0);

	/* Disable Tx DMA */
	_reg_write( dev, GMAC_TXCTL_TX_EN(q), 0);

	/* Disable all interrupts */
	_reg_write( dev, GMAC_INTMASK, 0);
	barrier();

	/* Verify DMA has indeed stopped */
	amac_tx_show( dev );

	/* Stop NAPI processing */
	napi_disable( &priv->rx_napi );
	napi_disable( &priv->tx_napi );

	/* Stop UniMAC */
	_reg_write( dev, UMAC_CONFIG_TX_EN, 0);
	_reg_write( dev, UMAC_CONFIG_RX_EN, 0);
	_reg_write( dev, UMAC_CONFIG_SW_RESET, 1);

	netif_napi_del( &priv->tx_napi );
	netif_napi_del( &priv->rx_napi );

	/* Unmap any mapped DMA buffers */
	for(ix = 0; ix < priv->tx_ring.count; ix ++ ) {
		gmac_desc_t * tx_desc =
			priv->tx_desc_start + (ix<<GMAC_DESC_SIZE_SHIFT);

		if( tx_desc->desc_data_ptr )
			dma_unmap_single( &dev->dev, tx_desc->desc_data_ptr,
				tx_desc->desc_buf_sz, DMA_TO_DEVICE );
	}

	for(ix = 0; ix < priv->rx_ring.count; ix ++ ) {
		gmac_desc_t * rx_desc =
			priv->rx_desc_start + (ix<<GMAC_DESC_SIZE_SHIFT);

		if( rx_desc->desc_data_ptr )
			dma_unmap_single( &dev->dev, rx_desc->desc_data_ptr,
				rx_desc->desc_buf_sz, DMA_FROM_DEVICE );
	}

	/* Free <skb> that may be pending in the queues */
	for(ix = 0; ix < priv->tx_ring.count; ix ++ )
		if( priv->tx_skb[ ix ] )
			dev_kfree_skb( priv->tx_skb[ ix ] );

	for(ix = 0; ix < priv->rx_ring.count; ix ++ )
		if( priv->rx_skb[ ix ] )
			dev_kfree_skb( priv->rx_skb[ ix ] );

	/* Free auxiliary <skb> pointer arrays */
	if( NULL != priv->tx_skb )
		kfree( priv->tx_skb );
	if( NULL != priv->rx_skb )
		kfree( priv->rx_skb );

	/* Free DMA descriptors */
	dma_free_coherent( &dev->dev, 
		priv->tx_ring.count << GMAC_DESC_SIZE_SHIFT, 
		priv->tx_desc_start, priv->tx_desc_paddr );

	dma_free_coherent( &dev->dev, 
		priv->rx_ring.count << GMAC_DESC_SIZE_SHIFT, 
		priv->rx_desc_start, priv->rx_desc_paddr );

	priv->tx_ring.count = priv->rx_ring.count = 0;

	/* Release interrupt */
	free_irq( dev->irq, dev );

	_reg_write( dev, UMAC_CONFIG_SW_RESET, 0);
	return 0;
}

/*
 * Network device methods
 */
static struct net_device_ops amac_dev_ops = {
	.ndo_open	=	amac_open,
	.ndo_stop	=	amac_stop,
	.ndo_start_xmit	=	amac_start_xmit,
	.ndo_tx_timeout	=	amac_tx_timeout,
	.ndo_select_queue =	amac_select_queue,
	.ndo_set_multicast_list=amac_set_mc_list,
	.ndo_change_rx_flags =	amac_change_rx_flags,
	.ndo_get_stats64 =	amac_get_stats64,
	.ndo_set_mac_address =	eth_mac_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller =	amac_poll_controller,
#endif
};

static void __devinit amac_default_mac_addr( struct net_device * dev, u8 unit )
{
	static const u8 def_hw_addr[] = 
		{ 0x80, 0xde, 0xad, 0xfa, 0xce, 0x00 };

	u32 hw_addr[2];

	/* Get pre-set MAC address */
	hw_addr[0] = _reg_read( dev, UMAC_MACADDR_LOW );
	hw_addr[1] = _reg_read( dev, UMAC_MACADDR_HIGH );
	dev->perm_addr[0] = hw_addr[0] >> 24 ;
	dev->perm_addr[1] = hw_addr[0] >> 16 ;
	dev->perm_addr[2] = hw_addr[0] >> 8 ;
	dev->perm_addr[3] = hw_addr[0] ;
	dev->perm_addr[4] = hw_addr[1] >> 8 ;
	dev->perm_addr[5] = hw_addr[1] ;

	if( ! is_valid_ether_addr( &dev->perm_addr[0] ) ) {
		memcpy( &dev->perm_addr, def_hw_addr, 6 );
		dev->perm_addr[5] ^= unit ;
		}

	dev->dev_addr = dev->perm_addr;

	printk(KERN_INFO "%s: MAC addr %02x-%02x-%02x-%02x-%02x-%02x Driver: %s\n",
		dev->name,
		dev->perm_addr[0], dev->perm_addr[1], dev->perm_addr[2],
		dev->perm_addr[3], dev->perm_addr[4], dev->perm_addr[5],
		"$Id: bcm5301x_amac.c 332917 2012-05-11 20:15:35Z $"
		);

}

static int __devinit amac_dev_init( struct net_device * dev, unsigned unit )
{
	struct amac_priv * priv = netdev_priv(dev);
	void * __iomem base;
	int res;

	/* Reserve resources */
	res = request_resource( &iomem_resource, &amac_regs[ unit ] );
	if( res != 0) return res ;

	/* map registers in virtual space */
	base = ioremap(  amac_regs[ unit ].start,
			resource_size( &amac_regs[ unit ] ));

	if( IS_ERR_OR_NULL(base) ) {
		/* Release resources */
		release_resource( &amac_regs[ unit ] );
		return res;
	}

	dev->base_addr = (u32) base ;
	dev->irq = amac_irqs[ unit ].start ;

	/* Install device methods */
	dev->netdev_ops = & amac_dev_ops;
	/* Install ethertool methods */
	/* TBD */

	/* Declare features */
	dev->features |= /* NETIF_F_SG | */ NETIF_F_HIGHDMA ;

	/* Private vars */
	memset( priv, 0, sizeof( struct amac_priv ));
	priv->unit = unit ;

	/* Init spinlock */
	spin_lock_init( & priv->tx_lock );
	spin_lock_init( & priv->rx_lock );

	/* MAC address */
	amac_default_mac_addr( dev, unit );

	return 0;
}

static int __init amac_init(void)
{
	struct net_device *dev;
	unsigned i;
	int res = 0;

	/* Compile-time sanity checks */

	if( 1 ) {
		gmac_desc_t desc;
		gmac_rxstat_t rxs;
		u32 r;
		u32 d[ 4 ];

		BUG_ON( sizeof( gmac_rxstat_t) != sizeof( u32 ) );
		BUG_ON( sizeof( gmac_desc_t) != 1 << GMAC_DESC_SIZE_SHIFT);

		memset( &d, 0, sizeof(d) );
		memset( &desc, 0, sizeof(desc) );
		memset( &rxs, 0, sizeof(rxs) );

		rxs.rxstat_desc_cnt = 0xa ;
		r = 0xa << 24 ;
		BUG_ON(memcmp( &rxs, &r, sizeof(rxs)));

		desc.desc_buf_sz = 0x400 ; 
		d[1] = 0x400 ;
		BUG_ON( memcmp( & desc, &d, sizeof(desc)));
	}

	/* Create and registers all instances */
	for(i = 0; i < 4; i ++ ) {
		dev = alloc_etherdev( sizeof( struct amac_priv ) );
		if( IS_ERR_OR_NULL(dev) )
			{
			if( IS_ERR( dev ))
				return PTR_ERR(dev);
			else
				return -ENOSYS;
			}
		res = amac_dev_init( dev, i );
		if( res != 0 ){
			printk(KERN_WARNING "%s: failed to initialize %d\n",
				dev->name, res );
			continue;
			}
		res = register_netdev( dev );
		if( res != 0 ){
			printk(KERN_WARNING "%s: failed to register %d\n",
				dev->name, res );
		}
	}

	return res;
}

device_initcall(amac_init);
