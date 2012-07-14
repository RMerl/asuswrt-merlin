/****************************************************************************
*
*						Realmode X86 Emulator Library
*
*            	Copyright (C) 1996-1999 SciTech Software, Inc.
* 				     Copyright (C) David Mosberger-Tang
* 					   Copyright (C) 1999 Egbert Eich
*
*  ========================================================================
*
*  Permission to use, copy, modify, distribute, and sell this software and
*  its documentation for any purpose is hereby granted without fee,
*  provided that the above copyright notice appear in all copies and that
*  both that copyright notice and this permission notice appear in
*  supporting documentation, and that the name of the authors not be used
*  in advertising or publicity pertaining to distribution of the software
*  without specific, written prior permission.  The authors makes no
*  representations about the suitability of this software for any purpose.
*  It is provided "as is" without express or implied warranty.
*
*  THE AUTHORS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
*  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
*  EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
*  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
*  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
*  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
*  PERFORMANCE OF THIS SOFTWARE.
*
*  ========================================================================
*
* Language:		ANSI C
* Environment:	Any
* Developer:    Kendall Bennett
*
* Description:  This file contains the code to handle debugging of the
*				emulator.
*
****************************************************************************/
/* $XFree86: xc/extras/x86emu/src/x86emu/debug.c,v 1.4 2000/04/17 16:29:45 eich Exp $ */

#include "x86emu/x86emui.h"
#ifdef IN_MODULE
#include "xf86_ansic.h"
#else
#include <stdarg.h>
#ifndef _CFE_
#include <stdlib.h>
#endif
#endif

#ifdef _CFE_
#include "cfe_console.h"
#endif


/*----------------------------- Implementation ----------------------------*/

#ifdef DEBUG

static void     print_encoded_bytes (u16 s, u16 o);
static void     print_decoded_instruction (void);
static int      parse_line (char *s, int *ps, int *n);
  
/* should look something like debug's output. */
void X86EMU_trace_regs (void)
{
	if (DEBUG_TRACE()) {
		x86emu_dump_regs();
    }
	if (DEBUG_DECODE() && ! DEBUG_DECODE_NOPRINT()) {
		printk("%04x:%04x ",M.x86.saved_cs, M.x86.saved_ip);
		print_encoded_bytes( M.x86.saved_cs, M.x86.saved_ip);
		print_decoded_instruction();
    }
}

void X86EMU_trace_xregs (void)
{
	if (DEBUG_TRACE()) {
		x86emu_dump_xregs();
    }
}

void x86emu_just_disassemble (void)
{
    /*
     * This routine called if the flag DEBUG_DISASSEMBLE is set kind
     * of a hack!
     */
	printk("%04x:%04x ",M.x86.saved_cs, M.x86.saved_ip);
	print_encoded_bytes( M.x86.saved_cs, M.x86.saved_ip);
	print_decoded_instruction();
}

static u16 disassemble_forward (u16 seg, u16 off, int n)
{
	X86EMU_sysEnv tregs;
	int i;
	u8 op1;
	u16 finaloffset;
    /*
     * hack, hack, hack.  What we do is use the exact machinery set up
     * for execution, except that now there is an additional state
     * flag associated with the "execution", and we are using a copy
     * of the register struct.  All the major opcodes, once fully
     * decoded, have the following two steps: TRACE_REGS(r,m);
     * SINGLE_STEP(r,m); which disappear if DEBUG is not defined to
     * the preprocessor.  The TRACE_REGS macro expands to:
     *
     * if (debug&DEBUG_DISASSEMBLE) 
     *     {just_disassemble(); goto EndOfInstruction;}
     *     if (debug&DEBUG_TRACE) trace_regs(r,m);
     *
     * ......  and at the last line of the routine. 
     *
     * EndOfInstruction: end_instr();
     *
     * Up to the point where TRACE_REG is expanded, NO modifications
     * are done to any register EXCEPT the IP register, for fetch and
     * decoding purposes.
     *
     * This was done for an entirely different reason, but makes a
     * nice way to get the system to help debug codes.
     */
	tregs = M;
    M.x86.R_IP = off; 
    M.x86.R_CS = seg;
    M.x86.saved_ip = off; 
    M.x86.saved_cs = seg;
    
    /* reset the decoding buffers */
    M.x86.enc_str_pos = 0;
    M.x86.enc_pos = 0;
    
    /* turn on the "disassemble only, no execute" flag */
    M.x86.debug |= DEBUG_DISASSEMBLE_F;
 
    /* DUMP NEXT n instructions to screen in straight_line fashion */
    for (i=0; i<n; i++) {
	op1 = (*sys_rdb)(((u32)M.x86.R_CS<<4) + (M.x86.R_IP++));
	INC_DECODED_INST_LEN(1);
	(x86emu_optab[op1])(op1);
	M.x86.saved_ip = M.x86.R_IP;
    }
    finaloffset = M.x86.R_IP;
    M = tregs;	
    /* end major hack mode. */
    return finaloffset;
}

void x86emu_check_ip_access (void)
{
    /* NULL as of now */
}

void x86emu_check_sp_access (void)
{
}

void x86emu_check_mem_access (u32 dummy)
{
	/*  check bounds, etc */
}

void x86emu_check_data_access (uint dummy1, uint dummy2)
{
	/*  check bounds, etc */
}

void x86emu_inc_decoded_inst_len (int x)
{
	M.x86.enc_pos += x;
}

void x86emu_decode_printf (char *x)
{
	sprintf(M.x86.decoded_buf+M.x86.enc_str_pos,"%s",x);
	M.x86.enc_str_pos += strlen(x);
}

void x86emu_decode_printf2 (char *x, int y)
{
	char temp[100];
	sprintf(temp,x,y);
	sprintf(M.x86.decoded_buf+M.x86.enc_str_pos,"%s",temp);
	M.x86.enc_str_pos += strlen(temp);
}

void x86emu_end_instr (void)
{
	M.x86.enc_str_pos = 0;
	M.x86.enc_pos = 0;
}

static void print_encoded_bytes (u16 s, u16 o)
{
    int i;
    char buf1[64];
    buf1[0] = 0;
	for (i=0; i< M.x86.enc_pos; i++) {
		sprintf(buf1+2*i,"%02x", fetch_data_byte_abs(s,o+i));
    }
	printk("%-20s",buf1);
}

static void print_decoded_instruction (void)
{
	printk("%s", M.x86.decoded_buf);
}

void x86emu_print_int_vect (u16 iv)
{
	u16 seg,off;

	if (iv > 256) return;
	seg   = fetch_data_word_abs(0,iv*4);
	off   = fetch_data_word_abs(0,iv*4+2);
	printk("%04x:%04x ", seg, off);
}

void X86EMU_dump_memory (u16 seg, u16 off, u32 amt)
{
	u32 start = off & 0xfffffff0;
	u32 end  = (off+16) & 0xfffffff0;
	u32 i;
	u32 current;

	current = start;
	while (end <= off + amt) {
		printk("%04x:%04x ", seg, start);
		for (i=start; i< off; i++)
		  printk("   ");
		for (       ; i< end; i++)
		  printk("%02x ", fetch_data_byte_abs(seg,i));
		printk("\n");
		start = end;
		end = start + 16;
	}
}

void x86emu_single_step (void)
{
    char s[1024];
    int ps[10];
    int ntok;
    int cmd;
    int done;
    int segment;
    int offset;
    static int breakpoint;
    static int noDecode = 1;
    
    char *p;

    if (DEBUG_BREAK()) {
	if (M.x86.saved_ip != breakpoint) {
	    return;
	    } 
	else {
	    M.x86.debug &= ~DEBUG_DECODE_NOPRINT_F;
	    M.x86.debug |= DEBUG_TRACE_F;
	    M.x86.debug &= ~DEBUG_BREAK_F;
	    print_decoded_instruction ();
	    X86EMU_trace_regs();
	    }
	}
    done=0;
    offset = M.x86.saved_ip;
    while (!done) {
#ifdef _CFE_
	cmd = console_readline("-",s,1023);
	if (cmd) { s[cmd] = '\n'; s[cmd+1] = 0;}
	p = s;
#else
        printk("-");
        p = fgets(s, 1023, stdin);
#endif
        cmd = parse_line(s, ps, &ntok);
        switch(cmd) {
	    case 'u':
		if (ntok == 2) { offset = ps[1]; }
		offset = disassemble_forward(M.x86.saved_cs,(u16)offset,10);
		break;
	    case 'd':  
		if (ntok == 2) {
		    segment = M.x86.saved_cs;
		    offset = ps[1];
		    X86EMU_dump_memory(segment,(u16)offset,16);
		    offset += 16;
		    } else if (ntok == 3) {
			segment = ps[1];
			offset = ps[2];
			X86EMU_dump_memory(segment,(u16)offset,16);
			offset += 16;
			} else {
			    segment = M.x86.saved_cs;
			    X86EMU_dump_memory(segment,(u16)offset,16);
			    offset += 16;
			    }
		break;
	    case 'c':
		M.x86.debug ^= DEBUG_TRACECALL_F;
		break;
	    case 's':
		M.x86.debug ^= DEBUG_SVC_F | DEBUG_SYS_F | DEBUG_SYSINT_F;
		break;
	    case 'r':
		X86EMU_trace_regs();
		break;
	    case 'x':
		X86EMU_trace_xregs();
		break;
	    case 'g':
		if (ntok == 2) {
		    breakpoint = ps[1];
		    if (noDecode) {
			M.x86.debug |= DEBUG_DECODE_NOPRINT_F;
			} else {
			    M.x86.debug &= ~DEBUG_DECODE_NOPRINT_F;
			    }
		    M.x86.debug &= ~DEBUG_TRACE_F;
		    M.x86.debug |= DEBUG_BREAK_F;
		    done = 1;
		    }
		break;
	    case 'q':
		M.x86.debug &= ~DEBUG_TRACE_F;
		M.x86.debug &= ~DEBUG_BREAK_F;
		M.x86.debug &= ~DEBUG_STEP_F;
		M.x86.debug &= ~DEBUG_DECODE_F;
		done = 1;
		break;
	    case 'P':
		noDecode = (noDecode)?0:1;
		printk("Toggled decoding to %s\n",(noDecode)?"FALSE":"TRUE");
		break;
	    case 't':
	    case 0:
		done = 1;
		break;
	    }   
	}
}

int X86EMU_trace_on(void)
{
	return M.x86.debug |= DEBUG_STEP_F | DEBUG_DECODE_F | DEBUG_TRACE_F;
}

int X86EMU_trace_off(void)
{
	return M.x86.debug &= ~(DEBUG_STEP_F | DEBUG_DECODE_F | DEBUG_TRACE_F);
}

static int parse_line (char *s, int *ps, int *n)
{
    int cmd;

    *n = 0;
    while(*s == ' ' || *s == '\t') s++;
    ps[*n] = *s;
    switch (*s) {
      case '\n':
        *n += 1;
        return 0;
      default:
        cmd = *s;
        *n += 1;
    }

	while (1) {
		while (*s != ' ' && *s != '\t' && *s != '\n')  s++;
		
		if (*s == '\n')
			return cmd;
		
		while(*s == ' ' || *s == '\t') s++;
#ifdef _CFE_
		ps[*n] = xtoi(s);
#else		
		sscanf(s,"%x",&ps[*n]);
#endif
		*n += 1;
	}
}

#endif /* DEBUG */

void x86emu_dump_regs (void)
{
	printk("\tAX=%04x  ", M.x86.R_AX );
	printk("BX=%04x  ", M.x86.R_BX );
	printk("CX=%04x  ", M.x86.R_CX );
	printk("DX=%04x  ", M.x86.R_DX );
	printk("SP=%04x  ", M.x86.R_SP );
	printk("BP=%04x  ", M.x86.R_BP );
	printk("SI=%04x  ", M.x86.R_SI );
	printk("DI=%04x\n", M.x86.R_DI );
	printk("\tDS=%04x  ", M.x86.R_DS );
	printk("ES=%04x  ", M.x86.R_ES );
	printk("SS=%04x  ", M.x86.R_SS );
	printk("CS=%04x  ", M.x86.R_CS );
	printk("IP=%04x   ", M.x86.R_IP );
	if (ACCESS_FLAG(F_OF))    printk("OV ");     /* CHECKED... */
	else                        printk("NV ");
	if (ACCESS_FLAG(F_DF))    printk("DN ");
	else                        printk("UP ");
	if (ACCESS_FLAG(F_IF))    printk("EI ");
	else                        printk("DI ");
	if (ACCESS_FLAG(F_SF))    printk("NG ");
	else                        printk("PL ");
	if (ACCESS_FLAG(F_ZF))    printk("ZR ");
	else                        printk("NZ ");
	if (ACCESS_FLAG(F_AF))    printk("AC ");
	else                        printk("NA ");
	if (ACCESS_FLAG(F_PF))    printk("PE ");
	else                        printk("PO ");
	if (ACCESS_FLAG(F_CF))    printk("CY ");
	else                        printk("NC ");
	printk("\n");
}

void x86emu_dump_xregs (void)
{
	printk("\tEAX=%08x  ", M.x86.R_EAX );
	printk("EBX=%08x  ", M.x86.R_EBX );
	printk("ECX=%08x  ", M.x86.R_ECX );
	printk("EDX=%08x  \n", M.x86.R_EDX );
	printk("\tESP=%08x  ", M.x86.R_ESP );
	printk("EBP=%08x  ", M.x86.R_EBP );
	printk("ESI=%08x  ", M.x86.R_ESI );
	printk("EDI=%08x\n", M.x86.R_EDI );
	printk("\tDS=%04x  ", M.x86.R_DS );
	printk("ES=%04x  ", M.x86.R_ES );
	printk("SS=%04x  ", M.x86.R_SS );
	printk("CS=%04x  ", M.x86.R_CS );
	printk("EIP=%08x\n\t", M.x86.R_EIP );
	if (ACCESS_FLAG(F_OF))    printk("OV ");     /* CHECKED... */
	else                        printk("NV ");
	if (ACCESS_FLAG(F_DF))    printk("DN ");
	else                        printk("UP ");
	if (ACCESS_FLAG(F_IF))    printk("EI ");
	else                        printk("DI ");
	if (ACCESS_FLAG(F_SF))    printk("NG ");
	else                        printk("PL ");
	if (ACCESS_FLAG(F_ZF))    printk("ZR ");
	else                        printk("NZ ");
	if (ACCESS_FLAG(F_AF))    printk("AC ");
	else                        printk("NA ");
	if (ACCESS_FLAG(F_PF))    printk("PE ");
	else                        printk("PO ");
	if (ACCESS_FLAG(F_CF))    printk("CY ");
	else                        printk("NC ");
	printk("\n");
}
