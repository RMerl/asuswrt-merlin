#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <bcmnvram.h>
#include <bcmdevs.h>
#include "shared.h"

int get_model(void)
{
	if(nvram_match("productid","RT-N66U")) return MODEL_RTN66U;
	else if(nvram_match("productid","RT-AC66U")) return MODEL_RTAC66U;
	else if(nvram_match("productid","RT-N53")) return MODEL_RTN53;
	else if(nvram_match("productid","RT-N16")) return MODEL_RTN16;
	else if(nvram_match("productid","RT-N15U")) return MODEL_RTN15U;
	else if(nvram_match("productid", "RT-N12")) return MODEL_RTN12;
	else if(nvram_match("productid", "RT-N12B1")) return MODEL_RTN12B1;
	else if(nvram_match("productid", "RT-N12C1")) return MODEL_RTN12C1;
	else if(nvram_match("productid", "RT-N12D1")) return MODEL_RTN12D1;
	else if(nvram_match("productid", "RT-N12HP")) return MODEL_RTN12HP;
	else if(nvram_match("productid", "RT-N10U")) return MODEL_RTN10U;
	else if(nvram_match("productid", "RT-N10D")) return MODEL_RTN10D;
#ifdef RTCONFIG_RALINK
	else if(nvram_match("productid", "EA-N66")) return MODEL_EAN66;
	else if(nvram_match("productid", "RT-N56U")) return MODEL_RTN56U;
#ifdef RTCONFIG_DSL
	else if(nvram_match("productid", "DSL-N55U")) return MODEL_DSLN55U;
	else if(nvram_match("productid", "DSL-N55U-B")) return MODEL_DSLN55U;	
#endif
	else if(nvram_match("productid", "RT-N13U")) return MODEL_RTN13U;
#endif
	else return MODEL_UNKNOWN;
}
