/* 
   Unix SMB/CIFS implementation.
   client quota functions
   Copyright (C) Stefan (metze) Metzmacher	2003

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
#include "libsmb/libsmb.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "fake_file.h"
#include "../libcli/security/security.h"
#include "trans2.h"

NTSTATUS cli_get_quota_handle(struct cli_state *cli, uint16_t *quota_fnum)
{
	return cli_ntcreate(cli, FAKE_FILE_NAME_QUOTA_WIN32,
		 0x00000016, DESIRED_ACCESS_PIPE,
		 0x00000000, FILE_SHARE_READ|FILE_SHARE_WRITE,
		 FILE_OPEN, 0x00000000, 0x03, quota_fnum);
}

void free_ntquota_list(SMB_NTQUOTA_LIST **qt_list)
{
	if (!qt_list)
		return;

	if ((*qt_list)->mem_ctx)
		talloc_destroy((*qt_list)->mem_ctx);

	(*qt_list) = NULL;

	return;	
}

static bool parse_user_quota_record(const uint8_t *rdata,
				    unsigned int rdata_count,
				    unsigned int *offset,
				    SMB_NTQUOTA_STRUCT *pqt)
{
	int sid_len;
	SMB_NTQUOTA_STRUCT qt;

	ZERO_STRUCT(qt);

	if (!rdata||!offset||!pqt) {
		smb_panic("parse_quota_record: called with NULL POINTER!");
	}

	if (rdata_count < 40) {
		return False;
	}

	/* offset to next quota record.
	 * 4 bytes IVAL(rdata,0)
	 * unused here...
	 */
	*offset = IVAL(rdata,0);

	/* sid len */
	sid_len = IVAL(rdata,4);

	if (rdata_count < 40+sid_len) {
		return False;		
	}

	/* unknown 8 bytes in pdata 
	 * maybe its the change time in NTTIME
	 */

	/* the used space 8 bytes (uint64_t)*/
	qt.usedspace = BVAL(rdata,16);

	/* the soft quotas 8 bytes (uint64_t)*/
	qt.softlim = BVAL(rdata,24);

	/* the hard quotas 8 bytes (uint64_t)*/
	qt.hardlim = BVAL(rdata,32);

	if (!sid_parse((char *)rdata+40,sid_len,&qt.sid)) {
		return false;
	}

	qt.qtype = SMB_USER_QUOTA_TYPE;

	*pqt = qt;

	return True;
}

NTSTATUS cli_get_user_quota(struct cli_state *cli, int quota_fnum,
			    SMB_NTQUOTA_STRUCT *pqt)
{
	uint16_t setup[1];
	uint8_t params[16];
	unsigned int data_len;
	uint8_t data[SID_MAX_SIZE+8];
	uint8_t *rparam, *rdata;
	uint32_t rparam_count, rdata_count;
	unsigned int sid_len;
	unsigned int offset;
	NTSTATUS status;

	if (!cli||!pqt) {
		smb_panic("cli_get_user_quota() called with NULL Pointer!");
	}

	SSVAL(setup + 0, 0, NT_TRANSACT_GET_USER_QUOTA);

	SSVAL(params, 0,quota_fnum);
	SSVAL(params, 2,TRANSACT_GET_USER_QUOTA_FOR_SID);
	SIVAL(params, 4,0x00000024);
	SIVAL(params, 8,0x00000000);
	SIVAL(params,12,0x00000024);

	sid_len = ndr_size_dom_sid(&pqt->sid, 0);
	data_len = sid_len+8;
	SIVAL(data, 0, 0x00000000);
	SIVAL(data, 4, sid_len);
	sid_linearize((char *)data+8, sid_len, &pqt->sid);

	status = cli_trans(talloc_tos(), cli, SMBnttrans,
			   NULL, -1, /* name, fid */
			   NT_TRANSACT_GET_USER_QUOTA, 0,
			   setup, 1, 0, /* setup */
			   params, 16, 4, /* params */
			   data, data_len, 112, /* data */
			   NULL,		/* recv_flags2 */
			   NULL, 0, NULL,	/* rsetup */
			   &rparam, 4, &rparam_count,
			   &rdata, 8, &rdata_count);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("NT_TRANSACT_GET_USER_QUOTA failed: %s\n",
			  nt_errstr(status)));
		return status;
	}

	if (!parse_user_quota_record(rdata, rdata_count, &offset, pqt)) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		DEBUG(0,("Got INVALID NT_TRANSACT_GET_USER_QUOTA reply.\n"));
	}

	TALLOC_FREE(rparam);
	TALLOC_FREE(rdata);
	return status;
}

NTSTATUS cli_set_user_quota(struct cli_state *cli, int quota_fnum,
			    SMB_NTQUOTA_STRUCT *pqt)
{
	uint16_t setup[1];
	uint8_t params[2];
	uint8_t data[112];
	unsigned int sid_len;	
	NTSTATUS status;

	memset(data,'\0',112);

	if (!cli||!pqt) {
		smb_panic("cli_set_user_quota() called with NULL Pointer!");
	}

	SSVAL(setup + 0, 0, NT_TRANSACT_SET_USER_QUOTA);

	SSVAL(params,0,quota_fnum);

	sid_len = ndr_size_dom_sid(&pqt->sid, 0);
	SIVAL(data,0,0);
	SIVAL(data,4,sid_len);
	SBIG_UINT(data, 8,(uint64_t)0);
	SBIG_UINT(data,16,pqt->usedspace);
	SBIG_UINT(data,24,pqt->softlim);
	SBIG_UINT(data,32,pqt->hardlim);
	sid_linearize((char *)data+40, sid_len, &pqt->sid);

	status = cli_trans(talloc_tos(), cli, SMBnttrans,
			   NULL, -1, /* name, fid */
			   NT_TRANSACT_SET_USER_QUOTA, 0,
			   setup, 1, 0, /* setup */
			   params, 2, 0, /* params */
			   data, 112, 0, /* data */
			   NULL,		/* recv_flags2 */
			   NULL, 0, NULL,	/* rsetup */
			   NULL, 0, NULL,	/* rparams */
			   NULL, 0, NULL);	/* rdata */

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("NT_TRANSACT_SET_USER_QUOTA failed: %s\n",
			  nt_errstr(status)));
	}

	return status;
}

NTSTATUS cli_list_user_quota(struct cli_state *cli, int quota_fnum,
			     SMB_NTQUOTA_LIST **pqt_list)
{
	uint16_t setup[1];
	uint8_t params[16];
	uint8_t *rparam=NULL, *rdata=NULL;
	uint32_t rparam_count=0, rdata_count=0;
	unsigned int offset;
	const uint8_t *curdata = NULL;
	unsigned int curdata_count = 0;
	TALLOC_CTX *mem_ctx = NULL;
	SMB_NTQUOTA_STRUCT qt;
	SMB_NTQUOTA_LIST *tmp_list_ent;
	NTSTATUS status;

	if (!cli||!pqt_list) {
		smb_panic("cli_list_user_quota() called with NULL Pointer!");
	}

	SSVAL(setup + 0, 0, NT_TRANSACT_GET_USER_QUOTA);

	SSVAL(params, 0,quota_fnum);
	SSVAL(params, 2,TRANSACT_GET_USER_QUOTA_LIST_START);
	SIVAL(params, 4,0x00000000);
	SIVAL(params, 8,0x00000000);
	SIVAL(params,12,0x00000000);

	status = cli_trans(talloc_tos(), cli, SMBnttrans,
			   NULL, -1, /* name, fid */
			   NT_TRANSACT_GET_USER_QUOTA, 0,
			   setup, 1, 0, /* setup */
			   params, 16, 4, /* params */
			   NULL, 0, 2048, /* data */
			   NULL,		/* recv_flags2 */
			   NULL, 0, NULL,	/* rsetup */
			   &rparam, 0, &rparam_count,
			   &rdata, 0, &rdata_count);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("NT_TRANSACT_GET_USER_QUOTA failed: %s\n",
			  nt_errstr(status)));
		goto cleanup;
	}

	if (rdata_count == 0) {
		*pqt_list = NULL;
		return NT_STATUS_OK;
	}

	if ((mem_ctx=talloc_init("SMB_USER_QUOTA_LIST"))==NULL) {
		DEBUG(0,("talloc_init() failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	offset = 1;
	for (curdata=rdata,curdata_count=rdata_count;
		((curdata)&&(curdata_count>=8)&&(offset>0));
		curdata +=offset,curdata_count -= offset) {
		ZERO_STRUCT(qt);
		if (!parse_user_quota_record((uint8_t *)curdata, curdata_count,
					     &offset, &qt)) {
			DEBUG(1,("Failed to parse the quota record\n"));
			goto cleanup;
		}

		if ((tmp_list_ent=TALLOC_ZERO_P(mem_ctx,SMB_NTQUOTA_LIST))==NULL) {
			DEBUG(0,("TALLOC_ZERO() failed\n"));
			talloc_destroy(mem_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		if ((tmp_list_ent->quotas=TALLOC_ZERO_P(mem_ctx,SMB_NTQUOTA_STRUCT))==NULL) {
			DEBUG(0,("TALLOC_ZERO() failed\n"));
			talloc_destroy(mem_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		memcpy(tmp_list_ent->quotas,&qt,sizeof(qt));
		tmp_list_ent->mem_ctx = mem_ctx;		

		DLIST_ADD((*pqt_list),tmp_list_ent);
	}

	SSVAL(params, 2,TRANSACT_GET_USER_QUOTA_LIST_CONTINUE);	
	while(1) {

		TALLOC_FREE(rparam);
		TALLOC_FREE(rdata);

		status = cli_trans(talloc_tos(), cli, SMBnttrans,
				   NULL, -1, /* name, fid */
				   NT_TRANSACT_GET_USER_QUOTA, 0,
				   setup, 1, 0, /* setup */
				   params, 16, 4, /* params */
				   NULL, 0, 2048, /* data */
				   NULL,		/* recv_flags2 */
				   NULL, 0, NULL,	/* rsetup */
				   &rparam, 0, &rparam_count,
				   &rdata, 0, &rdata_count);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("NT_TRANSACT_GET_USER_QUOTA failed: %s\n",
				  nt_errstr(status)));
			goto cleanup;
		}

		if (rdata_count == 0) {
			break;
		}

		offset = 1;
		for (curdata=rdata,curdata_count=rdata_count;
			((curdata)&&(curdata_count>=8)&&(offset>0));
			curdata +=offset,curdata_count -= offset) {
			ZERO_STRUCT(qt);
			if (!parse_user_quota_record((uint8_t *)curdata,
						     curdata_count, &offset,
						     &qt)) {
				DEBUG(1,("Failed to parse the quota record\n"));
				goto cleanup;
			}

			if ((tmp_list_ent=TALLOC_ZERO_P(mem_ctx,SMB_NTQUOTA_LIST))==NULL) {
				DEBUG(0,("TALLOC_ZERO() failed\n"));
				talloc_destroy(mem_ctx);
				goto cleanup;
			}

			if ((tmp_list_ent->quotas=TALLOC_ZERO_P(mem_ctx,SMB_NTQUOTA_STRUCT))==NULL) {
				DEBUG(0,("TALLOC_ZERO() failed\n"));
				talloc_destroy(mem_ctx);
				goto cleanup;
			}

			memcpy(tmp_list_ent->quotas,&qt,sizeof(qt));
			tmp_list_ent->mem_ctx = mem_ctx;		

			DLIST_ADD((*pqt_list),tmp_list_ent);
		}
	}

 cleanup:
	TALLOC_FREE(rparam);
	TALLOC_FREE(rdata);

	return status;
}

NTSTATUS cli_get_fs_quota_info(struct cli_state *cli, int quota_fnum,
			       SMB_NTQUOTA_STRUCT *pqt)
{
	uint16_t setup[1];
	uint8_t param[2];
	uint8_t *rdata=NULL;
	uint32_t rdata_count=0;
	SMB_NTQUOTA_STRUCT qt;
	NTSTATUS status;

	ZERO_STRUCT(qt);

	if (!cli||!pqt) {
		smb_panic("cli_get_fs_quota_info() called with NULL Pointer!");
	}

	SSVAL(setup + 0, 0, TRANSACT2_QFSINFO);

	SSVAL(param,0,SMB_FS_QUOTA_INFORMATION);

	status = cli_trans(talloc_tos(), cli, SMBtrans2,
			   NULL, -1, /* name, fid */
			   0, 0,     /* function, flags */
			   setup, 1, 0, /* setup */
			   param, 2, 0, /* param */
			   NULL, 0, 560, /* data */
			   NULL,	 /* recv_flags2 */
			   NULL, 0, NULL, /* rsetup */
			   NULL, 0, NULL, /* rparam */
			   &rdata, 48, &rdata_count);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("SMB_FS_QUOTA_INFORMATION failed: %s\n",
			  nt_errstr(status)));
		return status;
	}

	/* unknown_1 24 NULL bytes in pdata*/

	/* the soft quotas 8 bytes (uint64_t)*/
	qt.softlim = BVAL(rdata,24);

	/* the hard quotas 8 bytes (uint64_t)*/
	qt.hardlim = BVAL(rdata,32);

	/* quota_flags 2 bytes **/
	qt.qflags = SVAL(rdata,40);

	qt.qtype = SMB_USER_FS_QUOTA_TYPE;

	*pqt = qt;

	TALLOC_FREE(rdata);
	return status;
}

NTSTATUS cli_set_fs_quota_info(struct cli_state *cli, int quota_fnum,
			       SMB_NTQUOTA_STRUCT *pqt)
{
	uint16_t setup[1];
	uint8_t param[4];
	uint8_t data[48];
	SMB_NTQUOTA_STRUCT qt;
	NTSTATUS status;
	ZERO_STRUCT(qt);
	memset(data,'\0',48);

	if (!cli||!pqt) {
		smb_panic("cli_set_fs_quota_info() called with NULL Pointer!");
	}

	SSVAL(setup + 0, 0,TRANSACT2_SETFSINFO);

	SSVAL(param,0,quota_fnum);
	SSVAL(param,2,SMB_FS_QUOTA_INFORMATION);

	/* Unknown1 24 NULL bytes*/

	/* Default Soft Quota 8 bytes */
	SBIG_UINT(data,24,pqt->softlim);

	/* Default Hard Quota 8 bytes */
	SBIG_UINT(data,32,pqt->hardlim);

	/* Quota flag 2 bytes */
	SSVAL(data,40,pqt->qflags);

	/* Unknown3 6 NULL bytes */

	status = cli_trans(talloc_tos(), cli, SMBtrans2,
			   NULL, -1, /* name, fid */
			   0, 0,     /* function, flags */
			   setup, 1, 0, /* setup */
			   param, 8, 0, /* param */
			   data, 48, 0, /* data */
			   NULL,	 /* recv_flags2 */
			   NULL, 0, NULL, /* rsetup */
			   NULL, 0, NULL, /* rparam */
			   NULL, 0, NULL); /* rdata */

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("SMB_FS_QUOTA_INFORMATION failed: %s\n",
			  nt_errstr(status)));
	}

	return status;
}
