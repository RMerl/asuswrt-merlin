/*
 * MDIO used to interact with the PHY when using GMII/MII
 */
#ifndef _TITAN_MDIO_H
#define _TITAN_MDIO_H

#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include "titan_ge.h"


#define	TITAN_GE_MDIO_ERROR	(-9000)
#define	TITAN_GE_MDIO_GOOD	0

#define	TITAN_GE_MDIO_BASE		titan_ge_base

#define	TITAN_GE_MDIO_READ(offset)	\
	*(volatile u32 *)(titan_ge_base + (offset))

#define	TITAN_GE_MDIO_WRITE(offset, data)	\
	*(volatile u32 *)(titan_ge_base + (offset)) = (data)


/* GMII specific registers */
#define	TITAN_GE_MARVEL_PHY_ID		0x00
#define	TITAN_PHY_AUTONEG_ADV		0x04
#define	TITAN_PHY_LP_ABILITY		0x05
#define	TITAN_GE_MDIO_MII_CTRL		0x09
#define	TITAN_GE_MDIO_MII_EXTENDED	0x0f
#define	TITAN_GE_MDIO_PHY_CTRL		0x10
#define	TITAN_GE_MDIO_PHY_STATUS	0x11
#define	TITAN_GE_MDIO_PHY_IE		0x12
#define	TITAN_GE_MDIO_PHY_IS		0x13
#define	TITAN_GE_MDIO_PHY_LED		0x18
#define	TITAN_GE_MDIO_PHY_LED_OVER	0x19
#define	PHY_ANEG_TIME_WAIT		45	/* 45 seconds wait time */

/*
 * MDIO Config Structure
 */
typedef struct {
	unsigned int		clka;
	int			mdio_spre;
	int			mdio_mode;
} titan_ge_mdio_config;

/*
 * Function Prototypes
 */
int titan_ge_mdio_setup(titan_ge_mdio_config *);
int titan_ge_mdio_inaddrs(int, int);
int titan_ge_mdio_read(int, int, unsigned int *);
int titan_ge_mdio_write(int, int, unsigned int);

#endif /* _TITAN_MDIO_H */
