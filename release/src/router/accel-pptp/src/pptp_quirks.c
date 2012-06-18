/* pptp_quirks.c ...... various options to fix quirks found in buggy adsl modems
 *                      mulix <mulix@actcom.co.il>
 *
 * $Id: pptp_quirks.c,v 1.2 2001/11/23 03:42:51 quozl Exp $
 */

#include <string.h>
#include "orckit_quirks.h"
#include "pptp_quirks.h"

static int quirk_index = -1;

struct pptp_fixup pptp_fixups[] = {
    {BEZEQ_ISRAEL, ORCKIT, ORCKIT_ATUR3,
     orckit_atur3_build_hook,
     orckit_atur3_start_ctrl_conn_hook,
     orckit_atur3_set_link_hook}
};

static int fixups_sz = sizeof(pptp_fixups)/sizeof(pptp_fixups[0]);

/* return 0 on success, non 0 otherwise */
int set_quirk_index(int index)
{
    if (index >= 0 && index < fixups_sz) {
	quirk_index = index;
	return 0;
    }

    return -1;
}

int get_quirk_index()
{
    return quirk_index;
}

/* return the index for this isp in the quirks table, -1 if not found */
int find_quirk(const char* isp_name)
{
    int i = 0;
    if (isp_name) {
	while (i < fixups_sz && pptp_fixups[i].isp) {
	    if (!strcmp(pptp_fixups[i].isp, isp_name)) {
		return i;
	    }
	    ++i;
	}
    }

    return -1;
}


