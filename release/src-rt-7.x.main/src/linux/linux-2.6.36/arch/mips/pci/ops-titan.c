/*
 * Copyright 2003 PMC-Sierra
 * Author: Manish Lachwani (lachwani@pmc-sierra.com)
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>

#include <asm/pci.h>
#include <asm/io.h>
#include <asm/rm9k-ocd.h>

/*
 * PCI specific defines
 */
#define	TITAN_PCI_0_CONFIG_ADDRESS	0x780
#define	TITAN_PCI_0_CONFIG_DATA		0x784

/*
 * Titan PCI Config Read Byte
 */
static int titan_read_config(struct pci_bus *bus, unsigned int devfn, int reg,
	int size, u32 * val)
{
	uint32_t address, tmp;
	int dev, busno, func;

	busno = bus->number;
	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	address = (busno << 16) | (dev << 11) | (func << 8) |
	          (reg & 0xfc) | 0x80000000;


	/* start the configuration cycle */
	ocd_writel(address, TITAN_PCI_0_CONFIG_ADDRESS);
	tmp = ocd_readl(TITAN_PCI_0_CONFIG_DATA) >> ((reg & 3) << 3);

	switch (size) {
	case 1:
		tmp &= 0xff;
	case 2:
		tmp &= 0xffff;
	}
	*val = tmp;

	return PCIBIOS_SUCCESSFUL;
}

static int titan_write_config(struct pci_bus *bus, unsigned int devfn, int reg,
	int size, u32 val)
{
	uint32_t address;
	int dev, busno, func;

	busno = bus->number;
	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	address = (busno << 16) | (dev << 11) | (func << 8) |
		(reg & 0xfc) | 0x80000000;

	/* start the configuration cycle */
	ocd_writel(address, TITAN_PCI_0_CONFIG_ADDRESS);

	/* write the data */
	switch (size) {
	case 1:
		ocd_writeb(val, TITAN_PCI_0_CONFIG_DATA + (~reg & 0x3));
		break;

	case 2:
		ocd_writew(val, TITAN_PCI_0_CONFIG_DATA + (~reg & 0x2));
		break;

	case 4:
		ocd_writel(val, TITAN_PCI_0_CONFIG_DATA);
		break;
	}

	return PCIBIOS_SUCCESSFUL;
}

/*
 * Titan PCI structure
 */
struct pci_ops titan_pci_ops = {
	titan_read_config,
	titan_write_config,
};
