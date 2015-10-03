/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "globals.h"

#include "statemachines.h"
#include "bandfuncs.h"
#include "packetio.h"

char TUX_Content[] = {0x9C,0x01,0x00,0x00,0x00,0x00,0xCD,0xCE,0xD1,0x03,0xD1,0xD1,0xD1,0x05,0xCB,0xC8,
                      0xC7,0x06,0xCC,0xCB,0xCB,0x06,0xD5,0xD1,0xD4,0x04,0xDE,0xDB,0xDB,0x05,0xDC,0xDD};

char WRT_Content[] = {0xC0,0xFF,0xEC,0xE2,0xC3,0xFF,0xCC,0xC5,0xAF,0xFF,0x94,0x91,0x8B,0xFF,0x72,0x71,
                      0x6E,0xFF,0x6E,0x6D,0x6A,0xFF,0x7D,0x7C,0x79,0xFF,0x5D,0x5C,0x58,0xE5,0x27,0x27};

static char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int 
openvpn_base64_encode(const void *data, int size, char **str) {
    char *s, *p;
    int i;
    int c;
    const unsigned char *q;

    if (size < 0)
		return -1;
    p = s = (char *) malloc(size * 4 / 3 + 4);
    if (p == NULL)
		return -1;
    q = (const unsigned char *) data;
    i = 0;
    for (i = 0; i < size;) {
		c = q[i++];
		c *= 256;
		if (i < size)
			c += q[i];
		i++;
		c *= 256;
		if (i < size)
			c += q[i];
		i++;
		p[0] = base64_chars[(c & 0x00fc0000) >> 18];
		p[1] = base64_chars[(c & 0x0003f000) >> 12];
		p[2] = base64_chars[(c & 0x00000fc0) >> 6];
		p[3] = base64_chars[(c & 0x0000003f) >> 0];
		if (i > size)
			p[3] = '=';
		if (i > size + 1)
			p[2] = '=';
		p += 4;
    }
    *p = 0;
    *str = s;

    return strlen(s);
}

int 
image_to_base64(char *file_path,  char *client_mac) {
	FILE * pFile;
	long lSize;
	char *buffer;
	char *base64;
	size_t result;
	int i;

	//toUpperCase
	char client_mac_temp[15];
	memset(client_mac_temp, 0, sizeof(client_mac_temp));
	sprintf(client_mac_temp, client_mac);
	int idex = 0;
	while ( client_mac_temp[idex] != 0 ) {
		if ( ( client_mac_temp[idex] >= 'a' ) && ( client_mac_temp[idex] <= 'z' ) ) {
			client_mac_temp[idex] = client_mac_temp[idex] - 'a' + 'A';
		}
		idex++;
	}

	pFile = fopen (file_path , "rb");
	if (pFile == NULL) {
		printf("File error\n");
		i = 0;
		return i;
	}

	// obtain file size:
	fseek (pFile , 0 , SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);

	// allocate memory to contain the whole file:
	buffer = (char*) malloc (sizeof(char)*lSize);
	base64 = (char*) malloc (sizeof(char)*lSize);

	if (buffer == NULL) {
		printf("Memory error\n");
		i = 0;
		return i;
	}

	// copy the file into the buffer:
	result = fread (buffer,1,lSize,pFile);
	if (result != lSize) {
		printf("Reading error\n");
		i = 0;
		return i;
	}

	//image binary to base64
	if (openvpn_base64_encode (buffer, lSize, &base64) <= 0) {
		printf("binary encode error \n");
		i = 0;
		return i;
	}

	//image header + image base64
	char image_base64[(strlen(base64) + 25)];
	memset(image_base64, 0, sizeof(image_base64));

	sprintf(image_base64, "data:image/jpeg;base64,");
	strcat(image_base64, base64);
	
	// terminate
	fclose (pFile);
	free (buffer);
	free (base64);

	//wirte image_base64 to file
	char write_file_path[35];
	memset(write_file_path, 0, sizeof(write_file_path)); //file path: /jffs/usericon/mac.log
	
	//Check folder exist or not
	if(!check_if_dir_exist("/jffs/usericon"))
		system("mkdir /jffs/usericon");

	sprintf(write_file_path, "/jffs/usericon/%s.log", client_mac_temp);

	//if have exist mac file , don't witre
	if(!check_if_file_exist(write_file_path)) {
		int str_len = strlen(image_base64);
		int i;
		FILE * pFile;
		pFile = fopen (write_file_path , "w");

		if (pFile == NULL) {
			printf("File error\n");
			i = 0;
			return i;
		}

		for(i = 0; i < str_len; i++) {
			fputc(image_base64[i], pFile);
		}
		fclose (pFile);
	}

	i = 1;
	return i;
}

/****************************
 * Timeout handlers - 
 * these are the time-based entry points, which also clock the state machine.
 */

void
state_block_timeout(void *cookie)
{
    g_block_timer = NULL;
    g_this_event.evtType = evtBlockTimeout;
    state_process_timeout();
}

void
state_charge_timeout(void *cookie)
{
    g_charge_timer     = NULL;
    g_ctc_packets      = 0;
    g_ctc_bytes        = 0;
    g_this_event.evtType  = evtChargeTimeout;
    g_this_event.ssn      = (session_t*) cookie;
    state_process_timeout();
}

void
state_emit_timeout(void *cookie)
{
    g_emit_timer       = NULL;
    g_this_event.evtType  = evtEmitTimeout;

    /* Probes are forced to associate with the mapping session, if there is one. */
    if (g_topo_session != NULL  &&  g_topo_session->ssn_is_valid)
    {
        g_this_event.ssn = g_topo_session;
    }

    state_process_timeout();
}

void
state_hello_delay_timeout(void *cookie)
{
    g_hello_timer      = NULL;
    g_this_event.evtType  = evtHelloDelayTimeout;
    g_this_event.ssn      = (session_t*) cookie;
    state_process_timeout();
}

void
state_inactivity_timeout(void *cookie)
{
    g_this_event.evtType  = evtInactivityTimeout;
    g_this_event.ssn      = (session_t*) cookie;
    g_this_event.ssn->ssn_InactivityTimer = NULL;
    state_process_timeout();
}


/* This function locates an existing session that is associated with the passed-in address */
static session_t *
find_session(etheraddr_t *this_addr)
{
    session_t   *ssn = &g_sessions[0];
    int i;

    for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
    {
        if ( (ssn->ssn_is_valid) && (ETHERADDR_EQUALS(&ssn->ssn_mapper_real, this_addr)) )
        {
            DEBUG({printf("find_session returning session %d @ %X\n",i,(uint)ssn);})
            return ssn;
        }
    }
    DEBUG({puts("find_session returning NULL");})
    return NULL;
}


static session_t *
new_session()
{
    session_t   *ssn = &g_sessions[0];
    int i;

    for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
    {
        if ( !(ssn->ssn_is_valid) )
        {
            ssn->ssn_is_valid = TRUE;
            ssn->ssn_XID = 0;
            memset(&ssn->ssn_mapper_real,0,sizeof(etheraddr_t));
            memset(&ssn->ssn_mapper_current,0,sizeof(etheraddr_t));
            ssn->ssn_use_broadcast = TRUE;
            ssn->ssn_TypeOfSvc = ToS_Unknown;
            ssn->ssn_InactivityTimer = NULL;
            return ssn;
        }
    }
    return NULL;
}


void
dump_packet(uint8_t *buf)
{
    topo_ether_header_t*	ehdr;
    topo_base_header_t*		bhdr;
    topo_discover_header_t*	dhdr;
    topo_hello_header_t*	hhdr;
    topo_qltlv_header_t*	qlhdr;
    topo_queryresp_header_t*	qrhdr;
    topo_flat_header_t*         fhdr;

    uint16_t	seqNum;
    uint16_t	genNum;
    uint16_t	numSta;
    uint16_t	tripleCnt;
    uint16_t	pktCnt;
    uint32_t	byteCnt;

    ehdr = (topo_ether_header_t*)(buf);
    bhdr = (topo_base_header_t*)(ehdr + 1);
    dhdr = (topo_discover_header_t*)(bhdr + 1);
    hhdr = (topo_hello_header_t*)(bhdr + 1);
    qlhdr = (topo_qltlv_header_t*)(bhdr + 1);
    qrhdr = (topo_queryresp_header_t*)(bhdr + 1);
    fhdr  = (topo_flat_header_t*)(bhdr + 1);

    /* Print Ether-hdr */
    DEBUG({printf("==================\nEthernet-header:\n  src=" ETHERADDR_FMT " dst=" ETHERADDR_FMT " E-type=0x%0X\n\n",
		ETHERADDR_PRINT(&ehdr->eh_src), ETHERADDR_PRINT(&ehdr->eh_dst), ehdr->eh_ethertype);})

    /* Print the Base hdr */
    seqNum = ntohs(bhdr->tbh_seqnum);
    DEBUG({printf("Base-header:\n  version=%d  ToS=%s  Opcode=%s  SeqNum=%d\n", bhdr->tbh_version, Lld2_tos_names[bhdr->tbh_tos],
           Topo_opcode_names[bhdr->tbh_opcode], seqNum);})

    DEBUG({printf("  real-src=" ETHERADDR_FMT " real-dst=" ETHERADDR_FMT "\n\n",
		ETHERADDR_PRINT(&bhdr->tbh_realsrc), ETHERADDR_PRINT(&bhdr->tbh_realdst));})
 
    switch (bhdr->tbh_opcode)
    {
      case Opcode_Hello:
        genNum = ntohs(hhdr->hh_gen);
        break;

      case Opcode_Discover:
        genNum = ntohs(dhdr->mh_gen);
        numSta = ntohs(dhdr->mh_numstations);
        break;

      case Opcode_QueryResp:
        tripleCnt = ntohs(qrhdr->qr_numdescs);
        break;

      case Opcode_Flat:
        pktCnt  = ntohs(fhdr->fh_ctc_packets);
        byteCnt = ntohl(fhdr->fh_ctc_bytes);
        break;

      default:
        break;
    }
}



/*****************************************************************************
 *
 * This code processes the current packet-event (in g_this_event) by locating
 * (and possibly creating) the session with which it must be associated,
 * then passing it to the 3 state machines, smS, smE, and smT, in that order.
 *
 *****************************************************************************/ 
extern etheraddr_t	uutMAC;
extern MacInfo 		*MacList; //Yau
extern MacInfo		*MacQry;
extern IconFile		*IconList;
MacInfo			*MacNext;
char 			icon_file[64];
char                	src_mac[13];

uint 
icon_compare(char *cmp_icon)
{
    if( memcmp(TUX_Content, cmp_icon, 32) == 0)
	return 0;
    if( memcmp(WRT_Content, cmp_icon, 32) == 0)
	return 0;

    return 1;
}

uint
state_process_packet()
{
    session_t           *this_session;
    enum sm_Status      smStatus;
    int			MAC_chk = 0; //Yau
    MacInfo		*MacNode, *MacChk;

    IF_TRACED((TRC_STATE|TRC_PACKET))
        printf("state_process_packet: Entered with event %s",smEvent_names[g_this_event.evtType]);
        if (g_this_event.evtType==evtPacketRcvd)
        {
            printf(" (%s)\n",Topo_opcode_names[g_opcode]);
        } else {
            puts("");
        }
    END_TRACE

    if (g_opcode == Opcode_Hello)//  &&  ETHERADDR_IS_ZERO(&uutMAC))
    {
        memcpy(&uutMAC, &g_base_hdr->tbh_realsrc, sizeof(etheraddr_t));
/* Yau add */
	MacChk = MacList;
	while( MacChk != NULL )
	{
	//printf("\n  *** COMPARE MAC: " ETHERADDR_FMT " ? " ETHERADDR_FMT " ***\n",
	//ETHERADDR_PRINT(&uutMAC), ETHERADDR_PRINT(&MacChk->MacAddr));

	    if(memcmp(&MacChk->MacAddr, &uutMAC, sizeof(etheraddr_t))) //MAC different
		MacChk = MacChk->next;
	    else
		break;
	}

	if( MacChk == NULL )
	{
	    MacNode = (MacInfo *)malloc(sizeof(MacInfo));
            memcpy(&MacNode->MacAddr, &uutMAC, sizeof(etheraddr_t));
	    MacNode->getIcon = 0;
	    MacNode->query = 0;
	    MacNode->next = NULL;
	    DEBUG({printf("Get new MACAddr "ETHERADDR_FMT "\n", ETHERADDR_PRINT(&MacNode->MacAddr));})

		IconFile *icon = IconList;
		while( icon != NULL )
		{
                        sprintf(src_mac, "%02x%02x%02x%02x%02x%02x",
                                ETHERADDR_PRINT(&uutMAC));
			if( memcmp(icon->name, src_mac, 12) == 0)
			{
				MacNode->getIcon = 1;
				break;
			}
			icon = icon->next;
		}

	    if( MacList == NULL )
	    {
		MacList = MacNode;
		MacQry = MacNode;
	    }
	    else
		MacNext->next = MacNode;

	    MacNext = MacNode;
        }
/* End */
    }
    dump_packet(g_rxbuf);

    if (g_opcode == Opcode_QueryLargeTlvResp)
    {
    	topo_ether_header_t*    ehdr;
    	topo_base_header_t*     bhdr;
	icon_image_header_t*	icon_image_hdr;
	char *icon_ptr, *cmp_ptr;
	int  icon_copy_len, retry;

	retry = 0;
    	ehdr = (topo_ether_header_t*)(g_rxbuf);
    	bhdr = (topo_base_header_t*)(ehdr + 1);
	icon_image_hdr = (icon_image_header_t*)(bhdr + 1);

	if(icon_image_hdr->icon_image_type == ICON_IMAGE_TYPE)
	{
	    icon_ptr = icon_image_hdr + 1;
	    icon_copy_len = g_rcvd_pkt_len - 34;
	    g_rcvd_icon_len += icon_copy_len;
	    //copy data
	    DEBUG({printf("    *** Received icon len = %d\n", g_rcvd_icon_len);})
	    if(g_rcvd_icon_len == 1480)
	    {
		cmp_ptr = icon_ptr;
		cmp_ptr+= 1296;
	     	if (icon_compare(cmp_ptr))
		{
		    while( (g_icon_fd == NULL) && (retry < 5) )
		    {
		        sprintf(src_mac, "%02x%02x%02x%02x%02x%02x", 
                		ETHERADDR_PRINT(&ehdr->eh_src));//Yau
			sprintf(icon_file, "%s%s.ico", TMP_ICON_PATH, src_mac);
			DEBUG({printf("<>Write icon_file : %s <>\n", icon_file);})
			g_icon_fd = fopen(icon_file, "wb");

			if(g_icon_fd != NULL)
				break;
			else
				retry++;
		    }
		}
		else
		{
		    DEBUG({printf("\n!!!!! Get Default Icon !!!!!\n");})
		    g_rcvd_icon_len = -2;
		    return 0;
		}
	    }
	    if(g_icon_fd != NULL)
		fwrite(icon_ptr, sizeof(char), icon_copy_len, g_icon_fd);
	}
	else if(icon_image_hdr->icon_image_type == ICON_IMAGE_END)
        {
                icon_ptr = icon_image_hdr + 1;
		icon_copy_len = g_rcvd_pkt_len - 34;
                g_rcvd_icon_len += icon_copy_len;
                //copy data
		DEBUG({printf("End Of Icon Image!!!\n");})
                DEBUG({printf("    *** Reveiced icon len = %d\n", g_rcvd_icon_len);})
		g_rcvd_icon_len = -1;

		if(g_icon_fd != NULL)
		{
			fwrite(icon_ptr, sizeof(char), icon_copy_len, g_icon_fd);
			fclose(g_icon_fd);
			g_icon_fd = NULL;
			image_to_base64(icon_file, src_mac);
			//remove(icon_file);
		}
        }
	else
	{
		DEBUG({printf("\n*** Receive Icon Image Fail!!! ***\n\n");})
                g_rcvd_icon_len = -3;

                if (g_icon_fd != NULL)
                {
                        fwrite(icon_ptr, sizeof(char), icon_copy_len, g_icon_fd);
                        fclose(g_icon_fd);
                        g_icon_fd = NULL;
                }
                remove(icon_file);
	}

    }

    return 0;	/* Success! */
}


/******************************************************************************
 *
 * This code processes the current timeout-event (in g_this_event). Any session
 * associated with this event (only happens with activity-timeouts) is already
 * noted in the GLOBAL g_this_event (g_this_event).
 *
 ******************************************************************************/ 

uint
state_process_timeout()
{
    enum sm_Status         smStatus;

    IF_TRACED(TRC_STATE)
        if (g_this_event.evtType!=evtBlockTimeout)
            printf("state_process_timeout: Entered with event %s\n",smEvent_names[g_this_event.evtType]);
    END_TRACE

    return 0;	/* Success! */
}


/****************************
 * Helper functions -
 * actions performed as part of state processing.
 */

/* Restart the inactivity timer for the session associated with the current event */
void
restart_inactivity_timer(uint32_t timeout)
{
    struct timeval now;

    if (g_this_event.ssn == NULL  ||  g_this_event.ssn->ssn_is_valid != TRUE)   return;

    gettimeofday(&now, NULL);
    timeval_add_ms(&now, timeout);
    CANCEL(g_this_event.ssn->ssn_InactivityTimer);
    g_this_event.ssn->ssn_InactivityTimer = event_add(&now, state_inactivity_timeout, g_this_event.ssn);
}


/* Searches session table - returns TRUE if all valid sessions are in smS_Complete state */

bool_t
OnlyCompleteSessions(void)
{
    session_t   *ssn = &g_sessions[0];
    int i;

    for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
    {
        if (ssn->ssn_is_valid && ssn->ssn_state != smS_Complete)
        {
            return FALSE;
        }
    }
    return TRUE;
}


/* Searches session table - returns TRUE if all sessions are invalid */

bool_t
SessionTableIsEmpty(void)
{
    session_t   *ssn = &g_sessions[0];
    int i;

    for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
    {
        if (ssn->ssn_is_valid)
        {
            return FALSE;
        }
    }
    return TRUE;
}

bool_t
set_emit_timer(void)
{
    topo_emitee_desc_t *ed;

    assert(g_emit_remaining > 0);
    assert(g_emit_timer == NULL);

    /* get the next emitee_desc and schedule a callback when it is due
     * to be transmitted */
    ed = g_emitdesc;
    if (ed->ed_pause != 0)
    {
	struct timeval now;
	gettimeofday(&now, NULL);
	timeval_add_ms(&now, ed->ed_pause);
	g_emit_timer = event_add(&now, state_emit_timeout, NULL);
        return TRUE;
    } else {
	/* no pause; return PAUSE=FALSE */
	return FALSE;
    }
}
