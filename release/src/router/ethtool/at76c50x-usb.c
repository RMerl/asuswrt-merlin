#include <stdio.h>
#include "internal.h"

static char *hw_versions[] = {
        "503_ISL3861",
        "503_ISL3863",
        "        503",
        "    503_ACC",
        "        505",
        "   505_2958",
        "       505A",
        "     505AMX",
};

int
at76c50x_usb_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u8 version = (u8)(regs->version >> 24);
	u8 rev_id = (u8)(regs->version);
	char *ver_string;

	if (version != 0)
		return -1;

	ver_string = hw_versions[rev_id];
	fprintf(stdout,
		"Hardware Version                    %s\n",
		ver_string);

	return 0;
}

