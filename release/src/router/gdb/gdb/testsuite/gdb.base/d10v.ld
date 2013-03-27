OUTPUT_FORMAT("elf32-d10v", "elf32-d10v",
	      "elf32-d10v")
OUTPUT_ARCH(d10v)
ENTRY(_start)
/* Do we need any of these for elf?
   __DYNAMIC = 0;    */

MEMORY
{
  UNIFIED : org = 0,         len = 0x1000000
  INSN    : org = 0x1014000, len = 0x40000
  DATA    : org = 0x2000004, len = 0x7FFC
  STACK   : org = 0x200BFFE, len = 4
}

SECTIONS
{
  /* Overlay sections: */
  .ovly0  0x1001000 : AT (0x8000) { foo.o(.text)  }
  .ovly1  0x1001000 : AT (0x9000) { bar.o(.text)  }
  .ovly2  0x1002000 : AT (0xa000) { baz.o(.text)  }
  .ovly3  0x1002000 : AT (0xb000) { grbx.o(.text) }
  .data00 0x2001000 : AT (0xc000) { foo.o(.data)  }
  .data01 0x2001000 : AT (0xd000) { bar.o(.data)  }
  .data02 0x2002000 : AT (0xe000) { baz.o(.data)  }
  .data03 0x2002000 : AT (0xf000) { grbx.o(.data) }

  .text :
  {
    KEEP (*(.init))
    KEEP (*(.init.*))
    KEEP (*(.fini))
    KEEP (*(.fini.*))
    *(.text)
    *(.text.*)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
    *(.gnu.linkonce.t*)
    _etext = .;
    PROVIDE (etext = .);
  } >INSN =0
  .rodata    : {
    *(.rodata) 
    *(.gnu.linkonce.r*)
    *(.rodata.*) 
  } >DATA
  .rodata1   : { 
    *(.rodata1)
    *(.rodata1.*)
  } >DATA

  .data    :
  {
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d*)
    _ovly_table = .; 
	LONG(ABSOLUTE(ADDR(.ovly0)));
	LONG(SIZEOF(.ovly0));
	LONG(LOADADDR(.ovly0));
	LONG(0);
	LONG(ABSOLUTE(ADDR(.ovly1)));
	LONG(SIZEOF(.ovly1));
	LONG(LOADADDR(.ovly1));
	LONG(0);
	LONG(ABSOLUTE(ADDR(.ovly2)));
	LONG(SIZEOF(.ovly2));
	LONG(LOADADDR(.ovly2));
	LONG(0);
	LONG(ABSOLUTE(ADDR(.ovly3)));
	LONG(SIZEOF(.ovly3));
	LONG(LOADADDR(.ovly3));
	LONG(0);
	LONG(ABSOLUTE(ADDR(.data00)));
	LONG(SIZEOF(.data00));
	LONG(LOADADDR(.data00));
	LONG(0);
	LONG(ABSOLUTE(ADDR(.data01)));
	LONG(SIZEOF(.data01));
	LONG(LOADADDR(.data01));
	LONG(0);
	LONG(ABSOLUTE(ADDR(.data02)));
	LONG(SIZEOF(.data02));
	LONG(LOADADDR(.data02));
	LONG(0);
	LONG(ABSOLUTE(ADDR(.data03)));
	LONG(SIZEOF(.data03));
	LONG(LOADADDR(.data03));
	LONG(0);
    _novlys = .;
	LONG((_novlys - _ovly_table) / 16);
    CONSTRUCTORS
  } >DATA
  .data1   : {
    *(.data1)
    *(.data1.*)
  } >DATA
  .ctors         :
  {
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */

    KEEP (*crtbegin.o(.ctors))

    /* We don't want to include the .ctor section from
       from the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */

    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
  } >DATA
  .dtors         :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  } >DATA
  /* We want the small data sections together, so single-instruction offsets
     can access them all, and initialized data all before uninitialized, so
     we can shorten the on-disk segment size.  */
  .sdata     : {
    *(.sdata)
    *(.sdata.*)
  } >DATA
  _edata  =  .;
  PROVIDE (edata = .);
  __bss_start = .;
  .sbss      : { *(.sbss) *(.scommon) } >DATA
  .bss       :
  {
   *(.dynbss)
   *(.dynbss.*)
   *(.bss)
   *(.bss.*)
   *(COMMON)
  } >DATA

  _end = . ;
  PROVIDE (end = .);

  .stack : { _stack = .; *(.stack) } >STACK

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }

  .comment 0 : { *(.comment) }

  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */

  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }

  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }

  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }

  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) *(.gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }

  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
}
