/*

	rstats
	Copyright (C) 2006-2009 Jonathan Zarate


	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <stdint.h>
#include <syslog.h>

#include <bcmnvram.h>
#include <shutils.h>

//#define DEBUG_NOISY
//#define DEBUG_STIME

#include <shared.h>

#ifdef RTCONFIG_ISP_METER
	#include <rtstate.h>
#endif

#ifdef RTCONFIG_QTN
#include "web-qtn.h"
#endif

//#define DEBUG
#define K 1024
#define M (1024 * 1024)
#define G (1024 * 1024 * 1024)

#define SMIN	60
#define	SHOUR	(60 * 60)
#define	SDAY	(60 * 60 * 24)
#define Y2K		946684800UL

#define INTERVAL		30

#ifdef RTCONFIG_ISP_METER
	#define MTD_WRITE_INTERVEL	60
#endif

#define MAX_NSPEED		((24 * SHOUR) / INTERVAL)
#define MAX_NDAILY		62
#define MAX_NMONTHLY	25
#define MAX_SPEED_IF	10
#define MAX_ROLLOVER	(225 * M)

#define MAX_COUNTER	2
#define RX 			0
#define TX 			1

#define DAILY		0
#define MONTHLY		1

#define ID_V0		0x30305352
#define ID_V1		0x31305352
#define CURRENT_ID	ID_V1

#define HI_BACK		5

#define RA_OFFSET_ISP_METER	0x4FF00

typedef struct {
	uint32_t xtime;
	uint64_t counter[MAX_COUNTER];
} data_t;

typedef struct {
	uint32_t id;

	data_t daily[MAX_NDAILY];
	int dailyp;

	data_t monthly[MAX_NMONTHLY];
	int monthlyp;
} history_t;

typedef struct {
	uint32_t id;

	data_t daily[62];
	int dailyp;

	data_t monthly[12];
	int monthlyp;
} history_v0_t;

typedef struct {
	char ifname[12];
	long utime;
	unsigned long speed[MAX_NSPEED][MAX_COUNTER];
	unsigned long last[MAX_COUNTER];
	int tail;
	int sync;
} speed_t;

history_t history;
speed_t speed[MAX_SPEED_IF];
int speed_count;
long save_utime;
char save_path[96];
long current_uptime;

volatile int gothup = 0;
volatile int gotuser = 0;
volatile int gotterm = 0;

const char history_fn[] = "/var/lib/misc/rstats-history";
const char speed_fn[] = "/var/lib/misc/rstats-speed";
const char uncomp_fn[] = "/var/tmp/rstats-uncomp";
const char source_fn[] = "/var/lib/misc/rstats-source";

#ifdef RTCONFIG_ISP_METER
#define ISP_METER_FILE	"/jffs/isp_meter"
int isp_limit, isp_limit_time;
unsigned long last_day_rx, last_day_tx, last_month_rx, last_month_tx;
unsigned long today_rx, today_tx, month_rx, month_tx;
unsigned long reset_base_day_rx, reset_base_day_tx, reset_base_month_rx, reset_base_month_tx;
long cur_conn_time, last_connect_time, total_connect_time, reset_base_time;

void reset_traffic_meter_counter(){
	FILE *fp;
        last_day_rx = 0;
        last_day_tx = 0;
        last_month_rx = 0;
        last_month_tx = 0;
        today_rx = 0;
        today_tx = 0;
        month_rx = 0;
        month_tx = 0;
        reset_base_day_rx = (history.daily[history.dailyp].counter[0]/K);
        reset_base_day_tx = (history.daily[history.dailyp].counter[1]/K);
        reset_base_month_rx = (history.monthly[history.monthlyp].counter[0]/K);
        reset_base_month_tx = (history.monthly[history.monthlyp].counter[1]/K);
        nvram_set("isp_day_tx", "0");
        nvram_set("isp_day_rx", "0");
        nvram_set("isp_month_tx", "0");
        nvram_set("isp_month_rx", "0");
        isp_limit = nvram_get_int("isp_limit");
	isp_limit_time = nvram_get_int("isp_limit_time");

	reset_base_time = cur_conn_time;
	last_connect_time = 0;
	total_connect_time = 0;

        set_meter_file("isp_meter:0,0,0,end");

        if(!nvram_match("isp_meter", "disable")
          && !(nvram_match("wan0_state_t", "2") && nvram_match("wan0_auxstate_t", "0")) ) {
                notify_rc_and_wait("isp_meter up");
        }
}

void get_meter_file(char *meter_buf) 
{
#ifdef CONFIG_BCMWL5 
	FILE *fp;
        if (fp=fopen(ISP_METER_FILE, "r")) {
                fgets(meter_buf, sizeof(meter_buf), fp);
                fclose(fp);
	}
#else
	FRead(meter_buf, RA_OFFSET_ISP_METER, 64);
#endif
	_dprintf("meter_buf: %s\n", meter_buf);
	return;
}

int set_meter_file(char *meter_buf)
{
	if(meter_buf == NULL)
		return 0;
#ifdef CONFIG_BCMWL5 
        FILE *fp;
        if (fp=fopen(ISP_METER_FILE, "w")) {
               fprintf(fp, "%s", meter_buf);
               fclose(fp);
	}
#else
	FWrite(meter_buf, RA_OFFSET_ISP_METER, sizeof(meter_buf));
#endif
	return 1;
}
#endif

static int get_stime(void)
{
#ifdef DEBUG_STIME
	return 90;
#else
	int t;
	t = nvram_get_int("rstats_stime");
	if (t < 1) t = 1;
		else if (t > 8760) t = 8760;
	return t * SHOUR;
#endif
}

static int comp(const char *path, void *buffer, int size)
{
	char s[256];

	if (f_write(path, buffer, size, 0, 0) != size) return 0;

	sprintf(s, "%s.gz", path);
	unlink(s);

	sprintf(s, "gzip %s", path);
	return system(s) == 0;
}

static void save(int quick)
{
	int i;
	char *bi, *bo;
	int n;
	int b;
	char hgz[256];
	char tmp[256];
	char bak[256];
	char bkp[256];
	time_t now;
	struct tm *tms;
	static int lastbak = -1;

	//_dprintf("%s: quick=%d\n", __FUNCTION__, quick);

	f_write("/var/lib/misc/rstats-stime", &save_utime, sizeof(save_utime), 0, 0);

	comp(speed_fn, speed, sizeof(speed[0]) * speed_count);

/*
	if ((now = time(0)) < Y2K) {
		_dprintf("%s: time not set\n", __FUNCTION__);
		return;
	}
*/

	comp(history_fn, &history, sizeof(history));

	//_dprintf("%s: write source=%s\n", __FUNCTION__, save_path);
	f_write_string(source_fn, save_path, 0, 0);

	if (quick) {
		return;
	}

	sprintf(hgz, "%s.gz", history_fn);

	if (strcmp(save_path, "*nvram") == 0) {
		if (!wait_action_idle(10)) {
			_dprintf("%s: busy, not saving\n", __FUNCTION__);
			return;
		}

		if ((n = f_read_alloc(hgz, &bi, 20 * 1024)) > 0) {
			if ((bo = malloc(base64_encoded_len(n) + 1)) != NULL) {
				n = base64_encode(bi, bo, n);
				bo[n] = 0;
				nvram_set("rstats_data", bo);
				if (!nvram_match("debug_nocommit", "1")) nvram_commit();

				_dprintf("%s: nvram commit\n", __FUNCTION__);

				free(bo);
			}
		}
		free(bi);
	}
	else if (save_path[0] != 0) {
		strcpy(tmp, save_path);
		strcat(tmp, ".tmp");

		for (i = 15; i > 0; --i) {
			if (!wait_action_idle(10)) {
				_dprintf("%s: busy, not saving\n", __FUNCTION__);
			}
			else {
				_dprintf("%s: cp %s %s\n", __FUNCTION__, hgz, tmp);
				if (eval("cp", hgz, tmp) == 0) {
					_dprintf("%s: copy ok\n", __FUNCTION__);

					if (!nvram_match("rstats_bak", "0")) {
						now = time(0);
						tms = localtime(&now);
						if (lastbak != tms->tm_yday) {
							strcpy(bak, save_path);
							n = strlen(bak);
							if ((n > 3) && (strcmp(bak + (n - 3), ".gz") == 0)) n -= 3;
							strcpy(bkp, bak);
							for (b = HI_BACK-1; b > 0; --b) {
								sprintf(bkp + n, "_%d.bak", b + 1);
								sprintf(bak + n, "_%d.bak", b);
								rename(bak, bkp);
							}
							if (eval("cp", "-p", save_path, bak) == 0) lastbak = tms->tm_yday;
						}
					}

					_dprintf("%s: rename %s %s\n", __FUNCTION__, tmp, save_path);
					if (rename(tmp, save_path) == 0) {
						_dprintf("%s: rename ok\n", __FUNCTION__);
						break;
					}
				}
			}

			// might not be ready
			sleep(3);
			if (gotterm) break;
		}
	}
}

static int decomp(const char *fname, void *buffer, int size, int max)
{
	char s[256];
	int n;

	_dprintf("%s: fname=%s\n", __FUNCTION__, fname);

	unlink(uncomp_fn);

	n = 0;
	sprintf(s, "gzip -dc %s > %s", fname, uncomp_fn);
	if (system(s) == 0) {
		n = f_read(uncomp_fn, buffer, size * max);
		_dprintf("%s: n=%d\n", __FUNCTION__, n);
		if (n <= 0) n = 0;
			else n = n / size;
	}
	else {
		_dprintf("%s: %s != 0\n", __FUNCTION__, s);
	}
	unlink(uncomp_fn);
	memset((char *)buffer + (size * n), 0, (max - n) * size);
	return n;
}

static void clear_history(void)
{
	memset(&history, 0, sizeof(history));
	history.id = CURRENT_ID;
}

static int load_history(const char *fname)
{
	history_t hist;

	_dprintf("%s: fname=%s\n", __FUNCTION__, fname);

	if ((decomp(fname, &hist, sizeof(hist), 1) != 1) || (hist.id != CURRENT_ID)) {
		history_v0_t v0;

		if ((decomp(fname, &v0, sizeof(v0), 1) != 1) || (v0.id != ID_V0)) {
			_dprintf("%s: load failed\n", __FUNCTION__);
			return 0;
		}
		else {
			// --- temp conversion ---
			clear_history();

			// V0 -> V1
			history.id = CURRENT_ID;
			memcpy(history.daily, v0.daily, sizeof(history.daily));
			history.dailyp = v0.dailyp;
			memcpy(history.monthly, v0.monthly, sizeof(v0.monthly));	// v0 is just shorter
			history.monthlyp = v0.monthlyp;
		}
	}
	else {
		memcpy(&history, &hist, sizeof(history));
	}

	_dprintf("%s: dailyp=%d monthlyp=%d\n", __FUNCTION__, history.dailyp, history.monthlyp);
	return 1;
}

/* Try loading from the backup versions.
 * We'll try from oldest to newest, then
 * retry the requested one again last.  In case the drive mounts while
 * we are trying to find a good version.
 */
static int try_hardway(const char *fname)
{
	char fn[256];
	int n, b, found = 0;

	strcpy(fn, fname);
	n = strlen(fn);
	if ((n > 3) && (strcmp(fn + (n - 3), ".gz") == 0))
		n -= 3;
	for (b = HI_BACK; b > 0; --b) {
		sprintf(fn + n, "_%d.bak", b);
		found |= load_history(fn);
	}
	found |= load_history(fname);

	return found;
}

static void load_new(void)
{
	char hgz[256];

	sprintf(hgz, "%s.gz.new", history_fn);
	if (load_history(hgz)) save(0);
	unlink(hgz);
}

static void load(int new)
{
	int i;
	long t;
	char *bi, *bo;
	int n;
	char hgz[256];
	char sp[sizeof(save_path)];
	unsigned char mac[6];

	current_uptime = uptime();

	strlcpy(save_path, nvram_safe_get("rstats_path"), sizeof(save_path) - 32);
	if (((n = strlen(save_path)) > 0) && (save_path[n - 1] == '/')) {
#ifdef RTCONFIG_RGMII_BRCM5301X
		ether_atoe(nvram_safe_get("et1macaddr"), mac);
#else
		ether_atoe(nvram_safe_get("et0macaddr"), mac);
#endif
		sprintf(save_path + n, "tomato_rstats_%02x%02x%02x%02x%02x%02x.gz",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	if (f_read("/var/lib/misc/rstats-stime", &save_utime, sizeof(save_utime)) != sizeof(save_utime)) {
		save_utime = 0;
	}
	t = current_uptime + get_stime();
	if ((save_utime < current_uptime) || (save_utime > t)) save_utime = t;
	//_dprintf("%s: uptime = %dm, save_utime = %dm\n", __FUNCTION__, current_uptime / 60, save_utime / 60);

	//

	sprintf(hgz, "%s.gz", speed_fn);
	speed_count = decomp(hgz, speed, sizeof(speed[0]), MAX_SPEED_IF);
	_dprintf("%s: speed_count = %d\n", __FUNCTION__, speed_count);

	for (i = 0; i < speed_count; ++i) {
		if (speed[i].utime > current_uptime) {
			speed[i].utime = current_uptime;
			speed[i].sync = 1;
		}
	}

	//

	sprintf(hgz, "%s.gz", history_fn);

	if (new) {
		unlink(hgz);
		save_utime = 0;
		return;
	}

	f_read_string(source_fn, sp, sizeof(sp));	// always terminated
	_dprintf("%s: read source=%s save_path=%s\n", __FUNCTION__, sp, save_path);
	if ((strcmp(sp, save_path) == 0) && (load_history(hgz))) {
		_dprintf("%s: using local file\n", __FUNCTION__);
		return;
	}

	if (save_path[0] != 0) {
		if (strcmp(save_path, "*nvram") == 0) {
			if (!wait_action_idle(60)) exit(0);

			bi = nvram_safe_get("rstats_data");
			if ((n = strlen(bi)) > 0) {
				if ((bo = malloc(base64_decoded_len(n))) != NULL) {
					n = base64_decode(bi, bo, n);
					_dprintf("%s: nvram n=%d\n", __FUNCTION__, n);
					f_write(hgz, bo, n, 0, 0);
					free(bo);
					load_history(hgz);
				}
			}
		}
		else {
			i = 1;
			while (1) {
				if (wait_action_idle(10)) {

					// cifs quirk: try forcing refresh
					eval("ls", save_path);

					/* If we can't access the path, keep trying - maybe it isn't mounted yet.
					 * If we can, and we can sucessfully load it, oksy.
					 * If we can, and we cannot load it, then maybe it has been deleted, or
					 * maybe it's corrupted (like 0 bytes long).
					 * In these cases, try the backup files.
					 */
					if (load_history(save_path) || try_hardway(save_path)) {
						f_write_string(source_fn, save_path, 0, 0);
						break;
					}
				}

				// not ready...
				sleep(i);
				if ((i *= 2) > 900) i = 900;	// 15m

				if (gotterm) {
					save_path[0] = 0;
					return;
				}

				if (i > (3 * 60)) {
					syslog(LOG_WARNING, "Problem loading %s. Still trying...", save_path);
				}
			}
		}
	}
}

static void save_speedjs(long next)
{
	int i, j, k;
	speed_t *sp;
	int p;
	FILE *f;
	uint64_t total;
	uint64_t tmax;
	unsigned long n;
	char c;

	if ((f = fopen("/var/tmp/rstats-speed.js", "w")) == NULL) return;

	_dprintf("%s: speed_count = %d\n", __FUNCTION__, speed_count);

	fprintf(f, "\nspeed_history = {\n");

	for (i = 0; i < speed_count; ++i) {
		sp = &speed[i];
		fprintf(f, "%s'%s': {\n", i ? " },\n" : "", sp->ifname);
		for (j = 0; j < MAX_COUNTER; ++j) {
			total = tmax = 0;
			fprintf(f, "%sx: [", j ? ",\n t" : " r");
			p = sp->tail;
			for (k = 0; k < MAX_NSPEED; ++k) {
				p = (p + 1) % MAX_NSPEED;
				n = sp->speed[p][j];
				fprintf(f, "%s%lu", k ? "," : "", n);
				total += n;
				if (n > tmax) tmax = n;
			}
			fprintf(f, "],\n");

			c = j ? 't' : 'r';
			fprintf(f, " %cx_avg: %llu,\n %cx_max: %llu,\n %cx_total: %llu",
				c, total / MAX_NSPEED, c, tmax, c, total);
		}
	}
	fprintf(f, "%s_next: %ld};\n", speed_count ? "},\n" : "", ((next >= 1) ? next : 1));
	fclose(f);

	rename("/var/tmp/rstats-speed.js", "/var/spool/rstats-speed.js");
}


static void save_datajs(FILE *f, int mode)
{
	data_t *data;
	int p;
	int max;
	int k, kn;

_dprintf("save_datajs:\n");
	fprintf(f, "\n%s_history = [\n", (mode == DAILY) ? "daily" : "monthly");

	if (mode == DAILY) {
		data = history.daily;
		p = history.dailyp;
		max = MAX_NDAILY;
_dprintf("DAILY: p= %d\n", p);
	}
	else {
		data = history.monthly;
		p = history.monthlyp;
		max = MAX_NMONTHLY;
_dprintf("MONTHLY: p= %d\n", p);
	}
	kn = 0;
	for (k = max; k > 0; --k) {
		p = (p + 1) % max;
		if (data[p].xtime == 0) continue;
		fprintf(f, "%s[0x%lx,0x%llx,0x%llx]", kn ? "," : "",
			(unsigned long)data[p].xtime, data[p].counter[0] / K, data[p].counter[1] / K);
		++kn;
_dprintf("%d:: [0x%lx,0x%llx,0x%llx]\n", p, 
	(unsigned long)data[p].xtime, data[p].counter[0] / K, data[p].counter[1] / K);
	}
	fprintf(f, "];\n");
}

static void save_histjs(void)
{
	FILE *f;

	if ((f = fopen("/var/tmp/rstats-history.js", "w")) != NULL) {
		save_datajs(f, DAILY);
		save_datajs(f, MONTHLY);
		fclose(f);
		rename("/var/tmp/rstats-history.js", "/var/spool/rstats-history.js");
	}
}


static void bump(data_t *data, int *tail, int max, uint32_t xnow, unsigned long *counter)
{
	int t, i;
	t = *tail;
	if (data[t].xtime != xnow) {
		for (i = max - 1; i >= 0; --i) {
			if (data[i].xtime == xnow) {
				t = i;
				break;
			}
		}
		if (i < 0) {
			*tail = t = (t + 1) % max;
			data[t].xtime = xnow;
			memset(data[t].counter, 0, sizeof(data[0].counter));
		}
	}
	for (i = 0; i < MAX_COUNTER; ++i) {
		data[t].counter[i] += counter[i];
	}
}

static void calc(void)
{
	FILE *f;
	char buf[256];
	char *ifname;
	char ifname_desc[12], ifname_desc2[12];
	char *p;
	unsigned long counter[MAX_COUNTER];
	unsigned long rx2, tx2;
	speed_t *sp;
	int i, j;
	time_t now;
	time_t mon;
	struct tm *tms;
	uint32_t c;
	uint32_t sc;
	unsigned long diff;
	long tick;
	int n;
	char *exclude;
#ifdef RTCONFIG_ISP_METER
        char traffic[64];
#endif
#ifdef RTCONFIG_QTN
	qcsapi_unsigned_int l_counter_value;
#endif

	rx2 = 0;
	tx2 = 0;
	now = time(0);
	exclude = nvram_safe_get("rstats_exclude");

	if ((f = fopen("/proc/net/dev", "r")) == NULL) return;
	fgets(buf, sizeof(buf), f);	// header
	fgets(buf, sizeof(buf), f);	// "
	while (fgets(buf, sizeof(buf), f)) {
		if ((p = strchr(buf, ':')) == NULL) continue;
			//_dprintf("\n=== %s\n", buf);
		*p = 0;
		if ((ifname = strrchr(buf, ' ')) == NULL) ifname = buf;
			else ++ifname;
		if ((strcmp(ifname, "lo") == 0) || (find_word(exclude, ifname))) continue;

		// <rx bytes, packets, errors, dropped, fifo errors, frame errors, compressed, multicast><tx ...>
		if (sscanf(p + 1, "%lu%*u%*u%*u%*u%*u%*u%*u%lu", &counter[0], &counter[1]) != 2) continue;

		if(!netdev_calc(ifname, ifname_desc, &counter[0], &counter[1], ifname_desc2, &rx2, &tx2))
			continue;
		//_dprintf(">>> %s, %s, %lu, %lu, %s, %lu, %lu, %s <<<\n",ifname, ifname_desc, counter[0], counter[1], ifname_desc2, rx2, tx2);
#ifdef RTCONFIG_QTN		
		if (!strcmp(ifname, nvram_safe_get("wl_ifname")))
				strcpy(ifname_desc2, "WIRELESS1");
#endif			
loopagain:

		sp = speed;
		for (i = speed_count; i > 0; --i) {
			if (strcmp(sp->ifname, ifname_desc) == 0) break;
			++sp;
		}

		if (i == 0) {
			if (speed_count >= MAX_SPEED_IF) continue;

			//_dprintf("%s: add %s as #%d\n", __FUNCTION__, ifname_desc, speed_count);

			i = speed_count++;
			sp = &speed[i];
			memset(sp, 0, sizeof(*sp));
			strcpy(sp->ifname, ifname_desc);
			sp->sync = 1;
			sp->utime = current_uptime;
		}
		if (sp->sync) {
			//_dprintf("%s: sync %s\n", __FUNCTION__, ifname_desc);
			sp->sync = -1;

			memcpy(sp->last, counter, sizeof(sp->last));
			memset(counter, 0, sizeof(counter));
		}
		else {

			sp->sync = -1;

			tick = current_uptime - sp->utime;
			n = tick / INTERVAL;

			sp->utime += (n * INTERVAL);
			//_dprintf("%s: %s n=%d tick=%d\n", __FUNCTION__, ifname, n, tick);

			for (i = 0; i < MAX_COUNTER; ++i) {
				c = counter[i];
				sc = sp->last[i];
				if (c < sc) {
					diff = (0xFFFFFFFF - sc + 1) + c;
					if (diff > MAX_ROLLOVER) diff = 0;
				}
				else {
					 diff = c - sc;
				}
				sp->last[i] = c;
				counter[i] = diff;
			}

			for (j = 0; j < n; ++j) {
				sp->tail = (sp->tail + 1) % MAX_NSPEED;
				for (i = 0; i < MAX_COUNTER; ++i) {
					sp->speed[sp->tail][i] = counter[i] / n;
				}
			}
		}		

		// todo: split, delay

		if (now > Y2K && strcmp(ifname_desc, "INTERNET")==0) {	
			/* Skip this if the time&date is not set yet */
			/* Skip non-INTERNET interface only 	     */
			tms = localtime(&now);
			bump(history.daily, &history.dailyp, MAX_NDAILY,
				(tms->tm_year << 16) | ((uint32_t)tms->tm_mon << 8) | tms->tm_mday, counter);
			n = nvram_get_int("rstats_offset");
			if ((n < 1) || (n > 31)) n = 1;
			mon = now + ((1 - n) * (60 * 60 * 24));
			tms = localtime(&mon);
			bump(history.monthly, &history.monthlyp, MAX_NMONTHLY,
				(tms->tm_year << 16) | ((uint32_t)tms->tm_mon << 8), counter);
#ifdef RTCONFIG_ISP_METER
			today_rx = last_day_rx + (history.daily[history.dailyp].counter[0]/K);
			today_tx = last_day_tx + (history.daily[history.dailyp].counter[1]/K);
			memset(traffic, 0, 64);
			sprintf(traffic, "%lu", today_rx);
			nvram_set("isp_day_rx", traffic);
			memset(traffic, 0, 64);
			sprintf(traffic, "%lu", today_tx);
			nvram_set("isp_day_tx", traffic);
			month_rx = last_month_rx + (history.monthly[history.monthlyp].counter[0]/K) - reset_base_month_rx;
			month_tx = last_month_tx + (history.monthly[history.monthlyp].counter[1]/K) - reset_base_month_tx;
			memset(traffic, 0, 64);
			sprintf(traffic, "%lu", month_rx);
			nvram_set("isp_month_rx", traffic);
			memset(traffic, 0, 64);
			sprintf(traffic, "%lu", month_tx);
			nvram_set("isp_month_tx", traffic);
#ifdef DEBUG
_dprintf("CUR MONTH Rx= %lu = %lu + %llu - %lu\n",month_rx,last_month_rx,(history.monthly[history.monthlyp].counter[0]/K), reset_base_month_rx);
_dprintf("CUR MONTH Tx= %lu = %lu + %llu - %lu\n",month_tx,last_month_tx,(history.monthly[history.monthlyp].counter[0]/K), reset_base_month_tx);
#endif
#endif
		}

#ifdef RTCONFIG_QTN  //RT-AC87
		if(!rpc_qtn_ready())	continue;
		if (strlen(ifname_desc2)) 
		{
			strcpy(ifname_desc, ifname_desc2);
			qcsapi_interface_get_counter(WIFINAME, qcsapi_total_bytes_received, &l_counter_value);
			counter[0] = l_counter_value;
			qcsapi_interface_get_counter(WIFINAME, qcsapi_total_bytes_sent, &l_counter_value);
			counter[1] = l_counter_value;
			strcpy(ifname_desc2, "");
			goto loopagain;
		}
#else
		if (strlen(ifname_desc2)) 
		{
			strcpy(ifname_desc, ifname_desc2);
			counter[0] = rx2;
			counter[1] = tx2;
                        strcpy(ifname_desc2, "");
                        goto loopagain;
		}
#endif

	}			
	fclose(f);
			
	// cleanup stale entries
	for (i = 0; i < speed_count; ++i) {
		sp = &speed[i];
		if (sp->sync == -1) {
			sp->sync = 0;
			continue;
		}
		if (((current_uptime - sp->utime) > (10 * SMIN)) || (find_word(exclude, sp->ifname))) {
			_dprintf("%s: #%d removing. > time limit or excluded\n", __FUNCTION__, i);
			--speed_count;
			memcpy(sp, sp + 1, (speed_count - i) * sizeof(speed[0]));
		}
		else {
			_dprintf("%s: %s not found setting sync=1\n", __FUNCTION__, sp->ifname);
			sp->sync = 1;
		}
	}

	// todo: total > user
	if (current_uptime >= save_utime) {
		save(0);
		save_utime = current_uptime + get_stime();
		_dprintf("%s: uptime = %dm, save_utime = %dm\n", __FUNCTION__, current_uptime / 60, save_utime / 60);
	}
}

static void sig_handler(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		gotterm = 1;
		break;
	case SIGHUP:
		gothup = 1;
		break;
	case SIGUSR1:
		gotuser = 1;
		break;
	case SIGUSR2:
		gotuser = 2;
		break;
	}
}

int main(int argc, char *argv[])
{
	struct sigaction sa;
	long z;
	int new;
#ifdef RTCONFIG_ISP_METER
	long zzz, pppd_uptime, pppd_conntime, isp_connect_time; 
	FILE *fp;
	char isp_meter_buf[64];
	unsigned long isp_rx, isp_tx;
	int get_connect_time;
	struct timeval timenow;
#endif

	printf("rstats\nCopyright (C) 2006-2009 Jonathan Zarate\n\n");

	if (fork() != 0) return 0;

	openlog("rstats", LOG_PID, LOG_USER);

	new = 0;
	if (argc > 1) {
		if (strcmp(argv[1], "--new") == 0) {
			new = 1;
			_dprintf("new=1\n");
		}
	}

	clear_history();
	unlink("/var/tmp/rstats-load");

	sa.sa_handler = sig_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
#ifdef RTCONFIG_ISP_METER
        signal(SIGTSTP, reset_traffic_meter_counter);

	get_connect_time = 0;
        reset_base_day_rx = 0;
        reset_base_day_tx = 0;
        reset_base_month_rx = 0;
        reset_base_month_tx = 0;
        isp_limit = nvram_get_int("isp_limit");
        last_day_rx = nvram_get_int("isp_day_rx");
        last_day_tx = nvram_get_int("isp_day_tx");
        last_month_rx = nvram_get_int("isp_month_rx");
        last_month_tx = nvram_get_int("isp_month_tx");
	isp_limit_time = nvram_get_int("isp_limit_time");
	last_connect_time = nvram_get_int("isp_connect_time");
#ifdef DEBUG
_dprintf("rstats get last data from nvram:\n");
_dprintf("day rx= %lu\n", last_day_rx);
_dprintf("day tx= %lu\n", last_day_tx);
_dprintf("mon rx= %lu\n", last_month_rx);
_dprintf("mon tx= %lu\n", last_month_tx);
_dprintf("limit = %d\n", isp_limit);
_dprintf("L_time= %ld\n",isp_limit_time);
#endif
	memset(isp_meter_buf, 0, sizeof(isp_meter_buf));
        get_meter_file(isp_meter_buf);
	if(isp_meter_buf!=NULL) {
                if (sscanf(isp_meter_buf, "isp_meter:%lu,%lu,%ld,end", &isp_rx, &isp_tx, &isp_connect_time) == 3) {
                        _dprintf("isp_rx= %lu, isp_tx= %lu, isp_connent_time= %ld\n", 
				 isp_rx, isp_tx, isp_connect_time);
                        if((isp_rx>last_month_rx)&&(isp_tx>last_month_tx)){
                                last_month_rx = isp_rx;
				last_month_tx = isp_tx;
                        }
			if(isp_connect_time > last_connect_time)
				last_connect_time = isp_connect_time;
                }
        }
	else {
		isp_rx = 0;
		isp_tx = 0;
		last_connect_time = 0;
	}
#ifdef DEBUG
_dprintf("Update last data from mtd:\n");
_dprintf("mon rx= %lu\n", last_month_rx);
_dprintf("mon tx= %lu\n", last_month_tx);
_dprintf("last_t= %ld\n", last_connect_time);
#endif
	month_rx = last_month_rx;
	month_tx = last_month_tx;

	zzz = current_uptime = uptime();
#endif
	load(new);

	z = current_uptime = uptime();

	while (1) {
		while (current_uptime < z) {
			sleep(z - current_uptime);
			if (gothup) {
				if (unlink("/var/tmp/rstats-load") == 0) load_new();
					else save(0);
				gothup = 0;
			}
			if (gotterm) {
				save(!nvram_match("rstats_sshut", "1"));
				exit(0);
			}
			if (gotuser == 1) {
				save_speedjs(z - uptime());
				gotuser = 0;
			}
			else if (gotuser == 2) {
				save_histjs();
				gotuser = 0;
			}
			current_uptime = uptime();
		}
		calc();
		z += INTERVAL;

#ifdef RTCONFIG_ISP_METER
		gettimeofday(&timenow, NULL);
#ifdef DEBUG
		_dprintf("********************************\n");
		_dprintf(" now time          = %ld\n", timenow.tv_sec);
#endif
		if((fp=fopen("/var/pppd_time", "r"))!=NULL) {
			fgets(isp_meter_buf, sizeof(isp_meter_buf), fp);
		        fclose(fp);
			if( sscanf(isp_meter_buf, "uptime:%ld", &pppd_uptime) == 1 ) {
				cur_conn_time = timenow.tv_sec - pppd_uptime;
#ifdef DEBUG
				_dprintf(" up time           = %ld\n", pppd_uptime);
				_dprintf(" cur_conn time     = %ld\n", cur_conn_time);
#endif
				total_connect_time = last_connect_time + cur_conn_time - reset_base_time;
				get_connect_time = 0;
			}
			else if( (sscanf(isp_meter_buf, "conntime:%ld", &pppd_conntime)==1) && get_connect_time==0 ) {
#ifdef DEBUG
                		_dprintf(" last connect time = %ld\n", pppd_conntime);
#endif
				last_connect_time += pppd_conntime - reset_base_time;
				total_connect_time = last_connect_time;
				reset_base_time = 0;
				get_connect_time = 1;
		        }
			memset(isp_meter_buf, 0, sizeof(isp_meter_buf));
		}
#ifdef DEBUG
		_dprintf(" reset_base_time   = %ld\n", reset_base_time);
		_dprintf(" total_connect_time= %ld\n", total_connect_time);
		_dprintf("******************************** %ld\n", z);
#endif
//_dprintf("isp_meter= %s\n", nvram_get("isp_meter"));
                if(!nvram_match("isp_meter", "disable")) {
                        isp_limit  = nvram_get_int("isp_limit");
#ifdef DEBUG
_dprintf("* Month: tx= %lu, rx= %lu \n", month_tx, month_rx);
_dprintf("* isplimit = %d\n", isp_limit);
_dprintf("* wan_state= %s\n", nvram_get("wan0_state_t"));
#endif
                        if(nvram_match("wan0_state_t", "2")) { //Connected
                                if(nvram_match("isp_meter", "download")) {
                                        if(month_rx > (isp_limit*1000))
                                                notify_rc_and_wait("isp_meter down");
                                }
                                else if(nvram_match("isp_meter", "both")) {
                                        if((month_tx)+(month_rx) > (isp_limit*1000))
                                                notify_rc_and_wait("isp_meter down");
                                }
				else if(nvram_match("isp_meter", "time")) {
					if(total_connect_time > (isp_limit_time*60)) {
						notify_rc_and_wait("isp_meter down");
						cur_conn_time = 0;
					}
				}
                        }

			//Write to Flash
			if(current_uptime - zzz >= MTD_WRITE_INTERVEL) {	
				nvram_commit();
				sprintf(isp_meter_buf,"isp_meter:%lu,%lu,%ld,end", month_rx, month_tx, total_connect_time);
#ifdef DEBUG
_dprintf("*WRITE: %s\n", isp_meter_buf);
#endif
				set_meter_file(isp_meter_buf);
				zzz = current_uptime;
         		}
		}
#endif //RTCONFIG_ISP_METER
	}

	return 0;
}
