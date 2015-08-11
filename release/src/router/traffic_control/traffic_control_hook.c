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
long long int traffic_control_realtime_traffic(char *interface, int unit)
{
	long long int result = 0;
	FILE *fp = NULL;
	FILE *fn = NULL;
	unsigned long long tx = 0, rx = 0;	// current
	unsigned long long tx_t = 0, rx_t = 0;	// old
	unsigned long long tx_n = 0, rx_n = 0;	// diff
	char ifmap[IFNAME_MAX];	// ifname after mapping
	char wanif[IFNAME_MAX];
	char buf[256];
	char tx_buf[IFPATH_MAX], rx_buf[IFPATH_MAX];
	char tx_path[IFPATH_MAX], rx_path[IFPATH_MAX];
	char *p = NULL;
 	char *ifname = NULL;

	int debug = nvram_get_int("traffic_control_debug");

	memset(buf, 0, sizeof(buf));
	memset(ifmap, 0, sizeof(ifmap));
	ifname_mapping(interface, ifmap); // database interface mapping
	if(debug) dbg("%s: interface=%s, ifmap=%s\n", __FUNCTION__, interface, ifmap);

	snprintf(tx_path, sizeof(tx_path), "/jffs/traffic_control/%s/tx_t", ifmap);
	snprintf(rx_path, sizeof(rx_path), "/jffs/traffic_control/%s/rx_t", ifmap);

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

			memset(wanif, 0, sizeof(wanif));
			snprintf(wanif, sizeof(wanif), "wan%d_ifname", unit);
			if(strcmp(ifname, nvram_safe_get(wanif))) continue;
			else match = 1;

			// search traffic
			if (sscanf(p + 1, "%llu%*u%*u%*u%*u%*u%*u%*u%llu", &rx, &tx) != 2) continue;

			// tx / rx unit = KB
			tx = tx / 1024;
			rx = rx / 1024;
			
			memset(tx_buf, 0, sizeof(tx_buf));
			if((fn = fopen(tx_path, "r")) != NULL){
				fgets(tx_buf, sizeof(tx_buf), fn);
				tx_t = strtoull(tx_buf, NULL, 10);
				fclose(fn);
			}
			else
				tx_t = 0;

			memset(rx_buf, 0, sizeof(rx_buf));
			if((fn = fopen(rx_path, "r")) != NULL){
				fgets(rx_buf, sizeof(rx_buf), fn);
				rx_t = strtoull(rx_buf, NULL, 10);
				fclose(fn);
			}
			else
				rx_t = 0;

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

			if(match) break;
		}
		fclose(fp);
	}

	return result;
}

void traffic_cotrol_WanStat(char *buf, char *ifname, char *start, char *end, int unit)
{
	if(ifname == NULL || start == NULL || end == NULL ||
		(strcmp(ifname, "wan") && strcmp(ifname, "lan") && strcmp(ifname, "usb"))){
		sprintf(buf, "[0]");
		return;
	}

	int lock;	// file lock
	int ret;
	char path[IFPATH_MAX];
	char ifmap[IFNAME_MAX];	// ifname after mapping
	char sql[256];
	char cmd[256];
	sqlite3 *db;
	int rows;
	int cols;
	char **result;
	time_t ts, te;
	long long int tx = 0, rx = 0, current = 0;
	time_t now;

	int debug = nvram_get_int("traffic_control_debug");
	lock = file_lock("traffic_control");

	// word and ifmap mapping
	memset(ifmap, 0, sizeof(ifmap));
	ifname_mapping(ifname, ifmap); // database interface mapping
	if(debug) dbg("%s: ifname=%s, ifmap=%s\n", __FUNCTION__, ifname, ifmap);

	snprintf(path, sizeof(path), "/jffs/traffic_control/%s/traffic.db", ifmap);

	ret = sqlite3_open(path, &db);
	if(ret){
		printf("%s : Can't open database %s\n", __FUNCTION__, sqlite3_errmsg(db));
		sqlite3_close(db);
		file_unlock(lock);
		sprintf(buf, "[0]");
		return;
	}
	
	memset(sql, 0, sizeof(sql));
	ts = atoll(start);
	te = atoll(end);
	sprintf(sql, "SELECT ifname, SUM(tx), SUM(rx) FROM (SELECT timestamp, ifname, tx, rx FROM traffic WHERE timestamp BETWEEN %ld AND %ld) WHERE ifname = \"%s\"",
		(ts - 30), (te + 30), ifmap);

	// get current timestamp
	time(&now);
	
	// real-time traffic (not data in database)
	if(debug) dbg("%s : te=%ld, now=%ld, diff=%ld, DAY=%ld\n", __FUNCTION__, te, now, (te - now), DAY);
	if((te - now) < DAY && (te - now) > 0){
		current = traffic_control_realtime_traffic(ifname, unit);
		memset(cmd, 0, sizeof(cmd));
		sprintf(cmd, "echo -n %lld > /jffs/traffic_control/%s/tmp", current, ifmap);
		system(cmd);
	}

	if(sql_get_table(db, sql, &result, &rows, &cols) == SQLITE_OK)
	{
		int i = 0;
		int j = 0;
		int index = cols;
		for(i = 0; i < rows; i++){
			for(j = 0; j < cols; j++){
				if(j == 1) tx = (result[index] == NULL) ? 0 : atoll(result[index]);
				if(j == 2) rx = (result[index] == NULL) ? 0 : atoll(result[index]);
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK)
				sprintf(buf, "[%llu]", (tx + rx + current));
#else
				sprintf(buf, "[%llu]", (tx + rx + current)/2);
#endif
				++index;
			}
			if(debug) dbg("%s : ifname=%s, start=%s, end=%s, current=%lld, buf=%s\n", __FUNCTION__, ifname, start, end, current, buf);
		}
		sqlite3_free_table(result);
	}
	sqlite3_close(db);
	file_unlock(lock);
}

void traffic_control_hook(char *ifname, char *start, char *end, char *unit, int *retval, webs_t wp)
{
	char buf[64];
	int unit_t = atoi(unit);
	memset(buf, 0, sizeof(buf));
	int debug = nvram_get_int("traffic_control_debug");
	if(debug) dbg("%s : ifname=%s, start=%s, end=%s, unit_t=%d\n", __FUNCTION__, ifname, start, end, unit_t);

	traffic_cotrol_WanStat(buf, ifname, start, end, unit_t);
	
	*retval += websWrite(wp, buf);
}

/*
	interface mapping rule
	ex.
	word = wan / lan / usb
	ifname = wan / lan / usb / imsi(number)
*/
void ifname_mapping(char *word, char *ifname)
{
	if(!strcmp(word, "usb") && nvram_get_int("usb_modem_act_imsi"))
		sprintf(ifname, nvram_safe_get("usb_modem_act_imsi"));
	else
		sprintf(ifname, word);
}
