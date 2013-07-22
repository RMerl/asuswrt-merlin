/*
** Copyright 2000, 2001  Broadcom Corporation
** All Rights Reserved
**
** No portions of this material may be reproduced in any form 
** without the written permission of:
**
**   Broadcom Corporation
**   16215 Alton Parkway
**   P.O. Box 57013
**   Irvine, CA  92619-7013
**
** All information contained in this document is Broadcom 
** Corporation company private proprietary, and trade secret.
**
** ----------------------------------------------------------
**
**
**  $Id:: ddr40_phy_custom.h 1306 2012-06-21 14:10:10Z jeung      $:
**  $Rev::file =  : Global SVN Revision = 1780                    $:
**
*/

#ifndef __DDR40_PHY_CUSTOM_H__
#define __DDR40_PHY_CUSTOM_H__


#ifndef DDR40_TYPES__
#define DDR40_TYPES__
typedef unsigned long	ddr40_addr_t;
typedef uint32	uint32_t;
typedef uint32	ddr40_uint32_t;
#endif /* DDR40_TYPES__ */

#define  DDR40_PHY_RegWr(addr, value)	tb_w(addr, value)
#define  DDR40_PHY_RegRd(addr)		tb_r(addr)
#define  DDR40_PHY_Print(args...)	print_log(args)
#define  DDR40_PHY_Error(args...)	error_log(args)
#define  DDR40_PHY_Fatal(args...)	fatal_log(args)
#define  DDR40_PHY_Timeout(args...)	OSL_DELAY(args)

#define FUNC_PROTOTYPE_PREFIX inline
#define FUNC_PROTOTYPE_SUFFIX
#define FUNC_PREFIX inline
#define FUNC_SUFFIX
#endif /* __DDR40_PHY_CUSTOM_H__ */


/*
**
** $Log: $
**
*/
