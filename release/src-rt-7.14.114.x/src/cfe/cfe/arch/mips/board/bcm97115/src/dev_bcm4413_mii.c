#include "lib_types.h"
#include "lib_printf.h"

#include "cfe_timer.h"

#include "dev_bcm4413.h"
#include "dev_bcm4413_mii.h"


	#define MIITRACE( x )


#define CTL_RESET   (1 << 15)   /* reset */
#define CTL_SPEED   (1 << 13)   /* speed selection 0=10, 1=100 */
#define CTL_ANENAB  (1 << 12)   /* autonegotiation enable */
#define CTL_RESTART (1 << 9)    /* restart autonegotiation */
#define CTL_DUPLEX  (1 << 8)    /* duplex mode 0=half, 1=full */

#define	ADV_10FULL	(1 << 6)	/* autonegotiate advertise 10full capability */
#define	ADV_10HALF	(1 << 5)	/* autonegotiate advertise 10half capability */
#define	ADV_100FULL	(1 << 8)	/* autonegotiate advertise 100full capability */
#define	ADV_100HALF	(1 << 7)	/* autonegotiate advertise 100half capability */


static uint16_t mii_read( bcmenetregs_t * regs , unsigned int reg )
{
	uint32_t mdiodata;

	MIITRACE( "mii_read entered\n" );

#ifdef DEBUG
	if( reg >= 32 ) {
		xprintf( "ERROR : invalid register in mii_read (%d)!\n", reg );
		return -1;
	}
#endif

	/* clear mii_int */
	W_REG(NULL,  &regs->emacintstatus, EI_MII );

	mdiodata = MD_SB_START | MD_OP_READ | (1 << MD_PMD_SHIFT)
	         | (reg << MD_RA_SHIFT) | MD_TA_VALID;

	/* issue the read */
	W_REG(NULL,  &regs->mdiodata, mdiodata );

	MIITRACE( "Waiting for hardware...\n" );

	/* wait for it to complete */
	while( !(R_REG(NULL, &regs->emacintstatus) & EI_MII) ) {
		cfe_usleep( 10 );
	}

	mdiodata = R_REG(NULL, &regs->mdiodata);

	MIITRACE( "mii_read exited\n" );

	return mdiodata & MD_DATA_MASK;
}


static void mii_write( bcmenetregs_t * regs, unsigned int reg, uint16_t v )
{
	uint32_t mdiodata;

	MIITRACE( "mii_write entered\n" );

#ifdef DEBUG
	if( reg >= 32 ) {
		xprintf( "ERROR : invalid register in mii_write (%d)!\n", reg );
		return;
	}
#endif

	/* clear mii_int */
	W_REG(NULL,  &regs->emacintstatus, EI_MII );

	mdiodata = MD_SB_START | MD_OP_WRITE | (1 << MD_PMD_SHIFT)
	         | (reg << MD_RA_SHIFT) | MD_TA_VALID | v;

	/* issue the write */
	W_REG(NULL,  &regs->mdiodata, mdiodata );

	MIITRACE( "Waiting for hardware...\n" );

	/* wait for it to complete */
	while( !(R_REG(NULL, &regs->emacintstatus) & EI_MII) ) {
		cfe_usleep( 10 );
	}

	MIITRACE( "mii_write exited\n" );
}


static void mii_or( bcmenetregs_t * regs, unsigned int reg, uint16_t v )
{
	uint16_t tmp;

	tmp = mii_read( regs, reg );
	tmp |= v;
	mii_write( regs, reg, tmp );
}


static void mii_and( bcmenetregs_t * regs, unsigned int reg, uint16_t v )
{
	uint16_t tmp;

	tmp = mii_read( regs, reg );
	tmp &= v;
	mii_write( regs, reg, tmp );
}


void mii_init( bcmenetregs_t * regs )
{
	mii_write( regs, 0, CTL_RESET );
	cfe_usleep( 10 );
	if (mii_read(regs, 0) & CTL_RESET) {
		xprintf( "mii_reset: reset not complete\n" );
	}

	/* enable activity led */
	mii_and( regs, 26, 0x7fff );

	/* enable traffic meter led mode */
	mii_or( regs, 27, (1 << 6) );
}


void mii_setspeed( bcmenetregs_t * regs )
{
	uint16_t reg;

	/*
	 * Because of our bcm4413 interface limitations, force 10Mbps.
	 */

	/* reset our advertised capabilitity bits */
	reg = mii_read(regs, 4);
	reg &= ~(ADV_100FULL | ADV_100HALF | ADV_10FULL | ADV_10HALF);
	reg |= (ADV_10FULL | ADV_10HALF);
	mii_write(regs, 4, reg);

	/* restart autonegotiation */
	mii_or(regs, 0, CTL_RESTART);
}
