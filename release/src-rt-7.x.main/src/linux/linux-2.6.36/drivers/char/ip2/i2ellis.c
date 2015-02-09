/*******************************************************************************
*
*   (c) 1998 by Computone Corporation
*
********************************************************************************
*
*
*   PACKAGE:     Linux tty Device Driver for IntelliPort family of multiport
*                serial I/O controllers.
*
*   DESCRIPTION: Low-level interface code for the device driver
*                (This is included source code, not a separate compilation
*                module.)
*
*******************************************************************************/
//---------------------------------------------
// Function declarations private to this module
//---------------------------------------------
// Functions called only indirectly through i2eBordStr entries.

static int iiWriteBuf16(i2eBordStrPtr, unsigned char *, int);
static int iiWriteBuf8(i2eBordStrPtr, unsigned char *, int);
static int iiReadBuf16(i2eBordStrPtr, unsigned char *, int);
static int iiReadBuf8(i2eBordStrPtr, unsigned char *, int);

static unsigned short iiReadWord16(i2eBordStrPtr);
static unsigned short iiReadWord8(i2eBordStrPtr);
static void iiWriteWord16(i2eBordStrPtr, unsigned short);
static void iiWriteWord8(i2eBordStrPtr, unsigned short);

static int iiWaitForTxEmptyII(i2eBordStrPtr, int);
static int iiWaitForTxEmptyIIEX(i2eBordStrPtr, int);
static int iiTxMailEmptyII(i2eBordStrPtr);
static int iiTxMailEmptyIIEX(i2eBordStrPtr);
static int iiTrySendMailII(i2eBordStrPtr, unsigned char);
static int iiTrySendMailIIEX(i2eBordStrPtr, unsigned char);

static unsigned short iiGetMailII(i2eBordStrPtr);
static unsigned short iiGetMailIIEX(i2eBordStrPtr);

static void iiEnableMailIrqII(i2eBordStrPtr);
static void iiEnableMailIrqIIEX(i2eBordStrPtr);
static void iiWriteMaskII(i2eBordStrPtr, unsigned char);
static void iiWriteMaskIIEX(i2eBordStrPtr, unsigned char);

static void ii2Nop(void);

//***************
//* Static Data *
//***************

static int ii2Safe;         // Safe I/O address for delay routine

static int iiDelayed;	// Set when the iiResetDelay function is
							// called. Cleared when ANY board is reset.
static DEFINE_RWLOCK(Dl_spinlock);

//********
//* Code *
//********

//=======================================================
// Initialization Routines
//
// iiSetAddress
// iiReset
// iiResetDelay
// iiInitialize
//=======================================================

//******************************************************************************
// Function:   iiSetAddress(pB, address, delay)
// Parameters: pB      - pointer to the board structure
//             address - the purported I/O address of the board
//             delay   - pointer to the 1-ms delay function to use
//                       in this and any future operations to this board
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// This routine (roughly) checks for address validity, sets the i2eValid OK and
// sets the state to II_STATE_COLD which means that we haven't even sent a reset
// yet.
//
//******************************************************************************
static int
iiSetAddress( i2eBordStrPtr pB, int address, delayFunc_t delay )
{
	// Should any failure occur before init is finished...
	pB->i2eValid = I2E_INCOMPLETE;

	// Cannot check upper limit except extremely: Might be microchannel
	// Address must be on an 8-byte boundary

	if ((unsigned int)address <= 0x100
		|| (unsigned int)address >= 0xfff8
		|| (address & 0x7)
		)
	{
		I2_COMPLETE(pB, I2EE_BADADDR);
	}

	// Initialize accelerators
	pB->i2eBase    = address;
	pB->i2eData    = address + FIFO_DATA;
	pB->i2eStatus  = address + FIFO_STATUS;
	pB->i2ePointer = address + FIFO_PTR;
	pB->i2eXMail   = address + FIFO_MAIL;
	pB->i2eXMask   = address + FIFO_MASK;

	// Initialize i/o address for ii2DelayIO
	ii2Safe = address + FIFO_NOP;

	// Initialize the delay routine
	pB->i2eDelay = ((delay != (delayFunc_t)NULL) ? delay : (delayFunc_t)ii2Nop);

	pB->i2eValid = I2E_MAGIC;
	pB->i2eState = II_STATE_COLD;

	I2_COMPLETE(pB, I2EE_GOOD);
}

//******************************************************************************
// Function:   iiReset(pB)
// Parameters: pB - pointer to the board structure
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Attempts to reset the board (see also i2hw.h). Normally, we would use this to
// reset a board immediately after iiSetAddress(), but it is valid to reset a
// board from any state, say, in order to change or re-load loadware. (Under
// such circumstances, no reason to re-run iiSetAddress(), which is why it is a
// separate routine and not included in this routine.
//
//******************************************************************************
static int
iiReset(i2eBordStrPtr pB)
{
	// Magic number should be set, else even the address is suspect
	if (pB->i2eValid != I2E_MAGIC)
	{
		I2_COMPLETE(pB, I2EE_BADMAGIC);
	}

	outb(0, pB->i2eBase + FIFO_RESET);  /* Any data will do */
	iiDelay(pB, 50);                    // Pause between resets
	outb(0, pB->i2eBase + FIFO_RESET);  /* Second reset */

	// We must wait before even attempting to read anything from the FIFO: the
	// board's P.O.S.T may actually attempt to read and write its end of the
	// FIFO in order to check flags, loop back (where supported), etc. On
	// completion of this testing it would reset the FIFO, and on completion
	// of all // P.O.S.T., write the message. We must not mistake data which
	// might have been sent for testing as part of the reset message. To
	// better utilize time, say, when resetting several boards, we allow the
	// delay to be performed externally; in this way the caller can reset 
	// several boards, delay a single time, then call the initialization
	// routine for all.

	pB->i2eState = II_STATE_RESET;

	iiDelayed = 0;	// i.e., the delay routine hasn't been called since the most
					// recent reset.

	// Ensure anything which would have been of use to standard loadware is
	// blanked out, since board has now forgotten everything!.

	pB->i2eUsingIrq = I2_IRQ_UNDEFINED; /* to not use an interrupt so far */
	pB->i2eWaitingForEmptyFifo = 0;
	pB->i2eOutMailWaiting = 0;
	pB->i2eChannelPtr = NULL;
	pB->i2eChannelCnt = 0;

	pB->i2eLeadoffWord[0] = 0;
	pB->i2eFifoInInts = 0;
	pB->i2eFifoOutInts = 0;
	pB->i2eFatalTrap = NULL;
	pB->i2eFatal = 0;

	I2_COMPLETE(pB, I2EE_GOOD);
}

//******************************************************************************
// Function:   iiResetDelay(pB)
// Parameters: pB - pointer to the board structure
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Using the delay defined in board structure, waits two seconds (for board to
// reset).
//
//******************************************************************************
static int
iiResetDelay(i2eBordStrPtr pB)
{
	if (pB->i2eValid != I2E_MAGIC) {
		I2_COMPLETE(pB, I2EE_BADMAGIC);
	}
	if (pB->i2eState != II_STATE_RESET) {
		I2_COMPLETE(pB, I2EE_BADSTATE);
	}
	iiDelay(pB,2000);       /* Now we wait for two seconds. */
	iiDelayed = 1;          /* Delay has been called: ok to initialize */
	I2_COMPLETE(pB, I2EE_GOOD);
}

//******************************************************************************
// Function:   iiInitialize(pB)
// Parameters: pB - pointer to the board structure
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Attempts to read the Power-on reset message. Initializes any remaining fields
// in the pB structure.
//
// This should be called as the third step of a process beginning with
// iiReset(), then iiResetDelay(). This routine checks to see that the structure
// is "valid" and in the reset state, also confirms that the delay routine has
// been called since the latest reset (to any board! overly strong!).
//
//******************************************************************************
static int
iiInitialize(i2eBordStrPtr pB)
{
	int itemp;
	unsigned char c;
	unsigned short utemp;
	unsigned int ilimit;

	if (pB->i2eValid != I2E_MAGIC)
	{
		I2_COMPLETE(pB, I2EE_BADMAGIC);
	}

	if (pB->i2eState != II_STATE_RESET || !iiDelayed)
	{
		I2_COMPLETE(pB, I2EE_BADSTATE);
	}

	// In case there is a failure short of our completely reading the power-up
	// message.
	pB->i2eValid = I2E_INCOMPLETE;


	// Now attempt to read the message.

	for (itemp = 0; itemp < sizeof(porStr); itemp++)
	{
		// We expect the entire message is ready.
		if (!I2_HAS_INPUT(pB)) {
			pB->i2ePomSize = itemp;
			I2_COMPLETE(pB, I2EE_PORM_SHORT);
		}

		pB->i2ePom.c[itemp] = c = inb(pB->i2eData);

		// We check the magic numbers as soon as they are supposed to be read
		// (rather than after) to minimize effect of reading something we
		// already suspect can't be "us".
		if (  (itemp == POR_1_INDEX && c != POR_MAGIC_1) ||
				(itemp == POR_2_INDEX && c != POR_MAGIC_2))
		{
			pB->i2ePomSize = itemp+1;
			I2_COMPLETE(pB, I2EE_BADMAGIC);
		}
	}

	pB->i2ePomSize = itemp;

	// Ensure that this was all the data...
	if (I2_HAS_INPUT(pB))
		I2_COMPLETE(pB, I2EE_PORM_LONG);

	// For now, we'll fail to initialize if P.O.S.T reports bad chip mapper:
	// Implying we will not be able to download any code either:  That's ok: the
	// condition is pretty explicit.
	if (pB->i2ePom.e.porDiag1 & POR_BAD_MAPPER)
	{
		I2_COMPLETE(pB, I2EE_POSTERR);
	}

	// Determine anything which must be done differently depending on the family
	// of boards!
	switch (pB->i2ePom.e.porID & POR_ID_FAMILY)
	{
	case POR_ID_FII:  // IntelliPort-II

		pB->i2eFifoStyle   = FIFO_II;
		pB->i2eFifoSize    = 512;     // 512 bytes, always
		pB->i2eDataWidth16 = false;

		pB->i2eMaxIrq = 15;	// Because board cannot tell us it is in an 8-bit
							// slot, we do allow it to be done (documentation!)

		pB->i2eGoodMap[1] =
		pB->i2eGoodMap[2] =
		pB->i2eGoodMap[3] =
		pB->i2eChannelMap[1] =
		pB->i2eChannelMap[2] =
		pB->i2eChannelMap[3] = 0;

		switch (pB->i2ePom.e.porID & POR_ID_SIZE)
		{
		case POR_ID_II_4:
			pB->i2eGoodMap[0] =
			pB->i2eChannelMap[0] = 0x0f;  // four-port

			// Since porPorts1 is based on the Hardware ID register, the numbers
			// should always be consistent for IntelliPort-II.  Ditto below...
			if (pB->i2ePom.e.porPorts1 != 4)
			{
				I2_COMPLETE(pB, I2EE_INCONSIST);
			}
			break;

		case POR_ID_II_8:
		case POR_ID_II_8R:
			pB->i2eGoodMap[0] =
			pB->i2eChannelMap[0] = 0xff;  // Eight port
			if (pB->i2ePom.e.porPorts1 != 8)
			{
				I2_COMPLETE(pB, I2EE_INCONSIST);
			}
			break;

		case POR_ID_II_6:
			pB->i2eGoodMap[0] =
			pB->i2eChannelMap[0] = 0x3f;  // Six Port
			if (pB->i2ePom.e.porPorts1 != 6)
			{
				I2_COMPLETE(pB, I2EE_INCONSIST);
			}
			break;
		}

		// Fix up the "good channel list based on any errors reported.
		if (pB->i2ePom.e.porDiag1 & POR_BAD_UART1)
		{
			pB->i2eGoodMap[0] &= ~0x0f;
		}

		if (pB->i2ePom.e.porDiag1 & POR_BAD_UART2)
		{
			pB->i2eGoodMap[0] &= ~0xf0;
		}

		break;   // POR_ID_FII case

	case POR_ID_FIIEX:   // IntelliPort-IIEX

		pB->i2eFifoStyle = FIFO_IIEX;

		itemp = pB->i2ePom.e.porFifoSize;

		// Implicit assumption that fifo would not grow beyond 32k, 
		// nor would ever be less than 256.

		if (itemp < 8 || itemp > 15)
		{
			I2_COMPLETE(pB, I2EE_INCONSIST);
		}
		pB->i2eFifoSize = (1 << itemp);

		// These are based on what P.O.S.T thinks should be there, based on
		// box ID registers
		ilimit = pB->i2ePom.e.porNumBoxes;
		if (ilimit > ABS_MAX_BOXES)
		{
			ilimit = ABS_MAX_BOXES;
		}

		// For as many boxes as EXIST, gives the type of box.
		// Added 8/6/93: check for the ISA-4 (asic) which looks like an
		// expandable but for whom "8 or 16?" is not the right question.

		utemp = pB->i2ePom.e.porFlags;
		if (utemp & POR_CEX4)
		{
			pB->i2eChannelMap[0] = 0x000f;
		} else {
			utemp &= POR_BOXES;
			for (itemp = 0; itemp < ilimit; itemp++)
			{
				pB->i2eChannelMap[itemp] = 
					((utemp & POR_BOX_16) ? 0xffff : 0x00ff);
				utemp >>= 1;
			}
		}

		// These are based on what P.O.S.T actually found.

		utemp = (pB->i2ePom.e.porPorts2 << 8) + pB->i2ePom.e.porPorts1;

		for (itemp = 0; itemp < ilimit; itemp++)
		{
			pB->i2eGoodMap[itemp] = 0;
			if (utemp & 1) pB->i2eGoodMap[itemp] |= 0x000f;
			if (utemp & 2) pB->i2eGoodMap[itemp] |= 0x00f0;
			if (utemp & 4) pB->i2eGoodMap[itemp] |= 0x0f00;
			if (utemp & 8) pB->i2eGoodMap[itemp] |= 0xf000;
			utemp >>= 4;
		}

		// Now determine whether we should transfer in 8 or 16-bit mode.
		switch (pB->i2ePom.e.porBus & (POR_BUS_SLOT16 | POR_BUS_DIP16) )
		{
		case POR_BUS_SLOT16 | POR_BUS_DIP16:
			pB->i2eDataWidth16 = true;
			pB->i2eMaxIrq = 15;
			break;

		case POR_BUS_SLOT16:
			pB->i2eDataWidth16 = false;
			pB->i2eMaxIrq = 15;
			break;

		case 0:
		case POR_BUS_DIP16:     // In an 8-bit slot, DIP switch don't care.
		default:
			pB->i2eDataWidth16 = false;
			pB->i2eMaxIrq = 7;
			break;
		}
		break;   // POR_ID_FIIEX case

	default:    // Unknown type of board
		I2_COMPLETE(pB, I2EE_BAD_FAMILY);
		break;
	}  // End the switch based on family

	// Temporarily, claim there is no room in the outbound fifo. 
	// We will maintain this whenever we check for an empty outbound FIFO.
	pB->i2eFifoRemains = 0;

	// Now, based on the bus type, should we expect to be able to re-configure
	// interrupts (say, for testing purposes).
	switch (pB->i2ePom.e.porBus & POR_BUS_TYPE)
	{
	case POR_BUS_T_ISA:
	case POR_BUS_T_UNK:  // If the type of bus is undeclared, assume ok.
	case POR_BUS_T_MCA:
	case POR_BUS_T_EISA:
		break;
	default:
		I2_COMPLETE(pB, I2EE_BADBUS);
	}

	if (pB->i2eDataWidth16)
	{
		pB->i2eWriteBuf  = iiWriteBuf16;
		pB->i2eReadBuf   = iiReadBuf16;
		pB->i2eWriteWord = iiWriteWord16;
		pB->i2eReadWord  = iiReadWord16;
	} else {
		pB->i2eWriteBuf  = iiWriteBuf8;
		pB->i2eReadBuf   = iiReadBuf8;
		pB->i2eWriteWord = iiWriteWord8;
		pB->i2eReadWord  = iiReadWord8;
	}

	switch(pB->i2eFifoStyle)
	{
	case FIFO_II:
		pB->i2eWaitForTxEmpty = iiWaitForTxEmptyII;
		pB->i2eTxMailEmpty    = iiTxMailEmptyII;
		pB->i2eTrySendMail    = iiTrySendMailII;
		pB->i2eGetMail        = iiGetMailII;
		pB->i2eEnableMailIrq  = iiEnableMailIrqII;
		pB->i2eWriteMask      = iiWriteMaskII;

		break;

	case FIFO_IIEX:
		pB->i2eWaitForTxEmpty = iiWaitForTxEmptyIIEX;
		pB->i2eTxMailEmpty    = iiTxMailEmptyIIEX;
		pB->i2eTrySendMail    = iiTrySendMailIIEX;
		pB->i2eGetMail        = iiGetMailIIEX;
		pB->i2eEnableMailIrq  = iiEnableMailIrqIIEX;
		pB->i2eWriteMask      = iiWriteMaskIIEX;

		break;

	default:
		I2_COMPLETE(pB, I2EE_INCONSIST);
	}

	// Initialize state information.
	pB->i2eState = II_STATE_READY;   // Ready to load loadware.

	// Some Final cleanup:
	// For some boards, the bootstrap firmware may perform some sort of test
	// resulting in a stray character pending in the incoming mailbox. If one is
	// there, it should be read and discarded, especially since for the standard
	// firmware, it's the mailbox that interrupts the host.

	pB->i2eStartMail = iiGetMail(pB);

	// Throw it away and clear the mailbox structure element
	pB->i2eStartMail = NO_MAIL_HERE;

	// Everything is ok now, return with good status/

	pB->i2eValid = I2E_MAGIC;
	I2_COMPLETE(pB, I2EE_GOOD);
}

//******************************************************************************
// Function:   ii2DelayTimer(mseconds)
// Parameters: mseconds - number of milliseconds to delay
//
// Returns:    Nothing
//
// Description:
//
// This routine delays for approximately mseconds milliseconds and is intended
// to be called indirectly through i2Delay field in i2eBordStr. It uses the
// Linux timer_list mechanism.
//
// The Linux timers use a unit called "jiffies" which are 10mS in the Intel
// architecture. This function rounds the delay period up to the next "jiffy".
// In the Alpha architecture the "jiffy" is 1mS, but this driver is not intended
// for Alpha platforms at this time.
//
//******************************************************************************
static void
ii2DelayTimer(unsigned int mseconds)
{
	msleep_interruptible(mseconds);
}


//******************************************************************************
// Function:   ii2Nop()
// Parameters: None
//
// Returns:    Nothing
//
// Description:
//
// iiInitialize will set i2eDelay to this if the delay parameter is NULL. This
// saves checking for a NULL pointer at every call.
//******************************************************************************
static void
ii2Nop(void)
{
	return;	// no mystery here
}

//=======================================================
// Routines which are available in 8/16-bit versions, or
// in different fifo styles. These are ALL called
// indirectly through the board structure.
//=======================================================

//******************************************************************************
// Function:   iiWriteBuf16(pB, address, count)
// Parameters: pB      - pointer to board structure
//             address - address of data to write
//             count   - number of data bytes to write
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Writes 'count' bytes from 'address' to the data fifo specified by the board
// structure pointer pB. Should count happen to be odd, an extra pad byte is
// sent (identity unknown...). Uses 16-bit (word) operations. Is called
// indirectly through pB->i2eWriteBuf.
//
//******************************************************************************
static int
iiWriteBuf16(i2eBordStrPtr pB, unsigned char *address, int count)
{
	// Rudimentary sanity checking here.
	if (pB->i2eValid != I2E_MAGIC)
		I2_COMPLETE(pB, I2EE_INVALID);

	I2_OUTSW(pB->i2eData, address, count);

	I2_COMPLETE(pB, I2EE_GOOD);
}

//******************************************************************************
// Function:   iiWriteBuf8(pB, address, count)
// Parameters: pB      - pointer to board structure
//             address - address of data to write
//             count   - number of data bytes to write
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Writes 'count' bytes from 'address' to the data fifo specified by the board
// structure pointer pB. Should count happen to be odd, an extra pad byte is
// sent (identity unknown...). This is to be consistent with the 16-bit version.
// Uses 8-bit (byte) operations. Is called indirectly through pB->i2eWriteBuf.
//
//******************************************************************************
static int
iiWriteBuf8(i2eBordStrPtr pB, unsigned char *address, int count)
{
	/* Rudimentary sanity checking here */
	if (pB->i2eValid != I2E_MAGIC)
		I2_COMPLETE(pB, I2EE_INVALID);

	I2_OUTSB(pB->i2eData, address, count);

	I2_COMPLETE(pB, I2EE_GOOD);
}

//******************************************************************************
// Function:   iiReadBuf16(pB, address, count)
// Parameters: pB      - pointer to board structure
//             address - address to put data read
//             count   - number of data bytes to read
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Reads 'count' bytes into 'address' from the data fifo specified by the board
// structure pointer pB. Should count happen to be odd, an extra pad byte is
// received (identity unknown...). Uses 16-bit (word) operations. Is called
// indirectly through pB->i2eReadBuf.
//
//******************************************************************************
static int
iiReadBuf16(i2eBordStrPtr pB, unsigned char *address, int count)
{
	// Rudimentary sanity checking here.
	if (pB->i2eValid != I2E_MAGIC)
		I2_COMPLETE(pB, I2EE_INVALID);

	I2_INSW(pB->i2eData, address, count);

	I2_COMPLETE(pB, I2EE_GOOD);
}

//******************************************************************************
// Function:   iiReadBuf8(pB, address, count)
// Parameters: pB      - pointer to board structure
//             address - address to put data read
//             count   - number of data bytes to read
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Reads 'count' bytes into 'address' from the data fifo specified by the board
// structure pointer pB. Should count happen to be odd, an extra pad byte is
// received (identity unknown...). This to match the 16-bit behaviour. Uses
// 8-bit (byte) operations. Is called indirectly through pB->i2eReadBuf.
//
//******************************************************************************
static int
iiReadBuf8(i2eBordStrPtr pB, unsigned char *address, int count)
{
	// Rudimentary sanity checking here.
	if (pB->i2eValid != I2E_MAGIC)
		I2_COMPLETE(pB, I2EE_INVALID);

	I2_INSB(pB->i2eData, address, count);

	I2_COMPLETE(pB, I2EE_GOOD);
}

//******************************************************************************
// Function:   iiReadWord16(pB)
// Parameters: pB      - pointer to board structure
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Returns the word read from the data fifo specified by the board-structure
// pointer pB. Uses a 16-bit operation. Is called indirectly through
// pB->i2eReadWord.
//
//******************************************************************************
static unsigned short
iiReadWord16(i2eBordStrPtr pB)
{
	return inw(pB->i2eData);
}

//******************************************************************************
// Function:   iiReadWord8(pB)
// Parameters: pB      - pointer to board structure
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Returns the word read from the data fifo specified by the board-structure
// pointer pB. Uses two 8-bit operations. Bytes are assumed to be LSB first. Is
// called indirectly through pB->i2eReadWord.
//
//******************************************************************************
static unsigned short
iiReadWord8(i2eBordStrPtr pB)
{
	unsigned short urs;

	urs = inb(pB->i2eData);

	return (inb(pB->i2eData) << 8) | urs;
}

//******************************************************************************
// Function:   iiWriteWord16(pB, value)
// Parameters: pB    - pointer to board structure
//             value - data to write
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Writes the word 'value' to the data fifo specified by the board-structure
// pointer pB. Uses 16-bit operation. Is called indirectly through
// pB->i2eWriteWord.
//
//******************************************************************************
static void
iiWriteWord16(i2eBordStrPtr pB, unsigned short value)
{
	outw((int)value, pB->i2eData);
}

//******************************************************************************
// Function:   iiWriteWord8(pB, value)
// Parameters: pB    - pointer to board structure
//             value - data to write
//
// Returns:    True if everything appears copacetic.
//             False if there is any error: the pB->i2eError field has the error
//
// Description:
//
// Writes the word 'value' to the data fifo specified by the board-structure
// pointer pB. Uses two 8-bit operations (writes LSB first). Is called
// indirectly through pB->i2eWriteWord.
//
//******************************************************************************
static void
iiWriteWord8(i2eBordStrPtr pB, unsigned short value)
{
	outb((char)value, pB->i2eData);
	outb((char)(value >> 8), pB->i2eData);
}

//******************************************************************************
// Function:   iiWaitForTxEmptyII(pB, mSdelay)
// Parameters: pB      - pointer to board structure
//             mSdelay - period to wait before returning
//
// Returns:    True if the FIFO is empty.
//             False if it not empty in the required time: the pB->i2eError
//             field has the error.
//
// Description:
//
// Waits up to "mSdelay" milliseconds for the outgoing FIFO to become empty; if
// not empty by the required time, returns false and error in pB->i2eError,
// otherwise returns true.
//
// mSdelay == 0 is taken to mean must be empty on the first test.
//
// This version operates on IntelliPort-II - style FIFO's
//
// Note this routine is organized so that if status is ok there is no delay at
// all called either before or after the test.  Is called indirectly through
// pB->i2eWaitForTxEmpty.
//
//******************************************************************************
static int
iiWaitForTxEmptyII(i2eBordStrPtr pB, int mSdelay)
{
	unsigned long	flags;
	int itemp;

	for (;;)
	{
		// This routine hinges on being able to see the "other" status register
		// (as seen by the local processor).  His incoming fifo is our outgoing
		// FIFO.
		//
		// By the nature of this routine, you would be using this as part of a
		// larger atomic context: i.e., you would use this routine to ensure the
		// fifo empty, then act on this information. Between these two halves, 
		// you will generally not want to service interrupts or in any way 
		// disrupt the assumptions implicit in the larger context.
		//
		// Even worse, however, this routine "shifts" the status register to 
		// point to the local status register which is not the usual situation.
		// Therefore for extra safety, we force the critical section to be
		// completely atomic, and pick up after ourselves before allowing any
		// interrupts of any kind.


		write_lock_irqsave(&Dl_spinlock, flags);
		outb(SEL_COMMAND, pB->i2ePointer);
		outb(SEL_CMD_SH, pB->i2ePointer);

		itemp = inb(pB->i2eStatus);

		outb(SEL_COMMAND, pB->i2ePointer);
		outb(SEL_CMD_UNSH, pB->i2ePointer);

		if (itemp & ST_IN_EMPTY)
		{
			I2_UPDATE_FIFO_ROOM(pB);
			write_unlock_irqrestore(&Dl_spinlock, flags);
			I2_COMPLETE(pB, I2EE_GOOD);
		}

		write_unlock_irqrestore(&Dl_spinlock, flags);

		if (mSdelay-- == 0)
			break;

		iiDelay(pB, 1);      /* 1 mS granularity on checking condition */
	}
	I2_COMPLETE(pB, I2EE_TXE_TIME);
}

//******************************************************************************
// Function:   iiWaitForTxEmptyIIEX(pB, mSdelay)
// Parameters: pB      - pointer to board structure
//             mSdelay - period to wait before returning
//
// Returns:    True if the FIFO is empty.
//             False if it not empty in the required time: the pB->i2eError
//             field has the error.
//
// Description:
//
// Waits up to "mSdelay" milliseconds for the outgoing FIFO to become empty; if
// not empty by the required time, returns false and error in pB->i2eError,
// otherwise returns true.
//
// mSdelay == 0 is taken to mean must be empty on the first test.
//
// This version operates on IntelliPort-IIEX - style FIFO's
//
// Note this routine is organized so that if status is ok there is no delay at
// all called either before or after the test.  Is called indirectly through
// pB->i2eWaitForTxEmpty.
//
//******************************************************************************
static int
iiWaitForTxEmptyIIEX(i2eBordStrPtr pB, int mSdelay)
{
	unsigned long	flags;

	for (;;)
	{
		// By the nature of this routine, you would be using this as part of a
		// larger atomic context: i.e., you would use this routine to ensure the
		// fifo empty, then act on this information. Between these two halves,
		// you will generally not want to service interrupts or in any way
		// disrupt the assumptions implicit in the larger context.

		write_lock_irqsave(&Dl_spinlock, flags);

		if (inb(pB->i2eStatus) & STE_OUT_MT) {
			I2_UPDATE_FIFO_ROOM(pB);
			write_unlock_irqrestore(&Dl_spinlock, flags);
			I2_COMPLETE(pB, I2EE_GOOD);
		}
		write_unlock_irqrestore(&Dl_spinlock, flags);

		if (mSdelay-- == 0)
			break;

		iiDelay(pB, 1);      // 1 mS granularity on checking condition
	}
	I2_COMPLETE(pB, I2EE_TXE_TIME);
}

//******************************************************************************
// Function:   iiTxMailEmptyII(pB)
// Parameters: pB      - pointer to board structure
//
// Returns:    True if the transmit mailbox is empty.
//             False if it not empty.
//
// Description:
//
// Returns true or false according to whether the transmit mailbox is empty (and
// therefore able to accept more mail)
//
// This version operates on IntelliPort-II - style FIFO's
//
//******************************************************************************
static int
iiTxMailEmptyII(i2eBordStrPtr pB)
{
	int port = pB->i2ePointer;
	outb(SEL_OUTMAIL, port);
	return inb(port) == 0;
}

//******************************************************************************
// Function:   iiTxMailEmptyIIEX(pB)
// Parameters: pB      - pointer to board structure
//
// Returns:    True if the transmit mailbox is empty.
//             False if it not empty.
//
// Description:
//
// Returns true or false according to whether the transmit mailbox is empty (and
// therefore able to accept more mail)
//
// This version operates on IntelliPort-IIEX - style FIFO's
//
//******************************************************************************
static int
iiTxMailEmptyIIEX(i2eBordStrPtr pB)
{
	return !(inb(pB->i2eStatus) & STE_OUT_MAIL);
}

//******************************************************************************
// Function:   iiTrySendMailII(pB,mail)
// Parameters: pB   - pointer to board structure
//             mail - value to write to mailbox
//
// Returns:    True if the transmit mailbox is empty, and mail is sent.
//             False if it not empty.
//
// Description:
//
// If outgoing mailbox is empty, sends mail and returns true. If outgoing
// mailbox is not empty, returns false.
//
// This version operates on IntelliPort-II - style FIFO's
//
//******************************************************************************
static int
iiTrySendMailII(i2eBordStrPtr pB, unsigned char mail)
{
	int port = pB->i2ePointer;

	outb(SEL_OUTMAIL, port);
	if (inb(port) == 0) {
		outb(SEL_OUTMAIL, port);
		outb(mail, port);
		return 1;
	}
	return 0;
}

//******************************************************************************
// Function:   iiTrySendMailIIEX(pB,mail)
// Parameters: pB   - pointer to board structure
//             mail - value to write to mailbox
//
// Returns:    True if the transmit mailbox is empty, and mail is sent.
//             False if it not empty.
//
// Description:
//
// If outgoing mailbox is empty, sends mail and returns true. If outgoing
// mailbox is not empty, returns false.
//
// This version operates on IntelliPort-IIEX - style FIFO's
//
//******************************************************************************
static int
iiTrySendMailIIEX(i2eBordStrPtr pB, unsigned char mail)
{
	if (inb(pB->i2eStatus) & STE_OUT_MAIL)
		return 0;
	outb(mail, pB->i2eXMail);
	return 1;
}

//******************************************************************************
// Function:   iiGetMailII(pB,mail)
// Parameters: pB   - pointer to board structure
//
// Returns:    Mailbox data or NO_MAIL_HERE.
//
// Description:
//
// If no mail available, returns NO_MAIL_HERE otherwise returns the data from
// the mailbox, which is guaranteed != NO_MAIL_HERE.
//
// This version operates on IntelliPort-II - style FIFO's
//
//******************************************************************************
static unsigned short
iiGetMailII(i2eBordStrPtr pB)
{
	if (I2_HAS_MAIL(pB)) {
		outb(SEL_INMAIL, pB->i2ePointer);
		return inb(pB->i2ePointer);
	} else {
		return NO_MAIL_HERE;
	}
}

//******************************************************************************
// Function:   iiGetMailIIEX(pB,mail)
// Parameters: pB   - pointer to board structure
//
// Returns:    Mailbox data or NO_MAIL_HERE.
//
// Description:
//
// If no mail available, returns NO_MAIL_HERE otherwise returns the data from
// the mailbox, which is guaranteed != NO_MAIL_HERE.
//
// This version operates on IntelliPort-IIEX - style FIFO's
//
//******************************************************************************
static unsigned short
iiGetMailIIEX(i2eBordStrPtr pB)
{
	if (I2_HAS_MAIL(pB))
		return inb(pB->i2eXMail);
	else
		return NO_MAIL_HERE;
}

//******************************************************************************
// Function:   iiEnableMailIrqII(pB)
// Parameters: pB - pointer to board structure
//
// Returns:    Nothing
//
// Description:
//
// Enables board to interrupt host (only) by writing to host's in-bound mailbox.
//
// This version operates on IntelliPort-II - style FIFO's
//
//******************************************************************************
static void
iiEnableMailIrqII(i2eBordStrPtr pB)
{
	outb(SEL_MASK, pB->i2ePointer);
	outb(ST_IN_MAIL, pB->i2ePointer);
}

//******************************************************************************
// Function:   iiEnableMailIrqIIEX(pB)
// Parameters: pB - pointer to board structure
//
// Returns:    Nothing
//
// Description:
//
// Enables board to interrupt host (only) by writing to host's in-bound mailbox.
//
// This version operates on IntelliPort-IIEX - style FIFO's
//
//******************************************************************************
static void
iiEnableMailIrqIIEX(i2eBordStrPtr pB)
{
	outb(MX_IN_MAIL, pB->i2eXMask);
}

//******************************************************************************
// Function:   iiWriteMaskII(pB)
// Parameters: pB - pointer to board structure
//
// Returns:    Nothing
//
// Description:
//
// Writes arbitrary value to the mask register.
//
// This version operates on IntelliPort-II - style FIFO's
//
//******************************************************************************
static void
iiWriteMaskII(i2eBordStrPtr pB, unsigned char value)
{
	outb(SEL_MASK, pB->i2ePointer);
	outb(value, pB->i2ePointer);
}

//******************************************************************************
// Function:   iiWriteMaskIIEX(pB)
// Parameters: pB - pointer to board structure
//
// Returns:    Nothing
//
// Description:
//
// Writes arbitrary value to the mask register.
//
// This version operates on IntelliPort-IIEX - style FIFO's
//
//******************************************************************************
static void
iiWriteMaskIIEX(i2eBordStrPtr pB, unsigned char value)
{
	outb(value, pB->i2eXMask);
}

//******************************************************************************
// Function:   iiDownloadBlock(pB, pSource, isStandard)
// Parameters: pB         - pointer to board structure
//             pSource    - loadware block to download
//             isStandard - True if "standard" loadware, else false.
//
// Returns:    Success or Failure
//
// Description:
//
// Downloads a single block (at pSource)to the board referenced by pB. Caller
// sets isStandard to true/false according to whether the "standard" loadware is
// what's being loaded. The normal process, then, is to perform an iiInitialize
// to the board, then perform some number of iiDownloadBlocks using the returned
// state to determine when download is complete.
//
// Possible return values: (see I2ELLIS.H)
// II_DOWN_BADVALID
// II_DOWN_BADFILE
// II_DOWN_CONTINUING
// II_DOWN_GOOD
// II_DOWN_BAD
// II_DOWN_BADSTATE
// II_DOWN_TIMEOUT
//
// Uses the i2eState and i2eToLoad fields (initialized at iiInitialize) to
// determine whether this is the first block, whether to check for magic
// numbers, how many blocks there are to go...
//
//******************************************************************************
static int
iiDownloadBlock ( i2eBordStrPtr pB, loadHdrStrPtr pSource, int isStandard)
{
	int itemp;
	int loadedFirst;

	if (pB->i2eValid != I2E_MAGIC) return II_DOWN_BADVALID;

	switch(pB->i2eState)
	{
	case II_STATE_READY:

		// Loading the first block after reset. Must check the magic number of the
		// loadfile, store the number of blocks we expect to load.
		if (pSource->e.loadMagic != MAGIC_LOADFILE)
		{
			return II_DOWN_BADFILE;
		}

		// Next we store the total number of blocks to load, including this one.
		pB->i2eToLoad = 1 + pSource->e.loadBlocksMore;

		// Set the state, store the version numbers. ('Cause this may have come
		// from a file - we might want to report these versions and revisions in
		// case of an error!
		pB->i2eState = II_STATE_LOADING;
		pB->i2eLVersion = pSource->e.loadVersion;
		pB->i2eLRevision = pSource->e.loadRevision;
		pB->i2eLSub = pSource->e.loadSubRevision;

		// The time and date of compilation is also available but don't bother
		// storing it for normal purposes.
		loadedFirst = 1;
		break;

	case II_STATE_LOADING:
		loadedFirst = 0;
		break;

	default:
		return II_DOWN_BADSTATE;
	}

	// Now we must be in the II_STATE_LOADING state, and we assume i2eToLoad
	// must be positive still, because otherwise we would have cleaned up last
	// time and set the state to II_STATE_LOADED.
	if (!iiWaitForTxEmpty(pB, MAX_DLOAD_READ_TIME)) {
		return II_DOWN_TIMEOUT;
	}

	if (!iiWriteBuf(pB, pSource->c, LOADWARE_BLOCK_SIZE)) {
		return II_DOWN_BADVALID;
	}

	// If we just loaded the first block, wait for the fifo to empty an extra
	// long time to allow for any special startup code in the firmware, like
	// sending status messages to the LCD's.

	if (loadedFirst) {
		if (!iiWaitForTxEmpty(pB, MAX_DLOAD_START_TIME)) {
			return II_DOWN_TIMEOUT;
		}
	}

	// Determine whether this was our last block!
	if (--(pB->i2eToLoad)) {
		return II_DOWN_CONTINUING;    // more to come...
	}

	// It WAS our last block: Clean up operations...
	// ...Wait for last buffer to drain from the board...
	if (!iiWaitForTxEmpty(pB, MAX_DLOAD_READ_TIME)) {
		return II_DOWN_TIMEOUT;
	}
	// If there were only a single block written, this would come back
	// immediately and be harmless, though not strictly necessary.
	itemp = MAX_DLOAD_ACK_TIME/10;
	while (--itemp) {
		if (I2_HAS_INPUT(pB)) {
			switch (inb(pB->i2eData)) {
			case LOADWARE_OK:
				pB->i2eState =
					isStandard ? II_STATE_STDLOADED :II_STATE_LOADED;

				// Some revisions of the bootstrap firmware (e.g. ISA-8 1.0.2)
				// will, // if there is a debug port attached, require some
				// time to send information to the debug port now. It will do
				// this before // executing any of the code we just downloaded.
				// It may take up to 700 milliseconds.
				if (pB->i2ePom.e.porDiag2 & POR_DEBUG_PORT) {
					iiDelay(pB, 700);
				}

				return II_DOWN_GOOD;

			case LOADWARE_BAD:
			default:
				return II_DOWN_BAD;
			}
		}

		iiDelay(pB, 10);      // 10 mS granularity on checking condition
	}

	// Drop-through --> timed out waiting for firmware confirmation

	pB->i2eState = II_STATE_BADLOAD;
	return II_DOWN_TIMEOUT;
}

//******************************************************************************
// Function:   iiDownloadAll(pB, pSource, isStandard, size)
// Parameters: pB         - pointer to board structure
//             pSource    - loadware block to download
//             isStandard - True if "standard" loadware, else false.
//             size       - size of data to download (in bytes)
//
// Returns:    Success or Failure
//
// Description:
//
// Given a pointer to a board structure, a pointer to the beginning of some
// loadware, whether it is considered the "standard loadware", and the size of
// the array in bytes loads the entire array to the board as loadware.
//
// Assumes the board has been freshly reset and the power-up reset message read.
// (i.e., in II_STATE_READY). Complains if state is bad, or if there seems to be
// too much or too little data to load, or if iiDownloadBlock complains.
//******************************************************************************
static int
iiDownloadAll(i2eBordStrPtr pB, loadHdrStrPtr pSource, int isStandard, int size)
{
	int status;

	// We know (from context) board should be ready for the first block of
	// download.  Complain if not.
	if (pB->i2eState != II_STATE_READY) return II_DOWN_BADSTATE;

	while (size > 0) {
		size -= LOADWARE_BLOCK_SIZE;	// How much data should there be left to
										// load after the following operation ?

		// Note we just bump pSource by "one", because its size is actually that
		// of an entire block, same as LOADWARE_BLOCK_SIZE.
		status = iiDownloadBlock(pB, pSource++, isStandard);

		switch(status)
		{
		case II_DOWN_GOOD:
			return ( (size > 0) ? II_DOWN_OVER : II_DOWN_GOOD);

		case II_DOWN_CONTINUING:
			break;

		default:
			return status;
		}
	}

	// We shouldn't drop out: it means "while" caught us with nothing left to
	// download, yet the previous DownloadBlock did not return complete. Ergo,
	// not enough data to match the size byte in the header.
	return II_DOWN_UNDER;
}
