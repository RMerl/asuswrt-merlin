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
**	Module		: cmdpkt.h
**	SID		: 1.2
**	Last Modified	: 11/6/98 11:34:09
**	Retrieved	: 11/6/98 11:34:20
**
**  ident @(#)cmdpkt.h	1.2
**
** -----------------------------------------------------------------------------
*/
#ifndef __rio_cmdpkt_h__
#define __rio_cmdpkt_h__

/*
** overlays for the data area of a packet. Used in both directions
** (to build a packet to send, and to interpret a packet that arrives)
** and is very inconvenient for MIPS, so they appear as two separate
** structures - those used for modifying/reading packets on the card
** and those for modifying/reading packets in real memory, which have an _M
** suffix.
*/

#define	RTA_BOOT_DATA_SIZE (PKT_MAX_DATA_LEN-2)

/*
** The boot information packet looks like this:
** This structure overlays a PktCmd->CmdData structure, and so starts
** at Data[2] in the actual pkt!
*/
struct BootSequence {
	u16 NumPackets;
	u16 LoadBase;
	u16 CodeSize;
};

#define	BOOT_SEQUENCE_LEN	8

struct SamTop {
	u8 Unit;
	u8 Link;
};

struct CmdHdr {
	u8 PcCommand;
	union {
		u8 PcPhbNum;
		u8 PcLinkNum;
		u8 PcIDNum;
	} U0;
};


struct PktCmd {
	union {
		struct {
			struct CmdHdr CmdHdr;
			struct BootSequence PcBootSequence;
		} S1;
		struct {
			u16 PcSequence;
			u8 PcBootData[RTA_BOOT_DATA_SIZE];
		} S2;
		struct {
			u16 __crud__;
			u8 PcUniqNum[4];	/* this is really a uint. */
			u8 PcModuleTypes;	/* what modules are fitted */
		} S3;
		struct {
			struct CmdHdr CmdHdr;
			u8 __undefined__;
			u8 PcModemStatus;
			u8 PcPortStatus;
			u8 PcSubCommand;	/* commands like mem or register dump */
			u16 PcSubAddr;	/* Address for command */
			u8 PcSubData[64];	/* Date area for command */
		} S4;
		struct {
			struct CmdHdr CmdHdr;
			u8 PcCommandText[1];
			u8 __crud__[20];
			u8 PcIDNum2;	/* It had to go somewhere! */
		} S5;
		struct {
			struct CmdHdr CmdHdr;
			struct SamTop Topology[LINKS_PER_UNIT];
		} S6;
	} U1;
};

struct PktCmd_M {
	union {
		struct {
			struct {
				u8 PcCommand;
				union {
					u8 PcPhbNum;
					u8 PcLinkNum;
					u8 PcIDNum;
				} U0;
			} CmdHdr;
			struct {
				u16 NumPackets;
				u16 LoadBase;
				u16 CodeSize;
			} PcBootSequence;
		} S1;
		struct {
			u16 PcSequence;
			u8 PcBootData[RTA_BOOT_DATA_SIZE];
		} S2;
		struct {
			u16 __crud__;
			u8 PcUniqNum[4];	/* this is really a uint. */
			u8 PcModuleTypes;	/* what modules are fitted */
		} S3;
		struct {
			u16 __cmd_hdr__;
			u8 __undefined__;
			u8 PcModemStatus;
			u8 PcPortStatus;
			u8 PcSubCommand;
			u16 PcSubAddr;
			u8 PcSubData[64];
		} S4;
		struct {
			u16 __cmd_hdr__;
			u8 PcCommandText[1];
			u8 __crud__[20];
			u8 PcIDNum2;	/* Tacked on end */
		} S5;
		struct {
			u16 __cmd_hdr__;
			struct Top Topology[LINKS_PER_UNIT];
		} S6;
	} U1;
};

#define Command		U1.S1.CmdHdr.PcCommand
#define PhbNum		U1.S1.CmdHdr.U0.PcPhbNum
#define IDNum		U1.S1.CmdHdr.U0.PcIDNum
#define IDNum2		U1.S5.PcIDNum2
#define LinkNum		U1.S1.CmdHdr.U0.PcLinkNum
#define Sequence	U1.S2.PcSequence
#define BootData	U1.S2.PcBootData
#define BootSequence	U1.S1.PcBootSequence
#define UniqNum		U1.S3.PcUniqNum
#define ModemStatus	U1.S4.PcModemStatus
#define PortStatus	U1.S4.PcPortStatus
#define SubCommand	U1.S4.PcSubCommand
#define SubAddr		U1.S4.PcSubAddr
#define SubData		U1.S4.PcSubData
#define CommandText	U1.S5.PcCommandText
#define RouteTopology	U1.S6.Topology
#define ModuleTypes	U1.S3.PcModuleTypes

#endif
