/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-2000,
 *  Copyright (C) Jean François Micouleau      1998-2000.
 *  Copyright (C) Gerald Carter                2002-2005.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "printing/nt_printing_tdb.h"
#include "../librpc/gen_ndr/ndr_spoolss.h"
#include "rpc_server/spoolss/srv_spoolss_util.h"
#include "nt_printing.h"
#include "secrets.h"
#include "../librpc/gen_ndr/netlogon.h"
#include "../libcli/security/security.h"
#include "passdb/machine_sid.h"
#include "smbd/smbd.h"
#include "auth.h"
#include "messages.h"
#include "rpc_server/spoolss/srv_spoolss_nt.h"
#include "rpc_client/cli_winreg_spoolss.h"

/* Map generic permissions to printer object specific permissions */

const struct generic_mapping printer_generic_mapping = {
	PRINTER_READ,
	PRINTER_WRITE,
	PRINTER_EXECUTE,
	PRINTER_ALL_ACCESS
};

/* Map generic permissions to print server object specific permissions */

const struct generic_mapping printserver_generic_mapping = {
	SERVER_READ,
	SERVER_WRITE,
	SERVER_EXECUTE,
	SERVER_ALL_ACCESS
};

/* Map generic permissions to job object specific permissions */

const struct generic_mapping job_generic_mapping = {
	JOB_READ,
	JOB_WRITE,
	JOB_EXECUTE,
	JOB_ALL_ACCESS
};

static const struct print_architecture_table_node archi_table[]= {

	{"Windows 4.0",          SPL_ARCH_WIN40,	0 },
	{"Windows NT x86",       SPL_ARCH_W32X86,	2 },
	{"Windows NT R4000",     SPL_ARCH_W32MIPS,	2 },
	{"Windows NT Alpha_AXP", SPL_ARCH_W32ALPHA,	2 },
	{"Windows NT PowerPC",   SPL_ARCH_W32PPC,	2 },
	{"Windows IA64",   	 SPL_ARCH_IA64,		3 },
	{"Windows x64",   	 SPL_ARCH_X64,		3 },
	{NULL,                   "",		-1 }
};

/****************************************************************************
 Open the NT printing tdbs. Done once before fork().
****************************************************************************/

bool nt_printing_init(struct messaging_context *msg_ctx)
{
	WERROR win_rc;

	if (!nt_printing_tdb_upgrade()) {
		return false;
	}

	/*
	 * register callback to handle updating printers as new
	 * drivers are installed
	 */
	messaging_register(msg_ctx, NULL, MSG_PRINTER_DRVUPGRADE,
			   do_drv_upgrade_printer);

	/* of course, none of the message callbacks matter if you don't
	   tell messages.c that you interested in receiving PRINT_GENERAL
	   msgs.  This is done in serverid_register() */

	if ( lp_security() == SEC_ADS ) {
		win_rc = check_published_printers(msg_ctx);
		if (!W_ERROR_IS_OK(win_rc))
			DEBUG(0, ("nt_printing_init: error checking published printers: %s\n", win_errstr(win_rc)));
	}

	return true;
}

/*******************************************************************
 Function to allow filename parsing "the old way".
********************************************************************/

static NTSTATUS driver_unix_convert(connection_struct *conn,
				    const char *old_name,
				    struct smb_filename **smb_fname)
{
	NTSTATUS status;
	TALLOC_CTX *ctx = talloc_tos();
	char *name = talloc_strdup(ctx, old_name);

	if (!name) {
		return NT_STATUS_NO_MEMORY;
	}
	unix_format(name);
	name = unix_clean_name(ctx, name);
	if (!name) {
		return NT_STATUS_NO_MEMORY;
	}
	trim_string(name,"/","/");

	status = unix_convert(ctx, conn, name, smb_fname, 0);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Function to do the mapping between the long architecture name and
 the short one.
****************************************************************************/

const char *get_short_archi(const char *long_archi)
{
        int i=-1;

        DEBUG(107,("Getting architecture dependent directory\n"));
        do {
                i++;
        } while ( (archi_table[i].long_archi!=NULL ) &&
                  StrCaseCmp(long_archi, archi_table[i].long_archi) );

        if (archi_table[i].long_archi==NULL) {
                DEBUGADD(10,("Unknown architecture [%s] !\n", long_archi));
                return NULL;
        }

	/* this might be client code - but shouldn't this be an fstrcpy etc? */

        DEBUGADD(108,("index: [%d]\n", i));
        DEBUGADD(108,("long architecture: [%s]\n", archi_table[i].long_archi));
        DEBUGADD(108,("short architecture: [%s]\n", archi_table[i].short_archi));

	return archi_table[i].short_archi;
}

/****************************************************************************
 Version information in Microsoft files is held in a VS_VERSION_INFO structure.
 There are two case to be covered here: PE (Portable Executable) and NE (New
 Executable) files. Both files support the same INFO structure, but PE files
 store the signature in unicode, and NE files store it as !unicode.
 returns -1 on error, 1 on version info found, and 0 on no version info found.
****************************************************************************/

static int get_file_version(files_struct *fsp, char *fname,uint32 *major, uint32 *minor)
{
	int     i;
	char    *buf = NULL;
	ssize_t byte_count;

	if ((buf=(char *)SMB_MALLOC(DOS_HEADER_SIZE)) == NULL) {
		DEBUG(0,("get_file_version: PE file [%s] DOS Header malloc failed bytes = %d\n",
				fname, DOS_HEADER_SIZE));
		goto error_exit;
	}

	if ((byte_count = vfs_read_data(fsp, buf, DOS_HEADER_SIZE)) < DOS_HEADER_SIZE) {
		DEBUG(3,("get_file_version: File [%s] DOS header too short, bytes read = %lu\n",
			 fname, (unsigned long)byte_count));
		goto no_version_info;
	}

	/* Is this really a DOS header? */
	if (SVAL(buf,DOS_HEADER_MAGIC_OFFSET) != DOS_HEADER_MAGIC) {
		DEBUG(6,("get_file_version: File [%s] bad DOS magic = 0x%x\n",
				fname, SVAL(buf,DOS_HEADER_MAGIC_OFFSET)));
		goto no_version_info;
	}

	/* Skip OEM header (if any) and the DOS stub to start of Windows header */
	if (SMB_VFS_LSEEK(fsp, SVAL(buf,DOS_HEADER_LFANEW_OFFSET), SEEK_SET) == (SMB_OFF_T)-1) {
		DEBUG(3,("get_file_version: File [%s] too short, errno = %d\n",
				fname, errno));
		/* Assume this isn't an error... the file just looks sort of like a PE/NE file */
		goto no_version_info;
	}

	/* Note: DOS_HEADER_SIZE and NE_HEADER_SIZE are incidentally same */
	if ((byte_count = vfs_read_data(fsp, buf, NE_HEADER_SIZE)) < NE_HEADER_SIZE) {
		DEBUG(3,("get_file_version: File [%s] Windows header too short, bytes read = %lu\n",
			 fname, (unsigned long)byte_count));
		/* Assume this isn't an error... the file just looks sort of like a PE/NE file */
		goto no_version_info;
	}

	/* The header may be a PE (Portable Executable) or an NE (New Executable) */
	if (IVAL(buf,PE_HEADER_SIGNATURE_OFFSET) == PE_HEADER_SIGNATURE) {
		unsigned int num_sections;
		unsigned int section_table_bytes;

		/* Just skip over optional header to get to section table */
		if (SMB_VFS_LSEEK(fsp,
				SVAL(buf,PE_HEADER_OPTIONAL_HEADER_SIZE)-(NE_HEADER_SIZE-PE_HEADER_SIZE),
				SEEK_CUR) == (SMB_OFF_T)-1) {
			DEBUG(3,("get_file_version: File [%s] Windows optional header too short, errno = %d\n",
				fname, errno));
			goto error_exit;
		}

		/* get the section table */
		num_sections        = SVAL(buf,PE_HEADER_NUMBER_OF_SECTIONS);
		section_table_bytes = num_sections * PE_HEADER_SECT_HEADER_SIZE;
		if (section_table_bytes == 0)
			goto error_exit;

		SAFE_FREE(buf);
		if ((buf=(char *)SMB_MALLOC(section_table_bytes)) == NULL) {
			DEBUG(0,("get_file_version: PE file [%s] section table malloc failed bytes = %d\n",
					fname, section_table_bytes));
			goto error_exit;
		}

		if ((byte_count = vfs_read_data(fsp, buf, section_table_bytes)) < section_table_bytes) {
			DEBUG(3,("get_file_version: PE file [%s] Section header too short, bytes read = %lu\n",
				 fname, (unsigned long)byte_count));
			goto error_exit;
		}

		/* Iterate the section table looking for the resource section ".rsrc" */
		for (i = 0; i < num_sections; i++) {
			int sec_offset = i * PE_HEADER_SECT_HEADER_SIZE;

			if (strcmp(".rsrc", &buf[sec_offset+PE_HEADER_SECT_NAME_OFFSET]) == 0) {
				unsigned int section_pos   = IVAL(buf,sec_offset+PE_HEADER_SECT_PTR_DATA_OFFSET);
				unsigned int section_bytes = IVAL(buf,sec_offset+PE_HEADER_SECT_SIZE_DATA_OFFSET);

				if (section_bytes == 0)
					goto error_exit;

				SAFE_FREE(buf);
				if ((buf=(char *)SMB_MALLOC(section_bytes)) == NULL) {
					DEBUG(0,("get_file_version: PE file [%s] version malloc failed bytes = %d\n",
							fname, section_bytes));
					goto error_exit;
				}

				/* Seek to the start of the .rsrc section info */
				if (SMB_VFS_LSEEK(fsp, section_pos, SEEK_SET) == (SMB_OFF_T)-1) {
					DEBUG(3,("get_file_version: PE file [%s] too short for section info, errno = %d\n",
							fname, errno));
					goto error_exit;
				}

				if ((byte_count = vfs_read_data(fsp, buf, section_bytes)) < section_bytes) {
					DEBUG(3,("get_file_version: PE file [%s] .rsrc section too short, bytes read = %lu\n",
						 fname, (unsigned long)byte_count));
					goto error_exit;
				}

				if (section_bytes < VS_VERSION_INFO_UNICODE_SIZE)
					goto error_exit;

				for (i=0; i<section_bytes-VS_VERSION_INFO_UNICODE_SIZE; i++) {
					/* Scan for 1st 3 unicoded bytes followed by word aligned magic value */
					if (buf[i] == 'V' && buf[i+1] == '\0' && buf[i+2] == 'S') {
						/* Align to next long address */
						int pos = (i + sizeof(VS_SIGNATURE)*2 + 3) & 0xfffffffc;

						if (IVAL(buf,pos) == VS_MAGIC_VALUE) {
							*major = IVAL(buf,pos+VS_MAJOR_OFFSET);
							*minor = IVAL(buf,pos+VS_MINOR_OFFSET);

							DEBUG(6,("get_file_version: PE file [%s] Version = %08x:%08x (%d.%d.%d.%d)\n",
									  fname, *major, *minor,
									  (*major>>16)&0xffff, *major&0xffff,
									  (*minor>>16)&0xffff, *minor&0xffff));
							SAFE_FREE(buf);
							return 1;
						}
					}
				}
			}
		}

		/* Version info not found, fall back to origin date/time */
		DEBUG(10,("get_file_version: PE file [%s] has no version info\n", fname));
		SAFE_FREE(buf);
		return 0;

	} else if (SVAL(buf,NE_HEADER_SIGNATURE_OFFSET) == NE_HEADER_SIGNATURE) {
		if (CVAL(buf,NE_HEADER_TARGET_OS_OFFSET) != NE_HEADER_TARGOS_WIN ) {
			DEBUG(3,("get_file_version: NE file [%s] wrong target OS = 0x%x\n",
					fname, CVAL(buf,NE_HEADER_TARGET_OS_OFFSET)));
			/* At this point, we assume the file is in error. It still could be somthing
			 * else besides a NE file, but it unlikely at this point. */
			goto error_exit;
		}

		/* Allocate a bit more space to speed up things */
		SAFE_FREE(buf);
		if ((buf=(char *)SMB_MALLOC(VS_NE_BUF_SIZE)) == NULL) {
			DEBUG(0,("get_file_version: NE file [%s] malloc failed bytes  = %d\n",
					fname, PE_HEADER_SIZE));
			goto error_exit;
		}

		/* This is a HACK! I got tired of trying to sort through the messy
		 * 'NE' file format. If anyone wants to clean this up please have at
		 * it, but this works. 'NE' files will eventually fade away. JRR */
		while((byte_count = vfs_read_data(fsp, buf, VS_NE_BUF_SIZE)) > 0) {
			/* Cover case that should not occur in a well formed 'NE' .dll file */
			if (byte_count-VS_VERSION_INFO_SIZE <= 0) break;

			for(i=0; i<byte_count; i++) {
				/* Fast skip past data that can't possibly match */
				if (buf[i] != 'V') continue;

				/* Potential match data crosses buf boundry, move it to beginning
				 * of buf, and fill the buf with as much as it will hold. */
				if (i>byte_count-VS_VERSION_INFO_SIZE) {
					int bc;

					memcpy(buf, &buf[i], byte_count-i);
					if ((bc = vfs_read_data(fsp, &buf[byte_count-i], VS_NE_BUF_SIZE-
								   (byte_count-i))) < 0) {

						DEBUG(0,("get_file_version: NE file [%s] Read error, errno=%d\n",
								 fname, errno));
						goto error_exit;
					}

					byte_count = bc + (byte_count - i);
					if (byte_count<VS_VERSION_INFO_SIZE) break;

					i = 0;
				}

				/* Check that the full signature string and the magic number that
				 * follows exist (not a perfect solution, but the chances that this
				 * occurs in code is, well, remote. Yes I know I'm comparing the 'V'
				 * twice, as it is simpler to read the code. */
				if (strcmp(&buf[i], VS_SIGNATURE) == 0) {
					/* Compute skip alignment to next long address */
					int skip = -(SMB_VFS_LSEEK(fsp, 0, SEEK_CUR) - (byte_count - i) +
								 sizeof(VS_SIGNATURE)) & 3;
					if (IVAL(buf,i+sizeof(VS_SIGNATURE)+skip) != 0xfeef04bd) continue;

					*major = IVAL(buf,i+sizeof(VS_SIGNATURE)+skip+VS_MAJOR_OFFSET);
					*minor = IVAL(buf,i+sizeof(VS_SIGNATURE)+skip+VS_MINOR_OFFSET);
					DEBUG(6,("get_file_version: NE file [%s] Version = %08x:%08x (%d.%d.%d.%d)\n",
							  fname, *major, *minor,
							  (*major>>16)&0xffff, *major&0xffff,
							  (*minor>>16)&0xffff, *minor&0xffff));
					SAFE_FREE(buf);
					return 1;
				}
			}
		}

		/* Version info not found, fall back to origin date/time */
		DEBUG(0,("get_file_version: NE file [%s] Version info not found\n", fname));
		SAFE_FREE(buf);
		return 0;

	} else
		/* Assume this isn't an error... the file just looks sort of like a PE/NE file */
		DEBUG(3,("get_file_version: File [%s] unknown file format, signature = 0x%x\n",
				fname, IVAL(buf,PE_HEADER_SIGNATURE_OFFSET)));

	no_version_info:
		SAFE_FREE(buf);
		return 0;

	error_exit:
		SAFE_FREE(buf);
		return -1;
}

/****************************************************************************
Drivers for Microsoft systems contain multiple files. Often, multiple drivers
share one or more files. During the MS installation process files are checked
to insure that only a newer version of a shared file is installed over an
older version. There are several possibilities for this comparison. If there
is no previous version, the new one is newer (obviously). If either file is
missing the version info structure, compare the creation date (on Unix use
the modification date). Otherwise chose the numerically larger version number.
****************************************************************************/

static int file_version_is_newer(connection_struct *conn, fstring new_file, fstring old_file)
{
	bool use_version = true;

	uint32 new_major;
	uint32 new_minor;
	time_t new_create_time;

	uint32 old_major;
	uint32 old_minor;
	time_t old_create_time;

	struct smb_filename *smb_fname = NULL;
	files_struct    *fsp = NULL;
	SMB_STRUCT_STAT st;

	NTSTATUS status;
	int ret;

	SET_STAT_INVALID(st);
	new_create_time = (time_t)0;
	old_create_time = (time_t)0;

	/* Get file version info (if available) for previous file (if it exists) */
	status = driver_unix_convert(conn, old_file, &smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		goto error_exit;
	}

	status = SMB_VFS_CREATE_FILE(
		conn,					/* conn */
		NULL,					/* req */
		0,					/* root_dir_fid */
		smb_fname,				/* fname */
		FILE_GENERIC_READ,			/* access_mask */
		FILE_SHARE_READ | FILE_SHARE_WRITE,	/* share_access */
		FILE_OPEN,				/* create_disposition*/
		0,					/* create_options */
		FILE_ATTRIBUTE_NORMAL,			/* file_attributes */
		INTERNAL_OPEN_ONLY,			/* oplock_request */
		0,					/* allocation_size */
		0,					/* private_flags */
		NULL,					/* sd */
		NULL,					/* ea_list */
		&fsp,					/* result */
		NULL);					/* pinfo */

	if (!NT_STATUS_IS_OK(status)) {
		/* Old file not found, so by definition new file is in fact newer */
		DEBUG(10,("file_version_is_newer: Can't open old file [%s], "
			  "errno = %d\n", smb_fname_str_dbg(smb_fname),
			  errno));
		ret = 1;
		goto done;

	} else {
		ret = get_file_version(fsp, old_file, &old_major, &old_minor);
		if (ret == -1) {
			goto error_exit;
		}

		if (!ret) {
			DEBUG(6,("file_version_is_newer: Version info not found [%s], use mod time\n",
					 old_file));
			use_version = false;
			if (SMB_VFS_FSTAT(fsp, &st) == -1) {
				 goto error_exit;
			}
			old_create_time = convert_timespec_to_time_t(st.st_ex_mtime);
			DEBUGADD(6,("file_version_is_newer: mod time = %ld sec\n",
				(long)old_create_time));
		}
	}
	close_file(NULL, fsp, NORMAL_CLOSE);
	fsp = NULL;

	/* Get file version info (if available) for new file */
	status = driver_unix_convert(conn, new_file, &smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		goto error_exit;
	}

	status = SMB_VFS_CREATE_FILE(
		conn,					/* conn */
		NULL,					/* req */
		0,					/* root_dir_fid */
		smb_fname,				/* fname */
		FILE_GENERIC_READ,			/* access_mask */
		FILE_SHARE_READ | FILE_SHARE_WRITE,	/* share_access */
		FILE_OPEN,				/* create_disposition*/
		0,					/* create_options */
		FILE_ATTRIBUTE_NORMAL,			/* file_attributes */
		INTERNAL_OPEN_ONLY,			/* oplock_request */
		0,					/* allocation_size */
		0,					/* private_flags */
		NULL,					/* sd */
		NULL,					/* ea_list */
		&fsp,					/* result */
		NULL);					/* pinfo */

	if (!NT_STATUS_IS_OK(status)) {
		/* New file not found, this shouldn't occur if the caller did its job */
		DEBUG(3,("file_version_is_newer: Can't open new file [%s], "
			 "errno = %d\n", smb_fname_str_dbg(smb_fname), errno));
		goto error_exit;

	} else {
		ret = get_file_version(fsp, new_file, &new_major, &new_minor);
		if (ret == -1) {
			goto error_exit;
		}

		if (!ret) {
			DEBUG(6,("file_version_is_newer: Version info not found [%s], use mod time\n",
					 new_file));
			use_version = false;
			if (SMB_VFS_FSTAT(fsp, &st) == -1) {
				goto error_exit;
			}
			new_create_time = convert_timespec_to_time_t(st.st_ex_mtime);
			DEBUGADD(6,("file_version_is_newer: mod time = %ld sec\n",
				(long)new_create_time));
		}
	}
	close_file(NULL, fsp, NORMAL_CLOSE);
	fsp = NULL;

	if (use_version && (new_major != old_major || new_minor != old_minor)) {
		/* Compare versions and choose the larger version number */
		if (new_major > old_major ||
			(new_major == old_major && new_minor > old_minor)) {

			DEBUG(6,("file_version_is_newer: Replacing [%s] with [%s]\n", old_file, new_file));
			ret = 1;
			goto done;
		}
		else {
			DEBUG(6,("file_version_is_newer: Leaving [%s] unchanged\n", old_file));
			ret = 0;
			goto done;
		}

	} else {
		/* Compare modification time/dates and choose the newest time/date */
		if (new_create_time > old_create_time) {
			DEBUG(6,("file_version_is_newer: Replacing [%s] with [%s]\n", old_file, new_file));
			ret = 1;
			goto done;
		}
		else {
			DEBUG(6,("file_version_is_newer: Leaving [%s] unchanged\n", old_file));
			ret = 0;
			goto done;
		}
	}

 error_exit:
	if(fsp)
		close_file(NULL, fsp, NORMAL_CLOSE);
	ret = -1;
 done:
	TALLOC_FREE(smb_fname);
	return ret;
}

/****************************************************************************
Determine the correct cVersion associated with an architecture and driver
****************************************************************************/
static uint32 get_correct_cversion(struct auth_serversupplied_info *session_info,
				   const char *architecture,
				   const char *driverpath_in,
				   WERROR *perr)
{
	int cversion = -1;
	NTSTATUS          nt_status;
	struct smb_filename *smb_fname = NULL;
	char *driverpath = NULL;
	files_struct      *fsp = NULL;
	connection_struct *conn = NULL;
	char *oldcwd;
	char *printdollar = NULL;
	int printdollar_snum;

	*perr = WERR_INVALID_PARAM;

	/* If architecture is Windows 95/98/ME, the version is always 0. */
	if (strcmp(architecture, SPL_ARCH_WIN40) == 0) {
		DEBUG(10,("get_correct_cversion: Driver is Win9x, cversion = 0\n"));
		*perr = WERR_OK;
		return 0;
	}

	/* If architecture is Windows x64, the version is always 3. */
	if (strcmp(architecture, SPL_ARCH_X64) == 0) {
		DEBUG(10,("get_correct_cversion: Driver is x64, cversion = 3\n"));
		*perr = WERR_OK;
		return 3;
	}

	printdollar_snum = find_service(talloc_tos(), "print$", &printdollar);
	if (!printdollar) {
		*perr = WERR_NOMEM;
		return -1;
	}
	if (printdollar_snum == -1) {
		*perr = WERR_NO_SUCH_SHARE;
		return -1;
	}

	nt_status = create_conn_struct(talloc_tos(), &conn, printdollar_snum,
				       lp_pathname(printdollar_snum),
				       session_info, &oldcwd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0,("get_correct_cversion: create_conn_struct "
			 "returned %s\n", nt_errstr(nt_status)));
		*perr = ntstatus_to_werror(nt_status);
		return -1;
	}

	nt_status = set_conn_force_user_group(conn, printdollar_snum);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("failed set force user / group\n"));
		*perr = ntstatus_to_werror(nt_status);
		goto error_free_conn;
	}

	if (!become_user_by_session(conn, session_info)) {
		DEBUG(0, ("failed to become user\n"));
		*perr = WERR_ACCESS_DENIED;
		goto error_free_conn;
	}

	/* Open the driver file (Portable Executable format) and determine the
	 * deriver the cversion. */
	driverpath = talloc_asprintf(talloc_tos(),
					"%s/%s",
					architecture,
					driverpath_in);
	if (!driverpath) {
		*perr = WERR_NOMEM;
		goto error_exit;
	}

	nt_status = driver_unix_convert(conn, driverpath, &smb_fname);
	if (!NT_STATUS_IS_OK(nt_status)) {
		*perr = ntstatus_to_werror(nt_status);
		goto error_exit;
	}

	nt_status = vfs_file_exist(conn, smb_fname);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(3,("get_correct_cversion: vfs_file_exist failed\n"));
		*perr = WERR_BADFILE;
		goto error_exit;
	}

	nt_status = SMB_VFS_CREATE_FILE(
		conn,					/* conn */
		NULL,					/* req */
		0,					/* root_dir_fid */
		smb_fname,				/* fname */
		FILE_GENERIC_READ,			/* access_mask */
		FILE_SHARE_READ | FILE_SHARE_WRITE,	/* share_access */
		FILE_OPEN,				/* create_disposition*/
		0,					/* create_options */
		FILE_ATTRIBUTE_NORMAL,			/* file_attributes */
		INTERNAL_OPEN_ONLY,			/* oplock_request */
		0,					/* private_flags */
		0,					/* allocation_size */
		NULL,					/* sd */
		NULL,					/* ea_list */
		&fsp,					/* result */
		NULL);					/* pinfo */

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(3,("get_correct_cversion: Can't open file [%s], errno = "
			 "%d\n", smb_fname_str_dbg(smb_fname), errno));
		*perr = WERR_ACCESS_DENIED;
		goto error_exit;
	} else {
		uint32 major;
		uint32 minor;
		int    ret;

		ret = get_file_version(fsp, smb_fname->base_name, &major, &minor);
		if (ret == -1) {
			*perr = WERR_INVALID_PARAM;
			goto error_exit;
		} else if (!ret) {
			DEBUG(6,("get_correct_cversion: Version info not "
				 "found [%s]\n",
				 smb_fname_str_dbg(smb_fname)));
			*perr = WERR_INVALID_PARAM;
			goto error_exit;
		}

		/*
		 * This is a Microsoft'ism. See references in MSDN to VER_FILEVERSION
		 * for more details. Version in this case is not just the version of the
		 * file, but the version in the sense of kernal mode (2) vs. user mode
		 * (3) drivers. Other bits of the version fields are the version info.
		 * JRR 010716
		*/
		cversion = major & 0x0000ffff;
		switch (cversion) {
			case 2: /* WinNT drivers */
			case 3: /* Win2K drivers */
				break;

			default:
				DEBUG(6,("get_correct_cversion: cversion "
					 "invalid [%s]  cversion = %d\n",
					 smb_fname_str_dbg(smb_fname),
					 cversion));
				goto error_exit;
		}

		DEBUG(10,("get_correct_cversion: Version info found [%s] major"
			  " = 0x%x  minor = 0x%x\n",
			  smb_fname_str_dbg(smb_fname), major, minor));
	}

	DEBUG(10,("get_correct_cversion: Driver file [%s] cversion = %d\n",
		  smb_fname_str_dbg(smb_fname), cversion));
	*perr = WERR_OK;

 error_exit:
	unbecome_user();
 error_free_conn:
	TALLOC_FREE(smb_fname);
	if (fsp != NULL) {
		close_file(NULL, fsp, NORMAL_CLOSE);
	}
	if (conn != NULL) {
		vfs_ChDir(conn, oldcwd);
		SMB_VFS_DISCONNECT(conn);
		conn_free(conn);
	}
	if (!NT_STATUS_IS_OK(*perr)) {
		cversion = -1;
	}

	return cversion;
}

/****************************************************************************
****************************************************************************/

#define strip_driver_path(_mem_ctx, _element) do { \
	if (_element && ((_p = strrchr((_element), '\\')) != NULL)) { \
		(_element) = talloc_asprintf((_mem_ctx), "%s", _p+1); \
		W_ERROR_HAVE_NO_MEMORY((_element)); \
	} \
} while (0);

static WERROR clean_up_driver_struct_level(TALLOC_CTX *mem_ctx,
					   struct auth_serversupplied_info *session_info,
					   const char *architecture,
					   const char **driver_path,
					   const char **data_file,
					   const char **config_file,
					   const char **help_file,
					   struct spoolss_StringArray *dependent_files,
					   enum spoolss_DriverOSVersion *version)
{
	const char *short_architecture;
	int i;
	WERROR err;
	char *_p;

	if (!*driver_path || !*data_file) {
		return WERR_INVALID_PARAM;
	}

	if (!strequal(architecture, SPOOLSS_ARCHITECTURE_4_0) && !*config_file) {
		return WERR_INVALID_PARAM;
	}

	/* clean up the driver name.
	 * we can get .\driver.dll
	 * or worse c:\windows\system\driver.dll !
	 */
	/* using an intermediate string to not have overlaping memcpy()'s */

	strip_driver_path(mem_ctx, *driver_path);
	strip_driver_path(mem_ctx, *data_file);
	if (*config_file) {
		strip_driver_path(mem_ctx, *config_file);
	}
	if (help_file) {
		strip_driver_path(mem_ctx, *help_file);
	}

	if (dependent_files && dependent_files->string) {
		for (i=0; dependent_files->string[i]; i++) {
			strip_driver_path(mem_ctx, dependent_files->string[i]);
		}
	}

	short_architecture = get_short_archi(architecture);
	if (!short_architecture) {
		return WERR_UNKNOWN_PRINTER_DRIVER;
	}

	/* jfm:7/16/2000 the client always sends the cversion=0.
	 * The server should check which version the driver is by reading
	 * the PE header of driver->driverpath.
	 *
	 * For Windows 95/98 the version is 0 (so the value sent is correct)
	 * For Windows NT (the architecture doesn't matter)
	 *	NT 3.1: cversion=0
	 *	NT 3.5/3.51: cversion=1
	 *	NT 4: cversion=2
	 *	NT2K: cversion=3
	 */

	*version = get_correct_cversion(session_info, short_architecture,
					*driver_path, &err);
	if (*version == -1) {
		return err;
	}

	return WERR_OK;
}

/****************************************************************************
****************************************************************************/

WERROR clean_up_driver_struct(TALLOC_CTX *mem_ctx,
			      struct auth_serversupplied_info *session_info,
			      struct spoolss_AddDriverInfoCtr *r)
{
	switch (r->level) {
	case 3:
		return clean_up_driver_struct_level(mem_ctx, session_info,
						    r->info.info3->architecture,
						    &r->info.info3->driver_path,
						    &r->info.info3->data_file,
						    &r->info.info3->config_file,
						    &r->info.info3->help_file,
						    r->info.info3->dependent_files,
						    &r->info.info3->version);
	case 6:
		return clean_up_driver_struct_level(mem_ctx, session_info,
						    r->info.info6->architecture,
						    &r->info.info6->driver_path,
						    &r->info.info6->data_file,
						    &r->info.info6->config_file,
						    &r->info.info6->help_file,
						    r->info.info6->dependent_files,
						    &r->info.info6->version);
	default:
		return WERR_NOT_SUPPORTED;
	}
}

/****************************************************************************
 This function sucks and should be replaced. JRA.
****************************************************************************/

static void convert_level_6_to_level3(struct spoolss_AddDriverInfo3 *dst,
				      const struct spoolss_AddDriverInfo6 *src)
{
	dst->version		= src->version;

	dst->driver_name	= src->driver_name;
	dst->architecture 	= src->architecture;
	dst->driver_path	= src->driver_path;
	dst->data_file		= src->data_file;
	dst->config_file	= src->config_file;
	dst->help_file		= src->help_file;
	dst->monitor_name	= src->monitor_name;
	dst->default_datatype	= src->default_datatype;
	dst->_ndr_size_dependent_files = src->_ndr_size_dependent_files;
	dst->dependent_files	= src->dependent_files;
}

/****************************************************************************
****************************************************************************/

static WERROR move_driver_file_to_download_area(TALLOC_CTX *mem_ctx,
						connection_struct *conn,
						const char *driver_file,
						const char *short_architecture,
						uint32_t driver_version,
						uint32_t version)
{
	struct smb_filename *smb_fname_old = NULL;
	struct smb_filename *smb_fname_new = NULL;
	char *old_name = NULL;
	char *new_name = NULL;
	NTSTATUS status;
	WERROR ret;

	old_name = talloc_asprintf(mem_ctx, "%s/%s",
				   short_architecture, driver_file);
	W_ERROR_HAVE_NO_MEMORY(old_name);

	new_name = talloc_asprintf(mem_ctx, "%s/%d/%s",
				   short_architecture, driver_version, driver_file);
	if (new_name == NULL) {
		TALLOC_FREE(old_name);
		return WERR_NOMEM;
	}

	if (version != -1 && (version = file_version_is_newer(conn, old_name, new_name)) > 0) {

		status = driver_unix_convert(conn, old_name, &smb_fname_old);
		if (!NT_STATUS_IS_OK(status)) {
			ret = WERR_NOMEM;
			goto out;
		}

		/* Setup a synthetic smb_filename struct */
		smb_fname_new = TALLOC_ZERO_P(mem_ctx, struct smb_filename);
		if (!smb_fname_new) {
			ret = WERR_NOMEM;
			goto out;
		}

		smb_fname_new->base_name = new_name;

		DEBUG(10,("move_driver_file_to_download_area: copying '%s' to "
			  "'%s'\n", smb_fname_old->base_name,
			  smb_fname_new->base_name));

		status = copy_file(mem_ctx, conn, smb_fname_old, smb_fname_new,
				   OPENX_FILE_EXISTS_TRUNCATE |
				   OPENX_FILE_CREATE_IF_NOT_EXIST,
				   0, false);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("move_driver_file_to_download_area: Unable "
				 "to rename [%s] to [%s]: %s\n",
				 smb_fname_old->base_name, new_name,
				 nt_errstr(status)));
			ret = WERR_ACCESS_DENIED;
			goto out;
		}
	}

	ret = WERR_OK;
 out:
	TALLOC_FREE(smb_fname_old);
	TALLOC_FREE(smb_fname_new);
	return ret;
}

WERROR move_driver_to_download_area(struct auth_serversupplied_info *session_info,
				    struct spoolss_AddDriverInfoCtr *r)
{
	struct spoolss_AddDriverInfo3 *driver;
	struct spoolss_AddDriverInfo3 converted_driver;
	const char *short_architecture;
	struct smb_filename *smb_dname = NULL;
	char *new_dir = NULL;
	connection_struct *conn = NULL;
	NTSTATUS nt_status;
	int i;
	TALLOC_CTX *ctx = talloc_tos();
	int ver = 0;
	char *oldcwd;
	char *printdollar = NULL;
	int printdollar_snum;
	WERROR err = WERR_OK;

	switch (r->level) {
	case 3:
		driver = r->info.info3;
		break;
	case 6:
		convert_level_6_to_level3(&converted_driver, r->info.info6);
		driver = &converted_driver;
		break;
	default:
		DEBUG(0,("move_driver_to_download_area: Unknown info level (%u)\n", (unsigned int)r->level));
		return WERR_UNKNOWN_LEVEL;
	}

	short_architecture = get_short_archi(driver->architecture);
	if (!short_architecture) {
		return WERR_UNKNOWN_PRINTER_DRIVER;
	}

	printdollar_snum = find_service(ctx, "print$", &printdollar);
	if (!printdollar) {
		return WERR_NOMEM;
	}
	if (printdollar_snum == -1) {
		return WERR_NO_SUCH_SHARE;
	}

	nt_status = create_conn_struct(talloc_tos(), &conn, printdollar_snum,
				       lp_pathname(printdollar_snum),
				       session_info, &oldcwd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0,("move_driver_to_download_area: create_conn_struct "
			 "returned %s\n", nt_errstr(nt_status)));
		err = ntstatus_to_werror(nt_status);
		return err;
	}

	nt_status = set_conn_force_user_group(conn, printdollar_snum);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("failed set force user / group\n"));
		err = ntstatus_to_werror(nt_status);
		goto err_free_conn;
	}

	if (!become_user_by_session(conn, session_info)) {
		DEBUG(0, ("failed to become user\n"));
		err = WERR_ACCESS_DENIED;
		goto err_free_conn;
	}

	new_dir = talloc_asprintf(ctx,
				"%s/%d",
				short_architecture,
				driver->version);
	if (!new_dir) {
		err = WERR_NOMEM;
		goto err_exit;
	}
	nt_status = driver_unix_convert(conn, new_dir, &smb_dname);
	if (!NT_STATUS_IS_OK(nt_status)) {
		err = WERR_NOMEM;
		goto err_exit;
	}

	DEBUG(5,("Creating first directory: %s\n", smb_dname->base_name));

	nt_status = create_directory(conn, NULL, smb_dname);
	if (!NT_STATUS_IS_OK(nt_status)
	 && !NT_STATUS_EQUAL(nt_status, NT_STATUS_OBJECT_NAME_COLLISION)) {
		DEBUG(0, ("failed to create driver destination directory: %s\n",
			  nt_errstr(nt_status)));
		err = ntstatus_to_werror(nt_status);
		goto err_exit;
	}

	/* For each driver file, archi\filexxx.yyy, if there is a duplicate file
	 * listed for this driver which has already been moved, skip it (note:
	 * drivers may list the same file name several times. Then check if the
	 * file already exists in archi\version\, if so, check that the version
	 * info (or time stamps if version info is unavailable) is newer (or the
	 * date is later). If it is, move it to archi\version\filexxx.yyy.
	 * Otherwise, delete the file.
	 *
	 * If a file is not moved to archi\version\ because of an error, all the
	 * rest of the 'unmoved' driver files are removed from archi\. If one or
	 * more of the driver's files was already moved to archi\version\, it
	 * potentially leaves the driver in a partially updated state. Version
	 * trauma will most likely occur if an client attempts to use any printer
	 * bound to the driver. Perhaps a rewrite to make sure the moves can be
	 * done is appropriate... later JRR
	 */

	DEBUG(5,("Moving files now !\n"));

	if (driver->driver_path && strlen(driver->driver_path)) {

		err = move_driver_file_to_download_area(ctx,
							conn,
							driver->driver_path,
							short_architecture,
							driver->version,
							ver);
		if (!W_ERROR_IS_OK(err)) {
			goto err_exit;
		}
	}

	if (driver->data_file && strlen(driver->data_file)) {
		if (!strequal(driver->data_file, driver->driver_path)) {

			err = move_driver_file_to_download_area(ctx,
								conn,
								driver->data_file,
								short_architecture,
								driver->version,
								ver);
			if (!W_ERROR_IS_OK(err)) {
				goto err_exit;
			}
		}
	}

	if (driver->config_file && strlen(driver->config_file)) {
		if (!strequal(driver->config_file, driver->driver_path) &&
		    !strequal(driver->config_file, driver->data_file)) {

			err = move_driver_file_to_download_area(ctx,
								conn,
								driver->config_file,
								short_architecture,
								driver->version,
								ver);
			if (!W_ERROR_IS_OK(err)) {
				goto err_exit;
			}
		}
	}

	if (driver->help_file && strlen(driver->help_file)) {
		if (!strequal(driver->help_file, driver->driver_path) &&
		    !strequal(driver->help_file, driver->data_file) &&
		    !strequal(driver->help_file, driver->config_file)) {

			err = move_driver_file_to_download_area(ctx,
								conn,
								driver->help_file,
								short_architecture,
								driver->version,
								ver);
			if (!W_ERROR_IS_OK(err)) {
				goto err_exit;
			}
		}
	}

	if (driver->dependent_files && driver->dependent_files->string) {
		for (i=0; driver->dependent_files->string[i]; i++) {
			if (!strequal(driver->dependent_files->string[i], driver->driver_path) &&
			    !strequal(driver->dependent_files->string[i], driver->data_file) &&
			    !strequal(driver->dependent_files->string[i], driver->config_file) &&
			    !strequal(driver->dependent_files->string[i], driver->help_file)) {
				int j;
				for (j=0; j < i; j++) {
					if (strequal(driver->dependent_files->string[i], driver->dependent_files->string[j])) {
						goto NextDriver;
					}
				}

				err = move_driver_file_to_download_area(ctx,
									conn,
									driver->dependent_files->string[i],
									short_architecture,
									driver->version,
									ver);
				if (!W_ERROR_IS_OK(err)) {
					goto err_exit;
				}
			}
		NextDriver: ;
		}
	}

	err = WERR_OK;
 err_exit:
	unbecome_user();
 err_free_conn:
	TALLOC_FREE(smb_dname);

	if (conn != NULL) {
		vfs_ChDir(conn, oldcwd);
		SMB_VFS_DISCONNECT(conn);
		conn_free(conn);
	}

	return err;
}

/****************************************************************************
  Determine whether or not a particular driver is currently assigned
  to a printer
****************************************************************************/

bool printer_driver_in_use(TALLOC_CTX *mem_ctx,
			   const struct auth_serversupplied_info *session_info,
			   struct messaging_context *msg_ctx,
                           const struct spoolss_DriverInfo8 *r)
{
	int snum;
	int n_services = lp_numservices();
	bool in_use = False;
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	WERROR result;
	struct dcerpc_binding_handle *b = NULL;

	if (!r) {
		return false;
	}

	DEBUG(10,("printer_driver_in_use: Beginning search through ntprinters.tdb...\n"));

	/* loop through the printers.tdb and check for the drivername */

	for (snum=0; snum<n_services && !in_use; snum++) {
		if (!lp_snum_ok(snum) || !lp_print_ok(snum)) {
			continue;
		}

		if (b == NULL) {
			result = winreg_printer_binding_handle(mem_ctx,
							       session_info,
							       msg_ctx,
							       &b);
			if (!W_ERROR_IS_OK(result)) {
				return false;
			}
		}

		result = winreg_get_printer(mem_ctx, b,
					    lp_servicename(snum),
					    &pinfo2);
		if (!W_ERROR_IS_OK(result)) {
			continue; /* skip */
		}

		if (strequal(r->driver_name, pinfo2->drivername)) {
			in_use = True;
		}

		TALLOC_FREE(pinfo2);
	}

	DEBUG(10,("printer_driver_in_use: Completed search through ntprinters.tdb...\n"));

	if ( in_use ) {
		struct spoolss_DriverInfo8 *driver;
		WERROR werr;

		DEBUG(5,("printer_driver_in_use: driver \"%s\" is currently in use\n", r->driver_name));

		/* we can still remove the driver if there is one of
		   "Windows NT x86" version 2 or 3 left */

		if (!strequal("Windows NT x86", r->architecture)) {
			werr = winreg_get_driver(mem_ctx, b,
						 "Windows NT x86",
						 r->driver_name,
						 DRIVER_ANY_VERSION,
						 &driver);
		} else if (r->version == 2) {
			werr = winreg_get_driver(mem_ctx, b,
						 "Windows NT x86",
						 r->driver_name,
						 3, &driver);
		} else if (r->version == 3) {
			werr = winreg_get_driver(mem_ctx, b,
						 "Windows NT x86",
						 r->driver_name,
						 2, &driver);
		} else {
			DEBUG(0, ("printer_driver_in_use: ERROR!"
				  " unknown driver version (%d)\n",
				  r->version));
			werr = WERR_UNKNOWN_PRINTER_DRIVER;
		}

		/* now check the error code */

		if ( W_ERROR_IS_OK(werr) ) {
			/* it's ok to remove the driver, we have other architctures left */
			in_use = False;
			talloc_free(driver);
		}
	}

	/* report that the driver is not in use by default */

	return in_use;
}


/**********************************************************************
 Check to see if a ogiven file is in use by *info
 *********************************************************************/

static bool drv_file_in_use(const char *file, const struct spoolss_DriverInfo8 *info)
{
	int i = 0;

	if ( !info )
		return False;

	/* mz: skip files that are in the list but already deleted */
	if (!file || !file[0]) {
		return false;
	}

	if (strequal(file, info->driver_path))
		return True;

	if (strequal(file, info->data_file))
		return True;

	if (strequal(file, info->config_file))
		return True;

	if (strequal(file, info->help_file))
		return True;

	/* see of there are any dependent files to examine */

	if (!info->dependent_files)
		return False;

	while (info->dependent_files[i] && *info->dependent_files[i]) {
		if (strequal(file, info->dependent_files[i]))
			return True;
		i++;
	}

	return False;

}

/**********************************************************************
 Utility function to remove the dependent file pointed to by the
 input parameter from the list
 *********************************************************************/

static void trim_dependent_file(TALLOC_CTX *mem_ctx, const char **files, int idx)
{

	/* bump everything down a slot */

	while (files && files[idx+1]) {
		files[idx] = talloc_strdup(mem_ctx, files[idx+1]);
		idx++;
	}

	files[idx] = NULL;

	return;
}

/**********************************************************************
 Check if any of the files used by src are also used by drv
 *********************************************************************/

static bool trim_overlap_drv_files(TALLOC_CTX *mem_ctx,
				   struct spoolss_DriverInfo8 *src,
				   const struct spoolss_DriverInfo8 *drv)
{
	bool 	in_use = False;
	int 	i = 0;

	if ( !src || !drv )
		return False;

	/* check each file.  Remove it from the src structure if it overlaps */

	if (drv_file_in_use(src->driver_path, drv)) {
		in_use = True;
		DEBUG(10,("Removing driverfile [%s] from list\n", src->driver_path));
		src->driver_path = talloc_strdup(mem_ctx, "");
		if (!src->driver_path) { return false; }
	}

	if (drv_file_in_use(src->data_file, drv)) {
		in_use = True;
		DEBUG(10,("Removing datafile [%s] from list\n", src->data_file));
		src->data_file = talloc_strdup(mem_ctx, "");
		if (!src->data_file) { return false; }
	}

	if (drv_file_in_use(src->config_file, drv)) {
		in_use = True;
		DEBUG(10,("Removing configfile [%s] from list\n", src->config_file));
		src->config_file = talloc_strdup(mem_ctx, "");
		if (!src->config_file) { return false; }
	}

	if (drv_file_in_use(src->help_file, drv)) {
		in_use = True;
		DEBUG(10,("Removing helpfile [%s] from list\n", src->help_file));
		src->help_file = talloc_strdup(mem_ctx, "");
		if (!src->help_file) { return false; }
	}

	/* are there any dependentfiles to examine? */

	if (!src->dependent_files)
		return in_use;

	while (src->dependent_files[i] && *src->dependent_files[i]) {
		if (drv_file_in_use(src->dependent_files[i], drv)) {
			in_use = True;
			DEBUG(10,("Removing [%s] from dependent file list\n", src->dependent_files[i]));
			trim_dependent_file(mem_ctx, src->dependent_files, i);
		} else
			i++;
	}

	return in_use;
}

/****************************************************************************
  Determine whether or not a particular driver files are currently being
  used by any other driver.

  Return value is True if any files were in use by other drivers
  and False otherwise.

  Upon return, *info has been modified to only contain the driver files
  which are not in use

  Fix from mz:

  This needs to check all drivers to ensure that all files in use
  have been removed from *info, not just the ones in the first
  match.
****************************************************************************/

bool printer_driver_files_in_use(TALLOC_CTX *mem_ctx,
				 const struct auth_serversupplied_info *session_info,
				 struct messaging_context *msg_ctx,
				 struct spoolss_DriverInfo8 *info)
{
	int 				i;
	uint32 				version;
	struct spoolss_DriverInfo8 	*driver;
	bool in_use = false;
	uint32_t num_drivers;
	const char **drivers;
	WERROR result;
	struct dcerpc_binding_handle *b;

	if ( !info )
		return False;

	version = info->version;

	/* loop over all driver versions */

	DEBUG(5,("printer_driver_files_in_use: Beginning search of drivers...\n"));

	/* get the list of drivers */

	result = winreg_printer_binding_handle(mem_ctx,
					       session_info,
					       msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(result)) {
		return false;
	}

	result = winreg_get_driver_list(mem_ctx, b,
					info->architecture, version,
					&num_drivers, &drivers);
	if (!W_ERROR_IS_OK(result)) {
		return true;
	}

	DEBUGADD(4, ("we have:[%d] drivers in environment [%s] and version [%d]\n",
		     num_drivers, info->architecture, version));

	/* check each driver for overlap in files */

	for (i = 0; i < num_drivers; i++) {
		DEBUGADD(5,("\tdriver: [%s]\n", drivers[i]));

		driver = NULL;

		result = winreg_get_driver(mem_ctx, b,
					   info->architecture, drivers[i],
					   version, &driver);
		if (!W_ERROR_IS_OK(result)) {
			talloc_free(drivers);
			return True;
		}

		/* check if d2 uses any files from d1 */
		/* only if this is a different driver than the one being deleted */

		if (!strequal(info->driver_name, driver->driver_name)) {
			if (trim_overlap_drv_files(mem_ctx, info, driver)) {
				/* mz: Do not instantly return -
				 * we need to ensure this file isn't
				 * also in use by other drivers. */
				in_use = true;
			}
		}

		talloc_free(driver);
	}

	talloc_free(drivers);

	DEBUG(5,("printer_driver_files_in_use: Completed search of drivers...\n"));

	return in_use;
}

static NTSTATUS driver_unlink_internals(connection_struct *conn,
					const char *short_arch,
					int vers,
					const char *fname)
{
	TALLOC_CTX *tmp_ctx = talloc_new(conn);
	struct smb_filename *smb_fname = NULL;
	char *print_dlr_path;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	print_dlr_path = talloc_asprintf(tmp_ctx, "%s/%d/%s",
					 short_arch, vers, fname);
	if (print_dlr_path == NULL) {
		goto err_out;
	}

	status = create_synthetic_smb_fname(tmp_ctx, print_dlr_path,
					    NULL, NULL, &smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		goto err_out;
	}

	status = unlink_internals(conn, NULL, 0, smb_fname, false);
err_out:
	talloc_free(tmp_ctx);
	return status;
}

/****************************************************************************
  Actually delete the driver files.  Make sure that
  printer_driver_files_in_use() return False before calling
  this.
****************************************************************************/

bool delete_driver_files(const struct auth_serversupplied_info *session_info,
			 const struct spoolss_DriverInfo8 *r)
{
	const char *short_arch;
	connection_struct *conn;
	NTSTATUS nt_status;
	char *oldcwd;
	char *printdollar = NULL;
	int printdollar_snum;
	bool ret = false;

	if (!r) {
		return false;
	}

	DEBUG(6,("delete_driver_files: deleting driver [%s] - version [%d]\n",
		r->driver_name, r->version));

	printdollar_snum = find_service(talloc_tos(), "print$", &printdollar);
	if (!printdollar) {
		return false;
	}
	if (printdollar_snum == -1) {
		return false;
	}

	nt_status = create_conn_struct(talloc_tos(), &conn, printdollar_snum,
				       lp_pathname(printdollar_snum),
				       session_info, &oldcwd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0,("delete_driver_files: create_conn_struct "
			 "returned %s\n", nt_errstr(nt_status)));
		return false;
	}

	nt_status = set_conn_force_user_group(conn, printdollar_snum);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("failed set force user / group\n"));
		ret = false;
		goto err_free_conn;
	}

	if (!become_user_by_session(conn, session_info)) {
		DEBUG(0, ("failed to become user\n"));
		ret = false;
		goto err_free_conn;
	}

	if ( !CAN_WRITE(conn) ) {
		DEBUG(3,("delete_driver_files: Cannot delete print driver when [print$] is read-only\n"));
		ret = false;
		goto err_out;
	}

	short_arch = get_short_archi(r->architecture);
	if (short_arch == NULL) {
		DEBUG(0, ("bad architecture %s\n", r->architecture));
		ret = false;
		goto err_out;
	}

	/* now delete the files */

	if (r->driver_path && r->driver_path[0]) {
		DEBUG(10,("deleting driverfile [%s]\n", r->driver_path));
		driver_unlink_internals(conn, short_arch, r->version, r->driver_path);
	}

	if (r->config_file && r->config_file[0]) {
		DEBUG(10,("deleting configfile [%s]\n", r->config_file));
		driver_unlink_internals(conn, short_arch, r->version, r->config_file);
	}

	if (r->data_file && r->data_file[0]) {
		DEBUG(10,("deleting datafile [%s]\n", r->data_file));
		driver_unlink_internals(conn, short_arch, r->version, r->data_file);
	}

	if (r->help_file && r->help_file[0]) {
		DEBUG(10,("deleting helpfile [%s]\n", r->help_file));
		driver_unlink_internals(conn, short_arch, r->version, r->help_file);
	}

	if (r->dependent_files) {
		int i = 0;
		while (r->dependent_files[i] && r->dependent_files[i][0]) {
			DEBUG(10,("deleting dependent file [%s]\n", r->dependent_files[i]));
			driver_unlink_internals(conn, short_arch, r->version, r->dependent_files[i]);
			i++;
		}
	}

	ret = true;
 err_out:
	unbecome_user();
 err_free_conn:
	if (conn != NULL) {
		vfs_ChDir(conn, oldcwd);
		SMB_VFS_DISCONNECT(conn);
		conn_free(conn);
	}
	return ret;
}

/* error code:
	0: everything OK
	1: level not implemented
	2: file doesn't exist
	3: can't allocate memory
	4: can't free memory
	5: non existant struct
*/

/*
	A printer and a printer driver are 2 different things.
	NT manages them separatelly, Samba does the same.
	Why ? Simply because it's easier and it makes sense !

	Now explanation: You have 3 printers behind your samba server,
	2 of them are the same make and model (laser A and B). But laser B
	has an 3000 sheet feeder and laser A doesn't such an option.
	Your third printer is an old dot-matrix model for the accounting :-).

	If the /usr/local/samba/lib directory (default dir), you will have
	5 files to describe all of this.

	3 files for the printers (1 by printer):
		NTprinter_laser A
		NTprinter_laser B
		NTprinter_accounting
	2 files for the drivers (1 for the laser and 1 for the dot matrix)
		NTdriver_printer model X
		NTdriver_printer model Y

jfm: I should use this comment for the text file to explain
	same thing for the forms BTW.
	Je devrais mettre mes commentaires en francais, ca serait mieux :-)

*/

/* Convert generic access rights to printer object specific access rights.
   It turns out that NT4 security descriptors use generic access rights and
   NT5 the object specific ones. */

void map_printer_permissions(struct security_descriptor *sd)
{
	int i;

	for (i = 0; sd->dacl && i < sd->dacl->num_aces; i++) {
		se_map_generic(&sd->dacl->aces[i].access_mask,
			       &printer_generic_mapping);
	}
}

void map_job_permissions(struct security_descriptor *sd)
{
	int i;

	for (i = 0; sd->dacl && i < sd->dacl->num_aces; i++) {
		se_map_generic(&sd->dacl->aces[i].access_mask,
			       &job_generic_mapping);
	}
}


/****************************************************************************
 Check a user has permissions to perform the given operation.  We use the
 permission constants defined in include/rpc_spoolss.h to check the various
 actions we perform when checking printer access.

   PRINTER_ACCESS_ADMINISTER:
       print_queue_pause, print_queue_resume, update_printer_sec,
       update_printer, spoolss_addprinterex_level_2,
       _spoolss_setprinterdata

   PRINTER_ACCESS_USE:
       print_job_start

   JOB_ACCESS_ADMINISTER:
       print_job_delete, print_job_pause, print_job_resume,
       print_queue_purge

  Try access control in the following order (for performance reasons):
    1)  root and SE_PRINT_OPERATOR can do anything (easy check)
    2)  check security descriptor (bit comparisons in memory)
    3)  "printer admins" (may result in numerous calls to winbind)

 ****************************************************************************/
bool print_access_check(const struct auth_serversupplied_info *session_info,
			struct messaging_context *msg_ctx, int snum,
			int access_type)
{
	struct spoolss_security_descriptor *secdesc = NULL;
	uint32 access_granted;
	size_t sd_size;
	NTSTATUS status;
	WERROR result;
	const char *pname;
	TALLOC_CTX *mem_ctx = NULL;

	/* If user is NULL then use the current_user structure */

	/* Always allow root or SE_PRINT_OPERATROR to do anything */

	if (session_info->utok.uid == sec_initial_uid()
	    || security_token_has_privilege(session_info->security_token, SEC_PRIV_PRINT_OPERATOR)) {
		return True;
	}

	/* Get printer name */

	pname = lp_printername(snum);

	if (!pname || !*pname) {
		errno = EACCES;
		return False;
	}

	/* Get printer security descriptor */

	if(!(mem_ctx = talloc_init("print_access_check"))) {
		errno = ENOMEM;
		return False;
	}

	result = winreg_get_printer_secdesc_internal(mem_ctx,
					    get_session_info_system(),
					    msg_ctx,
					    pname,
					    &secdesc);
	if (!W_ERROR_IS_OK(result)) {
		talloc_destroy(mem_ctx);
		errno = ENOMEM;
		return False;
	}

	if (access_type == JOB_ACCESS_ADMINISTER) {
		struct spoolss_security_descriptor *parent_secdesc = secdesc;

		/* Create a child security descriptor to check permissions
		   against.  This is because print jobs are child objects
		   objects of a printer. */
		status = se_create_child_secdesc(mem_ctx,
						 &secdesc,
						 &sd_size,
						 parent_secdesc,
						 parent_secdesc->owner_sid,
						 parent_secdesc->group_sid,
						 false);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_destroy(mem_ctx);
			errno = map_errno_from_nt_status(status);
			return False;
		}

		map_job_permissions(secdesc);
	} else {
		map_printer_permissions(secdesc);
	}

	/* Check access */
	status = se_access_check(secdesc, session_info->security_token, access_type,
				 &access_granted);

	DEBUG(4, ("access check was %s\n", NT_STATUS_IS_OK(status) ? "SUCCESS" : "FAILURE"));

        /* see if we need to try the printer admin list */

        if (!NT_STATUS_IS_OK(status) &&
	    (token_contains_name_in_list(uidtoname(session_info->utok.uid),
					 session_info->info3->base.domain.string,
					 NULL, session_info->security_token,
					 lp_printer_admin(snum)))) {
		talloc_destroy(mem_ctx);
		return True;
        }

	talloc_destroy(mem_ctx);

	if (!NT_STATUS_IS_OK(status)) {
		errno = EACCES;
	}

	return NT_STATUS_IS_OK(status);
}

/****************************************************************************
 Check the time parameters allow a print operation.
*****************************************************************************/

bool print_time_access_check(const struct auth_serversupplied_info *session_info,
			     struct messaging_context *msg_ctx,
			     const char *servicename)
{
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	WERROR result;
	bool ok = False;
	time_t now = time(NULL);
	struct tm *t;
	uint32 mins;

	result = winreg_get_printer_internal(NULL, session_info, msg_ctx,
				    servicename, &pinfo2);
	if (!W_ERROR_IS_OK(result)) {
		return False;
	}

	if (pinfo2->starttime == 0 && pinfo2->untiltime == 0) {
		ok = True;
	}

	t = gmtime(&now);
	mins = (uint32)t->tm_hour*60 + (uint32)t->tm_min;

	if (mins >= pinfo2->starttime && mins <= pinfo2->untiltime) {
		ok = True;
	}

	TALLOC_FREE(pinfo2);

	if (!ok) {
		errno = EACCES;
	}

	return ok;
}

void nt_printer_remove(TALLOC_CTX *mem_ctx,
			const struct auth_serversupplied_info *session_info,
			struct messaging_context *msg_ctx,
			const char *printer)
{
	WERROR result;

	result = winreg_delete_printer_key_internal(mem_ctx, session_info, msg_ctx,
					   printer, "");
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("nt_printer_remove: failed to remove printer %s: "
			  "%s\n", printer, win_errstr(result)));
	}
}

void nt_printer_add(TALLOC_CTX *mem_ctx,
		    const struct auth_serversupplied_info *session_info,
		    struct messaging_context *msg_ctx,
		    const char *printer)
{
	WERROR result;

	result = winreg_create_printer_internal(mem_ctx, session_info, msg_ctx,
						printer);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("nt_printer_add: failed to add printer %s: %s\n",
			  printer, win_errstr(result)));
	}
}
