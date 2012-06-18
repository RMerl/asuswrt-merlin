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

/*******************************************************************
 ********************************************************************/

WERROR pull_spoolss_PrinterData(TALLOC_CTX *mem_ctx,
				const DATA_BLOB *blob,
				union spoolss_PrinterData *data,
				enum winreg_Type type)
{
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_union_blob(blob, mem_ctx, NULL, data, type,
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
	ndr_err = ndr_push_union_blob(blob, mem_ctx, NULL, data, type,
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
