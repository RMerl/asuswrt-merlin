/*
 * Copyright 2007 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2009 Daniel Mack <daniel@caiaq.de>
 *
 * Based on code for mobots boards,
 *   Copyright (C) 2009 Valentin Longchamp, EPFL Mobots group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef __ASM_ARCH_MXC_BOARD_MX31LITE_H__
#define __ASM_ARCH_MXC_BOARD_MX31LITE_H__

#ifndef __ASSEMBLY__

enum mx31lite_boards {
	MX31LITE_NOBOARD	= 0,
	MX31LITE_DB		= 1,
};

/*
 * This CPU module needs a baseboard to work. After basic initializing
 * its own devices, it calls the baseboard's init function.
 */

extern void mx31lite_db_init(void);

#endif

#endif /* __ASM_ARCH_MXC_BOARD_MX31LITE_H__ */
