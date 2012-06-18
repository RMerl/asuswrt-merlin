
#include <stdio.h>
#include <string.h>
#include <bcmnvram.h>
#include "rc.h"

/* activity date_time for rule */
int parental_macfilter_daytime(void)
{
	char word_local[256], *next_local;
	unsigned int clients_local;
	char str_time[10], str_date[9];

	clients_local = 0;

	foreach_62 (word_local, nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"), next_local)
	{
		memset(str_date, 0, sizeof(str_date));
		memset(str_time, 0, sizeof(str_time));

		strncpy(str_date, word_local, 7);
		strncpy(str_time, word_local+7, 8);

		nvram_set_by_seq("macfilter_date_x", clients_local, str_date);
		nvram_set_by_seq("macfilter_time_x", clients_local, str_time);
		clients_local++;
	}
	fprintf(stderr, "[parental] parental_macfilter_daytime(%d)\n", clients_local);
	return clients_local;
}

/* MAC filter rulse enable or disable */
int parental_clients_status(void)
{
	char word_local[256], *next_local;
	unsigned int clients_local;

	clients_local = 0;

	foreach_62 (word_local, nvram_safe_get("MULTIFILTER_ENABLE"), next_local)
	{
		nvram_set_by_seq("macfilter_enable_status_x", clients_local, word_local);
		clients_local++;
	}
	fprintf(stderr, "[parental] parental_clients_status(%d)\n", clients_local);
	return clients_local;
}

/* Return: clients count */
int parental_clients_mac(void)
{
	char word_local[256], *next_local;
	unsigned int clients_local;

	clients_local = 0;

	foreach_62 (word_local, nvram_safe_get("MULTIFILTER_MAC"), next_local)
	{
		nvram_set_by_seq("macfilter_list_x", clients_local, word_local);
		clients_local++;
	}
	fprintf(stderr, "[parental] parental_clients_mac(%d)\n", clients_local);

	return clients_local;
}

int parental_clean_macfilter(void)
{
	int tmp_idx, tmp_count;
	char tmp_entry[21] = {0};

	tmp_count = atoi(nvram_safe_get("macfilter_num_x"));
	for( tmp_idx = 0; tmp_idx < tmp_count; ++tmp_idx){
		snprintf(tmp_entry, 21 * sizeof(char), "macfilter_list_x%d", tmp_idx);
		nvram_unset(tmp_entry);
	}
	/* reset count */
	nvram_set("macfilter_num_x", "0");

	fprintf(stderr, "[parental] parental_clean_macfilter)\n");

	return 1;
}

int parental_ctrl(void)
{
	unsigned int clients_local;
	char tmp_name_local[255];

	clients_local = 0;

	fprintf(stderr, "[parental] parental_ctrl_main()\n");

	nvram_set("macfilter_enable_x", "2");	/* Force Reject mode */
	nvram_set("macfilter_rulelist", "");	/* Clean macfilter rules */

	parental_clean_macfilter();

	clients_local = parental_clients_mac();
	snprintf(tmp_name_local, sizeof(tmp_name_local), "%d", clients_local);
	nvram_set("macfilter_num_x", tmp_name_local);

	parental_clients_status();
	parental_macfilter_daytime();

	return 1;
}

int parental_ctrl_main(int argc, char *argv[])
{
	parental_ctrl();
}

