/* orckit_quirks.c ...... fix quirks in orckit adsl modems
 *                        mulix <mulix@actcom.co.il>
 *
 * $Id: orckit_quirks.c,v 1.3 2002/03/01 01:23:36 quozl Exp $
 */

#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "pptp_msg.h"
#include "pptp_options.h"
#include "pptp_ctrl.h"
#include "util.h"



/* return 0 on success, non zero otherwise */
int
orckit_atur3_build_hook(struct pptp_out_call_rqst* packet)
{
    unsigned int name_length = 10;

    struct pptp_out_call_rqst fixed_packet = {
	PPTP_HEADER_CTRL(PPTP_OUT_CALL_RQST),
	0, /* hton16(call->callid) */
	0, /* hton16(call->sernum) */
	hton32(PPTP_BPS_MIN), hton32(PPTP_BPS_MAX),
	hton32(PPTP_BEARER_DIGITAL), hton32(PPTP_FRAME_ANY),
	hton16(PPTP_WINDOW), 0, hton16(name_length), 0,
	{'R','E','L','A','Y','_','P','P','P','1',0}, {0}
    };

    if (!packet)
	return -1;

    memcpy(packet, &fixed_packet, sizeof(*packet));

    return 0;
}

/* return 0 on success, non zero otherwise */
int
orckit_atur3_set_link_hook(struct pptp_set_link_info* packet,
			   int peer_call_id)
{
    struct pptp_set_link_info fixed_packet = {
	PPTP_HEADER_CTRL(PPTP_SET_LINK_INFO),
	hton16(peer_call_id),
	0,
	0xffffffff,
	0xffffffff};

    if (!packet)
	return -1;

    memcpy(packet, &fixed_packet, sizeof(*packet));
    return 0;
}

/* return 0 on success, non 0 otherwise */
int
orckit_atur3_start_ctrl_conn_hook(struct pptp_start_ctrl_conn* packet)
{
    struct pptp_start_ctrl_conn fixed_packet = {
	{0}, /* we'll set the header later */
	hton16(PPTP_VERSION), 0, 0,
	hton32(PPTP_FRAME_ASYNC), hton32(PPTP_BEARER_ANALOG),
	hton16(0) /* max channels */,
	hton16(0x6021),
	{'R','E','L','A','Y','_','P','P','P','1',0}, /* hostname */
	{'M','S',' ','W','i','n',' ','N','T',0} /* vendor */
    };

    if (!packet)
	return -1;

    /* grab the header from the original packet, since we dont
       know if this is a request or a reply */
    memcpy(&fixed_packet.header, &packet->header, sizeof(struct pptp_header));

    /* and now overwrite the full packet, effectively preserving the header */
    memcpy(packet, &fixed_packet, sizeof(*packet));
    return 0;
}


