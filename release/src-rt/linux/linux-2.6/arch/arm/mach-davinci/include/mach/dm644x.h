/*
 * This file contains the processor specific definitions
 * of the TI DM644x.
 *
 * Copyright (C) 2008 Texas Instruments.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef __ASM_ARCH_DM644X_H
#define __ASM_ARCH_DM644X_H

#include <linux/davinci_emac.h>
#include <mach/hardware.h>
#include <mach/asp.h>
#include <media/davinci/vpfe_capture.h>

#define DM644X_EMAC_BASE		(0x01C80000)
#define DM644X_EMAC_MDIO_BASE		(DM644X_EMAC_BASE + 0x4000)
#define DM644X_EMAC_CNTRL_OFFSET	(0x0000)
#define DM644X_EMAC_CNTRL_MOD_OFFSET	(0x1000)
#define DM644X_EMAC_CNTRL_RAM_OFFSET	(0x2000)
#define DM644X_EMAC_CNTRL_RAM_SIZE	(0x2000)

#define DM644X_ASYNC_EMIF_CONTROL_BASE	0x01E00000
#define DM644X_ASYNC_EMIF_DATA_CE0_BASE 0x02000000
#define DM644X_ASYNC_EMIF_DATA_CE1_BASE 0x04000000
#define DM644X_ASYNC_EMIF_DATA_CE2_BASE 0x06000000
#define DM644X_ASYNC_EMIF_DATA_CE3_BASE 0x08000000

void __init dm644x_init(void);
void __init dm644x_init_asp(struct snd_platform_data *pdata);
void dm644x_set_vpfe_config(struct vpfe_config *cfg);

#endif /* __ASM_ARCH_DM644X_H */
