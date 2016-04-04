/*
 * Speedtest-cli
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <float.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <math.h>

#include <bcmnvram.h>
#include <shutils.h>
#include <linux/types.h>


#define CLOSEST_SERVERS_NUM 5
#define DL_FILE_NUM 10
#define DL_FILE_TIMES 4
#define MAX_FILE_LEN 20
#define UL_SIZE_NUM 2

#define PI 3.14159265358979

static pthread_mutex_t finished_mutex = PTHREAD_MUTEX_INITIALIZER;

static double finished = 0;
static int dl_thread_num = 1;
static int ul_thread_num = 1;
static double time_dl_start;
static double time_ul_start;

typedef struct client_config {
	char *ip;
	char *lat;
	char *lon;
	char *isp;
	char *isprating;
	char *rating;
	char *ispdlavg;
	char *ispulavg;
	char *loggedin;
} client_config_t;

typedef struct server_config {
	char *url;
	char *lat;
	char *lon;
	char *name;
	char *country;
	char *cc;
	char *sponsor;
	char *id;
	double dist;	/* distance */
	double latency;
} server_config_t;

typedef struct dl_thread_arg {
	char *url;
	char file_count[3];
} dl_thread_arg_t;

typedef struct ul_thread_arg {
	char *url;
	char *ul_file;
	int size;
	char *file_result;
} ul_thread_arg_t;

static int
get_uptime(double *uptime)
{
	FILE *fp;
	char line[256];
	char *time = NULL;

	if (!(fp = fopen("/proc/uptime", "r"))) {
		perror("fopen /proc/uptime");
		return errno;
	}

	if (fgets(line, 255, fp)) {
		fclose(fp);
		if ((time = strtok(line, " ")) != NULL)
			*uptime = atof(time);
		else
			return -1;
	} else {
		fclose(fp);
		return -1;
	}

	return 0;
}

static void
client_free(client_config_t *client)
{
	if (client->ip)
		free(client->ip);
	if (client->lat)
		free(client->lat);
	if (client->lon)
		free(client->lon);
	if (client->isp)
		free(client->isp);
	if (client->isprating)
		free(client->isprating);
	if (client->rating)
		free(client->rating);
	if (client->ispdlavg)
		free(client->ispdlavg);
	if (client->ispulavg)
		free(client->ispulavg);
	if (client->loggedin)
		free(client->loggedin);
}

static char *
get_str(char *in, char *line)
{
	int line_len, in_len, i, j;
	int val_len = 0, val_index = 0;
	char *val;

	line_len = strlen(line);
	in_len = strlen(in);

	for (i = 1; i < line_len; i++) {
		if ((line[i] == in[0]) && line[i-1] == ' ' && !strncmp(&line[i], in, in_len)) {
			break;
		}
	}

	if (i == line_len) {
		return NULL;
	}

	j = i + in_len + 1;
	for (; j < line_len; j++) {
		if ((line[j] == '\"') && (val_len == 0)) {
			val_len++;
			val_index = j + 1;
		} else if ((line[j] == '\"') && (val_len != 0)) {
			val_len--;
			val = (char *)malloc(val_len + 1);
			strncpy(val, (char *)&line[val_index], val_len);
			val[val_len] = '\0';
			return val;
		} else if (val_len > 0) {
			val_len++;
		}
	}

	return NULL;
}

static int
get_speedtest_config(client_config_t *client)
{
	FILE *fp1;
	char line[256];

	eval("curl", "-L", "-s", "-o", "/tmp/speedtest-config.php",
		"http://www.speedtest.net/speedtest-config.php");

	if (!(fp1 = fopen("/tmp/speedtest-config.php", "r"))) {
		perror("fopen /tmp/speedtest-config.php");
		return errno;
	}

	while (fgets(line, 255, fp1)) {
		if (!strncmp(line, "<client", 7)) {
			if ((client->ip = get_str("ip", line)) == NULL) {
				client_free(client);
				return -1;
			}

			if ((client->lat = get_str("lat", line)) == NULL) {
				client_free(client);
				return -1;
			}
			if ((client->lon = get_str("lon", line)) == NULL) {
				client_free(client);
				return -1;
			}
			if ((client->isp = get_str("isp", line)) == NULL) {
				client_free(client);
				return -1;
			}
			if ((client->isprating = get_str("isprating", line)) == NULL) {
				client_free(client);
				return -1;
			}
			if ((client->rating = get_str("rating", line)) == NULL) {
				client_free(client);
				return -1;
			}
			if ((client->ispdlavg = get_str("ispdlavg", line)) == NULL) {
				client_free(client);
				return -1;
			}
			if ((client->ispulavg = get_str("ispulavg", line)) == NULL) {
				client_free(client);
				return -1;
			}
			if ((client->loggedin = get_str("loggedin", line)) == NULL) {
				client_free(client);
				return -1;
			}
			break;
		}
	}
	fclose(fp1);

	eval("rm", "/tmp/speedtest-config.php");

	return 0;
}

static void
server_free(server_config_t *server)
{
	if (server->url)
		free(server->url);
	if (server->lat)
		free(server->lat);
	if (server->lon)
		free(server->lon);
	if (server->name)
		free(server->name);
	if (server->country)
		free(server->country);
	if (server->cc)
		free(server->cc);
	if (server->sponsor)
		free(server->sponsor);
	if (server->id)
		free(server->id);
}

static void
init_server(server_config_t *server)
{
	memset(server, 0, sizeof(server_config_t));
}

/* calculate distance between client and server */
static double
get_distance(client_config_t *client, server_config_t *server)
{
	double lat_c, lat_s, lon_c, lon_s, d, a;
	double dlat, dlon;
	int radius = 6371;	/* earth radius */

	lat_c = atof(client->lat);
	lon_c = atof(client->lon);
	lat_s = atof(server->lat);
	lon_s = atof(server->lon);
	dlat = (lat_s - lat_c) * PI/180;
	dlon = (lon_s - lon_c) * PI/180;

	a = sin(dlat / 2) * sin(dlat / 2) + cos(lat_c) * cos(lat_s) * sin(dlon / 2) * sin(dlon / 2);
	d = radius * 2 * atan2(sqrt(a), sqrt(1 - a));

	return d;
}


/* Determine the 5 nearest speedtest.net servers based on geographic distance */
static int
get_nearest_servers(client_config_t *client, server_config_t *servers)
{
	FILE *fp1;
	char line[256];
	server_config_t server;
	int j, k;

	eval("curl", "-L", "-s", "-o", "/tmp/speedtest-servers.php",
		"http://c.speedtest.net/speedtest-servers-static.php");

	if (!(fp1 = fopen("/tmp/speedtest-servers.php", "r"))) {
		perror("fopen /tmp/speedtest-servers.php");
		return errno;
	}
	init_server(&server);

	while (fgets(line, 255, fp1)) {
		if ((server.url = get_str("url", line)) == NULL) {
			server_free(&server);
			init_server(&server);
			continue;
		}
		if ((server.lat = get_str("lat", line)) == NULL) {
			server_free(&server);
			init_server(&server);
			continue;
		}
		if ((server.lon = get_str("lon", line)) == NULL) {
			server_free(&server);
			init_server(&server);
			continue;
		}
		if ((server.name = get_str("name", line)) == NULL) {
			server_free(&server);
			init_server(&server);
			continue;
		}
		if ((server.country = get_str("country", line)) == NULL) {
			server_free(&server);
			init_server(&server);
			continue;
		}
		if ((server.cc = get_str("cc", line)) == NULL) {
			server_free(&server);
			init_server(&server);
			continue;
		}
		if ((server.sponsor = get_str("sponsor", line)) == NULL) {
			server_free(&server);
			init_server(&server);
			continue;
		}
		if ((server.id = get_str("id", line)) == NULL) {
			server_free(&server);
			init_server(&server);
			continue;
		}

		/* calculate distance between client and server */
		server.dist = get_distance(client, &server);

		for (j = 0; j < CLOSEST_SERVERS_NUM; j++) {
			if (servers[j].url == NULL) {
				servers[j].url = server.url;
				servers[j].lat = server.lat;
				servers[j].lon = server.lon;
				servers[j].name = server.name;
				servers[j].country = server.country;
				servers[j].cc = server.cc;
				servers[j].sponsor = server.sponsor;
				servers[j].id = server.id;
				servers[j].dist = server.dist;
				init_server(&server);
				break;
			} else {
				if (server.dist <= servers[j].dist) {
					server_free(&servers[CLOSEST_SERVERS_NUM - 1]);
					init_server(&servers[CLOSEST_SERVERS_NUM - 1]);
					for (k = CLOSEST_SERVERS_NUM - 1; k > j; k--) {
						servers[k].url = servers[k-1].url;
						servers[k].lat = servers[k-1].lat;
						servers[k].lon = servers[k-1].lon;
						servers[k].name = servers[k-1].name;
						servers[k].country = servers[k-1].country;
						servers[k].cc = servers[k-1].cc;
						servers[k].sponsor = servers[k-1].sponsor;
						servers[k].id = servers[k-1].id;
						servers[k].dist = servers[k-1].dist;
					}
					servers[j].url = server.url;
					servers[j].lat = server.lat;
					servers[j].lon = server.lon;
					servers[j].name = server.name;
					servers[j].country = server.country;
					servers[j].cc = server.cc;
					servers[j].sponsor = server.sponsor;
					servers[j].id = server.id;
					servers[j].dist = server.dist;
					init_server(&server);
					break;
				} else {
					if (j == CLOSEST_SERVERS_NUM - 1) {
						server_free(&server);
						init_server(&server);
					}
				}
			}
		}
	}

	fclose(fp1);

	eval("rm", "/tmp/speedtest-servers.php");

	return 0;
}

static int
get_lowest_latency_server(server_config_t *servers, server_config_t *best_server)
{
	int i, j, len, best;
	char *url = NULL;
	struct timeval tv1, tv2;
	FILE *fp1;
	char line[12];
	double lat;
	double latency[3];

	for (i = 0; i < CLOSEST_SERVERS_NUM; i++) {
		len = strlen(servers[i].url);
		url = malloc(len - strlen("upload.php") + strlen("latency.txt") + 1);
		strncpy(url, servers[i].url, len - strlen("upload.php"));
		url[len - strlen("upload.php")] = '\0';
		strcat(url, "latency.txt");

		for (j = 0; j < 3; j++) {
			gettimeofday(&tv1, NULL);
			eval("curl", "-L", "-s", "-o", "/tmp/latency.txt", url);
			gettimeofday(&tv2, NULL);

			if (!(fp1 = fopen("/tmp/latency.txt", "r"))) {
				perror("fopen /tmp/latency.txt");
				latency[j] = 3600000000;
				continue;
			}
			fgets(line, sizeof(line), fp1);
			if (!strncmp(line, "test=test", strlen("test=test"))) {
				latency[j] = (tv2.tv_sec - tv1.tv_sec) * 1000000
						+ tv2.tv_usec - tv1.tv_usec;
			} else {
				latency[j] = 3600000000;
			}
			fclose(fp1);
			eval("rm", "/tmp/latency.txt");
			memset(line, 0, sizeof(line));
		}
		servers[i].latency = (latency[0] + latency[1] + latency[2]) / 3;
		free(url);
		url = NULL;
	}

	for (i = 0; i < CLOSEST_SERVERS_NUM; i++) {
		if (i == 0) {
			best = i;
			lat = servers[i].latency;
		} else {
			if (servers[i].latency < lat) {
				best = i;
				lat = servers[i].latency;
			}
		}
	}

	best_server->url = servers[best].url;
	best_server->lat = servers[best].lat;
	best_server->lon = servers[best].lon;
	best_server->name = servers[best].name;
	best_server->country = servers[best].country;
	best_server->cc = servers[best].cc;
	best_server->sponsor = servers[best].sponsor;
	best_server->id = servers[best].id;
	best_server->dist = servers[best].dist;
	best_server->latency = servers[best].latency;

	return 0;
}

static void *
download_thread(void *ptr)
{
	dl_thread_arg_t *in;
	struct stat file_stat;
	char file[16];
	double time_diff, time_thread;

	if (get_uptime(&time_thread)) {
		fprintf(stderr, "Error on getting /proc/uptime\n");
		return NULL;
	}
	time_diff = (time_thread - time_dl_start);
	if (time_diff > 10) {
		return NULL;
	}

	in = (dl_thread_arg_t *)ptr;

	strcpy(file, "/tmp/random");
	strcat(file, in->file_count);
	eval("curl", "-L", "-s", "-o", file, in->url);

	if (stat(file, &file_stat)) {
		fprintf(stderr, "stat file %s error\n", file);
		return NULL;
	}

	pthread_mutex_lock(&finished_mutex);
	finished += (double)file_stat.st_size;
	pthread_mutex_unlock(&finished_mutex);

	if (get_uptime(&time_thread)) {
		fprintf(stderr, "Error on getting /proc/uptime\n");
		return NULL;
	}
	time_diff = (time_thread - time_dl_start);
	printf("download = %.2f\n", ((finished / 1024 / 1024 / time_diff) * 8));

	eval("rm", file);

	return NULL;
}

static int
test_download_speed(server_config_t *best_server)
{
	int i, j, k = 0, ret, url_len, queue_count = 0;
	const char sizes[DL_FILE_NUM][MAX_FILE_LEN] = {"random350x350.jpg",
				"random500x500.jpg",
				"random750x750.jpg",
				"random1000x1000.jpg",
				"random1500x1500.jpg",
				"random2000x2000.jpg",
				"random2500x2500.jpg",
				"random3000x3000.jpg",
				"random3500x3500.jpg",
				"random4000x4000.jpg"};
	dl_thread_arg_t download_url[DL_FILE_NUM * DL_FILE_TIMES];
	pthread_t q[dl_thread_num];
	double duration;
	double time_dl_end;
	FILE *fp_result;
	char result[32];

	nvram_set("speedtest_running", "0");

	url_len = strlen(best_server->url) - strlen("upload.php");

	for (i = 0; i < DL_FILE_NUM; i++) {
		for (j = 0; j < DL_FILE_TIMES; j++) {
			download_url[k].url = malloc(url_len + MAX_FILE_LEN);
			strncpy(download_url[k].url, best_server->url, url_len);
			download_url[k].url[url_len] = '\0';
			strcat(download_url[k].url, (char *)&sizes[i]);
			k++;
		}
	}

	if (get_uptime(&time_dl_start)) {
		fprintf(stderr, "Error on getting /proc/uptime\n");
		return -1;
	}

	for (i = 0; i < (DL_FILE_NUM * DL_FILE_TIMES); i++) {
		if (queue_count < dl_thread_num) {
			for (j = 0; j < (dl_thread_num - 1); j++) {
				q[dl_thread_num - j - 1] = q[dl_thread_num - j - 2];
			}
			sprintf(download_url[i].file_count, "%d", i);
			ret = pthread_create(q, NULL, download_thread, (void *)&download_url[i]);
			queue_count++;
		}
		if (queue_count == dl_thread_num) {
			if (i == (DL_FILE_NUM * DL_FILE_TIMES - 1)) {
				/* all task have been put in queue, consume all threads in queue */
				for (j = 0; j < dl_thread_num; j++) {
					pthread_join(q[dl_thread_num - 1 - j], NULL);
				}

			} else {
				/* consume a thread in queue to provide space for next task */
				pthread_join(q[dl_thread_num-1], NULL);
				queue_count--;
			}
		}
	}

	if (get_uptime(&time_dl_end)) {
		fprintf(stderr, "Error on getting /proc/uptime\n");
		return -1;
	}

	duration = time_dl_end - time_dl_start;

	for (i = 0; i < DL_FILE_NUM * DL_FILE_TIMES; i++) {
		free(download_url[i].url);
	}
	printf("speedtest_cli: Download = %.2f Mbit/s (%.2f Kbyte/s)\n",
		((finished / 1024 / 1024 / duration) * 8),
		(finished / 1024/ duration));

	if (!(fp_result = fopen("/tmp/speedtest_download_result", "w"))) {
		perror("fopen /tmp/speedtest_download_result");
		return errno;
	}
	strcpy(result, "ceil_down=");
	sprintf(&result[strlen("ceil_down=")], "%ld", (long)(finished / 1024/ duration));
	strcat(result, "kbps");
	fprintf(fp_result, "%s", result);
	fclose(fp_result);

	memset(result, 0, sizeof(result));
	sprintf(result, "%.2f", ((finished / 1024 / duration) * 8));
	nvram_set("speedtest_ibw", result);

	return 0;
}

static void *
upload_thread(void *ptr)
{
	ul_thread_arg_t *in;
	struct stat file_stat;
	double time_diff;
	double time_thread;

	if (get_uptime(&time_thread)) {
		fprintf(stderr, "Error on getting /proc/uptime\n");
		return NULL;
	}
	time_diff = time_thread - time_ul_start;
	if (time_diff > 10)
		return NULL;

	in = (ul_thread_arg_t *)ptr;

	eval("curl", "-L", "-s", "-0", "-d", in->ul_file, "-o", in->file_result, in->url);

	pthread_mutex_lock(&finished_mutex);
	if (stat(in->file_result, &file_stat)) {
		fprintf(stderr, "stat file %s error\n", in->file_result);
	} else {
		finished += (double)in->size;
	}
	pthread_mutex_unlock(&finished_mutex);

	if (get_uptime(&time_thread)) {
		fprintf(stderr, "Error on getting /proc/uptime\n");
		return NULL;
	}
	time_diff = (time_thread - time_ul_start);
	printf("upload = %.2f\n", ((finished / 1024 / 1024 / time_diff) * 8));

	eval("rm", in->file_result);

	return NULL;
}

static int
test_upload_speed(server_config_t *best_server)
{
	const char *data = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char *head = "content1=";
	const char *tail = "0123456789ABCDEFGHIJKLMNOPQ";
	const char *ul_file = "/tmp/speedtest_ulfile";
	const char *ul_file_result = "/tmp/speedtest_ulfile_result";
	const int ul_times = 25;
	char file_tmp[UL_SIZE_NUM][strlen(ul_file) + 2];
	char ul_file_name[UL_SIZE_NUM][strlen(ul_file) + 3];
	char ul_file_result_name[UL_SIZE_NUM * ul_times][strlen(ul_file_result) + 3];
	double size[UL_SIZE_NUM] = {250000, 500000};
	double duration;
	double time_ul_end;
	int i, j, data_len, queue_count = 0;
	ul_thread_arg_t upload_arg[UL_SIZE_NUM * ul_times];
	pthread_t q[ul_thread_num];
	FILE *fp[UL_SIZE_NUM];
	FILE *fp_result;
	char result[32];

	nvram_set("speedtest_running", "0");

	for (i = 0; i < UL_SIZE_NUM; i++) {
		data_len = (int)round(size[i] / strlen(data));
		sprintf(&file_tmp[i][0], "%s%d", ul_file, i);
		if (!(fp[i] = fopen(&file_tmp[i][0], "w"))) {
			fprintf(stderr, "open %s error\n", &file_tmp[i][0]);
			return errno;
		}
		fprintf(fp[i], "%s", head);
		for (j = 0; j < (data_len - 1); j++) {
			fprintf(fp[i], "%s", data);
		}
		fprintf(fp[i], "%s", tail);
		fclose(fp[i]);
		strcpy(&ul_file_name[i][0], "@");
		strcat(&ul_file_name[i][0], &file_tmp[i][0]);
	}
	for (i = 0; i < (UL_SIZE_NUM * ul_times); i++) {
		upload_arg[i].url = best_server->url;
		upload_arg[i].size = ((int)round(size[i / ul_times] / strlen(data))) * strlen(data);
		upload_arg[i].ul_file = &ul_file_name[i / ul_times][0];
		sprintf(&ul_file_result_name[i][0], "%s%d", ul_file_result, i);
		upload_arg[i].file_result = &ul_file_result_name[i][0];
	}

	if (get_uptime(&time_ul_start)) {
		fprintf(stderr, "Error on getting /proc/uptime\n");
		return -1;
	}

	for (i = 0; i < (UL_SIZE_NUM * ul_times); i++) {
		if (queue_count < ul_thread_num) {
			for (j = 0; j < (ul_thread_num - 1); j++) {
				q[ul_thread_num - j - 1] = q[ul_thread_num - j - 2];
			}
			pthread_create(q, NULL, upload_thread, (void *)&upload_arg[i]);
			queue_count++;
		}
		if (queue_count == ul_thread_num) {
			if (i == ((UL_SIZE_NUM * ul_times) - 1)) {
				/* all task have been put in queue, consume all threads in queue */
				for (j = 0; j < ul_thread_num; j++) {
					pthread_join(q[ul_thread_num - 1 - j], NULL);
				}

			} else {
				/* consume a thread in queue to provide space for next task */
				pthread_join(q[ul_thread_num-1], NULL);
				queue_count--;
			}
		}

	}

	if (get_uptime(&time_ul_end)) {
		fprintf(stderr, "Error on getting /proc/uptime\n");
		return -1;
	}

	duration = time_ul_end - time_ul_start;
	printf("speedtest_cli: Upload = %.2f Mbit/s (%.2f Kbyte/s)\n",
		((finished / 1024 / 1024 / duration) * 8),
		(finished / 1024/ duration));

	if (!(fp_result = fopen("/tmp/speedtest_upload_result", "w"))) {
		perror("fopen /tmp/speedtest_upload_result");
		return errno;
	}
	strcpy(result, "ceil_up=");
	sprintf(&result[strlen("ceil_up=")], "%ld", (long)(finished / 1024/ duration));
	strcat(result, "kbps");
	fprintf(fp_result, "%s", result);
	fclose(fp_result);

	memset(result, 0, sizeof(result));
	sprintf(result, "%.2f", ((finished / 1024 / duration) * 8));
	nvram_set("speedtest_obw", result);

	for (i = 0; i < UL_SIZE_NUM; i++) {
		eval("rm", &file_tmp[i][0]);
	}

	return 0;
}

static void
init_client(client_config_t *client)
{
	memset(client, 0, sizeof(client_config_t));
}

static int
speedtest(int dl_enable, int ul_enable)
{
	int i;
	client_config_t client;
	server_config_t servers[CLOSEST_SERVERS_NUM];
	server_config_t best_server;

	for (i = 0; i < CLOSEST_SERVERS_NUM; i++) {
		init_server(&servers[i]);
	}
	init_server(&best_server);
	init_client(&client);

	if (get_speedtest_config(&client)) {
		fprintf(stderr, "get_speedtest_config error!\n");
		nvram_set("speedtest_running", "2");
		return -1;
	}

	if (get_nearest_servers(&client, servers)) {
		fprintf(stderr, "get_nearest_servers error!\n");
		nvram_set("speedtest_running", "2");
		return -1;
	}

	client_free(&client);

	if (get_lowest_latency_server(servers, &best_server)) {
		fprintf(stderr, "get_lowest_latency_server error!\n");
		nvram_set("speedtest_running", "2");
		return -1;
	}

	if (dl_enable == 1) {
		if (test_download_speed(&best_server)) {
			fprintf(stderr, "test_download_speed error!\n");
			nvram_set("speedtest_running", "2");
			return -1;
		}
	}

	if (ul_enable == 1) {
		finished = (double)0;
		if (test_upload_speed(&best_server)) {
			fprintf(stderr, "test_upload_speed error!\n");
			nvram_set("speedtest_running", "2");
			return -1;
		}
	}

	for (i = 0; i < CLOSEST_SERVERS_NUM; i++) {
		server_free(&servers[i]);
	}

	nvram_set("speedtest_running", "1");
	nvram_commit();
	return 0;
}

static void
usage(void)
{
	printf("usage:\n");
	printf("speedtest_cli [1|0](download test enable|disable) [download thread number] ");
	printf("[1|0](upload test enable|disable) [upload thread number]\n");
	printf("Range of thread number: 1 - 20\n");
}

int
speedtest_main(int argc, char **argv)
{
	int num;
	int dl_enable, ul_enable;

	if (argc != 5) {
		usage();
		return 0;
	} else {
		if (!strcmp(argv[1], "1")) {
			dl_enable = 1;
		} else if (!strcmp(argv[1], "0")) {
			dl_enable = 0;
		} else {
			usage();
			return 0;
		}

		if (!strcmp(argv[3], "1")) {
			ul_enable = 1;
		} else if (!strcmp(argv[3], "0")) {
			ul_enable = 0;
		} else {
			usage();
			return 0;
		}

		num = atoi(argv[2]);
		if ((num < 1) || (num > 20)) {
			usage();
			return 0;
		} else {
			dl_thread_num = num;
		}

		num = atoi(argv[4]);
		if ((num < 1) || (num > 20)) {
			usage();
			return 0;
		} else {
			ul_thread_num = num;
		}
	}

	if ((dl_enable == 0) && (ul_enable == 0)) {
		return 0;
	}

	if (speedtest(dl_enable, ul_enable)) {
		fprintf(stderr, "speedtest error\n");
		return -1;
	}

	return 0;
}

void wan_bandwidth_detect(void)
{
	/* Execute WAN port bandwidth detection */
#ifndef RTCONFIG_SPRINT
//	if (nvram_match("qos_wan_bw_auto", "1")) 
//	{
#endif

		struct stat file_stat;
		int retry = 0;

		/* In case router is not ready to connect to internet
		 * and test whether speedtest server is reachable.
		 */
		do {
			sleep(1);
			eval("curl", "-s", "-o", "/tmp/speedtest-test",
				"http://www.speedtest.net/speedtest-config.php");
			if (stat("/tmp/speedtest-test", &file_stat)) {
				if (retry > 3) {
					fprintf(stderr, "speedtest server is not reachable\n");
					return;
				} else {
					retry++;
					continue;
				}
			}
			eval("rm", "/tmp/speedtest-test");
			break;
		} while (1);

		/* Execute speedtest_cli to test download and upload bandwidth */
		/*
			speedtest:
			argv[1] : download_enable
			argv[2] : download_thread_num
			argv[3] : upload_enable
			argv[4] : upload_thread_num
		*/
		eval("speedtest", "1", "10", "1", "10");
#ifndef RTCONFIG_SPRINT
//	}
#endif
}
