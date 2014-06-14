/*
	stat.c for TrendMicro app parental control / application statistics / device statistics
*/

#include "bwdpi.h"

int stat_main(int timeval, char *action)
{
	if(!f_exists(TMP_BWDPI))
		eval("mkdir", "-p", TMP_BWDPI);

	if(action == NULL)
		return 0;

	if(timeval != 0)
		return 0;	
	
	return 1;
}
