/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2004,2009 Guenther Deschner (gd@samba.org)

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
#include "includes.h"
#include "system/filesys.h"
#include "utils/net.h"
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_spoolss_c.h"
#include "rpc_client/cli_spoolss.h"
#include "rpc_client/init_spoolss.h"
#include "nt_printing.h"
#include "registry/reg_objects.h"
#include "../libcli/security/security.h"
#include "../libcli/registry/util_reg.h"
#include "libsmb/libsmb.h"

/* support itanium as well */
static const struct print_architecture_table_node archi_table[]= {

	{"Windows 4.0",          "WIN40",	0 },
	{"Windows NT x86",       "W32X86",	2 },
	{"Windows NT x86",       "W32X86",	3 },
	{"Windows NT R4000",     "W32MIPS",	2 },
	{"Windows NT Alpha_AXP", "W32ALPHA",	2 },
	{"Windows NT PowerPC",   "W32PPC",	2 },
	{"Windows IA64",         "IA64",	3 },
	{"Windows x64",          "x64",		3 },
	{NULL,                   "",		-1 }
};


/**
 * This display-printdriver-functions was borrowed from rpcclient/cmd_spoolss.c.
 * It is here for debugging purpose and should be removed later on.
 **/

/****************************************************************************
 Printer info level 3 display function.
****************************************************************************/

static void display_print_driver3(struct spoolss_DriverInfo3 *r)
{
	int i;

	if (!r) {
		return;
	}

	printf(_("Printer Driver Info 3:\n"));
	printf(_("\tVersion: [%x]\n"), r->version);
	printf(_("\tDriver Name: [%s]\n"), r->driver_name);
	printf(_("\tArchitecture: [%s]\n"), r->architecture);
	printf(_("\tDriver Path: [%s]\n"), r->driver_path);
	printf(_("\tDatafile: [%s]\n"), r->data_file);
	printf(_("\tConfigfile: [%s]\n\n"), r->config_file);
	printf(_("\tHelpfile: [%s]\n\n"), r->help_file);

	for (i=0; r->dependent_files && r->dependent_files[i] != NULL; i++) {
		printf(_("\tDependentfiles: [%s]\n"), r->dependent_files[i]);
	}

	printf("\n");

	printf(_("\tMonitorname: [%s]\n"), r->monitor_name);
	printf(_("\tDefaultdatatype: [%s]\n\n"), r->default_datatype);
}

static void display_reg_value(const char *subkey, struct regval_blob *value)
{
	const char *text;
	DATA_BLOB blob;

	switch(regval_type(value)) {
	case REG_DWORD:
		d_printf(_("\t[%s:%s]: REG_DWORD: 0x%08x\n"), subkey,
			regval_name(value), *((uint32_t *) regval_data_p(value)));
		break;

	case REG_SZ:
		blob = data_blob_const(regval_data_p(value), regval_size(value));
		pull_reg_sz(talloc_tos(), &blob, &text);
		if (!text) {
			break;
		}
		d_printf(_("\t[%s:%s]: REG_SZ: %s\n"), subkey, regval_name(value),
			 text);
		break;

	case REG_BINARY:
		d_printf(_("\t[%s:%s]: REG_BINARY: unknown length value not "
			   "displayed\n"),
			 subkey, regval_name(value));
		break;

	case REG_MULTI_SZ: {
		uint32_t i;
		const char **values;
		blob = data_blob_const(regval_data_p(value), regval_size(value));

		if (!pull_reg_multi_sz(NULL, &blob, &values)) {
			d_printf("pull_reg_multi_sz failed\n");
			break;
		}

		printf("%s: REG_MULTI_SZ: \n", regval_name(value));
		for (i=0; values[i] != NULL; i++) {
			d_printf("%s\n", values[i]);
		}
		TALLOC_FREE(values);
		break;
	}

	default:
		d_printf(_("\t%s: unknown type %d\n"), regval_name(value),
			 regval_type(value));
	}

}

/**
 * Copies ACLs, DOS-attributes and timestamps from one
 * file or directory from one connected share to another connected share
 *
 * @param c			A net_context structure
 * @param mem_ctx		A talloc-context
 * @param cli_share_src		A connected cli_state
 * @param cli_share_dst		A connected cli_state
 * @param src_file		The source file-name
 * @param dst_file		The destination file-name
 * @param copy_acls		Whether to copy acls
 * @param copy_attrs		Whether to copy DOS attributes
 * @param copy_timestamps	Whether to preserve timestamps
 * @param is_file		Whether this file is a file or a dir
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS net_copy_fileattr(struct net_context *c,
		  TALLOC_CTX *mem_ctx,
		  struct cli_state *cli_share_src,
		  struct cli_state *cli_share_dst,
		  const char *src_name, const char *dst_name,
		  bool copy_acls, bool copy_attrs,
		  bool copy_timestamps, bool is_file)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint16_t fnum_src = 0;
	uint16_t fnum_dst = 0;
	struct security_descriptor *sd = NULL;
	uint16_t attr;
	time_t f_atime, f_ctime, f_mtime;


	if (!copy_timestamps && !copy_acls && !copy_attrs)
		return NT_STATUS_OK;

	/* open file/dir on the originating server */

	DEBUGADD(3,("opening %s %s on originating server\n",
		is_file?"file":"dir", src_name));

	if (!NT_STATUS_IS_OK(cli_ntcreate(cli_share_src, src_name, 0, READ_CONTROL_ACCESS, 0,
				FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum_src))) {
		DEBUGADD(0,("cannot open %s %s on originating server %s\n",
			is_file?"file":"dir", src_name, cli_errstr(cli_share_src)));
		nt_status = cli_nt_error(cli_share_src);
		goto out;
	}


	if (copy_acls) {

		/* get the security descriptor */
		sd = cli_query_secdesc(cli_share_src, fnum_src, mem_ctx);
		if (!sd) {
			DEBUG(0,("failed to get security descriptor: %s\n",
				cli_errstr(cli_share_src)));
			nt_status = cli_nt_error(cli_share_src);
			goto out;
		}

		if (c->opt_verbose && DEBUGLEVEL >= 3)
			display_sec_desc(sd);
	}


	if (copy_attrs || copy_timestamps) {

		/* get file attributes */
		if (!NT_STATUS_IS_OK(cli_getattrE(cli_share_src, fnum_src, &attr, NULL,
				 &f_ctime, &f_atime, &f_mtime))) {
			DEBUG(0,("failed to get file-attrs: %s\n",
				cli_errstr(cli_share_src)));
			nt_status = cli_nt_error(cli_share_src);
			goto out;
		}
	}


	/* open the file/dir on the destination server */

	if (!NT_STATUS_IS_OK(cli_ntcreate(cli_share_dst, dst_name, 0, WRITE_DAC_ACCESS | WRITE_OWNER_ACCESS, 0,
				FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum_dst))) {
		DEBUG(0,("failed to open %s on the destination server: %s: %s\n",
			is_file?"file":"dir", dst_name, cli_errstr(cli_share_dst)));
		nt_status = cli_nt_error(cli_share_dst);
		goto out;
	}

	if (copy_timestamps) {

		/* set timestamps */
		if (!NT_STATUS_IS_OK(cli_setattrE(cli_share_dst, fnum_dst, f_ctime, f_atime, f_mtime))) {
			DEBUG(0,("failed to set file-attrs (timestamps): %s\n",
				cli_errstr(cli_share_dst)));
			nt_status = cli_nt_error(cli_share_dst);
			goto out;
		}
	}

	if (copy_acls) {
		NTSTATUS status;

		/* set acls */
		status = cli_set_secdesc(cli_share_dst, fnum_dst, sd);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("could not set secdesc on %s: %s\n",
				  dst_name, nt_errstr(status)));
			nt_status = status;
			goto out;
		}
	}

	if (copy_attrs) {

		/* set attrs */
		if (!NT_STATUS_IS_OK(cli_setatr(cli_share_dst, dst_name, attr, 0))) {
			DEBUG(0,("failed to set file-attrs: %s\n",
				cli_errstr(cli_share_dst)));
			nt_status = cli_nt_error(cli_share_dst);
			goto out;
		}
	}


	/* closing files */

	if (!NT_STATUS_IS_OK(cli_close(cli_share_src, fnum_src))) {
		d_fprintf(stderr,
			_("could not close %s on originating server: %s\n"),
			is_file?"file":"dir", cli_errstr(cli_share_src));
		nt_status = cli_nt_error(cli_share_src);
		goto out;
	}

	if (!NT_STATUS_IS_OK(cli_close(cli_share_dst, fnum_dst))) {
		d_fprintf(stderr,
			_("could not close %s on destination server: %s\n"),
			is_file?"file":"dir", cli_errstr(cli_share_dst));
		nt_status = cli_nt_error(cli_share_dst);
		goto out;
	}


	nt_status = NT_STATUS_OK;

out:

	/* cleaning up */
	if (fnum_src)
		cli_close(cli_share_src, fnum_src);

	if (fnum_dst)
		cli_close(cli_share_dst, fnum_dst);

	return nt_status;
}

/**
 * Copy a file or directory from a connected share to another connected share
 *
 * @param c			A net_context structure
 * @param mem_ctx		A talloc-context
 * @param cli_share_src		A connected cli_state
 * @param cli_share_dst		A connected cli_state
 * @param src_file		The source file-name
 * @param dst_file		The destination file-name
 * @param copy_acls		Whether to copy acls
 * @param copy_attrs		Whether to copy DOS attributes
 * @param copy_timestamps	Whether to preserve timestamps
 * @param is_file		Whether this file is a file or a dir
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS net_copy_file(struct net_context *c,
		       TALLOC_CTX *mem_ctx,
		       struct cli_state *cli_share_src,
		       struct cli_state *cli_share_dst,
		       const char *src_name, const char *dst_name,
		       bool copy_acls, bool copy_attrs,
		       bool copy_timestamps, bool is_file)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint16_t fnum_src = 0;
	uint16_t fnum_dst = 0;
	static int io_bufsize = 64512;
	int read_size = io_bufsize;
	char *data = NULL;
	off_t nread = 0;


	if (!src_name || !dst_name)
		goto out;

	if (cli_share_src == NULL || cli_share_dst == NULL)
		goto out;

	/* open on the originating server */
	DEBUGADD(3,("opening %s %s on originating server\n",
		is_file ? "file":"dir", src_name));
	if (is_file)
		nt_status = cli_open(cli_share_src, src_name, O_RDONLY, DENY_NONE, &fnum_src);
	else
		nt_status = cli_ntcreate(cli_share_src, src_name, 0, READ_CONTROL_ACCESS, 0,
				FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum_src);

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUGADD(0,("cannot open %s %s on originating server %s\n",
			is_file ? "file":"dir",
			src_name, cli_errstr(cli_share_src)));
		goto out;
	}


	if (is_file) {

		/* open file on the destination server */
		DEBUGADD(3,("opening file %s on destination server\n", dst_name));
		nt_status = cli_open(cli_share_dst, dst_name,
				O_RDWR|O_CREAT|O_TRUNC, DENY_NONE, &fnum_dst);

		if (!NT_STATUS_IS_OK(nt_status)) {
			DEBUGADD(1,("cannot create file %s on destination server: %s\n", 
				dst_name, cli_errstr(cli_share_dst)));
			goto out;
		}

		/* allocate memory */
		if (!(data = (char *)SMB_MALLOC(read_size))) {
			d_fprintf(stderr, _("malloc fail for size %d\n"),
				  read_size);
			nt_status = NT_STATUS_NO_MEMORY;
			goto out;
		}

	}


	if (c->opt_verbose) {

		d_printf(_("copying [\\\\%s\\%s%s] => [\\\\%s\\%s%s] "
			   "%s ACLs and %s DOS Attributes %s\n"),
			cli_share_src->desthost, cli_share_src->share, src_name,
			cli_share_dst->desthost, cli_share_dst->share, dst_name,
			copy_acls ?  _("with") : _("without"),
			copy_attrs ? _("with") : _("without"),
			copy_timestamps ? _("(preserving timestamps)") : "" );
	}


	while (is_file) {

		/* copying file */
		int n;
		n = cli_read(cli_share_src, fnum_src, data, nread,
				read_size);

		if (n <= 0)
			break;

		nt_status = cli_writeall(cli_share_dst, fnum_dst, 0,
					 (uint8_t *)data, nread, n, NULL);

		if (!NT_STATUS_IS_OK(nt_status)) {
			d_fprintf(stderr, _("Error writing file: %s\n"),
				  nt_errstr(nt_status));
			goto out;
		}

		nread += n;
	}


	if (!is_file && !NT_STATUS_IS_OK(cli_chkpath(cli_share_dst, dst_name))) {

		/* creating dir */
		DEBUGADD(3,("creating dir %s on the destination server\n",
			dst_name));

		if (!NT_STATUS_IS_OK(cli_mkdir(cli_share_dst, dst_name))) {
			DEBUG(0,("cannot create directory %s: %s\n",
				dst_name, cli_errstr(cli_share_dst)));
			nt_status = NT_STATUS_NO_SUCH_FILE;
		}

		if (!NT_STATUS_IS_OK(cli_chkpath(cli_share_dst, dst_name))) {
			d_fprintf(stderr,
				_("cannot check for directory %s: %s\n"),
				dst_name, cli_errstr(cli_share_dst));
			goto out;
		}
	}


	/* closing files */
	if (!NT_STATUS_IS_OK(cli_close(cli_share_src, fnum_src))) {
		d_fprintf(stderr,
			_("could not close file on originating server: %s\n"),
			cli_errstr(cli_share_src));
		nt_status = cli_nt_error(cli_share_src);
		goto out;
	}

	if (is_file && !NT_STATUS_IS_OK(cli_close(cli_share_dst, fnum_dst))) {
		d_fprintf(stderr,
			_("could not close file on destination server: %s\n"),
			cli_errstr(cli_share_dst));
		nt_status = cli_nt_error(cli_share_dst);
		goto out;
	}

	/* possibly we have to copy some file-attributes / acls / sd */
	nt_status = net_copy_fileattr(c, mem_ctx, cli_share_src, cli_share_dst,
				      src_name, dst_name, copy_acls,
				      copy_attrs, copy_timestamps, is_file);
	if (!NT_STATUS_IS_OK(nt_status))
		goto out;


	nt_status = NT_STATUS_OK;

out:

	/* cleaning up */
	if (fnum_src)
		cli_close(cli_share_src, fnum_src);

	if (fnum_dst)
		cli_close(cli_share_dst, fnum_dst);

	SAFE_FREE(data);

	return nt_status;
}

/**
 * Copy a driverfile from on connected share to another connected share
 * This silently assumes that a driver-file is picked up from
 *
 *	\\src_server\print$\{arch}\{version}\file
 *
 * and copied to
 *
 * 	\\dst_server\print$\{arch}\file
 *
 * to be added via setdriver-calls later.
 * @param c			A net_context structure
 * @param mem_ctx		A talloc-context
 * @param cli_share_src		A cli_state connected to source print$-share
 * @param cli_share_dst		A cli_state connected to destination print$-share
 * @param file			The file-name to be copied
 * @param short_archi		The name of the driver-architecture (short form)
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS net_copy_driverfile(struct net_context *c,
				    TALLOC_CTX *mem_ctx,
				    struct cli_state *cli_share_src,
				    struct cli_state *cli_share_dst,
				    const char *file, const char *short_archi) {

	const char *p;
	char *src_name;
	char *dst_name;
	char *version = NULL;
	char *filename = NULL;
	char *tok;

	if (!file) {
		return NT_STATUS_OK;
	}

	/* scroll through the file until we have the part
	   beyond archi_table.short_archi */
	p = file;
	while (next_token_talloc(mem_ctx, &p, &tok, "\\")) {
		if (strequal(tok, short_archi)) {
			next_token_talloc(mem_ctx, &p, &version, "\\");
			next_token_talloc(mem_ctx, &p, &filename, "\\");
		}
	}

	if (version == NULL || filename == NULL) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* build source file name */
	src_name = talloc_asprintf(mem_ctx, "\\%s\\%s\\%s",
				   short_archi, version, filename);
	if (src_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* create destination file name */
	dst_name = talloc_asprintf(mem_ctx, "\\%s\\%s", short_archi, filename);
	if (dst_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}


	/* finally copy the file */
	return net_copy_file(c, mem_ctx, cli_share_src, cli_share_dst,
			     src_name, dst_name, false, false, false, true);
}

/**
 * Check for existing Architecture directory on a given server
 *
 * @param cli_share		A cli_state connected to a print$-share
 * @param short_archi		The Architecture for the print-driver
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS check_arch_dir(struct cli_state *cli_share, const char *short_archi)
{

	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	char *dir;

	if (asprintf(&dir, "\\%s", short_archi) < 0) {
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(10,("creating print-driver dir for architecture: %s\n",
		short_archi));

	if (!NT_STATUS_IS_OK(cli_mkdir(cli_share, dir))) {
                DEBUG(1,("cannot create directory %s: %s\n",
                         dir, cli_errstr(cli_share)));
                nt_status = NT_STATUS_NO_SUCH_FILE;
        }

	if (!NT_STATUS_IS_OK(cli_chkpath(cli_share, dir))) {
		d_fprintf(stderr, _("cannot check %s: %s\n"),
			dir, cli_errstr(cli_share));
		goto out;
	}

	nt_status = NT_STATUS_OK;

out:
	SAFE_FREE(dir);
	return nt_status;
}

/**
 * Copy a print-driver (level 3) from one connected print$-share to another
 * connected print$-share
 *
 * @param c			A net_context structure
 * @param mem_ctx		A talloc-context
 * @param cli_share_src		A cli_state connected to a print$-share
 * @param cli_share_dst		A cli_state connected to a print$-share
 * @param short_archi		The Architecture for the print-driver
 * @param i1			The DRIVER_INFO_3-struct
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS copy_print_driver_3(struct net_context *c,
		    TALLOC_CTX *mem_ctx,
		    struct cli_state *cli_share_src,
		    struct cli_state *cli_share_dst,
		    const char *short_archi,
		    struct spoolss_DriverInfo3 *r)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	int i;

	if (r == NULL) {
		return nt_status;
	}

	if (c->opt_verbose)
		d_printf(_("copying driver: [%s], for architecture: [%s], "
			   "version: [%d]\n"),
			  r->driver_name, short_archi, r->version);

	nt_status = net_copy_driverfile(c, mem_ctx, cli_share_src, cli_share_dst,
		r->driver_path, short_archi);
	if (!NT_STATUS_IS_OK(nt_status))
		return nt_status;

	nt_status = net_copy_driverfile(c, mem_ctx, cli_share_src, cli_share_dst,
		r->data_file, short_archi);
	if (!NT_STATUS_IS_OK(nt_status))
		return nt_status;

	nt_status = net_copy_driverfile(c, mem_ctx, cli_share_src, cli_share_dst,
		r->config_file, short_archi);
	if (!NT_STATUS_IS_OK(nt_status))
		return nt_status;

	nt_status = net_copy_driverfile(c, mem_ctx, cli_share_src, cli_share_dst,
		r->help_file, short_archi);
	if (!NT_STATUS_IS_OK(nt_status))
		return nt_status;

	for (i=0; r->dependent_files[i] != NULL; i++) {

		nt_status = net_copy_driverfile(c, mem_ctx,
				cli_share_src, cli_share_dst,
				r->dependent_files[i], short_archi);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	}

	return NT_STATUS_OK;
}

/**
 * net_spoolss-functions
 * =====================
 *
 * the net_spoolss-functions aim to simplify spoolss-client-functions
 * required during the migration-process wrt buffer-sizes, returned
 * error-codes, etc.
 *
 * this greatly reduces the complexitiy of the migrate-functions.
 *
 **/

static bool net_spoolss_enum_printers(struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					char *name,
					uint32_t flags,
					uint32_t level,
					uint32_t *num_printers,
					union spoolss_PrinterInfo **info)
{
	WERROR result;

	/* enum printers */

	result = rpccli_spoolss_enumprinters(pipe_hnd, mem_ctx,
					     flags,
					     name,
					     level,
					     0,
					     num_printers,
					     info);
	if (!W_ERROR_IS_OK(result)) {
		printf(_("cannot enum printers: %s\n"), win_errstr(result));
		return false;
	}

	return true;
}

static bool net_spoolss_open_printer_ex(struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					const char *printername,
					uint32_t access_required,
					const char *username,
					struct policy_handle *hnd)
{
	WERROR result;
	fstring printername2;

	fstrcpy(printername2, pipe_hnd->srv_name_slash);
	fstrcat(printername2, "\\");
	fstrcat(printername2, printername);

	DEBUG(10,("connecting to: %s as %s for %s and access: %x\n",
		pipe_hnd->srv_name_slash, username, printername2, access_required));

	/* open printer */
	result = rpccli_spoolss_openprinter_ex(pipe_hnd, mem_ctx,
					       printername2,
					       access_required,
					       hnd);

	/* be more verbose */
	if (W_ERROR_V(result) == W_ERROR_V(WERR_ACCESS_DENIED)) {
		d_fprintf(stderr,
			_("no access to printer [%s] on [%s] for user [%s] "
			  "granted\n"),
			printername2, pipe_hnd->srv_name_slash, username);
		return false;
	}

	if (!W_ERROR_IS_OK(result)) {
		d_fprintf(stderr,_("cannot open printer %s on server %s: %s\n"),
			printername2, pipe_hnd->srv_name_slash, win_errstr(result));
		return false;
	}

	DEBUG(2,("got printer handle for printer: %s, server: %s\n",
		printername2, pipe_hnd->srv_name_slash));

	return true;
}

static bool net_spoolss_getprinter(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *hnd,
				uint32_t level,
				union spoolss_PrinterInfo *info)
{
	WERROR result;

	/* getprinter call */
	result = rpccli_spoolss_getprinter(pipe_hnd, mem_ctx,
					   hnd,
					   level,
					   0, /* offered */
					   info);
	if (!W_ERROR_IS_OK(result)) {
		printf(_("cannot get printer-info: %s\n"), win_errstr(result));
		return false;
	}

	return true;
}

static bool net_spoolss_setprinter(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *hnd,
				uint32_t level,
				union spoolss_PrinterInfo *info)
{
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;
	WERROR result;
	NTSTATUS status;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_SetPrinterInfo2 info2;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	/* setprinter call */

	info_ctr.level = level;
	switch (level) {
	case 0:
		info_ctr.info.info0 = (struct spoolss_SetPrinterInfo0 *)
			(void *)&info->info0;
		break;
	case 1:
		info_ctr.info.info1 = (struct spoolss_SetPrinterInfo1 *)
			(void *)&info->info1;
		break;
	case 2:
		spoolss_printerinfo2_to_setprinterinfo2(&info->info2, &info2);
		info_ctr.info.info2 = &info2;
		break;
	case 3:
		info_ctr.info.info3 = (struct spoolss_SetPrinterInfo3 *)
			(void *)&info->info3;
		break;
	case 4:
		info_ctr.info.info4 = (struct spoolss_SetPrinterInfo4 *)
			(void *)&info->info4;
		break;
	case 5:
		info_ctr.info.info5 = (struct spoolss_SetPrinterInfo5 *)
			(void *)&info->info5;
		break;
	case 6:
		info_ctr.info.info6 = (struct spoolss_SetPrinterInfo6 *)
			(void *)&info->info6;
		break;
	case 7:
		info_ctr.info.info7 = (struct spoolss_SetPrinterInfo7 *)
			(void *)&info->info7;
		break;
#if 0 /* FIXME GD */
	case 8:
		info_ctr.info.info8 = (struct spoolss_SetPrinterInfo8 *)
			(void *)&info->info8;
		break;
	case 9:
		info_ctr.info.info9 = (struct spoolss_SetPrinterInfo9 *)
			(void *)&info->info9;
		break;
#endif
	default:
		break; /* FIXME */
	}

	status = dcerpc_spoolss_SetPrinter(b, mem_ctx,
					   hnd,
					   &info_ctr,
					   &devmode_ctr,
					   &secdesc_ctr,
					   0, /* command */
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		printf(_("cannot set printer-info: %s\n"), nt_errstr(status));
		return false;
	}
	if (!W_ERROR_IS_OK(result)) {
		printf(_("cannot set printer-info: %s\n"), win_errstr(result));
		return false;
	}

	return true;
}


static bool net_spoolss_setprinterdata(struct rpc_pipe_client *pipe_hnd,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *hnd,
				       const char *value_name,
				       enum winreg_Type type,
				       uint8_t *data,
				       uint32_t offered)
{
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;
	WERROR result;
	NTSTATUS status;

	/* setprinterdata call */
	status = dcerpc_spoolss_SetPrinterData(b, mem_ctx,
					       hnd,
					       value_name,
					       type,
					       data,
					       offered,
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		printf (_("unable to set printerdata: %s\n"),
			nt_errstr(status));
		return false;
	}
	if (!W_ERROR_IS_OK(result)) {
		printf (_("unable to set printerdata: %s\n"),
			win_errstr(result));
		return false;
	}

	return true;
}


static bool net_spoolss_enumprinterkey(struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *hnd,
					const char *keyname,
					const char ***keylist)
{
	WERROR result;

	/* enumprinterkey call */
	result = rpccli_spoolss_enumprinterkey(pipe_hnd, mem_ctx, hnd, keyname, keylist, 0);

	if (!W_ERROR_IS_OK(result)) {
		printf(_("enumprinterkey failed: %s\n"), win_errstr(result));
		return false;
	}

	return true;
}

static bool net_spoolss_enumprinterdataex(struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					uint32_t offered,
					struct policy_handle *hnd,
					const char *keyname,
					uint32_t *count,
					struct spoolss_PrinterEnumValues **info)
{
	WERROR result;

	/* enumprinterdataex call */
	result = rpccli_spoolss_enumprinterdataex(pipe_hnd, mem_ctx,
						  hnd,
						  keyname,
						  0, /* offered */
						  count,
						  info);

	if (!W_ERROR_IS_OK(result)) {
		printf(_("enumprinterdataex failed: %s\n"), win_errstr(result));
		return false;
	}

	return true;
}


static bool net_spoolss_setprinterdataex(struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *hnd,
					const char *keyname,
					struct regval_blob *value)
{
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;
	WERROR result;
	NTSTATUS status;

	/* setprinterdataex call */
	status = dcerpc_spoolss_SetPrinterDataEx(b, mem_ctx,
						 hnd,
						 keyname,
						 regval_name(value),
						 regval_type(value),
						 regval_data_p(value),
						 regval_size(value),
						 &result);
	if (!NT_STATUS_IS_OK(status)) {
		printf(_("could not set printerdataex: %s\n"),
		       nt_errstr(status));
		return false;
	}
	if (!W_ERROR_IS_OK(result)) {
		printf(_("could not set printerdataex: %s\n"),
		       win_errstr(result));
		return false;
	}

	return true;
}

static bool net_spoolss_enumforms(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *hnd,
				int level,
				uint32_t *num_forms,
				union spoolss_FormInfo **forms)
{
	WERROR result;

	/* enumforms call */
	result = rpccli_spoolss_enumforms(pipe_hnd, mem_ctx,
					  hnd,
					  level,
					  0,
					  num_forms,
					  forms);
	if (!W_ERROR_IS_OK(result)) {
		printf(_("could not enum forms: %s\n"), win_errstr(result));
		return false;
	}

	return true;
}

static bool net_spoolss_enumprinterdrivers (struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					uint32_t level, const char *env,
					uint32_t *count,
					union spoolss_DriverInfo **info)
{
	WERROR result;

	/* enumprinterdrivers call */
	result = rpccli_spoolss_enumprinterdrivers(pipe_hnd, mem_ctx,
						   pipe_hnd->srv_name_slash,
						   env,
						   level,
						   0,
						   count,
						   info);
	if (!W_ERROR_IS_OK(result)) {
		if (W_ERROR_V(result) != W_ERROR_V(WERR_INVALID_ENVIRONMENT)) {
			printf(_("cannot enum drivers for environment %s: %s\n"), env,
				win_errstr(result));
			return false;
		} else {
			printf(_("Server does not support environment [%s]\n"),
				env);
		}
	}

	return true;
}

static bool net_spoolss_getprinterdriver(struct rpc_pipe_client *pipe_hnd,
			     TALLOC_CTX *mem_ctx,
			     struct policy_handle *hnd, uint32_t level,
			     const char *env, int version,
			     union spoolss_DriverInfo *info)
{
	WERROR result;
	uint32_t server_major_version;
	uint32_t server_minor_version;

	/* getprinterdriver call */
	result = rpccli_spoolss_getprinterdriver2(pipe_hnd, mem_ctx,
						  hnd,
						  env,
						  level,
						  0,
						  version,
						  2,
						  info,
						  &server_major_version,
						  &server_minor_version);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(1,("cannot get driver (for architecture: %s): %s\n",
			env, win_errstr(result)));
		if (W_ERROR_V(result) != W_ERROR_V(WERR_UNKNOWN_PRINTER_DRIVER) &&
		    W_ERROR_V(result) != W_ERROR_V(WERR_INVALID_ENVIRONMENT)) {
			printf(_("cannot get driver: %s\n"),
			       win_errstr(result));
		}
		return false;
	}

	return true;
}


static bool net_spoolss_addprinterdriver(struct rpc_pipe_client *pipe_hnd,
			     TALLOC_CTX *mem_ctx, uint32_t level,
			     union spoolss_DriverInfo *info)
{
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;
	WERROR result;
	NTSTATUS status;
	struct spoolss_AddDriverInfoCtr info_ctr;

	info_ctr.level = level;

	switch (level) {
	case 2:
		info_ctr.info.info2 = (struct spoolss_AddDriverInfo2 *)
			(void *)&info->info2;
		break;
	case 3:
		info_ctr.info.info3 = (struct spoolss_AddDriverInfo3 *)
			(void *)&info->info3;
		break;
	default:
		printf(_("unsupported info level: %d\n"), level);
		return false;
	}

	/* addprinterdriver call */
	status = dcerpc_spoolss_AddPrinterDriver(b, mem_ctx,
						 pipe_hnd->srv_name_slash,
						 &info_ctr,
						 &result);
	if (!NT_STATUS_IS_OK(status)) {
		printf(_("cannot add driver: %s\n"), nt_errstr(status));
		return false;
	}
	/* be more verbose */
	if (W_ERROR_V(result) == W_ERROR_V(WERR_ACCESS_DENIED)) {
		printf(_("You are not allowed to add drivers\n"));
		return false;
	}
	if (!W_ERROR_IS_OK(result)) {
		printf(_("cannot add driver: %s\n"), win_errstr(result));
		return false;
	}

	return true;
}

/**
 * abstraction function to get uint32_t num_printers and PRINTER_INFO_CTR ctr
 * for a single printer or for all printers depending on argc/argv
 **/

static bool get_printer_info(struct rpc_pipe_client *pipe_hnd,
			TALLOC_CTX *mem_ctx,
			int level,
			int argc,
			const char **argv,
			uint32_t *num_printers,
			union spoolss_PrinterInfo **info_p)
{
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;
	struct policy_handle hnd;
	WERROR werr;

	/* no arguments given, enumerate all printers */
	if (argc == 0) {

		if (!net_spoolss_enum_printers(pipe_hnd, mem_ctx, NULL,
				PRINTER_ENUM_LOCAL|PRINTER_ENUM_SHARED,
				level, num_printers, info_p))
			return false;

		goto out;
	}

	/* argument given, get a single printer by name */
	if (!net_spoolss_open_printer_ex(pipe_hnd, mem_ctx, argv[0],
					 MAXIMUM_ALLOWED_ACCESS,
					 pipe_hnd->auth->user_name,
					 &hnd))
		return false;

	*info_p = talloc_zero(mem_ctx, union spoolss_PrinterInfo);
	if (*info_p == NULL) {
		return false;
	}

	if (!net_spoolss_getprinter(pipe_hnd, mem_ctx, &hnd, level, *info_p)) {
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &werr);
		return false;
	}

	dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &werr);

	*num_printers = 1;

out:
	DEBUG(3,("got %d printers\n", *num_printers));

	return true;

}

/**
 * List print-queues (including local printers that are not shared)
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure
 * @param domain_sid The domain sid aquired from the remote server
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destoyed on compleation of the function.
 * @param argc  Standard main() style argc
 * @param argv  Standard main() style argv.  Initial components are already
 *              stripped
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_printer_list_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint32_t i, num_printers;
	uint32_t level = 2;
	const char *printername, *sharename;
	union spoolss_PrinterInfo *info;

	printf("listing printers\n");

	if (!get_printer_info(pipe_hnd, mem_ctx, level, argc, argv, &num_printers, &info))
		return nt_status;

	for (i = 0; i < num_printers; i++) {

		/* do some initialization */
		printername = info[i].info2.printername;
		sharename = info[i].info2.sharename;

		if (printername && sharename) {
			d_printf(_("printer %d: %s, shared as: %s\n"),
				i+1, printername, sharename);
		}
	}

	return NT_STATUS_OK;
}

/**
 * List printer-drivers from a server
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure
 * @param domain_sid The domain sid aquired from the remote server
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destoyed on compleation of the function.
 * @param argc  Standard main() style argc
 * @param argv  Standard main() style argv.  Initial components are already
 *              stripped
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_printer_driver_list_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint32_t i;
	uint32_t level = 3;
	union spoolss_DriverInfo *info;
	int d;

	printf(_("listing printer-drivers\n"));

        for (i=0; archi_table[i].long_archi!=NULL; i++) {

		uint32_t num_drivers;

		/* enum remote drivers */
		if (!net_spoolss_enumprinterdrivers(pipe_hnd, mem_ctx, level,
				archi_table[i].long_archi,
				&num_drivers, &info)) {
			nt_status = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		if (num_drivers == 0) {
			d_printf(_("no drivers found on server for "
				   "architecture: [%s].\n"),
				archi_table[i].long_archi);
			continue;
		}

		d_printf(_("got %d printer-drivers for architecture: [%s]\n"),
			num_drivers, archi_table[i].long_archi);


		/* do something for all drivers for architecture */
		for (d = 0; d < num_drivers; d++) {
			display_print_driver3(&info[d].info3);
		}
	}

	nt_status = NT_STATUS_OK;

done:
	return nt_status;

}

/**
 * Publish print-queues with args-wrapper
 *
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destoyed on compleation of the function.
 * @param argc  Standard main() style argc
 * @param argv  Standard main() style argv.  Initial components are already
 *              stripped
 * @param action
 *
 * @return Normal NTSTATUS return.
 **/

static NTSTATUS rpc_printer_publish_internals_args(struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv,
					uint32_t action)
{
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint32_t i, num_printers;
	uint32_t level = 7;
	const char *printername, *sharename;
	union spoolss_PrinterInfo *info_enum;
	union spoolss_PrinterInfo info;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	struct policy_handle hnd;
	WERROR result;
	const char *action_str;

	if (!get_printer_info(pipe_hnd, mem_ctx, 2, argc, argv, &num_printers, &info_enum))
		return nt_status;

	for (i = 0; i < num_printers; i++) {

		/* do some initialization */
		printername = info_enum[i].info2.printername;
		sharename = info_enum[i].info2.sharename;
		if (!printername || !sharename) {
			goto done;
		}

		/* open printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd, mem_ctx, sharename,
			PRINTER_ALL_ACCESS, pipe_hnd->auth->user_name, &hnd))
			goto done;

		/* check for existing dst printer */
		if (!net_spoolss_getprinter(pipe_hnd, mem_ctx, &hnd, level, &info))
			goto done;

		/* check action and set string */
		switch (action) {
		case DSPRINT_PUBLISH:
			action_str = N_("published");
			break;
		case DSPRINT_UPDATE:
			action_str = N_("updated");
			break;
		case DSPRINT_UNPUBLISH:
			action_str = N_("unpublished");
			break;
		default:
			action_str = N_("unknown action");
			printf(_("unknown action: %d\n"), action);
			break;
		}

		info.info7.action = action;
		info_ctr.level = 7;
		info_ctr.info.info7 = (struct spoolss_SetPrinterInfo7 *)
			(void *)&info.info7;

		ZERO_STRUCT(devmode_ctr);
		ZERO_STRUCT(secdesc_ctr);

		nt_status = dcerpc_spoolss_SetPrinter(b, mem_ctx,
						      &hnd,
						      &info_ctr,
						      &devmode_ctr,
						      &secdesc_ctr,
						      0, /* command */
						      &result);
		if (!NT_STATUS_IS_OK(nt_status)) {
			printf(_("cannot set printer-info: %s\n"),
			       nt_errstr(nt_status));
			goto done;
		}
		if (!W_ERROR_IS_OK(result) && !W_ERROR_EQUAL(result, WERR_IO_PENDING)) {
			if ((action == DSPRINT_UPDATE) && W_ERROR_EQUAL(result, W_ERROR(0x80070002))) {
				printf(_("printer not published yet\n"));
			} else {
				printf(_("cannot set printer-info: %s\n"),
				       win_errstr(result));
			}
			nt_status = werror_to_ntstatus(result);
			goto done;
		}

		printf(_("successfully %s printer %s in Active Directory\n"),
		       action_str, sharename);
	}

	nt_status = NT_STATUS_OK;

done:
	if (is_valid_policy_hnd(&hnd)) {
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &result);
	}

	return nt_status;
}

NTSTATUS rpc_printer_publish_publish_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	return rpc_printer_publish_internals_args(pipe_hnd, mem_ctx, argc, argv, DSPRINT_PUBLISH);
}

NTSTATUS rpc_printer_publish_unpublish_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	return rpc_printer_publish_internals_args(pipe_hnd, mem_ctx, argc, argv, DSPRINT_UNPUBLISH);
}

NTSTATUS rpc_printer_publish_update_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	return rpc_printer_publish_internals_args(pipe_hnd, mem_ctx, argc, argv, DSPRINT_UPDATE);
}

/**
 * List print-queues w.r.t. their publishing state
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure
 * @param domain_sid The domain sid aquired from the remote server
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destoyed on compleation of the function.
 * @param argc  Standard main() style argc
 * @param argv  Standard main() style argv.  Initial components are already
 *              stripped
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_printer_publish_list_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint32_t i, num_printers;
	uint32_t level = 7;
	const char *printername, *sharename;
	union spoolss_PrinterInfo *info_enum;
	union spoolss_PrinterInfo info;
	struct policy_handle hnd;
	int state;
	WERROR werr;

	if (!get_printer_info(pipe_hnd, mem_ctx, 2, argc, argv, &num_printers, &info_enum))
		return nt_status;

	for (i = 0; i < num_printers; i++) {

		/* do some initialization */
		printername = info_enum[i].info2.printername;
		sharename = info_enum[i].info2.sharename;

		if (!printername || !sharename) {
			goto done;
		}

		/* open printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd, mem_ctx, sharename,
			PRINTER_ALL_ACCESS, cli->user_name, &hnd))
			goto done;

		/* check for existing dst printer */
		if (!net_spoolss_getprinter(pipe_hnd, mem_ctx, &hnd, level, &info))
			goto done;

		if (!info.info7.guid) {
			goto done;
		}
		state = info.info7.action;
		switch (state) {
			case DSPRINT_PUBLISH:
				printf(_("printer [%s] is published"),
				       sharename);
				if (c->opt_verbose)
					printf(_(", guid: %s"),info.info7.guid);
				printf("\n");
				break;
			case DSPRINT_UNPUBLISH:
				printf(_("printer [%s] is unpublished\n"),
				       sharename);
				break;
			case DSPRINT_UPDATE:
				printf(_("printer [%s] is currently updating\n"),
				       sharename);
				break;
			default:
				printf(_("unknown state: %d\n"), state);
				break;
		}
	}

	nt_status = NT_STATUS_OK;

done:
	if (is_valid_policy_hnd(&hnd)) {
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &werr);
	}

	return nt_status;
}

/**
 * Migrate Printer-ACLs from a source server to the destination server
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure
 * @param domain_sid The domain sid aquired from the remote server
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destoyed on compleation of the function.
 * @param argc  Standard main() style argc
 * @param argv  Standard main() style argv.  Initial components are already
 *              stripped
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_printer_migrate_security_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	struct dcerpc_binding_handle *b_src = pipe_hnd->binding_handle;
	/* TODO: what now, info2 or info3 ?
	   convince jerry that we should add clientside setacls level 3 at least
	*/
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint32_t i = 0;
	uint32_t num_printers;
	uint32_t level = 2;
	const char *printername, *sharename;
	struct rpc_pipe_client *pipe_hnd_dst = NULL;
	struct dcerpc_binding_handle *b_dst = NULL;
	struct policy_handle hnd_src, hnd_dst;
	union spoolss_PrinterInfo *info_enum;
	struct cli_state *cli_dst = NULL;
	union spoolss_PrinterInfo info_src, info_dst;
	WERROR werr;

	DEBUG(3,("copying printer ACLs\n"));

	/* connect destination PI_SPOOLSS */
	nt_status = connect_dst_pipe(c, &cli_dst, &pipe_hnd_dst,
				     &ndr_table_spoolss.syntax_id);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	b_dst = pipe_hnd_dst->binding_handle;

	/* enum source printers */
	if (!get_printer_info(pipe_hnd, mem_ctx, level, argc, argv, &num_printers, &info_enum)) {
		nt_status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (!num_printers) {
		printf (_("no printers found on server.\n"));
		nt_status = NT_STATUS_OK;
		goto done;
	}

	/* do something for all printers */
	for (i = 0; i < num_printers; i++) {

		/* do some initialization */
		printername = info_enum[i].info2.printername;
		sharename = info_enum[i].info2.sharename;

		if (!printername || !sharename) {
			nt_status = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		/* we can reset NT_STATUS here because we do not
		   get any real NT_STATUS-codes anymore from now on */
		nt_status = NT_STATUS_UNSUCCESSFUL;

		d_printf(_("migrating printer ACLs for:     [%s] / [%s]\n"),
			printername, sharename);

		/* according to msdn you have specify these access-rights
		   to see the security descriptor
			- READ_CONTROL (DACL)
			- ACCESS_SYSTEM_SECURITY (SACL)
		*/

		/* open src printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd, mem_ctx, sharename,
			MAXIMUM_ALLOWED_ACCESS, cli->user_name, &hnd_src))
			goto done;

		/* open dst printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd_dst, mem_ctx, sharename,
			PRINTER_ALL_ACCESS, cli_dst->user_name, &hnd_dst))
			goto done;

		/* check for existing dst printer */
		if (!net_spoolss_getprinter(pipe_hnd_dst, mem_ctx, &hnd_dst, level, &info_dst))
			goto done;

		/* check for existing src printer */
		if (!net_spoolss_getprinter(pipe_hnd, mem_ctx, &hnd_src, 3, &info_src))
			goto done;

		/* Copy Security Descriptor */

		/* copy secdesc (info level 2) */
		info_dst.info2.devmode = NULL;
		info_dst.info2.secdesc = dup_sec_desc(mem_ctx, info_src.info3.secdesc);

		if (c->opt_verbose)
			display_sec_desc(info_dst.info2.secdesc);

		if (!net_spoolss_setprinter(pipe_hnd_dst, mem_ctx, &hnd_dst, 2, &info_dst))
			goto done;

		DEBUGADD(1,("\tSetPrinter of SECDESC succeeded\n"));


		/* close printer handles here */
		if (is_valid_policy_hnd(&hnd_src)) {
			dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &werr);
		}

		if (is_valid_policy_hnd(&hnd_dst)) {
			dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &werr);
		}

	}

	nt_status = NT_STATUS_OK;

done:

	if (is_valid_policy_hnd(&hnd_src)) {
		dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &werr);
	}

	if (is_valid_policy_hnd(&hnd_dst)) {
		dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &werr);
	}

	if (cli_dst) {
		cli_shutdown(cli_dst);
	}
	return nt_status;
}

/**
 * Migrate printer-forms from a src server to the dst server
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure
 * @param domain_sid The domain sid aquired from the remote server
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destoyed on compleation of the function.
 * @param argc  Standard main() style argc
 * @param argv  Standard main() style argv.  Initial components are already
 *              stripped
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_printer_migrate_forms_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	struct dcerpc_binding_handle *b_src = pipe_hnd->binding_handle;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	WERROR result;
	uint32_t i, f;
	uint32_t num_printers;
	uint32_t level = 1;
	const char *printername, *sharename;
	struct rpc_pipe_client *pipe_hnd_dst = NULL;
	struct dcerpc_binding_handle *b_dst = NULL;
	struct policy_handle hnd_src, hnd_dst;
	union spoolss_PrinterInfo *info_enum;
	union spoolss_PrinterInfo info_dst;
	uint32_t num_forms;
	union spoolss_FormInfo *forms;
	struct cli_state *cli_dst = NULL;

	DEBUG(3,("copying forms\n"));

	/* connect destination PI_SPOOLSS */
	nt_status = connect_dst_pipe(c, &cli_dst, &pipe_hnd_dst,
				     &ndr_table_spoolss.syntax_id);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	b_dst = pipe_hnd_dst->binding_handle;

	/* enum src printers */
	if (!get_printer_info(pipe_hnd, mem_ctx, 2, argc, argv, &num_printers, &info_enum)) {
		nt_status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (!num_printers) {
		printf (_("no printers found on server.\n"));
		nt_status = NT_STATUS_OK;
		goto done;
	}

	/* do something for all printers */
	for (i = 0; i < num_printers; i++) {

		/* do some initialization */
		printername = info_enum[i].info2.printername;
		sharename = info_enum[i].info2.sharename;

		if (!printername || !sharename) {
			nt_status = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
		/* we can reset NT_STATUS here because we do not
		   get any real NT_STATUS-codes anymore from now on */
		nt_status = NT_STATUS_UNSUCCESSFUL;

		d_printf(_("migrating printer forms for:    [%s] / [%s]\n"),
			printername, sharename);


		/* open src printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd, mem_ctx, sharename,
			MAXIMUM_ALLOWED_ACCESS, cli->user_name, &hnd_src))
			goto done;

		/* open dst printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd_dst, mem_ctx, sharename,
			PRINTER_ALL_ACCESS, cli->user_name, &hnd_dst))
			goto done;

		/* check for existing dst printer */
		if (!net_spoolss_getprinter(pipe_hnd_dst, mem_ctx, &hnd_dst, level, &info_dst))
			goto done;

		/* finally migrate forms */
		if (!net_spoolss_enumforms(pipe_hnd, mem_ctx, &hnd_src, level, &num_forms, &forms))
			goto done;

		DEBUG(1,("got %d forms for printer\n", num_forms));


		for (f = 0; f < num_forms; f++) {

			union spoolss_AddFormInfo info;
			NTSTATUS status;

			/* only migrate FORM_PRINTER types, according to jerry
			   FORM_BUILTIN-types are hard-coded in samba */
			if (forms[f].info1.flags != SPOOLSS_FORM_PRINTER)
				continue;

			if (c->opt_verbose)
				d_printf(_("\tmigrating form # %d [%s] of type "
					   "[%d]\n"),
					f, forms[f].info1.form_name,
					forms[f].info1.flags);

			info.info1 = (struct spoolss_AddFormInfo1 *)
				(void *)&forms[f].info1;

			/* FIXME: there might be something wrong with samba's
			   builtin-forms */
			status = dcerpc_spoolss_AddForm(b_dst, mem_ctx,
							&hnd_dst,
							1,
							info,
							&result);
			if (!NT_STATUS_IS_OK(status)) {
				d_printf(_("\tdcerpc_spoolss_AddForm form %d: [%s] - %s\n"),
					f, forms[f].info1.form_name, nt_errstr(status));
				continue;
			}
			if (!W_ERROR_IS_OK(result)) {
				d_printf(_("\tAddForm form %d: [%s] refused.\n"),
					f, forms[f].info1.form_name);
				continue;
			}

			DEBUGADD(1,("\tAddForm of [%s] succeeded\n",
				forms[f].info1.form_name));
		}


		/* close printer handles here */
		if (is_valid_policy_hnd(&hnd_src)) {
			dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &result);
		}

		if (is_valid_policy_hnd(&hnd_dst)) {
			dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &result);
		}
	}

	nt_status = NT_STATUS_OK;

done:

	if (is_valid_policy_hnd(&hnd_src)) {
		dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &result);
	}

	if (is_valid_policy_hnd(&hnd_dst)) {
		dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &result);
	}

	if (cli_dst) {
		cli_shutdown(cli_dst);
	}
	return nt_status;
}

/**
 * Migrate printer-drivers from a src server to the dst server
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure
 * @param domain_sid The domain sid aquired from the remote server
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destoyed on compleation of the function.
 * @param argc  Standard main() style argc
 * @param argv  Standard main() style argv.  Initial components are already
 *              stripped
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_printer_migrate_drivers_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	struct dcerpc_binding_handle *b_src = pipe_hnd->binding_handle;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint32_t i, p;
	uint32_t num_printers;
	uint32_t level = 3;
	const char *printername, *sharename;
	bool got_src_driver_share = false;
	bool got_dst_driver_share = false;
	struct rpc_pipe_client *pipe_hnd_dst = NULL;
	struct dcerpc_binding_handle *b_dst = NULL;
	struct policy_handle hnd_src, hnd_dst;
	union spoolss_DriverInfo drv_info_src;
	union spoolss_PrinterInfo *info_enum;
	union spoolss_PrinterInfo info_dst;
	struct cli_state *cli_dst = NULL;
	struct cli_state *cli_share_src = NULL;
	struct cli_state *cli_share_dst = NULL;
	const char *drivername = NULL;
	WERROR werr;

	DEBUG(3,("copying printer-drivers\n"));

	nt_status = connect_dst_pipe(c, &cli_dst, &pipe_hnd_dst,
				     &ndr_table_spoolss.syntax_id);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	b_dst = pipe_hnd_dst->binding_handle;

	/* open print$-share on the src server */
	nt_status = connect_to_service(c, &cli_share_src, &cli->dest_ss,
			cli->desthost, "print$", "A:");
	if (!NT_STATUS_IS_OK(nt_status))
		goto done;

	got_src_driver_share = true;


	/* open print$-share on the dst server */
	nt_status = connect_to_service(c, &cli_share_dst, &cli_dst->dest_ss,
			cli_dst->desthost, "print$", "A:");
	if (!NT_STATUS_IS_OK(nt_status))
		return nt_status;

	got_dst_driver_share = true;


	/* enum src printers */
	if (!get_printer_info(pipe_hnd, mem_ctx, 2, argc, argv, &num_printers, &info_enum)) {
		nt_status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (num_printers == 0) {
		printf (_("no printers found on server.\n"));
		nt_status = NT_STATUS_OK;
		goto done;
	}


	/* do something for all printers */
	for (p = 0; p < num_printers; p++) {

		/* do some initialization */
		printername = info_enum[p].info2.printername;
		sharename = info_enum[p].info2.sharename;

		if (!printername || !sharename) {
			nt_status = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		/* we can reset NT_STATUS here because we do not
		   get any real NT_STATUS-codes anymore from now on */
		nt_status = NT_STATUS_UNSUCCESSFUL;

		d_printf(_("migrating printer driver for:   [%s] / [%s]\n"),
			printername, sharename);

		/* open dst printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd_dst, mem_ctx, sharename,
			PRINTER_ALL_ACCESS, cli->user_name, &hnd_dst))
			goto done;

		/* check for existing dst printer */
		if (!net_spoolss_getprinter(pipe_hnd_dst, mem_ctx, &hnd_dst, 2, &info_dst))
			goto done;


		/* open src printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd, mem_ctx, sharename,
						 MAXIMUM_ALLOWED_ACCESS,
						 pipe_hnd->auth->user_name,
						 &hnd_src))
			goto done;

		/* in a first step call getdriver for each shared printer (per arch)
		   to get a list of all files that have to be copied */

	        for (i=0; archi_table[i].long_archi!=NULL; i++) {

			/* getdriver src */
			if (!net_spoolss_getprinterdriver(pipe_hnd, mem_ctx, &hnd_src,
					level, archi_table[i].long_archi,
					archi_table[i].version, &drv_info_src))
				continue;

			drivername = drv_info_src.info3.driver_name;

			if (c->opt_verbose)
				display_print_driver3(&drv_info_src.info3);

			/* check arch dir */
			nt_status = check_arch_dir(cli_share_dst, archi_table[i].short_archi);
			if (!NT_STATUS_IS_OK(nt_status))
				goto done;


			/* copy driver-files */
			nt_status = copy_print_driver_3(c, mem_ctx, cli_share_src, cli_share_dst,
							archi_table[i].short_archi,
							&drv_info_src.info3);
			if (!NT_STATUS_IS_OK(nt_status))
				goto done;


			/* adddriver dst */
			if (!net_spoolss_addprinterdriver(pipe_hnd_dst, mem_ctx, level, &drv_info_src)) {
				nt_status = NT_STATUS_UNSUCCESSFUL;
				goto done;
			}

			DEBUGADD(1,("Sucessfully added driver [%s] for printer [%s]\n",
				drivername, printername));

		}

		if (!drivername || strlen(drivername) == 0) {
			DEBUGADD(1,("Did not get driver for printer %s\n",
				    printername));
			goto done;
		}

		/* setdriver dst */
		info_dst.info2.drivername = drivername;

		if (!net_spoolss_setprinter(pipe_hnd_dst, mem_ctx, &hnd_dst, 2, &info_dst)) {
			nt_status = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		DEBUGADD(1,("Sucessfully set driver %s for printer %s\n",
			drivername, printername));

		/* close dst */
		if (is_valid_policy_hnd(&hnd_dst)) {
			dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &werr);
		}

		/* close src */
		if (is_valid_policy_hnd(&hnd_src)) {
			dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &werr);
		}
	}

	nt_status = NT_STATUS_OK;

done:

	if (is_valid_policy_hnd(&hnd_dst)) {
		dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &werr);
	}

	/* close src */
	if (is_valid_policy_hnd(&hnd_src)) {
		dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &werr);
	}

	if (cli_dst) {
		cli_shutdown(cli_dst);
	}

	if (got_src_driver_share)
		cli_shutdown(cli_share_src);

	if (got_dst_driver_share)
		cli_shutdown(cli_share_dst);

	return nt_status;

}

/**
 * Migrate printer-queues from a src to the dst server
 * (requires a working "addprinter command" to be installed for the local smbd)
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure
 * @param domain_sid The domain sid aquired from the remote server
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destoyed on compleation of the function.
 * @param argc  Standard main() style argc
 * @param argv  Standard main() style argv.  Initial components are already
 *              stripped
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_printer_migrate_printers_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	struct dcerpc_binding_handle *b_src = pipe_hnd->binding_handle;
	WERROR result;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint32_t i = 0, num_printers;
	uint32_t level = 2;
	union spoolss_PrinterInfo info_dst, info_src;
	union spoolss_PrinterInfo *info_enum;
	struct cli_state *cli_dst = NULL;
	struct policy_handle hnd_dst, hnd_src;
	const char *printername, *sharename;
	struct rpc_pipe_client *pipe_hnd_dst = NULL;
	struct dcerpc_binding_handle *b_dst = NULL;
	struct spoolss_SetPrinterInfoCtr info_ctr;

	DEBUG(3,("copying printers\n"));

	/* connect destination PI_SPOOLSS */
	nt_status = connect_dst_pipe(c, &cli_dst, &pipe_hnd_dst,
				     &ndr_table_spoolss.syntax_id);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	b_dst = pipe_hnd_dst->binding_handle;

	/* enum printers */
	if (!get_printer_info(pipe_hnd, mem_ctx, level, argc, argv, &num_printers, &info_enum)) {
		nt_status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (!num_printers) {
		printf (_("no printers found on server.\n"));
		nt_status = NT_STATUS_OK;
		goto done;
	}

	/* do something for all printers */
	for (i = 0; i < num_printers; i++) {

		struct spoolss_SetPrinterInfo2 info2;

		/* do some initialization */
		printername = info_enum[i].info2.printername;
		sharename = info_enum[i].info2.sharename;

		if (!printername || !sharename) {
			nt_status = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
		/* we can reset NT_STATUS here because we do not
		   get any real NT_STATUS-codes anymore from now on */
		nt_status = NT_STATUS_UNSUCCESSFUL;

		d_printf(_("migrating printer queue for:    [%s] / [%s]\n"),
			printername, sharename);

		/* open dst printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd_dst, mem_ctx, sharename,
			PRINTER_ALL_ACCESS, cli->user_name, &hnd_dst)) {

			DEBUG(1,("could not open printer: %s\n", sharename));
		}

		/* check for existing dst printer */
		if (!net_spoolss_getprinter(pipe_hnd_dst, mem_ctx, &hnd_dst, level, &info_dst)) {
			printf (_("could not get printer, creating printer.\n"));
		} else {
			DEBUG(1,("printer already exists: %s\n", sharename));
			/* close printer handle here - dst only, not got src yet. */
			if (is_valid_policy_hnd(&hnd_dst)) {
				dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &result);
			}
			continue;
		}

		/* now get again src printer ctr via getprinter,
		   we first need a handle for that */

		/* open src printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd, mem_ctx, sharename,
			MAXIMUM_ALLOWED_ACCESS, cli->user_name, &hnd_src))
			goto done;

		/* getprinter on the src server */
		if (!net_spoolss_getprinter(pipe_hnd, mem_ctx, &hnd_src, level, &info_src))
			goto done;

		/* copy each src printer to a dst printer 1:1,
		   maybe some values have to be changed though */
		d_printf(_("creating printer: %s\n"), printername);

		info_ctr.level = level;
		spoolss_printerinfo2_to_setprinterinfo2(&info_src.info2, &info2);
		info_ctr.info.info2 = &info2;

		result = rpccli_spoolss_addprinterex(pipe_hnd_dst,
						     mem_ctx,
						     &info_ctr);

		if (W_ERROR_IS_OK(result))
			d_printf (_("printer [%s] successfully added.\n"),
				  printername);
		else if (W_ERROR_V(result) == W_ERROR_V(WERR_PRINTER_ALREADY_EXISTS))
			d_fprintf (stderr, _("printer [%s] already exists.\n"),
				   printername);
		else {
			d_fprintf (stderr, _("could not create printer [%s]\n"),
				   printername);
			goto done;
		}

		/* close printer handles here */
		if (is_valid_policy_hnd(&hnd_src)) {
			dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &result);
		}

		if (is_valid_policy_hnd(&hnd_dst)) {
			dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &result);
		}
	}

	nt_status = NT_STATUS_OK;

done:
	if (is_valid_policy_hnd(&hnd_src)) {
		dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &result);
	}

	if (is_valid_policy_hnd(&hnd_dst)) {
		dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &result);
	}

	if (cli_dst) {
		cli_shutdown(cli_dst);
	}
	return nt_status;
}

/**
 * Migrate Printer-Settings from a src server to the dst server
 * (for this to work, printers and drivers already have to be migrated earlier)
 *
 * All parameters are provided by the run_rpc_command function, except for
 * argc, argv which are passed through.
 *
 * @param c	A net_context structure
 * @param domain_sid The domain sid aquired from the remote server
 * @param cli A cli_state connected to the server.
 * @param mem_ctx Talloc context, destoyed on compleation of the function.
 * @param argc  Standard main() style argc
 * @param argv  Standard main() style argv.  Initial components are already
 *              stripped
 *
 * @return Normal NTSTATUS return.
 **/

NTSTATUS rpc_printer_migrate_settings_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	struct dcerpc_binding_handle *b_src = pipe_hnd->binding_handle;

	/* FIXME: Here the nightmare begins */

	WERROR result;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint32_t i = 0, j = 0;
	uint32_t num_printers;
	uint32_t level = 2;
	const char *printername, *sharename;
	struct rpc_pipe_client *pipe_hnd_dst = NULL;
	struct dcerpc_binding_handle *b_dst = NULL;
	struct policy_handle hnd_src, hnd_dst;
	union spoolss_PrinterInfo *info_enum;
	union spoolss_PrinterInfo info_dst_publish;
	union spoolss_PrinterInfo info_dst;
	struct cli_state *cli_dst = NULL;
	char *devicename = NULL, *unc_name = NULL, *url = NULL;
	const char *longname;
	const char **keylist = NULL;

	/* FIXME GD */
	ZERO_STRUCT(info_dst_publish);

	DEBUG(3,("copying printer settings\n"));

	/* connect destination PI_SPOOLSS */
	nt_status = connect_dst_pipe(c, &cli_dst, &pipe_hnd_dst,
				     &ndr_table_spoolss.syntax_id);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	b_dst = pipe_hnd_dst->binding_handle;

	/* enum src printers */
	if (!get_printer_info(pipe_hnd, mem_ctx, level, argc, argv, &num_printers, &info_enum)) {
		nt_status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (!num_printers) {
		printf (_("no printers found on server.\n"));
		nt_status = NT_STATUS_OK;
		goto done;
	}


	/* needed for dns-strings in regkeys */
	longname = get_mydnsfullname();
	if (!longname) {
		nt_status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	/* do something for all printers */
	for (i = 0; i < num_printers; i++) {

		uint32_t value_needed;
		uint32_t data_needed;
		enum winreg_Type type;
		struct spoolss_EnumPrinterData r;

		/* do some initialization */
		printername = info_enum[i].info2.printername;
		sharename = info_enum[i].info2.sharename;

		if (!printername || !sharename) {
			nt_status = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
		/* we can reset NT_STATUS here because we do not
		   get any real NT_STATUS-codes anymore from now on */
		nt_status = NT_STATUS_UNSUCCESSFUL;

		d_printf(_("migrating printer settings for: [%s] / [%s]\n"),
			printername, sharename);


		/* open src printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd, mem_ctx, sharename,
			MAXIMUM_ALLOWED_ACCESS, cli->user_name, &hnd_src))
			goto done;

		/* open dst printer handle */
		if (!net_spoolss_open_printer_ex(pipe_hnd_dst, mem_ctx, sharename,
			PRINTER_ALL_ACCESS, cli_dst->user_name, &hnd_dst))
			goto done;

		/* check for existing dst printer */
		if (!net_spoolss_getprinter(pipe_hnd_dst, mem_ctx, &hnd_dst,
				level, &info_dst))
			goto done;


		/* STEP 1: COPY DEVICE-MODE and other
			   PRINTER_INFO_2-attributes
		*/

		info_dst.info2 = info_enum[i].info2;

		/* why is the port always disconnected when the printer
		   is correctly installed (incl. driver ???) */
		info_dst.info2.portname = SAMBA_PRINTER_PORT_NAME;

		/* check if printer is published */
		if (info_enum[i].info2.attributes & PRINTER_ATTRIBUTE_PUBLISHED) {

			/* check for existing dst printer */
			if (!net_spoolss_getprinter(pipe_hnd_dst, mem_ctx, &hnd_dst, 7, &info_dst_publish))
				goto done;

			info_dst_publish.info7.action = DSPRINT_PUBLISH;

			/* ignore false from setprinter due to WERR_IO_PENDING */
			net_spoolss_setprinter(pipe_hnd_dst, mem_ctx, &hnd_dst, 7, &info_dst_publish);

			DEBUG(3,("republished printer\n"));
		}

		if (info_enum[i].info2.devmode != NULL) {

			/* copy devmode (info level 2) */
			info_dst.info2.devmode = info_enum[i].info2.devmode;

			/* do not copy security descriptor (we have another
			 * command for that) */
			info_dst.info2.secdesc = NULL;

#if 0
			info_dst.info2.devmode.devicename =
				talloc_asprintf(mem_ctx, "\\\\%s\\%s",
						longname, printername);
			if (!info_dst.info2.devmode.devicename) {
				nt_status = NT_STATUS_NO_MEMORY;
				goto done;
			}
#endif
			if (!net_spoolss_setprinter(pipe_hnd_dst, mem_ctx, &hnd_dst,
						    level, &info_dst))
				goto done;

			DEBUGADD(1,("\tSetPrinter of DEVICEMODE succeeded\n"));
		}

		/* STEP 2: COPY REGISTRY VALUES */

		/* please keep in mind that samba parse_spools gives horribly
		   crippled results when used to rpccli_spoolss_enumprinterdataex
		   a win2k3-server.  (Bugzilla #1851)
		   FIXME: IIRC I've seen it too on a win2k-server
		*/

		r.in.handle = &hnd_src;
		r.in.enum_index = 0;
		r.in.value_offered = 0;
		r.in.data_offered = 0;
		r.out.value_name = NULL;
		r.out.value_needed = &value_needed;
		r.out.type = &type;
		r.out.data = NULL;
		r.out.data_needed = &data_needed;

		/* enumerate data on src handle */
		nt_status = dcerpc_spoolss_EnumPrinterData_r(b_src, mem_ctx, &r);

		r.in.data_offered	= *r.out.data_needed;
		r.in.value_offered	= *r.out.value_needed;
		r.out.data		= talloc_zero_array(mem_ctx, uint8_t, r.in.data_offered);
		r.out.value_name	= talloc_zero_array(mem_ctx, char, r.in.value_offered);

		/* loop for all printerdata of "PrinterDriverData" */
		while (NT_STATUS_IS_OK(nt_status) && W_ERROR_IS_OK(r.out.result)) {

			r.in.enum_index++;

			nt_status = dcerpc_spoolss_EnumPrinterData_r(b_src, mem_ctx, &r);

			/* loop for all reg_keys */
			if (NT_STATUS_IS_OK(nt_status) && W_ERROR_IS_OK(r.out.result)) {

				/* display_value */
				if (c->opt_verbose) {
					struct regval_blob *v;

					v = regval_compose(talloc_tos(),
							   r.out.value_name,
							   *r.out.type,
							   r.out.data,
							   r.in.data_offered);
					if (v == NULL) {
						nt_status = NT_STATUS_NO_MEMORY;
						goto done;
					}

					display_reg_value(SPOOL_PRINTERDATA_KEY, v);
					talloc_free(v);
				}

				/* set_value */
				if (!net_spoolss_setprinterdata(pipe_hnd_dst, mem_ctx,
								&hnd_dst, r.out.value_name,
								*r.out.type, r.out.data, r.in.data_offered))
					goto done;

				DEBUGADD(1,("\tSetPrinterData of [%s] succeeded\n",
					    r.out.value_name));
			}
		}

		/* STEP 3: COPY SUBKEY VALUES */

		/* here we need to enum all printer_keys and then work
		   on the result with enum_printer_key_ex. nt4 does not
		   respond to enumprinterkey, win2k does, so continue
		   in case of an error */

		if (!net_spoolss_enumprinterkey(pipe_hnd, mem_ctx, &hnd_src, "", &keylist)) {
			printf(_("got no key-data\n"));
			continue;
		}


		/* work on a list of printer keys
		   each key has to be enumerated to get all required
		   information.  information is then set via setprinterdataex-calls */

		if (keylist == NULL)
			continue;

		for (i=0; keylist && keylist[i] != NULL; i++) {

			const char *subkey = keylist[i];
			uint32_t count;
			struct spoolss_PrinterEnumValues *info;

			/* enumerate all src subkeys */
			if (!net_spoolss_enumprinterdataex(pipe_hnd, mem_ctx, 0,
							   &hnd_src, subkey,
							   &count, &info)) {
				goto done;
			}

			for (j=0; j < count; j++) {

				struct regval_blob *value;
				DATA_BLOB blob;

				ZERO_STRUCT(blob);

				/* although samba replies with sane data in most cases we
				   should try to avoid writing wrong registry data */

				if (strequal(info[j].value_name, SPOOL_REG_PORTNAME) ||
				    strequal(info[j].value_name, SPOOL_REG_UNCNAME) ||
				    strequal(info[j].value_name, SPOOL_REG_URL) ||
				    strequal(info[j].value_name, SPOOL_REG_SHORTSERVERNAME) ||
				    strequal(info[j].value_name, SPOOL_REG_SERVERNAME)) {

					if (strequal(info[j].value_name, SPOOL_REG_PORTNAME)) {

						/* although windows uses a multi-sz, we use a sz */
						push_reg_sz(mem_ctx, &blob, SAMBA_PRINTER_PORT_NAME);
					}

					if (strequal(info[j].value_name, SPOOL_REG_UNCNAME)) {

						if (asprintf(&unc_name, "\\\\%s\\%s", longname, sharename) < 0) {
							nt_status = NT_STATUS_NO_MEMORY;
							goto done;
						}
						push_reg_sz(mem_ctx, &blob, unc_name);
					}

					if (strequal(info[j].value_name, SPOOL_REG_URL)) {

						continue;

#if 0
						/* FIXME: should we really do that ??? */
						if (asprintf(&url, "http://%s:631/printers/%s", longname, sharename) < 0) {
							nt_status = NT_STATUS_NO_MEMORY;
							goto done;
						}
						push_reg_sz(mem_ctx, NULL, &blob, url);
						fstrcpy(value.valuename, SPOOL_REG_URL);
#endif
					}

					if (strequal(info[j].value_name, SPOOL_REG_SERVERNAME)) {

						push_reg_sz(mem_ctx, &blob, longname);
					}

					if (strequal(info[j].value_name, SPOOL_REG_SHORTSERVERNAME)) {

						push_reg_sz(mem_ctx, &blob, global_myname());
					}

					value = regval_compose(talloc_tos(),
							       info[j].value_name,
							       REG_SZ,
							       blob.length == 0 ? NULL : blob.data,
							       blob.length);
					if (value == NULL) {
						nt_status = NT_STATUS_NO_MEMORY;
						goto done;
					}

					if (c->opt_verbose)
						display_reg_value(subkey, value);

					/* here we have to set all subkeys on the dst server */
					if (!net_spoolss_setprinterdataex(pipe_hnd_dst, mem_ctx, &hnd_dst,
							subkey, value))
					{
						talloc_free(value);
						goto done;
					}

					talloc_free(value);
				} else {

					struct regval_blob *v;

					v = regval_compose(talloc_tos(),
							   info[j].value_name,
							   info[j].type,
							   info[j].data->data,
							   info[j].data->length);
					if (v == NULL) {
						nt_status = NT_STATUS_NO_MEMORY;
						goto done;
					}

					if (c->opt_verbose) {
						display_reg_value(subkey, v);
					}

					/* here we have to set all subkeys on the dst server */
					if (!net_spoolss_setprinterdataex(pipe_hnd_dst, mem_ctx, &hnd_dst,
							subkey, v)) {
						goto done;
					}

					talloc_free(v);
				}

				DEBUGADD(1,("\tSetPrinterDataEx of key [%s\\%s] succeeded\n",
						subkey, info[j].value_name));

			}
		}

		TALLOC_FREE(keylist);

		/* close printer handles here */
		if (is_valid_policy_hnd(&hnd_src)) {
			dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &result);
		}

		if (is_valid_policy_hnd(&hnd_dst)) {
			dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &result);
		}
	}

	nt_status = NT_STATUS_OK;

done:
	SAFE_FREE(devicename);
	SAFE_FREE(url);
	SAFE_FREE(unc_name);

	if (is_valid_policy_hnd(&hnd_src)) {
		dcerpc_spoolss_ClosePrinter(b_src, mem_ctx, &hnd_src, &result);
	}

	if (is_valid_policy_hnd(&hnd_dst)) {
		dcerpc_spoolss_ClosePrinter(b_dst, mem_ctx, &hnd_dst, &result);
	}

	if (cli_dst) {
		cli_shutdown(cli_dst);
	}
	return nt_status;
}
