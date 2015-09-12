#ifndef _TITAN_GE_H_
#define _TITAN_GE_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <asm/byteorder.h>

/*
 * These functions should be later moved to a more generic location since there
 * will be others accessing it also
 */

/*
 * This is the way it works: LKB5 Base is at 0x0128. TITAN_BASE is defined in
 * include/asm/titan_dep.h. TITAN_GE_BASE is the value in the TITAN_GE_LKB5
 * register.
 */

#define	TITAN_GE_BASE	0xfe000000UL
#define	TITAN_GE_SIZE	0x10000UL

extern unsigned long titan_ge_base;

#define	TITAN_GE_WRITE(offset, data) \
		*(volatile u32 *)(titan_ge_base + (offset)) = (data)

#define TITAN_GE_READ(offset) *(volatile u32 *)(titan_ge_base + (offset))

#ifndef msec_delay
#define msec_delay(x)   do { if(in_interrupt()) { \
				/* Don't mdelay in interrupt context! */ \
				BUG(); \
			} else { \
				set_current_state(TASK_UNINTERRUPTIBLE); \
				schedule_timeout((x * HZ)/1000); \
			} } while(0)
#endif

#define TITAN_GE_PORT_0

#define	TITAN_SRAM_BASE		((OCD_READ(RM9000x2_OCD_LKB13) & ~1) << 4)
#define	TITAN_SRAM_SIZE		0x2000UL

/*
 * We may need these constants
 */
#define TITAN_BIT0    0x00000001
#define TITAN_BIT1    0x00000002
#define TITAN_BIT2    0x00000004
#define TITAN_BIT3    0x00000008
#define TITAN_BIT4    0x00000010
#define TITAN_BIT5    0x00000020
#define TITAN_BIT6    0x00000040
#define TITAN_BIT7    0x00000080
#define TITAN_BIT8    0x00000100
#define TITAN_BIT9    0x00000200
#define TITAN_BIT10   0x00000400
#define TITAN_BIT11   0x00000800
#define TITAN_BIT12   0x00001000
#define TITAN_BIT13   0x00002000
#define TITAN_BIT14   0x00004000
#define TITAN_BIT15   0x00008000
#define TITAN_BIT16   0x00010000
#define TITAN_BIT17   0x00020000
#define TITAN_BIT18   0x00040000
#define TITAN_BIT19   0x00080000
#define TITAN_BIT20   0x00100000
#define TITAN_BIT21   0x00200000
#define TITAN_BIT22   0x00400000
#define TITAN_BIT23   0x00800000
#define TITAN_BIT24   0x01000000
#define TITAN_BIT25   0x02000000
#define TITAN_BIT26   0x04000000
#define TITAN_BIT27   0x08000000
#define TITAN_BIT28   0x10000000
#define TITAN_BIT29   0x20000000
#define TITAN_BIT30   0x40000000
#define TITAN_BIT31   0x80000000

/* Flow Control */
#define	TITAN_GE_FC_NONE	0x0
#define	TITAN_GE_FC_FULL	0x1
#define	TITAN_GE_FC_TX_PAUSE	0x2
#define	TITAN_GE_FC_RX_PAUSE	0x3

/* Duplex Settings */
#define	TITAN_GE_FULL_DUPLEX	0x1
#define	TITAN_GE_HALF_DUPLEX	0x2

/* Speed settings */
#define	TITAN_GE_SPEED_1000	0x1
#define	TITAN_GE_SPEED_100	0x2
#define	TITAN_GE_SPEED_10	0x3

/* Debugging info only */
#undef TITAN_DEBUG

/* Keep the rings in the Titan's SSRAM */
#define TITAN_RX_RING_IN_SRAM

#ifdef CONFIG_64BIT
#define	TITAN_GE_IE_MASK	0xfffffffffb001b64
#define	TITAN_GE_IE_STATUS	0xfffffffffb001b60
#else
#define	TITAN_GE_IE_MASK	0xfb001b64
#define	TITAN_GE_IE_STATUS	0xfb001b60
#endif

/* Support for Jumbo Frames */
#undef TITAN_GE_JUMBO_FRAMES

/* Rx buffer size */
#ifdef TITAN_GE_JUMBO_FRAMES
#define	TITAN_GE_JUMBO_BUFSIZE	9080
#else
#define	TITAN_GE_STD_BUFSIZE	1580
#endif

/*
 * Tx and Rx Interrupt Coalescing parameter. These values are
 * for 1 Ghz processor. Rx coalescing can be taken care of
 * by NAPI. NAPI is adaptive and hence useful. Tx coalescing
 * is not adaptive. Hence, these values need to be adjusted
 * based on load, CPU speed etc.
 */
#define	TITAN_GE_RX_COAL	150
#define	TITAN_GE_TX_COAL	300

#if defined(__BIG_ENDIAN)

/* Define the Rx descriptor */
typedef struct eth_rx_desc {
	u32     reserved;	/* Unused 		*/
	u32     buffer_addr;	/* CPU buffer address 	*/
	u32	cmd_sts;	/* Command and Status	*/
	u32	buffer;		/* XDMA buffer address	*/
} titan_ge_rx_desc;

/* Define the Tx descriptor */
typedef struct eth_tx_desc {
	u16     cmd_sts;	/* Command, Status and Buffer count */
	u16	buffer_len;	/* Length of the buffer	*/
	u32     buffer_addr;	/* Physical address of the buffer */
} titan_ge_tx_desc;

#elif defined(__LITTLE_ENDIAN)

/* Define the Rx descriptor */
typedef struct eth_rx_desc {
	u32	buffer_addr;	/* CPU buffer address   */
	u32	reserved;	/* Unused               */
	u32	buffer;		/* XDMA buffer address  */
	u32	cmd_sts;	/* Command and Status   */
} titan_ge_rx_desc;

/* Define the Tx descriptor */
typedef struct eth_tx_desc {
	u32     buffer_addr;	/* Physical address of the buffer */
	u16     buffer_len;     /* Length of the buffer */
	u16     cmd_sts;        /* Command, Status and Buffer count */
} titan_ge_tx_desc;
#endif

/* Default Tx Queue Size */
#define	TITAN_GE_TX_QUEUE	128
#define TITAN_TX_RING_BYTES	(TITAN_GE_TX_QUEUE * sizeof(struct eth_tx_desc))

/* Default Rx Queue Size */
#define	TITAN_GE_RX_QUEUE	64
#define TITAN_RX_RING_BYTES	(TITAN_GE_RX_QUEUE * sizeof(struct eth_rx_desc))

/* Packet Structure */
typedef struct _pkt_info {
	unsigned int           len;
	unsigned int            cmd_sts;
	unsigned int            buffer;
	struct sk_buff          *skb;
	unsigned int		checksum;
} titan_ge_packet;


#define	PHYS_CNT	3

/* Titan Port specific data structure */
typedef struct _eth_port_ctrl {
	unsigned int		port_num;
	u8			port_mac_addr[6];

	/* Rx descriptor pointers */
	int 			rx_curr_desc_q, rx_used_desc_q;

	/* Tx descriptor pointers */
	int 			tx_curr_desc_q, tx_used_desc_q;

	/* Rx descriptor area */
	volatile titan_ge_rx_desc	*rx_desc_area;
	unsigned int			rx_desc_area_size;
	struct sk_buff*			rx_skb[TITAN_GE_RX_QUEUE];

	/* Tx Descriptor area */
	volatile titan_ge_tx_desc	*tx_desc_area;
	unsigned int                    tx_desc_area_size;
	struct sk_buff*                 tx_skb[TITAN_GE_TX_QUEUE];

	/* Timeout task */
	struct work_struct		tx_timeout_task;

	/* DMA structures and handles */
	dma_addr_t			tx_dma;
	dma_addr_t			rx_dma;
	dma_addr_t			tx_dma_array[TITAN_GE_TX_QUEUE];

	/* Device lock */
	spinlock_t			lock;

	unsigned int			tx_ring_skbs;
	unsigned int			rx_ring_size;
	unsigned int			tx_ring_size;
	unsigned int			rx_ring_skbs;

	struct net_device_stats		stats;

	/* Tx and Rx coalescing */
	unsigned long			rx_int_coal;
	unsigned long			tx_int_coal;

	/* Threshold for replenishing the Rx and Tx rings */
	unsigned int			tx_threshold;
	unsigned int			rx_threshold;

	/* NAPI work limit */
	unsigned int			rx_work_limit;
} titan_ge_port_info;

/* Titan specific constants */
#define	TITAN_ETH_PORT_IRQ		3

/* Max Rx buffer */
#define	TITAN_GE_MAX_RX_BUFFER		65536

/* Tx and Rx Error */
#define	TITAN_GE_ERROR

/* Rx Descriptor Command and Status */

#define	TITAN_GE_RX_CRC_ERROR		TITAN_BIT27	/* crc error */
#define	TITAN_GE_RX_OVERFLOW_ERROR	TITAN_BIT15	/* overflow */
#define TITAN_GE_RX_BUFFER_OWNED	TITAN_BIT21	/* buffer ownership */
#define	TITAN_GE_RX_STP			TITAN_BIT31	/* start of packet */
#define	TITAN_GE_RX_BAM			TITAN_BIT30	/* broadcast address match */
#define TITAN_GE_RX_PAM			TITAN_BIT28	/* physical address match */
#define TITAN_GE_RX_LAFM		TITAN_BIT29	/* logical address filter match */
#define TITAN_GE_RX_VLAN		TITAN_BIT26	/* virtual lans */
#define TITAN_GE_RX_PERR		TITAN_BIT19	/* packet error */
#define TITAN_GE_RX_TRUNC		TITAN_BIT20	/* packet size greater than 32 buffers */

/* Tx Descriptor Command */
#define	TITAN_GE_TX_BUFFER_OWNED	TITAN_BIT5	/* buffer ownership */
#define	TITAN_GE_TX_ENABLE_INTERRUPT	TITAN_BIT15	/* Interrupt Enable */

/* Return Status */
#define	TITAN_OK	0x1	/* Good Status */
#define	TITAN_ERROR	0x2	/* Error Status */

/* MIB specific register offset */
#define TITAN_GE_MSTATX_STATS_BASE_LOW       0x0800  /* MSTATX COUNTL[15:0] */
#define TITAN_GE_MSTATX_STATS_BASE_MID       0x0804  /* MSTATX COUNTM[15:0] */
#define TITAN_GE_MSTATX_STATS_BASE_HI        0x0808  /* MSTATX COUNTH[7:0] */
#define TITAN_GE_MSTATX_CONTROL              0x0828  /* MSTATX Control */
#define TITAN_GE_MSTATX_VARIABLE_SELECT      0x082C  /* MSTATX Variable Select */

/* MIB counter offsets, add to the TITAN_GE_MSTATX_STATS_BASE_XXX */
#define TITAN_GE_MSTATX_RXFRAMESOK                   0x0040
#define TITAN_GE_MSTATX_RXOCTETSOK                   0x0050
#define TITAN_GE_MSTATX_RXFRAMES                     0x0060
#define TITAN_GE_MSTATX_RXOCTETS                     0x0070
#define TITAN_GE_MSTATX_RXUNICASTFRAMESOK            0x0080
#define TITAN_GE_MSTATX_RXBROADCASTFRAMESOK          0x0090
#define TITAN_GE_MSTATX_RXMULTICASTFRAMESOK          0x00A0
#define TITAN_GE_MSTATX_RXTAGGEDFRAMESOK             0x00B0
#define TITAN_GE_MSTATX_RXMACPAUSECONTROLFRAMESOK    0x00C0
#define TITAN_GE_MSTATX_RXMACCONTROLFRAMESOK         0x00D0
#define TITAN_GE_MSTATX_RXFCSERROR                   0x00E0
#define TITAN_GE_MSTATX_RXALIGNMENTERROR             0x00F0
#define TITAN_GE_MSTATX_RXSYMBOLERROR                0x0100
#define TITAN_GE_MSTATX_RXLAYER1ERROR                0x0110
#define TITAN_GE_MSTATX_RXINRANGELENGTHERROR         0x0120
#define TITAN_GE_MSTATX_RXLONGLENGTHERROR            0x0130
#define TITAN_GE_MSTATX_RXLONGLENGTHCRCERROR         0x0140
#define TITAN_GE_MSTATX_RXSHORTLENGTHERROR           0x0150
#define TITAN_GE_MSTATX_RXSHORTLLENGTHCRCERROR       0x0160
#define TITAN_GE_MSTATX_RXFRAMES64OCTETS             0x0170
#define TITAN_GE_MSTATX_RXFRAMES65TO127OCTETS        0x0180
#define TITAN_GE_MSTATX_RXFRAMES128TO255OCTETS       0x0190
#define TITAN_GE_MSTATX_RXFRAMES256TO511OCTETS       0x01A0
#define TITAN_GE_MSTATX_RXFRAMES512TO1023OCTETS      0x01B0
#define TITAN_GE_MSTATX_RXFRAMES1024TO1518OCTETS     0x01C0
#define TITAN_GE_MSTATX_RXFRAMES1519TOMAXSIZE        0x01D0
#define TITAN_GE_MSTATX_RXSTATIONADDRESSFILTERED     0x01E0
#define TITAN_GE_MSTATX_RXVARIABLE                   0x01F0
#define TITAN_GE_MSTATX_GENERICADDRESSFILTERED       0x0200
#define TITAN_GE_MSTATX_UNICASTFILTERED              0x0210
#define TITAN_GE_MSTATX_MULTICASTFILTERED            0x0220
#define TITAN_GE_MSTATX_BROADCASTFILTERED            0x0230
#define TITAN_GE_MSTATX_HASHFILTERED                 0x0240
#define TITAN_GE_MSTATX_TXFRAMESOK                   0x0250
#define TITAN_GE_MSTATX_TXOCTETSOK                   0x0260
#define TITAN_GE_MSTATX_TXOCTETS                     0x0270
#define TITAN_GE_MSTATX_TXTAGGEDFRAMESOK             0x0280
#define TITAN_GE_MSTATX_TXMACPAUSECONTROLFRAMESOK    0x0290
#define TITAN_GE_MSTATX_TXFCSERROR                   0x02A0
#define TITAN_GE_MSTATX_TXSHORTLENGTHERROR           0x02B0
#define TITAN_GE_MSTATX_TXLONGLENGTHERROR            0x02C0
#define TITAN_GE_MSTATX_TXSYSTEMERROR                0x02D0
#define TITAN_GE_MSTATX_TXMACERROR                   0x02E0
#define TITAN_GE_MSTATX_TXCARRIERSENSEERROR          0x02F0
#define TITAN_GE_MSTATX_TXSQETESTERROR               0x0300
#define TITAN_GE_MSTATX_TXUNICASTFRAMESOK            0x0310
#define TITAN_GE_MSTATX_TXBROADCASTFRAMESOK          0x0320
#define TITAN_GE_MSTATX_TXMULTICASTFRAMESOK          0x0330
#define TITAN_GE_MSTATX_TXUNICASTFRAMESATTEMPTED     0x0340
#define TITAN_GE_MSTATX_TXBROADCASTFRAMESATTEMPTED   0x0350
#define TITAN_GE_MSTATX_TXMULTICASTFRAMESATTEMPTED   0x0360
#define TITAN_GE_MSTATX_TXFRAMES64OCTETS             0x0370
#define TITAN_GE_MSTATX_TXFRAMES65TO127OCTETS        0x0380
#define TITAN_GE_MSTATX_TXFRAMES128TO255OCTETS       0x0390
#define TITAN_GE_MSTATX_TXFRAMES256TO511OCTETS       0x03A0
#define TITAN_GE_MSTATX_TXFRAMES512TO1023OCTETS      0x03B0
#define TITAN_GE_MSTATX_TXFRAMES1024TO1518OCTETS     0x03C0
#define TITAN_GE_MSTATX_TXFRAMES1519TOMAXSIZE        0x03D0
#define TITAN_GE_MSTATX_TXVARIABLE                   0x03E0
#define TITAN_GE_MSTATX_RXSYSTEMERROR                0x03F0
#define TITAN_GE_MSTATX_SINGLECOLLISION              0x0400
#define TITAN_GE_MSTATX_MULTIPLECOLLISION            0x0410
#define TITAN_GE_MSTATX_DEFERREDXMISSIONS            0x0420
#define TITAN_GE_MSTATX_LATECOLLISIONS               0x0430
#define TITAN_GE_MSTATX_ABORTEDDUETOXSCOLLS          0x0440

/* Interrupt specific defines */
#define TITAN_GE_DEVICE_ID         0x0000  /* Device ID */
#define TITAN_GE_RESET             0x0004  /* Reset reg */
#define TITAN_GE_TSB_CTRL_0        0x000C  /* TSB Control reg 0 */
#define TITAN_GE_TSB_CTRL_1        0x0010  /* TSB Control reg 1 */
#define TITAN_GE_INTR_GRP0_STATUS  0x0040  /* General Interrupt Group 0 Status */
#define TITAN_GE_INTR_XDMA_CORE_A  0x0048  /* XDMA Channel Interrupt Status, Core A*/
#define TITAN_GE_INTR_XDMA_CORE_B  0x004C  /* XDMA Channel Interrupt Status, Core B*/
#define	TITAN_GE_INTR_XDMA_IE	   0x0058  /* XDMA Channel Interrupt Enable */
#define TITAN_GE_SDQPF_ECC_INTR    0x480C  /* SDQPF ECC Interrupt Status */
#define TITAN_GE_SDQPF_RXFIFO_CTL  0x4828  /* SDQPF RxFifo Control and Interrupt Enb*/
#define TITAN_GE_SDQPF_RXFIFO_INTR 0x482C  /* SDQPF RxFifo Interrupt Status */
#define TITAN_GE_SDQPF_TXFIFO_CTL  0x4928  /* SDQPF TxFifo Control and Interrupt Enb*/
#define TITAN_GE_SDQPF_TXFIFO_INTR 0x492C  /* SDQPF TxFifo Interrupt Status */
#define	TITAN_GE_SDQPF_RXFIFO_0	   0x4840  /* SDQPF RxFIFO Enable */
#define	TITAN_GE_SDQPF_TXFIFO_0	   0x4940  /* SDQPF TxFIFO Enable */
#define TITAN_GE_XDMA_CONFIG       0x5000  /* XDMA Global Configuration */
#define TITAN_GE_XDMA_INTR_SUMMARY 0x5010  /* XDMA Interrupt Summary */
#define TITAN_GE_XDMA_BUFADDRPRE   0x5018  /* XDMA Buffer Address Prefix */
#define TITAN_GE_XDMA_DESCADDRPRE  0x501C  /* XDMA Descriptor Address Prefix */
#define TITAN_GE_XDMA_PORTWEIGHT   0x502C  /* XDMA Port Weight Configuration */

/* Rx MAC defines */
#define TITAN_GE_RMAC_CONFIG_1               0x1200  /* RMAC Configuration 1 */
#define TITAN_GE_RMAC_CONFIG_2               0x1204  /* RMAC Configuration 2 */
#define TITAN_GE_RMAC_MAX_FRAME_LEN          0x1208  /* RMAC Max Frame Length */
#define TITAN_GE_RMAC_STATION_HI             0x120C  /* Rx Station Address High */
#define TITAN_GE_RMAC_STATION_MID            0x1210  /* Rx Station Address Middle */
#define TITAN_GE_RMAC_STATION_LOW            0x1214  /* Rx Station Address Low */
#define TITAN_GE_RMAC_LINK_CONFIG            0x1218  /* RMAC Link Configuration */

/* Tx MAC defines */
#define TITAN_GE_TMAC_CONFIG_1               0x1240  /* TMAC Configuration 1 */
#define TITAN_GE_TMAC_CONFIG_2               0x1244  /* TMAC Configuration 2 */
#define TITAN_GE_TMAC_IPG                    0x1248  /* TMAC Inter-Packet Gap */
#define TITAN_GE_TMAC_STATION_HI             0x124C  /* Tx Station Address High */
#define TITAN_GE_TMAC_STATION_MID            0x1250  /* Tx Station Address Middle */
#define TITAN_GE_TMAC_STATION_LOW            0x1254  /* Tx Station Address Low */
#define TITAN_GE_TMAC_MAX_FRAME_LEN          0x1258  /* TMAC Max Frame Length */
#define TITAN_GE_TMAC_MIN_FRAME_LEN          0x125C  /* TMAC Min Frame Length */
#define TITAN_GE_TMAC_PAUSE_FRAME_TIME       0x1260  /* TMAC Pause Frame Time */
#define TITAN_GE_TMAC_PAUSE_FRAME_INTERVAL   0x1264  /* TMAC Pause Frame Interval */

/* GMII register */
#define TITAN_GE_GMII_INTERRUPT_STATUS       0x1348  /* GMII Interrupt Status */
#define TITAN_GE_GMII_CONFIG_GENERAL         0x134C  /* GMII Configuration General */
#define TITAN_GE_GMII_CONFIG_MODE            0x1350  /* GMII Configuration Mode */

/* Tx and Rx XDMA defines */
#define	TITAN_GE_INT_COALESCING		     0x5030 /* Interrupt Coalescing */
#define	TITAN_GE_CHANNEL0_CONFIG	     0x5040 /* Channel 0 XDMA config */
#define	TITAN_GE_CHANNEL0_INTERRUPT	     0x504c /* Channel 0 Interrupt Status */
#define	TITAN_GE_GDI_INTERRUPT_ENABLE        0x5050 /* IE for the GDI Errors */
#define	TITAN_GE_CHANNEL0_PACKET	     0x5060 /* Channel 0 Packet count */
#define	TITAN_GE_CHANNEL0_BYTE		     0x5064 /* Channel 0 Byte count */
#define	TITAN_GE_CHANNEL0_TX_DESC	     0x5054 /* Channel 0 Tx first desc */
#define	TITAN_GE_CHANNEL0_RX_DESC	     0x5058 /* Channel 0 Rx first desc */

/* AFX (Address Filter Exact) register offsets for Slice 0 */
#define TITAN_GE_AFX_EXACT_MATCH_LOW         0x1100  /* AFX Exact Match Address Low*/
#define TITAN_GE_AFX_EXACT_MATCH_MID         0x1104  /* AFX Exact Match Address Mid*/
#define TITAN_GE_AFX_EXACT_MATCH_HIGH        0x1108  /* AFX Exact Match Address Hi */
#define TITAN_GE_AFX_EXACT_MATCH_VID         0x110C  /* AFX Exact Match VID */
#define TITAN_GE_AFX_MULTICAST_HASH_LOW      0x1110  /* AFX Multicast HASH Low */
#define TITAN_GE_AFX_MULTICAST_HASH_MIDLOW   0x1114  /* AFX Multicast HASH MidLow */
#define TITAN_GE_AFX_MULTICAST_HASH_MIDHI    0x1118  /* AFX Multicast HASH MidHi */
#define TITAN_GE_AFX_MULTICAST_HASH_HI       0x111C  /* AFX Multicast HASH Hi */
#define TITAN_GE_AFX_ADDRS_FILTER_CTRL_0     0x1120  /* AFX Address Filter Ctrl 0 */
#define TITAN_GE_AFX_ADDRS_FILTER_CTRL_1     0x1124  /* AFX Address Filter Ctrl 1 */
#define TITAN_GE_AFX_ADDRS_FILTER_CTRL_2     0x1128  /* AFX Address Filter Ctrl 2 */

/* Traffic Groomer block */
#define        TITAN_GE_TRTG_CONFIG	     0x1000  /* TRTG Config */

#endif 				/* _TITAN_GE_H_ */
