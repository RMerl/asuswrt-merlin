/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Main Module				File: cfe_main.c       
    *  
    *  This module contains the main "C" routine for CFE and 
    *  the main processing loop.  There should not be any board-specific
    *  stuff in here.
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


#include "lib_types.h"
#include "lib_string.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_timer.h"

#include "env_subr.h"
#include "ui_command.h"
#include "cfe_mem.h"
#include "cfe.h"

#include "exception.h"

#include "bsp_config.h"

#include "segtable.h"

#include "initdata.h"

#if CFG_PCI
#include "pcivar.h"
#endif

#include "epivers.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#ifndef CFG_STACK_SIZE
#define STACK_SIZE	8192
#else
#define STACK_SIZE	((CFG_STACK_SIZE+1023) & ~1023)
#endif

/*  *********************************************************************
    *  Externs
    ********************************************************************* */

void cfe_main(int,int);
extern void cfe_device_poll(void *x);

extern int ui_init_envcmds(void);
extern int ui_init_devcmds(void);
extern int ui_init_netcmds(void);
extern int ui_init_memcmds(void);
extern int ui_init_loadcmds(void);
extern int ui_init_pcicmds(void);
extern int ui_init_examcmds(void);
extern int ui_init_flashcmds(void);
extern int ui_init_misccmds(void);
#if CFG_VAPI
extern int ui_init_vapicmds(void);
#endif

#if CFG_VENDOR_EXTENSIONS
extern int ui_init_vendorcmds(void);
#endif

void cfe_command_restart(uint64_t status);

extern segtable_t *_getsegtbl(void);

extern const char *builddate;
extern const char *builduser;

#if CFG_MULTI_CPUS
extern int altcpu_cmd_start(uint64_t,uint64_t *);
extern int altcpu_cmd_stop(uint64_t);
#endif


/*  *********************************************************************
    *  Globals
    ********************************************************************* */

const char *cfe_boardname = CFG_BOARDNAME;
unsigned int cfe_startflags =
#if CFG_PCI
    CFE_INIT_PCI |
#endif
    0;

/*  *********************************************************************
    *  cfe_setup_default_env()
    *  
    *  Initialize the default environment for CFE.  These are all
    *  the temporary variables that do not get stored in the NVRAM
    *  but are available to other programs and command-line macros.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void cfe_setup_default_env(void)
{
    char buffer[80];

    xsprintf(buffer,"%d.%d.%d",CFE_VER_MAJOR,CFE_VER_MINOR,CFE_VER_BUILD);
    env_setenv("CFE_VERSION",buffer,ENV_FLG_BUILTIN | ENV_FLG_READONLY);

    if (cfe_boardname) {
	env_setenv("CFE_BOARDNAME",(char *) cfe_boardname,
		   ENV_FLG_BUILTIN | ENV_FLG_READONLY);
	}

    xsprintf(buffer,"%d",mem_totalsize);
    env_setenv("CFE_MEMORYSIZE",buffer,ENV_FLG_BUILTIN | ENV_FLG_READONLY);

}


/*  *********************************************************************
    *  cfe_init_ui()
    *  
    *  Call all the other UI initialization modules.  Each of these
    *  modules usually calls back to the UI dispatcher to add command
    *  tables.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#if CFG_MINIMAL_SIZE
#define OPTIONAL(x)
#else 
#define OPTIONAL(x) x
#endif

static void cfe_init_ui(void)
{
    ui_init_cmddisp();

    OPTIONAL(ui_init_envcmds());
    //OPTIONAL(ui_init_devcmds());
    ui_init_devcmds();
#if CFG_NETWORK
    ui_init_netcmds();
#endif
    ui_init_loadcmds();
    OPTIONAL(ui_init_memcmds());

#if CFG_PCI
    ui_init_pcicmds();
#endif
    OPTIONAL(ui_init_examcmds());
    ui_init_flashcmds();
#if CFG_VAPI
    ui_init_vapicmds();
#endif

#if CFG_VENDOR_EXTENSIONS
    ui_init_vendorcmds();
#endif

    OPTIONAL(ui_init_misccmds());

}


/*  *********************************************************************
    *  cfe_ledstr(leds)
    *  
    *  Display a string on the board's LED display, if it has one.
    *  This routine depends on the board-support package to
    *  include a "driver" to write to the actual LED, if the board
    *  does not have one this routine will do nothing.
    *  
    *  The LEDs are written at various places in the initialization
    *  sequence, to debug board problems.
    *  
    *  Input parameters: 
    *  	   leds - pointer to four-character ASCII string
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void cfe_ledstr(const char *leds)
{
    unsigned int val;

    val = ((((unsigned int) leds[0]) << 24) |
	   (((unsigned int) leds[1]) << 16) |
	   (((unsigned int) leds[2]) << 8) |
	   ((unsigned int) leds[3]));

    cfe_leds(val);
}


/*  *********************************************************************
    *  cfe_say_hello()
    *  
    *  Print out the CFE startup message and copyright notice
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


static void cfe_say_hello(void)
{
    xprintf("\n\n");
    xprintf("CFE version %s based on BBP %d.%d.%d"
#ifdef CFE_VER_RELEASE
	    ".%d"
#endif
            " for %s (%s)\n", EPI_VERSION_STR,
	    CFE_VER_MAJOR,CFE_VER_MINOR,CFE_VER_BUILD,
#ifdef CFE_VER_RELEASE
	    CFE_VER_RELEASE,
#endif
	    cfe_boardname,
#ifdef __long64
	    "64bit,"
#else
	    "32bit,"
#endif	
#if CFG_MULTI_CPUS
	    "MP,"
#else
	    "SP,"
#endif
#ifdef __MIPSEL
	    "LE"
#endif
#ifdef __MIPSEB
	    "BE"
#endif
#if CFG_VAPI
	    ",VAPI"
#endif
	);

    xprintf("Build Date: %s (%s)\n",builddate,builduser);
    xprintf("Copyright (C) 2000-2008 Broadcom Corporation.\n");
    xprintf("\n");
}


/*  *********************************************************************
    *  cfe_restart()
    *  
    *  Restart CFE from scratch, jumping back to the boot vector.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */

void cfe_restart(void)
{
    _exc_restart();
}


/*  *********************************************************************
    *  cfe_start(ept)
    *  
    *  Start a user program
    *  
    *  Input parameters: 
    *  	   ept - entry point
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void cfe_start(unsigned long ept)
{
    SETLEDS("RUN!");
    cfe_launch(ept);
}


/*  *********************************************************************
    *  cfe_startup_info()
    *  
    *  Display startup memory configuration messages
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void cfe_startup_info(void)
{
    segtable_t *segtbl;

    segtbl = _getsegtbl();
    xprintf("CPU type 0x%X: ",(uint32_t)cpu_prid);
    if (cfe_cpu_speed < 1000000) xprintf("%dKHz\n",cfe_cpu_speed/1000);
    else xprintf("%dMHz\n",cfe_cpu_speed/1000000);
    xprintf("Tot mem: %u KBytes\n", mem_totalsize);

    xprintf("\n");
    xprintf("CFE mem:    0x%08X - 0x%08X (%d)\n",
	    (uint32_t) mem_bottomofmem,
	    (uint32_t) mem_topofmem,
	    (uint32_t) mem_topofmem-mem_bottomofmem);
    xprintf("Data:       0x%08X - 0x%08X (%d)\n",
	    (uint32_t) (segtbl[R_SEG_FDATA] + mem_datareloc),
	    (uint32_t) (segtbl[R_SEG_EDATA] + mem_datareloc),
	    (uint32_t) (segtbl[R_SEG_EDATA] - segtbl[R_SEG_FDATA]));
    xprintf("BSS:        0x%08X - 0x%08X (%d)\n",
	    (uint32_t) (segtbl[R_SEG_FBSS] + mem_datareloc),
	    (uint32_t) (segtbl[R_SEG_END] + mem_datareloc),
	    (uint32_t) (segtbl[R_SEG_END] - segtbl[R_SEG_FBSS]));
    xprintf("Heap:       0x%08X - 0x%08X (%d)\n",
	    (uint32_t)(mem_heapstart),
	    (uint32_t)(mem_heapstart + (CFG_HEAP_SIZE*1024)),
	    (CFG_HEAP_SIZE*1024));
    xprintf("Stack:      0x%08X - 0x%08X (%d)\n",
	    (uint32_t)(mem_heapstart + (CFG_HEAP_SIZE*1024)),
	    (uint32_t)(mem_heapstart + (CFG_HEAP_SIZE*1024) + STACK_SIZE),
	    STACK_SIZE);
    xprintf("Text:       0x%08X - 0x%08X (%d)\n",
	    (uint32_t)mem_textbase,
	    (uint32_t)(mem_textbase+mem_textsize),
	    (uint32_t) mem_textsize);
#if !CFG_MINIMAL_SIZE
    xprintf("Boot:       0x%08X - 0x%08X\n",
	    mem_bootarea_start,mem_bootarea_start+mem_bootarea_size);
    xprintf("Reloc:      I:%08X - D:%08X\n",
	    (uint32_t) mem_textreloc,(uint32_t) mem_datareloc);
#endif
}


/*  *********************************************************************
    *  cfe_autostart()
    *  
    *  Process automatic commands at startup
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void cfe_autostart(void)
{
    char *env;
    int noauto = 0;
    char ch = 0;

    env = env_getenv("STARTUP");
    if (!env) return;

    while (console_status()) {
	console_read(&ch,1);
	if (ch == 3) noauto = TRUE;	/* Ctrl-C means no auto */
	}

    if (noauto) {
	xprintf("Startup canceled\n");
	return;
	}

    ui_docommands(env);
}

/*  *********************************************************************
    *  cfe_main(a,b)
    *  
    *  It's gotta start somewhere.
    *  
    *  Input parameters: 
    *  	   a,b - not used
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */

void cfe_main(int a,int b)
{

    /*
     * By the time this routine is called, the following things have
     * already been done:
     *
     * 1. The processor(s) is(are) initialized.
     * 2. The caches are initialized.
     * 3. The memory controller is initialized.
     * 4. BSS has been zeroed.
     * 5. The data has been moved to R/W space.
     * 6. The "C" Stack has been initialized.
     */

    cfe_bg_init();		       		/* init background processing */
    cfe_attach_init();
    cfe_timer_init();				/* Timer process */
    cfe_bg_add(cfe_device_poll,NULL);

    /*
     * Initialize the memory allocator
     */

    SETLEDS("KMEM");
    KMEMINIT((unsigned char *) (uintptr_t) mem_heapstart,
	     ((CFG_HEAP_SIZE)*1024));

    /*
     * Initialize the console.  It is done before the other devices
     * get turned on.  The console init also sets the variable that
     * contains the CPU speed.
     */

    SETLEDS("CONS");
    board_console_init();

    /*
     * Set up the exception vectors
     */

    cfe_setup_exceptions();

    /* 
     * Say hello.
     */

    SETLEDS("CIOK");
    cfe_say_hello();

    /*
     * Initialize the other devices.
     */

    SETLEDS("AREN");
#if CFG_XIP
	xprintf("CFE XIP\n");
#endif
    xprintf("Init Arena\n");
    cfe_arena_init();

#if CFG_PCI
    if (cfe_startflags & CFE_INIT_PCI) {
	pci_flags_t flags = PCI_FLG_NORMAL | PCI_FLG_LDT_PREFETCH;
#ifndef EB332
	char *str;
	extern cons_t pci_optnames[];

	flags = PCI_FLG_NORMAL | PCI_FLG_LDT_PREFETCH;
#if CFG_LDT_REV_017
	flags |= PCI_FLG_LDT_REV_017;
#endif
	str = env_getenv("PCI_OPTIONS");
	setoptions(pci_optnames,str,&flags);

	xprintf("Initializing PCI. [%s]\n",str ? str : "normal");
#endif
	pci_configure(flags);
	}
#endif

    SETLEDS("DEVI");
    xprintf("Init Devs.\n");
    board_device_init();

    cfe_startup_info();
    SETLEDS("ENVI");
    cfe_setup_default_env();

    xprintf("\n");

    cfe_init_ui();

    board_final_init();

    cfe_autostart();

    cfe_command_loop();
}

/*  *********************************************************************
    *  cfe_command_restart()
    *  
    *  This routine is called when an application wants to restart
    *  the firmware's command processor.  Reopen the console and
    *  jump back into the command loop.
    *  
    *  Input parameters: 
    *  	   status - A0 value of program returning to firmware
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */

void cfe_command_restart(uint64_t status)
{
    /*
     * Stop alternate CPU(s).  If they're already stopped, this
     * command will make sure they're stopped.
     */

#if CFG_MULTI_CPUS
    altcpu_cmd_stop(1);
#endif

    /*
     * Call board reset functions
     */
    board_device_reset();

    /*
     * Reset devices
     */
    cfe_device_reset();

    /*
     * Reset timers
     */
    cfe_timer_init();

    /*
     * Reopen console 
     */
    console_open(console_name);

    /*
     * Display program return status
     */

    xprintf("*exit status = %d\n", (int)status);

    /*
     * Back to processing user commands
     */
    cfe_command_loop();
}



/*  *********************************************************************
    *  cfe_command_loop()
    *  
    *  This routine reads and processes user commands
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */

void cfe_command_loop()
{
    char buffer[300];
    int status;
    char *prompt;

    SETLEDS("CFE ");

	for (;;) {
	prompt = env_getenv("PROMPT");
#if CFG_RAMAPP
	SETLEDS("CFE*");
	if (!prompt) prompt = "CFE_RAM> ";
#else
	if (!prompt) prompt = "CFE> ";
#endif
	console_readline(prompt,buffer,sizeof(buffer));

	status = ui_docommands(buffer);

	if (status != CMD_ERR_BLANK) {
	    xprintf("*** command status = %d\n", status);
	    }
	}
}
