/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2007, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************

    Module Name:
	rt3090.h

    Abstract:

    Revision History:
    Who          When          What
    ---------    ----------    ----------------------------------------------
 */

#ifndef __RT3090_H__
#define __RT3090_H__

#ifdef RT3090

#ifndef RTMP_PCI_SUPPORT
#error "For RT3090, you should define the compile flag -DRTMP_PCI_SUPPORT"
#endif

#ifndef RTMP_MAC_PCI
#error "For RT3090, you should define the compile flag -DRTMP_MAC_PCI"
#endif

#ifndef RTMP_RF_RW_SUPPORT
#error "For RT3090, you should define the compile flag -DRTMP_RF_RW_SUPPORT"
#endif

#ifndef RT30xx
#error "For RT3090, you should define the compile flag -DRT30xx"
#endif

#define PCIE_PS_SUPPORT

#include "mac_pci.h"
#include "rt30xx.h"

/* */
/* Device ID & Vendor ID, these values should match EEPROM value */
/* */
#define NIC3090_PCIe_DEVICE_ID  0x3090	/* 1T/1R miniCard */
#define NIC3091_PCIe_DEVICE_ID  0x3091	/* 1T/2R miniCard */
#define NIC3092_PCIe_DEVICE_ID  0x3092	/* 2T/2R miniCard */

#endif /* RT3090 // */

#endif /*__RT3090_H__ // */
