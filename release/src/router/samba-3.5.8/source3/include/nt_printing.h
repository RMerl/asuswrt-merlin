/*
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell              1992-2000,
   Copyright (C) Jean Francois Micouleau      1998-2000.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NT_PRINTING_H_
#define NT_PRINTING_H_

/* container for a single registry key */

typedef struct {
	char			*name;
	struct regval_ctr	*values;
} NT_PRINTER_KEY;

/* container for all printer data */

typedef struct {
	int		num_keys;
	NT_PRINTER_KEY	*keys;
} NT_PRINTER_DATA;

typedef struct ntdevicemode
{
	fstring	devicename;
	fstring	formname;

	uint16	specversion;
	uint16	driverversion;
	uint16	size;
	uint16	driverextra;
	uint16	orientation;
	uint16	papersize;
	uint16	paperlength;
	uint16	paperwidth;
	uint16	scale;
	uint16	copies;
	uint16	defaultsource;
	uint16	printquality;
	uint16	color;
	uint16	duplex;
	uint16	yresolution;
	uint16	ttoption;
	uint16	collate;
	uint16	logpixels;

	uint32	fields;
	uint32	bitsperpel;
	uint32	pelswidth;
	uint32	pelsheight;
	uint32	displayflags;
	uint32	displayfrequency;
	uint32	icmmethod;
	uint32	icmintent;
	uint32	mediatype;
	uint32	dithertype;
	uint32	reserved1;
	uint32	reserved2;
	uint32	panningwidth;
	uint32	panningheight;
	uint8 	*nt_dev_private;
} NT_DEVICEMODE;

typedef struct nt_printer_info_level_2
{
	uint32 attributes;
	uint32 priority;
	uint32 default_priority;
	uint32 starttime;
	uint32 untiltime;
	uint32 status;
	uint32 cjobs;
	uint32 averageppm;
	fstring servername;
	fstring printername;
	fstring sharename;
	fstring portname;
	fstring drivername;
	char comment[1024];
	fstring location;
	NT_DEVICEMODE *devmode;
	fstring sepfile;
	fstring printprocessor;
	fstring datatype;
	fstring parameters;
	NT_PRINTER_DATA *data;
	SEC_DESC_BUF *secdesc_buf;
	uint32 changeid;
	uint32 c_setprinter;
	uint32 setuptime;	
} NT_PRINTER_INFO_LEVEL_2;

typedef struct nt_printer_info_level
{
	NT_PRINTER_INFO_LEVEL_2 *info_2;
} NT_PRINTER_INFO_LEVEL;

typedef struct
{
	fstring name;
	uint32 flag;
	uint32 width;
	uint32 length;
	uint32 left;
	uint32 top;
	uint32 right;
	uint32 bottom;
} nt_forms_struct;

#ifndef SAMBA_PRINTER_PORT_NAME
#define SAMBA_PRINTER_PORT_NAME "Samba Printer Port"
#endif

/* DOS header format */
#define DOS_HEADER_SIZE                 64
#define DOS_HEADER_MAGIC_OFFSET         0
#define DOS_HEADER_MAGIC                0x5A4D
#define DOS_HEADER_LFANEW_OFFSET        60

/* New Executable format (Win or OS/2 1.x segmented) */
#define NE_HEADER_SIZE                  64
#define NE_HEADER_SIGNATURE_OFFSET      0
#define NE_HEADER_SIGNATURE             0x454E
#define NE_HEADER_TARGET_OS_OFFSET      54
#define NE_HEADER_TARGOS_WIN            0x02
#define NE_HEADER_MINOR_VER_OFFSET      62
#define NE_HEADER_MAJOR_VER_OFFSET      63

/* Portable Executable format */
#define PE_HEADER_SIZE                  24
#define PE_HEADER_SIGNATURE_OFFSET      0
#define PE_HEADER_SIGNATURE             0x00004550
#define PE_HEADER_MACHINE_OFFSET        4
#define PE_HEADER_MACHINE_I386          0x14c
#define PE_HEADER_NUMBER_OF_SECTIONS    6
#define PE_HEADER_OPTIONAL_HEADER_SIZE  20
#define PE_HEADER_SECT_HEADER_SIZE      40
#define PE_HEADER_SECT_NAME_OFFSET      0
#define PE_HEADER_SECT_SIZE_DATA_OFFSET 16
#define PE_HEADER_SECT_PTR_DATA_OFFSET  20

/* Microsoft file version format */
#define VS_SIGNATURE                    "VS_VERSION_INFO"
#define VS_MAGIC_VALUE                  0xfeef04bd
#define VS_MAJOR_OFFSET					8
#define VS_MINOR_OFFSET					12
#define VS_VERSION_INFO_UNICODE_SIZE    (sizeof(VS_SIGNATURE)*2+4+VS_MINOR_OFFSET+4) /* not true size! */
#define VS_VERSION_INFO_SIZE            (sizeof(VS_SIGNATURE)+4+VS_MINOR_OFFSET+4)   /* not true size! */
#define VS_NE_BUF_SIZE                  4096  /* Must be > 2*VS_VERSION_INFO_SIZE */

/* Notify spoolss clients that something has changed.  The
   notification data is either stored in two uint32 values or a
   variable length array. */

#define SPOOLSS_NOTIFY_MSG_UNIX_JOBID 0x0001    /* Job id is unix  */

typedef struct spoolss_notify_msg {
	fstring printer;	/* Name of printer notified */
	uint32 type;		/* Printer or job notify */
	uint32 field;		/* Notify field changed */
	uint32 id;		/* Job id */
	uint32 len;		/* Length of data, 0 for two uint32 value */
	uint32 flags;
	union {
		uint32 value[2];
		char *data;
	} notify;
} SPOOLSS_NOTIFY_MSG;

typedef struct {
	fstring 		printername;
	uint32			num_msgs;
	SPOOLSS_NOTIFY_MSG	*msgs;
} SPOOLSS_NOTIFY_MSG_GROUP;

typedef struct {
	TALLOC_CTX 			*ctx;
	uint32				num_groups;
	SPOOLSS_NOTIFY_MSG_GROUP	*msg_groups;
} SPOOLSS_NOTIFY_MSG_CTR;

#define SPLHND_PRINTER		1
#define SPLHND_SERVER	 	2
#define SPLHND_PORTMON_TCP	3
#define SPLHND_PORTMON_LOCAL	4

/* structure to store the printer handles */
/* and a reference to what it's pointing to */
/* and the notify info asked about */
/* that's the central struct */
typedef struct _Printer{
	struct _Printer *prev, *next;
	bool document_started;
	bool page_started;
	uint32 jobid; /* jobid in printing backend */
	int printer_type;
	fstring servername;
	fstring sharename;
	uint32 type;
	uint32 access_granted;
	struct {
		uint32 flags;
		uint32 options;
		fstring localmachine;
		uint32 printerlocal;
		struct spoolss_NotifyOption *option;
		struct policy_handle client_hnd;
		bool client_connected;
		uint32 change;
		/* are we in a FindNextPrinterChangeNotify() call? */
		bool fnpcn;
	} notify;
	struct {
		fstring machine;
		fstring user;
	} client;
	
	/* devmode sent in the OpenPrinter() call */
	NT_DEVICEMODE	*nt_devmode;
	
	/* cache the printer info */
	NT_PRINTER_INFO_LEVEL *printer_info;
	
} Printer_entry;

/*
 * The printer attributes.
 * I #defined all of them (grabbed form MSDN)
 * I'm only using:
 * ( SHARED | NETWORK | RAW_ONLY )
 * RAW_ONLY _MUST_ be present otherwise NT will send an EMF file
 */

#define PRINTER_ATTRIBUTE_SAMBA			(PRINTER_ATTRIBUTE_RAW_ONLY|\
						 PRINTER_ATTRIBUTE_SHARED|\
						 PRINTER_ATTRIBUTE_LOCAL)
#define PRINTER_ATTRIBUTE_NOT_SAMBA		(PRINTER_ATTRIBUTE_NETWORK)

#define DRIVER_ANY_VERSION		0xffffffff
#define DRIVER_MAX_VERSION		4

struct print_architecture_table_node {
	const char 	*long_archi;
	const char 	*short_archi;
	int	version;
};

#endif /* NT_PRINTING_H_ */
