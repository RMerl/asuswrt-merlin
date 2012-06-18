/*
 * FILE NAME au1000_gpio.h
 *
 * BRIEF MODULE DESCRIPTION
 *	API to Alchemy Au1000 GPIO device.
 *
 *  Author: MontaVista Software, Inc.  <source@mvista.com>
 *          Steve Longerbeam <stevel@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 *  Revision history
 *    01/31/02  0.01   Initial release. Steve Longerbeam, MontaVista
 *    10/12/03  0.1    Added Au1100/Au1500, GPIO2, and bit operations. K.C. Nishio, AMD
 *    08/05/04  0.11   Added Au1550 and Au1200. K.C. Nishio
 */

#ifndef __AU1000_GPIO_H
#define __AU1000_GPIO_H

#include <linux/ioctl.h>

#define AU1000GPIO_IOC_MAGIC 'A'

#define AU1000GPIO_IN		_IOR (AU1000GPIO_IOC_MAGIC, 0, int)
#define AU1000GPIO_SET		_IOW (AU1000GPIO_IOC_MAGIC, 1, int)
#define AU1000GPIO_CLEAR	_IOW (AU1000GPIO_IOC_MAGIC, 2, int)
#define AU1000GPIO_OUT		_IOW (AU1000GPIO_IOC_MAGIC, 3, int)
#define AU1000GPIO_TRISTATE	_IOW (AU1000GPIO_IOC_MAGIC, 4, int)
#define AU1000GPIO_AVAIL_MASK	_IOR (AU1000GPIO_IOC_MAGIC, 5, int)

// bit operations
#define AU1000GPIO_BIT_READ	_IOW (AU1000GPIO_IOC_MAGIC, 6, int)
#define AU1000GPIO_BIT_SET	_IOW (AU1000GPIO_IOC_MAGIC, 7, int)
#define AU1000GPIO_BIT_CLEAR	_IOW (AU1000GPIO_IOC_MAGIC, 8, int)
#define AU1000GPIO_BIT_TRISTATE	_IOW (AU1000GPIO_IOC_MAGIC, 9, int)
#define AU1000GPIO_BIT_INIT	_IOW (AU1000GPIO_IOC_MAGIC, 10, int)
#define AU1000GPIO_BIT_TERM	_IOW (AU1000GPIO_IOC_MAGIC, 11, int)

/* set this major numer same as the CRIS GPIO driver */
#define AU1X00_GPIO_MAJOR	(120)

#define ENABLED_ZERO		(0)
#define ENABLED_ONE		(1)
#define ENABLED_10		(0x2)
#define ENABLED_11		(0x3)
#define ENABLED_111		(0x7)
#define NOT_AVAIL		(-1)
#define AU1X00_MAX_PRIMARY_GPIO	(32) 

#define AU1000_GPIO_MINOR_MAX	AU1X00_MAX_PRIMARY_GPIO
/* Au1100, 1500, 1550 and 1200 have the secondary GPIO block */
#define AU1XX0_GPIO_MINOR_MAX	(48)

#define AU1X00_GPIO_NAME	"gpio"

/* GPIO pins which are not multiplexed */
#if defined(CONFIG_SOC_AU1000)
  #define NATIVE_GPIOPIN	((1 << 15) | (1 << 8) | (1 << 7) | (1 << 1) | (1 << 0))
  #define NATIVE_GPIO2PIN	(0)
#elif defined(CONFIG_SOC_AU1100)
  #define NATIVE_GPIOPIN	((1 << 23) | (1 << 22) | (1 << 21) | (1 << 20) | (1 << 19) | (1 << 18) | \
				 (1 << 17) | (1 << 16) | (1 << 7) | (1 << 1) | (1 << 0))
  #define NATIVE_GPIO2PIN	(0)
#elif defined(CONFIG_SOC_AU1500)
  #define NATIVE_GPIOPIN	((1 << 15) | (1 << 8) | (1 << 7) | (1 << 1) | (1 << 0))
  /* exclude the PCI reset output signal: GPIO[200], DMA_REQ2 and DMA_REQ3 */
  #define NATIVE_GPIO2PIN	(0xfffe & ~((1 << 9) | (1 << 8))) 
#elif defined(CONFIG_SOC_AU1550)
  #define NATIVE_GPIOPIN	((1 << 15) | (1 << 8) | (1 << 7) | (1 << 6) | (1 << 1) | (1 << 0))
  /* please refere Au1550 Data Book, chapter 15 */
  #define NATIVE_GPIO2PIN	(1 << 5) 
#elif defined(CONFIG_SOC_AU1200)
  #define NATIVE_GPIOPIN	((1 << 7) | (1 << 5))
  #define NATIVE_GPIO2PIN	(0) 
#endif

/* minor as u32 */
#define MINOR_TO_GPIOPIN(minor)		((minor < AU1X00_MAX_PRIMARY_GPIO) ? minor : (minor - AU1X00_MAX_PRIMARY_GPIO))
#define IS_PRIMARY_GPIOPIN(minor)	((minor < AU1X00_MAX_PRIMARY_GPIO) ? 1 : 0)

/*
 * pin to minor mapping.
 * GPIO0-GPIO31, minor=0-31.
 * GPIO200-GPIO215, minor=32-47.
 */
typedef struct _au1x00_gpio_bit_ctl {
	int direction;	// The direction of this GPIO pin. 0: IN, 1: OUT.
	int data;	// Pin output when itized (0/1), or at the term. 0/1/-1 (tristate).
} au1x00_gpio_bit_ctl;

typedef struct _au1x00_gpio_driver {
	const char	*driver_name;
	const char	*name;
	int		name_base;	/* offset of printed name */
	short		major;		/* major device number */
	short		minor_start;	/* start of minor device number*/
	short		num;		/* number of devices */
} au1x00_gpio_driver;

#ifdef __KERNEL__
extern u32 get_au1000_avail_gpio_mask(u32 *avail_gpio2);
extern int au1000gpio_tristate(u32 minor, u32 data);
extern int au1000gpio_in(u32 minor, u32 *data);
extern int au1000gpio_set(u32 minor, u32 data);
extern int au1000gpio_clear(u32 minor, u32 data);
extern int au1000gpio_out(u32 minor, u32 data);
extern int au1000gpio_bit_read(u32 minor, u32 *read_data);
extern int au1000gpio_bit_set(u32 minor);
extern int au1000gpio_bit_clear(u32 minor);
extern int au1000gpio_bit_tristate(u32 minor);
extern int check_minor_to_gpio(u32 minor);
extern int au1000gpio_bit_init(u32 minor, au1x00_gpio_bit_ctl *bit_opt);
extern int au1000gpio_bit_term(u32 minor, au1x00_gpio_bit_ctl *bit_opt);

extern void gpio_register_devfs (au1x00_gpio_driver *driver, unsigned int flags, unsigned minor);
extern void gpio_unregister_devfs (au1x00_gpio_driver *driver, unsigned minor);
extern int gpio_register_driver(au1x00_gpio_driver *driver);
extern int gpio_unregister_driver(au1x00_gpio_driver *driver);
#endif

#endif
