/*
*
*   (c) 1999 by Computone Corporation
*
********************************************************************************
*
*   PACKAGE:     Linux tty Device Driver for IntelliPort family of multiport
*                serial I/O controllers.
*
*   DESCRIPTION: Mainline code for the device driver
*
*******************************************************************************/
// ToDo:
//
// Fix the immediate DSS_NOW problem.
// Work over the channel stats return logic in ip2_ipl_ioctl so they
//	make sense for all 256 possible channels and so the user space
//	utilities will compile and work properly.
//
// Done:
//
// 1.2.14	/\/\|=mhw=|\/\/
// Added bounds checking to ip2_ipl_ioctl to avoid potential terroristic acts.
// Changed the definition of ip2trace to be more consistent with kernel style
//	Thanks to Andreas Dilger <adilger@turbolabs.com> for these updates
//
// 1.2.13	/\/\|=mhw=|\/\/
// DEVFS: Renamed ttf/{n} to tts/F{n} and cuf/{n} to cua/F{n} to conform
//	to agreed devfs serial device naming convention.
//
// 1.2.12	/\/\|=mhw=|\/\/
// Cleaned up some remove queue cut and paste errors
//
// 1.2.11	/\/\|=mhw=|\/\/
// Clean up potential NULL pointer dereferences
// Clean up devfs registration
// Add kernel command line parsing for io and irq
//	Compile defaults for io and irq are now set in ip2.c not ip2.h!
// Reworked poll_only hack for explicit parameter setting
//	You must now EXPLICITLY set poll_only = 1 or set all irqs to 0
// Merged ip2_loadmain and old_ip2_init
// Converted all instances of interruptible_sleep_on into queue calls
//	Most of these had no race conditions but better to clean up now
//
// 1.2.10	/\/\|=mhw=|\/\/
// Fixed the bottom half interrupt handler and enabled USE_IQI
//	to split the interrupt handler into a formal top-half / bottom-half
// Fixed timing window on high speed processors that queued messages to
// 	the outbound mail fifo faster than the board could handle.
//
// 1.2.9
// Four box EX was barfing on >128k kmalloc, made structure smaller by
// reducing output buffer size
//
// 1.2.8
// Device file system support (MHW)
//
// 1.2.7 
// Fixed
// Reload of ip2 without unloading ip2main hangs system on cat of /proc/modules
//
// 1.2.6
//Fixes DCD problems
//	DCD was not reported when CLOCAL was set on call to TIOCMGET
//
//Enhancements:
//	TIOCMGET requests and waits for status return
//	No DSS interrupts enabled except for DCD when needed
//
// For internal use only
//
//#define IP2DEBUG_INIT
//#define IP2DEBUG_OPEN
//#define IP2DEBUG_WRITE
//#define IP2DEBUG_READ
//#define IP2DEBUG_IOCTL
//#define IP2DEBUG_IPL

//#define IP2DEBUG_TRACE
//#define DEBUG_FIFO

/************/
/* Includes */
/************/

#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/wait.h>
#include <linux/device.h>

#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/termios.h>
#include <linux/tty_driver.h>
#include <linux/serial.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>

#include <linux/cdk.h>
#include <linux/comstats.h>
#include <linux/delay.h>
#include <linux/bitops.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/vmalloc.h>
#include <linux/init.h>

#include <asm/uaccess.h>

#include "ip2types.h"
#include "ip2trace.h"
#include "ip2ioctl.h"
#include "ip2.h"
#include "i2ellis.h"
#include "i2lib.h"

/*****************
 * /proc/ip2mem  *
 *****************/

#include <linux/proc_fs.h>

static int ip2_read_procmem(char *, char **, off_t, int);
static int ip2_read_proc(char *, char **, off_t, int, int *, void * );

/********************/
/* Type Definitions */
/********************/

/*************/
/* Constants */
/*************/

/* String constants to identify ourselves */
static char *pcName    = "Computone IntelliPort Plus multiport driver";
static char *pcVersion = "1.2.14";

/* String constants for port names */
static char *pcDriver_name   = "ip2";
static char *pcIpl    		 = "ip2ipl";

/* Serial subtype definitions */
#define SERIAL_TYPE_NORMAL    1

// cheezy kludge or genius - you decide?
int ip2_loadmain(int *, int *, unsigned char *, int);
static unsigned char *Fip_firmware;
static int Fip_firmware_size;

/***********************/
/* Function Prototypes */
/***********************/

/* Global module entry functions */

/* Private (static) functions */
static int  ip2_open(PTTY, struct file *);
static void ip2_close(PTTY, struct file *);
static int  ip2_write(PTTY, const unsigned char *, int);
static void ip2_putchar(PTTY, unsigned char);
static void ip2_flush_chars(PTTY);
static int  ip2_write_room(PTTY);
static int  ip2_chars_in_buf(PTTY);
static void ip2_flush_buffer(PTTY);
static int  ip2_ioctl(PTTY, struct file *, UINT, ULONG);
static void ip2_set_termios(PTTY, struct ktermios *);
static void ip2_set_line_discipline(PTTY);
static void ip2_throttle(PTTY);
static void ip2_unthrottle(PTTY);
static void ip2_stop(PTTY);
static void ip2_start(PTTY);
static void ip2_hangup(PTTY);
static int  ip2_tiocmget(struct tty_struct *tty, struct file *file);
static int  ip2_tiocmset(struct tty_struct *tty, struct file *file,
			 unsigned int set, unsigned int clear);

static void set_irq(int, int);
static void ip2_interrupt_bh(struct work_struct *work);
static irqreturn_t ip2_interrupt(int irq, void *dev_id);
static void ip2_poll(unsigned long arg);
static inline void service_all_boards(void);
static void do_input(struct work_struct *);
static void do_status(struct work_struct *);

static void ip2_wait_until_sent(PTTY,int);

static void set_params (i2ChanStrPtr, struct ktermios *);
static int get_serial_info(i2ChanStrPtr, struct serial_struct __user *);
static int set_serial_info(i2ChanStrPtr, struct serial_struct __user *);

static ssize_t ip2_ipl_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t ip2_ipl_write(struct file *, const char __user *, size_t, loff_t *);
static int ip2_ipl_ioctl(struct inode *, struct file *, UINT, ULONG);
static int ip2_ipl_open(struct inode *, struct file *);

static int DumpTraceBuffer(char __user *, int);
static int DumpFifoBuffer( char __user *, int);

static void ip2_init_board(int);
static unsigned short find_eisa_board(int);

/***************/
/* Static Data */
/***************/

static struct tty_driver *ip2_tty_driver;

/* Here, then is a table of board pointers which the interrupt routine should
 * scan through to determine who it must service.
 */
static unsigned short i2nBoards; // Number of boards here

static i2eBordStrPtr i2BoardPtrTable[IP2_MAX_BOARDS];

static i2ChanStrPtr  DevTable[IP2_MAX_PORTS];
//DevTableMem just used to save addresses for kfree
static void  *DevTableMem[IP2_MAX_BOARDS];

/* This is the driver descriptor for the ip2ipl device, which is used to
 * download the loadware to the boards.
 */
static const struct file_operations ip2_ipl = {
	.owner		= THIS_MODULE,
	.read		= ip2_ipl_read,
	.write		= ip2_ipl_write,
	.ioctl		= ip2_ipl_ioctl,
	.open		= ip2_ipl_open,
}; 

static unsigned long irq_counter = 0;
static unsigned long bh_counter = 0;

// Use immediate queue to service interrupts
#define USE_IQI
//#define USE_IQ	// PCI&2.2 needs work

/* The timer_list entry for our poll routine. If interrupt operation is not
 * selected, the board is serviced periodically to see if anything needs doing.
 */
#define  POLL_TIMEOUT   (jiffies + 1)
static DEFINE_TIMER(PollTimer, ip2_poll, 0, 0);
static char  TimerOn;

#ifdef IP2DEBUG_TRACE
/* Trace (debug) buffer data */
#define TRACEMAX  1000
static unsigned long tracebuf[TRACEMAX];
static int tracestuff;
static int tracestrip;
static int tracewrap;
#endif

/**********/
/* Macros */
/**********/

#if defined(MODULE) && defined(IP2DEBUG_OPEN)
#define DBG_CNT(s) printk(KERN_DEBUG "(%s): [%x] refc=%d, ttyc=%d, modc=%x -> %s\n", \
		    tty->name,(pCh->flags),ip2_tty_driver->refcount, \
		    tty->count,/*GET_USE_COUNT(module)*/0,s)
#else
#define DBG_CNT(s)
#endif

/********/
/* Code */
/********/

#include "i2ellis.c"    /* Extremely low-level interface services */
#include "i2cmd.c"      /* Standard loadware command definitions */
#include "i2lib.c"      /* High level interface services */

/* Configuration area for modprobe */

MODULE_AUTHOR("Doug McNash");
MODULE_DESCRIPTION("Computone IntelliPort Plus Driver");

static int poll_only = 0;

static int Eisa_irq;
static int Eisa_slot;

static int iindx;
static char rirqs[IP2_MAX_BOARDS];
static int Valid_Irqs[] = { 3, 4, 5, 7, 10, 11, 12, 15, 0};

/* for sysfs class support */
static struct class *ip2_class;

// Some functions to keep track of what irq's we have

static int
is_valid_irq(int irq)
{
	int *i = Valid_Irqs;
	
	while ((*i != 0) && (*i != irq)) {
		i++;
	}
	return (*i);
}

static void
mark_requested_irq( char irq )
{
	rirqs[iindx++] = irq;
}

#ifdef MODULE
static int
clear_requested_irq( char irq )
{
	int i;
	for ( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		if (rirqs[i] == irq) {
			rirqs[i] = 0;
			return 1;
		}
	}
	return 0;
}
#endif

static int
have_requested_irq( char irq )
{
	// array init to zeros so 0 irq will not be requested as a side effect
	int i;
	for ( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		if (rirqs[i] == irq)
			return 1;
	}
	return 0;
}

/******************************************************************************/
/* Function:   init_module()                                                  */
/* Parameters: None                                                           */
/* Returns:    Success (0)                                                    */
/*                                                                            */
/* Description:                                                               */
/* This is a required entry point for an installable module. It simply calls  */
/* the driver initialisation function and returns what it returns.            */
/******************************************************************************/
#ifdef MODULE
int
init_module(void)
{
#ifdef IP2DEBUG_INIT
	printk (KERN_DEBUG "Loading module ...\n" );
#endif
    return 0;
}
#endif /* MODULE */

/******************************************************************************/
/* Function:   cleanup_module()                                               */
/* Parameters: None                                                           */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/* This is a required entry point for an installable module. It has to return */
/* the device and the driver to a passive state. It should not be necessary   */
/* to reset the board fully, especially as the loadware is downloaded         */
/* externally rather than in the driver. We just want to disable the board    */
/* and clear the loadware to a reset state. To allow this there has to be a   */
/* way to detect whether the board has the loadware running at init time to   */
/* handle subsequent installations of the driver. All memory allocated by the */
/* driver should be returned since it may be unloaded from memory.            */
/******************************************************************************/
#ifdef MODULE
void
cleanup_module(void)
{
	int err;
	int i;

#ifdef IP2DEBUG_INIT
	printk (KERN_DEBUG "Unloading %s: version %s\n", pcName, pcVersion );
#endif
	/* Stop poll timer if we had one. */
	if ( TimerOn ) {
		del_timer ( &PollTimer );
		TimerOn = 0;
	}

	/* Reset the boards we have. */
	for( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		if ( i2BoardPtrTable[i] ) {
			iiReset( i2BoardPtrTable[i] );
		}
	}

	/* The following is done at most once, if any boards were installed. */
	for ( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		if ( i2BoardPtrTable[i] ) {
			iiResetDelay( i2BoardPtrTable[i] );
			/* free io addresses and Tibet */
			release_region( ip2config.addr[i], 8 );
			class_device_destroy(ip2_class, MKDEV(IP2_IPL_MAJOR, 4 * i));
			class_device_destroy(ip2_class, MKDEV(IP2_IPL_MAJOR, 4 * i + 1));
		}
		/* Disable and remove interrupt handler. */
		if ( (ip2config.irq[i] > 0) && have_requested_irq(ip2config.irq[i]) ) {	
			free_irq ( ip2config.irq[i], (void *)&pcName);
			clear_requested_irq( ip2config.irq[i]);
		}
	}
	class_destroy(ip2_class);
	if ( ( err = tty_unregister_driver ( ip2_tty_driver ) ) ) {
		printk(KERN_ERR "IP2: failed to unregister tty driver (%d)\n", err);
	}
	put_tty_driver(ip2_tty_driver);
	if ( ( err = unregister_chrdev ( IP2_IPL_MAJOR, pcIpl ) ) ) {
		printk(KERN_ERR "IP2: failed to unregister IPL driver (%d)\n", err);
	}
	remove_proc_entry("ip2mem", &proc_root);

	// free memory
	for (i = 0; i < IP2_MAX_BOARDS; i++) {
		void *pB;
#ifdef CONFIG_PCI
		if (ip2config.type[i] == PCI && ip2config.pci_dev[i]) {
			pci_disable_device(ip2config.pci_dev[i]);
			pci_dev_put(ip2config.pci_dev[i]);
			ip2config.pci_dev[i] = NULL;
		}
#endif
		if ((pB = i2BoardPtrTable[i]) != 0 ) {
			kfree ( pB );
			i2BoardPtrTable[i] = NULL;
		}
		if ((DevTableMem[i]) != NULL ) {
			kfree ( DevTableMem[i]  );
			DevTableMem[i] = NULL;
		}
	}

	/* Cleanup the iiEllis subsystem. */
	iiEllisCleanup();
#ifdef IP2DEBUG_INIT
	printk (KERN_DEBUG "IP2 Unloaded\n" );
#endif
}
#endif /* MODULE */

static const struct tty_operations ip2_ops = {
	.open            = ip2_open,
	.close           = ip2_close,
	.write           = ip2_write,
	.put_char        = ip2_putchar,
	.flush_chars     = ip2_flush_chars,
	.write_room      = ip2_write_room,
	.chars_in_buffer = ip2_chars_in_buf,
	.flush_buffer    = ip2_flush_buffer,
	.ioctl           = ip2_ioctl,
	.throttle        = ip2_throttle,
	.unthrottle      = ip2_unthrottle,
	.set_termios     = ip2_set_termios,
	.set_ldisc       = ip2_set_line_discipline,
	.stop            = ip2_stop,
	.start           = ip2_start,
	.hangup          = ip2_hangup,
	.read_proc       = ip2_read_proc,
	.tiocmget	 = ip2_tiocmget,
	.tiocmset	 = ip2_tiocmset,
};

/******************************************************************************/
/* Function:   ip2_loadmain()                                                 */
/* Parameters: irq, io from command line of insmod et. al.                    */
/*		pointer to fip firmware and firmware size for boards	      */
/* Returns:    Success (0)                                                    */
/*                                                                            */
/* Description:                                                               */
/* This was the required entry point for all drivers (now in ip2.c)           */
/* It performs all                                                            */
/* initialisation of the devices and driver structures, and registers itself  */
/* with the relevant kernel modules.                                          */
/******************************************************************************/
/* IRQF_DISABLED - if set blocks all interrupts else only this line */
/* IRQF_SHARED    - for shared irq PCI or maybe EISA only */
/* SA_RANDOM   - can be source for cert. random number generators */
#define IP2_SA_FLAGS	0

int
ip2_loadmain(int *iop, int *irqp, unsigned char *firmware, int firmsize) 
{
	int i, j, box;
	int err = 0;
	int status = 0;
	static int loaded;
	i2eBordStrPtr pB = NULL;
	int rc = -1;
	static struct pci_dev *pci_dev_i = NULL;

	ip2trace (ITRC_NO_PORT, ITRC_INIT, ITRC_ENTER, 0 );

	/* process command line arguments to modprobe or
		insmod i.e. iop & irqp */
	/* irqp and iop should ALWAYS be specified now...  But we check
		them individually just to be sure, anyways... */
	for ( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		if (iop) {
			ip2config.addr[i] = iop[i];
			if (irqp) {
				if( irqp[i] >= 0 ) {
					ip2config.irq[i] = irqp[i];
				} else {
					ip2config.irq[i] = 0;
				}
	// This is a little bit of a hack.  If poll_only=1 on command
	// line back in ip2.c OR all IRQs on all specified boards are
	// explicitly set to 0, then drop to poll only mode and override
	// PCI or EISA interrupts.  This superceeds the old hack of
	// triggering if all interrupts were zero (like da default).
	// Still a hack but less prone to random acts of terrorism.
	//
	// What we really should do, now that the IRQ default is set
	// to -1, is to use 0 as a hard coded, do not probe.
	//
	//	/\/\|=mhw=|\/\/
				poll_only |= irqp[i];
			}
		}
	}
	poll_only = !poll_only;

	Fip_firmware = firmware;
	Fip_firmware_size = firmsize;

	/* Announce our presence */
	printk( KERN_INFO "%s version %s\n", pcName, pcVersion );

	// ip2 can be unloaded and reloaded for no good reason
	// we can't let that happen here or bad things happen
	// second load hoses board but not system - fixme later
	if (loaded) {
		printk( KERN_INFO "Still loaded\n" );
		return 0;
	}
	loaded++;

	ip2_tty_driver = alloc_tty_driver(IP2_MAX_PORTS);
	if (!ip2_tty_driver)
		return -ENOMEM;

	/* Initialise the iiEllis subsystem. */
	iiEllisInit();

	/* Initialize arrays. */
	memset( i2BoardPtrTable, 0, sizeof i2BoardPtrTable );
	memset( DevTable, 0, sizeof DevTable );

	/* Initialise all the boards we can find (up to the maximum). */
	for ( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		switch ( ip2config.addr[i] ) { 
		case 0:	/* skip this slot even if card is present */
			break;
		default: /* ISA */
		   /* ISA address must be specified */
			if ( (ip2config.addr[i] < 0x100) || (ip2config.addr[i] > 0x3f8) ) {
				printk ( KERN_ERR "IP2: Bad ISA board %d address %x\n",
							 i, ip2config.addr[i] );
				ip2config.addr[i] = 0;
			} else {
				ip2config.type[i] = ISA;

				/* Check for valid irq argument, set for polling if invalid */
				if (ip2config.irq[i] && !is_valid_irq(ip2config.irq[i])) {
					printk(KERN_ERR "IP2: Bad IRQ(%d) specified\n",ip2config.irq[i]);
					ip2config.irq[i] = 0;// 0 is polling and is valid in that sense
				}
			}
			break;
		case PCI:
#ifdef CONFIG_PCI
			{
				pci_dev_i = pci_get_device(PCI_VENDOR_ID_COMPUTONE,
							  PCI_DEVICE_ID_COMPUTONE_IP2EX, pci_dev_i);
				if (pci_dev_i != NULL) {
					unsigned int addr;

					if (pci_enable_device(pci_dev_i)) {
						printk( KERN_ERR "IP2: can't enable PCI device at %s\n",
							pci_name(pci_dev_i));
						break;
					}
					ip2config.type[i] = PCI;
					ip2config.pci_dev[i] = pci_dev_get(pci_dev_i);
					status =
					pci_read_config_dword(pci_dev_i, PCI_BASE_ADDRESS_1, &addr);
					if ( addr & 1 ) {
						ip2config.addr[i]=(USHORT)(addr&0xfffe);
					} else {
						printk( KERN_ERR "IP2: PCI I/O address error\n");
					}

//		If the PCI BIOS assigned it, lets try and use it.  If we
//		can't acquire it or it screws up, deal with it then.

//					if (!is_valid_irq(pci_irq)) {
//						printk( KERN_ERR "IP2: Bad PCI BIOS IRQ(%d)\n",pci_irq);
//						pci_irq = 0;
//					}
					ip2config.irq[i] = pci_dev_i->irq;
				} else {	// ann error
					ip2config.addr[i] = 0;
					if (status == PCIBIOS_DEVICE_NOT_FOUND) {
						printk( KERN_ERR "IP2: PCI board %d not found\n", i );
					} else {
						printk( KERN_ERR "IP2: PCI error 0x%x \n", status );
					}
				} 
			}
#else
			printk( KERN_ERR "IP2: PCI card specified but PCI support not\n");
			printk( KERN_ERR "IP2: configured in this kernel.\n");
			printk( KERN_ERR "IP2: Recompile kernel with CONFIG_PCI defined!\n");
#endif /* CONFIG_PCI */
			break;
		case EISA:
			if ( (ip2config.addr[i] = find_eisa_board( Eisa_slot + 1 )) != 0) {
				/* Eisa_irq set as side effect, boo */
				ip2config.type[i] = EISA;
			} 
			ip2config.irq[i] = Eisa_irq;
			break;
		}	/* switch */
	}	/* for */
	if (pci_dev_i)
		pci_dev_put(pci_dev_i);

	for ( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		if ( ip2config.addr[i] ) {
			pB = kmalloc( sizeof(i2eBordStr), GFP_KERNEL);
			if ( pB != NULL ) {
				i2BoardPtrTable[i] = pB;
				memset( pB, 0, sizeof(i2eBordStr) );
				iiSetAddress( pB, ip2config.addr[i], ii2DelayTimer );
				iiReset( pB );
			} else {
				printk(KERN_ERR "IP2: board memory allocation error\n");
			}
		}
	}
	for ( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		if ( ( pB = i2BoardPtrTable[i] ) != NULL ) {
			iiResetDelay( pB );
			break;
		}
	}
	for ( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		if ( i2BoardPtrTable[i] != NULL ) {
			ip2_init_board( i );
		}
	}

	ip2trace (ITRC_NO_PORT, ITRC_INIT, 2, 0 );

	ip2_tty_driver->owner		    = THIS_MODULE;
	ip2_tty_driver->name                 = "ttyF";
	ip2_tty_driver->driver_name          = pcDriver_name;
	ip2_tty_driver->major                = IP2_TTY_MAJOR;
	ip2_tty_driver->minor_start          = 0;
	ip2_tty_driver->type                 = TTY_DRIVER_TYPE_SERIAL;
	ip2_tty_driver->subtype              = SERIAL_TYPE_NORMAL;
	ip2_tty_driver->init_termios         = tty_std_termios;
	ip2_tty_driver->init_termios.c_cflag = B9600|CS8|CREAD|HUPCL|CLOCAL;
	ip2_tty_driver->flags                = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(ip2_tty_driver, &ip2_ops);

	ip2trace (ITRC_NO_PORT, ITRC_INIT, 3, 0 );

	/* Register the tty devices. */
	if ( ( err = tty_register_driver ( ip2_tty_driver ) ) ) {
		printk(KERN_ERR "IP2: failed to register tty driver (%d)\n", err);
		put_tty_driver(ip2_tty_driver);
		return -EINVAL;
	} else
	/* Register the IPL driver. */
	if ( ( err = register_chrdev ( IP2_IPL_MAJOR, pcIpl, &ip2_ipl ) ) ) {
		printk(KERN_ERR "IP2: failed to register IPL device (%d)\n", err );
	} else {
		/* create the sysfs class */
		ip2_class = class_create(THIS_MODULE, "ip2");
		if (IS_ERR(ip2_class)) {
			err = PTR_ERR(ip2_class);
			goto out_chrdev;	
		}
	}
	/* Register the read_procmem thing */
	if (!create_proc_info_entry("ip2mem",0,&proc_root,ip2_read_procmem)) {
		printk(KERN_ERR "IP2: failed to register read_procmem\n");
	} else {

	ip2trace (ITRC_NO_PORT, ITRC_INIT, 4, 0 );
		/* Register the interrupt handler or poll handler, depending upon the
		 * specified interrupt.
		 */

		for( i = 0; i < IP2_MAX_BOARDS; ++i ) {
			if ( 0 == ip2config.addr[i] ) {
				continue;
			}

			if ( NULL != ( pB = i2BoardPtrTable[i] ) ) {
				class_device_create(ip2_class, NULL,
						MKDEV(IP2_IPL_MAJOR, 4 * i),
						NULL, "ipl%d", i);
				class_device_create(ip2_class, NULL,
						MKDEV(IP2_IPL_MAJOR, 4 * i + 1),
						NULL, "stat%d", i);

			    for ( box = 0; box < ABS_MAX_BOXES; ++box )
			    {
			        for ( j = 0; j < ABS_BIGGEST_BOX; ++j )
			        {
				    if ( pB->i2eChannelMap[box] & (1 << j) )
				    {
				        tty_register_device(ip2_tty_driver,
					    j + ABS_BIGGEST_BOX *
						    (box+i*ABS_MAX_BOXES), NULL);
			    	    }
			        }
			    }
			}

			if (poll_only) {
//		Poll only forces driver to only use polling and
//		to ignore the probed PCI or EISA interrupts.
				ip2config.irq[i] = CIR_POLL;
			}
			if ( ip2config.irq[i] == CIR_POLL ) {
retry:
				if (!TimerOn) {
					PollTimer.expires = POLL_TIMEOUT;
					add_timer ( &PollTimer );
					TimerOn = 1;
					printk( KERN_INFO "IP2: polling\n");
				}
			} else {
				if (have_requested_irq(ip2config.irq[i]))
					continue;
				rc = request_irq( ip2config.irq[i], ip2_interrupt,
					IP2_SA_FLAGS | (ip2config.type[i] == PCI ? IRQF_SHARED : 0),
					pcName, (void *)&pcName);
				if (rc) {
					printk(KERN_ERR "IP2: an request_irq failed: error %d\n",rc);
					ip2config.irq[i] = CIR_POLL;
					printk( KERN_INFO "IP2: Polling %ld/sec.\n",
							(POLL_TIMEOUT - jiffies));
					goto retry;
				} 
				mark_requested_irq(ip2config.irq[i]);
				/* Initialise the interrupt handler bottom half (aka slih). */
			}
		}
		for( i = 0; i < IP2_MAX_BOARDS; ++i ) {
			if ( i2BoardPtrTable[i] ) {
				set_irq( i, ip2config.irq[i] ); /* set and enable board interrupt */
			}
		}
	}
	ip2trace (ITRC_NO_PORT, ITRC_INIT, ITRC_RETURN, 0 );
	goto out;

out_chrdev:
	unregister_chrdev(IP2_IPL_MAJOR, "ip2");
out:
	return err;
}

EXPORT_SYMBOL(ip2_loadmain);

/******************************************************************************/
/* Function:   ip2_init_board()                                               */
/* Parameters: Index of board in configuration structure                      */
/* Returns:    Success (0)                                                    */
/*                                                                            */
/* Description:                                                               */
/* This function initializes the specified board. The loadware is copied to   */
/* the board, the channel structures are initialized, and the board details   */
/* are reported on the console.                                               */
/******************************************************************************/
static void
ip2_init_board( int boardnum )
{
	int i;
	int nports = 0, nboxes = 0;
	i2ChanStrPtr pCh;
	i2eBordStrPtr pB = i2BoardPtrTable[boardnum];

	if ( !iiInitialize ( pB ) ) {
		printk ( KERN_ERR "IP2: Failed to initialize board at 0x%x, error %d\n",
			 pB->i2eBase, pB->i2eError );
		goto err_initialize;
	}
	printk(KERN_INFO "IP2: Board %d: addr=0x%x irq=%d\n", boardnum + 1,
	       ip2config.addr[boardnum], ip2config.irq[boardnum] );

	if (!request_region( ip2config.addr[boardnum], 8, pcName )) {
		printk(KERN_ERR "IP2: bad addr=0x%x\n", ip2config.addr[boardnum]);
		goto err_initialize;
	}

	if ( iiDownloadAll ( pB, (loadHdrStrPtr)Fip_firmware, 1, Fip_firmware_size )
	    != II_DOWN_GOOD ) {
		printk ( KERN_ERR "IP2: failed to download loadware\n" );
		goto err_release_region;
	} else {
		printk ( KERN_INFO "IP2: fv=%d.%d.%d lv=%d.%d.%d\n",
			 pB->i2ePom.e.porVersion,
			 pB->i2ePom.e.porRevision,
			 pB->i2ePom.e.porSubRev, pB->i2eLVersion,
			 pB->i2eLRevision, pB->i2eLSub );
	}

	switch ( pB->i2ePom.e.porID & ~POR_ID_RESERVED ) {

	default:
		printk( KERN_ERR "IP2: Unknown board type, ID = %x\n",
				pB->i2ePom.e.porID );
		nports = 0;
		goto err_release_region;
		break;

	case POR_ID_II_4: /* IntelliPort-II, ISA-4 (4xRJ45) */
		printk ( KERN_INFO "IP2: ISA-4\n" );
		nports = 4;
		break;

	case POR_ID_II_8: /* IntelliPort-II, 8-port using standard brick. */
		printk ( KERN_INFO "IP2: ISA-8 std\n" );
		nports = 8;
		break;

	case POR_ID_II_8R: /* IntelliPort-II, 8-port using RJ11's (no CTS) */
		printk ( KERN_INFO "IP2: ISA-8 RJ11\n" );
		nports = 8;
		break;

	case POR_ID_FIIEX: /* IntelliPort IIEX */
	{
		int portnum = IP2_PORTS_PER_BOARD * boardnum;
		int            box;

		for( box = 0; box < ABS_MAX_BOXES; ++box ) {
			if ( pB->i2eChannelMap[box] != 0 ) {
				++nboxes;
			}
			for( i = 0; i < ABS_BIGGEST_BOX; ++i ) {
				if ( pB->i2eChannelMap[box] & 1<< i ) {
					++nports;
				}
			}
		}
		DevTableMem[boardnum] = pCh =
			kmalloc( sizeof(i2ChanStr) * nports, GFP_KERNEL );
		if ( !pCh ) {
			printk ( KERN_ERR "IP2: (i2_init_channel:) Out of memory.\n");
			goto err_release_region;
		}
		if ( !i2InitChannels( pB, nports, pCh ) ) {
			printk(KERN_ERR "IP2: i2InitChannels failed: %d\n",pB->i2eError);
			kfree ( pCh );
			goto err_release_region;
		}
		pB->i2eChannelPtr = &DevTable[portnum];
		pB->i2eChannelCnt = ABS_MOST_PORTS;

		for( box = 0; box < ABS_MAX_BOXES; ++box, portnum += ABS_BIGGEST_BOX ) {
			for( i = 0; i < ABS_BIGGEST_BOX; ++i ) {
				if ( pB->i2eChannelMap[box] & (1 << i) ) {
					DevTable[portnum + i] = pCh;
					pCh->port_index = portnum + i;
					pCh++;
				}
			}
		}
		printk(KERN_INFO "IP2: EX box=%d ports=%d %d bit\n",
			nboxes, nports, pB->i2eDataWidth16 ? 16 : 8 );
		}
		goto ex_exit;
	}
	DevTableMem[boardnum] = pCh =
		kmalloc ( sizeof (i2ChanStr) * nports, GFP_KERNEL );
	if ( !pCh ) {
		printk ( KERN_ERR "IP2: (i2_init_channel:) Out of memory.\n");
		goto err_release_region;
	}
	pB->i2eChannelPtr = pCh;
	pB->i2eChannelCnt = nports;
	if ( !i2InitChannels( pB, nports, pCh ) ) {
		printk(KERN_ERR "IP2: i2InitChannels failed: %d\n",pB->i2eError);
		kfree ( pCh );
		goto err_release_region;
	}
	pB->i2eChannelPtr = &DevTable[IP2_PORTS_PER_BOARD * boardnum];

	for( i = 0; i < pB->i2eChannelCnt; ++i ) {
		DevTable[IP2_PORTS_PER_BOARD * boardnum + i] = pCh;
		pCh->port_index = (IP2_PORTS_PER_BOARD * boardnum) + i;
		pCh++;
	}
ex_exit:
	INIT_WORK(&pB->tqueue_interrupt, ip2_interrupt_bh);
	return;

err_release_region:
	release_region(ip2config.addr[boardnum], 8);
err_initialize:
	kfree ( pB );
	i2BoardPtrTable[boardnum] = NULL;
	return;
}

/******************************************************************************/
/* Function:   find_eisa_board ( int start_slot )                             */
/* Parameters: First slot to check                                            */
/* Returns:    Address of EISA IntelliPort II controller                      */
/*                                                                            */
/* Description:                                                               */
/* This function searches for an EISA IntelliPort controller, starting        */
/* from the specified slot number. If the motherboard is not identified as an */
/* EISA motherboard, or no valid board ID is selected it returns 0. Otherwise */
/* it returns the base address of the controller.                             */
/******************************************************************************/
static unsigned short
find_eisa_board( int start_slot )
{
	int i, j;
	unsigned int idm = 0;
	unsigned int idp = 0;
	unsigned int base = 0;
	unsigned int value;
	int setup_address;
	int setup_irq;
	int ismine = 0;

	/*
	 * First a check for an EISA motherboard, which we do by comparing the
	 * EISA ID registers for the system board and the first couple of slots.
	 * No slot ID should match the system board ID, but on an ISA or PCI
	 * machine the odds are that an empty bus will return similar values for
	 * each slot.
	 */
	i = 0x0c80;
	value = (inb(i) << 24) + (inb(i+1) << 16) + (inb(i+2) << 8) + inb(i+3);
	for( i = 0x1c80; i <= 0x4c80; i += 0x1000 ) {
		j = (inb(i)<<24)+(inb(i+1)<<16)+(inb(i+2)<<8)+inb(i+3);
		if ( value == j )
			return 0;
	}

	/*
	 * OK, so we are inclined to believe that this is an EISA machine. Find
	 * an IntelliPort controller.
	 */
	for( i = start_slot; i < 16; i++ ) {
		base = i << 12;
		idm = (inb(base + 0xc80) << 8) | (inb(base + 0xc81) & 0xff);
		idp = (inb(base + 0xc82) << 8) | (inb(base + 0xc83) & 0xff);
		ismine = 0;
		if ( idm == 0x0e8e ) {
			if ( idp == 0x0281 || idp == 0x0218 ) {
				ismine = 1;
			} else if ( idp == 0x0282 || idp == 0x0283 ) {
				ismine = 3;	/* Can do edge-trigger */
			}
			if ( ismine ) {
				Eisa_slot = i;
				break;
			}
		}
	}
	if ( !ismine )
		return 0;

	/* It's some sort of EISA card, but at what address is it configured? */

	setup_address = base + 0xc88;
	value = inb(base + 0xc86);
	setup_irq = (value & 8) ? Valid_Irqs[value & 7] : 0;

	if ( (ismine & 2) && !(value & 0x10) ) {
		ismine = 1;	/* Could be edging, but not */
	}

	if ( Eisa_irq == 0 ) {
		Eisa_irq = setup_irq;
	} else if ( Eisa_irq != setup_irq ) {
		printk ( KERN_ERR "IP2: EISA irq mismatch between EISA controllers\n" );
	}

#ifdef IP2DEBUG_INIT
printk(KERN_DEBUG "Computone EISA board in slot %d, I.D. 0x%x%x, Address 0x%x",
	       base >> 12, idm, idp, setup_address);
	if ( Eisa_irq ) {
		printk(KERN_DEBUG ", Interrupt %d %s\n",
		       setup_irq, (ismine & 2) ? "(edge)" : "(level)");
	} else {
		printk(KERN_DEBUG ", (polled)\n");
	}
#endif
	return setup_address;
}

/******************************************************************************/
/* Function:   set_irq()                                                      */
/* Parameters: index to board in board table                                  */
/*             IRQ to use                                                     */
/* Returns:    Success (0)                                                    */
/*                                                                            */
/* Description:                                                               */
/******************************************************************************/
static void
set_irq( int boardnum, int boardIrq )
{
	unsigned char tempCommand[16];
	i2eBordStrPtr pB = i2BoardPtrTable[boardnum];
	unsigned long flags;

	/*
	 * Notify the boards they may generate interrupts. This is done by
	 * sending an in-line command to channel 0 on each board. This is why
	 * the channels have to be defined already. For each board, if the
	 * interrupt has never been defined, we must do so NOW, directly, since
	 * board will not send flow control or even give an interrupt until this
	 * is done.  If polling we must send 0 as the interrupt parameter.
	 */

	// We will get an interrupt here at the end of this function

	iiDisableMailIrq(pB);

	/* We build up the entire packet header. */
	CHANNEL_OF(tempCommand) = 0;
	PTYPE_OF(tempCommand) = PTYPE_INLINE;
	CMD_COUNT_OF(tempCommand) = 2;
	(CMD_OF(tempCommand))[0] = CMDVALUE_IRQ;
	(CMD_OF(tempCommand))[1] = boardIrq;
	/*
	 * Write to FIFO; don't bother to adjust fifo capacity for this, since
	 * board will respond almost immediately after SendMail hit.
	 */
	WRITE_LOCK_IRQSAVE(&pB->write_fifo_spinlock,flags);
	iiWriteBuf(pB, tempCommand, 4);
	WRITE_UNLOCK_IRQRESTORE(&pB->write_fifo_spinlock,flags);
	pB->i2eUsingIrq = boardIrq;
	pB->i2eOutMailWaiting |= MB_OUT_STUFFED;

	/* Need to update number of boards before you enable mailbox int */
	++i2nBoards;

	CHANNEL_OF(tempCommand) = 0;
	PTYPE_OF(tempCommand) = PTYPE_BYPASS;
	CMD_COUNT_OF(tempCommand) = 6;
	(CMD_OF(tempCommand))[0] = 88;	// SILO
	(CMD_OF(tempCommand))[1] = 64;	// chars
	(CMD_OF(tempCommand))[2] = 32;	// ms

	(CMD_OF(tempCommand))[3] = 28;	// MAX_BLOCK
	(CMD_OF(tempCommand))[4] = 64;	// chars

	(CMD_OF(tempCommand))[5] = 87;	// HW_TEST
	WRITE_LOCK_IRQSAVE(&pB->write_fifo_spinlock,flags);
	iiWriteBuf(pB, tempCommand, 8);
	WRITE_UNLOCK_IRQRESTORE(&pB->write_fifo_spinlock,flags);

	CHANNEL_OF(tempCommand) = 0;
	PTYPE_OF(tempCommand) = PTYPE_BYPASS;
	CMD_COUNT_OF(tempCommand) = 1;
	(CMD_OF(tempCommand))[0] = 84;	/* get BOX_IDS */
	iiWriteBuf(pB, tempCommand, 3);

#ifdef XXX
	// enable heartbeat for test porpoises
	CHANNEL_OF(tempCommand) = 0;
	PTYPE_OF(tempCommand) = PTYPE_BYPASS;
	CMD_COUNT_OF(tempCommand) = 2;
	(CMD_OF(tempCommand))[0] = 44;	/* get ping */
	(CMD_OF(tempCommand))[1] = 200;	/* 200 ms */
	WRITE_LOCK_IRQSAVE(&pB->write_fifo_spinlock,flags);
	iiWriteBuf(pB, tempCommand, 4);
	WRITE_UNLOCK_IRQRESTORE(&pB->write_fifo_spinlock,flags);
#endif

	iiEnableMailIrq(pB);
	iiSendPendingMail(pB);
}

/******************************************************************************/
/* Interrupt Handler Section                                                  */
/******************************************************************************/

static inline void
service_all_boards(void)
{
	int i;
	i2eBordStrPtr  pB;

	/* Service every board on the list */
	for( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		pB = i2BoardPtrTable[i];
		if ( pB ) {
			i2ServiceBoard( pB );
		}
	}
}


/******************************************************************************/
/* Function:   ip2_interrupt_bh(work)                                         */
/* Parameters: work - pointer to the board structure                          */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*	Service the board in a bottom half interrupt handler and then         */
/*	reenable the board's interrupts if it has an IRQ number               */
/*                                                                            */
/******************************************************************************/
static void
ip2_interrupt_bh(struct work_struct *work)
{
	i2eBordStrPtr pB = container_of(work, i2eBordStr, tqueue_interrupt);
//	pB better well be set or we have a problem!  We can only get
//	here from the IMMEDIATE queue.  Here, we process the boards.
//	Checking pB doesn't cost much and it saves us from the sanity checkers.

	bh_counter++; 

	if ( pB ) {
		i2ServiceBoard( pB );
		if( pB->i2eUsingIrq ) {
//			Re-enable his interrupts
			iiEnableMailIrq(pB);
		}
	}
}


/******************************************************************************/
/* Function:   ip2_interrupt(int irq, void *dev_id)    */
/* Parameters: irq - interrupt number                                         */
/*             pointer to optional device ID structure                        */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*	Our task here is simply to identify each board which needs servicing. */
/*	If we are queuing then, queue it to be serviced, and disable its irq  */
/*	mask otherwise process the board directly.                            */
/*                                                                            */
/*	We could queue by IRQ but that just complicates things on both ends   */
/*	with very little gain in performance (how many instructions does      */
/*	it take to iterate on the immediate queue).                           */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static irqreturn_t
ip2_interrupt(int irq, void *dev_id)
{
	int i;
	i2eBordStrPtr  pB;
	int handled = 0;

	ip2trace (ITRC_NO_PORT, ITRC_INTR, 99, 1, irq );

	/* Service just the boards on the list using this irq */
	for( i = 0; i < i2nBoards; ++i ) {
		pB = i2BoardPtrTable[i];

//		Only process those boards which match our IRQ.
//			IRQ = 0 for polled boards, we won't poll "IRQ" boards

		if ( pB && (pB->i2eUsingIrq == irq) ) {
			handled = 1;
#ifdef USE_IQI

		    if (NO_MAIL_HERE != ( pB->i2eStartMail = iiGetMail(pB))) {
//			Disable his interrupt (will be enabled when serviced)
//			This is mostly to protect from reentrancy.
			iiDisableMailIrq(pB);

//			Park the board on the immediate queue for processing.
			schedule_work(&pB->tqueue_interrupt);

//			Make sure the immediate queue is flagged to fire.
		    }
#else
//		We are using immediate servicing here.  This sucks and can
//		cause all sorts of havoc with ppp and others.  The failsafe
//		check on iiSendPendingMail could also throw a hairball.
			i2ServiceBoard( pB );
#endif /* USE_IQI */
		}
	}

	++irq_counter;

	ip2trace (ITRC_NO_PORT, ITRC_INTR, ITRC_RETURN, 0 );
	return IRQ_RETVAL(handled);
}

/******************************************************************************/
/* Function:   ip2_poll(unsigned long arg)                                    */
/* Parameters: ?                                                              */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/* This function calls the library routine i2ServiceBoard for each board in   */
/* the board table. This is used instead of the interrupt routine when polled */
/* mode is specified.                                                         */
/******************************************************************************/
static void
ip2_poll(unsigned long arg)
{
	ip2trace (ITRC_NO_PORT, ITRC_INTR, 100, 0 );

	TimerOn = 0; // it's the truth but not checked in service

	// Just polled boards, IRQ = 0 will hit all non-interrupt boards.
	// It will NOT poll boards handled by hard interrupts.
	// The issue of queued BH interrups is handled in ip2_interrupt().
	ip2_interrupt(0, NULL);

	PollTimer.expires = POLL_TIMEOUT;
	add_timer( &PollTimer );
	TimerOn = 1;

	ip2trace (ITRC_NO_PORT, ITRC_INTR, ITRC_RETURN, 0 );
}

static void do_input(struct work_struct *work)
{
	i2ChanStrPtr pCh = container_of(work, i2ChanStr, tqueue_input);
	unsigned long flags;

	ip2trace(CHANN, ITRC_INPUT, 21, 0 );

	// Data input
	if ( pCh->pTTY != NULL ) {
		READ_LOCK_IRQSAVE(&pCh->Ibuf_spinlock,flags)
		if (!pCh->throttled && (pCh->Ibuf_stuff != pCh->Ibuf_strip)) {
			READ_UNLOCK_IRQRESTORE(&pCh->Ibuf_spinlock,flags)
			i2Input( pCh );
		} else
			READ_UNLOCK_IRQRESTORE(&pCh->Ibuf_spinlock,flags)
	} else {
		ip2trace(CHANN, ITRC_INPUT, 22, 0 );

		i2InputFlush( pCh );
	}
}

// code duplicated from n_tty (ldisc)
static inline void  isig(int sig, struct tty_struct *tty, int flush)
{
	if (tty->pgrp)
		kill_pgrp(tty->pgrp, sig, 1);
	if (flush || !L_NOFLSH(tty)) {
		if ( tty->ldisc.flush_buffer )  
			tty->ldisc.flush_buffer(tty);
		i2InputFlush( tty->driver_data );
	}
}

static void do_status(struct work_struct *work)
{
	i2ChanStrPtr pCh = container_of(work, i2ChanStr, tqueue_status);
	int status;

	status =  i2GetStatus( pCh, (I2_BRK|I2_PAR|I2_FRA|I2_OVR) );

	ip2trace (CHANN, ITRC_STATUS, 21, 1, status );

	if (pCh->pTTY && (status & (I2_BRK|I2_PAR|I2_FRA|I2_OVR)) ) {
		if ( (status & I2_BRK) ) {
			// code duplicated from n_tty (ldisc)
			if (I_IGNBRK(pCh->pTTY))
				goto skip_this;
			if (I_BRKINT(pCh->pTTY)) {
				isig(SIGINT, pCh->pTTY, 1);
				goto skip_this;
			}
			wake_up_interruptible(&pCh->pTTY->read_wait);
		}
#ifdef NEVER_HAPPENS_AS_SETUP_XXX
	// and can't work because we don't know the_char
	// as the_char is reported on a separate path
	// The intelligent board does this stuff as setup
	{
	char brkf = TTY_NORMAL;
	unsigned char brkc = '\0';
	unsigned char tmp;
		if ( (status & I2_BRK) ) {
			brkf = TTY_BREAK;
			brkc = '\0';
		} 
		else if (status & I2_PAR) {
			brkf = TTY_PARITY;
			brkc = the_char;
		} else if (status & I2_FRA) {
			brkf = TTY_FRAME;
			brkc = the_char;
		} else if (status & I2_OVR) {
			brkf = TTY_OVERRUN;
			brkc = the_char;
		}
		tmp = pCh->pTTY->real_raw;
		pCh->pTTY->real_raw = 0;
		pCh->pTTY->ldisc.receive_buf( pCh->pTTY, &brkc, &brkf, 1 );
		pCh->pTTY->real_raw = tmp;
	}
#endif /* NEVER_HAPPENS_AS_SETUP_XXX */
	}
skip_this:

	if ( status & (I2_DDCD | I2_DDSR | I2_DCTS | I2_DRI) ) {
		wake_up_interruptible(&pCh->delta_msr_wait);

		if ( (pCh->flags & ASYNC_CHECK_CD) && (status & I2_DDCD) ) {
			if ( status & I2_DCD ) {
				if ( pCh->wopen ) {
					wake_up_interruptible ( &pCh->open_wait );
				}
			} else {
				if (pCh->pTTY &&  (!(pCh->pTTY->termios->c_cflag & CLOCAL)) ) {
					tty_hangup( pCh->pTTY );
				}
			}
		}
	}

	ip2trace (CHANN, ITRC_STATUS, 26, 0 );
}

/******************************************************************************/
/* Device Open/Close/Ioctl Entry Point Section                                */
/******************************************************************************/

/******************************************************************************/
/* Function:   open_sanity_check()                                            */
/* Parameters: Pointer to tty structure                                       */
/*             Pointer to file structure                                      */
/* Returns:    Success or failure                                             */
/*                                                                            */
/* Description:                                                               */
/* Verifies the structure magic numbers and cross links.                      */
/******************************************************************************/
#ifdef IP2DEBUG_OPEN
static void 
open_sanity_check( i2ChanStrPtr pCh, i2eBordStrPtr pBrd )
{
	if ( pBrd->i2eValid != I2E_MAGIC ) {
		printk(KERN_ERR "IP2: invalid board structure\n" );
	} else if ( pBrd != pCh->pMyBord ) {
		printk(KERN_ERR "IP2: board structure pointer mismatch (%p)\n",
			 pCh->pMyBord );
	} else if ( pBrd->i2eChannelCnt < pCh->port_index ) {
		printk(KERN_ERR "IP2: bad device index (%d)\n", pCh->port_index );
	} else if (&((i2ChanStrPtr)pBrd->i2eChannelPtr)[pCh->port_index] != pCh) {
	} else {
		printk(KERN_INFO "IP2: all pointers check out!\n" );
	}
}
#endif


/******************************************************************************/
/* Function:   ip2_open()                                                     */
/* Parameters: Pointer to tty structure                                       */
/*             Pointer to file structure                                      */
/* Returns:    Success or failure                                             */
/*                                                                            */
/* Description: (MANDATORY)                                                   */
/* A successful device open has to run a gauntlet of checks before it         */
/* completes. After some sanity checking and pointer setup, the function      */
/* blocks until all conditions are satisfied. It then initialises the port to */
/* the default characteristics and returns.                                   */
/******************************************************************************/
static int
ip2_open( PTTY tty, struct file *pFile )
{
	wait_queue_t wait;
	int rc = 0;
	int do_clocal = 0;
	i2ChanStrPtr  pCh = DevTable[tty->index];

	ip2trace (tty->index, ITRC_OPEN, ITRC_ENTER, 0 );

	if ( pCh == NULL ) {
		return -ENODEV;
	}
	/* Setup pointer links in device and tty structures */
	pCh->pTTY = tty;
	tty->driver_data = pCh;

#ifdef IP2DEBUG_OPEN
	printk(KERN_DEBUG \
			"IP2:open(tty=%p,pFile=%p):dev=%s,ch=%d,idx=%d\n",
	       tty, pFile, tty->name, pCh->infl.hd.i2sChannel, pCh->port_index);
	open_sanity_check ( pCh, pCh->pMyBord );
#endif

	i2QueueCommands(PTYPE_INLINE, pCh, 100, 3, CMD_DTRUP,CMD_RTSUP,CMD_DCD_REP);
	pCh->dataSetOut |= (I2_DTR | I2_RTS);
	serviceOutgoingFifo( pCh->pMyBord );

	/* Block here until the port is ready (per serial and istallion) */
	/*
	 * 1. If the port is in the middle of closing wait for the completion
	 *    and then return the appropriate error.
	 */
	init_waitqueue_entry(&wait, current);
	add_wait_queue(&pCh->close_wait, &wait);
	set_current_state( TASK_INTERRUPTIBLE );

	if ( tty_hung_up_p(pFile) || ( pCh->flags & ASYNC_CLOSING )) {
		if ( pCh->flags & ASYNC_CLOSING ) {
			schedule();
		}
		if ( tty_hung_up_p(pFile) ) {
			set_current_state( TASK_RUNNING );
			remove_wait_queue(&pCh->close_wait, &wait);
			return( pCh->flags & ASYNC_HUP_NOTIFY ) ? -EAGAIN : -ERESTARTSYS;
		}
	}
	set_current_state( TASK_RUNNING );
	remove_wait_queue(&pCh->close_wait, &wait);

	/*
	 * 3. Handle a non-blocking open of a normal port.
	 */
	if ( (pFile->f_flags & O_NONBLOCK) || (tty->flags & (1<<TTY_IO_ERROR) )) {
		pCh->flags |= ASYNC_NORMAL_ACTIVE;
		goto noblock;
	}
	/*
	 * 4. Now loop waiting for the port to be free and carrier present
	 *    (if required).
	 */
	if ( tty->termios->c_cflag & CLOCAL )
		do_clocal = 1;

#ifdef IP2DEBUG_OPEN
	printk(KERN_DEBUG "OpenBlock: do_clocal = %d\n", do_clocal);
#endif

	++pCh->wopen;

	init_waitqueue_entry(&wait, current);
	add_wait_queue(&pCh->open_wait, &wait);

	for(;;) {
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 2, CMD_DTRUP, CMD_RTSUP);
		pCh->dataSetOut |= (I2_DTR | I2_RTS);
		set_current_state( TASK_INTERRUPTIBLE );
		serviceOutgoingFifo( pCh->pMyBord );
		if ( tty_hung_up_p(pFile) ) {
			set_current_state( TASK_RUNNING );
			remove_wait_queue(&pCh->open_wait, &wait);
			return ( pCh->flags & ASYNC_HUP_NOTIFY ) ? -EBUSY : -ERESTARTSYS;
		}
		if (!(pCh->flags & ASYNC_CLOSING) && 
				(do_clocal || (pCh->dataSetIn & I2_DCD) )) {
			rc = 0;
			break;
		}

#ifdef IP2DEBUG_OPEN
		printk(KERN_DEBUG "ASYNC_CLOSING = %s\n",
			(pCh->flags & ASYNC_CLOSING)?"True":"False");
		printk(KERN_DEBUG "OpenBlock: waiting for CD or signal\n");
#endif
		ip2trace (CHANN, ITRC_OPEN, 3, 2, 0,
				(pCh->flags & ASYNC_CLOSING) );
		/* check for signal */
		if (signal_pending(current)) {
			rc = (( pCh->flags & ASYNC_HUP_NOTIFY ) ? -EAGAIN : -ERESTARTSYS);
			break;
		}
		schedule();
	}
	set_current_state( TASK_RUNNING );
	remove_wait_queue(&pCh->open_wait, &wait);

	--pCh->wopen; //why count?

	ip2trace (CHANN, ITRC_OPEN, 4, 0 );

	if (rc != 0 ) {
		return rc;
	}
	pCh->flags |= ASYNC_NORMAL_ACTIVE;

noblock:

	/* first open - Assign termios structure to port */
	if ( tty->count == 1 ) {
		i2QueueCommands(PTYPE_INLINE, pCh, 0, 2, CMD_CTSFL_DSAB, CMD_RTSFL_DSAB);
		/* Now we must send the termios settings to the loadware */
		set_params( pCh, NULL );
	}

	/*
	 * Now set any i2lib options. These may go away if the i2lib code ends
	 * up rolled into the mainline.
	 */
	pCh->channelOptions |= CO_NBLOCK_WRITE;

#ifdef IP2DEBUG_OPEN
	printk (KERN_DEBUG "IP2: open completed\n" );
#endif
	serviceOutgoingFifo( pCh->pMyBord );

	ip2trace (CHANN, ITRC_OPEN, ITRC_RETURN, 0 );

	return 0;
}

/******************************************************************************/
/* Function:   ip2_close()                                                    */
/* Parameters: Pointer to tty structure                                       */
/*             Pointer to file structure                                      */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static void
ip2_close( PTTY tty, struct file *pFile )
{
	i2ChanStrPtr  pCh = tty->driver_data;

	if ( !pCh ) {
		return;
	}

	ip2trace (CHANN, ITRC_CLOSE, ITRC_ENTER, 0 );

#ifdef IP2DEBUG_OPEN
	printk(KERN_DEBUG "IP2:close %s:\n",tty->name);
#endif

	if ( tty_hung_up_p ( pFile ) ) {

		ip2trace (CHANN, ITRC_CLOSE, 2, 1, 2 );

		return;
	}
	if ( tty->count > 1 ) { /* not the last close */

		ip2trace (CHANN, ITRC_CLOSE, 2, 1, 3 );

		return;
	}
	pCh->flags |= ASYNC_CLOSING;	// last close actually

	tty->closing = 1;

	if (pCh->ClosingWaitTime != ASYNC_CLOSING_WAIT_NONE) {
		/*
		 * Before we drop DTR, make sure the transmitter has completely drained.
		 * This uses an timeout, after which the close
		 * completes.
		 */
		ip2_wait_until_sent(tty, pCh->ClosingWaitTime );
	}
	/*
	 * At this point we stop accepting input. Here we flush the channel
	 * input buffer which will allow the board to send up more data. Any
	 * additional input is tossed at interrupt/poll time.
	 */
	i2InputFlush( pCh );

	/* disable DSS reporting */
	i2QueueCommands(PTYPE_INLINE, pCh, 100, 4,
				CMD_DCD_NREP, CMD_CTS_NREP, CMD_DSR_NREP, CMD_RI_NREP);
	if ( !tty || (tty->termios->c_cflag & HUPCL) ) {
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 2, CMD_RTSDN, CMD_DTRDN);
		pCh->dataSetOut &= ~(I2_DTR | I2_RTS);
		i2QueueCommands( PTYPE_INLINE, pCh, 100, 1, CMD_PAUSE(25));
	}

	serviceOutgoingFifo ( pCh->pMyBord );

	if ( tty->driver->flush_buffer ) 
		tty->driver->flush_buffer(tty);
	if ( tty->ldisc.flush_buffer )  
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	
	pCh->pTTY = NULL;

	if (pCh->wopen) {
		if (pCh->ClosingDelay) {
			msleep_interruptible(jiffies_to_msecs(pCh->ClosingDelay));
		}
		wake_up_interruptible(&pCh->open_wait);
	}

	pCh->flags &=~(ASYNC_NORMAL_ACTIVE|ASYNC_CLOSING);
	wake_up_interruptible(&pCh->close_wait);

#ifdef IP2DEBUG_OPEN
	DBG_CNT("ip2_close: after wakeups--");
#endif


	ip2trace (CHANN, ITRC_CLOSE, ITRC_RETURN, 1, 1 );

	return;
}

/******************************************************************************/
/* Function:   ip2_hangup()                                                   */
/* Parameters: Pointer to tty structure                                       */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static void
ip2_hangup ( PTTY tty )
{
	i2ChanStrPtr  pCh = tty->driver_data;

	if( !pCh ) {
		return;
	}

	ip2trace (CHANN, ITRC_HANGUP, ITRC_ENTER, 0 );

	ip2_flush_buffer(tty);

	/* disable DSS reporting */

	i2QueueCommands(PTYPE_BYPASS, pCh, 0, 1, CMD_DCD_NREP);
	i2QueueCommands(PTYPE_INLINE, pCh, 0, 2, CMD_CTSFL_DSAB, CMD_RTSFL_DSAB);
	if ( (tty->termios->c_cflag & HUPCL) ) {
		i2QueueCommands(PTYPE_BYPASS, pCh, 0, 2, CMD_RTSDN, CMD_DTRDN);
		pCh->dataSetOut &= ~(I2_DTR | I2_RTS);
		i2QueueCommands( PTYPE_INLINE, pCh, 100, 1, CMD_PAUSE(25));
	}
	i2QueueCommands(PTYPE_INLINE, pCh, 1, 3, 
				CMD_CTS_NREP, CMD_DSR_NREP, CMD_RI_NREP);
	serviceOutgoingFifo ( pCh->pMyBord );

	wake_up_interruptible ( &pCh->delta_msr_wait );

	pCh->flags &= ~ASYNC_NORMAL_ACTIVE;
	pCh->pTTY = NULL;
	wake_up_interruptible ( &pCh->open_wait );

	ip2trace (CHANN, ITRC_HANGUP, ITRC_RETURN, 0 );
}

/******************************************************************************/
/******************************************************************************/
/* Device Output Section                                                      */
/******************************************************************************/
/******************************************************************************/

/******************************************************************************/
/* Function:   ip2_write()                                                    */
/* Parameters: Pointer to tty structure                                       */
/*             Flag denoting data is in user (1) or kernel (0) space          */
/*             Pointer to data                                                */
/*             Number of bytes to write                                       */
/* Returns:    Number of bytes actually written                               */
/*                                                                            */
/* Description: (MANDATORY)                                                   */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static int
ip2_write( PTTY tty, const unsigned char *pData, int count)
{
	i2ChanStrPtr  pCh = tty->driver_data;
	int bytesSent = 0;
	unsigned long flags;

	ip2trace (CHANN, ITRC_WRITE, ITRC_ENTER, 2, count, -1 );

	/* Flush out any buffered data left over from ip2_putchar() calls. */
	ip2_flush_chars( tty );

	/* This is the actual move bit. Make sure it does what we need!!!!! */
	WRITE_LOCK_IRQSAVE(&pCh->Pbuf_spinlock,flags);
	bytesSent = i2Output( pCh, pData, count);
	WRITE_UNLOCK_IRQRESTORE(&pCh->Pbuf_spinlock,flags);

	ip2trace (CHANN, ITRC_WRITE, ITRC_RETURN, 1, bytesSent );

	return bytesSent > 0 ? bytesSent : 0;
}

/******************************************************************************/
/* Function:   ip2_putchar()                                                  */
/* Parameters: Pointer to tty structure                                       */
/*             Character to write                                             */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static void
ip2_putchar( PTTY tty, unsigned char ch )
{
	i2ChanStrPtr  pCh = tty->driver_data;
	unsigned long flags;

//	ip2trace (CHANN, ITRC_PUTC, ITRC_ENTER, 1, ch );

	WRITE_LOCK_IRQSAVE(&pCh->Pbuf_spinlock,flags);
	pCh->Pbuf[pCh->Pbuf_stuff++] = ch;
	if ( pCh->Pbuf_stuff == sizeof pCh->Pbuf ) {
		WRITE_UNLOCK_IRQRESTORE(&pCh->Pbuf_spinlock,flags);
		ip2_flush_chars( tty );
	} else
		WRITE_UNLOCK_IRQRESTORE(&pCh->Pbuf_spinlock,flags);

//	ip2trace (CHANN, ITRC_PUTC, ITRC_RETURN, 1, ch );
}

/******************************************************************************/
/* Function:   ip2_flush_chars()                                              */
/* Parameters: Pointer to tty structure                                       */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/******************************************************************************/
static void
ip2_flush_chars( PTTY tty )
{
	int   strip;
	i2ChanStrPtr  pCh = tty->driver_data;
	unsigned long flags;

	WRITE_LOCK_IRQSAVE(&pCh->Pbuf_spinlock,flags);
	if ( pCh->Pbuf_stuff ) {

//		ip2trace (CHANN, ITRC_PUTC, 10, 1, strip );

		//
		// We may need to restart i2Output if it does not fullfill this request
		//
		strip = i2Output( pCh, pCh->Pbuf, pCh->Pbuf_stuff);
		if ( strip != pCh->Pbuf_stuff ) {
			memmove( pCh->Pbuf, &pCh->Pbuf[strip], pCh->Pbuf_stuff - strip );
		}
		pCh->Pbuf_stuff -= strip;
	}
	WRITE_UNLOCK_IRQRESTORE(&pCh->Pbuf_spinlock,flags);
}

/******************************************************************************/
/* Function:   ip2_write_room()                                               */
/* Parameters: Pointer to tty structure                                       */
/* Returns:    Number of bytes that the driver can accept                     */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/******************************************************************************/
static int
ip2_write_room ( PTTY tty )
{
	int bytesFree;
	i2ChanStrPtr  pCh = tty->driver_data;
	unsigned long flags;

	READ_LOCK_IRQSAVE(&pCh->Pbuf_spinlock,flags);
	bytesFree = i2OutputFree( pCh ) - pCh->Pbuf_stuff;
	READ_UNLOCK_IRQRESTORE(&pCh->Pbuf_spinlock,flags);

	ip2trace (CHANN, ITRC_WRITE, 11, 1, bytesFree );

	return ((bytesFree > 0) ? bytesFree : 0);
}

/******************************************************************************/
/* Function:   ip2_chars_in_buf()                                             */
/* Parameters: Pointer to tty structure                                       */
/* Returns:    Number of bytes queued for transmission                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static int
ip2_chars_in_buf ( PTTY tty )
{
	i2ChanStrPtr  pCh = tty->driver_data;
	int rc;
	unsigned long flags;

	ip2trace (CHANN, ITRC_WRITE, 12, 1, pCh->Obuf_char_count + pCh->Pbuf_stuff );

#ifdef IP2DEBUG_WRITE
	printk (KERN_DEBUG "IP2: chars in buffer = %d (%d,%d)\n",
				 pCh->Obuf_char_count + pCh->Pbuf_stuff,
				 pCh->Obuf_char_count, pCh->Pbuf_stuff );
#endif
	READ_LOCK_IRQSAVE(&pCh->Obuf_spinlock,flags);
	rc =  pCh->Obuf_char_count;
	READ_UNLOCK_IRQRESTORE(&pCh->Obuf_spinlock,flags);
	READ_LOCK_IRQSAVE(&pCh->Pbuf_spinlock,flags);
	rc +=  pCh->Pbuf_stuff;
	READ_UNLOCK_IRQRESTORE(&pCh->Pbuf_spinlock,flags);
	return rc;
}

/******************************************************************************/
/* Function:   ip2_flush_buffer()                                             */
/* Parameters: Pointer to tty structure                                       */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static void
ip2_flush_buffer( PTTY tty )
{
	i2ChanStrPtr  pCh = tty->driver_data;
	unsigned long flags;

	ip2trace (CHANN, ITRC_FLUSH, ITRC_ENTER, 0 );

#ifdef IP2DEBUG_WRITE
	printk (KERN_DEBUG "IP2: flush buffer\n" );
#endif
	WRITE_LOCK_IRQSAVE(&pCh->Pbuf_spinlock,flags);
	pCh->Pbuf_stuff = 0;
	WRITE_UNLOCK_IRQRESTORE(&pCh->Pbuf_spinlock,flags);
	i2FlushOutput( pCh );
	ip2_owake(tty);

	ip2trace (CHANN, ITRC_FLUSH, ITRC_RETURN, 0 );

}

/******************************************************************************/
/* Function:   ip2_wait_until_sent()                                          */
/* Parameters: Pointer to tty structure                                       */
/*             Timeout for wait.                                              */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/* This function is used in place of the normal tty_wait_until_sent, which    */
/* only waits for the driver buffers to be empty (or rather, those buffers    */
/* reported by chars_in_buffer) which doesn't work for IP2 due to the         */
/* indeterminate number of bytes buffered on the board.                       */
/******************************************************************************/
static void
ip2_wait_until_sent ( PTTY tty, int timeout )
{
	int i = jiffies;
	i2ChanStrPtr  pCh = tty->driver_data;

	tty_wait_until_sent(tty, timeout );
	if ( (i = timeout - (jiffies -i)) > 0)
		i2DrainOutput( pCh, i );
}

/******************************************************************************/
/******************************************************************************/
/* Device Input Section                                                       */
/******************************************************************************/
/******************************************************************************/

/******************************************************************************/
/* Function:   ip2_throttle()                                                 */
/* Parameters: Pointer to tty structure                                       */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static void
ip2_throttle ( PTTY tty )
{
	i2ChanStrPtr  pCh = tty->driver_data;

#ifdef IP2DEBUG_READ
	printk (KERN_DEBUG "IP2: throttle\n" );
#endif
	/*
	 * Signal the poll/interrupt handlers not to forward incoming data to
	 * the line discipline. This will cause the buffers to fill up in the
	 * library and thus cause the library routines to send the flow control
	 * stuff.
	 */
	pCh->throttled = 1;
}

/******************************************************************************/
/* Function:   ip2_unthrottle()                                               */
/* Parameters: Pointer to tty structure                                       */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static void
ip2_unthrottle ( PTTY tty )
{
	i2ChanStrPtr  pCh = tty->driver_data;
	unsigned long flags;

#ifdef IP2DEBUG_READ
	printk (KERN_DEBUG "IP2: unthrottle\n" );
#endif

	/* Pass incoming data up to the line discipline again. */
	pCh->throttled = 0;
 	i2QueueCommands(PTYPE_BYPASS, pCh, 0, 1, CMD_RESUME);
	serviceOutgoingFifo( pCh->pMyBord );
	READ_LOCK_IRQSAVE(&pCh->Ibuf_spinlock,flags)
	if ( pCh->Ibuf_stuff != pCh->Ibuf_strip ) {
		READ_UNLOCK_IRQRESTORE(&pCh->Ibuf_spinlock,flags)
#ifdef IP2DEBUG_READ
		printk (KERN_DEBUG "i2Input called from unthrottle\n" );
#endif
		i2Input( pCh );
	} else
		READ_UNLOCK_IRQRESTORE(&pCh->Ibuf_spinlock,flags)
}

static void
ip2_start ( PTTY tty )
{
 	i2ChanStrPtr  pCh = DevTable[tty->index];

 	i2QueueCommands(PTYPE_BYPASS, pCh, 0, 1, CMD_RESUME);
 	i2QueueCommands(PTYPE_BYPASS, pCh, 100, 1, CMD_UNSUSPEND);
 	i2QueueCommands(PTYPE_BYPASS, pCh, 100, 1, CMD_RESUME);
#ifdef IP2DEBUG_WRITE
	printk (KERN_DEBUG "IP2: start tx\n" );
#endif
}

static void
ip2_stop ( PTTY tty )
{
 	i2ChanStrPtr  pCh = DevTable[tty->index];

 	i2QueueCommands(PTYPE_BYPASS, pCh, 100, 1, CMD_SUSPEND);
#ifdef IP2DEBUG_WRITE
	printk (KERN_DEBUG "IP2: stop tx\n" );
#endif
}

/******************************************************************************/
/* Device Ioctl Section                                                       */
/******************************************************************************/

static int ip2_tiocmget(struct tty_struct *tty, struct file *file)
{
	i2ChanStrPtr pCh = DevTable[tty->index];
#ifdef	ENABLE_DSSNOW
	wait_queue_t wait;
#endif

	if (pCh == NULL)
		return -ENODEV;

/*
	FIXME - the following code is causing a NULL pointer dereference in
	2.3.51 in an interrupt handler.  It's suppose to prompt the board
	to return the DSS signal status immediately.  Why doesn't it do
	the same thing in 2.2.14?
*/

/*	This thing is still busted in the 1.2.12 driver on 2.4.x
	and even hoses the serial console so the oops can be trapped.
		/\/\|=mhw=|\/\/			*/

#ifdef	ENABLE_DSSNOW
	i2QueueCommands(PTYPE_BYPASS, pCh, 100, 1, CMD_DSS_NOW);

	init_waitqueue_entry(&wait, current);
	add_wait_queue(&pCh->dss_now_wait, &wait);
	set_current_state( TASK_INTERRUPTIBLE );

	serviceOutgoingFifo( pCh->pMyBord );

	schedule();

	set_current_state( TASK_RUNNING );
	remove_wait_queue(&pCh->dss_now_wait, &wait);

	if (signal_pending(current)) {
		return -EINTR;
	}
#endif
	return  ((pCh->dataSetOut & I2_RTS) ? TIOCM_RTS : 0)
	      | ((pCh->dataSetOut & I2_DTR) ? TIOCM_DTR : 0)
	      | ((pCh->dataSetIn  & I2_DCD) ? TIOCM_CAR : 0)
	      | ((pCh->dataSetIn  & I2_RI)  ? TIOCM_RNG : 0)
	      | ((pCh->dataSetIn  & I2_DSR) ? TIOCM_DSR : 0)
	      | ((pCh->dataSetIn  & I2_CTS) ? TIOCM_CTS : 0);
}

static int ip2_tiocmset(struct tty_struct *tty, struct file *file,
			unsigned int set, unsigned int clear)
{
	i2ChanStrPtr pCh = DevTable[tty->index];

	if (pCh == NULL)
		return -ENODEV;

	if (set & TIOCM_RTS) {
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_RTSUP);
		pCh->dataSetOut |= I2_RTS;
	}
	if (set & TIOCM_DTR) {
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_DTRUP);
		pCh->dataSetOut |= I2_DTR;
	}

	if (clear & TIOCM_RTS) {
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_RTSDN);
		pCh->dataSetOut &= ~I2_RTS;
	}
	if (clear & TIOCM_DTR) {
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_DTRDN);
		pCh->dataSetOut &= ~I2_DTR;
	}
	serviceOutgoingFifo( pCh->pMyBord );
	return 0;
}

/******************************************************************************/
/* Function:   ip2_ioctl()                                                    */
/* Parameters: Pointer to tty structure                                       */
/*             Pointer to file structure                                      */
/*             Command                                                        */
/*             Argument                                                       */
/* Returns:    Success or failure                                             */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static int
ip2_ioctl ( PTTY tty, struct file *pFile, UINT cmd, ULONG arg )
{
	wait_queue_t wait;
	i2ChanStrPtr pCh = DevTable[tty->index];
	i2eBordStrPtr pB;
	struct async_icount cprev, cnow;	/* kernel counter temps */
	struct serial_icounter_struct __user *p_cuser;
	int rc = 0;
	unsigned long flags;
	void __user *argp = (void __user *)arg;

	if ( pCh == NULL )
		return -ENODEV;

	pB = pCh->pMyBord;

	ip2trace (CHANN, ITRC_IOCTL, ITRC_ENTER, 2, cmd, arg );

#ifdef IP2DEBUG_IOCTL
	printk(KERN_DEBUG "IP2: ioctl cmd (%x), arg (%lx)\n", cmd, arg );
#endif

	switch(cmd) {
	case TIOCGSERIAL:

		ip2trace (CHANN, ITRC_IOCTL, 2, 1, rc );

		rc = get_serial_info(pCh, argp);
		if (rc)
			return rc;
		break;

	case TIOCSSERIAL:

		ip2trace (CHANN, ITRC_IOCTL, 3, 1, rc );

		rc = set_serial_info(pCh, argp);
		if (rc)
			return rc;
		break;

	case TCXONC:
		rc = tty_check_change(tty);
		if (rc)
			return rc;
		switch (arg) {
		case TCOOFF:
			//return  -ENOIOCTLCMD;
			break;
		case TCOON:
			//return  -ENOIOCTLCMD;
			break;
		case TCIOFF:
			if (STOP_CHAR(tty) != __DISABLED_CHAR) {
				i2QueueCommands( PTYPE_BYPASS, pCh, 100, 1,
						CMD_XMIT_NOW(STOP_CHAR(tty)));
			}
			break;
		case TCION:
			if (START_CHAR(tty) != __DISABLED_CHAR) {
				i2QueueCommands( PTYPE_BYPASS, pCh, 100, 1,
						CMD_XMIT_NOW(START_CHAR(tty)));
			}
			break;
		default:
			return -EINVAL;
		}
		return 0;

	case TCSBRK:   /* SVID version: non-zero arg --> no break */
		rc = tty_check_change(tty);

		ip2trace (CHANN, ITRC_IOCTL, 4, 1, rc );

		if (!rc) {
			ip2_wait_until_sent(tty,0);
			if (!arg) {
				i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_SEND_BRK(250));
				serviceOutgoingFifo( pCh->pMyBord );
			}
		}
		break;

	case TCSBRKP:  /* support for POSIX tcsendbreak() */
		rc = tty_check_change(tty);

		ip2trace (CHANN, ITRC_IOCTL, 5, 1, rc );

		if (!rc) {
			ip2_wait_until_sent(tty,0);
			i2QueueCommands(PTYPE_INLINE, pCh, 100, 1,
				CMD_SEND_BRK(arg ? arg*100 : 250));
			serviceOutgoingFifo ( pCh->pMyBord );	
		}
		break;

	case TIOCGSOFTCAR:

		ip2trace (CHANN, ITRC_IOCTL, 6, 1, rc );

			rc = put_user(C_CLOCAL(tty) ? 1 : 0, (unsigned long __user *)argp);
		if (rc)	
			return rc;
	break;

	case TIOCSSOFTCAR:

		ip2trace (CHANN, ITRC_IOCTL, 7, 1, rc );

		rc = get_user(arg,(unsigned long __user *) argp);
		if (rc) 
			return rc;
		tty->termios->c_cflag = ((tty->termios->c_cflag & ~CLOCAL)
					 | (arg ? CLOCAL : 0));
		
		break;

	/*
	 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change - mask
	 * passed in arg for lines of interest (use |'ed TIOCM_RNG/DSR/CD/CTS
	 * for masking). Caller should use TIOCGICOUNT to see which one it was
	 */
	case TIOCMIWAIT:
		WRITE_LOCK_IRQSAVE(&pB->read_fifo_spinlock, flags);
		cprev = pCh->icount;	 /* note the counters on entry */
		WRITE_UNLOCK_IRQRESTORE(&pB->read_fifo_spinlock, flags);
		i2QueueCommands(PTYPE_BYPASS, pCh, 100, 4, 
						CMD_DCD_REP, CMD_CTS_REP, CMD_DSR_REP, CMD_RI_REP);
		init_waitqueue_entry(&wait, current);
		add_wait_queue(&pCh->delta_msr_wait, &wait);
		set_current_state( TASK_INTERRUPTIBLE );

		serviceOutgoingFifo( pCh->pMyBord );
		for(;;) {
			ip2trace (CHANN, ITRC_IOCTL, 10, 0 );

			schedule();

			ip2trace (CHANN, ITRC_IOCTL, 11, 0 );

			/* see if a signal did it */
			if (signal_pending(current)) {
				rc = -ERESTARTSYS;
				break;
			}
			WRITE_LOCK_IRQSAVE(&pB->read_fifo_spinlock, flags);
			cnow = pCh->icount; /* atomic copy */
			WRITE_UNLOCK_IRQRESTORE(&pB->read_fifo_spinlock, flags);
			if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
				cnow.dcd == cprev.dcd && cnow.cts == cprev.cts) {
				rc =  -EIO; /* no change => rc */
				break;
			}
			if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
			    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
			    ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
			    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
				rc =  0;
				break;
			}
			cprev = cnow;
		}
		set_current_state( TASK_RUNNING );
		remove_wait_queue(&pCh->delta_msr_wait, &wait);

		i2QueueCommands(PTYPE_BYPASS, pCh, 100, 3, 
						 CMD_CTS_NREP, CMD_DSR_NREP, CMD_RI_NREP);
		if ( ! (pCh->flags	& ASYNC_CHECK_CD)) {
			i2QueueCommands(PTYPE_BYPASS, pCh, 100, 1, CMD_DCD_NREP);
		}
		serviceOutgoingFifo( pCh->pMyBord );
		return rc;
		break;

	/*
	 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
	 * Return: write counters to the user passed counter struct
	 * NB: both 1->0 and 0->1 transitions are counted except for RI where
	 * only 0->1 is counted. The controller is quite capable of counting
	 * both, but this done to preserve compatibility with the standard
	 * serial driver.
	 */
	case TIOCGICOUNT:
		ip2trace (CHANN, ITRC_IOCTL, 11, 1, rc );

		WRITE_LOCK_IRQSAVE(&pB->read_fifo_spinlock, flags);
		cnow = pCh->icount;
		WRITE_UNLOCK_IRQRESTORE(&pB->read_fifo_spinlock, flags);
		p_cuser = argp;
		rc = put_user(cnow.cts, &p_cuser->cts);
		rc = put_user(cnow.dsr, &p_cuser->dsr);
		rc = put_user(cnow.rng, &p_cuser->rng);
		rc = put_user(cnow.dcd, &p_cuser->dcd);
		rc = put_user(cnow.rx, &p_cuser->rx);
		rc = put_user(cnow.tx, &p_cuser->tx);
		rc = put_user(cnow.frame, &p_cuser->frame);
		rc = put_user(cnow.overrun, &p_cuser->overrun);
		rc = put_user(cnow.parity, &p_cuser->parity);
		rc = put_user(cnow.brk, &p_cuser->brk);
		rc = put_user(cnow.buf_overrun, &p_cuser->buf_overrun);
		break;

	/*
	 * The rest are not supported by this driver. By returning -ENOIOCTLCMD they
	 * will be passed to the line discipline for it to handle.
	 */
	case TIOCSERCONFIG:
	case TIOCSERGWILD:
	case TIOCSERGETLSR:
	case TIOCSERSWILD:
	case TIOCSERGSTRUCT:
	case TIOCSERGETMULTI:
	case TIOCSERSETMULTI:

	default:
		ip2trace (CHANN, ITRC_IOCTL, 12, 0 );

		rc =  -ENOIOCTLCMD;
		break;
	}

	ip2trace (CHANN, ITRC_IOCTL, ITRC_RETURN, 0 );

	return rc;
}

/******************************************************************************/
/* Function:   GetSerialInfo()                                                */
/* Parameters: Pointer to channel structure                                   */
/*             Pointer to old termios structure                               */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/* This is to support the setserial command, and requires processing of the   */
/* standard Linux serial structure.                                           */
/******************************************************************************/
static int
get_serial_info ( i2ChanStrPtr pCh, struct serial_struct __user *retinfo )
{
	struct serial_struct tmp;

	memset ( &tmp, 0, sizeof(tmp) );
	tmp.type = pCh->pMyBord->channelBtypes.bid_value[(pCh->port_index & (IP2_PORTS_PER_BOARD-1))/16];
	if (BID_HAS_654(tmp.type)) {
		tmp.type = PORT_16650;
	} else {
		tmp.type = PORT_CIRRUS;
	}
	tmp.line = pCh->port_index;
	tmp.port = pCh->pMyBord->i2eBase;
	tmp.irq  = ip2config.irq[pCh->port_index/64];
	tmp.flags = pCh->flags;
	tmp.baud_base = pCh->BaudBase;
	tmp.close_delay = pCh->ClosingDelay;
	tmp.closing_wait = pCh->ClosingWaitTime;
	tmp.custom_divisor = pCh->BaudDivisor;
   	return copy_to_user(retinfo,&tmp,sizeof(*retinfo));
}

/******************************************************************************/
/* Function:   SetSerialInfo()                                                */
/* Parameters: Pointer to channel structure                                   */
/*             Pointer to old termios structure                               */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/* This function provides support for setserial, which uses the TIOCSSERIAL   */
/* ioctl. Not all setserial parameters are relevant. If the user attempts to  */
/* change the IRQ, address or type of the port the ioctl fails.               */
/******************************************************************************/
static int
set_serial_info( i2ChanStrPtr pCh, struct serial_struct __user *new_info )
{
	struct serial_struct ns;
	int   old_flags, old_baud_divisor;

	if (copy_from_user(&ns, new_info, sizeof (ns)))
		return -EFAULT;

	/*
	 * We don't allow setserial to change IRQ, board address, type or baud
	 * base. Also line nunber as such is meaningless but we use it for our
	 * array index so it is fixed also.
	 */
	if ( (ns.irq  	    != ip2config.irq[pCh->port_index])
	    || ((int) ns.port      != ((int) (pCh->pMyBord->i2eBase)))
	    || (ns.baud_base != pCh->BaudBase)
	    || (ns.line      != pCh->port_index) ) {
		return -EINVAL;
	}

	old_flags = pCh->flags;
	old_baud_divisor = pCh->BaudDivisor;

	if ( !capable(CAP_SYS_ADMIN) ) {
		if ( ( ns.close_delay != pCh->ClosingDelay ) ||
		    ( (ns.flags & ~ASYNC_USR_MASK) !=
		      (pCh->flags & ~ASYNC_USR_MASK) ) ) {
			return -EPERM;
		}

		pCh->flags = (pCh->flags & ~ASYNC_USR_MASK) |
			       (ns.flags & ASYNC_USR_MASK);
		pCh->BaudDivisor = ns.custom_divisor;
	} else {
		pCh->flags = (pCh->flags & ~ASYNC_FLAGS) |
			       (ns.flags & ASYNC_FLAGS);
		pCh->BaudDivisor = ns.custom_divisor;
		pCh->ClosingDelay = ns.close_delay * HZ/100;
		pCh->ClosingWaitTime = ns.closing_wait * HZ/100;
	}

	if ( ( (old_flags & ASYNC_SPD_MASK) != (pCh->flags & ASYNC_SPD_MASK) )
	    || (old_baud_divisor != pCh->BaudDivisor) ) {
		// Invalidate speed and reset parameters
		set_params( pCh, NULL );
	}

	return 0;
}

/******************************************************************************/
/* Function:   ip2_set_termios()                                              */
/* Parameters: Pointer to tty structure                                       */
/*             Pointer to old termios structure                               */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static void
ip2_set_termios( PTTY tty, struct ktermios *old_termios )
{
	i2ChanStrPtr pCh = (i2ChanStrPtr)tty->driver_data;

#ifdef IP2DEBUG_IOCTL
	printk (KERN_DEBUG "IP2: set termios %p\n", old_termios );
#endif

	set_params( pCh, old_termios );
}

/******************************************************************************/
/* Function:   ip2_set_line_discipline()                                      */
/* Parameters: Pointer to tty structure                                       */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:  Does nothing                                                 */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static void
ip2_set_line_discipline ( PTTY tty )
{
#ifdef IP2DEBUG_IOCTL
	printk (KERN_DEBUG "IP2: set line discipline\n" );
#endif

	ip2trace (((i2ChanStrPtr)tty->driver_data)->port_index, ITRC_IOCTL, 16, 0 );

}

/******************************************************************************/
/* Function:   SetLine Characteristics()                                      */
/* Parameters: Pointer to channel structure                                   */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/* This routine is called to update the channel structure with the new line   */
/* characteristics, and send the appropriate commands to the board when they  */
/* change.                                                                    */
/******************************************************************************/
static void
set_params( i2ChanStrPtr pCh, struct ktermios *o_tios )
{
	tcflag_t cflag, iflag, lflag;
	char stop_char, start_char;
	struct ktermios dummy;

	lflag = pCh->pTTY->termios->c_lflag;
	cflag = pCh->pTTY->termios->c_cflag;
	iflag = pCh->pTTY->termios->c_iflag;

	if (o_tios == NULL) {
		dummy.c_lflag = ~lflag;
		dummy.c_cflag = ~cflag;
		dummy.c_iflag = ~iflag;
		o_tios = &dummy;
	}

	{
		switch ( cflag & CBAUD ) {
		case B0:
			i2QueueCommands( PTYPE_BYPASS, pCh, 100, 2, CMD_RTSDN, CMD_DTRDN);
			pCh->dataSetOut &= ~(I2_DTR | I2_RTS);
			i2QueueCommands( PTYPE_INLINE, pCh, 100, 1, CMD_PAUSE(25));
			pCh->pTTY->termios->c_cflag |= (CBAUD & o_tios->c_cflag);
			goto service_it;
			break;
		case B38400:
			/*
			 * This is the speed that is overloaded with all the other high
			 * speeds, depending upon the flag settings.
			 */
			if ( ( pCh->flags & ASYNC_SPD_MASK ) == ASYNC_SPD_HI ) {
				pCh->speed = CBR_57600;
			} else if ( (pCh->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI ) {
				pCh->speed = CBR_115200;
			} else if ( (pCh->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST ) {
				pCh->speed = CBR_C1;
			} else {
				pCh->speed = CBR_38400;
			}
			break;
		case B50:      pCh->speed = CBR_50;      break;
		case B75:      pCh->speed = CBR_75;      break;
		case B110:     pCh->speed = CBR_110;     break;
		case B134:     pCh->speed = CBR_134;     break;
		case B150:     pCh->speed = CBR_150;     break;
		case B200:     pCh->speed = CBR_200;     break;
		case B300:     pCh->speed = CBR_300;     break;
		case B600:     pCh->speed = CBR_600;     break;
		case B1200:    pCh->speed = CBR_1200;    break;
		case B1800:    pCh->speed = CBR_1800;    break;
		case B2400:    pCh->speed = CBR_2400;    break;
		case B4800:    pCh->speed = CBR_4800;    break;
		case B9600:    pCh->speed = CBR_9600;    break;
		case B19200:   pCh->speed = CBR_19200;   break;
		case B57600:   pCh->speed = CBR_57600;   break;
		case B115200:  pCh->speed = CBR_115200;  break;
		case B153600:  pCh->speed = CBR_153600;  break;
		case B230400:  pCh->speed = CBR_230400;  break;
		case B307200:  pCh->speed = CBR_307200;  break;
		case B460800:  pCh->speed = CBR_460800;  break;
		case B921600:  pCh->speed = CBR_921600;  break;
		default:       pCh->speed = CBR_9600;    break;
		}
		if ( pCh->speed == CBR_C1 ) {
			// Process the custom speed parameters.
			int bps = pCh->BaudBase / pCh->BaudDivisor;
			if ( bps == 921600 ) {
				pCh->speed = CBR_921600;
			} else {
				bps = bps/10;
				i2QueueCommands( PTYPE_INLINE, pCh, 100, 1, CMD_BAUD_DEF1(bps) );
			}
		}
		i2QueueCommands( PTYPE_INLINE, pCh, 100, 1, CMD_SETBAUD(pCh->speed));
		
		i2QueueCommands ( PTYPE_INLINE, pCh, 100, 2, CMD_DTRUP, CMD_RTSUP);
		pCh->dataSetOut |= (I2_DTR | I2_RTS);
	}
	if ( (CSTOPB & cflag) ^ (CSTOPB & o_tios->c_cflag)) 
	{
		i2QueueCommands ( PTYPE_INLINE, pCh, 100, 1, 
			CMD_SETSTOP( ( cflag & CSTOPB ) ? CST_2 : CST_1));
	}
	if (((PARENB|PARODD) & cflag) ^ ((PARENB|PARODD) & o_tios->c_cflag)) 
	{
		i2QueueCommands ( PTYPE_INLINE, pCh, 100, 1,
			CMD_SETPAR( 
				(cflag & PARENB ?  (cflag & PARODD ? CSP_OD : CSP_EV) : CSP_NP)
			)
		);
	}
	/* byte size and parity */
	if ( (CSIZE & cflag)^(CSIZE & o_tios->c_cflag)) 
	{
		int datasize;
		switch ( cflag & CSIZE ) {
		case CS5: datasize = CSZ_5; break;
		case CS6: datasize = CSZ_6; break;
		case CS7: datasize = CSZ_7; break;
		case CS8: datasize = CSZ_8; break;
		default:  datasize = CSZ_5; break;	/* as per serial.c */
		}
		i2QueueCommands ( PTYPE_INLINE, pCh, 100, 1, CMD_SETBITS(datasize) );
	}
	/* Process CTS flow control flag setting */
	if ( (cflag & CRTSCTS) ) {
		i2QueueCommands(PTYPE_INLINE, pCh, 100,
						2, CMD_CTSFL_ENAB, CMD_RTSFL_ENAB);
	} else {
		i2QueueCommands(PTYPE_INLINE, pCh, 100,
						2, CMD_CTSFL_DSAB, CMD_RTSFL_DSAB);
	}
	//
	// Process XON/XOFF flow control flags settings
	//
	stop_char = STOP_CHAR(pCh->pTTY);
	start_char = START_CHAR(pCh->pTTY);

	//////////// can't be \000
	if (stop_char == __DISABLED_CHAR ) 
	{
		stop_char = ~__DISABLED_CHAR; 
	}
	if (start_char == __DISABLED_CHAR ) 
	{
		start_char = ~__DISABLED_CHAR;
	}
	/////////////////////////////////

	if ( o_tios->c_cc[VSTART] != start_char ) 
	{
		i2QueueCommands(PTYPE_BYPASS, pCh, 100, 1, CMD_DEF_IXON(start_char));
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_DEF_OXON(start_char));
	}
	if ( o_tios->c_cc[VSTOP] != stop_char ) 
	{
		 i2QueueCommands(PTYPE_BYPASS, pCh, 100, 1, CMD_DEF_IXOFF(stop_char));
		 i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_DEF_OXOFF(stop_char));
	}
	if (stop_char == __DISABLED_CHAR ) 
	{
		stop_char = ~__DISABLED_CHAR;  //TEST123
		goto no_xoff;
	}
	if ((iflag & (IXOFF))^(o_tios->c_iflag & (IXOFF))) 
	{
		if ( iflag & IXOFF ) {	// Enable XOFF output flow control
			i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_OXON_OPT(COX_XON));
		} else {	// Disable XOFF output flow control
no_xoff:
			i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_OXON_OPT(COX_NONE));
		}
	}
	if (start_char == __DISABLED_CHAR ) 
	{
		goto no_xon;
	}
	if ((iflag & (IXON|IXANY)) ^ (o_tios->c_iflag & (IXON|IXANY))) 
	{
		if ( iflag & IXON ) {
			if ( iflag & IXANY ) { // Enable XON/XANY output flow control
				i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_IXON_OPT(CIX_XANY));
			} else { // Enable XON output flow control
				i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_IXON_OPT(CIX_XON));
			}
		} else { // Disable XON output flow control
no_xon:
			i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_IXON_OPT(CIX_NONE));
		}
	}
	if ( (iflag & ISTRIP) ^ ( o_tios->c_iflag & (ISTRIP)) ) 
	{
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, 
				CMD_ISTRIP_OPT((iflag & ISTRIP ? 1 : 0)));
	}
	if ( (iflag & INPCK) ^ ( o_tios->c_iflag & (INPCK)) ) 
	{
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, 
				CMD_PARCHK((iflag & INPCK) ? CPK_ENAB : CPK_DSAB));
	}

	if ( (iflag & (IGNBRK|PARMRK|BRKINT|IGNPAR)) 
			^	( o_tios->c_iflag & (IGNBRK|PARMRK|BRKINT|IGNPAR)) ) 
	{
		char brkrpt = 0;
		char parrpt = 0;

		if ( iflag & IGNBRK ) { /* Ignore breaks altogether */
			/* Ignore breaks altogether */
			i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_BRK_NREP);
		} else {
			if ( iflag & BRKINT ) {
				if ( iflag & PARMRK ) {
					brkrpt = 0x0a;	// exception an inline triple
				} else {
					brkrpt = 0x1a;	// exception and NULL
				}
				brkrpt |= 0x04;	// flush input
			} else {
				if ( iflag & PARMRK ) {
					brkrpt = 0x0b;	//POSIX triple \0377 \0 \0
				} else {
					brkrpt = 0x01;	// Null only
				}
			}
			i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_BRK_REP(brkrpt));
		} 

		if (iflag & IGNPAR) {
			parrpt = 0x20;
													/* would be 2 for not cirrus bug */
													/* would be 0x20 cept for cirrus bug */
		} else {
			if ( iflag & PARMRK ) {
				/*
				 * Replace error characters with 3-byte sequence (\0377,\0,char)
				 */
				parrpt = 0x04 ;
				i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_ISTRIP_OPT((char)0));
			} else {
				parrpt = 0x03;
			} 
		}
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_SET_ERROR(parrpt));
	}
	if (cflag & CLOCAL) {
		// Status reporting fails for DCD if this is off
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_DCD_NREP);
		pCh->flags &= ~ASYNC_CHECK_CD;
	} else {
		i2QueueCommands(PTYPE_INLINE, pCh, 100, 1, CMD_DCD_REP);
		pCh->flags	|= ASYNC_CHECK_CD;
	}

service_it:
	i2DrainOutput( pCh, 100 );		
}

/******************************************************************************/
/* IPL Device Section                                                         */
/******************************************************************************/

/******************************************************************************/
/* Function:   ip2_ipl_read()                                                  */
/* Parameters: Pointer to device inode                                        */
/*             Pointer to file structure                                      */
/*             Pointer to data                                                */
/*             Number of bytes to read                                        */
/* Returns:    Success or failure                                             */
/*                                                                            */
/* Description:   Ugly                                                        */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

static 
ssize_t
ip2_ipl_read(struct file *pFile, char __user *pData, size_t count, loff_t *off )
{
	unsigned int minor = iminor(pFile->f_path.dentry->d_inode);
	int rc = 0;

#ifdef IP2DEBUG_IPL
	printk (KERN_DEBUG "IP2IPL: read %p, %d bytes\n", pData, count );
#endif

	switch( minor ) {
	case 0:	    // IPL device
		rc = -EINVAL;
		break;
	case 1:	    // Status dump
		rc = -EINVAL;
		break;
	case 2:	    // Ping device
		rc = -EINVAL;
		break;
	case 3:	    // Trace device
		rc = DumpTraceBuffer ( pData, count );
		break;
	case 4:	    // Trace device
		rc = DumpFifoBuffer ( pData, count );
		break;
	default:
		rc = -ENODEV;
		break;
	}
	return rc;
}

static int
DumpFifoBuffer ( char __user *pData, int count )
{
#ifdef DEBUG_FIFO
	int rc;
	rc = copy_to_user(pData, DBGBuf, count);

	printk(KERN_DEBUG "Last index %d\n", I );

	return count;
#endif	/* DEBUG_FIFO */
	return 0;
}

static int
DumpTraceBuffer ( char __user *pData, int count )
{
#ifdef IP2DEBUG_TRACE
	int rc;
	int dumpcount;
	int chunk;
	int *pIndex = (int __user *)pData;

	if ( count < (sizeof(int) * 6) ) {
		return -EIO;
	}
	rc = put_user(tracewrap, pIndex );
	rc = put_user(TRACEMAX, ++pIndex );
	rc = put_user(tracestrip, ++pIndex );
	rc = put_user(tracestuff, ++pIndex );
	pData += sizeof(int) * 6;
	count -= sizeof(int) * 6;

	dumpcount = tracestuff - tracestrip;
	if ( dumpcount < 0 ) {
		dumpcount += TRACEMAX;
	}
	if ( dumpcount > count ) {
		dumpcount = count;
	}
	chunk = TRACEMAX - tracestrip;
	if ( dumpcount > chunk ) {
		rc = copy_to_user(pData, &tracebuf[tracestrip],
			      chunk * sizeof(tracebuf[0]) );
		pData += chunk * sizeof(tracebuf[0]);
		tracestrip = 0;
		chunk = dumpcount - chunk;
	} else {
		chunk = dumpcount;
	}
	rc = copy_to_user(pData, &tracebuf[tracestrip],
		      chunk * sizeof(tracebuf[0]) );
	tracestrip += chunk;
	tracewrap = 0;

	rc = put_user(tracestrip, ++pIndex );
	rc = put_user(tracestuff, ++pIndex );

	return dumpcount;
#else
	return 0;
#endif
}

/******************************************************************************/
/* Function:   ip2_ipl_write()                                                 */
/* Parameters:                                                                */
/*             Pointer to file structure                                      */
/*             Pointer to data                                                */
/*             Number of bytes to write                                       */
/* Returns:    Success or failure                                             */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static ssize_t
ip2_ipl_write(struct file *pFile, const char __user *pData, size_t count, loff_t *off)
{
#ifdef IP2DEBUG_IPL
	printk (KERN_DEBUG "IP2IPL: write %p, %d bytes\n", pData, count );
#endif
	return 0;
}

/******************************************************************************/
/* Function:   ip2_ipl_ioctl()                                                */
/* Parameters: Pointer to device inode                                        */
/*             Pointer to file structure                                      */
/*             Command                                                        */
/*             Argument                                                       */
/* Returns:    Success or failure                                             */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static int
ip2_ipl_ioctl ( struct inode *pInode, struct file *pFile, UINT cmd, ULONG arg )
{
	unsigned int iplminor = iminor(pInode);
	int rc = 0;
	void __user *argp = (void __user *)arg;
	ULONG __user *pIndex = argp;
	i2eBordStrPtr pB = i2BoardPtrTable[iplminor / 4];
	i2ChanStrPtr pCh;

#ifdef IP2DEBUG_IPL
	printk (KERN_DEBUG "IP2IPL: ioctl cmd %d, arg %ld\n", cmd, arg );
#endif

	switch ( iplminor ) {
	case 0:	    // IPL device
		rc = -EINVAL;
		break;
	case 1:	    // Status dump
	case 5:
	case 9:
	case 13:
		switch ( cmd ) {
		case 64:	/* Driver - ip2stat */
			rc = put_user(ip2_tty_driver->refcount, pIndex++ );
			rc = put_user(irq_counter, pIndex++  );
			rc = put_user(bh_counter, pIndex++  );
			break;

		case 65:	/* Board  - ip2stat */
			if ( pB ) {
				rc = copy_to_user(argp, pB, sizeof(i2eBordStr));
				rc = put_user(INB(pB->i2eStatus),
					(ULONG __user *)(arg + (ULONG)(&pB->i2eStatus) - (ULONG)pB ) );
			} else {
				rc = -ENODEV;
			}
			break;

		default:
			if (cmd < IP2_MAX_PORTS) {
				pCh = DevTable[cmd];
				if ( pCh )
				{
					rc = copy_to_user(argp, pCh, sizeof(i2ChanStr));
				} else {
					rc = -ENODEV;
				}
			} else {
				rc = -EINVAL;
			}
		}
		break;

	case 2:	    // Ping device
		rc = -EINVAL;
		break;
	case 3:	    // Trace device
		/*
		 * akpm: This used to write a whole bunch of function addresses
		 * to userspace, which generated lots of put_user() warnings.
		 * I killed it all.  Just return "success" and don't do
		 * anything.
		 */
		if (cmd == 1)
			rc = 0;
		else
			rc = -EINVAL;
		break;

	default:
		rc = -ENODEV;
		break;
	}
	return rc;
}

/******************************************************************************/
/* Function:   ip2_ipl_open()                                                 */
/* Parameters: Pointer to device inode                                        */
/*             Pointer to file structure                                      */
/* Returns:    Success or failure                                             */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
static int
ip2_ipl_open( struct inode *pInode, struct file *pFile )
{
	unsigned int iplminor = iminor(pInode);
	i2eBordStrPtr pB;
	i2ChanStrPtr  pCh;

#ifdef IP2DEBUG_IPL
	printk (KERN_DEBUG "IP2IPL: open\n" );
#endif

	switch(iplminor) {
	// These are the IPL devices
	case 0:
	case 4:
	case 8:
	case 12:
		break;

	// These are the status devices
	case 1:
	case 5:
	case 9:
	case 13:
		break;

	// These are the debug devices
	case 2:
	case 6:
	case 10:
	case 14:
		pB = i2BoardPtrTable[iplminor / 4];
		pCh = (i2ChanStrPtr) pB->i2eChannelPtr;
		break;

	// This is the trace device
	case 3:
		break;
	}
	return 0;
}
/******************************************************************************/
/* Function:   ip2_read_procmem                                               */
/* Parameters:                                                                */
/*                                                                            */
/* Returns: Length of output                                                  */
/*                                                                            */
/* Description:                                                               */
/*   Supplies some driver operating parameters                                */
/*	Not real useful unless your debugging the fifo							  */
/*                                                                            */
/******************************************************************************/

#define LIMIT  (PAGE_SIZE - 120)

static int
ip2_read_procmem(char *buf, char **start, off_t offset, int len)
{
	i2eBordStrPtr  pB;
	i2ChanStrPtr  pCh;
	PTTY tty;
	int i;

	len = 0;

#define FMTLINE	"%3d: 0x%08x 0x%08x 0%011o 0%011o\n"
#define FMTLIN2	"     0x%04x 0x%04x tx flow 0x%x\n"
#define FMTLIN3	"     0x%04x 0x%04x rc flow\n"

	len += sprintf(buf+len,"\n");

	for( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		pB = i2BoardPtrTable[i];
		if ( pB ) {
			len += sprintf(buf+len,"board %d:\n",i);
			len += sprintf(buf+len,"\tFifo rem: %d mty: %x outM %x\n",
				pB->i2eFifoRemains,pB->i2eWaitingForEmptyFifo,pB->i2eOutMailWaiting);
		}
	}

	len += sprintf(buf+len,"#: tty flags, port flags,     cflags,     iflags\n");
	for (i=0; i < IP2_MAX_PORTS; i++) {
		if (len > LIMIT)
			break;
		pCh = DevTable[i];
		if (pCh) {
			tty = pCh->pTTY;
			if (tty && tty->count) {
				len += sprintf(buf+len,FMTLINE,i,(int)tty->flags,pCh->flags,
									tty->termios->c_cflag,tty->termios->c_iflag);

				len += sprintf(buf+len,FMTLIN2,
						pCh->outfl.asof,pCh->outfl.room,pCh->channelNeeds);
				len += sprintf(buf+len,FMTLIN3,pCh->infl.asof,pCh->infl.room);
			}
		}
	}
	return len;
}

/*
 * This is the handler for /proc/tty/driver/ip2
 *
 * This stretch of code has been largely plagerized from at least three
 * different sources including ip2mkdev.c and a couple of other drivers.
 * The bugs are all mine.  :-)	=mhw=
 */
static int ip2_read_proc(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int	i, j, box;
	int	len = 0;
	int	boxes = 0;
	int	ports = 0;
	int	tports = 0;
	off_t	begin = 0;
	i2eBordStrPtr  pB;

	len += sprintf(page, "ip2info: 1.0 driver: %s\n", pcVersion );
	len += sprintf(page+len, "Driver: SMajor=%d CMajor=%d IMajor=%d MaxBoards=%d MaxBoxes=%d MaxPorts=%d\n",
			IP2_TTY_MAJOR, IP2_CALLOUT_MAJOR, IP2_IPL_MAJOR,
			IP2_MAX_BOARDS, ABS_MAX_BOXES, ABS_BIGGEST_BOX);

	for( i = 0; i < IP2_MAX_BOARDS; ++i ) {
		/* This need to be reset for a board by board count... */
		boxes = 0;
		pB = i2BoardPtrTable[i];
		if( pB ) {
			switch( pB->i2ePom.e.porID & ~POR_ID_RESERVED ) 
			{
			case POR_ID_FIIEX:
				len += sprintf( page+len, "Board %d: EX ports=", i );
				for( box = 0; box < ABS_MAX_BOXES; ++box )
				{
					ports = 0;

					if( pB->i2eChannelMap[box] != 0 ) ++boxes;
					for( j = 0; j < ABS_BIGGEST_BOX; ++j ) 
					{
						if( pB->i2eChannelMap[box] & 1<< j ) {
							++ports;
						}
					}
					len += sprintf( page+len, "%d,", ports );
					tports += ports;
				}

				--len;	/* Backup over that last comma */

				len += sprintf( page+len, " boxes=%d width=%d", boxes, pB->i2eDataWidth16 ? 16 : 8 );
				break;

			case POR_ID_II_4:
				len += sprintf(page+len, "Board %d: ISA-4 ports=4 boxes=1", i );
				tports = ports = 4;
				break;

			case POR_ID_II_8:
				len += sprintf(page+len, "Board %d: ISA-8-std ports=8 boxes=1", i );
				tports = ports = 8;
				break;

			case POR_ID_II_8R:
				len += sprintf(page+len, "Board %d: ISA-8-RJ11 ports=8 boxes=1", i );
				tports = ports = 8;
				break;

			default:
				len += sprintf(page+len, "Board %d: unknown", i );
				/* Don't try and probe for minor numbers */
				tports = ports = 0;
			}

		} else {
			/* Don't try and probe for minor numbers */
			len += sprintf(page+len, "Board %d: vacant", i );
			tports = ports = 0;
		}

		if( tports ) {
			len += sprintf(page+len, " minors=" );

			for ( box = 0; box < ABS_MAX_BOXES; ++box )
			{
				for ( j = 0; j < ABS_BIGGEST_BOX; ++j )
				{
					if ( pB->i2eChannelMap[box] & (1 << j) )
					{
						len += sprintf (page+len,"%d,",
							j + ABS_BIGGEST_BOX *
							(box+i*ABS_MAX_BOXES));
					}
				}
			}

			page[ len - 1 ] = '\n';	/* Overwrite that last comma */
		} else {
			len += sprintf (page+len,"\n" );
		}

		if (len+begin > off+count)
			break;
		if (len+begin < off) {
			begin += len;
			len = 0;
		}
	}

	if (i >= IP2_MAX_BOARDS)
		*eof = 1;
	if (off >= len+begin)
		return 0;

	*start = page + (off-begin);
	return ((count < begin+len-off) ? count : begin+len-off);
 }
 
/******************************************************************************/
/* Function:   ip2trace()                                                     */
/* Parameters: Value to add to trace buffer                                   */
/* Returns:    Nothing                                                        */
/*                                                                            */
/* Description:                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
#ifdef IP2DEBUG_TRACE
void
ip2trace (unsigned short pn, unsigned char cat, unsigned char label, unsigned long codes, ...)
{
	long flags;
	unsigned long *pCode = &codes;
	union ip2breadcrumb bc;
	i2ChanStrPtr  pCh;


	tracebuf[tracestuff++] = jiffies;
	if ( tracestuff == TRACEMAX ) {
		tracestuff = 0;
	}
	if ( tracestuff == tracestrip ) {
		if ( ++tracestrip == TRACEMAX ) {
			tracestrip = 0;
		}
		++tracewrap;
	}

	bc.hdr.port  = 0xff & pn;
	bc.hdr.cat   = cat;
	bc.hdr.codes = (unsigned char)( codes & 0xff );
	bc.hdr.label = label;
	tracebuf[tracestuff++] = bc.value;

	for (;;) {
		if ( tracestuff == TRACEMAX ) {
			tracestuff = 0;
		}
		if ( tracestuff == tracestrip ) {
			if ( ++tracestrip == TRACEMAX ) {
				tracestrip = 0;
			}
			++tracewrap;
		}

		if ( !codes-- )
			break;

		tracebuf[tracestuff++] = *++pCode;
	}
}
#endif


MODULE_LICENSE("GPL");

static struct pci_device_id ip2main_pci_tbl[] __devinitdata = {
	{ PCI_DEVICE(PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_IP2EX) },
	{ }
};

MODULE_DEVICE_TABLE(pci, ip2main_pci_tbl);
