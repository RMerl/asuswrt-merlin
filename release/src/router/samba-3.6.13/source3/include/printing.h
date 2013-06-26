#ifndef PRINTING_H_
#define PRINTING_H_

/* 
   Unix SMB/CIFS implementation.
   printing definitions
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

/*
   This file defines the low-level printing system interfaces used by the
   SAMBA printing subsystem.
*/

/* Extra fields above "LPQ_PRINTING" are used to map extra NT status codes. */

enum {
	LPQ_QUEUED = 0,
	LPQ_PAUSED,
	LPQ_SPOOLING,
	LPQ_PRINTING,
	LPQ_ERROR,
	LPQ_DELETING,
	LPQ_OFFLINE,
	LPQ_PAPEROUT,
	LPQ_PRINTED,
	LPQ_DELETED,
	LPQ_BLOCKED,
	LPQ_USER_INTERVENTION,

	/* smbd is dooing the file spooling before passing control to spoolss */
	PJOB_SMBD_SPOOLING
};

typedef struct _print_queue_struct {
	int sysjob;		/* normally the UNIX jobid -- see note in
				   printing.c:traverse_fn_delete() */
	int size;
	int page_count;
	int status;
	int priority;
	time_t time;
	fstring fs_user;
	fstring fs_file;
} print_queue_struct;

enum {LPSTAT_OK, LPSTAT_STOPPED, LPSTAT_ERROR};

typedef struct {
	fstring message;
	int qcount;
	int status;
}  print_status_struct;

/* Information for print jobs */
struct printjob {
	pid_t pid; /* which process launched the job */
	uint32_t jobid; /* the spoolss print job identifier */
	int sysjob; /* the system (lp) job number */
	int fd; /* file descriptor of open file if open */
	time_t starttime; /* when the job started spooling */
	int status; /* the status of this job */
	size_t size; /* the size of the job so far */
	int page_count;	/* then number of pages so far */
	bool spooled; /* has it been sent to the spooler yet? */
	bool smbjob; /* set if the job is a SMB job */
	fstring filename; /* the filename used to spool the file */
	fstring jobname; /* the job name given to us by the client */
	fstring user; /* the user who started the job */
	fstring clientmachine; /* The client machine which started this job */
	fstring queuename; /* service number of printer for this job */
	struct spoolss_DeviceMode *devmode;
};

/* Information for print interfaces */
struct printif
{
  /* value of the 'printing' option for this service */
  enum printing_types type;

  int (*queue_get)(const char *printer_name,
                   enum printing_types printing_type,
                   char *lpq_command,
                   print_queue_struct **q,
                   print_status_struct *status);
  int (*queue_pause)(int snum);
  int (*queue_resume)(int snum);
  int (*job_delete)(const char *sharename, const char *lprm_command, struct printjob *pjob);
  int (*job_pause)(int snum, struct printjob *pjob);
  int (*job_resume)(int snum, struct printjob *pjob);
  int (*job_submit)(int snum, struct printjob *pjob,
		    enum printing_types printing_type,
		    char *lpq_command);
};

extern struct printif	generic_printif;

#ifdef HAVE_CUPS
extern struct printif	cups_printif;
#endif /* HAVE_CUPS */

#ifdef HAVE_IPRINT
extern struct printif	iprint_printif;
#endif /* HAVE_IPRINT */

/* PRINT_MAX_JOBID is now defined in local.h */
#define UNIX_JOB_START PRINT_MAX_JOBID
#define NEXT_JOBID(j) ((j+1) % PRINT_MAX_JOBID > 0 ? (j+1) % PRINT_MAX_JOBID : 1)

#define MAX_CACHE_VALID_TIME 3600
#define CUPS_DEFAULT_CONNECTION_TIMEOUT 30

#ifndef PRINT_SPOOL_PREFIX
#define PRINT_SPOOL_PREFIX "smbprn."
#endif
#define PRINT_DATABASE_VERSION 8

#ifdef AIX
#define DEFAULT_PRINTING PRINT_AIX
#define PRINTCAP_NAME "/etc/qconfig"
#endif

#ifdef HPUX
#define DEFAULT_PRINTING PRINT_HPUX
#endif

#ifdef QNX
#define DEFAULT_PRINTING PRINT_QNX
#endif

#ifndef DEFAULT_PRINTING
#ifdef HAVE_CUPS
#define DEFAULT_PRINTING PRINT_CUPS
#define PRINTCAP_NAME "cups"
#elif defined(SYSV)
#define DEFAULT_PRINTING PRINT_SYSV
#define PRINTCAP_NAME "lpstat"
#else
#define DEFAULT_PRINTING PRINT_BSD
#define PRINTCAP_NAME "/etc/printcap"
#endif
#endif

#ifndef PRINTCAP_NAME
#define PRINTCAP_NAME "/etc/printcap"
#endif

/* There can be this many printing tdb's open, plus any locked ones. */
#define MAX_PRINT_DBS_OPEN 1

struct TDB_DATA;
struct tdb_context;

struct tdb_print_db {
	struct tdb_print_db *next, *prev;
	struct tdb_context *tdb;
	int ref_count;
	fstring printer_name;
};

/* 
 * Used for print notify
 */

#define NOTIFY_PID_LIST_KEY "NOTIFY_PID_LIST"

/* The following definitions come from printing/printspoolss.c  */

NTSTATUS print_spool_open(files_struct *fsp,
			  const char *fname,
			  uint16_t current_vuid);

int print_spool_write(files_struct *fsp, const char *data, uint32_t size,
		      SMB_OFF_T offset, uint32_t *written);

void print_spool_end(files_struct *fsp, enum file_close_type close_type);

void print_spool_terminate(struct connection_struct *conn,
			   struct print_file_data *print_file);

/* The following definitions come from printing/printing.c  */

uint32 sysjob_to_jobid(int unix_jobid);
bool print_notify_register_pid(int snum);
bool print_notify_deregister_pid(int snum);
bool print_job_exists(const char* sharename, uint32 jobid);
struct spoolss_DeviceMode *print_job_devmode(TALLOC_CTX *mem_ctx,
					     const char *sharename,
					     uint32 jobid);
bool print_job_set_name(struct tevent_context *ev,
			struct messaging_context *msg_ctx,
			const char *sharename, uint32 jobid, const char *name);
bool print_job_get_name(TALLOC_CTX *mem_ctx, const char *sharename, uint32_t jobid, char **name);
WERROR print_job_delete(const struct auth_serversupplied_info *server_info,
			struct messaging_context *msg_ctx,
			int snum, uint32_t jobid);
WERROR print_job_pause(const struct auth_serversupplied_info *server_info,
		     struct messaging_context *msg_ctx,
		     int snum, uint32 jobid);
WERROR print_job_resume(const struct auth_serversupplied_info *server_info,
		      struct messaging_context *msg_ctx,
		      int snum, uint32 jobid);
ssize_t print_job_write(struct tevent_context *ev,
			struct messaging_context *msg_ctx,
			int snum, uint32 jobid, const char *buf, size_t size);
int print_queue_length(struct messaging_context *msg_ctx, int snum,
		       print_status_struct *pstatus);
WERROR print_job_start(const struct auth_serversupplied_info *server_info,
		       struct messaging_context *msg_ctx,
		       const char *clientmachine,
		       int snum, const char *docname, const char *filename,
		       struct spoolss_DeviceMode *devmode, uint32_t *_jobid);
void print_job_endpage(struct messaging_context *msg_ctx,
		       int snum, uint32 jobid);
NTSTATUS print_job_end(struct messaging_context *msg_ctx, int snum,
		       uint32 jobid, enum file_close_type close_type);
int print_queue_status(struct messaging_context *msg_ctx, int snum,
		       print_queue_struct **ppqueue,
		       print_status_struct *status);
WERROR print_queue_pause(const struct auth_serversupplied_info *server_info,
			 struct messaging_context *msg_ctx, int snum);
WERROR print_queue_resume(const struct auth_serversupplied_info *server_info,
			  struct messaging_context *msg_ctx, int snum);
WERROR print_queue_purge(const struct auth_serversupplied_info *server_info,
			 struct messaging_context *msg_ctx, int snum);
uint16 pjobid_to_rap(const char* sharename, uint32 jobid);
bool rap_to_pjobid(uint16 rap_jobid, fstring sharename, uint32 *pjobid);
void rap_jobid_delete(const char* sharename, uint32 jobid);
bool print_backend_init(struct messaging_context *msg_ctx);
void start_background_queue(struct tevent_context *ev,
			    struct messaging_context *msg);
void printing_end(void);

/* The following definitions come from printing/lpq_parse.c  */

bool parse_lpq_entry(enum printing_types printing_type,char *line,
		     print_queue_struct *buf,
		     print_status_struct *status,bool first);

/* The following definitions come from printing/printing_db.c  */

struct tdb_print_db *get_print_db_byname(const char *printername);
void release_print_db( struct tdb_print_db *pdb);
void close_all_print_db(void);
struct TDB_DATA get_printer_notify_pid_list(struct tdb_context *tdb, const char *printer_name, bool cleanlist);

void print_queue_receive(struct messaging_context *msg,
				void *private_data,
				uint32_t msg_type,
				struct server_id server_id,
				DATA_BLOB *data);
#endif /* PRINTING_H_ */
