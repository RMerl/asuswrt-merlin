/*
	notification center database
	- hook or httpd usage API
	- only read and delete
*/

#include <libnt.h>

#define MyDBG(fmt,args...) \
	if(isFileExist(NOTIFY_DB_DEBUG) > 0) { \
		Debug2Console("[nt_db_stat][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args); \
	}

NOTIFY_DATABASE_T *event_listcreate(NOTIFY_DATABASE_T input)
{
	NOTIFY_DATABASE_T *new=malloc(sizeof(*new));
	
	memcpy(new,&input,sizeof(*new));
	return new;
}

NOTIFY_DATABASE_T *initial_db_input()
{
	NOTIFY_DATABASE_T *input;
	input = malloc(sizeof(NOTIFY_DATABASE_T));
	if (!input) 
		return NULL;
	memset(input, 0, sizeof(NOTIFY_DATABASE_T));
	return input;
}

void db_input_free(NOTIFY_DATABASE_T *input)
{
	if(input) free(input);
}
static void DatabaseAddLinkedList(struct list *event_list, NOTIFY_DATABASE_T event_t)
{
	NOTIFY_DATABASE_T *sevent_t;

	if(event_list)
	{
		sevent_t = NULL;
		sevent_t = event_listcreate(event_t);
		if(sevent_t)
			listnode_add(event_list, (void*)sevent_t);
	
	}
}

void NT_DBFree(struct list *event_list)
{
	list_delete(event_list);
}

int sql_get_table(sqlite3 *db, const char *sql, char ***pazResult, int *pnRow, int *pnColumn)
{
	int ret;
	char *errMsg = NULL;
	
	ret = sqlite3_get_table(db, sql, pazResult, pnRow, pnColumn, &errMsg);
	if( ret != SQLITE_OK )
	{
		if (errMsg) sqlite3_free(errMsg);
	}

	return ret;
}

void get_format_loop(char *tmp, char *format)
{
	int count = 0;
	int first = 1;
	char *p = NULL, *g = NULL;

	g = format;
	while (g)
	{
		count++;
		MyDBG("p=%s, g=%s, format=%s, count=%d, first=%d\n", p, g, format, count, first);
		if ((p = strsep(&g, "<")) == NULL) break;
		if (!strcmp(p, "1") && count == 1){
			sprintf(tmp, "webui=1");
			first = 0;
		}
		else if (!strcmp(p, "1") && count == 2){
			if(first == 1){
				sprintf(tmp, "email=1");
				first = 0;
			}
			else sprintf(tmp, "%s OR email=1", tmp);
		}
		else if (!strcmp(p, "1") && count == 3){
			if(first == 1){
				sprintf(tmp, "app=1");
				first = 0;
			}
			else sprintf(tmp, "%s OR app=1", tmp);
		}
		else if (!strcmp(p, "1") && count == 4){
			if(first == 1){
				sprintf(tmp, "weekly=1");
				first = 0;
			}
			else sprintf(tmp, "%s OR weekly=1", tmp);
		}
	}
	MyDBG("tmp=%s\n", tmp);
}

int NT_DBCommand(char *action, NOTIFY_DATABASE_T *input)
{
	int result = 0;
	int ret = 0, lock = 0;
	char path[24];
	char format[8];
	char *zErr;
	sqlite3 *db;

	int webui = 0, email = 0, app = 0, weekly = 0;

	lock = file_lock("nt_db");

	if(!isFileExist(NOTIFY_DB_FOLDER)) mkdir(NOTIFY_DB_FOLDER, 0666);
	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), NOTIFY_DB_FOLDER"nt_center.db");

	MyDBG("path = %s\n", path);
	ret = sqlite3_open(path, &db);
	if(ret){
		MyDBG("can't open database, return\n");
		goto error;
	}

	/* action sevice evnet */
	memset(format, 0, sizeof(format));
	if(input->flag != 0){
		if(input->flag & ACTION_NOTIFY_WEBUI) webui = 1;
		if(input->flag & ACTION_NOTIFY_EMAIL) email = 1;
		if(input->flag & ACTION_NOTIFY_APP) app = 1;
		if(input->flag & ACTION_NOTIFY_WEEKLY) weekly = 1;
		snprintf(format, sizeof(format), "%d<%d<%d<%d", webui, email, app, weekly);
	}
	MyDBG("webui=%d, email=%d, app=%d, weekly=%d, foramt=%s\n", webui, email, app, weekly, format);

	if(!strcasecmp(action, "write"))
	{
		if(input->tstamp == 0) {
			MyDBG("Error tstamp CANT BE ZERO. return\n");
			goto error;
		}
		/* initial format */
		ret = sqlite3_exec(db,
			"CREATE TABLE nt_center("
			"timestamp VARCHAR(12),"
			"event VARCHAR(8),"
			"webui VARCHAR(1),"
			"email VARCHAR(1),"
			"app VARCHAR(1),"
			"weekly VARCHAR(1),"
			"msg VARCHAR(100))",
			NULL, NULL, &zErr);

		if(ret != SQLITE_OK){
			if(zErr != NULL){
				MyDBG("SQL error - write: %s\n", zErr);
				sqlite3_free(zErr);
			}
		}

		/* index : event */
		if(sqlite3_exec(db, "CREATE INDEX event ON nt_center(event ASC)", NULL, NULL, &zErr) != SQLITE_OK)
		{
			if(zErr != NULL) sqlite3_free(zErr);
		}

		/* index : flag */
		if(sqlite3_exec(db, "CREATE INDEX flag ON nt_center(flag ASC)", NULL, NULL, &zErr) != SQLITE_OK)
		{
			if(zErr != NULL) sqlite3_free(zErr);
		}

		/* initial */
		char sql[NOTIFY_DB_QLEN];
		char tmp[100];
		memset(sql, 0, sizeof(sql));
		memset(tmp, 0, sizeof(tmp));

		/* msg */
		if(!strcmp(input->msg, ""))
			snprintf(tmp, sizeof(tmp), "%s", "");
		else
			snprintf(tmp, sizeof(tmp), "%s", input->msg);
		
		/* save data into database */
		snprintf(sql, sizeof(sql),
			"INSERT INTO nt_center VALUES ('%ld', '%x', '%d', '%d', '%d', '%d', '%s')",
			input->tstamp, input->event, webui, email, app, weekly, tmp);
		sqlite3_exec(db, sql, NULL, NULL, &zErr);
		if(zErr != NULL) sqlite3_free(zErr);
		result = 1;
	}
	else if(!strcasecmp(action, "read"))
	{
		/* initial */
		int rows;
		int cols;
		char **res;
		char sql[NOTIFY_DB_QLEN];
		char tmp[100];
		memset(sql, 0, sizeof(sql));
		memset(tmp, 0, sizeof(tmp));

		/* check input : flag */
		if(input->flag == 0){
			printf("[nt_db] MSUT input flag to read data\n");
			goto error;
		}

		/* query condition from flag */
		get_format_loop(tmp, format);
		snprintf(sql, sizeof(sql), "SELECT timestamp, event, msg FROM nt_center WHERE %s", tmp);

		/* result */
		if(sql_get_table(db, sql, &res, &rows, &cols) == SQLITE_OK){
			int i = 0, j = 0;
			int index = cols;
			for(i = 0; i < rows; i++){
				for(j = 0; j < cols; j++){
					MyDBG("[%7d/%7d] result: %s\n", i, j, res[index]);
					++index;
				}
			}
			sqlite3_free_table(res);
		}
		result = 1;
	}
	else if(!strcasecmp(action, "delete"))
	{
		/* initial */
		char sql[NOTIFY_DB_QLEN];
		memset(sql, 0, sizeof(sql));

		/* query condition */
		if(input->tstamp == 0 || input->event == 0){
			printf("[nt_db] MSUT input timestamp and event to delete\n");
			goto error;
		}
		snprintf(sql, sizeof(sql), "DELETE from nt_center WHERE timestamp=%ld AND event=%x", input->tstamp, input->event);

		/* execute to delete */
		if(sqlite3_exec(db, sql,  NULL, NULL, &zErr) != SQLITE_OK){
			if(zErr != NULL){
				printf("SQL error: %s\n", zErr);
				sqlite3_free(zErr);
				goto error;
			}
		}

		/* compact database */
		if(sqlite3_exec(db, "VACUUM;",  NULL, NULL, &zErr) != SQLITE_OK){
			if(zErr != NULL){
				printf("SQL error: %s\n", zErr);
				sqlite3_free(zErr);
				goto error;
			}
		}

		MyDBG("delete data success!!\n");
		result = 1;
	}

/* error : close database and unlock file */
error:
	sqlite3_close(db);
	file_unlock(lock);
	return result;
}

void NT_DBAction(struct list *event_list, char *action, NOTIFY_DATABASE_T *input)
{
	int ret = 0, lock = 0;
	char path[24];
	char format[8];
	char *zErr;
	sqlite3 *db;

	int webui = 0, email = 0, app = 0, weekly = 0;

	lock = file_lock("nt_db");

	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), NOTIFY_DB_FOLDER"nt_center.db");
	if(!isFileExist(path)){
		MyDBG("%s no database!\n", path);
		goto error;
	}

	ret = sqlite3_open(path, &db);
	if(ret){
		MyDBG("can't open database, return\n");
		goto error;
	}
	
	/* action sevice evnet */
	memset(format, 0, sizeof(format));
	if(input->flag != 0){
		if(input->flag & ACTION_NOTIFY_WEBUI) webui = 1;
		if(input->flag & ACTION_NOTIFY_EMAIL) email = 1;
		if(input->flag & ACTION_NOTIFY_APP) app = 1;
		if(input->flag & ACTION_NOTIFY_WEEKLY) weekly = 1;
		snprintf(format, sizeof(format), "%d<%d<%d<%d", webui, email, app, weekly);
	}
	MyDBG("webui=%d, email=%d, app=%d, weekly=%d, foramt=%s\n", webui, email, app, weekly, format);

	if(!strcasecmp(action, "read"))
	{
		/* initial */
		int rows;
		int cols;
		char **res;
		char sql[NOTIFY_DB_QLEN];
		char tmp[100];
		memset(sql, 0, sizeof(sql));
		memset(tmp, 0, sizeof(tmp));
		NOTIFY_DATABASE_T event_t;

		/* check input : flag */
		if(input->flag == 0){
			MyDBG("MSUT input flag to read data\n");
			goto error;
		}

		/* query condition from flag */
		get_format_loop(tmp, format);
		snprintf(sql, sizeof(sql), "SELECT timestamp, event, msg FROM nt_center WHERE %s", tmp);

		/* result */
		if(sql_get_table(db, sql, &res, &rows, &cols) == SQLITE_OK){
			int i = 0, j = 0;
			int index = cols;
			for(i = 0; i < rows; i++)
			{
				/* MUST initial event_t */
				memset(&event_t, 0, sizeof(NOTIFY_DATABASE_T));

				for(j = 0; j < cols; j++){
					MyDBG("[%7d/%7d] result: %s\n", i, j, res[index]);
					if(j == 0) event_t.tstamp = atoi(res[index]);
					if(j == 1) event_t.event = strtol(res[index], NULL, 16);
					if(j == 2) strcpy(event_t.msg, res[index]);
					++index;
				}
				DatabaseAddLinkedList(event_list, event_t);
			}
			sqlite3_free_table(res);
		}
	}
	else if(!strcasecmp(action, "delete"))
	{
		/* initial */
		char sql[NOTIFY_DB_QLEN];
		memset(sql, 0, sizeof(sql));

		/* query condition */
		if(input->tstamp == 0 || input->event == 0){
			MyDBG("MSUT input timestamp and event to delete\n");
			goto error;
		}
		snprintf(sql, sizeof(sql), "DELETE from nt_center WHERE timestamp=%ld AND event=%x", input->tstamp, input->event);

		/* execute to delete */
		if(sqlite3_exec(db, sql,  NULL, NULL, &zErr) != SQLITE_OK){
			if(zErr != NULL){
				MyDBG("SQL error: %s\n", zErr);
				sqlite3_free(zErr);
				goto error;
			}
		}

		/* compact database */
		if(sqlite3_exec(db, "VACUUM;",  NULL, NULL, &zErr) != SQLITE_OK){
			if(zErr != NULL){
				MyDBG("SQL error: %s\n", zErr);
				sqlite3_free(zErr);
				goto error;
			}
		}

		MyDBG("delete data success!!\n");
	}

/* error : close database and unlock file */
error:
	sqlite3_close(db);
	file_unlock(lock);
}
