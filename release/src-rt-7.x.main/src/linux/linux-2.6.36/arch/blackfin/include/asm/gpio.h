/*
 * Copyright 2006-2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __ARCH_BLACKFIN_GPIO_H__
#define __ARCH_BLACKFIN_GPIO_H__

#define gpio_bank(x)	((x) >> 4)
#define gpio_bit(x)	(1<<((x) & 0xF))
#define gpio_sub_n(x)	((x) & 0xF)

#define GPIO_BANKSIZE	16
#define GPIO_BANK_NUM	DIV_ROUND_UP(MAX_BLACKFIN_GPIOS, GPIO_BANKSIZE)

#include <mach/gpio.h>

#define GPIO_0	0
#define GPIO_1	1
#define GPIO_2	2
#define GPIO_3	3
#define GPIO_4	4
#define GPIO_5	5
#define GPIO_6	6
#define GPIO_7	7
#define GPIO_8	8
#define GPIO_9	9
#define GPIO_10	10
#define GPIO_11	11
#define GPIO_12	12
#define GPIO_13	13
#define GPIO_14	14
#define GPIO_15	15
#define GPIO_16	16
#define GPIO_17	17
#define GPIO_18	18
#define GPIO_19	19
#define GPIO_20	20
#define GPIO_21	21
#define GPIO_22	22
#define GPIO_23	23
#define GPIO_24	24
#define GPIO_25	25
#define GPIO_26	26
#define GPIO_27	27
#define GPIO_28	28
#define GPIO_29	29
#define GPIO_30	30
#define GPIO_31	31
#define GPIO_32	32
#define GPIO_33	33
#define GPIO_34	34
#define GPIO_35	35
#define GPIO_36	36
#define GPIO_37	37
#define GPIO_38	38
#define GPIO_39	39
#define GPIO_40	40
#define GPIO_41	41
#define GPIO_42	42
#define GPIO_43	43
#define GPIO_44	44
#define GPIO_45	45
#define GPIO_46	46
#define GPIO_47	47

#define PERIPHERAL_USAGE 1
#define GPIO_USAGE 0

#ifndef __ASSEMBLY__

#include <linux/compiler.h>

/***********************************************************
*
* FUNCTIONS: Blackfin General Purpose Ports Access Functions
*
* INPUTS/OUTPUTS:
* gpio - GPIO Number between 0 and MAX_BLACKFIN_GPIOS
*
*
* DESCRIPTION: These functions abstract direct register access
*              to Blackfin processor General Purpose
*              Ports Regsiters
*
* CAUTION: These functions do not belong to the GPIO Driver API
*************************************************************
* MODIFICATION HISTORY :
**************************************************************/

#ifndef CONFIG_BF54x
void set_gpio_dir(unsigned, unsigned short);
void set_gpio_inen(unsigned, unsigned short);
void set_gpio_polar(unsigned, unsigned short);
void set_gpio_edge(unsigned, unsigned short);
void set_gpio_both(unsigned, unsigned short);
void set_gpio_data(unsigned, unsigned short);
void set_gpio_maska(unsigned, unsigned short);
void set_gpio_maskb(unsigned, unsigned short);
void set_gpio_toggle(unsigned);
void set_gpiop_dir(unsigned, unsigned short);
void set_gpiop_inen(unsigned, unsigned short);
void set_gpiop_polar(unsigned, unsigned short);
void set_gpiop_edge(unsigned, unsigned short);
void set_gpiop_both(unsigned, unsigned short);
void set_gpiop_data(unsigned, unsigned short);
void set_gpiop_maska(unsigned, unsigned short);
void set_gpiop_maskb(unsigned, unsigned short);
unsigned short get_gpio_dir(unsigned);
unsigned short get_gpio_inen(unsigned);
unsigned short get_gpio_polar(unsigned);
unsigned short get_gpio_edge(unsigned);
unsigned short get_gpio_both(unsigned);
unsigned short get_gpio_maska(unsigned);
unsigned short get_gpio_maskb(unsigned);
unsigned short get_gpio_data(unsigned);
unsigned short get_gpiop_dir(unsigned);
unsigned short get_gpiop_inen(unsigned);
unsigned short get_gpiop_polar(unsigned);
unsigned short get_gpiop_edge(unsigned);
unsigned short get_gpiop_both(unsigned);
unsigned short get_gpiop_maska(unsigned);
unsigned short get_gpiop_maskb(unsigned);
unsigned short get_gpiop_data(unsigned);

struct gpio_port_t {
	unsigned short data;
	unsigned short dummy1;
	unsigned short data_clear;
	unsigned short dummy2;
	unsigned short data_set;
	unsigned short dummy3;
	unsigned short toggle;
	unsigned short dummy4;
	unsigned short maska;
	unsigned short dummy5;
	unsigned short maska_clear;
	unsigned short dummy6;
	unsigned short maska_set;
	unsigned short dummy7;
	unsigned short maska_toggle;
	unsigned short dummy8;
	unsigned short maskb;
	unsigned short dummy9;
	unsigned short maskb_clear;
	unsigned short dummy10;
	unsigned short maskb_set;
	unsigned short dummy11;
	unsigned short maskb_toggle;
	unsigned short dummy12;
	unsigned short dir;
	unsigned short dummy13;
	unsigned short polar;
	unsigned short dummy14;
	unsigned short edge;
	unsigned short dummy15;
	unsigned short both;
	unsigned short dummy16;
	unsigned short inen;
};
#endif

#ifdef BFIN_SPECIAL_GPIO_BANKS
void bfin_special_gpio_free(unsigned gpio);
int bfin_special_gpio_request(unsigned gpio, const char *label);
#endif

#ifdef CONFIG_PM
int bfin_pm_standby_ctrl(unsigned ctrl);

static inline int bfin_pm_standby_setup(void)
{
	return bfin_pm_standby_ctrl(1);
}

static inline void bfin_pm_standby_restore(void)
{
	bfin_pm_standby_ctrl(0);
}

void bfin_gpio_pm_hibernate_restore(void);
void bfin_gpio_pm_hibernate_suspend(void);

#ifndef CONFIG_BF54x
int gpio_pm_wakeup_ctrl(unsigned gpio, unsigned ctrl);

struct gpio_port_s {
	unsigned short data;
	unsigned short maska;
	unsigned short maskb;
	unsigned short dir;
	unsigned short polar;
	unsigned short edge;
	unsigned short both;
	unsigned short inen;

	unsigned short fer;
	unsigned short reserved;
	unsigned short mux;
};
#endif /*CONFIG_BF54x*/
#endif /*CONFIG_PM*/
/***********************************************************
*
* FUNCTIONS: Blackfin GPIO Driver
*
* INPUTS/OUTPUTS:
* gpio - GPIO Number between 0 and MAX_BLACKFIN_GPIOS
*
*
* DESCRIPTION: Blackfin GPIO Driver API
*
* CAUTION:
*************************************************************
* MODIFICATION HISTORY :
**************************************************************/

int bfin_gpio_request(unsigned gpio, const char *label);
void bfin_gpio_free(unsigned gpio);
int bfin_gpio_irq_request(unsigned gpio, const char *label);
void bfin_gpio_irq_free(unsigned gpio);
int bfin_gpio_direction_input(unsigned gpio);
int bfin_gpio_direction_output(unsigned gpio, int value);
int bfin_gpio_get_value(unsigned gpio);
void bfin_gpio_set_value(unsigned gpio, int value);

#include <asm/irq.h>
#include <asm/errno.h>

#ifdef CONFIG_GPIOLIB
#include <asm-generic/gpio.h>		/* cansleep wrappers */

static inline int gpio_get_value(unsigned int gpio)
{
	if (gpio < MAX_BLACKFIN_GPIOS)
		return bfin_gpio_get_value(gpio);
	else
		return __gpio_get_value(gpio);
}

static inline void gpio_set_value(unsigned int gpio, int value)
{
	if (gpio < MAX_BLACKFIN_GPIOS)
		bfin_gpio_set_value(gpio, value);
	else
		__gpio_set_value(gpio, value);
}

static inline int gpio_cansleep(unsigned int gpio)
{
	return __gpio_cansleep(gpio);
}

static inline int gpio_to_irq(unsigned gpio)
{
	return __gpio_to_irq(gpio);
}

#else /* !CONFIG_GPIOLIB */

static inline int gpio_request(unsigned gpio, const char *label)
{
	return bfin_gpio_request(gpio, label);
}

static inline void gpio_free(unsigned gpio)
{
	return bfin_gpio_free(gpio);
}

static inline int gpio_direction_input(unsigned gpio)
{
	return bfin_gpio_direction_input(gpio);
}

static inline int gpio_direction_output(unsigned gpio, int value)
{
	return bfin_gpio_direction_output(gpio, value);
}

static inline int gpio_set_debounce(unsigned gpio, unsigned debounce)
{
	return -EINVAL;
}

static inline int gpio_get_value(unsigned gpio)
{
	return bfin_gpio_get_value(gpio);
}

static inline void gpio_set_value(unsigned gpio, int value)
{
	return bfin_gpio_set_value(gpio, value);
}

static inline int gpio_to_irq(unsigned gpio)
{
	if (likely(gpio < MAX_BLACKFIN_GPIOS))
		return gpio + GPIO_IRQ_BASE;

	return -EINVAL;
}

#include <asm-generic/gpio.h>		/* cansleep wrappers */
#endif	/* !CONFIG_GPIOLIB */

static inline int irq_to_gpio(unsigned irq)
{
	return (irq - GPIO_IRQ_BASE);
}

#endif /* __ASSEMBLY__ */

#endif /* __ARCH_BLACKFIN_GPIO_H__ */
