/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  README
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

RELEASE NOTES FOR:  CFE version 1.0.36

------------------------------------------------------------------------------
INTRODUCTION
------------------------------------------------------------------------------

This directory contains Broadcom's Broadband processor division
"Common Firmware Environment," or CFE.  (pronounce it 'cafe' if you like)

It is intended to be a flexible toolkit of CPU initialization and 
bootstrap code for use on processors like the SB1250 and its derivatives.

CFE contains the following important features:

   * Easy to port to new SB1250 designs
   * Initializes CPUs, caches, memory controllers, and peripherals
   * Built-in device drivers for SB1250 SOC peripherals
   * Several console choices, including serial ports, ROM
     emulators, JTAG, etc.
   * Environment storage in NV EEPROM, flash, etc.
   * Supports big or little endian operation
   * Supports 32-bit and 64-bit operation
   * Support for network bootstrap.  Network protocols supported 
     include IP,ARP,ICMP,UDP,DHCP,TFTP.
   * Support for disk bootstrap.
   * Provides an external API for boot loaders and startup programs
   * Simple user interface.  UI is easy to remove for embedded apps.

See the file 'TODO' for a list of things that are being considered
as CFE matures.

There is some documentation in PDF format in the docs/ directory.

------------------------------------------------------------------------
Customers: If you have any comments or suggestions about this
firmware, please email me at mpl@broadcom.com - I have tried to
anticipate many common requirements, but I need your feedback to
make this software complete and useful for your designs.
------------------------------------------------------------------------

Directory organization
----------------------

CFE is laid out to make it easy to build and maintain versions for
different boards at the same time.  The directories at this level 
are the build areas for ports of CFE:

	cfe/		Main CFE source tree
	build/		The "new" build tree location

The 'build' directory contains build areas for various
targets, and the skeletal Makefiles that build them.  This
directory has subdirectories by vendor, so you can create
your own directory here and avoid hassles with merging changes.

	build/broadcom/bcm947xx/	   BCM947XX evaluation board

The 'arch' directory contains architecture-specific stuff:

	cfe/arch		Top of architecture tree
	cfe/arch/mips		All MIPS-related subdirectories
	cfe/arch/mips/cpu	CPU-specfic subdirectories
	cfe/arch/mips/board	Board-specific subdirectories
	cfe/arch/mips/common	Common MIPS-related sources

Platform-independent sources continue to live in the cfe/
directory and its subdirectories:

	cfe/api/     Application programming interface to CFE
	cfe/applets/	Test "applets" for firmware API
	cfe/dev/	Device drivers for consoles and boot storage
	cfe/hosttools/  Tools built on the host
	cfe/include/	Common include files
	cfe/lib/	Common library routines
	cfe/main/	Main program
	cfe/net/	Network subsystem (IP, DHCP, TFTP, etc.)
	cfe/pccons/	PC console routines
	cfe/pci/	PCI and LDT configuration
	cfe/ui/		User interface
	cfe/usb/     CFE USB stack
	cfe/vendor/     Vendor extensions to CFE
	cfe/verif/      Stuff for running chip verification tests
	cfe/x86emu/	X86 emulator for VGA initialization
	cfe/zlib/     General purpose data compression library


Building CFE
------------

To build the firmware for the BCM947xx evaluation boards. 
for example:

	cd bcm947xx ; gmake all ; cd ..

This will produce:

	bcm947xx/cfe     		ELF file
	bcm947xx/cfe.bin 		Executable file
	bcm947xx/cfe.srec 	Motorola S-records


------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (1.0.35)
------------------------------------------------------------------------------

  * The license has been modified to be "GPL compatible."  This
    means that you should be able to incorporate parts of CFE
    into a GPL'd program without license hassles.  

  * The Hypertransport initialization code in CFE now supports both
    the LDT 0.17 and the HyperTransport 1.03 styles of fabric
    initialization.  There is a new flag, ldt_rev_017, that can be
    included in the value of the PCI_OPTIONS environment variable.  If
    that flag is set, the 0.17 initialization procedure will be used.
    Otherwise, the 1.03 procedure is followed.  There is also a new
    configuration option, CFG_LDT_REV_017, that sets the default value
    of the ldt_rev_017 attribute.  This option is specified in the
    Makefile.

    You must choose the initialization procedure properly, either by
    default or by making a permanent assignment to PCI_OPTIONS, to
    match the type of devices present on your HyperTransport chain.
    If the choice is incorrect, the system can fail to initialize the
    Hypertransport links and may hang at startup while attempting to
    do so.

    If set to '1', fabric initialization will be appropriate for
    the HyperTransport 0.17 specification.  The SP1011 HT->PCI bridge
    can only operate in 0.17 mode, and standard Makefiles for boards
    with SP1011s make this the default.

    If set to '0', fabric initalization will be approprate for the 1.01
    (and later) specification.  The PLX 7520 HT->PCI-X bridge can only
    operate in this mode.  Standard Makefiles for boards with only HT
    expansion connectors make this the default.

    Either setting is acceptable when communicating with another
    BCM1250 in a double-hosted chain, but both ends must agree.

  * CFE now supports initialization of HyperTransport fabrics that
    include the PLX 7520 HT->PCI-X chip.  For link speeds other than
    200 MHz, the 1.03 initialization option (above) must be used.

  * The BCM1250CPCI port had incorrectly programmed the GPIO interrupt
    mask register, preventing Linux from being able to use the on-board
    IDE interface.  This has been fixed.

  * Some 64-bit/32-bit issues have been fixed in the "flashop engine"
    that is used for programming flash devices.  In particular, 64-bit
    ops are used when manipulating addresses in 64-bit mode.

  * The default values for the drive strengths and skews for the
    DRAM controllers have been modified to be more reasonable for
    currently shipping parts.    

  * The workarounds for known bugs in the BCM1125's memory controller
    have been made a run-time check.

  * The command line parser has been substantially rewritten to be more
    shell-like in its expansion of environment variables.  In particular
    you can now set an environment variable to include multiple CFE
    commands.  

  * The command line parser supports "aliases" - if the first word of
    a command line matches an environment variable, it will be expanded
    even if there is no preceding dollar sign.  For example, you can
    now do:

	CFE> setenv start "ifconfig eth0 -auto; boot -elf server:myprogram"

    and later just type "start" to start the program.  

    PLEASE NOTE: If you define an alias that has the same name as a built-in
    command, you will need to quote the command to prevent the expansion
    from occuring:

	CFE> setenv e "ifconfig eth0 -auto"
	CFE> e			(this will run the "ifconfig" command)
	CFE> 'e'                (this will run the "edit memory" command)

  * By enabling an option, CFG_URLS in your bsp_config.h file, CFE can
    now process file names in URL syntax.  This option defaults to OFF 
    to use the current syntax.  When enabled, you can use boot files
    in the following format:

	CFE> boot -elf tftp://servername/path/to/filename
	CFE> boot -elf fat://ide0.0/path/to/filename
	CFE> boot -elf rawfs://flash0.os

    This syntax works with the "boot", "load", and "flash" commands.

  * You can now boot from an HTTP server by enabling "CFG_HTTPFS" in
    your bsp_config.h file.  This also requires TCP support (define CFG_TCP).

	CFE> boot -elf -http servername:path/to/binary		(old syntax)
	CFE> boot -elf http://servername/path/to/binary		(URL syntax)


------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (1.0.34)
------------------------------------------------------------------------------

  * CFE now builds using the "sb1-elf" toolchain by default.  
    It will still build using mips64-sb1sim, but it is recommended
    that you switch to the new toolchain.

  * The BCM1125E port now uses the SPD EEPROM to store memory
    parameters.  If the EEPROM is not programmed, a default memory
    table will be used.

  * For versions of the 1250 and 1125 that are step A0 (anything but
    pass1) or newer,  PCI code now configures HyperTransport 
    interrupts for level triggered mode.  For older versions, 
    configuration is for edge triggered mode as before.

  * If CFE is used to load an operating system such as Linux that
    relies on CFE's configuration of interrupts, be sure to use only
    versions of that operating system that can deal with level
    triggered HyperTransport interrupts (must issue EOIs).

  * Exceptions now display the CAUSE field symbolically, displaying
    the exception name along with the CAUSE register value.

  * A number of fixes have been made to the SB1250 include files,
    including the addition of BCM1125 DMA features and some additional
    constants for the drive strength registers.
 
  * Some additional test code has been placed in the firmware's
    ethernet driver for testing FIFO mode.

  * The memory initialization routines have had an off-by-one 
    error corrected that can cause the memory to be run slower
    than the rated maximum.

  * The memory initialization routines will not operate properly
    on the BCM1125 without recompiling (the #ifdef _SB11XX_ is gone)

  * The memory initialization routines were losing precision
    in the calculation of tCpuClk, so the calculation has been
    adjusted to avoid the precision loss.

  * The #ifdef _SB11XX_ in the l2 cache routines is no longer
    necessary.

  * The ethernet driver and the PHY commands now preserve the state
    of the GENC pin while doing MII commands.

  * The command line recall should work more like you expect - if you
    recall the most recent command, it is NOT added to the history,
    and if you recall any previous command, it IS added.

  * Some fixes have been made to the USB ethernet drivers.

  * The flash driver now handles some broken AMD flash parts
    better (some AMD parts have reversed sector tables,
    that need to be sorted in the other order before they
    can be used).



------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (1.0.32)
------------------------------------------------------------------------------

  * Support has been included for the BCM1125 and the errata
    that is relevant for the firmware.  In particular, there are
    workarounds for a couple of memory controller issues that affect
    the BCM112x (and only the 112x, this does not affect the BCM1250).

  * The system include files have been updated to include constants and
    macros that are useful for BCM1125 users, including support for the
    new Ethernet DMA features, data mover features, etc.

  * The BCM1250CPCI port now enables memory ECC by default.

  * Autoboot support.  CFE now has a new feature to allow a board package
    to supply a list of boot devices to try.  For example, you can now
    say "first try PCMCIA, then the IDE disk, then the flash device,
    then the network."  You configure the autoboot list in your
    board_devs.c file by calling "cfe_add_autoboot" one or more times
    (see swarm_devs.c for an example).  

    You can then either use the "autoboot" UI command (put it in your
    STARTUP environment) or call cfe_autoboot() at the end of
    board_final_init() to enable automatic bootstrap. 

    Additional documentation will be placed in the manual for this, but
    is not there at this release.

  * setjmp has been renamed "lib_setjmp" to prevent future versions 
    of GCC from doing things that assume how setjmp work.

  * The include files have been modified to allow you to select which
    chip features will be present.  This can help you avoid using 
    features that your chip does not have, such as using pass2 features
    on a pass1 chip, etc.  See the comments in include/sb1250_defs.h
    for an explanation of the possible selections.  The default is
    to include all constants for all chips.

  * The ethernet driver now preserves the value of the 'genc' bit
    on the MDIO pins.  Previously it would always set this bit to zero.

  * The DRAM init routines (and some other places) let you specify the
    reference clock in hertz via SB1250_REFCLK_HZ.  The reference clock
    affects timing calculations, so if your reference clock is very
    nonstandard, you should change this.

  * The DRAM init routines include support for BCM112x processors.

  * The DRAM init routine will now automatically disable CS interleaving
    on large memory systems that use more thena 1GB of memory per
    chip-select.

  * The ARP implementation was sending incorrect response messages to
    inbound ARP requests.  This fix should easily apply to
    older versions of CFE.  (see net/net_arp.c)

  * The DHCP implementation makes more information available after
    configuring from a DHCP server.  

    Parameter #130 is a CFE extension that will be placed in the "BOOT_SCRIPT"
    environment variable.

    Parameter #133 is a CFE extension that will be placed in the "BOOT_OPTIONS"
    environment variable.

    You can use these variables any way you like.  For example, you could
    set up an autoboot to read a batch file whose name is specified in
    the DHCP server's configuration file.  Individual hosts can then
    have their own private scripts.

  * PCI/LDT configuration has had numerous changes; among them the removal
    of many device and vendor codes from the text database.  This 
    reduces CFE's size significantly, but you won't get a pretty
    message when you install your 8-year-old obscure PCI mouse accelerator
    anymore.


------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (1.0.30)
------------------------------------------------------------------------------

  * The memory initialization routines have been improved.  The
    r2wIdle_twocycles bit is now always set to work around a silicon
    bug, and two bits in the include files (sb1250_mc.h) were
    reversed.  The new draminit module does a better job at 
    calculating timing and will take into account a new global
    parameter (tROUNDTRIP) which represents the total round trip
    time in nanoseconds from the pins on the 1250 out to the 
    memory and back.  Using this new module is HIGHLY RECOMMENDED.

  * The 'memconfig' program has been improved - it uses the actual
    copy of draminit and can be used to calculate memory
    parameters outside of CFE to be sure they make sense.
    Use "memconfig -i" for interactive mode.

  * The UART driver for the console has been changed to include a fix
    for errata 1956, which can cause the baud rate register to be
    written with bad data, particularly at high "odd" (ending in 50,
    like 550, 650, 750) speeds.

  * The flash driver has been rewritten.  The new driver is called
    "newflash" and the implementation is in dev/dev_newflash.c.
    The new flash driver is smaller and simpler than the old one, and
    supports some new features, including:

       - Partitioning - you can instantiate sub-flash devices by breaking
	 a large flash part into pieces, which will have names such
	 as "flash0.boot", "flash0.os", "flash0.nvram", etc.

       - Support for using flash as NVRAM is much improved.  You use
 	 the above partitioning scheme to create a partition for the
 	 NVRAM and attach the NVRAM subsystem to that.  There is no
	 "hidden" reserved sectors anymore.

       - New flash algorithm code - all of the flash operations are
 	 in a single assembly file, dev_flashop_engine.S, which is 
	 relocated to RAM when CFE is running from the flash.  

       - The probe code has changed.  You will need to specify the 
	 bus width (usually 8 bits for BCM1250 parts) and the flash 
 	 part width (16-bits or 8-bits).  You would specify 16 bit flash
	 widths if your 16-bit part is in "8-bit mode.".

    The old flash driver (dev_flash.c) is still there for use by older
    firmware ports.

  * The memory enum API has been improved to let you enumerate all
    memory blocks (not just available DRAM).  You can add your own
    regions to the arena during initialization and query them from
    applications.

  * There is a new device driver for the National Semiconductor
    DP83815 PCI Ethernet chipsets.  It is getting hard to find
    Intel/Digital DC21143 cards, this is the replacement.

  * The concept of "full names" and "boot names" has been removed.
    All device drivers now have only one name.

  * Support for the RHONE board is now included in the source 
    distribution and binaries are on the web site.  The RHONE is
    a SENTOSA-like board that includes a BCM1125 processor.

  * Support for the BCM1250CPCI board is now included in the source 
    distribution and binaries are on the web site.  The BCM1250CPCI is
    a SWARM-like board that fits into a CompactPCI chassis.

  * Routines to display information about the CPU type have been added
    to the common BCM1250 sources.  Most board packages now call this 
    during initialization to print out CPU revision information.

  * A number of the commands that used to start "test" such as 
    "test flash" have been either removed or relocated to 
    other files (most of these things lived in "cfe_tests.c").
    These were old test routines used to check out internal 
    CFE features are are no longer needed.

  * The "memorytest" command (in 64-bit firmware versions) has been
    enhanced slightly to let you specify the cacheability attribute.
    -cca=5 will do cached accesses, which promotes lots of evict
    activity.  

  * The include files have been updated in a few places to reflect
    the user's manual.

  * A USB host stack is now available in the firmware.  The host stack
    works with OHCI-compatible host controllers and supports USB hubs,
    keyboards, some mass storage devices, a serial port device, and
    a couple of Ethernet controllers.  It's there mostly for
    your amusement, we don't really support this actively.

  * The VGA Console system has been improved to include support for the
    USB keyboard.  It is now possible to build a version of CFE that
    will display on a VGA and take its input from a USB keyboard
    (no serial port!).  Linux can be compiled to "take over"
    the VGA and keyboard, so you can almost have that "PC" experience
    you've always wanted with your SWARM.  (or not.)   Like the USB
    support, it's for your amusement, we don't support this actively.


------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (1.0.29)
------------------------------------------------------------------------------

  * A missing routine in init_ram.S has been added.  This is required
    for the CFG_RAMAPP version (a RAM version of CFE you can load from a
    TFTP server).

  * A simple cache error handler has been added to the firmware.

  * There is a new defined, _SERIAL_PORT_LEDS_ that you can put in the
    swarm_init.S file (or copy into your own board's init routines) to 
    send the LED messages to the serial port.

------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (1.0.27)
------------------------------------------------------------------------------

  * A simple TCP stack has been added to the firmware.  If you define
    CFG_TCP=1, this stack will be added to the firmware build.  
    Note that this stack is unfinished, largely untested,
    and is not meant to provide high performance.  As of this version,
    several important TCP features are not implemented, including
    slow-start, fast-retransmit, round-trip-delay calculation,
    out-of-band data, etc.  It works well enough for simple interactive
    applications (a miniature rlogin client is included) and may be
    sufficient for a simple httpd.  This stack will be improved over
    time; it is meant for amusement purposes only at this time.

  * Support has been added for the ST Micro clock chips present
    on newer SWARM (rev3) boards.  This chip replaces the Xicor X1241
    on previous versions.  The environment variables are now
    stored in a second Microchip 24LC128 on SMBus1.

  * A new bsp_config.h option, CFG_UNIPROCESSOR_CPU0, has been
    added.  If this is set to '1' and CFG_MULTI_CPUS=0 (uniprocessor mode)
    then the firmware will switch the CPU into "uniprocessor CPU0"
    mode, making it look more like an 1125 (it will actually report
    itself as an 1150, since 512KB of L2 cache are available).

  * Moved the exception vectors into RAM.  For relocatable versions
    of CFE, the exception vectors are now in RAM.  Previously, 
    exceptions were handled by the ROM version whether or not the
    firmware was relocated, causing all sorts of problems, including 
    running the wrong code after an exception.  As a result, CFE is much
    more robust now at handling exceptions when the GP register 
    has been trashed, since it can recover the GP value from a low-memory
    vector.  The PromICE (BOOTRAM) version does not use RAM exception 
    vectors.  This change also paves the way for building versions
    of CFE that include a real interrupt handler, should that be required.

  * The exception register dump now includes the register names as
    well as the numbers.

  * Support has been added to the Ethernet driver for the quirks of the
    BCM5421 PHY (the quirks only affect A0 silicon).

  * If you run the multiprocessor version of CFE on a single-processor
    chip (for example, a 1250 that has been restarted in uniprocessor mode),
    it will not hang when starting the secondary core.  Error codes will
    be returned to applications that attempt to start the secondary
    cores.

  * The cache operations now include routines for invalidating  
    or flushing ranges.  This is of limited use on the 1250.

  * A simple cache error handler has been added.  It will display 
    'Cerr' on the LEDs.

  * The DRAM init routines now have better support for large memory
    systems, using an external decoder on the chip select lines.

  * Support has been added to the flash driver for 16-bit Intel-style
    flash parts, and "burst mode" on the pass2 generic bus interface.

  * The X86 emulator in the VGA init code has been enhanced to include
    some previously unimplemented instructions.


------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (1.0.26)
------------------------------------------------------------------------------

  * The memory initialization code has been dramatically improved.  
    It now supports calculating all of the timing parameters using
    the CPU speed and the timing data in the SPD ROMs on the DIMMs.
    You can also specify this information from the datasheets for
    systems with soldered-down memory.

    Better support has been added for non-JEDEC memory types
    such as FCRAMs and SGRAMs, and this information is also stored
    in the "initialization table."

    Systems with more than 1GB of physical memory are now supported 
    correctly by the dram init routines.  The MC_CS_START/END registers
    were being programmed incorrectly.  A limitation of the current
    design is that memory DIMMs must not be broken across the 1GB
    line (for example, in MSB-CS mode, do not install a 512M DIMM,
    a 128M DIMM, and a 512M DIMM). 

    NOTE: TO MAKE THIS CHANGE, COMPATIBILITY WITH OLD DRAM INIT
    TABLES HAS BEEN BROKEN!

    You should re-read the CFE manual for information about the new
    DRAM init table format, which now contains multiple record types
    to describe memory channels, chip select information, timing
    information, and geometries.   As more features are added to
    the memory init routine in the future, new record types can be
    added.

  * An bug in the L2 cache flush routine has been fixed
    that might cause some boards to hang at the "L12F"
    display on the LEDs.  The bug can occur depending on
    the previous contents of the cache (presumably garbage).
    In rare circumstances, an ALU overflow can occur because
    of an incorrect 'dadd' instruction that should have been 'daddu'    

  * All of the BSPs have been updated to support the new memory init
    routine.  This involves changes to the bsp_config.h files and
    the various board_init.S files.

  * The memory initialization routine now returns the memory size
    in megabytes, not bytes.  This prevents overflows on large memory
    systems using 32-bit firmware.

  * An L2 cache diagnostic is now included.  It is only run for
    pass2 parts, since the tag format has changed.  If a quadrant
    of the cache fails the diagnostic, it will be disabled.

  * Support for downloading binaries to SENTOSA boards over the PCI
    bus is now included.   Enable the CFG_DOWNLOAD parameter 
    in the makefile to add the required object files and 
    source.  See the manual for more information about how to
    use this feature.

  * A better (but still not ideal) memory diagnostic is included 
    in the 64-bit firmware.  The "memorytest" command will 
    test all memory not used by CFE.

  * The Ethernet interface will be reset when an OS like Linux
    exits.  In previous versions Linux would not probe
    the Ethernet if the kernel was restarted, since the
    Ethernet address register was not reset.

  * Numerous improvements have been made to the LDT configuration
    routines.

  * The real-time clock driver for the ST Micro part used on the
    SENTOSA boards is now included.

  * It is possible to build a variant of CFE that runs in DRAM
    (to be loaded like an application program) by setting the
    configuration parameter "CFG_RAMAPP=1" in your Makefile.
    When this option is used, the resulting CFE binary
    skips the CPU and DRAM initialization, but continues to install
    its device drivers.  You can use CFE like a big runtime 
    library or as a framework for diagnostics or other small apps.

------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (1.0.25)
------------------------------------------------------------------------------

  * More improvements to the directory structure, moving architecture-specific
    code and macros into appropriate subdirectories.

  * The new "Sentosa" evaluation board is now a supported target.

  * More macros to customize 32 vs 64-bit implementations  For example,
    certain CPU-specific things like ERET and hazard avoidance have
    been moved to each CPU's cpu_config.h file.

  * For the SWARM target, you can now build a "bi-endian"
    firmware image.  This actually contains two different
    versions of CFE with some magic code at the exception
    vectors to transfer control to the appropriate version
    depending on system endianness.  See the 'biend' target 
    in swarm/Makefile, arch/mips/common/include/mipsmacros.h,
    arch/mips/common/src/init_mips.S, and the documentation.

  * The "filesystem" calls have been moved into cfe_filesys.c and are 
    now functions, not macros.  There is a 'hook' facility to allow
    you to intercept I/O calls to do preprocessing on data received
    from the filesys.

  * A 'hook' has been created for zlib (compressed file support).  If you
    compile with CFG_ZLIB=1 in your Makefile, CFE will support the
    "-z" switch on the 'boot' and 'load' commands.  This can be used
    to load compressed binaries, elf files, and s-records.

  * A new 'build' directory has been created.  This directory and
    its subdirectories will contain the makefiles and object file areas
    for supported builds.  You can create your own directories in there
    for your ports, and this should ease merge headaches.

  * The 'dump', 'edit' and 'disassemble' commands now gracefully
    trap exceptions that occur when you mistype addresses.  CFE will no
    longer reboot in these cases.  Over time we'll use this feature
    to catch more exceptions.

  * The "-p" (physical) switch for the dump, edit, and disassemble 
    commands now makes uncached references to the addresses you specify.
    It used to make cached references.

  * Non-relocatable builds work again.  There was a bug in one of the
    macros used in init_mips.S

  * A new include file, sb1250_draminit.h, has been created to hold
    structures and constants for the DRAM init code.  For example,
    the DRAMINFO macro, which was duplicated in all the board packages,
    has been moved here.

  * The DRAM init code now uses the upper bit (bit 7) of the SMBus
    device address to indicate which SMBus channel the SPDs are on.
    this way you can use both SMBus channels for SPD ROMs.  There
    are macros in sb1250_draminit.h to take apart the SMBus
    device ID into the channel # and device.

  * The DRAM init code now supports chipselect interleaving
    by setting the CFG_DRAM_CSINTERLEAVE value in bsp_config.h
    You set this value to the number of bits of chip selects
    you want to interleave (0=none, 1=CS0/CS1, 2=CS0/1/2/3)

  * The flash device has been substantially rewritten.  It now supports
    Intel-style flash, 16-bit devices (not particularly useful on
    the 1250), and "manual" sectoring for JEDEC flash devices that
    do not have CFI support.  Usage of the flash for NVRAM (environment)
    storage has been substantially improved and tested.

  * There's a #define in the SWARM init module (swarm_init.S) to cause
    the LED messages to go to the serial port.  #define _SERIAL_PORT_LEDS_
    if you want to use it.

  * The environment storage format has changed slightly, but 
    in a backward-compatible way.  It is now possible to store
    TLVs whose data portions is more than 255 bytes, and a 
    portion of the TLV code range has been reserved for 
    customer use. 

  * The VGA initialization code is once again alive on the SWARM board.
    (In the processs, it got broken for the P5064.  Oh, well.)  There's
    still no USB keyboard support, but adventurous souls that want to
    put text out on a VGA display can now do that, at least for some
    of the cards we've tried.

  * The real-time-clock commands were not properly setting the clock
    to "military time" mode, affecting the notion of AM and PM.

  * The real-time-clock code has been divided into two pieces,
    the user-interface and a standard CFE driver.  This is to
    make things easier when supporting different RTC chips.

  * Calls through the "init table" in init_mips.S that must take place
    after relocation should use the new CALLINIT_RELOC macro.  this
    macro makes use of the text relocation amount (mem_textreloc)
    and the GP register, so care must be taken when using it.  If 
    you don't use this macro, however, you can end up running the code
    non-relocated even though you've gone and moved it!  Basically,
    pointers stored in the text segment (like the init table) are
    not fixed up, so fixups must be applied manually.

  * The installboot program in the ./hosttools directory has been
    improved to be actually useful for installing bootstraps on
    disks.  You can compile this program under Linux and use it to
    put a boot sector and boot program on an IDE disk, then boot
    from that disk via CFE.

  * The FAT filesystem code should now properly detect whether the
    underlying disk has or doesn't have a partition table.  This
    can be a problem with CF flash cards - they are 
    formatted at the factory with partition tables, but if
    reformatted under Windows 2000, the partition table will be 
    eliminated (more like a floppy).

  * The FAT filesystem code should now correctly find files stored
    in subdirectories (below the root)


------------------------------------------------------------------------------
SPECIAL NOTE FOR RELEASES STARTING WITH 1.0.25
------------------------------------------------------------------------------

  * The directory structure of the CFE firmware has changed
    substantially starting with 1.0.25.  You will notice many 
    changes, including:

	- Board, CPU, and architecture-specific files are now
	  in their own directories.

	- The makefile is distributed among several subdirectories

	- Wherever possible, code has been made more generic.

  	- Some user-interface code has been changed.

    These changes are in anticipation of the use of CFE on
    future Broadcom processors and reference designs.

    Should you wish, you can also make use of this new organization
    to port CFE to other MIPS designs, and with some difficulty
    to non-MIPS designs as well.  

  * The build procedure has been modified to allow you to
    build the object files into an arbitrary directory
    that is not related to the source directories.

  * The release that contains this reorganization is now 
    called "1.0.xx"

  * The documentation (docs/cfe.pdf) contains important information
    related to the change in the directory structure.

  * The code reorganization that goes with this change is not
    fully complete, so expect some additional changes in the
    future, including:

	- A way to conditionally remove all the debug/bringup
	  code specific to the BCM1250

	- New include files to abstract certain aspects of device
	  I/O, especially for PCI devices

	- A potential change in the default location that the
	  default CFE builds take place.  They are currently in
	  their familiar locations to make it easier for
 	  you (customers) to incorporate your code.

   * Please let me know if this new directory organization
     has caused major headaches.

   * The license text has been updated.


------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (0.0.23)
------------------------------------------------------------------------------

    * The directory layout has been substantially modified
      (see above)

    * The user interface for the memory-related commands,
      (dump, edit, disassemble) have been improved.  They
      remember the previous address and length and now
      have simple switches (-p, -v) to deal with physical
      and virtual (kernel or useg) addresses.

    * The PCI configuration option has moved from bsp_config.h
      to the Makefile

    * Some initialization of UI modules have been moved
      out of cfe_main.c and into board-specific startup 
      files.

    * The Algorithmics P5064 port has been resurrected to
      verify the build procedure for other designs.

    * There are new commands to deal with the SWARM board's
      Xicor X1241 real time clock.  See "show time, set time, set date"

------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (0.0.21)
------------------------------------------------------------------------------

    * The memory initialization module (sb1250_draminit.c)
      had a bug where it was not setting the START/END
      registers properly when using double-sided DIMMs.
      This is the most important fix in this release.

    * The cfe_ioctl() internal routine now takes the 'offset'
      parameter for the iocb.  This is used by programs and
      extensions inside the firmware.

    * The CMD PCI0648 chip has been added to the list of
      PCI devices probed by the IDE routines.

    * A "vendor commands" file has been added to the
      vendor/ directory.  Some other changes have been 
      made to "un-static" variables useful to vendor
      extensions.


    
------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (0.0.19, 0.0.20)
------------------------------------------------------------------------------

    * The "C" memory initialization code introduced in 0.0.19
      has been improved to calculate refresh timing based on
      CPU speed.  In the future, additional timing parameters will
      be automatically calculated, particularly for DIMMS.
      If you have special values for the clock config register you
      can now specify those in your bsp_config.h file.

    * The PCI subsystem now reads an environment variable PCI_OPTIONS
      which contains a comma-separated list of flags to control
      PCI startup.  You can prefix an option name with "no" to
      turn off an option.  Currently the following flags are defined:

	verbose     	Be very verbose while probing
	ldt_prefetch	Turn on prefetching from the Sturgeon bridges
 
      The default is to enable ldt_prefetch. 

      Some SWARM boards appear to have a problem related to
      prefetch across the HyperTransport (LDT) bus.  If you experience
      DMA failures with devices connected to the PCI slots behind
      the Sturgeon bridge, you may wish to disable prefetching
      to work around this issue.   To do this, set the
      PCI_OPTIONS environment variable as follows:

	CFE> setenv -p PCI_OPTIONS "noldt_prefetch" 

 	(reboot for changes to take effect)

      Board support packages should be modified to add the NVRAM
      device and call cfe_set_envdevice() in the board_console_init()
      routine instead of board_device_init(), since the PCI
      init code is done before device initialization.


    * A number of improvements have been made to the Tulip (Intel 21143)
      device driver.

    * The SB1250 include files have had some minor fixes.  In particular,
      the correct values are now used for the interframe gap in the MAC.

    * The SB1250 Ethernet driver was erroneously setting the M_MAC_FC_SEL
      bit in all full-duplex modes.  M_MAC_FC_SEL is only supposed to be
      used to force pause frames.

    * The SWARM board package supports Rev2 SWARM boards, which
      have all 4 configuration bits on the configuration switch (SW2)

    * A better memory test is now part of the 64-bit CFE,
      see the "memorytest" command.

    * The 'flash2.m4' file was missing from the 0.0.19 distribution.


------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (0.0.17, 0.0.18)
------------------------------------------------------------------------------

    * The memory initialization code has been rewritten in "C".
      It should be much easier to read and more flexible now,
      supporting more memory configurations.  In particular,
      if you have CFG_DRAM_INTERLEAVE set, CFE will automatically
      use port interleaving if the DIMMs on adjacent channels
      are of the same type and geometry.

    * There is a new 'save' command that invokes a TFTP client
      for writing regions of memory to a TFTP server.  
      You should ensure that your TFTP server is capable of
      write access before using this feature.

    * The API functions for the cache flush routines have been
      filled in, so the bitmask in the iocb.iocb_flags field
      for determining which type of cache flush to do should
      behave as the documentation indicates.

    * For customers wishing to extend CFE's APIs, a new 
      directory "vendor/" has been added to the tree.  
      If CFG_VENDOR_EXTENSIONS is defined, IOCBs above
      the value CFE_FW_CMD_VENDOR_USE will be redirected to
      a vendor-specific dispatch area.  You can implement 
      custom IOCB functions there.

    * On the C3 platform there is some code to send the
      4-character LED messages to the serial port.  If your
      design does not include a 4-character LED, you can 
      incorporate this into your code to see the LED messages.

    * Many improvements have been made to the Tulip (DC21143) driver.

    * The disassembler has been modified to create far fewer
      initialized pointers, increasing the amount of space
      available in the GP area.

    * The value in the A1 register for the primary processor
      startup is now zero.  Eventually you'll be able to pass
      a parameter here like you can to CPU1.

    * The "-addr" and "-max" switches to the "load -raw" 
      command now work.

------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (0.0.16)
------------------------------------------------------------------------------

    * The LDT initialization code has been improved.  LDT 
      operation should be much more reliable on SWARM
      boards.
    * If you set the configuration switch to zero, CFE was
      still trying to access PCI space on SWARM boards.
      This has been fixed.

------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS VERSION (0.0.15)
------------------------------------------------------------------------------

BUGS FIXED

    * If you don't initialize the PCI bus, CFE was still trying to
      read configuration space to configure PCI devices.  This has been
      fixed.

------------------------------------------------------------------------------
CHANGES SINCE PREVIOUS MAJOR VERSION (0.0.10)
------------------------------------------------------------------------------

BUGS FIXED

    * The DRAM CAS latency now defaults to 2.5 for 500MHz 
      operation.
    * To work around a PASS1 bug (see the errata) some portions
      of the multi-CPU startup have been changed.  See the
      stuff in init_mips.S in the _SB1250_PASS1_WORKAROUNDS_ 
      blocks.

NEW FEATURES

    * The build procedure now uses 'mkflashimage' to build
      a header on the front of a flash image.  This is 
      used to prevent you from flashing an invalid file
      over a running CFE.
    * Many improvements in the PCI/LDT configuration code.
    * The DC21143 (Tulip) driver has been improved.
    * The IDE driver has been revamped to perform a little
      better and work in either endianness without 
      unnecessary byte swapping.  
    * The PCMCIA driver has been updated to support the SWARM
    * A new switch "-p" is required on SETENV to set the
      environment in the NVRAM.  It now defaults to storing
      in RAM only unless this switch is supplied.
    * The environment variable "STARTUP" can be used to run
      some commands at boot time.
    * You can put multiple commands on a command line by
      separating them with semicolons
    * The configuration switch can now configure some
      aspects of CFE at run time.  See the manual.

KNOWN PROBLEMS

    * The flash update erases the entire device, not just the 
      sectors it needs.
    * IDE disks work on real hardware, but ATAPI devices don't.
      The Generic Bus timing registers probably need to be tweaked.


------------------------------------------------------------------------------
RUNNING CFE UNDER THE FUNCTIONAL SIMULATOR
------------------------------------------------------------------------------

One goal for the functional simulator is for it to more-or-less completely
emulate the functionality and peripherals available on the "SWARM"
BCM12500 checkout board.  

The configuration files in the swarm/ directory contain the current
port of CFE to the evaluation board.

Heed this warning:

NOTE: CFE and the functional simulator are often out of sync, so be sure
      to read these release notes for information on running CFE on
      older versions of the simulator.

* Compiling CFE for use under the functional simulator

  Because the functional simulator is significantly slower than real
  hardware (on a 900MHz PC it operates at the equivalent of 500Khz)
  it is important to define the following symbols before compiling
  the firmware (place this in the Makefile, see the comments there):

	CFLAGS += -D_FUNCSIM_ -D_FASTEMUL_

  You can also build the simulator in the 'sim' target directory
  where these symbols are already defined.

  These two preprocessor symbols _FUNCSIM_ and _FASTEMUL_ change the
  timing loops to be appropriate for the slow processor and eliminate
  certain parts of the cache initialization , since the simulated caches
  start in an initialized state.  

  Don't forget to remove this before running on real hardware!

* Running CFE under the functional simulator.

  To run the simulator using the new sb1250-run script (part of the
  1.9.1 and later toolchains), you can do:

	#!/bin/sh
	sb1250-run \
                --with-boot-flash-file cfe.srec \
                --with-boot-flash-type ram \
                --no-file \
                --with-swarm-devs \
		--with-sample-pci-devs \
                --with-swarm-ide \
                --with-swarm-ide-disk0-file disk0.dsk \
                --with-swarm-ide-disk0-size 60 \
                --with-memory-config 2x32 \
                --with-swarm-rtc-eeprom-file x1240rom.bin 

  In this configuration, CFE will have "null" back-ends for the Ethernet
  and use standard I/O for the console.  It is a convenient way
  to verify that CFE is operational, but it does not provide a good
  way to load programs.  See the next section for configuring
  network operation.

  PCI and LDT configuration are supported under the 1.8.12 and newer
  toolchains.

* Simulated hardware description:

This command and the devices file will configure the simulator as follows:

   * 64MB of memory in one DIMM, as two 32MB DIMMs
   * Two Serial Presence Detect modules at SMBus Channel 0 address 0x54
   * A Xicor X1240 clock/eeprom module on SMBus Channel 1 (device "eeprom0")
   * A four-character LED at 0x100A0000
   * A 60MB IDE disk at 0x100B0000 (device "ide0")
   * 4MB of flash at 0x1FC00000 (device "flash0")
   * Two UARTs on the BCM12500 (devices "uart0", "uart1")
   * Three Ethernet devices (devices "eth0", "eth1", and "eth2")

   * The Xicor's EEPROM contents will be stored in a local file 
     called "x1240rom.bin"
   * The IDE disk's contents will be stored in the local file "disk0.dsk"
   * The flash at 0x1FC00000 will read the file "cfe.srec" when the 
     simulator starts.
   * The flash at 0x1FC00000 is set to behave like SRAM.  You can use this
     area to load other programs into the boot ROM and set breakpoints when 
     running under GDB.  If you want the flash to behave like real flash, 
     edit sb1250-run command above and change the flash-type to "flash"


If successful, you should see the following when you start
the simulator:

----------------------------------------------------------------------

[mpl@swarm test]# ./runcfe
Disk 0: 60MB, 122 Cyl 16 Head 63 Sect: File disk1.dsk
Loading S-Record file cfe.srec (offset 0x00000000)
Finished loading file.
sim: cpu model mips:sb-1, word size 64, addr size 64, big endian

CFE version 0.0.10 for CSWARM-SIM (32bit,MP,BE)
Build Date: Wed Jun 20 07:02:57 PDT 2001 (mpl@swarm.sibyte.com)
Copyright (C) 2000,2001 Broadcom Corporation.  All Rights Reserved.

Initializing Arena.
Initializing Devices.
SysCfg: 0000000000480500 [PLL_DIV: 10, IOB0_DIV: CPUCLK/4, IOB1_DIV: CPUCLK/3]
Config switch: 0
CPU type 0x1040101: 500KHz
Total memory: 0x4000000 bytes (64MB)

Total memory used by CFE:  0x81E00000 - 0x81F06290 (1073808)
Initialized Data:          0x81E00000 - 0x81E04020 (16416)
BSS Area:                  0x81E04020 - 0x81E04290 (624)
Local Heap:                0x81E04290 - 0x81F04290 (1048576)
Stack Area:                0x81F04290 - 0x81F06290 (8192)
Text (code) segment:       0x9FC00000 - 0x9FC26720 (157472)
Boot area (physical):      0x01F07000 - 0x01F47000
Relocation Factor:         I:00000000 - D:00000000

CFE> 


----------------------------------------------------------------------

------------------------------------------------------------------------------
NETWORK BOOTSTRAP
------------------------------------------------------------------------------

CFE includes an Ethernet driver, so you should be able to use
the "simether-live" program to access the live network.  The program
"simether-live" feeds packets from your real network into the simulated
Ethernet devices of the functional simulator.  To use this, you will need
to activate the simulator's "backends."  Invoke the simulator with the
following command:

	#!/bin/sh
	sb1250-run \
                --with-boot-flash-file cfe.srec \
                --with-boot-flash-type ram \
                --no-file \
                --with-swarm-devs \
		--with-sample-pci-devs \
                --with-swarm-ide \
		--with-sockets-for-io \
		--stream-socket-base-addr "0.0.0.0:10100" \
		--sim-wait-after-init \
                --with-swarm-ide-disk0-file disk0.dsk \
                --with-swarm-ide-disk0-size 60 \
                --with-memory-config 2x32 \
                --with-swarm-rtc-eeprom-file x1240rom.bin 


The simulator wll start up as follows:

----------------------------------------------------------------------
/sb1250sio/backend_a: listening on TCP socket 0.0.0.0:10100
/sb1250sio/backend_b: listening on TCP socket 0.0.0.0:10101
/sb1250jtag/backend: listening on TCP socket 0.0.0.0:10102
/sb1250eth@0x10064000/backend: listening on TCP socket 0.0.0.0:10103
/sb1250eth@0x10065000/backend: listening on TCP socket 0.0.0.0:10104
/sb1250eth@0x10066000/backend: listening on TCP socket 0.0.0.0:10105
Disk 0: 60MB, 122 Cyl 16 Head 63 Sect: File disk0.dsk
Loading S-Record file cfe.srec (offset 0x00000000)
Finished loading file.
sim: cpu model mips:sb-1, word size 64, addr size 64, big endian
sb1-elf-run: initialization complete.
sb1-elf-run: hit return to run...
----------------------------------------------------------------------

The messages from the simulator in the form 

	/device/backend: listening on TCP socket 0.0.0.0:xxxxx

let you know how to connect external programs to the simulator.

In this case, the console will be connected to port 10100, since
it is the serial port's "backend_a" device.  Start another window
and run the "conn" program to connect to the serial port
as follows:

	$ conn localhost 10100

The Ethernets will also be available as TCP sockets.  In this case,
MAC 0 (ast SB1250 physical address 10064000) has a socket at
10103.  To connect to this socket, create another shell window 
and "su" to super-user mode.  Run the simether-live
program as follows:

	# simether-live localhost:10103 eth0

(replace 'eth0' with the name of your Ethernet interface).

Now, let the simulator begin execution by pressing "return"
in the simulator's window.  CFE should start up and display
its startup messages in the window you ran "conn" in.

If you are running more than one copy of the simulator on 
your network, you can set the hardware address for the Ethernet
port by typing:

	CFE> setenv ETH0_HWADDR 40:00:00:11:22:33

(replace 11-22-33 with a unique value for your network).  If you
have configured the EEPROM device file, this setting will be
persistent across restarts of the simulator.

If you configure a DHCP server on your network, you should be
able to:

	CFE> ifconfig eth0 -auto

Or, you can manually configure the network:

	CFE> ifconfig eth0 -addr=192.168.168.168 -mask=255.255.255.0 \
             -gw=192.168.168.1 -dns=192.168.168.240

(the backslash is just for this document - you should type the entire
command on one line).

Then, you can boot a program from your TFTP server:

	CFE> boot -elf myserver:path/filename

You'll need the -elf switch if the program you're loading is in
ELF format.  Eventually, CFE will do this automatically.


------------------------------------------------------------------------------
LOADING CFE INTO THE FUNCTIONAL SIMULATOR'S DEBUGGER
------------------------------------------------------------------------------

The 'debugcfe' script in the sim/ directory will invoke
sb1250-run to generate hardware description files, and then
run CFE under the debugger and step to the first instruction.

------------------------------------------------------------------------------
SIMULATOR HACK: LOADING LARGE IMAGES
------------------------------------------------------------------------------

Loading large programs via TFTP can be painfully slow in the 
simulator.  To work around this, the simulator's bsp includes
an additional flash device called 'flash2'.  This flash is
mapped in the address space normally occupied by the PCMCIA 
adapter and is 64MB in size.

You can instantiate a flash device in the simulator to live at
that same address and pre-load a binary file into it.  CFE
can then be used to boot that file using the "raw" file system
loader.

The files "runcfe" and "runcfe_withnet" in the sim/ directory
have been provided for this purpose.  For example, to boot
Linux in the simulator using this method, you can do:

	./runcfe /path/to/my/vmlinux

(this assumes you have "cfe.srec" in your current directory
from a recent CFE build, you can modify the shell script
if you want it located somewhere else).

When CFE starts, you can do:

	CFE> boot -elf -rawfs flash2:

This will cause CFE to read bytes sequentially from the 
flash2 device and parse them as an ELF file.  The additional
hardware configuration information in the "flash2.m4" 
file (loaded via the "runcfe" script) will cause the file
you choose to live at the PCMCIA flash address 0xB1000000).

You can also use this same technique to load elf files via
the simulated disk, but it is much slower.  To do this, modify
the shell script "runcfe" to point the disk container
file (it's called "disk0.dsk" in the sample) to your binary.
Then you can do:

	CFE> boot -elf -rawfs ide0:

to read the binary in from the disk.  Flash emulation is 
much faster and is the preferred way to load binaries
into the simulator quickly.













