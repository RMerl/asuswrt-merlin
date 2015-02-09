/*
** -----------------------------------------------------------------------------
**
**  Perle Specialix driver for Linux
**  Ported from existing RIO Driver for SCO sources.
 *
 *  (C) 1990 - 2000 Specialix International Ltd., Byfleet, Surrey, UK.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**	Module		: rioboot.c
**	SID		: 1.3
**	Last Modified	: 11/6/98 10:33:36
**	Retrieved	: 11/6/98 10:33:48
**
**  ident @(#)rioboot.c	1.3
**
** -----------------------------------------------------------------------------
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/termios.h>
#include <linux/serial.h>
#include <linux/vmalloc.h>
#include <linux/generic_serial.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/string.h>
#include <asm/uaccess.h>


#include "linux_compat.h"
#include "rio_linux.h"
#include "pkt.h"
#include "daemon.h"
#include "rio.h"
#include "riospace.h"
#include "cmdpkt.h"
#include "map.h"
#include "rup.h"
#include "port.h"
#include "riodrvr.h"
#include "rioinfo.h"
#include "func.h"
#include "errors.h"
#include "pci.h"

#include "parmmap.h"
#include "unixrup.h"
#include "board.h"
#include "host.h"
#include "phb.h"
#include "link.h"
#include "cmdblk.h"
#include "route.h"

static int RIOBootComplete(struct rio_info *p, struct Host *HostP, unsigned int Rup, struct PktCmd __iomem *PktCmdP);

static const unsigned char RIOAtVec2Ctrl[] = {
	/* 0 */ INTERRUPT_DISABLE,
	/* 1 */ INTERRUPT_DISABLE,
	/* 2 */ INTERRUPT_DISABLE,
	/* 3 */ INTERRUPT_DISABLE,
	/* 4 */ INTERRUPT_DISABLE,
	/* 5 */ INTERRUPT_DISABLE,
	/* 6 */ INTERRUPT_DISABLE,
	/* 7 */ INTERRUPT_DISABLE,
	/* 8 */ INTERRUPT_DISABLE,
	/* 9 */ IRQ_9 | INTERRUPT_ENABLE,
	/* 10 */ INTERRUPT_DISABLE,
	/* 11 */ IRQ_11 | INTERRUPT_ENABLE,
	/* 12 */ IRQ_12 | INTERRUPT_ENABLE,
	/* 13 */ INTERRUPT_DISABLE,
	/* 14 */ INTERRUPT_DISABLE,
	/* 15 */ IRQ_15 | INTERRUPT_ENABLE
};

/**
 *	RIOBootCodeRTA		-	Load RTA boot code
 *	@p: RIO to load
 *	@rbp: Download descriptor
 *
 *	Called when the user process initiates booting of the card firmware.
 *	Lads the firmware
 */

int RIOBootCodeRTA(struct rio_info *p, struct DownLoad * rbp)
{
	int offset;

	func_enter();

	rio_dprintk(RIO_DEBUG_BOOT, "Data at user address %p\n", rbp->DataP);

	/*
	 ** Check that we have set asside enough memory for this
	 */
	if (rbp->Count > SIXTY_FOUR_K) {
		rio_dprintk(RIO_DEBUG_BOOT, "RTA Boot Code Too Large!\n");
		p->RIOError.Error = HOST_FILE_TOO_LARGE;
		func_exit();
		return -ENOMEM;
	}

	if (p->RIOBooting) {
		rio_dprintk(RIO_DEBUG_BOOT, "RTA Boot Code : BUSY BUSY BUSY!\n");
		p->RIOError.Error = BOOT_IN_PROGRESS;
		func_exit();
		return -EBUSY;
	}

	/*
	 ** The data we load in must end on a (RTA_BOOT_DATA_SIZE) byte boundary,
	 ** so calculate how far we have to move the data up the buffer
	 ** to achieve this.
	 */
	offset = (RTA_BOOT_DATA_SIZE - (rbp->Count % RTA_BOOT_DATA_SIZE)) % RTA_BOOT_DATA_SIZE;

	/*
	 ** Be clean, and clear the 'unused' portion of the boot buffer,
	 ** because it will (eventually) be part of the Rta run time environment
	 ** and so should be zeroed.
	 */
	memset(p->RIOBootPackets, 0, offset);

	/*
	 ** Copy the data from user space into the array
	 */

	if (copy_from_user(((u8 *)p->RIOBootPackets) + offset, rbp->DataP, rbp->Count)) {
		rio_dprintk(RIO_DEBUG_BOOT, "Bad data copy from user space\n");
		p->RIOError.Error = COPYIN_FAILED;
		func_exit();
		return -EFAULT;
	}

	/*
	 ** Make sure that our copy of the size includes that offset we discussed
	 ** earlier.
	 */
	p->RIONumBootPkts = (rbp->Count + offset) / RTA_BOOT_DATA_SIZE;
	p->RIOBootCount = rbp->Count;

	func_exit();
	return 0;
}

/**
 *	rio_start_card_running		-	host card start
 *	@HostP: The RIO to kick off
 *
 *	Start a RIO processor unit running. Encapsulates the knowledge
 *	of the card type.
 */

void rio_start_card_running(struct Host *HostP)
{
	switch (HostP->Type) {
	case RIO_AT:
		rio_dprintk(RIO_DEBUG_BOOT, "Start ISA card running\n");
		writeb(BOOT_FROM_RAM | EXTERNAL_BUS_ON | HostP->Mode | RIOAtVec2Ctrl[HostP->Ivec & 0xF], &HostP->Control);
		break;
	case RIO_PCI:
		/*
		 ** PCI is much the same as MCA. Everything is once again memory
		 ** mapped, so we are writing to memory registers instead of io
		 ** ports.
		 */
		rio_dprintk(RIO_DEBUG_BOOT, "Start PCI card running\n");
		writeb(PCITpBootFromRam | PCITpBusEnable | HostP->Mode, &HostP->Control);
		break;
	default:
		rio_dprintk(RIO_DEBUG_BOOT, "Unknown host type %d\n", HostP->Type);
		break;
	}
	return;
}

/*
** Load in the host boot code - load it directly onto all halted hosts
** of the correct type.
**
** Put your rubber pants on before messing with this code - even the magic
** numbers have trouble understanding what they are doing here.
*/

int RIOBootCodeHOST(struct rio_info *p, struct DownLoad *rbp)
{
	struct Host *HostP;
	u8 __iomem *Cad;
	PARM_MAP __iomem *ParmMapP;
	int RupN;
	int PortN;
	unsigned int host;
	u8 __iomem *StartP;
	u8 __iomem *DestP;
	int wait_count;
	u16 OldParmMap;
	u16 offset;		/* It is very important that this is a u16 */
	u8 *DownCode = NULL;
	unsigned long flags;

	HostP = NULL;		/* Assure the compiler we've initialized it */


	/* Walk the hosts */
	for (host = 0; host < p->RIONumHosts; host++) {
		rio_dprintk(RIO_DEBUG_BOOT, "Attempt to boot host %d\n", host);
		HostP = &p->RIOHosts[host];

		rio_dprintk(RIO_DEBUG_BOOT, "Host Type = 0x%x, Mode = 0x%x, IVec = 0x%x\n", HostP->Type, HostP->Mode, HostP->Ivec);

		/* Don't boot hosts already running */
		if ((HostP->Flags & RUN_STATE) != RC_WAITING) {
			rio_dprintk(RIO_DEBUG_BOOT, "%s %d already running\n", "Host", host);
			continue;
		}

		/*
		 ** Grab a pointer to the card (ioremapped)
		 */
		Cad = HostP->Caddr;

		/*
		 ** We are going to (try) and load in rbp->Count bytes.
		 ** The last byte will reside at p->RIOConf.HostLoadBase-1;
		 ** Therefore, we need to start copying at address
		 ** (caddr+p->RIOConf.HostLoadBase-rbp->Count)
		 */
		StartP = &Cad[p->RIOConf.HostLoadBase - rbp->Count];

		rio_dprintk(RIO_DEBUG_BOOT, "kernel virtual address for host is %p\n", Cad);
		rio_dprintk(RIO_DEBUG_BOOT, "kernel virtual address for download is %p\n", StartP);
		rio_dprintk(RIO_DEBUG_BOOT, "host loadbase is 0x%x\n", p->RIOConf.HostLoadBase);
		rio_dprintk(RIO_DEBUG_BOOT, "size of download is 0x%x\n", rbp->Count);

		/* Make sure it fits */
		if (p->RIOConf.HostLoadBase < rbp->Count) {
			rio_dprintk(RIO_DEBUG_BOOT, "Bin too large\n");
			p->RIOError.Error = HOST_FILE_TOO_LARGE;
			func_exit();
			return -EFBIG;
		}
		/*
		 ** Ensure that the host really is stopped.
		 ** Disable it's external bus & twang its reset line.
		 */
		RIOHostReset(HostP->Type, HostP->CardP, HostP->Slot);

		/*
		 ** Copy the data directly from user space to the SRAM.
		 ** This ain't going to be none too clever if the download
		 ** code is bigger than this segment.
		 */
		rio_dprintk(RIO_DEBUG_BOOT, "Copy in code\n");

		/* Buffer to local memory as we want to use I/O space and
		   some cards only do 8 or 16 bit I/O */

		DownCode = vmalloc(rbp->Count);
		if (!DownCode) {
			p->RIOError.Error = NOT_ENOUGH_CORE_FOR_PCI_COPY;
			func_exit();
			return -ENOMEM;
		}
		if (copy_from_user(DownCode, rbp->DataP, rbp->Count)) {
			kfree(DownCode);
			p->RIOError.Error = COPYIN_FAILED;
			func_exit();
			return -EFAULT;
		}
		HostP->Copy(DownCode, StartP, rbp->Count);
		vfree(DownCode);

		rio_dprintk(RIO_DEBUG_BOOT, "Copy completed\n");

		/*
		 **                     S T O P !
		 **
		 ** Upto this point the code has been fairly rational, and possibly
		 ** even straight forward. What follows is a pile of crud that will
		 ** magically turn into six bytes of transputer assembler. Normally
		 ** you would expect an array or something, but, being me, I have
		 ** chosen [been told] to use a technique whereby the startup code
		 ** will be correct if we change the loadbase for the code. Which
		 ** brings us onto another issue - the loadbase is the *end* of the
		 ** code, not the start.
		 **
		 ** If I were you I wouldn't start from here.
		 */

		/*
		 ** We now need to insert a short boot section into
		 ** the memory at the end of Sram2. This is normally (de)composed
		 ** of the last eight bytes of the download code. The
		 ** download has been assembled/compiled to expect to be
		 ** loaded from 0x7FFF downwards. We have loaded it
		 ** at some other address. The startup code goes into the small
		 ** ram window at Sram2, in the last 8 bytes, which are really
		 ** at addresses 0x7FF8-0x7FFF.
		 **
		 ** If the loadbase is, say, 0x7C00, then we need to branch to
		 ** address 0x7BFE to run the host.bin startup code. We assemble
		 ** this jump manually.
		 **
		 ** The two byte sequence 60 08 is loaded into memory at address
		 ** 0x7FFE,F. This is a local branch to location 0x7FF8 (60 is nfix 0,
		 ** which adds '0' to the .O register, complements .O, and then shifts
		 ** it left by 4 bit positions, 08 is a jump .O+8 instruction. This will
		 ** add 8 to .O (which was 0xFFF0), and will branch RELATIVE to the new
		 ** location. Now, the branch starts from the value of .PC (or .IP or
		 ** whatever the bloody register is called on this chip), and the .PC
		 ** will be pointing to the location AFTER the branch, in this case
		 ** .PC == 0x8000, so the branch will be to 0x8000+0xFFF8 = 0x7FF8.
		 **
		 ** A long branch is coded at 0x7FF8. This consists of loading a four
		 ** byte offset into .O using nfix (as above) and pfix operators. The
		 ** pfix operates in exactly the same way as the nfix operator, but
		 ** without the complement operation. The offset, of course, must be
		 ** relative to the address of the byte AFTER the branch instruction,
		 ** which will be (urm) 0x7FFC, so, our final destination of the branch
		 ** (loadbase-2), has to be reached from here. Imagine that the loadbase
		 ** is 0x7C00 (which it is), then we will need to branch to 0x7BFE (which
		 ** is the first byte of the initial two byte short local branch of the
		 ** download code).
		 **
		 ** To code a jump from 0x7FFC (which is where the branch will start
		 ** from) to 0x7BFE, we will need to branch 0xFC02 bytes (0x7FFC+0xFC02)=
		 ** 0x7BFE.
		 ** This will be coded as four bytes:
		 ** 60 2C 20 02
		 ** being nfix .O+0
		 **        pfix .O+C
		 **        pfix .O+0
		 **        jump .O+2
		 **
		 ** The nfix operator is used, so that the startup code will be
		 ** compatible with the whole Tp family. (lies, damn lies, it'll never
		 ** work in a month of Sundays).
		 **
		 ** The nfix nyble is the 1s complement of the nyble value you
		 ** want to load - in this case we wanted 'F' so we nfix loaded '0'.
		 */


		/*
		 ** Dest points to the top 8 bytes of Sram2. The Tp jumps
		 ** to 0x7FFE at reset time, and starts executing. This is
		 ** a short branch to 0x7FF8, where a long branch is coded.
		 */

		DestP = &Cad[0x7FF8];	/* <<<---- READ THE ABOVE COMMENTS */

#define	NFIX(N)	(0x60 | (N))	/* .O  = (~(.O + N))<<4 */
#define	PFIX(N)	(0x20 | (N))	/* .O  =   (.O + N)<<4  */
#define	JUMP(N)	(0x00 | (N))	/* .PC =   .PC + .O      */

		/*
		 ** 0x7FFC is the address of the location following the last byte of
		 ** the four byte jump instruction.
		 ** READ THE ABOVE COMMENTS
		 **
		 ** offset is (TO-FROM) % MEMSIZE, but with compound buggering about.
		 ** Memsize is 64K for this range of Tp, so offset is a short (unsigned,
		 ** cos I don't understand 2's complement).
		 */
		offset = (p->RIOConf.HostLoadBase - 2) - 0x7FFC;

		writeb(NFIX(((unsigned short) (~offset) >> (unsigned short) 12) & 0xF), DestP);
		writeb(PFIX((offset >> 8) & 0xF), DestP + 1);
		writeb(PFIX((offset >> 4) & 0xF), DestP + 2);
		writeb(JUMP(offset & 0xF), DestP + 3);

		writeb(NFIX(0), DestP + 6);
		writeb(JUMP(8), DestP + 7);

		rio_dprintk(RIO_DEBUG_BOOT, "host loadbase is 0x%x\n", p->RIOConf.HostLoadBase);
		rio_dprintk(RIO_DEBUG_BOOT, "startup offset is 0x%x\n", offset);

		/*
		 ** Flag what is going on
		 */
		HostP->Flags &= ~RUN_STATE;
		HostP->Flags |= RC_STARTUP;

		/*
		 ** Grab a copy of the current ParmMap pointer, so we
		 ** can tell when it has changed.
		 */
		OldParmMap = readw(&HostP->__ParmMapR);

		rio_dprintk(RIO_DEBUG_BOOT, "Original parmmap is 0x%x\n", OldParmMap);

		/*
		 ** And start it running (I hope).
		 ** As there is nothing dodgy or obscure about the
		 ** above code, this is guaranteed to work every time.
		 */
		rio_dprintk(RIO_DEBUG_BOOT, "Host Type = 0x%x, Mode = 0x%x, IVec = 0x%x\n", HostP->Type, HostP->Mode, HostP->Ivec);

		rio_start_card_running(HostP);

		rio_dprintk(RIO_DEBUG_BOOT, "Set control port\n");

		/*
		 ** Now, wait for upto five seconds for the Tp to setup the parmmap
		 ** pointer:
		 */
		for (wait_count = 0; (wait_count < p->RIOConf.StartupTime) && (readw(&HostP->__ParmMapR) == OldParmMap); wait_count++) {
			rio_dprintk(RIO_DEBUG_BOOT, "Checkout %d, 0x%x\n", wait_count, readw(&HostP->__ParmMapR));
			mdelay(100);

		}

		/*
		 ** If the parmmap pointer is unchanged, then the host code
		 ** has crashed & burned in a really spectacular way
		 */
		if (readw(&HostP->__ParmMapR) == OldParmMap) {
			rio_dprintk(RIO_DEBUG_BOOT, "parmmap 0x%x\n", readw(&HostP->__ParmMapR));
			rio_dprintk(RIO_DEBUG_BOOT, "RIO Mesg Run Fail\n");
			HostP->Flags &= ~RUN_STATE;
			HostP->Flags |= RC_STUFFED;
			RIOHostReset( HostP->Type, HostP->CardP, HostP->Slot );
			continue;
		}

		rio_dprintk(RIO_DEBUG_BOOT, "Running 0x%x\n", readw(&HostP->__ParmMapR));

		/*
		 ** Well, the board thought it was OK, and setup its parmmap
		 ** pointer. For the time being, we will pretend that this
		 ** board is running, and check out what the error flag says.
		 */

		/*
		 ** Grab a 32 bit pointer to the parmmap structure
		 */
		ParmMapP = (PARM_MAP __iomem *) RIO_PTR(Cad, readw(&HostP->__ParmMapR));
		rio_dprintk(RIO_DEBUG_BOOT, "ParmMapP : %p\n", ParmMapP);
		ParmMapP = (PARM_MAP __iomem *)(Cad + readw(&HostP->__ParmMapR));
		rio_dprintk(RIO_DEBUG_BOOT, "ParmMapP : %p\n", ParmMapP);

		/*
		 ** The links entry should be 0xFFFF; we set it up
		 ** with a mask to say how many PHBs to use, and
		 ** which links to use.
		 */
		if (readw(&ParmMapP->links) != 0xFFFF) {
			rio_dprintk(RIO_DEBUG_BOOT, "RIO Mesg Run Fail %s\n", HostP->Name);
			rio_dprintk(RIO_DEBUG_BOOT, "Links = 0x%x\n", readw(&ParmMapP->links));
			HostP->Flags &= ~RUN_STATE;
			HostP->Flags |= RC_STUFFED;
			RIOHostReset( HostP->Type, HostP->CardP, HostP->Slot );
			continue;
		}

		writew(RIO_LINK_ENABLE, &ParmMapP->links);

		rio_dprintk(RIO_DEBUG_BOOT, "Looking for init_done - %d ticks\n", p->RIOConf.StartupTime);
		HostP->timeout_id = 0;
		for (wait_count = 0; (wait_count < p->RIOConf.StartupTime) && !readw(&ParmMapP->init_done); wait_count++) {
			rio_dprintk(RIO_DEBUG_BOOT, "Waiting for init_done\n");
			mdelay(100);
		}
		rio_dprintk(RIO_DEBUG_BOOT, "OK! init_done!\n");

		if (readw(&ParmMapP->error) != E_NO_ERROR || !readw(&ParmMapP->init_done)) {
			rio_dprintk(RIO_DEBUG_BOOT, "RIO Mesg Run Fail %s\n", HostP->Name);
			rio_dprintk(RIO_DEBUG_BOOT, "Timedout waiting for init_done\n");
			HostP->Flags &= ~RUN_STATE;
			HostP->Flags |= RC_STUFFED;
			RIOHostReset( HostP->Type, HostP->CardP, HostP->Slot );
			continue;
		}

		rio_dprintk(RIO_DEBUG_BOOT, "Got init_done\n");

		/*
		 ** It runs! It runs!
		 */
		rio_dprintk(RIO_DEBUG_BOOT, "Host ID %x Running\n", HostP->UniqueNum);

		/*
		 ** set the time period between interrupts.
		 */
		writew(p->RIOConf.Timer, &ParmMapP->timer);

		/*
		 ** Translate all the 16 bit pointers in the __ParmMapR into
		 ** 32 bit pointers for the driver in ioremap space.
		 */
		HostP->ParmMapP = ParmMapP;
		HostP->PhbP = (struct PHB __iomem *) RIO_PTR(Cad, readw(&ParmMapP->phb_ptr));
		HostP->RupP = (struct RUP __iomem *) RIO_PTR(Cad, readw(&ParmMapP->rups));
		HostP->PhbNumP = (unsigned short __iomem *) RIO_PTR(Cad, readw(&ParmMapP->phb_num_ptr));
		HostP->LinkStrP = (struct LPB __iomem *) RIO_PTR(Cad, readw(&ParmMapP->link_str_ptr));

		/*
		 ** point the UnixRups at the real Rups
		 */
		for (RupN = 0; RupN < MAX_RUP; RupN++) {
			HostP->UnixRups[RupN].RupP = &HostP->RupP[RupN];
			HostP->UnixRups[RupN].Id = RupN + 1;
			HostP->UnixRups[RupN].BaseSysPort = NO_PORT;
			spin_lock_init(&HostP->UnixRups[RupN].RupLock);
		}

		for (RupN = 0; RupN < LINKS_PER_UNIT; RupN++) {
			HostP->UnixRups[RupN + MAX_RUP].RupP = &HostP->LinkStrP[RupN].rup;
			HostP->UnixRups[RupN + MAX_RUP].Id = 0;
			HostP->UnixRups[RupN + MAX_RUP].BaseSysPort = NO_PORT;
			spin_lock_init(&HostP->UnixRups[RupN + MAX_RUP].RupLock);
		}

		/*
		 ** point the PortP->Phbs at the real Phbs
		 */
		for (PortN = p->RIOFirstPortsMapped; PortN < p->RIOLastPortsMapped + PORTS_PER_RTA; PortN++) {
			if (p->RIOPortp[PortN]->HostP == HostP) {
				struct Port *PortP = p->RIOPortp[PortN];
				struct PHB __iomem *PhbP;
				/* int oldspl; */

				if (!PortP->Mapped)
					continue;

				PhbP = &HostP->PhbP[PortP->HostPort];
				rio_spin_lock_irqsave(&PortP->portSem, flags);

				PortP->PhbP = PhbP;

				PortP->TxAdd = (u16 __iomem *) RIO_PTR(Cad, readw(&PhbP->tx_add));
				PortP->TxStart = (u16 __iomem *) RIO_PTR(Cad, readw(&PhbP->tx_start));
				PortP->TxEnd = (u16 __iomem *) RIO_PTR(Cad, readw(&PhbP->tx_end));
				PortP->RxRemove = (u16 __iomem *) RIO_PTR(Cad, readw(&PhbP->rx_remove));
				PortP->RxStart = (u16 __iomem *) RIO_PTR(Cad, readw(&PhbP->rx_start));
				PortP->RxEnd = (u16 __iomem *) RIO_PTR(Cad, readw(&PhbP->rx_end));

				rio_spin_unlock_irqrestore(&PortP->portSem, flags);
				/*
				 ** point the UnixRup at the base SysPort
				 */
				if (!(PortN % PORTS_PER_RTA))
					HostP->UnixRups[PortP->RupNum].BaseSysPort = PortN;
			}
		}

		rio_dprintk(RIO_DEBUG_BOOT, "Set the card running... \n");
		/*
		 ** last thing - show the world that everything is in place
		 */
		HostP->Flags &= ~RUN_STATE;
		HostP->Flags |= RC_RUNNING;
	}
	/*
	 ** MPX always uses a poller. This is actually patched into the system
	 ** configuration and called directly from each clock tick.
	 **
	 */
	p->RIOPolling = 1;

	p->RIOSystemUp++;

	rio_dprintk(RIO_DEBUG_BOOT, "Done everything %x\n", HostP->Ivec);
	func_exit();
	return 0;
}



/**
 *	RIOBootRup		-	Boot an RTA
 *	@p: rio we are working with
 *	@Rup: Rup number
 *	@HostP: host object
 *	@PacketP: packet to use
 *
 *	If we have successfully processed this boot, then
 *	return 1. If we havent, then return 0.
 */

int RIOBootRup(struct rio_info *p, unsigned int Rup, struct Host *HostP, struct PKT __iomem *PacketP)
{
	struct PktCmd __iomem *PktCmdP = (struct PktCmd __iomem *) PacketP->data;
	struct PktCmd_M *PktReplyP;
	struct CmdBlk *CmdBlkP;
	unsigned int sequence;

	/*
	 ** If we haven't been told what to boot, we can't boot it.
	 */
	if (p->RIONumBootPkts == 0) {
		rio_dprintk(RIO_DEBUG_BOOT, "No RTA code to download yet\n");
		return 0;
	}

	/*
	 ** Special case of boot completed - if we get one of these then we
	 ** don't need a command block. For all other cases we do, so handle
	 ** this first and then get a command block, then handle every other
	 ** case, relinquishing the command block if disaster strikes!
	 */
	if ((readb(&PacketP->len) & PKT_CMD_BIT) && (readb(&PktCmdP->Command) == BOOT_COMPLETED))
		return RIOBootComplete(p, HostP, Rup, PktCmdP);

	/*
	 ** Try to allocate a command block. This is in kernel space
	 */
	if (!(CmdBlkP = RIOGetCmdBlk())) {
		rio_dprintk(RIO_DEBUG_BOOT, "No command blocks to boot RTA! come back later.\n");
		return 0;
	}

	/*
	 ** Fill in the default info on the command block
	 */
	CmdBlkP->Packet.dest_unit = Rup < (unsigned short) MAX_RUP ? Rup : 0;
	CmdBlkP->Packet.dest_port = BOOT_RUP;
	CmdBlkP->Packet.src_unit = 0;
	CmdBlkP->Packet.src_port = BOOT_RUP;

	CmdBlkP->PreFuncP = CmdBlkP->PostFuncP = NULL;
	PktReplyP = (struct PktCmd_M *) CmdBlkP->Packet.data;

	/*
	 ** process COMMANDS on the boot rup!
	 */
	if (readb(&PacketP->len) & PKT_CMD_BIT) {
		/*
		 ** We only expect one type of command - a BOOT_REQUEST!
		 */
		if (readb(&PktCmdP->Command) != BOOT_REQUEST) {
			rio_dprintk(RIO_DEBUG_BOOT, "Unexpected command %d on BOOT RUP %d of host %Zd\n", readb(&PktCmdP->Command), Rup, HostP - p->RIOHosts);
			RIOFreeCmdBlk(CmdBlkP);
			return 1;
		}

		/*
		 ** Build a Boot Sequence command block
		 **
		 ** We no longer need to use "Boot Mode", we'll always allow
		 ** boot requests - the boot will not complete if the device
		 ** appears in the bindings table.
		 **
		 ** We'll just (always) set the command field in packet reply
		 ** to allow an attempted boot sequence :
		 */
		PktReplyP->Command = BOOT_SEQUENCE;

		PktReplyP->BootSequence.NumPackets = p->RIONumBootPkts;
		PktReplyP->BootSequence.LoadBase = p->RIOConf.RtaLoadBase;
		PktReplyP->BootSequence.CodeSize = p->RIOBootCount;

		CmdBlkP->Packet.len = BOOT_SEQUENCE_LEN | PKT_CMD_BIT;

		memcpy((void *) &CmdBlkP->Packet.data[BOOT_SEQUENCE_LEN], "BOOT", 4);

		rio_dprintk(RIO_DEBUG_BOOT, "Boot RTA on Host %Zd Rup %d - %d (0x%x) packets to 0x%x\n", HostP - p->RIOHosts, Rup, p->RIONumBootPkts, p->RIONumBootPkts, p->RIOConf.RtaLoadBase);

		/*
		 ** If this host is in slave mode, send the RTA an invalid boot
		 ** sequence command block to force it to kill the boot. We wait
		 ** for half a second before sending this packet to prevent the RTA
		 ** attempting to boot too often. The master host should then grab
		 ** the RTA and make it its own.
		 */
		p->RIOBooting++;
		RIOQueueCmdBlk(HostP, Rup, CmdBlkP);
		return 1;
	}

	/*
	 ** It is a request for boot data.
	 */
	sequence = readw(&PktCmdP->Sequence);

	rio_dprintk(RIO_DEBUG_BOOT, "Boot block %d on Host %Zd Rup%d\n", sequence, HostP - p->RIOHosts, Rup);

	if (sequence >= p->RIONumBootPkts) {
		rio_dprintk(RIO_DEBUG_BOOT, "Got a request for packet %d, max is %d\n", sequence, p->RIONumBootPkts);
	}

	PktReplyP->Sequence = sequence;
	memcpy(PktReplyP->BootData, p->RIOBootPackets[p->RIONumBootPkts - sequence - 1], RTA_BOOT_DATA_SIZE);
	CmdBlkP->Packet.len = PKT_MAX_DATA_LEN;
	RIOQueueCmdBlk(HostP, Rup, CmdBlkP);
	return 1;
}

/**
 *	RIOBootComplete		-	RTA boot is done
 *	@p: RIO we are working with
 *	@HostP: Host structure
 *	@Rup: RUP being used
 *	@PktCmdP: Packet command that was used
 *
 *	This function is called when an RTA been booted.
 *	If booted by a host, HostP->HostUniqueNum is the booting host.
 *	If booted by an RTA, HostP->Mapping[Rup].RtaUniqueNum is the booting RTA.
 *	RtaUniq is the booted RTA.
 */

static int RIOBootComplete(struct rio_info *p, struct Host *HostP, unsigned int Rup, struct PktCmd __iomem *PktCmdP)
{
	struct Map *MapP = NULL;
	struct Map *MapP2 = NULL;
	int Flag;
	int found;
	int host, rta;
	int EmptySlot = -1;
	int entry, entry2;
	char *MyType, *MyName;
	unsigned int MyLink;
	unsigned short RtaType;
	u32 RtaUniq = (readb(&PktCmdP->UniqNum[0])) + (readb(&PktCmdP->UniqNum[1]) << 8) + (readb(&PktCmdP->UniqNum[2]) << 16) + (readb(&PktCmdP->UniqNum[3]) << 24);

	p->RIOBooting = 0;

	rio_dprintk(RIO_DEBUG_BOOT, "RTA Boot completed - BootInProgress now %d\n", p->RIOBooting);

	/*
	 ** Determine type of unit (16/8 port RTA).
	 */

	RtaType = GetUnitType(RtaUniq);
	if (Rup >= (unsigned short) MAX_RUP)
		rio_dprintk(RIO_DEBUG_BOOT, "RIO: Host %s has booted an RTA(%d) on link %c\n", HostP->Name, 8 * RtaType, readb(&PktCmdP->LinkNum) + 'A');
	else
		rio_dprintk(RIO_DEBUG_BOOT, "RIO: RTA %s has booted an RTA(%d) on link %c\n", HostP->Mapping[Rup].Name, 8 * RtaType, readb(&PktCmdP->LinkNum) + 'A');

	rio_dprintk(RIO_DEBUG_BOOT, "UniqNum is 0x%x\n", RtaUniq);

	if (RtaUniq == 0x00000000 || RtaUniq == 0xffffffff) {
		rio_dprintk(RIO_DEBUG_BOOT, "Illegal RTA Uniq Number\n");
		return 1;
	}

	/*
	 ** If this RTA has just booted an RTA which doesn't belong to this
	 ** system, or the system is in slave mode, do not attempt to create
	 ** a new table entry for it.
	 */

	if (!RIOBootOk(p, HostP, RtaUniq)) {
		MyLink = readb(&PktCmdP->LinkNum);
		if (Rup < (unsigned short) MAX_RUP) {
			/*
			 ** RtaUniq was clone booted (by this RTA). Instruct this RTA
			 ** to hold off further attempts to boot on this link for 30
			 ** seconds.
			 */
			if (RIOSuspendBootRta(HostP, HostP->Mapping[Rup].ID, MyLink)) {
				rio_dprintk(RIO_DEBUG_BOOT, "RTA failed to suspend booting on link %c\n", 'A' + MyLink);
			}
		} else
			/*
			 ** RtaUniq was booted by this host. Set the booting link
			 ** to hold off for 30 seconds to give another unit a
			 ** chance to boot it.
			 */
			writew(30, &HostP->LinkStrP[MyLink].WaitNoBoot);
		rio_dprintk(RIO_DEBUG_BOOT, "RTA %x not owned - suspend booting down link %c on unit %x\n", RtaUniq, 'A' + MyLink, HostP->Mapping[Rup].RtaUniqueNum);
		return 1;
	}

	/*
	 ** Check for a SLOT_IN_USE entry for this RTA attached to the
	 ** current host card in the driver table.
	 **
	 ** If it exists, make a note that we have booted it. Other parts of
	 ** the driver are interested in this information at a later date,
	 ** in particular when the booting RTA asks for an ID for this unit,
	 ** we must have set the BOOTED flag, and the NEWBOOT flag is used
	 ** to force an open on any ports that where previously open on this
	 ** unit.
	 */
	for (entry = 0; entry < MAX_RUP; entry++) {
		unsigned int sysport;

		if ((HostP->Mapping[entry].Flags & SLOT_IN_USE) && (HostP->Mapping[entry].RtaUniqueNum == RtaUniq)) {
			HostP->Mapping[entry].Flags |= RTA_BOOTED | RTA_NEWBOOT;
			if ((sysport = HostP->Mapping[entry].SysPort) != NO_PORT) {
				if (sysport < p->RIOFirstPortsBooted)
					p->RIOFirstPortsBooted = sysport;
				if (sysport > p->RIOLastPortsBooted)
					p->RIOLastPortsBooted = sysport;
				/*
				 ** For a 16 port RTA, check the second bank of 8 ports
				 */
				if (RtaType == TYPE_RTA16) {
					entry2 = HostP->Mapping[entry].ID2 - 1;
					HostP->Mapping[entry2].Flags |= RTA_BOOTED | RTA_NEWBOOT;
					sysport = HostP->Mapping[entry2].SysPort;
					if (sysport < p->RIOFirstPortsBooted)
						p->RIOFirstPortsBooted = sysport;
					if (sysport > p->RIOLastPortsBooted)
						p->RIOLastPortsBooted = sysport;
				}
			}
			if (RtaType == TYPE_RTA16)
				rio_dprintk(RIO_DEBUG_BOOT, "RTA will be given IDs %d+%d\n", entry + 1, entry2 + 1);
			else
				rio_dprintk(RIO_DEBUG_BOOT, "RTA will be given ID %d\n", entry + 1);
			return 1;
		}
	}

	rio_dprintk(RIO_DEBUG_BOOT, "RTA not configured for this host\n");

	if (Rup >= (unsigned short) MAX_RUP) {
		/*
		 ** It was a host that did the booting
		 */
		MyType = "Host";
		MyName = HostP->Name;
	} else {
		/*
		 ** It was an RTA that did the booting
		 */
		MyType = "RTA";
		MyName = HostP->Mapping[Rup].Name;
	}
	MyLink = readb(&PktCmdP->LinkNum);

	/*
	 ** There is no SLOT_IN_USE entry for this RTA attached to the current
	 ** host card in the driver table.
	 **
	 ** Check for a SLOT_TENTATIVE entry for this RTA attached to the
	 ** current host card in the driver table.
	 **
	 ** If we find one, then we re-use that slot.
	 */
	for (entry = 0; entry < MAX_RUP; entry++) {
		if ((HostP->Mapping[entry].Flags & SLOT_TENTATIVE) && (HostP->Mapping[entry].RtaUniqueNum == RtaUniq)) {
			if (RtaType == TYPE_RTA16) {
				entry2 = HostP->Mapping[entry].ID2 - 1;
				if ((HostP->Mapping[entry2].Flags & SLOT_TENTATIVE) && (HostP->Mapping[entry2].RtaUniqueNum == RtaUniq))
					rio_dprintk(RIO_DEBUG_BOOT, "Found previous tentative slots (%d+%d)\n", entry, entry2);
				else
					continue;
			} else
				rio_dprintk(RIO_DEBUG_BOOT, "Found previous tentative slot (%d)\n", entry);
			if (!p->RIONoMessage)
				printk("RTA connected to %s '%s' (%c) not configured.\n", MyType, MyName, MyLink + 'A');
			return 1;
		}
	}

	/*
	 ** There is no SLOT_IN_USE or SLOT_TENTATIVE entry for this RTA
	 ** attached to the current host card in the driver table.
	 **
	 ** Check if there is a SLOT_IN_USE or SLOT_TENTATIVE entry on another
	 ** host for this RTA in the driver table.
	 **
	 ** For a SLOT_IN_USE entry on another host, we need to delete the RTA
	 ** entry from the other host and add it to this host (using some of
	 ** the functions from table.c which do this).
	 ** For a SLOT_TENTATIVE entry on another host, we must cope with the
	 ** following scenario:
	 **
	 ** + Plug 8 port RTA into host A. (This creates SLOT_TENTATIVE entry
	 **   in table)
	 ** + Unplug RTA and plug into host B. (We now have 2 SLOT_TENTATIVE
	 **   entries)
	 ** + Configure RTA on host B. (This slot now becomes SLOT_IN_USE)
	 ** + Unplug RTA and plug back into host A.
	 ** + Configure RTA on host A. We now have the same RTA configured
	 **   with different ports on two different hosts.
	 */
	rio_dprintk(RIO_DEBUG_BOOT, "Have we seen RTA %x before?\n", RtaUniq);
	found = 0;
	Flag = 0;		/* Convince the compiler this variable is initialized */
	for (host = 0; !found && (host < p->RIONumHosts); host++) {
		for (rta = 0; rta < MAX_RUP; rta++) {
			if ((p->RIOHosts[host].Mapping[rta].Flags & (SLOT_IN_USE | SLOT_TENTATIVE)) && (p->RIOHosts[host].Mapping[rta].RtaUniqueNum == RtaUniq)) {
				Flag = p->RIOHosts[host].Mapping[rta].Flags;
				MapP = &p->RIOHosts[host].Mapping[rta];
				if (RtaType == TYPE_RTA16) {
					MapP2 = &p->RIOHosts[host].Mapping[MapP->ID2 - 1];
					rio_dprintk(RIO_DEBUG_BOOT, "This RTA is units %d+%d from host %s\n", rta + 1, MapP->ID2, p->RIOHosts[host].Name);
				} else
					rio_dprintk(RIO_DEBUG_BOOT, "This RTA is unit %d from host %s\n", rta + 1, p->RIOHosts[host].Name);
				found = 1;
				break;
			}
		}
	}

	/*
	 ** There is no SLOT_IN_USE or SLOT_TENTATIVE entry for this RTA
	 ** attached to the current host card in the driver table.
	 **
	 ** If we have not found a SLOT_IN_USE or SLOT_TENTATIVE entry on
	 ** another host for this RTA in the driver table...
	 **
	 ** Check for a SLOT_IN_USE entry for this RTA in the config table.
	 */
	if (!MapP) {
		rio_dprintk(RIO_DEBUG_BOOT, "Look for RTA %x in RIOSavedTable\n", RtaUniq);
		for (rta = 0; rta < TOTAL_MAP_ENTRIES; rta++) {
			rio_dprintk(RIO_DEBUG_BOOT, "Check table entry %d (%x)", rta, p->RIOSavedTable[rta].RtaUniqueNum);

			if ((p->RIOSavedTable[rta].Flags & SLOT_IN_USE) && (p->RIOSavedTable[rta].RtaUniqueNum == RtaUniq)) {
				MapP = &p->RIOSavedTable[rta];
				Flag = p->RIOSavedTable[rta].Flags;
				if (RtaType == TYPE_RTA16) {
					for (entry2 = rta + 1; entry2 < TOTAL_MAP_ENTRIES; entry2++) {
						if (p->RIOSavedTable[entry2].RtaUniqueNum == RtaUniq)
							break;
					}
					MapP2 = &p->RIOSavedTable[entry2];
					rio_dprintk(RIO_DEBUG_BOOT, "This RTA is from table entries %d+%d\n", rta, entry2);
				} else
					rio_dprintk(RIO_DEBUG_BOOT, "This RTA is from table entry %d\n", rta);
				break;
			}
		}
	}

	/*
	 ** There is no SLOT_IN_USE or SLOT_TENTATIVE entry for this RTA
	 ** attached to the current host card in the driver table.
	 **
	 ** We may have found a SLOT_IN_USE entry on another host for this
	 ** RTA in the config table, or a SLOT_IN_USE or SLOT_TENTATIVE entry
	 ** on another host for this RTA in the driver table.
	 **
	 ** Check the driver table for room to fit this newly discovered RTA.
	 ** RIOFindFreeID() first looks for free slots and if it does not
	 ** find any free slots it will then attempt to oust any
	 ** tentative entry in the table.
	 */
	EmptySlot = 1;
	if (RtaType == TYPE_RTA16) {
		if (RIOFindFreeID(p, HostP, &entry, &entry2) == 0) {
			RIODefaultName(p, HostP, entry);
			rio_fill_host_slot(entry, entry2, RtaUniq, HostP);
			EmptySlot = 0;
		}
	} else {
		if (RIOFindFreeID(p, HostP, &entry, NULL) == 0) {
			RIODefaultName(p, HostP, entry);
			rio_fill_host_slot(entry, 0, RtaUniq, HostP);
			EmptySlot = 0;
		}
	}

	/*
	 ** There is no SLOT_IN_USE or SLOT_TENTATIVE entry for this RTA
	 ** attached to the current host card in the driver table.
	 **
	 ** If we found a SLOT_IN_USE entry on another host for this
	 ** RTA in the config or driver table, and there are enough free
	 ** slots in the driver table, then we need to move it over and
	 ** delete it from the other host.
	 ** If we found a SLOT_TENTATIVE entry on another host for this
	 ** RTA in the driver table, just delete the other host entry.
	 */
	if (EmptySlot == 0) {
		if (MapP) {
			if (Flag & SLOT_IN_USE) {
				rio_dprintk(RIO_DEBUG_BOOT, "This RTA configured on another host - move entry to current host (1)\n");
				HostP->Mapping[entry].SysPort = MapP->SysPort;
				memcpy(HostP->Mapping[entry].Name, MapP->Name, MAX_NAME_LEN);
				HostP->Mapping[entry].Flags = SLOT_IN_USE | RTA_BOOTED | RTA_NEWBOOT;
				RIOReMapPorts(p, HostP, &HostP->Mapping[entry]);
				if (HostP->Mapping[entry].SysPort < p->RIOFirstPortsBooted)
					p->RIOFirstPortsBooted = HostP->Mapping[entry].SysPort;
				if (HostP->Mapping[entry].SysPort > p->RIOLastPortsBooted)
					p->RIOLastPortsBooted = HostP->Mapping[entry].SysPort;
				rio_dprintk(RIO_DEBUG_BOOT, "SysPort %d, Name %s\n", (int) MapP->SysPort, MapP->Name);
			} else {
				rio_dprintk(RIO_DEBUG_BOOT, "This RTA has a tentative entry on another host - delete that entry (1)\n");
				HostP->Mapping[entry].Flags = SLOT_TENTATIVE | RTA_BOOTED | RTA_NEWBOOT;
			}
			if (RtaType == TYPE_RTA16) {
				if (Flag & SLOT_IN_USE) {
					HostP->Mapping[entry2].Flags = SLOT_IN_USE | RTA_BOOTED | RTA_NEWBOOT | RTA16_SECOND_SLOT;
					HostP->Mapping[entry2].SysPort = MapP2->SysPort;
					/*
					 ** Map second block of ttys for 16 port RTA
					 */
					RIOReMapPorts(p, HostP, &HostP->Mapping[entry2]);
					if (HostP->Mapping[entry2].SysPort < p->RIOFirstPortsBooted)
						p->RIOFirstPortsBooted = HostP->Mapping[entry2].SysPort;
					if (HostP->Mapping[entry2].SysPort > p->RIOLastPortsBooted)
						p->RIOLastPortsBooted = HostP->Mapping[entry2].SysPort;
					rio_dprintk(RIO_DEBUG_BOOT, "SysPort %d, Name %s\n", (int) HostP->Mapping[entry2].SysPort, HostP->Mapping[entry].Name);
				} else
					HostP->Mapping[entry2].Flags = SLOT_TENTATIVE | RTA_BOOTED | RTA_NEWBOOT | RTA16_SECOND_SLOT;
				memset(MapP2, 0, sizeof(struct Map));
			}
			memset(MapP, 0, sizeof(struct Map));
			if (!p->RIONoMessage)
				printk("An orphaned RTA has been adopted by %s '%s' (%c).\n", MyType, MyName, MyLink + 'A');
		} else if (!p->RIONoMessage)
			printk("RTA connected to %s '%s' (%c) not configured.\n", MyType, MyName, MyLink + 'A');
		RIOSetChange(p);
		return 1;
	}

	/*
	 ** There is no room in the driver table to make an entry for the
	 ** booted RTA. Keep a note of its Uniq Num in the overflow table,
	 ** so we can ignore it's ID requests.
	 */
	if (!p->RIONoMessage)
		printk("The RTA connected to %s '%s' (%c) cannot be configured.  You cannot configure more than 128 ports to one host card.\n", MyType, MyName, MyLink + 'A');
	for (entry = 0; entry < HostP->NumExtraBooted; entry++) {
		if (HostP->ExtraUnits[entry] == RtaUniq) {
			/*
			 ** already got it!
			 */
			return 1;
		}
	}
	/*
	 ** If there is room, add the unit to the list of extras
	 */
	if (HostP->NumExtraBooted < MAX_EXTRA_UNITS)
		HostP->ExtraUnits[HostP->NumExtraBooted++] = RtaUniq;
	return 1;
}


/*
** If the RTA or its host appears in the RIOBindTab[] structure then
** we mustn't boot the RTA and should return 0.
** This operation is slightly different from the other drivers for RIO
** in that this is designed to work with the new utilities
** not config.rio and is FAR SIMPLER.
** We no longer support the RIOBootMode variable. It is all done from the
** "boot/noboot" field in the rio.cf file.
*/
int RIOBootOk(struct rio_info *p, struct Host *HostP, unsigned long RtaUniq)
{
	int Entry;
	unsigned int HostUniq = HostP->UniqueNum;

	/*
	 ** Search bindings table for RTA or its parent.
	 ** If it exists, return 0, else 1.
	 */
	for (Entry = 0; (Entry < MAX_RTA_BINDINGS) && (p->RIOBindTab[Entry] != 0); Entry++) {
		if ((p->RIOBindTab[Entry] == HostUniq) || (p->RIOBindTab[Entry] == RtaUniq))
			return 0;
	}
	return 1;
}

/*
** Make an empty slot tentative. If this is a 16 port RTA, make both
** slots tentative, and the second one RTA_SECOND_SLOT as well.
*/

void rio_fill_host_slot(int entry, int entry2, unsigned int rta_uniq, struct Host *host)
{
	int link;

	rio_dprintk(RIO_DEBUG_BOOT, "rio_fill_host_slot(%d, %d, 0x%x...)\n", entry, entry2, rta_uniq);

	host->Mapping[entry].Flags = (RTA_BOOTED | RTA_NEWBOOT | SLOT_TENTATIVE);
	host->Mapping[entry].SysPort = NO_PORT;
	host->Mapping[entry].RtaUniqueNum = rta_uniq;
	host->Mapping[entry].HostUniqueNum = host->UniqueNum;
	host->Mapping[entry].ID = entry + 1;
	host->Mapping[entry].ID2 = 0;
	if (entry2) {
		host->Mapping[entry2].Flags = (RTA_BOOTED | RTA_NEWBOOT | SLOT_TENTATIVE | RTA16_SECOND_SLOT);
		host->Mapping[entry2].SysPort = NO_PORT;
		host->Mapping[entry2].RtaUniqueNum = rta_uniq;
		host->Mapping[entry2].HostUniqueNum = host->UniqueNum;
		host->Mapping[entry2].Name[0] = '\0';
		host->Mapping[entry2].ID = entry2 + 1;
		host->Mapping[entry2].ID2 = entry + 1;
		host->Mapping[entry].ID2 = entry2 + 1;
	}
	/*
	 ** Must set these up, so that utilities show
	 ** topology of 16 port RTAs correctly
	 */
	for (link = 0; link < LINKS_PER_UNIT; link++) {
		host->Mapping[entry].Topology[link].Unit = ROUTE_DISCONNECT;
		host->Mapping[entry].Topology[link].Link = NO_LINK;
		if (entry2) {
			host->Mapping[entry2].Topology[link].Unit = ROUTE_DISCONNECT;
			host->Mapping[entry2].Topology[link].Link = NO_LINK;
		}
	}
}
