/*
	dpi.c for TrendMicro DPI engine switch
*/

#include "bwdpi.h"

int dpi_main(char *cmd)
{
	dbg("dpi switch %s\n", cmd);
	eval("echo", cmd, ">", "bwdpi_set");

	return 1;
}
