/*
	main.c
*/

#include "traffic_limiter.h"

static void show_help()
{
	printf("Usage :\n");
	printf("    save : traffic_limiter -w\n");
	printf("    read : traffic_limiter -r -i [interface] -t [time of end] -s [time of start]\n");
	printf("    query : traffic_limiter -q\n");
	printf("    save traffic when HW reboot : traffic_limiter -c\n");
}

int main(int argc, char **argv)
{
	int c;
	char *type = NULL;
	char *ifname = NULL;
	char *time_end = NULL;
	char *time_start = NULL;

	if (argc == 1) {
		show_help();
		return 0;
	}
	
	while ((c = getopt(argc, argv, "wri:t:s:qc")) != -1)
	{
		switch (c) {
			case 'w':
				type = "w";
				break;
			case 'r':
				type = "r";
				break;
			case 'q':
				type = "q";
				break;
			case 'i':
				ifname = optarg;
				break;
			case 't':
				time_end = optarg;
				break;
			case 's':
				time_start = optarg;
				break;
			case 'c':
				type = "c";
				break;
			case '?':
				printf("%s: option %c has wrong command\n", __FUNCTION__, optopt);
				return -1;
			default:
				show_help();
				break;
		}
	}

	return traffic_limiter_main(type, ifname, time_end, time_start);
}
