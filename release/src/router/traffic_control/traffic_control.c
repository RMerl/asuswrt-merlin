/*
	for traffic control to write / read database
	
	NOTE:
		traffic unit in database : KB (Kbytes)
*/

#include "traffic_control.h"

/*
	q_if : query interface
	q_te : query end time
	q_ts : query start time
*/
int traffic_control_main(char *type, char *q_if, char *q_te, char *q_ts)
{
	int lock;	// file lock
	FILE *fp = NULL;
	unsigned long long tx = 0, rx = 0;	// current
	unsigned long long tx_t = 0, rx_t = 0;	// old
	unsigned long long tx_n = 0, rx_n = 0;	// diff
	char buf[256];
	char *p = NULL;
 	char *ifname = NULL;
	char ifname_desc1[12], ifname_desc2[12];
	time_t now;	// timestamp

	int ret;
	char *zErr = NULL;
	char *path = TRAFFIC_CONTROL_DATA;
	char sql[256];
	sqlite3 *db;

	int debug = nvram_get_int("traffic_control_debug");
	long int date_start = nvram_get_int("traffic_control_date_start");

	memset(buf, 0, sizeof(buf));
	memset(ifname_desc1, 0, sizeof(ifname_desc1));
	memset(ifname_desc2, 0, sizeof(ifname_desc2));

	lock = file_lock("traffic_control");

	if(!f_exists(path)){
		mkdir(TRAFFIC_CONTROL_FOLDER, 0666);
		eval("touch", path);
	}

	ret = sqlite3_open(path, &db);

	if(ret){
		printf("%s : Can't open database %s\n", __FUNCTION__, sqlite3_errmsg(db));
		sqlite3_close(db);
		file_unlock(lock);
		exit(1);
	}

	if(!strcmp(type, "w"))
	{
		// get timestamp
		time(&now);

		if(f_exists(path))
		{
			// delete database out of last 2 months (DATA_PERIOD)
			// we allow 30 secs delay because of save data will be delay some secs
			memset(sql, 0, sizeof(sql));
			sprintf(sql, "DELETE FROM traffic WHERE timestamp NOT BETWEEN %ld AND %ld",
				(date_start - DATA_PERIOD - 30), (now + 30));

			if(sqlite3_exec(db, sql, NULL, NULL, &zErr) != SQLITE_OK){
				if(zErr != NULL){
					printf("%s - delete : SQL error: %s\n", __FUNCTION__, zErr);
					sqlite3_free(zErr);
				}
			}

			// add index for timestamp
			sqlite3_exec(db, "CREATE INDEX timestamp ON traffic(timestamp ASC)", NULL, NULL, &zErr);
			if(zErr != NULL) sqlite3_free(zErr);
		}

		ret = sqlite3_exec(db,
			"CREATE TABLE traffic("
			"timestamp UNSIGNED BIG INT NOT NULL,"
			"ifname TEXT NOT NULL,"
			"tx UNSIGNED BIG INT NOT NULL,"
			"rx UNSIGNED BIG INT NOT NULL)",
			NULL, NULL, &zErr);

		if(ret != SQLITE_OK)
		{
			if(zErr != NULL)
			{
				printf("SQL error: %s\n", zErr);
				sqlite3_free(zErr);
			}
		}

		// record database
 		if ((fp = fopen("/proc/net/dev", "r")) != NULL)
		{
			// skip first two rows
			fgets(buf, sizeof(buf), fp);
			fgets(buf, sizeof(buf), fp);

			while (fgets(buf, sizeof(buf), fp))
			{
				int unit;
				char word[16], tmp[64], prefix[] = "wanXXXXXXXXXX_";
				int match = 0;

				// search interface
				if ((p = strchr(buf, ':')) == NULL) continue;
				*p = 0;
				if ((ifname = strrchr(buf, ' ')) == NULL)
					ifname = buf;
				else
					++ifname;

				// check wan ifname
				for(unit = 0; unit < 2; unit++){
					snprintf(prefix, sizeof(prefix), "wan%d_", unit);
					strncpy(word, nvram_safe_get(strcat_r(prefix, "ifname", tmp)), 16);
					if (!strcmp(ifname, word)) {
						match = 1;
						break;
					}
				}
				if(!match) continue;

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
				
				// save old tx / rx
				nvram_set_int("traffic_control_old_tx", tx);
				nvram_set_int("traffic_control_old_rx", rx);

				if(debug) dbg("%s : tx/tx_t/tx_n = %16llu/%16lld/%16lld\n", __FUNCTION__, tx, tx_t, tx_n);
				if(debug) dbg("%s : rx/rx_t/rx_n = %16llu/%16lld/%16lld\n", __FUNCTION__, rx, rx_t, rx_n);

				// debug message
				if(debug) dbg("%s : %12lu/%10s/%16llu/%16llu\n", __FUNCTION__, now, ifname, tx_n, rx_n);
				memset(sql, 0, sizeof(sql));
				sprintf(sql, "INSERT INTO traffic VALUES ('%lu','%s','%llu','%llu')",
					now, ifname, tx_n, rx_n);
				sqlite3_exec(db, sql, NULL, NULL, &zErr);
				if(zErr != NULL) sqlite3_free(zErr);
			}
			fclose(fp);
		}
		sqlite3_close(db);
		file_unlock(lock);
	}
	else if(!strcmp(type, "r"))
	{
		int rows;
		int cols;
		char **result;
		time_t ts, te;

		if(q_if == NULL || q_ts == NULL || q_te == NULL) goto finish2;

		// get data
		memset(sql, 0, sizeof(sql));
		te = atoll(q_te);
		ts = atoll(q_ts);
		sprintf(sql, "SELECT ifname, SUM(tx), SUM(rx) FROM (SELECT timestamp, ifname, tx, rx FROM traffic WHERE timestamp BETWEEN %ld AND %ld) WHERE ifname = \"%s\"",
			(ts - 30), (te + 30), q_if);

		if(sql_get_table(db, sql, &result, &rows, &cols) == SQLITE_OK)
		{
			int i = 0;
			int j = 0;
			int index = cols;
			for(i = 0; i < rows; i++){
				for(j = 0; j < cols; j++){
					if(debug) dbg("%s :[%3d/%3d] %16s\n",
						__FUNCTION__, i, j, result[index]);
					++index;
				}
			}
			sqlite3_free_table(result);
		}
		
finish2:
		sqlite3_close(db);
		file_unlock(lock);
	}
	else
	{
		dbg("%s : illegal type\n", __FUNCTION__);
		sqlite3_close(db);
		file_unlock(lock);
		return 0;
	}

	return 1;
}
