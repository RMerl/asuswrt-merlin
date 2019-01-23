/*
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Gerald Carter                2001-2005
   Copyright (C) Tim Potter                        2000
   Copyright (C) Andrew Tridgell              1992-1999
   Copyright (C) Luke Kenneth Casson Leighton 1996-1999
   Copyright (C) Guenther Deschner                 2009

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
#include "rpcclient.h"
#include "../librpc/gen_ndr/ndr_spoolss_c.h"
#include "rpc_client/cli_spoolss.h"
#include "rpc_client/init_spoolss.h"
#include "registry/reg_objects.h"
#include "nt_printing.h"
#include "../libcli/security/display_sec.h"
#include "../libcli/security/security_descriptor.h"
#include "../libcli/registry/util_reg.h"
#include "libsmb/libsmb.h"

#define RPCCLIENT_PRINTERNAME(_printername, _cli, _arg) \
{ \
	_printername = talloc_asprintf_strupper_m(mem_ctx, "%s\\%s", \
		_cli->srv_name_slash, _arg); \
	W_ERROR_HAVE_NO_MEMORY(_printername); \
}

/* The version int is used by getdrivers.  Note that
   all architecture strings that support mutliple
   versions must be grouped together since enumdrivers
   uses this property to prevent issuing multiple
   enumdriver calls for the same arch */


static const struct print_architecture_table_node archi_table[]= {

	{"Windows 4.0",          "WIN40",	0 },
	{"Windows NT x86",       "W32X86",	2 },
	{"Windows NT x86",       "W32X86",	3 },
	{"Windows NT R4000",     "W32MIPS",	2 },
	{"Windows NT Alpha_AXP", "W32ALPHA",	2 },
	{"Windows NT PowerPC",   "W32PPC",	2 },
	{"Windows IA64",         "IA64",        3 },
	{"Windows x64",          "x64",         3 },
	{NULL,                   "",		-1 }
};

/**
 * @file
 *
 * rpcclient module for SPOOLSS rpc pipe.
 *
 * This generally just parses and checks command lines, and then calls
 * a cli_spoolss function.
 **/

/****************************************************************************
 function to do the mapping between the long architecture name and
 the short one.
****************************************************************************/

static const char *cmd_spoolss_get_short_archi(const char *long_archi)
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
****************************************************************************/

static WERROR cmd_spoolss_open_printer_ex(struct rpc_pipe_client *cli,
                                            TALLOC_CTX *mem_ctx,
                                            int argc, const char **argv)
{
	WERROR 	        werror;
	struct policy_handle	hnd;
	uint32_t access_mask = PRINTER_ALL_ACCESS;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2) {
		printf("Usage: %s <printername> [access_mask]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 3) {
		sscanf(argv[2], "%x", &access_mask);
	}

	/* Open the printer handle */

	werror = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       argv[1],
					       access_mask,
					       &hnd);
	if (W_ERROR_IS_OK(werror)) {
		printf("Printer %s opened successfully\n", argv[1]);
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &werror);

		if (!W_ERROR_IS_OK(werror)) {
			printf("Error closing printer handle! (%s)\n",
				get_dos_error_msg(werror));
		}
	}

	return werror;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_open_printer(struct rpc_pipe_client *cli,
                                       TALLOC_CTX *mem_ctx,
                                       int argc, const char **argv)
{
	WERROR 	        werror;
	struct policy_handle	hnd;
	uint32_t access_mask = PRINTER_ALL_ACCESS;
	NTSTATUS status;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	ZERO_STRUCT(devmode_ctr);

	if (argc < 2) {
		printf("Usage: %s <printername> [access_mask]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 3) {
		sscanf(argv[2], "%x", &access_mask);
	}

	/* Open the printer handle */

	status = dcerpc_spoolss_OpenPrinter(b, mem_ctx,
					    argv[1],
					    NULL,
					    devmode_ctr,
					    access_mask,
					    &hnd,
					    &werror);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (W_ERROR_IS_OK(werror)) {
		printf("Printer %s opened successfully\n", argv[1]);
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &werror);

		if (!W_ERROR_IS_OK(werror)) {
			printf("Error closing printer handle! (%s)\n",
				get_dos_error_msg(werror));
		}
	}

	return werror;
}

/****************************************************************************
****************************************************************************/

static void display_print_info0(struct spoolss_PrinterInfo0 *r)
{
	if (!r)
		return;

	printf("\tprintername:[%s]\n", r->printername);
	printf("\tservername:[%s]\n", r->servername);
	printf("\tcjobs:[0x%x]\n", r->cjobs);
	printf("\ttotal_jobs:[0x%x]\n", r->total_jobs);
	printf("\ttotal_bytes:[0x%x]\n", r->total_bytes);
	printf("\t:date: [%d]-[%d]-[%d] (%d)\n", r->time.year, r->time.month,
	       r->time.day, r->time.day_of_week);
	printf("\t:time: [%d]-[%d]-[%d]-[%d]\n", r->time.hour, r->time.minute,
	       r->time.second, r->time.millisecond);

	printf("\tglobal_counter:[0x%x]\n", r->global_counter);
	printf("\ttotal_pages:[0x%x]\n", r->total_pages);

	printf("\tversion:[0x%x]\n", r->version);
	printf("\tfree_build:[0x%x]\n", r->free_build);
	printf("\tspooling:[0x%x]\n", r->spooling);
	printf("\tmax_spooling:[0x%x]\n", r->max_spooling);
	printf("\tsession_counter:[0x%x]\n", r->session_counter);
	printf("\tnum_error_out_of_paper:[0x%x]\n", r->num_error_out_of_paper);
	printf("\tnum_error_not_ready:[0x%x]\n", r->num_error_not_ready);
	printf("\tjob_error:[0x%x]\n", r->job_error);
	printf("\tnumber_of_processors:[0x%x]\n", r->number_of_processors);
	printf("\tprocessor_type:[0x%x]\n", r->processor_type);
	printf("\thigh_part_total_bytes:[0x%x]\n", r->high_part_total_bytes);
	printf("\tchange_id:[0x%x]\n", r->change_id);
	printf("\tlast_error: %s\n", win_errstr(r->last_error));
	printf("\tstatus:[0x%x]\n", r->status);
	printf("\tenumerate_network_printers:[0x%x]\n", r->enumerate_network_printers);
	printf("\tc_setprinter:[0x%x]\n", r->c_setprinter);
	printf("\tprocessor_architecture:[0x%x]\n", r->processor_architecture);
	printf("\tprocessor_level:[0x%x]\n", r->processor_level);
	printf("\tref_ic:[0x%x]\n", r->ref_ic);
	printf("\treserved2:[0x%x]\n", r->reserved2);
	printf("\treserved3:[0x%x]\n", r->reserved3);

	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_info1(struct spoolss_PrinterInfo1 *r)
{
	printf("\tflags:[0x%x]\n", r->flags);
	printf("\tname:[%s]\n", r->name);
	printf("\tdescription:[%s]\n", r->description);
	printf("\tcomment:[%s]\n", r->comment);

	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_info2(struct spoolss_PrinterInfo2 *r)
{
	printf("\tservername:[%s]\n", r->servername);
	printf("\tprintername:[%s]\n", r->printername);
	printf("\tsharename:[%s]\n", r->sharename);
	printf("\tportname:[%s]\n", r->portname);
	printf("\tdrivername:[%s]\n", r->drivername);
	printf("\tcomment:[%s]\n", r->comment);
	printf("\tlocation:[%s]\n", r->location);
	printf("\tsepfile:[%s]\n", r->sepfile);
	printf("\tprintprocessor:[%s]\n", r->printprocessor);
	printf("\tdatatype:[%s]\n", r->datatype);
	printf("\tparameters:[%s]\n", r->parameters);
	printf("\tattributes:[0x%x]\n", r->attributes);
	printf("\tpriority:[0x%x]\n", r->priority);
	printf("\tdefaultpriority:[0x%x]\n", r->defaultpriority);
	printf("\tstarttime:[0x%x]\n", r->starttime);
	printf("\tuntiltime:[0x%x]\n", r->untiltime);
	printf("\tstatus:[0x%x]\n", r->status);
	printf("\tcjobs:[0x%x]\n", r->cjobs);
	printf("\taverageppm:[0x%x]\n", r->averageppm);

	if (r->secdesc)
		display_sec_desc(r->secdesc);

	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_info3(struct spoolss_PrinterInfo3 *r)
{
	display_sec_desc(r->secdesc);

	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_info4(struct spoolss_PrinterInfo4 *r)
{
	printf("\tservername:[%s]\n", r->servername);
	printf("\tprintername:[%s]\n", r->printername);
	printf("\tattributes:[0x%x]\n", r->attributes);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_info5(struct spoolss_PrinterInfo5 *r)
{
	printf("\tprintername:[%s]\n", r->printername);
	printf("\tportname:[%s]\n", r->portname);
	printf("\tattributes:[0x%x]\n", r->attributes);
	printf("\tdevice_not_selected_timeout:[0x%x]\n", r->device_not_selected_timeout);
	printf("\ttransmission_retry_timeout:[0x%x]\n", r->transmission_retry_timeout);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_info6(struct spoolss_PrinterInfo6 *r)
{
	printf("\tstatus:[0x%x]\n", r->status);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_info7(struct spoolss_PrinterInfo7 *r)
{
	printf("\tguid:[%s]\n", r->guid);
	printf("\taction:[0x%x]\n", r->action);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_enum_printers(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					int argc, const char **argv)
{
	WERROR                  result;
	uint32_t		level = 1;
	union spoolss_PrinterInfo *info;
	uint32_t		i, count;
	const char *name;
	uint32_t flags = PRINTER_ENUM_LOCAL;

	if (argc > 4) {
		printf("Usage: %s [level] [name] [flags]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		level = atoi(argv[1]);
	}

	if (argc >= 3) {
		name = argv[2];
	} else {
		name = cli->srv_name_slash;
	}

	if (argc == 4) {
		flags = atoi(argv[3]);
	}

	result = rpccli_spoolss_enumprinters(cli, mem_ctx,
					     flags,
					     name,
					     level,
					     0,
					     &count,
					     &info);
	if (W_ERROR_IS_OK(result)) {

		if (!count) {
			printf ("No printers returned.\n");
			goto done;
		}

		for (i = 0; i < count; i++) {
			switch (level) {
			case 0:
				display_print_info0(&info[i].info0);
				break;
			case 1:
				display_print_info1(&info[i].info1);
				break;
			case 2:
				display_print_info2(&info[i].info2);
				break;
			case 3:
				display_print_info3(&info[i].info3);
				break;
			case 4:
				display_print_info4(&info[i].info4);
				break;
			case 5:
				display_print_info5(&info[i].info5);
				break;
			case 6:
				display_print_info6(&info[i].info6);
				break;
			default:
				printf("unknown info level %d\n", level);
				goto done;
			}
		}
	}
 done:

	return result;
}

/****************************************************************************
****************************************************************************/

static void display_port_info_1(struct spoolss_PortInfo1 *r)
{
	printf("\tPort Name:\t[%s]\n", r->port_name);
}

/****************************************************************************
****************************************************************************/

static void display_port_info_2(struct spoolss_PortInfo2 *r)
{
	printf("\tPort Name:\t[%s]\n", r->port_name);
	printf("\tMonitor Name:\t[%s]\n", r->monitor_name);
	printf("\tDescription:\t[%s]\n", r->description);
	printf("\tPort Type:\t" );
	if (r->port_type) {
		int comma = 0; /* hack */
		printf( "[" );
		if (r->port_type & SPOOLSS_PORT_TYPE_READ) {
			printf( "Read" );
			comma = 1;
		}
		if (r->port_type & SPOOLSS_PORT_TYPE_WRITE) {
			printf( "%sWrite", comma ? ", " : "" );
			comma = 1;
		}
		/* These two have slightly different interpretations
		 on 95/98/ME but I'm disregarding that for now */
		if (r->port_type & SPOOLSS_PORT_TYPE_REDIRECTED) {
			printf( "%sRedirected", comma ? ", " : "" );
			comma = 1;
		}
		if (r->port_type & SPOOLSS_PORT_TYPE_NET_ATTACHED) {
			printf( "%sNet-Attached", comma ? ", " : "" );
		}
		printf( "]\n" );
	} else {
		printf( "[Unset]\n" );
	}
	printf("\tReserved:\t[%d]\n", r->reserved);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_enum_ports(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx, int argc,
				       const char **argv)
{
	WERROR         		result;
	uint32_t		level = 1;
	uint32_t		count;
	union spoolss_PortInfo *info;

	if (argc > 2) {
		printf("Usage: %s [level]\n", argv[0]);
		return WERR_OK;
	}

	if (argc == 2) {
		level = atoi(argv[1]);
	}

	/* Enumerate ports */

	result = rpccli_spoolss_enumports(cli, mem_ctx,
					  cli->srv_name_slash,
					  level,
					  0,
					  &count,
					  &info);
	if (W_ERROR_IS_OK(result)) {
		int i;

		for (i = 0; i < count; i++) {
			switch (level) {
			case 1:
				display_port_info_1(&info[i].info1);
				break;
			case 2:
				display_port_info_2(&info[i].info2);
				break;
			default:
				printf("unknown info level %d\n", level);
				break;
			}
		}
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_setprinter(struct rpc_pipe_client *cli,
                                       TALLOC_CTX *mem_ctx,
                                       int argc, const char **argv)
{
	struct policy_handle pol;
	WERROR		result;
	NTSTATUS	status;
	uint32_t 	info_level = 2;
	union spoolss_PrinterInfo info;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_SetPrinterInfo2 info2;
	const char	*printername, *comment = NULL;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc == 1 || argc > 3) {
		printf("Usage: %s printername comment\n", argv[0]);

		return WERR_OK;
	}

	/* Open a printer handle */
	if (argc == 3) {
		comment = argv[2];
	}

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	/* get a printer handle */
	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       PRINTER_ALL_ACCESS,
					       &pol);
	if (!W_ERROR_IS_OK(result))
		goto done;

	/* Get printer info */
	result = rpccli_spoolss_getprinter(cli, mem_ctx,
					   &pol,
					   info_level,
					   0,
					   &info);
        if (!W_ERROR_IS_OK(result))
                goto done;


	/* Modify the comment. */
	spoolss_printerinfo2_to_setprinterinfo2(&info.info2, &info2);
	info2.comment = comment;

	info_ctr.level = 2;
	info_ctr.info.info2 = &info2;

	status = dcerpc_spoolss_SetPrinter(b, mem_ctx,
					   &pol,
					   &info_ctr,
					   &devmode_ctr,
					   &secdesc_ctr,
					   0, /* command */
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (W_ERROR_IS_OK(result))
		printf("Success in setting comment.\n");

 done:
	if (is_valid_policy_hnd(&pol)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &pol, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_setprintername(struct rpc_pipe_client *cli,
                                       TALLOC_CTX *mem_ctx,
                                       int argc, const char **argv)
{
	struct policy_handle pol;
	WERROR		result;
	NTSTATUS	status;
	uint32_t 	info_level = 2;
	union spoolss_PrinterInfo info;
	const char 	*printername,
			*new_printername = NULL;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_SetPrinterInfo2 info2;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	if (argc == 1 || argc > 3) {
		printf("Usage: %s printername new_printername\n", argv[0]);

		return WERR_OK;
	}

	/* Open a printer handle */
	if (argc == 3) {
		new_printername = argv[2];
	}

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	/* get a printer handle */
	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       PRINTER_ALL_ACCESS,
					       &pol);
	if (!W_ERROR_IS_OK(result))
		goto done;

	/* Get printer info */
	result = rpccli_spoolss_getprinter(cli, mem_ctx,
					   &pol,
					   info_level,
					   0,
					   &info);
        if (!W_ERROR_IS_OK(result))
                goto done;

	/* Modify the printername. */
	spoolss_printerinfo2_to_setprinterinfo2(&info.info2, &info2);
	info2.printername = new_printername;

	info_ctr.level = 2;
	info_ctr.info.info2 = &info2;

	status = dcerpc_spoolss_SetPrinter(b, mem_ctx,
					   &pol,
					   &info_ctr,
					   &devmode_ctr,
					   &secdesc_ctr,
					   0, /* command */
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (W_ERROR_IS_OK(result))
		printf("Success in setting printername.\n");

 done:
	if (is_valid_policy_hnd(&pol)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &pol, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_getprinter(struct rpc_pipe_client *cli,
                                       TALLOC_CTX *mem_ctx,
                                       int argc, const char **argv)
{
	struct policy_handle pol;
	WERROR          result;
	uint32_t 	level = 1;
	const char	*printername;
	union spoolss_PrinterInfo info;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc == 1 || argc > 3) {
		printf("Usage: %s <printername> [level]\n", argv[0]);
		return WERR_OK;
	}

	/* Open a printer handle */
	if (argc == 3) {
		level = atoi(argv[2]);
	}

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	/* get a printer handle */

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &pol);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Get printer info */

	result = rpccli_spoolss_getprinter(cli, mem_ctx,
					   &pol,
					   level,
					   0,
					   &info);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Display printer info */
	switch (level) {
	case 0:
		display_print_info0(&info.info0);
		break;
	case 1:
		display_print_info1(&info.info1);
		break;
	case 2:
		display_print_info2(&info.info2);
		break;
	case 3:
		display_print_info3(&info.info3);
		break;
	case 4:
		display_print_info4(&info.info4);
		break;
	case 5:
		display_print_info5(&info.info5);
		break;
	case 6:
		display_print_info6(&info.info6);
		break;
	case 7:
		display_print_info7(&info.info7);
		break;
	default:
		printf("unknown info level %d\n", level);
		break;
	}
 done:
	if (is_valid_policy_hnd(&pol)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &pol, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static void display_reg_value(struct regval_blob *value)
{
	const char *text = NULL;
	DATA_BLOB blob;

	switch(regval_type(value)) {
	case REG_DWORD:
		printf("%s: REG_DWORD: 0x%08x\n", regval_name(value),
		       *((uint32_t *) regval_data_p(value)));
		break;
	case REG_SZ:
		blob = data_blob_const(regval_data_p(value), regval_size(value));
		pull_reg_sz(talloc_tos(), &blob, &text);
		printf("%s: REG_SZ: %s\n", regval_name(value), text ? text : "");
		break;
	case REG_BINARY: {
		char *hex = hex_encode_talloc(NULL, regval_data_p(value), regval_size(value));
		size_t i, len;
		printf("%s: REG_BINARY:", regval_name(value));
		len = strlen(hex);
		for (i=0; i<len; i++) {
			if (hex[i] == '\0') {
				break;
			}
			if (i%40 == 0) {
				putchar('\n');
			}
			putchar(hex[i]);
		}
		TALLOC_FREE(hex);
		putchar('\n');
		break;
	}
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
		printf("%s: unknown type %d\n", regval_name(value), regval_type(value));
	}

}

/****************************************************************************
****************************************************************************/

static void display_printer_data(const char *v,
				 enum winreg_Type type,
				 uint8_t *data,
				 uint32_t length)
{
	int i;
	union spoolss_PrinterData r;
	DATA_BLOB blob = data_blob_const(data, length);
	WERROR result;

	result = pull_spoolss_PrinterData(talloc_tos(), &blob, &r, type);
	if (!W_ERROR_IS_OK(result)) {
		return;
	}

	switch (type) {
	case REG_DWORD:
		printf("%s: REG_DWORD: 0x%08x\n", v, r.value);
		break;
	case REG_SZ:
		printf("%s: REG_SZ: %s\n", v, r.string);
		break;
	case REG_BINARY: {
		char *hex = hex_encode_talloc(NULL,
			r.binary.data, r.binary.length);
		size_t len;
		printf("%s: REG_BINARY:", v);
		len = strlen(hex);
		for (i=0; i<len; i++) {
			if (hex[i] == '\0') {
				break;
			}
			if (i%40 == 0) {
				putchar('\n');
			}
			putchar(hex[i]);
		}
		TALLOC_FREE(hex);
		putchar('\n');
		break;
	}
	case REG_MULTI_SZ:
		printf("%s: REG_MULTI_SZ: ", v);
		for (i=0; r.string_array[i] != NULL; i++) {
			printf("%s ", r.string_array[i]);
		}
		printf("\n");
		break;
	default:
		printf("%s: unknown type 0x%02x:\n", v, type);
		break;
	}
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_getprinterdata(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   int argc, const char **argv)
{
	struct policy_handle pol;
	WERROR          result;
	fstring 	printername;
	const char *valuename;
	enum winreg_Type type;
	uint8_t *data;
	uint32_t needed;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 3) {
		printf("Usage: %s <printername> <valuename>\n", argv[0]);
		printf("<printername> of . queries print server\n");
		return WERR_OK;
	}
	valuename = argv[2];

	/* Open a printer handle */

	if (strncmp(argv[1], ".", sizeof(".")) == 0)
		fstrcpy(printername, cli->srv_name_slash);
	else
		slprintf(printername, sizeof(printername)-1, "%s\\%s",
			  cli->srv_name_slash, argv[1]);

	/* get a printer handle */

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &pol);
	if (!W_ERROR_IS_OK(result))
		goto done;

	/* Get printer info */

	result = rpccli_spoolss_getprinterdata(cli, mem_ctx,
					       &pol,
					       valuename,
					       0,
					       &type,
					       &needed,
					       &data);
	if (!W_ERROR_IS_OK(result))
		goto done;

	/* Display printer data */

	display_printer_data(valuename, type, data, needed);

 done:
	if (is_valid_policy_hnd(&pol)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &pol, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_getprinterdataex(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     int argc, const char **argv)
{
	struct policy_handle pol;
	WERROR          result;
	NTSTATUS	status;
	fstring 	printername;
	const char *valuename, *keyname;

	enum winreg_Type type;
	uint8_t *data = NULL;
	uint32_t offered = 0;
	uint32_t needed;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 4) {
		printf("Usage: %s <printername> <keyname> <valuename>\n",
		       argv[0]);
		printf("<printername> of . queries print server\n");
		return WERR_OK;
	}
	valuename = argv[3];
	keyname = argv[2];

	/* Open a printer handle */

	if (strncmp(argv[1], ".", sizeof(".")) == 0)
		fstrcpy(printername, cli->srv_name_slash);
	else
		slprintf(printername, sizeof(printername)-1, "%s\\%s",
			  cli->srv_name_slash, argv[1]);

	/* get a printer handle */

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &pol);
	if (!W_ERROR_IS_OK(result))
		goto done;

	/* Get printer info */

	data = talloc_zero_array(mem_ctx, uint8_t, offered);
	if (!data) {
		goto done;
	}

	status = dcerpc_spoolss_GetPrinterDataEx(b, mem_ctx,
						 &pol,
						 keyname,
						 valuename,
						 &type,
						 data,
						 offered,
						 &needed,
						 &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (W_ERROR_EQUAL(result, WERR_MORE_DATA)) {
		offered = needed;
		data = talloc_zero_array(mem_ctx, uint8_t, offered);
		if (!data) {
			goto done;
		}
		status = dcerpc_spoolss_GetPrinterDataEx(b, mem_ctx,
							 &pol,
							 keyname,
							 valuename,
							 &type,
							 data,
							 offered,
							 &needed,
							 &result);
	}

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result))
		goto done;

	/* Display printer data */

	display_printer_data(valuename, type, data, needed);


 done:
	if (is_valid_policy_hnd(&pol)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &pol, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static void display_print_driver1(struct spoolss_DriverInfo1 *r)
{
	if (!r) {
		return;
	}

	printf("Printer Driver Info 1:\n");
	printf("\tDriver Name: [%s]\n", r->driver_name);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_driver2(struct spoolss_DriverInfo2 *r)
{
	if (!r) {
		return;
	}

	printf("Printer Driver Info 2:\n");
	printf("\tVersion: [%x]\n", r->version);
	printf("\tDriver Name: [%s]\n", r->driver_name);
	printf("\tArchitecture: [%s]\n", r->architecture);
	printf("\tDriver Path: [%s]\n", r->driver_path);
	printf("\tDatafile: [%s]\n", r->data_file);
	printf("\tConfigfile: [%s]\n", r->config_file);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_driver3(struct spoolss_DriverInfo3 *r)
{
	int i;

	if (!r) {
		return;
	}

	printf("Printer Driver Info 3:\n");
	printf("\tVersion: [%x]\n", r->version);
	printf("\tDriver Name: [%s]\n", r->driver_name);
	printf("\tArchitecture: [%s]\n", r->architecture);
	printf("\tDriver Path: [%s]\n", r->driver_path);
	printf("\tDatafile: [%s]\n", r->data_file);
	printf("\tConfigfile: [%s]\n", r->config_file);
	printf("\tHelpfile: [%s]\n", r->help_file);

	for (i=0; r->dependent_files && r->dependent_files[i] != NULL; i++) {
		printf("\tDependentfiles: [%s]\n", r->dependent_files[i]);
	}

	printf("\tMonitorname: [%s]\n", r->monitor_name);
	printf("\tDefaultdatatype: [%s]\n", r->default_datatype);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_driver4(struct spoolss_DriverInfo4 *r)
{
	int i;

	if (!r) {
		return;
	}

	printf("Printer Driver Info 4:\n");
	printf("\tVersion: [%x]\n", r->version);
	printf("\tDriver Name: [%s]\n", r->driver_name);
	printf("\tArchitecture: [%s]\n", r->architecture);
	printf("\tDriver Path: [%s]\n", r->driver_path);
	printf("\tDatafile: [%s]\n", r->data_file);
	printf("\tConfigfile: [%s]\n", r->config_file);
	printf("\tHelpfile: [%s]\n", r->help_file);

	for (i=0; r->dependent_files && r->dependent_files[i] != NULL; i++) {
		printf("\tDependentfiles: [%s]\n", r->dependent_files[i]);
	}

	printf("\tMonitorname: [%s]\n", r->monitor_name);
	printf("\tDefaultdatatype: [%s]\n", r->default_datatype);

	for (i=0; r->previous_names && r->previous_names[i] != NULL; i++) {
		printf("\tPrevious Names: [%s]\n", r->previous_names[i]);
	}
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_driver5(struct spoolss_DriverInfo5 *r)
{
	if (!r) {
		return;
	}

	printf("Printer Driver Info 5:\n");
	printf("\tVersion: [%x]\n", r->version);
	printf("\tDriver Name: [%s]\n", r->driver_name);
	printf("\tArchitecture: [%s]\n", r->architecture);
	printf("\tDriver Path: [%s]\n", r->driver_path);
	printf("\tDatafile: [%s]\n", r->data_file);
	printf("\tConfigfile: [%s]\n", r->config_file);
	printf("\tDriver Attributes: [0x%x]\n", r->driver_attributes);
	printf("\tConfig Version: [0x%x]\n", r->config_version);
	printf("\tDriver Version: [0x%x]\n", r->driver_version);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_driver6(struct spoolss_DriverInfo6 *r)
{
	int i;

	if (!r) {
		return;
	}

	printf("Printer Driver Info 6:\n");
	printf("\tVersion: [%x]\n", r->version);
	printf("\tDriver Name: [%s]\n", r->driver_name);
	printf("\tArchitecture: [%s]\n", r->architecture);
	printf("\tDriver Path: [%s]\n", r->driver_path);
	printf("\tDatafile: [%s]\n", r->data_file);
	printf("\tConfigfile: [%s]\n", r->config_file);
	printf("\tHelpfile: [%s]\n", r->help_file);

	for (i=0; r->dependent_files && r->dependent_files[i] != NULL; i++) {
		printf("\tDependentfiles: [%s]\n", r->dependent_files[i]);
	}

	printf("\tMonitorname: [%s]\n", r->monitor_name);
	printf("\tDefaultdatatype: [%s]\n", r->default_datatype);

	for (i=0; r->previous_names && r->previous_names[i] != NULL; i++) {
		printf("\tPrevious Names: [%s]\n", r->previous_names[i]);
	}

	printf("\tDriver Date: [%s]\n", nt_time_string(talloc_tos(), r->driver_date));
	printf("\tDriver Version: [0x%016llx]\n", (long long unsigned int)r->driver_version);
	printf("\tManufacturer Name: [%s]\n", r->manufacturer_name);
	printf("\tManufacturer Url: [%s]\n", r->manufacturer_url);
	printf("\tHardware ID: [%s]\n", r->hardware_id);
	printf("\tProvider: [%s]\n", r->provider);

	printf("\n");
}

/****************************************************************************
****************************************************************************/

static void display_print_driver8(struct spoolss_DriverInfo8 *r)
{
	int i;

	if (!r) {
		return;
	}

	printf("Printer Driver Info 8:\n");
	printf("\tVersion: [%x]\n", r->version);
	printf("\tDriver Name: [%s]\n", r->driver_name);
	printf("\tArchitecture: [%s]\n", r->architecture);
	printf("\tDriver Path: [%s]\n", r->driver_path);
	printf("\tDatafile: [%s]\n", r->data_file);
	printf("\tConfigfile: [%s]\n", r->config_file);
	printf("\tHelpfile: [%s]\n", r->help_file);
	printf("\tMonitorname: [%s]\n", r->monitor_name);
	printf("\tDefaultdatatype: [%s]\n", r->default_datatype);

	for (i=0; r->dependent_files && r->dependent_files[i] != NULL; i++) {
		printf("\tDependentfiles: [%s]\n", r->dependent_files[i]);
	}

	for (i=0; r->previous_names && r->previous_names[i] != NULL; i++) {
		printf("\tPrevious Names: [%s]\n", r->previous_names[i]);
	}

	printf("\tDriver Date: [%s]\n", nt_time_string(talloc_tos(), r->driver_date));
	printf("\tDriver Version: [0x%016llx]\n", (long long unsigned int)r->driver_version);
	printf("\tManufacturer Name: [%s]\n", r->manufacturer_name);
	printf("\tManufacturer Url: [%s]\n", r->manufacturer_url);
	printf("\tHardware ID: [%s]\n", r->hardware_id);
	printf("\tProvider: [%s]\n", r->provider);
	printf("\tPrint Processor: [%s]\n", r->print_processor);
	printf("\tVendor Setup: [%s]\n", r->vendor_setup);
	for (i=0; r->color_profiles && r->color_profiles[i] != NULL; i++) {
		printf("\tColor Profiles: [%s]\n", r->color_profiles[i]);
	}
	printf("\tInf Path: [%s]\n", r->inf_path);
	printf("\tPrinter Driver Attributes: [0x%x]\n", r->printer_driver_attributes);
	for (i=0; r->core_driver_dependencies && r->core_driver_dependencies[i] != NULL; i++) {
		printf("\tCore Driver Dependencies: [%s]\n", r->core_driver_dependencies[i]);
	}
	printf("\tMin Driver Inbox Driver Version Date: [%s]\n", nt_time_string(talloc_tos(), r->min_inbox_driver_ver_date));
	printf("\tMin Driver Inbox Driver Version Version: [0x%016llx]\n",
		(long long unsigned int)r->min_inbox_driver_ver_version);

	printf("\n");
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_getdriver(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    int argc, const char **argv)
{
	struct policy_handle pol;
	WERROR          werror;
	uint32_t	level = 3;
	const char	*printername;
	uint32_t	i;
	bool		success = false;
	union spoolss_DriverInfo info;
	uint32_t server_major_version;
	uint32_t server_minor_version;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if ((argc == 1) || (argc > 3)) {
		printf("Usage: %s <printername> [level]\n", argv[0]);
		return WERR_OK;
	}

	/* get the arguments need to open the printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	if (argc == 3) {
		level = atoi(argv[2]);
	}

	/* Open a printer handle */

	werror = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       PRINTER_ACCESS_USE,
					       &pol);
	if (!W_ERROR_IS_OK(werror)) {
		printf("Error opening printer handle for %s!\n", printername);
		return werror;
	}

	/* loop through and print driver info level for each architecture */

	for (i=0; archi_table[i].long_archi!=NULL; i++) {

		werror = rpccli_spoolss_getprinterdriver2(cli, mem_ctx,
							  &pol,
							  archi_table[i].long_archi,
							  level,
							  0, /* offered */
							  archi_table[i].version,
							  2,
							  &info,
							  &server_major_version,
							  &server_minor_version);
		if (!W_ERROR_IS_OK(werror)) {
			continue;
		}

		/* need at least one success */

		success = true;

		printf("\n[%s]\n", archi_table[i].long_archi);

		switch (level) {
		case 1:
			display_print_driver1(&info.info1);
			break;
		case 2:
			display_print_driver2(&info.info2);
			break;
		case 3:
			display_print_driver3(&info.info3);
			break;
		case 4:
			display_print_driver4(&info.info4);
			break;
		case 5:
			display_print_driver5(&info.info5);
			break;
		case 6:
			display_print_driver6(&info.info6);
			break;
		case 8:
			display_print_driver8(&info.info8);
			break;
		default:
			printf("unknown info level %d\n", level);
			break;
		}
	}

	/* Cleanup */

	if (is_valid_policy_hnd(&pol)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &pol, &_result);
	}

	if (success) {
		werror = WERR_OK;
	}

	return werror;
}

/****************************************************************************
****************************************************************************/

static WERROR enum_driver_by_architecture(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  const char *architecture,
					  uint32_t level)
{
	WERROR werror;
	uint32_t count = 0;
	union spoolss_DriverInfo *info = NULL;
	uint32_t j;

	werror = rpccli_spoolss_enumprinterdrivers(cli, mem_ctx,
						   cli->srv_name_slash,
						   architecture,
						   level,
						   0,
						   &count,
						   &info);

	if (W_ERROR_EQUAL(werror, WERR_INVALID_ENVIRONMENT)) {
		printf("Server does not support environment [%s]\n",
			architecture);
		return WERR_OK;
	}

	if (count == 0) {
		return WERR_OK;
	}

	if (!W_ERROR_IS_OK(werror)) {
		printf("Error getting driver for environment [%s] - %s\n",
			architecture, win_errstr(werror));
		return werror;
	}

	printf("\n[%s]\n", architecture);

	switch (level) {
	case 1:
		for (j=0; j < count; j++) {
			display_print_driver1(&info[j].info1);
		}
		break;
	case 2:
		for (j=0; j < count; j++) {
			display_print_driver2(&info[j].info2);
		}
		break;
	case 3:
		for (j=0; j < count; j++) {
			display_print_driver3(&info[j].info3);
		}
		break;
	case 4:
		for (j=0; j < count; j++) {
			display_print_driver4(&info[j].info4);
		}
		break;
	case 5:
		for (j=0; j < count; j++) {
			display_print_driver5(&info[j].info5);
		}
		break;
	case 6:
		for (j=0; j < count; j++) {
			display_print_driver6(&info[j].info6);
		}
		break;
	case 8:
		for (j=0; j < count; j++) {
			display_print_driver8(&info[j].info8);
		}
		break;
	default:
		printf("unknown info level %d\n", level);
		return WERR_UNKNOWN_LEVEL;
	}

	return werror;
}

static WERROR cmd_spoolss_enum_drivers(struct rpc_pipe_client *cli,
                                         TALLOC_CTX *mem_ctx,
                                         int argc, const char **argv)
{
	WERROR werror = WERR_OK;
	uint32_t        level = 1;
	uint32_t	i;
	const char *architecture = NULL;

	if (argc > 3) {
		printf("Usage: enumdrivers [level] [architecture]\n");
		return WERR_OK;
	}

	if (argc >= 2) {
		level = atoi(argv[1]);
	}

	if (argc == 3) {
		architecture = argv[2];
	}

	if (architecture) {
		return enum_driver_by_architecture(cli, mem_ctx,
						   architecture,
						   level);
	}

	/* loop through and print driver info level for each architecture */
	for (i=0; archi_table[i].long_archi!=NULL; i++) {
		/* check to see if we already asked for this architecture string */

		if (i>0 && strequal(archi_table[i].long_archi, archi_table[i-1].long_archi)) {
			continue;
		}

		werror = enum_driver_by_architecture(cli, mem_ctx,
						     archi_table[i].long_archi,
						     level);
		if (!W_ERROR_IS_OK(werror)) {
			break;
		}
	}

	return werror;
}

/****************************************************************************
****************************************************************************/

static void display_printdriverdir_1(struct spoolss_DriverDirectoryInfo1 *r)
{
	printf("\tDirectory Name:[%s]\n", r->directory_name);
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_getdriverdir(struct rpc_pipe_client *cli,
                                         TALLOC_CTX *mem_ctx,
                                         int argc, const char **argv)
{
	WERROR result;
	NTSTATUS status;
	const char *env = SPOOLSS_ARCHITECTURE_NT_X86;
	DATA_BLOB buffer;
	uint32_t offered;
	union spoolss_DriverDirectoryInfo info;
	uint32_t needed;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 2) {
		printf("Usage: %s [environment]\n", argv[0]);
		return WERR_OK;
	}

	/* Get the arguments need to open the printer handle */

	if (argc == 2) {
		env = argv[1];
	}

	/* Get the directory.  Only use Info level 1 */

	status = dcerpc_spoolss_GetPrinterDriverDirectory(b, mem_ctx,
							  cli->srv_name_slash,
							  env,
							  1,
							  NULL, /* buffer */
							  0, /* offered */
							  NULL, /* info */
							  &needed,
							  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (W_ERROR_EQUAL(result, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);

		status = dcerpc_spoolss_GetPrinterDriverDirectory(b, mem_ctx,
								  cli->srv_name_slash,
								  env,
								  1,
								  &buffer,
								  offered,
								  &info,
								  &needed,
								  &result);
		if (!NT_STATUS_IS_OK(status)) {
			return ntstatus_to_werror(status);
		}
	}

	if (W_ERROR_IS_OK(result)) {
		display_printdriverdir_1(&info.info1);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static void set_drv_info_3_env(TALLOC_CTX *mem_ctx,
			       struct spoolss_AddDriverInfo3 *info,
			       const char *arch)
{

	int i;

	for (i=0; archi_table[i].long_archi != NULL; i++)
	{
		if (strcmp(arch, archi_table[i].short_archi) == 0)
		{
			info->version = archi_table[i].version;
			info->architecture = talloc_strdup(mem_ctx, archi_table[i].long_archi);
			break;
		}
	}

	if (archi_table[i].long_archi == NULL)
	{
		DEBUG(0, ("set_drv_info_3_env: Unknown arch [%s]\n", arch));
	}

	return;
}


/**************************************************************************
 wrapper for strtok to get the next parameter from a delimited list.
 Needed to handle the empty parameter string denoted by "NULL"
 *************************************************************************/

static char *get_driver_3_param(TALLOC_CTX *mem_ctx, char *str,
				const char *delim, const char **dest,
				char **saveptr)
{
	char	*ptr;

	/* get the next token */
	ptr = strtok_r(str, delim, saveptr);

	/* a string of 'NULL' is used to represent an empty
	   parameter because two consecutive delimiters
	   will not return an empty string.  See man strtok(3)
	   for details */
	if (ptr && (StrCaseCmp(ptr, "NULL") == 0)) {
		ptr = NULL;
	}

	if (dest != NULL) {
		*dest = talloc_strdup(mem_ctx, ptr);
	}

	return ptr;
}

/********************************************************************************
 fill in the members of a spoolss_AddDriverInfo3 struct using a character
 string in the form of
 	 <Long Printer Name>:<Driver File Name>:<Data File Name>:\
	     <Config File Name>:<Help File Name>:<Language Monitor Name>:\
	     <Default Data Type>:<Comma Separated list of Files>
 *******************************************************************************/

static bool init_drv_info_3_members(TALLOC_CTX *mem_ctx, struct spoolss_AddDriverInfo3 *r,
                                    char *args)
{
	char	*str, *str2;
	int count = 0;
	char *saveptr = NULL;
	struct spoolss_StringArray *deps;
	const char **file_array = NULL;
	int i;

	/* fill in the UNISTR fields */
	str = get_driver_3_param(mem_ctx, args, ":", &r->driver_name, &saveptr);
	str = get_driver_3_param(mem_ctx, NULL, ":", &r->driver_path, &saveptr);
	str = get_driver_3_param(mem_ctx, NULL, ":", &r->data_file, &saveptr);
	str = get_driver_3_param(mem_ctx, NULL, ":", &r->config_file, &saveptr);
	str = get_driver_3_param(mem_ctx, NULL, ":", &r->help_file, &saveptr);
	str = get_driver_3_param(mem_ctx, NULL, ":", &r->monitor_name, &saveptr);
	str = get_driver_3_param(mem_ctx, NULL, ":", &r->default_datatype, &saveptr);

	/* <Comma Separated List of Dependent Files> */
	/* save the beginning of the string */
	str2 = get_driver_3_param(mem_ctx, NULL, ":", NULL, &saveptr);
	str = str2;

	/* begin to strip out each filename */
	str = strtok_r(str, ",", &saveptr);

	/* no dependent files, we are done */
	if (!str) {
		return true;
	}

	deps = talloc_zero(mem_ctx, struct spoolss_StringArray);
	if (!deps) {
		return false;
	}

	while (str != NULL) {
		add_string_to_array(deps, str, &file_array, &count);
		str = strtok_r(NULL, ",", &saveptr);
	}

	deps->string = talloc_zero_array(deps, const char *, count + 1);
	if (!deps->string) {
		return false;
	}

	for (i=0; i < count; i++) {
		deps->string[i] = file_array[i];
	}

	r->dependent_files = deps;

	return true;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_addprinterdriver(struct rpc_pipe_client *cli,
                                             TALLOC_CTX *mem_ctx,
                                             int argc, const char **argv)
{
	WERROR result;
	NTSTATUS status;
	uint32_t                  level = 3;
	struct spoolss_AddDriverInfoCtr info_ctr;
	struct spoolss_AddDriverInfo3 info3;
	const char		*arch;
	char 			*driver_args;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	/* parse the command arguments */
	if (argc != 3 && argc != 4)
	{
		printf ("Usage: %s <Environment> \\\n", argv[0]);
		printf ("\t<Long Printer Name>:<Driver File Name>:<Data File Name>:\\\n");
    		printf ("\t<Config File Name>:<Help File Name>:<Language Monitor Name>:\\\n");
	    	printf ("\t<Default Data Type>:<Comma Separated list of Files> \\\n");
		printf ("\t[version]\n");

            return WERR_OK;
        }

	/* Fill in the spoolss_AddDriverInfo3 struct */
	ZERO_STRUCT(info3);

	arch = cmd_spoolss_get_short_archi(argv[1]);
	if (!arch) {
		printf ("Error Unknown architecture [%s]\n", argv[1]);
		return WERR_INVALID_PARAM;
	}

	set_drv_info_3_env(mem_ctx, &info3, arch);

	driver_args = talloc_strdup( mem_ctx, argv[2] );
	if (!init_drv_info_3_members(mem_ctx, &info3, driver_args ))
	{
		printf ("Error Invalid parameter list - %s.\n", argv[2]);
		return WERR_INVALID_PARAM;
	}

	/* if printer driver version specified, override the default version
	 * used by the architecture.  This allows installation of Windows
	 * 2000 (version 3) printer drivers. */
	if (argc == 4)
	{
		info3.version = atoi(argv[3]);
	}


	info_ctr.level		= level;
	info_ctr.info.info3	= &info3;

	status = dcerpc_spoolss_AddPrinterDriver(b, mem_ctx,
						 cli->srv_name_slash,
						 &info_ctr,
						 &result);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (W_ERROR_IS_OK(result)) {
		printf ("Printer Driver %s successfully installed.\n",
			info3.driver_name);
	}

	return result;
}


/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_addprinterex(struct rpc_pipe_client *cli,
                                         TALLOC_CTX *mem_ctx,
                                         int argc, const char **argv)
{
	WERROR result;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_SetPrinterInfo2 info2;

	/* parse the command arguments */
	if (argc != 5)
	{
		printf ("Usage: %s <name> <shared name> <driver> <port>\n", argv[0]);
		return WERR_OK;
        }

	/* Fill in the DRIVER_INFO_2 struct */
	ZERO_STRUCT(info2);

	info2.printername	= argv[1];
	info2.drivername	= argv[3];
	info2.sharename		= argv[2];
	info2.portname		= argv[4];
	info2.comment		= "Created by rpcclient";
	info2.printprocessor	= "winprint";
	info2.datatype		= "RAW";
	info2.devmode_ptr	= 0;
	info2.secdesc_ptr	= 0;
	info2.attributes 	= PRINTER_ATTRIBUTE_SHARED;
	info2.priority 		= 0;
	info2.defaultpriority	= 0;
	info2.starttime		= 0;
	info2.untiltime		= 0;

	/* These three fields must not be used by AddPrinter()
	   as defined in the MS Platform SDK documentation..
	   --jerry
	info2.status		= 0;
	info2.cjobs		= 0;
	info2.averageppm	= 0;
	*/

	info_ctr.level = 2;
	info_ctr.info.info2 = &info2;

	result = rpccli_spoolss_addprinterex(cli, mem_ctx,
					     &info_ctr);
	if (W_ERROR_IS_OK(result))
		printf ("Printer %s successfully installed.\n", argv[1]);

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_setdriver(struct rpc_pipe_client *cli,
                                      TALLOC_CTX *mem_ctx,
                                      int argc, const char **argv)
{
	struct policy_handle	pol;
	WERROR                  result;
	NTSTATUS		status;
	uint32_t		level = 2;
	const char		*printername;
	union spoolss_PrinterInfo info;
	struct spoolss_SetPrinterInfoCtr info_ctr;
	struct spoolss_SetPrinterInfo2 info2;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct sec_desc_buf secdesc_ctr;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	ZERO_STRUCT(devmode_ctr);
	ZERO_STRUCT(secdesc_ctr);

	/* parse the command arguments */
	if (argc != 3)
	{
		printf ("Usage: %s <printer> <driver>\n", argv[0]);
		return WERR_OK;
        }

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	/* Get a printer handle */

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       PRINTER_ALL_ACCESS,
					       &pol);
	if (!W_ERROR_IS_OK(result))
		goto done;

	/* Get printer info */

	result = rpccli_spoolss_getprinter(cli, mem_ctx,
					   &pol,
					   level,
					   0,
					   &info);
	if (!W_ERROR_IS_OK(result)) {
		printf ("Unable to retrieve printer information!\n");
		goto done;
	}

	/* Set the printer driver */

	spoolss_printerinfo2_to_setprinterinfo2(&info.info2, &info2);
	info2.drivername = argv[2];

	info_ctr.level = 2;
	info_ctr.info.info2 = &info2;

	status = dcerpc_spoolss_SetPrinter(b, mem_ctx,
					   &pol,
					   &info_ctr,
					   &devmode_ctr,
					   &secdesc_ctr,
					   0, /* command */
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		printf("SetPrinter call failed!\n");
		goto done;
	}

	printf("Successfully set %s to driver %s.\n", argv[1], argv[2]);

done:
	/* Cleanup */

	if (is_valid_policy_hnd(&pol)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &pol, &_result);
	}

	return result;
}


/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_deletedriverex(struct rpc_pipe_client *cli,
                                         TALLOC_CTX *mem_ctx,
                                         int argc, const char **argv)
{
	WERROR result, ret = WERR_UNKNOWN_PRINTER_DRIVER;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	int   i;
	int vers = -1;

	const char *arch = NULL;
	uint32_t delete_flags = 0;

	/* parse the command arguments */
	if (argc < 2 || argc > 4) {
		printf ("Usage: %s <driver> [arch] [version]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 3)
		arch = argv[2];
	if (argc == 4)
		vers = atoi (argv[3]);

	if (vers >= 0) {
		delete_flags |= DPD_DELETE_SPECIFIC_VERSION;
	}

	/* delete the driver for all architectures */
	for (i=0; archi_table[i].long_archi; i++) {

		if (arch &&  !strequal( archi_table[i].long_archi, arch))
			continue;

		if (vers >= 0 && archi_table[i].version != vers)
			continue;

		/* make the call to remove the driver */
		status = dcerpc_spoolss_DeletePrinterDriverEx(b, mem_ctx,
							      cli->srv_name_slash,
							      archi_table[i].long_archi,
							      argv[1],
							      delete_flags,
							      archi_table[i].version,
							      &result);
		if (!NT_STATUS_IS_OK(status)) {
			return ntstatus_to_werror(status);
		}
		if ( !W_ERROR_IS_OK(result) )
		{
			if ( !W_ERROR_EQUAL(result, WERR_UNKNOWN_PRINTER_DRIVER) ) {
				printf ("Failed to remove driver %s for arch [%s] (version: %d): %s\n",
					argv[1], archi_table[i].long_archi, archi_table[i].version, win_errstr(result));
			}
		}
		else
		{
			printf ("Driver %s and files removed for arch [%s] (version: %d).\n", argv[1],
			archi_table[i].long_archi, archi_table[i].version);
			ret = WERR_OK;
		}
	}

	return ret;
}


/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_deletedriver(struct rpc_pipe_client *cli,
                                         TALLOC_CTX *mem_ctx,
                                         int argc, const char **argv)
{
	WERROR result = WERR_OK;
	NTSTATUS status;
	int			i;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	/* parse the command arguments */
	if (argc != 2) {
		printf ("Usage: %s <driver>\n", argv[0]);
		return WERR_OK;
        }

	/* delete the driver for all architectures */
	for (i=0; archi_table[i].long_archi; i++) {
		result = WERR_OK;

		/* make the call to remove the driver */
		status = dcerpc_spoolss_DeletePrinterDriver(b, mem_ctx,
							    cli->srv_name_slash,
							    archi_table[i].long_archi,
							    argv[1],
							    &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
			continue;
		}
		if ( !W_ERROR_IS_OK(result) ) {
			if ( !W_ERROR_EQUAL(result, WERR_UNKNOWN_PRINTER_DRIVER) ) {
				printf ("Failed to remove driver %s for arch [%s] - error %s!\n",
					argv[1], archi_table[i].long_archi,
					win_errstr(result));
			}
		} else {
			printf ("Driver %s removed for arch [%s].\n", argv[1],
				archi_table[i].long_archi);
		}
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_getprintprocdir(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    int argc, const char **argv)
{
	WERROR result;
	NTSTATUS status;
	const char *environment = SPOOLSS_ARCHITECTURE_NT_X86;
	DATA_BLOB buffer;
	uint32_t offered;
	union spoolss_PrintProcessorDirectoryInfo info;
	uint32_t needed;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	/* parse the command arguments */
	if (argc > 2) {
		printf ("Usage: %s [environment]\n", argv[0]);
		return WERR_OK;
        }

	if (argc == 2) {
		environment = argv[1];
	}

	status = dcerpc_spoolss_GetPrintProcessorDirectory(b, mem_ctx,
							   cli->srv_name_slash,
							   environment,
							   1,
							   NULL, /* buffer */
							   0, /* offered */
							   NULL, /* info */
							   &needed,
							   &result);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	if (W_ERROR_EQUAL(result, WERR_INSUFFICIENT_BUFFER)) {
		offered = needed;
		buffer = data_blob_talloc_zero(mem_ctx, needed);

		status = dcerpc_spoolss_GetPrintProcessorDirectory(b, mem_ctx,
								   cli->srv_name_slash,
								   environment,
								   1,
								   &buffer,
								   offered,
								   &info,
								   &needed,
								   &result);
		if (!NT_STATUS_IS_OK(status)) {
			return ntstatus_to_werror(status);
		}
	}

	if (W_ERROR_IS_OK(result)) {
		printf("%s\n", info.info1.directory_name);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_addform(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
				    int argc, const char **argv)
{
	struct policy_handle handle;
	WERROR werror;
	NTSTATUS status;
	const char *printername;
	union spoolss_AddFormInfo info;
	struct spoolss_AddFormInfo1 info1;
	struct spoolss_AddFormInfo2 info2;
	uint32_t level = 1;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	/* Parse the command arguments */

	if (argc < 3 || argc > 5) {
		printf ("Usage: %s <printer> <formname> [level]\n", argv[0]);
		return WERR_OK;
        }

	/* Get a printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	werror = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       PRINTER_ALL_ACCESS,
					       &handle);
	if (!W_ERROR_IS_OK(werror))
		goto done;

	/* Dummy up some values for the form data */

	if (argc == 4) {
		level = atoi(argv[3]);
	}

	switch (level) {
	case 1:
		info1.flags		= SPOOLSS_FORM_USER;
		info1.form_name		= argv[2];
		info1.size.width	= 100;
		info1.size.height	= 100;
		info1.area.left		= 0;
		info1.area.top		= 10;
		info1.area.right	= 20;
		info1.area.bottom	= 30;

		info.info1 = &info1;

		break;
	case 2:
		info2.flags		= SPOOLSS_FORM_USER;
		info2.form_name		= argv[2];
		info2.size.width	= 100;
		info2.size.height	= 100;
		info2.area.left		= 0;
		info2.area.top		= 10;
		info2.area.right	= 20;
		info2.area.bottom	= 30;
		info2.keyword		= argv[2];
		info2.string_type	= SPOOLSS_FORM_STRING_TYPE_NONE;
		info2.mui_dll		= NULL;
		info2.ressource_id	= 0;
		info2.display_name	= argv[2];
		info2.lang_id		= 0;

		info.info2 = &info2;

		break;
	default:
		werror = WERR_INVALID_PARAM;
		goto done;
	}

	/* Add the form */


	status = dcerpc_spoolss_AddForm(b, mem_ctx,
					&handle,
					level,
					info,
					&werror);
	if (!NT_STATUS_IS_OK(status)) {
		werror = ntstatus_to_werror(status);
		goto done;
	}
 done:
	if (is_valid_policy_hnd(&handle)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &handle, &_result);
	}

	return werror;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_setform(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
				    int argc, const char **argv)
{
	struct policy_handle handle;
	WERROR werror;
	NTSTATUS status;
	const char *printername;
	union spoolss_AddFormInfo info;
	struct spoolss_AddFormInfo1 info1;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	/* Parse the command arguments */

	if (argc != 3) {
		printf ("Usage: %s <printer> <formname>\n", argv[0]);
		return WERR_OK;
        }

	/* Get a printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	werror = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &handle);
	if (!W_ERROR_IS_OK(werror))
		goto done;

	/* Dummy up some values for the form data */

	info1.flags		= SPOOLSS_FORM_PRINTER;
	info1.size.width	= 100;
	info1.size.height	= 100;
	info1.area.left		= 0;
	info1.area.top		= 1000;
	info1.area.right	= 2000;
	info1.area.bottom	= 3000;
	info1.form_name		= argv[2];

	info.info1 = &info1;

	/* Set the form */

	status = dcerpc_spoolss_SetForm(b, mem_ctx,
					&handle,
					argv[2],
					1,
					info,
					&werror);
	if (!NT_STATUS_IS_OK(status)) {
		werror = ntstatus_to_werror(status);
		goto done;
	}
 done:
	if (is_valid_policy_hnd(&handle)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &handle, &_result);
	}

	return werror;
}

/****************************************************************************
****************************************************************************/

static const char *get_form_flag(int form_flag)
{
	switch (form_flag) {
	case SPOOLSS_FORM_USER:
		return "FORM_USER";
	case SPOOLSS_FORM_BUILTIN:
		return "FORM_BUILTIN";
	case SPOOLSS_FORM_PRINTER:
		return "FORM_PRINTER";
	default:
		return "unknown";
	}
}

/****************************************************************************
****************************************************************************/

static void display_form_info1(struct spoolss_FormInfo1 *r)
{
	printf("%s\n" \
		"\tflag: %s (%d)\n" \
		"\twidth: %d, length: %d\n" \
		"\tleft: %d, right: %d, top: %d, bottom: %d\n\n",
		r->form_name, get_form_flag(r->flags), r->flags,
		r->size.width, r->size.height,
		r->area.left, r->area.right,
		r->area.top, r->area.bottom);
}

/****************************************************************************
****************************************************************************/

static void display_form_info2(struct spoolss_FormInfo2 *r)
{
	printf("%s\n" \
		"\tflag: %s (%d)\n" \
		"\twidth: %d, length: %d\n" \
		"\tleft: %d, right: %d, top: %d, bottom: %d\n",
		r->form_name, get_form_flag(r->flags), r->flags,
		r->size.width, r->size.height,
		r->area.left, r->area.right,
		r->area.top, r->area.bottom);
	printf("\tkeyword: %s\n", r->keyword);
	printf("\tstring_type: 0x%08x\n", r->string_type);
	printf("\tmui_dll: %s\n", r->mui_dll);
	printf("\tressource_id: 0x%08x\n", r->ressource_id);
	printf("\tdisplay_name: %s\n", r->display_name);
	printf("\tlang_id: %d\n", r->lang_id);
	printf("\n");
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_getform(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
				    int argc, const char **argv)
{
	struct policy_handle handle;
	WERROR werror;
	NTSTATUS status;
	const char *printername;
	DATA_BLOB buffer;
	uint32_t offered = 0;
	union spoolss_FormInfo info;
	uint32_t needed;
	uint32_t level = 1;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	/* Parse the command arguments */

	if (argc < 3 || argc > 5) {
		printf ("Usage: %s <printer> <formname> [level]\n", argv[0]);
		return WERR_OK;
        }

	/* Get a printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	werror = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &handle);
	if (!W_ERROR_IS_OK(werror))
		goto done;

	if (argc == 4) {
		level = atoi(argv[3]);
	}

	/* Get the form */

	status = dcerpc_spoolss_GetForm(b, mem_ctx,
					&handle,
					argv[2],
					level,
					NULL,
					offered,
					&info,
					&needed,
					&werror);
	if (!NT_STATUS_IS_OK(status)) {
		werror = ntstatus_to_werror(status);
		goto done;
	}
	if (W_ERROR_EQUAL(werror, WERR_INSUFFICIENT_BUFFER)) {
		buffer = data_blob_talloc_zero(mem_ctx, needed);
		offered = needed;
		status = dcerpc_spoolss_GetForm(b, mem_ctx,
						&handle,
						argv[2],
						level,
						&buffer,
						offered,
						&info,
						&needed,
						&werror);
		if (!NT_STATUS_IS_OK(status)) {
			werror = ntstatus_to_werror(status);
			goto done;
		}
	}

	if (!W_ERROR_IS_OK(werror)) {
		goto done;
	}

	switch (level) {
	case 1:
		display_form_info1(&info.info1);
		break;
	case 2:
		display_form_info2(&info.info2);
		break;
	}

 done:
	if (is_valid_policy_hnd(&handle)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &handle, &_result);
	}

	return werror;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_deleteform(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx, int argc,
				       const char **argv)
{
	struct policy_handle handle;
	WERROR werror;
	NTSTATUS status;
	const char *printername;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	/* Parse the command arguments */

	if (argc != 3) {
		printf ("Usage: %s <printer> <formname>\n", argv[0]);
		return WERR_OK;
        }

	/* Get a printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	werror = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &handle);
	if (!W_ERROR_IS_OK(werror))
		goto done;

	/* Delete the form */

	status = dcerpc_spoolss_DeleteForm(b, mem_ctx,
					   &handle,
					   argv[2],
					   &werror);
	if (!NT_STATUS_IS_OK(status)) {
		werror = ntstatus_to_werror(status);
		goto done;
	}

 done:
	if (is_valid_policy_hnd(&handle)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &handle, &_result);
	}

	return werror;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_enum_forms(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx, int argc,
				       const char **argv)
{
	struct policy_handle handle;
	WERROR werror;
	const char *printername;
	uint32_t num_forms, level = 1, i;
	union spoolss_FormInfo *forms;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	/* Parse the command arguments */

	if (argc < 2 || argc > 4) {
		printf ("Usage: %s <printer> [level]\n", argv[0]);
		return WERR_OK;
        }

	/* Get a printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	werror = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &handle);
	if (!W_ERROR_IS_OK(werror))
		goto done;

	if (argc == 3) {
		level = atoi(argv[2]);
	}

	/* Enumerate forms */

	werror = rpccli_spoolss_enumforms(cli, mem_ctx,
					  &handle,
					  level,
					  0,
					  &num_forms,
					  &forms);

	if (!W_ERROR_IS_OK(werror))
		goto done;

	/* Display output */

	for (i = 0; i < num_forms; i++) {
		switch (level) {
		case 1:
			display_form_info1(&forms[i].info1);
			break;
		case 2:
			display_form_info2(&forms[i].info2);
			break;
		}
	}

 done:
	if (is_valid_policy_hnd(&handle)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &handle, &_result);
	}

	return werror;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_setprinterdata(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    int argc, const char **argv)
{
	WERROR result;
	NTSTATUS status;
	const char *printername;
	struct policy_handle pol;
	union spoolss_PrinterInfo info;
	enum winreg_Type type;
	union spoolss_PrinterData data;
	DATA_BLOB blob;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	/* parse the command arguments */
	if (argc < 5) {
		printf ("Usage: %s <printer> <string|binary|dword|multistring>"
			" <value> <data>\n",
			argv[0]);
		return WERR_OK;
	}

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	type = REG_NONE;

	if (strequal(argv[2], "string")) {
		type = REG_SZ;
	}

	if (strequal(argv[2], "binary")) {
		type = REG_BINARY;
	}

	if (strequal(argv[2], "dword")) {
		type = REG_DWORD;
	}

	if (strequal(argv[2], "multistring")) {
		type = REG_MULTI_SZ;
	}

	if (type == REG_NONE) {
		printf("Unknown data type: %s\n", argv[2]);
		result =  WERR_INVALID_PARAM;
		goto done;
	}

	/* get a printer handle */

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &pol);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	result = rpccli_spoolss_getprinter(cli, mem_ctx,
					   &pol,
					   0,
					   0,
					   &info);
        if (!W_ERROR_IS_OK(result)) {
                goto done;
	}

	printf("%s\n", current_timestring(mem_ctx, true));
	printf("\tchange_id (before set)\t:[0x%x]\n", info.info0.change_id);

	/* Set the printer data */

	switch (type) {
	case REG_SZ:
		data.string = talloc_strdup(mem_ctx, argv[4]);
		W_ERROR_HAVE_NO_MEMORY(data.string);
		break;
	case REG_DWORD:
		data.value = strtoul(argv[4], NULL, 10);
		break;
	case REG_BINARY:
		data.binary = strhex_to_data_blob(mem_ctx, argv[4]);
		break;
	case REG_MULTI_SZ: {
		int i, num_strings;
		const char **strings = NULL;

		for (i=4; i<argc; i++) {
			if (strcmp(argv[i], "NULL") == 0) {
				argv[i] = "";
			}
			if (!add_string_to_array(mem_ctx, argv[i],
						 &strings,
						 &num_strings)) {
				result = WERR_NOMEM;
				goto done;
			}
		}
		data.string_array = talloc_zero_array(mem_ctx, const char *, num_strings + 1);
		if (!data.string_array) {
			result = WERR_NOMEM;
			goto done;
		}
		for (i=0; i < num_strings; i++) {
			data.string_array[i] = strings[i];
		}
		break;
		}
	default:
		printf("Unknown data type: %s\n", argv[2]);
		result = WERR_INVALID_PARAM;
		goto done;
	}

	result = push_spoolss_PrinterData(mem_ctx, &blob, type, &data);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	status = dcerpc_spoolss_SetPrinterData(b, mem_ctx,
					       &pol,
					       argv[3], /* value_name */
					       type,
					       blob.data,
					       blob.length,
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		printf ("Unable to set [%s=%s]!\n", argv[3], argv[4]);
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		printf ("Unable to set [%s=%s]!\n", argv[3], argv[4]);
		goto done;
	}
	printf("\tSetPrinterData succeeded [%s: %s]\n", argv[3], argv[4]);

	result = rpccli_spoolss_getprinter(cli, mem_ctx,
					   &pol,
					   0,
					   0,
					   &info);
        if (!W_ERROR_IS_OK(result)) {
                goto done;
	}

	printf("%s\n", current_timestring(mem_ctx, true));
	printf("\tchange_id (after set)\t:[0x%x]\n", info.info0.change_id);

done:
	/* cleanup */
	if (is_valid_policy_hnd(&pol)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &pol, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static void display_job_info1(struct spoolss_JobInfo1 *r)
{
	printf("%d: jobid[%d]: %s %s %s %d/%d pages\n", r->position, r->job_id,
	       r->user_name, r->document_name, r->text_status, r->pages_printed,
	       r->total_pages);
}

/****************************************************************************
****************************************************************************/

static void display_job_info2(struct spoolss_JobInfo2 *r)
{
	printf("%d: jobid[%d]: %s %s %s %d/%d pages, %d bytes\n",
	       r->position, r->job_id,
	       r->user_name, r->document_name, r->text_status, r->pages_printed,
	       r->total_pages, r->size);
}

/****************************************************************************
****************************************************************************/

static void display_job_info3(struct spoolss_JobInfo3 *r)
{
	printf("jobid[%d], next_jobid[%d]\n",
		r->job_id, r->next_job_id);
}

/****************************************************************************
****************************************************************************/

static void display_job_info4(struct spoolss_JobInfo4 *r)
{
	printf("%d: jobid[%d]: %s %s %s %d/%d pages, %d/%d bytes\n",
	       r->position, r->job_id,
	       r->user_name, r->document_name, r->text_status, r->pages_printed,
	       r->total_pages, r->size, r->size_high);
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_enum_jobs(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx, int argc,
				      const char **argv)
{
	WERROR result;
	uint32_t level = 1, count, i;
	const char *printername;
	struct policy_handle hnd;
	union spoolss_JobInfo *info;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2 || argc > 3) {
		printf("Usage: %s printername [level]\n", argv[0]);
		return WERR_OK;
	}

	if (argc == 3) {
		level = atoi(argv[2]);
	}

	/* Open printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &hnd);
	if (!W_ERROR_IS_OK(result))
		goto done;

	/* Enumerate ports */

	result = rpccli_spoolss_enumjobs(cli, mem_ctx,
					 &hnd,
					 0, /* firstjob */
					 1000, /* numjobs */
					 level,
					 0,
					 &count,
					 &info);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	for (i = 0; i < count; i++) {
		switch (level) {
		case 1:
			display_job_info1(&info[i].info1);
			break;
		case 2:
			display_job_info2(&info[i].info2);
			break;
		default:
			d_printf("unknown info level %d\n", level);
			break;
		}
	}

done:
	if (is_valid_policy_hnd(&hnd)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_get_job(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx, int argc,
				  const char **argv)
{
	WERROR result;
	const char *printername;
	struct policy_handle hnd;
	uint32_t job_id;
	uint32_t level = 1;
	union spoolss_JobInfo info;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 3 || argc > 4) {
		printf("Usage: %s printername job_id [level]\n", argv[0]);
		return WERR_OK;
	}

	job_id = atoi(argv[2]);

	if (argc == 4) {
		level = atoi(argv[3]);
	}

	/* Open printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &hnd);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Enumerate ports */

	result = rpccli_spoolss_getjob(cli, mem_ctx,
				       &hnd,
				       job_id,
				       level,
				       0,
				       &info);

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	switch (level) {
	case 1:
		display_job_info1(&info.info1);
		break;
	case 2:
		display_job_info2(&info.info2);
		break;
	case 3:
		display_job_info3(&info.info3);
		break;
	case 4:
		display_job_info4(&info.info4);
		break;
	default:
		d_printf("unknown info level %d\n", level);
		break;
	}

done:
	if (is_valid_policy_hnd(&hnd)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static struct {
	const char *name;
	enum spoolss_JobControl val;
} cmdvals[] = {
	{"PAUSE", SPOOLSS_JOB_CONTROL_PAUSE},
	{"RESUME", SPOOLSS_JOB_CONTROL_RESUME},
	{"CANCEL", SPOOLSS_JOB_CONTROL_CANCEL},
	{"RESTART", SPOOLSS_JOB_CONTROL_RESTART},
	{"DELETE", SPOOLSS_JOB_CONTROL_DELETE},
	{"SEND_TO_PRINTER", SPOOLSS_JOB_CONTROL_SEND_TO_PRINTER},
	{"EJECTED", SPOOLSS_JOB_CONTROL_LAST_PAGE_EJECTED},
	{"RETAIN", SPOOLSS_JOB_CONTROL_RETAIN},
	{"RELEASE", SPOOLSS_JOB_CONTROL_RELEASE}
};

static enum spoolss_JobControl parse_setjob_command(const char *cmd)
{
	int i;

	for (i = 0; i < sizeof(cmdvals)/sizeof(cmdvals[0]); i++) {
		if (strequal(cmdvals[i].name, cmd)) {
			return cmdvals[i].val;
		}
	}
	return (enum spoolss_JobControl)atoi(cmd);
}

static WERROR cmd_spoolss_set_job(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx, int argc,
				  const char **argv)
{
	WERROR result;
	NTSTATUS status;
	const char *printername;
	struct policy_handle hnd;
	uint32_t job_id;
	enum spoolss_JobControl command;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 4) {
		printf("Usage: %s printername job_id command\n", argv[0]);
		printf("command = [PAUSE|RESUME|CANCEL|RESTART|DELETE|"
			"SEND_TO_PRINTER|EJECTED|RETAIN|RELEASE]\n");
		return WERR_OK;
	}

	job_id = atoi(argv[2]);
	command = parse_setjob_command(argv[3]);

	/* Open printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &hnd);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Set Job */

	status = dcerpc_spoolss_SetJob(b, mem_ctx,
				       &hnd,
				       job_id,
				       NULL,
				       command,
				       &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

done:
	if (is_valid_policy_hnd(&hnd)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_enum_data(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx, int argc,
				    const char **argv)
{
	WERROR result;
	NTSTATUS status;
	const char *printername;
	struct policy_handle hnd;
	uint32_t value_needed;
	enum winreg_Type type;
	uint32_t data_needed;
	struct dcerpc_binding_handle *b = cli->binding_handle;
	struct spoolss_EnumPrinterData r;

	if (argc != 2) {
		printf("Usage: %s printername\n", argv[0]);
		return WERR_OK;
	}

	/* Open printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &hnd);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Enumerate data */

	r.in.handle = &hnd;
	r.in.enum_index = 0;
	r.in.value_offered = 0;
	r.in.data_offered = 0;
	r.out.value_name = NULL;
	r.out.value_needed = &value_needed;
	r.out.type = &type;
	r.out.data = NULL;
	r.out.data_needed = &data_needed;

	status = dcerpc_spoolss_EnumPrinterData_r(b, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(r.out.result)) {
		result = r.out.result;
		goto done;
	}

	r.in.data_offered	= *r.out.data_needed;
	r.in.value_offered	= *r.out.value_needed;
	r.out.data		= talloc_zero_array(mem_ctx, uint8_t, r.in.data_offered);
	r.out.value_name	= talloc_zero_array(mem_ctx, char, r.in.value_offered);

	do {

		status = dcerpc_spoolss_EnumPrinterData_r(b, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
			goto done;
		}

		if (W_ERROR_EQUAL(r.out.result, WERR_NO_MORE_ITEMS)) {
			result = WERR_OK;
			break;
		}

		r.in.enum_index++;

		{
			struct regval_blob *v;

			v = regval_compose(talloc_tos(),
					   r.out.value_name,
					   *r.out.type,
					   r.out.data,
					   r.in.data_offered);
			if (v == NULL) {
				result = WERR_NOMEM;
				goto done;
			}

			display_reg_value(v);
			talloc_free(v);
		}

	} while (W_ERROR_IS_OK(r.out.result));

done:
	if (is_valid_policy_hnd(&hnd)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_enum_data_ex( struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx, int argc,
					  const char **argv)
{
	WERROR result;
	uint32_t i;
	const char *printername;
	struct policy_handle hnd;
	uint32_t count;
	struct spoolss_PrinterEnumValues *info;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 3) {
		printf("Usage: %s printername <keyname>\n", argv[0]);
		return WERR_OK;
	}

	/* Open printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &hnd);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Enumerate subkeys */

	result = rpccli_spoolss_enumprinterdataex(cli, mem_ctx,
						  &hnd,
						  argv[2],
						  0,
						  &count,
						  &info);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	for (i=0; i < count; i++) {
		display_printer_data(info[i].value_name,
				     info[i].type,
				     info[i].data->data,
				     info[i].data->length);
	}

 done:
	if (is_valid_policy_hnd(&hnd)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_enum_printerkey(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx, int argc,
					  const char **argv)
{
	WERROR result;
	const char *printername;
	const char *keyname = NULL;
	struct policy_handle hnd;
	const char **key_buffer = NULL;
	int i;
	uint32_t offered = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2 || argc > 4) {
		printf("Usage: %s printername [keyname] [offered]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 3) {
		keyname = argv[2];
	} else {
		keyname = "";
	}

	if (argc == 4) {
		offered = atoi(argv[3]);
	}

	/* Open printer handle */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &hnd);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Enumerate subkeys */

	result = rpccli_spoolss_enumprinterkey(cli, mem_ctx,
					       &hnd,
					       keyname,
					       &key_buffer,
					       offered);

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	for (i=0; key_buffer && key_buffer[i]; i++) {
		printf("%s\n", key_buffer[i]);
	}

 done:

	if (is_valid_policy_hnd(&hnd)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static WERROR cmd_spoolss_rffpcnex(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx, int argc,
				     const char **argv)
{
	const char *printername;
	const char *clientname;
	struct policy_handle hnd;
	WERROR result;
	NTSTATUS status;
	struct spoolss_NotifyOption option;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 2) {
		printf("Usage: %s printername\n", argv[0]);
		result = WERR_OK;
		goto done;
	}

	/* Open printer */

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &hnd);
	if (!W_ERROR_IS_OK(result)) {
		printf("Error opening %s\n", argv[1]);
		goto done;
	}

	/* Create spool options */

	option.version = 2;
	option.count = 2;

	option.types = talloc_array(mem_ctx, struct spoolss_NotifyOptionType, 2);
	if (option.types == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	option.types[0].type = PRINTER_NOTIFY_TYPE;
	option.types[0].count = 1;
	option.types[0].fields = talloc_array(mem_ctx, union spoolss_Field, 1);
	if (option.types[0].fields == NULL) {
		result = WERR_NOMEM;
		goto done;
	}
	option.types[0].fields[0].field = PRINTER_NOTIFY_FIELD_SERVER_NAME;

	option.types[1].type = JOB_NOTIFY_TYPE;
	option.types[1].count = 1;
	option.types[1].fields = talloc_array(mem_ctx, union spoolss_Field, 1);
	if (option.types[1].fields == NULL) {
		result = WERR_NOMEM;
		goto done;
	}
	option.types[1].fields[0].field = JOB_NOTIFY_FIELD_PRINTER_NAME;

	clientname = talloc_asprintf(mem_ctx, "\\\\%s", global_myname());
	if (!clientname) {
		result = WERR_NOMEM;
		goto done;
	}

	/* Send rffpcnex */

	status = dcerpc_spoolss_RemoteFindFirstPrinterChangeNotifyEx(b, mem_ctx,
								     &hnd,
								     0,
								     0,
								     clientname,
								     123,
								     &option,
								     &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		printf("Error rffpcnex %s\n", argv[1]);
		goto done;
	}

done:
	if (is_valid_policy_hnd(&hnd)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &hnd, &_result);
	}

	return result;
}

/****************************************************************************
****************************************************************************/

static bool compare_printer( struct rpc_pipe_client *cli1, struct policy_handle *hnd1,
                             struct rpc_pipe_client *cli2, struct policy_handle *hnd2 )
{
	union spoolss_PrinterInfo info1, info2;
	WERROR werror;
	TALLOC_CTX *mem_ctx = talloc_init("compare_printer");

	printf("Retrieving printer propertiesfor %s...", cli1->desthost);
	werror = rpccli_spoolss_getprinter(cli1, mem_ctx,
					   hnd1,
					   2,
					   0,
					   &info1);
	if ( !W_ERROR_IS_OK(werror) ) {
		printf("failed (%s)\n", win_errstr(werror));
		talloc_destroy(mem_ctx);
		return false;
	}
	printf("ok\n");

	printf("Retrieving printer properties for %s...", cli2->desthost);
	werror = rpccli_spoolss_getprinter(cli2, mem_ctx,
					   hnd2,
					   2,
					   0,
					   &info2);
	if ( !W_ERROR_IS_OK(werror) ) {
		printf("failed (%s)\n", win_errstr(werror));
		talloc_destroy(mem_ctx);
		return false;
	}
	printf("ok\n");

	talloc_destroy(mem_ctx);

	return true;
}

/****************************************************************************
****************************************************************************/

static bool compare_printer_secdesc( struct rpc_pipe_client *cli1, struct policy_handle *hnd1,
                                     struct rpc_pipe_client *cli2, struct policy_handle *hnd2 )
{
	union spoolss_PrinterInfo info1, info2;
	WERROR werror;
	TALLOC_CTX *mem_ctx = talloc_init("compare_printer_secdesc");
	struct security_descriptor *sd1, *sd2;
	bool result = true;


	printf("Retrieving printer security for %s...", cli1->desthost);
	werror = rpccli_spoolss_getprinter(cli1, mem_ctx,
					   hnd1,
					   3,
					   0,
					   &info1);
	if ( !W_ERROR_IS_OK(werror) ) {
		printf("failed (%s)\n", win_errstr(werror));
		result = false;
		goto done;
	}
	printf("ok\n");

	printf("Retrieving printer security for %s...", cli2->desthost);
	werror = rpccli_spoolss_getprinter(cli2, mem_ctx,
					   hnd2,
					   3,
					   0,
					   &info2);
	if ( !W_ERROR_IS_OK(werror) ) {
		printf("failed (%s)\n", win_errstr(werror));
		result = false;
		goto done;
	}
	printf("ok\n");


	printf("++ ");

	sd1 = info1.info3.secdesc;
	sd2 = info2.info3.secdesc;

	if ( (sd1 != sd2) && ( !sd1 || !sd2 ) ) {
		printf("NULL secdesc!\n");
		result = false;
		goto done;
	}

	if (!security_descriptor_equal( sd1, sd2 ) ) {
		printf("Security Descriptors *not* equal!\n");
		result = false;
		goto done;
	}

	printf("Security descriptors match\n");

done:
	talloc_destroy(mem_ctx);
	return result;
}


/****************************************************************************
****************************************************************************/

extern struct user_auth_info *rpcclient_auth_info;

static WERROR cmd_spoolss_printercmp(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx, int argc,
				     const char **argv)
{
	const char *printername;
	char *printername_path = NULL;
	struct cli_state *cli_server2 = NULL;
	struct rpc_pipe_client *cli2 = NULL;
	struct policy_handle hPrinter1, hPrinter2;
	NTSTATUS nt_status;
	WERROR werror;

	if ( argc != 3 )  {
		printf("Usage: %s <printer> <server>\n", argv[0]);
		return WERR_OK;
	}

	printername = argv[1];

	/* first get the connection to the remote server */

	nt_status = cli_full_connection(&cli_server2, global_myname(), argv[2],
					NULL, 0,
					"IPC$", "IPC",
					get_cmdline_auth_info_username(rpcclient_auth_info),
					lp_workgroup(),
					get_cmdline_auth_info_password(rpcclient_auth_info),
					get_cmdline_auth_info_use_kerberos(rpcclient_auth_info) ? CLI_FULL_CONNECTION_USE_KERBEROS : 0,
					get_cmdline_auth_info_signing_state(rpcclient_auth_info));

	if ( !NT_STATUS_IS_OK(nt_status) )
		return WERR_GENERAL_FAILURE;

	nt_status = cli_rpc_pipe_open_noauth(cli_server2, &ndr_table_spoolss.syntax_id,
					     &cli2);
	if (!NT_STATUS_IS_OK(nt_status)) {
		printf("failed to open spoolss pipe on server %s (%s)\n",
			argv[2], nt_errstr(nt_status));
		return WERR_GENERAL_FAILURE;
	}

	/* now open up both printers */

	RPCCLIENT_PRINTERNAME(printername_path, cli, printername);

	printf("Opening %s...", printername_path);

	werror = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername_path,
					       PRINTER_ALL_ACCESS,
					       &hPrinter1);
	if ( !W_ERROR_IS_OK(werror) ) {
		printf("failed (%s)\n", win_errstr(werror));
		goto done;
	}
	printf("ok\n");

	RPCCLIENT_PRINTERNAME(printername_path, cli2, printername);

	printf("Opening %s...", printername_path);
	werror = rpccli_spoolss_openprinter_ex(cli2, mem_ctx,
					       printername_path,
					       PRINTER_ALL_ACCESS,
					       &hPrinter2);
	if ( !W_ERROR_IS_OK(werror) ) {
		 printf("failed (%s)\n", win_errstr(werror));
		goto done;
	}
	printf("ok\n");

	compare_printer( cli, &hPrinter1, cli2, &hPrinter2 );
	compare_printer_secdesc( cli, &hPrinter1, cli2, &hPrinter2 );
#if 0
	compare_printerdata( cli_server1, &hPrinter1, cli_server2, &hPrinter2 );
#endif


done:
	/* cleanup */

	printf("Closing printers...");
	{
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(cli->binding_handle, mem_ctx, &hPrinter1, &_result);
		dcerpc_spoolss_ClosePrinter(cli2->binding_handle, mem_ctx, &hPrinter2, &_result);
	}
	printf("ok\n");

	/* close the second remote connection */

	cli_shutdown( cli_server2 );
	return WERR_OK;
}

static void display_proc_info1(struct spoolss_PrintProcessorInfo1 *r)
{
	printf("print_processor_name: %s\n", r->print_processor_name);
}

static WERROR cmd_spoolss_enum_procs(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx, int argc,
				     const char **argv)
{
	WERROR werror;
	const char *environment = SPOOLSS_ARCHITECTURE_NT_X86;
	uint32_t num_procs, level = 1, i;
	union spoolss_PrintProcessorInfo *procs;

	/* Parse the command arguments */

	if (argc < 1 || argc > 4) {
		printf ("Usage: %s [environment] [level]\n", argv[0]);
		return WERR_OK;
        }

	if (argc >= 2) {
		environment = argv[1];
	}

	if (argc == 3) {
		level = atoi(argv[2]);
	}

	/* Enumerate Print Processors */

	werror = rpccli_spoolss_enumprintprocessors(cli, mem_ctx,
						    cli->srv_name_slash,
						    environment,
						    level,
						    0,
						    &num_procs,
						    &procs);
	if (!W_ERROR_IS_OK(werror))
		goto done;

	/* Display output */

	for (i = 0; i < num_procs; i++) {
		switch (level) {
		case 1:
			display_proc_info1(&procs[i].info1);
			break;
		}
	}

 done:
	return werror;
}

static void display_proc_data_types_info1(struct spoolss_PrintProcDataTypesInfo1 *r)
{
	printf("name_array: %s\n", r->name_array);
}

static WERROR cmd_spoolss_enum_proc_data_types(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx, int argc,
					       const char **argv)
{
	WERROR werror;
	const char *print_processor_name = "winprint";
	uint32_t num_procs, level = 1, i;
	union spoolss_PrintProcDataTypesInfo *procs;

	/* Parse the command arguments */

	if (argc < 1 || argc > 4) {
		printf ("Usage: %s [environment] [level]\n", argv[0]);
		return WERR_OK;
        }

	if (argc >= 2) {
		print_processor_name = argv[1];
	}

	if (argc == 3) {
		level = atoi(argv[2]);
	}

	/* Enumerate Print Processor Data Types */

	werror = rpccli_spoolss_enumprintprocessordatatypes(cli, mem_ctx,
							    cli->srv_name_slash,
							    print_processor_name,
							    level,
							    0,
							    &num_procs,
							    &procs);
	if (!W_ERROR_IS_OK(werror))
		goto done;

	/* Display output */

	for (i = 0; i < num_procs; i++) {
		switch (level) {
		case 1:
			display_proc_data_types_info1(&procs[i].info1);
			break;
		}
	}

 done:
	return werror;
}

static void display_monitor1(const struct spoolss_MonitorInfo1 *r)
{
	printf("monitor_name: %s\n", r->monitor_name);
}

static void display_monitor2(const struct spoolss_MonitorInfo2 *r)
{
	printf("monitor_name: %s\n", r->monitor_name);
	printf("environment: %s\n", r->environment);
	printf("dll_name: %s\n", r->dll_name);
}

static WERROR cmd_spoolss_enum_monitors(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx, int argc,
					const char **argv)
{
	WERROR werror;
	uint32_t count, level = 1, i;
	union spoolss_MonitorInfo *info;

	/* Parse the command arguments */

	if (argc > 2) {
		printf("Usage: %s [level]\n", argv[0]);
		return WERR_OK;
	}

	if (argc == 2) {
		level = atoi(argv[1]);
	}

	/* Enumerate Print Monitors */

	werror = rpccli_spoolss_enummonitors(cli, mem_ctx,
					     cli->srv_name_slash,
					     level,
					     0,
					     &count,
					     &info);
	if (!W_ERROR_IS_OK(werror)) {
		goto done;
	}

	/* Display output */

	for (i = 0; i < count; i++) {
		switch (level) {
		case 1:
			display_monitor1(&info[i].info1);
			break;
		case 2:
			display_monitor2(&info[i].info2);
			break;
		}
	}

 done:
	return werror;
}

static WERROR cmd_spoolss_create_printer_ic(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx, int argc,
					    const char **argv)
{
	WERROR result;
	NTSTATUS status;
	struct policy_handle handle, gdi_handle;
	const char *printername;
	struct spoolss_DevmodeContainer devmode_ctr;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	RPCCLIENT_PRINTERNAME(printername, cli, argv[1]);

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &handle);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	ZERO_STRUCT(devmode_ctr);

	status = dcerpc_spoolss_CreatePrinterIC(b, mem_ctx,
						&handle,
						&gdi_handle,
						&devmode_ctr,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

 done:
	if (is_valid_policy_hnd(&gdi_handle)) {
		WERROR _result;
		dcerpc_spoolss_DeletePrinterIC(b, mem_ctx, &gdi_handle, &_result);
	}
	if (is_valid_policy_hnd(&handle)) {
		WERROR _result;
		dcerpc_spoolss_ClosePrinter(b, mem_ctx, &handle, &_result);
	}

	return result;
}

/* List of commands exported by this module */
struct cmd_set spoolss_commands[] = {

	{ "SPOOLSS"  },

	{ "adddriver",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_addprinterdriver,	&ndr_table_spoolss.syntax_id, NULL, "Add a print driver",                  "" },
	{ "addprinter",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_addprinterex,	&ndr_table_spoolss.syntax_id, NULL, "Add a printer",                       "" },
	{ "deldriver",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_deletedriver,	&ndr_table_spoolss.syntax_id, NULL, "Delete a printer driver",             "" },
	{ "deldriverex",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_deletedriverex,	&ndr_table_spoolss.syntax_id, NULL, "Delete a printer driver with files",  "" },
	{ "enumdata",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_data,		&ndr_table_spoolss.syntax_id, NULL, "Enumerate printer data",              "" },
	{ "enumdataex",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_data_ex,	&ndr_table_spoolss.syntax_id, NULL, "Enumerate printer data for a key",    "" },
	{ "enumkey",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_printerkey,	&ndr_table_spoolss.syntax_id, NULL, "Enumerate printer keys",              "" },
	{ "enumjobs",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_jobs,          &ndr_table_spoolss.syntax_id, NULL, "Enumerate print jobs",                "" },
	{ "getjob",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_get_job,		&ndr_table_spoolss.syntax_id, NULL, "Get print job",                       "" },
	{ "setjob",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_set_job,		&ndr_table_spoolss.syntax_id, NULL, "Set print job",                       "" },
	{ "enumports", 		RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_ports, 	&ndr_table_spoolss.syntax_id, NULL, "Enumerate printer ports",             "" },
	{ "enumdrivers", 	RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_drivers, 	&ndr_table_spoolss.syntax_id, NULL, "Enumerate installed printer drivers", "" },
	{ "enumprinters", 	RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_printers, 	&ndr_table_spoolss.syntax_id, NULL, "Enumerate printers",                  "" },
	{ "getdata",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_getprinterdata,	&ndr_table_spoolss.syntax_id, NULL, "Get print driver data",               "" },
	{ "getdataex",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_getprinterdataex,	&ndr_table_spoolss.syntax_id, NULL, "Get printer driver data with keyname", ""},
	{ "getdriver",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_getdriver,		&ndr_table_spoolss.syntax_id, NULL, "Get print driver information",        "" },
	{ "getdriverdir",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_getdriverdir,	&ndr_table_spoolss.syntax_id, NULL, "Get print driver upload directory",   "" },
	{ "getprinter", 	RPC_RTYPE_WERROR, NULL, cmd_spoolss_getprinter, 	&ndr_table_spoolss.syntax_id, NULL, "Get printer info",                    "" },
	{ "openprinter",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_open_printer,	&ndr_table_spoolss.syntax_id, NULL, "Open printer handle",                 "" },
	{ "openprinter_ex",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_open_printer_ex,	&ndr_table_spoolss.syntax_id, NULL, "Open printer handle",                 "" },
	{ "setdriver", 		RPC_RTYPE_WERROR, NULL, cmd_spoolss_setdriver,		&ndr_table_spoolss.syntax_id, NULL, "Set printer driver",                  "" },
	{ "getprintprocdir",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_getprintprocdir,    &ndr_table_spoolss.syntax_id, NULL, "Get print processor directory",       "" },
	{ "addform",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_addform,            &ndr_table_spoolss.syntax_id, NULL, "Add form",                            "" },
	{ "setform",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_setform,            &ndr_table_spoolss.syntax_id, NULL, "Set form",                            "" },
	{ "getform",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_getform,            &ndr_table_spoolss.syntax_id, NULL, "Get form",                            "" },
	{ "deleteform",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_deleteform,         &ndr_table_spoolss.syntax_id, NULL, "Delete form",                         "" },
	{ "enumforms",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_forms,         &ndr_table_spoolss.syntax_id, NULL, "Enumerate forms",                     "" },
	{ "setprinter",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_setprinter,         &ndr_table_spoolss.syntax_id, NULL, "Set printer comment",                 "" },
	{ "setprintername",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_setprintername,	&ndr_table_spoolss.syntax_id, NULL, "Set printername",                 "" },
	{ "setprinterdata",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_setprinterdata,     &ndr_table_spoolss.syntax_id, NULL, "Set REG_SZ printer data",             "" },
	{ "rffpcnex",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_rffpcnex,           &ndr_table_spoolss.syntax_id, NULL, "Rffpcnex test", "" },
	{ "printercmp",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_printercmp,         &ndr_table_spoolss.syntax_id, NULL, "Printer comparison test", "" },
	{ "enumprocs",		RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_procs,         &ndr_table_spoolss.syntax_id, NULL, "Enumerate Print Processors",          "" },
	{ "enumprocdatatypes",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_proc_data_types, &ndr_table_spoolss.syntax_id, NULL, "Enumerate Print Processor Data Types", "" },
	{ "enummonitors",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_enum_monitors,      &ndr_table_spoolss.syntax_id, NULL, "Enumerate Print Monitors", "" },
	{ "createprinteric",	RPC_RTYPE_WERROR, NULL, cmd_spoolss_create_printer_ic,  &ndr_table_spoolss.syntax_id, NULL, "Create Printer IC", "" },

	{ NULL }
};
