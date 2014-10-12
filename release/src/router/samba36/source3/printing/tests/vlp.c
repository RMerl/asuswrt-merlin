/* 
   Unix SMB/Netbios implementation.

   Virtual lp system for printer testing

   Copyright (C) Tim Potter 2000
   
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
#include "system/passwd.h"
#include "system/filesys.h"
#include "printing.h"
#include "util_tdb.h"

#ifdef malloc
#undef malloc
#endif

#define PRINT_FIRSTJOB "100"

static TDB_CONTEXT *tdb;

struct vlp_job {
	fstring owner;
	int jobid;
	fstring jobname;
	int size;
	int status;
	time_t submit_time;
	int deleted;
};

/* Print usage */

static void usage(void)
{
	printf("Usage: vlp tdbfile=/tmp/vlp.tdb lpq|lprm|print|queuepause|queueresume|"
	       "lppause|lpresume [args]\n");
}

/* Return an array of vlp jobs that is the printer queue */

static void get_job_list(char *printer, struct vlp_job **job_list, 
			 int *num_jobs)
{
	fstring keystr;
	TDB_DATA data;

	slprintf(keystr, sizeof(keystr) - 1, "LPQ/%s", printer);
	data = tdb_fetch_bystring(tdb, keystr);

	*job_list = (struct vlp_job *)data.dptr;
	*num_jobs = data.dsize / sizeof(struct vlp_job);
}

/* Store an array of vl jobs for the queue */

static void set_job_list(char *printer, struct vlp_job *job_list, 
			 int num_jobs)
{
	fstring keystr;
	TDB_DATA data;

	slprintf(keystr, sizeof(keystr) - 1, "LPQ/%s", printer);

	data.dptr = (unsigned char *)job_list;
	data.dsize = num_jobs * sizeof(struct vlp_job);
	tdb_store_bystring(tdb, keystr, data, TDB_REPLACE);
}

/* Return the next job number for a printer */

static int next_jobnum(char *printer)
{
	fstring keystr;
	int jobnum;

	slprintf(keystr, sizeof(keystr) - 1, "JOBNUM/%s", printer);

	tdb_lock_bystring(tdb, keystr);

	jobnum = tdb_fetch_int32(tdb, keystr);

	/* Create next job index if none exists */

	if (jobnum == -1) {
		jobnum = atoi(PRINT_FIRSTJOB);
	}

	jobnum++;
	tdb_store_int32(tdb, keystr, jobnum);

	tdb_unlock_bystring(tdb, keystr);

	return jobnum;
}

static void set_printer_status(char *printer, int status)
{
	fstring keystr;
	int result;

	slprintf(keystr, sizeof(keystr) - 1, "STATUS/%s", printer);
	result = tdb_store_int32(tdb, keystr, status);
}

static int get_printer_status(char *printer)
{
	fstring keystr;
	TDB_DATA data;

	slprintf(keystr, sizeof(keystr) - 1, "STATUS/%s", printer);

	data.dptr = (unsigned char *)keystr;
	data.dsize = strlen(keystr) + 1;

	if (!tdb_exists(tdb, data)) {
		set_printer_status(printer, LPSTAT_OK);
		return LPSTAT_OK;
	}

	return tdb_fetch_int32(tdb, keystr);
}

/* Display printer queue */

static int lpq_command(int argc, char **argv)
{
	char *printer;
	struct vlp_job *job_list = NULL;
	int i, num_jobs, job_count = 0;

	if (argc != 2) {
		printf("Usage: lpq <printername>\n");
		return 1;
	}

	printer = argv[1];

	/* Display printer status */

	switch (get_printer_status(printer)) {
	case LPSTAT_OK:
		printf("enabled\n");
		break;
	case LPSTAT_STOPPED:
		printf("disabled\n");
		break;
	case LPSTAT_ERROR:
	default:
		printf("error\n");
		break;
	}

	/* Print queued documents */

	get_job_list(printer, &job_list, &num_jobs);

	for (i = 0; i < num_jobs; i++) {
		if (job_list[i].deleted) continue;
		printf("%d\t%d\t%d\t%ld\t%s\t%s\n", job_list[i].jobid,
		       job_list[i].size, 
		       (i == 0 && job_list[i].status == LPQ_QUEUED) ? 
		       LPQ_SPOOLING : job_list[i].status,
		       (long int)job_list[i].submit_time, job_list[i].owner, 
		       job_list[i].jobname);
		job_count++;
	}

	free(job_list);

	return 0;
}

/* Remove a job */

static int lprm_command(int argc, char **argv)
{
	char *printer;
	int jobid, num_jobs, i;
	struct vlp_job *job_list;

	if (argc < 3) {
		printf("Usage: lprm <printername> <jobid>\n");
		return 1;
	}

	printer = argv[1];
	jobid = atoi(argv[2]);

	get_job_list(printer, &job_list, &num_jobs);

	for (i = 0; i < num_jobs; i++) {
		if (job_list[i].jobid == jobid) {
			job_list[i].deleted = 1;
			set_job_list(printer, job_list, num_jobs);
			break;
		}
	}

	return 0;
}

/* print command = print-test %p %s */

static int print_command(int argc, char **argv)
{
	char *printer;
	fstring keystr;
	struct passwd *pw;
	TDB_DATA value, queue;
	struct vlp_job job;

	if (argc < 3) {
		printf("Usage: print <printername> <jobname>\n");
		return 1;
	}

	printer = argv[1];

	ZERO_STRUCT(job);

	/* Create a job record */

	slprintf(job.jobname, sizeof(job.jobname) - 1, "%s", argv[2]);

	if (!(pw = getpwuid(geteuid()))) {
		printf("getpwuid failed\n");
		return 1;
	}

	slprintf(job.owner, sizeof(job.owner) - 1, "%s", pw->pw_name);

	job.jobid = next_jobnum(printer);
	job.size = 666;
	job.submit_time = time(NULL);

	/* Store job entry in queue */

	slprintf(keystr, sizeof(keystr) - 1, "LPQ/%s", printer);

	value = tdb_fetch_bystring(tdb, keystr);

	if (value.dptr) {

		/* Add job to end of queue */

		queue.dptr = (unsigned char *)malloc(value.dsize + sizeof(struct vlp_job));
		if (!queue.dptr) return 1;

		memcpy(queue.dptr, value.dptr, value.dsize);
		memcpy(queue.dptr + value.dsize, &job, sizeof(struct vlp_job));

		queue.dsize = value.dsize + sizeof(struct vlp_job);

		tdb_store_bystring(tdb, keystr, queue, TDB_REPLACE);

		free(queue.dptr);

	} else {

		/* Create new queue */
		queue.dptr = (unsigned char *)&job;
		queue.dsize = sizeof(struct vlp_job);

		tdb_store_bystring(tdb, keystr, queue, TDB_REPLACE);
	}

	return 0;
}

/* Pause the queue */

static int queuepause_command(int argc, char **argv)
{
	char *printer;

	if (argc != 2) {
		printf("Usage: queuepause <printername>\n");
		return 1;
	}

	printer = argv[1];
	set_printer_status(printer, LPSTAT_STOPPED);

	return 0;
}

/* Resume the queue */

static int queueresume_command(int argc, char **argv)
{
	char *printer;

	if (argc != 2) {
		printf("Usage: queueresume <printername>\n");
		return 1;
	}

	printer = argv[1];
	set_printer_status(printer, LPSTAT_OK);

	return 0;
}

/* Pause a job */

static int lppause_command(int argc, char **argv)
{
	struct vlp_job *job_list;
	char *printer;
	int jobid, num_jobs, i;

	if (argc != 3) {
		printf("Usage: lppause <printername> <jobid>\n");
		return 1;
	}

	printer = argv[1];
	jobid = atoi(argv[2]);

	get_job_list(printer, &job_list, &num_jobs);

	for (i = 0; i < num_jobs; i++) {
		if (job_list[i].jobid == jobid) {
			job_list[i].status = LPQ_PAUSED;
			set_job_list(printer, job_list, num_jobs);
			return 0;
		}
	}

	return 1;
}

/* Resume a job */

static int lpresume_command(int argc, char **argv)
{
	struct vlp_job *job_list;
	char *printer;
	int jobid, num_jobs, i;

	if (argc != 3) {
		printf("Usage: lpresume <printername> <jobid>\n");
		return 1;
	}

	printer = argv[1];
	jobid = atoi(argv[2]);

	get_job_list(printer, &job_list, &num_jobs);

	for (i = 0; i < num_jobs; i++) {
		if (job_list[i].jobid == jobid) {
			job_list[i].status = LPQ_QUEUED;
			set_job_list(printer, job_list, num_jobs);
			return 0;
		}
	}

	return 1;
}

int main(int argc, char **argv)
{
	/* Parameter check */
	const char *printdb_path = NULL;

	if (argc < 2) {
		usage();
		return 1;
	}

	if (strncmp(argv[1], "tdbfile", strlen("tdbfile")) != 0) {
		usage();
		return 1;
	}

	printdb_path = get_string_param(argv[1]);
	if (!printdb_path) {
		return 1;
	}

	if (!(tdb = tdb_open(printdb_path, 0, 0, O_RDWR | O_CREAT,
			     0666))) {
		printf("%s: unable to open %s\n", argv[0], printdb_path);
		return 1;
	}

	/* Ensure we are modes 666 */

	chmod(printdb_path, 0666);

	/* Do commands */

	if (strcmp(argv[2], "lpq") == 0) {
		return lpq_command(argc - 2, &argv[2]);
	}

	if (strcmp(argv[2], "lprm") == 0) {
		return lprm_command(argc - 2, &argv[2]);
	}

	if (strcmp(argv[2], "print") == 0) {
		return print_command(argc - 2, &argv[2]);
	}

	if (strcmp(argv[2], "queuepause") == 0) {
		return queuepause_command(argc - 2, &argv[2]);
	}

	if (strcmp(argv[2], "queueresume") == 0) {
		return queueresume_command(argc - 2, &argv[2]);
	}

	if (strcmp(argv[2], "lppause") == 0) {
		return lppause_command(argc - 2, &argv[2]);
	}

	if (strcmp(argv[2], "lpresume") == 0) {
		return lpresume_command(argc - 2, &argv[2]);
	}

	/* Unknown command */

	printf("%s: invalid command %s\n", argv[0], argv[1]);
	return 1;
}
