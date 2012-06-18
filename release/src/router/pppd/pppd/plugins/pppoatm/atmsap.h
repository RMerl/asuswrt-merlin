/* atmsap.h - ATM Service Access Point addressing definitions */

/* Written 1996-1998 by Werner Almesberger, EPFL LRC/ICA */


#ifndef _ATMSAP_H
#define _ATMSAP_H

#include <stdint.h>
#include <linux/atmsap.h>


/*
 * Selected ISO/IEC TR 9577 Network Layer Protocol Identifiers (NLPID)
 */

#define NLPID_IEEE802_1_SNAP	0x80	/* IEEE 802.1 SNAP */

/*
 * Selected Organizationally Unique Identifiers (OUIs)
 */

#define ATM_FORUM_OUI		"\x00\xA0\x3E"	/* ATM Forum */
#define EPFL_OUI		"\x00\x60\xD7"	/* EPF Lausanne, CH */

/*
 * Selected vendor-specific application identifiers (for B-HLI). Such an
 * identifier consists of three bytes containing the OUI, followed by four
 * bytes assigned by the organization owning the OUI.
 */

#define ANS_HLT_VS_ID		ATM_FORUM_OUI "\x00\x00\x00\x01"
					 /* ATM Name System, af-saa-0069.000 */
#define VOD_HLT_VS_ID		ATM_FORUM_OUI "\x00\x00\x00\x02"
						     /* VoD, af-saa-0049.001 */
#define AREQUIPA_HLT_VS_ID	EPFL_OUI "\x01\x00\x00\x01"	/* Arequipa  */
#define TTCP_HLT_VS_ID		EPFL_OUI "\x01\x00\x00\x03"	/* ttcp_atm */


/* Mapping of "well-known" TCP, UDP, etc. port numbers to ATM BHLIs.
   btd-saa-api-bhli-01.02 */

void atm_tcpip_port_mapping(char *vs_id,uint8_t protocol,uint16_t port);

#endif
