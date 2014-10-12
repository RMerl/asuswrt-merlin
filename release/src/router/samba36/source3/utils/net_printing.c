/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Local printing tdb migration interface

   Copyright (C) Guenther Deschner 2010

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
#include "rpc_client/cli_pipe.h"
#include "librpc/gen_ndr/ndr_ntprinting.h"
#include "librpc/gen_ndr/ndr_spoolss.h"
#include "../libcli/security/security.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "../librpc/gen_ndr/ndr_winreg.h"
#include "util_tdb.h"
#include "printing/nt_printing_migrate.h"

#define FORMS_PREFIX "FORMS/"
#define DRIVERS_PREFIX "DRIVERS/"
#define PRINTERS_PREFIX "PRINTERS/"
#define SECDESC_PREFIX "SECDESC/"

#define ARG_ENCODING "encoding="

struct printing_opts {
	const char *encoding;
	const char *tdb;
};

static NTSTATUS printing_parse_args(TALLOC_CTX *mem_ctx,
				    struct printing_opts **popts,
				    int argc, const char **argv)
{
	size_t c;
	struct printing_opts *o;

	if (argc == 0) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	o = talloc_zero(mem_ctx, struct printing_opts);
	if (o == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	for (c = 0; c < argc; c++) {
		if (strnequal(argv[c], ARG_ENCODING, sizeof(ARG_ENCODING) - 1)) {
			o->encoding = talloc_strdup(o,
					argv[c] + sizeof(ARG_ENCODING) - 1);
			if (o->encoding == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
		} else {
			o->tdb = talloc_strdup(o, argv[c]);
			if (o->tdb == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
		}
	}

	*popts = o;
	return NT_STATUS_OK;
}

static void dump_form(TALLOC_CTX *mem_ctx,
		      const char *key_name,
		      unsigned char *data,
		      size_t length)
{
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	char *s;
	struct ntprinting_form r;

	printf("found form: %s\n", key_name);

	blob = data_blob_const(data, length);

	ZERO_STRUCT(r);

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
		   (ndr_pull_flags_fn_t)ndr_pull_ntprinting_form);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		d_fprintf(stderr, _("form pull failed: %s\n"),
			  ndr_errstr(ndr_err));
		return;
	}

	s = NDR_PRINT_STRUCT_STRING(mem_ctx, ntprinting_form, &r);
	if (s) {
		printf("%s\n", s);
	}
}

static void dump_driver(TALLOC_CTX *mem_ctx,
			const char *key_name,
			unsigned char *data,
			size_t length,
			bool do_string_conversion)
{
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	char *s;
	struct ntprinting_driver r;

	printf("found driver: %s\n", key_name);

	blob = data_blob_const(data, length);

	ZERO_STRUCT(r);

	if (do_string_conversion) {
		r.string_flags = LIBNDR_FLAG_STR_ASCII;
	}

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
		   (ndr_pull_flags_fn_t)ndr_pull_ntprinting_driver);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		d_fprintf(stderr, _("driver pull failed: %s\n"),
			  ndr_errstr(ndr_err));
		return;
	}

	s = NDR_PRINT_STRUCT_STRING(mem_ctx, ntprinting_driver, &r);
	if (s) {
		printf("%s\n", s);
	}
}

static void dump_printer(TALLOC_CTX *mem_ctx,
			 const char *key_name,
			 unsigned char *data,
			 size_t length,
			 bool do_string_conversion)
{
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	char *s;
	struct ntprinting_printer r;

	printf("found printer: %s\n", key_name);

	blob = data_blob_const(data, length);

	ZERO_STRUCT(r);

	if (do_string_conversion) {
		r.info.string_flags = LIBNDR_FLAG_STR_ASCII;
	}

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
		   (ndr_pull_flags_fn_t)ndr_pull_ntprinting_printer);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		d_fprintf(stderr, _("printer pull failed: %s\n"),
			  ndr_errstr(ndr_err));
		return;
	}

	s = NDR_PRINT_STRUCT_STRING(mem_ctx, ntprinting_printer, &r);
	if (s) {
		printf("%s\n", s);
	}
}

static void dump_sd(TALLOC_CTX *mem_ctx,
		    const char *key_name,
		    unsigned char *data,
		    size_t length)
{
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	char *s;
	struct sec_desc_buf r;

	printf("found security descriptor: %s\n", key_name);

	blob = data_blob_const(data, length);

	ZERO_STRUCT(r);

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
		   (ndr_pull_flags_fn_t)ndr_pull_sec_desc_buf);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		d_fprintf(stderr, _("security descriptor pull failed: %s\n"),
			  ndr_errstr(ndr_err));
		return;
	}

	s = NDR_PRINT_STRUCT_STRING(mem_ctx, sec_desc_buf, &r);
	if (s) {
		printf("%s\n", s);
	}
}


static int net_printing_dump(struct net_context *c, int argc,
			     const char **argv)
{
	int ret = -1;
	TALLOC_CTX *ctx = talloc_stackframe();
	TDB_CONTEXT *tdb;
	TDB_DATA kbuf, newkey, dbuf;
	struct printing_opts *o;
	const char *save_dos_charset = lp_dos_charset();
	bool do_string_conversion = false;
	NTSTATUS status;

	if (argc < 1 || c->display_usage) {
		d_printf(  "%s\n"
			   "net printing dump [options] <file.tdb>\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Dump formated printer information of the tdb."));
		d_printf(_("Valid options:\n"));
		d_printf(_("    encoding=<CP>   Set the Code Page of the tdb file.\n"
			   "                    See iconv -l for the list of CP values\n"
			   "                    (CP1252 is Western latin1, CP1251 is Cyrillic).\n"));
		goto done;
	}

	status = printing_parse_args(ctx, &o, argc, argv);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("failed to parse arguments\n"));
		goto done;
	}

	tdb = tdb_open_log(o->tdb, 0, TDB_DEFAULT, O_RDONLY, 0600);
	if (!tdb) {
		d_fprintf(stderr, _("failed to open tdb file: %s\n"), o->tdb);
		goto done;
	}

	if (o->encoding != NULL) {
		lp_set_cmdline("dos charset", o->encoding);
		d_fprintf(stderr, _("do string conversion from %s to %s\n"),
				    lp_dos_charset(), lp_unix_charset());
		do_string_conversion = true;
	}

	for (kbuf = tdb_firstkey(tdb);
	     kbuf.dptr;
	     newkey = tdb_nextkey(tdb, kbuf), free(kbuf.dptr), kbuf=newkey)
	{
		dbuf = tdb_fetch(tdb, kbuf);
		if (!dbuf.dptr) {
			continue;
		}

		if (strncmp((const char *)kbuf.dptr, FORMS_PREFIX, strlen(FORMS_PREFIX)) == 0) {
			dump_form(ctx, (const char *)kbuf.dptr+strlen(FORMS_PREFIX), dbuf.dptr, dbuf.dsize);
			SAFE_FREE(dbuf.dptr);
			continue;
		}

		if (strncmp((const char *)kbuf.dptr, DRIVERS_PREFIX, strlen(DRIVERS_PREFIX)) == 0) {
			dump_driver(ctx,
				    (const char *)kbuf.dptr+strlen(DRIVERS_PREFIX),
				    dbuf.dptr,
				    dbuf.dsize,
				    do_string_conversion);
			SAFE_FREE(dbuf.dptr);
			continue;
		}

		if (strncmp((const char *)kbuf.dptr, PRINTERS_PREFIX, strlen(PRINTERS_PREFIX)) == 0) {
			dump_printer(ctx,
				     (const char *)kbuf.dptr+strlen(PRINTERS_PREFIX),
				     dbuf.dptr,
				     dbuf.dsize,
				     do_string_conversion);
			SAFE_FREE(dbuf.dptr);
			continue;
		}

		if (strncmp((const char *)kbuf.dptr, SECDESC_PREFIX, strlen(SECDESC_PREFIX)) == 0) {
			dump_sd(ctx, (const char *)kbuf.dptr+strlen(SECDESC_PREFIX), dbuf.dptr, dbuf.dsize);
			SAFE_FREE(dbuf.dptr);
			continue;
		}

	}

	ret = 0;

 done:
	lp_set_cmdline("dos charset", save_dos_charset);
	talloc_free(ctx);
	return ret;
}

static NTSTATUS printing_migrate_internal(struct net_context *c,
					  const struct dom_sid *domain_sid,
					  const char *domain_name,
					  struct cli_state *cli,
					  struct rpc_pipe_client *winreg_pipe,
					  TALLOC_CTX *mem_ctx,
					  int argc,
					  const char **argv)
{
	struct printing_opts *o;
	TALLOC_CTX *tmp_ctx;
	TDB_CONTEXT *tdb;
	TDB_DATA kbuf, newkey, dbuf;
	NTSTATUS status;
	const char *save_dos_charset = lp_dos_charset();
	bool do_string_conversion = false;

	tmp_ctx = talloc_new(mem_ctx);
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = printing_parse_args(tmp_ctx, &o, argc, argv);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, _("failed to parse arguments\n"));
		goto done;
	}

	tdb = tdb_open_log(o->tdb, 0, TDB_DEFAULT, O_RDONLY, 0600);
	if (tdb == NULL) {
		d_fprintf(stderr, _("failed to open tdb file: %s\n"), o->tdb);
		status = NT_STATUS_NO_SUCH_FILE;
		goto done;
	}

	if (o->encoding != NULL) {
		lp_set_cmdline("dos charset", o->encoding);
		d_fprintf(stderr, _("do string conversion from %s to %s\n"),
				    lp_dos_charset(), lp_unix_charset());
		do_string_conversion = true;
	}

	for (kbuf = tdb_firstkey(tdb);
	     kbuf.dptr;
	     newkey = tdb_nextkey(tdb, kbuf), free(kbuf.dptr), kbuf = newkey)
	{
		dbuf = tdb_fetch(tdb, kbuf);
		if (!dbuf.dptr) {
			continue;
		}

		if (strncmp((const char *) kbuf.dptr, FORMS_PREFIX, strlen(FORMS_PREFIX)) == 0) {
			printing_tdb_migrate_form(tmp_ctx,
				     winreg_pipe,
				     (const char *) kbuf.dptr + strlen(FORMS_PREFIX),
				     dbuf.dptr,
				     dbuf.dsize);
			SAFE_FREE(dbuf.dptr);
			continue;
		}

		if (strncmp((const char *) kbuf.dptr, DRIVERS_PREFIX, strlen(DRIVERS_PREFIX)) == 0) {
			printing_tdb_migrate_driver(tmp_ctx,
				       winreg_pipe,
				       (const char *) kbuf.dptr + strlen(DRIVERS_PREFIX),
				       dbuf.dptr,
				       dbuf.dsize,
				       do_string_conversion);
			SAFE_FREE(dbuf.dptr);
			continue;
		}

		if (strncmp((const char *) kbuf.dptr, PRINTERS_PREFIX, strlen(PRINTERS_PREFIX)) == 0) {
			printing_tdb_migrate_printer(tmp_ctx,
					winreg_pipe,
					(const char *) kbuf.dptr + strlen(PRINTERS_PREFIX),
					dbuf.dptr,
					dbuf.dsize,
					do_string_conversion);
			SAFE_FREE(dbuf.dptr);
			continue;
		}
		SAFE_FREE(dbuf.dptr);
	}

	for (kbuf = tdb_firstkey(tdb);
	     kbuf.dptr;
	     newkey = tdb_nextkey(tdb, kbuf), free(kbuf.dptr), kbuf = newkey)
	{
		dbuf = tdb_fetch(tdb, kbuf);
		if (!dbuf.dptr) {
			continue;
		}

		if (strncmp((const char *) kbuf.dptr, SECDESC_PREFIX, strlen(SECDESC_PREFIX)) == 0) {
			printing_tdb_migrate_secdesc(tmp_ctx,
					winreg_pipe,
					(const char *) kbuf.dptr + strlen(SECDESC_PREFIX),
					dbuf.dptr,
					dbuf.dsize);
			SAFE_FREE(dbuf.dptr);
			continue;
		}
		SAFE_FREE(dbuf.dptr);

	}

	status = NT_STATUS_OK;

 done:
	lp_set_cmdline("dos charset", save_dos_charset);
	talloc_free(tmp_ctx);
	return status;
}

static int net_printing_migrate(struct net_context *c,
				int argc,
				const char **argv)
{
	if (argc < 1 || c->display_usage) {
		d_printf(  "%s\n"
			   "net printing migrate [options] <file.tdb>\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Migrate tdb printing files to new storage"));
		d_printf(_("Valid options:\n"));
		d_printf(_("    encoding=<CP>   Set the Code Page of the tdb file.\n"
			   "                    See iconv -l for the list of CP values\n"
			   "                    (CP1252 is Western latin1, CP1251 is Cyrillic).\n"));
		return 0;
	}

	return run_rpc_command(c,
			       NULL,
			       &ndr_table_winreg.syntax_id,
			       0,
			       printing_migrate_internal,
			       argc,
			       argv);
}
/**
 * 'net printing' entrypoint.
 * @param argc  Standard main() style argc.
 * @param argv  Standard main() style argv. Initial components are already
 *              stripped.
 **/

int net_printing(struct net_context *c, int argc, const char **argv)
{
	int ret = -1;

	struct functable func[] = {
		{
			"dump",
			net_printing_dump,
			NET_TRANSPORT_LOCAL,
			N_("Dump printer databases"),
			N_("net printing dump\n"
			   "    Dump tdb printing file")
		},

		{
			"migrate",
			net_printing_migrate,
			NET_TRANSPORT_LOCAL | NET_TRANSPORT_RPC,
			N_("Migrate printer databases"),
			N_("net printing migrate\n"
			   "    Migrate tdb printing files to new storage")
		},

	{ NULL, NULL, 0, NULL, NULL }
	};

	ret = net_run_function(c, argc, argv, "net printing", func);

	return ret;
}
