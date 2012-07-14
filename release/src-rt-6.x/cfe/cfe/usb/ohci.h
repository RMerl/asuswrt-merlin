/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  OHCI defs				File: ohci.h
    *  
    *  Open Host controller interface definitions
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


/*  *********************************************************************
    *  Macros to muck with bitfields
    ********************************************************************* */

#define _OHCI_MAKE32(x) ((uint32_t)(x))

/*
 * Make a mask for 1 bit at position 'n'
 */

#define _OHCI_MAKEMASK1(n) (_OHCI_MAKE32(1) << _OHCI_MAKE32(n))

/*
 * Make a mask for 'v' bits at position 'n'
 */

#define _OHCI_MAKEMASK(v,n) (_OHCI_MAKE32((_OHCI_MAKE32(1)<<(v))-1) << _OHCI_MAKE32(n))

/*
 * Make a value at 'v' at bit position 'n'
 */

#define _OHCI_MAKEVALUE(v,n) (_OHCI_MAKE32(v) << _OHCI_MAKE32(n))
#define _OHCI_GETVALUE(v,n,m) ((_OHCI_MAKE32(v) & _OHCI_MAKE32(m)) >> _OHCI_MAKE32(n))



/*  *********************************************************************
    *  Endpoint Descriptor (interrupt, bulk)
    ********************************************************************* */

#define OHCI_ED_ALIGN	32

typedef struct ohci_ed_s {
    uint32_t ed_control;
    uint32_t ed_tailp;
    uint32_t ed_headp;
    uint32_t ed_next_ed;
} ohci_ed_t;

#define S_OHCI_ED_FA		0
#define M_OHCI_ED_FA		_OHCI_MAKEMASK(7,S_OHCI_ED_FA)
#define V_OHCI_ED_FA(x)		_OHCI_MAKEVALUE(x,S_OHCI_ED_FA)
#define G_OHCI_ED_FA(x)		_OHCI_GETVALUE(x,S_OHCI_ED_FA,M_OHCI_ED_FA)

#define S_OHCI_ED_EN		7
#define M_OHCI_ED_EN		_OHCI_MAKEMASK(4,S_OHCI_ED_EN)
#define V_OHCI_ED_EN(x)		_OHCI_MAKEVALUE(x,S_OHCI_ED_EN)
#define G_OHCI_ED_EN(x)		_OHCI_GETVALUE(x,S_OHCI_ED_EN,M_OHCI_ED_EN)

#define S_OHCI_ED_DIR		11
#define M_OHCI_ED_DIR		_OHCI_MAKEMASK(2,S_OHCI_ED_DIR)
#define V_OHCI_ED_DIR(x)	_OHCI_MAKEVALUE(x,S_OHCI_ED_DIR)
#define G_OHCI_ED_DIR(x)	_OHCI_GETVALUE(x,S_OHCI_ED_DIR,M_OHCI_ED_DIR)

#define K_OHCI_ED_DIR_FROMTD	0
#define K_OHCI_ED_DIR_OUT	1
#define K_OHCI_ED_DIR_IN	2

#define M_OHCI_ED_LOWSPEED	_OHCI_MAKEMASK1(13)
#define M_OHCI_ED_SKIP		_OHCI_MAKEMASK1(14)
#define M_OHCI_ED_ISOCFMT	_OHCI_MAKEMASK1(15)

#define S_OHCI_ED_MPS		16
#define M_OHCI_ED_MPS		_OHCI_MAKEMASK(11,S_OHCI_ED_MPS)
#define V_OHCI_ED_MPS(x)	_OHCI_MAKEVALUE(x,S_OHCI_ED_MPS)
#define G_OHCI_ED_MPS(x)	_OHCI_GETVALUE(x,S_OHCI_ED_MPS,M_OHCI_ED_MPS)

#define M_OHCI_ED_PTRMASK	0xFFFFFFF0
#define M_OHCI_ED_HALT		_OHCI_MAKEMASK1(0)
#define M_OHCI_ED_TOGGLECARRY	_OHCI_MAKEMASK1(1)

/*  *********************************************************************
    *  Transfer Descriptor
    ********************************************************************* */

#define OHCI_TD_ALIGN	32

typedef struct ohci_td_s {
    uint32_t td_control;
    uint32_t td_cbp;
    uint32_t td_next_td;
    uint32_t td_be;
} ohci_td_t;

#define M_OHCI_TD_SHORTOK	_OHCI_MAKEMASK1(18)

#define S_OHCI_TD_PID		19
#define M_OHCI_TD_PID		_OHCI_MAKEMASK(2,S_OHCI_TD_PID)
#define V_OHCI_TD_PID(x)	_OHCI_MAKEVALUE(x,S_OHCI_TD_PID)
#define G_OHCI_TD_PID(x)	_OHCI_GETVALUE(x,S_OHCI_TD_PID,M_OHCI_TD_PID)

#define K_OHCI_TD_SETUP		0
#define K_OHCI_TD_OUT		1
#define K_OHCI_TD_IN		2
#define K_OHCI_TD_RESERVED	3

#define V_OHCI_TD_SETUP		V_OHCI_TD_PID(K_OHCI_TD_SETUP)
#define V_OHCI_TD_OUT		V_OHCI_TD_PID(K_OHCI_TD_OUT)
#define V_OHCI_TD_IN		V_OHCI_TD_PID(K_OHCI_TD_IN)
#define V_OHCI_TD_RESERVED	V_OHCI_TD_PID(K_OHCI_TD_RESERVED)

#define S_OHCI_TD_DI		21
#define M_OHCI_TD_DI		_OHCI_MAKEMASK(3,S_OHCI_TD_DI)
#define V_OHCI_TD_DI(x)		_OHCI_MAKEVALUE(x,S_OHCI_TD_DI)
#define G_OHCI_TD_DI(x)		_OHCI_GETVALUE(x,S_OHCI_TD_DI,M_OHCI_TD_DI)

#define K_OHCI_TD_NOINTR	7
#define V_OHCI_TD_NOINTR	V_OHCI_TD_DI(K_OHCI_TD_NOINTR)

#define S_OHCI_TD_DT		24
#define M_OHCI_TD_DT		_OHCI_MAKEMASK(2,S_OHCI_TD_DT)
#define V_OHCI_TD_DT(x)		_OHCI_MAKEVALUE(x,S_OHCI_TD_DT)
#define G_OHCI_TD_DT(x)		_OHCI_GETVALUE(x,S_OHCI_TD_DT,M_OHCI_TD_DT)

#define K_OHCI_TD_DT_DATA0	2
#define K_OHCI_TD_DT_DATA1	3
#define K_OHCI_TD_DT_TCARRY	0

#define S_OHCI_TD_EC		26
#define M_OHCI_TD_EC		_OHCI_MAKEMASK(2,S_OHCI_TD_EC)
#define V_OHCI_TD_EC(x)		_OHCI_MAKEVALUE(x,S_OHCI_TD_EC)
#define G_OHCI_TD_EC(x)		_OHCI_GETVALUE(x,S_OHCI_TD_EC,M_OHCI_TD_EC)

#define S_OHCI_TD_CC		28
#define M_OHCI_TD_CC		_OHCI_MAKEMASK(4,S_OHCI_TD_CC)
#define V_OHCI_TD_CC(x)		_OHCI_MAKEVALUE(x,S_OHCI_TD_CC)
#define G_OHCI_TD_CC(x)		_OHCI_GETVALUE(x,S_OHCI_TD_CC,M_OHCI_TD_CC)

#define K_OHCI_CC_NOERROR		0
#define K_OHCI_CC_CRC			1
#define K_OHCI_CC_BITSTUFFING		2
#define K_OHCI_CC_DATATOGGLEMISMATCH	3
#define K_OHCI_CC_STALL			4
#define K_OHCI_CC_DEVICENOTRESPONDING	5
#define K_OHCI_CC_PIDCHECKFAILURE	6
#define K_OHCI_CC_UNEXPECTEDPID		7
#define K_OHCI_CC_DATAOVERRUN		8
#define K_OHCI_CC_DATAUNDERRUN		9
#define K_OHCI_CC_BUFFEROVERRUN		12
#define K_OHCI_CC_BUFFERUNDERRUN	13
#define K_OHCI_CC_NOTACCESSED		15

#define K_OHCI_CC_CANCELLED		0xFF

#define OHCI_TD_MAX_DATA		8192


/*  *********************************************************************
    *  Endpoint descriptor (isochronous)
    ********************************************************************* */

/*
 * TBA
 */

/*  *********************************************************************
    *  Host Controller Communications Area (HCCA)
    ********************************************************************* */

#define OHCI_INTTABLE_SIZE	32

#define OHCI_HCCA_ALIGN		256	/* Align on 256-byte boundary */

typedef struct ohci_hcca_s {
    uint32_t hcca_inttable[OHCI_INTTABLE_SIZE];
    uint32_t hcca_framenum;		/* note: actually two 16-bit fields */
    uint32_t hcca_donehead;
    uint32_t hcca_reserved[29];		/* round to 256 bytes */
    uint32_t hcca_pad;
} ohci_hcca_t;

/*  *********************************************************************
    *  Registers
    ********************************************************************* */

#define _OHCI_REGIDX(x)	((x)*4)

#define R_OHCI_REVISION		_OHCI_REGIDX(0)
#define R_OHCI_CONTROL		_OHCI_REGIDX(1)
#define R_OHCI_CMDSTATUS	_OHCI_REGIDX(2)
#define R_OHCI_INTSTATUS	_OHCI_REGIDX(3)
#define R_OHCI_INTENABLE	_OHCI_REGIDX(4)
#define R_OHCI_INTDISABLE	_OHCI_REGIDX(5)
#define R_OHCI_HCCA		_OHCI_REGIDX(6)
#define R_OHCI_PERIODCURRENTED	_OHCI_REGIDX(7)
#define R_OHCI_CONTROLHEADED	_OHCI_REGIDX(8)
#define R_OHCI_CONTROLCURRENTED _OHCI_REGIDX(9)
#define R_OHCI_BULKHEADED	_OHCI_REGIDX(10)
#define R_OHCI_BULKCURRENTED	_OHCI_REGIDX(11)
#define R_OHCI_DONEHEAD		_OHCI_REGIDX(12)
#define R_OHCI_FMINTERVAL	_OHCI_REGIDX(13)
#define R_OHCI_FMREMAINING	_OHCI_REGIDX(14)
#define R_OHCI_FMNUMBER		_OHCI_REGIDX(15)
#define R_OHCI_PERIODICSTART	_OHCI_REGIDX(16)
#define R_OHCI_LSTHRESHOLD	_OHCI_REGIDX(17)
#define R_OHCI_RHDSCRA		_OHCI_REGIDX(18)
#define R_OHCI_RHDSCRB		_OHCI_REGIDX(19)
#define R_OHCI_RHSTATUS		_OHCI_REGIDX(20)
#define R_OHCI_RHPORTSTATUS(x)	_OHCI_REGIDX(20+(x))	/* note: 1-based! */


/*
 * R_OHCI_REVISION
 */

#define S_OHCI_REV_REV		0
#define M_OHCI_REV_REV		_OHCI_MAKEMASK(8,S_OHCI_REV_REV)
#define V_OHCI_REV_REV(x)	_OHCI_MAKEVALUE(x,S_OHCI_REV_REV)
#define G_OHCI_REV_REV(x)	_OHCI_GETVALUE(x,S_OHCI_REV_REV,M_OHCI_REV_REV)
#define K_OHCI_REV_11		0x10

/*
 * R_OHCI_CONTROL
 */

#define S_OHCI_CONTROL_CBSR	0
#define M_OHCI_CONTROL_CBSR	_OHCI_MAKEMASK(2,S_OHCI_CONTROL_CBSR)
#define V_OHCI_CONTROL_CBSR(x)	_OHCI_MAKEVALUE(x,S_OHCI_CONTROL_CBSR)
#define G_OHCI_CONTROL_CBSR(x)	_OHCI_GETVALUE(x,S_OHCI_CONTROL_CBSR,M_OHCI_CONTROL_CBSR)

#define K_OHCI_CBSR_11		0
#define K_OHCI_CBSR_21		1
#define K_OHCI_CBSR_31		2
#define K_OHCI_CBSR_41		3

#define M_OHCI_CONTROL_PLE	_OHCI_MAKEMASK1(2)
#define M_OHCI_CONTROL_IE	_OHCI_MAKEMASK1(3)
#define M_OHCI_CONTROL_CLE	_OHCI_MAKEMASK1(4)
#define M_OHCI_CONTROL_BLE	_OHCI_MAKEMASK1(5)

#define S_OHCI_CONTROL_HCFS	6
#define M_OHCI_CONTROL_HCFS	_OHCI_MAKEMASK(2,S_OHCI_CONTROL_HCFS)
#define V_OHCI_CONTROL_HCFS(x)	_OHCI_MAKEVALUE(x,S_OHCI_CONTROL_HCFS)
#define G_OHCI_CONTROL_HCFS(x)	_OHCI_GETVALUE(x,S_OHCI_CONTROL_HCFS,M_OHCI_CONTROL_HCFS)

#define K_OHCI_HCFS_RESET	0
#define K_OHCI_HCFS_RESUME	1
#define K_OHCI_HCFS_OPERATIONAL	2
#define K_OHCI_HCFS_SUSPEND	3

#define M_OHCI_CONTROL_IR	_OHCI_MAKEMASK1(8)
#define M_OHCI_CONTROL_RWC	_OHCI_MAKEMASK1(9)
#define M_OHCI_CONTROL_RWE	_OHCI_MAKEMASK1(10)

/*
 * R_OHCI_CMDSTATUS
 */

#define M_OHCI_CMDSTATUS_HCR	_OHCI_MAKEMASK1(0)
#define M_OHCI_CMDSTATUS_CLF	_OHCI_MAKEMASK1(1)
#define M_OHCI_CMDSTATUS_BLF	_OHCI_MAKEMASK1(2)
#define M_OHCI_CMDSTATUS_OCR	_OHCI_MAKEMASK1(3)

#define S_OHCI_CMDSTATUS_SOC	16
#define M_OHCI_CMDSTATUS_SOC	_OHCI_MAKEMASK(2,S_OHCI_CMDSTATUS_SOC)
#define V_OHCI_CMDSTATUS_SOC(x)	_OHCI_MAKEVALUE(x,S_OHCI_CMDSTATUS_SOC)
#define G_OHCI_CMDSTATUS_SOC(x)	_OHCI_GETVALUE(x,S_OHCI_CMDSTATUS_SOC,M_OHCI_CMDSTATUS_SOC)

/*
 * R_OHCI_INTSTATUS, R_OHCI_INTENABLE, R_OHCI_INTDISABLE
 */


#define M_OHCI_INT_SO		_OHCI_MAKEMASK1(0)
#define M_OHCI_INT_WDH		_OHCI_MAKEMASK1(1)
#define M_OHCI_INT_SF		_OHCI_MAKEMASK1(2)
#define M_OHCI_INT_RD		_OHCI_MAKEMASK1(3)
#define M_OHCI_INT_UE		_OHCI_MAKEMASK1(4)
#define M_OHCI_INT_FNO		_OHCI_MAKEMASK1(5)
#define M_OHCI_INT_RHSC		_OHCI_MAKEMASK1(6)
#define M_OHCI_INT_OC		_OHCI_MAKEMASK1(30)
#define M_OHCI_INT_MIE		_OHCI_MAKEMASK1(31)

#define M_OHCI_INT_ALL	M_OHCI_INT_SO | M_OHCI_INT_WDH | M_OHCI_INT_SF | \
                        M_OHCI_INT_RD | M_OHCI_INT_UE | M_OHCI_INT_FNO | \
                        M_OHCI_INT_RHSC | M_OHCI_INT_OC | M_OHCI_INT_MIE

/*
 * R_OHCI_FMINTERVAL
 */


#define S_OHCI_FMINTERVAL_FI		0
#define M_OHCI_FMINTERVAL_FI		_OHCI_MAKEMASK(14,S_OHCI_FMINTERVAL_FI)
#define V_OHCI_FMINTERVAL_FI(x)		_OHCI_MAKEVALUE(x,S_OHCI_FMINTERVAL_FI)
#define G_OHCI_FMINTERVAL_FI(x)		_OHCI_GETVALUE(x,S_OHCI_FMINTERVAL_FI,M_OHCI_FMINTERVAL_FI)

#define S_OHCI_FMINTERVAL_FSMPS		16
#define M_OHCI_FMINTERVAL_FSMPS		_OHCI_MAKEMASK(15,S_OHCI_FMINTERVAL_FSMPS)
#define V_OHCI_FMINTERVAL_FSMPS(x)	_OHCI_MAKEVALUE(x,S_OHCI_FMINTERVAL_FSMPS)
#define G_OHCI_FMINTERVAL_FSMPS(x)	_OHCI_GETVALUE(x,S_OHCI_FMINTERVAL_FSMPS,M_OHCI_FMINTERVAL_FSMPS)

#define OHCI_CALC_FSMPS(x) ((((x)-210)*6/7))


#define M_OHCI_FMINTERVAL_FIT		_OHCI_MAKEMASK1(31)

/*
 * R_OHCI_FMREMAINING
 */


#define S_OHCI_FMREMAINING_FR		0
#define M_OHCI_FMREMAINING_FR		_OHCI_MAKEMASK(14,S_OHCI_FMREMAINING_FR)
#define V_OHCI_FMREMAINING_FR(x)	_OHCI_MAKEVALUE(x,S_OHCI_FMREMAINING_FR)
#define G_OHCI_FMREMAINING_FR(x)	_OHCI_GETVALUE(x,S_OHCI_FMREMAINING_FR,M_OHCI_FMREMAINING_FR)

#define M_OHCI_FMREMAINING_FRT		_OHCI_MAKEMASK1(31)

/*
 * R_OHCI_RHDSCRA
 */


#define S_OHCI_RHDSCRA_NDP	0
#define M_OHCI_RHDSCRA_NDP	_OHCI_MAKEMASK(8,S_OHCI_RHDSCRA_NDP)
#define V_OHCI_RHDSCRA_NDP(x)	_OHCI_MAKEVALUE(x,S_OHCI_RHDSCRA_NDP)
#define G_OHCI_RHDSCRA_NDP(x)	_OHCI_GETVALUE(x,S_OHCI_RHDSCRA_NDP,M_OHCI_RHDSCRA_NDP)

#define M_OHCI_RHDSCRA_PSM	_OHCI_MAKEMASK1(8)
#define M_OHCI_RHDSCRA_NPS	_OHCI_MAKEMASK1(9)
#define M_OHCI_RHDSCRA_DT	_OHCI_MAKEMASK1(10)
#define M_OHCI_RHDSCRA_OCPM	_OHCI_MAKEMASK1(11)
#define M_OHCI_RHDSCRA_NOCP	_OHCI_MAKEMASK1(12)

#define S_OHCI_RHDSCRA_POTPGT	 24
#define M_OHCI_RHDSCRA_POTPGT	 _OHCI_MAKEMASK(8,S_OHCI_RHDSCRA_POTPGT)
#define V_OHCI_RHDSCRA_POTPGT(x) _OHCI_MAKEVALUE(x,S_OHCI_RHDSCRA_POTPGT)
#define G_OHCI_RHDSCRA_POTPGT(x) _OHCI_GETVALUE(x,S_OHCI_RHDSCRA_POTPGT,M_OHCI_RHDSCRA_POTPGT)

/*
 * R_OHCI_RHDSCRB
 */

#define S_OHCI_RHDSCRB_DR	0
#define M_OHCI_RHDSCRB_DR	_OHCI_MAKEMASK(16,S_OHCI_RHDSCRB_DR)
#define V_OHCI_RHDSCRB_DR(x) 	_OHCI_MAKEVALUE(x,S_OHCI_RHDSCRB_DR)
#define G_OHCI_RHDSCRB_DR(x) 	_OHCI_GETVALUE(x,S_OHCI_RHDSCRB_DR,M_OHCI_RHDSCRB_DR)

#define S_OHCI_RHDSCRB_PPCM	16
#define M_OHCI_RHDSCRB_PPCM	_OHCI_MAKEMASK(16,S_OHCI_RHDSCRB_PPCM)
#define V_OHCI_RHDSCRB_PPCM(x)	_OHCI_MAKEVALUE(x,S_OHCI_RHDSCRB_PPCM)
#define G_OHCI_RHDSCRB_PPCM(x) 	_OHCI_GETVALUE(x,S_OHCI_RHDSCRB_PPCM,M_OHCI_RHDSCRB_PPCM)

/*
 * R_OHCI_RHSTATUS
 */

#define M_OHCI_RHSTATUS_LPS	_OHCI_MAKEMASK1(0)
#define M_OHCI_RHSTATUS_OCI	_OHCI_MAKEMASK1(1)
#define M_OHCI_RHSTATUS_DRWE	_OHCI_MAKEMASK1(15)
#define M_OHCI_RHSTATUS_LPSC    _OHCI_MAKEMASK1(16)
#define M_OHCI_RHSTATUS_OCIC	_OHCI_MAKEMASK1(17)
#define M_OHCI_RHSTATUS_CRWE	_OHCI_MAKEMASK1(31)

/*
 * R_OHCI_RHPORTSTATUS
 */

#define M_OHCI_RHPORTSTAT_CCS	_OHCI_MAKEMASK1(0)
#define M_OHCI_RHPORTSTAT_PES	_OHCI_MAKEMASK1(1)
#define M_OHCI_RHPORTSTAT_PSS	_OHCI_MAKEMASK1(2)
#define M_OHCI_RHPORTSTAT_POCI	_OHCI_MAKEMASK1(3)
#define M_OHCI_RHPORTSTAT_PRS	_OHCI_MAKEMASK1(4)
#define M_OHCI_RHPORTSTAT_PPS	_OHCI_MAKEMASK1(8)
#define M_OHCI_RHPORTSTAT_LSDA	_OHCI_MAKEMASK1(9)
#define M_OHCI_RHPORTSTAT_CSC	_OHCI_MAKEMASK1(16)
#define M_OHCI_RHPORTSTAT_PESC	_OHCI_MAKEMASK1(17)
#define M_OHCI_RHPORTSTAT_PSSC	_OHCI_MAKEMASK1(18)
#define M_OHCI_RHPORTSTAT_OCIC	_OHCI_MAKEMASK1(19)
#define M_OHCI_RHPORTSTAT_PRSC	_OHCI_MAKEMASK1(20)

#define M_OHCI_RHPORTSTAT_ALLC (M_OHCI_RHPORTSTAT_CSC | \
				M_OHCI_RHPORTSTAT_PSSC | \
				M_OHCI_RHPORTSTAT_OCIC | \
				M_OHCI_RHPORTSTAT_PRSC)

/*  *********************************************************************
    *  OHCI Structures
    ********************************************************************* */

#define beginningof(ptr,type,field) ((type *) (((int) (ptr)) - ((int) ((type *) 0)->field)))

#define OHCI_INTTREE_SIZE	63

#define OHCI_EDPOOL_SIZE	128
#define OHCI_TDPOOL_SIZE	32

typedef struct ohci_endpoint_s {
    struct ohci_endpoint_s *ep_next;
    uint32_t ep_phys;
    int ep_flags;
    int ep_mps;
    int ep_num;
} ohci_endpoint_t;

typedef struct ohci_transfer_s {
    void *t_ref;
    int t_length;
    struct ohci_transfer_s *t_next;
} ohci_transfer_t;

typedef struct ohci_softc_s {
    ohci_endpoint_t *ohci_edtable[OHCI_INTTREE_SIZE];
    ohci_endpoint_t *ohci_inttable[OHCI_INTTABLE_SIZE];
    ohci_endpoint_t *ohci_isoc_list;
    ohci_endpoint_t *ohci_ctl_list;
    ohci_endpoint_t *ohci_bulk_list;
    ohci_hcca_t *ohci_hcca;
    ohci_endpoint_t *ohci_endpoint_pool;
    ohci_transfer_t *ohci_transfer_pool;
    ohci_ed_t *ohci_hwedpool;
    ohci_td_t *ohci_hwtdpool;
    ohci_endpoint_t *ohci_endpoint_freelist;
    ohci_transfer_t *ohci_transfer_freelist;
#ifdef _CFE_
    physaddr_t ohci_regs;
#else
    volatile uint32_t *ohci_regs;
#endif
    int ohci_ndp;
    long ohci_addr;
    uint32_t ohci_intdisable;

    int ohci_rh_newaddr;	/* Address to be set on next status update */
    int ohci_rh_addr;		/* address of root hub */
    int ohci_rh_conf;		/* current configuration # */
    uint8_t ohci_rh_buf[128];	/* buffer to hold hub responses */
    uint8_t *ohci_rh_ptr;	/* pointer into buffer */
    int ohci_rh_len;		/* remaining bytes to transfer */
    queue_t ohci_rh_intrq;	/* Interrupt request queue */
    usbbus_t *ohci_bus;		/* owning usbbus structure */

} ohci_softc_t;


/*
 * Misc stuff
 */
#define OHCI_RESET_DELAY	10
