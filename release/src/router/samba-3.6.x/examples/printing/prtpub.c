/*
 *  Set printer capabilities in DsDriver Keys on remote printer
 *  Copyright (C) Jim McDonough	<jmcd@us.ibm.com> 2002.
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

/* This needs to be defined for certain compilers */
#define WINVER 0x0500

#include <tchar.h>
#include <windows.h>
#include <stdio.h>

#define SAMBA_PORT _T("Samba")

TCHAR *PrintLastError(void)
{
	static TCHAR msgtxt[1024*sizeof(TCHAR)];

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, GetLastError(), 
			0, msgtxt, 0, NULL);

	return msgtxt;
}

void map_orientation(HANDLE ph, TCHAR *printer, TCHAR *port)
{
	DWORD rot;
	TCHAR portrait_only[] = _T("PORTRAIT\0");
	TCHAR both[] = _T("LANDSCAPE\0PORTRAIT\0");

	/* orentation of 90 or 270 indicates landscape supported, 0 means it isn't */
	rot = DeviceCapabilities(printer, port, DC_BINNAMES, NULL, NULL);
	
	printf("printOrientationsSupported:\n");

	if (rot) {
		printf("\tPORTRAIT\n\tLANDSCAPE\n");
		SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printOrientationsSupported"), REG_MULTI_SZ,
			both, sizeof(both));
	} else {
		printf("\tPORTRAIT\n");
		SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printOrientationsSupported"), REG_MULTI_SZ,
			portrait_only, sizeof(portrait_only));
	}
}

void map_resolution(HANDLE ph, TCHAR *printer, TCHAR *port)
{
	DWORD num, *res, maxres = 0, i;

	num = DeviceCapabilities(printer, port, DC_ENUMRESOLUTIONS, NULL, NULL);
	if ((DWORD) -1 == num)
		return;
	res = malloc(num*2*sizeof(DWORD));
	num = DeviceCapabilities(printer, port, DC_ENUMRESOLUTIONS, (BYTE *) res, NULL);
	for (i=0; i < num*2; i++) {
		maxres = (res[i] > maxres) ? res[i] : maxres;
	}
	printf("printMaxResolutionSupported: %d\n", maxres);
	SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printMaxResolutionSupported"), REG_DWORD, 
		(BYTE *) &maxres, sizeof(maxres));
}

void map_extents(HANDLE ph, TCHAR *printer, TCHAR *port)
{
	DWORD extentval, xval, yval;

	extentval = DeviceCapabilities(printer, port, DC_MINEXTENT, NULL, NULL);
	xval = (DWORD) (LOWORD(extentval));
	yval = (DWORD) (HIWORD(extentval));
	printf("printMinXExtent: %d\n", xval);
	printf("printMinYExtent: %d\n", yval);
	SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printMinXExtent"), REG_DWORD, (BYTE *) &xval, sizeof(xval));
	SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printMinYExtent"), REG_DWORD, (BYTE *) &yval, sizeof(yval));
	extentval = DeviceCapabilities(printer, port, DC_MAXEXTENT, NULL, NULL);
	xval = (DWORD) (LOWORD(extentval));
	yval = (DWORD) (HIWORD(extentval));
	printf("printMaxXExtent: %d\n", xval);
	printf("printMaxYExtent: %d\n", yval);
	SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printMaxXExtent"), REG_DWORD, (BYTE *) &xval, sizeof(xval));
	SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printMaxYExtent"), REG_DWORD, (BYTE *) &yval, sizeof(yval));
}

void map_printrateunit(HANDLE ph, TCHAR *printer, TCHAR *port)
{
	DWORD unit;
	TCHAR ppm[] = _T("PagesPerMinute");
	TCHAR ipm[] = _T("InchesPerMinute");
	TCHAR lpm[] = _T("LinesPerMinute");
	TCHAR cps[] = _T("CharactersPerSecond");

	unit = DeviceCapabilities(printer, port, DC_PRINTRATEUNIT, NULL, NULL);
	switch(unit) {
	case PRINTRATEUNIT_PPM:
		printf("printRateUnit: %s\n", ppm);
		SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printRateUnit"), REG_SZ, ppm, sizeof(ppm));
		break;
	case PRINTRATEUNIT_IPM:
		printf("printRateUnit: %s\n", ipm);
		SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printRateUnit"), REG_SZ, ipm, sizeof(ipm));
		break;
	case PRINTRATEUNIT_LPM:
		printf("printRateUnit: %s\n", lpm);
		SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printRateUnit"), REG_SZ, lpm, sizeof(lpm));
		break;
	case PRINTRATEUNIT_CPS:
		printf("printRateUnit: %s\n", cps);
		SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, _T("printRateUnit"), REG_SZ, cps, sizeof(cps));
		break;
	default:
		printf("printRateUnit: unknown value %d\n", unit);
	}
}

void map_generic_boolean(HANDLE ph, TCHAR *printer, TCHAR *port, WORD cap, TCHAR *key)
{
	BYTE boolval;
	/* DeviceCapabilities doesn't always return 1 for true...just nonzero */
	boolval = (BYTE) (DeviceCapabilities(printer, port, cap, NULL, NULL) ? 1 : 0);
	printf("%s: %s\n", key, boolval ? "TRUE" : "FALSE");
	SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, key, REG_BINARY, &boolval, sizeof(boolval));
}

void map_generic_dword(HANDLE ph, TCHAR *printer, TCHAR *port, WORD cap, TCHAR *key)
{
	DWORD dword;

	dword = DeviceCapabilities(printer, port, cap, NULL, NULL);
	if ((DWORD) -1 == dword)
		return;

	printf("%s: %d\n", key, dword);
	SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, key, REG_DWORD, (BYTE *) &dword, sizeof(dword));
}

void map_generic_multi_sz(HANDLE ph, TCHAR *printer, TCHAR *port, WORD cap, TCHAR *key, int size)
{
	TCHAR *strings_in;
	TCHAR *strings_out, *strings_cur;
	DWORD num_items, i;

	num_items = DeviceCapabilities(printer, port, cap, NULL, NULL);
	if ((DWORD) -1 == num_items)
		return;
	strings_in = malloc(num_items * size);
	strings_out = calloc(num_items, size);
	num_items = DeviceCapabilities(printer, port, cap, strings_in, NULL);
	printf("%s:\n", key);
	for (i=0, strings_cur = strings_out; i < num_items; i++) {
		_tcsncpy(strings_cur, &strings_in[i*size], size);
		printf("\t%s\n", strings_cur);
		strings_cur += _tcslen(strings_cur) + 1;
	}
		
	SetPrinterDataEx(ph, SPLDS_DRIVER_KEY, key, REG_MULTI_SZ, strings_out, 
		(strings_cur - strings_out + 1) * sizeof(TCHAR));

	free(strings_in);
	free(strings_out);
}

int main(int argc, char *argv[])
{
	HANDLE ph;
	BYTE *driver_info;
	DWORD needed;
	TCHAR *printer;
	TCHAR *port = SAMBA_PORT;
	PRINTER_DEFAULTS admin_access = {NULL, NULL, PRINTER_ACCESS_ADMINISTER};
	PRINTER_INFO_7 publish = {NULL, DSPRINT_PUBLISH};
	
	if (argc < 2) {
		printf("Usage: %s <printername>\n", argv[0]);
		return -1;
	}

	printer = argv[1];

	if (!(OpenPrinter(printer, &ph, &admin_access))) {
		printf("OpenPrinter failed, error = %s\n", PrintLastError());
		return -1;
	}

	GetPrinterDriver(ph, NULL, 1, NULL, 0, &needed);
	if (!needed) {
		printf("GetPrinterDriver failed, error = %s\n", PrintLastError());
		ClosePrinter(ph);
		return -1;
	}
	driver_info = malloc(needed);
	if (!(GetPrinterDriver(ph, NULL, 1, driver_info, needed, &needed))) {
		printf("GetPrinterDriver failed, error = %s\n", PrintLastError());
		ClosePrinter(ph);
		return -1;
	}

	map_generic_multi_sz(ph, printer, port, DC_BINNAMES, _T("printBinNames"), 24);
	map_generic_boolean(ph, printer, port, DC_COLLATE, _T("printCollate"));
	map_generic_dword(ph, printer, port, DC_COPIES, _T("printMaxCopies"));
	map_generic_dword(ph, printer, port, DC_DRIVER, _T("driverVersion"));
	map_generic_boolean(ph, printer, port, DC_DUPLEX, _T("printDuplexSupported"));
	map_extents(ph, printer, port);
	map_resolution(ph, printer, port);
	map_orientation(ph, printer, port);
	map_generic_multi_sz(ph, printer, port, DC_PAPERNAMES, _T("printMediaSupported"), 64);
#if (WINVER >= 0x0500)
	map_generic_boolean(ph, printer, port, DC_COLORDEVICE, _T("printColor"));
	map_generic_multi_sz(ph, printer, port, DC_PERSONALITY, _T("printLanguage"), 64);
	map_generic_multi_sz(ph, printer, port, DC_MEDIAREADY, _T("printMediaReady"),64);
	map_generic_dword(ph, printer, port, DC_PRINTERMEM, _T("printMemory"));
	map_generic_dword(ph, printer, port, DC_PRINTRATE, _T("printRate"));
	map_printrateunit(ph, printer, port);
#ifdef DC_STAPLE
	map_generic_boolean(ph, printer, port, DC_STAPLE, _T("printStaplingSupported"));
#endif
#ifdef DC_PRINTRATEPPM
	map_generic_dword(ph, printer, port, DC_PRINTRATEPPM, _T("printPagesPerMinute"));
#endif
#endif
	SetPrinter(ph, 7, (BYTE *) &publish, 0);
	ClosePrinter(ph);
	return 0;
}
