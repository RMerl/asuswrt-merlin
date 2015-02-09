/****************************************************************************
 *******                                                              *******
 *******            P A C K E T   H E A D E R   F I L E
 *******                                                              *******
 ****************************************************************************

 Author  : Ian Nandhra / Jeremy Rolls
 Date    :

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

 Version : 0.01


                            Mods
 ----------------------------------------------------------------------------
  Date     By                Description
 ----------------------------------------------------------------------------

 ***************************************************************************/

#ifndef _pkt_h
#define _pkt_h 1

#define PKT_CMD_BIT     ((ushort) 0x080)
#define PKT_CMD_DATA    ((ushort) 0x080)

#define PKT_ACK         ((ushort) 0x040)

#define PKT_TGL         ((ushort) 0x020)

#define PKT_LEN_MASK    ((ushort) 0x07f)

#define DATA_WNDW       ((ushort) 0x10)
#define PKT_TTL_MASK    ((ushort) 0x0f)

#define PKT_MAX_DATA_LEN   72

#define PKT_LENGTH         sizeof(struct PKT)
#define SYNC_PKT_LENGTH    (PKT_LENGTH + 4)

#define CONTROL_PKT_LEN_MASK PKT_LEN_MASK
#define CONTROL_PKT_CMD_BIT  PKT_CMD_BIT
#define CONTROL_PKT_ACK (PKT_ACK << 8)
#define CONTROL_PKT_TGL (PKT_TGL << 8)
#define CONTROL_PKT_TTL_MASK (PKT_TTL_MASK << 8)
#define CONTROL_DATA_WNDW  (DATA_WNDW << 8)

struct PKT {
	u8 dest_unit;		/* Destination Unit Id */
	u8 dest_port;		/* Destination POrt */
	u8 src_unit;		/* Source Unit Id */
	u8 src_port;		/* Source POrt */
	u8 len;
	u8 control;
	u8 data[PKT_MAX_DATA_LEN];
	/* Actual data :-) */
	u16 csum;		/* C-SUM */
};
#endif

/*********** end of file ***********/
