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

#ifndef _RPC_CLIENT_INIT_SPOOLSS_H_
#define _RPC_CLIENT_INIT_SPOOLSS_H_

/* The following definitions come from rpc_client/init_spoolss.c  */

bool init_systemtime(struct spoolss_Time *r,
		     struct tm *unixtime);
time_t spoolss_Time_to_time_t(const struct spoolss_Time *r);
WERROR pull_spoolss_PrinterData(TALLOC_CTX *mem_ctx,
				const DATA_BLOB *blob,
				union spoolss_PrinterData *data,
				enum winreg_Type type);
WERROR push_spoolss_PrinterData(TALLOC_CTX *mem_ctx, DATA_BLOB *blob,
				enum winreg_Type type,
				union spoolss_PrinterData *data);
void spoolss_printerinfo2_to_setprinterinfo2(const struct spoolss_PrinterInfo2 *i,
					     struct spoolss_SetPrinterInfo2 *s);
bool driver_info_ctr_to_info8(struct spoolss_AddDriverInfoCtr *r,
			      struct spoolss_DriverInfo8 *_info8);

WERROR spoolss_create_default_devmode(TALLOC_CTX *mem_ctx,
				      const char *devicename,
				      struct spoolss_DeviceMode **devmode);

WERROR spoolss_create_default_secdesc(TALLOC_CTX *mem_ctx,
				      struct spoolss_security_descriptor **secdesc);

#endif /* _RPC_CLIENT_INIT_SPOOLSS_H_ */
