/*
 * Inband EAP
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_eap_sm.h 268017 2011-06-21 09:48:46Z $
 */

#ifndef _AP_EAP_SM_H_
#define _AP_EAP_SM_H_

#include <eap_defs.h>
#include <proto/ethernet.h>

typedef struct {
	char state; /* current state. */
	char sent_msg_id; /* out wps message ID */
	char recv_msg_id; /* in wps message ID */
	unsigned char sta_mac[ETHER_ADDR_LEN]; /* STA's ethernet address */
	unsigned char bssid[ETHER_ADDR_LEN]; /* incoming bssid */
	unsigned char eap_id; /* identifier of the last request  */
	char msg_to_send[WPS_EAP_DATA_MAX_LENGTH]; /* message to be sent */
	int msg_len;

	/* For fragmented packets */
	char frags_received[WPS_EAP_DATA_MAX_LENGTH]; /* buffer to store all fragmentations  */
	/*
	 * From the Length Field of the first frag.  To be set to 0 when packet is not fragmented
	 * or set to the new value when receiving the begining of the next fragmented packet.
	 */
	int total_bytes_to_recv; /* total bytes of WPS data need to receive */
	int total_received; /* total WPS data received so far */
	/*
	 * After a fragment is sent and acked, the next_frag_to_send pointer
	 * is moved along the msg_to_send buffer. It is reset to the
	 * begining of the buffer after all fragments have been sent.
	 * When this pointer is not at the begining, the receive ap_eap_sm_process_sta_msg
	 * must only accept WPS_FRAG_ACK messages.
	 */
	char *next_frag_to_send; /* pointer to the next fragment to send */
	int frag_size;

	int ap_m2d_len;
	char *ap_m2d_data;
	int flags;

	char *last_sent; /* remember last sent EAPOL packet location */
	int last_sent_len;
	int resent_count;
	unsigned long last_check_time;
	int send_count;
	void *mc_dev;
	char * (*parse_msg)(char *, int, int *);
	unsigned int (*send_data)(char *, uint32);
	int eap_frag_threshold;
} EAP_WPS_AP_STATE;

#define AP_EAP_SM_AP_M2D_READY	0x1
#define AP_EAP_SM_M1_RECVD	0x2
#define AP_EAP_SM_M2_SENT	0x4

int ap_eap_sm_process_timeout();
int ap_eap_sm_startWPSReg(unsigned char *sta_mac, unsigned char *ap_mac);
uint32 ap_eap_sm_process_sta_msg(char *msg, int msg_len);
uint32 ap_eap_sm_init(void *mc_dev, char *mac_sta, char * (*parse_msg)(char *, int, int *),
	unsigned int (*send_data)(char *, uint32), int eap_frag_threshold);
uint32 ap_eap_sm_deinit(void);
uint32 ap_eap_sm_sendMsg(char * dataBuffer, uint32 dataLen);
uint32 ap_eap_sm_sendWPSStart(void);
unsigned char *ap_eap_sm_getMac(int mode);
int ap_eap_sm_get_msg_to_send(char **data);
void ap_eap_sm_Failure(int deauthFlag);
int ap_eap_sm_check_timer(uint32 time);
int ap_eap_sm_get_eap_state(void);
int ap_eap_sm_resend_last_sent();


#endif /* _AP_EAP_SM_H_ */
