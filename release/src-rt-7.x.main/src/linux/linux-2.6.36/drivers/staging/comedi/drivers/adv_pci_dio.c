/*
 * comedi/drivers/adv_pci_dio.c
 *
 * Author: Michal Dobes <dobes@tesnet.cz>
 *
 *  Hardware driver for Advantech PCI DIO cards.
*/
/*
Driver: adv_pci_dio
Description: Advantech PCI-1730, PCI-1733, PCI-1734, PCI-1735U,
	PCI-1736UP, PCI-1750, PCI-1751, PCI-1752, PCI-1753/E,
	PCI-1754, PCI-1756, PCI-1762
Author: Michal Dobes <dobes@tesnet.cz>
Devices: [Advantech] PCI-1730 (adv_pci_dio), PCI-1733,
  PCI-1734, PCI-1735U, PCI-1736UP, PCI-1750,
  PCI-1751, PCI-1752, PCI-1753,
  PCI-1753+PCI-1753E, PCI-1754, PCI-1756,
  PCI-1760, PCI-1762
Status: untested
Updated: Tue, 04 May 2010 13:00:00 +0000

This driver supports now only insn interface for DI/DO/DIO.

Configuration options:
  [0] - PCI bus of device (optional)
  [1] - PCI slot of device (optional)
	If bus/slot is not specified, the first available PCI
	device will be used.

*/

#include "../comedidev.h"

#include <linux/delay.h>

#include "comedi_pci.h"
#include "8255.h"
#include "8253.h"

#undef PCI_DIO_EXTDEBUG		/* if defined, enable extensive debug logging */

#undef DPRINTK
#ifdef PCI_DIO_EXTDEBUG
#define DPRINTK(fmt, args...) printk(fmt, ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#define PCI_VENDOR_ID_ADVANTECH		0x13fe

/* hardware types of the cards */
enum hw_cards_id {
	TYPE_PCI1730, TYPE_PCI1733, TYPE_PCI1734, TYPE_PCI1735, TYPE_PCI1736,
	TYPE_PCI1750,
	TYPE_PCI1751,
	TYPE_PCI1752,
	TYPE_PCI1753, TYPE_PCI1753E,
	TYPE_PCI1754, TYPE_PCI1756,
	TYPE_PCI1760,
	TYPE_PCI1762
};

/* which I/O instructions to use */
enum hw_io_access {
	IO_8b, IO_16b
};

#define MAX_DI_SUBDEVS	2	/* max number of DI subdevices per card */
#define MAX_DO_SUBDEVS	2	/* max number of DO subdevices per card */
#define MAX_DIO_SUBDEVG	2	/* max number of DIO subdevices group per
				 * card */
#define MAX_8254_SUBDEVS   1	/* max number of 8254 counter subdevs per
				 * card */
				/* (could be more than one 8254 per
				 * subdevice) */

#define SIZE_8254	   4	/* 8254 IO space length */
#define SIZE_8255	   4	/* 8255 IO space length */

#define PCIDIO_MAINREG	   2	/* main I/O region for all Advantech cards? */

/* Register offset definitions */
/*  Advantech PCI-1730/3/4 */
#define PCI1730_IDI	   0	/* R:   Isolated digital input  0-15 */
#define PCI1730_IDO	   0	/* W:   Isolated digital output 0-15 */
#define PCI1730_DI	   2	/* R:   Digital input  0-15 */
#define PCI1730_DO	   2	/* W:   Digital output 0-15 */
#define PCI1733_IDI	   0	/* R:   Isolated digital input  0-31 */
#define	PCI1730_3_INT_EN	0x08	/* R/W: enable/disable interrupts */
#define	PCI1730_3_INT_RF	0x0c	/* R/W: set falling/raising edge for
					 * interrupts */
#define	PCI1730_3_INT_CLR	0x10	/* R/W: clear interrupts */
#define PCI1734_IDO	   0	/* W:   Isolated digital output 0-31 */
#define PCI173x_BOARDID	   4	/* R:   Board I/D switch for 1730/3/4 */

/* Advantech PCI-1735U */
#define PCI1735_DI	   0	/* R:   Digital input  0-31 */
#define PCI1735_DO	   0	/* W:   Digital output 0-31 */
#define PCI1735_C8254	   4	/* R/W: 8254 counter */
#define PCI1735_BOARDID	   8    /* R:   Board I/D switch for 1735U */

/*  Advantech PCI-1736UP */
#define PCI1736_IDI        0	/* R:   Isolated digital input  0-15 */
#define PCI1736_IDO        0	/* W:   Isolated digital output 0-15 */
#define PCI1736_3_INT_EN        0x08	/* R/W: enable/disable interrupts */
#define PCI1736_3_INT_RF        0x0c	/* R/W: set falling/raising edge for
					 * interrupts */
#define PCI1736_3_INT_CLR       0x10	/* R/W: clear interrupts */
#define PCI1736_BOARDID    4	/* R:   Board I/D switch for 1736UP */
#define PCI1736_MAINREG    0	/* Normal register (2) doesn't work */

/*  Advantech PCI-1750 */
#define PCI1750_IDI	   0	/* R:   Isolated digital input  0-15 */
#define PCI1750_IDO	   0	/* W:   Isolated digital output 0-15 */
#define PCI1750_ICR	  32	/* W:   Interrupt control register */
#define PCI1750_ISR	  32	/* R:   Interrupt status register */

/*  Advantech PCI-1751/3/3E */
#define PCI1751_DIO	   0	/* R/W: begin of 8255 registers block */
#define PCI1751_ICR	  32	/* W:   Interrupt control register */
#define PCI1751_ISR	  32	/* R:   Interrupt status register */
#define PCI1753_DIO	   0	/* R/W: begin of 8255 registers block */
#define PCI1753_ICR0	  16	/* R/W: Interrupt control register group 0 */
#define PCI1753_ICR1	  17	/* R/W: Interrupt control register group 1 */
#define PCI1753_ICR2	  18	/* R/W: Interrupt control register group 2 */
#define PCI1753_ICR3	  19	/* R/W: Interrupt control register group 3 */
#define PCI1753E_DIO	  32	/* R/W: begin of 8255 registers block */
#define PCI1753E_ICR0	  48	/* R/W: Interrupt control register group 0 */
#define PCI1753E_ICR1	  49	/* R/W: Interrupt control register group 1 */
#define PCI1753E_ICR2	  50	/* R/W: Interrupt control register group 2 */
#define PCI1753E_ICR3	  51	/* R/W: Interrupt control register group 3 */

/*  Advantech PCI-1752/4/6 */
#define PCI1752_IDO	   0	/* R/W: Digital output  0-31 */
#define PCI1752_IDO2	   4	/* R/W: Digital output 32-63 */
#define PCI1754_IDI	   0	/* R:   Digital input   0-31 */
#define PCI1754_IDI2	   4	/* R:   Digital input  32-64 */
#define PCI1756_IDI	   0	/* R:   Digital input   0-31 */
#define PCI1756_IDO	   4	/* R/W: Digital output  0-31 */
#define PCI1754_6_ICR0	0x08	/* R/W: Interrupt control register group 0 */
#define PCI1754_6_ICR1	0x0a	/* R/W: Interrupt control register group 1 */
#define PCI1754_ICR2	0x0c	/* R/W: Interrupt control register group 2 */
#define PCI1754_ICR3	0x0e	/* R/W: Interrupt control register group 3 */
#define PCI1752_6_CFC	0x12	/* R/W: set/read channel freeze function */
#define PCI175x_BOARDID	0x10	/* R:   Board I/D switch for 1752/4/6 */

/*  Advantech PCI-1762 registers */
#define PCI1762_RO	   0	/* R/W: Relays status/output */
#define PCI1762_IDI	   2	/* R:   Isolated input status */
#define PCI1762_BOARDID	   4	/* R:   Board I/D switch */
#define PCI1762_ICR	   6	/* W:   Interrupt control register */
#define PCI1762_ISR	   6	/* R:   Interrupt status register */

/*  Advantech PCI-1760 registers */
#define OMB0		0x0c	/* W:   Mailbox outgoing registers */
#define OMB1		0x0d
#define OMB2		0x0e
#define OMB3		0x0f
#define IMB0		0x1c	/* R:   Mailbox incoming registers */
#define IMB1		0x1d
#define IMB2		0x1e
#define IMB3		0x1f
#define INTCSR0		0x38	/* R/W: Interrupt control registers */
#define INTCSR1		0x39
#define INTCSR2		0x3a
#define INTCSR3		0x3b

/*  PCI-1760 mailbox commands */
#define CMD_ClearIMB2		0x00	/* Clear IMB2 status and return actual
					 * DI status in IMB3 */
#define CMD_SetRelaysOutput	0x01	/* Set relay output from OMB0 */
#define CMD_GetRelaysStatus	0x02	/* Get relay status to IMB0 */
#define CMD_ReadCurrentStatus	0x07	/* Read the current status of the
					 * register in OMB0, result in IMB0 */
#define CMD_ReadFirmwareVersion	0x0e	/* Read the firmware ver., result in
					 * IMB1.IMB0 */
#define CMD_ReadHardwareVersion	0x0f	/* Read the hardware ver., result in
					 * IMB1.IMB0 */
#define CMD_EnableIDIFilters	0x20	/* Enable IDI filters based on bits in
					 * OMB0 */
#define CMD_EnableIDIPatternMatch 0x21	/* Enable IDI pattern match based on
					 * bits in OMB0 */
#define CMD_SetIDIPatternMatch	0x22	/* Enable IDI pattern match based on
					 * bits in OMB0 */
#define CMD_EnableIDICounters	0x28	/* Enable IDI counters based on bits in
					 * OMB0 */
#define CMD_ResetIDICounters	0x29	/* Reset IDI counters based on bits in
					 * OMB0 to its reset values */
#define CMD_OverflowIDICounters	0x2a	/* Enable IDI counters overflow
					 * interrupts  based on bits in OMB0 */
#define CMD_MatchIntIDICounters	0x2b	/* Enable IDI counters match value
					 * interrupts  based on bits in OMB0 */
#define CMD_EdgeIDICounters	0x2c	/* Set IDI up counters count edge (bit=0
					 * - rising, =1 - falling) */
#define CMD_GetIDICntCurValue	0x2f	/* Read IDI{OMB0} up counter current
					 * value */
#define CMD_SetIDI0CntResetValue 0x40	/* Set IDI0 Counter Reset Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI1CntResetValue 0x41	/* Set IDI1 Counter Reset Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI2CntResetValue 0x42	/* Set IDI2 Counter Reset Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI3CntResetValue 0x43	/* Set IDI3 Counter Reset Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI4CntResetValue 0x44	/* Set IDI4 Counter Reset Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI5CntResetValue 0x45	/* Set IDI5 Counter Reset Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI6CntResetValue 0x46	/* Set IDI6 Counter Reset Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI7CntResetValue 0x47	/* Set IDI7 Counter Reset Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI0CntMatchValue 0x48	/* Set IDI0 Counter Match Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI1CntMatchValue 0x49	/* Set IDI1 Counter Match Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI2CntMatchValue 0x4a	/* Set IDI2 Counter Match Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI3CntMatchValue 0x4b	/* Set IDI3 Counter Match Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI4CntMatchValue 0x4c	/* Set IDI4 Counter Match Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI5CntMatchValue 0x4d	/* Set IDI5 Counter Match Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI6CntMatchValue 0x4e	/* Set IDI6 Counter Match Value
					 * 256*OMB1+OMB0 */
#define CMD_SetIDI7CntMatchValue 0x4f	/* Set IDI7 Counter Match Value
					 * 256*OMB1+OMB0 */

#define OMBCMD_RETRY	0x03	/* 3 times try request before error */

static int pci_dio_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it);
static int pci_dio_detach(struct comedi_device *dev);

struct diosubd_data {
	int chans;		/*  num of chans */
	int addr;		/*  PCI address ofset */
	int regs;		/*  number of registers to read or 8255
				    subdevices or 8254 chips */
	unsigned int specflags;	/*  addon subdevice flags */
};

struct dio_boardtype {
	const char *name;	/*  board name */
	int vendor_id;		/*  vendor/device PCI ID */
	int device_id;
	int main_pci_region;	/*  main I/O PCI region */
	enum hw_cards_id cardtype;
	struct diosubd_data sdi[MAX_DI_SUBDEVS];	/*  DI chans */
	struct diosubd_data sdo[MAX_DO_SUBDEVS];	/*  DO chans */
	struct diosubd_data sdio[MAX_DIO_SUBDEVG];	/*  DIO 8255 chans */
	struct diosubd_data boardid;	/*  card supports board ID switch */
	struct diosubd_data s8254[MAX_8254_SUBDEVS];	/* 8254 subdevices */
	enum hw_io_access io_access;
};

static DEFINE_PCI_DEVICE_TABLE(pci_dio_pci_table) = {
	{
	PCI_VENDOR_ID_ADVANTECH, 0x1730, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1733, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1734, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1735, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1736, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1750, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1751, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1752, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1753, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1754, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1756, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1760, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	PCI_VENDOR_ID_ADVANTECH, 0x1762, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	0}
};

MODULE_DEVICE_TABLE(pci, pci_dio_pci_table);

static const struct dio_boardtype boardtypes[] = {
	{"pci1730", PCI_VENDOR_ID_ADVANTECH, 0x1730, PCIDIO_MAINREG,
	 TYPE_PCI1730,
	 { {16, PCI1730_DI, 2, 0}, {16, PCI1730_IDI, 2, 0} },
	 { {16, PCI1730_DO, 2, 0}, {16, PCI1730_IDO, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI173x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1733", PCI_VENDOR_ID_ADVANTECH, 0x1733, PCIDIO_MAINREG,
	 TYPE_PCI1733,
	 { {0, 0, 0, 0}, {32, PCI1733_IDI, 4, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI173x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1734", PCI_VENDOR_ID_ADVANTECH, 0x1734, PCIDIO_MAINREG,
	 TYPE_PCI1734,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {32, PCI1734_IDO, 4, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI173x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1735", PCI_VENDOR_ID_ADVANTECH, 0x1735, PCIDIO_MAINREG,
	 TYPE_PCI1735,
	 { {32, PCI1735_DI, 4, 0}, {0, 0, 0, 0} },
	 { {32, PCI1735_DO, 4, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { 4, PCI1735_BOARDID, 1, SDF_INTERNAL},
	 { {3, PCI1735_C8254, 1, 0} },
	 IO_8b},
	{"pci1736", PCI_VENDOR_ID_ADVANTECH, 0x1736, PCI1736_MAINREG,
	 TYPE_PCI1736,
	 { {0, 0, 0, 0}, {16, PCI1736_IDI, 2, 0} },
	 { {0, 0, 0, 0}, {16, PCI1736_IDO, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI1736_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1750", PCI_VENDOR_ID_ADVANTECH, 0x1750, PCIDIO_MAINREG,
	 TYPE_PCI1750,
	 { {0, 0, 0, 0}, {16, PCI1750_IDI, 2, 0} },
	 { {0, 0, 0, 0}, {16, PCI1750_IDO, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1751", PCI_VENDOR_ID_ADVANTECH, 0x1751, PCIDIO_MAINREG,
	 TYPE_PCI1751,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {48, PCI1751_DIO, 2, 0}, {0, 0, 0, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1752", PCI_VENDOR_ID_ADVANTECH, 0x1752, PCIDIO_MAINREG,
	 TYPE_PCI1752,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {32, PCI1752_IDO, 2, 0}, {32, PCI1752_IDO2, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI175x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_16b},
	{"pci1753", PCI_VENDOR_ID_ADVANTECH, 0x1753, PCIDIO_MAINREG,
	 TYPE_PCI1753,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {96, PCI1753_DIO, 4, 0}, {0, 0, 0, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1753e", PCI_VENDOR_ID_ADVANTECH, 0x1753, PCIDIO_MAINREG,
	 TYPE_PCI1753E,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {96, PCI1753_DIO, 4, 0}, {96, PCI1753E_DIO, 4, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1754", PCI_VENDOR_ID_ADVANTECH, 0x1754, PCIDIO_MAINREG,
	 TYPE_PCI1754,
	 { {32, PCI1754_IDI, 2, 0}, {32, PCI1754_IDI2, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI175x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_16b},
	{"pci1756", PCI_VENDOR_ID_ADVANTECH, 0x1756, PCIDIO_MAINREG,
	 TYPE_PCI1756,
	 { {0, 0, 0, 0}, {32, PCI1756_IDI, 2, 0} },
	 { {0, 0, 0, 0}, {32, PCI1756_IDO, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI175x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_16b},
	{"pci1760", PCI_VENDOR_ID_ADVANTECH, 0x1760, 0,
	 TYPE_PCI1760,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} }, /* This card have own setup work */
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1762", PCI_VENDOR_ID_ADVANTECH, 0x1762, PCIDIO_MAINREG,
	 TYPE_PCI1762,
	 { {0, 0, 0, 0}, {16, PCI1762_IDI, 1, 0} },
	 { {0, 0, 0, 0}, {16, PCI1762_RO, 1, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI1762_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_16b}
};

#define n_boardtypes (sizeof(boardtypes)/sizeof(struct dio_boardtype))

static struct comedi_driver driver_pci_dio = {
	.driver_name = "adv_pci_dio",
	.module = THIS_MODULE,
	.attach = pci_dio_attach,
	.detach = pci_dio_detach
};

struct pci_dio_private {
	struct pci_dio_private *prev;	/*  previous private struct */
	struct pci_dio_private *next;	/*  next private struct */
	struct pci_dev *pcidev;	/*  pointer to board's pci_dev */
	char valid;		/*  card is usable */
	char GlobalIrqEnabled;	/*  1= any IRQ source is enabled */
	/*  PCI-1760 specific data */
	unsigned char IDICntEnable;	/* counter's counting enable status */
	unsigned char IDICntOverEnable;	/* counter's overflow interrupts enable
					 * status */
	unsigned char IDICntMatchEnable;	/* counter's match interrupts
						 * enable status */
	unsigned char IDICntEdge;	/* counter's count edge value
					 * (bit=0 - rising, =1 - falling) */
	unsigned short CntResValue[8];	/*  counters' reset value */
	unsigned short CntMatchValue[8]; /*  counters' match interrupt value */
	unsigned char IDIFiltersEn; /*  IDI's digital filters enable status */
	unsigned char IDIPatMatchEn;	/*  IDI's pattern match enable status */
	unsigned char IDIPatMatchValue;	/*  IDI's pattern match value */
	unsigned short IDIFiltrLow[8];	/*  IDI's filter value low signal */
	unsigned short IDIFiltrHigh[8];	/*  IDI's filter value high signal */
};

static struct pci_dio_private *pci_priv = NULL;	/* list of allocated cards */

#define devpriv ((struct pci_dio_private *)dev->private)
#define this_board ((const struct dio_boardtype *)dev->board_ptr)

/*
==============================================================================
*/
static int pci_dio_insn_bits_di_b(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	int i;

	data[1] = 0;
	for (i = 0; i < d->regs; i++)
		data[1] |= inb(dev->iobase + d->addr + i) << (8 * i);


	return 2;
}

/*
==============================================================================
*/
static int pci_dio_insn_bits_di_w(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	int i;

	data[1] = 0;
	for (i = 0; i < d->regs; i++)
		data[1] |= inw(dev->iobase + d->addr + 2 * i) << (16 * i);

	return 2;
}

/*
==============================================================================
*/
static int pci_dio_insn_bits_do_b(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	int i;

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
		for (i = 0; i < d->regs; i++)
			outb((s->state >> (8 * i)) & 0xff,
			     dev->iobase + d->addr + i);
	}
	data[1] = s->state;

	return 2;
}

/*
==============================================================================
*/
static int pci_dio_insn_bits_do_w(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	int i;

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
		for (i = 0; i < d->regs; i++)
			outw((s->state >> (16 * i)) & 0xffff,
			     dev->iobase + d->addr + 2 * i);
	}
	data[1] = s->state;

	return 2;
}

/*
==============================================================================
*/
static int pci_8254_insn_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	unsigned int chan, chip, chipchan;
	unsigned long flags;

	chan = CR_CHAN(insn->chanspec);	/* channel on subdevice */
	chip = chan / 3;		/* chip on subdevice */
	chipchan = chan - (3 * chip);	/* channel on chip on subdevice */
	spin_lock_irqsave(&s->spin_lock, flags);
	data[0] = i8254_read(dev->iobase + d->addr + (SIZE_8254 * chip),
			0, chipchan);
	spin_unlock_irqrestore(&s->spin_lock, flags);
	return 1;
}

/*
==============================================================================
*/
static int pci_8254_insn_write(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	unsigned int chan, chip, chipchan;
	unsigned long flags;

	chan = CR_CHAN(insn->chanspec);	/* channel on subdevice */
	chip = chan / 3;		/* chip on subdevice */
	chipchan = chan - (3 * chip);	/* channel on chip on subdevice */
	spin_lock_irqsave(&s->spin_lock, flags);
	i8254_write(dev->iobase + d->addr + (SIZE_8254 * chip),
			0, chipchan, data[0]);
	spin_unlock_irqrestore(&s->spin_lock, flags);
	return 1;
}

/*
==============================================================================
*/
static int pci_8254_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	unsigned int chan, chip, chipchan;
	unsigned long iobase;
	int ret = 0;
	unsigned long flags;

	chan = CR_CHAN(insn->chanspec);	/* channel on subdevice */
	chip = chan / 3;		/* chip on subdevice */
	chipchan = chan - (3 * chip);	/* channel on chip on subdevice */
	iobase = dev->iobase + d->addr + (SIZE_8254 * chip);
	spin_lock_irqsave(&s->spin_lock, flags);
	switch (data[0]) {
	case INSN_CONFIG_SET_COUNTER_MODE:
		ret = i8254_set_mode(iobase, 0, chipchan, data[1]);
		if (ret < 0)
			ret = -EINVAL;
		break;
	case INSN_CONFIG_8254_READ_STATUS:
		data[1] = i8254_status(iobase, 0, chipchan);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	spin_unlock_irqrestore(&s->spin_lock, flags);
	return ret < 0 ? ret : insn->n;
}

/*
==============================================================================
*/
static int pci1760_unchecked_mbxrequest(struct comedi_device *dev,
					unsigned char *omb, unsigned char *imb,
					int repeats)
{
	int cnt, tout, ok = 0;

	for (cnt = 0; cnt < repeats; cnt++) {
		outb(omb[0], dev->iobase + OMB0);
		outb(omb[1], dev->iobase + OMB1);
		outb(omb[2], dev->iobase + OMB2);
		outb(omb[3], dev->iobase + OMB3);
		for (tout = 0; tout < 251; tout++) {
			imb[2] = inb(dev->iobase + IMB2);
			if (imb[2] == omb[2]) {
				imb[0] = inb(dev->iobase + IMB0);
				imb[1] = inb(dev->iobase + IMB1);
				imb[3] = inb(dev->iobase + IMB3);
				ok = 1;
				break;
			}
			udelay(1);
		}
		if (ok)
			return 0;
	}

	comedi_error(dev, "PCI-1760 mailbox request timeout!");
	return -ETIME;
}

static int pci1760_clear_imb2(struct comedi_device *dev)
{
	unsigned char omb[4] = { 0x0, 0x0, CMD_ClearIMB2, 0x0 };
	unsigned char imb[4];
	/* check if imb2 is already clear */
	if (inb(dev->iobase + IMB2) == CMD_ClearIMB2)
		return 0;
	return pci1760_unchecked_mbxrequest(dev, omb, imb, OMBCMD_RETRY);
}

static int pci1760_mbxrequest(struct comedi_device *dev,
			      unsigned char *omb, unsigned char *imb)
{
	if (omb[2] == CMD_ClearIMB2) {
		comedi_error(dev,
			     "bug! this function should not be used for CMD_ClearIMB2 command");
		return -EINVAL;
	}
	if (inb(dev->iobase + IMB2) == omb[2]) {
		int retval;
		retval = pci1760_clear_imb2(dev);
		if (retval < 0)
			return retval;
	}
	return pci1760_unchecked_mbxrequest(dev, omb, imb, OMBCMD_RETRY);
}

/*
==============================================================================
*/
static int pci1760_insn_bits_di(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	data[1] = inb(dev->iobase + IMB3);

	return 2;
}

/*
==============================================================================
*/
static int pci1760_insn_bits_do(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int ret;
	unsigned char omb[4] = {
		0x00,
		0x00,
		CMD_SetRelaysOutput,
		0x00
	};
	unsigned char imb[4];

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
		omb[0] = s->state;
		ret = pci1760_mbxrequest(dev, omb, imb);
		if (!ret)
			return ret;
	}
	data[1] = s->state;

	return 2;
}

/*
==============================================================================
*/
static int pci1760_insn_cnt_read(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data)
{
	int ret, n;
	unsigned char omb[4] = {
		CR_CHAN(insn->chanspec) & 0x07,
		0x00,
		CMD_GetIDICntCurValue,
		0x00
	};
	unsigned char imb[4];

	for (n = 0; n < insn->n; n++) {
		ret = pci1760_mbxrequest(dev, omb, imb);
		if (!ret)
			return ret;
		data[n] = (imb[1] << 8) + imb[0];
	}

	return n;
}

/*
==============================================================================
*/
static int pci1760_insn_cnt_write(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	int ret;
	unsigned char chan = CR_CHAN(insn->chanspec) & 0x07;
	unsigned char bitmask = 1 << chan;
	unsigned char omb[4] = {
		data[0] & 0xff,
		(data[0] >> 8) & 0xff,
		CMD_SetIDI0CntResetValue + chan,
		0x00
	};
	unsigned char imb[4];

	/* Set reset value if different */
	if (devpriv->CntResValue[chan] != (data[0] & 0xffff)) {
		ret = pci1760_mbxrequest(dev, omb, imb);
		if (!ret)
			return ret;
		devpriv->CntResValue[chan] = data[0] & 0xffff;
	}

	omb[0] = bitmask;	/*  reset counter to it reset value */
	omb[2] = CMD_ResetIDICounters;
	ret = pci1760_mbxrequest(dev, omb, imb);
	if (!ret)
		return ret;

	/*  start counter if it don't run */
	if (!(bitmask & devpriv->IDICntEnable)) {
		omb[0] = bitmask;
		omb[2] = CMD_EnableIDICounters;
		ret = pci1760_mbxrequest(dev, omb, imb);
		if (!ret)
			return ret;
		devpriv->IDICntEnable |= bitmask;
	}
	return 1;
}

/*
==============================================================================
*/
static int pci1760_reset(struct comedi_device *dev)
{
	int i;
	unsigned char omb[4] = { 0x00, 0x00, 0x00, 0x00 };
	unsigned char imb[4];

	outb(0, dev->iobase + INTCSR0);	/*  disable IRQ */
	outb(0, dev->iobase + INTCSR1);
	outb(0, dev->iobase + INTCSR2);
	outb(0, dev->iobase + INTCSR3);
	devpriv->GlobalIrqEnabled = 0;

	omb[0] = 0x00;
	omb[2] = CMD_SetRelaysOutput;	/*  reset relay outputs */
	pci1760_mbxrequest(dev, omb, imb);

	omb[0] = 0x00;
	omb[2] = CMD_EnableIDICounters;	/*  disable IDI up counters */
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDICntEnable = 0;

	omb[0] = 0x00;
	omb[2] = CMD_OverflowIDICounters; /* disable counters overflow
					   * interrupts */
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDICntOverEnable = 0;

	omb[0] = 0x00;
	omb[2] = CMD_MatchIntIDICounters; /* disable counters match value
					   * interrupts */
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDICntMatchEnable = 0;

	omb[0] = 0x00;
	omb[1] = 0x80;
	for (i = 0; i < 8; i++) {	/*  set IDI up counters match value */
		omb[2] = CMD_SetIDI0CntMatchValue + i;
		pci1760_mbxrequest(dev, omb, imb);
		devpriv->CntMatchValue[i] = 0x8000;
	}

	omb[0] = 0x00;
	omb[1] = 0x00;
	for (i = 0; i < 8; i++) {	/*  set IDI up counters reset value */
		omb[2] = CMD_SetIDI0CntResetValue + i;
		pci1760_mbxrequest(dev, omb, imb);
		devpriv->CntResValue[i] = 0x0000;
	}

	omb[0] = 0xff;
	omb[2] = CMD_ResetIDICounters; /* reset IDI up counters to reset
					* values */
	pci1760_mbxrequest(dev, omb, imb);

	omb[0] = 0x00;
	omb[2] = CMD_EdgeIDICounters;	/*  set IDI up counters count edge */
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDICntEdge = 0x00;

	omb[0] = 0x00;
	omb[2] = CMD_EnableIDIFilters;	/*  disable all digital in filters */
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDIFiltersEn = 0x00;

	omb[0] = 0x00;
	omb[2] = CMD_EnableIDIPatternMatch;	/*  disable pattern matching */
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDIPatMatchEn = 0x00;

	omb[0] = 0x00;
	omb[2] = CMD_SetIDIPatternMatch;	/*  set pattern match value */
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDIPatMatchValue = 0x00;

	return 0;
}

/*
==============================================================================
*/
static int pci_dio_reset(struct comedi_device *dev)
{
	DPRINTK("adv_pci_dio EDBG: BGN: pci171x_reset(...)\n");

	switch (this_board->cardtype) {
	case TYPE_PCI1730:
		outb(0, dev->iobase + PCI1730_DO);	/*  clear outputs */
		outb(0, dev->iobase + PCI1730_DO + 1);
		outb(0, dev->iobase + PCI1730_IDO);
		outb(0, dev->iobase + PCI1730_IDO + 1);
		/* NO break there! */
	case TYPE_PCI1733:
		/* disable interrupts */
		outb(0, dev->iobase + PCI1730_3_INT_EN);
		/* clear interrupts */
		outb(0x0f, dev->iobase + PCI1730_3_INT_CLR);
		/* set rising edge trigger */
		outb(0, dev->iobase + PCI1730_3_INT_RF);
		break;
	case TYPE_PCI1734:
		outb(0, dev->iobase + PCI1734_IDO);	/*  clear outputs */
		outb(0, dev->iobase + PCI1734_IDO + 1);
		outb(0, dev->iobase + PCI1734_IDO + 2);
		outb(0, dev->iobase + PCI1734_IDO + 3);
		break;
	case TYPE_PCI1735:
		outb(0, dev->iobase + PCI1735_DO);	/*  clear outputs */
		outb(0, dev->iobase + PCI1735_DO + 1);
		outb(0, dev->iobase + PCI1735_DO + 2);
		outb(0, dev->iobase + PCI1735_DO + 3);
		i8254_set_mode(dev->iobase + PCI1735_C8254, 0, 0, I8254_MODE0);
		i8254_set_mode(dev->iobase + PCI1735_C8254, 0, 1, I8254_MODE0);
		i8254_set_mode(dev->iobase + PCI1735_C8254, 0, 2, I8254_MODE0);
		break;

	case TYPE_PCI1736:
		outb(0, dev->iobase + PCI1736_IDO);
		outb(0, dev->iobase + PCI1736_IDO + 1);
		/* disable interrupts */
		outb(0, dev->iobase + PCI1736_3_INT_EN);
		/* clear interrupts */
		outb(0x0f, dev->iobase + PCI1736_3_INT_CLR);
		/* set rising edge trigger */
		outb(0, dev->iobase + PCI1736_3_INT_RF);
		break;

	case TYPE_PCI1750:
	case TYPE_PCI1751:
		/* disable & clear interrupts */
		outb(0x88, dev->iobase + PCI1750_ICR);
		break;
	case TYPE_PCI1752:
		outw(0, dev->iobase + PCI1752_6_CFC); /* disable channel freeze
						       * function */
		outw(0, dev->iobase + PCI1752_IDO);	/*  clear outputs */
		outw(0, dev->iobase + PCI1752_IDO + 2);
		outw(0, dev->iobase + PCI1752_IDO2);
		outw(0, dev->iobase + PCI1752_IDO2 + 2);
		break;
	case TYPE_PCI1753E:
		outb(0x88, dev->iobase + PCI1753E_ICR0); /* disable & clear
							  * interrupts */
		outb(0x80, dev->iobase + PCI1753E_ICR1);
		outb(0x80, dev->iobase + PCI1753E_ICR2);
		outb(0x80, dev->iobase + PCI1753E_ICR3);
		/* NO break there! */
	case TYPE_PCI1753:
		outb(0x88, dev->iobase + PCI1753_ICR0); /* disable & clear
							 * interrupts */
		outb(0x80, dev->iobase + PCI1753_ICR1);
		outb(0x80, dev->iobase + PCI1753_ICR2);
		outb(0x80, dev->iobase + PCI1753_ICR3);
		break;
	case TYPE_PCI1754:
		outw(0x08, dev->iobase + PCI1754_6_ICR0); /* disable and clear
							   * interrupts */
		outw(0x08, dev->iobase + PCI1754_6_ICR1);
		outw(0x08, dev->iobase + PCI1754_ICR2);
		outw(0x08, dev->iobase + PCI1754_ICR3);
		break;
	case TYPE_PCI1756:
		outw(0, dev->iobase + PCI1752_6_CFC); /* disable channel freeze
						       * function */
		outw(0x08, dev->iobase + PCI1754_6_ICR0); /* disable and clear
							   * interrupts */
		outw(0x08, dev->iobase + PCI1754_6_ICR1);
		outw(0, dev->iobase + PCI1756_IDO);	/*  clear outputs */
		outw(0, dev->iobase + PCI1756_IDO + 2);
		break;
	case TYPE_PCI1760:
		pci1760_reset(dev);
		break;
	case TYPE_PCI1762:
		outw(0x0101, dev->iobase + PCI1762_ICR); /* disable & clear
							  * interrupts */
		break;
	}

	DPRINTK("adv_pci_dio EDBG: END: pci171x_reset(...)\n");

	return 0;
}

/*
==============================================================================
*/
static int pci1760_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int subdev = 0;

	s = dev->subdevices + subdev;
	s->type = COMEDI_SUBD_DI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_COMMON;
	s->n_chan = 8;
	s->maxdata = 1;
	s->len_chanlist = 8;
	s->range_table = &range_digital;
	s->insn_bits = pci1760_insn_bits_di;
	subdev++;

	s = dev->subdevices + subdev;
	s->type = COMEDI_SUBD_DO;
	s->subdev_flags = SDF_WRITABLE | SDF_GROUND | SDF_COMMON;
	s->n_chan = 8;
	s->maxdata = 1;
	s->len_chanlist = 8;
	s->range_table = &range_digital;
	s->state = 0;
	s->insn_bits = pci1760_insn_bits_do;
	subdev++;

	s = dev->subdevices + subdev;
	s->type = COMEDI_SUBD_TIMER;
	s->subdev_flags = SDF_WRITABLE | SDF_LSAMPL;
	s->n_chan = 2;
	s->maxdata = 0xffffffff;
	s->len_chanlist = 2;
/*       s->insn_config=pci1760_insn_pwm_cfg; */
	subdev++;

	s = dev->subdevices + subdev;
	s->type = COMEDI_SUBD_COUNTER;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 8;
	s->maxdata = 0xffff;
	s->len_chanlist = 8;
	s->insn_read = pci1760_insn_cnt_read;
	s->insn_write = pci1760_insn_cnt_write;
/*       s->insn_config=pci1760_insn_cnt_cfg; */
	subdev++;

	return 0;
}

/*
==============================================================================
*/
static int pci_dio_add_di(struct comedi_device *dev, struct comedi_subdevice *s,
			  const struct diosubd_data *d, int subdev)
{
	s->type = COMEDI_SUBD_DI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_COMMON | d->specflags;
	if (d->chans > 16)
		s->subdev_flags |= SDF_LSAMPL;
	s->n_chan = d->chans;
	s->maxdata = 1;
	s->len_chanlist = d->chans;
	s->range_table = &range_digital;
	switch (this_board->io_access) {
	case IO_8b:
		s->insn_bits = pci_dio_insn_bits_di_b;
		break;
	case IO_16b:
		s->insn_bits = pci_dio_insn_bits_di_w;
		break;
	}
	s->private = (void *)d;

	return 0;
}

/*
==============================================================================
*/
static int pci_dio_add_do(struct comedi_device *dev, struct comedi_subdevice *s,
			  const struct diosubd_data *d, int subdev)
{
	s->type = COMEDI_SUBD_DO;
	s->subdev_flags = SDF_WRITABLE | SDF_GROUND | SDF_COMMON;
	if (d->chans > 16)
		s->subdev_flags |= SDF_LSAMPL;
	s->n_chan = d->chans;
	s->maxdata = 1;
	s->len_chanlist = d->chans;
	s->range_table = &range_digital;
	s->state = 0;
	switch (this_board->io_access) {
	case IO_8b:
		s->insn_bits = pci_dio_insn_bits_do_b;
		break;
	case IO_16b:
		s->insn_bits = pci_dio_insn_bits_do_w;
		break;
	}
	s->private = (void *)d;

	return 0;
}

/*
==============================================================================
*/
static int pci_dio_add_8254(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    const struct diosubd_data *d, int subdev)
{
	s->type = COMEDI_SUBD_COUNTER;
	s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
	s->n_chan = d->chans;
	s->maxdata = 65535;
	s->len_chanlist = d->chans;
	s->insn_read = pci_8254_insn_read;
	s->insn_write = pci_8254_insn_write;
	s->insn_config = pci_8254_insn_config;
	s->private = (void *)d;

	return 0;
}

/*
==============================================================================
*/
static int CheckAndAllocCard(struct comedi_device *dev,
			     struct comedi_devconfig *it,
			     struct pci_dev *pcidev)
{
	struct pci_dio_private *pr, *prev;

	for (pr = pci_priv, prev = NULL; pr != NULL; prev = pr, pr = pr->next) {
		if (pr->pcidev == pcidev)
			return 0; /* this card is used, look for another */

	}

	if (prev) {
		devpriv->prev = prev;
		prev->next = devpriv;
	} else {
		pci_priv = devpriv;
	}

	devpriv->pcidev = pcidev;

	return 1;
}

/*
==============================================================================
*/
static int pci_dio_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int ret, subdev, n_subdevices, i, j;
	unsigned long iobase;
	struct pci_dev *pcidev = NULL;

	printk("comedi%d: adv_pci_dio: ", dev->minor);

	ret = alloc_private(dev, sizeof(struct pci_dio_private));
	if (ret < 0) {
		printk(", Error: Cann't allocate private memory!\n");
		return -ENOMEM;
	}

	for_each_pci_dev(pcidev) {
		/*  loop through cards supported by this driver */
		for (i = 0; i < n_boardtypes; ++i) {
			if (boardtypes[i].vendor_id != pcidev->vendor)
				continue;
			if (boardtypes[i].device_id != pcidev->device)
				continue;
			/*  was a particular bus/slot requested? */
			if (it->options[0] || it->options[1]) {
				/*  are we on the wrong bus/slot? */
				if (pcidev->bus->number != it->options[0] ||
				    PCI_SLOT(pcidev->devfn) != it->options[1]) {
					continue;
				}
			}
			ret = CheckAndAllocCard(dev, it, pcidev);
			if (ret != 1)
				continue;
			dev->board_ptr = boardtypes + i;
			break;
		}
		if (dev->board_ptr)
			break;
	}

	if (!dev->board_ptr) {
		printk(", Error: Requested type of the card was not found!\n");
		return -EIO;
	}

	if (comedi_pci_enable(pcidev, driver_pci_dio.driver_name)) {
		printk
		    (", Error: Can't enable PCI device and request regions!\n");
		return -EIO;
	}
	iobase = pci_resource_start(pcidev, this_board->main_pci_region);
	printk(", b:s:f=%d:%d:%d, io=0x%4lx",
	       pcidev->bus->number, PCI_SLOT(pcidev->devfn),
	       PCI_FUNC(pcidev->devfn), iobase);

	dev->iobase = iobase;
	dev->board_name = this_board->name;

	if (this_board->cardtype == TYPE_PCI1760) {
		n_subdevices = 4;	/*  8 IDI, 8 IDO, 2 PWM, 8 CNT */
	} else {
		n_subdevices = 0;
		for (i = 0; i < MAX_DI_SUBDEVS; i++)
			if (this_board->sdi[i].chans)
				n_subdevices++;
		for (i = 0; i < MAX_DO_SUBDEVS; i++)
			if (this_board->sdo[i].chans)
				n_subdevices++;
		for (i = 0; i < MAX_DIO_SUBDEVG; i++)
			n_subdevices += this_board->sdio[i].regs;
		if (this_board->boardid.chans)
			n_subdevices++;
		for (i = 0; i < MAX_8254_SUBDEVS; i++)
			if (this_board->s8254[i].chans)
				n_subdevices++;
	}

	ret = alloc_subdevices(dev, n_subdevices);
	if (ret < 0) {
		printk(", Error: Cann't allocate subdevice memory!\n");
		return ret;
	}

	printk(".\n");

	subdev = 0;

	for (i = 0; i < MAX_DI_SUBDEVS; i++)
		if (this_board->sdi[i].chans) {
			s = dev->subdevices + subdev;
			pci_dio_add_di(dev, s, &this_board->sdi[i], subdev);
			subdev++;
		}

	for (i = 0; i < MAX_DO_SUBDEVS; i++)
		if (this_board->sdo[i].chans) {
			s = dev->subdevices + subdev;
			pci_dio_add_do(dev, s, &this_board->sdo[i], subdev);
			subdev++;
		}

	for (i = 0; i < MAX_DIO_SUBDEVG; i++)
		for (j = 0; j < this_board->sdio[i].regs; j++) {
			s = dev->subdevices + subdev;
			subdev_8255_init(dev, s, NULL,
					 dev->iobase +
					 this_board->sdio[i].addr +
					 SIZE_8255 * j);
			subdev++;
		}

	if (this_board->boardid.chans) {
		s = dev->subdevices + subdev;
		s->type = COMEDI_SUBD_DI;
		pci_dio_add_di(dev, s, &this_board->boardid, subdev);
		subdev++;
	}

	for (i = 0; i < MAX_8254_SUBDEVS; i++)
		if (this_board->s8254[i].chans) {
			s = dev->subdevices + subdev;
			pci_dio_add_8254(dev, s, &this_board->s8254[i], subdev);
			subdev++;
		}

	if (this_board->cardtype == TYPE_PCI1760)
		pci1760_attach(dev, it);

	devpriv->valid = 1;

	pci_dio_reset(dev);

	return 0;
}

/*
==============================================================================
*/
static int pci_dio_detach(struct comedi_device *dev)
{
	int i, j;
	struct comedi_subdevice *s;
	int subdev;

	if (dev->private) {
		if (devpriv->valid)
			pci_dio_reset(dev);


		/* This shows the silliness of using this kind of
		 * scheme for numbering subdevices.  Don't do it.  --ds */
		subdev = 0;
		for (i = 0; i < MAX_DI_SUBDEVS; i++) {
			if (this_board->sdi[i].chans)
				subdev++;

		}
		for (i = 0; i < MAX_DO_SUBDEVS; i++) {
			if (this_board->sdo[i].chans)
				subdev++;

		}
		for (i = 0; i < MAX_DIO_SUBDEVG; i++) {
			for (j = 0; j < this_board->sdio[i].regs; j++) {
				s = dev->subdevices + subdev;
				subdev_8255_cleanup(dev, s);
				subdev++;
			}
		}

		if (this_board->boardid.chans)
			subdev++;

		for (i = 0; i < MAX_8254_SUBDEVS; i++)
			if (this_board->s8254[i].chans)
				subdev++;

		for (i = 0; i < dev->n_subdevices; i++) {
			s = dev->subdevices + i;
			s->private = NULL;
		}

		if (devpriv->pcidev) {
			if (dev->iobase)
				comedi_pci_disable(devpriv->pcidev);

			pci_dev_put(devpriv->pcidev);
		}

		if (devpriv->prev)
			devpriv->prev->next = devpriv->next;
		else
			pci_priv = devpriv->next;

		if (devpriv->next)
			devpriv->next->prev = devpriv->prev;

	}

	return 0;
}

/*
==============================================================================
*/
static int __devinit driver_pci_dio_pci_probe(struct pci_dev *dev,
					      const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, driver_pci_dio.driver_name);
}

static void __devexit driver_pci_dio_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_pci_dio_pci_driver = {
	.id_table = pci_dio_pci_table,
	.probe = &driver_pci_dio_pci_probe,
	.remove = __devexit_p(&driver_pci_dio_pci_remove)
};

static int __init driver_pci_dio_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_pci_dio);
	if (retval < 0)
		return retval;

	driver_pci_dio_pci_driver.name = (char *)driver_pci_dio.driver_name;
	return pci_register_driver(&driver_pci_dio_pci_driver);
}

static void __exit driver_pci_dio_cleanup_module(void)
{
	pci_unregister_driver(&driver_pci_dio_pci_driver);
	comedi_driver_unregister(&driver_pci_dio);
}

module_init(driver_pci_dio_init_module);
module_exit(driver_pci_dio_cleanup_module);
/*
==============================================================================
*/

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
