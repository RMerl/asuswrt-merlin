/* Line printer stuff mostly follows the original Centronics printers. See
   IEEE Std.1284-1994 Standard Signaling Method for a Bi-directional Parallel
   Peripheral Interface for Personal Computers for 5 modes of data transfer. */

/* Parallel port registers: data (0x3bc, 0x378, 0x278), status=data+1, control=data+2 */

/* Parallel port status register (read only):
bit 7: NBSY	(1: ready, 0: busy or error or off-line)
bit 6: NACK	(if NBSY=1, then 1; if NBSY=0 then 1: sending data, 0: ready with data)
bit 5:  PAP	(1: out-of-paper)
bit 4: OFON	(1: on-line)
bit 3: NFEH	(1: OK, 0: printer error)
bits 2-0: 07

On out-of-paper: PAP=1, OFON=0, NFEH=1.

"When reading the busy status, read twice in a row and use the value
obtained from the second read. This improves the reliability on some
parallel ports when the busy line may not be connected and is floating."
"On some Okidata printers when the busy signal switches from high to low,
the printer sends a 50 ns glitch on the paper out signal line. Before
declaring a paper out condition, check the status again."
(The Undocumented PC, F. van Gilluwe, p. 711)
*/

/* Parallel port control register (read/write):
bit 4:  IRQ	(1: we want an interrupt when NACK goes from 1 to 0)
bit 3:  DSL	(1: activate printer)
bit 2: NINI	(0: initialise printer)
bit 1:  ALF	(1: printer performs automatic linefeed after each line)
bit 0:  STR	(0->1: generate a Strobe pulse: transport data to printer)
*/

/* Parallel port timing:
   1. wait for NBSY=1
   2. outb(data byte, data port)
   3. wait for at least 0.5us
   4. read control port, OR with STR=0x1, output to control port - purpose:
      generate strobe pulse; this will make the busy line go high
   5. wait for at least 0.5us
   6. read control port, AND with !STR, output to control port
   7. wait for at least 0.5us
   8. in a loop: read status register until NACK bit is 0 or NBSY=1
      (the printer will keep NACK=0 for at least 0.5us, then NACK=NBSY=1).
*/


/* lp ioctls */
#define LPCHAR   0x0601  /* specify the number of times we ask for the status
			    (waiting for ready) before giving up with a timeout
			    The duration may mainly depend on the timing of an inb. */
#define LPTIME   0x0602  /* time to sleep after each timeout (in units of 0.01 sec) */
#define LPABORT  0x0604  /* call with TRUE arg to abort on error,
			    FALSE to retry.  Default is retry.  */
#define LPSETIRQ 0x0605  /* call with new IRQ number,
			    or 0 for polling (no IRQ) */
#define LPGETIRQ 0x0606  /* get the current IRQ number */
#define LPWAIT   0x0608  /* #of loops to wait before taking strobe high */
#define LPCAREFUL   0x0609  /* call with TRUE arg to require out-of-paper, off-
			    line, and error indicators good on all writes,
			    FALSE to ignore them.  Default is ignore. */
#define LPABORTOPEN 0x060a  /* call with TRUE arg to abort open() on error,
			    FALSE to ignore error.  Default is ignore.  */
#define LPGETSTATUS 0x060b  /* return LP_S(minor) */
#define LPRESET     0x060c  /* reset printer */
#define LPGETFLAGS  0x060e  /* get status flags */

#define LPSTRICT    0x060f  /* enable/disable strict compliance (2.0.36) */
                            /* Strict: wait until !READY before taking strobe low;
			       this may be bad for the Epson Stylus 800.
			       Not strict: wait a constant time as given by LPWAIT */
#define LPTRUSTIRQ  0x060f  /* set/unset the LP_TRUST_IRQ flag (2.1.131) */

/* 
 * bit defines for 8255 status port
 * base + 1
 * accessed with LP_S(minor), which gets the byte...
 */
#define LP_PBUSY	0x80  /* inverted input, active high */
#define LP_PACK		0x40  /* unchanged input, active low */
#define LP_POUTPA	0x20  /* unchanged input, active high */
#define LP_PSELECD	0x10  /* unchanged input, active high */
#define LP_PERRORP	0x08  /* unchanged input, active low */
