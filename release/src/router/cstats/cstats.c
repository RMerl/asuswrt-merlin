/*

	cstats
	Copyright (C) 2011-2012 Augusto Bott

	based on rstats
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

//#define DEBUG_CSTATS

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
#include <shared.h>

#include <tree.h>

#include "cstats.h"

long save_utime;
char save_path[96];
long current_uptime;

volatile int gothup = 0;
volatile int gotuser = 0;
volatile int gotterm = 0;

#ifdef DEBUG_CSTATS
void Node_print(Node *self, FILE *stream) {
	fprintf(stream, "%s", self->ipaddr);
}

void Node_printer(Node *self, void *stream) {
	Node_print(self, (FILE *)stream);
	fprintf((FILE *)stream, " ");
}

void Tree_info(void) {
//	printf("Tree = ");
//	TREE_FORWARD_APPLY(&tree, _Node, linkage, Node_printer, stdout);
//	printf("\n");
//	printf("Tree depth = %d\n", TREE_DEPTH(&tree, linkage));
}
#endif

Node *Node_new(char *ipaddr) {
	Node *self;
	if ((self = malloc(sizeof(Node))) != NULL) {
		memset(self, 0, sizeof(Node));
		self->id = CURRENT_ID;
		strncpy(self->ipaddr, ipaddr, INET_ADDRSTRLEN);
		printf("%s: new node ip=%s, version=%d, sizeof(Node)=%d (bytes)\n", __FUNCTION__, self->ipaddr, self->id, sizeof(Node));
	}
	return self;
}

int Node_compare(Node *lhs, Node *rhs) {
	return strncmp(lhs->ipaddr, rhs->ipaddr, INET_ADDRSTRLEN);
}

Tree tree = TREE_INITIALIZER(Node_compare);

static int get_stime(void) {
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

void Node_save(Node *self, void *t) {
	node_print_mode_t *info = (node_print_mode_t *)t;
	if(fwrite(self, sizeof(Node), 1, info->stream) > 0) {
		info->kn++;
	}
}

static int save_history_from_tree(const char *fname) {
	FILE *f;
	node_print_mode_t info;
	char s[256];

	info.kn=0;
	printf("%s: fname=%s\n", __FUNCTION__, fname);

	unlink(uncomp_fn);
	if ((f = fopen(uncomp_fn, "wb")) != NULL) {
		info.mode=0;
		info.stream=f;
		TREE_FORWARD_APPLY(&tree, _Node, linkage, Node_save, &info);
		fclose(f);

		sprintf(s, "%s.gz", fname);
		unlink(s);

		if (rename(uncomp_fn, fname) == 0) {
			sprintf(s, "gzip %s", fname);
			system(s);
		}
	}
	return info.kn;
}

static void save(int quick) {
	int i;
	int n;
	int b;
	char hgz[256];
	char tmp[256];
	char bak[256];
	char bkp[256];
	time_t now;
	struct tm *tms;
	static int lastbak = -1;

	printf("%s: quick=%d\n", __FUNCTION__, quick);

	f_write("/var/lib/misc/cstats-stime", &save_utime, sizeof(save_utime), 0, 0);

	n = save_history_from_tree(history_fn);
	printf("%s: saved %d records from tree on file %s\n", __FUNCTION__, n, history_fn);

	printf("%s: write source=%s\n", __FUNCTION__, save_path);
	f_write_string(source_fn, save_path, 0, 0);

	if (quick) {
		return;
	}

	sprintf(hgz, "%s.gz", history_fn);

	if (save_path[0] != 0) {
		strcpy(tmp, save_path);
		strcat(tmp, ".tmp");

		for (i = 15; i > 0; --i) {
			if (!wait_action_idle(10)) {
				printf("%s: busy, not saving\n", __FUNCTION__);
			}
			else {
				printf("%s: cp %s %s\n", __FUNCTION__, hgz, tmp);
				if (eval("cp", hgz, tmp) == 0) {
					printf("%s: copy ok\n", __FUNCTION__);

					if (!nvram_match("rstats_bak", "0")) {
						now = time(0);
						tms = localtime(&now);
						if (lastbak != tms->tm_yday) {
							strcpy(bak, save_path);
							n = strlen(bak);
							if ((n > 3) && (strcmp(bak + (n - 3), ".gz") == 0)) n -= 3;
//							sprintf(bak + n, "_%d.bak", ((tms->tm_yday / 7) % 3) + 1);
//							if (eval("cp", save_path, bak) == 0) lastbak = tms->tm_yday;
							strcpy(bkp, bak);
							for (b = HI_BACK-1; b > 0; --b) {
								sprintf(bkp + n, "_%d.bak", b + 1);
								sprintf(bak + n, "_%d.bak", b);
								rename(bak, bkp);
							}
							if (eval("cp", "-p", save_path, bak) == 0) lastbak = tms->tm_yday;
						}
					}

					printf("%s: rename %s %s\n", __FUNCTION__, tmp, save_path);
					if (rename(tmp, save_path) == 0) {
						printf("%s: rename ok\n", __FUNCTION__);
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

static int load_history_to_tree(const char *fname) {
	int n;
	FILE *f;
	char s[256];
	Node tmp;
	Node *ptr;
	char *exclude;

	exclude = nvram_safe_get("cstats_exclude");
	printf("%s: cstats_exclude='%s'\n", __FUNCTION__, exclude);
	printf("%s: fname=%s\n", __FUNCTION__, fname);
	unlink(uncomp_fn);

	n = 0;
	sprintf(s, "gzip -dc %s > %s", fname, uncomp_fn);
	if (system(s) == 0) {
		if ((f = fopen(uncomp_fn, "rb")) != NULL) {
			while (fread(&tmp, sizeof(Node), 1, f) > 0) {
				if ((find_word(exclude, tmp.ipaddr))) {
					printf("%s: not loading excluded ip '%s'\n", __FUNCTION__, tmp.ipaddr);
					continue;
				}

				if (tmp.id == CURRENT_ID) {
					printf("%s: found data for ip %s\n", __FUNCTION__, tmp.ipaddr);

					ptr = TREE_FIND(&tree, _Node, linkage, &tmp);
					if (ptr) {
						printf("%s: removing/reloading new data for ip %s\n", __FUNCTION__, ptr->ipaddr);
						TREE_REMOVE(&tree, _Node, linkage, ptr);
						free(ptr);
						ptr = NULL;
					}

					TREE_INSERT(&tree, _Node, linkage, Node_new(tmp.ipaddr));

					ptr = TREE_FIND(&tree, _Node, linkage, &tmp);

					memcpy(ptr->daily, &tmp.daily, sizeof(data_t) * MAX_NDAILY);
					ptr->dailyp = tmp.dailyp;
					memcpy(ptr->monthly, &tmp.monthly, sizeof(data_t) * MAX_NMONTHLY);
					ptr->monthlyp = tmp.monthlyp;

					ptr->utime = tmp.utime;
					memcpy(ptr->speed, &tmp.speed, sizeof(unsigned long) * MAX_NSPEED * MAX_COUNTER);
					memcpy(ptr->last, &tmp.last, sizeof(unsigned long) * MAX_COUNTER);
					ptr->tail = tmp.tail;
//					ptr->sync = tmp.sync;
					ptr->sync = -1;

					if (ptr->utime > current_uptime) {
						ptr->utime = current_uptime;
						ptr->sync = 1;
					}

					++n;
				} else {
					printf("%s: data for ip '%s' version %d not loaded (current version is %d)\n", __FUNCTION__, tmp.ipaddr, tmp.id, CURRENT_ID);
				}
			}

		fclose(f);
		}
	}
	else {
		printf("%s: %s != 0\n", __FUNCTION__, s);
	}
	unlink(uncomp_fn);

	printf("%s: loaded %d records\n", __FUNCTION__, n);

	return n;
}

static int load_history(const char *fname) {
	printf("%s: fname=%s\n", __FUNCTION__, fname);
	return load_history_to_tree(fname);
}

/* Try loading from the backup versions.
 * We'll try from oldest to newest, then
 * retry the requested one again last.  In case the drive mounts while
 * we are trying to find a good version.
 */
static int try_hardway(const char *fname) {
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

static void load_new(void) {
	char hgz[256];

	sprintf(hgz, "%s.gz.new", history_fn);
	if (load_history(hgz)) save(0);
	unlink(hgz);
}

static void load(int new) {
	int i;
	long t;
	int n;
	char hgz[256];
	unsigned char mac[6];

	current_uptime = uptime();

	printf("%s: new=%d, uptime=%lu\n", __FUNCTION__, new, current_uptime);

	strlcpy(save_path, nvram_safe_get("rstats_path"), sizeof(save_path) - 32);
	if (((n = strlen(save_path)) > 0) && (save_path[n - 1] == '/')) {
		ether_atoe(nvram_safe_get("et0macaddr"), mac);
		sprintf(save_path + n, "tomato_cstats_%02x%02x%02x%02x%02x%02x.gz",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	if (f_read("/var/lib/misc/cstats-stime", &save_utime, sizeof(save_utime)) != sizeof(save_utime)) {
		save_utime = 0;
	}
	t = current_uptime + get_stime();
	if ((save_utime < current_uptime) || (save_utime > t)) save_utime = t;
	printf("%s: uptime = %lum, save_utime = %lum\n", __FUNCTION__, current_uptime / 60, save_utime / 60);

	sprintf(hgz, "%s.gz", history_fn);

	if (new) {
		unlink(hgz);
		save_utime = 0;
		return;
	}

	if (save_path[0] != 0) {
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
//				if (load_history(save_path)) {
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

void Node_print_speedjs(Node *self, void *t) {
	int j, k, p;
	uint64_t total, tmax;
	unsigned long n;
	char c;

	node_print_mode_t *info = (node_print_mode_t *)t;

	fprintf(info->stream, "%s'%s': {\n", info->kn ? " },\n" : "", self->ipaddr);
	for (j = 0; j < MAX_COUNTER; ++j) {
		total = tmax = 0;
		fprintf(info->stream, "%sx: [", j ? ",\n t" : " r");
		p = self->tail;
		for (k = 0; k < MAX_NSPEED; ++k) {
			p = (p + 1) % MAX_NSPEED;
			n = self->speed[p][j];
			fprintf(info->stream, "%s%lu", k ? "," : "", n);
			total += n;
			if (n > tmax) tmax = n;
		}
		fprintf(info->stream, "],\n");

		c = j ? 't' : 'r';
		fprintf(info->stream, " %cx_avg: %llu,\n %cx_max: %llu,\n %cx_total: %llu",
			c, total / MAX_NSPEED, c, tmax, c, total);
	}
	info->kn++;
}

static void save_speedjs(long next) {
	FILE *f;

	if ((f = fopen("/var/tmp/cstats-speed.js", "w")) == NULL) return;

	node_print_mode_t info;
	info.mode=0;
	info.stream=f;
	info.kn=0;

	fprintf(f, "\nspeed_history = {\n");
	TREE_FORWARD_APPLY(&tree, _Node, linkage, Node_print_speedjs, &info);
	fprintf(f, "%s_next: %ld};\n", info.kn ? "},\n" : "", ((next >= 1) ? next : 1));

	fclose(f);

	rename("/var/tmp/cstats-speed.js", "/var/spool/cstats-speed.js");
}

void Node_print_datajs(Node *self, void *t) {
	data_t *data;
	int p, max, k;

	node_print_mode_t *info = (node_print_mode_t *)t;

	if (info->mode == DAILY) {
		data = self->daily;
		p = self->dailyp;
		max = MAX_NDAILY;
	}

	else {
		data = self->monthly;
		p = self->monthlyp;
		max = MAX_NMONTHLY;
	}

	for (k = max; k > 0; --k) {
		p = (p + 1) % max;
		if (data[p].xtime == 0) continue;
		fprintf(info->stream, "%s[0x%lx,'%s',%llu,%llu]", info->kn ? "," : "",
			(unsigned long)data[p].xtime, self->ipaddr, data[p].counter[0] / K, data[p].counter[1] / K);
		info->kn++;
	}
}

static void save_datajs(FILE *f, int mode) {
	node_print_mode_t info;
	info.mode=mode;
	info.stream=f;
	info.kn=0;
	fprintf(f, "\n%s_history = [\n", (mode == DAILY) ? "daily" : "monthly");
	TREE_FORWARD_APPLY(&tree, _Node, linkage, Node_print_datajs, &info);
	fprintf(f, "\n];\n");
}

static void save_histjs(void) {
	FILE *f;

	if ((f = fopen("/var/tmp/cstats-history.js", "w")) != NULL) {
		save_datajs(f, DAILY);
		save_datajs(f, MONTHLY);
		fclose(f);
		rename("/var/tmp/cstats-history.js", "/var/spool/cstats-history.js");
	}
}

static void bump(data_t *data, int *tail, int max, uint32_t xnow, unsigned long *counter) {
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

void Node_housekeeping(Node *self, void *info) {
	if (self) {
		if (self->sync == -1) {
			self->sync = 0;
		} else {
			self->sync = 1;
		}
	}
}

static void calc(void) {
	FILE *f;
	char buf[512];
	char *ipaddr = NULL;
	unsigned long counter[MAX_COUNTER];
	int i, j;
	time_t now;
	time_t mon;
	struct tm *tms;
	uint32_t c;
	uint32_t sc;
	unsigned long diff;
	long tick;
	int n;
	char *exclude = NULL;
	char *include = NULL;

	Node *ptr = NULL;
	Node test;

	now = time(0);

	exclude = strdup(nvram_safe_get("cstats_exclude"));
	printf("%s: cstats_exclude='%s'\n", __FUNCTION__, exclude);

	include = strdup(nvram_safe_get("cstats_include"));
	printf("%s: cstats_include='%s'\n", __FUNCTION__, include);


	unsigned long tx;
	unsigned long rx;
	char ip[INET_ADDRSTRLEN];

	if ((f = fopen("/proc/net/ipt_account/lan", "r"))) {

		while (fgets(buf, sizeof(buf), f)) {
			if(sscanf(buf, 
				"ip = %s bytes_src = %lu %*u %*u %*u %*u packets_src = %*u %*u %*u %*u %*u bytes_dst = %lu %*u %*u %*u %*u packets_dst = %*u %*u %*u %*u %*u time = %*u",
//				"ip = %s bytes_src = %Lu %*Lu %*Lu %*Lu %*Lu packets_src = %*Lu %*Lu %*Lu %*Lu %*Lu bytes_dest = %Lu %*Lu %*Lu %*Lu %*Lu packets_dest = %*Lu %*Lu %*Lu %*Lu %*Lu time = %*lu",
				ip, &rx, &tx) != 3 ) continue;
#ifdef DEBUG_CSTATS
			printf("%s: %s tx=%lu rx=%lu\n", __FUNCTION__, ip, tx, rx);
#endif

			if (find_word(exclude, ip)) continue;
			if ((tx < 1) && (rx < 1)) continue;

			counter[0] = tx;
			counter[1] = rx;
			ipaddr=ip;

			strncpy(test.ipaddr, ipaddr, INET_ADDRSTRLEN);
			ptr = TREE_FIND(&tree, _Node, linkage, &test);

			if ( (ptr) || (nvram_get_int("cstats_all")) || (find_word(include, ipaddr)) ) {

				if (!ptr) {
					printf("%s: new ip: %s\n", __FUNCTION__, ipaddr);
					TREE_INSERT(&tree, _Node, linkage, Node_new(ipaddr));
					ptr = TREE_FIND(&tree, _Node, linkage, &test);
					ptr->sync = 1;
					ptr->utime = current_uptime;
				}
#ifdef DEBUG_CSTATS
				Tree_info();
#endif
				printf("%s: sync[%s]=%d\n", __FUNCTION__, ptr->ipaddr, ptr->sync);
				if (ptr->sync) {
					printf("%s: sync[%s] changed to -1\n", __FUNCTION__, ptr->ipaddr);
					ptr->sync = -1;
#ifdef DEBUG_CSTATS
					for (i = 0; i < MAX_COUNTER; ++i) {
						printf("%s: counter[%d]=%lu ptr->last[%d]=%lu\n", __FUNCTION__, i, counter[i], i, ptr->last[i]);
					}
#endif
					memcpy(ptr->last, counter, sizeof(ptr->last));
					memset(counter, 0, sizeof(counter));
					for (i = 0; i < MAX_COUNTER; ++i) {
						printf("%s: counter[%d]=%lu ptr->last[%d]=%lu\n", __FUNCTION__, i, counter[i], i, ptr->last[i]);
					}
				}
				else {
#ifdef DEBUG_CSTATS
					printf("%s: sync[%s] = %d \n", __FUNCTION__, ptr->ipaddr, ptr->sync);
#endif
					ptr->sync = -1;
					printf("%s: sync[%s] = %d \n", __FUNCTION__, ptr->ipaddr, ptr->sync);
					tick = current_uptime - ptr->utime;
					n = tick / INTERVAL;
					if (n < 1) {
						printf("%s: %s is a little early... %lu < %d\n", __FUNCTION__, ipaddr, tick, INTERVAL);
					} else {
						ptr->utime += (n * INTERVAL);
						printf("%s: %s n=%d tick=%lu utime=%lu ptr->utime=%lu\n", __FUNCTION__, ipaddr, n, tick, current_uptime, ptr->utime);
						for (i = 0; i < MAX_COUNTER; ++i) {
							c = counter[i];
							sc = ptr->last[i];
#ifdef DEBUG_CSTATS
							printf("%s: counter[%d]=%lu ptr->last[%d]=%lu c=%u sc=%u\n", __FUNCTION__, i, counter[i], i, ptr->last[i], c, sc);
#endif
							if (c < sc) {
								diff = (0xFFFFFFFF - sc) + c;
								if (diff > MAX_ROLLOVER) diff = 0;
							}
							else {
								 diff = c - sc;
							}
							ptr->last[i] = c;
							counter[i] = diff;
							printf("%s: counter[%d]=%lu ptr->last[%d]=%lu c=%u sc=%u diff=%lu\n", __FUNCTION__, i, counter[i], i, ptr->last[i], c, sc, diff);
						}
						printf("%s: ip=%s n=%d ptr->tail=%d\n", __FUNCTION__, ptr->ipaddr, n, ptr->tail);
						for (j = 0; j < n; ++j) {
							ptr->tail = (ptr->tail + 1) % MAX_NSPEED;
#ifdef DEBUG_CSTATS
							printf("%s: ip=%s j=%d n=%d ptr->tail=%d\n", __FUNCTION__, ptr->ipaddr, j, n, ptr->tail);
#endif
							for (i = 0; i < MAX_COUNTER; ++i) {
								ptr->speed[ptr->tail][i] = counter[i] / n;
							}
						}
						printf("%s: ip=%s j=%d n=%d ptr->tail=%d\n", __FUNCTION__, ptr->ipaddr, j, n, ptr->tail);
					}
				}

				if (now > 1325376000) {	/* Skip this if the time&date is not set yet */
#ifdef DEBUG_CSTATS
					printf("%s: calling bump %s ptr->dailyp=%d\n", __FUNCTION__, ptr->ipaddr, ptr->dailyp);
#endif
					tms = localtime(&now);
					bump(ptr->daily, &ptr->dailyp, MAX_NDAILY,
						(tms->tm_year << 16) | ((uint32_t)tms->tm_mon << 8) | tms->tm_mday, counter);

#ifdef DEBUG_CSTATS
					printf("%s: calling bump %s ptr->monthlyp=%d\n", __FUNCTION__, ptr->ipaddr, ptr->monthlyp);
#endif
					n = nvram_get_int("rstats_offset");
					if ((n < 1) || (n > 31)) n = 1;
					mon = now + ((1 - n) * (60 * 60 * 24));
					tms = localtime(&mon);
					bump(ptr->monthly, &ptr->monthlyp, MAX_NMONTHLY,
						(tms->tm_year << 16) | ((uint32_t)tms->tm_mon << 8), counter);
				}

			}
		}
		fclose(f);
	}
	// remove/exclude history (if we still have any data previously stored)
	char *nvp, *b;
	nvp = exclude;
	if (nvp) {
		while ((b = strsep(&nvp, ",")) != NULL) {
			printf("%s: check exclude='%s'\n", __FUNCTION__, b);
			strncpy(test.ipaddr, b, INET_ADDRSTRLEN);
			ptr = TREE_FIND(&tree, _Node, linkage, &test);
			if (ptr) {
				printf("%s: excluding '%s'\n", __FUNCTION__, ptr->ipaddr);
				TREE_REMOVE(&tree, _Node, linkage, ptr);
				free(ptr);
				ptr = NULL;
			}
		}
	}

	// cleanup remaining entries for next time
	TREE_FORWARD_APPLY(&tree, _Node, linkage, Node_housekeeping, NULL);

	// todo: total > user ???
	if (current_uptime >= save_utime) {
		save(0);
		save_utime = current_uptime + get_stime();
		printf("%s: uptime = %lum, save_utime = %lum\n", __FUNCTION__, current_uptime / 60, save_utime / 60);
	}

	printf("%s: ====================================\n", __FUNCTION__);

//QUIT:
	if (exclude) {
		free(exclude);
		exclude = NULL;
	}
	if (include) {
		free(include);
		include = NULL;
	}
}

static void sig_handler(int sig) {
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

int main(int argc, char *argv[]) {

	struct sigaction sa;
	long z;
	int new;

	printf("cstats - Copyright (C) 2011-2012 Augusto Bott\n");
	printf("based on rstats - Copyright (C) 2006-2009 Jonathan Zarate\n\n");

	if (fork() != 0) return 0;

	openlog("cstats", LOG_PID, LOG_USER);

	new = 0;
	if (argc > 1) {
		if (strcmp(argv[1], "--new") == 0) {
			new = 1;
			printf("new=1\n");
		}
	}

	unlink("/var/tmp/cstats-load");

	sa.sa_handler = sig_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	load(new);

	z = current_uptime = uptime();
	while (1) {
		while (current_uptime < z) {
			sleep(z - current_uptime);
			if (gothup) {
				if (unlink("/var/tmp/cstats-load") == 0) load_new();
					else save(0);
				gothup = 0;
			}
			if (gotterm) {
				save(!nvram_match("cstats_sshut", "1"));
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
	}

	return 0;
}
