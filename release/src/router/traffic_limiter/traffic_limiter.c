/*
	for traffic limiter to write / read database
	
	NOTE:
		traffic unit in database : KB (Kbytes)
*/

#include "traffic_limiter.h"

static 
void traffic_limiter_datawrite(char *dualwan_mode)
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
	char sql[256];
	char tx_buf[64], rx_buf[64];
	char tmp[32];
	FILE *fp = NULL;
	unsigned long long tx = 0, rx = 0;	// current
	unsigned long long tx_t = 0, rx_t = 0;	// old
	unsigned long long tx_n = 0, rx_n = 0;	// diff

	int debug = nvram_get_int("tl_debug");
	long int date_start = nvram_get_int("tl_date_start");
	
	lock = file_lock("traffic_limiter");
	mkdir(TL_PATH, 0666);

	/* check daul wan mode */
	if (traffic_limiter_dualwan_check(dualwan_mode) == 0) {
		file_unlock(lock);
		return;
	}

	g = dualwan = strdup(nvram_safe_get("wans_dualwan"));
	for (unit = TL_UNIT_S; unit < TL_UNIT_E; ++unit)
	{
		memset(ifmap, 0, sizeof(ifmap));
		memset(wanif, 0, sizeof(wanif));
		if((!g) || ((word = strsep(&g, " ")) == NULL)) continue;
		if(!strcmp(word, "none")) continue;
		snprintf(wanif, sizeof(wanif), "wan%d_ifname", unit);
		ifname_mapping(word, ifmap); // database interface mapping

		snprintf(folder, sizeof(folder), TL_PATH"%s", ifmap);
		snprintf(path, sizeof(path), TL_PATH"%s/traffic.db", ifmap);
		snprintf(tx_path, sizeof(tx_path), TL_PATH"%s/tx_t", ifmap);
		snprintf(rx_path, sizeof(rx_path), TL_PATH"%s/rx_t", ifmap);
		
		if (!f_exists(path)) {
			mkdir(folder, 0666);
			eval("touch", path);
		}

		ret = sqlite3_open(path, &db);
		if (ret) {
			printf("%s : Can't open database %s\n", __FUNCTION__, sqlite3_errmsg(db));
			sqlite3_close(db);
			file_unlock(lock);
			exit(1);
		}

		// get timestamp
		time(&now);

		if (f_exists(path)) {
			// delete database out of last 1 months (DATA_PERIOD)
			// we allow 30 secs delay because of save data will be delay some secs
			memset(sql, 0, sizeof(sql));
			sprintf(sql, "DELETE FROM traffic WHERE timestamp NOT BETWEEN %ld AND %ld", (date_start - DATA_PERIOD - 30), (now + 30));
				if (sqlite3_exec(db, sql, NULL, NULL, &zErr) != SQLITE_OK) {
				if (zErr != NULL) {
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

		if (ret != SQLITE_OK)
		{
			if (zErr != NULL)
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

				if (f_read_string(tx_path, tx_buf, sizeof(tx_buf)) > 0)
					tx_t = strtoull(tx_buf, NULL, 10);
				else
					tx_t = 0;

				if (f_read_string(rx_path, rx_buf, sizeof(rx_buf)) > 0)
					rx_t = strtoull(rx_buf, NULL, 10);
				else
					rx_t = 0;

				// proc/net/dev max bytes is unsigned long (2^32)
				// diff traffic
				if (tx < tx_t)
					tx_n = (tx + LONGSIZE) - tx_t;
				else
					tx_n = tx - tx_t;
					
				if (rx < rx_t)
					rx_n = (rx + LONGSIZE) - rx_t;
				else
					rx_n = rx - rx_t;
				
				// individual interface tx_t
				sprintf(tmp, "%llu", tx);
				f_write_string(tx_path, tmp, 0, 0);

				// individual interface rx_t
				sprintf(tmp, "%llu", rx);
				f_write_string(rx_path, tmp, 0, 0);

				if (debug) dbg("%s : tx/tx_t/tx_n = %16llu/%16lld/%16lld\n", __FUNCTION__, tx, tx_t, tx_n);
				if (debug) dbg("%s : rx/rx_t/rx_n = %16llu/%16lld/%16lld\n", __FUNCTION__, rx, rx_t, rx_n);

				// debug message
				if (debug) dbg("%s : %12lu/%10s/%16llu/%16llu\n", __FUNCTION__, now, ifmap, tx_n, rx_n);
				memset(sql, 0, sizeof(sql));
				sprintf(sql, "INSERT INTO traffic VALUES ('%lu','%s','%llu','%llu')",
					now, ifmap, tx_n, rx_n);
				sqlite3_exec(db, sql, NULL, NULL, &zErr);
				if (zErr != NULL) sqlite3_free(zErr);
			}// while loop
			fclose(fp);
		}// record database
		sqlite3_close(db);
	}// create database for each interface
	free(dualwan);
	file_unlock(lock);
}

static 
void traffic_limiter_dataread(char *q_if, char *q_ts, char *q_te)
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

	int debug = nvram_get_int("tl_debug");
	lock = file_lock("traffic_limiter");

	if (q_if == NULL || q_ts == NULL || q_te == NULL) goto finish;

	// q_if and ifmap mapping
	memset(ifmap, 0, sizeof(ifmap));
	ifname_mapping(q_if, ifmap); // database interface mapping
	if (debug) dbg("%s: q_if=%s, ifmap=%s\n", __FUNCTION__, q_if, ifmap);
	snprintf(path, sizeof(path), TL_PATH"%s/traffic.db", ifmap);

	ret = sqlite3_open(path, &db);
	if (ret) {
		printf("%s : Can't open database %s\n", __FUNCTION__, sqlite3_errmsg(db));
		goto finish;
	}

	// get data
	memset(sql, 0, sizeof(sql));
	te = atoll(q_te);
	ts = atoll(q_ts);
	sprintf(sql, "SELECT ifname, SUM(tx), SUM(rx) FROM (SELECT timestamp, ifname, tx, rx FROM traffic WHERE timestamp BETWEEN %ld AND %ld) WHERE ifname = \"%s\"",
		(ts - 30), (te + 30), ifmap);

	if (sql_get_table(db, sql, &result, &rows, &cols) == SQLITE_OK)
	{
		int i = 0;
		int j = 0;
		int index = cols;
		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++) {
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
	for traffic_limiter alert mail usage
*/
static
void traffic_limiter_queryreal(char *dualwan_mode)
{
	int lock;	// file lock
	int unit;
	time_t now;
	char *start;
	char *g, *word, *dualwan; // wan / lan / usb
	char buf[64], end[32];
	char tmp[32], prefix[] = "tlXXXXXXXX_";
	char path[IFPATH_MAX];
	
	memset(buf, 0, sizeof(buf));
	memset(end, 0, sizeof(end));
	memset(tmp, 0, sizeof(tmp));

	int debug = nvram_get_int("tl_debug");
	lock = file_lock("traffic_limiter");
	start = nvram_safe_get("tl_date_start");

	/* check daul wan mode */
	if (traffic_limiter_dualwan_check(dualwan_mode) == 0) {
		file_unlock(lock);
		return;
	}

	g = dualwan = strdup(nvram_safe_get("wans_dualwan"));
	for (unit = TL_UNIT_S; unit < TL_UNIT_E; ++unit)
	{
		char *pos = NULL;
		char *old = NULL;
		char new[12];
		int size = 0;
		double real = 0;

		time(&now);
		sprintf(end, "%ld", now + 30); // for get realtime, to add 30 secs

		if ((!g) || ((word = strsep(&g, " ")) == NULL)) continue;
		if (!strcmp(word, "none")) continue;
		if (!strcmp(end, "") || !strcmp(word, "")) start = NULL;

		traffic_limiter_WanStat(buf, word, start, end, unit);

		old = buf;
		pos = strchr(old, ']');
		size = pos - old - 1;
		memset(new, 0, sizeof(new));
		strncpy(new, (old+1), size);
		real = atof(new);
		real = real / 1024 / 1024; // KB->GB
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "%.2f", real);

		// get different unit's setting
		snprintf(prefix, sizeof(prefix), "tl%d_", unit);
		int limit_s = nvram_get_int(strcat_r(prefix, "limit_enable", tmp));
		int alert_s = nvram_get_int(strcat_r(prefix, "alert_enable", tmp));
		double limit_v = atof(nvram_safe_get(strcat_r(prefix, "limit_max", tmp)));
		double alert_v = atof(nvram_safe_get(strcat_r(prefix, "alert_max", tmp)));
		if (debug) dbg("%s : unit=%d, ls=%d, lv=%.2f, as=%d, av=%.2f\n", __FUNCTION__, unit, limit_s, limit_v, alert_s, alert_v);

		// save realtime traffic
		snprintf(path, sizeof(path), "/tmp/tl%d_realtime", unit);
		f_write_string(path, buf, 0, 0);

		// trigger limit line
		if (limit_s && (real > limit_v)) traffic_limiter_set_bit("limit", unit);

		// trigger alert line
		if (alert_s && (real > alert_v)) traffic_limiter_set_bit("alert", unit);
	}
	free(dualwan);
	file_unlock(lock);
}


/*
	when HW reboot, save last data into database
*/
static
void traffic_limiter_HW_reboot(char *dualwan_mode)
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
	char tmp_t[64];
	char sql[256];
	char rx_buf[64];
	unsigned long long current = 0;	// current

	lock = file_lock("traffic_limiter");

	/* check daul wan mode */
	if (traffic_limiter_dualwan_check(dualwan_mode) == 0) {
		file_unlock(lock);
		return;
	}

	g = dualwan = strdup(nvram_safe_get("wans_dualwan"));
	for (unit = TL_UNIT_S; unit < TL_UNIT_E; ++unit)
	{
		memset(ifmap, 0, sizeof(ifmap));
		if ((!g) || ((word = strsep(&g, " ")) == NULL)) continue;
		if (!strcmp(word, "none")) continue;
		ifname_mapping(word, ifmap); // database interface mapping

		snprintf(path, sizeof(path), TL_PATH"%s/traffic.db", ifmap);
		snprintf(rx_path, sizeof(rx_path), TL_PATH"%s/tmp", ifmap);
		snprintf(tmp_t, sizeof(tmp_t), TL_PATH"%s", ifmap);

		memset(rx_buf, 0, sizeof(rx_buf));
		if (f_read_string(rx_path, rx_buf, sizeof(rx_buf)) > 0)
			current = strtoull(rx_buf, NULL, 10);

		if (current)
		{
			ret = sqlite3_open(path, &db);
			if (ret) {
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
			if (zErr != NULL) sqlite3_free(zErr);

			f_write_string(rx_path, "0", 0, 0);

			sprintf(rx_path, "%s/tx_t", tmp_t);
			f_write_string(rx_path, "0", 0, 0);

			sprintf(rx_path, "%s/rx_t", tmp_t);
			f_write_string(rx_path, "0", 0, 0);

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
int traffic_limiter_main(char *type, char *q_if, char *q_te, char *q_ts)
{
	char *dualwan_mode = nvram_safe_get("wans_mode");

	if (!strcmp(type, "w"))
	{
		traffic_limiter_datawrite(dualwan_mode);
	}
	else if (!strcmp(type, "r"))
	{
		traffic_limiter_dataread(q_if, q_ts, q_te);
	}
	else if (!strcmp(type, "q"))
	{
		traffic_limiter_queryreal(dualwan_mode);
	}
	else if (!strcmp(type, "c"))
	{
		traffic_limiter_HW_reboot(dualwan_mode);
	}
	
	return 1;
}
