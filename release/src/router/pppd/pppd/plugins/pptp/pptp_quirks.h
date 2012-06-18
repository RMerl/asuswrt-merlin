/* pptp_quirks.h ...... various options to fix quirks found in buggy adsl modems
 *                      mulix <mulix@actcom.co.il>
 *
 * $Id: pptp_quirks.h,v 1.1 2001/11/20 06:30:10 quozl Exp $
 */

#ifndef INC_PPTP_QUIRKS_H
#define INC_PPTP_QUIRKS_H

/* isp defs - correspond to slots in the fixups table */
#define BEZEQ_ISRAEL "BEZEQ_ISRAEL"

/* vendor defs */

#define ORCKIT 1
#define ALCATEL 2

/* device defs */

#define ORCKIT_ATUR2 1
#define ORCKIT_ATUR3 2

#include "pptp_msg.h"
#include "pptp_ctrl.h"

struct pptp_fixup {
    const char* isp;    /* which isp? e.g. Bezeq in Israel */
    int vendor; /* which vendor? e.g. Orckit */
    int device; /* which device? e.g. Orckit Atur3 */

    /* use this hook to build your own out call request packet */
    int (*out_call_rqst_hook)(struct pptp_out_call_rqst* packet);

    /* use this hook to build your own start control connection packet */
    /* note that this hook is called from two different places, depending
       on whether this is a request or reply */
    int (*start_ctrl_conn)(struct pptp_start_ctrl_conn* packet);

    /* use this hook if you need to send a 'set_link' packet once
       the connection is established */
    int (*set_link_hook)(struct pptp_set_link_info* packet,
			 int peer_call_id);
};

extern struct pptp_fixup pptp_fixups[];

/* find the index for this isp in the quirks table */
/* return the index on success, -1 if not found */
int find_quirk(const char* isp_name);

/* set the global quirk index. return 0 on success, non 0 otherwise */
int set_quirk_index(int index);

/* get the global quirk index. return the index on success,
   -1 if no quirk is defined */
int get_quirk_index();


#endif /* INC_PPTP_QUIRKS_H */
