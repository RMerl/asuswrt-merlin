/* arch/arm/mach-s3c2410/include/mach/gpio-nrs.h
 *
 * Copyright (c) 2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 - GPIO bank numbering
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __MACH_GPIONRS_H
#define __MACH_GPIONRS_H

#define S3C2410_GPIONO(bank,offset) ((bank) + (offset))

#define S3C2410_GPIO_BANKG   (32*6)
#define S3C2410_GPIO_BANKH   (32*7)

/* GPIO sizes for various SoCs:
 *
 *             2442
 *   2410 2412 2440 2443 2416
 *   ---- ---- ---- ---- ----
 * A 23   22   25   16   25
 * B 11   11   11   11   9
 * C 16   15   16   16   16
 * D 16   16   16   16   16
 * E 16   16   16   16   16
 * F 8    8    8    8    8
 * G 16   16   16   16   8
 * H 11   11   9    15   15
 * J --   --   13   16   --
 * K --   --   --   --   16
 * L --   --   --   15   7
 * M --   --   --   2    2
 */

/* GPIO bank sizes */
#define S3C2410_GPIO_A_NR	(32)
#define S3C2410_GPIO_B_NR	(32)
#define S3C2410_GPIO_C_NR	(32)
#define S3C2410_GPIO_D_NR	(32)
#define S3C2410_GPIO_E_NR	(32)
#define S3C2410_GPIO_F_NR	(32)
#define S3C2410_GPIO_G_NR	(32)
#define S3C2410_GPIO_H_NR	(32)
#define S3C2410_GPIO_J_NR	(32)	/* technically 16. */
#define S3C2410_GPIO_K_NR	(32)	/* technically 16. */
#define S3C2410_GPIO_L_NR	(32)	/* technically 15. */
#define S3C2410_GPIO_M_NR	(32)	/* technically 2. */

#if CONFIG_S3C_GPIO_SPACE != 0
#error CONFIG_S3C_GPIO_SPACE cannot be zero at the moment
#endif

#define S3C2410_GPIO_NEXT(__gpio) \
	((__gpio##_START) + (__gpio##_NR) + CONFIG_S3C_GPIO_SPACE + 0)

#ifndef __ASSEMBLY__

enum s3c_gpio_number {
	S3C2410_GPIO_A_START = 0,
	S3C2410_GPIO_B_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_A),
	S3C2410_GPIO_C_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_B),
	S3C2410_GPIO_D_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_C),
	S3C2410_GPIO_E_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_D),
	S3C2410_GPIO_F_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_E),
	S3C2410_GPIO_G_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_F),
	S3C2410_GPIO_H_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_G),
	S3C2410_GPIO_J_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_H),
	S3C2410_GPIO_K_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_J),
	S3C2410_GPIO_L_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_K),
	S3C2410_GPIO_M_START = S3C2410_GPIO_NEXT(S3C2410_GPIO_L),
};

#endif /* __ASSEMBLY__ */

/* S3C2410 GPIO number definitions. */

#define S3C2410_GPA(_nr)	(S3C2410_GPIO_A_START + (_nr))
#define S3C2410_GPB(_nr)	(S3C2410_GPIO_B_START + (_nr))
#define S3C2410_GPC(_nr)	(S3C2410_GPIO_C_START + (_nr))
#define S3C2410_GPD(_nr)	(S3C2410_GPIO_D_START + (_nr))
#define S3C2410_GPE(_nr)	(S3C2410_GPIO_E_START + (_nr))
#define S3C2410_GPF(_nr)	(S3C2410_GPIO_F_START + (_nr))
#define S3C2410_GPG(_nr)	(S3C2410_GPIO_G_START + (_nr))
#define S3C2410_GPH(_nr)	(S3C2410_GPIO_H_START + (_nr))
#define S3C2410_GPJ(_nr)	(S3C2410_GPIO_J_START + (_nr))
#define S3C2410_GPK(_nr)	(S3C2410_GPIO_K_START + (_nr))
#define S3C2410_GPL(_nr)	(S3C2410_GPIO_L_START + (_nr))
#define S3C2410_GPM(_nr)	(S3C2410_GPIO_M_START + (_nr))

/* compatibility until drivers can be modified */

#define S3C2410_GPA0	S3C2410_GPA(0)
#define S3C2410_GPA1	S3C2410_GPA(1)
#define S3C2410_GPA3	S3C2410_GPA(3)
#define S3C2410_GPA7	S3C2410_GPA(7)

#define S3C2410_GPE0	S3C2410_GPE(0)
#define S3C2410_GPE1	S3C2410_GPE(1)
#define S3C2410_GPE2	S3C2410_GPE(2)
#define S3C2410_GPE3	S3C2410_GPE(3)
#define S3C2410_GPE4	S3C2410_GPE(4)
#define S3C2410_GPE5	S3C2410_GPE(5)
#define S3C2410_GPE6	S3C2410_GPE(6)
#define S3C2410_GPE7	S3C2410_GPE(7)
#define S3C2410_GPE8	S3C2410_GPE(8)
#define S3C2410_GPE9	S3C2410_GPE(9)
#define S3C2410_GPE10	S3C2410_GPE(10)

#define S3C2410_GPH10	S3C2410_GPH(10)

#endif /* __MACH_GPIONRS_H */
