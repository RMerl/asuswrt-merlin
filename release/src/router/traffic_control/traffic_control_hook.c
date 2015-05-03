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
long long int traffic_control_realtime_traffic(char *interface)
{
	long long int result = 0;
	FILE *fp = NULL;
	unsigned long long tx = 0, rx = 0;	// current
	unsigned long long tx_t = 0, rx_t = 0;	// old
	unsigned long long tx_n = 0, rx_n = 0;	// diff
	char buf[256];
	char *p = NULL;
 	char *ifname = NULL;

	int debug = nvram_get_int("traffic_control_debug");

	memset(buf, 0, sizeof(buf));

 	if ((fp = fopen("/proc/net/dev", "r")) != NULL)
	{
		// skip first two rows
		fgets(buf, sizeof(buf), fp);
		fgets(buf, sizeof(buf), fp);

		while (fgets(buf, sizeof(buf), fp))
		{
			int match = 0;

			// search interface
			if ((p = strchr(buf, ':')) == NULL) continue;
			*p = 0;
			if ((ifname = strrchr(buf, ' ')) == NULL)
				ifname = buf;
			else
				++ifname;

			if(strcmp(ifname, interface)) continue;
			else match = 1;

			// search traffic
			if (sscanf(p + 1, "%llu%*u%*u%*u%*u%*u%*u%*u%llu", &rx, &tx) != 2) continue;

			// tx / rx unit = KB
			tx = tx / 1024;
			rx = rx / 1024;
			
			// check nvram
			if(nvram_safe_get("traffic_control_old_tx") == NULL)
				nvram_set("traffic_control_old_tx", "0");
			if(nvram_safe_get("traffic_control_old_rx") == NULL)
				nvram_set("traffic_control_old_rx", "0");
				
			// load old tx /rx
			tx_t = strtoull(nvram_safe_get("traffic_control_old_tx"), NULL, 10);
			rx_t = strtoull(nvram_safe_get("traffic_control_old_rx"), NULL, 10);
						
			// proc/net/dev max bytes is unsigned long (2^32)
			// diff traffic
			if(tx < tx_t)
				tx_n = (tx + LONGSIZE) - tx_t;
			else
				tx_n = tx - tx_t;
				
			if(rx < rx_t)
				rx_n = (rx + LONGSIZE) - rx_t;
			else
				rx_n = rx - rx_t;

			result = tx_n + rx_n;

			if(debug) dbg("%s : tx/tx_t/tx_n = %16llu/%16lld/%16lld\n", __FUNCTION__, tx, tx_t, tx_n);
			if(debug) dbg("%s : rx/rx_t/rx_n = %16llu/%16lld/%16lld\n", __FUNCTION__, rx, rx_t, rx_n);
			if(debug) dbg("%s : result = %16llu\n", __FUNCTION__, result);

			if(match) break;
		}
		fclose(fp);
	}

	return result;
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
	long long int tx = 0, rx = 0, current = 0;
	time_t now;

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

	// get current timestamp
	time(&now);
	
	// real-time traffic (not data in database)
	if(debug) dbg("%s : te=%ld, now=%ld, diff=%ld, DAY=%ld\n", __FUNCTION__, te, now, (te - now), DAY);
	if((te - now) < DAY && (te - now) > 0){
		current = traffic_control_realtime_traffic(ifname);
	}

	if(debug) dbg("%s : sql=%s, ifname=%s, start=%s, end=%s\n", __FUNCTION__, sql, ifname, start, end);
	if(sql_get_table(db, sql, &result, &rows, &cols) == SQLITE_OK)
	{
		int i = 0;
		int j = 0;
		int index = cols;
		for(i = 0; i < rows; i++){
			for(j = 0; j < cols; j++){
				if(j == 1) tx = (result[index] == NULL) ? 0 : atoll(result[index]);
				if(j == 2) rx = (result[index] == NULL) ? 0 : atoll(result[index]);
				sprintf(buf, "[%llu]", (tx + rx + current));
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
