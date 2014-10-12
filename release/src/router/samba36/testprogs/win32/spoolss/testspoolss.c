/*
   Unix SMB/CIFS implementation.
   test suite for spoolss rpc operations

   Copyright (C) Guenther Deschner 2009-2010

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

/****************************************************************************
****************************************************************************/

#include "testspoolss.h"
#include "string.h"
#include "torture.h"

/****************************************************************************
****************************************************************************/

static BOOL test_OpenPrinter(struct torture_context *tctx,
			     LPSTR printername,
			     LPPRINTER_DEFAULTS defaults,
			     LPHANDLE handle)
{
	torture_comment(tctx, "Testing OpenPrinter(%s)", printername);

	if (!OpenPrinter(printername, handle, defaults)) {
		char tmp[1024];
		sprintf(tmp, "failed to open printer %s, error was: 0x%08x\n",
			printername, GetLastError());
		torture_fail(tctx, tmp);
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_ClosePrinter(struct torture_context *tctx,
			      HANDLE handle)
{
	torture_comment(tctx, "Testing ClosePrinter");

	if (!ClosePrinter(handle)) {
		char tmp[1024];
		sprintf(tmp, "failed to close printer, error was: %s\n",
			errstr(GetLastError()));
		torture_fail(tctx, tmp);
	}

	return TRUE;
}


/****************************************************************************
****************************************************************************/

static BOOL test_EnumPrinters(struct torture_context *tctx,
			      LPSTR servername)
{
	DWORD levels[]  = { 1, 2, 5 };
	DWORD success[] = { 1, 1, 1 };
	DWORD i;
	DWORD flags = PRINTER_ENUM_NAME;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD returned = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing EnumPrinters level %d", levels[i]);

		EnumPrinters(flags, servername, levels[i], NULL, 0, &needed, &returned);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!EnumPrinters(flags, servername, levels[i], buffer, needed, &needed, &returned)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "EnumPrinters failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_printer_info_bylevel(levels[i], buffer, returned);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EnumDrivers(struct torture_context *tctx,
			     LPSTR servername,
			     LPSTR architecture)
{
	DWORD levels[]  = { 1, 2, 3, 4, 5, 6 };
	DWORD success[] = { 1, 1, 1, 1, 1, 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD returned = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing EnumPrinterDrivers(%s) level %d",
			architecture, levels[i]);

		EnumPrinterDrivers(servername, architecture, levels[i], NULL, 0, &needed, &returned);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!EnumPrinterDrivers(servername, architecture, levels[i], buffer, needed, &needed, &returned)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "EnumPrinterDrivers failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_driver_info_bylevel(levels[i], buffer, returned);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_GetForm(struct torture_context *tctx,
			 LPSTR servername,
			 HANDLE handle,
			 LPSTR formname)
{
	DWORD levels[]  = { 1, 2 };
	DWORD success[] = { 1, 0 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing GetForm(%s) level %d", formname, levels[i]);

		GetForm(handle, formname, levels[i], NULL, 0, &needed);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!GetForm(handle, formname, levels[i], buffer, needed, &needed)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "GetForm failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_form_info_bylevel(levels[i], buffer, 1);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EnumForms(struct torture_context *tctx,
			   LPSTR servername,
			   HANDLE handle)
{
	DWORD levels[]  = { 1, 2 };
	DWORD success[] = { 1, 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD returned = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing EnumForms level %d", levels[i]);

		if (tctx->samba3 && levels[i] == 2) {
			torture_comment(tctx, "skipping level %d enum against samba\n", levels[i]);
			continue;
		}

		EnumForms(handle, levels[i], NULL, 0, &needed, &returned);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!EnumForms(handle, levels[i], buffer, needed, &needed, &returned)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "EnumForms failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_form_info_bylevel(levels[i], buffer, returned);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EnumPorts(struct torture_context *tctx,
			   LPSTR servername)
{
	DWORD levels[]  = { 1, 2 };
	DWORD success[] = { 1, 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD returned = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing EnumPorts level %d", levels[i]);

		EnumPorts(servername, levels[i], NULL, 0, &needed, &returned);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!EnumPorts(servername, levels[i], buffer, needed, &needed, &returned)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "EnumPorts failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_port_info_bylevel(levels[i], buffer, returned);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EnumMonitors(struct torture_context *tctx,
			      LPSTR servername)
{
	DWORD levels[]  = { 1, 2 };
	DWORD success[] = { 1, 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD returned = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing EnumMonitors level %d", levels[i]);

		EnumMonitors(servername, levels[i], NULL, 0, &needed, &returned);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!EnumMonitors(servername, levels[i], buffer, needed, &needed, &returned)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "EnumMonitors failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_monitor_info_bylevel(levels[i], buffer, returned);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EnumPrintProcessors(struct torture_context *tctx,
				     LPSTR servername,
				     LPSTR architecture)
{
	DWORD levels[]  = { 1 };
	DWORD success[] = { 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD returned = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing EnumPrintProcessors(%s) level %d", architecture, levels[i]);

		EnumPrintProcessors(servername, architecture, levels[i], NULL, 0, &needed, &returned);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!EnumPrintProcessors(servername, architecture, levels[i], buffer, needed, &needed, &returned)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "EnumPrintProcessors failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_printprocessor_info_bylevel(levels[i], buffer, returned);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EnumPrintProcessorDatatypes(struct torture_context *tctx,
					     LPSTR servername)
{
	DWORD levels[]  = { 1 };
	DWORD success[] = { 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD returned = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing EnumPrintProcessorDatatypes level %d", levels[i]);

		EnumPrintProcessorDatatypes(servername, "winprint", levels[i], NULL, 0, &needed, &returned);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!EnumPrintProcessorDatatypes(servername, "winprint", levels[i], buffer, needed, &needed, &returned)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "EnumPrintProcessorDatatypes failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_datatypes_info_bylevel(levels[i], buffer, returned);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EnumPrinterKey(struct torture_context *tctx,
				LPSTR servername,
				HANDLE handle,
				LPCSTR key)
{
	LPSTR buffer = NULL;
	DWORD needed = 0;
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing EnumPrinterKey(%s)", key);

	err = EnumPrinterKey(handle, key, NULL, 0, &needed);
	if (err == ERROR_MORE_DATA) {
		buffer = (LPTSTR)malloc(needed);
		torture_assert(tctx, buffer, "malloc failed");
		err = EnumPrinterKey(handle, key, buffer, needed, &needed);
	}
	if (err) {
		sprintf(tmp, "EnumPrinterKey(%s) failed on [%s] (buffer size = %d), error: %s\n",
			key, servername, needed, errstr(err));
		torture_fail(tctx, tmp);
	}

	if (tctx->print) {
		print_printer_keys(buffer);
	}

	free(buffer);

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_GetPrinter(struct torture_context *tctx,
			    LPSTR printername,
			    HANDLE handle)
{
	DWORD levels[]  = { 1, 2, 3, 4, 5, 6, 7, 8 };
	DWORD success[] = { 1, 1, 1, 1, 1, 1, 1, 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing GetPrinter level %d", levels[i]);

		GetPrinter(handle, levels[i], NULL, 0, &needed);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!GetPrinter(handle, levels[i], buffer, needed, &needed)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "GetPrinter failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], printername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_printer_info_bylevel(levels[i], buffer, 1);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_GetPrinterDriver(struct torture_context *tctx,
				  LPSTR printername,
				  LPSTR architecture,
				  HANDLE handle)
{
	DWORD levels[]  = { 1, 2, 3, 4, 5, 6, 8 };
	DWORD success[] = { 1, 1, 1, 1, 1, 1, 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing GetPrinterDriver(%s) level %d",
			architecture, levels[i]);

		GetPrinterDriver(handle, architecture, levels[i], NULL, 0, &needed);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!GetPrinterDriver(handle, architecture, levels[i], buffer, needed, &needed)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "GetPrinterDriver failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], printername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_driver_info_bylevel(levels[i], buffer, 1);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}


/****************************************************************************
****************************************************************************/

static BOOL test_EnumJobs(struct torture_context *tctx,
			  LPSTR printername,
			  HANDLE handle)
{
	DWORD levels[]  = { 1, 2, 3, 4 };
	DWORD success[] = { 1, 1, 1, 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD returned = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing EnumJobs level %d", levels[i]);

		if (tctx->samba3 && levels[i] == 4) {
			torture_comment(tctx, "skipping level %d enum against samba\n", levels[i]);
			continue;
		}

		EnumJobs(handle, 0, 100, levels[i], NULL, 0, &needed, &returned);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!EnumJobs(handle, 0, 100, levels[i], buffer, needed, &needed, &returned)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "EnumJobs failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], printername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_job_info_bylevel(levels[i], buffer, returned);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EnumPrinterData(struct torture_context *tctx,
				 LPSTR servername,
				 HANDLE handle)
{
	DWORD err = 0;
	LPTSTR value_name;
	LPBYTE data;
	DWORD index = 0;
	DWORD type;
	DWORD value_offered = 0, value_needed;
	DWORD data_offered = 0, data_needed;
	char tmp[1024];

	torture_comment(tctx, "Testing EnumPrinterData(%d) (value offered: %d, data_offered: %d)\n",
		index, value_offered, data_offered);

	err = EnumPrinterData(handle, 0, NULL, 0, &value_needed, NULL, NULL, 0, &data_needed);
	if (err) {
		sprintf(tmp, "EnumPrinterData(%d) failed on [%s] (value size = %d, data size = %d), error: %s\n",
			index, servername, value_offered, data_offered, errstr(err));
		torture_fail(tctx, tmp);
	}

	value_name = malloc(value_needed);
	torture_assert(tctx, value_name, "malloc failed");
	data = malloc(data_needed);
	torture_assert(tctx, data, "malloc failed");

	value_offered = value_needed;
	data_offered = data_needed;

	do {

		value_needed = 0;
		data_needed = 0;

		torture_comment(tctx, "Testing EnumPrinterData(%d) (value offered: %d, data_offered: %d)\n",
			index, value_offered, data_offered);

		err = EnumPrinterData(handle, index++, value_name, value_offered, &value_needed, &type, data, data_offered, &data_needed);
		if (err == ERROR_NO_MORE_ITEMS) {
			break;
		}
		if (err) {
			sprintf(tmp, "EnumPrinterData(%d) failed on [%s] (value size = %d, data size = %d), error: %s\n",
				index, servername, value_offered, data_offered, errstr(err));
			torture_fail(tctx, tmp);
		}

		if (tctx->print) {
			print_printer_data(NULL, value_name, data_needed, data, type);
		}

	} while (err != ERROR_NO_MORE_ITEMS);

	free(value_name);
	free(data);

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EnumPrinterDataEx(struct torture_context *tctx,
				   LPSTR servername,
				   LPSTR keyname,
				   HANDLE handle,
				   LPBYTE *buffer_p,
				   DWORD *returned_p)
{
	LPBYTE buffer = NULL;
	DWORD needed = 0;
	DWORD returned = 0;
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing EnumPrinterDataEx(%s)", keyname);

	err = EnumPrinterDataEx(handle, keyname, NULL, 0, &needed, &returned);
	if (err == ERROR_MORE_DATA) {
		buffer = malloc(needed);
		torture_assert(tctx, buffer, "malloc failed");
		err = EnumPrinterDataEx(handle, keyname, buffer, needed, &needed, &returned);
	}
	if (err) {
		sprintf(tmp, "EnumPrinterDataEx(%s) failed on [%s] (buffer size = %d), error: %s\n",
			keyname, servername, needed, errstr(err));
		torture_fail(tctx, tmp);
	}

	if (tctx->print) {
		DWORD i;
		LPPRINTER_ENUM_VALUES v = (LPPRINTER_ENUM_VALUES)buffer;
		for (i=0; i < returned; i++) {
			print_printer_enum_values(&v[i]);
		}
	}

	if (returned_p) {
		*returned_p = returned;
	}

	if (buffer_p) {
		*buffer_p = buffer;
	} else {
		free(buffer);
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_devicemode_equal(struct torture_context *tctx,
				  const DEVMODE *d1,
				  const DEVMODE *d2)
{
	if (d1 == d2) {
		return TRUE;
	}

	if (!d1 || !d2) {
		torture_comment(tctx, "%s\n", __location__);
		return FALSE;
	}

	torture_assert_str_equal(tctx, (const char *)d1->dmDeviceName, (const char *)d2->dmDeviceName, "dmDeviceName mismatch");
	torture_assert_int_equal(tctx, d1->dmSpecVersion, d2->dmSpecVersion, "dmSpecVersion mismatch");
	torture_assert_int_equal(tctx, d1->dmDriverVersion, d2->dmDriverVersion, "dmDriverVersion mismatch");
	torture_assert_int_equal(tctx, d1->dmSize, d2->dmSize, "size mismatch");
	torture_assert_int_equal(tctx, d1->dmDriverExtra, d2->dmDriverExtra, "dmDriverExtra mismatch");
	torture_assert_int_equal(tctx, d1->dmFields, d2->dmFields, "dmFields mismatch");

	torture_assert_int_equal(tctx, d1->dmOrientation, d2->dmOrientation, "dmOrientation mismatch");
	torture_assert_int_equal(tctx, d1->dmPaperSize, d2->dmPaperSize, "dmPaperSize mismatch");
	torture_assert_int_equal(tctx, d1->dmPaperLength, d2->dmPaperLength, "dmPaperLength mismatch");
	torture_assert_int_equal(tctx, d1->dmPaperWidth, d2->dmPaperWidth, "dmPaperWidth mismatch");
	torture_assert_int_equal(tctx, d1->dmScale, d2->dmScale, "dmScale mismatch");
	torture_assert_int_equal(tctx, d1->dmCopies, d2->dmCopies, "dmCopies mismatch");
	torture_assert_int_equal(tctx, d1->dmDefaultSource, d2->dmDefaultSource, "dmDefaultSource mismatch");
	torture_assert_int_equal(tctx, d1->dmPrintQuality, d2->dmPrintQuality, "dmPrintQuality mismatch");

	torture_assert_int_equal(tctx, d1->dmColor, d2->dmColor, "dmColor mismatch");
	torture_assert_int_equal(tctx, d1->dmDuplex, d2->dmDuplex, "dmDuplex mismatch");
	torture_assert_int_equal(tctx, d1->dmYResolution, d2->dmYResolution, "dmYResolution mismatch");
	torture_assert_int_equal(tctx, d1->dmTTOption, d2->dmTTOption, "dmTTOption mismatch");
	torture_assert_int_equal(tctx, d1->dmCollate, d2->dmCollate, "dmCollate mismatch");
	torture_assert_str_equal(tctx, (const char *)d1->dmFormName, (const char *)d2->dmFormName, "dmFormName mismatch");
	torture_assert_int_equal(tctx, d1->dmLogPixels, d2->dmLogPixels, "dmLogPixels mismatch");
	torture_assert_int_equal(tctx, d1->dmBitsPerPel, d2->dmBitsPerPel, "dmBitsPerPel mismatch");
	torture_assert_int_equal(tctx, d1->dmPelsWidth, d2->dmPelsWidth, "dmPelsWidth mismatch");
	torture_assert_int_equal(tctx, d1->dmPelsHeight, d2->dmPelsHeight, "dmPelsHeight mismatch");

	torture_assert_int_equal(tctx, d1->dmDisplayFlags, d2->dmDisplayFlags, "dmDisplayFlags mismatch");
	/* or dmNup ? */
	torture_assert_int_equal(tctx, d1->dmDisplayFrequency, d2->dmDisplayFrequency, "dmDisplayFrequency mismatch");

	torture_assert_int_equal(tctx, d1->dmICMMethod, d2->dmICMMethod, "dmICMMethod mismatch");
	torture_assert_int_equal(tctx, d1->dmICMIntent, d2->dmICMIntent, "dmICMIntent mismatch");
	torture_assert_int_equal(tctx, d1->dmMediaType, d2->dmMediaType, "dmMediaType mismatch");
	torture_assert_int_equal(tctx, d1->dmDitherType, d2->dmDitherType, "dmDitherType mismatch");
	torture_assert_int_equal(tctx, d1->dmReserved1, d2->dmReserved1, "dmReserved1 mismatch");
	torture_assert_int_equal(tctx, d1->dmReserved2, d2->dmReserved2, "reserved2 mismatch");

	torture_assert_int_equal(tctx, d1->dmPanningWidth, d2->dmPanningWidth, "dmPanningWidth mismatch");
	torture_assert_int_equal(tctx, d1->dmPanningHeight, d2->dmPanningHeight, "dmPanningHeight mismatch");

	/* torture_assert_mem_equal(tctx, d1 + d1->dmSize, d2 + d2->dmSize, d1->dmDriverExtra, "private extra data mismatch"); */

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_DeviceModes(struct torture_context *tctx,
			     LPSTR printername,
			     HANDLE handle)
{
	PPRINTER_INFO_2 info2 = NULL;
	PPRINTER_INFO_8 info8 = NULL;
	DWORD needed = 0;
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing DeviceModes");

	torture_comment(tctx, "Testing GetPrinter level %d", 2);

	GetPrinter(handle, 2, NULL, 0, &needed);
	err = GetLastError();
	if (err == ERROR_INSUFFICIENT_BUFFER) {
		err = 0;
		info2 = (PPRINTER_INFO_2)malloc(needed);
		torture_assert(tctx, (LPBYTE)info2, "malloc failed");
		if (!GetPrinter(handle, 2, (LPBYTE)info2, needed, &needed)) {
			err = GetLastError();
		}
	}
	if (err) {
		sprintf(tmp, "GetPrinter failed level %d on [%s] (buffer size = %d), error: %s\n",
			2, printername, needed, errstr(err));
		torture_fail(tctx, tmp);
	}

	if (tctx->print) {
		print_printer_info_2(info2);
	}

	torture_comment(tctx, "Testing GetPrinter level %d", 8);

	GetPrinter(handle, 8, NULL, 0, &needed);
	err = GetLastError();
	if (err == ERROR_INSUFFICIENT_BUFFER) {
		err = 0;
		info8 = (PPRINTER_INFO_8)malloc(needed);
		torture_assert(tctx, (LPBYTE)info8, "malloc failed");
		if (!GetPrinter(handle, 8, (LPBYTE)info8, needed, &needed)) {
			err = GetLastError();
		}
	}
	if (err) {
		sprintf(tmp, "GetPrinter failed level %d on [%s] (buffer size = %d), error: %s\n",
			8, printername, needed, errstr(err));
		torture_fail(tctx, tmp);
	}

	if (tctx->print) {
		print_printer_info_8(info8);
	}

	torture_assert(tctx, test_devicemode_equal(tctx, info2->pDevMode, info8->pDevMode), "");

	free(info2);
	free(info8);

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_GetJob(struct torture_context *tctx,
			LPSTR printername,
			HANDLE handle,
			DWORD job_id)
{
	DWORD levels[]  = { 1, 2, 3, 4 };
	DWORD success[] = { 1, 1, 1, 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing GetJob(%d) level %d", job_id, levels[i]);

		if (tctx->samba3 && (levels[i] == 4) || (levels[i] == 3)) {
			torture_comment(tctx, "skipping level %d getjob against samba\n", levels[i]);
			continue;
		}

		GetJob(handle, job_id, levels[i], NULL, 0, &needed);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!GetJob(handle, job_id, levels[i], buffer, needed, &needed)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "GetJob failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], printername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			print_job_info_bylevel(levels[i], buffer, 1);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EachJob(struct torture_context *tctx,
			 LPSTR printername,
			 HANDLE handle)
{
	DWORD i;
	PJOB_INFO_1 buffer = NULL;
	DWORD needed = 0;
	DWORD returned = 0;
	DWORD err = 0;
	DWORD level = 1;
	char tmp[1024];
	BOOL ret = TRUE;

	torture_comment(tctx, "Testing Each PrintJob %d");

	EnumJobs(handle, 0, 100, level, NULL, 0, &needed, &returned);
	err = GetLastError();
	if (err == ERROR_INSUFFICIENT_BUFFER) {
		err = 0;
		buffer = (PJOB_INFO_1)malloc(needed);
		torture_assert(tctx, buffer, "malloc failed");
		if (!EnumJobs(handle, 0, 100, level, (LPBYTE)buffer, needed, &needed, &returned)) {
			err = GetLastError();
		}
	}
	if (err) {
		sprintf(tmp, "EnumJobs failed level %d on [%s] (buffer size = %d), error: %s\n",
			level, printername, needed, errstr(err));
		torture_fail(tctx, tmp);
	}

	if (tctx->print) {
		print_job_info_bylevel(level, (LPBYTE)buffer, returned);
	}

	for (i=0; i < returned; i++) {
		ret = test_GetJob(tctx, printername, handle, buffer[i].JobId);
	}

	free(buffer);

	return ret;

}

/****************************************************************************
****************************************************************************/

static BOOL test_OnePrinter(struct torture_context *tctx,
			    LPSTR printername,
			    LPSTR architecture,
			    LPPRINTER_DEFAULTS defaults)
{
	HANDLE handle;
	BOOL ret = TRUE;

	torture_comment(tctx, "Testing Printer %s", printername);

	ret &= test_OpenPrinter(tctx, printername, defaults, &handle);
	ret &= test_GetPrinter(tctx, printername, handle);
	ret &= test_GetPrinterDriver(tctx, printername, architecture, handle);
	ret &= test_EnumForms(tctx, printername, handle);
	ret &= test_EnumJobs(tctx, printername, handle);
	ret &= test_EachJob(tctx, printername, handle);
	ret &= test_EnumPrinterKey(tctx, printername, handle, "");
	ret &= test_EnumPrinterKey(tctx, printername, handle, "PrinterDriverData");
	ret &= test_EnumPrinterData(tctx, printername, handle);
	ret &= test_EnumPrinterDataEx(tctx, printername, "PrinterDriverData", handle, NULL, NULL);
	ret &= test_DeviceModes(tctx, printername, handle);
#if 0
	/* dont run these at the moment, behaviour is PrinterData API calls (not
	 * dcerpc calls) is almost unpredictable - gd */
	ret &= test_PrinterData(tctx, printername, handle);
	ret &= test_PrinterDataW(tctx, printername, handle);
#endif
	ret &= test_ClosePrinter(tctx, handle);

	return ret;
}

/****************************************************************************
****************************************************************************/

static BOOL test_EachPrinter(struct torture_context *tctx,
			     LPSTR servername,
			     LPSTR architecture,
			     LPPRINTER_DEFAULTS defaults)
{
	DWORD needed = 0;
	DWORD returned = 0;
	DWORD err = 0;
	char tmp[1024];
	DWORD i;
	DWORD flags = PRINTER_ENUM_NAME;
	PPRINTER_INFO_1 buffer = NULL;
	BOOL ret = TRUE;

	torture_comment(tctx, "Testing EnumPrinters level %d", 1);

	EnumPrinters(flags, servername, 1, NULL, 0, &needed, &returned);
	err = GetLastError();
	if (err == ERROR_INSUFFICIENT_BUFFER) {
		err = 0;
		buffer = (PPRINTER_INFO_1)malloc(needed);
		torture_assert(tctx, buffer, "malloc failed");
		if (!EnumPrinters(flags, servername, 1, (LPBYTE)buffer, needed, &needed, &returned)) {
			err = GetLastError();
		}
	}
	if (err) {
		sprintf(tmp, "EnumPrinters failed level %d on [%s] (buffer size = %d), error: %s\n",
			1, servername, needed, errstr(err));
		torture_fail(tctx, tmp);
	}

	for (i=0; i < returned; i++) {
		ret &= test_OnePrinter(tctx, buffer[i].pName, architecture, defaults);
	}

	free(buffer);

	return ret;
}

/****************************************************************************
****************************************************************************/

static BOOL test_GetPrintProcessorDirectory(struct torture_context *tctx,
					    LPSTR servername,
					    LPSTR architecture)
{
	DWORD levels[]  = { 1 };
	DWORD success[] = { 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing GetPrintProcessorDirectory(%s) level %d",
			architecture, levels[i]);

		GetPrintProcessorDirectory(servername, architecture, levels[i], NULL, 0, &needed);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!GetPrintProcessorDirectory(servername, architecture, levels[i], buffer, needed, &needed)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "GetPrintProcessorDirectory failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			printf("\tPrint Processor Directory\t= %s\n\n", (LPSTR)buffer);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_GetPrinterDriverDirectory(struct torture_context *tctx,
					   LPSTR servername,
					   LPSTR architecture)
{
	DWORD levels[]  = { 1 };
	DWORD success[] = { 1 };
	DWORD i;
	LPBYTE buffer = NULL;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		DWORD needed = 0;
		DWORD err = 0;
		char tmp[1024];

		torture_comment(tctx, "Testing GetPrinterDriverDirectory(%s) level %d",
			architecture, levels[i]);

		GetPrinterDriverDirectory(servername, architecture, levels[i], NULL, 0, &needed);
		err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			err = 0;
			buffer = malloc(needed);
			torture_assert(tctx, buffer, "malloc failed");
			if (!GetPrinterDriverDirectory(servername, architecture, levels[i], buffer, needed, &needed)) {
				err = GetLastError();
			}
		}
		if (err) {
			sprintf(tmp, "GetPrinterDriverDirectory failed level %d on [%s] (buffer size = %d), error: %s\n",
				levels[i], servername, needed, errstr(err));
			if (success[i]) {
				torture_fail(tctx, tmp);
			} else {
				torture_warning(tctx, tmp);
			}
		}

		if (tctx->print) {
			printf("\tPrinter Driver Directory\t= %s\n\n", (LPSTR)buffer);
		}

		free(buffer);
		buffer = NULL;
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_GetPrinterData(struct torture_context *tctx,
				LPSTR servername,
				LPSTR valuename,
				HANDLE handle,
				DWORD *type_p,
				LPBYTE *buffer_p,
				DWORD *size_p)
{
	LPBYTE buffer = NULL;
	DWORD needed = 0;
	DWORD type;
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing GetPrinterData(%s)", valuename);

	err = GetPrinterData(handle, valuename, &type, NULL, 0, &needed);
	if (err == ERROR_MORE_DATA) {
		buffer = (LPBYTE)malloc(needed);
		torture_assert(tctx, buffer, "malloc failed");
		err = GetPrinterData(handle, valuename, &type, buffer, needed, &needed);
	}
	if (err) {
		sprintf(tmp, "GetPrinterData(%s) failed on [%s] (buffer size = %d), error: %s\n",
			valuename, servername, needed, errstr(err));
		torture_fail(tctx, tmp);
	}

	if (tctx->print) {
		print_printer_data("PrinterDriverData", valuename, needed, buffer, type);
	}

	if (type_p) {
		*type_p = type;
	}

	if (size_p) {
		*size_p = needed;
	}

	if (buffer_p) {
		*buffer_p = buffer;
	} else {
		free(buffer);
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_GetPrinterDataEx(struct torture_context *tctx,
				  LPSTR servername,
				  LPSTR keyname,
				  LPSTR valuename,
				  HANDLE handle,
				  DWORD *type_p,
				  LPBYTE *buffer_p,
				  DWORD *size_p)
{
	LPBYTE buffer = NULL;
	DWORD needed = 0;
	DWORD type;
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing GetPrinterDataEx(%s - %s)", keyname, valuename);

	err = GetPrinterDataEx(handle, keyname, valuename, &type, NULL, 0, &needed);
	if (err == ERROR_MORE_DATA) {
		buffer = (LPBYTE)malloc(needed);
		torture_assert(tctx, buffer, "malloc failed");
		err = GetPrinterDataEx(handle, keyname, valuename, &type, buffer, needed, &needed);
	}
	if (err) {
		sprintf(tmp, "GetPrinterDataEx(%s) failed on [%s] (buffer size = %d), error: %s\n",
			valuename, servername, needed, errstr(err));
		torture_fail(tctx, tmp);
	}

	if (tctx->print) {
		print_printer_data(keyname, valuename, needed, buffer, type);
	}

	if (type_p) {
		*type_p = type;
	}

	if (size_p) {
		*size_p = needed;
	}

	if (buffer_p) {
		*buffer_p = buffer;
	} else {
		free(buffer);
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_GetPrinterDataExW(struct torture_context *tctx,
				   LPSTR servername,
				   LPCWSTR keyname,
				   LPCWSTR valuename,
				   HANDLE handle,
				   DWORD *type_p,
				   LPBYTE *buffer_p,
				   DWORD *size_p)
{
	LPBYTE buffer = NULL;
	DWORD needed = 0;
	DWORD type;
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing GetPrinterDataExW(%ls - %ls)", keyname, valuename);

	err = GetPrinterDataExW(handle, keyname, valuename, &type, NULL, 0, &needed);
	if (err == ERROR_MORE_DATA) {
		buffer = (LPBYTE)malloc(needed);
		torture_assert(tctx, buffer, "malloc failed");
		err = GetPrinterDataExW(handle, keyname, valuename, &type, buffer, needed, &needed);
	}
	if (err) {
		sprintf(tmp, "GetPrinterDataExW(%ls) failed on [%s] (buffer size = %d), error: %s\n",
			valuename, servername, needed, errstr(err));
		torture_fail(tctx, tmp);
	}

	if (tctx->print) {
		print_printer_dataw(keyname, valuename, needed, buffer, type);
	}

	if (type_p) {
		*type_p = type;
	}

	if (size_p) {
		*size_p = needed;
	}

	if (buffer_p) {
		*buffer_p = buffer;
	} else {
		free(buffer);
	}

	return TRUE;
}


/****************************************************************************
****************************************************************************/

static BOOL test_DeletePrinterDataEx(struct torture_context *tctx,
				     LPSTR servername,
				     LPSTR keyname,
				     LPSTR valuename,
				     HANDLE handle)
{
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing DeletePrinterDataEx(%s - %s)", keyname, valuename);

	err = DeletePrinterDataEx(handle, keyname, valuename);
	if (err) {
		sprintf(tmp, "DeletePrinterDataEx(%s - %s) failed on [%s], error: %s\n",
			keyname, valuename, servername, errstr(err));
		torture_fail(tctx, tmp);
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_DeletePrinterDataExW(struct torture_context *tctx,
				      LPSTR servername,
				      LPCWSTR keyname,
				      LPCWSTR valuename,
				      HANDLE handle)
{
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing DeletePrinterDataExW(%ls - %ls)", keyname, valuename);

	err = DeletePrinterDataExW(handle, keyname, valuename);
	if (err) {
		sprintf(tmp, "DeletePrinterDataExW(%ls - %ls) failed on [%s], error: %s\n",
			keyname, valuename, servername, errstr(err));
		torture_fail(tctx, tmp);
	}

	return TRUE;
}


/****************************************************************************
****************************************************************************/

static BOOL test_DeletePrinterKey(struct torture_context *tctx,
				  LPSTR servername,
				  LPSTR keyname,
				  HANDLE handle)
{
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing DeletePrinterKey(%s)", keyname);

	err = DeletePrinterKey(handle, keyname);
	if (err) {
		sprintf(tmp, "DeletePrinterKey(%s) failed on [%s], error: %s\n",
			keyname, servername, errstr(err));
		torture_fail(tctx, tmp);
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_DeletePrinterKeyW(struct torture_context *tctx,
				   LPSTR servername,
				   LPCWSTR keyname,
				   HANDLE handle)
{
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing DeletePrinterKeyW(%ls)", keyname);

	err = DeletePrinterKeyW(handle, keyname);
	if (err) {
		sprintf(tmp, "DeletePrinterKeyW(%ls) failed on [%s], error: %s\n",
			keyname, servername, errstr(err));
		torture_fail(tctx, tmp);
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_SetPrinterDataEx(struct torture_context *tctx,
				  LPSTR servername,
				  LPSTR keyname,
				  LPSTR valuename,
				  HANDLE handle,
				  DWORD type,
				  LPBYTE buffer,
				  DWORD offered)
{
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing SetPrinterDataEx(%s - %s)", keyname, valuename);

	err = SetPrinterDataEx(handle, keyname, valuename, type, buffer, offered);
	if (err) {
		sprintf(tmp, "SetPrinterDataEx(%s) failed on [%s] (buffer size = %d), error: %s\n",
			valuename, servername, offered, errstr(err));
		torture_fail(tctx, tmp);
	}

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_SetPrinterDataExW(struct torture_context *tctx,
				   LPCSTR servername,
				   LPCWSTR keyname,
				   LPCWSTR valuename,
				   HANDLE handle,
				   DWORD type,
				   LPBYTE buffer,
				   DWORD offered)
{
	DWORD err = 0;
	char tmp[1024];

	torture_comment(tctx, "Testing SetPrinterDataExW(%ls - %ls)", keyname, valuename);

	err = SetPrinterDataExW(handle, keyname, valuename, type, buffer, offered);
	if (err) {
		sprintf(tmp, "SetPrinterDataExW(%ls) failed on [%s] (buffer size = %d), error: %s\n",
			valuename, servername, offered, errstr(err));
		torture_fail(tctx, tmp);
	}

	return TRUE;
}


/****************************************************************************
****************************************************************************/

static BOOL test_PrinterData_Server(struct torture_context *tctx,
				    LPSTR servername,
				    HANDLE handle)
{
	BOOL ret = TRUE;
	DWORD i;
	DWORD type, type_ex;
	LPBYTE buffer, buffer_ex;
	DWORD size, size_ex;
	LPSTR valuenames[] = {
		SPLREG_DEFAULT_SPOOL_DIRECTORY,
		SPLREG_MAJOR_VERSION,
		SPLREG_MINOR_VERSION,
		SPLREG_DS_PRESENT,
		SPLREG_DNS_MACHINE_NAME,
		SPLREG_ARCHITECTURE,
		SPLREG_OS_VERSION
	};

	for (i=0; i < ARRAY_SIZE(valuenames); i++) {
		ret &= test_GetPrinterData(tctx, servername, valuenames[i], handle, &type, &buffer, &size);
		ret &= test_GetPrinterDataEx(tctx, servername, "random", valuenames[i], handle, &type_ex, &buffer_ex, &size_ex);
		torture_assert_int_equal(tctx, type, type_ex, "type mismatch");
		torture_assert_int_equal(tctx, size, size_ex, "size mismatch");
		torture_assert_mem_equal(tctx, buffer, buffer_ex, size, "buffer mismatch");
		free(buffer);
		free(buffer_ex);
	}

	return ret;
}

/****************************************************************************
****************************************************************************/

static BOOL PrinterDataEqual(struct torture_context *tctx,
			     DWORD type1, DWORD type2,
			     DWORD size1, DWORD size2,
			     LPBYTE buffer1, LPBYTE buffer2)
{
	torture_assert_int_equal(tctx, type1, type2, "type mismatch");
	torture_assert_int_equal(tctx, size1, size2, "size mismatch");
	torture_assert_mem_equal(tctx, buffer1, buffer2, size1, "buffer mismatch");

	return TRUE;
}

/****************************************************************************
****************************************************************************/

static BOOL test_PrinterData(struct torture_context *tctx,
			     LPSTR printername,
			     HANDLE handle)
{
	char tmp[1024];
	LPSTR keyname = "torture_key";
	LPSTR valuename = "torture_value";
	BOOL ret = TRUE;
	DWORD types[] = {
		REG_SZ,
		REG_DWORD,
		REG_BINARY
	};
	DWORD value = 12345678;
	LPSTR str = "abcdefghijklmnopqrstuvwxzy";
	DWORD t, s;

	for (t=0; t < ARRAY_SIZE(types); t++) {
	for (s=0; s < strlen(str); s++) {

		DWORD type, type_ex;
		LPBYTE buffer, buffer_ex;
		DWORD size, size_ex;

		if (types[t] == REG_DWORD) {
			s = 0xffff;
		}

		switch (types[t]) {
		case REG_BINARY:
			buffer = malloc(s);
			memcpy(buffer, str, s);
			size = s;
			break;
		case REG_DWORD:
			buffer = malloc(4);
			memcpy(buffer, &value, 4);
			size = 4;
			break;
		case REG_SZ:
			buffer = malloc(s);
			memcpy(buffer, str, s);
			size = s;
			break;
		default:
			sprintf(tmp, "type %d untested\n", types[t]);
			torture_fail(tctx, tmp);
			break;
		}

		type = types[t];

		torture_comment(tctx, "Testing PrinterData (type: %s, size: 0x%08x)", reg_type_str(type), size);

		torture_assert(tctx,
			test_SetPrinterDataEx(tctx, printername, keyname, valuename, handle, type, buffer, size),
			"failed to call SetPrinterDataEx");
		torture_assert(tctx,
			test_GetPrinterDataEx(tctx, printername, keyname, valuename, handle, &type_ex, &buffer_ex, &size_ex),
			"failed to call GetPrinterDataEx");

		if (!PrinterDataEqual(tctx, type_ex, type, size_ex, size, buffer_ex, buffer)) {
			torture_warning(tctx, "GetPrinterDataEx does not return the same info as we set with SetPrinterDataEx");
			ret = FALSE;
		}
		ret &= test_DeletePrinterDataEx(tctx, printername, keyname, valuename, handle);
		ret &= test_DeletePrinterKey(tctx, printername, keyname, handle);

		free(buffer);
		free(buffer_ex);
	}
	}

	return ret;
}

/****************************************************************************
****************************************************************************/

static BOOL test_PrinterDataW(struct torture_context *tctx,
			      LPSTR printername,
			      HANDLE handle)
{
	char tmp[1024];
	LPCWSTR keyname = L"torture_key";
	LPCWSTR valuename = L"torture_value";
	BOOL ret = TRUE;
	DWORD types[] = {
		REG_SZ,
		REG_DWORD,
		REG_BINARY
	};
	DWORD value = 12345678;
	LPSTR str = "abcdefghijklmnopqrstuvwxzy";
	DWORD t, s;

	for (t=0; t < ARRAY_SIZE(types); t++) {
	for (s=0; s < strlen(str); s++) {

		DWORD type, type_ex;
		LPBYTE buffer, buffer_ex;
		DWORD size, size_ex;

		if (types[t] == REG_DWORD) {
			s = 0xffff;
		}

		switch (types[t]) {
		case REG_BINARY:
			buffer = malloc(s);
			memcpy(buffer, str, s);
			size = s;
			break;
		case REG_DWORD:
			buffer = malloc(4);
			memcpy(buffer, &value, 4);
			size = 4;
			break;
		case REG_SZ:
			buffer = malloc(s);
			memcpy(buffer, str, s);
			size = s;
			break;
		default:
			sprintf(tmp, "type %d untested\n", types[t]);
			torture_fail(tctx, tmp);
			break;
		}

		type = types[t];

		torture_comment(tctx, "Testing PrinterDataW (type: %s, size: 0x%08x)", reg_type_str(type), size);

		torture_assert(tctx,
			test_SetPrinterDataExW(tctx, printername, keyname, valuename, handle, type, buffer, size),
			"failed to call SetPrinterDataExW");
		torture_assert(tctx,
			test_GetPrinterDataExW(tctx, printername, keyname, valuename, handle, &type_ex, &buffer_ex, &size_ex),
			"failed to call GetPrinterDataExW");

		if (!PrinterDataEqual(tctx, type_ex, type, size_ex, size, buffer_ex, buffer)) {
			torture_warning(tctx, "GetPrinterDataExW does not return the same info as we set with SetPrinterDataExW");
			ret = FALSE;
		}
		ret &= test_DeletePrinterDataExW(tctx, printername, keyname, valuename, handle);
		ret &= test_DeletePrinterKeyW(tctx, printername, keyname, handle);

		free(buffer);
		free(buffer_ex);
	}
	}

	return ret;
}

/****************************************************************************
****************************************************************************/

const char *get_string_param(const char *str)
{
	const char *p;

	p = strchr(str, '=');
	if (!p) {
		return NULL;
	}

	return (p+1);
}

/****************************************************************************
****************************************************************************/

int main(int argc, char *argv[])
{
	BOOL ret = FALSE;
	LPSTR servername;
	LPSTR architecture = "Windows NT x86";
	HANDLE server_handle;
	PRINTER_DEFAULTS defaults_admin, defaults_use;
	struct torture_context *tctx;
	int i;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <name> [print] [samba3] [architecture=ARCHITECTURE]\n\n", argv[0]);
		fprintf(stderr, "\t<name>           can be a server or printer name URI\n");
		fprintf(stderr, "\t[print]          will print all data that has been retrieved\n");
		fprintf(stderr, "\t                 from the printserver\n");
		fprintf(stderr, "\t[samba3]         will skip some tests samba servers are known\n");
		fprintf(stderr, "\t                 not to have implemented\n");
		fprintf(stderr, "\t[architecture=X] allows to define a specific\n");
		fprintf(stderr, "\t                 architecture to test with. choose between:\n");
		fprintf(stderr, "\t                 \"Windows NT x86\" or \"Windows x64\"\n");
		exit(-1);
	}

	tctx = malloc(sizeof(struct torture_context));
	if (!tctx) {
		fprintf(stderr, "out of memory\n");
		exit(-1);
	}
	memset(tctx, '\0', sizeof(*tctx));

	servername = argv[1];

	for (i=1; i < argc; i++) {
		if (strcmp(argv[i], "print") == 0) {
			tctx->print = TRUE;
		}
		if (strcmp(argv[i], "samba3") == 0) {
			tctx->samba3 = TRUE;
		}
		if (strncmp(argv[i], "architecture", strlen("architecture")) == 0) {
			architecture = get_string_param(argv[i]);
		}
	}

	printf("Running testsuite with architecture: %s\n", architecture);

	defaults_admin.pDatatype = NULL;
	defaults_admin.pDevMode = NULL;
	defaults_admin.DesiredAccess = PRINTER_ACCESS_ADMINISTER;

	defaults_use.pDatatype = NULL;
	defaults_use.pDevMode = NULL;
	defaults_use.DesiredAccess = PRINTER_ACCESS_USE;

	if ((servername[0] == '\\') && (servername[1] == '\\')) {
		LPSTR p = servername+2;
		LPSTR p2;
		if ((p2 = strchr(p, '\\')) != NULL) {
			ret = test_OnePrinter(tctx, servername, architecture, &defaults_admin);
			goto done;
		}
	}

	ret &= test_EnumPrinters(tctx, servername);
	ret &= test_EnumDrivers(tctx, servername, architecture);
	ret &= test_OpenPrinter(tctx, servername, NULL, &server_handle);
/*	ret &= test_EnumPrinterKey(tctx, servername, server_handle, ""); */
	ret &= test_PrinterData_Server(tctx, servername, server_handle);
	ret &= test_EnumForms(tctx, servername, server_handle);
	ret &= test_ClosePrinter(tctx, server_handle);
	ret &= test_EnumPorts(tctx, servername);
	ret &= test_EnumMonitors(tctx, servername);
	ret &= test_EnumPrintProcessors(tctx, servername, architecture);
	ret &= test_EnumPrintProcessorDatatypes(tctx, servername);
	ret &= test_GetPrintProcessorDirectory(tctx, servername, architecture);
	ret &= test_GetPrinterDriverDirectory(tctx, servername, architecture);
	ret &= test_EachPrinter(tctx, servername, architecture, &defaults_admin);

 done:
	if (!ret) {
		if (tctx->last_reason) {
			fprintf(stderr, "failed: %s\n", tctx->last_reason);
		}
		free(tctx);
		return -1;
	}

	printf("%s run successfully\n", argv[0]);

	free(tctx);
	return 0;
}
