/*
	for traffic control to write / read database
	
	NOTE:
		traffic unit in database : KB (Kbytes)
*/

#include "traffic_control.h"

static 
void traffic_control_datawrite()
{
	sqlite3 *db;
	time_t now;	// timestamp
	int unit;
	int lock;	// file lock
	int ret;
	char *p = NULL, *ifname = NULL;
	char *g, *word, *dualwan; // wan / lan / usb
	char ifmap[IFNAME_MAX];	// ifname after mapping
	char wanif[IFNAME_MAX];
	char *zErr = NULL;
	char path[IFPATH_MAX], folder[IFPATH_MAX], tx_path[IFPATH_MAX], rx_path[IFPATH_MAX];
	char buf[256];
	char cmd[256];
	char sql[256];
	char tx_buf[64], rx_buf[64];
	FILE *fp = NULL;
	FILE *fn = NULL;
	unsigned long long tx = 0, rx = 0;	// current
	unsigned long long tx_t = 0, rx_t = 0;	// old
	unsigned long long tx_n = 0, rx_n = 0;	// diff

	int debug = nvram_get_int("traffic_control_debug");
	long int date_start = nvram_get_int("traffic_control_date_start");
	
	lock = file_lock("traffic_control");
	mkdir("/jffs/traffic_control", 0666);

	// create database for each interface
	g = dualwan = strdup(nvram_safe_get("wans_dualwan"));
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
	{
		memset(ifmap, 0, sizeof(ifmap));
		memset(wanif, 0, sizeof(wanif));
		if((!g) || ((word = strsep(&g, " ")) == NULL)) continue;
		if(!strcmp(word, "none")) continue;
		snprintf(wanif, sizeof(wanif), "wan%d_ifname", unit);
		ifname_mapping(word, ifmap); // database interface mapping

		snprintf(folder, sizeof(folder), "/jffs/traffic_control/%s", ifmap);
		snprintf(path, sizeof(path), "/jffs/traffic_control/%s/traffic.db", ifmap);
		snprintf(tx_path, sizeof(tx_path), "/jffs/traffic_control/%s/tx_t", ifmap);
		snprintf(rx_path, sizeof(rx_path), "/jffs/traffic_control/%s/rx_t", ifmap);
		
		if(!f_exists(path)){
			mkdir(folder, 0666);
			eval("touch", path);
		}

		ret = sqlite3_open(path, &db);
		if(ret){
			printf("%s : Can't open database %s\n", __FUNCTION__, sqlite3_errmsg(db));
			sqlite3_close(db);
			file_unlock(lock);
			exit(1);
		}

		// get timestamp
		time(&now);

		if(f_exists(path))
		{
			// delete database out of last 1 months (DATA_PERIOD)
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

			// while loop
			while (fgets(buf, sizeof(buf), fp))
			{
				// search interface
				if ((p = strchr(buf, ':')) == NULL) continue;
				*p = 0;
				if ((ifname = strrchr(buf, ' ')) == NULL)
					ifname = buf;
				else
					++ifname;

				// check wan ifname
				if (strcmp(ifname, nvram_safe_get(wanif))) continue;
				if (debug) dbg("%s: wanif=%s, word=%s, ifmap=%s, ifname=%s\n", __FUNCTION__, nvram_safe_get(wanif), word, ifmap, ifname);

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
				
				// individual interface tx_t
				memset(cmd, 0, sizeof(cmd));
				sprintf(cmd, "echo -n %llu > %s", tx, tx_path);
				system(cmd);

				// individual interface rx_t
				memset(cmd, 0, sizeof(cmd));
				sprintf(cmd, "echo -n %llu > %s", rx, rx_path);
				system(cmd);

				if(debug) dbg("%s : tx/tx_t/tx_n = %16llu/%16lld/%16lld\n", __FUNCTION__, tx, tx_t, tx_n);
				if(debug) dbg("%s : rx/rx_t/rx_n = %16llu/%16lld/%16lld\n", __FUNCTION__, rx, rx_t, rx_n);

				// debug message
				if(debug) dbg("%s : %12lu/%10s/%16llu/%16llu\n", __FUNCTION__, now, ifmap, tx_n, rx_n);
				memset(sql, 0, sizeof(sql));
				sprintf(sql, "INSERT INTO traffic VALUES ('%lu','%s','%llu','%llu')",
					now, ifmap, tx_n, rx_n);
				sqlite3_exec(db, sql, NULL, NULL, &zErr);
				if(zErr != NULL) sqlite3_free(zErr);
			}// while loop
			fclose(fp);
		}// record database
		sqlite3_close(db);
	}// create database for each interface
	free(dualwan);
	file_unlock(lock);
}

static 
void traffic_control_dataread(char *q_if, char *q_ts, char *q_te)
{
	sqlite3 *db;
	int lock;	// file lock
	int ret;
	int rows;
	int cols;
	char **result;
	time_t ts, te;
	char ifmap[IFNAME_MAX];	// ifname after mapping
	char path[IFPATH_MAX];
	char sql[256];

	int debug = nvram_get_int("traffic_control_debug");
	lock = file_lock("traffic_control");

	if(q_if == NULL || q_ts == NULL || q_te == NULL) goto finish;

	// q_if and ifmap mapping
	memset(ifmap, 0, sizeof(ifmap));
	ifname_mapping(q_if, ifmap); // database interface mapping
	if(debug) dbg("%s: q_if=%s, ifmap=%s\n", __FUNCTION__, q_if, ifmap);
	snprintf(path, sizeof(path), "/jffs/traffic_control/%s/traffic.db", ifmap);

	ret = sqlite3_open(path, &db);
	if(ret){
		printf("%s : Can't open database %s\n", __FUNCTION__, sqlite3_errmsg(db));
		goto finish;
	}

	// get data
	memset(sql, 0, sizeof(sql));
	te = atoll(q_te);
	ts = atoll(q_ts);
	sprintf(sql, "SELECT ifname, SUM(tx), SUM(rx) FROM (SELECT timestamp, ifname, tx, rx FROM traffic WHERE timestamp BETWEEN %ld AND %ld) WHERE ifname = \"%s\"",
		(ts - 30), (te + 30), ifmap);

	if(sql_get_table(db, sql, &result, &rows, &cols) == SQLITE_OK)
	{
		int i = 0;
		int j = 0;
		int index = cols;
		for(i = 0; i < rows; i++){
			for(j = 0; j < cols; j++){
				printf("%s :[%3d/%3d] %16s\n", __FUNCTION__, i, j, result[index]);
				++index;
			}
		}
		sqlite3_free_table(result);
	}
		
finish:
	sqlite3_close(db);
	file_unlock(lock);
}

/*
	for traffic_control alert mail usage
*/
static
void traffic_control_queryreal()
{
	int lock;	// file lock
	int result = 0;
	int unit;
	int limit = 0;
	time_t now;
	char *start;
	char *g, *word, *dualwan; // wan / lan / usb
	char buf[64], end[32];
	char tmp2[4];

	memset(buf, 0, sizeof(buf));
	memset(end, 0, sizeof(end));
	memset(tmp2, 0, sizeof(tmp2));

	int debug = nvram_get_int("traffic_control_debug");
	lock = file_lock("traffic_control");
	
	start = nvram_safe_get("traffic_control_date_start");
	time(&now);
	sprintf(end, "%ld", now + 30); // for get realtime, to add 30 secs

	g = dualwan = strdup(nvram_safe_get("wans_dualwan"));
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
	{
		char *pos = NULL;
		char *old = NULL;
		char new[12];
		int size = 0;
		double real = 0;

		if((!g) || ((word = strsep(&g, " ")) == NULL)) continue;
		if(!strcmp(word, "none")) continue;
		if(!strcmp(end, "") || !strcmp(word, "")) start = NULL;

		traffic_cotrol_WanStat(buf, word, start, end, unit);

		old = buf;
		pos = strchr(old, ']');
		size = pos - old - 1;
		memset(new, 0, sizeof(new));
		strncpy(new, (old+1), size);
		real = atof(new);
		real = real / 1024 / 1024; // KB->GB
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "%.2f", real);

		// traffic control limit
		if(nvram_get_int("traffic_control_limit_enable") && (real >= nvram_get_int("traffic_control_limit_max")))
			limit = 1;
		else
			limit = 0;

		int count = nvram_get_int("traffic_control_count");

		if(unit == 0){
			nvram_set("traffic_control_realtime_0", buf);

			if(real >= nvram_get_int("traffic_control_alert_max") && ((count & 1) == 0)){
				if(debug) dbg("%s : primary add count\n", __FUNCTION__);
				count |= 1;
				result = 1;
			}

			// traffic control limit
			sprintf(tmp2, "%d", limit);
		}
		else if(unit == 1){
			nvram_set("traffic_control_realtime_1", buf);

			if(real >= nvram_get_int("traffic_control_alert_max") && ((count & 2) == 0)){
				if(debug) dbg("%s : secondary add count\n", __FUNCTION__);
				count |= 2;
				result = 1;
			}

			// traffic control limit
			sprintf(tmp2, "%s,%d", tmp2, limit);
		}

		nvram_set_int("traffic_control_count", count);

		if(debug) dbg("%s : word=%s, pos=%s, old=%s, size=%d, new=%s, real=%.2f, result = %d, count = %d, tmp2 = %s\n",
			 __FUNCTION__, word, pos, old, size, new, real, result, count, tmp2);
	}
	free(dualwan);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "echo -n %d > /tmp/traffic_control_alert_mail", result);
	system(buf);

	// traffic control limit
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "echo -n \"%s\" > /tmp/traffic_control_limit", tmp2);
	system(buf);

	file_unlock(lock);
}


/*
	when HW reboot, save last data into database
*/
static
void traffic_control_HW_reboot()
{
	sqlite3 *db;
	time_t now;	// timestamp
	int unit;
	int lock;	// file lock
	int ret;
	char *zErr = NULL;
	char path[IFPATH_MAX], rx_path[IFPATH_MAX];
	char *g, *word, *dualwan; // wan / lan / usb
	char ifmap[IFNAME_MAX];	// ifname after mapping
	char cmd[256];
	char tmp_t[64];
	char sql[256];
	char rx_buf[64];
	FILE *fn = NULL;
	unsigned long long current = 0;	// current

	lock = file_lock("traffic_control");
	
	g = dualwan = strdup(nvram_safe_get("wans_dualwan"));
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
	{
		memset(ifmap, 0, sizeof(ifmap));
		if((!g) || ((word = strsep(&g, " ")) == NULL)) continue;
		if(!strcmp(word, "none")) continue;
		ifname_mapping(word, ifmap); // database interface mapping

		snprintf(path, sizeof(path), "/jffs/traffic_control/%s/traffic.db", ifmap);
		snprintf(rx_path, sizeof(rx_path), "/jffs/traffic_control/%s/tmp", ifmap);
		snprintf(tmp_t, sizeof(tmp_t), "/jffs/traffic_control/%s", ifmap);

		memset(rx_buf, 0, sizeof(rx_buf));
		if((fn = fopen(rx_path, "r")) != NULL)
		{
			fgets(rx_buf, sizeof(rx_buf), fn);
			current = strtoull(rx_buf, NULL, 10);
			fclose(fn);
		}

		if(current != 0)
		{
			ret = sqlite3_open(path, &db);
			if(ret){
				printf("%s : Can't open database %s\n", __FUNCTION__, sqlite3_errmsg(db));
				sqlite3_close(db);
				file_unlock(lock);
				exit(1);
			}

			// get timestamp
			time(&now);
			memset(sql, 0, sizeof(sql));
			sprintf(sql, "INSERT INTO traffic VALUES ('%lu','%s','0','%llu')", now, ifmap, current);
			sqlite3_exec(db, sql, NULL, NULL, &zErr);
			if(zErr != NULL) sqlite3_free(zErr);

			memset(cmd, 0, sizeof(cmd));
			sprintf(cmd, "echo -n 0 > %s", rx_path);
			system(cmd);

			memset(cmd, 0, sizeof(cmd));
			sprintf(cmd, "echo -n 0 > %s/tx_t", tmp_t);
			system(cmd);

			memset(cmd, 0, sizeof(cmd));
			sprintf(cmd, "echo -n 0 > %s/rx_t", tmp_t);
			system(cmd);

		}
		sqlite3_close(db);
	}
	free(dualwan);
	file_unlock(lock);
}

/*
	q_if : query interface
	q_te : query end time
	q_ts : query start time
*/
int traffic_control_main(char *type, char *q_if, char *q_te, char *q_ts)
{
	if(!strcmp(type, "w"))
	{
		traffic_control_datawrite();
	}
	else if(!strcmp(type, "r"))
	{
		traffic_control_dataread(q_if, q_ts, q_te);
	}
	else if(!strcmp(type, "q"))
	{
		traffic_control_queryreal();
	}
	else if(!strcmp(type, "c"))
	{
		traffic_control_HW_reboot();
	}
	
	return 1;
}
