/* 
   Unix SMB/CIFS implementation.
   printing command routines
   Copyright (C) Andrew Tridgell 1992-2000
   
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
#include "printing.h"

extern struct current_user current_user;
extern userdom_struct current_user_info;

/****************************************************************************
 Run a given print command
 a null terminated list of value/substitute pairs is provided
 for local substitution strings
****************************************************************************/
static int print_run_command(int snum, const char* printername, bool do_sub,
			     const char *command, int *outfd, ...)
{
	char *syscmd;
	char *arg;
	int ret;
	TALLOC_CTX *ctx = talloc_tos();
	va_list ap;
	va_start(ap, outfd);

	/* check for a valid system printername and valid command to run */

	if ( !printername || !*printername ) {
		va_end(ap);
		return -1;
	}

	if (!command || !*command) {
		va_end(ap);
		return -1;
	}

	syscmd = talloc_strdup(ctx, command);
	if (!syscmd) {
		va_end(ap);
		return -1;
	}

	while ((arg = va_arg(ap, char *))) {
		char *value = va_arg(ap,char *);
		syscmd = talloc_string_sub(ctx, syscmd, arg, value);
		if (!syscmd) {
			va_end(ap);
			return -1;
		}
	}
	va_end(ap);

	syscmd = talloc_string_sub(ctx, syscmd, "%p", printername);
	if (!syscmd) {
		return -1;
	}

	if (do_sub && snum != -1) {
		syscmd = talloc_sub_advanced(ctx,
				lp_servicename(snum),
				current_user_info.unix_name,
				"",
				current_user.ut.gid,
				get_current_username(),
				current_user_info.domain,
				syscmd);
		if (!syscmd) {
			return -1;
		}
	}

	ret = smbrun_no_sanitize(syscmd,outfd);

	DEBUG(3,("Running the command `%s' gave %d\n",syscmd,ret));

	return ret;
}


/****************************************************************************
delete a print job
****************************************************************************/
static int generic_job_delete( const char *sharename, const char *lprm_command, struct printjob *pjob)
{
	fstring jobstr;

	/* need to delete the spooled entry */
	slprintf(jobstr, sizeof(jobstr)-1, "%d", pjob->sysjob);
	return print_run_command( -1, sharename, False, lprm_command, NULL,
		   "%j", jobstr,
		   "%T", http_timestring(talloc_tos(), pjob->starttime),
		   NULL);
}

/****************************************************************************
pause a job
****************************************************************************/
static int generic_job_pause(int snum, struct printjob *pjob)
{
	fstring jobstr;
	
	/* need to pause the spooled entry */
	slprintf(jobstr, sizeof(jobstr)-1, "%d", pjob->sysjob);
	return print_run_command(snum, lp_printername(snum), True,
				 lp_lppausecommand(snum), NULL,
				 "%j", jobstr,
				 NULL);
}

/****************************************************************************
resume a job
****************************************************************************/
static int generic_job_resume(int snum, struct printjob *pjob)
{
	fstring jobstr;

	/* need to pause the spooled entry */
	slprintf(jobstr, sizeof(jobstr)-1, "%d", pjob->sysjob);
	return print_run_command(snum, lp_printername(snum), True,
				 lp_lpresumecommand(snum), NULL,
				 "%j", jobstr,
				 NULL);
}

/****************************************************************************
get the current list of queued jobs
****************************************************************************/
static int generic_queue_get(const char *printer_name,
                             enum printing_types printing_type,
                             char *lpq_command,
                             print_queue_struct **q,
                             print_status_struct *status)
{
	char **qlines;
	int fd;
	int numlines, i, qcount;
	print_queue_struct *queue = NULL;

	/* never do substitution when running the 'lpq command' since we can't
	   get it rigt when using the background update daemon.  Make the caller
	   do it before passing off the command string to us here. */

	print_run_command(-1, printer_name, False, lpq_command, &fd, NULL);

	if (fd == -1) {
		DEBUG(5,("generic_queue_get: Can't read print queue status for printer %s\n",
			printer_name ));
		return 0;
	}

	numlines = 0;
	qlines = fd_lines_load(fd, &numlines,0,NULL);
	close(fd);

	/* turn the lpq output into a series of job structures */
	qcount = 0;
	ZERO_STRUCTP(status);
	if (numlines && qlines) {
		queue = SMB_MALLOC_ARRAY(print_queue_struct, numlines+1);
		if (!queue) {
			TALLOC_FREE(qlines);
			*q = NULL;
			return 0;
		}
		memset(queue, '\0', sizeof(print_queue_struct)*(numlines+1));

		for (i=0; i<numlines; i++) {
			/* parse the line */
			if (parse_lpq_entry(printing_type,qlines[i],
					    &queue[qcount],status,qcount==0)) {
				qcount++;
			}
		}
	}

	TALLOC_FREE(qlines);
        *q = queue;
	return qcount;
}

/****************************************************************************
 Submit a file for printing - called from print_job_end()
****************************************************************************/

static int generic_job_submit(int snum, struct printjob *pjob,
			      enum printing_types printing_type,
			      char *lpq_cmd)
{
	int ret = -1;
	char *current_directory = NULL;
	char *print_directory = NULL;
	char *wd = NULL;
	char *p = NULL;
	char *jobname = NULL;
	TALLOC_CTX *ctx = talloc_tos();
	fstring job_page_count, job_size;
	print_queue_struct *q;
	print_status_struct status;

	/* we print from the directory path to give the best chance of
           parsing the lpq output */
	current_directory = TALLOC_ARRAY(ctx,
					char,
					PATH_MAX+1);
	if (!current_directory) {
		return -1;
	}
	wd = sys_getwd(current_directory);
	if (!wd) {
		return -1;
	}

	print_directory = talloc_strdup(ctx, pjob->filename);
	if (!print_directory) {
		return -1;
	}
	p = strrchr_m(print_directory,'/');
	if (!p) {
		return -1;
	}
	*p++ = 0;

	if (chdir(print_directory) != 0) {
		return -1;
	}

	jobname = talloc_strdup(ctx, pjob->jobname);
	if (!jobname) {
		ret = -1;
		goto out;
	}
	jobname = talloc_string_sub(ctx, jobname, "'", "_");
	if (!jobname) {
		ret = -1;
		goto out;
	}
	slprintf(job_page_count, sizeof(job_page_count)-1, "%d", pjob->page_count);
	slprintf(job_size, sizeof(job_size)-1, "%lu", (unsigned long)pjob->size);

	/* send it to the system spooler */
	ret = print_run_command(snum, lp_printername(snum), True,
			lp_printcommand(snum), NULL,
			"%s", p,
			"%J", jobname,
			"%f", p,
			"%z", job_size,
			"%c", job_page_count,
			NULL);
	if (ret != 0) {
		ret = -1;
		goto out;
	}

	/*
	 * check the queue for the newly submitted job, this allows us to
	 * determine the backend job identifier (sysjob).
	 */
	pjob->sysjob = -1;
	ret = generic_queue_get(lp_printername(snum), printing_type, lpq_cmd,
				&q, &status);
	if (ret > 0) {
		int i;
		for (i = 0; i < ret; i++) {
			if (strcmp(q[i].fs_file, p) == 0) {
				pjob->sysjob = q[i].sysjob;
				DEBUG(5, ("new job %u (%s) matches sysjob %d\n",
					  pjob->jobid, jobname, pjob->sysjob));
				break;
			}
		}
		SAFE_FREE(q);
		ret = 0;
	}
	if (pjob->sysjob == -1) {
		DEBUG(2, ("failed to get sysjob for job %u (%s), tracking as "
			  "Unix job\n", pjob->jobid, jobname));
	}


 out:

	if (chdir(wd) == -1) {
		smb_panic("chdir failed in generic_job_submit");
	}
	TALLOC_FREE(current_directory);
        return ret;
}

/****************************************************************************
 pause a queue
****************************************************************************/
static int generic_queue_pause(int snum)
{
	return print_run_command(snum, lp_printername(snum), True,
				 lp_queuepausecommand(snum), NULL, NULL);
}

/****************************************************************************
 resume a queue
****************************************************************************/
static int generic_queue_resume(int snum)
{
	return print_run_command(snum, lp_printername(snum), True,
				 lp_queueresumecommand(snum), NULL, NULL);
}

/****************************************************************************
 * Generic printing interface definitions...
 ***************************************************************************/

struct printif	generic_printif =
{
	DEFAULT_PRINTING,
	generic_queue_get,
	generic_queue_pause,
	generic_queue_resume,
	generic_job_delete,
	generic_job_pause,
	generic_job_resume,
	generic_job_submit,
};

