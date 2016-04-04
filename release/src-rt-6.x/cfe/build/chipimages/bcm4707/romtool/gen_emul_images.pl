#! /usr/bin/perl -w
# Generate ROM image

if (-e "./IPR_G40RMHS16384X64R532COVTSSY_WD0.codefile") {
  open(IN, "< IPR_G40RMHS16384X64R532COVTSSY_WD0.codefile");
  open(OUT, " > rom.bin");
  printf(OUT "\$INSTANCE iproc.u_rom_top.u_rom_word0.mem\n");
  printf(OUT "\$RADIX   BIN\n");
  printf(OUT "\$ADDRESS   0    7ff\n");
  $addr = 0;

  while (<IN>) {
    printf(OUT "%x   $_", $addr);
    $addr++;
  }
	
}

# Generate nand flash image
if (-e "./cfez.bin.mem") {
  open(IN, "< ./cfez.bin.mem");
  open(OUT, "> cfez-nand.hex");
  printf(OUT "\$INSTANCE nand_flash.mem_array\n");
  printf(OUT "\$RADIX   HEX\n");
  printf(OUT "\$ADDRESS   0    7ff\n");
  $addr = 0;

  while (<IN>) {
    if (/\'d.*h([\da-fA-F]+).*ADDR\s*=\s*([\da-fA-F]+)/) {
      printf(OUT "$2 $1\n");
    }
	
  }
}

# Generate spi flash image
if (-e "./images/spi.mem") {
  open(IN, "< ./images/spi.mem");
  open(OUT, "> sflash.hex");
  printf(OUT "\$INSTANCE qt_qspi_serial_flash.core_i.mem_array\n");
  printf(OUT "\$RADIX   HEX\n");
  printf(OUT "\$ADDRESS   0    7ff\n");
  $addr = 0;

  while (<IN>) {
    if (/\'d.*h([\da-fA-F]+).*ADDR\s*=\s*([\da-fA-F]+)/) {
      printf(OUT "$2 $1\n");
    }
	
  }
}

# Generate spi flash image
if (-e "./images/ddr.mem") {
  open(IN, "< ./images/ddr.mem");
  open(OUT, "> ddr.hex");
  printf(OUT "\$INSTANCE ddr_mem_zero_0.memcore\n");
  printf(OUT "\$RADIX   HEX\n");
  printf(OUT "\$ADDRESS   0    7ff\n");
  $addr = 0x0000000;

  while (<IN>) {
    if (/\'d.*h([\da-fA-F]+).*Addr\s*=\s*([\da-fA-F]+)/) {
      $addr = hex($2)/2;
      printf(OUT "%x $1\n", $addr);

    }
	
  }
}
