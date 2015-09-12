/*
 *  Copyright 2000,2001
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
 *     and retain this copyright notice and list of conditions as 
 *     they appear in the source file.
 *  
 *  2) No right is granted to use any trade name, trademark, or 
 *     logo of Broadcom Corporation. Neither the "Broadcom 
 *     Corporation" name nor any trademark or logo of Broadcom 
 *     Corporation may be used to endorse or promote products 
 *     derived from this software without the prior written 
 *     permission of Broadcom Corporation.
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
 */

#ifndef _LDTREG_H_
#define _LDTREG_H_

/*
 * LDT capability register definitions and macros.
 * Derived from Revison 0.17 of the LDT (now HyperTransport)
 * Specification with some 1.02/1.03 features.
 *
 * Note: Register and field definitions assume 32-bit register accesses.
 */

/*
 * LDT Capability registers (identified by offsets within the capability)
 */

/*
 * Command Register; contains command and capability fields.
 */
#define LDT_COMMAND_CAP_OFF    		0x00

#define LDT_CAP_ID_MASK          		0x000000ff
#define LDT_CAP_ID_SHIFT         		0
#define LDT_CAP_PTR_MASK         		0x0000ff00
#define LDT_CAP_PTR_SHIFT        		8

#define LDT_COMMAND_MASK			0xffff0000
#define LDT_COMMAND_SHIFT			16
#define LDT_COMMAND(cmd) \
	(((cmd) & LDT_COMMAND_MASK) >> LDT_COMMAND_SHIFT)
#define LDT_COMMAND_TYPE_MASK            	0xe0000000
#define LDT_COMMAND_TYPE_SHIFT           	(16+13)
#define LDT_COMMAND_TYPE(cmd) \
	(((cmd) & LDT_COMMAND_TYPE_MASK) >> LDT_COMMAND_TYPE_SHIFT)
#define LDT_COMMAND_TYPE_SLAVE           	0x0
#define LDT_COMMAND_TYPE_HOST            	0x1
#define LDT_COMMAND_TYPE_IDC                    0x4
#define LDT_COMMAND_TYPE_AMAP                   0x5
/* Following for 1.0x only */
#define LDT_COMMAND_DROP_ON_INIT                0x10000000

/*
 * An LDT capability for type Slave (aka Primary, aka "tunnel") consists
 * of a Command register, two Link registers, and two Freq/Rev registers.
 */
/* Slave/Primary commands */
#define LDT_COMMAND_UNIT_ID_MASK         	0x001f0000
#define LDT_COMMAND_UNIT_ID_SHIFT        	(16+0)
#define LDT_COMMAND_UNIT_ID(cmd) \
	(((cmd) & LDT_COMMAND_UNIT_ID_MASK) >> LDT_COMMAND_UNIT_ID_SHIFT)
#define LDT_COMMAND_UNIT_COUNT_MASK        	0x03e00000
#define LDT_COMMAND_UNIT_COUNT_SHIFT       	(16+5)
#define LDT_COMMAND_UNIT_COUNT(cmd) \
	(((cmd) & LDT_COMMAND_UNIT_COUNT_MASK) >> LDT_COMMAND_UNIT_COUNT_SHIFT)
#define LDT_COMMAND_MASTER_HOST_MASK		0x04000000
#define LDT_COMMAND_MASTER_HOST(cmd) \
	(((cmd) & LDT_COMMAND_MASTER_HOST_MASK) ? 1 : 0)
#define LDT_COMMAND_DEFAULT_DIRECTION_MASK     	0x08000000
#define LDT_COMMAND_DEFAULT_DIRECTION(cmd) \
	(((cmd) & LDT_COMMAND_DEFAULT_DIRECTION_MASK) ? 1 : 0)

/*
 * An LDT capability for type Host (aka Secondary) consists of a
 * Command register, a single Link register, and a Freq/Rev register.
 */
/* Host/Secondary command fields */
#define LDT_COMMAND_WARM_RESET           	0x00010000
#define LDT_COMMAND_DOUBLE_ENDED            	0x00020000
#define LDT_COMMAND_DEVICE_NUMBER_MASK         	0x007c0000
#define LDT_COMMAND_DEVICE_NUMBER_SHIFT        	(16+2)
/* Following for 1.0x only */
#define LDT_COMMAND_CHAIN_SIDE                  0x00800000
#define LDT_COMMAND_HOST_HIDE                   0x01000000
#define LDT_COMMAND_ACT_AS_SLAVE                0x02000000
#define LDT_COMMAND_INBOUND_EOC_ERROR           0x04000000


/*
 * Link Register; contains control and config fields.
 */
#define LDT_LINK_OFF(n)			(0x04 + ((n)<<2))

#define LDT_LINKCTRL_MASK			0x0000ffff
#define LDT_LINKCTRL_SHIFT			0
#define LDT_LINKCTRL(cr) \
	(((cr) & LDT_LINKCTRL_MASK) >> LDT_LINKCTRL_SHIFT)
#define LDT_LINKCTRL_CFLE			0x00000002
#define LDT_LINKCTRL_CST			0x00000004
#define LDT_LINKCTRL_CFE			0x00000008
#define LDT_LINKCTRL_LINKFAIL			0x00000010
#define LDT_LINKCTRL_INITDONE			0x00000020
#define LDT_LINKCTRL_EOC			0x00000040
#define LDT_LINKCTRL_TXOFF			0x00000080
#define LDT_LINKCTRL_CRCERROR_MASK		0x00000f00
#define LDT_LINKCTRL_CRCERROR_SHIFT		8
#define LDT_LINKCTRL_ISOCEN			0x00001000
#define LDT_LINKCTRL_LSEN			0x00002000
/* Following for 1.0x only */
#define LDT_LINKCTRL_EXTCTL                     0x00004000

#define LDT_LINKCFG_MASK			0xffff0000
#define LDT_LINKCFG_SHIFT			16
#define LDT_LINKCFG(cr) \
	(((cr) & LDT_LINKCFG_MASK) >> LDT_LINKCFG_SHIFT)
#define LDT_LINKCFG_MAX_WIDTH_IN(cr) \
	(((cr) >> (16+0)) & 0xf)
#define LDT_LINKCFG_MAX_WIDTH_OUT(cr) \
	(((cr) >> (16+4)) & 0xf)
#define LDT_LINKCFG_WIDTH_IN(cr) \
	(((cr) >> (16+8)) & 0xf)
#define LDT_LINKCFG_WIDTH_OUT(cr) \
	(((cr) >> (16+12)) & 0xf)


/*
 * Link Frequency Register; contains version and frequency fields.
 */
#define LDT_LINKFREQ_CAP(cr) \
	(((cr) >> 16) & 0xffff)

#define LDT_LINKFREQ_MASK                       0x00000f00
#define LDT_LINKFREQ_SHIFT                      8
#define LDT_LINKFREQ(cr) \
	(((cr) >> 8) & 0x0f)

#define LDT_REVISION_ID(cr) \
	(((cr) >> 0) & 0xff)

#define LDT_REV_017                     0x11
#define LDT_REV_102                     0x22

/* Slave/Primary offsets */
#define LDT_FREQ0_OFF			0x0c
#define LDT_FREQ1_OFF			0x10
#define LDT_FREQn_OFF(n)		(0x0c + ((n)<<2))

/* Host/Secondary offsets */
#define LDT_FREQ_OFF			0x08

#define LDT_FREQ_200                    0x00
#define LDT_FREQ_300                    0x01
#define LDT_FREQ_400                    0x02
#define LDT_FREQ_500                    0x03
#define LDT_FREQ_600                    0x04
#define LDT_FREQ_800                    0x05
#define LDT_FREQ_1000                   0x06

#endif /* _LDTREG_H_ */
