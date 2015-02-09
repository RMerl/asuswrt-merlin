/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  SMBUS Constants                          File: sb1250_smbus.h
    *  
    *  This module contains constants and macros useful for 
    *  manipulating the SB1250's SMbus devices.
    *  
    *  SB1250 specification level:  01/02/2002
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


#ifndef _SB1250_SMBUS_H
#define _SB1250_SMBUS_H

#include "sb1250_defs.h"

/*
 * SMBus Clock Frequency Register (Table 14-2)
 */

#define S_SMB_FREQ_DIV              0
#define M_SMB_FREQ_DIV              _SB_MAKEMASK(13,S_SMB_FREQ_DIV)
#define V_SMB_FREQ_DIV(x)           _SB_MAKEVALUE(x,S_SMB_FREQ_DIV)

#define K_SMB_FREQ_400KHZ	    0x1F
#define K_SMB_FREQ_100KHZ	    0x7D

#define S_SMB_CMD                   0
#define M_SMB_CMD                   _SB_MAKEMASK(8,S_SMB_CMD)
#define V_SMB_CMD(x)                _SB_MAKEVALUE(x,S_SMB_CMD)

/*
 * SMBus control register (Table 14-4)
 */

#define M_SMB_ERR_INTR              _SB_MAKEMASK1(0)
#define M_SMB_FINISH_INTR           _SB_MAKEMASK1(1)
#define M_SMB_DATA_OUT              _SB_MAKEMASK1(4)
#define M_SMB_DATA_DIR              _SB_MAKEMASK1(5)
#define M_SMB_DATA_DIR_OUTPUT       M_SMB_DATA_DIR
#define M_SMB_CLK_OUT               _SB_MAKEMASK1(6)
#define M_SMB_DIRECT_ENABLE         _SB_MAKEMASK1(7)

/*
 * SMBus status registers (Table 14-5)
 */

#define M_SMB_BUSY                  _SB_MAKEMASK1(0)
#define M_SMB_ERROR                 _SB_MAKEMASK1(1)
#define M_SMB_ERROR_TYPE            _SB_MAKEMASK1(2)
#define M_SMB_REF                   _SB_MAKEMASK1(6)
#define M_SMB_DATA_IN               _SB_MAKEMASK1(7)

/*
 * SMBus Start/Command registers (Table 14-9)
 */

#define S_SMB_ADDR                  0
#define M_SMB_ADDR                  _SB_MAKEMASK(7,S_SMB_ADDR)
#define V_SMB_ADDR(x)               _SB_MAKEVALUE(x,S_SMB_ADDR)
#define G_SMB_ADDR(x)               _SB_GETVALUE(x,S_SMB_ADDR,M_SMB_ADDR)

#define M_SMB_QDATA                 _SB_MAKEMASK1(7)

#define S_SMB_TT                    8
#define M_SMB_TT                    _SB_MAKEMASK(3,S_SMB_TT)
#define V_SMB_TT(x)                 _SB_MAKEVALUE(x,S_SMB_TT)
#define G_SMB_TT(x)                 _SB_GETVALUE(x,S_SMB_TT,M_SMB_TT)

#define K_SMB_TT_WR1BYTE            0
#define K_SMB_TT_WR2BYTE            1
#define K_SMB_TT_WR3BYTE            2
#define K_SMB_TT_CMD_RD1BYTE        3
#define K_SMB_TT_CMD_RD2BYTE        4
#define K_SMB_TT_RD1BYTE            5
#define K_SMB_TT_QUICKCMD           6
#define K_SMB_TT_EEPROMREAD         7

#define V_SMB_TT_WR1BYTE	    V_SMB_TT(K_SMB_TT_WR1BYTE)
#define V_SMB_TT_WR2BYTE	    V_SMB_TT(K_SMB_TT_WR2BYTE)
#define V_SMB_TT_WR3BYTE	    V_SMB_TT(K_SMB_TT_WR3BYTE)
#define V_SMB_TT_CMD_RD1BYTE	    V_SMB_TT(K_SMB_TT_CMD_RD1BYTE)
#define V_SMB_TT_CMD_RD2BYTE	    V_SMB_TT(K_SMB_TT_CMD_RD2BYTE)
#define V_SMB_TT_RD1BYTE	    V_SMB_TT(K_SMB_TT_RD1BYTE)
#define V_SMB_TT_QUICKCMD	    V_SMB_TT(K_SMB_TT_QUICKCMD)
#define V_SMB_TT_EEPROMREAD	    V_SMB_TT(K_SMB_TT_EEPROMREAD)

#define M_SMB_PEC                   _SB_MAKEMASK1(15)

/*
 * SMBus Data Register (Table 14-6) and SMBus Extra Register (Table 14-7)
 */

#define S_SMB_LB                    0
#define M_SMB_LB                    _SB_MAKEMASK(8,S_SMB_LB)
#define V_SMB_LB(x)                 _SB_MAKEVALUE(x,S_SMB_LB)

#define S_SMB_MB                    8
#define M_SMB_MB                    _SB_MAKEMASK(8,S_SMB_MB)
#define V_SMB_MB(x)                 _SB_MAKEVALUE(x,S_SMB_MB)


/*
 * SMBus Packet Error Check register (Table 14-8)
 */

#define S_SPEC_PEC                  0
#define M_SPEC_PEC                  _SB_MAKEMASK(8,S_SPEC_PEC)
#define V_SPEC_MB(x)                _SB_MAKEVALUE(x,S_SPEC_PEC)


#if SIBYTE_HDR_FEATURE(1250, PASS2) || SIBYTE_HDR_FEATURE(112x, PASS1)

#define S_SMB_CMDH                  8
#define M_SMB_CMDH                  _SB_MAKEMASK(8,S_SMBH_CMD)
#define V_SMB_CMDH(x)               _SB_MAKEVALUE(x,S_SMBH_CMD)

#define M_SMB_EXTEND		    _SB_MAKEMASK1(14)

#define M_SMB_DIR		    _SB_MAKEMASK1(13)

#define S_SMB_DFMT                  8
#define M_SMB_DFMT                  _SB_MAKEMASK(3,S_SMB_DFMT)
#define V_SMB_DFMT(x)               _SB_MAKEVALUE(x,S_SMB_DFMT)
#define G_SMB_DFMT(x)               _SB_GETVALUE(x,S_SMB_DFMT,M_SMB_DFMT)

#define K_SMB_DFMT_1BYTE            0
#define K_SMB_DFMT_2BYTE            1
#define K_SMB_DFMT_3BYTE            2
#define K_SMB_DFMT_4BYTE            3
#define K_SMB_DFMT_NODATA           4
#define K_SMB_DFMT_CMD4BYTE         5
#define K_SMB_DFMT_CMD5BYTE         6
#define K_SMB_DFMT_RESERVED         7

#define V_SMB_DFMT_1BYTE	    V_SMB_DFMT(K_SMB_DFMT_1BYTE)
#define V_SMB_DFMT_2BYTE	    V_SMB_DFMT(K_SMB_DFMT_2BYTE)
#define V_SMB_DFMT_3BYTE	    V_SMB_DFMT(K_SMB_DFMT_3BYTE)
#define V_SMB_DFMT_4BYTE	    V_SMB_DFMT(K_SMB_DFMT_4BYTE)
#define V_SMB_DFMT_NODATA	    V_SMB_DFMT(K_SMB_DFMT_NODATA)
#define V_SMB_DFMT_CMD4BYTE	    V_SMB_DFMT(K_SMB_DFMT_CMD4BYTE)
#define V_SMB_DFMT_CMD5BYTE	    V_SMB_DFMT(K_SMB_DFMT_CMD5BYTE)
#define V_SMB_DFMT_RESERVED	    V_SMB_DFMT(K_SMB_DFMT_RESERVED)

#endif /* 1250 PASS2 || 112x PASS1 */

#endif
