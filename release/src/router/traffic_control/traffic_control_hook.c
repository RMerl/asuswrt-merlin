/*
	for traffic control hook
	
	NOTE:
		traffic unit in database : KB (Kbytes)
*/

#include "traffic_control.h"

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

static
void traffic_cotrol_WanStat(char *buf, char *ifname, char *start, char *end)
{
	if(ifname == NULL || start == NULL || end == NULL){
		sprintf(buf, "[0, 0]");
		return;
	}

	int lock;	// file lock
	int ret;
	char *path = TRAFFIC_CONTROL_DATA;
	char sql[256];
	sqlite3 *db;

	int rows;
	int cols;
	char **result;
	time_t ts, te;

	int debug = nvram_get_int("traffic_control_debug");

	lock = file_lock("traffic_control");
	ret = sqlite3_open(path, &db);
	if(ret){
		printf("%s : Can't open database %s\n", __FUNCTION__, sqlite3_errmsg(db));
		sqlite3_close(db);
		file_unlock(lock);
		exit(1);
	}
	
	memset(sql, 0, sizeof(sql));
	ts = atoll(start);
	te = atoll(end);
	sprintf(sql, "SELECT ifname, SUM(tx), SUM(rx) FROM (SELECT timestamp, ifname, tx, rx FROM traffic WHERE timestamp BETWEEN %ld AND %ld) WHERE ifname = \"%s\"",
		(ts - 30), (te + 30), ifname);

	if(debug) dbg("%s : sql=%s, ifname=%s, start=%s, end=%s\n", __FUNCTION__, sql, ifname, start, end);
	if(sql_get_table(db, sql, &result, &rows, &cols) == SQLITE_OK)
	{
		int i = 0;
		int j = 0;
		int index = cols;
		for(i = 0; i < rows; i++){
			for(j = 0; j < cols; j++){
				if(j == 1) sprintf(buf, "[%s,", (result[index] == NULL) ? "0" :result[index]);
				if(j == 2) sprintf(buf, "%s %s]", buf, (result[index] == NULL) ? "0" :result[index]);
				++index;
			}
			if(debug) dbg("%s : buf=%s\n", __FUNCTION__, buf);
		}
		sqlite3_free_table(result);
	}
	sqlite3_close(db);
	file_unlock(lock);
}

void traffic_control_hook(char *ifname, char *start, char *end, int *retval, webs_t wp)
{
	char buf[64];
	memset(buf, 0, sizeof(buf));
	int debug = nvram_get_int("traffic_control_debug");
	if(debug) dbg("%s : ifname=%s, start=%s, end=%s\n", __FUNCTION__, ifname, start, end);

	traffic_cotrol_WanStat(buf, ifname, start, end);
	
	*retval += websWrite(wp, buf);
}
