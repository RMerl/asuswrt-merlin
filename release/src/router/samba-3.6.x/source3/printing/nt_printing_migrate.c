/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *
 *  Copyright (c) Andreas Schneider            2010.
 *  Copyright (C) Bjoern Baumbach <bb@sernet.de> 2011
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
#include "printing/nt_printing_migrate.h"

#include "rpc_client/rpc_client.h"
#include "librpc/gen_ndr/ndr_ntprinting.h"
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "rpc_client/cli_winreg_spoolss.h"

static const char *driver_file_basename(const char *file)
{
	const char *basefile;

	basefile = strrchr(file, '\\');
	if (basefile == NULL) {
		basefile = file;
	} else {
		basefile++;
	}

	return basefile;
}

NTSTATUS printing_tdb_migrate_form(TALLOC_CTX *mem_ctx,
				   struct rpc_pipe_client *winreg_pipe,
				   const char *key_name,
				   unsigned char *data,
				   size_t length)
{
	struct dcerpc_binding_handle *b = winreg_pipe->binding_handle;
	enum ndr_err_code ndr_err;
	struct ntprinting_form r;
	struct spoolss_AddFormInfo1 f1;
	DATA_BLOB blob;
	WERROR result;

	blob = data_blob_const(data, length);

	ZERO_STRUCT(r);

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
		   (ndr_pull_flags_fn_t)ndr_pull_ntprinting_form);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(2, ("Form pull failed: %s\n",
			  ndr_errstr(ndr_err)));
		return NT_STATUS_NO_MEMORY;
	}

	/* Don't migrate builtin forms */
	if (r.flag == SPOOLSS_FORM_BUILTIN) {
		return NT_STATUS_OK;
	}

	DEBUG(2, ("Migrating Form: %s\n", key_name));

	f1.form_name = key_name;
	f1.flags = r.flag;

	f1.size.width = r.width;
	f1.size.height = r.length;

	f1.area.top = r.top;
	f1.area.right = r.right;
	f1.area.bottom = r.bottom;
	f1.area.left = r.left;

	result = winreg_printer_addform1(mem_ctx,
					 b,
					 &f1);
	if (W_ERROR_EQUAL(result, WERR_FILE_EXISTS)) {
		/* Don't migrate form if it already exists. */
		result = WERR_OK;
	}
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	return NT_STATUS_OK;
}

NTSTATUS printing_tdb_migrate_driver(TALLOC_CTX *mem_ctx,
				     struct rpc_pipe_client *winreg_pipe,
				     const char *key_name,
				     unsigned char *data,
				     size_t length,
				     bool do_string_conversion)
{
	struct dcerpc_binding_handle *b = winreg_pipe->binding_handle;
	enum ndr_err_code ndr_err;
	struct ntprinting_driver r;
	struct spoolss_AddDriverInfoCtr d;
	struct spoolss_AddDriverInfo3 d3;
	struct spoolss_StringArray a;
	DATA_BLOB blob;
	WERROR result;
	const char *driver_name;
	uint32_t driver_version;
	int i;

	blob = data_blob_const(data, length);

	ZERO_STRUCT(r);

	if (do_string_conversion) {
		r.string_flags = LIBNDR_FLAG_STR_ASCII;
	}

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
		   (ndr_pull_flags_fn_t)ndr_pull_ntprinting_driver);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(2, ("Driver pull failed: %s\n",
			  ndr_errstr(ndr_err)));
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(2, ("Migrating Printer Driver: %s\n", key_name));

	ZERO_STRUCT(d3);
	ZERO_STRUCT(a);

	/* remove paths from file names */
	if (r.dependent_files != NULL) {
		for (i = 0 ; r.dependent_files[i] != NULL; i++) {
			r.dependent_files[i] = driver_file_basename(r.dependent_files[i]);
		}
	}
	a.string = r.dependent_files;

	r.driverpath = driver_file_basename(r.driverpath);
	r.configfile = driver_file_basename(r.configfile);
	r.datafile = driver_file_basename(r.datafile);
	r.helpfile = driver_file_basename(r.helpfile);

	d3.architecture = r.environment;
	d3.config_file = r.configfile;
	d3.data_file = r.datafile;
	d3.default_datatype = r.defaultdatatype;
	d3.dependent_files = &a;
	d3.driver_path = r.driverpath;
	d3.help_file = r.helpfile;
	d3.monitor_name = r.monitorname;
	d3.driver_name = r.name;
	d3.version = r.version;

	d.level = 3;
	d.info.info3 = &d3;

	result = winreg_add_driver(mem_ctx,
				   b,
				   &d,
				   &driver_name,
				   &driver_version);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	return NT_STATUS_OK;
}

NTSTATUS printing_tdb_migrate_printer(TALLOC_CTX *mem_ctx,
				      struct rpc_pipe_client *winreg_pipe,
				      const char *key_name,
				      unsigned char *data,
				      size_t length,
				      bool do_string_conversion)
{
	struct dcerpc_binding_handle *b = winreg_pipe->binding_handle;
	enum ndr_err_code ndr_err;
	struct ntprinting_printer r;
	struct spoolss_SetPrinterInfo2 info2;
	struct spoolss_DeviceMode dm;
	struct spoolss_DevmodeContainer devmode_ctr;
	DATA_BLOB blob;
	NTSTATUS status;
	WERROR result;
	int j;
	uint32_t info2_mask = (SPOOLSS_PRINTER_INFO_ALL)
				& ~SPOOLSS_PRINTER_INFO_SECDESC;

	if (strequal(key_name, "printers")) {
		return NT_STATUS_OK;
	}

	blob = data_blob_const(data, length);

	ZERO_STRUCT(r);

	if (do_string_conversion) {
		r.info.string_flags = LIBNDR_FLAG_STR_ASCII;
	}

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
		   (ndr_pull_flags_fn_t) ndr_pull_ntprinting_printer);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(2, ("printer pull failed: %s\n",
			  ndr_errstr(ndr_err)));
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(2, ("Migrating Printer: %s\n", key_name));

	ZERO_STRUCT(devmode_ctr);

	/* Create printer info level 2 */
	ZERO_STRUCT(info2);

	info2.attributes = r.info.attributes;
	info2.averageppm = r.info.averageppm;
	info2.cjobs = r.info.cjobs;
	info2.comment = r.info.comment;
	info2.datatype = r.info.datatype;
	info2.defaultpriority = r.info.default_priority;
	info2.drivername = r.info.drivername;
	info2.location = r.info.location;
	info2.parameters = r.info.parameters;
	info2.portname = r.info.portname;
	info2.printername = r.info.printername;
	info2.printprocessor = r.info.printprocessor;
	info2.priority = r.info.priority;
	info2.sepfile = r.info.sepfile;
	info2.sharename = r.info.sharename;
	info2.starttime = r.info.starttime;
	info2.status = r.info.status;
	info2.untiltime = r.info.untiltime;

	/* Create Device Mode */
	if (r.devmode == NULL) {
		info2_mask &= ~SPOOLSS_PRINTER_INFO_DEVMODE;
	} else {
		ZERO_STRUCT(dm);

		dm.bitsperpel              = r.devmode->bitsperpel;
		dm.collate                 = r.devmode->collate;
		dm.color                   = r.devmode->color;
		dm.copies                  = r.devmode->copies;
		dm.defaultsource           = r.devmode->defaultsource;
		dm.devicename              = r.devmode->devicename;
		dm.displayflags            = r.devmode->displayflags;
		dm.displayfrequency        = r.devmode->displayfrequency;
		dm.dithertype              = r.devmode->dithertype;
		dm.driverversion           = r.devmode->driverversion;
		dm.duplex                  = r.devmode->duplex;
		dm.fields                  = r.devmode->fields;
		dm.formname                = r.devmode->formname;
		dm.icmintent               = r.devmode->icmintent;
		dm.icmmethod               = r.devmode->icmmethod;
		dm.logpixels               = r.devmode->logpixels;
		dm.mediatype               = r.devmode->mediatype;
		dm.orientation             = r.devmode->orientation;
		dm.panningheight           = r.devmode->pelsheight;
		dm.panningwidth            = r.devmode->panningwidth;
		dm.paperlength             = r.devmode->paperlength;
		dm.papersize               = r.devmode->papersize;
		dm.paperwidth              = r.devmode->paperwidth;
		dm.pelsheight              = r.devmode->pelsheight;
		dm.pelswidth               = r.devmode->pelswidth;
		dm.printquality            = r.devmode->printquality;
		dm.size                    = r.devmode->size;
		dm.scale                   = r.devmode->scale;
		dm.specversion             = r.devmode->specversion;
		dm.ttoption                = r.devmode->ttoption;
		dm.yresolution             = r.devmode->yresolution;

		if (r.devmode->nt_dev_private != NULL) {
			dm.driverextra_data.data   = r.devmode->nt_dev_private->data;
			dm.driverextra_data.length = r.devmode->nt_dev_private->length;
			dm.__driverextra_length    = r.devmode->nt_dev_private->length;
		}

		devmode_ctr.devmode = &dm;

		info2.devmode_ptr = 1;
	}

	result = winreg_update_printer(mem_ctx, b,
				       key_name,
				       info2_mask,
				       &info2,
				       &dm,
				       NULL);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("SetPrinter(%s) level 2 refused -- %s.\n",
			  key_name, win_errstr(result)));
		status = werror_to_ntstatus(result);
		goto done;
	}

	/* migrate printerdata */
	for (j = 0; j < r.count; j++) {
		char *valuename;
		char *keyname;

		if (r.printer_data[j].type == REG_NONE) {
			continue;
		}

		keyname = CONST_DISCARD(char *, r.printer_data[j].name);
		valuename = strchr(keyname, '\\');
		if (valuename == NULL) {
			continue;
		} else {
			valuename[0] = '\0';
			valuename++;
		}

		result = winreg_set_printer_dataex(mem_ctx, b,
						   key_name,
						   keyname,
						   valuename,
						   r.printer_data[j].type,
						   r.printer_data[j].data.data,
						   r.printer_data[j].data.length);
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(2, ("SetPrinterDataEx: printer [%s], keyname [%s], "
				  "valuename [%s] refused -- %s.\n",
				  key_name, keyname, valuename,
				  win_errstr(result)));
			status = werror_to_ntstatus(result);
			break;
		}
	}

	status = NT_STATUS_OK;
 done:

	return status;
}

NTSTATUS printing_tdb_migrate_secdesc(TALLOC_CTX *mem_ctx,
				      struct rpc_pipe_client *winreg_pipe,
				      const char *key_name,
				      unsigned char *data,
				      size_t length)
{
	struct dcerpc_binding_handle *b = winreg_pipe->binding_handle;
	enum ndr_err_code ndr_err;
	struct sec_desc_buf secdesc_ctr;
	DATA_BLOB blob;
	WERROR result;

	if (strequal(key_name, "printers")) {
		return NT_STATUS_OK;
	}

	blob = data_blob_const(data, length);

	ZERO_STRUCT(secdesc_ctr);

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &secdesc_ctr,
		   (ndr_pull_flags_fn_t)ndr_pull_sec_desc_buf);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(2, ("security descriptor pull failed: %s\n",
			  ndr_errstr(ndr_err)));
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(2, ("Migrating Security Descriptor: %s\n", key_name));

	result = winreg_set_printer_secdesc(mem_ctx, b,
					    key_name,
					    secdesc_ctr.sd);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	return NT_STATUS_OK;
}
