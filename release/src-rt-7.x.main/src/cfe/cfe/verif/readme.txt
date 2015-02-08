CFE Diagnostic Entry Points
---------------------------

---------------------------------------------------------------------------

The CFE diagnostic entry points are used when running verification
programs under the control of the firmware.  They are fixed (constant)
addresses and have register-based calling sequences.  These entry
points are designed to be as minimal as possible so that as much of the
verification code as possible can be reused.

You can call the KSEG0 or KSEG1 version of the routine.  It is
recommended that you call the cached version from cached code and 
vice versa.

The firmware will reserve the top megabyte of memory for itself.  The
diagnostic must not touch this memory.

The firmware will be compiled to *NOT* use relocatable data and
code segments. 

The firmware will need one general register that it is allowed to
trash without saving - I'll be using this to generate the pointer to
the save area.

The diagnostics can generate records in a log buffer.  This buffer
is allocated in the diagnostic's memory space but is filled in
by the firmware through the diagnostic entry points.  At the end
of the diagnostic run, user commands in the firmware may be used
to look through accumulated log records.

If you mess with the caches or with the console device, the
VAPI functions that print messages to the console may not work.

Log records follow this format:

        +0  SIGNATURE, FORMAT, and ID-CODE
        +8  Number of 64-bit words in 'Log Data' field.
            Upper 32 bits are CP0 count register
        +16 Return address of routine generating this record
        +24 Log Data

The "Log Record size" field is the number of bytes in the "log data"
field.  No log data would use a value of zero.

The bytes in the SIGNATURE word are broken down as follows:

      S1 S2 P1 F1 I1 I2 I3 I4
      CF E1 pp xx ii ii ii ii

The "F1" byte is the format code; it describes the type
of log record being generated. 

     0x00 - General register and CP0 dump
     0x01 - SOC state dump
     0x02 - Generic log data (multiple of 8 bytes)
     0x03 - trace RAM
     0x04 - Diagnostic termination status (8 bytes) 
     0x05 - Floating point registers

The "P1" byte is the processor number, 0 or 1.

The "I1" through "I4" bytes are supplied by the diagnostic
and can take on any value.  You can use these bytes to identify
what part of the program generated this particular log record.

For example, if the diagnostic logs a single value of
0x0123_4567_89ab_cdef the log entry might look like:

      0xCCFF_EE02_0000_0001
      0x0001_3F22_0000_0008
      0xFFFF_FFFF_8000_0120
      0x0123_4567_89AB_CDEF


RETURN TO FIRMWARE
------------------

Description:

   Returns control to the firmware and displays the test status.  
   The status result is in register A0.  

   The firmware will store a "diagnostic termination status" 
   record in the log with the A0 register value.  The ID code
   will be zero for this record.

   CFE's log scanning commands can be used to display log 
   records accumulated by the test.


Routine address:        0xBFC00510  (KSEG1)
                        0x9FC00510  (KSEG0)

On entry:               A0 ($4)     Exit status (9=ok, nonzero=fail)
On return:              Does not return
Registers used:         All


DUMP GENERAL REGISTERS
----------------------

Description:

   This routine causes CFE to display a register dump on the console
   port.  It is assumed that the console hardware state has not been
   altered by the diagnostic.  

   The format of the register dump is:  TBD [XXX should it look like the
   one that the functional simulator uses?]

   The firmware needs one scratch register.


Routine address:        0xBFC00520 (KSEG1)
                        0x9FC00520 (KSEG0)

On Entry:               RA ($31)    Return Address
On Return:              nothing
Registers used:         K0 ($26)    Scratch register for CFE


SET LOG BUFFER
--------------

Description:

   This routine sets the address of the log buffer.  This
   call must be made once at the beginning of the diagnostic
   or else the "SAVE" functions will be considered as
   NOPs.  

   The buffer addresses must be 64-bit aligned.

Routine address:	0xBFC00530 (KSEG1)
			0x9FC00530 (KSEG0)

On Entry:               RA ($31)    Return Address
                        A0 ($4)     Address of start of buffer
                        A1 ($5)     Address of end of buffer
On Return:              Nothing
Registers Used:         K0 ($26)    Scratch register for CFE



LOG SINGLE VALUE
----------------

Description:

   This routine saves a single 64-bit value in the log.


Routine address:	0xBFC00540 (KSEG1)
			0x9FC00540 (KSEG0)

On Entry:               RA ($31)    Return Address
                        A0 ($4)     Low 32 bits are ID code for value
                        A1 ($5)     Value to log
On Return:              Nothing
Registers Used:         K0 ($26)    Scratch register for CFE



LOG MEMORY DATA
---------------

Description:

   This routine saves a block of memory in the log.  The source
   buffer must be 64-bit aligned.


Routine address:	0xBFC00550 (KSEG1)
			0x9FC00550 (KSEG0)

On Entry:               RA ($31)    Return Address
                        A0 ($4)     Low 32 bits are ID code for values
                        A1 ($5)     Address of buffer containing values
                        A2 ($6)     Number of 64-bit words to store
On Return:              Nothing
Registers Used:         K0 ($26)    Scratch register for CFE





SAVE SOC STATE
--------------

Description:

   This routine saves the SOC state in a user-supplied buffer.
   The buffer must be large enough to accomodate the SOC state.
   The SOC state will be written as records with the following
   format:

        uint64_t        phys_address
        uint64_t        value
        uint64_t        phys_address
        uint64_t        value
        ...
        uint64_t        phys_address
        uint64_t        value

   The table of SOC registers to dump will be maintained by 
   the firmware.

   The firmware needs one scratch register.

Routine address:        0xBFC00570 (KSEG1)
                        0x9FC00570 (KSEG0)

On entry:               A0 ($4)    Low 32 bits are ID code for values
                        A1 ($5)    Bitmask of agents to store in log
On return:              nothing
Registers used:         K0 ($26)   Scratch register for CFE
        

SAVE CPU REGISTERS
------------------

Description:

   This routine saves the CPU general registers and certain CP0
   registers in a user-supplied buffer.

   This buffer must be large enough to accomodate the data
   that will be saved.  The registers will be written in 
   the following format:

        uint64_t        general_registers[32]
	uint64_t	C0_INX
	uint64_t	C0_RAND
	uint64_t	C0_TLBLO0
	uint64_t	C0_TLBLO1
	uint64_t	C0_CTEXT
	uint64_t	C0_PGMASK
	uint64_t	C0_WIRED
	uint64_t	C0_BADVADDR
	uint64_t	C0_COUNT
	uint64_t	C0_TLBHI
	uint64_t	C0_COMPARE
	uint64_t	C0_SR
	uint64_t	C0_CAUSE
	uint64_t	C0_EPC
	uint64_t	C0_PRID
	uint64_t	C0_CONFIG
	uint64_t	C0_LLADDR
	uint64_t	C0_WATCHLO
	uint64_t	C0_WATCHHI
	uint64_t	C0_XCTEXT
	uint64_t	C0_ECC
	uint64_t	C0_CACHEERR
	uint64_t	C0_TAGLO
	uint64_t	C0_TAGHI
	uint64_t	C0_ERREPC

   The firmware needs one scratch register.

Routine address:        0xBFC00580 (KSEG1)
                        0x9FC00580 (KSEG0)

On entry:               RA ($31)   Return address
                        A0 ($4)    Low 32 bits are ID code for values
On return:              nothing
Registers used:         K0 ($26)   Scratch register for CFE


SAVE FPU REGISTERS
------------------

Description:

   This routine saves the floating point and floating point
   control registers. The registers will be written in 
   the following format:

        uint64_t        fp_registers[32]
	uint64_t	fp_fir
	uint64_t	fp_status
	uint64_t	fp_condition_codes
	uint64_t	fp_exceptions
	uint64_t	fp_enables

   The firmware needs one scratch register.

Routine address:        0xBFC005B0 (KSEG1)
                        0x9FC005B0 (KSEG0)

On entry:               RA ($31)   Return address
                        A0 ($4)    Low 32 bits are ID code for values
On return:              nothing
Registers used:         K0 ($26)   Scratch register for CFE


DUMP STRING
-----------

Description:

   This routine displays a zero-terminated ASCII text string on the
   console port.  

   The firmware needs one scratch register.


Routine address:        0xBFC00590 (KSEG1)
                        0x9FC00590 (KSEG0)

On entry:               RA ($31)    Return address
                        A0 ($4)     Pointer to null-terminated string
On return:              nothing
Registers used:         K0 ($26)    Scratch register for CFE



SHOW LED MESSAGE
----------------

Description:

   This routine writes four characters onto the SWARM board LEDs. 
   Writing to the LEDs is very fast compared to writing to the 
   console and can be useful for providing progress feedback
   during a run.

   The characters are packed into the low 4 bytes of register A0.
   The string ABCD would be hex 0x0000_0000_4142_4344

   The firmware needs one scratch register

Routine address:        0xBFC005A0 (KSEG1)
                        0x9FC005A0 (KSEG0)

On entry:               RA ($31)    Return Address
                        A0 ($4)     Four characters
On return:              nothing
Registers used:         K0 ($26)    Scratch register for CFE

------------------------------------------------------------------------
