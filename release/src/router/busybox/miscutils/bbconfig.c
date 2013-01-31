/* vi: set sw=4 ts=4: */
/* This file was released into the public domain by Paul Fox.
 */

//usage:#define bbconfig_trivial_usage
//usage:       ""
//usage:#define bbconfig_full_usage "\n\n"
//usage:       "Print the config file used by busybox build"

#include "libbb.h"
#include "bbconfigopts.h"
#if ENABLE_FEATURE_COMPRESS_BBCONFIG
# include "bb_archive.h"
# include "bbconfigopts_bz2.h"
#endif

int bbconfig_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bbconfig_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
#if ENABLE_FEATURE_COMPRESS_BBCONFIG
	bunzip_data *bd;
	int i = start_bunzip(&bd,
			/* src_fd: */ -1,
			/* inbuf:  */ bbconfig_config_bz2,
			/* len:    */ sizeof(bbconfig_config_bz2));
	/* read_bunzip can longjmp to start_bunzip, and ultimately
	 * end up here with i != 0 on read data errors! Not trivial */
	if (!i) {
		/* Cannot use xmalloc: will leak bd in NOFORK case! */
		char *outbuf = malloc_or_warn(sizeof(bbconfig_config));
		if (outbuf) {
			read_bunzip(bd, outbuf, sizeof(bbconfig_config));
			full_write1_str(outbuf);
		}
	}
#else
	full_write1_str(bbconfig_config);
#endif
	return 0;
}
