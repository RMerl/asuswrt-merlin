/* 
   Unix SMB/CIFS implementation.
   SMB torture tester - scanning functions
   Copyright (C) Andrew Tridgell 2001
   
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
#include "libcli/libcli.h"
#include "torture/util.h"
#include "libcli/raw/raw_proto.h"
#include "system/filesys.h"
#include "param/param.h"

#define VERBOSE 0
#define OP_MIN 0
#define OP_MAX 100
#define PARAM_SIZE 1024

/****************************************************************************
look for a partial hit
****************************************************************************/
static void trans2_check_hit(const char *format, int op, int level, NTSTATUS status)
{
	if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_LEVEL) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_UNSUCCESSFUL) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_INVALID_INFO_CLASS)) {
		return;
	}
#if VERBOSE
	printf("possible %s hit op=%3d level=%5d status=%s\n",
	       format, op, level, nt_errstr(status));
#endif
}

/****************************************************************************
check for existance of a trans2 call
****************************************************************************/
static NTSTATUS try_trans2(struct smbcli_state *cli, 
			   int op,
			   uint8_t *param, uint8_t *data,
			   int param_len, int data_len,
			   int *rparam_len, int *rdata_len)
{
	NTSTATUS status;
	struct smb_trans2 t2;
	uint16_t setup = op;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("try_trans2");

	t2.in.max_param = UINT16_MAX;
	t2.in.max_data = UINT16_MAX;
	t2.in.max_setup = 10;
	t2.in.flags = 0;
	t2.in.timeout = 0;
	t2.in.setup_count = 1;
	t2.in.setup = &setup;
	t2.in.params.data = param;
	t2.in.params.length = param_len;
	t2.in.data.data = data;
	t2.in.data.length = data_len;

	status = smb_raw_trans2(cli->tree, mem_ctx, &t2);

	*rparam_len = t2.out.params.length;
	*rdata_len = t2.out.data.length;

	talloc_free(mem_ctx);

	return status;
}


static NTSTATUS try_trans2_len(struct smbcli_state *cli,
			     const char *format,
			     int op, int level,
			     uint8_t *param, uint8_t *data,
			     int param_len, int *data_len,
			     int *rparam_len, int *rdata_len)
{
	NTSTATUS ret=NT_STATUS_OK;

	ret = try_trans2(cli, op, param, data, param_len,
			 PARAM_SIZE, rparam_len, rdata_len);
#if VERBOSE
	printf("op=%d level=%d ret=%s\n", op, level, nt_errstr(ret));
#endif
	if (!NT_STATUS_IS_OK(ret)) return ret;

	*data_len = 0;
	while (*data_len < PARAM_SIZE) {
		ret = try_trans2(cli, op, param, data, param_len,
				 *data_len, rparam_len, rdata_len);
		if (NT_STATUS_IS_OK(ret)) break;
		*data_len += 2;
	}
	if (NT_STATUS_IS_OK(ret)) {
		printf("found %s level=%d data_len=%d rparam_len=%d rdata_len=%d\n",
		       format, level, *data_len, *rparam_len, *rdata_len);
	} else {
		trans2_check_hit(format, op, level, ret);
	}
	return ret;
}


/****************************************************************************
check whether a trans2 opnum exists at all
****************************************************************************/
static bool trans2_op_exists(struct smbcli_state *cli, int op)
{
	int data_len = PARAM_SIZE;
	int param_len = PARAM_SIZE;
	int rparam_len, rdata_len;
	uint8_t *param, *data;
	NTSTATUS status1, status2;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("trans2_op_exists");

	/* try with a info level only */

	param = talloc_array(mem_ctx, uint8_t, param_len);
	data  = talloc_array(mem_ctx, uint8_t, data_len);

	memset(param, 0xFF, param_len);
	memset(data, 0xFF, data_len);

	status1 = try_trans2(cli, 0xFFFF, param, data, param_len, data_len,
			     &rparam_len, &rdata_len);

	status2 = try_trans2(cli, op, param, data, param_len, data_len,
			     &rparam_len, &rdata_len);

	if (NT_STATUS_EQUAL(status1, status2)) {
		talloc_free(mem_ctx);
		return false;
	}

	printf("Found op %d (status=%s)\n", op, nt_errstr(status2));

	talloc_free(mem_ctx);
	return true;
}

/****************************************************************************
check for existance of a trans2 call
****************************************************************************/
static bool scan_trans2(
			struct smbcli_state *cli, int op, int level,
			int fnum, int dnum, int qfnum, const char *fname)
{
	int data_len = 0;
	int param_len = 0;
	int rparam_len, rdata_len;
	uint8_t *param, *data;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("scan_trans2");

	data = talloc_array(mem_ctx, uint8_t, PARAM_SIZE);
	param = talloc_array(mem_ctx, uint8_t, PARAM_SIZE);

	memset(data, 0, PARAM_SIZE);
	data_len = 4;

	/* try with a info level only */
	param_len = 2;
	SSVAL(param, 0, level);
	status = try_trans2_len(cli, "void", op, level, param, data, param_len,
			&data_len, &rparam_len, &rdata_len);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try with a file descriptor */
	param_len = 6;
	SSVAL(param, 0, fnum);
	SSVAL(param, 2, level);
	SSVAL(param, 4, 0);
	status = try_trans2_len(cli, "fnum", op, level, param, data, param_len,
			&data_len, &rparam_len, &rdata_len);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try with a quota file descriptor */
	param_len = 6;
	SSVAL(param, 0, qfnum);
	SSVAL(param, 2, level);
	SSVAL(param, 4, 0);
	status = try_trans2_len(cli, "qfnum", op, level, param, data, param_len,
		       	&data_len, &rparam_len, &rdata_len);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try with a notify style */
	param_len = 6;
	SSVAL(param, 0, dnum);
	SSVAL(param, 2, dnum);
	SSVAL(param, 4, level);
	status = try_trans2_len(cli, "notify", op, level, param, data,
			param_len, &data_len, &rparam_len, &rdata_len);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try with a file name */
	param_len = 6;
	SSVAL(param, 0, level);
	SSVAL(param, 2, 0);
	SSVAL(param, 4, 0);
	param_len += push_string(
			&param[6], fname, PARAM_SIZE-7,
			STR_TERMINATE|STR_UNICODE);

	status = try_trans2_len(cli, "fname", op, level, param, data, param_len,
			&data_len, &rparam_len, &rdata_len);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try with a new file name */
	param_len = 6;
	SSVAL(param, 0, level);
	SSVAL(param, 2, 0);
	SSVAL(param, 4, 0);
	param_len += push_string(
			&param[6], "\\newfile.dat", PARAM_SIZE-7,
			STR_TERMINATE|STR_UNICODE);

	status = try_trans2_len(cli, "newfile", op, level, param, data,
			param_len, &data_len, &rparam_len, &rdata_len);
	smbcli_unlink(cli->tree, "\\newfile.dat");
	smbcli_rmdir(cli->tree, "\\newfile.dat");
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try dfs style  */
	smbcli_mkdir(cli->tree, "\\testdir");
	param_len = 2;
	SSVAL(param, 0, level);
	param_len += push_string(
			&param[2], "\\testdir", PARAM_SIZE-3,
			STR_TERMINATE|STR_UNICODE);

	status = try_trans2_len(cli, "dfs", op, level, param, data, param_len,
			&data_len, &rparam_len, &rdata_len);
	smbcli_rmdir(cli->tree, "\\testdir");
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	talloc_free(mem_ctx);
	return false;
}

bool torture_trans2_scan(struct torture_context *torture,
						 struct smbcli_state *cli)
{
	int op, level;
	const char *fname = "\\scanner.dat";
	int fnum, dnum, qfnum;

	fnum = smbcli_open(cli->tree, fname, O_RDWR | O_CREAT | O_TRUNC, DENY_NONE);
	if (fnum == -1) {
		printf("file open failed - %s\n", smbcli_errstr(cli->tree));
	}
	dnum = smbcli_nt_create_full(cli->tree, "\\", 
				     0, 
				     SEC_RIGHTS_FILE_READ, 
				     FILE_ATTRIBUTE_NORMAL,
				     NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE, 
				     NTCREATEX_DISP_OPEN, 
				     NTCREATEX_OPTIONS_DIRECTORY, 0);
	if (dnum == -1) {
		printf("directory open failed - %s\n", smbcli_errstr(cli->tree));
	}
	qfnum = smbcli_nt_create_full(cli->tree, "\\$Extend\\$Quota:$Q:$INDEX_ALLOCATION", 
				   NTCREATEX_FLAGS_EXTENDED, 
				   SEC_FLAG_MAXIMUM_ALLOWED, 
				   0,
				   NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE, 
				   NTCREATEX_DISP_OPEN, 
				   0, 0);
	if (qfnum == -1) {
		printf("quota open failed - %s\n", smbcli_errstr(cli->tree));
	}

	for (op=OP_MIN; op<=OP_MAX; op++) {

		if (!trans2_op_exists(cli, op)) {
			continue;
		}

		for (level = 0; level <= 50; level++) {
			scan_trans2(cli, op, level, fnum, dnum, qfnum, fname);
		}

		for (level = 0x100; level <= 0x130; level++) {
			scan_trans2(cli, op, level, fnum, dnum, qfnum, fname);
		}

		for (level = 1000; level < 1050; level++) {
			scan_trans2(cli, op, level, fnum, dnum, qfnum, fname);
		}
	}

	return true;
}




/****************************************************************************
look for a partial hit
****************************************************************************/
static void nttrans_check_hit(const char *format, int op, int level, NTSTATUS status)
{
	if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_LEVEL) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_UNSUCCESSFUL) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_INVALID_INFO_CLASS)) {
		return;
	}
#if VERBOSE
		printf("possible %s hit op=%3d level=%5d status=%s\n",
		       format, op, level, nt_errstr(status));
#endif
}

/****************************************************************************
check for existence of a nttrans call
****************************************************************************/
static NTSTATUS try_nttrans(struct smbcli_state *cli, 
			    int op,
			    uint8_t *param, uint8_t *data,
			    int param_len, int data_len,
			    int *rparam_len, int *rdata_len)
{
	struct smb_nttrans parms;
	DATA_BLOB ntparam_blob, ntdata_blob;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("try_nttrans");

	ntparam_blob.length = param_len;
	ntparam_blob.data = param;
	ntdata_blob.length = data_len;
	ntdata_blob.data = data;

	parms.in.max_param = UINT32_MAX;
	parms.in.max_data = UINT32_MAX;
	parms.in.max_setup = 0;
	parms.in.setup_count = 0;
	parms.in.function = op;
	parms.in.params = ntparam_blob;
	parms.in.data = ntdata_blob;
	
	status = smb_raw_nttrans(cli->tree, mem_ctx, &parms);
	
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1,("Failed to send NT_TRANS\n"));
		talloc_free(mem_ctx);
		return status;
	}
	*rparam_len = parms.out.params.length;
	*rdata_len = parms.out.data.length;

	talloc_free(mem_ctx);

	return status;
}


static NTSTATUS try_nttrans_len(struct smbcli_state *cli,
			     const char *format,
			     int op, int level,
			     uint8_t *param, uint8_t *data,
			     int param_len, int *data_len,
			     int *rparam_len, int *rdata_len)
{
	NTSTATUS ret=NT_STATUS_OK;

	ret = try_nttrans(cli, op, param, data, param_len,
			 PARAM_SIZE, rparam_len, rdata_len);
#if VERBOSE
	printf("op=%d level=%d ret=%s\n", op, level, nt_errstr(ret));
#endif
	if (!NT_STATUS_IS_OK(ret)) return ret;

	*data_len = 0;
	while (*data_len < PARAM_SIZE) {
		ret = try_nttrans(cli, op, param, data, param_len,
				 *data_len, rparam_len, rdata_len);
		if (NT_STATUS_IS_OK(ret)) break;
		*data_len += 2;
	}
	if (NT_STATUS_IS_OK(ret)) {
		printf("found %s level=%d data_len=%d rparam_len=%d rdata_len=%d\n",
		       format, level, *data_len, *rparam_len, *rdata_len);
	} else {
		nttrans_check_hit(format, op, level, ret);
	}
	return ret;
}

/****************************************************************************
check for existance of a nttrans call
****************************************************************************/
static bool scan_nttrans(struct smbcli_state *cli, int op, int level,
			int fnum, int dnum, const char *fname)
{
	int data_len = 0;
	int param_len = 0;
	int rparam_len, rdata_len;
	uint8_t *param, *data;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("scan_nttrans");

	param = talloc_array(mem_ctx, uint8_t, PARAM_SIZE);
	data = talloc_array(mem_ctx, uint8_t, PARAM_SIZE);
	memset(data, 0, PARAM_SIZE);
	data_len = 4;

	/* try with a info level only */
	param_len = 2;
	SSVAL(param, 0, level);
	status = try_nttrans_len(cli, "void", op, level, param, data, param_len,
			&data_len, &rparam_len, &rdata_len);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try with a file descriptor */
	param_len = 6;
	SSVAL(param, 0, fnum);
	SSVAL(param, 2, level);
	SSVAL(param, 4, 0);
	status = try_nttrans_len(cli, "fnum", op, level, param, data, param_len,
			&data_len, &rparam_len, &rdata_len);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try with a notify style */
	param_len = 6;
	SSVAL(param, 0, dnum);
	SSVAL(param, 2, dnum);
	SSVAL(param, 4, level);
	status = try_nttrans_len(cli, "notify", op, level, param, data,
			param_len, &data_len, &rparam_len, &rdata_len);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try with a file name */
	param_len = 6;
	SSVAL(param, 0, level);
	SSVAL(param, 2, 0);
	SSVAL(param, 4, 0);
	param_len += push_string(
			&param[6], fname, PARAM_SIZE,
			STR_TERMINATE | STR_UNICODE);

	status = try_nttrans_len(cli, "fname", op, level, param, data,
			param_len, &data_len, &rparam_len, &rdata_len);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try with a new file name */
	param_len = 6;
	SSVAL(param, 0, level);
	SSVAL(param, 2, 0);
	SSVAL(param, 4, 0);
	param_len += push_string(
			&param[6], "\\newfile.dat", PARAM_SIZE,
			STR_TERMINATE | STR_UNICODE);

	status = try_nttrans_len(cli, "newfile", op, level, param, data,
			param_len, &data_len, &rparam_len, &rdata_len);
	smbcli_unlink(cli->tree, "\\newfile.dat");
	smbcli_rmdir(cli->tree, "\\newfile.dat");
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	/* try dfs style  */
	smbcli_mkdir(cli->tree, "\\testdir");
	param_len = 2;
	SSVAL(param, 0, level);
	param_len += push_string(&param[2], "\\testdir", PARAM_SIZE,
			STR_TERMINATE | STR_UNICODE);

	status = try_nttrans_len(cli, "dfs", op, level, param, data, param_len,
			&data_len, &rparam_len, &rdata_len);
	smbcli_rmdir(cli->tree, "\\testdir");
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return true;
	}

	talloc_free(mem_ctx);
	return false;
}


bool torture_nttrans_scan(struct torture_context *torture, 
			  struct smbcli_state *cli)
{
	int op, level;
	const char *fname = "\\scanner.dat";
	int fnum, dnum;

	fnum = smbcli_open(cli->tree, fname, O_RDWR | O_CREAT | O_TRUNC, 
			 DENY_NONE);
	dnum = smbcli_open(cli->tree, "\\", O_RDONLY, DENY_NONE);

	for (op=OP_MIN; op<=OP_MAX; op++) {
		printf("Scanning op=%d\n", op);
		for (level = 0; level <= 50; level++) {
			scan_nttrans(cli, op, level, fnum, dnum, fname);
		}

		for (level = 0x100; level <= 0x130; level++) {
			scan_nttrans(cli, op, level, fnum, dnum, fname);
		}

		for (level = 1000; level < 1050; level++) {
			scan_nttrans(cli, op, level, fnum, dnum, fname);
		}
	}

	printf("nttrans scan finished\n");
	return true;
}


/* scan for valid base SMB requests */
bool torture_smb_scan(struct torture_context *torture)
{
	static struct smbcli_state *cli;
	int op;
	struct smbcli_request *req;
	NTSTATUS status;

	for (op=0x0;op<=0xFF;op++) {
		if (op == SMBreadbraw) continue;

		if (!torture_open_connection(&cli, torture, 0)) {
			return false;
		}

		req = smbcli_request_setup(cli->tree, op, 0, 0);

		if (!smbcli_request_send(req)) {
			smbcli_request_destroy(req);
			break;
		}

		usleep(10000);
		smbcli_transport_process(cli->transport);
		if (req->state > SMBCLI_REQUEST_RECV) {
			status = smbcli_request_simple_recv(req);
			printf("op=0x%x status=%s\n", op, nt_errstr(status));
			torture_close_connection(cli);
			continue;
		}

		sleep(1);
		smbcli_transport_process(cli->transport);
		if (req->state > SMBCLI_REQUEST_RECV) {
			status = smbcli_request_simple_recv(req);
			printf("op=0x%x status=%s\n", op, nt_errstr(status));
		} else {
			printf("op=0x%x no reply\n", op);
			smbcli_request_destroy(req);
			continue; /* don't attempt close! */
		}

		torture_close_connection(cli);
	}


	printf("smb scan finished\n");
	return true;
}
