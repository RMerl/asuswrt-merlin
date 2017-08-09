/*
 *  Unix SMB/CIFS implementation.
 *
 *  SPOOLSS RPC Pipe server / winreg client routines
 *
 *  Copyright (c) 2010      Andreas Schneider <asn@samba.org>
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
#include "nt_printing.h"
#include "../librpc/gen_ndr/ndr_spoolss.h"
#include "../librpc/gen_ndr/ndr_winreg_c.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "secrets.h"
#include "../libcli/security/security.h"
#include "rpc_client/cli_winreg.h"
#include "../libcli/registry/util_reg.h"
#include "rpc_client/cli_winreg_spoolss.h"
#include "printing/nt_printing_os2.h"
#include "rpc_client/init_spoolss.h"

#define TOP_LEVEL_PRINT_KEY "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Print"
#define TOP_LEVEL_PRINT_PRINTERS_KEY TOP_LEVEL_PRINT_KEY "\\Printers"
#define TOP_LEVEL_CONTROL_KEY "SYSTEM\\CurrentControlSet\\Control\\Print"
#define TOP_LEVEL_CONTROL_FORMS_KEY TOP_LEVEL_CONTROL_KEY "\\Forms"

#define EMPTY_STRING ""

#define FILL_STRING(mem_ctx, in, out) \
	do { \
		if (in && strlen(in)) { \
			out = talloc_strdup(mem_ctx, in); \
		} else { \
			out = talloc_strdup(mem_ctx, ""); \
		} \
		W_ERROR_HAVE_NO_MEMORY(out); \
	} while (0);

#define CHECK_ERROR(result) \
	if (W_ERROR_IS_OK(result)) continue; \
	if (W_ERROR_EQUAL(result, WERR_NOT_FOUND)) result = WERR_OK; \
	if (!W_ERROR_IS_OK(result)) break

/*        FLAGS,                NAME,                              with,   height,   left, top, right, bottom */
static const struct spoolss_FormInfo1 builtin_forms1[] = {
        { SPOOLSS_FORM_BUILTIN, "Letter",                         {0x34b5c,0x44368}, {0x0,0x0,0x34b5c,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "Letter Small",                   {0x34b5c,0x44368}, {0x0,0x0,0x34b5c,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "Tabloid",                        {0x44368,0x696b8}, {0x0,0x0,0x44368,0x696b8} },
        { SPOOLSS_FORM_BUILTIN, "Ledger",                         {0x696b8,0x44368}, {0x0,0x0,0x696b8,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "Legal",                          {0x34b5c,0x56d10}, {0x0,0x0,0x34b5c,0x56d10} },
        { SPOOLSS_FORM_BUILTIN, "Statement",                      {0x221b4,0x34b5c}, {0x0,0x0,0x221b4,0x34b5c} },
        { SPOOLSS_FORM_BUILTIN, "Executive",                      {0x2cf56,0x411cc}, {0x0,0x0,0x2cf56,0x411cc} },
        { SPOOLSS_FORM_BUILTIN, "A3",                             {0x48828,0x668a0}, {0x0,0x0,0x48828,0x668a0} },
        { SPOOLSS_FORM_BUILTIN, "A4",                             {0x33450,0x48828}, {0x0,0x0,0x33450,0x48828} },
        { SPOOLSS_FORM_BUILTIN, "A4 Small",                       {0x33450,0x48828}, {0x0,0x0,0x33450,0x48828} },
        { SPOOLSS_FORM_BUILTIN, "A5",                             {0x24220,0x33450}, {0x0,0x0,0x24220,0x33450} },
        { SPOOLSS_FORM_BUILTIN, "B4 (JIS)",                       {0x3ebe8,0x58de0}, {0x0,0x0,0x3ebe8,0x58de0} },
        { SPOOLSS_FORM_BUILTIN, "B5 (JIS)",                       {0x2c6f0,0x3ebe8}, {0x0,0x0,0x2c6f0,0x3ebe8} },
        { SPOOLSS_FORM_BUILTIN, "Folio",                          {0x34b5c,0x509d8}, {0x0,0x0,0x34b5c,0x509d8} },
        { SPOOLSS_FORM_BUILTIN, "Quarto",                         {0x347d8,0x43238}, {0x0,0x0,0x347d8,0x43238} },
        { SPOOLSS_FORM_BUILTIN, "10x14",                          {0x3e030,0x56d10}, {0x0,0x0,0x3e030,0x56d10} },
        { SPOOLSS_FORM_BUILTIN, "11x17",                          {0x44368,0x696b8}, {0x0,0x0,0x44368,0x696b8} },
        { SPOOLSS_FORM_BUILTIN, "Note",                           {0x34b5c,0x44368}, {0x0,0x0,0x34b5c,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "Envelope #9",                    {0x18079,0x37091}, {0x0,0x0,0x18079,0x37091} },
        { SPOOLSS_FORM_BUILTIN, "Envelope #10",                   {0x19947,0x3ae94}, {0x0,0x0,0x19947,0x3ae94} },
        { SPOOLSS_FORM_BUILTIN, "Envelope #11",                   {0x1be7c,0x40565}, {0x0,0x0,0x1be7c,0x40565} },
        { SPOOLSS_FORM_BUILTIN, "Envelope #12",                   {0x1d74a,0x44368}, {0x0,0x0,0x1d74a,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "Envelope #14",                   {0x1f018,0x47504}, {0x0,0x0,0x1f018,0x47504} },
        { SPOOLSS_FORM_BUILTIN, "C size sheet",                   {0x696b8,0x886d0}, {0x0,0x0,0x696b8,0x886d0} },
        { SPOOLSS_FORM_BUILTIN, "D size sheet",                   {0x886d0,0xd2d70}, {0x0,0x0,0x886d0,0xd2d70} },
        { SPOOLSS_FORM_BUILTIN, "E size sheet",                   {0xd2d70,0x110da0},{0x0,0x0,0xd2d70,0x110da0} },
        { SPOOLSS_FORM_BUILTIN, "Envelope DL",                    {0x1adb0,0x35b60}, {0x0,0x0,0x1adb0,0x35b60} },
        { SPOOLSS_FORM_BUILTIN, "Envelope C5",                    {0x278d0,0x37e88}, {0x0,0x0,0x278d0,0x37e88} },
        { SPOOLSS_FORM_BUILTIN, "Envelope C3",                    {0x4f1a0,0x6fd10}, {0x0,0x0,0x4f1a0,0x6fd10} },
        { SPOOLSS_FORM_BUILTIN, "Envelope C4",                    {0x37e88,0x4f1a0}, {0x0,0x0,0x37e88,0x4f1a0} },
        { SPOOLSS_FORM_BUILTIN, "Envelope C6",                    {0x1bd50,0x278d0}, {0x0,0x0,0x1bd50,0x278d0} },
        { SPOOLSS_FORM_BUILTIN, "Envelope C65",                   {0x1bd50,0x37e88}, {0x0,0x0,0x1bd50,0x37e88} },
        { SPOOLSS_FORM_BUILTIN, "Envelope B4",                    {0x3d090,0x562e8}, {0x0,0x0,0x3d090,0x562e8} },
        { SPOOLSS_FORM_BUILTIN, "Envelope B5",                    {0x2af80,0x3d090}, {0x0,0x0,0x2af80,0x3d090} },
        { SPOOLSS_FORM_BUILTIN, "Envelope B6",                    {0x2af80,0x1e848}, {0x0,0x0,0x2af80,0x1e848} },
        { SPOOLSS_FORM_BUILTIN, "Envelope",                       {0x1adb0,0x38270}, {0x0,0x0,0x1adb0,0x38270} },
        { SPOOLSS_FORM_BUILTIN, "Envelope Monarch",               {0x18079,0x2e824}, {0x0,0x0,0x18079,0x2e824} },
        { SPOOLSS_FORM_BUILTIN, "6 3/4 Envelope",                 {0x167ab,0x284ec}, {0x0,0x0,0x167ab,0x284ec} },
        { SPOOLSS_FORM_BUILTIN, "US Std Fanfold",                 {0x5c3e1,0x44368}, {0x0,0x0,0x5c3e1,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "German Std Fanfold",             {0x34b5c,0x4a6a0}, {0x0,0x0,0x34b5c,0x4a6a0} },
        { SPOOLSS_FORM_BUILTIN, "German Legal Fanfold",           {0x34b5c,0x509d8}, {0x0,0x0,0x34b5c,0x509d8} },
        { SPOOLSS_FORM_BUILTIN, "B4 (ISO)",                       {0x3d090,0x562e8}, {0x0,0x0,0x3d090,0x562e8} },
        { SPOOLSS_FORM_BUILTIN, "Japanese Postcard",              {0x186a0,0x24220}, {0x0,0x0,0x186a0,0x24220} },
        { SPOOLSS_FORM_BUILTIN, "9x11",                           {0x37cf8,0x44368}, {0x0,0x0,0x37cf8,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "10x11",                          {0x3e030,0x44368}, {0x0,0x0,0x3e030,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "15x11",                          {0x5d048,0x44368}, {0x0,0x0,0x5d048,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "Envelope Invite",                {0x35b60,0x35b60}, {0x0,0x0,0x35b60,0x35b60} },
        { SPOOLSS_FORM_BUILTIN, "Reserved48",                     {0x1,0x1},         {0x0,0x0,0x1,0x1} },
        { SPOOLSS_FORM_BUILTIN, "Reserved49",                     {0x1,0x1},         {0x0,0x0,0x1,0x1} },
        { SPOOLSS_FORM_BUILTIN, "Letter Extra",                   {0x3ae94,0x4a6a0}, {0x0,0x0,0x3ae94,0x4a6a0} },
        { SPOOLSS_FORM_BUILTIN, "Legal Extra",                    {0x3ae94,0x5d048}, {0x0,0x0,0x3ae94,0x5d048} },
        { SPOOLSS_FORM_BUILTIN, "Tabloid Extra",                  {0x4a6a0,0x6f9f0}, {0x0,0x0,0x4a6a0,0x6f9f0} },
        { SPOOLSS_FORM_BUILTIN, "A4 Extra",                       {0x397c2,0x4eb16}, {0x0,0x0,0x397c2,0x4eb16} },
        { SPOOLSS_FORM_BUILTIN, "Letter Transverse",              {0x34b5c,0x44368}, {0x0,0x0,0x34b5c,0x44368} },
        { SPOOLSS_FORM_BUILTIN, "A4 Transverse",                  {0x33450,0x48828}, {0x0,0x0,0x33450,0x48828} },
        { SPOOLSS_FORM_BUILTIN, "Letter Extra Transverse",        {0x3ae94,0x4a6a0}, {0x0,0x0,0x3ae94,0x4a6a0} },
        { SPOOLSS_FORM_BUILTIN, "Super A",                        {0x376b8,0x56ea0}, {0x0,0x0,0x376b8,0x56ea0} },
        { SPOOLSS_FORM_BUILTIN, "Super B",                        {0x4a768,0x76e58}, {0x0,0x0,0x4a768,0x76e58} },
        { SPOOLSS_FORM_BUILTIN, "Letter Plus",                    {0x34b5c,0x4eb16}, {0x0,0x0,0x34b5c,0x4eb16} },
        { SPOOLSS_FORM_BUILTIN, "A4 Plus",                        {0x33450,0x50910}, {0x0,0x0,0x33450,0x50910} },
        { SPOOLSS_FORM_BUILTIN, "A5 Transverse",                  {0x24220,0x33450}, {0x0,0x0,0x24220,0x33450} },
        { SPOOLSS_FORM_BUILTIN, "B5 (JIS) Transverse",            {0x2c6f0,0x3ebe8}, {0x0,0x0,0x2c6f0,0x3ebe8} },
        { SPOOLSS_FORM_BUILTIN, "A3 Extra",                       {0x4e9d0,0x6ca48}, {0x0,0x0,0x4e9d0,0x6ca48} },
        { SPOOLSS_FORM_BUILTIN, "A5 Extra",                       {0x2a7b0,0x395f8}, {0x0,0x0,0x2a7b0,0x395f8} },
        { SPOOLSS_FORM_BUILTIN, "B5 (ISO) Extra",                 {0x31128,0x43620}, {0x0,0x0,0x31128,0x43620} },
        { SPOOLSS_FORM_BUILTIN, "A2",                             {0x668a0,0x91050}, {0x0,0x0,0x668a0,0x91050} },
        { SPOOLSS_FORM_BUILTIN, "A3 Transverse",                  {0x48828,0x668a0}, {0x0,0x0,0x48828,0x668a0} },
        { SPOOLSS_FORM_BUILTIN, "A3 Extra Transverse",            {0x4e9d0,0x6ca48}, {0x0,0x0,0x4e9d0,0x6ca48} },
        { SPOOLSS_FORM_BUILTIN, "Japanese Double Postcard",       {0x30d40,0x24220}, {0x0,0x0,0x30d40,0x24220} },
        { SPOOLSS_FORM_BUILTIN, "A6",                             {0x19a28,0x24220}, {0x0,0x0,0x19a28,0x24220} },
        { SPOOLSS_FORM_BUILTIN, "Japan Envelope Kaku #2 Rotated", {0x510e0,0x3a980}, {0x0,0x0,0x510e0,0x3a980} },
        { SPOOLSS_FORM_BUILTIN, "Japan Envelope Kaku #3 Rotated", {0x43a08,0x34bc0}, {0x0,0x0,0x43a08,0x34bc0} },
        { SPOOLSS_FORM_BUILTIN, "Japan Envelope Chou #3 Rotated", {0x395f8,0x1d4c0}, {0x0,0x0,0x395f8,0x1d4c0} },
        { SPOOLSS_FORM_BUILTIN, "Japan Envelope Chou #4 Rotated", {0x320c8,0x15f90}, {0x0,0x0,0x320c8,0x15f90} },
        { SPOOLSS_FORM_BUILTIN, "Letter Rotated",                 {0x44368,0x34b5c}, {0x0,0x0,0x44368,0x34b5c} },
        { SPOOLSS_FORM_BUILTIN, "A3 Rotated",                     {0x668a0,0x48828}, {0x0,0x0,0x668a0,0x48828} },
        { SPOOLSS_FORM_BUILTIN, "A4 Rotated",                     {0x48828,0x33450}, {0x0,0x0,0x48828,0x33450} },
        { SPOOLSS_FORM_BUILTIN, "A5 Rotated",                     {0x33450,0x24220}, {0x0,0x0,0x33450,0x24220} },
        { SPOOLSS_FORM_BUILTIN, "B4 (JIS) Rotated",               {0x58de0,0x3ebe8}, {0x0,0x0,0x58de0,0x3ebe8} },
        { SPOOLSS_FORM_BUILTIN, "B5 (JIS) Rotated",               {0x3ebe8,0x2c6f0}, {0x0,0x0,0x3ebe8,0x2c6f0} },
        { SPOOLSS_FORM_BUILTIN, "Japanese Postcard Rotated",      {0x24220,0x186a0}, {0x0,0x0,0x24220,0x186a0} },
        { SPOOLSS_FORM_BUILTIN, "Double Japan Postcard Rotated",  {0x24220,0x30d40}, {0x0,0x0,0x24220,0x30d40} },
        { SPOOLSS_FORM_BUILTIN, "A6 Rotated",                     {0x24220,0x19a28}, {0x0,0x0,0x24220,0x19a28} },
        { SPOOLSS_FORM_BUILTIN, "Japanese Envelope Kaku #2",      {0x3a980,0x510e0}, {0x0,0x0,0x3a980,0x510e0} },
        { SPOOLSS_FORM_BUILTIN, "Japanese Envelope Kaku #3",      {0x34bc0,0x43a08}, {0x0,0x0,0x34bc0,0x43a08} },
        { SPOOLSS_FORM_BUILTIN, "Japanese Envelope Chou #3",      {0x1d4c0,0x395f8}, {0x0,0x0,0x1d4c0,0x395f8} },
        { SPOOLSS_FORM_BUILTIN, "Japanese Envelope Chou #4",      {0x15f90,0x320c8}, {0x0,0x0,0x15f90,0x320c8} },
        { SPOOLSS_FORM_BUILTIN, "B6 (JIS)",                       {0x1f400,0x2c6f0}, {0x0,0x0,0x1f400,0x2c6f0} },
        { SPOOLSS_FORM_BUILTIN, "B6 (JIS) Rotated",               {0x2c6f0,0x1f400}, {0x0,0x0,0x2c6f0,0x1f400} },
        { SPOOLSS_FORM_BUILTIN, "12x11",                          {0x4a724,0x443e1}, {0x0,0x0,0x4a724,0x443e1} },
        { SPOOLSS_FORM_BUILTIN, "Japan Envelope You #4",          {0x19a28,0x395f8}, {0x0,0x0,0x19a28,0x395f8} },
        { SPOOLSS_FORM_BUILTIN, "Japan Envelope You #4 Rotated",  {0x395f8,0x19a28}, {0x0,0x0,0x395f8,0x19a28} },
        { SPOOLSS_FORM_BUILTIN, "PRC 16K",                        {0x2de60,0x3f7a0}, {0x0,0x0,0x2de60,0x3f7a0} },
        { SPOOLSS_FORM_BUILTIN, "PRC 32K",                        {0x1fbd0,0x2cec0}, {0x0,0x0,0x1fbd0,0x2cec0} },
        { SPOOLSS_FORM_BUILTIN, "PRC 32K(Big)",                   {0x222e0,0x318f8}, {0x0,0x0,0x222e0,0x318f8} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #1",                {0x18e70,0x28488}, {0x0,0x0,0x18e70,0x28488} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #2",                {0x18e70,0x2af80}, {0x0,0x0,0x18e70,0x2af80} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #3",                {0x1e848,0x2af80}, {0x0,0x0,0x1e848,0x2af80} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #4",                {0x1adb0,0x32c80}, {0x0,0x0,0x1adb0,0x32c80} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #5",                {0x1adb0,0x35b60}, {0x0,0x0,0x1adb0,0x35b60} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #6",                {0x1d4c0,0x38270}, {0x0,0x0,0x1d4c0,0x38270} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #7",                {0x27100,0x38270}, {0x0,0x0,0x27100,0x38270} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #8",                {0x1d4c0,0x4b708}, {0x0,0x0,0x1d4c0,0x4b708} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #9",                {0x37e88,0x4f1a0}, {0x0,0x0,0x37e88,0x4f1a0} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #10",               {0x4f1a0,0x6fd10}, {0x0,0x0,0x4f1a0,0x6fd10} },
        { SPOOLSS_FORM_BUILTIN, "PRC 16K Rotated",                {0x3f7a0,0x2de60}, {0x0,0x0,0x3f7a0,0x2de60} },
        { SPOOLSS_FORM_BUILTIN, "PRC 32K Rotated",                {0x2cec0,0x1fbd0}, {0x0,0x0,0x2cec0,0x1fbd0} },
        { SPOOLSS_FORM_BUILTIN, "PRC 32K(Big) Rotated",           {0x318f8,0x222e0}, {0x0,0x0,0x318f8,0x222e0} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #1 Rotated",        {0x28488,0x18e70}, {0x0,0x0,0x28488,0x18e70} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #2 Rotated",        {0x2af80,0x18e70}, {0x0,0x0,0x2af80,0x18e70} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #3 Rotated",        {0x2af80,0x1e848}, {0x0,0x0,0x2af80,0x1e848} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #4 Rotated",        {0x32c80,0x1adb0}, {0x0,0x0,0x32c80,0x1adb0} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #5 Rotated",        {0x35b60,0x1adb0}, {0x0,0x0,0x35b60,0x1adb0} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #6 Rotated",        {0x38270,0x1d4c0}, {0x0,0x0,0x38270,0x1d4c0} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #7 Rotated",        {0x38270,0x27100}, {0x0,0x0,0x38270,0x27100} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #8 Rotated",        {0x4b708,0x1d4c0}, {0x0,0x0,0x4b708,0x1d4c0} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #9 Rotated",        {0x4f1a0,0x37e88}, {0x0,0x0,0x4f1a0,0x37e88} },
        { SPOOLSS_FORM_BUILTIN, "PRC Envelope #10 Rotated",       {0x6fd10,0x4f1a0}, {0x0,0x0,0x6fd10,0x4f1a0} }
};

/********************************************************************
 static helper functions
********************************************************************/

/****************************************************************************
 Update the changeid time.
****************************************************************************/
/**
 * @internal
 *
 * @brief Update the ChangeID time of a printer.
 *
 * This is SO NASTY as some drivers need this to change, others need it
 * static. This value will change every second, and I must hope that this
 * is enough..... DON'T CHANGE THIS CODE WITHOUT A TEST MATRIX THE SIZE OF
 * UTAH ! JRA.
 *
 * @return              The ChangeID.
 */
static uint32_t winreg_printer_rev_changeid(void)
{
	struct timeval tv;

	get_process_uptime(&tv);

#if 1	/* JERRY */
	/* Return changeid as msec since spooler restart */
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#else
	/*
	 * This setting seems to work well but is too untested
	 * to replace the above calculation.  Left in for experimentation
	 * of the reader            --jerry (Tue Mar 12 09:15:05 CST 2002)
	 */
	return tv.tv_sec * 10 + tv.tv_usec / 100000;
#endif
}

static WERROR winreg_printer_openkey(TALLOC_CTX *mem_ctx,
			      struct dcerpc_binding_handle *binding_handle,
			      const char *path,
			      const char *key,
			      bool create_key,
			      uint32_t access_mask,
			      struct policy_handle *hive_handle,
			      struct policy_handle *key_handle)
{
	struct winreg_String wkey, wkeyclass;
	char *keyname;
	NTSTATUS status;
	WERROR result = WERR_OK;

	status = dcerpc_winreg_OpenHKLM(binding_handle,
					mem_ctx,
					NULL,
					access_mask,
					hive_handle,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_printer_openkey: Could not open HKLM hive: %s\n",
			  nt_errstr(status)));
		return ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_openkey: Could not open HKLM hive: %s\n",
			  win_errstr(result)));
		return result;
	}

	if (key && *key) {
		keyname = talloc_asprintf(mem_ctx, "%s\\%s", path, key);
	} else {
		keyname = talloc_strdup(mem_ctx, path);
	}
	if (keyname == NULL) {
		return WERR_NOMEM;
	}

	ZERO_STRUCT(wkey);
	wkey.name = keyname;

	if (create_key) {
		enum winreg_CreateAction action = REG_ACTION_NONE;

		ZERO_STRUCT(wkeyclass);
		wkeyclass.name = "";

		status = dcerpc_winreg_CreateKey(binding_handle,
						 mem_ctx,
						 hive_handle,
						 wkey,
						 wkeyclass,
						 0,
						 access_mask,
						 NULL,
						 key_handle,
						 &action,
						 &result);
		switch (action) {
			case REG_ACTION_NONE:
				DEBUG(8, ("winreg_printer_openkey:createkey did nothing -- huh?\n"));
				break;
			case REG_CREATED_NEW_KEY:
				DEBUG(8, ("winreg_printer_openkey: createkey created %s\n", keyname));
				break;
			case REG_OPENED_EXISTING_KEY:
				DEBUG(8, ("winreg_printer_openkey: createkey opened existing %s\n", keyname));
				break;
		}
	} else {
		status = dcerpc_winreg_OpenKey(binding_handle,
					       mem_ctx,
					       hive_handle,
					       wkey,
					       0,
					       access_mask,
					       key_handle,
					       &result);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	return WERR_OK;
}

/**
 * @brief Create the registry keyname for the given printer.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  printer  The name of the printer to get the registry key.
 *
 * @return     The registry key or NULL on error.
 */
static char *winreg_printer_data_keyname(TALLOC_CTX *mem_ctx, const char *printer) {
	return talloc_asprintf(mem_ctx, "%s\\%s", TOP_LEVEL_PRINT_PRINTERS_KEY, printer);
}

/**
 * @internal
 *
 * @brief Enumerate values of an opened key handle and retrieve the data.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  winreg_handle The binding handle for the rpc connection.
 *
 * @param[in]  key_hnd  The opened key handle.
 *
 * @param[out] pnum_values A pointer to store he number of values found.
 *
 * @param[out] pnum_values A pointer to store the number of values we found.
 *
 * @return                   WERR_OK on success, the corresponding DOS error
 *                           code if something gone wrong.
 */
static WERROR winreg_printer_enumvalues(TALLOC_CTX *mem_ctx,
					struct dcerpc_binding_handle *winreg_handle,
					struct policy_handle *key_hnd,
					uint32_t *pnum_values,
					struct spoolss_PrinterEnumValues **penum_values)
{
	TALLOC_CTX *tmp_ctx;
	uint32_t num_subkeys, max_subkeylen, max_classlen;
	uint32_t num_values, max_valnamelen, max_valbufsize;
	uint32_t secdescsize;
	uint32_t i;
	NTTIME last_changed_time;
	struct winreg_String classname;

	struct spoolss_PrinterEnumValues *enum_values;

	WERROR result = WERR_OK;
	NTSTATUS status;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	ZERO_STRUCT(classname);

	status = dcerpc_winreg_QueryInfoKey(winreg_handle,
					    tmp_ctx,
					    key_hnd,
					    &classname,
					    &num_subkeys,
					    &max_subkeylen,
					    &max_classlen,
					    &num_values,
					    &max_valnamelen,
					    &max_valbufsize,
					    &secdescsize,
					    &last_changed_time,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_printer_enumvalues: Could not query info: %s\n",
			  nt_errstr(status)));
		result = ntstatus_to_werror(status);
		goto error;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_enumvalues: Could not query info: %s\n",
			  win_errstr(result)));
		goto error;
	}

	if (num_values == 0) {
		*pnum_values = 0;
		TALLOC_FREE(tmp_ctx);
		return WERR_OK;
	}

	enum_values = talloc_array(tmp_ctx, struct spoolss_PrinterEnumValues, num_values);
	if (enum_values == NULL) {
		result = WERR_NOMEM;
		goto error;
	}

	for (i = 0; i < num_values; i++) {
		struct spoolss_PrinterEnumValues val;
		struct winreg_ValNameBuf name_buf;
		enum winreg_Type type = REG_NONE;
		uint8_t *data;
		uint32_t data_size;
		uint32_t length;
		char n = '\0';

		name_buf.name = &n;
		name_buf.size = max_valnamelen + 2;
		name_buf.length = 0;

		data_size = max_valbufsize;
		data = NULL;
		if (data_size) {
			data = (uint8_t *) talloc_zero_size(tmp_ctx, data_size);
		}
		length = 0;

		status = dcerpc_winreg_EnumValue(winreg_handle,
						 tmp_ctx,
						 key_hnd,
						 i,
						 &name_buf,
						 &type,
						 data,
						 data_size ? &data_size : NULL,
						 &length,
						 &result);
		if (W_ERROR_EQUAL(result, WERR_NO_MORE_ITEMS) ) {
			result = WERR_OK;
			status = NT_STATUS_OK;
			break;
		}

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("winreg_printer_enumvalues: Could not enumerate values: %s\n",
				  nt_errstr(status)));
			result = ntstatus_to_werror(status);
			goto error;
		}
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(0, ("winreg_printer_enumvalues: Could not enumerate values: %s\n",
				  win_errstr(result)));
			goto error;
		}

		if (name_buf.name == NULL) {
			result = WERR_INVALID_PARAMETER;
			goto error;
		}

		val.value_name = talloc_strdup(enum_values, name_buf.name);
		if (val.value_name == NULL) {
			result = WERR_NOMEM;
			goto error;
		}
		val.value_name_len = strlen_m_term(val.value_name) * 2;

		val.type = type;
		val.data_length = length;
		val.data = NULL;
		if (val.data_length) {
			val.data = talloc(enum_values, DATA_BLOB);
			if (val.data == NULL) {
				result = WERR_NOMEM;
				goto error;
			}
			*val.data = data_blob_talloc(val.data, data, val.data_length);
		}

		enum_values[i] = val;
	}

	*pnum_values = num_values;
	if (penum_values) {
		*penum_values = talloc_move(mem_ctx, &enum_values);
	}

	result = WERR_OK;

 error:
	TALLOC_FREE(tmp_ctx);
	return result;
}

/**
 * @internal
 *
 * @brief A function to delete a key and its subkeys recurively.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  winreg_handle The binding handle for the rpc connection.
 *
 * @param[in]  hive_handle A opened hive handle to the key.
 *
 * @param[in]  access_mask The access mask to access the key.
 *
 * @param[in]  key      The key to delete
 *
 * @return              WERR_OK on success, the corresponding DOS error
 *                      code if something gone wrong.
 */
static WERROR winreg_printer_delete_subkeys(TALLOC_CTX *mem_ctx,
					    struct dcerpc_binding_handle *winreg_handle,
					    struct policy_handle *hive_handle,
					    uint32_t access_mask,
					    const char *key)
{
	const char **subkeys = NULL;
	uint32_t num_subkeys = 0;
	struct policy_handle key_hnd;
	struct winreg_String wkey = { 0, };
	WERROR result = WERR_OK;
	NTSTATUS status;
	uint32_t i;

	ZERO_STRUCT(key_hnd);
	wkey.name = key;

	DEBUG(2, ("winreg_printer_delete_subkeys: delete key %s\n", key));
	/* open the key */
	status = dcerpc_winreg_OpenKey(winreg_handle,
				       mem_ctx,
				       hive_handle,
				       wkey,
				       0,
				       access_mask,
				       &key_hnd,
				       &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_printer_delete_subkeys: Could not open key %s: %s\n",
			  wkey.name, nt_errstr(status)));
		return ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_delete_subkeys: Could not open key %s: %s\n",
			  wkey.name, win_errstr(result)));
		return result;
	}

	status = dcerpc_winreg_enum_keys(mem_ctx,
					 winreg_handle,
					 &key_hnd,
					 &num_subkeys,
					 &subkeys,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	for (i = 0; i < num_subkeys; i++) {
		/* create key + subkey */
		char *subkey = talloc_asprintf(mem_ctx, "%s\\%s", key, subkeys[i]);
		if (subkey == NULL) {
			goto done;
		}

		DEBUG(2, ("winreg_printer_delete_subkeys: delete subkey %s\n", subkey));
		result = winreg_printer_delete_subkeys(mem_ctx,
						       winreg_handle,
						       hive_handle,
						       access_mask,
						       subkey);
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (is_valid_policy_hnd(&key_hnd)) {
		WERROR ignore;
		dcerpc_winreg_CloseKey(winreg_handle, mem_ctx, &key_hnd, &ignore);
	}

	wkey.name = key;

	status = dcerpc_winreg_DeleteKey(winreg_handle,
					 mem_ctx,
					 hive_handle,
					 wkey,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}

done:
	if (is_valid_policy_hnd(&key_hnd)) {
		WERROR ignore;

		dcerpc_winreg_CloseKey(winreg_handle, mem_ctx, &key_hnd, &ignore);
	}

	return result;
}

static WERROR winreg_printer_opendriver(TALLOC_CTX *mem_ctx,
					struct dcerpc_binding_handle *winreg_handle,
					const char *drivername,
					const char *architecture,
					uint32_t version,
					uint32_t access_mask,
					bool create,
					struct policy_handle *hive_hnd,
					struct policy_handle *key_hnd)
{
	WERROR result;
	char *key_name;

	key_name = talloc_asprintf(mem_ctx, "%s\\Environments\\%s\\Drivers\\Version-%u",
				   TOP_LEVEL_CONTROL_KEY,
				   architecture, version);
	if (!key_name) {
		return WERR_NOMEM;
	}

	result = winreg_printer_openkey(mem_ctx,
					winreg_handle,
					key_name,
					drivername,
					create,
					access_mask,
					hive_hnd,
					key_hnd);
	return result;
}

static WERROR winreg_enumval_to_dword(TALLOC_CTX *mem_ctx,
				      struct spoolss_PrinterEnumValues *v,
				      const char *valuename, uint32_t *dw)
{
	/* just return if it is not the one we are looking for */
	if (strcmp(valuename, v->value_name) != 0) {
		return WERR_NOT_FOUND;
	}

	if (v->type != REG_DWORD) {
		return WERR_INVALID_DATATYPE;
	}

	if (v->data_length != 4) {
		*dw = 0;
		return WERR_OK;
	}

	*dw = IVAL(v->data->data, 0);
	return WERR_OK;
}

static WERROR winreg_enumval_to_sz(TALLOC_CTX *mem_ctx,
				   struct spoolss_PrinterEnumValues *v,
				   const char *valuename, const char **_str)
{
	/* just return if it is not the one we are looking for */
	if (strcmp(valuename, v->value_name) != 0) {
		return WERR_NOT_FOUND;
	}

	if (v->type != REG_SZ) {
		return WERR_INVALID_DATATYPE;
	}

	if (v->data_length == 0) {
		*_str = talloc_strdup(mem_ctx, EMPTY_STRING);
		if (*_str == NULL) {
			return WERR_NOMEM;
		}
		return WERR_OK;
	}

	if (!pull_reg_sz(mem_ctx, v->data, _str)) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

static WERROR winreg_enumval_to_multi_sz(TALLOC_CTX *mem_ctx,
					 struct spoolss_PrinterEnumValues *v,
					 const char *valuename,
					 const char ***array)
{
	/* just return if it is not the one we are looking for */
	if (strcmp(valuename, v->value_name) != 0) {
		return WERR_NOT_FOUND;
	}

	if (v->type != REG_MULTI_SZ) {
		return WERR_INVALID_DATATYPE;
	}

	if (v->data_length == 0) {
		*array = talloc_array(mem_ctx, const char *, 1);
		if (*array == NULL) {
			return WERR_NOMEM;
		}
		*array[0] = NULL;
		return WERR_OK;
	}

	if (!pull_reg_multi_sz(mem_ctx, v->data, array)) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

static WERROR winreg_printer_write_date(TALLOC_CTX *mem_ctx,
					struct dcerpc_binding_handle *winreg_handle,
					struct policy_handle *key_handle,
					const char *value,
					NTTIME data)
{
	struct winreg_String wvalue = { 0, };
	DATA_BLOB blob;
	WERROR result = WERR_OK;
	NTSTATUS status;
	const char *str;
	struct tm *tm;
	time_t t;

	if (data == 0) {
		str = talloc_strdup(mem_ctx, "01/01/1601");
	} else {
		t = nt_time_to_unix(data);
		tm = localtime(&t);
		if (tm == NULL) {
			return map_werror_from_unix(errno);
		}
		str = talloc_asprintf(mem_ctx, "%02d/%02d/%04d",
				      tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900);
	}
	if (!str) {
		return WERR_NOMEM;
	}

	wvalue.name = value;
	if (!push_reg_sz(mem_ctx, &blob, str)) {
		return WERR_NOMEM;
	}
	status = dcerpc_winreg_SetValue(winreg_handle,
					mem_ctx,
					key_handle,
					wvalue,
					REG_SZ,
					blob.data,
					blob.length,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_write_date: Could not set value %s: %s\n",
			wvalue.name, win_errstr(result)));
	}

	return result;
}

static WERROR winreg_printer_date_to_NTTIME(const char *str, NTTIME *data)
{
	struct tm tm;
	time_t t;

	if (strequal(str, "01/01/1601")) {
		*data = 0;
		return WERR_OK;
	}

	ZERO_STRUCT(tm);

	if (sscanf(str, "%d/%d/%d",
		   &tm.tm_mon, &tm.tm_mday, &tm.tm_year) != 3) {
		return WERR_INVALID_PARAMETER;
	}
	tm.tm_mon -= 1;
	tm.tm_year -= 1900;
	tm.tm_isdst = -1;

	t = mktime(&tm);
	unix_to_nt_time(data, t);

	return WERR_OK;
}

static WERROR winreg_printer_write_ver(TALLOC_CTX *mem_ctx,
				       struct dcerpc_binding_handle *winreg_handle,
				       struct policy_handle *key_handle,
				       const char *value,
				       uint64_t data)
{
	struct winreg_String wvalue = { 0, };
	DATA_BLOB blob;
	WERROR result = WERR_OK;
	NTSTATUS status;
	char *str;

	/* FIXME: check format is right,
	 *	this needs to be something like: 6.1.7600.16385 */
	str = talloc_asprintf(mem_ctx, "%u.%u.%u.%u",
			      (unsigned)((data >> 48) & 0xFFFF),
			      (unsigned)((data >> 32) & 0xFFFF),
			      (unsigned)((data >> 16) & 0xFFFF),
			      (unsigned)(data & 0xFFFF));
	if (!str) {
		return WERR_NOMEM;
	}

	wvalue.name = value;
	if (!push_reg_sz(mem_ctx, &blob, str)) {
		return WERR_NOMEM;
	}
	status = dcerpc_winreg_SetValue(winreg_handle,
					mem_ctx,
					key_handle,
					wvalue,
					REG_SZ,
					blob.data,
					blob.length,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_write_date: Could not set value %s: %s\n",
			wvalue.name, win_errstr(result)));
	}

	return result;
}

static WERROR winreg_printer_ver_to_dword(const char *str, uint64_t *data)
{
	unsigned int v1, v2, v3, v4;

	if (sscanf(str, "%u.%u.%u.%u", &v1, &v2, &v3, &v4) != 4) {
		return WERR_INVALID_PARAMETER;
	}

	*data = ((uint64_t)(v1 & 0xFFFF) << 48) +
		((uint64_t)(v2 & 0xFFFF) << 32) +
		((uint64_t)(v3 & 0xFFFF) << 16) +
		(uint64_t)(v2 & 0xFFFF);

	return WERR_OK;
}

/********************************************************************
 Public winreg function for spoolss
********************************************************************/

WERROR winreg_create_printer(TALLOC_CTX *mem_ctx,
			     struct dcerpc_binding_handle *winreg_handle,
			     const char *sharename)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct spoolss_SetPrinterInfo2 *info2;
	struct security_descriptor *secdesc;
	struct winreg_String wkey, wkeyclass;
	const char *path;
	const char *subkeys[] = { SPOOL_DSDRIVER_KEY, SPOOL_DSSPOOLER_KEY, SPOOL_PRINTERDATA_KEY };
	uint32_t i, count = ARRAY_SIZE(subkeys);
	uint32_t info2_mask = 0;
	WERROR result = WERR_OK;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, sharename);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					"",
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (W_ERROR_IS_OK(result)) {
		DEBUG(2, ("winreg_create_printer: Skipping, %s already exists\n", path));
		goto done;
	} else if (W_ERROR_EQUAL(result, WERR_BADFILE)) {
		DEBUG(2, ("winreg_create_printer: Creating default values in %s\n", path));
	} else if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_create_printer: Could not open key %s: %s\n",
			path, win_errstr(result)));
		goto done;
	}

	/* Create the main key */
	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					"",
					true,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_create_printer_keys: Could not create key %s: %s\n",
			path, win_errstr(result)));
		goto done;
	}

	if (is_valid_policy_hnd(&key_hnd)) {
		dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &result);
	}

	/* Create subkeys */
	for (i = 0; i < count; i++) {
		NTSTATUS status;
		enum winreg_CreateAction action = REG_ACTION_NONE;

		ZERO_STRUCT(key_hnd);
		ZERO_STRUCT(wkey);

		wkey.name = talloc_asprintf(tmp_ctx, "%s\\%s", path, subkeys[i]);
		if (wkey.name == NULL) {
			result = WERR_NOMEM;
			goto done;
		}

		ZERO_STRUCT(wkeyclass);
		wkeyclass.name = "";

		status = dcerpc_winreg_CreateKey(winreg_handle,
						 tmp_ctx,
						 &hive_hnd,
						 wkey,
						 wkeyclass,
						 0,
						 access_mask,
						 NULL,
						 &key_hnd,
						 &action,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(0, ("winreg_create_printer_keys: Could not create key %s: %s\n",
				wkey.name, win_errstr(result)));
			goto done;
		}

		if (strequal(subkeys[i], SPOOL_DSSPOOLER_KEY)) {
			const char *dnssuffix;
			const char *longname;
			const char *uncname;

			status = dcerpc_winreg_set_sz(tmp_ctx,
						      winreg_handle,
						      &key_hnd,
						      SPOOL_REG_PRINTERNAME,
						      sharename,
						      &result);
			if (!NT_STATUS_IS_OK(status)) {
				result = ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}

			status = dcerpc_winreg_set_sz(tmp_ctx,
						      winreg_handle,
						      &key_hnd,
						      SPOOL_REG_SHORTSERVERNAME,
						      global_myname(),
						      &result);
			if (!NT_STATUS_IS_OK(status)) {
				result = ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}

			/* We make the assumption that the netbios name
			 * is the same as the DNS name since the former
			 * will be what we used to join the domain
			 */
			dnssuffix = get_mydnsdomname(tmp_ctx);
			if (dnssuffix != NULL && dnssuffix[0] != '\0') {
				longname = talloc_asprintf(tmp_ctx, "%s.%s", global_myname(), dnssuffix);
			} else {
				longname = talloc_strdup(tmp_ctx, global_myname());
			}
			if (longname == NULL) {
				result = WERR_NOMEM;
				goto done;
			}

			status = dcerpc_winreg_set_sz(tmp_ctx,
						      winreg_handle,
						      &key_hnd,
						      SPOOL_REG_SERVERNAME,
						      longname,
						      &result);
			if (!NT_STATUS_IS_OK(status)) {
				result = ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}

			uncname = talloc_asprintf(tmp_ctx, "\\\\%s\\%s",
						  longname, sharename);
			if (uncname == NULL) {
				result = WERR_NOMEM;
				goto done;
			}

			status = dcerpc_winreg_set_sz(tmp_ctx,
						      winreg_handle,
						      &key_hnd,
						      SPOOL_REG_UNCNAME,
						      uncname,
						      &result);
			if (!NT_STATUS_IS_OK(status)) {
				result = ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}

			status = dcerpc_winreg_set_dword(tmp_ctx,
							 winreg_handle,
							 &key_hnd,
							 SPOOL_REG_VERSIONNUMBER,
							 4,
							 &result);
			if (!NT_STATUS_IS_OK(status)) {
				result = ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}

			status = dcerpc_winreg_set_dword(tmp_ctx,
							 winreg_handle,
							 &key_hnd,
							 SPOOL_REG_PRINTSTARTTIME,
							 0,
							 &result);
			if (!NT_STATUS_IS_OK(status)) {
				result = ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}

			status = dcerpc_winreg_set_dword(tmp_ctx,
							 winreg_handle,
							 &key_hnd,
							 SPOOL_REG_PRINTENDTIME,
							 0,
							 &result);
			if (!NT_STATUS_IS_OK(status)) {
				result = ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}

			status = dcerpc_winreg_set_dword(tmp_ctx,
							 winreg_handle,
							 &key_hnd,
							 SPOOL_REG_PRIORITY,
							 1,
							 &result);
			if (!NT_STATUS_IS_OK(status)) {
				result = ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}

			status = dcerpc_winreg_set_dword(tmp_ctx,
							 winreg_handle,
							 &key_hnd,
							 SPOOL_REG_PRINTKEEPPRINTEDJOBS,
							 0,
							 &result);
			if (!NT_STATUS_IS_OK(status)) {
				result = ntstatus_to_werror(status);
			}
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}
		}

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &result);
		}
	}
	info2 = talloc_zero(tmp_ctx, struct spoolss_SetPrinterInfo2);
	if (info2 == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	info2->printername = sharename;
	if (info2->printername == NULL) {
		result = WERR_NOMEM;
		goto done;
	}
	info2_mask |= SPOOLSS_PRINTER_INFO_PRINTERNAME;

	info2->sharename = sharename;
	info2_mask |= SPOOLSS_PRINTER_INFO_SHARENAME;

	info2->portname = SAMBA_PRINTER_PORT_NAME;
	info2_mask |= SPOOLSS_PRINTER_INFO_PORTNAME;

	info2->printprocessor = "winprint";
	info2_mask |= SPOOLSS_PRINTER_INFO_PRINTPROCESSOR;

	info2->datatype = "RAW";
	info2_mask |= SPOOLSS_PRINTER_INFO_DATATYPE;

	info2->comment = "";
	info2_mask |= SPOOLSS_PRINTER_INFO_COMMENT;

	info2->attributes = PRINTER_ATTRIBUTE_SAMBA;
	info2_mask |= SPOOLSS_PRINTER_INFO_ATTRIBUTES;

	info2->starttime = 0; /* Minutes since 12:00am GMT */
	info2_mask |= SPOOLSS_PRINTER_INFO_STARTTIME;

	info2->untiltime = 0; /* Minutes since 12:00am GMT */
	info2_mask |= SPOOLSS_PRINTER_INFO_UNTILTIME;

	info2->priority = 1;
	info2_mask |= SPOOLSS_PRINTER_INFO_PRIORITY;

	info2->defaultpriority = 1;
	info2_mask |= SPOOLSS_PRINTER_INFO_DEFAULTPRIORITY;

	result = spoolss_create_default_secdesc(tmp_ctx, &secdesc);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}
	info2_mask |= SPOOLSS_PRINTER_INFO_SECDESC;

	/*
	 * Don't write a default Device Mode to the registry! The Device Mode is
	 * only written to disk with a SetPrinter level 2 or 8.
	 */

	result = winreg_update_printer(tmp_ctx,
				       winreg_handle,
				       sharename,
				       info2_mask,
				       info2,
				       NULL,
				       secdesc);

done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	talloc_free(tmp_ctx);
	return result;
}

WERROR winreg_update_printer(TALLOC_CTX *mem_ctx,
			     struct dcerpc_binding_handle *winreg_handle,
			     const char *sharename,
			     uint32_t info2_mask,
			     struct spoolss_SetPrinterInfo2 *info2,
			     struct spoolss_DeviceMode *devmode,
			     struct security_descriptor *secdesc)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	int snum = lp_servicenumber(sharename);
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	char *path;
	WERROR result = WERR_OK;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, sharename);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					"",
					true,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_update_printer: Could not open key %s: %s\n",
			path, win_errstr(result)));
		goto done;
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_ATTRIBUTES) {
		status = dcerpc_winreg_set_dword(tmp_ctx,
						 winreg_handle,
						 &key_hnd,
						 "Attributes",
						 info2->attributes,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

#if 0
	if (info2_mask & SPOOLSS_PRINTER_INFO_AVERAGEPPM) {
		status = dcerpc_winreg_set_dword(tmp_ctx,
						 winreg_handle,
						 &key_hnd,
						 "AveragePpm",
						 info2->attributes,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}
#endif

	if (info2_mask & SPOOLSS_PRINTER_INFO_COMMENT) {
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Description",
					      info2->comment,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_DATATYPE) {
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Datatype",
					      info2->datatype,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_DEFAULTPRIORITY) {
		status = dcerpc_winreg_set_dword(tmp_ctx,
						 winreg_handle,
						 &key_hnd,
						 "Default Priority",
						 info2->defaultpriority,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_DEVMODE) {
		/*
		 * Some client drivers freak out if there is a NULL devmode
		 * (probably the driver is not checking before accessing
		 * the devmode pointer)   --jerry
		 */
		if (devmode == NULL && lp_default_devmode(snum) && info2 != NULL) {
			result = spoolss_create_default_devmode(tmp_ctx,
								info2->printername,
								&devmode);
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}
		}

		if (devmode->size != (ndr_size_spoolss_DeviceMode(devmode, 0) - devmode->__driverextra_length)) {
			result = WERR_INVALID_PARAM;
			goto done;
		}

		ndr_err = ndr_push_struct_blob(&blob, tmp_ctx, devmode,
				(ndr_push_flags_fn_t) ndr_push_spoolss_DeviceMode);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(0, ("winreg_update_printer: Failed to marshall device mode\n"));
			result = WERR_NOMEM;
			goto done;
		}

		status = dcerpc_winreg_set_binary(tmp_ctx,
						  winreg_handle,
						  &key_hnd,
						  "Default DevMode",
						  &blob,
						  &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_DRIVERNAME) {
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Printer Driver",
					      info2->drivername,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_LOCATION) {
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Location",
					      info2->location,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_PARAMETERS) {
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Parameters",
					      info2->parameters,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_PORTNAME) {
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Port",
					      info2->portname,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_PRINTERNAME) {
		/*
		 * in addprinter: no servername and the printer is the name
		 * in setprinter: servername is \\server
		 *                and printer is \\server\\printer
		 *
		 * Samba manages only local printers.
		 * we currently don't support things like i
		 * path=\\other_server\printer
		 *
		 * We only store the printername, not \\server\printername
		 */
		const char *p = strrchr(info2->printername, '\\');
		if (p == NULL) {
			p = info2->printername;
		} else {
			p++;
		}
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Name",
					      p,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_PRINTPROCESSOR) {
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Print Processor",
					      info2->printprocessor,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_PRIORITY) {
		status = dcerpc_winreg_set_dword(tmp_ctx,
						 winreg_handle,
						 &key_hnd,
						 "Priority",
						 info2->priority,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_SECDESC) {
		/*
		 * We need a security descriptor, if it isn't specified by
		 * AddPrinter{Ex} then create a default descriptor.
		 */
		if (secdesc == NULL) {
			result = spoolss_create_default_secdesc(tmp_ctx, &secdesc);
			if (!W_ERROR_IS_OK(result)) {
				goto done;
			}
		}
		result = winreg_set_printer_secdesc(tmp_ctx,
						    winreg_handle,
						    sharename,
						    secdesc);
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_SEPFILE) {
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Separator File",
					      info2->sepfile,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_SHARENAME) {
		status = dcerpc_winreg_set_sz(tmp_ctx,
					      winreg_handle,
					      &key_hnd,
					      "Share Name",
					      info2->sharename,
					      &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_STARTTIME) {
		status = dcerpc_winreg_set_dword(tmp_ctx,
						 winreg_handle,
						 &key_hnd,
						 "StartTime",
						 info2->starttime,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_STATUS) {
		status = dcerpc_winreg_set_dword(tmp_ctx,
						 winreg_handle,
						 &key_hnd,
						 "Status",
						 info2->status,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2_mask & SPOOLSS_PRINTER_INFO_UNTILTIME) {
		status = dcerpc_winreg_set_dword(tmp_ctx,
						 winreg_handle,
						 &key_hnd,
						 "UntilTime",
						 info2->untiltime,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
		}
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	status = dcerpc_winreg_set_dword(tmp_ctx,
					 winreg_handle,
					 &key_hnd,
					 "ChangeID",
					 winreg_printer_rev_changeid(),
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_get_printer(TALLOC_CTX *mem_ctx,
			  struct dcerpc_binding_handle *winreg_handle,
			  const char *printer,
			  struct spoolss_PrinterInfo2 **pinfo2)
{
	struct spoolss_PrinterInfo2 *info2;
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct spoolss_PrinterEnumValues *enum_values = NULL;
	struct spoolss_PrinterEnumValues *v = NULL;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	int snum = lp_servicenumber(printer);
	uint32_t num_values = 0;
	uint32_t i;
	char *path;
	NTSTATUS status;
	WERROR result = WERR_OK;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, printer);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					"",
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("winreg_get_printer: Could not open key %s: %s\n",
			  path, win_errstr(result)));
		goto done;
	}

	result = winreg_printer_enumvalues(tmp_ctx,
					   winreg_handle,
					   &key_hnd,
					   &num_values,
					   &enum_values);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_get_printer: Could not enumerate values in %s: %s\n",
			  path, win_errstr(result)));
		goto done;
	}

	info2 = talloc_zero(tmp_ctx, struct spoolss_PrinterInfo2);
	if (info2 == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	FILL_STRING(info2, EMPTY_STRING, info2->servername);
	FILL_STRING(info2, EMPTY_STRING, info2->printername);
	FILL_STRING(info2, EMPTY_STRING, info2->sharename);
	FILL_STRING(info2, EMPTY_STRING, info2->portname);
	FILL_STRING(info2, EMPTY_STRING, info2->drivername);
	FILL_STRING(info2, EMPTY_STRING, info2->comment);
	FILL_STRING(info2, EMPTY_STRING, info2->location);
	FILL_STRING(info2, EMPTY_STRING, info2->sepfile);
	FILL_STRING(info2, EMPTY_STRING, info2->printprocessor);
	FILL_STRING(info2, EMPTY_STRING, info2->datatype);
	FILL_STRING(info2, EMPTY_STRING, info2->parameters);

	for (i = 0; i < num_values; i++) {
		v = &enum_values[i];

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Name",
					      &info2->printername);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Share Name",
					      &info2->sharename);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Port",
					      &info2->portname);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Description",
					      &info2->comment);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Location",
					      &info2->location);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Separator File",
					      &info2->sepfile);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Print Processor",
					      &info2->printprocessor);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Datatype",
					      &info2->datatype);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Parameters",
					      &info2->parameters);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info2,
					      v,
					      "Printer Driver",
					      &info2->drivername);
		CHECK_ERROR(result);

		result = winreg_enumval_to_dword(info2,
						 v,
						 "Attributes",
						 &info2->attributes);
		CHECK_ERROR(result);

		result = winreg_enumval_to_dword(info2,
						 v,
						 "Priority",
						 &info2->priority);
		CHECK_ERROR(result);

		result = winreg_enumval_to_dword(info2,
						 v,
						 "Default Priority",
						 &info2->defaultpriority);
		CHECK_ERROR(result);

		result = winreg_enumval_to_dword(info2,
						 v,
						 "StartTime",
						 &info2->starttime);
		CHECK_ERROR(result);

		result = winreg_enumval_to_dword(info2,
						 v,
						 "UntilTime",
						 &info2->untiltime);
		CHECK_ERROR(result);

		result = winreg_enumval_to_dword(info2,
						 v,
						 "Status",
						 &info2->status);
		CHECK_ERROR(result);

		result = winreg_enumval_to_dword(info2,
						 v,
						 "StartTime",
						 &info2->starttime);
		CHECK_ERROR(result);
	}

	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_get_printer: winreg_enumval_to_TYPE() failed "
					"for %s: %s\n",
					v->value_name,
					win_errstr(result)));
		goto done;
	}

	/* Construct the Device Mode */
	status = dcerpc_winreg_query_binary(tmp_ctx,
					    winreg_handle,
					    &key_hnd,
					    "Default DevMode",
					    &blob,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (W_ERROR_IS_OK(result)) {
		info2->devmode = talloc_zero(info2, struct spoolss_DeviceMode);
		if (info2->devmode == NULL) {
			result = WERR_NOMEM;
			goto done;
		}
		ndr_err = ndr_pull_struct_blob(&blob,
					       info2->devmode,
					       info2->devmode,
					       (ndr_pull_flags_fn_t) ndr_pull_spoolss_DeviceMode);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(0, ("winreg_get_printer: Failed to unmarshall device mode\n"));
			result = WERR_NOMEM;
			goto done;
		}
	}

	if (info2->devmode == NULL && lp_default_devmode(snum)) {
		result = spoolss_create_default_devmode(info2,
							info2->printername,
							&info2->devmode);
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
	}

	if (info2->devmode) {
		info2->devmode->size = ndr_size_spoolss_DeviceMode(info2->devmode, 0) - info2->devmode->driverextra_data.length;
	}

	result = winreg_get_printer_secdesc(info2,
					    winreg_handle,
					    printer,
					    &info2->secdesc);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Fix for OS/2 drivers. */
	if (get_remote_arch() == RA_OS2) {
		spoolss_map_to_os2_driver(info2, &info2->drivername);
	}

	if (pinfo2) {
		*pinfo2 = talloc_move(mem_ctx, &info2);
	}

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_get_printer_secdesc(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *winreg_handle,
				  const char *sharename,
				  struct spoolss_security_descriptor **psecdesc)
{
	struct spoolss_security_descriptor *secdesc;
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	const char *path;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;
	WERROR result;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, sharename);
	if (path == NULL) {
		talloc_free(tmp_ctx);
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					"",
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		if (W_ERROR_EQUAL(result, WERR_BADFILE)) {
			goto create_default;
		}
		goto done;
	}

	status = dcerpc_winreg_query_sd(tmp_ctx,
					winreg_handle,
					&key_hnd,
					"Security",
					&secdesc,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		if (W_ERROR_EQUAL(result, WERR_BADFILE)) {
			goto create_default;
		}
		goto done;
	}

	if (psecdesc) {
		*psecdesc = talloc_move(mem_ctx, &secdesc);
	}

	result = WERR_OK;
	goto done;

create_default:
	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					"",
					true,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	result = spoolss_create_default_secdesc(tmp_ctx, &secdesc);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* If security descriptor is owned by S-1-1-0 and winbindd is up,
	   this security descriptor has been created when winbindd was
	   down.  Take ownership of security descriptor. */
	if (dom_sid_equal(secdesc->owner_sid, &global_sid_World)) {
		struct dom_sid owner_sid;

		/* Change sd owner to workgroup administrator */

		if (secrets_fetch_domain_sid(lp_workgroup(), &owner_sid)) {
			struct spoolss_security_descriptor *new_secdesc;
			size_t size;

			/* Create new sd */
			sid_append_rid(&owner_sid, DOMAIN_RID_ADMINISTRATOR);

			new_secdesc = make_sec_desc(tmp_ctx,
						    secdesc->revision,
						    secdesc->type,
						    &owner_sid,
						    secdesc->group_sid,
						    secdesc->sacl,
						    secdesc->dacl,
						    &size);

			if (new_secdesc == NULL) {
				result = WERR_NOMEM;
				goto done;
			}

			/* Swap with other one */
			secdesc = new_secdesc;
		}
	}

	status = dcerpc_winreg_set_sd(tmp_ctx,
					  winreg_handle,
					  &key_hnd,
					  "Security",
					  secdesc,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	if (psecdesc) {
		*psecdesc = talloc_move(mem_ctx, &secdesc);
	}

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	talloc_free(tmp_ctx);
	return result;
}

WERROR winreg_set_printer_secdesc(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *winreg_handle,
				  const char *sharename,
				  const struct spoolss_security_descriptor *secdesc)
{
	const struct spoolss_security_descriptor *new_secdesc = secdesc;
	struct spoolss_security_descriptor *old_secdesc;
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	const char *path;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;
	WERROR result;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, sharename);
	if (path == NULL) {
		talloc_free(tmp_ctx);
		return WERR_NOMEM;
	}

	/*
	 * The old owner and group sids of the security descriptor are not
	 * present when new ACEs are added or removed by changing printer
	 * permissions through NT.  If they are NULL in the new security
	 * descriptor then copy them over from the old one.
	 */
	if (!secdesc->owner_sid || !secdesc->group_sid) {
		struct dom_sid *owner_sid, *group_sid;
		struct security_acl *dacl, *sacl;
		size_t size;

		result = winreg_get_printer_secdesc(tmp_ctx,
						    winreg_handle,
						    sharename,
						    &old_secdesc);
		if (!W_ERROR_IS_OK(result)) {
			talloc_free(tmp_ctx);
			return result;
		}

		/* Pick out correct owner and group sids */
		owner_sid = secdesc->owner_sid ?
			    secdesc->owner_sid :
			    old_secdesc->owner_sid;

		group_sid = secdesc->group_sid ?
			    secdesc->group_sid :
			    old_secdesc->group_sid;

		dacl = secdesc->dacl ?
		       secdesc->dacl :
		       old_secdesc->dacl;

		sacl = secdesc->sacl ?
		       secdesc->sacl :
		       old_secdesc->sacl;

		/* Make a deep copy of the security descriptor */
		new_secdesc = make_sec_desc(tmp_ctx,
					    secdesc->revision,
					    secdesc->type,
					    owner_sid,
					    group_sid,
					    sacl,
					    dacl,
					    &size);
		if (new_secdesc == NULL) {
			talloc_free(tmp_ctx);
			return WERR_NOMEM;
		}
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					"",
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sd(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Security",
				      new_secdesc,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}

done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	talloc_free(tmp_ctx);
	return result;
}

/* Set printer data over the winreg pipe. */
WERROR winreg_set_printer_dataex(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *winreg_handle,
				 const char *printer,
				 const char *key,
				 const char *value,
				 enum winreg_Type type,
				 uint8_t *data,
				 uint32_t data_size)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct winreg_String wvalue = { 0, };
	char *path;
	WERROR result = WERR_OK;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, printer);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	DEBUG(8, ("winreg_set_printer_dataex: Open printer key %s, value %s, access_mask: 0x%05x for [%s]\n",
			key, value, access_mask, printer));
	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					key,
					true,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_set_printer_dataex: Could not open key %s: %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	wvalue.name = value;
	status = dcerpc_winreg_SetValue(winreg_handle,
					tmp_ctx,
					&key_hnd,
					wvalue,
					type,
					data,
					data_size,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_set_printer_dataex: Could not set value %s: %s\n",
			  value, nt_errstr(status)));
		result = ntstatus_to_werror(status);
	}

done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

/* Get printer data over a winreg pipe. */
WERROR winreg_get_printer_dataex(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *winreg_handle,
				 const char *printer,
				 const char *key,
				 const char *value,
				 enum winreg_Type *type,
				 uint8_t **data,
				 uint32_t *data_size)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct winreg_String wvalue;
	enum winreg_Type type_in = REG_NONE;
	char *path;
	uint8_t *data_in = NULL;
	uint32_t data_in_size = 0;
	uint32_t value_len = 0;
	WERROR result = WERR_OK;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, printer);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					key,
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("winreg_get_printer_dataex: Could not open key %s: %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	wvalue.name = value;

	/*
	 * call QueryValue once with data == NULL to get the
	 * needed memory size to be allocated, then allocate
	 * data buffer and call again.
	 */
	status = dcerpc_winreg_QueryValue(winreg_handle,
					  tmp_ctx,
					  &key_hnd,
					  &wvalue,
					  &type_in,
					  NULL,
					  &data_in_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_get_printer_dataex: Could not query value %s: %s\n",
			  value, nt_errstr(status)));
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	data_in = (uint8_t *) TALLOC(tmp_ctx, data_in_size);
	if (data_in == NULL) {
		result = WERR_NOMEM;
		goto done;
	}
	value_len = 0;

	status = dcerpc_winreg_QueryValue(winreg_handle,
					  tmp_ctx,
					  &key_hnd,
					  &wvalue,
					  &type_in,
					  data_in,
					  &data_in_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_get_printer_dataex: Could not query value %s: %s\n",
			  value, nt_errstr(status)));
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	*type = type_in;
	*data_size = data_in_size;
	if (data_in_size) {
		*data = talloc_move(mem_ctx, &data_in);
	}

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

/* Enumerate on the values of a given key and provide the data. */
WERROR winreg_enum_printer_dataex(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *winreg_handle,
				  const char *printer,
				  const char *key,
				  uint32_t *pnum_values,
				  struct spoolss_PrinterEnumValues **penum_values)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;

	struct spoolss_PrinterEnumValues *enum_values = NULL;
	uint32_t num_values = 0;
	char *path;
	WERROR result = WERR_OK;

	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, printer);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					key,
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("winreg_enum_printer_dataex: Could not open key %s: %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	result = winreg_printer_enumvalues(tmp_ctx,
					   winreg_handle,
					   &key_hnd,
					   &num_values,
					   &enum_values);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_enum_printer_dataex: Could not enumerate values in %s: %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	*pnum_values = num_values;
	if (penum_values) {
		*penum_values = talloc_move(mem_ctx, &enum_values);
	}

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

/* Delete printer data over a winreg pipe. */
WERROR winreg_delete_printer_dataex(TALLOC_CTX *mem_ctx,
				    struct dcerpc_binding_handle *winreg_handle,
				    const char *printer,
				    const char *key,
				    const char *value)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct winreg_String wvalue = { 0, };
	char *path;
	WERROR result = WERR_OK;
	NTSTATUS status;

	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, printer);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					key,
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_delete_printer_dataex: Could not open key %s: %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	wvalue.name = value;
	status = dcerpc_winreg_DeleteValue(winreg_handle,
					   tmp_ctx,
					   &key_hnd,
					   wvalue,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_delete_printer_dataex: Could not delete value %s: %s\n",
			  value, nt_errstr(status)));
		result = ntstatus_to_werror(status);
	}

done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

/* Enumerate on the subkeys of a given key and provide the data. */
WERROR winreg_enum_printer_key(TALLOC_CTX *mem_ctx,
			       struct dcerpc_binding_handle *winreg_handle,
			       const char *printer,
			       const char *key,
			       uint32_t *pnum_subkeys,
			       const char ***psubkeys)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	char *path;
	const char **subkeys = NULL;
	uint32_t num_subkeys = -1;

	WERROR result = WERR_OK;
	NTSTATUS status;

	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, printer);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					key,
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("winreg_enum_printer_key: Could not open key %s: %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_enum_keys(tmp_ctx,
					 winreg_handle,
					 &key_hnd,
					 &num_subkeys,
					 &subkeys,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_enum_printer_key: Could not enumerate subkeys in %s: %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	*pnum_subkeys = num_subkeys;
	if (psubkeys) {
		*psubkeys = talloc_move(mem_ctx, &subkeys);
	}

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

/* Delete a key with subkeys of a given printer. */
WERROR winreg_delete_printer_key(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *winreg_handle,
				 const char *printer,
				 const char *key)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	char *keyname;
	char *path;
	WERROR result;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, printer);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					key,
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		/* key doesn't exist */
		if (W_ERROR_EQUAL(result, WERR_BADFILE)) {
			result = WERR_OK;
			goto done;
		}

		DEBUG(0, ("winreg_delete_printer_key: Could not open key %s: %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	if (is_valid_policy_hnd(&key_hnd)) {
		dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &result);
	}

	if (key == NULL || key[0] == '\0') {
		keyname = path;
	} else {
		keyname = talloc_asprintf(tmp_ctx,
					  "%s\\%s",
					  path,
					  key);
		if (keyname == NULL) {
			result = WERR_NOMEM;
			goto done;
		}
	}

	result = winreg_printer_delete_subkeys(tmp_ctx,
					       winreg_handle,
					       &hive_hnd,
					       access_mask,
					       keyname);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_delete_printer_key: Could not delete key %s: %s\n",
			  key, win_errstr(result)));
		goto done;
	}

done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_printer_update_changeid(TALLOC_CTX *mem_ctx,
				      struct dcerpc_binding_handle *winreg_handle,
				      const char *printer)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	char *path;
	NTSTATUS status;
	WERROR result;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, printer);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					"",
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_update_changeid: Could not open key %s: %s\n",
			  path, win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_set_dword(tmp_ctx,
					 winreg_handle,
					 &key_hnd,
					 "ChangeID",
					 winreg_printer_rev_changeid(),
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_printer_get_changeid(TALLOC_CTX *mem_ctx,
				   struct dcerpc_binding_handle *winreg_handle,
				   const char *printer,
				   uint32_t *pchangeid)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	uint32_t changeid = 0;
	char *path;
	NTSTATUS status;
	WERROR result;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	path = winreg_printer_data_keyname(tmp_ctx, printer);
	if (path == NULL) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					path,
					"",
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("winreg_printer_get_changeid: Could not open key %s: %s\n",
			  path, win_errstr(result)));
		goto done;
	}

	DEBUG(10, ("winreg_printer_get_changeid: get changeid from %s\n", path));

	status = dcerpc_winreg_query_dword(tmp_ctx,
					   winreg_handle,
					   &key_hnd,
					   "ChangeID",
					   &changeid,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	if (pchangeid) {
		*pchangeid = changeid;
	}

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

/*
 * The special behaviour of the spoolss forms is documented at the website:
 *
 * Managing Win32 Printserver Forms
 * http://unixwiz.net/techtips/winspooler-forms.html
 */

WERROR winreg_printer_addform1(TALLOC_CTX *mem_ctx,
			       struct dcerpc_binding_handle *winreg_handle,
			       struct spoolss_AddFormInfo1 *form)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct winreg_String wvalue = { 0, };
	DATA_BLOB blob;
	uint32_t num_info = 0;
	union spoolss_FormInfo *info = NULL;
	uint32_t i;
	WERROR result;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					TOP_LEVEL_CONTROL_FORMS_KEY,
					"",
					true,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_addform1: Could not open key %s: %s\n",
			  TOP_LEVEL_CONTROL_FORMS_KEY, win_errstr(result)));
		goto done;
	}

	result = winreg_printer_enumforms1(tmp_ctx, winreg_handle,
					   &num_info, &info);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_addform: Could not enum keys %s: %s\n",
			  TOP_LEVEL_CONTROL_FORMS_KEY, win_errstr(result)));
		goto done;
	}

	/* If form name already exists or is builtin return ALREADY_EXISTS */
	for (i = 0; i < num_info; i++) {
		if (strequal(info[i].info1.form_name, form->form_name)) {
			result = WERR_FILE_EXISTS;
			goto done;
		}
	}

	wvalue.name = form->form_name;

	blob = data_blob_talloc(tmp_ctx, NULL, 32);
	SIVAL(blob.data,  0, form->size.width);
	SIVAL(blob.data,  4, form->size.height);
	SIVAL(blob.data,  8, form->area.left);
	SIVAL(blob.data, 12, form->area.top);
	SIVAL(blob.data, 16, form->area.right);
	SIVAL(blob.data, 20, form->area.bottom);
	SIVAL(blob.data, 24, num_info + 1); /* FIXME */
	SIVAL(blob.data, 28, form->flags);

	status = dcerpc_winreg_SetValue(winreg_handle,
					tmp_ctx,
					&key_hnd,
					wvalue,
					REG_BINARY,
					blob.data,
					blob.length,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_printer_addform1: Could not set value %s: %s\n",
			  wvalue.name, nt_errstr(status)));
		result = ntstatus_to_werror(status);
	}

done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(info);
	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_printer_enumforms1(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *winreg_handle,
				 uint32_t *pnum_info,
				 union spoolss_FormInfo **pinfo)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	union spoolss_FormInfo *info;
	struct spoolss_PrinterEnumValues *enum_values = NULL;
	uint32_t num_values = 0;
	uint32_t num_builtin = ARRAY_SIZE(builtin_forms1);
	uint32_t i;
	WERROR result;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					TOP_LEVEL_CONTROL_FORMS_KEY,
					"",
					true,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		/* key doesn't exist */
		if (W_ERROR_EQUAL(result, WERR_BADFILE)) {
			result = WERR_OK;
			goto done;
		}

		DEBUG(0, ("winreg_printer_enumforms1: Could not open key %s: %s\n",
			  TOP_LEVEL_CONTROL_FORMS_KEY, win_errstr(result)));
		goto done;
	}

	result = winreg_printer_enumvalues(tmp_ctx,
					   winreg_handle,
					   &key_hnd,
					   &num_values,
					   &enum_values);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_enumforms1: Could not enumerate values in %s: %s\n",
			  TOP_LEVEL_CONTROL_FORMS_KEY, win_errstr(result)));
		goto done;
	}

	info = talloc_array(tmp_ctx, union spoolss_FormInfo, num_builtin + num_values);
	if (info == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	/* Enumerate BUILTIN forms */
	for (i = 0; i < num_builtin; i++) {
		info[i].info1 = builtin_forms1[i];
	}

	/* Enumerate registry forms */
	for (i = 0; i < num_values; i++) {
		union spoolss_FormInfo val;

		if (enum_values[i].type != REG_BINARY ||
		    enum_values[i].data_length != 32) {
			continue;
		}

		val.info1.form_name = talloc_strdup(info, enum_values[i].value_name);
		if (val.info1.form_name == NULL) {
			result = WERR_NOMEM;
			goto done;
		}

		val.info1.size.width  = IVAL(enum_values[i].data->data,  0);
		val.info1.size.height = IVAL(enum_values[i].data->data,  4);
		val.info1.area.left   = IVAL(enum_values[i].data->data,  8);
		val.info1.area.top    = IVAL(enum_values[i].data->data, 12);
		val.info1.area.right  = IVAL(enum_values[i].data->data, 16);
		val.info1.area.bottom = IVAL(enum_values[i].data->data, 20);
		/* skip form index      IVAL(enum_values[i].data->data, 24)));*/
		val.info1.flags       = (enum spoolss_FormFlags) IVAL(enum_values[i].data->data, 28);

		info[i + num_builtin] = val;
	}

	*pnum_info = num_builtin + num_values;
	if (pinfo) {
		*pinfo = talloc_move(mem_ctx, &info);
	}

done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(enum_values);
	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_printer_deleteform1(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *winreg_handle,
				  const char *form_name)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct winreg_String wvalue = { 0, };
	uint32_t num_builtin = ARRAY_SIZE(builtin_forms1);
	uint32_t i;
	WERROR result = WERR_OK;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx;

	for (i = 0; i < num_builtin; i++) {
		if (strequal(builtin_forms1[i].form_name, form_name)) {
			return WERR_INVALID_PARAMETER;
		}
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					TOP_LEVEL_CONTROL_FORMS_KEY,
					"",
					false,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_deleteform1: Could not open key %s: %s\n",
			  TOP_LEVEL_CONTROL_FORMS_KEY, win_errstr(result)));
		if (W_ERROR_EQUAL(result, WERR_BADFILE)) {
			result = WERR_INVALID_FORM_NAME;
		}
		goto done;
	}

	wvalue.name = form_name;
	status = dcerpc_winreg_DeleteValue(winreg_handle,
					   tmp_ctx,
					   &key_hnd,
					   wvalue,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		/* If the value doesn't exist, return WERR_INVALID_FORM_NAME */
		DEBUG(0, ("winreg_printer_delteform1: Could not delete value %s: %s\n",
			  wvalue.name, nt_errstr(status)));
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (W_ERROR_EQUAL(result, WERR_BADFILE)) {
		result = WERR_INVALID_FORM_NAME;
	}

done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_printer_setform1(TALLOC_CTX *mem_ctx,
			       struct dcerpc_binding_handle *winreg_handle,
			       const char *form_name,
			       struct spoolss_AddFormInfo1 *form)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct winreg_String wvalue = { 0, };
	DATA_BLOB blob;
	uint32_t num_builtin = ARRAY_SIZE(builtin_forms1);
	uint32_t i;
	WERROR result;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = NULL;

	for (i = 0; i < num_builtin; i++) {
		if (strequal(builtin_forms1[i].form_name, form->form_name)) {
			result = WERR_INVALID_PARAM;
			goto done;
		}
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					TOP_LEVEL_CONTROL_FORMS_KEY,
					"",
					true,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_printer_setform1: Could not open key %s: %s\n",
			  TOP_LEVEL_CONTROL_FORMS_KEY, win_errstr(result)));
		goto done;
	}

	/* If form_name != form->form_name then we renamed the form */
	if (strequal(form_name, form->form_name)) {
		result = winreg_printer_deleteform1(tmp_ctx, winreg_handle,
						    form_name);
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(0, ("winreg_printer_setform1: Could not open key %s: %s\n",
				  TOP_LEVEL_CONTROL_FORMS_KEY, win_errstr(result)));
			goto done;
		}
	}

	wvalue.name = form->form_name;

	blob = data_blob_talloc(tmp_ctx, NULL, 32);
	SIVAL(blob.data,  0, form->size.width);
	SIVAL(blob.data,  4, form->size.height);
	SIVAL(blob.data,  8, form->area.left);
	SIVAL(blob.data, 12, form->area.top);
	SIVAL(blob.data, 16, form->area.right);
	SIVAL(blob.data, 20, form->area.bottom);
	SIVAL(blob.data, 24, 42);
	SIVAL(blob.data, 28, form->flags);

	status = dcerpc_winreg_SetValue(winreg_handle,
					tmp_ctx,
					&key_hnd,
					wvalue,
					REG_BINARY,
					blob.data,
					blob.length,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_printer_setform1: Could not set value %s: %s\n",
			  wvalue.name, nt_errstr(status)));
		result = ntstatus_to_werror(status);
	}

done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_printer_getform1(TALLOC_CTX *mem_ctx,
			       struct dcerpc_binding_handle *winreg_handle,
			       const char *form_name,
			       struct spoolss_FormInfo1 *r)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct winreg_String wvalue;
	enum winreg_Type type_in = REG_NONE;
	uint8_t *data_in = NULL;
	uint32_t data_in_size = 0;
	uint32_t value_len = 0;
	uint32_t num_builtin = ARRAY_SIZE(builtin_forms1);
	uint32_t i;
	WERROR result;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx;

	/* check builtin forms first */
	for (i = 0; i < num_builtin; i++) {
		if (strequal(builtin_forms1[i].form_name, form_name)) {
			*r = builtin_forms1[i];
			return WERR_OK;
		}
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	result = winreg_printer_openkey(tmp_ctx,
					winreg_handle,
					TOP_LEVEL_CONTROL_FORMS_KEY,
					"",
					true,
					access_mask,
					&hive_hnd,
					&key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2, ("winreg_printer_getform1: Could not open key %s: %s\n",
			  TOP_LEVEL_CONTROL_FORMS_KEY, win_errstr(result)));
		goto done;
	}

	wvalue.name = form_name;

	/*
	 * call QueryValue once with data == NULL to get the
	 * needed memory size to be allocated, then allocate
	 * data buffer and call again.
	 */
	status = dcerpc_winreg_QueryValue(winreg_handle,
					  tmp_ctx,
					  &key_hnd,
					  &wvalue,
					  &type_in,
					  NULL,
					  &data_in_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_printer_getform1: Could not query value %s: %s\n",
			  wvalue.name, nt_errstr(status)));
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	data_in = (uint8_t *) TALLOC(tmp_ctx, data_in_size);
	if (data_in == NULL) {
		result = WERR_NOMEM;
		goto done;
	}
	value_len = 0;

	status = dcerpc_winreg_QueryValue(winreg_handle,
					  tmp_ctx,
					  &key_hnd,
					  &wvalue,
					  &type_in,
					  data_in,
					  &data_in_size,
					  &value_len,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_printer_getform1: Could not query value %s: %s\n",
			  wvalue.name, nt_errstr(status)));
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	r->form_name = talloc_strdup(mem_ctx, form_name);
	if (r->form_name == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	r->size.width  = IVAL(data_in,  0);
	r->size.height = IVAL(data_in,  4);
	r->area.left   = IVAL(data_in,  8);
	r->area.top    = IVAL(data_in, 12);
	r->area.right  = IVAL(data_in, 16);
	r->area.bottom = IVAL(data_in, 20);
	/* skip index    IVAL(data_in, 24)));*/
	r->flags       = (enum spoolss_FormFlags) IVAL(data_in, 28);

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_add_driver(TALLOC_CTX *mem_ctx,
			 struct dcerpc_binding_handle *winreg_handle,
			 struct spoolss_AddDriverInfoCtr *r,
			 const char **driver_name,
			 uint32_t *driver_version)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct spoolss_DriverInfo8 info8;
	TALLOC_CTX *tmp_ctx = NULL;
	NTSTATUS status;
	WERROR result;

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);
	ZERO_STRUCT(info8);

	if (!driver_info_ctr_to_info8(r, &info8)) {
		result = WERR_INVALID_PARAMETER;
		goto done;
	}

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	result = winreg_printer_opendriver(tmp_ctx,
					   winreg_handle,
					   info8.driver_name,
					   info8.architecture,
					   info8.version,
					   access_mask, true,
					   &hive_hnd,
					   &key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_add_driver: "
			  "Could not open driver key (%s,%s,%d): %s\n",
			  info8.driver_name, info8.architecture,
			  info8.version, win_errstr(result)));
		goto done;
	}

	/* TODO: "Attributes" ? */

	status = dcerpc_winreg_set_dword(tmp_ctx,
					 winreg_handle,
					 &key_hnd,
					 "Version",
					 info8.version,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Driver",
				      info8.driver_path,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Data File",
				      info8.data_file,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Configuration File",
				      info8.config_file,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Help File",
				      info8.help_file,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_multi_sz(tmp_ctx,
					    winreg_handle,
					    &key_hnd,
					    "Dependent Files",
					    info8.dependent_files,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Monitor",
				      info8.monitor_name,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Datatype",
				      info8.default_datatype,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_multi_sz(tmp_ctx,
					    winreg_handle,
					    &key_hnd, "Previous Names",
					    info8.previous_names,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	result = winreg_printer_write_date(tmp_ctx, winreg_handle,
					   &key_hnd, "DriverDate",
					   info8.driver_date);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	result = winreg_printer_write_ver(tmp_ctx, winreg_handle,
					  &key_hnd, "DriverVersion",
					  info8.driver_version);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Manufacturer",
				      info8.manufacturer_name,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "OEM URL",
				      info8.manufacturer_url,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "HardwareID",
				      info8.hardware_id,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Provider",
				      info8.provider,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "Print Processor",
				      info8.print_processor,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "VendorSetup",
				      info8.vendor_setup,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_multi_sz(tmp_ctx,
					    winreg_handle,
					    &key_hnd,
					    "Color Profiles",
					    info8.color_profiles,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(tmp_ctx,
				      winreg_handle,
				      &key_hnd,
				      "InfPath",
				      info8.inf_path,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_dword(tmp_ctx,
					 winreg_handle,
					 &key_hnd,
					 "PrinterDriverAttributes",
					 info8.printer_driver_attributes,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_winreg_set_multi_sz(tmp_ctx,
					    winreg_handle,
					    &key_hnd,
					    "CoreDependencies",
					    info8.core_driver_dependencies,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	result = winreg_printer_write_date(tmp_ctx, winreg_handle,
					   &key_hnd, "MinInboxDriverVerDate",
					   info8.min_inbox_driver_ver_date);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	result = winreg_printer_write_ver(tmp_ctx, winreg_handle, &key_hnd,
					  "MinInboxDriverVerVersion",
					  info8.min_inbox_driver_ver_version);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	*driver_name = info8.driver_name;
	*driver_version = info8.version;
	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_get_driver(TALLOC_CTX *mem_ctx,
			 struct dcerpc_binding_handle *winreg_handle,
			 const char *architecture,
			 const char *driver_name,
			 uint32_t driver_version,
			 struct spoolss_DriverInfo8 **_info8)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	struct spoolss_DriverInfo8 i8, *info8;
	struct spoolss_PrinterEnumValues *enum_values = NULL;
	struct spoolss_PrinterEnumValues *v;
	uint32_t num_values = 0;
	TALLOC_CTX *tmp_ctx;
	WERROR result;
	uint32_t i;

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);
	ZERO_STRUCT(i8);

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	if (driver_version == DRIVER_ANY_VERSION) {
		/* look for Win2k first and then for NT4 */
		result = winreg_printer_opendriver(tmp_ctx,
						   winreg_handle,
						   driver_name,
						   architecture,
						   3,
						   access_mask, false,
						   &hive_hnd,
						   &key_hnd);
		if (!W_ERROR_IS_OK(result)) {
			result = winreg_printer_opendriver(tmp_ctx,
							   winreg_handle,
							   driver_name,
							   architecture,
							   2,
							   access_mask, false,
							   &hive_hnd,
							   &key_hnd);
		}
	} else {
		/* ok normal case */
		result = winreg_printer_opendriver(tmp_ctx,
						   winreg_handle,
						   driver_name,
						   architecture,
						   driver_version,
						   access_mask, false,
						   &hive_hnd,
						   &key_hnd);
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(5, ("winreg_get_driver: "
			  "Could not open driver key (%s,%s,%d): %s\n",
			  driver_name, architecture,
			  driver_version, win_errstr(result)));
		goto done;
	}

	result = winreg_printer_enumvalues(tmp_ctx,
					   winreg_handle,
					   &key_hnd,
					   &num_values,
					   &enum_values);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_get_driver: "
			  "Could not enumerate values for (%s,%s,%d): %s\n",
			  driver_name, architecture,
			  driver_version, win_errstr(result)));
		goto done;
	}

	info8 = talloc_zero(tmp_ctx, struct spoolss_DriverInfo8);
	if (info8 == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	info8->driver_name = talloc_strdup(info8, driver_name);
	if (info8->driver_name == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	info8->architecture = talloc_strdup(info8, architecture);
	if (info8->architecture == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	result = WERR_OK;

	for (i = 0; i < num_values; i++) {
		const char *tmp_str;
		uint32_t tmp = 0;

		v = &enum_values[i];

		result = winreg_enumval_to_dword(info8, v,
						 "Version",
						 &tmp);
		if (NT_STATUS_IS_OK(result)) {
			info8->version = (enum spoolss_DriverOSVersion) tmp;
		}
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "Driver",
					      &info8->driver_path);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "Data File",
					      &info8->data_file);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "Configuration File",
					      &info8->config_file);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "Help File",
					      &info8->help_file);
		CHECK_ERROR(result);

		result = winreg_enumval_to_multi_sz(info8, v,
						    "Dependent Files",
						    &info8->dependent_files);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "Monitor",
					      &info8->monitor_name);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "Datatype",
					      &info8->default_datatype);
		CHECK_ERROR(result);

		result = winreg_enumval_to_multi_sz(info8, v,
						    "Previous Names",
						    &info8->previous_names);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "DriverDate",
					      &tmp_str);
		if (W_ERROR_IS_OK(result)) {
			result = winreg_printer_date_to_NTTIME(tmp_str,
						&info8->driver_date);
		}
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "DriverVersion",
					      &tmp_str);
		if (W_ERROR_IS_OK(result)) {
			result = winreg_printer_ver_to_dword(tmp_str,
						&info8->driver_version);
		}
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "Manufacturer",
					      &info8->manufacturer_name);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "OEM URL",
					      &info8->manufacturer_url);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "HardwareID",
					      &info8->hardware_id);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "Provider",
					      &info8->provider);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "Print Processor",
					      &info8->print_processor);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "VendorSetup",
					      &info8->vendor_setup);
		CHECK_ERROR(result);

		result = winreg_enumval_to_multi_sz(info8, v,
						    "Color Profiles",
						    &info8->color_profiles);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "InfPath",
					      &info8->inf_path);
		CHECK_ERROR(result);

		result = winreg_enumval_to_dword(info8, v,
						 "PrinterDriverAttributes",
						 &info8->printer_driver_attributes);
		CHECK_ERROR(result);

		result = winreg_enumval_to_multi_sz(info8, v,
						    "CoreDependencies",
						    &info8->core_driver_dependencies);
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "MinInboxDriverVerDate",
					      &tmp_str);
		if (W_ERROR_IS_OK(result)) {
			result = winreg_printer_date_to_NTTIME(tmp_str,
					&info8->min_inbox_driver_ver_date);
		}
		CHECK_ERROR(result);

		result = winreg_enumval_to_sz(info8, v,
					      "MinInboxDriverVerVersion",
					      &tmp_str);
		if (W_ERROR_IS_OK(result)) {
			result = winreg_printer_ver_to_dword(tmp_str,
					&info8->min_inbox_driver_ver_version);
		}
		CHECK_ERROR(result);
	}

	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_enumval_to_TYPE() failed "
			  "for %s: %s\n", v->value_name,
			  win_errstr(result)));
		goto done;
	}

	*_info8 = talloc_steal(mem_ctx, info8);
	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_del_driver(TALLOC_CTX *mem_ctx,
			 struct dcerpc_binding_handle *winreg_handle,
			 struct spoolss_DriverInfo8 *info8,
			 uint32_t version)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	TALLOC_CTX *tmp_ctx;
	char *key_name;
	WERROR result;

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	/* test that the key exists */
	result = winreg_printer_opendriver(tmp_ctx,
					   winreg_handle,
					   info8->driver_name,
					   info8->architecture,
					   version,
					   access_mask, false,
					   &hive_hnd,
					   &key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		/* key doesn't exist */
		if (W_ERROR_EQUAL(result, WERR_BADFILE)) {
			result = WERR_OK;
			goto done;
		}

		DEBUG(5, ("winreg_del_driver: "
			  "Could not open driver (%s,%s,%u): %s\n",
			  info8->driver_name, info8->architecture,
			  version, win_errstr(result)));
		goto done;
	}


	if (is_valid_policy_hnd(&key_hnd)) {
		dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &result);
	}

	key_name = talloc_asprintf(tmp_ctx,
				   "%s\\Environments\\%s\\Drivers\\Version-%u\\%s",
				   TOP_LEVEL_CONTROL_KEY,
				   info8->architecture, version,
				   info8->driver_name);
	if (key_name == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	result = winreg_printer_delete_subkeys(tmp_ctx,
					       winreg_handle,
					       &hive_hnd,
					       access_mask,
					       key_name);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_del_driver: "
			  "Could not open driver (%s,%s,%u): %s\n",
			  info8->driver_name, info8->architecture,
			  version, win_errstr(result)));
		goto done;
	}

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}

WERROR winreg_get_driver_list(TALLOC_CTX *mem_ctx,
			      struct dcerpc_binding_handle *winreg_handle,
			      const char *architecture,
			      uint32_t version,
			      uint32_t *num_drivers,
			      const char ***drivers_p)
{
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	const char **drivers;
	TALLOC_CTX *tmp_ctx;
	WERROR result;
	NTSTATUS status;

	*num_drivers = 0;
	*drivers_p = NULL;

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	/* use NULL for the driver name so we open the key that is
	 * parent of all drivers for this architecture and version */
	result = winreg_printer_opendriver(tmp_ctx,
					   winreg_handle,
					   NULL,
					   architecture,
					   version,
					   access_mask, false,
					   &hive_hnd,
					   &key_hnd);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(5, ("winreg_get_driver_list: "
			  "Could not open key (%s,%u): %s\n",
			  architecture, version, win_errstr(result)));
		result = WERR_OK;
		goto done;
	}

	status = dcerpc_winreg_enum_keys(tmp_ctx,
					 winreg_handle,
					 &key_hnd,
					 num_drivers,
					 &drivers,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("winreg_get_driver_list: "
			  "Could not enumerate drivers for (%s,%u): %s\n",
			  architecture, version, win_errstr(result)));
		goto done;
	}

	*drivers_p = talloc_steal(mem_ctx, drivers);

	result = WERR_OK;
done:
	if (winreg_handle != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(winreg_handle, tmp_ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(tmp_ctx);
	return result;
}
