/*
 * This file is part of SIS.
 * 
 * SIS, SPARC instruction simulator. Copyright (C) 1995 Jiri Gaisler, European
 * Space Agency
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

#include "config.h"
#include <signal.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#include <time.h>
#include <sys/fcntl.h>
#include "sis.h"
#include <dis-asm.h>
#include "sim-config.h"

#define	VAL(x)	strtol(x,(char **)NULL,0)

/* Structures and functions from readline library */

typedef struct {
  char *line;
  char *data;
} HIST_ENTRY;

extern char *	readline PARAMS ((char *prompt));
extern void	using_history PARAMS ((void));
extern void	add_history PARAMS ((char *string));
extern HIST_ENTRY *remove_history PARAMS ((int which));



/* Command history buffer length - MUST be binary */
#define HIST_LEN	64

extern struct disassemble_info dinfo;
extern struct pstate sregs;
extern struct estate ebase;

extern int      ctrl_c;
extern int      nfp;
extern int      ift;
extern int      wrp;
extern int      rom8;
extern int      uben;
extern int      sis_verbose;
extern char    *sis_version;
extern struct estate ebase;
extern struct evcell evbuf[];
extern struct irqcell irqarr[];
extern int      irqpend, ext_irl;
extern int      termsave;
extern int      sparclite;
extern int      dumbio;
extern char     uart_dev1[];
extern char     uart_dev2[];
extern uint32   last_load_addr;

#ifdef ERA
extern int era;
#endif

int
run_sim(sregs, icount, dis)
    struct pstate  *sregs;
    unsigned int    icount;
    int             dis;
{
    int             irq, mexc, deb, asi;

    sregs->starttime = time(NULL);
    init_stdio();
    if (sregs->err_mode) icount = 0;
    deb = dis || sregs->histlen || sregs->bptnum;
    irq = 0;
    while (icount > 0) {

	if (sregs->psr & 0x080)
	    asi = 9;
   	else
	    asi = 8;
	mexc = memory_read(asi, sregs->pc, &sregs->inst, 2, &sregs->hold);
	sregs->icnt = 1;
	if (sregs->annul) {
	    sregs->annul = 0;
	    sregs->pc = sregs->npc;
	    sregs->npc = sregs->npc + 4;
	} else {
	    sregs->fhold = 0;
	    if (ext_irl) irq = check_interrupts(sregs);
	    if (!irq) {
		if (mexc) {
		    sregs->trap = I_ACC_EXC;
		} else {
		    if (deb) {
	    		if ((sregs->bphit = check_bpt(sregs)) != 0) {
            		    restore_stdio();
	    		    return (BPT_HIT);
	    		}
		        if (sregs->histlen) {
			    sregs->histbuf[sregs->histind].addr = sregs->pc;
			    sregs->histbuf[sregs->histind].time = ebase.simtime;
			    sregs->histind++;
			    if (sregs->histind >= sregs->histlen)
			        sregs->histind = 0;
		        }
		        if (dis) {
			    printf(" %8u ", ebase.simtime);
			    dis_mem(sregs->pc, 1, &dinfo);
		        }
		    }
		    dispatch_instruction(sregs);
		    icount--;
		}
	    }
	    if (sregs->trap) {
		irq = 0;
		sregs->err_mode = execute_trap(sregs);
        	if (sregs->err_mode) {
	            error_mode(sregs->pc);
	            icount = 0;
	        }
	    }
	}
	advance_time(sregs);
	if (ctrl_c || (sregs->tlimit <= ebase.simtime)) {
	    icount = 0;
	    if (sregs->tlimit <= ebase.simtime) sregs->tlimit = -1;
	}
    }
    sregs->tottime += time(NULL) - sregs->starttime;
    restore_stdio();
    if (sregs->err_mode)
	return (ERROR);
    if (ctrl_c) {
	ctrl_c = 0;
	return (CTRL_C);
    }
    return (TIME_OUT);
}

int
main(argc, argv)
    int             argc;
    char          **argv;
{

    int             cont = 1;
    int             stat = 1;
    int             freq = 14;
    int             copt = 0;

    char           *cfile, *bacmd;
    char           *cmdq[HIST_LEN];
    int             cmdi = 0;
    int             i;

    cfile = 0;
    for (i = 0; i < 64; i++)
	cmdq[i] = 0;
    printf("\n SIS - SPARC intruction simulator %s,  copyright Jiri Gaisler 1995\n", sis_version);
    printf(" Bug-reports to jgais@wd.estec.esa.nl\n\n");
    while (stat < argc) {
	if (argv[stat][0] == '-') {
	    if (strcmp(argv[stat], "-v") == 0) {
		sis_verbose = 1;
	    } else if (strcmp(argv[stat], "-c") == 0) {
		if ((stat + 1) < argc) {
		    copt = 1;
		    cfile = argv[++stat];
		}
	    } else if (strcmp(argv[stat], "-nfp") == 0)
		nfp = 1;
	    else if (strcmp(argv[stat], "-ift") == 0)
		ift = 1;
	    else if (strcmp(argv[stat], "-wrp") == 0)
		wrp = 1;
	    else if (strcmp(argv[stat], "-rom8") == 0)
		rom8 = 1;
	    else if (strcmp(argv[stat], "-uben") == 0)
		uben = 1;
	    else if (strcmp(argv[stat], "-uart1") == 0) {
		if ((stat + 1) < argc)
		    strcpy(uart_dev1, argv[++stat]);
	    } else if (strcmp(argv[stat], "-uart2") == 0) {
		if ((stat + 1) < argc)
		    strcpy(uart_dev2, argv[++stat]);
	    } else if (strcmp(argv[stat], "-freq") == 0) {
		if ((stat + 1) < argc)
		    freq = VAL(argv[++stat]);
	    } else if (strcmp(argv[stat], "-sparclite") == 0) {
		sparclite = 1;
#ifdef ERA
	    } else if (strcmp(argv[stat], "-era") == 0) {
		era = 1;
#endif
            } else if (strcmp(argv[stat], "-dumbio") == 0) {
		dumbio = 1;
	    } else {
		printf("unknown option %s\n", argv[stat]);
		usage();
		exit(1);
	    }
	} else {
	    last_load_addr = bfd_load(argv[stat]);
	}
	stat++;
    }
    if (nfp)
	printf("FPU disabled\n");
#ifdef ERA
    if (era)
	printf("ERA ECC emulation enabled\n");
#endif
    sregs.freq = freq;

    INIT_DISASSEMBLE_INFO(dinfo, stdout, (fprintf_ftype) fprintf);
    dinfo.endian = BFD_ENDIAN_BIG;

    termsave = fcntl(0, F_GETFL, 0);
    using_history();
    init_signals();
    ebase.simtime = 0;
    reset_all();
    init_bpt(&sregs);
    init_sim();
#ifdef STAT
    reset_stat(&sregs);
#endif

    if (copt) {
	bacmd = (char *) malloc(256);
	strcpy(bacmd, "batch ");
	strcat(bacmd, cfile);
	exec_cmd(&sregs, bacmd);
    }
    while (cont) {

	if (cmdq[cmdi] != 0) {
#if 0
	    remove_history(cmdq[cmdi]);
#else
	    remove_history(cmdi);
#endif
	    free(cmdq[cmdi]);
	    cmdq[cmdi] = 0;
	}
	cmdq[cmdi] = readline("sis> ");
	if (cmdq[cmdi] && *cmdq[cmdi])
	    add_history(cmdq[cmdi]);
	if (cmdq[cmdi])
	    stat = exec_cmd(&sregs, cmdq[cmdi]);
	else {
	    puts("\n");
	    exit(0);
	}
	switch (stat) {
	case OK:
	    break;
	case CTRL_C:
	    printf("\b\bInterrupt!\n");
	case TIME_OUT:
	    printf(" Stopped at time %d (%.3f ms)\n", ebase.simtime, 
	      ((double) ebase.simtime / (double) sregs.freq) / 1000.0);
	    break;
	case BPT_HIT:
	    printf("breakpoint at 0x%08x reached\n", sregs.pc);
	    sregs.bphit = 1;
	    break;
	case ERROR:
	    printf("IU in error mode (%d)\n", sregs.trap);
	    stat = 0;
	    printf(" %8d ", ebase.simtime);
	    dis_mem(sregs.pc, 1, &dinfo);
	    break;
	default:
	    break;
	}
	ctrl_c = 0;
	stat = OK;

	cmdi = (cmdi + 1) & (HIST_LEN - 1);

    }
    return 0;
}

