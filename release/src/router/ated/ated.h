
#ifndef __RACONFIG_H__

//#include "rt2880.h"


typedef unsigned char   boolean;
typedef unsigned char	u8;
typedef int				s32;
typedef unsigned long	u32;
typedef unsigned short	u16;

#ifndef TRUE
#define TRUE              (1)
#define FALSE             (0)
#endif


/*
 *	IEEE 802.3 Ethernet magic constants.  The frame sizes omit the preamble
 *	and FCS/CRC (frame check sequence). 
 */

#define ETH_ALEN	6		/* Octets in one ethernet addr	 */
#define ETH_HLEN	14		/* Total octets in header.	 */


/* 
 *	Ethernet Protocol ID used by RaCfg Protocol
 */

#define ETH_P_RACFG	0x2880

/*
 * This is an RaCfg frame header 
 */

 struct racfghdr {
 	u32		magic_no;
	u16		comand_type;
	u16		comand_id;
	u16		length;
	u16		sequence;
	u8		data[0];	
}  __attribute__((packed));



struct reg_str  {
	u32		address;
	u32		value;
} __attribute__((packed));


struct cmd_id_tbl  {
	u16		command_id;
	u16		length;
} __attribute__((packed));


#define RACFG_MAGIC_NO		0x18142880 /* RALINK:0x2880 */

/* 
 * RaCfg frame Comand Type
 */

/* use in bootstrapping state */

#define RACFG_CMD_TYPE_PASSIVE_MASK 0x7FFF

/* 
 * Bootstrapping command group 
 */

/* command type */
#define RACFG_CMD_TYPE_ETHREQ		0x0008

/* command id */
#define RACFG_CMD_RF_WRITE_ALL		0x0000
#define RACFG_CMD_E2PROM_READ16		0x0001
#define RACFG_CMD_E2PROM_WRITE16	0x0002
#define RACFG_CMD_E2PROM_READ_ALL	0x0003
#define RACFG_CMD_E2PROM_WRITE_ALL	0x0004
#define RACFG_CMD_IO_READ			0x0005
#define RACFG_CMD_IO_WRITE			0x0006
#define RACFG_CMD_IO_READ_BULK		0x0007
#define RACFG_CMD_BBP_READ8			0x0008
#define RACFG_CMD_BBP_RWRITE8		0x0009
#define RACFG_CMD_BBP_READ_ALL		0x000a
#define RACFG_CMD_GET_COUNTER		0x000b
#define RACFG_CMD_CLEAR_COUNTER		0x000c
#define RACFG_CMD_RSV1				0x000d
#define RACFG_CMD_RSV2				0x000e
#define RACFG_CMD_RSV3				0x000f
#define RACFG_CMD_TX_START			0x0010
#define RACFG_CMD_GET_TX_STATUS		0x0011
#define RACFG_CMD_TX_STOP			0x0012
#define RACFG_CMD_RX_START			0x0013
#define RACFG_CMD_RX_STOP			0x0014

#define RACFG_CMD_AP_STOP			0x0080
#define RACFG_CMD_AP_START			0x0081

unsigned short cmd_id_len_tbl[] = 
{
	18, 2, 4, 0, 0xffff, 4, 8, 6, 2, 3, 0, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0, 0, 0, 0,
};

#define SIZE_OF_CMD_ID_TABLE    (sizeof(cmd_id_len_tbl) / sizeof(unsigned short) )


#define RTPRIV_IOCTL_ATE			(SIOCIWFIRSTPRIV + 0x08)



// Roger finish
#define RT2880_REG(x)  x

#define cpu_to_le32(x) htonl(x)
#define cpu_to_le16(x) htons(x)
#define le32_to_cpu(x) ntohl(x)
#define le16_to_cpu(x) ntohs(x)

#endif /* __RACONFIG_H__*/
