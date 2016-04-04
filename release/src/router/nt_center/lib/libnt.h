 /*
 * Copyright 2015, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <nt_db.h>
#include <nt_common.h>
#include <linklist.h>

#ifndef __libnt_h__
#define __libnt_h__

/* nt_share.c */
extern NOTIFY_EVENT_T *initial_nt_event();
extern void nt_event_free(NOTIFY_EVENT_T *e);
extern int send_notify_event(void *data, char *socketpath);
extern int send_trigger_event(NOTIFY_EVENT_T *event);
extern int eInfo_get_idx_by_evalue(int e);
extern char *eInfo_get_eName(int e);
extern int GetDebugValue(char *path);

/* nt_utility.c */
extern void Debug2Console(const char * format, ...);
extern int isFileExist(char *fname);
extern int get_pid_num_by_name(char *pidName);
extern void StampToDate(unsigned long timestamp, char *date);
extern int file_lock(char *tag);
extern void file_unlock(int lockfd);
extern int f_read_string(const char *path, char *buffer, int max);
extern int f_write_string(const char *path, const char *buffer, unsigned flags, unsigned cmode);

/* nt_db_stat.c */

/* #### API for NC #### */
extern int NT_DBCommand(char *action, NOTIFY_DATABASE_T *input);

/* #### API for httpd #### */
extern void NT_DBAction(struct list *event_list, char *action, NOTIFY_DATABASE_T *input);
extern void NT_DBFree(struct list *event_list);

/* #### API for Normal #### */
extern NOTIFY_DATABASE_T *initial_db_input();
extern void db_input_free(NOTIFY_DATABASE_T *input);
extern int sql_get_table(sqlite3 *db, const char *sql, char ***pazResult, int *pnRow, int *pnColumn);
extern void get_format_loop(char *tmp, char *format);

#endif
