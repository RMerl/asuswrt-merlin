#ifndef _ISP1760_HCD_H_
#define _ISP1760_HCD_H_

/* exports for if */
struct usb_hcd *isp1760_register(phys_addr_t res_start, resource_size_t res_len,
				 int irq, unsigned long irqflags,
				 struct device *dev, const char *busname,
				 unsigned int devflags);
int init_kmem_once(void);
void deinit_kmem_cache(void);

/* EHCI capability registers */
#define HC_CAPLENGTH		0x00
#define HC_HCSPARAMS		0x04
#define HC_HCCPARAMS		0x08

/* EHCI operational registers */
#define HC_USBCMD		0x20
#define HC_USBSTS		0x24
#define HC_FRINDEX		0x2c
#define HC_CONFIGFLAG		0x60
#define HC_PORTSC1		0x64
#define HC_ISO_PTD_DONEMAP_REG	0x130
#define HC_ISO_PTD_SKIPMAP_REG	0x134
#define HC_ISO_PTD_LASTPTD_REG	0x138
#define HC_INT_PTD_DONEMAP_REG	0x140
#define HC_INT_PTD_SKIPMAP_REG	0x144
#define HC_INT_PTD_LASTPTD_REG	0x148
#define HC_ATL_PTD_DONEMAP_REG	0x150
#define HC_ATL_PTD_SKIPMAP_REG	0x154
#define HC_ATL_PTD_LASTPTD_REG	0x158

/* Configuration Register */
#define HC_HW_MODE_CTRL		0x300
#define ALL_ATX_RESET		(1 << 31)
#define HW_ANA_DIGI_OC		(1 << 15)
#define HW_DATA_BUS_32BIT	(1 << 8)
#define HW_DACK_POL_HIGH	(1 << 6)
#define HW_DREQ_POL_HIGH	(1 << 5)
#define HW_INTR_HIGH_ACT	(1 << 2)
#define HW_INTR_EDGE_TRIG	(1 << 1)
#define HW_GLOBAL_INTR_EN	(1 << 0)

#define HC_CHIP_ID_REG		0x304
#define HC_SCRATCH_REG		0x308

#define HC_RESET_REG		0x30c
#define SW_RESET_RESET_HC	(1 << 1)
#define SW_RESET_RESET_ALL	(1 << 0)

#define HC_BUFFER_STATUS_REG	0x334
#define ATL_BUFFER		0x1
#define INT_BUFFER		0x2
#define ISO_BUFFER		0x4
#define BUFFER_MAP		0x7

#define HC_MEMORY_REG		0x33c
#define ISP_BANK(x)		((x) << 16)

#define HC_PORT1_CTRL		0x374
#define PORT1_POWER		(3 << 3)
#define PORT1_INIT1		(1 << 7)
#define PORT1_INIT2		(1 << 23)
#define HW_OTG_CTRL_SET		0x374
#define HW_OTG_CTRL_CLR		0x376

/* Interrupt Register */
#define HC_INTERRUPT_REG	0x310

#define HC_INTERRUPT_ENABLE	0x314
#define INTERRUPT_ENABLE_MASK	(HC_INTL_INT | HC_ATL_INT | HC_EOT_INT)

#define HC_ISO_INT		(1 << 9)
#define HC_ATL_INT		(1 << 8)
#define HC_INTL_INT		(1 << 7)
#define HC_EOT_INT		(1 << 3)
#define HC_SOT_INT		(1 << 1)

#define HC_ISO_IRQ_MASK_OR_REG	0x318
#define HC_INT_IRQ_MASK_OR_REG	0x31C
#define HC_ATL_IRQ_MASK_OR_REG	0x320
#define HC_ISO_IRQ_MASK_AND_REG	0x324
#define HC_INT_IRQ_MASK_AND_REG	0x328
#define HC_ATL_IRQ_MASK_AND_REG	0x32C

/* Register sets */
#define HC_BEGIN_OF_ATL		0x0c00
#define HC_BEGIN_OF_INT		0x0800
#define HC_BEGIN_OF_ISO		0x0400
#define HC_BEGIN_OF_PAYLOAD	0x1000

/* urb state*/
#define DELETE_URB		(0x0008)
#define NO_TRANSFER_ACTIVE	(0xffffffff)

#define ATL_REGS_OFFSET		(0xc00)
#define INT_REGS_OFFSET		(0x800)

/* Philips Transfer Descriptor (PTD) */
struct ptd {
	__le32 dw0;
	__le32 dw1;
	__le32 dw2;
	__le32 dw3;
	__le32 dw4;
	__le32 dw5;
	__le32 dw6;
	__le32 dw7;
};

struct inter_packet_info {
	void *data_buffer;
	u32 payload;
#define PTD_FIRE_NEXT		(1 << 0)
#define PTD_URB_FINISHED	(1 << 1)
	struct urb *urb;
	struct isp1760_qh *qh;
	struct isp1760_qtd *qtd;
};


typedef void (packet_enqueue)(struct usb_hcd *hcd, struct isp1760_qh *qh,
		struct isp1760_qtd *qtd);

#define isp1760_dbg(priv, fmt, args...) \
	dev_dbg(priv_to_hcd(priv)->self.controller, fmt, ##args)

#define isp1760_info(priv, fmt, args...) \
	dev_info(priv_to_hcd(priv)->self.controller, fmt, ##args)

#define isp1760_err(priv, fmt, args...) \
	dev_err(priv_to_hcd(priv)->self.controller, fmt, ##args)

/*
 * Device flags that can vary from board to board.  All of these
 * indicate the most "atypical" case, so that a devflags of 0 is
 * a sane default configuration.
 */
#define ISP1760_FLAG_BUS_WIDTH_16	0x00000002 /* 16-bit data bus width */
#define ISP1760_FLAG_OTG_EN		0x00000004 /* Port 1 supports OTG */
#define ISP1760_FLAG_ANALOG_OC		0x00000008 /* Analog overcurrent */
#define ISP1760_FLAG_DACK_POL_HIGH	0x00000010 /* DACK active high */
#define ISP1760_FLAG_DREQ_POL_HIGH	0x00000020 /* DREQ active high */
#define ISP1760_FLAG_ISP1761		0x00000040 /* Chip is ISP1761 */
#define ISP1760_FLAG_INTR_POL_HIGH	0x00000080 /* Interrupt polarity active high */
#define ISP1760_FLAG_INTR_EDGE_TRIG	0x00000100 /* Interrupt edge triggered */

/* chip memory management */
struct memory_chunk {
	unsigned int start;
	unsigned int size;
	unsigned int free;
};

/*
 * 60kb divided in:
 * - 32 blocks @ 256  bytes
 * - 20 blocks @ 1024 bytes
 * -  4 blocks @ 8192 bytes
 */

#define BLOCK_1_NUM 32
#define BLOCK_2_NUM 20
#define BLOCK_3_NUM 4

#define BLOCK_1_SIZE 256
#define BLOCK_2_SIZE 1024
#define BLOCK_3_SIZE 8192
#define BLOCKS (BLOCK_1_NUM + BLOCK_2_NUM + BLOCK_3_NUM)
#define PAYLOAD_SIZE 0xf000

/* I saw if some reloads if the pointer was negative */
#define ISP1760_NULL_POINTER	(0x400)

/* ATL */
/* DW0 */
#define PTD_VALID			1
#define PTD_LENGTH(x)			(((u32) x) << 3)
#define PTD_MAXPACKET(x)		(((u32) x) << 18)
#define PTD_MULTI(x)			(((u32) x) << 29)
#define PTD_ENDPOINT(x)			(((u32)	x) << 31)
/* DW1 */
#define PTD_DEVICE_ADDR(x)		(((u32) x) << 3)
#define PTD_PID_TOKEN(x)		(((u32) x) << 10)
#define PTD_TRANS_BULK			((u32) 2 << 12)
#define PTD_TRANS_INT			((u32) 3 << 12)
#define PTD_TRANS_SPLIT			((u32) 1 << 14)
#define PTD_SE_USB_LOSPEED		((u32) 2 << 16)
#define PTD_PORT_NUM(x)			(((u32) x) << 18)
#define PTD_HUB_NUM(x)			(((u32) x) << 25)
#define PTD_PING(x)			(((u32) x) << 26)
/* DW2 */
#define PTD_RL_CNT(x)			(((u32) x) << 25)
#define PTD_DATA_START_ADDR(x)		(((u32) x) << 8)
#define BASE_ADDR			0x1000
/* DW3 */
#define PTD_CERR(x)			(((u32) x) << 23)
#define PTD_NAC_CNT(x)			(((u32) x) << 19)
#define PTD_ACTIVE			((u32) 1 << 31)
#define PTD_DATA_TOGGLE(x)		(((u32) x) << 25)

#define DW3_HALT_BIT			(1 << 30)
#define DW3_ERROR_BIT			(1 << 28)
#define DW3_QTD_ACTIVE			(1 << 31)

#define INT_UNDERRUN			(1 << 2)
#define INT_BABBLE			(1 << 1)
#define INT_EXACT			(1 << 0)

#define DW1_GET_PID(x)			(((x) >> 10) & 0x3)
#define PTD_XFERRED_LENGTH(x)		((x) & 0x7fff)
#define PTD_XFERRED_LENGTH_LO(x)	((x) & 0x7ff)

#define SETUP_PID	(2)
#define IN_PID		(1)
#define OUT_PID		(0)
#define GET_QTD_TOKEN_TYPE(x)	((x) & 0x3)

#define DATA_TOGGLE		(1 << 31)
#define GET_DATA_TOGGLE(x)	((x) >> 31)

/* Errata 1 */
#define RL_COUNTER	(0)
#define NAK_COUNTER	(0)
#define ERR_COUNTER	(2)

#define HC_ATL_PL_SIZE	(8192)

#endif
