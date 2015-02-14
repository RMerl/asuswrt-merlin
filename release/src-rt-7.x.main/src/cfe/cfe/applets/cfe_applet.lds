OUTPUT_ARCH(mips)
ENTRY(__start)
SECTIONS
{
  . = 0x20000000;
/*  . = 0x81000000; */
  .text : 
    {
     _ftext = . ;
    *(.init)
     eprol  =  .;
    *(.text)
    *(.fini)
    *(.rodata)
     _etext  =  .;
   }

  .data :
   {
   _gp = ALIGN(16) + 0x8000;
   _fdata = . ;
    *(.data)
    CONSTRUCTORS
    *(.sdata)
   }
   _edata  =  .;
   _fbss = .;
  .sbss : {
    *(.sbss)
    *(.scommon)
  }
  .bss : {
    *(.bss)
    *(COMMON)
  }
   _end = .;
}
