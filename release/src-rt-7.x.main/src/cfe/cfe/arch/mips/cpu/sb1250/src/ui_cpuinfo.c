/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  All you never wanted to know about CPUs	File: ui_cpuinfo.c
    *
    *  Routines to display CPU info (common to all CPUs)
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
#include "lib_queue.h"
#include "lib_printf.h"
#include "ui_command.h"

#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"
#include "sb1250_wid.h"

#include "bsp_config.h"

#include "env_subr.h"


/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define SB1250_PASS1 (K_SYS_REVISION_PASS1)	/* 1 */
#define SB1250_PASS2 (K_SYS_REVISION_PASS2)	/* 3 */
#define SB1250_PASS20FL  (4)
#define SB1250_PASS20WB  (3)
#define SB1250_PASS21FL  (5)
#define SB1250_PASS21WB  (6)
#define SB1250_STEP_A6	 (7)	/* A6 is really rev=4 && wid!=0 */
#define SB1250_STEP_A7	 (8)
#define SB1250_STEP_B0	 (9)
#define SB1250_STEP_A8	 (11)
#define SB1250_STEP_B1   (16)
#define SB1250_STEP_B2   (17)
#define SB1250_STEP_C0	 (32)

/*
 * This lets us override the WID by poking values into our PromICE 
 */
#ifdef _MAGICWID_
#undef A_SCD_SYSTEM_REVISION
#define A_SCD_SYSTEM_REVISION 0x1FC00508
#undef A_SCD_SYSTEM_MANUF
#define A_SCD_SYSTEM_MANUF 0x1FC00518
#endif

/*  *********************************************************************
    *  Externs/forwards
    ********************************************************************* */

void sb1250_show_cpu_type(void);

/* XXXCGD: could be const, when env_setenv can cope.  */
static char *show_cpu_type_bcm1250(uint64_t syscfg, uint64_t sysrev);
static char *show_cpu_type_bcm112x(uint64_t syscfg, uint64_t sysrev);

/*  *********************************************************************
    *  ui_show_cpu_type()
    *  
    *  Display board CPU information
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void sb1250_show_cpu_type(void)
{
    uint64_t syscfg, sysrev;
    int plldiv;
    /* XXXCGD: could be const, when env_setenv can cope.  */
    char *envval;
    char *(*infofn)(uint64_t, uint64_t);
    char temp[32];

    syscfg = SBREADCSR(A_SCD_SYSTEM_CFG);
    sysrev = SBREADCSR(A_SCD_SYSTEM_REVISION);

    switch (SYS_SOC_TYPE(sysrev)) {
    case K_SYS_SOC_TYPE_BCM1250:
	envval = "1250";
	infofn = show_cpu_type_bcm1250;
	break;

    case K_SYS_SOC_TYPE_BCM1120:
	envval = "1120";
	infofn = show_cpu_type_bcm112x;
	break;

    case K_SYS_SOC_TYPE_BCM1125:
	envval = "1125";
	infofn = show_cpu_type_bcm112x;
	break;

    case K_SYS_SOC_TYPE_BCM1125H:
	envval = "1125H";
	infofn = show_cpu_type_bcm112x;
	break;

    default:
        sprintf(temp, "unknown_%04x", (int)G_SYS_PART(sysrev));
	envval = temp;
	infofn = NULL;
	break;
    }
    env_setenv("CPU_TYPE", envval,
	       ENV_FLG_BUILTIN | ENV_FLG_READONLY | ENV_FLG_ADMIN);

    envval = NULL;
    if (infofn != NULL)
	envval = (*infofn)(syscfg, sysrev);
    if (envval == NULL) {
        sprintf(temp, "unknown_%02x", (int)G_SYS_REVISION(sysrev));
	envval = temp;
    }
    env_setenv("CPU_REVISION", envval,
	       ENV_FLG_BUILTIN | ENV_FLG_READONLY | ENV_FLG_ADMIN);

    /* Set # of CPUs based on 2nd hex digit of part number */
    sprintf(temp, "%d", (int)((G_SYS_PART(sysrev) >> 8) & 0x0F));
    env_setenv("CPU_NUM_CORES", temp,
	       ENV_FLG_BUILTIN | ENV_FLG_READONLY | ENV_FLG_ADMIN);

    /*
     * Set variable that contains CPU speed, spit out config register
     */
    printf("SysCfg: %016llX [PLL_DIV: %d, IOB0_DIV: %s, IOB1_DIV: %s]\n",
	   syscfg,
	   (int)G_SYS_PLL_DIV(syscfg),
	   (syscfg & M_SYS_IOB0_DIV) ? "CPUCLK/3" : "CPUCLK/4",
	   (syscfg & M_SYS_IOB1_DIV) ? "CPUCLK/2" : "CPUCLK/3");

    plldiv = G_SYS_PLL_DIV(syscfg);
    if (plldiv == 0) {
	printf("PLL_DIV of zero found, assuming 6 (300MHz)\n");
	plldiv = 6;
    }

    sprintf(temp,"%d", plldiv * 50);
    env_setenv("CPU_SPEED", temp,
	       ENV_FLG_BUILTIN | ENV_FLG_READONLY | ENV_FLG_ADMIN);
}

/*  *********************************************************************
    *  show_cpu_type_bcm1250()
    *  
    *  Display CPU information for BCM1250 CPUs
    *  
    *  Input parameters: 
    *  	   revstr: pointer to string pointer, to be filled in with
    *      revision name.
    *  	   
    *  Return value:
    *  	   none.  fills in revstr.
    ********************************************************************* */
static char *
show_cpu_type_bcm1250(uint64_t syscfg, uint64_t sysrev)
{
    char *revstr, *revprintstr;
    uint64_t cachetest;
    uint64_t sysmanuf;
    uint32_t wid;
    int bin;
    unsigned int cpu_pass;
    char temp[32];
    static uint8_t cachesizes[16] = {4,2,2,2,2,1,1,1,2,1,1,1,2,1,1,0};
    static char *binnames[8] = {
	"2CPU_FI_1D_H2",
	"2CPU_FI_FD_F2 (OK)",
	"2CPU_FI_FD_H2",
	"2CPU_3I_3D_F2",
	"2CPU_3I_3D_H2",
	"1CPU_FI_FD_F2",
	"1CPU_FI_FD_H2",
	"2CPU_1I_1D_Q2"};

    cpu_pass = G_SYS_REVISION(sysrev);

    wid = G_SYS_WID(SBREADCSR(A_SCD_SYSTEM_REVISION));
    wid = WID_UNCONVOLUTE(wid);

    if ((wid != 0) && (cpu_pass == 0x4)) {
	cpu_pass = SB1250_STEP_A6;
	}

    switch (cpu_pass) {
	case SB1250_PASS1:
	    revstr = "PASS1";
	    revprintstr = "Pass 1";
	    break;
	case SB1250_PASS20WB:
	    revstr = "A1";
	    revprintstr = "Pass 2.0 (wirebond)";
	    break;
	case SB1250_PASS20FL:
	    revstr = "A2";
	    revprintstr = "Pass 2.0 (flip-chip)";
	    break;
	case SB1250_PASS21WB:
	    revstr = "A4";
	    revprintstr = "A4 Pass 2.1 (wirebond)";
	    break;
	case SB1250_PASS21FL:
	    revstr = "A3";
	    revprintstr = "A3 Pass 2.1 (flip-chip)";
	    break;
	case SB1250_STEP_A6:
	    revstr = revprintstr = "A6";
	    break;
	case SB1250_STEP_A7:
	    revstr = revprintstr = "A7";
	    break;
	case SB1250_STEP_A8:
	    revprintstr = "A8/A10";
	    revstr = "A8";
	    break;
	case SB1250_STEP_B0:
	    revstr = revprintstr = "B0";
	    break;
	case SB1250_STEP_B1:
	    revstr = revprintstr = "B1";
	    break;
	case SB1250_STEP_B2:
	    revstr = revprintstr = "B2";
	    break;
	case SB1250_STEP_C0:
	    revstr = revprintstr = "C0";
	    break;
	default:
	    revstr = NULL;
	    sprintf(temp, "rev 0x%x", (int)G_SYS_REVISION(sysrev));
	    revprintstr = temp;
	    break;
	}
    printf("CPU: BCM1250 %s\n", revprintstr);

    if (((G_SYS_PART(sysrev) >> 8) & 0x0F) == 1) {
	printf("[Uniprocessor CPU mode]\n");
	}

    /*
     * Report cache status if the cache was disabled, or the status of
     * the cache test for non-WID pass2 and pass3 parts.
     */
    printf("L2 Cache Status: ");
    if ((syscfg & M_SYS_L2C_RESET) != 0) {
	printf("disabled via JTAG\n");
	}
    else if ((cpu_pass == SB1250_PASS20FL) || (cpu_pass == SB1250_PASS20WB) ||
	     (cpu_pass == SB1250_STEP_C0)) {
	cachetest = (SBREADCSR(A_MAC_REGISTER(2,R_MAC_HASH_BASE)) & 0x0F);
	printf("0x%llX    Available L2 Cache: %dKB\n",cachetest,
	       ((int)cachesizes[(int)cachetest])*128);
	}
    else printf("OK\n");

    if (wid == 0) {
	printf("Wafer ID:  Not set\n");
	}
    else if (cpu_pass != SB1250_STEP_C0) {

	printf("Wafer ID:   0x%08X  [Lot %d, Wafer %d]\n",wid,
	       G_WID_LOTID(wid),G_WID_WAFERID(wid));

	bin = G_WID_BIN(wid);

	printf("Manuf Test: Bin %c [%s] ","EABCDFGH"[bin],binnames[bin]);

	if (bin != K_WID_BIN_2CPU_FI_FD_F2)  {
	    printf("L2:%d ",G_WID_L2QTR(wid));
	    printf("CPU0:[I=%d D=%d]  ",G_WID_CPU0_L1I(wid),G_WID_CPU0_L1D(wid));
	    printf("CPU1:[I=%d D=%d]",G_WID_CPU1_L1I(wid),G_WID_CPU1_L1D(wid));
	    }
	printf("\n");
	}

    if (cpu_pass == SB1250_STEP_C0) {
	/* Read system_manuf register for C0 */
	sysmanuf = SBREADCSR(A_SCD_SYSTEM_MANUF);

	printf("SysManuf:  %016llX [X: %d Y: %d]",sysmanuf, (int)G_SYS_XPOS(sysmanuf), 
	                                            (int)G_SYS_YPOS(sysmanuf)); 
	printf("\n");
	}

    return (revstr);
}

/*  *********************************************************************
    *  show_cpu_type_bcm112x()
    *  
    *  Display CPU information for BCM112x CPUs
    *  
    *  Input parameters: 
    *  	   revstr: pointer to string pointer, to be filled in with
    *      revision name.
    *  	   
    *  Return value:
    *  	   none.  fills in revstr.
    ********************************************************************* */
static char *
show_cpu_type_bcm112x(uint64_t syscfg, uint64_t sysrev)
{
    char *revstr, *revprintstr;
    char temp[32];

    switch (G_SYS_REVISION(sysrev)) {
	case K_SYS_REVISION_BCM112x_A1:
	    revstr = revprintstr = "A1";
	    break;
	case K_SYS_REVISION_BCM112x_A2:
	    revstr = revprintstr = "A2";
	    break;
	default:
	    revstr = NULL;
	    sprintf(temp, "rev 0x%x", (int)G_SYS_REVISION(sysrev));
	    revprintstr = temp;
	    break;
	}
    printf("CPU: %s %s\n", env_getenv("CPU_TYPE"), revprintstr);

    printf("L2 Cache: ");
    if ((syscfg & M_SYS_L2C_RESET) != 0)
	printf("disabled via JTAG\n");
    else
	printf("256KB\n");

    return (revstr);
}
