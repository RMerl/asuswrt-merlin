/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  Memory Config Utility			File: memconfig.c
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *  This host tool lets you enter DRAM parameters and run CFE's
    *  standard memory configuration to calculate the relevant timing
    *  parameters.  It's a good way to see what CFE would have done,
    *  to find bogus timing calculations.
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

#include <stdio.h>
#include <string.h>

/*  *********************************************************************
    *  Basic types
    ********************************************************************* */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

/*  *********************************************************************
    *  SB1250 stuff
    ********************************************************************* */

#include "sb1250_defs.h"
#include "sb1250_mc.h"
#include "sb1250_draminit.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"

/*  *********************************************************************
    *  BCD macros
    ********************************************************************* */

#define DECTO10THS(x) ((((x) >> 4)*10)+((x) & 0x0F))

/*  *********************************************************************
    *  Global defaults
    ********************************************************************* */

#define MIN_tMEMCLK DRT10(8,0)
#define tROUNDTRIP DRT10(2,5)

/*  *********************************************************************
    *  Types
    ********************************************************************* */

typedef struct encvalue_s {
    char *name;
    uint8_t val;
} encvalue_t;

typedef struct spdbyte_s {
    char *name;
    uint8_t *data;
    int decimal;
    encvalue_t *values;
    char *units;
    char *description;
    char *deflt;
} spdbyte_t;

#define SPD_DEC_BCD	1
#define SPD_DEC_QTR	2
#define SPD_ENCODED	3
#define SPD_ENCODED2	4

/*  *********************************************************************
    *  Globals
    ********************************************************************* */


uint8_t spd[64] = {0};			/* SPD data */
uint8_t mintmemclk = MIN_tMEMCLK;	/* Default value: 8.0ns */
uint8_t roundtrip = tROUNDTRIP;		/* Default value: 2.5ns */
uint8_t dramtype = JEDEC;		/* Regular DDR SDRAMs */
uint8_t plldiv = 10;			/* 500 MHz using 100Mhz refclk */
uint8_t refclk = 100;			/* 100Mhz reference clock */
uint8_t portintlv = 0;			/* no port interleaving */

uint8_t addrskew = 0x8;
uint8_t dqoskew = 0x8;
uint8_t dqiskew = 0x8;
uint8_t addrdrive = 0xF;
uint8_t datadrive = 0xF;
uint8_t clkdrive = 0xF;

uint64_t mc0_mclkcfg;			/* Value programmed by draminit */
uint64_t mc0_timing1;			/* Value programmed by draminit */
uint64_t smbus0_start = 0;		/* Rememberd SMBus register value */
uint64_t smbus0_cmd = 0;		/* Rememberd SMBus register value */

extern int sb1250_refclk;		/* from draminit - reference clock */
extern int dram_cas_latency;		/* from draminit - calc'd cas latency */
extern int dram_tMemClk;		/* from draminit - calc'd tMemClk */

draminittab_t inittab[16];		/* our init tab */

int debug = 0;

/*  *********************************************************************
    *  Parameter and value tables
    ********************************************************************* */

encvalue_t caslatencies[] = {
    {"3.5",JEDEC_CASLAT_35},
    {"3.0",JEDEC_CASLAT_30},
    {"2.5",JEDEC_CASLAT_25},
    {"2.0",JEDEC_CASLAT_20},
    {"1.5",JEDEC_CASLAT_15},
    {"1.0",JEDEC_CASLAT_10},
    {NULL,0}};

encvalue_t refreshrates[] = {
    {"64",JEDEC_RFSH_64khz},
    {"256",JEDEC_RFSH_256khz},
    {"128",JEDEC_RFSH_128khz},
    {"32",JEDEC_RFSH_32khz},
    {"16",JEDEC_RFSH_16khz},
    {"8",JEDEC_RFSH_8khz},
    {NULL,0}};

encvalue_t modattribs[] = {
    {"none",0},
    {"reg",JEDEC_ATTRIB_REG},
    {"diffclk",0x20},
    {NULL,0}};

encvalue_t dramtypes[] = {
    {"jedec",JEDEC},
    {"fcram",FCRAM},
    {"sgram",SGRAM},
    {NULL,0}};

spdbyte_t spdfields[] = {
    {"mintmemclk",&mintmemclk,SPD_DEC_BCD,NULL,"ns","Minimum value for tMEMCLK","8.0"},
    {"roundtrip", &roundtrip, SPD_DEC_BCD,NULL,"ns","Round trip time from CLK to returned DQS","2.5"},
    {"plldiv",    &plldiv,    0,NULL,"","PLL Ratio (System Config Register)","10"},
    {"refclk",    &refclk,    0,NULL,"Mhz","Reference clock, usually 100Mhz","100"},
//    {"portintlv", &portintlv, 0,NULL,"","Port interleave (1=on)","0"},
    {"memtype",   &dramtype,  SPD_ENCODED,dramtypes,"","Memory type (jedec, fcram, sgram)","jedec"},
    {"rows",      &spd[JEDEC_SPD_ROWS],0,NULL,"","[3 ] Number of row bits","13"},
    {"cols",      &spd[JEDEC_SPD_COLS],0,NULL,"","[4 ] Number of column bits","9"},
    {"banks",     &spd[JEDEC_SPD_BANKS],0,NULL,"","[17] Number of banks","4"},
    {"tCK25",     &spd[JEDEC_SPD_tCK25],SPD_DEC_BCD,NULL,"ns","[9 ] tCK value for CAS Latency 2.5","7.5"},
    {"tCK20",     &spd[JEDEC_SPD_tCK20],SPD_DEC_BCD,NULL,"ns","[23] tCK value for CAS Latency 2.0","0"},
    {"tCK10",     &spd[JEDEC_SPD_tCK10],SPD_DEC_BCD,NULL,"ns","[25] tCK value for CAS Latency 1.0","0"},
    {"rfsh",      &spd[JEDEC_SPD_RFSH],SPD_ENCODED,refreshrates,"","[12] Refresh rate (KHz)","8"},
    {"caslat",    &spd[JEDEC_SPD_CASLATENCIES],SPD_ENCODED2,caslatencies,"","[18] CAS Latencies supported","2.5"},
    {"attrib",    &spd[JEDEC_SPD_ATTRIBUTES],SPD_ENCODED,modattribs,"","[21] Module attributes","none"},
    {"tRAS",      &spd[JEDEC_SPD_tRAS],0,NULL,"ns","[30]","45"},
    {"tRP",       &spd[JEDEC_SPD_tRP],SPD_DEC_QTR,NULL,"ns","[27]","20.0"},
    {"tRRD",      &spd[JEDEC_SPD_tRRD],SPD_DEC_QTR,NULL,"ns","[28]","15.0"},
    {"tRCD",      &spd[JEDEC_SPD_tRCD],SPD_DEC_QTR,NULL,"ns","[29]","20.0"},
    {"tRFC",      &spd[JEDEC_SPD_tRFC],0,NULL,"ns","[42]","0"},
    {"tRC",       &spd[JEDEC_SPD_tRC],0,NULL,"ns","[41]","0"},

    {"addrskew",  &addrskew, 0, NULL, "","Address Skew","0x0F"},
    {"dqoskew",   &dqoskew, 0, NULL, "","DQO Skew","0x08"},
    {"dqikew",    &dqiskew, 0, NULL, "","DQI Skew","0x08"},
    {"addrdrive", &addrdrive, 0, NULL, "","Address Drive","0x0F"},
    {"datadrive", &datadrive, 0, NULL, "","Data Drive","0x0F"},
    {"clkdrive",  &clkdrive, 0, NULL, "","Clock Drive","0"},
    {NULL,0,0,NULL,NULL,NULL,NULL}};

char *lookupstr(encvalue_t *ev,uint8_t val)
{
    while (ev->name) {
	if (ev->val == val) return ev->name;
	ev++;
	}
    return "unknown";
}

uint64_t sbreadcsr(uint64_t reg)
{
    uint64_t val = 0;

    if (debug) printf("READ %08X\n",(uint32_t) reg);

    switch ((uint32_t) reg) {
	case A_SCD_SYSTEM_REVISION:
	    val = V_SYS_PART(0x1250) | V_SYS_WID(0) | V_SYS_REVISION(1) | 0xFF;
	    break;
	case A_SCD_SYSTEM_CFG:
	    val = V_SYS_PLL_DIV(plldiv);
	    break;
	case A_SMB_STATUS_0:
	    val = 0;
	    break;
	case A_SMB_CMD_0:
	    val = smbus0_cmd;
	    break;
	case A_SMB_START_0:
	    val = smbus0_start;
	    break;
	case A_SMB_DATA_0:
	    val = spd[smbus0_cmd & 0x3F];
	    break;
	}
    return val;
}

void sbwritecsr(uint64_t reg,uint64_t val)
{
    if (debug) printf("WRITE %08X  %016llX\n",(uint32_t) reg,val);

    switch ((uint32_t) reg) {
	case A_MC_REGISTER(0,R_MC_MCLK_CFG):
	    mc0_mclkcfg = val;
	    break;
	case A_MC_REGISTER(0,R_MC_TIMING1):
	    mc0_timing1 = val;
	    break;
	case A_SMB_CMD_0:
	    smbus0_cmd = val;
	    break;
	case A_SMB_START_0:
	    smbus0_start = val;
	    break;
	}
}


int procfield(char *txt)
{
    int num = 0;
    int a,b;
    spdbyte_t *sf;
    encvalue_t *ev;
    char *x;
    char *tok;

    x = strchr(txt,'=');
    if (!x) {
	printf("Fields must be specified as 'name=value'\n");
	exit(1);
	}
    *x++ = '\0';

    sf = spdfields;
    while (sf->name) {
	if (strcmp(sf->name,txt) == 0) break;
	sf++;
	}

    if (sf->name == NULL) {
	printf("Invalid field name: %s\n",txt);
	return -1;
	}

    if (memcmp(x,"0x",2) == 0) {
	sscanf(x+2,"%x",&num);
	}
    else {
	if (strchr(x,'.')) {
	    if (sscanf(x,"%d.%d",&a,&b) != 2) {
		printf("%s: invalid number: %s\n",sf->name,x);
		return -1;
		}
	    }
	else {
	    a = atoi(x);
	    b = 0;
	    }

	switch (sf->decimal) {
	    case SPD_DEC_BCD:
		if ((b < 0) || (b > 9)) {
		    printf("%s: Invalid BCD number: %s\n",sf->name,x);
		    return -1;
		    }
		num = (a*16)+b;
		break;
	    case SPD_DEC_QTR:
		if ((b != 0) && (b != 25) && (b != 50) && (b != 75)) {
		    printf("%s: Invalid 2-bit fraction number: %s\n",sf->name,x);
		    printf("(number after decimal should be 0,25,50,75)\n");
		    exit(1);
		    }
		num = (a*4)+(b/25);
		break;
	    case SPD_ENCODED:
		ev = sf->values;
		while (ev->name) {
		    if (strcmp(ev->name,x) == 0) break;
		    ev++;
		    }
		if (!ev->name) {
		    printf("%s: Invalid value.  Valid values are: ",x);
		    ev = sf->values;
		    while (ev->name) { printf("%s ",ev->name); ev++; }
		    printf("\n");
		    return -1;
		    }
		num = ev->val;
		break;
	    case SPD_ENCODED2:
		tok = strtok(x," ,");
		num = 0;
		while (tok) {
		    ev = sf->values;
		    while (ev->name) {
			if (strcmp(ev->name,tok) == 0) break;
			ev++;
			}
		    if (!ev->name) {
			printf("%s: Invalid value.  Valid values are: ",tok);
			ev = sf->values;
			while (ev->name) { printf("%s ",ev->name); ev++; }
			printf("\n");
			return -1;
			}
		    num |= ev->val;
		    tok = strtok(NULL," ,");
		    }
		break;
	    default:
		num = a;
		break;
	    }
	}

    *(sf->data) = num;

    return 0;
}

void interactive(void)
{
    spdbyte_t *sf;
    char field[100];
    char ask[100];
    char prompt[100];
    char *x;

    sf = spdfields;

    printf("%-65.65s: Value\n","Parameter");
    printf("%-65.65s: -----\n","-----------------------------------------------------------------");

    while (sf->name) {
	for (;;) {
	    x = prompt;
	    x += sprintf(x,"%s (%s", sf->name,sf->description);
	    if (sf->units && sf->units[0]) {
		if (sf->description && sf->description[0]) x += sprintf(x,", ");
		x += sprintf(x,"%s",sf->units);
		}
	    x += sprintf(x,"): [%s]", sf->deflt);
	    printf("%-65.65s: ",prompt);

	    fgets(ask,sizeof(ask),stdin);
	    if ((x = strchr(ask,'\n'))) *x = '\0';
	    if (ask[0] == 0) strcpy(ask,sf->deflt);
	    sprintf(field,"%s=%s",sf->name,ask);
	    if (procfield(field) < 0) continue;
	    break;
	    }
	sf++;
	}

    printf("\n\n");
}

int swcnt = 0;
char *swnames[32];

int proc_args(int argc,char *argv[])
{
    int inidx,outidx;

    outidx = 1;

    for (inidx = 1; inidx < argc; inidx++) {
	if (argv[inidx][0] != '-') {
	    argv[outidx++] = argv[inidx];
	    }
	else {
	    swnames[swcnt] = argv[inidx];
	    swcnt++;
	    }
	}

    return outidx;
}

int swisset(char *x) 
{
    int idx;

    for (idx = 0; idx < swcnt; idx++) {
	if (strcmp(x,swnames[idx]) == 0) return 1;
	}
    return 0;
}

void dumpmclkcfg(uint64_t val)
{
    printf("clk_ratio = %d\n",G_MC_CLK_RATIO(val));
    printf("ref_rate  = %d\n",G_MC_REF_RATE(val));

}

void dumptiming1(uint64_t val)
{
    printf("w2rIdle  = %d\n",(val & M_MC_w2rIDLE_TWOCYCLES) ? 1 : 0);
    printf("r2rIdle  = %d\n",(val & M_MC_r2rIDLE_TWOCYCLES) ? 1 : 0);
    printf("r2wIdle  = %d\n",(val & M_MC_r2wIDLE_TWOCYCLES) ? 1 : 0);
    printf("tCrD     = %d\n",(int)G_MC_tCrD(val));
    printf("tCrDh    = %d\n",(val & M_MC_tCrDh) ? 1 : 0);
    printf("tFIFO    = %d\n",(int)G_MC_tFIFO(val));
    printf("tCwD     = %d\n",(int)G_MC_tCwD(val));

    printf("tRP      = %d\n",(int)G_MC_tRP(val));
    printf("tRRD     = %d\n",(int)G_MC_tRRD(val));
    printf("tRCD     = %d\n",(int)G_MC_tRCD(val));

    printf("tRFC     = %d\n",(int)G_MC_tRFC(val));
    printf("tRCw     = %d\n",(int)G_MC_tRCw(val));
    printf("tRCr     = %d\n",(int)G_MC_tRCr(val));
    printf("tCwCr    = %d\n",(int)G_MC_tCwCr(val));
}

int main(int argc,char *argv[])
{
    spdbyte_t *sf;
    uint8_t t;
    int idx;
    int mclk;
    draminittab_t *init;

    spd[JEDEC_SPD_MEMTYPE] = JEDEC_MEMTYPE_DDRSDRAM2;
    spd[JEDEC_SPD_ROWS]	   = 13;
    spd[JEDEC_SPD_COLS]    = 9;
    spd[JEDEC_SPD_BANKS]   = 2;
    spd[JEDEC_SPD_SIDES]   = 1;
    spd[JEDEC_SPD_WIDTH]   = 72;

    argc = proc_args(argc,argv);

    if ((argc == 1) && !swisset("-i")) {
	printf("usage: memconfig name=value name=value ...\n");
	printf("\n");
	printf("Available fields: ");
	sf = spdfields;
	while (sf->name) {
	    printf("%s ",sf->name);
	    sf++;
	    }
	printf("\n");
	exit(1);
	}

    if (swisset("-i")) {
	interactive();
	}
    else {
	for (idx = 1; idx < argc; idx++) {
	    if (procfield(argv[idx]) < 0) exit(1);
	    }
	}

    debug = swisset("-d");

    printf("-------Memory Parameters---------\n");

    sf = spdfields;
    while (sf->name) {
	char buffer[64];
	char *p = buffer;

	t = *(sf->data);
	printf("%-10.10s = 0x%02X  ",sf->name,t);
	switch (sf->decimal) {
	    case SPD_DEC_BCD:
		p += sprintf(p,"(%d.%d)",
		       t >> 4, t & 0x0F);
		break;
	    case SPD_DEC_QTR:
		p += sprintf(p,"(%d.%02d)",
		       t/4,(t&3)*25);
		break;
	    case SPD_ENCODED:
		p += sprintf(p,"(%s)",lookupstr(sf->values,t));
		break;
	    default:
		p += sprintf(p,"(%d)",t);
		break;
	    }

	p += sprintf(p," %s",sf->units);
	printf("%-16.16s  %s\n",buffer,sf->description);
	sf++;
	}

    printf("\n");

    init = &inittab[0];
    memset(inittab,0,sizeof(inittab));

    init->gbl.gbl_type = MCR_GLOBALS;
    init->gbl.gbl_intlv_ch = portintlv;
    init++;

    init->cfg.cfg_type = MCR_CHCFG;
    init->cfg.cfg_chan = 0;
    init->cfg.cfg_mintmemclk = mintmemclk;
    init->cfg.cfg_dramtype = dramtype;
    init->cfg.cfg_pagepolicy = CASCHECK;
    init->cfg.cfg_blksize = BLKSIZE32;
    init->cfg.cfg_intlv_cs = NOCSINTLV;
    init->cfg.cfg_ecc = 0;
    init->cfg.cfg_roundtrip = roundtrip;
    init++;

    init->clk.clk_type = MCR_CLKCFG;
    init->clk.clk_addrskew = addrskew;
    init->clk.clk_dqoskew = dqoskew;
    init->clk.clk_dqiskew = dqiskew;
    init->clk.clk_addrdrive = addrdrive;
    init->clk.clk_datadrive = datadrive;
    init->clk.clk_clkdrive = clkdrive;
    init++;

    init->geom.geom_type = MCR_GEOM;
    init->geom.geom_csel = 0;
    init->geom.geom_rows = spd[JEDEC_SPD_ROWS];
    init->geom.geom_cols = spd[JEDEC_SPD_COLS];
    init->geom.geom_banks = spd[JEDEC_SPD_BANKS];
    init++;

    init->spd.spd_type = MCR_SPD;
    init->spd.spd_csel = 0;
    init->spd.spd_flags = 0;
    init->spd.spd_smbuschan = 0;
    init->spd.spd_smbusdev = 0x50;
    init++;

    init->mcr.mcr_type = MCR_EOT;


    sb1250_refclk = (int) refclk;

    sb1250_dram_init(inittab);


    printf("-----Memory Timing Register Values-----\n");
    printf("System Clock    %dMHz\n",plldiv*refclk/2);
    printf("CAS latency     %d.%d\n",dram_cas_latency>>1,(dram_cas_latency&1)?5:0);
    printf("tMemClk         %d.%d ns\n",dram_tMemClk/10,dram_tMemClk%10);
    mclk = (plldiv*refclk)*10/2/((int)G_MC_CLK_RATIO(mc0_mclkcfg));
    printf("MCLK Freq       %d.%dMHz\n",mclk/10,mclk%10);
    printf("\n");
    printf("MC_TIMING1  = %016llX\n",mc0_timing1);
    printf("MCLK_CONFIG = %016llX\n",mc0_mclkcfg);
    printf("\n");

    printf("-----Memory Timing Register Fields-----\n");
    dumptiming1(mc0_timing1);

    printf("-----Memory Clock Config Register Fields-----\n");
    dumpmclkcfg(mc0_mclkcfg);

    printf("---Done!---\n");

    printf("%s ",argv[0]);
    sf = spdfields;
    while (sf->name) {
	char buffer[64];
	char *p = buffer;

	t = *(sf->data);

	p += sprintf(p,"%s=",sf->name);
	switch (sf->decimal) {
	    case SPD_DEC_BCD:
		p += sprintf(p,"%d.%d",
		       t >> 4, t & 0x0F);
		break;
	    case SPD_DEC_QTR:
		p += sprintf(p,"%d.%02d",
		       t/4,(t&3)*25);
		break;
	    case SPD_ENCODED:
	    default:
		p += sprintf(p,"0x%02X",t);
		break;
	    }

	printf("%s ",buffer);
	sf++;
	}

    printf("\n");

    return 0;
}
