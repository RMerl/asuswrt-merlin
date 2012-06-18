/* 
   Unix SMB/CIFS implementation.
   client security descriptor functions
   Copyright (C) Andrew Tridgell 2000
   
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

/****************************************************************************
  query the security descriptor for a open file
 ****************************************************************************/
SEC_DESC *cli_query_secdesc(struct cli_state *cli, uint16_t fnum, 
			    TALLOC_CTX *mem_ctx)
{
	uint8_t param[8];
	uint8_t *rparam=NULL, *rdata=NULL;
	unsigned int rparam_count=0, rdata_count=0;
	SEC_DESC *psd = NULL;
	NTSTATUS status;

	SIVAL(param, 0, fnum);
	SIVAL(param, 4, 0x7);

	status = cli_trans(talloc_tos(), cli, SMBnttrans,
			   NULL, -1, /* name, fid */
			   NT_TRANSACT_QUERY_SECURITY_DESC, 0, /* function, flags */
			   NULL, 0, 0, /* setup, length, max */
			   param, 8, 4, /* param, length, max */
			   NULL, 0, 0x10000, /* data, length, max */
			   NULL, NULL, /* rsetup, length */
			   &rparam, &rparam_count,
			   &rdata, &rdata_count);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("NT_TRANSACT_QUERY_SECURITY_DESC failed: %s\n",
			  nt_errstr(status)));
		goto cleanup;
	}

	status = unmarshall_sec_desc(mem_ctx, (uint8 *)rdata, rdata_count,
				     &psd);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("unmarshall_sec_desc failed: %s\n",
			   nt_errstr(status)));
		goto cleanup;
	}

 cleanup:

	TALLOC_FREE(rparam);
	TALLOC_FREE(rdata);

	return psd;
}

/****************************************************************************
  set the security descriptor for a open file
 ****************************************************************************/
bool cli_set_secdesc(struct cli_state *cli, uint16_t fnum, SEC_DESC *sd)
{
	char param[8];
	char *rparam=NULL, *rdata=NULL;
	unsigned int rparam_count=0, rdata_count=0;
	uint32 sec_info = 0;
	TALLOC_CTX *frame = talloc_stackframe();
	bool ret = False;
	uint8 *data;
	size_t len;
	NTSTATUS status;

	status = marshall_sec_desc(talloc_tos(), sd, &data, &len);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("marshall_sec_desc failed: %s\n",
			   nt_errstr(status)));
		goto cleanup;
	}

	SIVAL(param, 0, fnum);

	if (sd->dacl)
		sec_info |= DACL_SECURITY_INFORMATION;
	if (sd->owner_sid)
		sec_info |= OWNER_SECURITY_INFORMATION;
	if (sd->group_sid)
		sec_info |= GROUP_SECURITY_INFORMATION;
	SSVAL(param, 4, sec_info);

	if (!cli_send_nt_trans(cli, 
			       NT_TRANSACT_SET_SECURITY_DESC, 
			       0, 
			       NULL, 0, 0,
			       param, 8, 0,
			       (char *)data, len, 0)) {
		DEBUG(1,("Failed to send NT_TRANSACT_SET_SECURITY_DESC\n"));
		goto cleanup;
	}


	if (!cli_receive_nt_trans(cli, 
				  &rparam, &rparam_count,
				  &rdata, &rdata_count)) {
		DEBUG(1,("NT_TRANSACT_SET_SECURITY_DESC failed\n"));
		goto cleanup;
	}

	ret = True;

  cleanup:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	TALLOC_FREE(frame);

	return ret;
}
