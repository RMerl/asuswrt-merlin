#ifndef RA2882ETHEND_H
#define RA2882ETHEND_H

#ifdef DSP_VIA_NONCACHEABLE
#define ESRAM_BASE	0xa0800000	/* 0x0080-0000  ~ 0x00807FFF */
#else
#define ESRAM_BASE	0x80800000	/* 0x0080-0000  ~ 0x00807FFF */
#endif

#define RX_RING_BASE	((int)(ESRAM_BASE + 0x7000))
#define TX_RING_BASE	((int)(ESRAM_BASE + 0x7800))

#if defined(CONFIG_RALINK_RT2880)
#define NUM_TX_RINGS 	1
#else
#define NUM_TX_RINGS 	4
#endif
#ifdef MEMORY_OPTIMIZATION
#ifdef CONFIG_RAETH_ROUTER
#define NUM_RX_DESC     128
#define NUM_TX_DESC    	128
#elif CONFIG_RT_3052_ESW
#define NUM_RX_DESC     64
#define NUM_TX_DESC     64
#else
#define NUM_RX_DESC     128
#define NUM_TX_DESC     128
#endif
//#define NUM_RX_MAX_PROCESS 32
#define NUM_RX_MAX_PROCESS 64
#else
#if defined (CONFIG_RAETH_ROUTER)
#define NUM_RX_DESC     256
#define NUM_TX_DESC    	256
#elif defined (CONFIG_RT_3052_ESW)
#define NUM_RX_DESC     512
#define NUM_QRX_DESC     16
#define NUM_TX_DESC     512 
#else
#define NUM_RX_DESC     256
#define NUM_TX_DESC     256
#endif
#if defined(CONFIG_RALINK_RT3883) || defined(CONFIG_RALINK_MT7620) 
#define NUM_RX_MAX_PROCESS 2
#else
#define NUM_RX_MAX_PROCESS 16
#endif
#endif

#define NUM_TX_DESC_MASK	(NUM_TX_DESC - 1)	/* NUM_TX_DESC must be power of 2 */
#define MAX_TXD_PWR		(14)
#define MAX_PER_TXD		(1 << MAX_TXD_PWR)

#define DEV_NAME        "eth2"
#define DEV2_NAME       "eth3"

#if defined (CONFIG_RALINK_RT6855A) || defined (CONFIG_RALINK_MT7621)
#define GMAC0_OFFSET    0xE000
#define GMAC2_OFFSET    0xE006
#else
#define GMAC0_OFFSET    0x28 
#define GMAC2_OFFSET    0x22
#endif

#if defined(CONFIG_RALINK_RT6855A)
#define IRQ_ENET0	22
#else
#define IRQ_ENET0	3 	/* hardware interrupt #3, defined in RT2880 Soc Design Spec Rev 0.03, pp43 */
#endif

#define FE_INT_STATUS_REG (*(volatile unsigned long *)(FE_INT_STATUS))
#define FE_INT_STATUS_CLEAN(reg) (*(volatile unsigned long *)(FE_INT_STATUS)) = reg

//#define RAETH_DEBUG
#ifdef RAETH_DEBUG
#define RAETH_PRINT(fmt, args...) printk(KERN_INFO fmt, ## args)
#else
#define RAETH_PRINT(fmt, args...) { }
#endif

struct net_device_stats *ra_get_stats(struct net_device *dev);

void ei_tx_timeout(struct net_device *dev);
int rather_probe(struct net_device *dev);
int ei_open(struct net_device *dev);
int ei_close(struct net_device *dev);

int ra2882eth_init(void);
void ra2882eth_cleanup_module(void);

void ei_xmit_housekeeping(unsigned long data);

u32 mii_mgr_read(u32 phy_addr, u32 phy_register, u32 *read_data);
u32 mii_mgr_write(u32 phy_addr, u32 phy_register, u32 write_data);
u32 mii_mgr_cl45_set_address(u32 port_num, u32 dev_addr, u32 reg_addr);
u32 mii_mgr_read_cl45(u32 port_num, u32 dev_addr, u32 reg_addr, u32 *read_data);
u32 mii_mgr_write_cl45(u32 port_num, u32 dev_addr, u32 reg_addr, u32 write_data);
void fe_sw_init(void);

#endif
