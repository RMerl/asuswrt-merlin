#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bcmnvram.h>

#include "utils.h"
#include "shutils.h"
#include "shared.h"


#ifdef LINUX26
#define GPIO_IOCTL
#endif

// --- move begin ---
#ifdef GPIO_IOCTL

#include <sys/ioctl.h>
#include <linux_gpio.h>

static int _gpio_ioctl(int f, int gpioreg, unsigned int mask, unsigned int val)
{
	struct gpio_ioctl gpio;
                                                                                                                     
	gpio.val = val;
	gpio.mask = mask;

	if (ioctl(f, gpioreg, &gpio) < 0) {
		_dprintf("Invalid gpioreg %d\n", gpioreg);
		return -1;
	}
	return (gpio.val);
}

static int _gpio_open()
{
	int f = open("/dev/gpio", O_RDWR);
	if (f < 0)
		_dprintf ("Failed to open /dev/gpio\n");
	return f;
}

int gpio_open(uint32_t mask)
{
	uint32_t bit;
	int i;
	int f = _gpio_open();

	if ((f >= 0) && mask) {
		for (i = 0; i <= 15; i++) {
			bit = 1 << i;
			if ((mask & bit) == bit) {
				_gpio_ioctl(f, GPIO_IOC_RESERVE, bit, bit);
				_gpio_ioctl(f, GPIO_IOC_OUTEN, bit, 0);
			}
		}
		close(f);
		f = _gpio_open();
	}

	return f;
}

void gpio_write(uint32_t bitvalue, int en)
{
	int f;
	uint32_t bit;

	bit = 1<< bitvalue;

	if ((f = gpio_open(0)) < 0) return;

	_gpio_ioctl(f, GPIO_IOC_RESERVE, bit, bit);
	_gpio_ioctl(f, GPIO_IOC_OUTEN, bit, bit);
	_gpio_ioctl(f, GPIO_IOC_OUT, bit, en ? bit : 0);
	close(f);
}

uint32_t _gpio_read(int f)
{
	uint32_t r;
//	r = _gpio_ioctl(f, GPIO_IOC_IN, 0xFFFF, 0);
	r = _gpio_ioctl(f, GPIO_IOC_IN, 0x07FF, 0);
	if (r < 0) r = ~0;
	return r;
}

uint32_t gpio_read(void)
{
	int f;
	uint32_t r;

	if ((f = gpio_open(0)) < 0) return ~0;
	r = _gpio_read(f);
	close(f);
	return r;
}

#else

int gpio_open(uint32_t mask)
{
	int f = open(DEV_GPIO(in), O_RDONLY|O_SYNC);
	if (f < 0)
		_dprintf ("Failed to open %s\n", DEV_GPIO(in));
	return f;
}

void gpio_write(uint32_t bitvalue, int en)
{
	int f;
	uint32_t r;
	uint32_t bit;

	bit = 1<<bitvalue;

	if ((f = open(DEV_GPIO(control), O_RDWR)) < 0) return;
	read(f, &r, sizeof(r));
	r &= ~bit;
	write(f, &r, sizeof(r));
	close(f);

	if ((f = open(DEV_GPIO(outen), O_RDWR)) < 0) return;
	read(f, &r, sizeof(r));
	r |= bit;
	write(f, &r, sizeof(r));
	close(f);

	if ((f = open(DEV_GPIO(out), O_RDWR)) < 0) return;
	read(f, &r, sizeof(r));
	if (en) r |= bit;
		else r &= ~bit;
	write(f, &r, sizeof(r));
	close(f);
}

uint32_t _gpio_read(int f)
{
	uint32_t v;
	return (read(f, &v, sizeof(v)) == sizeof(v)) ? v : ~0;
}

uint32_t gpio_read(void)
{
	int f;
	uint32_t r;

	if ((f = open(DEV_GPIO(in), O_RDONLY)) < 0) return ~0;
	r = _gpio_read(f);
	close(f);
	return r;
}


#endif

