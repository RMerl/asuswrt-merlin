/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  VGA BIOS initialization			File: VGAINIT.C	
    *  
    *  This module interfaces with the X86 emulator borrowed from
    *  XFree86 to do VGA initialization.  
    *
    *  WARNING: This code is SB1250-specific for now.  It's not
    *  hard to change, but then again, aren't we interested in the 1250?
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
#include "lib_printf.h"
#include "lib_malloc.h"
#include "pcireg.h"
#include "pcivar.h"
#include "cfe_console.h"
#include "vga.h"
#include "pcibios.h"
#include "lib_physio.h"
#include "vga_subr.h"
#include "x86mem.h"
#include "x86emu.h"
#include "env_subr.h"


/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

#define BYTESONLY 0			/* Always write registers as bytes */
#define VGAINIT_NOISY 0			/* lots of debug output */

#define SB1250_PASS2_WORKAROUNDS

/*  *********************************************************************
    *  ISA port macros - currently SB1250-specific
    ********************************************************************* */

#define INB(x)    inb(x)
#define INW(x)    inw(x)
#define INL(x)    inl(x)
#define OUTB(x,y) outb(x,y)
#define OUTW(x,y) outw(x,y)
#define OUTL(x,y) outl(x,y)

/*  *********************************************************************
    *  ISA memory macros - currently SB1250-specific
    ********************************************************************* */

typedef uintptr_t vm_offset_t;

#if defined(_P5064_) || defined(_P6064_)
  #define PCI_MEM_SPACE	0x10000000	/* 128MB: s/w configurable */
  #define __ISAaddr(addr) ((physaddr_t)(PCI_MEM_SPACE+(addr)))
#else
  #define __ISAaddr(addr) ((physaddr_t)0x40000000+(addr))
#endif

#define __ISAreadbyte(addr)  phys_read8(__ISAaddr(addr))
#define __ISAreadword(addr)  phys_read16(__ISAaddr(addr))
#define __ISAreaddword(addr) phys_read32(__ISAaddr(addr))
#define __ISAwritebyte(addr,data)  phys_write8(__ISAaddr(addr),(data))
#define __ISAwriteword(addr,data)  phys_write16(__ISAaddr(addr),(data))
#define __ISAwritedword(addr,data) phys_write32(__ISAaddr(addr),(data))
  
/*  *********************************************************************
    *  Other macros
    ********************************************************************* */

#define OFFSET(addr)	(((addr) >> 0) & 0xffff)
#define SEGMENT(addr)	(((addr) >> 4) & 0xf000)

#define BSWAP_SHORT(s) ((((s) >> 8) & 0xFF) | (((s)&0xFF) << 8))
#define BSWAP_LONG(s) ((((s) & 0xFF000000) >> 24) | \
                       (((s) & 0x00FF0000) >> 8) | \
                       (((s) & 0x0000FF00) << 8) | \
                       (((s) & 0x000000FF) << 24))


#ifdef __MIPSEB
#define CPU_TO_LE16(s) BSWAP_SHORT(s)
#define CPU_TO_LE32(s) BSWAP_LONG(s)
#define LE16_TO_CPU(s) BSWAP_SHORT(s)
#define LE32_TO_CPU(s) BSWAP_LONG(s)
#else
#define CPU_TO_LE16(s) (s)
#define CPU_TO_LE32(s) (s)
#define LE16_TO_CPU(s) (s)
#define LE32_TO_CPU(s) (s)
#endif
 

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

int vga_biosinit(void);
int vga_probe(void);
extern void ui_restart(int);
void vgaraw_dump(char *tail);
int x86emutest(void);

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

static vga_term_t vga;
static x86mem_t x86mem;

#define BIOSRAMLOC		(0xC0000)
#define STACKSIZE 		4096
#define IRETOFFSET		12
static uint8_t x86initcode[] = {
#if (VGA_TEXTMODE_ROWS == 60)
    0xB8,0x02,0x4F,		/* mov ax,042F */
    0xBB,0x08,0x01,		/* mov bx,0108 */	/* VESA 80x60 */
#else
    0xB8,0x03,0x00,		/* mov AX,0003 */	/* 80x25 mode */
#endif

    0xCD,0x10,			/* int 10 */
    0xB8,0x34,0x12,		/* mov ax,1234 */
    0xBB,0x78,0x56,		/* mov bx,5678 */
    0xCC,			/* int 3 */
    0xCF};			/* IRET */

static uint8_t x86testcode[] = {
    0x90,0x90,0x90,0x90,0x90,	/* nop, nop, nop, nop, nop */
    0xeb,0x09,			/* jmp 10 */
    0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90, /* 9 nops */
    0xb8,0x34,0x12,		/* mov ax,1234 */
    0xbb,0x78,0x56,		/* mov bx,5678 */
    0xcc,			/* int 3 */
    0xcf};			/* iret */

static uint32_t __ISAreadmem(x86mem_t *mem,uint32_t addr,int size)
{
    unsigned long val;

    switch (size) {
	case M_BYTE:
	    val = __ISAreadbyte(addr);
	    break;
	case M_WORD:
	    if (BYTESONLY || (addr & 0x1)) {
		val = (__ISAreadbyte(addr) | (__ISAreadbyte(addr + 1) << 8));
		} 
	    else {
		val = __ISAreadword(addr);
		val = LE16_TO_CPU(val);
		}
	    break;
	case M_DWORD:
	    if (BYTESONLY || (addr & 0x3)) {
		val = (__ISAreadbyte(addr) |
		       (__ISAreadbyte(addr + 1) << 8) |
		       (__ISAreadbyte(addr + 2) << 16) |
		       (__ISAreadbyte(addr + 3) << 24));
		} 
	    else {
		val = __ISAreaddword(addr);
		val = LE32_TO_CPU(val);
		}
	    break;
	default:
	    val = 0;
	    }

    return val;
}



static void __ISAwritemem(x86mem_t *mem,uint32_t addr,uint32_t data,int size)
{
    switch (size) {
	case M_BYTE:
	    __ISAwritebyte(addr, data);
	    break;

	case M_WORD:
	    if (BYTESONLY || (addr & 0x1)) {
		__ISAwritebyte(addr, data >> 0);
		__ISAwritebyte(addr + 1, data >> 8);
		} 
	    else {
		data = CPU_TO_LE16(data);
		__ISAwriteword(addr, data);
		}
	    break;

	case M_DWORD:
	    if (BYTESONLY || (addr & 0x3)) {
		__ISAwritebyte(addr, data >> 0);
		__ISAwritebyte(addr + 1, data >> 8);
		__ISAwritebyte(addr + 2, data >> 16);
		__ISAwritebyte(addr + 3, data >> 24);
		} 
	    else {
		data = CPU_TO_LE32(data);
		__ISAwritedword(addr, data);
		}
	    break;
	}
}


static u8 __x86emu_rdb(u32 addr)
{
#if VGAINIT_NOISY
    if ((addr < 0x400) || (addr > 0x100000))  {
	xprintf("Read %08X (int %02X)  ",addr,addr/4);
	printf("CS:IP = %04X:%04X\n",M.x86.R_CS,M.x86.R_IP);
	}
#endif
    return x86mem_readb(&x86mem,addr);
}


static u16 __x86emu_rdw(u32 addr)
{
#if VGAINIT_NOISY
    if ((addr < 0x400) || (addr > 0x100000))  {
	xprintf("Read %08X (int %02X)  ",addr,addr/4);
	printf("CS:IP = %04X:%04X\n",M.x86.R_CS,M.x86.R_IP);
	}
#endif
    return x86mem_readw(&x86mem,addr);
}


static u32 __x86emu_rdl(u32 addr)
{
#if VGAINIT_NOISY
    if ((addr < 0x400) || (addr > 0x100000))  {
	xprintf("Read %08X (int %02X)  ",addr,addr/4);
	printf("CS:IP = %04X:%04X\n",M.x86.R_CS,M.x86.R_IP);
	}
#endif
    return x86mem_readl(&x86mem,addr);
}


static void __x86emu_wrb(u32 addr, u8 val)
{
#if VGAINIT_NOISY
    if ((addr < 0x400) || (addr > 0x100000))  {
	xprintf("Write %08X (int %02X)  ",addr,addr/4);
	printf("CS:IP = %04X:%04X\n",M.x86.R_CS,M.x86.R_IP);
	}
#endif
    x86mem_writeb(&x86mem,addr,val);
}


static void __x86emu_wrw(u32 addr, u16 val)
{
#if VGAINIT_NOISY
    if ((addr < 0x400) || (addr > 0x100000))  {
	xprintf("Write %08X %04X (int %02X)  ",addr,val,addr/4);
	printf("CS:IP = %04X:%04X\n",M.x86.R_CS,M.x86.R_IP);
	}
#endif
    x86mem_writew(&x86mem,addr,val);
}


static void __x86emu_wrl(u32 addr, u32 val)
{
#if VGAINIT_NOISY
    if ((addr < 0x400) || (addr > 0x100000))  {
	xprintf("Write %08X (int %02X)  ",addr,addr/4);
	printf("CS:IP = %04X:%04X\n",M.x86.R_CS,M.x86.R_IP);
	}
#endif
    x86mem_writel(&x86mem,addr,val);
}


#define TS_COMMAND	0
#define TS_DATA1	1
#define TS_DATA2	2
static uint16_t timerCount = 0;
static int timerState = TS_COMMAND;
static u8 __x86emu_inb(X86EMU_pioAddr port)
{
    u8 val;

    /*
     * Emulate just enough functionality of the
     * timer chip to fool the Trident BIOS
     */
    if (port == 0x40) {
	timerCount++;
	switch (timerState) {
	    case TS_COMMAND:
		return 0;
	    case TS_DATA1:
		timerState = TS_DATA1;
		return timerCount & 0xFF;
	    case TS_DATA2:
		timerState = TS_COMMAND;
		return (timerCount >> 8) & 0xFF;
	    }
	}

#ifdef SB1250_PASS2_WORKAROUNDS
    if ((port < 0x3BC) || (port > 0x3DF)) {
	return 0;
	}
#endif

    val = INB(port);

#if VGAINIT_NOISY
    /*if (port < 0x100)*/ xprintf("INB %08X %02X\n",port,val);
    if (console_status()) ui_restart(0);
#endif


    return val;
}


static u16 __x86emu_inw(X86EMU_pioAddr port)
{
    u16 val;

#ifdef SB1250_PASS2_WORKAROUNDS
    if ((port < 0x3BC) || (port > 0x3DF)) {
	return 0;
	}
#endif

    val = INW(port);

    val = LE16_TO_CPU(val);

#if VGAINIT_NOISY
    /*if (port < 0x100)*/ xprintf("INW %08X %04X\n",port,val);
#endif

    return val;
}


static u32 __x86emu_inl(X86EMU_pioAddr port)
{
    u32 val;

#ifdef SB1250_PASS2_WORKAROUNDS
    if ((port < 0x3BC) || (port > 0x3DF)) {
	return 0;
	}
#endif

    val = INL(port);

    val = LE32_TO_CPU(val);

#if VGAINIT_NOISY
    /*if (port < 0x100)*/ xprintf("INL %08X %08X\n",port,val);
#endif


    return val;
}


static void __x86emu_outb(X86EMU_pioAddr port, u8 val)
{
    /*
     * Emulate just enough functionality of the timer
     * chip to fool the Trident BIOS
     */
    if (port == 0x43) {
	timerCount++;
	timerState = TS_DATA1;
	return;
	}

#if VGAINIT_NOISY
    /*if (port < 0x100)*/ xprintf("OUTB %08X %08X\n",port,val);
#endif

#ifdef SB1250_PASS2_WORKAROUNDS
    if ((port < 0x3BC) || (port > 0x3DF)) {
	return;
	}
#endif

    OUTB(port,val);
}


static void __x86emu_outw(X86EMU_pioAddr port, u16 val)
{
    val = CPU_TO_LE16(val);

#if VGAINIT_NOISY
    /*if (port < 0x100)*/ xprintf("OUTW %08X %04X  ",port,val);
	printf("CS:IP = %04X:%04X\n",M.x86.R_CS,M.x86.R_IP);
#endif

#ifdef SB1250_PASS2_WORKAROUNDS
    if ((port < 0x3BC) || (port > 0x3DF)) {
	return;
	}
#endif

    OUTW(port,val);
}


static void __x86emu_outl(X86EMU_pioAddr port, u32 val)
{
    if (port == 0x2D) return;

    val = CPU_TO_LE32(val);

#if VGAINIT_NOISY
    /*if (port < 0x100)*/ xprintf("OUTL %08X %08X  ",port,val);
	printf("CS:IP = %04X:%04X\n",M.x86.R_CS,M.x86.R_IP);
#endif

#ifdef SB1250_PASS2_WORKAROUNDS
    if ((port < 0x3BC) || (port > 0x3DF)) {
	return;
	}
#endif


    OUTL(port,val);
}


static void regs2tag(pcitag_t *tag)
{
    pcitag_t mytag;
    int bus,device,function;

    bus = M.x86.R_BH;
    device = M.x86.R_BL >> 3;
    function = M.x86.R_BL & 0x07;

    mytag = pci_make_tag(0,bus,device,function);

    *tag = mytag;
}

static void __SIMint10(int intno)
{
#if VGAINIT_NOISY
    xprintf("Int10: BIOS function AX=%04X\n",M.x86.R_AX);
#endif

    /*
     * The only BIOS function that VGAs appear to 
     * depend on in the real BIOS is the one
     * that enables/disables video memory.
     */

    if ((M.x86.R_AH == 0x12) && (M.x86.R_BL == 0x32)) {
	if (M.x86.R_AL == 0) {
	    /* enable video memory */
	    __x86emu_outb(VGA_MISCOUTPUT_W, __x86emu_inb(VGA_MISCOUTPUT_R) | 0x02);
	    return;
	    } 
	else if (M.x86.R_AL == 1) {
	    /* disable video memory */
	    __x86emu_outb(VGA_MISCOUTPUT_W, __x86emu_inb(VGA_MISCOUTPUT_R) & ~0x02);
	    return;
	    } 
	else {
	    xprintf("Int10 unknown function AX=%04X\n",
		 M.x86.R_AX);
	    }
	} 
    else {

	/* Otherwise, pass the int10 on to the ROM */

	X86EMU_prepareForInt(0x10);
	}
}


static void __SIMint3(int intno)
{
#if VGAINIT_NOISY
    xprintf("Int3: Breakpoint reached.\n");
#endif
    HALT_SYS();
}


static void __SIMintunk(int intno)
{
#if VGAINIT_NOISY
    xprintf("Int%02X: Unhandled interrupt!\n",intno);
#endif
    HALT_SYS();
}

static void __SIMint42(int intno)
{
#if VGAINIT_NOISY
    xprintf("Int42: Function AX=%04X\n",M.x86.R_AX);
#endif
    switch (M.x86.R_AH) {
	case 0:
	    vga_reset(&vga);
	    break;
	default:
#if VGAINIT_NOISY
	    xprintf("Int42: Unknown INT42 command: %x\n",M.x86.R_AH);
#endif
	    break;
	}
}


static void __SIMint6D(int intno)
{
    int reflect = 1;

#if VGAINIT_NOISY
    xprintf("Int6D: Function AX=%04X\n",M.x86.R_AX);
#endif

    switch (M.x86.R_AH) {
	case 0:
	    break;
	case 0x13:
	    if (M.x86.R_AL == 1) {
		unsigned long addr;
		unsigned long count;
		uint8_t ch;

		addr = (M.x86.R_ES << 4) + M.x86.R_BP;
		count = M.x86.R_CX;

		while (count) {
		    ch = __x86emu_rdb(addr);
		    vga_writechar(&vga,ch,M.x86.R_BL);
		    addr++;	
		    count--;
		    }
		reflect = 0;
		}
	    break;
	default:
#if VGAINIT_NOISY
	    xprintf("Unknown INT6D command: %x\n",M.x86.R_AH);
#endif
	    break;
	}

    if (reflect) X86EMU_prepareForInt(0x6D);
}



static void __SIMint1A(int intno)
{
    pcitag_t tag;
    int bus,device,function;
    int ret;

    if (M.x86.R_AH != PCIBIOS_FN_MAJOR) return;

    switch (M.x86.R_AL) {
	case PCIBIOS_FN_INSTCHK:
	    M.x86.R_EAX = 0x00;	
	    M.x86.R_AL = 0x01;
	    M.x86.R_EDX = PCIBIOS_SIGNATURE;
	    M.x86.R_EBX = PCIBIOS_VERSION;
	    M.x86.R_ECX &= 0xFF00;
	    M.x86.R_CL = 0;	/* Highest bus number */
#ifdef VGAINIT_NOISY
	    xprintf("Int1A: Installation check\n");
#endif
	    CLEAR_FLAG(F_CF);
	    break;

	case PCIBIOS_FN_FINDDEV:
	    ret = pci_find_device(M.x86.R_DX,M.x86.R_CX,M.x86.R_SI,&tag);
#if VGAINIT_NOISY
	    xprintf("Int1A: Find device VID=%04X,DID=%04X,Idx=%d: ",
		    M.x86.R_DX,M.x86.R_CX,M.x86.R_SI);
	    if (ret == 0) {
		pci_break_tag(tag,NULL,&bus,&device,&function);
		xprintf("Found Bus%d, Dev%d, Func%d\n",bus,device,function);
		}
	    else {
		xprintf("not found.\n");
		}
#endif
	    if (ret == 0) {
		pci_break_tag(tag,NULL,&bus,&device,&function);
		M.x86.R_BH = bus;
		M.x86.R_BL = (device << 3) | function;
		M.x86.R_AH = PCIBIOS_SUCCESSFUL;
		}
	    else {		
		M.x86.R_AH = PCIBIOS_DEVICE_NOT_FOUND;
		}
	    
	    CONDITIONAL_SET_FLAG((M.x86.R_AH != PCIBIOS_SUCCESSFUL),F_CF);
	    break;

	case PCIBIOS_FN_FINDCLASS:
	    ret = pci_find_class(M.x86.R_ECX,M.x86.R_SI,&tag);
#if VGAINIT_NOISY
	    xprintf("Int1A: Find Class %08X,Idx=%d: ",
		    M.x86.R_ECX,M.x86.R_SI);
	    if (ret == 0) {
		pci_break_tag(tag,NULL,&bus,&device,&function);
		xprintf("Found Bus%d, Dev%d, Func%d\n",bus,device,function);
		}
	    else {
		xprintf("not found.\n");
		}
#endif

	    if (ret == 0) {
		pci_break_tag(tag,NULL,&bus,&device,&function);
		M.x86.R_BH = bus;
		M.x86.R_BL = (device << 3) | function;
		M.x86.R_AH = PCIBIOS_SUCCESSFUL;
		}
	    else {		
		M.x86.R_AH =PCIBIOS_DEVICE_NOT_FOUND;
		}

	    CONDITIONAL_SET_FLAG((M.x86.R_AH != PCIBIOS_SUCCESSFUL),F_CF);
	    break;

	case PCIBIOS_FN_RDCFGBYTE:
	    regs2tag(&tag);
	    M.x86.R_CL = pci_conf_read8(tag,M.x86.R_DI);
	    M.x86.R_AH = PCIBIOS_SUCCESSFUL;
#if VGAINIT_NOISY
	    xprintf("Int1A: Read Cfg Byte %04X from ",M.x86.R_DI);
	    pci_break_tag(tag,NULL,&bus,&device,&function);
	    xprintf("Bus%d, Dev%d, Func%d",bus,device,function);
	    xprintf(": %02X\n",M.x86.R_CX);
#endif

	    CLEAR_FLAG(F_CF);
	    break;

	case PCIBIOS_FN_RDCFGWORD:
	    regs2tag(&tag);
	    M.x86.R_CX = pci_conf_read16(tag,M.x86.R_DI);
	    M.x86.R_AH = PCIBIOS_SUCCESSFUL;
#if VGAINIT_NOISY
	    xprintf("Int1A: Read Cfg Word %04X from ",M.x86.R_DI);
	    pci_break_tag(tag,NULL,&bus,&device,&function);
	    xprintf("Bus%d, Dev%d, Func%d",bus,device,function);
	    xprintf(": %04X\n",M.x86.R_CX);
#endif
	    CLEAR_FLAG(F_CF);
	    break;

	case PCIBIOS_FN_RDCFGDWORD:
	    regs2tag(&tag);
	    M.x86.R_ECX = pci_conf_read(tag,M.x86.R_DI);
#if VGAINIT_NOISY
	    xprintf("Int1A: Read Cfg Dword %04X from ",M.x86.R_DI);
	    pci_break_tag(tag,NULL,&bus,&device,&function);
	    xprintf("Bus%d, Dev%d, Func%d",bus,device,function);
	    xprintf(": %08X\n",M.x86.R_ECX);
#endif
	    M.x86.R_AH = PCIBIOS_SUCCESSFUL;
	    CLEAR_FLAG(F_CF);
	    break;

	case PCIBIOS_FN_WRCFGBYTE:
	    regs2tag(&tag);
	    pci_conf_write8(tag,M.x86.R_DI,M.x86.R_CL);
#if VGAINIT_NOISY
	    xprintf("Int1A: Write Cfg byte %04X to ",M.x86.R_DI);
	    pci_break_tag(tag,NULL,&bus,&device,&function);
	    xprintf("Bus%d, Dev%d, Func%d",bus,device,function);
	    xprintf(": %02X\n",M.x86.R_CL);
#endif

	    M.x86.R_AH = PCIBIOS_SUCCESSFUL;
	    CLEAR_FLAG(F_CF);
	    break;

	case PCIBIOS_FN_WRCFGWORD:
	    regs2tag(&tag);
	    pci_conf_write16(tag,M.x86.R_DI,M.x86.R_CX);
#if VGAINIT_NOISY
	    xprintf("Int1A: Write Cfg Word %04X to ",M.x86.R_DI);
	    pci_break_tag(tag,NULL,&bus,&device,&function);
	    xprintf("Bus%d, Dev%d, Func%d",bus,device,function);
	    xprintf(": %04X\n",M.x86.R_CX);
#endif
	    M.x86.R_AH = PCIBIOS_SUCCESSFUL;
	    CLEAR_FLAG(F_CF);
	    break;

	case PCIBIOS_FN_WRCFGDWORD:
	    regs2tag(&tag);
	    pci_conf_write(tag,M.x86.R_DI,M.x86.R_ECX);
#if VGAINIT_NOISY
	    xprintf("Int1A: Write Cfg Dword %04X to ",M.x86.R_DI);
	    pci_break_tag(tag,NULL,&bus,&device,&function);
	    xprintf("Bus%d, Dev%d, Func%d",bus,device,function);
	    xprintf(": %08X\n",M.x86.R_ECX);
#endif
	    M.x86.R_AH = PCIBIOS_SUCCESSFUL;
	    CLEAR_FLAG(F_CF);
	    break;

	default:
#if VGAINIT_NOISY
	    xprintf("Int1A: Unimplemented PCI BIOS function AX=%04x\n", M.x86.R_AX);
#endif
	break;
	}
}



static int x86init(void)
{
    /*
     * Access functions for I/O ports
     */
    static X86EMU_pioFuncs piofuncs = {
	__x86emu_inb,
	__x86emu_inw,
	__x86emu_inl,
	__x86emu_outb,
	__x86emu_outw,
	__x86emu_outl
    };

    /*
     * Access functions for memory
     */
    static X86EMU_memFuncs memfuncs = {
	__x86emu_rdb,
	__x86emu_rdw,
	__x86emu_rdl,
	__x86emu_wrb,
	__x86emu_wrw,
	__x86emu_wrl
    };

    /*
     * Interrupt table
     */
    void (*funcs[256])(int num);
    int idx;

    /*
     * Establish hooks in the simulator
     */
    X86EMU_setupMemFuncs(&memfuncs);
    X86EMU_setupPioFuncs(&piofuncs);

    /*
     * Decode what X86 software interrupts we need to hook
     */

    for (idx = 0; idx < 256; idx++) {
	funcs[idx] = __SIMintunk;	/* assume all are bad */
	}
    funcs[0x42] = __SIMint42;		/* int42: video BIOS */
    funcs[0x1F] = NULL;			/* reflect INT1F */
    funcs[0x43] = NULL;			/* reflect INT43 */
    funcs[0x6D] = __SIMint6D;		/* int6D: video BIOS */

    funcs[0x03] = __SIMint3;		/* int3: firmware exit */
    funcs[0x10] = __SIMint10;		/* int10: video BIOS */
    funcs[0x1A] = __SIMint1A;		/* int1A: PCI BIOS */

    X86EMU_setupIntrFuncs(funcs);

    x86mem_init(&x86mem);
    x86mem_hook(&x86mem,0xA0000,__ISAreadmem,__ISAwritemem);
    x86mem_hook(&x86mem,0xA8000,__ISAreadmem,__ISAwritemem);
    x86mem_hook(&x86mem,0xB0000,__ISAreadmem,__ISAwritemem);
    x86mem_hook(&x86mem,0xB8000,__ISAreadmem,__ISAwritemem);

    return 0;

}


static void x86uninit(void)
{
    x86mem_uninit(&x86mem);
}


int vga_probe(void)
{
    pcitag_t tag;

    if (pci_find_class(PCI_CLASS_DISPLAY,0,&tag) == 0) {
	return 0;
	}

    return -1;
}

int vga_biosinit(void)
{
    physaddr_t biosaddr;
    pcitag_t tag;
    uint32_t addr;
    uint32_t romaddr;
    uint32_t destaddr;
    uint32_t stackaddr;
    uint32_t iretaddr;
    unsigned int biossize;
    int bus,device,function;
    int idx;
    int res;

    if (pci_find_class(PCI_CLASS_DISPLAY,0,&tag) == 0) {
	romaddr = pci_conf_read(tag,PCI_MAPREG_ROM);
	pci_conf_write(tag,PCI_MAPREG_ROM,romaddr | PCI_MAPREG_ROM_ENABLE);
	}
    else {
	xprintf("No suitable VGA device found in the system.\n");
	return -1;
	}

    addr = romaddr;
    addr &= PCI_MAPREG_ROM_ADDR_MASK;
#if defined(_P5064_) || defined(_P6064_)
    biosaddr = cpu_isamap((vm_offset_t) romaddr,0);
#else
    biosaddr = (physaddr_t) romaddr;
#endif

    /*
     * Check for the presence of a VGA BIOS on this adapter.
     */

    if (!((phys_read8(biosaddr+PCIBIOS_ROMSIG_OFFSET+0) == PCIBIOS_ROMSIG1) && 
	  (phys_read8(biosaddr+PCIBIOS_ROMSIG_OFFSET+1) == PCIBIOS_ROMSIG2))) {
	xprintf("No VGA BIOS on this adapter.\n");
	pci_conf_write(tag,PCI_MAPREG_ROM,romaddr);
	return -1;
	}
    biossize = PCIBIOS_ROMSIZE(phys_read8(biosaddr+PCIBIOS_ROMSIZE_OFFSET));

#if VGAINIT_NOISY
    xprintf("VGA BIOS size is %d bytes\n",biossize);
#endif

    /*
     * Initialize the X86 emulator
     */

    if (x86init() != 0) {
	xprintf("X86 emulator did not initialize.\n");
	pci_conf_write(tag,PCI_MAPREG_ROM,romaddr);
	return -1;
	}



    destaddr = BIOSRAMLOC;
    stackaddr = destaddr + biossize + STACKSIZE;

    /*
     * Copy the BIOS from the PCI rom into RAM
     */

#if VGAINIT_NOISY
    xprintf("Copying VGA BIOS to RAM.\n");
#endif

    for (idx = 0; idx < biossize; idx+=4) {
	uint32_t b;

	b = phys_read32(biosaddr+idx);
	x86mem_memcpy(&x86mem,destaddr+idx,(uint8_t *) &b,sizeof(uint32_t));
	}

    /*
     * Gross!  The NVidia TNT2 BIOS expects to
     * find a PC ROM BIOS date (just the slashes)
     * at the right place in the ROMs.
     */

    x86mem_memcpy(&x86mem,0xFFFF5,"08/13/99",8);

    /*
     * Turn off the BIOS ROM, we have our copy now.
     */

    pci_conf_write(tag,PCI_MAPREG_ROM,romaddr);

    /*
     * Point certain vectors at a dummy IRET in our code space.
     * Some ROMs don't take too kindly to null vectors, like
     * the 3dfx Voodoo3 BIOS, which makes sure int1a is
     * filled in before it attempts to call it.  The
     * code here is never really executed, since the emulator
     * hooks it.
     */

    iretaddr = stackaddr + IRETOFFSET;
    __x86emu_wrw(0x1A*4+0,OFFSET(iretaddr));
    __x86emu_wrw(0x1A*4+2,SEGMENT(iretaddr));


    /*
     * The actual code begins 3 bytes after the beginning of the ROM.  Set
     * the start address to the first byte of executable code.
     */

    M.x86.R_CS = SEGMENT(destaddr + PCIBIOS_ROMENTRY_OFFSET);
    M.x86.R_IP = OFFSET(destaddr + PCIBIOS_ROMENTRY_OFFSET);

    /*
     * Set the stack to point after our copy of the ROM
     */

    M.x86.R_SS = SEGMENT(stackaddr - 8);
    M.x86.R_SP = OFFSET(stackaddr - 8);

    /*
     * GROSS!  The Voodoo3 card expects BP to have
     * the following value:
     */

    M.x86.R_BP = 0x197;

    /*
     * The PCI BIOS spec says you pass the bus, device, and function
     * numbers in the AX register when starting the ROM code.
     */

    pci_break_tag(tag,NULL,&bus,&device,&function);
    M.x86.R_AH = bus;
    M.x86.R_AL = (device << 3) | (function & 7);

    /*
     * Arrange for the return address to point to a little piece
     * of code that will do an int10 to set text mode, followed
     * by storing a couple of simple signatures in the registers,
     * and an int3 to stop the simulator.
     *
     * The location of this piece of code is just after our
     * stack, and since the stack grows down, this is in 'stackaddr'
     */

    __x86emu_wrw(stackaddr-8,OFFSET(stackaddr));
    __x86emu_wrw(stackaddr-6,SEGMENT(stackaddr));

    /* copy in the code. */

    for (idx = 0; idx < sizeof(x86initcode); idx++) {
	__x86emu_wrb(stackaddr+idx,x86initcode[idx]);
	}

    /*
     * Set up the VGA console descriptor.  We need this to process the
     * int10's that write firmware copyright notices and such.
     */

    vga_init(&vga,(__ISAaddr(VGA_TEXTBUF_COLOR)),outb);

    /*
     * Launch the simulator.	
     */

    xprintf("Initializing VGA.\n");
#ifdef DEBUG
    X86EMU_trace_on();
#endif
    X86EMU_exec();

    /*
     * Check for the magic exit values in the registers.  These get set
     * by the code in the array 'x86initcode' that was loaded above
     */

    if ((M.x86.R_AX == 0x1234) && (M.x86.R_BX == 0x5678)) res = 0;
    else res = -1;

    /*
     * Done! 
     */

    x86uninit();

    if (res < 0) {
	xprintf("VGA initialization failed.\n");
	}
    else {
	char temp[32];
	char *str = "If you can see this message, the VGA has been successfully initialized!\r\n\r\n";

	xprintf("VGA initialization successful.\n");
	vga_writestr(&vga,str,0x07,strlen(str));

	sprintf(temp,"%d",VGA_TEXTMODE_ROWS);
	env_setenv("VGA_ROWS",temp,ENV_FLG_BUILTIN | ENV_FLG_READONLY | ENV_FLG_ADMIN);
	sprintf(temp,"%d",VGA_TEXTMODE_COLS);
	env_setenv("VGA_COLS",temp,ENV_FLG_BUILTIN | ENV_FLG_READONLY | ENV_FLG_ADMIN);

	}

    return res;
}

int x86emutest(void)
{
    uint32_t destaddr;
    uint32_t stackaddr;
    int res;

    /*
     * Initialize the X86 emulator
     */

    if (x86init() != 0) {
	xprintf("X86 emulator did not initialize.\n");
	return -1;
	}

    destaddr = BIOSRAMLOC;
    stackaddr = destaddr + 1024;

    /*
     * Copy the BIOS from the PCI rom into RAM
     */

    xprintf("Copying test program to RAM.\n");
    x86mem_memcpy(&x86mem,destaddr,x86testcode,sizeof(x86testcode));

    /*
     * The actual code begins 3 bytes after the beginning of the ROM.  Set
     * the start address to the first byte of executable code.
     */

    M.x86.R_CS = SEGMENT(destaddr + PCIBIOS_ROMENTRY_OFFSET);
    M.x86.R_IP = OFFSET(destaddr + PCIBIOS_ROMENTRY_OFFSET);

    /*
     * Set the stack to point after our copy of the ROM
     */

    M.x86.R_SS = SEGMENT(stackaddr - 8);
    M.x86.R_SP = OFFSET(stackaddr - 8);

    /*
     * Launch the simulator.	
     */

    xprintf("Running X86emu test.\n");
#ifdef DEBUG
    X86EMU_trace_on();
#endif
    X86EMU_exec();

    /*
     * Check for the magic exit values in the registers.  These get set
     * by the code in the array 'x86initcode' that was loaded above
     */

    if ((M.x86.R_AX == 0x1234) && (M.x86.R_BX == 0x5678)) res = 0;
    else res = -1;

    /*
     * Done! 
     */

    x86uninit();

    if (res < 0) xprintf("X86emu test failed.\n");
    else xprintf("X86emu test successful.\n");

    return res;
}



void vgaraw_dump(char *tail)
{
    physaddr_t biosaddr;
    pcitag_t tag;
    uint32_t addr;
    uint32_t romaddr;
    unsigned int biossize;
    int idx;
    int res;

    if (pci_find_class(PCI_CLASS_DISPLAY,0,&tag) == 0) {
	romaddr = pci_conf_read(tag,PCI_MAPREG_ROM);
	pci_conf_write(tag,PCI_MAPREG_ROM,romaddr | PCI_MAPREG_ROM_ENABLE);
	}
    else {
	xprintf("No suitable VGA device found in the system.\n");
	return ;
	}

    addr = romaddr;
    addr &= PCI_MAPREG_ROM_ADDR_MASK;
    
#if defined(_P5064_) || defined(_P6064_)
    biosaddr = cpu_isamap((vm_offset_t) romaddr,0);
#else
    biosaddr = romaddr;
#endif

    /*
     * Check for the presence of a VGA BIOS on this adapter.
     */

    xprintf("VGA BIOS is mapped to %08X\n",(uint32_t) biosaddr);

    if (!((phys_read8(biosaddr+PCIBIOS_ROMSIG_OFFSET+0) == PCIBIOS_ROMSIG1) && 
	  (phys_read8(biosaddr+PCIBIOS_ROMSIG_OFFSET+1) == PCIBIOS_ROMSIG2))) {
	xprintf("No VGA BIOS on this adapter, assuming 32K ROM\n");
	biossize = 32768;
	return;
	}
    else {
	biossize = PCIBIOS_ROMSIZE(phys_read8(biosaddr+PCIBIOS_ROMSIZE_OFFSET));
	xprintf("VGA BIOS size is %d bytes\n",biossize);
	}

    for (idx = 0; idx < biossize; idx+=16) {
	xprintf("%04X: ",idx);
	for (res = 0; res < 16; res++) {
	    xprintf("%02X ",phys_read8(biosaddr+idx+res));
	    }
	xprintf("\n");
	if (console_status()) break;
	}

//    pci_conf_write(tag,PCI_MAPREG_ROM,romaddr);
    
}
