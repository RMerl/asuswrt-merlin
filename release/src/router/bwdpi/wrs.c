/*
	wrs.c for TrendMicro WRS (parental control)
*/

#include "bwdpi.h"

void setup_wrs_conf()
{
	FILE *fp;
	if((fp = fopen(WRS_CONF, "w")) == NULL){
		printf("fail to open qosd.conf\n");
		return;
	}

	fprintf(fp, "enable_wre=1\n");
	fclose(fp);
}

void stop_wrs()
{
	eval("killall", "wred");
}

void start_wrs()
{
	char buff[128];

	// create wred.conf
	setup_wrs_conf();

	sprintf(buff, "LD_LIBRARY_PATH=%s wred -f %s -D -B&", USR_BWDPI, WRS_CONF);
	//dbg("start_wrs : buff =%s\n", buff);
	system(buff);
}

int wrs_main(char *cmd)
{
	if(!f_exists(TMP_BWDPI))
		eval("mkdir", "-p", TMP_BWDPI);

	if(!strcmp(cmd, "restart")){
		stop_wrs();
		start_wrs();
	}
	else if(!strcmp(cmd, "stop")){
		stop_wrs();
	}
	else if(!strcmp(cmd, "start")){
		start_wrs();
	}
	return 1;
}
