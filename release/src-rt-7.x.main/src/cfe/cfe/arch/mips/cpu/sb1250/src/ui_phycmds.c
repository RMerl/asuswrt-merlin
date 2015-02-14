/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  PHY hacking commands			File: ui_phycmds.c
    *  
    *  These commands let you directly muck with the PHYs
    *  attached to the Ethernet controllers.
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




#include "sbmips.h"

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_arena.h"

#include "ui_command.h"

#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_mac.h"

#include "cfe.h"
#include "cfe_error.h"
#include "mii.h"


#define PHY_READCSR(t) (*((volatile uint64_t *) (t)))
#define PHY_WRITECSR(t,v) *((volatile uint64_t *) (t)) = (v)

#define M_MAC_MDIO_DIR_OUTPUT	0		/* for clarity */

#ifdef __long64
typedef volatile uint64_t phy_port_t;
typedef uint64_t phy_physaddr_t;
#define PHY_PORT(x) PHYS_TO_K1(x)
#else
typedef volatile uint32_t phy_port_t;
typedef uint32_t phy_physaddr_t;
#define PHY_PORT(x) PHYS_TO_K1(x)
#endif

typedef struct phy_s {
    phy_port_t sbe_mdio;
} phy_t;


int ui_init_phycmds(void);

static int ui_cmd_phydump(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_physet(ui_cmdline_t *cmd,int argc,char *argv[]);

static void phy_mii_write(phy_t *s,int phyaddr,int regidx,
		     unsigned int regval);
static unsigned int phy_mii_read(phy_t *s,int phyaddr,int regidx);


int ui_init_phycmds(void)
{
    cmd_addcmd("phy dump",
	       ui_cmd_phydump,
	       NULL,
	       "Dump the registers on the PHY",
	       "phy dump macid [reg]\n\n"
	       "This command displays the contents of the registers on the PHY\n"
	       "attached to the specified Ethernet controller.  macid is the\n"
	       "Ethernet controller ID (0..2 for the BCM1250) and reg\n"
	       "is an optional register number (0..31).  By default, all registers\n"
	       "are displayed.",
	       "-phy=*;Specify PHY address (default=1)");

    cmd_addcmd("phy set",
	       ui_cmd_physet,
	       NULL,
	       "Set the value of a PHY register",
	       "phy set macid reg value\n\n"
	       "Sets the value of a register on a PHY.  macid is the Ethernet\n"
	       "controller number (0..2 for the BCM1250), reg is the register\n"
	       "number (0..31), and value is the 16-bit value to write to the\n"
	       "register.\n",
	       "-phy=*;Specify PHY address (default=1)");

    return 0;
}

static int ui_cmd_physet(ui_cmdline_t *cmd,int argc,char *argv[])
{
    phy_t phy;
    int phynum;
    int mac;
    char *x;
    unsigned int value;
    unsigned int reg;

    x = cmd_getarg(cmd,0);
    if (!x) return ui_showusage(cmd);

    mac = atoi(x);
    if ((mac < 0) || (mac > 2)) {
	return ui_showerror(CFE_ERR_INV_PARAM,"Invalid MAC number");
	}
    phy.sbe_mdio = PHY_PORT(A_MAC_REGISTER(mac,R_MAC_MDIO));

    if (cmd_sw_value(cmd,"-phy",&x)) {
	phynum = atoi(x);
	}
    else phynum = 1;
    
    x = cmd_getarg(cmd,1);
    if (!x) return ui_showusage(cmd);
    reg = atoi(x);
    if ((reg < 0) || (reg > 31)) {
	return ui_showerror(CFE_ERR_INV_PARAM,"Invalid phy register number");
	}

    x = cmd_getarg(cmd,2);
    if (!x) return ui_showusage(cmd);
    value = atoi(x) & 0xFFFF;

    phy_mii_write(&phy,phynum,reg,value);

    printf("Wrote 0x%04X to phy %d register 0x%02X on mac %d\n",
	   value,phynum,reg,mac);

    return 0;
}

static int ui_cmd_phydump(ui_cmdline_t *cmd,int argc,char *argv[])
{
    phy_t phy;
    int phynum;
    int idx;
    int mac;
    char *x;
    unsigned int reg;
    int allreg = 1;

    x = cmd_getarg(cmd,0);
    if (!x) return ui_showusage(cmd);

    mac = atoi(x);
    if ((mac < 0) || (mac > 2)) {
	return ui_showerror(CFE_ERR_INV_PARAM,"Invalid MAC number");
	}
    phy.sbe_mdio = PHY_PORT(A_MAC_REGISTER(mac,R_MAC_MDIO));

    if (cmd_sw_value(cmd,"-phy",&x)) {
	phynum = atoi(x);
	}
    else phynum = 1;
    
    x = cmd_getarg(cmd,1);
    reg = 0;
    if (x) {
	reg = atoi(x);
	if ((reg < 0) || (reg > 31)) {
	    return ui_showerror(CFE_ERR_INV_PARAM,"Invalid phy register number");
	    }
	allreg = 0;
	}

    if (allreg) {
	printf("** PHY registers on MAC %d PHY %d **\n",mac,phynum);
	for (idx = 0; idx < 31; idx+=2) {
	    printf("Reg 0x%02X  =  0x%04X   |  ",idx,phy_mii_read(&phy,phynum,idx));
	    printf("Reg 0x%02X  =  0x%04X",idx+1,phy_mii_read(&phy,phynum,idx+1));
	    printf("\n");
	    }
	}
    else {
	printf("Reg %02X = %04X\n",reg,phy_mii_read(&phy,phynum,reg));
	}

    return 0;

}




/*  *********************************************************************
    *  PHY_MII_SYNC(s)
    *  
    *  Synchronize with the MII - send a pattern of bits to the MII
    *  that will guarantee that it is ready to accept a command.
    *  
    *  Input parameters: 
    *  	   s - sbmac structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void phy_mii_sync(phy_t *s)
{
    int cnt;
    uint64_t bits;
    int mac_mdio_genc; /*genc bit needs to be saved*/

    mac_mdio_genc = PHY_READCSR(s->sbe_mdio) & M_MAC_GENC;

    bits = M_MAC_MDIO_DIR_OUTPUT | M_MAC_MDIO_OUT;

    PHY_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);

    for (cnt = 0; cnt < 32; cnt++) {
	PHY_WRITECSR(s->sbe_mdio,bits | M_MAC_MDC | mac_mdio_genc);
	PHY_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);
	}

}

/*  *********************************************************************
    *  PHY_MII_SENDDATA(s,data,bitcnt)
    *  
    *  Send some bits to the MII.  The bits to be sent are right-
    *  justified in the 'data' parameter.
    *  
    *  Input parameters: 
    *  	   s - sbmac structure
    *  	   data - data to send
    *  	   bitcnt - number of bits to send
    ********************************************************************* */

static void phy_mii_senddata(phy_t *s,unsigned int data, int bitcnt)
{
    int i;
    uint64_t bits;
    unsigned int curmask;
    int mac_mdio_genc;

    mac_mdio_genc = PHY_READCSR(s->sbe_mdio) & M_MAC_GENC;

    bits = M_MAC_MDIO_DIR_OUTPUT;
    PHY_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);

    curmask = 1 << (bitcnt - 1);

    for (i = 0; i < bitcnt; i++) {
	if (data & curmask) bits |= M_MAC_MDIO_OUT;
	else bits &= ~M_MAC_MDIO_OUT;
	PHY_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);
	PHY_WRITECSR(s->sbe_mdio,bits | M_MAC_MDC | mac_mdio_genc);
	PHY_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);
	curmask >>= 1;
	}
}



/*  *********************************************************************
    *  PHY_MII_READ(s,phyaddr,regidx)
    *  
    *  Read a PHY register.
    *  
    *  Input parameters: 
    *  	   s - sbmac structure
    *  	   phyaddr - PHY's address
    *  	   regidx = index of register to read
    *  	   
    *  Return value:
    *  	   value read, or 0 if an error occured.
    ********************************************************************* */

static unsigned int phy_mii_read(phy_t *s,int phyaddr,int regidx)
{
    int idx;
    int error;
    int regval;
    int mac_mdio_genc;

    /*
     * Synchronize ourselves so that the PHY knows the next
     * thing coming down is a command
     */

    phy_mii_sync(s);

    /*
     * Send the data to the PHY.  The sequence is
     * a "start" command (2 bits)
     * a "read" command (2 bits)
     * the PHY addr (5 bits)
     * the register index (5 bits)
     */

    phy_mii_senddata(s,MII_COMMAND_START, 2);
    phy_mii_senddata(s,MII_COMMAND_READ, 2);
    phy_mii_senddata(s,phyaddr, 5);
    phy_mii_senddata(s,regidx, 5);

    mac_mdio_genc = PHY_READCSR(s->sbe_mdio) & M_MAC_GENC;

    /* 
     * Switch the port around without a clock transition.
     */
    PHY_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | mac_mdio_genc);

    /*
     * Send out a clock pulse to signal we want the status
     */

    PHY_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | M_MAC_MDC | mac_mdio_genc);
    PHY_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | mac_mdio_genc);

    /* 
     * If an error occured, the PHY will signal '1' back
     */
    error = PHY_READCSR(s->sbe_mdio) & M_MAC_MDIO_IN;

    /* 
     * Issue an 'idle' clock pulse, but keep the direction
     * the same.
     */
    PHY_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | M_MAC_MDC | mac_mdio_genc);
    PHY_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | mac_mdio_genc);

    regval = 0;

    for (idx = 0; idx < 16; idx++) {
	regval <<= 1;

	if (error == 0) {
	    if (PHY_READCSR(s->sbe_mdio) & M_MAC_MDIO_IN) regval |= 1;
	    }

	PHY_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | M_MAC_MDC | mac_mdio_genc);
	PHY_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | mac_mdio_genc);
	}

    /* Switch back to output */
    PHY_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_OUTPUT | mac_mdio_genc);

    if (error == 0) return regval;
    return 0;
}


/*  *********************************************************************
    *  PHY_MII_WRITE(s,phyaddr,regidx,regval)
    *  
    *  Write a value to a PHY register.
    *  
    *  Input parameters: 
    *  	   s - sbmac structure
    *  	   phyaddr - PHY to use
    *  	   regidx - register within the PHY
    *  	   regval - data to write to register
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void phy_mii_write(phy_t *s,int phyaddr,int regidx,
			    unsigned int regval)
{
    int mac_mdio_genc;

    phy_mii_sync(s);

    phy_mii_senddata(s,MII_COMMAND_START,2);
    phy_mii_senddata(s,MII_COMMAND_WRITE,2);
    phy_mii_senddata(s,phyaddr, 5);
    phy_mii_senddata(s,regidx, 5);
    phy_mii_senddata(s,MII_COMMAND_ACK,2);
    phy_mii_senddata(s,regval,16);

    mac_mdio_genc = PHY_READCSR(s->sbe_mdio) & M_MAC_GENC;

    PHY_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_OUTPUT | mac_mdio_genc);
}
