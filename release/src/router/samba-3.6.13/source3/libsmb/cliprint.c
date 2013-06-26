/*
   Unix SMB/CIFS implementation.
   client print routines
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
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"

/*****************************************************************************
 Convert a character pointer in a cli_call_api() response to a form we can use.
 This function contains code to prevent core dumps if the server returns
 invalid data.
*****************************************************************************/
static const char *fix_char_ptr(unsigned int datap, unsigned int converter,
			  char *rdata, int rdrcnt)
{
	unsigned int offset;

	if (datap == 0)	{
		/* turn NULL pointers into zero length strings */
		return "";
	}

	offset = datap - converter;

	if (offset >= rdrcnt) {
		DEBUG(1,("bad char ptr: datap=%u, converter=%u rdrcnt=%d>",
			 datap, converter, rdrcnt));
		return "<ERROR>";
	}
	return &rdata[offset];
}

/****************************************************************************
call fn() on each entry in a print queue
****************************************************************************/

int cli_print_queue(struct cli_state *cli,
		    void (*fn)(struct print_job_info *))
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt, rprcnt;
	char param[1024];
	int result_code=0;
	int i = -1;

	memset(param,'\0',sizeof(param));

	p = param;
	SSVAL(p,0,76);         /* API function number 76 (DosPrintJobEnum) */
	p += 2;
	safe_strcpy_base(p,"zWrLeh", param, sizeof(param));   /* parameter description? */
	p = skip_string(param,sizeof(param),p);
	safe_strcpy_base(p,"WWzWWDDzz", param, sizeof(param));  /* returned data format */
	p = skip_string(param,sizeof(param),p);
	safe_strcpy_base(p,cli->share, param, sizeof(param));    /* name of queue */
	p = skip_string(param,sizeof(param),p);
	SSVAL(p,0,2);   /* API function level 2, PRJINFO_2 data structure */
	SSVAL(p,2,1000); /* size of bytes of returned data buffer */
	p += 4;
	safe_strcpy_base(p,"", param,sizeof(param));   /* subformat */
	p = skip_string(param,sizeof(param),p);

	DEBUG(4,("doing cli_print_queue for %s\n", cli->share));

	if (cli_api(cli,
		    param, PTR_DIFF(p,param), 1024,  /* Param, length, maxlen */
		    NULL, 0, CLI_BUFFER_SIZE,            /* data, length, maxlen */
		    &rparam, &rprcnt,                /* return params, length */
		    &rdata, &rdrcnt)) {               /* return data, length */
		int converter;
		result_code = SVAL(rparam,0);
		converter = SVAL(rparam,2);       /* conversion factor */

		if (result_code == 0) {
			struct print_job_info job;

			p = rdata;

			for (i = 0; i < SVAL(rparam,4); ++i) {
				job.id = SVAL(p,0);
				job.priority = SVAL(p,2);
				fstrcpy(job.user,
					fix_char_ptr(SVAL(p,4), converter,
						     rdata, rdrcnt));
				job.t = make_unix_date3(
					p + 12, cli->serverzone);
				job.size = IVAL(p,16);
				fstrcpy(job.name,fix_char_ptr(SVAL(p,24),
							      converter,
							      rdata, rdrcnt));
				fn(&job);
				p += 28;
			}
		}
	}

	/* If any parameters or data were returned, free the storage. */
	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return i;
}

/****************************************************************************
  cancel a print job
  ****************************************************************************/

int cli_printjob_del(struct cli_state *cli, int job)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int ret = -1;
	char param[1024];

	memset(param,'\0',sizeof(param));

	p = param;
	SSVAL(p,0,81);		/* DosPrintJobDel() */
	p += 2;
	safe_strcpy_base(p,"W", param,sizeof(param));
	p = skip_string(param,sizeof(param),p);
	safe_strcpy_base(p,"", param,sizeof(param));
	p = skip_string(param,sizeof(param),p);
	SSVAL(p,0,job);
	p += 2;

	if (cli_api(cli,
		    param, PTR_DIFF(p,param), 1024,  /* Param, length, maxlen */
		    NULL, 0, CLI_BUFFER_SIZE,            /* data, length, maxlen */
		    &rparam, &rprcnt,                /* return params, length */
		    &rdata, &rdrcnt)) {               /* return data, length */
		ret = SVAL(rparam,0);
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return ret;
}
