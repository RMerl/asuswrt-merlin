/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright, Ralink Technology, Inc.
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
 *
 *
 ***************************************************************************
 */

#ifndef __RALINK_GPIO_H__
#define __RALINK_GPIO_H__

//#include <asm/rt2880/rt_mmap.h>
#if defined(RTCONFIG_RALINK_MT7620) || defined(RTCONFIG_RALINK_MT7621)
#define RALINK_GPIO_LED_LOW_ACT		1

#define GPIO_TRACE_NONE		0
#define GPIO_TRACE_IO		1
#define GPIO_TRACE_INT		2
#define GPIO_TRACE_ALL		3
#else
# error "Invalid Gpio Trace In/Out Mapping"
#endif
/*
 * ioctl commands
 */

#if defined(RTCONFIG_RALINK_MT7620) || defined(RTCONFIG_RALINK_MT7621)
#define	RALINK_GPIO_READ_BIT		0x04
#define	RALINK_GPIO_WRITE_BIT		0x05
#define	RALINK_GPIO_READ_BYTE		0x06
#define	RALINK_GPIO_WRITE_BYTE		0x07
#define	RALINK_GPIO_READ_INT		0x02 //same as read
#define	RALINK_GPIO_WRITE_INT		0x03 //same as write
#define	RALINK_GPIO_SET_INT		0x21 //same as set
#define	RALINK_GPIO_CLEAR_INT		0x31 //same as clear
#define RALINK_GPIO_ENABLE_INTP		0x08
#define RALINK_GPIO_DISABLE_INTP	0x09
#define RALINK_GPIO_REG_IRQ		0x0A
#define RALINK_GPIO_LED_SET		0x41
#define RALINK_SET_GPIO_MODE		0x42
#else
# error "Invalid Gpio Ioctl Mapping"
#endif


#if defined(RTCONFIG_RALINK_MT7620)
//gpio 0~23
#define	RALINK_GPIO2300_SET_DIR		0x01 
#define RALINK_GPIO2300_SET_DIR_IN	0x11
#define RALINK_GPIO2300_SET_DIR_OUT	0x12
#define	RALINK_GPIO2300_READ		0x02
#define	RALINK_GPIO2300_WRITE		0x03
#define	RALINK_GPIO2300_SET		0x21
#define	RALINK_GPIO2300_CLEAR		0x31
//gpio 24~39
#define RALINK_GPIO3924_SET_DIR         0x51
#define RALINK_GPIO3924_SET_DIR_IN      0x13
#define RALINK_GPIO3924_SET_DIR_OUT     0x14
#define RALINK_GPIO3924_READ            0x52
#define RALINK_GPIO3924_WRITE           0x53
#define RALINK_GPIO3924_SET             0x22
#define RALINK_GPIO3924_CLEAR           0x32
//gpio 40~71
#define RALINK_GPIO7140_SET_DIR         0x61
#define RALINK_GPIO7140_SET_DIR_IN      0x15
#define RALINK_GPIO7140_SET_DIR_OUT     0x16
#define RALINK_GPIO7140_READ            0x62
#define RALINK_GPIO7140_WRITE           0x63
#define RALINK_GPIO7140_SET             0x23
#define RALINK_GPIO7140_CLEAR           0x33
//GPIO72 is WLAN_LED
#define RALINK_ATE_GPIO72		0x35
#define RALINK_GPIO_SET_MODE		0x60

#elif defined(RTCONFIG_RALINK_MT7621)
//gpio 0~31
#define	RALINK_GPIO3100_SET_DIR		0x01 
#define RALINK_GPIO3100_SET_DIR_IN	0x11
#define RALINK_GPIO3100_SET_DIR_OUT	0x12
#define	RALINK_GPIO3100_READ		0x02
#define	RALINK_GPIO3100_WRITE		0x03
#define	RALINK_GPIO3100_SET		0x21
#define	RALINK_GPIO3100_CLEAR		0x31

//gpio 32~63
#define RALINK_GPIO6332_SET_DIR         0x51
#define RALINK_GPIO6332_SET_DIR_IN      0x13
#define RALINK_GPIO6332_SET_DIR_OUT     0x14
#define RALINK_GPIO6332_READ            0x52
#define RALINK_GPIO6332_WRITE           0x53
#define RALINK_GPIO6332_SET             0x22
#define RALINK_GPIO6332_CLEAR           0x32

//gpio 64~95 
#define RALINK_GPIO9564_SET_DIR         0x61
#define RALINK_GPIO9564_SET_DIR_IN      0x15
#define RALINK_GPIO9564_SET_DIR_OUT     0x16
#define RALINK_GPIO9564_READ            0x62
#define RALINK_GPIO9564_WRITE           0x63
#define RALINK_GPIO9564_SET             0x23
#define RALINK_GPIO9564_CLEAR           0x33

#define RALINK_GPIO_SET_MODE		0x60
#else
# error "Invalid Gpio Number Mapping"
#endif

/*
 * Address of RALINK_ Registers
 */
//# RALINK_REG_RSTCTRL2		(RALINK_SYSCTL_ADDR + 0x834)	// RSTCTRL2
//#define RALINK_REG_GPIOOE		(RALINK_PRGIO_ADDR + 0x14)
//#define RALINK_REG_PIO3116DIR		(RALINK_PRGIO_ADDR + 0x20)	// GPIOCTRL1
//#define RALINK_REG_SGPIOLEDDATA		(RALINK_PRGIO_ADDR + 0x24)

#if defined(RTCONFIG_RALINK_MT7620) || defined(RTCONFIG_RALINK_MT7621)
#define RALINK_SYSCTL_ADDR		RALINK_SYSCTL_BASE	// system control
#define RALINK_REG_GPIOMODE		(RALINK_SYSCTL_ADDR + 0x60)
#define RALINK_IRQ_ADDR			RALINK_INTCL_BASE
#define RALINK_PRGIO_ADDR		RALINK_PIO_BASE // Programmable I/O
#else
# error "Invalid Gpio Sysctl Base"
#endif


#if defined(RTCONFIG_RALINK_MT7620) 
#define RALINK_REG_INTENA		(RALINK_IRQ_ADDR + 0x34)
#define RALINK_REG_INTDIS		(RALINK_IRQ_ADDR + 0x38)

#define RALINK_REG_PIOINT		(RALINK_PRGIO_ADDR + 0)
#define RALINK_REG_PIOEDGE		(RALINK_PRGIO_ADDR + 0x04)
#define RALINK_REG_PIORENA		(RALINK_PRGIO_ADDR + 0x08)
#define RALINK_REG_PIOFENA		(RALINK_PRGIO_ADDR + 0x0C)
#define RALINK_REG_PIODATA		(RALINK_PRGIO_ADDR + 0x20)
#define RALINK_REG_PIODIR		(RALINK_PRGIO_ADDR + 0x24)
#define RALINK_REG_PIOSET		(RALINK_PRGIO_ADDR + 0x2C)
#define RALINK_REG_PIORESET		(RALINK_PRGIO_ADDR + 0x30)

#elif defined(RTCONFIG_RALINK_MT7621)
#define RALINK_REG_INTENA               (RALINK_IRQ_ADDR   + 0x80)
#define RALINK_REG_INTDIS               (RALINK_IRQ_ADDR   + 0x78)
   
#define RALINK_REG_PIOINT               (RALINK_PRGIO_ADDR + 0x90)
#define RALINK_REG_PIOEDGE              (RALINK_PRGIO_ADDR + 0xA0)
#define RALINK_REG_PIORENA              (RALINK_PRGIO_ADDR + 0x50)
#define RALINK_REG_PIOFENA              (RALINK_PRGIO_ADDR + 0x60)
#define RALINK_REG_PIODATA              (RALINK_PRGIO_ADDR + 0x20)
#define RALINK_REG_PIODIR               (RALINK_PRGIO_ADDR + 0x00)
#define RALINK_REG_PIOSET               (RALINK_PRGIO_ADDR + 0x30)
#define RALINK_REG_PIORESET             (RALINK_PRGIO_ADDR + 0x40)
#else
# error "Invalid Gpio Reg"
#endif
/*
 * Values for the GPIOMODE Register
 */


#if defined(RTCONFIG_RALINK_MT7620) 
#define RALINK_GPIOMODE_I2C		0x01
#define RALINK_GPIOMODE_SPI		0x02
#define RALINK_GPIOMODE_UARTF		0x1C
#define RALINK_GPIOMODE_UARTL		0x20
#define RALINK_GPIOMODE_JTAG		0x40
#define RALINK_GPIOMODE_MDIO		0x80
#define RALINK_GPIOMODE_GE1		0x200
#define RALINK_GPIOMODE_LNA_G		0x40000
#define RALINK_GPIOMODE_PA_G		0x100000

#elif defined(RTCONFIG_RALINK_MT7621)
#define RALINK_GPIOMODE_UART1           0x02
#define RALINK_GPIOMODE_I2C             0x04
#define RALINK_GPIOMODE_UART3           0x08
#define RALINK_GPIOMODE_UART2           0x20
#define RALINK_GPIOMODE_JTAG            0x80
#define RALINK_GPIOMODE_WDT             0x100
#define RALINK_GPIOMODE_PERST           0x400
#define RALINK_GPIOMODE_MDIO            0x1000
#define RALINK_GPIOMODE_GE1             0x4000
#define RALINK_GPIOMODE_GE2             0x8000
#define RALINK_GPIOMODE_SPI             0x10000
#define RALINK_GPIOMODE_SDXC            0x40000
#define RALINK_GPIOMODE_ESWINT          0x100000
#else
# error "Invalid Gpio Mode"
#endif

/*
#define RALINK_GPIOMODE_PCM		0x01	// GPIO_PCM
#define RALINK_GPIOMODE_SPICS1		0x02	// GPIO_SPI_CS1_N
#define RALINK_GPIOMODE_SPICS2		0x04	// GPIO_SPI_CS2_N
#define RALINK_GPIOMODE_SPICS3		0x08	// GPIO_SPI_CS3_N
#define RALINK_GPIOMODE_SPICS4		0x10	// GPIO_SPI_CS4_N
#define RALINK_GPIOMODE_SPICS5		0x20	// GPIO_SPI_CS5_N
#define RALINK_GPIOMODE_UARTF		0x40	// GPIO_UART2
#define RALINK_GPIOMODE_I2S		0x80	// GPIO_I2S
#define RALINK_GPIOMODE_NFD		0x100	// GPIO_NFD_DATA_BUS
#define RALINK_GPIOMODE_EPHY0		0x200	// GPIO_EPHY_LED0
#define RALINK_GPIOMODE_EPHY1		0x400	// GPIO_EPHY_LED1
#define RALINK_GPIOMODE_EPHY2		0x800	// GPIO_EPHY_LED2
#define RALINK_GPIOMODE_EPHY3		0x1000	// GPIO_EPHY_LED3
#define RALINK_GPIOMODE_EPHY4		0x2000	// GPIO_EPHY_LED4
#define RALINK_GPIOMODE_I2C		0x8000	// 
*/
// if you would like to enable GPIO mode for other pins, please modify this value
// !! Warning: changing this value may make other features(MDIO, PCI, etc) lose efficacy

#if defined(RTCONFIG_RALINK_MT7620) 
#define RALINK_GPIOMODE_DFT		(RALINK_GPIOMODE_UARTF)
#elif defined(RTCONFIG_RALINK_MT7621)
#define RALINK_GPIOMODE_DFT             (RALINK_GPIOMODE_UART2 | RALINK_GPIOMODE_UART3 | RALINK_GPIOMODE_WDT)
#endif
/*
   MT7620 group 
 */

#if defined(RTCONFIG_RALINK_MT7620) 
//#define SUTIF_GPIOMODE		0x1e
#define WDT_RST_GPIOMODE	0x15
#define PA_G_GPIOMODE		0x14
#define ND_SD_GPIOMODE		0x12
#define PERST_GPIOMODE		0x10
#define EPHY_LED_GPIOMODE	0xf
#define WLED_GPIOMODE		0xd
#define SPI_REFCLK_GPIOMODE	0xc
#define SPI_GPIOMODE		0xb
#define RGMII2_GPIOMODE		0xa
#define RGMII1_GPIOMODE		0x9
#define MDIO_GPIOMODE		0x7
#define UARTL_GPIOMODE		0x5
//#define UARTF_GPIOMODE		0x2
#define I2C_GPIOMODE 		0x0 
#endif

/*
 * bit is the unit of length
 */
#if defined (CONFIG_RT63365_PCIE_PORT0_ENABLE)
#define RALINK_GPIO_NUMBER		33
#else
#if defined(RTCONFIG_RALINK_MT7620) 
#define RALINK_GPIO_NUMBER		72  //MT7620
#elif defined(RTCONFIG_RALINK_MT7621) 
#define RALINK_GPIO_NUMBER		95  //MT7621
#endif
#endif

#if defined(RTCONFIG_RALINK_MT7620) 
#define RALINK_GPIO_DATA_MASK		0x00FFFFFF
#define RALINK_GPIO_DATA_LEN		32
#define RALINK_GPIO_DIR_IN		0
#define RALINK_GPIO_DIR_OUT		1
#define RALINK_GPIO_DIR_ALLIN		0
#define RALINK_GPIO_DIR_ALLOUT		0x00FFFFFF
#elif defined(RTCONFIG_RALINK_MT7621) 
#define RALINK_GPIO_DATA_MASK           0xFFFFFFFF
#define RALINK_GPIO_DATA_LEN		32
#define RALINK_GPIO_DIR_IN              0
#define RALINK_GPIO_DIR_OUT             1
#define RALINK_GPIO_DIR_ALLIN           0
#define RALINK_GPIO_DIR_ALLOUT          0xFFFFFFFF
#endif
/*
 * structure used at regsitration
 */
typedef struct {
	unsigned int irq;		//request irq pin number
	pid_t pid;			//process id to notify
} ralink_gpio_reg_info;

#define RALINK_GPIO_LED_INFINITY	4000
typedef struct {
	int gpio;			//gpio number (0 ~ 23)
	unsigned int on;		//interval of led on
	unsigned int off;		//interval of led off
	unsigned int blinks;		//number of blinking cycles
	unsigned int rests;		//number of break cycles
	unsigned int times;		//blinking times
} ralink_gpio_led_info;


#define RALINK_GPIO(x)			(1 << x)

#endif


#ifndef __RALINK_MT7620_ESW_H__
#define __RALINK_MT7620_ESW_H__

#if defined(RTCONFIG_RALINK_MT7620) || defined(RTCONFIG_RALINK_MT7621)
#define REG_ESW_PORT_PCR_P0		0x2004
#define REG_ESW_PORT_PVC_P0		0x2010
#define REG_ESW_PORT_PPBV1_P0		0x2014
#define REG_ESW_PORT_BSR_P0		0x201c
#define REG_ESW_MAC_PMSR_P0		0x3008
#define REG_ESW_MAC_PMCR_P6		0x3600
#define REG_ESW_PORT_TGOCN_P0		0x4018
#define REG_ESW_PORT_RGOCN_P0		0x4028

/* for ATE Get_WanLanStatus command */

#if defined(RTCONFIG_RALINK_MT7620) 
#define MAX_PORT 5
#elif defined(RTCONFIG_RALINK_MT7621)
#define MAX_PORT 6
#endif
typedef struct {
	unsigned int link[MAX_PORT];
	unsigned int speed[MAX_PORT];
} phyState;
#endif

#endif
