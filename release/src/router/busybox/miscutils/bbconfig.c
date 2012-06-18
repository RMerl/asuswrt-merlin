/* vi: set sw=4 ts=4: */
/* This file was released into the public domain by Paul Fox.
 */
#include "libbb.h"
#include "bbconfigopts.h"

int bbconfig_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bbconfig_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	full_write1_str(bbconfig_config);
	return 0;
}
