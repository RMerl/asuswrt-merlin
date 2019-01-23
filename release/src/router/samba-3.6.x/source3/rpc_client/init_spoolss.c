/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Guenther Deschner                  2009.
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
#include "../librpc/gen_ndr/ndr_spoolss.h"
#include "rpc_client/init_spoolss.h"
#include "../libcli/security/security.h"
#include "secrets.h"
#include "passdb/machine_sid.h"

/*******************************************************************
********************************************************************/

bool init_systemtime(struct spoolss_Time *r,
		     struct tm *unixtime)
{
	if (!r || !unixtime) {
		return false;
	}

	r->year		= unixtime->tm_year+1900;
	r->month	= unixtime->tm_mon+1;
	r->day_of_week	= unixtime->tm_wday;
	r->day		= unixtime->tm_mday;
	r->hour		= unixtime->tm_hour;
	r->minute	= unixtime->tm_min;
	r->second	= unixtime->tm_sec;
	r->millisecond	= 0;

	return true;
}

time_t spoolss_Time_to_time_t(const struct spoolss_Time *r)
{
	struct tm unixtime;

	unixtime.tm_year	= r->year - 1900;
	unixtime.tm_mon		= r->month - 1;
	unixtime.tm_wday	= r->day_of_week;
	unixtime.tm_mday	= r->day;
	unixtime.tm_hour	= r->hour;
	unixtime.tm_min		= r->minute;
	unixtime.tm_sec		= r->second;

	return mktime(&unixtime);
}

/*******************************************************************
 ********************************************************************/

WERROR pull_spoolss_PrinterData(TALLOC_CTX *mem_ctx,
				const DATA_BLOB *blob,
				union spoolss_PrinterData *data,
				enum winreg_Type type)
{
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_union_blob(blob, mem_ctx, data, type,
			(ndr_pull_flags_fn_t)ndr_pull_spoolss_PrinterData);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return WERR_GENERAL_FAILURE;
	}
	return WERR_OK;
}

/*******************************************************************
 ********************************************************************/

WERROR push_spoolss_PrinterData(TALLOC_CTX *mem_ctx, DATA_BLOB *blob,
				enum winreg_Type type,
				union spoolss_PrinterData *data)
{
	enum ndr_err_code ndr_err;
	ndr_err = ndr_push_union_blob(blob, mem_ctx, data, type,
			(ndr_push_flags_fn_t)ndr_push_spoolss_PrinterData);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return WERR_GENERAL_FAILURE;
	}
	return WERR_OK;
}

/*******************************************************************
 ********************************************************************/

void spoolss_printerinfo2_to_setprinterinfo2(const struct spoolss_PrinterInfo2 *i,
					     struct spoolss_SetPrinterInfo2 *s)
{
	s->servername		= i->servername;
	s->printername		= i->printername;
	s->sharename		= i->sharename;
	s->portname		= i->portname;
	s->drivername		= i->drivername;
	s->comment		= i->comment;
	s->location		= i->location;
	s->devmode_ptr		= 0;
	s->sepfile		= i->sepfile;
	s->printprocessor	= i->printprocessor;
	s->datatype		= i->datatype;
	s->parameters		= i->parameters;
	s->secdesc_ptr		= 0;
	s->attributes		= i->attributes;
	s->priority		= i->priority;
	s->defaultpriority	= i->defaultpriority;
	s->starttime		= i->starttime;
	s->untiltime		= i->untiltime;
	s->status		= i->status;
	s->cjobs		= i->cjobs;
	s->averageppm		= i->averageppm;
}

/****************************************************************************
****************************************************************************/

bool driver_info_ctr_to_info8(struct spoolss_AddDriverInfoCtr *r,
			      struct spoolss_DriverInfo8 *_info8)
{
	struct spoolss_DriverInfo8 info8;

	ZERO_STRUCT(info8);

	switch (r->level) {
	case 3:
		info8.version		= r->info.info3->version;
		info8.driver_name	= r->info.info3->driver_name;
		info8.architecture	= r->info.info3->architecture;
		info8.driver_path	= r->info.info3->driver_path;
		info8.data_file		= r->info.info3->data_file;
		info8.config_file	= r->info.info3->config_file;
		info8.help_file		= r->info.info3->help_file;
		info8.monitor_name	= r->info.info3->monitor_name;
		info8.default_datatype	= r->info.info3->default_datatype;
		if (r->info.info3->dependent_files && r->info.info3->dependent_files->string) {
			info8.dependent_files	= r->info.info3->dependent_files->string;
		}
		break;
	case 6:
		info8.version		= r->info.info6->version;
		info8.driver_name	= r->info.info6->driver_name;
		info8.architecture	= r->info.info6->architecture;
		info8.driver_path	= r->info.info6->driver_path;
		info8.data_file		= r->info.info6->data_file;
		info8.config_file	= r->info.info6->config_file;
		info8.help_file		= r->info.info6->help_file;
		info8.monitor_name	= r->info.info6->monitor_name;
		info8.default_datatype	= r->info.info6->default_datatype;
		if (r->info.info6->dependent_files && r->info.info6->dependent_files->string) {
			info8.dependent_files	= r->info.info6->dependent_files->string;
		}
		info8.driver_date	= r->info.info6->driver_date;
		info8.driver_version	= r->info.info6->driver_version;
		info8.manufacturer_name = r->info.info6->manufacturer_name;
		info8.manufacturer_url	= r->info.info6->manufacturer_url;
		info8.hardware_id	= r->info.info6->hardware_id;
		info8.provider		= r->info.info6->provider;
		break;
	case 8:
		info8.version		= r->info.info8->version;
		info8.driver_name	= r->info.info8->driver_name;
		info8.architecture	= r->info.info8->architecture;
		info8.driver_path	= r->info.info8->driver_path;
		info8.data_file		= r->info.info8->data_file;
		info8.config_file	= r->info.info8->config_file;
		info8.help_file		= r->info.info8->help_file;
		info8.monitor_name	= r->info.info8->monitor_name;
		info8.default_datatype	= r->info.info8->default_datatype;
		if (r->info.info8->dependent_files && r->info.info8->dependent_files->string) {
			info8.dependent_files	= r->info.info8->dependent_files->string;
		}
		if (r->info.info8->previous_names && r->info.info8->previous_names->string) {
			info8.previous_names	= r->info.info8->previous_names->string;
		}
		info8.driver_date	= r->info.info8->driver_date;
		info8.driver_version	= r->info.info8->driver_version;
		info8.manufacturer_name = r->info.info8->manufacturer_name;
		info8.manufacturer_url	= r->info.info8->manufacturer_url;
		info8.hardware_id	= r->info.info8->hardware_id;
		info8.provider		= r->info.info8->provider;
		info8.print_processor	= r->info.info8->print_processor;
		info8.vendor_setup	= r->info.info8->vendor_setup;
		if (r->info.info8->color_profiles && r->info.info8->color_profiles->string) {
			info8.color_profiles = r->info.info8->color_profiles->string;
		}
		info8.inf_path		= r->info.info8->inf_path;
		info8.printer_driver_attributes = r->info.info8->printer_driver_attributes;
		if (r->info.info8->core_driver_dependencies && r->info.info8->core_driver_dependencies->string) {
			info8.core_driver_dependencies = r->info.info8->core_driver_dependencies->string;
		}
		info8.min_inbox_driver_ver_date = r->info.info8->min_inbox_driver_ver_date;
		info8.min_inbox_driver_ver_version = r->info.info8->min_inbox_driver_ver_version;
		break;
	default:
		return false;
	}

	*_info8 = info8;

	return true;
}

/****************************************************************************
 Create and allocate a default devicemode.
****************************************************************************/

WERROR spoolss_create_default_devmode(TALLOC_CTX *mem_ctx,
				      const char *devicename,
				      struct spoolss_DeviceMode **devmode)
{
	struct spoolss_DeviceMode *dm;
	char *dname;

	dm = talloc_zero(mem_ctx, struct spoolss_DeviceMode);
	if (dm == NULL) {
		return WERR_NOMEM;
	}

	dname = talloc_asprintf(dm, "%s", devicename);
	if (dname == NULL) {
		return WERR_NOMEM;
	}
	if (strlen(dname) > MAXDEVICENAME) {
		dname[MAXDEVICENAME] = '\0';
	}
	dm->devicename = dname;

	dm->formname = talloc_strdup(dm, "Letter");
	if (dm->formname == NULL) {
		return WERR_NOMEM;
	}

	dm->specversion          = DMSPEC_NT4_AND_ABOVE;
	dm->driverversion        = 0x0400;
	dm->size                 = 0x00DC;
	dm->__driverextra_length = 0;
	dm->fields               = DEVMODE_FORMNAME |
				   DEVMODE_TTOPTION |
				   DEVMODE_PRINTQUALITY |
				   DEVMODE_DEFAULTSOURCE |
				   DEVMODE_COPIES |
				   DEVMODE_SCALE |
				   DEVMODE_PAPERSIZE |
				   DEVMODE_ORIENTATION;
	dm->orientation          = DMORIENT_PORTRAIT;
	dm->papersize            = DMPAPER_LETTER;
	dm->paperlength          = 0;
	dm->paperwidth           = 0;
	dm->scale                = 0x64;
	dm->copies               = 1;
	dm->defaultsource        = DMBIN_FORMSOURCE;
	dm->printquality         = DMRES_HIGH;           /* 0x0258 */
	dm->color                = DMRES_MONOCHROME;
	dm->duplex               = DMDUP_SIMPLEX;
	dm->yresolution          = 0;
	dm->ttoption             = DMTT_SUBDEV;
	dm->collate              = DMCOLLATE_FALSE;
	dm->icmmethod            = 0;
	dm->icmintent            = 0;
	dm->mediatype            = 0;
	dm->dithertype           = 0;

	dm->logpixels            = 0;
	dm->bitsperpel           = 0;
	dm->pelswidth            = 0;
	dm->pelsheight           = 0;
	dm->displayflags         = 0;
	dm->displayfrequency     = 0;
	dm->reserved1            = 0;
	dm->reserved2            = 0;
	dm->panningwidth         = 0;
	dm->panningheight        = 0;

	dm->driverextra_data.data = NULL;
	dm->driverextra_data.length = 0;

        *devmode = dm;
	return WERR_OK;
}

WERROR spoolss_create_default_secdesc(TALLOC_CTX *mem_ctx,
				      struct spoolss_security_descriptor **secdesc)
{
	struct security_ace ace[7];	/* max number of ace entries */
	int i = 0;
	uint32_t sa;
	struct security_acl *psa = NULL;
	struct security_descriptor *psd = NULL;
	struct dom_sid adm_sid;
	size_t sd_size;

	/* Create an ACE where Everyone is allowed to print */

	sa = PRINTER_ACE_PRINT;
	init_sec_ace(&ace[i++], &global_sid_World, SEC_ACE_TYPE_ACCESS_ALLOWED,
		     sa, SEC_ACE_FLAG_CONTAINER_INHERIT);

	/* Add the domain admins group if we are a DC */

	if ( IS_DC ) {
		struct dom_sid domadmins_sid;

		sid_compose(&domadmins_sid, get_global_sam_sid(),
			    DOMAIN_RID_ADMINS);

		sa = PRINTER_ACE_FULL_CONTROL;
		init_sec_ace(&ace[i++], &domadmins_sid,
			SEC_ACE_TYPE_ACCESS_ALLOWED, sa,
			SEC_ACE_FLAG_OBJECT_INHERIT | SEC_ACE_FLAG_INHERIT_ONLY);
		init_sec_ace(&ace[i++], &domadmins_sid, SEC_ACE_TYPE_ACCESS_ALLOWED,
			sa, SEC_ACE_FLAG_CONTAINER_INHERIT);
	}
	else if (secrets_fetch_domain_sid(lp_workgroup(), &adm_sid)) {
		sid_append_rid(&adm_sid, DOMAIN_RID_ADMINISTRATOR);

		sa = PRINTER_ACE_FULL_CONTROL;
		init_sec_ace(&ace[i++], &adm_sid,
			SEC_ACE_TYPE_ACCESS_ALLOWED, sa,
			SEC_ACE_FLAG_OBJECT_INHERIT | SEC_ACE_FLAG_INHERIT_ONLY);
		init_sec_ace(&ace[i++], &adm_sid, SEC_ACE_TYPE_ACCESS_ALLOWED,
			sa, SEC_ACE_FLAG_CONTAINER_INHERIT);
	}

	/* add BUILTIN\Administrators as FULL CONTROL */

	sa = PRINTER_ACE_FULL_CONTROL;
	init_sec_ace(&ace[i++], &global_sid_Builtin_Administrators,
		SEC_ACE_TYPE_ACCESS_ALLOWED, sa,
		SEC_ACE_FLAG_OBJECT_INHERIT | SEC_ACE_FLAG_INHERIT_ONLY);
	init_sec_ace(&ace[i++], &global_sid_Builtin_Administrators,
		SEC_ACE_TYPE_ACCESS_ALLOWED,
		sa, SEC_ACE_FLAG_CONTAINER_INHERIT);

	/* add BUILTIN\Print Operators as FULL CONTROL */

	sa = PRINTER_ACE_FULL_CONTROL;
	init_sec_ace(&ace[i++], &global_sid_Builtin_Print_Operators,
		SEC_ACE_TYPE_ACCESS_ALLOWED, sa,
		SEC_ACE_FLAG_OBJECT_INHERIT | SEC_ACE_FLAG_INHERIT_ONLY);
	init_sec_ace(&ace[i++], &global_sid_Builtin_Print_Operators,
		SEC_ACE_TYPE_ACCESS_ALLOWED,
		sa, SEC_ACE_FLAG_CONTAINER_INHERIT);

	/* Make the security descriptor owned by the BUILTIN\Administrators */

	/* The ACL revision number in rpc_secdesc.h differs from the one
	   created by NT when setting ACE entries in printer
	   descriptors.  NT4 complains about the property being edited by a
	   NT5 machine. */

	if ((psa = make_sec_acl(mem_ctx, NT4_ACL_REVISION, i, ace)) != NULL) {
		psd = make_sec_desc(mem_ctx,
				    SD_REVISION,
				    SEC_DESC_SELF_RELATIVE,
				    &global_sid_Builtin_Administrators,
				    &global_sid_Builtin_Administrators,
				    NULL,
				    psa,
				    &sd_size);
	}

	if (psd == NULL) {
		DEBUG(0,("construct_default_printer_sd: Failed to make SEC_DESC.\n"));
		return WERR_NOMEM;
	}

	DEBUG(4,("construct_default_printer_sdb: size = %u.\n",
		 (unsigned int)sd_size));

	*secdesc = psd;

	return WERR_OK;
}
