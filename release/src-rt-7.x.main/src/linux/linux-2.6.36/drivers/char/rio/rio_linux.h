
/*
 *  rio_linux.h
 *
 *  Copyright (C) 1998,1999,2000 R.E.Wolff@BitWizard.nl
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  RIO serial driver.
 *
 *  Version 1.0 -- July, 1999. 
 * 
 */

#define RIO_NBOARDS        4
#define RIO_PORTSPERBOARD 128
#define RIO_NPORTS        (RIO_NBOARDS * RIO_PORTSPERBOARD)

#define MODEM_SUPPORT

#ifdef __KERNEL__

#define RIO_MAGIC 0x12345678


struct vpd_prom {
	unsigned short id;
	char hwrev;
	char hwass;
	int uniqid;
	char myear;
	char mweek;
	char hw_feature[5];
	char oem_id;
	char identifier[16];
};


#define RIO_DEBUG_ALL           0xffffffff

#define O_OTHER(tty)    \
      ((O_OLCUC(tty))  ||\
      (O_ONLCR(tty))   ||\
      (O_OCRNL(tty))   ||\
      (O_ONOCR(tty))   ||\
      (O_ONLRET(tty))  ||\
      (O_OFILL(tty))   ||\
      (O_OFDEL(tty))   ||\
      (O_NLDLY(tty))   ||\
      (O_CRDLY(tty))   ||\
      (O_TABDLY(tty))  ||\
      (O_BSDLY(tty))   ||\
      (O_VTDLY(tty))   ||\
      (O_FFDLY(tty)))

/* Same for input. */
#define I_OTHER(tty)    \
      ((I_INLCR(tty))  ||\
      (I_IGNCR(tty))   ||\
      (I_ICRNL(tty))   ||\
      (I_IUCLC(tty))   ||\
      (L_ISIG(tty)))


#endif				/* __KERNEL__ */


#define RIO_BOARD_INTR_LOCK  1


#ifndef RIOCTL_MISC_MINOR
/* Allow others to gather this into "major.h" or something like that */
#define RIOCTL_MISC_MINOR    169
#endif


/* Allow us to debug "in the field" without requiring clients to
   recompile.... */
#define rio_spin_lock_irqsave(sem, flags) do { \
	rio_dprintk (RIO_DEBUG_SPINLOCK, "spinlockirqsave: %p %s:%d\n", \
	                                sem, __FILE__, __LINE__);\
	spin_lock_irqsave(sem, flags);\
	} while (0)

#define rio_spin_unlock_irqrestore(sem, flags) do { \
	rio_dprintk (RIO_DEBUG_SPINLOCK, "spinunlockirqrestore: %p %s:%d\n",\
	                                sem, __FILE__, __LINE__);\
	spin_unlock_irqrestore(sem, flags);\
	} while (0)

#define rio_spin_lock(sem) do { \
	rio_dprintk (RIO_DEBUG_SPINLOCK, "spinlock: %p %s:%d\n",\
	                                sem, __FILE__, __LINE__);\
	spin_lock(sem);\
	} while (0)

#define rio_spin_unlock(sem) do { \
	rio_dprintk (RIO_DEBUG_SPINLOCK, "spinunlock: %p %s:%d\n",\
	                                sem, __FILE__, __LINE__);\
	spin_unlock(sem);\
	} while (0)



#ifdef CONFIG_RIO_OLDPCI
static inline void __iomem *rio_memcpy_toio(void __iomem *dummy, void __iomem *dest, void *source, int n)
{
	char __iomem *dst = dest;
	char *src = source;

	while (n--) {
		writeb(*src++, dst++);
		(void) readb(dummy);
	}

	return dest;
}

static inline void __iomem *rio_copy_toio(void __iomem *dest, void *source, int n)
{
	char __iomem *dst = dest;
	char *src = source;

	while (n--)
		writeb(*src++, dst++);

	return dest;
}


static inline void *rio_memcpy_fromio(void *dest, void __iomem *source, int n)
{
	char *dst = dest;
	char __iomem *src = source;

	while (n--)
		*dst++ = readb(src++);

	return dest;
}

#else
#define rio_memcpy_toio(dummy,dest,source,n)   memcpy_toio(dest, source, n)
#define rio_copy_toio   		       memcpy_toio
#define rio_memcpy_fromio                      memcpy_fromio
#endif

#define DEBUG 1


/* 
   This driver can spew a whole lot of debugging output at you. If you
   need maximum performance, you should disable the DEBUG define. To
   aid in debugging in the field, I'm leaving the compile-time debug
   features enabled, and disable them "runtime". That allows me to
   instruct people with problems to enable debugging without requiring
   them to recompile... 
*/

#ifdef DEBUG
#define rio_dprintk(f, str...) do { if (rio_debug & f) printk (str);} while (0)
#define func_enter() rio_dprintk (RIO_DEBUG_FLOW, "rio: enter %s\n", __func__)
#define func_exit()  rio_dprintk (RIO_DEBUG_FLOW, "rio: exit  %s\n", __func__)
#define func_enter2() rio_dprintk (RIO_DEBUG_FLOW, "rio: enter %s (port %d)\n",__func__, port->line)
#else
#define rio_dprintk(f, str...)	/* nothing */
#define func_enter()
#define func_exit()
#define func_enter2()
#endif
