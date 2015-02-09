/*
 * drivers/net/titan_mdio.c - Driver for Titan ethernet ports
 *
 * Copyright (C) 2003 PMC-Sierra Inc.
 * Author : Manish Lachwani (lachwani@pmc-sierra.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Management Data IO (MDIO) driver for the Titan GMII. Interacts with the Marvel PHY
 * on the Titan. No support for the TBI as yet.
 *
 */

#include	"titan_mdio.h"

#define MDIO_DEBUG

/*
 * Local constants
 */
#define MAX_CLKA            1023
#define MAX_PHY_DEV         31
#define MAX_PHY_REG         31
#define WRITEADDRS_OPCODE   0x0
#define	READ_OPCODE	    0x2
#define WRITE_OPCODE        0x1
#define MAX_MDIO_POLL       100

/*
 * Titan MDIO and SCMB registers
 */
#define TITAN_GE_SCMB_CONTROL                0x01c0  /* SCMB Control */
#define TITAN_GE_SCMB_CLKA	             0x01c4  /* SCMB Clock A */
#define TITAN_GE_MDIO_COMMAND                0x01d0  /* MDIO Command */
#define TITAN_GE_MDIO_DEVICE_PORT_ADDRESS    0x01d4  /* MDIO Device and Port addrs */
#define TITAN_GE_MDIO_DATA                   0x01d8  /* MDIO Data */
#define TITAN_GE_MDIO_INTERRUPTS             0x01dC  /* MDIO Interrupts */

/*
 * Function to poll the MDIO
 */
static int titan_ge_mdio_poll(void)
{
	int	i, val;

	for (i = 0; i < MAX_MDIO_POLL; i++) {
		val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_COMMAND);

		if (!(val & 0x8000))
			return TITAN_GE_MDIO_GOOD;
	}

	return TITAN_GE_MDIO_ERROR;
}


/*
 * Initialize and configure the MDIO
 */
int titan_ge_mdio_setup(titan_ge_mdio_config *titan_mdio)
{
	unsigned long	val;

	/* Reset the SCMB and program into MDIO mode*/
	TITAN_GE_MDIO_WRITE(TITAN_GE_SCMB_CONTROL, 0x9000);
	TITAN_GE_MDIO_WRITE(TITAN_GE_SCMB_CONTROL, 0x1000);

	/* CLK A */
	val = TITAN_GE_MDIO_READ(TITAN_GE_SCMB_CLKA);
	val = ( (val & ~(0x03ff)) | (titan_mdio->clka & 0x03ff));
	TITAN_GE_MDIO_WRITE(TITAN_GE_SCMB_CLKA, val);

	/* Preamble Suppresion */
	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_COMMAND);
	val = ( (val & ~(0x0001)) | (titan_mdio->mdio_spre & 0x0001));
	TITAN_GE_MDIO_WRITE(TITAN_GE_MDIO_COMMAND, val);

	/* MDIO mode */
	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_DEVICE_PORT_ADDRESS);
	val = ( (val & ~(0x4000)) | (titan_mdio->mdio_mode & 0x4000));
	TITAN_GE_MDIO_WRITE(TITAN_GE_MDIO_DEVICE_PORT_ADDRESS, val);

	return TITAN_GE_MDIO_GOOD;
}

/*
 * Set the PHY address in indirect mode
 */
int titan_ge_mdio_inaddrs(int dev_addr, int reg_addr)
{
	volatile unsigned long	val;

	/* Setup the PHY device */
	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_DEVICE_PORT_ADDRESS);
	val = ( (val & ~(0x1f00)) | ( (dev_addr << 8) & 0x1f00));
	val = ( (val & ~(0x001f)) | ( reg_addr & 0x001f));
	TITAN_GE_MDIO_WRITE(TITAN_GE_MDIO_DEVICE_PORT_ADDRESS, val);

	/* Write the new address */
	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_COMMAND);
	val = ( (val & ~(0x0300)) | ( (WRITEADDRS_OPCODE << 8) & 0x0300));
	TITAN_GE_MDIO_WRITE(TITAN_GE_MDIO_COMMAND, val);

	return TITAN_GE_MDIO_GOOD;
}

/*
 * Read the MDIO register. This is what the individual parametes mean:
 *
 * dev_addr : PHY ID
 * reg_addr : register offset
 *
 * See the spec for the Titan MAC. We operate in the Direct Mode.
 */

#define MAX_RETRIES	2

int titan_ge_mdio_read(int dev_addr, int reg_addr, unsigned int *pdata)
{
	volatile unsigned long	val;
	int retries = 0;

	/* Setup the PHY device */

again:
	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_DEVICE_PORT_ADDRESS);
	val = ( (val & ~(0x1f00)) | ( (dev_addr << 8) & 0x1f00));
	val = ( (val & ~(0x001f)) | ( reg_addr & 0x001f));
	val |= 0x4000;
	TITAN_GE_MDIO_WRITE(TITAN_GE_MDIO_DEVICE_PORT_ADDRESS, val);

	udelay(30);

	/* Issue the read command */
	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_COMMAND);
	val = ( (val & ~(0x0300)) | ( (READ_OPCODE << 8) & 0x0300));
	TITAN_GE_MDIO_WRITE(TITAN_GE_MDIO_COMMAND, val);

	udelay(30);

	if (titan_ge_mdio_poll() != TITAN_GE_MDIO_GOOD)
		return TITAN_GE_MDIO_ERROR;

	*pdata = (unsigned int)TITAN_GE_MDIO_READ(TITAN_GE_MDIO_DATA);
	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_INTERRUPTS);

	udelay(30);

	if (val & 0x2) {
		if (retries == MAX_RETRIES)
			return TITAN_GE_MDIO_ERROR;
		else {
			retries++;
			goto again;
		}
	}

	return TITAN_GE_MDIO_GOOD;
}

/*
 * Write to the MDIO register
 *
 * dev_addr : PHY ID
 * reg_addr : register that needs to be written to
 *
 */
int titan_ge_mdio_write(int dev_addr, int reg_addr, unsigned int data)
{
	volatile unsigned long	val;

	if (titan_ge_mdio_poll() != TITAN_GE_MDIO_GOOD)
		return TITAN_GE_MDIO_ERROR;

	/* Setup the PHY device */
	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_DEVICE_PORT_ADDRESS);
	val = ( (val & ~(0x1f00)) | ( (dev_addr << 8) & 0x1f00));
	val = ( (val & ~(0x001f)) | ( reg_addr & 0x001f));
	val |= 0x4000;
	TITAN_GE_MDIO_WRITE(TITAN_GE_MDIO_DEVICE_PORT_ADDRESS, val);

	udelay(30);

	/* Setup the data to write */
	TITAN_GE_MDIO_WRITE(TITAN_GE_MDIO_DATA, data);

	udelay(30);

	/* Issue the write command */
	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_COMMAND);
	val = ( (val & ~(0x0300)) | ( (WRITE_OPCODE << 8) & 0x0300));
	TITAN_GE_MDIO_WRITE(TITAN_GE_MDIO_COMMAND, val);

	udelay(30);

	if (titan_ge_mdio_poll() != TITAN_GE_MDIO_GOOD)
		return TITAN_GE_MDIO_ERROR;

	val = TITAN_GE_MDIO_READ(TITAN_GE_MDIO_INTERRUPTS);
	if (val & 0x2)
		return TITAN_GE_MDIO_ERROR;

	return TITAN_GE_MDIO_GOOD;
}
