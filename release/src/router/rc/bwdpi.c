/*
	bwdpi.c for TrendMicro DPI engine / iQoS / WRS / APP partol

	DPI engine 	: applications and devices identify engine
	iQoS 		: tc control rule and qosd.conf
	WRS 		: web protector or web content filter
	APP partol	: apps filter
	C&C		: C&C
	VP		: virtual patch
	DC		: data collection
*/

#include <rc.h>

static void show_help(char *base)
{
	printf("%s Usage :\n", base);
	printf("  bwdpi stat -m [mode] -n [name] -u [dura] -d [date]\n");
	printf("  mode: traffic / traffic_wan / app / client_apps / client_web\n");
	printf("  name: NULL / MAC / APP_NAME\n");
	printf("  dura: realtime / month / week / day\n");
	printf("  date: NULL / date\n");
}

int bwdpi_main(int argc, char **argv)
{
	//dbg("[bwdpi] argc=%d, argv[0]=%s, argv[1]=%s, argv[2]=%s\n", argc, argv[0], argv[1], argv[2]);
	int c;
	char *mode = NULL, *name = NULL, *dura = NULL, *date = NULL;
	char *MAC = NULL;
	char *page = NULL;
	int clean_flag = 0;

	if (argc == 1){
		printf("Usage :\n");
		printf("  bwdpi [iqos/qosd/wrs] [start/stop/restart]\n");
		printf("  bwdpi dc [start/stop/restart] [ptah]\n");
		printf("  bwdpi stat -m [mode] -n [name] -u [dura] -d [date]\n");
		printf("  bwpdi history -m [MAC] -z -p [page]\n");
		printf("  bwpdi app [0/1]\n");
		printf("  bwpdi cc [0/1]\n");
		printf("  bwpdi vp [0/1]\n");
		printf("  bwpdi device -m [MAC]\n");
		printf("  bwpdi device_info -m [MAC]\n");
		printf("  bwpdi get_vp [0/2]\n");
		printf("  bwpdi wrs_url\n");
		printf("  bwpdi rewrite path1 path2 path3\n");
		printf("  bwpdi checksize path size\n");
		printf("  bwpdi extract path\n");
		printf("  bwpdi get_app_patrol\n");
		printf("  bwpdi get_anomaly [0/2]\n");
		return 0;
	}

	if (!strcmp(argv[1], "iqos")){
		if(argc != 3)
		{
			printf("  bwdpi iqos [start/stop/restart]\n");
			return 0;
		}
		else
		{
			return tm_qos_main(argv[2]);
		}
	}
	else if (!strcmp(argv[1], "qosd")){
		if(argc != 3)
		{
			printf("  bwdpi qosd [start/stop/restart]\n");
			return 0;
		}
		else
		{
			return qosd_main(argv[2]);
		}
	}
	else if (!strcmp(argv[1], "wrs")){
		if(argc != 3)
		{
			printf("  bwdpi wrs [start/stop/restart]\n");
			return 0;
		}
		else
		{
			return wrs_main(argv[2]);
		}

	}
	else if (!strcmp(argv[1], "stat")){
		while ((c = getopt(argc, argv, "m:n:u:d:h")) != -1)
		{
			switch(c)
			{
				case 'm':
					mode = optarg;
					break;
				case 'n':
					name = optarg;
					break;
				case 'u':
					dura = optarg;
					break;
				case 'd':
					date = optarg;
					break;
				case 'h':
					show_help(argv[1]);
					break;
				default:
					printf("ERROR: unknown option %c\n", c);
					break;
			}
		}
		//dbg("[bwdpi] mode=%s, name=%s, dura=%s, date=%s\n", mode, name, dura, date);
		return stat_main(mode, name, dura, date);
	}
	else if (!strcmp(argv[1], "history")){
		while ((c = getopt(argc, argv, "m:zp:")) != -1)
		{
			switch(c)
			{
				case 'm':
					MAC = optarg;
					clean_flag = 0;
					break;
				case 'z':
					printf("clear web history\n");
					clean_flag = 1;
					break;
				case 'p':
					page = optarg;
					break;
				default:
					printf("  bwpdi history -m [MAC] -z -p [page]\n");
					break;
			}
		}

		if(clean_flag)
		{
			return clear_user_domain();
		}
		else
		{
			return web_history_main(MAC, page);
		}
	}
	else if (!strcmp(argv[1], "app")){
		if(argc != 3)
		{
			printf("  bwpdi app [0/1]\n");
			return 0;
		}
		else
		{
			return wrs_app_main(argv[2]);
		}
	}
	else if (!strcmp(argv[1], "cc")){
		if(argc != 3)
		{
			printf("  bwpdi cc [0/1]\n");
			return 0;
		}
		else
		{
			return set_cc(argv[2]);
		}
	}
	else if (!strcmp(argv[1], "vp")){
		if(argc != 3)
		{
			printf("  bwpdi vp [0/1]\n");
			return 0;
		}
		else
		{
			return set_vp(argv[2]);
		}
	}
	else if (!strcmp(argv[1], "dc")){
		if(argc == 3)
		{
			return data_collect_main(argv[2], NULL);
		}
		else if(argc == 4)
		{
			return data_collect_main(argv[2], argv[3]);
		}
		else
		{
			printf("  bwpdi dc [start/stop/restart] [path]\n");
			return 0;
		}
	}
	else if (!strcmp(argv[1], "device")){
		while ((c = getopt(argc, argv, "m:")) != -1)
		{
			switch(c)
			{
				case 'm':
					name = optarg;
					break;
				default:
					printf("  bwpdi device -m [MAC]\n");
					break;
			}
		}
		return device_main(name);
	}
	else if (!strcmp(argv[1], "device_info")){
		while ((c = getopt(argc, argv, "m:")) != -1)
		{
			switch(c)
			{
				case 'm':
					name = optarg;
					break;
				default:
					printf("  bwpdi device_info -m [MAC]\n");
					break;
			}
		}
		return device_info_main(name);
	}
	else if (!strcmp(argv[1], "get_vp")){
		if(argc != 3)
		{
			printf("  bwpdi get_vp [0/2]\n");
			return 0;
		}
		else
		{
			return get_vp(argv[2]);
		}
	}
	else if (!strcmp(argv[1], "wrs_url")){
		if(argc != 2)
		{
			printf("  bwpdi wrs_url\n");
			return 0;
		}
		else
		{
			return wrs_url_main();
		}
	}
	else if (!strcmp(argv[1], "rewrite")){
		if(argc != 5)
		{
			printf("  bwpdi rewrite path1 path2 path3\n");
			return 0;
		}
		else
		{
			return rewrite_main(argv[2], argv[3], argv[4]);
		}
	}
	else if (!strcmp(argv[1], "checksize")){
		if(argc != 4)
		{
			printf("  bwpdi checksize path size\n");
			return 0;
		}
		else
		{
			return check_filesize_main(argv[2], argv[3]);
		}
	}
	else if (!strcmp(argv[1], "extract")){
		if(argc != 3)
		{
			printf("  bwpdi extract path\n");
			return 0;
		}
		else
		{
			return extract_data_main(argv[2]);
		}
	}
	else if (!strcmp(argv[1], "get_app_patrol")){
		if(argc != 2)
		{
			printf("  bwpdi get_app_patrol\n");
			return 0;
		}
		else
		{
			return get_app_patrol_main();
		}
	}
	else if (!strcmp(argv[1], "get_anomaly")){
		if(argc != 3)
		{
			printf("  bwpdi get_anomaly [0/2]\n");
			return 0;
		}
		else
		{
			return get_anomaly_main(argv[2]);
		}
	}
	else{
		printf("Usage :\n");
		printf("  bwdpi [iqos/qosd/wrs] [start/stop/restart]\n");
		printf("  bwdpi dc [start/stop/restart] [ptah]\n");
		printf("  bwdpi stat -m [mode] -n [name] -u [dura] -d [date]\n");
		printf("  bwpdi history -m [MAC] -z -p [page]\n");
		printf("  bwpdi app [0/1]\n");
		printf("  bwpdi cc [0/1]\n");
		printf("  bwpdi vp [0/1]\n");
		printf("  bwpdi device -m [MAC]\n");
		printf("  bwpdi device_info -m [MAC]\n");
		printf("  bwpdi get_vp [0/2]\n");
		printf("  bwpdi wrs_url\n");
		printf("  bwpdi rewrite path1 path2 path3\n");
		printf("  bwpdi checksize path size\n");
		printf("  bwpdi extract path\n");
		printf("  bwpdi get_app_patrol\n");
		printf("  bwpdi get_anomaly [0/2]\n");
		return 0;
	}

	return 1;
}
