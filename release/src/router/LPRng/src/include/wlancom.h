/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __WLAN_COMMON__
#define __WLAN_COMMON__


/****************************************/
/*              FOR LINUX               */
/****************************************/
#ifndef  WIN32
#define ULONG   unsigned long
#define DWORD   unsigned long
#define BYTE    char
#define PBYTE   char *
#define WORD    unsigned short
#define INT     int
#endif //#ifndef  WIN32


//Define Error Code

/************ Use Internally in Program************/
#define	ERR_SUCCESS				0
#define	ERR_SOCKET				-1
#define	ERR_CONNFAILED			-2
#define	ERR_SERVERBUSY			-3
/************ Use Internally in Program************/

#define	ERR_BASE				100
#define	ERR_SERVER_OCCUPIED		ERR_BASE + 1
#define	ERR_SERVER_LPT_FAIL		ERR_BASE + 2
#define	ERR_SERVER_LPT_INTR		ERR_BASE + 3
#define	ERR_SERVER_LPT_BUSY		ERR_BASE + 4
#define	ERR_SERVER_LPT_OUTP		ERR_BASE + 5
#define	ERR_SERVER_LPT_OFFL		ERR_BASE + 6

//Use For Network Communication Protocol

//Packet Type Section
#define NET_SERVICE_ID_BASE	10
#define NET_SERVICE_ID_LPT_EMU	NET_SERVICE_ID_BASE + 1


//Packet Type Section
#define NET_PACKET_TYPE_BASE	20
#define NET_PACKET_TYPE_CMD	NET_PACKET_TYPE_BASE + 1
#define NET_PACKET_TYPE_RES	NET_PACKET_TYPE_BASE + 2
#define NET_PACKET_TYPE_DATA	NET_PACKET_TYPE_BASE + 3


//Command ID Section
#define NET_CMD_ID_BASE		30
#define NET_CMD_ID_OPEN		NET_CMD_ID_BASE + 1
#define NET_CMD_ID_CLOSE	NET_CMD_ID_BASE + 2
#define NET_CMD_ID_DATA_RES	NET_CMD_ID_BASE + 3
#define NET_CMD_ID_READ		NET_CMD_ID_BASE + 4


//Command Packet Header Structure

typedef struct lptCmdPKT
{
	BYTE		ServiceID;
	BYTE		PacketType;
	BYTE		CommandID;
	BYTE		ParaLength;
} LPT_CMD_PKT_HDR;


//Response Packet Header Structure

typedef struct lptResPKT
{
	BYTE		ServiceID;
	BYTE		PacketType;
	BYTE		CommandID;
	BYTE		ResLength;
} LPT_RES_PKT_HDR;




//Data Packet Header Structure

typedef struct lptDataPKT
{
	BYTE		ServiceID;
	BYTE		PacketType;
	WORD		DataLength;
} LPT_DATA_PKT_HDR;

typedef struct paraRes
{
	INT			msgid;
}ParaRes;

#endif

