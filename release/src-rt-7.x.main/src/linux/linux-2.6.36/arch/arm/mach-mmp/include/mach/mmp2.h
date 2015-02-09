#ifndef __ASM_MACH_MMP2_H
#define __ASM_MACH_MMP2_H

struct sys_timer;

extern struct sys_timer mmp2_timer;
extern void __init mmp2_init_icu(void);
extern void __init mmp2_init_irq(void);
extern void mmp2_clear_pmic_int(void);

#include <linux/i2c.h>
#include <mach/devices.h>
#include <plat/i2c.h>

extern struct pxa_device_desc mmp2_device_uart1;
extern struct pxa_device_desc mmp2_device_uart2;
extern struct pxa_device_desc mmp2_device_uart3;
extern struct pxa_device_desc mmp2_device_uart4;
extern struct pxa_device_desc mmp2_device_twsi1;
extern struct pxa_device_desc mmp2_device_twsi2;
extern struct pxa_device_desc mmp2_device_twsi3;
extern struct pxa_device_desc mmp2_device_twsi4;
extern struct pxa_device_desc mmp2_device_twsi5;
extern struct pxa_device_desc mmp2_device_twsi6;

static inline int mmp2_add_uart(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1: d = &mmp2_device_uart1; break;
	case 2: d = &mmp2_device_uart2; break;
	case 3: d = &mmp2_device_uart3; break;
	case 4: d = &mmp2_device_uart4; break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}

static inline int mmp2_add_twsi(int id, struct i2c_pxa_platform_data *data,
				  struct i2c_board_info *info, unsigned size)
{
	struct pxa_device_desc *d = NULL;
	int ret;

	switch (id) {
	case 1: d = &mmp2_device_twsi1; break;
	case 2: d = &mmp2_device_twsi2; break;
	case 3: d = &mmp2_device_twsi3; break;
	case 4: d = &mmp2_device_twsi4; break;
	case 5: d = &mmp2_device_twsi5; break;
	case 6: d = &mmp2_device_twsi6; break;
	default:
		return -EINVAL;
	}

	ret = i2c_register_board_info(id - 1, info, size);
	if (ret)
		return ret;

	return pxa_register_device(d, data, sizeof(*data));
}

#endif /* __ASM_MACH_MMP2_H */
