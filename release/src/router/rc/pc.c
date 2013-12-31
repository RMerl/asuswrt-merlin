#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bcmnvram.h>
#include <shutils.h>
#include "rc.h"

#include "pc.h"

//#define BLOCKLOCAL

#define pc_dbg(fmt, args...) do{ \
		FILE *fp = fopen("/dev/console", "a+"); \
		if(fp){ \
			fprintf(fp, "[pc_dbg: %s] ", __FUNCTION__); \
			fprintf(fp, fmt, ## args); \
			fclose(fp); \
		} \
	}while(0)


pc_event_s *initial_event(pc_event_s **target_e){
	pc_event_s *tmp_e;

	if(target_e == NULL)
		return NULL;

	*target_e = (pc_event_s *)malloc(sizeof(pc_event_s));
	if(*target_e == NULL)
		return NULL;

	tmp_e = *target_e;

	memset(tmp_e->e_name, 0, 32);
	tmp_e->start_day = 0;
	tmp_e->end_day = 0;
	tmp_e->start_hour = 0;
	tmp_e->end_hour = 0;
	tmp_e->next = NULL;

	return tmp_e;
}

void free_event_list(pc_event_s **target_list){
	pc_event_s *tmp_e, *old_e;

	if(target_list == NULL)
		return;

	tmp_e = *target_list;
	while(tmp_e != NULL){
		old_e = tmp_e;
		tmp_e = tmp_e->next;
		free(old_e);
	}

	return;
}

pc_event_s *get_event_list(pc_event_s **target_list, char *target_string){
	char word[4096], *next_word;
	pc_event_s **follow_e_list;
	int i, n_t;
	char *ptr, *ptr_end, bak;

	if(target_list == NULL || target_string == NULL)
		return NULL;

	follow_e_list = target_list;
	n_t = 1;
	foreach_60(word, target_string, next_word){
		if(n_t){
			if(initial_event(follow_e_list) == NULL){
				pc_dbg("No memory!!(follow_e_list)\n");
				continue;
			}

			++i;
			n_t = 0;

			strncpy((*follow_e_list)->e_name, word, 32);
		}
		else{
			n_t = 1;

			ptr = word;
			ptr_end = ptr+1;
			bak = ptr_end[0];
			ptr_end[0] = 0;
			(*follow_e_list)->start_day = atoi(ptr);
			ptr_end[0] = bak;

			ptr = word+1;
			ptr_end = ptr+1;
			bak = ptr_end[0];
			ptr_end[0] = 0;
			(*follow_e_list)->end_day = atoi(ptr);
			ptr_end[0] = bak;

			ptr = word+2;
			ptr_end = ptr+2;
			bak = ptr_end[0];
			ptr_end[0] = 0;
			(*follow_e_list)->start_hour = atoi(ptr);
			ptr_end[0] = bak;

			ptr = word+4;
			ptr_end = ptr+2;
			bak = ptr_end[0];
			ptr_end[0] = 0;
			(*follow_e_list)->end_hour = atoi(ptr);
			ptr_end[0] = bak;

			if((*follow_e_list)->start_hour == 24){
				(*follow_e_list)->start_day += 1;
				if((*follow_e_list)->start_day == 7)
					(*follow_e_list)->start_day = 0;
				(*follow_e_list)->start_hour = 0;
			}

			// if end_hour == 24, don't set the flag: "--timestop".
			/*if((*follow_e_list)->end_hour == 24){
				(*follow_e_list)->end_day += 1;
				if((*follow_e_list)->end_day == 7)
					(*follow_e_list)->end_day = 0;
				(*follow_e_list)->end_hour = 0;
			}//*/

			while(*follow_e_list != NULL)
				follow_e_list = &((*follow_e_list)->next);
		}
	}

	return *target_list;
}

pc_event_s *cp_event(pc_event_s **dest, const pc_event_s *src){
	if(initial_event(dest) == NULL){
		pc_dbg("No memory!!(dest)\n");
		return NULL;
	}

	strcpy((*dest)->e_name, src->e_name);
	(*dest)->start_day = src->start_day;
	(*dest)->end_day = src->end_day;
	(*dest)->start_hour = src->start_hour;
	(*dest)->end_hour = src->end_hour;

	return *dest;
}

void print_event_list(pc_event_s *e_list){
	pc_event_s *follow_e;
	int i;

	if(e_list == NULL)
		return;

	i = 0;
	for(follow_e = e_list; follow_e != NULL; follow_e = follow_e->next){
		++i;
		pc_dbg(" %3dth event:\n", i);
		pc_dbg("      e_name: %s.\n", follow_e->e_name);
		pc_dbg("   start_day: %d.\n", follow_e->start_day);
		pc_dbg("     end_day: %d.\n", follow_e->end_day);
		pc_dbg("  start_hour: %d.\n", follow_e->start_hour);
		pc_dbg("    end_hour: %d.\n", follow_e->end_hour);
		if(follow_e->next != NULL)
			pc_dbg("------------------------------\n");
	}
}

pc_s *initial_pc(pc_s **target_pc){
	pc_s *tmp_pc;

	if(target_pc == NULL)
		return NULL;

	*target_pc = (pc_s *)malloc(sizeof(pc_s));
	if(*target_pc == NULL)
		return NULL;

	tmp_pc = *target_pc;

	tmp_pc->enabled = 0;
	memset(tmp_pc->device, 0, 32);
	memset(tmp_pc->mac, 0, 18);
	tmp_pc->events = NULL;
	tmp_pc->next = NULL;

	return tmp_pc;
}

void free_pc_list(pc_s **target_list){
	pc_s *tmp_pc, *old_pc;

	if(target_list == NULL)
		return;

	tmp_pc = *target_list;
	while(tmp_pc != NULL){
		free_event_list(&(tmp_pc->events));

		old_pc = tmp_pc;
		tmp_pc = tmp_pc->next;
		free(old_pc);
	}

	return;
}

pc_s *get_all_pc_list(pc_s **pc_list){
	char word[4096], *next_word;
	pc_s *follow_pc, **follow_pc_list;
	int i;

	if(pc_list == NULL)
		return NULL;

	follow_pc_list = pc_list;
	foreach_62(word, nvram_safe_get("MULTIFILTER_ENABLE"), next_word){
		if(initial_pc(follow_pc_list) == NULL){
			pc_dbg("No memory!!(follow_pc_list)\n");
			continue;
		}

		if(strlen(word) > 0)
			(*follow_pc_list)->enabled = atoi(word);

		while(*follow_pc_list != NULL)
			follow_pc_list = &((*follow_pc_list)->next);
	}

	follow_pc = *pc_list;
	i = 0;
	foreach_62(word, nvram_safe_get("MULTIFILTER_DEVICENAME"), next_word){
		++i;
		if(follow_pc == NULL){
			pc_dbg("*** %3dth Parental Control rule(DEVICENAME) had something wrong!\n", i);
			return *pc_list;
		}

		strncpy(follow_pc->device, word, 32);

		follow_pc = follow_pc->next;
	}

	follow_pc = *pc_list;
	i = 0;
	foreach_62(word, nvram_safe_get("MULTIFILTER_MAC"), next_word){
		++i;
		if(follow_pc == NULL){
			pc_dbg("*** %3dth Parental Control rule(MAC) had something wrong!\n", i);
			return *pc_list;
		}

		strncpy(follow_pc->mac, word, 18);

		follow_pc = follow_pc->next;
	}

	follow_pc = *pc_list;
	i = 0;
	foreach_62(word, nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"), next_word){
		++i;
		if(follow_pc == NULL){
			pc_dbg("*** %3dth Parental Control rule(DAYTIME) had something wrong!\n", i);
			return *pc_list;
		}

		get_event_list(&(follow_pc->events), word);

		follow_pc = follow_pc->next;
	}

	return *pc_list;
}

pc_s *cp_pc(pc_s **dest, const pc_s *src){
	pc_event_s *follow_e, **follow_e_list;

	if(initial_pc(dest) == NULL){
		pc_dbg("No memory!!(dest)\n");
		return NULL;
	}

	(*dest)->enabled = src->enabled;
	strcpy((*dest)->device, src->device);
	strcpy((*dest)->mac, src->mac);

	follow_e_list = &((*dest)->events);
	for(follow_e = src->events; follow_e != NULL; follow_e = follow_e->next){
		cp_event(follow_e_list, follow_e);

		while(*follow_e_list != NULL)
			follow_e_list = &((*follow_e_list)->next);
	}

	return *dest;
}

void print_pc_list(pc_s *pc_list){
	pc_s *follow_pc;
	int i;

	if(pc_list == NULL)
		return;

	i = 0;
	for(follow_pc = pc_list; follow_pc != NULL; follow_pc = follow_pc->next){
		++i;
		pc_dbg("*** %3dth rule:\n", i);
		pc_dbg("   enabled: %d.\n", follow_pc->enabled);
		pc_dbg("    device: %s.\n", follow_pc->device);
		pc_dbg("       mac: %s.\n", follow_pc->mac);
		print_event_list(follow_pc->events);
		pc_dbg("******************************\n");
	}
}

pc_s *match_enabled_pc_list(pc_s *pc_list, pc_s **target_list, int enabled){
	pc_s *follow_pc, **follow_target_list;

	if(pc_list == NULL || target_list == NULL)
		return NULL;

	if(enabled != 0 && enabled != 1)
		return NULL;

	follow_target_list = target_list;
	for(follow_pc = pc_list; follow_pc != NULL; follow_pc = follow_pc->next){
		if(follow_pc->enabled == enabled){
			cp_pc(follow_target_list, follow_pc);

			while(*follow_target_list != NULL)
				follow_target_list = &((*follow_target_list)->next);
		}
	}

	return *target_list;
}

pc_s *match_day_pc_list(pc_s *pc_list, pc_s **target_list, int target_day){
	pc_s *follow_pc, **follow_target_list;
	pc_event_s *follow_e;

	if(pc_list == NULL || target_list == NULL)
		return NULL;

	if(target_day < MIN_DAY || target_day > MAX_DAY)
		return NULL;

	follow_target_list = target_list;
	for(follow_pc = pc_list; follow_pc != NULL; follow_pc = follow_pc->next){
		for(follow_e = follow_pc->events; follow_e != NULL; follow_e = follow_e->next){
			if(target_day >= follow_e->start_day && target_day <= follow_e->end_day){
				cp_pc(follow_target_list, follow_pc);

				while(*follow_target_list != NULL)
					follow_target_list = &((*follow_target_list)->next);

				break;
			}
		}
	}

	return *target_list;
}

pc_s *match_daytime_pc_list(pc_s *pc_list, pc_s **target_list, int target_day, int target_hour){
	pc_s *follow_pc, **follow_target_list;
	pc_event_s *follow_e;
	int target_num, com_start, com_end;

	if(pc_list == NULL || target_list == NULL)
		return NULL;

	if(target_day < MIN_DAY || target_day > MAX_DAY)
		return NULL;

	if(target_hour < MIN_HOUR || target_hour > MAX_HOUR)
		return NULL;

	target_num = target_day*24+target_hour;

	follow_target_list = target_list;
	for(follow_pc = pc_list; follow_pc != NULL; follow_pc = follow_pc->next){
		for(follow_e = follow_pc->events; follow_e != NULL; follow_e = follow_e->next){
			com_start = follow_e->start_day*24+follow_e->start_hour;
			com_end = follow_e->end_day*24+follow_e->end_hour;

			if(target_num >= com_start && target_num < com_end){ // exclude the end of daytime.
				cp_pc(follow_target_list, follow_pc);

				while(*follow_target_list != NULL)
					follow_target_list = &((*follow_target_list)->next);

				break;
			}
		}
	}

	return *target_list;
}

// Parental Control:
// MAC address not in list -> ACCEPT.
// MAC address in list and in time period -> ACCEPT.
// MAC address in list and not in time period -> DROP.
void config_daytime_string(FILE *fp, char *logaccept, char *logdrop)
{
	pc_s *pc_list = NULL, *enabled_list = NULL, *follow_pc;
	pc_event_s *follow_e;
	char *lan_if = nvram_safe_get("lan_ifname");
	char *datestr[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	int i;
	char *default_policy, *ftype, *fftype;

	default_policy = logdrop;
	ftype = logaccept;
	fftype = "PControls";

	follow_pc = get_all_pc_list(&pc_list);
	if(follow_pc == NULL){
		pc_dbg("Couldn't get the Parental-control rules correctly!\n");
		return;
	}

	follow_pc = match_enabled_pc_list(pc_list, &enabled_list, 1);
	free_pc_list(&pc_list);
	if(follow_pc == NULL){
		pc_dbg("Couldn't get the enabled rules of Parental-control correctly!\n");
		return;
	}

	for(follow_pc = enabled_list; follow_pc != NULL; follow_pc = follow_pc->next){
		for(follow_e = follow_pc->events; follow_e != NULL; follow_e = follow_e->next){
			if(follow_e->start_day == follow_e->end_day){
				if(follow_e->start_hour == follow_e->end_hour){ // whole week.
#ifdef BLOCKLOCAL
					fprintf(fp, "-A FORWARD -i %s -m mac --mac-source %s -j %s\n", lan_if, follow_pc->mac, ftype);
#endif
					fprintf(fp, "-A FORWARD -i %s -m mac --mac-source %s -j %s\n", lan_if, follow_pc->mac, fftype);
				}
				else{
#ifdef BLOCKLOCAL
					fprintf(fp, "-A INPUT -i %s -m time", lan_if);
					if(follow_e->start_hour > 0)
						fprintf(fp, " --timestart %d:0", follow_e->start_hour);
					if(follow_e->end_hour < 24)
						fprintf(fp, " --timestop %d:0", follow_e->end_hour);
					fprintf(fp, DAYS_PARAM "%s -m mac --mac-source %s -j %s\n", datestr[follow_e->start_day], follow_pc->mac, ftype);
#endif
					fprintf(fp, "-A FORWARD -i %s -m time", lan_if);
					if(follow_e->start_hour > 0)
						fprintf(fp, " --timestart %d:0", follow_e->start_hour);
					if(follow_e->end_hour < 24)
						fprintf(fp, " --timestop %d:0", follow_e->end_hour);
					fprintf(fp, DAYS_PARAM "%s -m mac --mac-source %s -j %s\n", datestr[follow_e->start_day], follow_pc->mac, fftype);
				}
			}
			else if(follow_e->start_day > follow_e->end_day)
				; // Don't care "start_day > end_day".
			else{ // start_day < end_day.
				// first interval.
#ifdef BLOCKLOCAL
				fprintf(fp, "-A INPUT -i %s -m time", lan_if);
				if(follow_e->start_hour > 0)
					fprintf(fp, " --timestart %d:0", follow_e->start_hour);
				fprintf(fp, DAYS_PARAM "%s -m mac --mac-source %s -j %s\n", datestr[follow_e->start_day], follow_pc->mac, ftype);
#endif
				fprintf(fp, "-A FORWARD -i %s -m time", lan_if);
				if(follow_e->start_hour > 0)
					fprintf(fp, " --timestart %d:0", follow_e->start_hour);
				fprintf(fp, DAYS_PARAM "%s -m mac --mac-source %s -j %s\n", datestr[follow_e->start_day], follow_pc->mac, fftype);

				// middle interval.
				if(follow_e->end_day-follow_e->start_day > 1){
#ifdef BLOCKLOCAL
					fprintf(fp, "-A INPUT -i %s -m time" DAYS_PARAM, lan_if);
					for(i = follow_e->start_day+1; i < follow_e->end_day; ++i)
						fprintf(fp, "%s%s", (i == follow_e->start_day+1)?"":",", datestr[i]);
					fprintf(fp, " -m mac --mac-source %s -j %s\n", follow_pc->mac, ftype);
#endif

					fprintf(fp, "-A FORWARD -i %s -m time" DAYS_PARAM, lan_if);
					for(i = follow_e->start_day+1; i < follow_e->end_day; ++i)
						fprintf(fp, "%s%s", (i == follow_e->start_day+1)?"":",", datestr[i]);
					fprintf(fp, " -m mac --mac-source %s -j %s\n", follow_pc->mac, fftype);
				}

				// end interval.
				if(follow_e->end_hour > 0){
#ifdef BLOCKLOCAL
					fprintf(fp, "-A INPUT -i %s -m time", lan_if);
					if(follow_e->end_hour < 24)
						fprintf(fp, " --timestop %d:0", follow_e->end_hour);
					fprintf(fp, DAYS_PARAM "%s -m mac --mac-source %s -j %s\n", datestr[follow_e->end_day], follow_pc->mac, ftype);
#endif
					fprintf(fp, "-A FORWARD -i %s -m time", lan_if);
					if(follow_e->end_hour < 24)
						fprintf(fp, " --timestop %d:0", follow_e->end_hour);
					fprintf(fp, DAYS_PARAM "%s -m mac --mac-source %s -j %s\n", datestr[follow_e->end_day], follow_pc->mac, fftype);
				}
			}
		}

		// MAC address in list and not in time period -> DROP.
#ifdef BLOCKLOCAL
		fprintf(fp, "-A INPUT -i %s -m mac --mac-source %s -j DROP\n", lan_if, follow_pc->mac);
#endif
		fprintf(fp, "-A FORWARD -i %s -m mac --mac-source %s -j DROP\n", lan_if, follow_pc->mac);
	}

	free_pc_list(&enabled_list);
}

int count_pc_rules(void)
{
	pc_s *pc_list = NULL, *enabled_list = NULL, *follow_pc;
	int count;

	follow_pc = get_all_pc_list(&pc_list);
	if(follow_pc == NULL){
		pc_dbg("Couldn't get the Parental-control rules correctly!\n");
		return 0;
	}

	follow_pc = match_enabled_pc_list(pc_list, &enabled_list, 1);
	free_pc_list(&pc_list);
	if(follow_pc == NULL){
		pc_dbg("Couldn't get the enabled rules of Parental-control correctly!\n");
		return 0;
	}

	for(count = 0, follow_pc = enabled_list; follow_pc != NULL; follow_pc = follow_pc->next)
		++count;

	free_pc_list(&enabled_list);

	return count;
}

int pc_main(int argc, char *argv[])
{
	pc_s *pc_list = NULL, *enabled_list = NULL, *daytime_list = NULL;

	get_all_pc_list(&pc_list);

	if(argc == 1 || (argc == 2 && !strcmp(argv[1], "show"))){
		print_pc_list(pc_list);
	}
	else if((argc == 2 && !strcmp(argv[1], "enabled"))
			|| (argc == 3 && !strcmp(argv[1], "enabled") && (!strcmp(argv[2], "0") || !strcmp(argv[2], "1")))){
		if(argc == 2)
			match_enabled_pc_list(pc_list, &enabled_list, 1);
		else
			match_enabled_pc_list(pc_list, &enabled_list, atoi(argv[2]));

		print_pc_list(enabled_list);

		free_pc_list(&enabled_list);
	}
	else if(argc == 4 && !strcmp(argv[1], "daytime")
			&& (atoi(argv[2]) >= MIN_DAY && atoi(argv[2]) <= MAX_DAY)
			&& (atoi(argv[3]) >= MIN_HOUR && atoi(argv[3]) <= MAX_HOUR)
			){
		match_daytime_pc_list(pc_list, &daytime_list, atoi(argv[2]), atoi(argv[3]));

		print_pc_list(daytime_list);

		free_pc_list(&daytime_list);
	}
	else if(argc == 2 && !strcmp(argv[1], "apply")){
		char prefix[]="wanXXXXXX_", tmp[100];
		int wan_unit = wan_primary_ifunit();
		snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);

		char *wan_if = get_wan_ifname(wan_unit);
		char *wan_ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		char *lan_if = nvram_safe_get("lan_ifname");
		char *lan_ip = nvram_safe_get("lan_ipaddr");
		char logaccept[32], logdrop[32];

		if(nvram_match("fw_log_x", "accept") || nvram_match("fw_log_x", "both"))
			strcpy(logaccept, "logaccept");
		else
			strcpy(logaccept, "ACCEPT");
		if(nvram_match("fw_log_x", "drop") || nvram_match("fw_log_x", "both"))
			strcpy(logdrop, "logdrop");
		else
			strcpy(logdrop, "DROP");

		match_enabled_pc_list(pc_list, &enabled_list, 1);

		filter_setting(wan_if, wan_ip, lan_if, lan_ip, logaccept, logdrop);

		free_pc_list(&enabled_list);
	}
	else if(argc == 2 && !strcmp(argv[1], "showrules")){
		config_daytime_string(stderr, "ACCEPT", "logdrop");
	}
	else{
		printf("Usage: pc [show]\n"
		       "          showrules\n"
		       "          enabled [1 | 0]\n"
		       "          daytime [1-7] [0-23]\n"
		       "          apply\n"
		       );
	}

	free_pc_list(&pc_list);

	return 0;
}
