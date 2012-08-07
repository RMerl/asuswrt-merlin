/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Memory dump commands			File: ui_examcmds.c
    *  
    *  UI functions for examining data in various ways
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
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_error.h"
#include "cfe_console.h"

#include "ui_command.h"
#include "cfe.h"
#include "disasm.h"

#include "addrspace.h"
#include "exchandler.h"


static int ui_cmd_memdump(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_memedit(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_memfill(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_disasm(ui_cmdline_t *cmd,int argc,char *argv[]);

#ifdef __long64
#define XTOI(x) xtoq(x)
#else
#define XTOI(x) xtoi(x)
#endif

int ui_init_examcmds(void);


#define ATYPE_SIZE_NONE	0
#define ATYPE_SIZE_BYTE	1
#define ATYPE_SIZE_HALF	2
#define ATYPE_SIZE_WORD	4
#define ATYPE_SIZE_QUAD	8
#define ATYPE_SIZE_MASK	0x0F

#define ATYPE_TYPE_NONE	0
#define ATYPE_TYPE_PHYS 0x10
#define ATYPE_TYPE_KERN 0x20
#define ATYPE_TYPE_MASK 0xF0

static long prev_addr = 0;	/* initialized below in ui_init_examcmds */
static int prev_length = 256;
static int prev_dlength = 16;
static int prev_wtype = ATYPE_SIZE_WORD | ATYPE_TYPE_KERN;

static int getaddrargs(ui_cmdline_t *cmd,int *curtype,long *addr,int *length)
{
    int atype = *curtype;
    long newaddr;
    int newlen;
    char *x;
    long wlen;

    if (cmd_sw_isset(cmd,"-b")) {
	atype &= ~ATYPE_SIZE_MASK;
	atype |= ATYPE_SIZE_BYTE;
	}
    else if (cmd_sw_isset(cmd,"-h")) {
	atype &= ~ATYPE_SIZE_MASK;
	atype |= ATYPE_SIZE_HALF;
	}
    else if (cmd_sw_isset(cmd,"-w")) {
	atype &= ~ATYPE_SIZE_MASK;
	atype |= ATYPE_SIZE_WORD;
	}
    else if (cmd_sw_isset(cmd,"-q")) {
	atype &= ~ATYPE_SIZE_MASK;
	atype |= ATYPE_SIZE_QUAD;
	}

    wlen = atype & ATYPE_SIZE_MASK;
    if (wlen == 0) wlen = 1;		/* bytes are the default */

    if (cmd_sw_isset(cmd,"-p")) {
	atype &= ~ATYPE_TYPE_MASK;
	atype |= ATYPE_TYPE_PHYS;
	}
    else if (cmd_sw_isset(cmd,"-v")) {
	atype &= ~ATYPE_TYPE_MASK;
	atype |= ATYPE_TYPE_KERN;
	}

    *curtype = atype;

    if (addr) {
	x = cmd_getarg(cmd,0);
	if (x) {
	    if (strcmp(x,".") == 0) newaddr = *addr;
	    else {
		/* 
		 * hold on to your lunch, this is really, really bad! 
		 * Make 64-bit addresses expressed as 8-digit numbers
		 * sign extend automagically.  Saves typing, but is very
		 * gross.
		 */
		int longaddr = 0;
		longaddr = strlen(x);
		if (memcmp(x,"0x",2) == 0) longaddr -= 2;
		longaddr = (longaddr > 8) ? 1 : 0;

		if (longaddr) newaddr = (long) xtoq(x);
		else newaddr = (long) xtoi(x);
		}
	    *addr = newaddr & ~(wlen - 1);	/* align to natural boundary */
	    }
	}

    if (length) {
	x = cmd_getarg(cmd,1);
	if (x) {
	    newlen = (long) xtoi(x);
	    *length = newlen;
	    }
	}

    return 0;

}

static int stuffmem(long addr,int wlen,char *tail)
{
    char *tok;
    int count = 0;
    uint8_t b;
    uint16_t h;
    uint32_t w;
    uint64_t q;
    int res = 0;

    addr &= ~(wlen-1);

    while ((tok = gettoken(&tail))) {
	switch (wlen) {
	    default:
	    case 1:
	      b = (uint8_t) xtoq(tok);
	      if ((res = mem_poke(addr, b, MEM_BYTE))) {
		  /*Did not edit*/
		  return res;
		}
	      break;
	    case 2:
	      h = (uint16_t) xtoq(tok);
	      if ((res = mem_poke(addr, h, MEM_HALFWORD))) {
		  /*Did not edit*/
		  return res;
		}		      
		break;
	    case 4:
	      w = (uint32_t) xtoq(tok);
	      if ((res = mem_poke(addr, w, MEM_WORD))) {
		  /*Did not edit*/
		  return res;
		}
		break;
	    case 8:
	      q = (uint64_t) xtoq(tok);
	      if ((res = mem_poke(addr, q, MEM_QUADWORD))) {
		  /*Did not edit*/
		  return res;
		}	            
		break;
	    }

	addr += wlen;
	count++;
	}

    return count;
}

static int dumpmem(long addr,long dispaddr,int length,int wlen)
{
    int idx,x;
    uint8_t b;
    uint16_t h;
    uint32_t w;
    uint64_t q;
    int res = 0;

    /*
     * The reason we save the line in this union is to provide the
     * property that the dump command will only touch the 
     * memory once.  This might be useful when looking at
     * device registers.
     */

    union {
	uint8_t bytes[16];
	uint16_t halves[8];
	uint32_t words[4];
	uint64_t quads[2];
    } line;
       
    addr &= ~(wlen-1);

    for (idx = 0; idx < length; idx += 16) {
	xprintf("%P%c ",dispaddr+idx,(dispaddr != addr) ? '%' : ':');
	switch (wlen) {
	    default:
	    case 1:
		for (x = 0; x < 16; x++) {
		    if (idx+x < length) {
			if ((res = mem_peek(&b, (addr+idx+x), MEM_BYTE))) {
			    return res;
			    }
			line.bytes[x] = b;
			xprintf("%02X ",b);
			}
		    else {
			xprintf("   ");
			}
		    }
		break;
	    case 2:
		for (x = 0; x < 16; x+=2) {
		    if (idx+x < length) { 
			if ((res = mem_peek(&h, (addr+idx+x), MEM_HALFWORD))) {
			    return res;
			    }
			line.halves[x/2] = h;
			xprintf("%04X ",h);
			}
		    else {
			xprintf("     ");
			}
		    }
		break;
	    case 4:
		for (x = 0; x < 16; x+=4) {
		    if (idx+x < length) { 
		      
			if ((res = mem_peek(&w , (addr+idx+x), MEM_WORD))) {
			    return res;
			    }
			line.words[x/4] = w;
			xprintf("%08X ",w);
			}
		    else {
			xprintf("         ");
			}
		    }
		break;
	    case 8:
		for (x = 0; x < 16; x+=8) { 
		    if (idx+x < length) {
			if ((res = mem_peek(&q, (addr+idx+x), MEM_QUADWORD))) {
			    return res;
			    }
			line.quads[x/8] = q;
			xprintf("%016llX ",q);
			}
		    else { 
			xprintf("                 ");
			}
		    }
		break;
	    }

	xprintf(" ");
	for (x = 0; x < 16; x++) {
	    if (idx+x < length) {
		b = line.bytes[x];
		if ((b < 32) || (b > 127)) xprintf(".");
		else xprintf("%c",b);
		}
	    else {
		xprintf(" ");
		}
	    }
	xprintf("\n");
	}

    return 0;
}

static int ui_cmd_memedit(ui_cmdline_t *cmd,int argc,char *argv[])
{
    uint8_t b;
    uint16_t h;
    uint32_t w;
    uint64_t q;

    long addr;
    char *vtext;
    int wlen;
    int count;
    int idx = 1;
    int stuffed = 0;
    int res = 0;

    getaddrargs(cmd,&prev_wtype,&prev_addr,NULL);

    wlen = prev_wtype & ATYPE_SIZE_MASK;

    vtext = cmd_getarg(cmd,idx++);

    addr = prev_addr;

    while (vtext) {
	count = stuffmem(addr,wlen,vtext);
	if (count < 0) {
	    ui_showerror(count,"Could not modify memory");
	    return count;			/* error */
	    }
	addr += count*wlen;
	prev_addr += count*wlen;
	stuffed += count;
	vtext = cmd_getarg(cmd,idx++);
	}

    if (stuffed == 0) {
	char line[256];
	char prompt[32];

	xprintf("Type '.' to exit, '-' to back up, '=' to dump memory.\n");
	for (;;) {

	    addr = prev_addr;
	    if ((prev_wtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_PHYS) {
		addr = UNCADDR(addr);
		}

	    xprintf("%P%c ",prev_addr,(addr != prev_addr) ? '%' : ':');

	    switch (wlen) {
		default:
		case 1:
		    if ((res = mem_peek(&b, addr, MEM_BYTE))) {
			return res;
			}
		    xsprintf(prompt,"[%02X]: ", b);
		    break;
		case 2:
		    if ((res = mem_peek(&h, addr, MEM_HALFWORD))) {
			return res;
			}
		    xsprintf(prompt,"[%04X]: ",h);
		    break;
		case 4:
		    if ((res = mem_peek(&w, addr, MEM_WORD))) {
			return res;
			}
		    xsprintf(prompt,"[%08X]: ",w);
		    break;
		case 8:
		    if ((res = mem_peek(&q, addr, MEM_QUADWORD))) {
			return res;
			}
		    xsprintf(prompt,"[%016llX]: ",q);
		    break;
		}

	    console_readline(prompt,line,sizeof(line));
	    if (line[0] == '-') {
		prev_addr -= wlen;
		continue;
		}
	    if (line[0] == '=') {
		dumpmem(prev_addr,prev_addr,16,wlen);
		continue;
		}
	    if (line[0] == '.') {
		break;
		}
	    if (line[0] == '\0') {
		prev_addr += wlen;
		continue;
		}
	    count = stuffmem(addr,wlen,line);
	    if (count < 0) return count;
	    if (count == 0) break;
	    prev_addr += count*wlen;
	    }
	}

    return 0;
}

static int ui_cmd_memfill(ui_cmdline_t *cmd,int argc,char *argv[])
{
    long addr;
    char *atext;
    int wlen;
    int idx = 2;
    int len;
    uint64_t pattern;
    uint8_t *b_ptr;
    uint16_t *h_ptr;
    uint32_t *w_ptr;
    uint64_t *q_ptr;
    int res;

    getaddrargs(cmd,&prev_wtype,&prev_addr,&len);

    atext = cmd_getarg(cmd,idx++);
    if (!atext) return ui_showusage(cmd);
    pattern = xtoq(atext);   

    addr = prev_addr;

    if ((prev_wtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_PHYS) {
	addr = UNCADDR(addr);
	}

    wlen = prev_wtype & ATYPE_SIZE_MASK;

    switch (wlen) {
	case 1:
	    b_ptr = (uint8_t *) addr;
	    while (len > 0) {
	        if ((res = mem_poke( ((long)(b_ptr)),  pattern, MEM_BYTE))) {
		    /*Did not edit*/
		    return 0;
		    }
		b_ptr++;
		len--;
		}
	    break;
	case 2:
	    h_ptr = (uint16_t *) addr;
	    while (len > 0) {
		if ((res = mem_poke( ((long)(h_ptr)),  pattern, MEM_HALFWORD))) {
		    return 0;
		    }
		h_ptr++;
		len--;
		}
	    break;
	case 4:
	    w_ptr = (uint32_t *) addr;
	    while (len > 0) {
	        if ((res = mem_poke( ((long)(w_ptr)),  pattern, MEM_WORD))) {
		    return -1;
		    }
		w_ptr++;
		len--;
		}
	    break;
	case 8:
	    q_ptr = (uint64_t *) addr;
	    while (len > 0) {
		if ((res = mem_poke( ((long)(q_ptr)),  pattern, MEM_QUADWORD))) {
		    return 0;
		    }
		q_ptr++;
		len--;
		}
	    break;
	}

    return 0;
}


#define FILL(ptr,len,pattern) printf("Pattern: %016llX\n",pattern); \
             for (idx = 0; idx < len; idx++) ptr[idx] = pattern
#define CHECK(ptr,len,pattern) for (idx = 0; idx < len; idx++) { \
             if (ptr[idx]!=pattern) {printf("Mismatch at %016llX: Expected %016llX got %016llX", \
					   (uint64_t) (uintptr_t) &(ptr[idx]),pattern,ptr[idx]); \
                                           error = 1; loopmode = 0;break;} \
             }

#define MEMTEST(ptr,len,pattern) if (!error) { FILL(ptr,len,pattern) ; CHECK(ptr,len,pattern); }

static int ui_cmd_memtest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    long addr = 0;
    int len = 0;
    int wtype = 0;
    int wlen;
    int idx = 0;
    uint64_t *ptr;
    int error = 0;
    int loopmode = 0;
    int pass =0;

    getaddrargs(cmd,&wtype,&addr,&len);

    wlen = 8;
    addr &= ~(wlen-1);

    if ((prev_wtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_PHYS) {
	addr = UNCADDR(addr);
	}

    if (cmd_sw_isset(cmd,"-loop")) {
	loopmode = 1;
	}

    len /= wlen;

    ptr = (uint64_t *) addr;
    pass = 0;
    for (;;) {
	if (loopmode) {
	    printf("Pass %d\n",pass);
	    if (console_status()) break;
	    }
	MEMTEST(ptr,len,(idx*8));
	MEMTEST(ptr,len,	0);
	MEMTEST(ptr,len,0xFFFFFFFFFFFFFFFFLL);
	MEMTEST(ptr,len,0x5555555555555555LL); 
	MEMTEST(ptr,len,0xAAAAAAAAAAAAAAAALL); 
	MEMTEST(ptr,len,0xFF00FF00FF00FF00LL);
	MEMTEST(ptr,len,0x00FF00FF00FF00FFLL);
	if (!loopmode) break;
	pass++;
	}

    return 0;
}

static int ui_cmd_memdump(ui_cmdline_t *cmd,int argc,char *argv[])
{
    long addr;
    int res;

    getaddrargs(cmd,&prev_wtype,&prev_addr,&prev_length);

    addr = prev_addr;
    if ((prev_wtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_PHYS) {
	addr = UNCADDR(addr);
	}
    
    res = dumpmem(addr,
	    prev_addr,
	    prev_length,
	    prev_wtype & ATYPE_SIZE_MASK);

    if (res < 0) {
	ui_showerror(res,"Could not display memory");
	}
    else {
	prev_addr += prev_length;
	}

    return res;
}

static int ui_cmd_disasm(ui_cmdline_t *cmd,int argc,char *argv[])
{
    long addr;
    char buf[512];
    int idx;
    uint32_t inst;
    int res;

    getaddrargs(cmd,&prev_wtype,&prev_addr,&prev_dlength);

    prev_addr &= ~3;

    addr = prev_addr;
    if ((prev_wtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_PHYS) {
	addr = UNCADDR(addr);
	}

    for (idx = 0; idx < prev_dlength; idx++) {
	if ((res = mem_peek(&inst, addr, MEM_WORD))) {
	    ui_showerror(res,"Could not disassemble memory");
	    return res;
	    }	  
	disasm_inst(buf,sizeof(buf),inst,(uint64_t) prev_addr);
	xprintf("%P%c %08x    %s\n",prev_addr,(addr != prev_addr) ? '%' : ':',inst,buf);
	addr += 4;
	prev_addr += 4;
	}

    return 0;
}

int ui_init_examcmds(void)
{
    cmd_addcmd("u",
	       ui_cmd_disasm,
	       NULL,
	       "Disassemble instructions.",
	       "u [addr [length]]\n\n"
	       "This command disassembles instructions at the specified address.\n"
	       "CFE will display standard register names and symbolic names for\n"
	       "certain CP0 registers.  The 'u' command remembers the last address\n"
	       "that was disassembled so you can enter 'u' again with no parameters\n"
	       "to continue a previous request.\n",
               "-p;Address is an uncached physical address|"
	       "-v;Address is a kernel virtual address");


    cmd_addcmd("d",
	       ui_cmd_memdump,
	       NULL,
	       "Dump memory.",
	       "d [-b|-h|-w|-q] [addr [length]]\n\n"
	       "This command displays data from memory as bytes, halfwords, words,\n"
	       "or quadwords.  ASCII text, if present, will appear to the right of\n"
	       "the hex data.  The dump command remembers the previous word size,\n"
	       "dump length and last displayed address, so you can enter 'd' again\n"
	       "to continue a previous dump request.",
	       "-b;Dump memory as bytes|"
	       "-h;Dump memory as halfwords (16-bits)|"
               "-w;Dump memory as words (32-bits)|"
               "-q;Dump memory as quadwords (64-bits)|"
               "-p;Address is an uncached physical address|"
	       "-v;Address is a kernel virtual address");


    cmd_addcmd("e",
	       ui_cmd_memedit,
	       NULL,
	       "Modify contents of memory.",
	       "e [-b|-h|-w|-q] [addr [data...]]\n\n"
	       "This command modifies the contents of memory.  If you do not specify\n"
	       "data on the command line, CFE will prompt for it.  When prompting for\n"
	       "data you may enter '-' to back up, '=' to dump memory at the current\n"
	       "location, or '.' to exit edit mode.",
	       "-b;Edit memory as bytes|"
	       "-h;Edit memory as halfwords (16-bits)|"
               "-w;Edit memory as words (32-bits)|"
               "-q;Edit memory as quadwords (64-bits)|"
	       "-p;Address is an uncached physical address|"
	       "-v;Address is a kernel virtual address");

    cmd_addcmd("f",
	       ui_cmd_memfill,
	       NULL,
	       "Fill contents of memory.",
	       "f [-b|-h|-w|-q] addr length pattern\n\n"
	       "This command modifies the contents of memory.  You can specify the\n"
	       "starting address, length, and pattern of data to fill (in hex)\n",
	       "-b;Edit memory as bytes|"
	       "-h;Edit memory as halfwords (16-bits)|"
               "-w;Edit memory as words (32-bits)|"
               "-q;Edit memory as quadwords (64-bits)|"
	       "-p;Address is an uncached physical address|"
	       "-v;Address is a kernel virtual address");

    cmd_addcmd("memtest",
	       ui_cmd_memtest,
	       NULL,
	       "Test memory.",
	       "memtest [options] addr length\n\n"
	       "This command tests memory.  It is a very crude test, so don't\n"
	       "rely on it for anything really important.  Addr and length are in hex\n",
	       "-p;Address is an uncached physical address|"
	       "-v;Address is a kernel virtual address|"
	       "-loop;Loop till keypress");


    prev_addr = KERNADDR(0);

    return 0;
}
