/*
   Unix SMB/CIFS implementation.
   client transaction calls
   Copyright (C) Andrew Tridgell 1994-1998

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
 Send a SMB trans or trans2 request.
****************************************************************************/

bool cli_send_trans(struct cli_state *cli, int trans,
		    const char *pipe_name,
		    int fid, int flags,
		    uint16 *setup, unsigned int lsetup, unsigned int msetup,
		    const char *param, unsigned int lparam, unsigned int mparam,
		    const char *data, unsigned int ldata, unsigned int mdata)
{
	unsigned int i;
	unsigned int this_ldata,this_lparam;
	unsigned int tot_data=0,tot_param=0;
	char *outdata,*outparam;
	char *p;
	int pipe_name_len=0;
	uint16 mid;

	this_lparam = MIN(lparam,cli->max_xmit - (500+lsetup*2)); /* hack */
	this_ldata = MIN(ldata,cli->max_xmit - (500+lsetup*2+this_lparam));

	memset(cli->outbuf,'\0',smb_size);
	cli_set_message(cli->outbuf,14+lsetup,0,True);
	SCVAL(cli->outbuf,smb_com,trans);
	SSVAL(cli->outbuf,smb_tid, cli->cnum);
	cli_setup_packet(cli);

	/*
	 * Save the mid we're using. We need this for finding
	 * signing replies.
	 */

	mid = cli->mid;

	if (pipe_name) {
		pipe_name_len = clistr_push(cli, smb_buf(cli->outbuf), pipe_name, -1, STR_TERMINATE);
	}

	outparam = smb_buf(cli->outbuf)+(trans==SMBtrans ? pipe_name_len : 3);
	outdata = outparam+this_lparam;

	/* primary request */
	SSVAL(cli->outbuf,smb_tpscnt,lparam);	/* tpscnt */
	SSVAL(cli->outbuf,smb_tdscnt,ldata);	/* tdscnt */
	SSVAL(cli->outbuf,smb_mprcnt,mparam);	/* mprcnt */
	SSVAL(cli->outbuf,smb_mdrcnt,mdata);	/* mdrcnt */
	SCVAL(cli->outbuf,smb_msrcnt,msetup);	/* msrcnt */
	SSVAL(cli->outbuf,smb_flags,flags);	/* flags */
	SIVAL(cli->outbuf,smb_timeout,0);		/* timeout */
	SSVAL(cli->outbuf,smb_pscnt,this_lparam);	/* pscnt */
	SSVAL(cli->outbuf,smb_psoff,smb_offset(outparam,cli->outbuf)); /* psoff */
	SSVAL(cli->outbuf,smb_dscnt,this_ldata);	/* dscnt */
	SSVAL(cli->outbuf,smb_dsoff,smb_offset(outdata,cli->outbuf)); /* dsoff */
	SCVAL(cli->outbuf,smb_suwcnt,lsetup);	/* suwcnt */
	for (i=0;i<lsetup;i++)		/* setup[] */
		SSVAL(cli->outbuf,smb_setup+i*2,setup[i]);
	p = smb_buf(cli->outbuf);
	if (trans != SMBtrans) {
		*p++ = 0;  /* put in a null smb_name */
		*p++ = 'D'; *p++ = ' ';	/* observed in OS/2 */
	}
	if (this_lparam)			/* param[] */
		memcpy(outparam,param,this_lparam);
	if (this_ldata)			/* data[] */
		memcpy(outdata,data,this_ldata);
	cli_setup_bcc(cli, outdata+this_ldata);

	show_msg(cli->outbuf);

	if (!cli_send_smb(cli)) {
		return False;
	}

	cli_state_seqnum_persistent(cli, mid);

	if (this_ldata < ldata || this_lparam < lparam) {
		/* receive interim response */
		if (!cli_receive_smb(cli) || cli_is_error(cli)) {
			cli_state_seqnum_remove(cli, mid);
			return(False);
		}

		tot_data = this_ldata;
		tot_param = this_lparam;

		while (tot_data < ldata || tot_param < lparam)  {
			this_lparam = MIN(lparam-tot_param,cli->max_xmit - 500); /* hack */
			this_ldata = MIN(ldata-tot_data,cli->max_xmit - (500+this_lparam));

			cli_set_message(cli->outbuf,trans==SMBtrans?8:9,0,True);
			SCVAL(cli->outbuf,smb_com,(trans==SMBtrans ? SMBtranss : SMBtranss2));

			outparam = smb_buf(cli->outbuf);
			outdata = outparam+this_lparam;

			/* secondary request */
			SSVAL(cli->outbuf,smb_tpscnt,lparam);	/* tpscnt */
			SSVAL(cli->outbuf,smb_tdscnt,ldata);	/* tdscnt */
			SSVAL(cli->outbuf,smb_spscnt,this_lparam);	/* pscnt */
			SSVAL(cli->outbuf,smb_spsoff,smb_offset(outparam,cli->outbuf)); /* psoff */
			SSVAL(cli->outbuf,smb_spsdisp,tot_param);	/* psdisp */
			SSVAL(cli->outbuf,smb_sdscnt,this_ldata);	/* dscnt */
			SSVAL(cli->outbuf,smb_sdsoff,smb_offset(outdata,cli->outbuf)); /* dsoff */
			SSVAL(cli->outbuf,smb_sdsdisp,tot_data);	/* dsdisp */
			if (trans==SMBtrans2)
				SSVALS(cli->outbuf,smb_sfid,fid);		/* fid */
			if (this_lparam)			/* param[] */
				memcpy(outparam,param+tot_param,this_lparam);
			if (this_ldata)			/* data[] */
				memcpy(outdata,data+tot_data,this_ldata);
			cli_setup_bcc(cli, outdata+this_ldata);

			show_msg(cli->outbuf);

			cli->mid = mid;
			if (!cli_send_smb(cli)) {
				cli_state_seqnum_remove(cli, mid);
				return False;
			}

			tot_data += this_ldata;
			tot_param += this_lparam;
		}
	}

	return(True);
}

/****************************************************************************
 Receive a SMB trans or trans2 response allocating the necessary memory.
****************************************************************************/

bool cli_receive_trans(struct cli_state *cli,int trans,
                              char **param, unsigned int *param_len,
                              char **data, unsigned int *data_len)
{
	unsigned int total_data=0;
	unsigned int total_param=0;
	unsigned int this_data,this_param;
	NTSTATUS status;
	bool ret = False;
	uint16_t mid;

	*data_len = *param_len = 0;

	mid = SVAL(cli->outbuf,smb_mid);

	if (!cli_receive_smb(cli)) {
		cli_state_seqnum_remove(cli, mid);
		return False;
	}

	show_msg(cli->inbuf);

	/* sanity check */
	if (CVAL(cli->inbuf,smb_com) != trans) {
		DEBUG(0,("Expected %s response, got command 0x%02x\n",
			 trans==SMBtrans?"SMBtrans":"SMBtrans2",
			 CVAL(cli->inbuf,smb_com)));
		cli_state_seqnum_remove(cli, mid);
		return False;
	}

	/*
	 * An NT RPC pipe call can return ERRDOS, ERRmoredata
	 * to a trans call. This is not an error and should not
	 * be treated as such. Note that STATUS_NO_MORE_FILES is
	 * returned when a trans2 findfirst/next finishes.
	 * When setting up an encrypted transport we can also
	 * see NT_STATUS_MORE_PROCESSING_REQUIRED here.
         *
         * Vista returns NT_STATUS_INACCESSIBLE_SYSTEM_SHORTCUT if the folder
         * "<share>/Users/All Users" is enumerated.  This is a special pseudo
         * folder, and the response does not have parameters (nor a parameter
         * length).
	 */
	status = cli_nt_error(cli);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		if (NT_STATUS_IS_ERR(status) ||
                    NT_STATUS_EQUAL(status,STATUS_NO_MORE_FILES) ||
                    NT_STATUS_EQUAL(status,NT_STATUS_INACCESSIBLE_SYSTEM_SHORTCUT)) {
			goto out;
		}
	}

	/* parse out the lengths */
	total_data = SVAL(cli->inbuf,smb_tdrcnt);
	total_param = SVAL(cli->inbuf,smb_tprcnt);

	/* allocate it */
	if (total_data!=0) {
		/* We know adding 2 is safe as total_data is an
		 * SVAL <= 0xFFFF. */
		*data = (char *)SMB_REALLOC(*data,total_data+2);
		if (!(*data)) {
			DEBUG(0,("cli_receive_trans: failed to enlarge data buffer\n"));
			goto out;
		}
	}

	if (total_param!=0) {
		/* We know adding 2 is safe as total_param is an
		 * SVAL <= 0xFFFF. */
		*param = (char *)SMB_REALLOC(*param,total_param+2);
		if (!(*param)) {
			DEBUG(0,("cli_receive_trans: failed to enlarge param buffer\n"));
			goto out;
		}
	}

	for (;;)  {
		this_data = SVAL(cli->inbuf,smb_drcnt);
		this_param = SVAL(cli->inbuf,smb_prcnt);

		if (this_data + *data_len > total_data ||
		    this_param + *param_len > total_param) {
			DEBUG(1,("Data overflow in cli_receive_trans\n"));
			goto out;
		}

		if (this_data + *data_len < this_data ||
				this_data + *data_len < *data_len ||
				this_param + *param_len < this_param ||
				this_param + *param_len < *param_len) {
			DEBUG(1,("Data overflow in cli_receive_trans\n"));
			goto out;
		}

		if (this_data) {
			unsigned int data_offset_out = SVAL(cli->inbuf,smb_drdisp);
			unsigned int data_offset_in = SVAL(cli->inbuf,smb_droff);

			if (data_offset_out > total_data ||
					data_offset_out + this_data > total_data ||
					data_offset_out + this_data < data_offset_out ||
					data_offset_out + this_data < this_data) {
				DEBUG(1,("Data overflow in cli_receive_trans\n"));
				goto out;
			}
			if (data_offset_in > cli->bufsize ||
					data_offset_in + this_data >  cli->bufsize ||
					data_offset_in + this_data < data_offset_in ||
					data_offset_in + this_data < this_data) {
				DEBUG(1,("Data overflow in cli_receive_trans\n"));
				goto out;
			}

			memcpy(*data + data_offset_out, smb_base(cli->inbuf) + data_offset_in, this_data);
		}
		if (this_param) {
			unsigned int param_offset_out = SVAL(cli->inbuf,smb_prdisp);
			unsigned int param_offset_in = SVAL(cli->inbuf,smb_proff);

			if (param_offset_out > total_param ||
					param_offset_out + this_param > total_param ||
					param_offset_out + this_param < param_offset_out ||
					param_offset_out + this_param < this_param) {
				DEBUG(1,("Param overflow in cli_receive_trans\n"));
				goto out;
			}
			if (param_offset_in > cli->bufsize ||
					param_offset_in + this_param >  cli->bufsize ||
					param_offset_in + this_param < param_offset_in ||
					param_offset_in + this_param < this_param) {
				DEBUG(1,("Param overflow in cli_receive_trans\n"));
				goto out;
			}

			memcpy(*param + param_offset_out, smb_base(cli->inbuf) + param_offset_in, this_param);
		}
		*data_len += this_data;
		*param_len += this_param;

		if (total_data <= *data_len && total_param <= *param_len) {
			ret = True;
			break;
		}

		if (!cli_receive_smb(cli)) {
			goto out;
		}

		show_msg(cli->inbuf);

		/* sanity check */
		if (CVAL(cli->inbuf,smb_com) != trans) {
			DEBUG(0,("Expected %s response, got command 0x%02x\n",
				 trans==SMBtrans?"SMBtrans":"SMBtrans2", 
				 CVAL(cli->inbuf,smb_com)));
			goto out;
		}
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			if (NT_STATUS_IS_ERR(cli_nt_error(cli))) {
				goto out;
			}
		}

		/* parse out the total lengths again - they can shrink! */
		if (SVAL(cli->inbuf,smb_tdrcnt) < total_data)
			total_data = SVAL(cli->inbuf,smb_tdrcnt);
		if (SVAL(cli->inbuf,smb_tprcnt) < total_param)
			total_param = SVAL(cli->inbuf,smb_tprcnt);

		if (total_data <= *data_len && total_param <= *param_len) {
			ret = True;
			break;
		}
	}

  out:

	cli_state_seqnum_remove(cli, mid);

	if (ret) {
		/* Ensure the last 2 bytes of param and data are 2 null
		 * bytes. These are malloc'ed, but not included in any
		 * length counts. This allows cli_XX string reading functions
		 * to safely null terminate. */
		if (total_data) {
			SSVAL(*data,total_data,0);
		}
		if (total_param) {
			SSVAL(*param,total_param,0);
		}
	}

	return ret;
}

/****************************************************************************
 Send a SMB nttrans request.
****************************************************************************/

bool cli_send_nt_trans(struct cli_state *cli,
		       int function,
		       int flags,
		       uint16 *setup, unsigned int lsetup, unsigned int msetup,
		       char *param, unsigned int lparam, unsigned int mparam,
		       char *data, unsigned int ldata, unsigned int mdata)
{
	unsigned int i;
	unsigned int this_ldata,this_lparam;
	unsigned int tot_data=0,tot_param=0;
	uint16 mid;
	char *outdata,*outparam;

	this_lparam = MIN(lparam,cli->max_xmit - (500+lsetup*2)); /* hack */
	this_ldata = MIN(ldata,cli->max_xmit - (500+lsetup*2+this_lparam));

	memset(cli->outbuf,'\0',smb_size);
	cli_set_message(cli->outbuf,19+lsetup,0,True);
	SCVAL(cli->outbuf,smb_com,SMBnttrans);
	SSVAL(cli->outbuf,smb_tid, cli->cnum);
	cli_setup_packet(cli);

	/*
	 * Save the mid we're using. We need this for finding
	 * signing replies.
	 */

	mid = cli->mid;

	outparam = smb_buf(cli->outbuf)+3;
	outdata = outparam+this_lparam;

	/* primary request */
	SCVAL(cli->outbuf,smb_nt_MaxSetupCount,msetup);
	SCVAL(cli->outbuf,smb_nt_Flags,flags);
	SIVAL(cli->outbuf,smb_nt_TotalParameterCount, lparam);
	SIVAL(cli->outbuf,smb_nt_TotalDataCount, ldata);
	SIVAL(cli->outbuf,smb_nt_MaxParameterCount, mparam);
	SIVAL(cli->outbuf,smb_nt_MaxDataCount, mdata);
	SIVAL(cli->outbuf,smb_nt_ParameterCount, this_lparam);
	SIVAL(cli->outbuf,smb_nt_ParameterOffset, smb_offset(outparam,cli->outbuf));
	SIVAL(cli->outbuf,smb_nt_DataCount, this_ldata);
	SIVAL(cli->outbuf,smb_nt_DataOffset, smb_offset(outdata,cli->outbuf));
	SIVAL(cli->outbuf,smb_nt_SetupCount, lsetup);
	SIVAL(cli->outbuf,smb_nt_Function, function);
	for (i=0;i<lsetup;i++)		/* setup[] */
		SSVAL(cli->outbuf,smb_nt_SetupStart+i*2,setup[i]);

	if (this_lparam)			/* param[] */
		memcpy(outparam,param,this_lparam);
	if (this_ldata)			/* data[] */
		memcpy(outdata,data,this_ldata);

	cli_setup_bcc(cli, outdata+this_ldata);

	show_msg(cli->outbuf);
	if (!cli_send_smb(cli)) {
		return False;
	}

	cli_state_seqnum_persistent(cli, mid);

	if (this_ldata < ldata || this_lparam < lparam) {
		/* receive interim response */
		if (!cli_receive_smb(cli) || cli_is_error(cli)) {
			cli_state_seqnum_remove(cli, mid);
			return(False);
		}

		tot_data = this_ldata;
		tot_param = this_lparam;

		while (tot_data < ldata || tot_param < lparam)  {
			this_lparam = MIN(lparam-tot_param,cli->max_xmit - 500); /* hack */
			this_ldata = MIN(ldata-tot_data,cli->max_xmit - (500+this_lparam));

			cli_set_message(cli->outbuf,18,0,True);
			SCVAL(cli->outbuf,smb_com,SMBnttranss);

			/* XXX - these should probably be aligned */
			outparam = smb_buf(cli->outbuf);
			outdata = outparam+this_lparam;

			/* secondary request */
			SIVAL(cli->outbuf,smb_nts_TotalParameterCount,lparam);
			SIVAL(cli->outbuf,smb_nts_TotalDataCount,ldata);
			SIVAL(cli->outbuf,smb_nts_ParameterCount,this_lparam);
			SIVAL(cli->outbuf,smb_nts_ParameterOffset,smb_offset(outparam,cli->outbuf));
			SIVAL(cli->outbuf,smb_nts_ParameterDisplacement,tot_param);
			SIVAL(cli->outbuf,smb_nts_DataCount,this_ldata);
			SIVAL(cli->outbuf,smb_nts_DataOffset,smb_offset(outdata,cli->outbuf));
			SIVAL(cli->outbuf,smb_nts_DataDisplacement,tot_data);
			if (this_lparam)			/* param[] */
				memcpy(outparam,param+tot_param,this_lparam);
			if (this_ldata)			/* data[] */
				memcpy(outdata,data+tot_data,this_ldata);
			cli_setup_bcc(cli, outdata+this_ldata);

			show_msg(cli->outbuf);

			cli->mid = mid;
			if (!cli_send_smb(cli)) {
				cli_state_seqnum_remove(cli, mid);
				return False;
			}

			tot_data += this_ldata;
			tot_param += this_lparam;
		}
	}

	return(True);
}

/****************************************************************************
 Receive a SMB nttrans response allocating the necessary memory.
****************************************************************************/

bool cli_receive_nt_trans(struct cli_state *cli,
			  char **param, unsigned int *param_len,
			  char **data, unsigned int *data_len)
{
	unsigned int total_data=0;
	unsigned int total_param=0;
	unsigned int this_data,this_param;
	uint8 eclass;
	uint32 ecode;
	bool ret = False;
	uint16_t mid;

	*data_len = *param_len = 0;

	mid = SVAL(cli->outbuf,smb_mid);

	if (!cli_receive_smb(cli)) {
		cli_state_seqnum_remove(cli, mid);
		return False;
	}

	show_msg(cli->inbuf);

	/* sanity check */
	if (CVAL(cli->inbuf,smb_com) != SMBnttrans) {
		DEBUG(0,("Expected SMBnttrans response, got command 0x%02x\n",
			 CVAL(cli->inbuf,smb_com)));
		cli_state_seqnum_remove(cli, mid);
		return(False);
	}

	/*
	 * An NT RPC pipe call can return ERRDOS, ERRmoredata
	 * to a trans call. This is not an error and should not
	 * be treated as such.
	 */
	if (cli_is_dos_error(cli)) {
                cli_dos_error(cli, &eclass, &ecode);
		if (!(eclass == ERRDOS && ecode == ERRmoredata)) {
			goto out;
		}
	}

	/*
	 * Likewise for NT_STATUS_BUFFER_TOO_SMALL
	 */
	if (cli_is_nt_error(cli)) {
		if (!NT_STATUS_EQUAL(cli_nt_error(cli),
				     NT_STATUS_BUFFER_TOO_SMALL)) {
			goto out;
		}
	}

	/* parse out the lengths */
	total_data = IVAL(cli->inbuf,smb_ntr_TotalDataCount);
	total_param = IVAL(cli->inbuf,smb_ntr_TotalParameterCount);
	/* Only allow 16 megs. */
	if (total_param > 16*1024*1024) {
		DEBUG(0,("cli_receive_nt_trans: param buffer too large %d\n",
					total_param));
		goto out;
	}
	if (total_data > 16*1024*1024) {
		DEBUG(0,("cli_receive_nt_trans: data buffer too large %d\n",
					total_data));
		goto out;
	}

	/* allocate it */
	if (total_data) {
		/* We know adding 2 is safe as total_data is less
		 * than 16mb (above). */
		*data = (char *)SMB_REALLOC(*data,total_data+2);
		if (!(*data)) {
			DEBUG(0,("cli_receive_nt_trans: failed to enlarge data buffer to %d\n",total_data));
			goto out;
		}
	}

	if (total_param) {
		/* We know adding 2 is safe as total_param is less
		 * than 16mb (above). */
		*param = (char *)SMB_REALLOC(*param,total_param+2);
		if (!(*param)) {
			DEBUG(0,("cli_receive_nt_trans: failed to enlarge param buffer to %d\n", total_param));
			goto out;
		}
	}

	while (1)  {
		this_data = SVAL(cli->inbuf,smb_ntr_DataCount);
		this_param = SVAL(cli->inbuf,smb_ntr_ParameterCount);

		if (this_data + *data_len > total_data ||
		    this_param + *param_len > total_param) {
			DEBUG(1,("Data overflow in cli_receive_nt_trans\n"));
			goto out;
		}

		if (this_data + *data_len < this_data ||
				this_data + *data_len < *data_len ||
				this_param + *param_len < this_param ||
				this_param + *param_len < *param_len) {
			DEBUG(1,("Data overflow in cli_receive_nt_trans\n"));
			goto out;
		}

		if (this_data) {
			unsigned int data_offset_out = SVAL(cli->inbuf,smb_ntr_DataDisplacement);
			unsigned int data_offset_in = SVAL(cli->inbuf,smb_ntr_DataOffset);

			if (data_offset_out > total_data ||
					data_offset_out + this_data > total_data ||
					data_offset_out + this_data < data_offset_out ||
					data_offset_out + this_data < this_data) {
				DEBUG(1,("Data overflow in cli_receive_nt_trans\n"));
				goto out;
			}
			if (data_offset_in > cli->bufsize ||
					data_offset_in + this_data >  cli->bufsize ||
					data_offset_in + this_data < data_offset_in ||
					data_offset_in + this_data < this_data) {
				DEBUG(1,("Data overflow in cli_receive_nt_trans\n"));
				goto out;
			}

			memcpy(*data + data_offset_out, smb_base(cli->inbuf) + data_offset_in, this_data);
		}

		if (this_param) {
			unsigned int param_offset_out = SVAL(cli->inbuf,smb_ntr_ParameterDisplacement);
			unsigned int param_offset_in = SVAL(cli->inbuf,smb_ntr_ParameterOffset);

			if (param_offset_out > total_param ||
					param_offset_out + this_param > total_param ||
					param_offset_out + this_param < param_offset_out ||
					param_offset_out + this_param < this_param) {
				DEBUG(1,("Param overflow in cli_receive_nt_trans\n"));
				goto out;
			}
			if (param_offset_in > cli->bufsize ||
					param_offset_in + this_param >  cli->bufsize ||
					param_offset_in + this_param < param_offset_in ||
					param_offset_in + this_param < this_param) {
				DEBUG(1,("Param overflow in cli_receive_nt_trans\n"));
				goto out;
			}

			memcpy(*param + param_offset_out, smb_base(cli->inbuf) + param_offset_in, this_param);
		}

		*data_len += this_data;
		*param_len += this_param;

		if (total_data <= *data_len && total_param <= *param_len) {
			ret = True;
			break;
		}

		if (!cli_receive_smb(cli)) {
			goto out;
		}

		show_msg(cli->inbuf);

		/* sanity check */
		if (CVAL(cli->inbuf,smb_com) != SMBnttrans) {
			DEBUG(0,("Expected SMBnttrans response, got command 0x%02x\n",
				 CVAL(cli->inbuf,smb_com)));
			goto out;
		}
		if (cli_is_dos_error(cli)) {
                        cli_dos_error(cli, &eclass, &ecode);
			if(!(eclass == ERRDOS && ecode == ERRmoredata)) {
				goto out;
			}
		}
		/*
		 * Likewise for NT_STATUS_BUFFER_TOO_SMALL
		 */
		if (cli_is_nt_error(cli)) {
			if (!NT_STATUS_EQUAL(cli_nt_error(cli),
					     NT_STATUS_BUFFER_TOO_SMALL)) {
				goto out;
			}
		}

		/* parse out the total lengths again - they can shrink! */
		if (IVAL(cli->inbuf,smb_ntr_TotalDataCount) < total_data)
			total_data = IVAL(cli->inbuf,smb_ntr_TotalDataCount);
		if (IVAL(cli->inbuf,smb_ntr_TotalParameterCount) < total_param)
			total_param = IVAL(cli->inbuf,smb_ntr_TotalParameterCount);

		if (total_data <= *data_len && total_param <= *param_len) {
			ret = True;
			break;
		}
	}

  out:

	cli_state_seqnum_remove(cli, mid);

	if (ret) {
		/* Ensure the last 2 bytes of param and data are 2 null
		 * bytes. These are malloc'ed, but not included in any
		 * length counts. This allows cli_XX string reading functions
		 * to safely null terminate. */
		if (total_data) {
			SSVAL(*data,total_data,0);
		}
		if (total_param) {
			SSVAL(*param,total_param,0);
		}
	}

	return ret;
}

struct trans_recvblob {
	uint8_t *data;
	uint32_t max, total, received;
};

struct cli_trans_state {
	struct cli_state *cli;
	struct event_context *ev;
	uint8_t cmd;
	uint16_t mid;
	uint32_t seqnum;
	const char *pipe_name;
	uint8_t *pipe_name_conv;
	size_t pipe_name_conv_len;
	uint16_t fid;
	uint16_t function;
	int flags;
	uint16_t *setup;
	uint8_t num_setup, max_setup;
	uint8_t *param;
	uint32_t num_param, param_sent;
	uint8_t *data;
	uint32_t num_data, data_sent;

	uint8_t num_rsetup;
	uint16_t *rsetup;
	struct trans_recvblob rparam;
	struct trans_recvblob rdata;

	TALLOC_CTX *secondary_request_ctx;

	struct iovec iov[4];
	uint8_t pad[4];
	uint16_t vwv[32];
};

static NTSTATUS cli_pull_trans(uint8_t *inbuf,
			       uint8_t wct, uint16_t *vwv,
			       uint16_t num_bytes, uint8_t *bytes,
			       uint8_t smb_cmd, bool expect_first_reply,
			       uint8_t *pnum_setup, uint16_t **psetup,
			       uint32_t *ptotal_param, uint32_t *pnum_param,
			       uint32_t *pparam_disp, uint8_t **pparam,
			       uint32_t *ptotal_data, uint32_t *pnum_data,
			       uint32_t *pdata_disp, uint8_t **pdata)
{
	uint32_t param_ofs, data_ofs;

	if (expect_first_reply) {
		if ((wct != 0) || (num_bytes != 0)) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		return NT_STATUS_OK;
	}

	switch (smb_cmd) {
	case SMBtrans:
	case SMBtrans2:
		if (wct < 10) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		*ptotal_param	= SVAL(vwv + 0, 0);
		*ptotal_data	= SVAL(vwv + 1, 0);
		*pnum_param	= SVAL(vwv + 3, 0);
		param_ofs	= SVAL(vwv + 4, 0);
		*pparam_disp	= SVAL(vwv + 5, 0);
		*pnum_data	= SVAL(vwv + 6, 0);
		data_ofs	= SVAL(vwv + 7, 0);
		*pdata_disp	= SVAL(vwv + 8, 0);
		*pnum_setup	= CVAL(vwv + 9, 0);
		if (wct < 10 + (*pnum_setup)) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		*psetup = vwv + 10;

		break;
	case SMBnttrans:
		if (wct < 18) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		*ptotal_param	= IVAL(vwv, 3);
		*ptotal_data	= IVAL(vwv, 7);
		*pnum_param	= IVAL(vwv, 11);
		param_ofs	= IVAL(vwv, 15);
		*pparam_disp	= IVAL(vwv, 19);
		*pnum_data	= IVAL(vwv, 23);
		data_ofs	= IVAL(vwv, 27);
		*pdata_disp	= IVAL(vwv, 31);
		*pnum_setup	= CVAL(vwv, 35);
		*psetup		= vwv + 18;
		break;

	default:
		return NT_STATUS_INTERNAL_ERROR;
	}

	/*
	 * Check for buffer overflows. data_ofs needs to be checked against
	 * the incoming buffer length, data_disp against the total
	 * length. Likewise for param_ofs/param_disp.
	 */

	if (trans_oob(smb_len(inbuf), param_ofs, *pnum_param)
	    || trans_oob(*ptotal_param, *pparam_disp, *pnum_param)
	    || trans_oob(smb_len(inbuf), data_ofs, *pnum_data)
	    || trans_oob(*ptotal_data, *pdata_disp, *pnum_data)) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	*pparam = (uint8_t *)inbuf + 4 + param_ofs;
	*pdata = (uint8_t *)inbuf + 4 + data_ofs;

	return NT_STATUS_OK;
}

static NTSTATUS cli_trans_pull_blob(TALLOC_CTX *mem_ctx,
				    struct trans_recvblob *blob,
				    uint32_t total, uint32_t thistime,
				    uint8_t *buf, uint32_t displacement)
{
	if (blob->data == NULL) {
		if (total > blob->max) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		blob->total = total;
		blob->data = TALLOC_ARRAY(mem_ctx, uint8_t, total);
		if (blob->data == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	if (total > blob->total) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	if (thistime) {
		memcpy(blob->data + displacement, buf, thistime);
		blob->received += thistime;
	}

	return NT_STATUS_OK;
}

static void cli_trans_format(struct cli_trans_state *state, uint8_t *pwct,
			     int *piov_count)
{
	uint8_t wct = 0;
	struct iovec *iov = state->iov;
	uint8_t *pad = state->pad;
	uint16_t *vwv = state->vwv;
	uint16_t param_offset;
	uint16_t this_param = 0;
	uint16_t this_data = 0;
	uint32_t useable_space;
	uint8_t cmd;

	cmd = state->cmd;

	if ((state->param_sent != 0) || (state->data_sent != 0)) {
		/* The secondary commands are one after the primary ones */
		cmd += 1;
	}

	param_offset = smb_size - 4;

	switch (cmd) {
	case SMBtrans:
		pad[0] = 0;
		iov[0].iov_base = (void *)pad;
		iov[0].iov_len = 1;
		iov[1].iov_base = (void *)state->pipe_name_conv;
		iov[1].iov_len = state->pipe_name_conv_len;
		wct = 14 + state->num_setup;
		param_offset += iov[0].iov_len + iov[1].iov_len;
		iov += 2;
		break;
	case SMBtrans2:
		pad[0] = 0;
		pad[1] = 'D'; /* Copy this from "old" 3.0 behaviour */
		pad[2] = ' ';
		iov[0].iov_base = (void *)pad;
		iov[0].iov_len = 3;
		wct = 14 + state->num_setup;
		param_offset += 3;
		iov += 1;
		break;
	case SMBtranss:
		wct = 8;
		break;
	case SMBtranss2:
		wct = 9;
		break;
	case SMBnttrans:
		wct = 19 + state->num_setup;
		break;
	case SMBnttranss:
		wct = 18;
		break;
	}

	useable_space = state->cli->max_xmit - smb_size - sizeof(uint16_t)*wct;

	if (state->param_sent < state->num_param) {
		this_param = MIN(state->num_param - state->param_sent,
				 useable_space);
		iov[0].iov_base = (void *)(state->param + state->param_sent);
		iov[0].iov_len = this_param;
		iov += 1;
	}

	if (state->data_sent < state->num_data) {
		this_data = MIN(state->num_data - state->data_sent,
				useable_space - this_param);
		iov[0].iov_base = (void *)(state->data + state->data_sent);
		iov[0].iov_len = this_data;
		iov += 1;
	}

	param_offset += wct * sizeof(uint16_t);

	DEBUG(10, ("num_setup=%u, max_setup=%u, "
		   "param_total=%u, this_param=%u, max_param=%u, "
		   "data_total=%u, this_data=%u, max_data=%u, "
		   "param_offset=%u, param_disp=%u, data_disp=%u\n",
		   (unsigned)state->num_setup, (unsigned)state->max_setup,
		   (unsigned)state->num_param, (unsigned)this_param,
		   (unsigned)state->rparam.max,
		   (unsigned)state->num_data, (unsigned)this_data,
		   (unsigned)state->rdata.max,
		   (unsigned)param_offset,
		   (unsigned)state->param_sent, (unsigned)state->data_sent));

	switch (cmd) {
	case SMBtrans:
	case SMBtrans2:
		SSVAL(vwv + 0, 0, state->num_param);
		SSVAL(vwv + 1, 0, state->num_data);
		SSVAL(vwv + 2, 0, state->rparam.max);
		SSVAL(vwv + 3, 0, state->rdata.max);
		SCVAL(vwv + 4, 0, state->max_setup);
		SCVAL(vwv + 4, 1, 0);	/* reserved */
		SSVAL(vwv + 5, 0, state->flags);
		SIVAL(vwv + 6, 0, 0);	/* timeout */
		SSVAL(vwv + 8, 0, 0);	/* reserved */
		SSVAL(vwv + 9, 0, this_param);
		SSVAL(vwv +10, 0, param_offset);
		SSVAL(vwv +11, 0, this_data);
		SSVAL(vwv +12, 0, param_offset + this_param);
		SCVAL(vwv +13, 0, state->num_setup);
		SCVAL(vwv +13, 1, 0);	/* reserved */
		memcpy(vwv + 14, state->setup,
		       sizeof(uint16_t) * state->num_setup);
		break;
	case SMBtranss:
	case SMBtranss2:
		SSVAL(vwv + 0, 0, state->num_param);
		SSVAL(vwv + 1, 0, state->num_data);
		SSVAL(vwv + 2, 0, this_param);
		SSVAL(vwv + 3, 0, param_offset);
		SSVAL(vwv + 4, 0, state->param_sent);
		SSVAL(vwv + 5, 0, this_data);
		SSVAL(vwv + 6, 0, param_offset + this_param);
		SSVAL(vwv + 7, 0, state->data_sent);
		if (cmd == SMBtranss2) {
			SSVAL(vwv + 8, 0, state->fid);
		}
		break;
	case SMBnttrans:
		SCVAL(vwv,  0, state->max_setup);
		SSVAL(vwv,  1, 0); /* reserved */
		SIVAL(vwv,  3, state->num_param);
		SIVAL(vwv,  7, state->num_data);
		SIVAL(vwv, 11, state->rparam.max);
		SIVAL(vwv, 15, state->rdata.max);
		SIVAL(vwv, 19, this_param);
		SIVAL(vwv, 23, param_offset);
		SIVAL(vwv, 27, this_data);
		SIVAL(vwv, 31, param_offset + this_param);
		SCVAL(vwv, 35, state->num_setup);
		SSVAL(vwv, 36, state->function);
		memcpy(vwv + 19, state->setup,
		       sizeof(uint16_t) * state->num_setup);
		break;
	case SMBnttranss:
		SSVAL(vwv,  0, 0); /* reserved */
		SCVAL(vwv,  2, 0); /* reserved */
		SIVAL(vwv,  3, state->num_param);
		SIVAL(vwv,  7, state->num_data);
		SIVAL(vwv, 11, this_param);
		SIVAL(vwv, 15, param_offset);
		SIVAL(vwv, 19, state->param_sent);
		SIVAL(vwv, 23, this_data);
		SIVAL(vwv, 27, param_offset + this_param);
		SIVAL(vwv, 31, state->data_sent);
		SCVAL(vwv, 35, 0); /* reserved */
		break;
	}

	state->param_sent += this_param;
	state->data_sent += this_data;

	*pwct = wct;
	*piov_count = iov - state->iov;
}

static void cli_trans_done(struct tevent_req *subreq);

struct tevent_req *cli_trans_send(
	TALLOC_CTX *mem_ctx, struct event_context *ev,
	struct cli_state *cli, uint8_t cmd,
	const char *pipe_name, uint16_t fid, uint16_t function, int flags,
	uint16_t *setup, uint8_t num_setup, uint8_t max_setup,
	uint8_t *param, uint32_t num_param, uint32_t max_param,
	uint8_t *data, uint32_t num_data, uint32_t max_data)
{
	struct tevent_req *req, *subreq;
	struct cli_trans_state *state;
	int iov_count;
	uint8_t wct;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state, struct cli_trans_state);
	if (req == NULL) {
		return NULL;
	}

	if ((cmd == SMBtrans) || (cmd == SMBtrans2)) {
		if ((num_param > 0xffff) || (max_param > 0xffff)
		    || (num_data > 0xffff) || (max_data > 0xffff)) {
			DEBUG(3, ("Attempt to send invalid trans2 request "
				  "(setup %u, params %u/%u, data %u/%u)\n",
				  (unsigned)num_setup,
				  (unsigned)num_param, (unsigned)max_param,
				  (unsigned)num_data, (unsigned)max_data));
			tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
			return tevent_req_post(req, ev);
		}
	}

	/*
	 * The largest wct will be for nttrans (19+num_setup). Make sure we
	 * don't overflow state->vwv in cli_trans_format.
	 */

	if ((num_setup + 19) > ARRAY_SIZE(state->vwv)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	state->cli = cli;
	state->ev = ev;
	state->cmd = cmd;
	state->flags = flags;
	state->num_rsetup = 0;
	state->rsetup = NULL;
	ZERO_STRUCT(state->rparam);
	ZERO_STRUCT(state->rdata);

	if ((pipe_name != NULL)
	    && (!convert_string_talloc(state, CH_UNIX,
				       cli_ucs2(cli) ? CH_UTF16LE : CH_DOS,
				       pipe_name, strlen(pipe_name) + 1,
				       &state->pipe_name_conv,
				       &state->pipe_name_conv_len, true))) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return tevent_req_post(req, ev);
	}
	state->fid = fid;	/* trans2 */
	state->function = function; /* nttrans */

	state->setup = setup;
	state->num_setup = num_setup;
	state->max_setup = max_setup;

	state->param = param;
	state->num_param = num_param;
	state->param_sent = 0;
	state->rparam.max = max_param;

	state->data = data;
	state->num_data = num_data;
	state->data_sent = 0;
	state->rdata.max = max_data;

	cli_trans_format(state, &wct, &iov_count);

	subreq = cli_smb_req_create(state, ev, cli, cmd, 0, wct, state->vwv,
				    iov_count, state->iov);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	state->mid = cli_smb_req_mid(subreq);
	status = cli_smb_req_send(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return tevent_req_post(req, state->ev);
	}
	cli_state_seqnum_persistent(cli, state->mid);
	tevent_req_set_callback(subreq, cli_trans_done, req);
	return req;
}

static void cli_trans_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_trans_state *state = tevent_req_data(
		req, struct cli_trans_state);
	NTSTATUS status;
	bool sent_all;
	uint8_t wct;
	uint16_t *vwv;
	uint32_t num_bytes;
	uint8_t *bytes;
	uint8_t num_setup	= 0;
	uint16_t *setup		= NULL;
	uint32_t total_param	= 0;
	uint32_t num_param	= 0;
	uint32_t param_disp	= 0;
	uint32_t total_data	= 0;
	uint32_t num_data	= 0;
	uint32_t data_disp	= 0;
	uint8_t *param		= NULL;
	uint8_t *data		= NULL;

	status = cli_smb_recv(subreq, 0, &wct, &vwv, &num_bytes, &bytes);

	/*
	 * We can receive something like STATUS_MORE_ENTRIES, so don't use
	 * !NT_STATUS_IS_OK(status) here.
	 */

	if (NT_STATUS_IS_ERR(status)) {
		goto fail;
	}

	sent_all = ((state->param_sent == state->num_param)
		    && (state->data_sent == state->num_data));

	status = cli_pull_trans(
		cli_smb_inbuf(subreq), wct, vwv, num_bytes, bytes,
		state->cmd, !sent_all, &num_setup, &setup,
		&total_param, &num_param, &param_disp, &param,
		&total_data, &num_data, &data_disp, &data);

	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	if (!sent_all) {
		int iov_count;

		TALLOC_FREE(subreq);

		cli_trans_format(state, &wct, &iov_count);

		subreq = cli_smb_req_create(state, state->ev, state->cli,
					    state->cmd + 1, 0, wct, state->vwv,
					    iov_count, state->iov);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		cli_smb_req_set_mid(subreq, state->mid);

		status = cli_smb_req_send(subreq);

		if (!NT_STATUS_IS_OK(status)) {
			goto fail;
		}
		tevent_req_set_callback(subreq, cli_trans_done, req);
		return;
	}

	status = cli_trans_pull_blob(
		state, &state->rparam, total_param, num_param, param,
		param_disp);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Pulling params failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	status = cli_trans_pull_blob(
		state, &state->rdata, total_data, num_data, data,
		data_disp);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Pulling data failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	if ((state->rparam.total == state->rparam.received)
	    && (state->rdata.total == state->rdata.received)) {
		TALLOC_FREE(subreq);
		cli_state_seqnum_remove(state->cli, state->mid);
		tevent_req_done(req);
		return;
	}

	if (!cli_smb_req_set_pending(subreq)) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}
	return;

 fail:
	cli_state_seqnum_remove(state->cli, state->mid);
	TALLOC_FREE(subreq);
	tevent_req_nterror(req, status);
}

NTSTATUS cli_trans_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			uint16_t **setup, uint8_t *num_setup,
			uint8_t **param, uint32_t *num_param,
			uint8_t **data, uint32_t *num_data)
{
	struct cli_trans_state *state = tevent_req_data(
		req, struct cli_trans_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	if (setup != NULL) {
		*setup = talloc_move(mem_ctx, &state->rsetup);
		*num_setup = state->num_rsetup;
	} else {
		TALLOC_FREE(state->rsetup);
	}

	if (param != NULL) {
		*param = talloc_move(mem_ctx, &state->rparam.data);
		*num_param = state->rparam.total;
	} else {
		TALLOC_FREE(state->rparam.data);
	}

	if (data != NULL) {
		*data = talloc_move(mem_ctx, &state->rdata.data);
		*num_data = state->rdata.total;
	} else {
		TALLOC_FREE(state->rdata.data);
	}

	return NT_STATUS_OK;
}

NTSTATUS cli_trans(TALLOC_CTX *mem_ctx, struct cli_state *cli,
		   uint8_t trans_cmd,
		   const char *pipe_name, uint16_t fid, uint16_t function,
		   int flags,
		   uint16_t *setup, uint8_t num_setup, uint8_t max_setup,
		   uint8_t *param, uint32_t num_param, uint32_t max_param,
		   uint8_t *data, uint32_t num_data, uint32_t max_data,
		   uint16_t **rsetup, uint8_t *num_rsetup,
		   uint8_t **rparam, uint32_t *num_rparam,
		   uint8_t **rdata, uint32_t *num_rdata)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_trans_send(frame, ev, cli, trans_cmd,
			     pipe_name, fid, function, flags,
			     setup, num_setup, max_setup,
			     param, num_param, max_param,
			     data, num_data, max_data);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_trans_recv(req, mem_ctx, rsetup, num_rsetup,
				rparam, num_rparam, rdata, num_rdata);
 fail:
	TALLOC_FREE(frame);
	if (!NT_STATUS_IS_OK(status)) {
		cli_set_error(cli, status);
	}
	return status;
}
