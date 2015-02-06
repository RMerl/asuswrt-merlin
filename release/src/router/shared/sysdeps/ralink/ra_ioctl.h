#ifndef _RAETH_IOCTL_H
#define _RAETH_IOCTL_H

/* ioctl commands */
#define RAETH_ESW_REG_READ		0x89F1
#define RAETH_ESW_REG_WRITE		0x89F2
#define RAETH_MII_READ			0x89F3
#define RAETH_MII_WRITE			0x89F4
#define RAETH_ESW_INGRESS_RATE		0x89F5
#define RAETH_ESW_EGRESS_RATE		0x89F6
#define RAETH_ESW_PHY_DUMP		0x89F7
/* ASUS Ext */
#define RAETH_ASUS			0x89FF

/* ASUS Ext sub-command */
#define RAETH_ASUS_RESET		0x0
#define RAETH_ASUS_STATS		0x1

#if defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)

#define REG_ESW_WT_MAC_MFC              0x10
#define REG_ESW_WT_MAC_ATA1             0x74
#define REG_ESW_WT_MAC_ATA2             0x78
#define REG_ESW_WT_MAC_ATWD             0x7C
#define REG_ESW_WT_MAC_ATC              0x80 

#define REG_ESW_TABLE_TSRA1		0x84
#define REG_ESW_TABLE_TSRA2		0x88
#define REG_ESW_TABLE_ATRD		0x8C


#define REG_ESW_VLAN_VTCR		0x90
#define REG_ESW_VLAN_VAWD1		0x94
#define REG_ESW_VLAN_VAWD2		0x98

#if !defined(RTN56UB1) //MT7621 don't use it
#define REG_ESW_VLAN_ID_BASE		0x100
#endif
//#define REG_ESW_VLAN_ID_BASE		0x50
#define REG_ESW_VLAN_MEMB_BASE		0x70
#define REG_ESW_TABLE_SEARCH		0x24
#define REG_ESW_TABLE_STATUS0		0x28
#define REG_ESW_TABLE_STATUS1		0x2C
#define REG_ESW_TABLE_STATUS2		0x30
#define REG_ESW_WT_MAC_AD0		0x34
#define REG_ESW_WT_MAC_AD1		0x38
#define REG_ESW_WT_MAC_AD2		0x3C

#else
/* rt3052 embedded ethernet switch registers */
#define REG_ESW_VLAN_ID_BASE		0x50
#define REG_ESW_VLAN_MEMB_BASE		0x70
#define REG_ESW_TABLE_SEARCH		0x24
#define REG_ESW_TABLE_STATUS0		0x28
#define REG_ESW_TABLE_STATUS1		0x2C
#define REG_ESW_TABLE_STATUS2		0x30
#define REG_ESW_WT_MAC_AD0		0x34
#define REG_ESW_WT_MAC_AD1		0x38
#define REG_ESW_WT_MAC_AD2		0x3C
#endif


#if defined(CONFIG_RALINK_RT3352)
#define REG_ESW_MAX			0x14C
#elif defined (CONFIG_RALINK_RT5350)
#define REG_ESW_MAX                     0x16C
#elif defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)
#define REG_ESW_MAX			0x7FFFF
#else //RT305x, RT3350
#define REG_ESW_MAX			0xFC
#endif


typedef struct rt3052_esw_reg {
	unsigned int off;
	unsigned int val;
} esw_reg;

typedef struct ralink_mii_ioctl_data {
        __u32   phy_id;
        __u32   reg_num;
        __u32   val_in;
        __u32   val_out;
} ra_mii_ioctl_data;

typedef struct rt335x_esw_reg {
	unsigned int on_off;
	unsigned int port;
	unsigned int bw;/*Mbps*/
} esw_rate;

typedef struct raeth_asus_data_s {
	__u32	subcmd;
	union	{
		__u32 subdata;
	};
} raeth_asus_data_t;


#endif
