#define TRAFFIC_DB_PATH "/jffs/traffic.db"
#define MON_SEC 86400 * 31
#define DAY_SEC 86400
#define HOURSEC 3600

// table.c
extern int sql_get_table(sqlite3 *db, const char *sql, char ***pazResult, int *pnRow, int *pnColumn);

// sqlite_stat.c
extern void sqlite_Stat_hook(int type, char *client, char *mode, char *dura, char *date, int *retval, webs_t wp);
