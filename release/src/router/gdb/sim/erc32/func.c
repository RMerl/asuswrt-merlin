/*
 * func.c, misc simulator functions. This file is part of SIS.
 * 
 * SIS, SPARC instruction simulator V1.8 Copyright (C) 1995 Jiri Gaisler,
 * European Space Agency
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "sis.h"
#include "end.h"
#include <dis-asm.h>
#include "sim-config.h"


#define	VAL(x)	strtoul(x,(char **)NULL,0)

extern int	current_target_byte_order;
struct disassemble_info dinfo;
struct pstate   sregs;
extern struct estate ebase;
int             ctrl_c = 0;
int             sis_verbose = 0;
char           *sis_version = "2.7.5";
int             nfp = 0;
int             ift = 0;
int             wrp = 0;
int             rom8 = 0;
int             uben = 0;
int		termsave;
int             sparclite = 0;		/* emulating SPARClite instructions? */
int             sparclite_board = 0;	/* emulating SPARClite board RAM? */
char            uart_dev1[128] = "";
char            uart_dev2[128] = "";
extern	int	ext_irl;
uint32		last_load_addr = 0;

#ifdef ERRINJ
uint32		errcnt = 0;
uint32		errper = 0;
uint32		errtt = 0;
uint32		errftt = 0;
uint32		errmec = 0;
#endif

/* Forward declarations */

static int	batch PARAMS ((struct pstate *sregs, char *fname));
static void	set_rega PARAMS ((struct pstate *sregs, char *reg, uint32 rval));
static void	disp_reg PARAMS ((struct pstate *sregs, char *reg));
static uint32	limcalc PARAMS ((float32 freq));
static void	int_handler PARAMS ((int32 sig));
static void	init_event PARAMS ((void));
static int	disp_fpu PARAMS ((struct pstate  *sregs));
static void	disp_regs PARAMS ((struct pstate  *sregs, int cwp));
static void	disp_ctrl PARAMS ((struct pstate *sregs));
static void	disp_mem PARAMS ((uint32 addr, uint32 len));

static int 
batch(sregs, fname)
    struct pstate  *sregs;
    char           *fname;
{
    FILE           *fp;
    char            lbuf[1024];

    if ((fp = fopen(fname, "r")) == NULL) {
	fprintf(stderr, "couldn't open batch file %s\n", fname);
	return (0);
    }
    while (!feof(fp)) {
	lbuf[0] = 0;
	fgets(lbuf, 1023, fp);
	if ((strlen(lbuf) > 0) && (lbuf[strlen(lbuf) - 1] == '\n'))
	    lbuf[strlen(lbuf) - 1] = 0;
	printf("sis> %s\n", lbuf);
	exec_cmd(sregs, lbuf);
    }
    fclose(fp);
    return (1);
}

void
set_regi(sregs, reg, rval)
    struct pstate  *sregs;
    int32           reg;
    uint32          rval;
{
    uint32          cwp;

    cwp = ((sregs->psr & 0x7) << 4);
    if ((reg > 0) && (reg < 8)) {
	sregs->g[reg] = rval;
    } else if ((reg >= 8) && (reg < 32)) {
	sregs->r[(cwp + reg) & 0x7f] = rval;
    } else if ((reg >= 32) && (reg < 64)) {
	sregs->fsi[reg - 32] = rval;
    } else {
	switch (reg) {
	case 64:
	    sregs->y = rval;
	    break;
	case 65:
	    sregs->psr = rval;
	    break;
	case 66:
	    sregs->wim = rval;
	    break;
	case 67:
	    sregs->tbr = rval;
	    break;
	case 68:
	    sregs->pc = rval;
	    break;
	case 69:
	    sregs->npc = rval;
	    break;
	case 70:
	    sregs->fsr = rval;
	    set_fsr(rval);
	    break;
    default:break;
	}
    }
}

void
get_regi(struct pstate * sregs, int32 reg, char *buf)
{
    uint32          cwp;
    uint32          rval = 0;

    cwp = ((sregs->psr & 0x7) << 4);
    if ((reg >= 0) && (reg < 8)) {
	rval = sregs->g[reg];
    } else if ((reg >= 8) && (reg < 32)) {
	rval = sregs->r[(cwp + reg) & 0x7f];
    } else if ((reg >= 32) && (reg < 64)) {
	rval = sregs->fsi[reg - 32];
    } else {
	switch (reg) {
	case 64:
	    rval = sregs->y;
	    break;
	case 65:
	    rval = sregs->psr;
	    break;
	case 66:
	    rval = sregs->wim;
	    break;
	case 67:
	    rval = sregs->tbr;
	    break;
	case 68:
	    rval = sregs->pc;
	    break;
	case 69:
	    rval = sregs->npc;
	    break;
	case 70:
	    rval = sregs->fsr;
	    break;
    default:break;
	}
    }
    if (current_target_byte_order == BIG_ENDIAN) {
	buf[0] = (rval >> 24) & 0x0ff;
	buf[1] = (rval >> 16) & 0x0ff;
	buf[2] = (rval >> 8) & 0x0ff;
	buf[3] = rval & 0x0ff;
    }
    else {
	buf[3] = (rval >> 24) & 0x0ff;
	buf[2] = (rval >> 16) & 0x0ff;
	buf[1] = (rval >> 8) & 0x0ff;
	buf[0] = rval & 0x0ff;
    }
}


static void
set_rega(sregs, reg, rval)
    struct pstate  *sregs;
    char           *reg;
    uint32          rval;
{
    uint32          cwp;
    int32           err = 0;

    cwp = ((sregs->psr & 0x7) << 4);
    if (strcmp(reg, "psr") == 0)
	sregs->psr = (rval = (rval & 0x00f03fff));
    else if (strcmp(reg, "tbr") == 0)
	sregs->tbr = (rval = (rval & 0xfffffff0));
    else if (strcmp(reg, "wim") == 0)
	sregs->wim = (rval = (rval & 0x0ff));
    else if (strcmp(reg, "y") == 0)
	sregs->y = rval;
    else if (strcmp(reg, "pc") == 0)
	sregs->pc = rval;
    else if (strcmp(reg, "npc") == 0)
	sregs->npc = rval;
    else if (strcmp(reg, "fsr") == 0) {
	sregs->fsr = rval;
	set_fsr(rval);
    } else if (strcmp(reg, "g0") == 0)
	err = 2;
    else if (strcmp(reg, "g1") == 0)
	sregs->g[1] = rval;
    else if (strcmp(reg, "g2") == 0)
	sregs->g[2] = rval;
    else if (strcmp(reg, "g3") == 0)
	sregs->g[3] = rval;
    else if (strcmp(reg, "g4") == 0)
	sregs->g[4] = rval;
    else if (strcmp(reg, "g5") == 0)
	sregs->g[5] = rval;
    else if (strcmp(reg, "g6") == 0)
	sregs->g[6] = rval;
    else if (strcmp(reg, "g7") == 0)
	sregs->g[7] = rval;
    else if (strcmp(reg, "o0") == 0)
	sregs->r[(cwp + 8) & 0x7f] = rval;
    else if (strcmp(reg, "o1") == 0)
	sregs->r[(cwp + 9) & 0x7f] = rval;
    else if (strcmp(reg, "o2") == 0)
	sregs->r[(cwp + 10) & 0x7f] = rval;
    else if (strcmp(reg, "o3") == 0)
	sregs->r[(cwp + 11) & 0x7f] = rval;
    else if (strcmp(reg, "o4") == 0)
	sregs->r[(cwp + 12) & 0x7f] = rval;
    else if (strcmp(reg, "o5") == 0)
	sregs->r[(cwp + 13) & 0x7f] = rval;
    else if (strcmp(reg, "o6") == 0)
	sregs->r[(cwp + 14) & 0x7f] = rval;
    else if (strcmp(reg, "o7") == 0)
	sregs->r[(cwp + 15) & 0x7f] = rval;
    else if (strcmp(reg, "l0") == 0)
	sregs->r[(cwp + 16) & 0x7f] = rval;
    else if (strcmp(reg, "l1") == 0)
	sregs->r[(cwp + 17) & 0x7f] = rval;
    else if (strcmp(reg, "l2") == 0)
	sregs->r[(cwp + 18) & 0x7f] = rval;
    else if (strcmp(reg, "l3") == 0)
	sregs->r[(cwp + 19) & 0x7f] = rval;
    else if (strcmp(reg, "l4") == 0)
	sregs->r[(cwp + 20) & 0x7f] = rval;
    else if (strcmp(reg, "l5") == 0)
	sregs->r[(cwp + 21) & 0x7f] = rval;
    else if (strcmp(reg, "l6") == 0)
	sregs->r[(cwp + 22) & 0x7f] = rval;
    else if (strcmp(reg, "l7") == 0)
	sregs->r[(cwp + 23) & 0x7f] = rval;
    else if (strcmp(reg, "i0") == 0)
	sregs->r[(cwp + 24) & 0x7f] = rval;
    else if (strcmp(reg, "i1") == 0)
	sregs->r[(cwp + 25) & 0x7f] = rval;
    else if (strcmp(reg, "i2") == 0)
	sregs->r[(cwp + 26) & 0x7f] = rval;
    else if (strcmp(reg, "i3") == 0)
	sregs->r[(cwp + 27) & 0x7f] = rval;
    else if (strcmp(reg, "i4") == 0)
	sregs->r[(cwp + 28) & 0x7f] = rval;
    else if (strcmp(reg, "i5") == 0)
	sregs->r[(cwp + 29) & 0x7f] = rval;
    else if (strcmp(reg, "i6") == 0)
	sregs->r[(cwp + 30) & 0x7f] = rval;
    else if (strcmp(reg, "i7") == 0)
	sregs->r[(cwp + 31) & 0x7f] = rval;
    else
	err = 1;
    switch (err) {
    case 0:
	printf("%s = %d (0x%08x)\n", reg, rval, rval);
	break;
    case 1:
	printf("no such regiser: %s\n", reg);
	break;
    case 2:
	printf("cannot set g0\n");
	break;
    default:
	break;
    }

}

static void
disp_reg(sregs, reg)
    struct pstate  *sregs;
    char           *reg;
{
    if (strncmp(reg, "w",1) == 0)
	disp_regs(sregs, VAL(&reg[1]));
}

#ifdef ERRINJ

void
errinj()
{
    int	err;

    switch (err = (random() % 12)) {
	case 0: errtt = 0x61; break;
	case 1: errtt = 0x62; break;
	case 2: errtt = 0x63; break;
	case 3: errtt = 0x64; break;
	case 4: errtt = 0x65; break;
	case 5: 
	case 6: 
	case 7: errftt = err; 
		break;
	case 8: errmec = 1; break;
	case 9: errmec = 2; break;
	case 10: errmec = 5; break;
	case 11: errmec = 6; break;
    }
    errcnt++;
    if (errper) event(errinj, 0, (random()%errper));
}

void
errinjstart()
{
    if (errper) event(errinj, 0, (random()%errper));
}

#endif

static uint32
limcalc (freq)
    float32		freq;
{
    uint32          unit, lim;
    double	    flim;
    char           *cmd1, *cmd2;

    unit = 1;
    lim = -1;
    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
        lim = VAL(cmd1);
        if ((cmd2 = strtok(NULL, " \t\n\r")) != NULL) {
            if (strcmp(cmd2,"us")==0) unit = 1;
      	    if (strcmp(cmd2,"ms")==0) unit = 1000;
            if (strcmp(cmd2,"s")==0)  unit = 1000000;
        }
        flim = (double) lim * (double) unit * (double) freq + 
	   (double) ebase.simtime;
        if ((flim > ebase.simtime) && (flim < 4294967296.0)) {
            lim = (uint32) flim;
        } else  {
            printf("error in expression\n");
            lim = -1;
        }
    }
    return (lim);
}
    
int
exec_cmd(sregs, cmd)
    char           *cmd;
    struct pstate  *sregs;
{
    char           *cmd1, *cmd2;
    int32           stat;
    uint32          len, i, clen, j;
    static uint32   daddr = 0;
    char           *cmdsave;

    stat = OK;
    cmdsave = strdup(cmd);
    if ((cmd1 = strtok(cmd, " \t")) != NULL) {
	clen = strlen(cmd1);
	if (strncmp(cmd1, "bp", clen) == 0) {
	    for (i = 0; i < sregs->bptnum; i++) {
		printf("  %d : 0x%08x\n", i + 1, sregs->bpts[i]);
	    }
	} else if (strncmp(cmd1, "+bp", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
		sregs->bpts[sregs->bptnum] = VAL(cmd1) & ~0x3;
		printf("added breakpoint %d at 0x%08x\n",
		       sregs->bptnum + 1, sregs->bpts[sregs->bptnum]);
		sregs->bptnum += 1;
	    }
	} else if (strncmp(cmd1, "-bp", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
		i = VAL(cmd1) - 1;
		if ((i >= 0) && (i < sregs->bptnum)) {
		    printf("deleted breakpoint %d at 0x%08x\n", i + 1,
			   sregs->bpts[i]);
		    for (; i < sregs->bptnum - 1; i++) {
			sregs->bpts[i] = sregs->bpts[i + 1];
		    }
		    sregs->bptnum -= 1;
		}
	    }
	} else if (strncmp(cmd1, "batch", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) == NULL) {
		printf("no file specified\n");
	    } else {
		batch(sregs, cmd1);
	    }
	} else if (strncmp(cmd1, "cont", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) == NULL) {
		stat = run_sim(sregs, -1, 0);
	    } else {
		stat = run_sim(sregs, VAL(cmd1), 0);
	    }
	    daddr = sregs->pc;
	    sim_halt();
	} else if (strncmp(cmd1, "debug", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
		sis_verbose = VAL(cmd1);
	    }
	    printf("Debug level = %d\n",sis_verbose);
	} else if (strncmp(cmd1, "dis", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
		daddr = VAL(cmd1);
	    }
	    if ((cmd2 = strtok(NULL, " \t\n\r")) != NULL) {
		len = VAL(cmd2);
	    } else
		len = 16;
	    printf("\n");
	    dis_mem(daddr, len, &dinfo);
	    printf("\n");
	    daddr += len * 4;
	} else if (strncmp(cmd1, "echo", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
		printf("%s\n", (&cmdsave[clen+1]));
	    }
#ifdef ERRINJ
	} else if (strncmp(cmd1, "error", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
		errper = VAL(cmd1);
	        if (errper) {
		    event(errinj, 0, (len = (random()%errper)));
		    printf("Error injection started with period %d\n",len);
	        } 
	     } else printf("Injected errors: %d\n",errcnt);
#endif
	} else if (strncmp(cmd1, "float", clen) == 0) {
	    stat = disp_fpu(sregs);
	} else if (strncmp(cmd1, "go", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) == NULL) {
		len = last_load_addr;
	    } else {
		len = VAL(cmd1);
	    }
	    sregs->pc = len & ~3;
	    sregs->npc = sregs->pc + 4;
	    printf("resuming at 0x%08x\n",sregs->pc);
	    if ((cmd2 = strtok(NULL, " \t\n\r")) != NULL) {
		stat = run_sim(sregs, VAL(cmd2), 0);
	    } else {
		stat = run_sim(sregs, -1, 0);
	    }
	    daddr = sregs->pc;
	    sim_halt();
	} else if (strncmp(cmd1, "help", clen) == 0) {
	    gen_help();
	} else if (strncmp(cmd1, "history", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
		sregs->histlen = VAL(cmd1);
		if (sregs->histbuf != NULL)
		    free(sregs->histbuf);
		sregs->histbuf = (struct histype *) calloc(sregs->histlen, sizeof(struct histype));
		printf("trace history length = %d\n\r", sregs->histlen);
		sregs->histind = 0;

	    } else {
		j = sregs->histind;
		for (i = 0; i < sregs->histlen; i++) {
		    if (j >= sregs->histlen)
			j = 0;
		    printf(" %8d ", sregs->histbuf[j].time);
		    dis_mem(sregs->histbuf[j].addr, 1, &dinfo);
		    j++;
		}
	    }

	} else if (strncmp(cmd1, "load", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
		last_load_addr = bfd_load(cmd1);
		while ((cmd1 = strtok(NULL, " \t\n\r")) != NULL)
		    last_load_addr = bfd_load(cmd1);
	    } else {
		printf("load: no file specified\n");
	    }
	} else if (strncmp(cmd1, "mem", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL)
		daddr = VAL(cmd1);
	    if ((cmd2 = strtok(NULL, " \t\n\r")) != NULL)
		len = VAL(cmd2);
	    else
		len = 64;
	    disp_mem(daddr, len);
	    daddr += len;
	} else if (strncmp(cmd1, "perf", clen) == 0) {
	    cmd1 = strtok(NULL, " \t\n\r");
	    if ((cmd1 != NULL) &&
		(strncmp(cmd1, "reset", strlen(cmd1)) == 0)) {
		reset_stat(sregs);
	    } else
		show_stat(sregs);
	} else if (strncmp(cmd1, "quit", clen) == 0) {
	    exit(0);
	} else if (strncmp(cmd1, "reg", clen) == 0) {
	    cmd1 = strtok(NULL, " \t\n\r");
	    cmd2 = strtok(NULL, " \t\n\r");
	    if (cmd2 != NULL)
		set_rega(sregs, cmd1, VAL(cmd2));
	    else if (cmd1 != NULL)
		disp_reg(sregs, cmd1);
	    else {
		disp_regs(sregs,sregs->psr);
		disp_ctrl(sregs);
	    }
	} else if (strncmp(cmd1, "reset", clen) == 0) {
	    ebase.simtime = 0;
	    reset_all();
	    reset_stat(sregs);
	} else if (strncmp(cmd1, "run", clen) == 0) {
	    ebase.simtime = 0;
	    reset_all();
	    reset_stat(sregs);
	    if ((cmd1 = strtok(NULL, " \t\n\r")) == NULL) {
		stat = run_sim(sregs, -1, 0);
	    } else {
		stat = run_sim(sregs, VAL(cmd1), 0);
	    }
	    daddr = sregs->pc;
	    sim_halt();
	} else if (strncmp(cmd1, "shell", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) != NULL) {
		system(&cmdsave[clen]);
	    }
	} else if (strncmp(cmd1, "step", clen) == 0) {
	    stat = run_sim(sregs, 1, 1);
	    daddr = sregs->pc;
	    sim_halt();
	} else if (strncmp(cmd1, "tcont", clen) == 0) {
	    sregs->tlimit = limcalc(sregs->freq);
	    stat = run_sim(sregs, -1, 0);
	    daddr = sregs->pc;
	    sim_halt();
	} else if (strncmp(cmd1, "tgo", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) == NULL) {
		len = last_load_addr;
	    } else {
		len = VAL(cmd1);
	        sregs->tlimit = limcalc(sregs->freq);
	    }
	    sregs->pc = len & ~3;
	    sregs->npc = sregs->pc + 4;
	    printf("resuming at 0x%08x\n",sregs->pc);
	    stat = run_sim(sregs, -1, 0);
	    daddr = sregs->pc;
	    sim_halt();
	} else if (strncmp(cmd1, "tlimit", clen) == 0) {
	   sregs->tlimit = limcalc(sregs->freq);
	   if (sregs->tlimit != (uint32) -1)
              printf("simulation limit = %u (%.3f ms)\n",(uint32) sregs->tlimit,
		sregs->tlimit / sregs->freq / 1000);
	} else if (strncmp(cmd1, "tra", clen) == 0) {
	    if ((cmd1 = strtok(NULL, " \t\n\r")) == NULL) {
		stat = run_sim(sregs, -1, 1);
	    } else {
		stat = run_sim(sregs, VAL(cmd1), 1);
	    }
	    printf("\n");
	    daddr = sregs->pc;
	    sim_halt();
	} else if (strncmp(cmd1, "trun", clen) == 0) {
	    ebase.simtime = 0;
	    reset_all();
	    reset_stat(sregs);
	    sregs->tlimit = limcalc(sregs->freq);
	    stat = run_sim(sregs, -1, 0);
	    daddr = sregs->pc;
	    sim_halt();
	} else
	    printf("syntax error\n");
    }
    if (cmdsave != NULL)
	free(cmdsave);
    return (stat);
}


void
reset_stat(sregs)
    struct pstate  *sregs;
{
    sregs->tottime = 0;
    sregs->pwdtime = 0;
    sregs->ninst = 0;
    sregs->fholdt = 0;
    sregs->holdt = 0;
    sregs->icntt = 0;
    sregs->finst = 0;
    sregs->nstore = 0;
    sregs->nload = 0;
    sregs->nbranch = 0;
    sregs->simstart = ebase.simtime;

}

void
show_stat(sregs)
    struct pstate  *sregs;
{
    uint32          iinst;
    uint32          stime, tottime;

    if (sregs->tottime == 0) tottime = 1; else tottime = sregs->tottime;
    stime = ebase.simtime - sregs->simstart;	/* Total simulated time */
#ifdef STAT

    iinst = sregs->ninst - sregs->finst - sregs->nload - sregs->nstore -
	sregs->nbranch;
#endif

    printf("\n Cycles       : %9d\n\r", ebase.simtime - sregs->simstart);
    printf(" Instructions : %9d\n", sregs->ninst);

#ifdef STAT
    printf("   integer    : %9.2f %%\n", 100.0 * (float) iinst / (float) sregs->ninst);
    printf("   load       : %9.2f %%\n",
	   100.0 * (float) sregs->nload / (float) sregs->ninst);
    printf("   store      : %9.2f %%\n",
	   100.0 * (float) sregs->nstore / (float) sregs->ninst);
    printf("   branch     : %9.2f %%\n",
	   100.0 * (float) sregs->nbranch / (float) sregs->ninst);
    printf("   float      : %9.2f %%\n",
	   100.0 * (float) sregs->finst / (float) sregs->ninst);
    printf(" Integer CPI  : %9.2f\n",
	   ((float) (stime - sregs->pwdtime - sregs->fholdt - sregs->finst))
	   /
	   (float) (sregs->ninst - sregs->finst));
    printf(" Float CPI    : %9.2f\n",
	   ((float) sregs->fholdt / (float) sregs->finst) + 1.0);
#endif
    printf(" Overall CPI  : %9.2f\n",
	   (float) (stime - sregs->pwdtime) / (float) sregs->ninst);
    printf("\n ERC32 performance (%4.1f MHz): %5.2f MOPS (%5.2f MIPS, %5.2f MFLOPS)\n",
	   sregs->freq, sregs->freq * (float) sregs->ninst / (float) (stime - sregs->pwdtime),
	   sregs->freq * (float) (sregs->ninst - sregs->finst) /
	   (float) (stime - sregs->pwdtime),
     sregs->freq * (float) sregs->finst / (float) (stime - sregs->pwdtime));
    printf(" Simulated ERC32 time        : %5.2f ms\n", (float) (ebase.simtime - sregs->simstart) / 1000.0 / sregs->freq);
    printf(" Processor utilisation       : %5.2f %%\n", 100.0 * (1.0 - ((float) sregs->pwdtime / (float) stime)));
    printf(" Real-time / simulator-time  : 1/%.2f \n",
      ((float) sregs->tottime) / ((float) (stime) / (sregs->freq * 1.0E6)));
    printf(" Simulator performance       : %d KIPS\n",sregs->ninst/tottime/1000);
    printf(" Used time (sys + user)      : %3d s\n\n", sregs->tottime);
}



void
init_bpt(sregs)
    struct pstate  *sregs;
{
    sregs->bptnum = 0;
    sregs->histlen = 0;
    sregs->histind = 0;
    sregs->histbuf = NULL;
    sregs->tlimit = -1;
}

static void
int_handler(sig)
    int32           sig;
{
    if (sig != 2)
	printf("\n\n Signal handler error  (%d)\n\n", sig);
    ctrl_c = 1;
}

void
init_signals()
{
    typedef void    (*PFI) ();
    static PFI      int_tab[2];

    int_tab[0] = signal(SIGTERM, int_handler);
    int_tab[1] = signal(SIGINT, int_handler);
}


extern struct disassemble_info dinfo;

struct estate   ebase;
struct evcell   evbuf[EVENT_MAX];
struct irqcell  irqarr[16];

static int
disp_fpu(sregs)
    struct pstate  *sregs;
{

    int         i;
    float	t;

    printf("\n fsr: %08X\n\n", sregs->fsr);

#ifdef HOST_LITTLE_ENDIAN_FLOAT
    for (i = 0; i < 32; i++)
      sregs->fdp[i ^ 1] = sregs->fs[i];
#endif

    for (i = 0; i < 32; i++) {
	t = sregs->fs[i];
	printf(" f%02d  %08x  %14e  ", i, sregs->fsi[i], sregs->fs[i]);
	if (!(i & 1))
	    printf("%14e\n", sregs->fd[i >> 1]);
	else
	    printf("\n");
    }
    printf("\n");
    return (OK);
}

static void
disp_regs(sregs,cwp)
    struct pstate  *sregs;
    int cwp;
{

    int           i;

    cwp = ((cwp & 0x7) << 4);
    printf("\n\t  INS       LOCALS      OUTS     GLOBALS\n");
    for (i = 0; i < 8; i++) {
	printf("   %d:  %08X   %08X   %08X   %08X\n", i,
	       sregs->r[(cwp + i + 24) & 0x7f],
	    sregs->r[(cwp + i + 16) & 0x7f], sregs->r[(cwp + i + 8) & 0x7f],
	       sregs->g[i]);
    }
}

static void
disp_ctrl(sregs)
    struct pstate  *sregs;
{

    unsigned char           i[4];

    printf("\n psr: %08X   wim: %08X   tbr: %08X   y: %08X\n",
	   sregs->psr, sregs->wim, sregs->tbr, sregs->y);
    sis_memory_read(sregs->pc, i, 4);
    printf("\n  pc: %08X = %02X%02X%02X%02X    ", sregs->pc,i[0],i[1],i[2],i[3]);
    print_insn_sparc(sregs->pc, &dinfo);
    sis_memory_read(sregs->npc, i, 4);
    printf("\n npc: %08X = %02X%02X%02X%02X    ",sregs->npc,i[0],i[1],i[2],i[3]);
    print_insn_sparc(sregs->npc, &dinfo);
    if (sregs->err_mode)
	printf("\n IU in error mode");
    printf("\n\n");
}

static void
disp_mem(addr, len)
    uint32          addr;
    uint32          len;
{

    uint32          i;
    unsigned char   data[4];
    uint32          mem[4], j;
    char           *p;

    for (i = addr & ~3; i < ((addr + len) & ~3); i += 16) {
	printf("\n %8X  ", i);
	for (j = 0; j < 4; j++) {
	    sis_memory_read((i + (j * 4)), data, 4);
	    printf("%02x%02x%02x%02x  ", data[0],data[1],data[2],data[3]);
	    mem[j] = *((int *) &data);
	}
	printf("  ");
	p = (char *) mem;
	for (j = 0; j < 16; j++) {
	    if (isprint(p[j]))
		putchar(p[j]);
	    else
		putchar('.');
	}
    }
    printf("\n\n");
}

void
dis_mem(addr, len, info)
    uint32          addr;
    uint32          len;
    struct disassemble_info *info;
{
    uint32          i;
    unsigned char   data[4];

    for (i = addr & -3; i < ((addr & -3) + (len << 2)); i += 4) {
	sis_memory_read(i, data, 4);
	printf(" %08x  %02x%02x%02x%02x  ", i, data[0],data[1],data[2],data[3]);
	print_insn_sparc(i, info);
        if (i >= 0xfffffffc) break;
	printf("\n");
    }
}

/* Add event to event queue */

void
event(cfunc, arg, delta)
    void            (*cfunc) ();
    int32           arg;
    uint32          delta;
{
    struct evcell  *ev1, *evins;

    if (ebase.freeq == NULL) {
	printf("Error, too many events in event queue\n");
	return;
    }
    ev1 = &ebase.eq;
    delta += ebase.simtime;
    while ((ev1->nxt != NULL) && (ev1->nxt->time <= delta)) {
	ev1 = ev1->nxt;
    }
    if (ev1->nxt == NULL) {
	ev1->nxt = ebase.freeq;
	ebase.freeq = ebase.freeq->nxt;
	ev1->nxt->nxt = NULL;
    } else {
	evins = ebase.freeq;
	ebase.freeq = ebase.freeq->nxt;
	evins->nxt = ev1->nxt;
	ev1->nxt = evins;
    }
    ev1->nxt->time = delta;
    ev1->nxt->cfunc = cfunc;
    ev1->nxt->arg = arg;
}

#if 0	/* apparently not used */
void
stop_event()
{
}
#endif

void
init_event()
{
    int32           i;

    ebase.eq.nxt = NULL;
    ebase.freeq = evbuf;
    for (i = 0; i < EVENT_MAX; i++) {
	evbuf[i].nxt = &evbuf[i + 1];
    }
    evbuf[EVENT_MAX - 1].nxt = NULL;
}

void
set_int(level, callback, arg)
    int32           level;
    void            (*callback) ();
    int32           arg;
{
    irqarr[level & 0x0f].callback = callback;
    irqarr[level & 0x0f].arg = arg;
}

/* Advance simulator time */

void
advance_time(sregs)
    struct pstate  *sregs;
{

    struct evcell  *evrem;
    void            (*cfunc) ();
    uint32          arg, endtime;

#ifdef STAT
    sregs->fholdt += sregs->fhold;
    sregs->holdt += sregs->hold;
    sregs->icntt += sregs->icnt;
#endif

    endtime = ebase.simtime + sregs->icnt + sregs->hold + sregs->fhold;

    while ((ebase.eq.nxt->time <= (endtime)) && (ebase.eq.nxt != NULL)) {
	ebase.simtime = ebase.eq.nxt->time;
	cfunc = ebase.eq.nxt->cfunc;
	arg = ebase.eq.nxt->arg;
	evrem = ebase.eq.nxt;
	ebase.eq.nxt = ebase.eq.nxt->nxt;
	evrem->nxt = ebase.freeq;
	ebase.freeq = evrem;
	cfunc(arg);
    }
    ebase.simtime = endtime;

}

uint32
now()
{
    return(ebase.simtime);
}


/* Advance time until an external interrupt is seen */

int
wait_for_irq()
{
    struct evcell  *evrem;
    void            (*cfunc) ();
    int32           arg, endtime;

    if (ebase.eq.nxt == NULL)
	printf("Warning: event queue empty - power-down mode not entered\n");
    endtime = ebase.simtime;
    while (!ext_irl && (ebase.eq.nxt != NULL)) {
	ebase.simtime = ebase.eq.nxt->time;
	cfunc = ebase.eq.nxt->cfunc;
	arg = ebase.eq.nxt->arg;
	evrem = ebase.eq.nxt;
	ebase.eq.nxt = ebase.eq.nxt->nxt;
	evrem->nxt = ebase.freeq;
	ebase.freeq = evrem;
	cfunc(arg);
	if (ctrl_c) {
	    printf("\bwarning: power-down mode interrupted\n");
	    break;
	}
    }
    sregs.pwdtime += ebase.simtime - endtime;
    return (ebase.simtime - endtime);
}

int
check_bpt(sregs)
    struct pstate  *sregs;
{
    int32           i;

    if ((sregs->bphit) || (sregs->annul))
	return (0);
    for (i = 0; i < (int32) sregs->bptnum; i++) {
	if (sregs->pc == sregs->bpts[i])
	    return (BPT_HIT);
    }
    return (0);
}

void
reset_all()
{
    init_event();		/* Clear event queue */
    init_regs(&sregs);
    reset();
#ifdef ERRINJ
    errinjstart();
#endif
}

void
sys_reset()
{
    reset_all();
    sregs.trap = 256;		/* Force fake reset trap */
}

void
sys_halt()
{
    sregs.trap = 257;           /* Force fake halt trap */
}

#include "ansidecl.h"

#include <stdarg.h>

#include "libiberty.h"
#include "bfd.h"

#define min(A, B) (((A) < (B)) ? (A) : (B))
#define LOAD_ADDRESS 0

int
bfd_load(fname)
    char           *fname;
{
    asection       *section;
    bfd            *pbfd;
    const bfd_arch_info_type *arch;

    pbfd = bfd_openr(fname, 0);

    if (pbfd == NULL) {
	printf("open of %s failed\n", fname);
	return (-1);
    }
    if (!bfd_check_format(pbfd, bfd_object)) {
	printf("file %s  doesn't seem to be an object file\n", fname);
	return (-1);
    }

    arch = bfd_get_arch_info (pbfd);
    if (bfd_little_endian (pbfd) || arch->mach == bfd_mach_sparc_sparclite_le)
        current_target_byte_order = LITTLE_ENDIAN;
    else
	current_target_byte_order = BIG_ENDIAN;
    if (sis_verbose)
	printf("file %s is %s-endian.\n", fname,
	       current_target_byte_order == BIG_ENDIAN ? "big" : "little");

    if (sis_verbose)
	printf("loading %s:", fname);
    for (section = pbfd->sections; section; section = section->next) {
	if (bfd_get_section_flags(pbfd, section) & SEC_ALLOC) {
	    bfd_vma         section_address;
	    unsigned long   section_size;
	    const char     *section_name;

	    section_name = bfd_get_section_name(pbfd, section);

	    section_address = bfd_get_section_vma(pbfd, section);
	    /*
	     * Adjust sections from a.out files, since they don't carry their
	     * addresses with.
	     */
	    if (bfd_get_flavour(pbfd) == bfd_target_aout_flavour) {
		if (strcmp (section_name, ".text") == 0)
		    section_address = bfd_get_start_address (pbfd);
		else if (strcmp (section_name, ".data") == 0) {
		    /* Read the first 8 bytes of the data section.
		       There should be the string 'DaTa' followed by
		       a word containing the actual section address. */
		    struct data_marker
		    {
			char signature[4];	/* 'DaTa' */
			unsigned char sdata[4];	/* &sdata */
		    } marker;
		    bfd_get_section_contents (pbfd, section, &marker, 0,
					      sizeof (marker));
		    if (strncmp (marker.signature, "DaTa", 4) == 0)
		      {
			if (current_target_byte_order == BIG_ENDIAN)
			  section_address = bfd_getb32 (marker.sdata);
			else
			  section_address = bfd_getl32 (marker.sdata);
		      }
		}
	    }

	    section_size = bfd_section_size(pbfd, section);

	    if (sis_verbose)
		printf("\nsection %s at 0x%08lx (0x%lx bytes)",
		       section_name, section_address, section_size);

	    /* Text, data or lit */
	    if (bfd_get_section_flags(pbfd, section) & SEC_LOAD) {
		file_ptr        fptr;

		fptr = 0;

		while (section_size > 0) {
		    char            buffer[1024];
		    int             count;

		    count = min(section_size, 1024);

		    bfd_get_section_contents(pbfd, section, buffer, fptr, count);

		    sis_memory_write(section_address, buffer, count);

		    section_address += count;
		    fptr += count;
		    section_size -= count;
		}
	    } else		/* BSS */
		if (sis_verbose)
		    printf("(not loaded)");
	}
    }
    if (sis_verbose)
	printf("\n");

    return(bfd_get_start_address (pbfd));
}
