#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <utils.h>
#include <shutils.h>
#include <shared.h>
#include <sys/stat.h>
#include <bcmnvram.h>

#include "httpd.h"
#include "sqlite3.h"

#define LONGSIZE		4194304 // 2^22 (KB)

#define DATA_PERIOD		2592000	// 1 months = 30*24*60*60
#define TRAFFIC_CONTROL_FOLDER	"/jffs/traffic_control/"
#define TRAFFIC_CONTROL_DATA	"/jffs/traffic_control/traffic_control.db"
#define DAY			3600*24

// traffic_control.c
extern int traffic_control_main(char *type, char *q_if, char *q_te, char *q_ts);

// traffic_control_hook.c
extern int sql_get_table(sqlite3 *db, const char *sql, char ***pazResult, int *pnRow, int *pnColumn);
extern void traffic_control_hook(char *ifname, char *start, char *end, int *retval, webs_t wp);
extern void traffic_cotrol_WanStat(char *buf, char *ifname, char *ifmap, char *start, char *end);
extern void ifname_mapping(char *word, char *ifname);
