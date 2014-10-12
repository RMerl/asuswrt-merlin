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

#include "client.h"
#include "../librpc/gen_ndr/spoolss.h"

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

bool nt_printing_init(struct messaging_context *msg_ctx);

const char *get_short_archi(const char *long_archi);

bool print_access_check(const struct auth_serversupplied_info *server_info,
			struct messaging_context *msg_ctx, int snum,
			int access_type);

WERROR nt_printer_guid_get(TALLOC_CTX *mem_ctx,
			   const struct auth_serversupplied_info *server_info,
			   struct messaging_context *msg_ctx,
			   const char *printer, struct GUID *guid);

WERROR nt_printer_publish(TALLOC_CTX *mem_ctx,
			  const struct auth_serversupplied_info *server_info,
			  struct messaging_context *msg_ctx,
			  struct spoolss_PrinterInfo2 *pinfo2,
			  int action);

bool is_printer_published(TALLOC_CTX *mem_ctx,
			  const struct auth_serversupplied_info *server_info,
			  struct messaging_context *msg_ctx,
			  const char *servername,
			  const char *printer,
			  struct spoolss_PrinterInfo2 **info2);

WERROR check_published_printers(struct messaging_context *msg_ctx);

bool printer_driver_in_use(TALLOC_CTX *mem_ctx,
			   const struct auth_serversupplied_info *server_info,
			   struct messaging_context *msg_ctx,
			   const struct spoolss_DriverInfo8 *r);
bool printer_driver_files_in_use(TALLOC_CTX *mem_ctx,
				 const struct auth_serversupplied_info *server_info,
				 struct messaging_context *msg_ctx,
				 struct spoolss_DriverInfo8 *r);
bool delete_driver_files(const struct auth_serversupplied_info *server_info,
			 const struct spoolss_DriverInfo8 *r);

WERROR move_driver_to_download_area(struct auth_serversupplied_info *session_info,
				    struct spoolss_AddDriverInfoCtr *r);

WERROR clean_up_driver_struct(TALLOC_CTX *mem_ctx,
			      struct auth_serversupplied_info *session_info,
			      struct spoolss_AddDriverInfoCtr *r);

void map_printer_permissions(struct security_descriptor *sd);

void map_job_permissions(struct security_descriptor *sd);

bool print_time_access_check(const struct auth_serversupplied_info *server_info,
			     struct messaging_context *msg_ctx,
			     const char *servicename);

void nt_printer_remove(TALLOC_CTX *mem_ctx,
			const struct auth_serversupplied_info *server_info,
			struct messaging_context *msg_ctx,
			const char *printer);
void nt_printer_add(TALLOC_CTX *mem_ctx,
		    const struct auth_serversupplied_info *server_info,
		    struct messaging_context *msg_ctx,
		    const char *printer);

#endif /* NT_PRINTING_H_ */
