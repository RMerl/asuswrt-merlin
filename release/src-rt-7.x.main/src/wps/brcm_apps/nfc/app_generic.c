/*****************************************************************************
 **
 **  Name:           app_generic.c
 **
 **  Description:    NSA generic application
 **
 **  Copyright (c) 2012, Broadcom Corp., All Rights Reserved.
 **  Broadcom Bluetooth Core. Proprietary and confidential.
 **
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "nsa_api.h"

#include "gki_int.h"
#include "uipc.h"

#include "nsa_dm_api.h"
#include "app_generic.h"
#include "app_nsa_utils.h"
#include "app_p2p_data_test.h"
#include "app_raw_frame_data_test.h"
#include "app_ndef_data_test.h"
#include "nsa_ndef_utils.h"
#include "ndef_utils.h"
/*
 * Defines
 */
/*it will register a not default SNEP server to support REQ_GET*/
#define BEAM_SUPPORT

/*
 * Types
 */

typedef enum
{
    APP_NSA_NO_ACTION,
    APP_NSA_RF_DISCOVERY,
    APP_NSA_SBEAM,
    APP_NSA_P2P_SERVER,
    APP_NSA_P2P_CLIENT,
    APP_NSA_SNEP_SERVER,
    APP_NSA_SNEP_CLIENT,
    APP_NSA_CHO,
}tAPP_NSA_STOP_ACTION;

typedef enum
{
    APP_NSA_NO_AUTO_CONNECTION = 0,
    APP_NSA_CONNECT_BY_NAME,
    APP_NSA_CONNECT_BY_SAP,
}tAPP_NSA_AUTO_CONNECTION_MODE;

typedef struct
{
	UINT8	remote_sap;	/*only used when readUI on connection-less*/
	UINT32	data_len;
	UINT8	data[MAX_P2P_DATA_LENGTH];	/*allocate a static buffer.... maybe need to increase*/
	BOOLEAN	more_data;
}tREAD_PACKET;

typedef struct
{
	tAPP_NSA_AUTO_CONNECTION_MODE	auto_conn;
	int		remote_sap;
	int		send_data_times;
	int		send_data_size;
	UINT16	local_miu;
	UINT16	remote_miu;
	BOOLEAN	disconnect_after_sending;
}tCLIENT_TEST;

typedef struct
{
	BOOLEAN		accept_connection;
	BOOLEAN		loopback_data;
}tSERVER_TEST;

typedef enum
{
	NO_OP,
	NDEF_APP_V2,
	NDEF_APP_V1,
	NDEF_SELECT_CC,
	NDEF_READ_CC,
	NDEF_READ_DUMMY,
}tT4T_OP_STATUS;

/*union of differetn tag operation status*/
typedef union
{
	tT4T_OP_STATUS	t4t_status;
}tTAG_OP_STATUS;

typedef struct
{
	tTAG_OP_STATUS		status;
}tRAW_DATA_CB;

typedef enum
{
	NSA_SNEP_PUT_REQ,
	NSA_SNEP_GET_REQ,
}tSNEP_REQ;

typedef struct
{
	tSNEP_REQ	req;
	UINT32		tx_ndef_length;
	UINT8*		tx_ndef_data;
}tSNEP_TEST;

typedef enum
{
	NSA_CHO_NONE,
	NSA_CHO_BT,
	NSA_CHO_WIFI,
}tNSA_CHO_SEL_OPTION;

typedef struct
{
	BOOLEAN		bt_availalble;
	BOOLEAN		wifi_availalble;
	tNSA_CHO_SEL_OPTION	preference;
	BOOLEAN		cho_server_started;
}tCHO_TEST;

typedef struct
{
	/*NDEF handler*/
    tNFA_HANDLE		ndef_default_handle;
    tNFA_HANDLE		ndef_sbeam_handle;
    tNFA_HANDLE		ndef_sbeam2_handle;
    /*SNEP variables*/
    tNFA_HANDLE		snep_server_handle;
    tNFA_HANDLE		snep_client_handle;
    char			snep_service_name[LLCP_MAX_SN_LEN];
    int				snep_service_name_lenght;
    tSNEP_TEST		snep_test;
    /*P2P variables*/
    tNFA_P2P_REG_SERVER	p2p_server;
    tNFA_P2P_REG_CLIENT p2p_client;
    tNFA_P2P_LINK_TYPE  server_link_type;
    tNFA_P2P_LINK_TYPE  client_link_type;
    BOOLEAN			p2p_llcp_is_active;
    tCLIENT_TEST	client_test;
    tSERVER_TEST	server_test;
    tREAD_PACKET	p2p_read_data;
    tNFA_HANDLE		conn_handle;  /*in this demo app code support just one p2p connection at time*/
    /*SEND RAW data variable*/
    tRAW_DATA_CB	raw_data_cb;
    BOOLEAN			rawData_done;
    tAPP_NSA_STOP_ACTION pending_stop_action;
    /*CHO test*/
    tCHO_TEST		cho_test;
} tAPP_NSA_CB;

/*
 * Global variables
 */
tAPP_NSA_CB app_nsa_cb;

/*
 * Local variables
 */
//static UINT8 *p_sbeam_rec_type = (UINT8 *) "application/com.sec.android.directconnect ";
static UINT8 *p_sbeam_rec_type = (UINT8 *) "text/DirectShareGallery";
static UINT8 *p_sbeam_rec_2_type = (UINT8 *) "android.com:pkg";
static char *p_snep_default_service_urn = "brcm_test:urn:nfc:sn:snep";    /* Extended SNEP server URN */
static char *p_sbeam_snep_extended_server_urn = "urn:nfc:sn:snepbeam";    /* Extended SNEP server URN */
static char *p_p2p_server_urn = "urn:nfc:xsn:broadcom.com:p2ptest";
/*use same to test one app as server and other as client; if test 3rd party device need to know the service*/
static char *p_p2p_client_urn = "urn:nfc:xsn:broadcom.com:p2ptest";
/*CHO*/
static char *p_bt_oob_type = "application/vnd.bluetooth.ep.oob";
static char *p_wsc_type = "application/vnd.wfa.wsc";

#define P2P_CLIENT_MIU	128
#define P2P_CLIENT_RW	1

#define P2P_SERVER_MIU	128
#define P2P_SERVER_RW	1

static const tNFA_INTF_TYPE protocol_to_rf_interface[] =
{
    NFA_INTERFACE_FRAME,    /* UNKNOWN */
    NFA_INTERFACE_FRAME,    /* NFA_PROTOCOL_T1T */
    NFA_INTERFACE_FRAME,    /* NFA_PROTOCOL_T2T */
    NFA_INTERFACE_FRAME,    /* NFA_PROTOCOL_T3T */
    NFA_INTERFACE_ISO_DEP,  /* NFA_PROTOCOL_ISO_DEP */
    NFA_INTERFACE_NFC_DEP   /* NFA_PROTOCOL_NFC_DEP */
};

/*
 * Local functions
 */
int app_nsa_dm_continue_stopRFdiscov(void);
int app_nsa_dm_continue_stopSbeam(void);
int app_nsa_p2p_continue_stopServer(void);
int app_nsa_p2p_continue_stopClient(void);
int app_nsa_dm_continue_stop_snep_client(void);
int app_nsa_dm_continue_stop_snep_server(void);
int app_nsa_dm_continue_stop_cho(void);
int app_nsa_p2p_connect(int connectBy);
int app_nsa_p2p_send_data(tNFA_HANDLE conn_handle, UINT8* p_data, int size);
int app_nsa_p2p_send_ui(tNFA_HANDLE handle, UINT16 dsap, UINT8* p_data, int size);
int app_nsa_p2p_read_data(tNFA_HANDLE conn_handle);
int app_nsa_p2p_read_ui(tNFA_HANDLE handle);
int app_nsa_p2p_disconnect(tNFA_HANDLE conn_handle);
int app_nsa_p2p_getRemoteSap(tNFA_HANDLE conn_handle);
int app_nsa_p2p_getLinkInfo(tNFA_HANDLE conn_handle);

extern void wps_nfc_dm_cback(tNSA_CONN_EVT event, int status);
extern void wps_nfc_done_cback(tNSA_CONN_EVT event, int status);
extern void wps_nfc_conn_cback(tNSA_CONN_EVT event, tNSA_STATUS status);
extern void wps_nfc_ndef_cback(UINT8 *ndef, UINT32 ndef_len);

/*definition of app callbacks*/

void nsa_parse_cho_req(tNSA_CHO_REQUEST * p_req, BOOLEAN * p_bt, BOOLEAN * p_wifi)
{
	UINT8*		p_rec;
	UINT8*      p_payload;
    UINT32      payload_len;
	int			i;

	*p_bt = FALSE;
	*p_wifi = FALSE;

	/*parse and display received request*/
	for (i=0; i < p_req->num_ac_rec; i++)
	{
		APP_INFO1("\t\t Parsing AC = %d ", i);
		APP_INFO1("\t\t   cps = % d ", p_req->ac_rec[i].cps);
		APP_INFO1("\t\t   aux_data_ref_count = % d ", p_req->ac_rec[i].aux_data_ref_count);
	}

	APP_DUMP("\t\tNDEF CHO req", p_req->ref_ndef, p_req->ref_ndef_len);
	APP_DEBUG0("\t\t Start to parse NDEF content of CHO req.");
	ParseNDefData(p_req->ref_ndef, p_req->ref_ndef_len);

	/*look for BT records*/
	p_rec = NDEF_MsgGetFirstRecByType (p_req->ref_ndef,
			NDEF_TNF_MEDIA,
			(UINT8*) p_bt_oob_type,
			(UINT8)strlen((char *) p_bt_oob_type));
	if (p_rec)
	{
		APP_INFO1("\t\t BT OOB present at 0x%x ", p_rec);
		*p_bt = TRUE;
	}

	/*look for WIFI records*/
	p_rec = NDEF_MsgGetFirstRecByType (p_req->ref_ndef,
			NDEF_TNF_WKT,
			(UINT8*)"Hc", 2);

	if (p_rec)
	{
		p_payload = NDEF_RecGetPayload (p_rec, &payload_len);

		if ((p_payload) && (payload_len >= strlen(p_wsc_type) + 2))
		{
			if ((*p_payload == NDEF_TNF_MEDIA)
					&&(*(p_payload + 1) == strlen(p_wsc_type))
					&&(!strncmp ((const char*)(p_payload + 2), p_wsc_type, strlen(p_wsc_type))))
			{
				APP_INFO1("\t\t Wifi Config present at 0x%x ", p_rec);
				*p_wifi = TRUE;
			}
		}
	}
}

void nsa_parse_cho_sel(tNSA_CHO_SELECT * p_sel)
{
	UINT8*		p_rec;
	int			i;

	/*parse and display received request*/
	for (i=0; i < p_sel->num_ac_rec; i++)
	{
		APP_INFO1("\t\t Parsing AC = %d ", i);
		APP_INFO1("\t\t   cps = % d ", p_sel->ac_rec[i].cps);
		APP_INFO1("\t\t   aux_data_ref_count = % d ", p_sel->ac_rec[i].aux_data_ref_count);
	}

	APP_DUMP("\t\tNDEF CHO sel", p_sel->ref_ndef, p_sel->ref_ndef_len);
	APP_DEBUG0("\t\t Start to parse NDEF content of CHO sel.");
	ParseNDefData(p_sel->ref_ndef, p_sel->ref_ndef_len);

	/*look for BT records*/
	p_rec = NDEF_MsgGetFirstRecByType (p_sel->ref_ndef,
			NDEF_TNF_MEDIA,
			(UINT8*) p_bt_oob_type,
			(UINT8)strlen((char *) p_bt_oob_type));
	if (p_rec)
	{
		APP_INFO1("\t\t BT OOB select at 0x%x ", p_rec);
	}

	/*look for WIFI records*/
	p_rec = NDEF_MsgGetFirstRecByType (p_sel->ref_ndef,
			NDEF_TNF_MEDIA,
			(UINT8*) p_wsc_type,
			(UINT8)strlen((char *) p_wsc_type));
	if (p_rec)
	{
		APP_INFO1("\t\t Wifi Config select present at 0x%x ", p_rec);
	}
}

void app_nsa_cho_cback(tNSA_CHO_EVT event, tNSA_CHO_EVT_DATA *p_data)
{
	BOOLEAN 				req_bt, req_wifi;
	tNSA_CHO_SEND_SEL		app_nsa_cho_send_sel;
	tNSA_CHO_SEND_SEL_ERR 	app_nsa_cho_send_sel_err;
	tNSA_STATUS				nsa_status;
	UINT8            		*p_ndef_header;

    switch(event)
    {
    case NSA_CHO_STARTED_EVT:
        APP_INFO1("NSA_CHO_STARTED_EVT Status:%d", p_data->status);
        break;
    case NSA_CHO_REQ_EVT:
    	APP_INFO1("NSA_CHO_REQ_EVT Status:%d", p_data->request.status);
    	if (p_data->request.status == NFA_STATUS_OK)
    	{
    		APP_INFO1("\t received %d Alternative Carrier", p_data->request.num_ac_rec);
    	}
    	else
    	{
    		app_nsa_cb.cho_test.cho_server_started = FALSE;
    		break;
    	}


    	nsa_parse_cho_req(&p_data->request, &req_bt, &req_wifi);

    	if (req_bt || req_wifi)
    	{
    		NSA_ChoSendSelectInit(&app_nsa_cho_send_sel);
    		NDEF_MsgInit (app_nsa_cho_send_sel.ndef_data, MAX_NDEF_LENGTH, &(app_nsa_cho_send_sel.ndef_len));
    	}
    	else
    	{
    		NSA_ChoSendSelectErrorInit(&app_nsa_cho_send_sel_err);

    		app_nsa_cho_send_sel_err.error_data = 0;
    		app_nsa_cho_send_sel_err.error_reason = NFA_CHO_ERROR_CARRIER;

    		nsa_status = NSA_ChoSendSelectError(&app_nsa_cho_send_sel_err);
    		if (nsa_status != NSA_SUCCESS)
    		{
    			APP_ERROR1("NSA_ChoSendSelectError failed:%d", nsa_status);
    		}
    	}
    	if ((req_bt) && (app_nsa_cb.cho_test.bt_availalble))
    	{/*if BT is requested by peer and it is avaialable... add it in case...*/
    		if ((!req_wifi) ||   // is the only alternative carrier or...
    			(app_nsa_cb.cho_test.preference == NSA_CHO_BT) ||   // it is the preferred carrier
    			(app_nsa_cb.cho_test.preference == NSA_CHO_NONE))   // no preference
    		{
    			/*add hard-coded BT_OOB and ac info value*/
    			/* restore NDEF header which might be updated during previous appending record */
    			p_ndef_header = (UINT8*)bt_oob_rec;
    			*p_ndef_header |= (NDEF_ME_MASK|NDEF_MB_MASK);
    			NDEF_MsgAppendRec (app_nsa_cho_send_sel.ndef_data, MAX_NDEF_LENGTH, &(app_nsa_cho_send_sel.ndef_len),
    					bt_oob_rec, BT_OOB_REC_LEN);

    			app_nsa_cho_send_sel.ac_info[app_nsa_cho_send_sel.num_ac_info].cps          = NFA_CHO_CPS_ACTIVE;
    			app_nsa_cho_send_sel.ac_info[app_nsa_cho_send_sel.num_ac_info].num_aux_data = 0;
    			app_nsa_cho_send_sel.num_ac_info++;
    		}

    	}
    	if ((req_wifi) && (app_nsa_cb.cho_test.wifi_availalble))
    	{/*if WiFi is requested by peer and it is avaialable... add it in case...*/
    		if ((!req_bt) ||   // is the only alternative carrier or...
    			(app_nsa_cb.cho_test.preference == NSA_CHO_WIFI) ||   // it is the preferred carrier
    			(app_nsa_cb.cho_test.preference == NSA_CHO_NONE))   // no preference
    		{
    			/*add hard-coded WIFI_OOB and ac info value*/
    			/* restore NDEF header which might be updated during previous appending record */
    			p_ndef_header = (UINT8*)wifi_hs_rec;
    			*p_ndef_header |= (NDEF_ME_MASK|NDEF_MB_MASK);
			/* Kenlo Modified */
    			NDEF_MsgAppendRec (app_nsa_cho_send_sel.ndef_data, MAX_NDEF_LENGTH, &(app_nsa_cho_send_sel.ndef_len),
    					wifi_hs_rec, wifi_hs_rec_len);

    			app_nsa_cho_send_sel.ac_info[app_nsa_cho_send_sel.num_ac_info].cps          = NFA_CHO_CPS_ACTIVE;
    			app_nsa_cho_send_sel.ac_info[app_nsa_cho_send_sel.num_ac_info].num_aux_data = 0;
    			app_nsa_cho_send_sel.num_ac_info++;
    		}
    	}

    	/*send select*/
    	if (req_bt || req_wifi)
    	{
    		NSA_ChoSendSelect(&app_nsa_cho_send_sel);
    	}
    	break;

    case NSA_CHO_SEL_EVT:
    	APP_INFO1("NSA_CHO_SEL_EVT Status:%d", p_data->status);
    	if (p_data->status == NFA_STATUS_OK)
    	{
    		APP_INFO1("\t received %d Alternative Carrier", p_data->request.num_ac_rec);
    	}
    	else
    		break;

    	/*just display recieved select NDEF*/
    	nsa_parse_cho_sel(&p_data->select);

    	break;

    case NSA_CHO_SEL_ERR_EVT:
    	APP_INFO0("NSA_CHO_SEL_ERR_EVT");
    	APP_INFO1("\terror_data = 0x%x", p_data->sel_err.error_data);
    	APP_INFO1("\terror_reason = 0x%x", p_data->sel_err.error_reason);
    	break;

    case NSA_CHO_TX_FAIL_ERR_EVT:
    	APP_INFO1("NSA_CHO_TX_FAIL_ERR_EVT Status:%d", p_data->status);
    	break;

    default:
        APP_ERROR1("Bad CHO Event", event);
        break;
    }
}

void app_nsa_rw_cback(tNSA_RW_EVT event, tNSA_RW_EVT_DATA *p_data)
{
	switch(event) {
	case NSA_RW_STOP_EVT:             /* complete RW action */
	{
		APP_INFO1("NSA_RW_STOP_EVT Status:%d", p_data->status);
		if (p_data->status == NFA_STATUS_OK) {
			APP_INFO0("Ready to receive a new RW command.");
		}

		wps_nfc_done_cback(NSA_RW_STOP_EVT, p_data->status == NFA_STATUS_OK ? 0 : -1);
		break;
	}

	default:
		APP_ERROR1("Bad RW Event", event);
		break;
	}
}


void app_nsa_dm_cback(tNSA_DM_EVT event, tNSA_DM_MSG *p_data)
{
    switch(event)
    {
    case NSA_DM_ENABLED_EVT:             /* Result of NFA_Enable             */
        APP_INFO1("NSA_DM_ENABLED_EVT Status:%d", p_data->status);
        break;
    case NSA_DM_DISABLED_EVT:            /* Result of NFA_Disable            */
        APP_INFO0("NSA_DM_DISABLED_EVT");
        break;
    case NSA_DM_SET_CONFIG_EVT:          /* Result of NFA_SetConfig          */
        APP_INFO0("NSA_DM_SET_CONFIG_EVT");
        break;
    case NSA_DM_GET_CONFIG_EVT:          /* Result of NFA_GetConfig          */
        APP_INFO0("NSA_DM_GET_CONFIG_EVT");
        break;
    case NSA_DM_PWR_MODE_CHANGE_EVT:     /* Result of NFA_PowerOffSleepMode  */
        APP_INFO0("NSA_DM_PWR_MODE_CHANGE_EVT");
        break;
    case NSA_DM_RF_FIELD_EVT:            /* Status of RF Field               */
        APP_INFO1("NSA_DM_RF_FIELD_EVT Status:%d RfFieldStatus:%d", p_data->rf_field.status,
                p_data->rf_field.rf_field_status);
        break;
    case NSA_DM_NFCC_TIMEOUT_EVT:        /* NFCC is not responding           */
        APP_INFO0("NSA_DM_ENABLED_EVT");
        break;
    case NSA_DM_NFCC_TRANSPORT_ERR_EVT:  /* NCI Tranport error               */
        APP_INFO0("NSA_DM_ENABLED_EVT");
        break;
    default:
        APP_ERROR1("Bad Event", event);
        break;
    }
}

void app_nsa_conn_cback(tNSA_CONN_EVT event, tNSA_CONN_MSG *p_data)
{
	tNSA_DM_SELECT	nsa_dm_select;
	tNSA_STATUS     nsa_status;

    switch (event)
    {
    case NSA_POLL_ENABLED_EVT:
        APP_INFO1("NSA_POLL_ENABLED_EVT status:%d", p_data->status);
        break;
    case NSA_POLL_DISABLED_EVT:
        APP_INFO1("NSA_POLL_DISABLED_EVT status:%d", p_data->status);
        break;
    case NSA_RF_DISCOVERY_STARTED_EVT:
        APP_INFO0("NSA_RF_DISCOVERY_STARTED_EVT");
        break;
    case NSA_ACTIVATED_EVT:
        APP_INFO0("NSA_ACTIVATED_EVT");
        break;
    case NSA_DEACTIVATED_EVT:
        APP_INFO0("NSA_DEACTIVATED_EVT");
	break;
    case NSA_NDEF_DETECT_EVT:
        APP_INFO1("NSA_NDEF_DETECT_EVT status:%d", p_data->ndef_detect.status);
        break;
    case NSA_RF_DISCOVERY_STOPPED_EVT:
        APP_DEBUG0("RF discovery has been stopped.");

        switch (app_nsa_cb.pending_stop_action)
        {
        case APP_NSA_RF_DISCOVERY:
        	app_nsa_dm_continue_stopRFdiscov();
        	break;
        case APP_NSA_SBEAM:
        	app_nsa_dm_continue_stopSbeam();
        	break;
        case APP_NSA_P2P_SERVER:
        	app_nsa_p2p_continue_stopServer();
        	break;
        case APP_NSA_P2P_CLIENT:
        	app_nsa_p2p_continue_stopClient();
        	break;
        case APP_NSA_SNEP_CLIENT:
        	app_nsa_dm_continue_stop_snep_client();
        	break;
        case APP_NSA_SNEP_SERVER:
        	app_nsa_dm_continue_stop_snep_server();
        	break;
        case APP_NSA_CHO:
        	app_nsa_dm_continue_stop_cho();
        	break;

        default:
        	APP_DEBUG1("nothing to be stopped (%d)", app_nsa_cb.pending_stop_action);
        }
        app_nsa_cb.pending_stop_action = APP_NSA_NO_ACTION;
        break;
    case NSA_DATA_EVT:
    	APP_INFO0("NSA_DATA_EVT");
    	break;
    case NSA_LLCP_ACTIVATED_EVT:
    	APP_INFO0("NSA_LLCP_ACTIVATED_EVT");
    	break;
    case NSA_LLCP_DEACTIVATED_EVT:
    	APP_INFO0("NSA_LLCP_DEACTIVATED_EVT");
    	break;
    case NSA_SET_P2P_LISTEN_TECH_EVT:
    	APP_INFO0("NSA_SET_P2P_LISTEN_TECH_EVT");
    	break;
    case NSA_SELECT_RESULT_EVT:
    	APP_INFO0("NSA_SELECT_RESULT_EVT");
    	if (p_data->status == NFA_STATUS_FAILED)
    	{
    		APP_ERROR0("selection failed!!!");;
    	}
    	break;
    case NSA_READ_CPLT_EVT:
    	APP_INFO1("NSA_READ_CPLT_EVT: %d", p_data->status);
    	break;
    case NSA_WRITE_CPLT_EVT:
    	APP_INFO1("NSA_WRITE_CPLT_EVT: %d", p_data->status);
        break;
    case NSA_FORMAT_CPLT_EVT:
    	APP_INFO1("NSA_FORMAT_CPLT_EVT: %d", p_data->status);
        break;
    case NSA_SET_TAG_RO_EVT:
    	APP_INFO1("NSA_SET_TAG_RO_EVT: %d", p_data->status);
        break;
    case NSA_DISC_RESULT_EVT:
    	APP_INFO0("NSA_DISC_RESULT_EVT");
    	APP_INFO1("rf_disc_id = %d", p_data->disc_result.discovery_ntf.rf_disc_id);
    	APP_INFO1("protocol = %d", p_data->disc_result.discovery_ntf.protocol);
    	APP_INFO1("more = %d", p_data->disc_result.discovery_ntf.more);
    	if (p_data->disc_result.discovery_ntf.more == 0)
    	{/*just choose latest disc result in this demo code*/
    		NSA_DmSelectInit(&nsa_dm_select);

    		nsa_dm_select.protocol = p_data->disc_result.discovery_ntf.protocol;
    		nsa_dm_select.rf_disc_id = p_data->disc_result.discovery_ntf.rf_disc_id;
    		if (nsa_dm_select.protocol > NFC_PROTOCOL_NFC_DEP)
    			nsa_dm_select.rf_interface = NCI_INTERFACE_FRAME;
    		else
    			nsa_dm_select.rf_interface = protocol_to_rf_interface[nsa_dm_select.protocol];

    		/*add dummy delay */
    		GKI_delay(100);

    		nsa_status = NSA_DmSelect(&nsa_dm_select);
    		if (nsa_status != NSA_SUCCESS)
    		{
    			APP_ERROR1("NSA_DmSelect failed:%d", nsa_status);
    		}
    	}
    	break;

    default:
        APP_ERROR1("Unknown event:%d", event);
    }
}

void app_nsa_default_ndef_hdlr(tNSA_NDEF_EVT event, tNSA_NDEF_EVT_DATA *p_data)
{
    switch (event)
    {
    case NSA_NDEF_DATA_EVT:
        APP_DUMP("NDEF", p_data->ndef_data.data, p_data->ndef_data.len);
        APP_DEBUG0("--->>> Start to parse NDEF content.");
        ParseNDefData(p_data->ndef_data.data, p_data->ndef_data.len);
        break;

    case NSA_NDEF_REGISTER_EVT:
        if (p_data->ndef_reg.status == NFA_STATUS_OK)
        {
            APP_DEBUG1("Ndef handler 0x%x has been correctly registered.", p_data->ndef_reg.ndef_type_handle);
            app_nsa_cb.ndef_default_handle = p_data->ndef_reg.ndef_type_handle;
        }
        else
        {
            APP_DEBUG1("Ndef handler 0x%x has NOT been correctly registered.", p_data->ndef_reg.ndef_type_handle);
        }
        break;

    default:
        APP_ERROR1("Unknown event:%d", event);
    }
}

void app_nsa_t4t_conn_cback(tNSA_CONN_EVT event, tNSA_CONN_MSG *p_data)
{
	tNSA_DM_SEND_RAW_FRAME	raw_data_req;
	tNSA_STATUS     nsa_status;
	UINT16*			p_sw;
	UINT16			raw_data_len = 0;
	UINT8*			p_raw_data = NULL;
	static int 		dummy_cnt = 0;

    switch (event)
    {
    case NSA_POLL_ENABLED_EVT:
        APP_INFO1("T4T: NSA_POLL_ENABLED_EVT status:%d", p_data->status);
        break;
    case NSA_POLL_DISABLED_EVT:
        APP_INFO1("T4T: NSA_POLL_DISABLED_EVT status:%d", p_data->status);
        break;
    case NSA_RF_DISCOVERY_STARTED_EVT:
        APP_INFO0("T4T: NSA_RF_DISCOVERY_STARTED_EVT");
        break;
    case NSA_ACTIVATED_EVT:
        APP_INFO1("T4T: NSA_ACTIVATED_EVT protocol = %d", p_data->activated.activate_ntf.protocol);
        if ((p_data->activated.activate_ntf.protocol == NFC_PROTOCOL_ISO_DEP) &&
        		(app_nsa_cb.rawData_done == FALSE))
        {
        	APP_INFO0("T4T: ISO-DEP protocol detected, send command to select NDEF tag app v2.0");
        	NSA_DmSendRawFrameInit(&raw_data_req);

        	raw_data_req.data_len = T4T_NDEF_APP_LENGTH;
        	memcpy(raw_data_req.raw_data, t4t_select_ndef_app_v2, raw_data_req.data_len);

        	nsa_status = NSA_DmSendRawFrame(&raw_data_req);
        	if (nsa_status != NSA_SUCCESS)
        	{
        		APP_ERROR1("NSA_DmSendRawFrame failed:%d", nsa_status);
        	}
        	app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_APP_V2;
        	/*reset each time activated and sent first command*/
        	dummy_cnt = 0;
        }
        break;
    case NSA_DEACTIVATED_EVT:
        APP_INFO0("T4T: NSA_DEACTIVATED_EVT");
        break;
    case NSA_NDEF_DETECT_EVT:
        APP_INFO1("T4T: NSA_NDEF_DETECT_EVT status:%d", p_data->ndef_detect.status);
        break;
    case NSA_RF_DISCOVERY_STOPPED_EVT:
        APP_DEBUG0("T4T: RF discovery has been stopped.");
        break;
    case NSA_DATA_EVT:
    	APP_INFO0("T4T: NSA_DATA_EVT");
    	APP_DUMP("T4T ", p_data->data.data, p_data->data.len);
    	/*get pointer to latest 2 byte where SW1 and SW2 are located*/
    	p_sw = (UINT16*) &p_data->data.data[p_data->data.len - 2];
    	APP_INFO1("SW1 and SW2 = 0x%x", *p_sw);
    	switch (app_nsa_cb.raw_data_cb.status.t4t_status)
    	{
    	case NDEF_APP_V2:
    		if (*p_sw == T4T_RSP_CMD_COMPLETED)
    		{/*command complete*/
    			APP_INFO0("T4T: ndef app v2.0 selected, send command to select CC");
    			raw_data_len = T4T_SELECT_CC_LENGTH;
    			p_raw_data = t4t_select_cc;
    			app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_SELECT_CC;
    		}
    		else
    		{/*command failed*/
    			APP_INFO0("T4T: ndef app v2.0 not found, try v1.0");
    			raw_data_len = T4T_NDEF_APP_LENGTH - 1;
    			p_raw_data = t4t_select_ndef_app_v1;
    			app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_APP_V1;
    		}
    		break;
    	case NDEF_APP_V1:
    		if (*p_sw == T4T_RSP_CMD_COMPLETED)
    		{/*command complete*/
    			APP_INFO0("T4T: ndef app v1.0 selected, send command to select CC");
    			raw_data_len = T4T_SELECT_CC_LENGTH;
    			p_raw_data = t4t_select_cc;
    			app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_SELECT_CC;
    		}
    		else
    		{/*command failed*/
    			APP_INFO0("T4T: ndef app v1.0 not found, send read dummy");
    			raw_data_len = T4T_READ_LENGTH;
    			p_raw_data = t4t_read_dummy;
    			app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_READ_DUMMY;
    		}
    		break;
    	case NDEF_SELECT_CC:
    		if (*p_sw == T4T_RSP_CMD_COMPLETED)
    		{/*command complete*/
    			APP_INFO0("T4T: CC selected, send command to read CC");
    			raw_data_len = T4T_READ_LENGTH;
    			p_raw_data = t4t_read_cc;
    			app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_READ_CC;
    		}
    		else
    		{/*command failed*/
    			APP_INFO0("T4T: CC not found, send read dummy");
    			raw_data_len = T4T_READ_LENGTH;
    			p_raw_data = t4t_read_dummy;
    			app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_READ_DUMMY;
    		}
    		break;
    	case NDEF_READ_CC:
    		if (*p_sw == T4T_RSP_CMD_COMPLETED)
    		{/*command complete*/
    			/*later in test app we may add command to read ndef file etc.etc.*/
    			APP_INFO0("T4T: CC read, send command to read dummy");
    			raw_data_len = T4T_READ_LENGTH;
    			p_raw_data = t4t_read_dummy;
    			app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_READ_DUMMY;
    		}
    		else
    		{/*command failed*/
    			APP_INFO0("T4T: CC not read, send read dummy");
    			raw_data_len = T4T_READ_LENGTH;
    			p_raw_data = t4t_read_dummy;
    			app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_READ_DUMMY;
    		}
    		break;
    	case NDEF_READ_DUMMY:
    		APP_INFO1("T4T: in dummy send %d", dummy_cnt);
    		raw_data_len = T4T_READ_LENGTH;
    		p_raw_data = t4t_read_dummy;
    		app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_READ_DUMMY;
    		dummy_cnt++;
    		if (dummy_cnt < SEND_DUMMY_MAX)
    		{
    			app_nsa_cb.raw_data_cb.status.t4t_status = NDEF_READ_DUMMY;
    		}
    		else
    		{
    			app_nsa_cb.raw_data_cb.status.t4t_status = NO_OP;
    			/*reset for next test*/
    			dummy_cnt = 0;
    		}
    		break;
    	case NO_OP:
    		APP_INFO0("T4T: no more RAW data to send");
    		raw_data_len = 0;
    		p_raw_data = NULL;
    		app_nsa_cb.rawData_done = TRUE;
    		break;
    	}

    	if (raw_data_len)
    	{
    		NSA_DmSendRawFrameInit(&raw_data_req);
    		raw_data_req.data_len = raw_data_len;
    		memcpy(raw_data_req.raw_data, p_raw_data, raw_data_req.data_len);

    		nsa_status = NSA_DmSendRawFrame(&raw_data_req);
    		if (nsa_status != NSA_SUCCESS)
    		{
    			APP_ERROR1("NSA_DmSendRawFrame failed:%d", nsa_status);
    		}
    	}

    	break;
    case NSA_LLCP_ACTIVATED_EVT:
    	APP_INFO0("T4T: NSA_LLCP_ACTIVATED_EVT");
    	break;
    case NSA_LLCP_DEACTIVATED_EVT:
    	APP_INFO0("T4T: NSA_LLCP_DEACTIVATED_EVT");
    	break;
    case NSA_SET_P2P_LISTEN_TECH_EVT:
    	APP_INFO0("T4T: NSA_SET_P2P_LISTEN_TECH_EVT");
    	break;
    case NSA_SELECT_RESULT_EVT:
    	APP_INFO0("T4T: NSA_SELECT_RESULT_EVT");
    	if (p_data->status == NFA_STATUS_FAILED)
    	{
    		APP_ERROR0("T4T: selection failed!!!");;
    	}
    	break;
    case NSA_DISC_RESULT_EVT:
    	APP_INFO0("T4T: NSA_DISC_RESULT_EVT");
    	break;
    case NSA_EXCLUSIVE_RF_CONTROL_STARTED_EVT:
    	APP_INFO0("T4T: NSA_EXCLUSIVE_RF_CONTROL_STARTED_EVT");
    	break;
    case NSA_EXCLUSIVE_RF_CONTROL_STOPPED_EVT:
    	APP_INFO0("T4T: NSA_EXCLUSIVE_RF_CONTROL_STOPPED_EVT");
    	break;


    default:
        APP_ERROR1("T4T: Unknown event:%d", event);
    }
}

void app_nsa_t4t_ndef_cback(tNSA_NDEF_EVT event, tNSA_NDEF_EVT_DATA *p_data)
{
    switch (event)
    {
    case NSA_NDEF_DATA_EVT:
        APP_DUMP("T4T NDEF", p_data->ndef_data.data, p_data->ndef_data.len);
        APP_DEBUG0("--->>> Start to parse NDEF content on T4T.");
        ParseNDefData(p_data->ndef_data.data, p_data->ndef_data.len);
        break;

    case NSA_NDEF_REGISTER_EVT:
        if (p_data->ndef_reg.status == NFA_STATUS_OK)
        {
            APP_DEBUG1("T4T Ndef handler 0x%x has been correctly registered.", p_data->ndef_reg.ndef_type_handle);
            app_nsa_cb.ndef_default_handle = p_data->ndef_reg.ndef_type_handle;
        }
        else
        {
            APP_DEBUG1("T4T Ndef handler 0x%x has NOT been correctly registered.", p_data->ndef_reg.ndef_type_handle);
        }
        break;

    default:
        APP_ERROR1("T4T Unknown event:%d", event);
    }
}

void app_nsa_sbeam_ndef_hdlr(tNSA_NDEF_EVT event, tNSA_NDEF_EVT_DATA *p_data)
{
    switch (event)
    {
    case NSA_NDEF_DATA_EVT:
        APP_DUMP("NDEF", p_data->ndef_data.data, p_data->ndef_data.len);
        APP_DEBUG0("--->>> Start to parse Sbeam NDEF content.");
        ParseNDefData(p_data->ndef_data.data, p_data->ndef_data.len);
        break;

    case NSA_NDEF_REGISTER_EVT:
        if (p_data->ndef_reg.status == NFA_STATUS_OK)
        {
            APP_DEBUG1("Ndef handler 0x%x has been correctly registered.", p_data->ndef_reg.ndef_type_handle);
            app_nsa_cb.ndef_sbeam_handle = p_data->ndef_reg.ndef_type_handle;
        }
        else
        {
            APP_DEBUG1("Ndef handler 0x%x has NOT been correctly registered.", p_data->ndef_reg.ndef_type_handle);
        }
        break;

    default:
        APP_ERROR1("Unknown event:%d", event);
    }
}

void app_nsa_sbeam_2_ndef_hdlr(tNSA_NDEF_EVT event, tNSA_NDEF_EVT_DATA *p_data)
{
    switch (event)
    {
    case NSA_NDEF_DATA_EVT:
        APP_DUMP("NDEF", p_data->ndef_data.data, p_data->ndef_data.len);
        APP_DEBUG0("--->>> Start to parse Sbeam 2 NDEF content.");
        ParseNDefData(p_data->ndef_data.data, p_data->ndef_data.len);
        break;

    case NSA_NDEF_REGISTER_EVT:
        if (p_data->ndef_reg.status == NFA_STATUS_OK)
        {
            APP_DEBUG1("Ndef handler 0x%x has been correctly registered.", p_data->ndef_reg.ndef_type_handle);
            app_nsa_cb.ndef_sbeam2_handle = p_data->ndef_reg.ndef_type_handle;
        }
        else
        {
            APP_DEBUG1("Ndef handler 0x%x has NOT been correctly registered.", p_data->ndef_reg.ndef_type_handle);
        }
        break;
    default:
        APP_ERROR1("Unknown event:%d", event);
    }
}

void app_nsa_snep_client_callback(tNSA_SNEP_EVT event,
        tNSA_SNEP_EVT_DATA *p_data)
{
	tNSA_SNEP_CONNECT	app_nsa_snep_connect;
	tNSA_SNEP_PUT		app_nsa_snep_put;
	tNSA_SNEP_GET		app_nsa_snep_get;

    switch (event)
    {
    case NSA_SNEP_REG_EVT:
        if (p_data->reg.status == NFA_STATUS_OK)
        {
            APP_DEBUG1("[Client SNEP] registration succeed for handle 0x%x", p_data->reg.reg_handle);
            app_nsa_cb.snep_client_handle = p_data->reg.reg_handle;
        }
        else
        {
            APP_DEBUG1("[Client SNEP] registration failed for handle 0x%x", p_data->reg.reg_handle);
        }
        break;
    case NSA_SNEP_ACTIVATED_EVT:
        APP_DEBUG0("[Client SNEP] received NSA_SNEP_ACTIVATED_EVT");
        NSA_SnepConnectInit(&app_nsa_snep_connect);
        app_nsa_snep_connect.client_handle = app_nsa_cb.snep_client_handle;
        memcpy(app_nsa_snep_connect.service_name, app_nsa_cb.snep_service_name, LLCP_MAX_SN_LEN);
        NSA_SnepConnect(&app_nsa_snep_connect);
        break;
    case NSA_SNEP_DEACTIVATED_EVT:
        APP_DEBUG0("[Client SNEP] received NSA_SNEP_DEACTIVATED_EVT");
        break;
    case NSA_SNEP_CONNECTED_EVT:
        APP_DEBUG0("[Client SNEP] received NSA_SNEP_CONNECTED_EVT");
        if (app_nsa_cb.snep_test.req == NSA_SNEP_PUT_REQ)
        {
        	NSA_SnepPutInit(&app_nsa_snep_put);
        	app_nsa_snep_put.client_handle = app_nsa_cb.snep_client_handle;
        	app_nsa_snep_put.ndef_length = app_nsa_cb.snep_test.tx_ndef_length;
        	memcpy(app_nsa_snep_put.ndef_data, app_nsa_cb.snep_test.tx_ndef_data, app_nsa_snep_put.ndef_length);
        	NSA_SnepPut(&app_nsa_snep_put);
        }
        else if (app_nsa_cb.snep_test.req == NSA_SNEP_GET_REQ)
        {
        	NSA_SnepGetInit(&app_nsa_snep_get);
        	app_nsa_snep_get.client_handle = app_nsa_cb.snep_client_handle;
        	app_nsa_snep_get.ndef_length = app_nsa_cb.snep_test.tx_ndef_length;
        	memcpy(app_nsa_snep_get.ndef_data, app_nsa_cb.snep_test.tx_ndef_data, app_nsa_snep_get.ndef_length);
        	NSA_SnepGet(&app_nsa_snep_get);
        }
        else
        {
        	APP_ERROR1("Wrong Snep request = %d", app_nsa_cb.snep_test.req);
        }

        break;
    case NSA_SNEP_PUT_RESP_EVT:
        APP_DEBUG0("[Client SNEP]  NSA_SNEP_PUT_RESP_EVT");
        break;
    case NSA_SNEP_GET_RESP_CMPL_EVT:
    	APP_DEBUG0("[Client SNEP]  NSA_SNEP_GET_RESP_EVT");
    	break;

    case NSA_SNEP_GET_RESP_EVT:
    	APP_DEBUG0("[Client SNEP]  NSA_SNEP_GET_RESP_EVT");
    	APP_DUMP("[Client SNEP] received NDEF", p_data->get_resp.ndef_data, p_data->get_resp.ndef_length);

    	APP_DEBUG1("[Client SNEP] parse  NDEF content of length = %d",p_data->get_resp.ndef_length);

    	ParseNDefData(p_data->get_resp.ndef_data, p_data->get_resp.ndef_length);
    	break;

    case NSA_SNEP_DISC_EVT:
    	APP_DEBUG0("[Client SNEP]  NSA_SNEP_DISC_EVT");
    	break;

    default:
    	APP_DEBUG1("[Client SNEP]  unexpected event = %d", event);
        break;
    }
}

void app_nsa_snep_server_callback(tNSA_SNEP_EVT event,
        tNSA_SNEP_EVT_DATA *p_data)
{
	tNSA_SNEP_PUT_RSP app_nsa_snep_put_rsp;
	tNSA_SNEP_GET_RSP app_nsa_snep_get_rsp;
	UINT8 tnf, type_len;
	UINT8 *p_type;
	UINT8 tempbuf[256];

    switch (event)
    {
    case NSA_SNEP_REG_EVT:
        if (p_data->reg.status == NFA_STATUS_OK)
        {
            APP_DEBUG1("[Server SNEP] registration succeed for handle 0x%x", p_data->reg.reg_handle);
            if ((!strncmp(p_data->reg.service_name,
                    app_nsa_cb.snep_service_name, app_nsa_cb.snep_service_name_lenght)))
            {
                APP_DEBUG1("[Server SNEP] %s correctly registered.", p_data->reg.service_name);
                app_nsa_cb.snep_server_handle = p_data->reg.reg_handle;
            }
            else
            {
                /*this sholud be the case when a client is registered*/
            	APP_DEBUG1("[Server SNEP] Unexpected registration event for %s.", p_data->reg.service_name);
            	APP_DEBUG1("[Server SNEP] expected service for %s.", p_data->reg.service_name);
            }
        }
        else
        {
            APP_DEBUG1("[Server SNEP] registration failed for handle 0x%x", p_data->reg.reg_handle);;
        }
        break;
    case NSA_SNEP_ACTIVATED_EVT:
        APP_DEBUG0("[Server SNEP] NSA_SNEP_ACTIVATED_EVT");
        break;
    case NSA_SNEP_DEACTIVATED_EVT:
        APP_DEBUG0("[Server SNEP] NSA_SNEP_DEACTIVATED_EVT");
        break;
    case NSA_SNEP_CONNECTED_EVT:
        APP_DEBUG0("[Server SNEP] received NSA_SNEP_CONNECTED_EVT");
        break;
    case NSA_SNEP_PUT_REQ_EVT:
        APP_DEBUG1("[Server SNEP] received NSA_SNEP_PUT_REQ_EVT for handle 0x%x of NDEF msg length = %d",
                p_data->put_req.conn_handle);

        APP_DUMP("[Server SNEP] received NDEF", p_data->put_req.ndef_data, p_data->put_req.ndef_length);

        APP_DEBUG1("[Server SNEP] parse  NDEF content of length = %d",p_data->put_req.ndef_length);

        ParseNDefData(p_data->put_req.ndef_data, p_data->put_req.ndef_length);

        /*send back PUT_RSP*/
        NSA_SnepPutRspInit(&app_nsa_snep_put_rsp);
        app_nsa_snep_put_rsp.conn_handle = p_data->put_req.conn_handle;
        app_nsa_snep_put_rsp.resp_code = NFA_SNEP_RESP_CODE_SUCCESS;
        NSA_SnepPutRsp(&app_nsa_snep_put_rsp);
        break;

    case NSA_SNEP_GET_REQ_EVT:
    	APP_DEBUG1("[Server SNEP] received NSA_SNEP_GET_REQ_EVT for handle 0x%x of NDEF msg length = %d",
    			p_data->get_req.conn_handle);

    	APP_DUMP("[Server SNEP] received NDEF", p_data->get_req.ndef_data, p_data->get_req.ndef_length);

    	APP_DEBUG1("[Server SNEP] parse  NDEF content of length = %d",p_data->get_req.ndef_length);

    	ParseNDefData(p_data->get_req.ndef_data, p_data->get_req.ndef_length);

    	/*simulate a processing and decide the GET_RSP ndef based on received NDEF*/
    	p_type = NDEF_RecGetType(p_data->get_req.ndef_data, &tnf, &type_len);

    	if (tnf<TNF_MAX)
    	{
    		//APP_DEBUG1("\tTNF: %s \n", TNF_STR[tnf]);
    	}
    	else
    	{
    		APP_DEBUG1("\tTNF: invalid (0x%02X) \n", tnf);
    	}

    	if (tnf == NDEF_TNF_WKT)
    	{
    		/* Copy type */
    		memcpy(tempbuf, p_type, type_len);
    		tempbuf[type_len] = 0;
    		if (strcmp((char *)tempbuf, "T") == 0)
    		{
    			/*if Text Well-know Type is recieved send back text example*/
    			NSA_SnepGetRspInit(&app_nsa_snep_get_rsp);

    			app_nsa_snep_get_rsp.conn_handle = p_data->get_req.conn_handle;
    			app_nsa_snep_get_rsp.ndef_length = TEXT_NDEF_LENGTH;
    			memcpy(app_nsa_snep_get_rsp.ndef_data, text_example, app_nsa_snep_get_rsp.ndef_length);

    			NSA_SnepGetRsp(&app_nsa_snep_get_rsp);
    		}
    	}
    	else
    	{
    		/*if not send back OOB for Bluetooth*/
    		NSA_SnepGetRspInit(&app_nsa_snep_get_rsp);

    		app_nsa_snep_get_rsp.conn_handle = p_data->get_req.conn_handle;
    		app_nsa_snep_get_rsp.ndef_length = BT_OOB_NDEF_LENGTH;
    		memcpy(app_nsa_snep_get_rsp.ndef_data, bt_oob, app_nsa_snep_get_rsp.ndef_length);

    		NSA_SnepGetRsp(&app_nsa_snep_get_rsp);
    	}


    	break;

    case NSA_SNEP_DEFAULT_SERVER_STARTED_EVT:
        APP_DEBUG0("[Server SNEP] NSA_SNEP_DEFAULT_SERVER_STARTED_EVT");
        break;
    case NSA_SNEP_DEFAULT_SERVER_STOPPED_EVT:
        APP_DEBUG0("[Server SNEP] received NSA_SNEP_DEFAULT_SERVER_STOPPED_EVT");
        break;

    default:
        break;
    }
}

/*P2P callabck*/
void app_nsa_p2p_server_cback(tNSA_P2P_EVT event, tNFA_P2P_EVT_DATA *p_data)
{
	tNSA_P2P_REJECT_CONN	nsa_p2p_reject_conn;
	tNSA_P2P_ACCEPT_CONN	nsa_p2p_accept_conn;
	tNSA_STATUS             nsa_status;

	switch (event)
	{
	case NSA_P2P_REG_SERVER_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_REG_SERVER_EVT");
		/*copy info of received registration server event into nsa app Control Block*/
		if (p_data->reg_server.server_handle == NFA_HANDLE_INVALID)
		{
			APP_DEBUG0("P2P server registration failure!!!");
		}
		else
		{
			memcpy(&app_nsa_cb.p2p_server, &p_data->reg_server, sizeof(tNFA_P2P_REG_SERVER));
			APP_DEBUG1("P2P server for %s registered ...", app_nsa_cb.p2p_server.service_name);
			APP_DEBUG1(" ......... at SAP = %d ... ",app_nsa_cb.p2p_server.server_sap);
			APP_DEBUG1(" ......... server Handle 0x%x = %d !!!!!",app_nsa_cb.p2p_server.server_handle);
		}
		break;
	case NSA_P2P_REG_CLIENT_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_REG_CLIENT_EVT");
		break;
	case NSA_P2P_ACTIVATED_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_ACTIVATED_EVT");
		app_nsa_cb.p2p_llcp_is_active = TRUE;
		break;
	case NSA_P2P_DEACTIVATED_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_DEACTIVATED_EVT");
		app_nsa_cb.p2p_llcp_is_active = FALSE;
		break;
	case NSA_P2P_CONN_REQ_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_CONN_REQ_EVT");
		APP_DEBUG1("p_data->conn_req.remote_sap = %d", p_data->conn_req.remote_sap);
		APP_DEBUG1("p_data->conn_req.server_handle = 0x%x", p_data->conn_req.server_handle);
		APP_DEBUG1("app_nsa_cb.p2p_server.server_sap = %d", app_nsa_cb.p2p_server.server_sap);
		APP_DEBUG1("app_nsa_cb.p2p_server.server_handle = 0x%x", app_nsa_cb.p2p_server.server_handle);

		if (app_nsa_cb.server_test.accept_connection)
		{
			APP_INFO0("Accept Connection!!!");
			NSA_P2pAcceptConnInit(&nsa_p2p_accept_conn);

			nsa_p2p_accept_conn.conn_handle = p_data->conn_req.conn_handle;
			nsa_p2p_accept_conn.miu = (p_data->conn_req.remote_miu > P2P_SERVER_MIU) ? P2P_SERVER_MIU : p_data->conn_req.remote_miu;
			nsa_p2p_accept_conn.rw = ((p_data->conn_req.remote_rw > P2P_SERVER_RW) ? P2P_SERVER_RW : p_data->conn_req.remote_rw);

			nsa_status = NSA_P2pAcceptConn(&nsa_p2p_accept_conn);
			if (nsa_status != NSA_SUCCESS)
			{
				APP_ERROR1("NSA_P2pAcceptConn failed:%d", nsa_status);
			}

			app_nsa_cb.conn_handle = p_data->conn_req.conn_handle;
			APP_DEBUG1("Accepted connection handle = 0x%x", app_nsa_cb.conn_handle);
		}
		else
		{
			APP_INFO0("Reject Connection!!!");
			NSA_P2pRejectConnInit(&nsa_p2p_reject_conn);
			nsa_p2p_reject_conn.conn_handle = p_data->conn_req.conn_handle;

			nsa_status = NSA_P2pRejectConn(&nsa_p2p_reject_conn);
			if (nsa_status != NSA_SUCCESS)
			{
				APP_ERROR1("NSA_P2pRejectConn failed:%d", nsa_status);
			}
		}
		break;
	case NSA_P2P_CONNECTED_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_CONNECTED_EVT");
		if (app_nsa_cb.conn_handle != p_data->connected.conn_handle)
		{
			APP_INFO1("connection handle different from expected one: 0x%x", p_data->connected.conn_handle);

			app_nsa_cb.conn_handle = p_data->connected.conn_handle;
		}
		break;
	case NSA_P2P_DISC_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_DISC_EVT");
		break;
	case NSA_P2P_DATA_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_DATA_EVT");
		APP_INFO1("link type = %d", p_data->data.link_type);
		APP_INFO1("remote_sap = %d", p_data->data.remote_sap);
		APP_INFO1("handle = 0x%x", p_data->data.handle);
		if (p_data->data.link_type == NFA_P2P_DLINK_TYPE)
		{
			app_nsa_p2p_read_data(p_data->data.handle);
		}
		else if (p_data->data.link_type == NFA_P2P_LLINK_TYPE)
		{
			app_nsa_p2p_read_ui(p_data->data.handle);
		}
		else
		{
			APP_ERROR1("Unexpected link = %d.", p_data->data.link_type);
		}

		if (app_nsa_cb.p2p_read_data.more_data)
		{
			APP_INFO0("Waiting more data to come");
		}
		else
		{
			APP_DUMP("P2P server rx data", app_nsa_cb.p2p_read_data.data, app_nsa_cb.p2p_read_data.data_len);
			if (app_nsa_cb.server_test.loopback_data)
			{
				APP_INFO0("Send back received data");
				/*send back data over same link type*/
				if (p_data->data.link_type == NFA_P2P_DLINK_TYPE)
				{
					APP_DEBUG0("DATA LINK: loopback received packet.");
					app_nsa_p2p_send_data(p_data->data.handle, app_nsa_cb.p2p_read_data.data, app_nsa_cb.p2p_read_data.data_len);
				}
				else if (p_data->data.link_type == NFA_P2P_LLINK_TYPE)
				{
					APP_DEBUG0("LOGICAL LINK: loopback received packet.");
					app_nsa_p2p_send_ui(p_data->data.handle, p_data->data.remote_sap, app_nsa_cb.p2p_read_data.data, app_nsa_cb.p2p_read_data.data_len);
				}
				else
				{
					APP_ERROR1("Unexpected link = %d.", p_data->data.link_type);
				}
			}
		}
		break;
	case NSA_P2P_CONGEST_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_CONGEST_EVT");
		break;
	case NSA_P2P_LINK_INFO_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_LINK_INFO_EVT");
		break;
	case NSA_P2P_SDP_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_SDP_EVT");
		break;

	default:
		break;
	}
}
void app_nsa_p2p_client_cback(tNSA_P2P_EVT event, tNFA_P2P_EVT_DATA *p_data)
{
	static int sent_cnt = 0;

	switch (event)
	{
	case NSA_P2P_REG_SERVER_EVT:
		APP_INFO0("app_nsa_p2p_client_cback:  NSA_P2P_REG_SERVER_EVT");
		break;
	case NSA_P2P_REG_CLIENT_EVT:
		/*copy info of received registration client event into nsa app Control Block*/
		if (p_data->reg_client.client_handle == NFA_HANDLE_INVALID)
		{
			APP_DEBUG0("P2P client registration failure!!!");
		}
		else
		{
			app_nsa_cb.p2p_client.client_handle = p_data->reg_client.client_handle;
			APP_DEBUG1("P2P client registered: handle = 0x%x ", app_nsa_cb.p2p_client.client_handle);
		}
		break;
	case NSA_P2P_ACTIVATED_EVT:
		APP_INFO0("app_nsa_p2p_client_cback:  NSA_P2P_ACTIVATED_EVT");
		/*get link info just for test purpose; MIUinfo are already in received data*/
		app_nsa_p2p_getLinkInfo(p_data->activated.handle);
		app_nsa_cb.client_test.local_miu = p_data->activated.local_link_miu;
		app_nsa_cb.client_test.remote_miu = p_data->activated.remote_link_miu;

		app_nsa_cb.p2p_llcp_is_active = TRUE;
		if (app_nsa_cb.client_test.auto_conn == APP_NSA_CONNECT_BY_NAME)
		{
			/*no need to trigger SAP discovery*/
			APP_INFO0("Connection-oriented by name!!!");
			app_nsa_p2p_connect(1);
		}
		else if (app_nsa_cb.client_test.auto_conn == APP_NSA_CONNECT_BY_SAP)
		{
			APP_INFO0("Connection-oriented by SAP!!!");
			/*get remote SAP to use to connect when it will be received...*/
			app_nsa_p2p_getRemoteSap(p_data->activated.handle);
			//app_nsa_p2p_connect(2);
		}
		else
		{
			APP_INFO0("Connection-less !!!");
			/*get remote SAP to use to send data when it will be received...*/
			app_nsa_p2p_getRemoteSap(p_data->activated.handle);
		}
		break;
	case NSA_P2P_DEACTIVATED_EVT:
		APP_INFO0("app_nsa_p2p_client_cback:  NSA_P2P_DEACTIVATED_EVT");
		app_nsa_cb.p2p_llcp_is_active = FALSE;
		break;
	case NSA_P2P_CONN_REQ_EVT:
		APP_INFO0("app_nsa_p2p_client_cback:  NSA_P2P_CONN_REQ_EVT");
		break;
	case NSA_P2P_CONNECTED_EVT:
		APP_INFO0("app_nsa_p2p_client_cback:  NSA_P2P_CONNECTED_EVT");
		if (p_data->connected.conn_handle == NFA_HANDLE_INVALID)
		{
			APP_DEBUG0("P2P connection failure!!!");
		}
		else
		{
			app_nsa_cb.conn_handle = p_data->connected.conn_handle;
			APP_DEBUG1("P2P connection handle = 0x%x.", app_nsa_cb.conn_handle);
		}
		sent_cnt = 0;
		APP_DEBUG1("Sent packet num = %d.", sent_cnt);
		app_nsa_p2p_send_data(app_nsa_cb.conn_handle, data_packet[sent_cnt%P2P_MAX_TIMES], app_nsa_cb.client_test.send_data_size);
		sent_cnt++;
		break;
	case NSA_P2P_DISC_EVT:
		APP_INFO0("app_nsa_p2p_client_cback:  NSA_P2P_DISC_EVT");
		break;
	case NSA_P2P_DATA_EVT:
		APP_INFO0("app_nsa_p2p_client_cback:  NSA_P2P_DATA_EVT");
		APP_INFO1("link type = %d", p_data->data.link_type);
		APP_INFO1("remote_sap = %d", p_data->data.remote_sap);
		APP_INFO1("handle = 0x%x", p_data->data.handle);
		if (p_data->data.link_type == NFA_P2P_DLINK_TYPE)
		{
			app_nsa_p2p_read_data(p_data->data.handle);
		}
		else if (p_data->data.link_type == NFA_P2P_LLINK_TYPE)
		{
			app_nsa_p2p_read_ui(p_data->data.handle);
		}
		else
		{
			APP_ERROR1("Unexpected link = %d.", p_data->data.link_type);
		}

		if (app_nsa_cb.p2p_read_data.more_data)
		{
			APP_INFO0("Waiting more data to come");
		}
		else
		{
			APP_DUMP("P2P client rx data", app_nsa_cb.p2p_read_data.data, app_nsa_cb.p2p_read_data.data_len);
			/*send n-th packet*/
			if (sent_cnt < app_nsa_cb.client_test.send_data_times)
			{
				if (p_data->data.link_type == NFA_P2P_DLINK_TYPE)
				{
					APP_DEBUG1("DATA LINK: Sent packet num = %d.", sent_cnt);
					app_nsa_p2p_send_data(p_data->data.handle, data_packet[sent_cnt%P2P_MAX_TIMES], app_nsa_cb.client_test.send_data_size);
					sent_cnt++;
				}
				else if (p_data->data.link_type == NFA_P2P_LLINK_TYPE)
				{
					APP_DEBUG1("LOGICAL LINK: Sent packet num = %d.", sent_cnt);
					app_nsa_p2p_send_ui(p_data->data.handle, p_data->data.remote_sap, data_packet[sent_cnt%P2P_MAX_TIMES], app_nsa_cb.client_test.send_data_size);
					sent_cnt++;
				}
				else
				{
					APP_ERROR1("Unexpected link = %d.", p_data->data.link_type);
				}
			}
			else
			{
				if ((app_nsa_cb.client_test.disconnect_after_sending) && (p_data->data.link_type == NFA_P2P_DLINK_TYPE))
				{
					APP_DEBUG0("Sending test packet DONE!! Disconnect...");
					app_nsa_p2p_disconnect(p_data->data.handle);
				}
				else
				{
					APP_DEBUG0("Sending test packet DONE!!.");
				}
			}
		}
		break;
	case NSA_P2P_CONGEST_EVT:
		APP_INFO0("app_nsa_p2p_client_cback:  NSA_P2P_CONGEST_EVT");
		break;
	case NSA_P2P_LINK_INFO_EVT:
		APP_INFO0("app_nsa_p2p_server_cback:  NSA_P2P_LINK_INFO_EVT");
		if (p_data->link_info.local_link_miu != app_nsa_cb.client_test.local_miu)
		{
			APP_INFO1("Unexpected local_MIU (%d)", p_data->link_info.local_link_miu);
		}
		if (p_data->link_info.remote_link_miu != app_nsa_cb.client_test.remote_miu)
		{
			APP_INFO1("Unexpected remote_MIU (%d)", p_data->link_info.remote_link_miu);
		}
		APP_INFO1("Weel-Known Service = %d", p_data->link_info.wks);

		break;
	case NSA_P2P_SDP_EVT:
		APP_INFO0("app_nsa_p2p_client_cback:  NSA_P2P_SDP_EVT");
		//APP_INFO1("remote SAP = 0x%x",p_data->sdp.remote_sap);
		app_nsa_cb.client_test.remote_sap = p_data->sdp.remote_sap;

		if (app_nsa_cb.client_test.auto_conn == APP_NSA_CONNECT_BY_SAP)
		{
			APP_INFO1("connect to remote SAP = 0x%x", app_nsa_cb.client_test.remote_sap);
			app_nsa_p2p_connect(2);
		}
		else if (app_nsa_cb.client_test.auto_conn == APP_NSA_NO_AUTO_CONNECTION)
		{/*connection-less test so send data to SAP directly*/
			sent_cnt = 0;
			APP_DEBUG1("Sent packet to remote SAP 0x%x.", app_nsa_cb.client_test.remote_sap);
			app_nsa_p2p_send_ui(app_nsa_cb.p2p_client.client_handle, app_nsa_cb.client_test.remote_sap,
					data_packet[sent_cnt%P2P_MAX_TIMES], app_nsa_cb.client_test.send_data_size);
			sent_cnt++;
		}
		break;

	default:
		break;
	}
}

/*******************************************************************************
 **
 ** Function         app_nsa_init
 **
 ** Description      Init NSA application
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_gen_init(void)
{
    memset(&app_nsa_cb, 0 ,sizeof(app_nsa_cb));
    return 0;
}

/*******************************************************************************
 **
 ** Function         app_nsa_end
 **
 ** Description      This function is used to close application
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_nsa_end(void)
{
    app_nsa_dm_disable();
}

/*******************************************************************************
 **
 ** Function         app_nsa_dm_enable
 **
 ** Description      Enable NSA
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_enable(void)
{
    tNSA_DM_ENABLE nsa_req;
    tNSA_STATUS nsa_status;
    int port;

    NSA_DmEnableInit(&nsa_req);

    nsa_req.dm_cback = app_nsa_dm_cback;
    nsa_req.conn_cback = app_nsa_conn_cback;

    port = app_get_choice("Select Port (0 for local, other for BSA IPC)");

    nsa_req.port = (UINT8)port;

    nsa_status = NSA_DmEnable(&nsa_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmEnable failed:%d", nsa_status);
        return -1;
    }
    return 0;
}

/*******************************************************************************
 **
 ** Function         app_nsa_dm_disable
 **
 ** Description      Disable NSA
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_disable(void)
{
    tNSA_DM_DISABLE nsa_req;
    tNSA_STATUS nsa_status;
    
    NSA_DmDisableInit(&nsa_req);

    nsa_req.graceful = TRUE;
    //nsa_req.graceful = FALSE;

    nsa_status = NSA_DmDisable(&nsa_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmDisable failed:%d", nsa_status);
        return -1;
    }
    return 0;
}

/*******************************************************************************
 **
 ** Function         app_nsa_dm_startRFdiscov
 **
 ** Description      NSA start RF discovery
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_startRFdiscov(void)
{
    tNSA_DM_ENABLEPOLL            nsa_enable_poll_req;
    tNSA_DM_START_RF_DISCOV     nsa_start_rf_req;
    tNSA_DM_REG_NDEF_TYPE_HDLR    nsa_ndef_reg_req;
    tNSA_STATUS                 nsa_status;

    /*first register a default NDEF handler*/
    NSA_DmRegNdefTypeHdlrInit(&nsa_ndef_reg_req);

    nsa_ndef_reg_req.handle_whole_msg = TRUE;
    nsa_ndef_reg_req.tnf = NSA_TNF_DEFAULT;
    nsa_ndef_reg_req.p_ndef_cback = app_nsa_default_ndef_hdlr;
    memcpy(nsa_ndef_reg_req.type_name , "", 0);
    nsa_ndef_reg_req.type_name_len = 0;

    nsa_status = NSA_DmRegNdefTypeHdlr(&nsa_ndef_reg_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmRegNdefTypeHdlr failed:%d", nsa_status);
        return -1;
    }

    /*set polling mask*/
    NSA_DmEnablePollingInit(&nsa_enable_poll_req);

    nsa_enable_poll_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;

    nsa_status = NSA_DmEnablePolling(&nsa_enable_poll_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmEnablePolling failed:%d", nsa_status);
        return -1;
    }

    /*start RF discovery*/
    NSA_DmStartRfDiscovInit(&nsa_start_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStartRfDiscov(&nsa_start_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStartRfDiscov failed:%d", nsa_status);
        return -1;
    }

    return 0;
}

/*******************************************************************************
 **
 ** Function         app_nsa_dm_startRF
 **
 ** Description      NSA start RF activity only
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_startRF(void)
{
    tNSA_DM_START_RF_DISCOV     nsa_start_rf_req;
    tNSA_STATUS                 nsa_status;

    /*start RF discovery*/
    NSA_DmStartRfDiscovInit(&nsa_start_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStartRfDiscov(&nsa_start_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStartRfDiscov failed:%d", nsa_status);
        return -1;
    }

    return 0;
}

/*******************************************************************************
 **
 ** Function         app_nsa_dm_stopRF
 **
 ** Description      NSA stop RF discovery only
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_stopRF(void)
{
    tNSA_DM_STOP_RF_DISCOV             nsa_stop_rf_req;
    tNSA_STATUS                     nsa_status;

    /*first stop RF discovery*/
    NSA_DmStopRfDiscovInit(&nsa_stop_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStopRfDiscov(&nsa_stop_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStopRfDiscovInit failed:%d", nsa_status);
        return -1;
    }

    /*it wll be continued once RF discovery stopped event is received*/
    app_nsa_cb.pending_stop_action = APP_NSA_NO_ACTION;

    return 0;
}

/*******************************************************************************
 **
 ** Function         app_nsa_dm_stopRFdiscov
 **
 ** Description      NSA stop RF discovery
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_stopRFdiscov(void)
{
    tNSA_DM_STOP_RF_DISCOV             nsa_stop_rf_req;
    tNSA_STATUS                     nsa_status;

    /*first stop RF discovery*/
    NSA_DmStopRfDiscovInit(&nsa_stop_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStopRfDiscov(&nsa_stop_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStopRfDiscovInit failed:%d", nsa_status);
        return -1;
    }

    /*it wll be continued once RF discovery stopped event is received*/
    app_nsa_cb.pending_stop_action = APP_NSA_RF_DISCOVERY;

    return 0;
}

int app_nsa_dm_continue_stopRFdiscov(void)
{
    tNSA_DM_DISABLEPOLL                nsa_disable_poll_req;
    tNSA_DM_DEREG_NDEF_TYPE_HDLR    nsa_ndef_dereg_req;
    tNSA_STATUS                     nsa_status;

    /*second disable polling*/
    NSA_DmDisablePollingInit(&nsa_disable_poll_req);
    nsa_status = NSA_DmDisablePolling(&nsa_disable_poll_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmDisablePolling failed:%d", nsa_status);
        return -1;
    }

    /*last deregister the default NDEF handler*/
    NSA_DmDeregNdefTypeHdlrInit(&nsa_ndef_dereg_req);

    nsa_ndef_dereg_req.ndef_type_handle = app_nsa_cb.ndef_default_handle;

    nsa_status = NSA_DmDeregNdefTypeHdlr(&nsa_ndef_dereg_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmDeregNdefTypeHdlr failed:%d", nsa_status);
        return -1;
    }

    return 0;
}
/*******************************************************************************
 **
 ** Function         app_nsa_dm_startSbeam
 **
 ** Description      NSA starts SBEAM to accept incoming Sbeam procedure from a peer
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_startSbeam(void)
{
    tNSA_DM_REG_NDEF_TYPE_HDLR    nsa_ndef_reg_req;
    tNSA_DM_SET_P2P_TECH        nsa_set_p2p_tech_req;
    tNSA_SNEP_START_DFLT_SRV    nsa_snep_start_dflt_srvr_req;
    tNSA_SNEP_INIT				nsa_snep_init_req;
    tNSA_DM_START_RF_DISCOV     nsa_start_rf_req;
    tNSA_STATUS                 nsa_status;
#ifdef BEAM_SUPPORT
    tNSA_SNEP_REG_SERVER        nsa_snep_server;
#endif

    /*first register an default NDEF default handler*/
    NSA_DmRegNdefTypeHdlrInit(&nsa_ndef_reg_req);

    nsa_ndef_reg_req.handle_whole_msg = TRUE;
    nsa_ndef_reg_req.tnf = NSA_TNF_DEFAULT;
    nsa_ndef_reg_req.p_ndef_cback = app_nsa_default_ndef_hdlr;
//    nsa_ndef_reg_req.type_name = "";
    memcpy(nsa_ndef_reg_req.type_name , "", 0);
    nsa_ndef_reg_req.type_name_len = 0;

    nsa_status = NSA_DmRegNdefTypeHdlr(&nsa_ndef_reg_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmRegNdefTypeHdlr failed to register default type handler:%d", nsa_status);
        return -1;
    }

    /*second register an NDEF handler for Sbeam message*/
    NSA_DmRegNdefTypeHdlrInit(&nsa_ndef_reg_req);

    nsa_ndef_reg_req.handle_whole_msg = FALSE;
    nsa_ndef_reg_req.tnf = NSA_TNF_RFC2046_MEDIA;
    nsa_ndef_reg_req.p_ndef_cback = app_nsa_sbeam_ndef_hdlr;
    nsa_ndef_reg_req.type_name_len = (UINT8) strlen ((char *) p_sbeam_rec_type);
    memcpy(nsa_ndef_reg_req.type_name , p_sbeam_rec_type, nsa_ndef_reg_req.type_name_len);

    nsa_status = NSA_DmRegNdefTypeHdlr(&nsa_ndef_reg_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmRegNdefTypeHdlr failed to register Sbeam  type handler:%d", nsa_status);
        return -1;
    }

    /*third register an NDEF handler for Sbeam second record in message*/
    NSA_DmRegNdefTypeHdlrInit(&nsa_ndef_reg_req);

    nsa_ndef_reg_req.handle_whole_msg = FALSE;
    nsa_ndef_reg_req.tnf = NSA_TNF_EXTERNAL;
    nsa_ndef_reg_req.p_ndef_cback = app_nsa_sbeam_2_ndef_hdlr;
    nsa_ndef_reg_req.type_name_len = (UINT8) strlen ((char *) p_sbeam_rec_2_type);
    memcpy(nsa_ndef_reg_req.type_name , p_sbeam_rec_2_type, nsa_ndef_reg_req.type_name_len);

    nsa_status = NSA_DmRegNdefTypeHdlr(&nsa_ndef_reg_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmRegNdefTypeHdlr failed to register Sbeam 2 type handler:%d", nsa_status);
        return -1;
    }


    /*set P2P tech listen mask for Snep*/
    NSA_DmSetP2pListenTechInit(&nsa_set_p2p_tech_req);

    nsa_set_p2p_tech_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;

    nsa_status = NSA_DmSetP2pListenTech(&nsa_set_p2p_tech_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmSetP2pListenTech failed:%d", nsa_status);
        return -1;
    }

    /*init SNEP*/
    NSA_SnepInitInit(&nsa_snep_init_req);

    nsa_snep_init_req.p_server_cback = app_nsa_snep_server_callback;
    nsa_status =NSA_SnepInit(&nsa_snep_init_req);
    if (nsa_status != NSA_SUCCESS)
    {
    	APP_ERROR1("NSA_SnepInit failed:%d", nsa_status);
    	return -1;
    }

    /*start default SNEP server*/
    NSA_SnepStartDefaultServerInit(&nsa_snep_start_dflt_srvr_req);

    nsa_status = NSA_SnepStartDefaultServer(&nsa_snep_start_dflt_srvr_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_SnepStartDefaultServer failed:%d", nsa_status);
        return -1;
    }

#ifdef BEAM_SUPPORT
    NSA_SnepRegisterServerInit(&nsa_snep_server);

    nsa_snep_server.server_sap = NSA_SNEP_ANY_SAP;
    app_nsa_cb.snep_service_name_lenght = strlen ((char *) p_sbeam_snep_extended_server_urn);
    memcpy(app_nsa_cb.snep_service_name, p_sbeam_snep_extended_server_urn, app_nsa_cb.snep_service_name_lenght);
    memcpy(nsa_snep_server.service_name, app_nsa_cb.snep_service_name, app_nsa_cb.snep_service_name_lenght);

    nsa_status = NSA_SnepRegisterServer(&nsa_snep_server);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_SnepRegisterServer failed:%d", nsa_status);
        return -1;
    }
#endif

    /*start RF discovery*/
    NSA_DmStartRfDiscovInit(&nsa_start_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStartRfDiscov(&nsa_start_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStartRfDiscov failed:%d", nsa_status);
        return -1;
    }

    return 0;
}

/*******************************************************************************
 **
 ** Function         app_nsa_dm_stopSbeam
 **
 ** Description      NSA stops SBEAM
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_stopSbeam(void)
{
    tNSA_DM_STOP_RF_DISCOV             nsa_stop_rf_req;
    tNSA_STATUS                     nsa_status;

    /*first stop RF discovery*/
    NSA_DmStopRfDiscovInit(&nsa_stop_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStopRfDiscov(&nsa_stop_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStopRfDiscov failed:%d", nsa_status);
        return -1;
    }

    /*it will be continued once RF discovery stopped event is received*/
    app_nsa_cb.pending_stop_action = APP_NSA_SBEAM;

    return 0;
}

int app_nsa_dm_continue_stopSbeam(void)
{
    tNSA_DM_DEREG_NDEF_TYPE_HDLR    nsa_ndef_dereg_req;
    tNSA_DM_SET_P2P_TECH            nsa_set_p2p_tech_req;
    tNSA_SNEP_STOP_DFLT_SRV            nsa_snep_stop_dflt_srvr_req;
    tNSA_STATUS                        nsa_status;
#ifdef BEAM_SUPPORT
    tNSA_SNEP_DEREG                    nsa_snep_dereg;
#endif

    /*disable P2P tech mask*/
    NSA_DmSetP2pListenTechInit(&nsa_set_p2p_tech_req);

    nsa_set_p2p_tech_req.poll_mask = 0x00;
    nsa_status = NSA_DmSetP2pListenTech(&nsa_set_p2p_tech_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmSetP2pListenTech failed:%d", nsa_status);
        return -1;
    }

    /*stop default SNEP server*/
    NSA_SnepStopDefaultServerInit(&nsa_snep_stop_dflt_srvr_req);

    nsa_status = NSA_SnepStopDefaultServer(&nsa_snep_stop_dflt_srvr_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_SnepStopDefaultServerInit failed:%d", nsa_status);
        return -1;
    }

#ifdef BEAM_SUPPORT
    NSA_SnepDeregisterInit(&nsa_snep_dereg);

    nsa_snep_dereg.reg_handle = app_nsa_cb.snep_server_handle;

    nsa_status = NSA_SnepDeregister(&nsa_snep_dereg);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_SnepDeregister failed:%d", nsa_status);
        return -1;
    }
#endif

    /*last deregister all NDEF handlers for Sbeam*/
    NSA_DmDeregNdefTypeHdlrInit(&nsa_ndef_dereg_req);
    nsa_ndef_dereg_req.ndef_type_handle = app_nsa_cb.ndef_default_handle;
    nsa_status = NSA_DmDeregNdefTypeHdlr(&nsa_ndef_dereg_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmDeregNdefTypeHdlr failed to deregister default type handler:%d", nsa_status);
        return -1;
    }

    NSA_DmDeregNdefTypeHdlrInit(&nsa_ndef_dereg_req);
    nsa_ndef_dereg_req.ndef_type_handle = app_nsa_cb.ndef_sbeam_handle;
    nsa_status = NSA_DmDeregNdefTypeHdlr(&nsa_ndef_dereg_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmRegNdefTypeHdlr failed to register Sbeam  type handler:%d", nsa_status);
        return -1;
    }

    NSA_DmDeregNdefTypeHdlrInit(&nsa_ndef_dereg_req);
    nsa_ndef_dereg_req.ndef_type_handle = app_nsa_cb.ndef_sbeam2_handle;
    nsa_status = NSA_DmDeregNdefTypeHdlr(&nsa_ndef_dereg_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmRegNdefTypeHdlr failed to register Sbeam 2 type handler:%d", nsa_status);
        return -1;
    }

    return 0;
}

/*******************************************************************************
 **
 ** Function         app_nsa_p2p_start_server
 **
 ** Description      NSA start a P2P server
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_p2p_start_server(void)
{
	tNSA_P2P_REG_SERVER		nsa_p2p_reg_server_req;
	tNSA_DM_SET_P2P_TECH	nsa_set_p2p_tech_req;
	tNSA_DM_START_RF_DISCOV nsa_start_rf_req;
	tNSA_P2P_INIT			nsa_p2p_init_req;
	tNSA_STATUS             nsa_status;
	int 					choice;

	/*demo app just let have an app to be either client or server so register only server here*/
	NSA_P2pInitInit(&nsa_p2p_init_req);
	nsa_p2p_init_req.p_server_cback = app_nsa_p2p_server_cback;
	nsa_status = NSA_P2pInit(&nsa_p2p_init_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pInit failed:%d", nsa_status);
		return -1;
	}

	/*start P2P server*/
	NSA_P2pRegisterServerInit(&nsa_p2p_reg_server_req);

	choice = app_get_choice("Server supports LOGICAL link? [0 for No and 1 for Yes]: ");
	if (choice)
		nsa_p2p_reg_server_req.link_type |= NFA_P2P_LLINK_TYPE;

	choice = app_get_choice("Server supports DATA link? [0 for No and 1 for Yes]: ");
	if (choice)
		nsa_p2p_reg_server_req.link_type |= NFA_P2P_DLINK_TYPE;

	/*store to remember*/
	app_nsa_cb.server_link_type = nsa_p2p_reg_server_req.link_type;

	nsa_p2p_reg_server_req.server_sap = NFA_P2P_ANY_SAP;
	memcpy(nsa_p2p_reg_server_req.service_name, p_p2p_server_urn, strlen ((char *) p_p2p_server_urn));

	nsa_status = NSA_P2pRegisterServer(&nsa_p2p_reg_server_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pRegisterServer failed:%d", nsa_status);
		return -1;
	}

	/*set P2P tech listen mask for P2P*/
	NSA_DmSetP2pListenTechInit(&nsa_set_p2p_tech_req);
	nsa_set_p2p_tech_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;
	nsa_status = NSA_DmSetP2pListenTech(&nsa_set_p2p_tech_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmSetP2pListenTech failed:%d", nsa_status);
		return -1;
	}

    /*start RF discovery*/
	NSA_DmStartRfDiscovInit(&nsa_start_rf_req);
	/*no param to be passed*/
	nsa_status = NSA_DmStartRfDiscov(&nsa_start_rf_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmStartRfDiscov failed:%d", nsa_status);
		return -1;
	}

	return 0;

}

/*******************************************************************************
 **
 ** Function         app_nsa_p2p_stop_server
 **
 ** Description      NSA stop a P2P server
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_p2p_stop_server(void)
{
	tNSA_DM_STOP_RF_DISCOV          nsa_stop_rf_req;
	tNSA_STATUS                     nsa_status;

	/*first stop RF discovery*/
	NSA_DmStopRfDiscovInit(&nsa_stop_rf_req);
	/*no param to be passed*/
	nsa_status = NSA_DmStopRfDiscov(&nsa_stop_rf_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmStopRfDiscov failed:%d", nsa_status);
		return -1;
	}

	/*it will be continued once RF discovery stopped event is received*/
	app_nsa_cb.pending_stop_action = APP_NSA_P2P_SERVER;

	return 0;
}



int app_nsa_p2p_continue_stopServer(void)
{
	tNSA_P2P_DEREG		nsa_p2p_dereg_req;
	tNSA_STATUS         nsa_status;
	tNSA_DM_SET_P2P_TECH            nsa_set_p2p_tech_req;

	/*disable P2P tech mask*/
	NSA_DmSetP2pListenTechInit(&nsa_set_p2p_tech_req);

	nsa_set_p2p_tech_req.poll_mask = 0x00;
	nsa_status = NSA_DmSetP2pListenTech(&nsa_set_p2p_tech_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmSetP2pListenTech failed:%d", nsa_status);
		return -1;
	}

	/*stop P2P server*/
	NSA_P2pDeregisterInit(&nsa_p2p_dereg_req);

	nsa_p2p_dereg_req.handle = app_nsa_cb.p2p_server.server_handle;

	nsa_status = NSA_P2pDeregister(&nsa_p2p_dereg_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pDeregister failed:%d", nsa_status);
		return -1;
	}

	return 0;

}


/*******************************************************************************
 **
 ** Function         app_nsa_p2p_start_client
 **
 ** Description      NSA start a P2P client
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_p2p_start_client(void)
{
	tNSA_P2P_REG_CLIENT		nsa_p2p_reg_client_req;
	tNSA_DM_ENABLEPOLL      nsa_enable_poll_req;
	tNSA_DM_START_RF_DISCOV nsa_start_rf_req;
	tNSA_P2P_INIT			nsa_p2p_init_req;
	tNSA_STATUS             nsa_status;
	int						choice;

	/*demo app just let have an app to be either client or server so register only server here*/
	NSA_P2pInitInit(&nsa_p2p_init_req);
	nsa_p2p_init_req.p_client_cback = app_nsa_p2p_client_cback;
	nsa_status =NSA_P2pInit(&nsa_p2p_init_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pInit failed:%d", nsa_status);
		return -1;
	}

	/*start P2P client*/
	NSA_P2pRegisterClientInit(&nsa_p2p_reg_client_req);

	choice = app_get_choice("Client supports LOGICAL link? [0 for No and 1 for Yes]: ");
	if (choice)
		nsa_p2p_reg_client_req.link_type |= NFA_P2P_LLINK_TYPE;

	choice = app_get_choice("Client supports DATA link? [0 for No and 1 for Yes]: ");
	if (choice)
		nsa_p2p_reg_client_req.link_type |= NFA_P2P_DLINK_TYPE;

	/*store to remember*/
	app_nsa_cb.client_link_type = nsa_p2p_reg_client_req.link_type;

	nsa_status = NSA_P2pRegisterClient(&nsa_p2p_reg_client_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pRegisterClient failed:%d", nsa_status);
		return -1;
	}

	/*set polling mask*/
	NSA_DmEnablePollingInit(&nsa_enable_poll_req);
	nsa_enable_poll_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;

	nsa_status = NSA_DmEnablePolling(&nsa_enable_poll_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmEnablePolling failed:%d", nsa_status);
		return -1;
	}

	/*start RF discovery*/
	NSA_DmStartRfDiscovInit(&nsa_start_rf_req);
	/*no param to be passed*/
	nsa_status = NSA_DmStartRfDiscov(&nsa_start_rf_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmStartRfDiscov failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

/*******************************************************************************
 **
 ** Function         app_nsa_p2p_stop_client
 **
 ** Description      NSA stop a P2P client
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_p2p_stop_client(void)
{
	tNSA_DM_STOP_RF_DISCOV          nsa_stop_rf_req;
	tNSA_STATUS                     nsa_status;

	/*first stop RF discovery*/
	NSA_DmStopRfDiscovInit(&nsa_stop_rf_req);
	/*no param to be passed*/
	nsa_status = NSA_DmStopRfDiscov(&nsa_stop_rf_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmStopRfDiscov failed:%d", nsa_status);
		return -1;
	}

	/*it will be continued once RF discovery stopped event is received*/
	app_nsa_cb.pending_stop_action = APP_NSA_P2P_CLIENT;

	return 0;
}


int app_nsa_p2p_continue_stopClient(void)
{
	tNSA_P2P_DEREG		nsa_p2p_dereg_req;
	tNSA_STATUS         nsa_status;
	tNSA_DM_DISABLEPOLL nsa_disable_poll_req;

	NSA_DmDisablePollingInit(&nsa_disable_poll_req);
	nsa_status = NSA_DmDisablePolling(&nsa_disable_poll_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmDisablePolling failed:%d", nsa_status);
		return -1;
	}

	/*stop P2P client*/
	NSA_P2pDeregisterInit(&nsa_p2p_dereg_req);

	nsa_p2p_dereg_req.handle = app_nsa_cb.p2p_client.client_handle;

	nsa_status = NSA_P2pDeregister(&nsa_p2p_dereg_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pDeregister failed:%d", nsa_status);
		return -1;
	}

	return 0;

}

int app_nsa_p2p_connect(int connectBy)
{
	tNSA_P2P_CONN_BY_NAME	nsa_p2p_conn_by_name_req;
	tNSA_P2P_CONN_BY_SAP	nsa_p2p_conn_by_sap_req;
	tNSA_STATUS             nsa_status;

	/*check first if LLCP has been detected as active*/
	if (app_nsa_cb.p2p_llcp_is_active != TRUE)
	{
		APP_ERROR0("app_nsa_p2p_connect failed since LLCP is not active");
	}

	//connectBy = app_get_choice("Choose 'connect by name' [1] or 'by SAP' [2]");

	if (connectBy == 1)
	{/*connect by name*/
		NSA_P2pConnectByNameInit(&nsa_p2p_conn_by_name_req);

		/*hard code service name */
		memcpy(nsa_p2p_conn_by_name_req.service_name, p_p2p_client_urn, strlen ((char *) p_p2p_client_urn));
		nsa_p2p_conn_by_name_req.client_handle = app_nsa_cb.p2p_client.client_handle;
		nsa_p2p_conn_by_name_req.miu = P2P_CLIENT_MIU;
		nsa_p2p_conn_by_name_req.rw = P2P_CLIENT_RW;

		nsa_status = NSA_P2pConnectByName(&nsa_p2p_conn_by_name_req);
		if (nsa_status != NSA_SUCCESS)
		{
			APP_ERROR1("NSA_P2pConnectByName failed:%d", nsa_status);
			return -1;
		}
	}
	else
	{/*connect by SAP*/
		NSA_P2pConnectBySapInit(&nsa_p2p_conn_by_sap_req);

		//nsa_p2p_conn_by_sap_req.dsap = app_get_choice("Enter SAP to connect: ");
		nsa_p2p_conn_by_sap_req.dsap = app_nsa_cb.client_test.remote_sap;
		nsa_p2p_conn_by_sap_req.client_handle = app_nsa_cb.p2p_client.client_handle;
		nsa_p2p_conn_by_sap_req.miu = P2P_CLIENT_MIU;
		nsa_p2p_conn_by_sap_req.rw = P2P_CLIENT_RW;

		nsa_status = NSA_P2pConnectBySap(&nsa_p2p_conn_by_sap_req);
		if (nsa_status != NSA_SUCCESS)
		{
			APP_ERROR1("NSA_P2pConnectBySap failed:%d", nsa_status);
			return -1;
		}
	}

	return 0;
}

int app_nsa_p2p_config_client_test(void)
{
	if (app_nsa_cb.client_link_type == NFA_P2P_DLINK_TYPE)
	{
		/*if only DATA link client has been started ask if want to test connect by name or sap */
		app_nsa_cb.client_test.auto_conn = app_get_choice("On LLCP activate do:  \
									\n   1 - Connection-oriented (by name)  \
								    \n   2 - Connection-oriented (by SAP)");

		/*ask if disconnect only in case of DATA link*/
		app_nsa_cb.client_test.disconnect_after_sending = app_get_choice("Disconnect after sending data? [0 for no - 1 for yes]");
	}
	else if (app_nsa_cb.client_link_type == NFA_P2P_LLINK_TYPE)
	{
		/*if only LOGICAL link client has been started do not ask nothing */
		app_nsa_cb.client_test.auto_conn = APP_NSA_NO_AUTO_CONNECTION;
		/*no need to disconnect*/
		app_nsa_cb.client_test.disconnect_after_sending = FALSE;
	}
	else if (app_nsa_cb.client_link_type == (NFA_P2P_DLINK_TYPE | NFA_P2P_LLINK_TYPE))
	{
		/*if both DATA and LOGICAL  link client have been started ask if want to test connect by name or sap or use connection less*/
		app_nsa_cb.client_test.auto_conn = app_get_choice("On LLCP activate do:  \
															\n   0 - Connection-less \
															\n   1 - Connection-oriented (by name)  \
														    \n   2 - Connection-oriented (by SAP)");

		/*ask if disconnect only in case of DATA link*/
		app_nsa_cb.client_test.disconnect_after_sending = app_get_choice("Disconnect after sending data over DLINK? [0 for no - 1 for yes]");
	}

	app_nsa_cb.client_test.send_data_times =  app_get_choice("Number of packet to send [max 10]: ");
	app_nsa_cb.client_test.send_data_size = app_get_choice("Size of packet to send [max 200]: ");

	return 0;
}

int app_nsa_p2p_config_server_test(void)
{
	if (app_nsa_cb.server_link_type & NFA_P2P_DLINK_TYPE)
	{
		app_nsa_cb.server_test.accept_connection = app_get_choice("Accept connection? [0 for no - 1 for yes] ");
	}

	app_nsa_cb.server_test.loopback_data = app_get_choice("Loopback to client the received data? [0 for no - 1 for yes] ");

	return 0;
}

int app_nsa_p2p_send_data(tNFA_HANDLE conn_handle, UINT8* p_data, int size)
{
	tNSA_P2P_SEND_DATA          nsa_send_data;
	tNSA_STATUS                 nsa_status;

	NSA_P2pSendDataInit(&nsa_send_data);

	if (conn_handle != app_nsa_cb.conn_handle)
	{
		APP_ERROR1("unexpected connection handle:%d", conn_handle);
	}
	nsa_send_data.conn_handle = conn_handle;
	nsa_send_data.length = size;
	memcpy(nsa_send_data.data, p_data, nsa_send_data.length);

	nsa_status = NSA_P2pSendData(&nsa_send_data);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pSendData failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_p2p_send_ui(tNFA_HANDLE handle, UINT16 dsap, UINT8* p_data, int size)
{
	tNSA_P2P_SEND_UI            nsa_send_ui;
	tNSA_STATUS                 nsa_status;

	NSA_P2pSendUIInit(&nsa_send_ui);

	if ((handle != app_nsa_cb.p2p_client.client_handle) &&
			(handle != app_nsa_cb.p2p_server.server_handle))
	{
		APP_ERROR1("unexpected  handle:0x%x", handle);
	}

	nsa_send_ui.handle = handle;
	nsa_send_ui.length = size;
	nsa_send_ui.dsap = dsap;
	memcpy(nsa_send_ui.data, p_data, nsa_send_ui.length);

	nsa_status = NSA_P2pSendUI(&nsa_send_ui);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pSendUI failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_p2p_read_data(tNFA_HANDLE conn_handle)
{
	tNSA_P2P_READ_DATA          nsa_read_data;
	tNSA_STATUS                 nsa_status;

	NSA_P2pReadDataInit(&nsa_read_data);

	if (conn_handle != app_nsa_cb.conn_handle)
	{
		APP_ERROR1("unexpected connection handle:%d", conn_handle);
	}

	nsa_read_data.handle = conn_handle;
	nsa_read_data.max_data_len = MAX_P2P_DATA_LENGTH;
	nsa_read_data.p_data_len = &(app_nsa_cb.p2p_read_data.data_len);
	nsa_read_data.p_data = app_nsa_cb.p2p_read_data.data;
	nsa_read_data.p_more = &(app_nsa_cb.p2p_read_data.more_data);

	nsa_status = NSA_P2pReadData(&nsa_read_data);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pReadData failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_p2p_read_ui(tNFA_HANDLE handle)
{
	tNSA_P2P_READ_UI          nsa_read_ui;
	tNSA_STATUS               nsa_status;

	NSA_P2pReadUIInit(&nsa_read_ui);

	if ((handle != app_nsa_cb.p2p_client.client_handle) &&
				(handle != app_nsa_cb.p2p_server.server_handle))
	{
		APP_ERROR1("unexpected  handle:%d", handle);
	}

	nsa_read_ui.handle = handle;
	nsa_read_ui.max_data_len = MAX_P2P_DATA_LENGTH;
	nsa_read_ui.p_data_len = &(app_nsa_cb.p2p_read_data.data_len);
	nsa_read_ui.p_data = app_nsa_cb.p2p_read_data.data;
	nsa_read_ui.p_more = &(app_nsa_cb.p2p_read_data.more_data);
	nsa_read_ui.p_remote_sap = &(app_nsa_cb.p2p_read_data.remote_sap);

	nsa_status = NSA_P2pReadUI(&nsa_read_ui);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pReadUI failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_p2p_disconnect(tNFA_HANDLE conn_handle)
{
	tNSA_P2P_DISCONNECT          nsa_disc;
	tNSA_STATUS                 nsa_status;

	NSA_P2pDisconnectInit(&nsa_disc);

	nsa_disc.conn_handle = conn_handle;
	nsa_disc.flush = TRUE;

	nsa_status = NSA_P2pDisconnect(&nsa_disc);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pDisconnect failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_p2p_getLinkInfo(tNFA_HANDLE conn_handle)
{
	tNSA_P2P_GET_LINK_INFO      nsa_getLinkInfo;
	tNSA_STATUS                 nsa_status;

	NSA_P2pGetLinkInfoInit(&nsa_getLinkInfo);

	nsa_getLinkInfo.handle = conn_handle;

	nsa_status = NSA_P2pGetLinkInfo(&nsa_getLinkInfo);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_P2pGetLinkInfo failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_p2p_getRemoteSap(tNFA_HANDLE conn_handle)
{
	tNSA_P2P_GET_REMOTE_SAP     nsa_get_sap;
	tNSA_STATUS                 nsa_status;

	NSA_P2pGetRemoteSapInit(&nsa_get_sap);

	nsa_get_sap.handle = conn_handle;
	memcpy(nsa_get_sap.service_name, p_p2p_client_urn, strlen ((char *) p_p2p_client_urn));

	nsa_status = NSA_P2pGetRemoteSap(&nsa_get_sap);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_GetRemoteSap failed:%d", nsa_status);
		return -1;
	}

	return 0;
}


int app_nsa_tags_start_t4t(void)
{
	tNSA_DM_REQ_EXCL_RF_CTRL    nsa_req_excl_ctrl;
	tNSA_STATUS                 nsa_status;

	app_nsa_cb.rawData_done = FALSE;

	NSA_DmRequestExclusiveRfControlInit(&nsa_req_excl_ctrl);

	/*set nfc-A and nfc-B*/
	nsa_req_excl_ctrl.poll_mask = NFA_TECHNOLOGY_MASK_A | NFA_TECHNOLOGY_MASK_B | NFA_TECHNOLOGY_MASK_B_PRIME | NFA_TECHNOLOGY_MASK_A_ACTIVE;
	nsa_req_excl_ctrl.p_conn_cback = app_nsa_t4t_conn_cback;
	nsa_req_excl_ctrl.p_ndef_cback = app_nsa_t4t_ndef_cback;
	/*TODO: try to set more specific listen config*/
	nsa_req_excl_ctrl.p_listen_cfg = NULL;

	nsa_status = NSA_DmRequestExclusiveRfControl(&nsa_req_excl_ctrl);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmRequestExclusiveRfControl failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_tags_stop_t4t(void)
{
	tNSA_DM_REL_EXCL_RF_CTRL    nsa_rel_excl_ctrl;
	tNSA_STATUS                 nsa_status;

	NSA_DmReleaseExclusiveRfControlInit(&nsa_rel_excl_ctrl);

	nsa_status = NSA_DmReleaseExclusiveRfControl(&nsa_rel_excl_ctrl);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmReleaseExclusiveRfControl failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

/*NSA SNEP command*/
int app_nsa_snep_start_server(void)
{
	tNSA_DM_REG_NDEF_TYPE_HDLR    nsa_ndef_reg_req;
	tNSA_DM_SET_P2P_TECH        nsa_set_p2p_tech_req;
    tNSA_SNEP_START_DFLT_SRV    nsa_snep_start_dflt_srvr_req;
    tNSA_DM_START_RF_DISCOV     nsa_start_rf_req;
    tNSA_STATUS                 nsa_status;
    tNSA_SNEP_REG_SERVER        nsa_snep_server;
    int 						choice;

    /*reset handle for server - it will be set only for not default SNEP server*/
    app_nsa_cb.snep_server_handle = NFA_HANDLE_INVALID;

    choice = app_get_choice("Start Default Server? [0 for No and 1 for Yes]: ");
    if (choice)
    {
    	/*first register an default NDEF default handler*/
    	NSA_DmRegNdefTypeHdlrInit(&nsa_ndef_reg_req);

    	nsa_ndef_reg_req.handle_whole_msg = TRUE;
    	nsa_ndef_reg_req.tnf = NSA_TNF_DEFAULT;
    	nsa_ndef_reg_req.p_ndef_cback = app_nsa_default_ndef_hdlr;
    	//    nsa_ndef_reg_req.type_name = "";
    	memcpy(nsa_ndef_reg_req.type_name , "", 0);
    	nsa_ndef_reg_req.type_name_len = 0;

    	nsa_status = NSA_DmRegNdefTypeHdlr(&nsa_ndef_reg_req);
    	if (nsa_status != NSA_SUCCESS)
    	{
    		APP_ERROR1("NSA_DmRegNdefTypeHdlr failed to register default type handler:%d", nsa_status);
    		return -1;
    	}

    	/*start default SNEP server*/
    	NSA_SnepStartDefaultServerInit(&nsa_snep_start_dflt_srvr_req);

    	nsa_status = NSA_SnepStartDefaultServer(&nsa_snep_start_dflt_srvr_req);
    	if (nsa_status != NSA_SUCCESS)
    	{
    		APP_ERROR1("NSA_SnepStartDefaultServer failed:%d", nsa_status);
    		return -1;
    	}
    }
    else
    {
    	APP_INFO0("Enter the SNEP server service name");
    	APP_INFO0("enter nothing to use brcm_test:urn:nfc:sn:snep.");

    	app_nsa_cb.snep_service_name_lenght = app_get_string(" :  ",
    			app_nsa_cb.snep_service_name, LLCP_MAX_SN_LEN);

    	if (app_nsa_cb.snep_service_name_lenght == 0)
    	{
    		app_nsa_cb.snep_service_name_lenght = strlen ((char *) p_snep_default_service_urn);
    		memcpy(app_nsa_cb.snep_service_name, p_snep_default_service_urn, app_nsa_cb.snep_service_name_lenght);
    	}


    	NSA_SnepRegisterServerInit(&nsa_snep_server);

    	nsa_snep_server.server_sap = NSA_SNEP_ANY_SAP;
    	memcpy(nsa_snep_server.service_name, app_nsa_cb.snep_service_name, app_nsa_cb.snep_service_name_lenght);

    	nsa_status = NSA_SnepRegisterServer(&nsa_snep_server);
    	if (nsa_status != NSA_SUCCESS)
    	{
    		APP_ERROR1("NSA_SnepRegisterServer failed:%d", nsa_status);
    		return -1;
    	}
    }

    /*set P2P tech listen mask for Snep*/
    NSA_DmSetP2pListenTechInit(&nsa_set_p2p_tech_req);

    nsa_set_p2p_tech_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;

    nsa_status = NSA_DmSetP2pListenTech(&nsa_set_p2p_tech_req);
    if (nsa_status != NSA_SUCCESS)
    {
    	APP_ERROR1("NSA_DmSetP2pListenTech failed:%d", nsa_status);
    	return -1;
    }

    /*start RF discovery*/
    NSA_DmStartRfDiscovInit(&nsa_start_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStartRfDiscov(&nsa_start_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStartRfDiscov failed:%d", nsa_status);
        return -1;
    }

    return 0;
}

int app_nsa_snep_stop_server(void)
{
    tNSA_DM_STOP_RF_DISCOV          nsa_stop_rf_req;
    tNSA_STATUS                     nsa_status;

    /*first stop RF discovery*/
    NSA_DmStopRfDiscovInit(&nsa_stop_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStopRfDiscov(&nsa_stop_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStopRfDiscov failed:%d", nsa_status);
        return -1;
    }

    /*it will be continued once RF discovery stopped event is received*/
    app_nsa_cb.pending_stop_action = APP_NSA_SNEP_SERVER;

    return 0;
}

int app_nsa_dm_continue_stop_snep_server(void)
{
	tNSA_DM_DEREG_NDEF_TYPE_HDLR    nsa_ndef_dereg_req;
    tNSA_DM_SET_P2P_TECH            nsa_set_p2p_tech_req;
    tNSA_SNEP_STOP_DFLT_SRV         nsa_snep_stop_dflt_srvr_req;
    tNSA_STATUS                     nsa_status;
    tNSA_SNEP_DEREG                 nsa_snep_dereg;

    /*disable P2P tech mask*/
    NSA_DmSetP2pListenTechInit(&nsa_set_p2p_tech_req);

    nsa_set_p2p_tech_req.poll_mask = 0x00;
    nsa_status = NSA_DmSetP2pListenTech(&nsa_set_p2p_tech_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmSetP2pListenTech failed:%d", nsa_status);
        return -1;
    }

    if (app_nsa_cb.snep_server_handle == NFA_HANDLE_INVALID)
    {
    	/*stop default SNEP server*/
    	NSA_SnepStopDefaultServerInit(&nsa_snep_stop_dflt_srvr_req);

    	nsa_status = NSA_SnepStopDefaultServer(&nsa_snep_stop_dflt_srvr_req);
    	if (nsa_status != NSA_SUCCESS)
    	{
    		APP_ERROR1("NSA_SnepStopDefaultServerInit failed:%d", nsa_status);
    		return -1;
    	}

    	/*last deregister default NDEF handler*/
    	NSA_DmDeregNdefTypeHdlrInit(&nsa_ndef_dereg_req);
    	nsa_ndef_dereg_req.ndef_type_handle = app_nsa_cb.ndef_default_handle;
    	nsa_status = NSA_DmDeregNdefTypeHdlr(&nsa_ndef_dereg_req);
    	if (nsa_status != NSA_SUCCESS)
    	{
    		APP_ERROR1("NSA_DmDeregNdefTypeHdlr failed to deregister default type handler:%d", nsa_status);
    		return -1;
    	}
    }
    else
    {
    	NSA_SnepDeregisterInit(&nsa_snep_dereg);

    	nsa_snep_dereg.reg_handle = app_nsa_cb.snep_server_handle;

    	nsa_status = NSA_SnepDeregister(&nsa_snep_dereg);
    	if (nsa_status != NSA_SUCCESS)
    	{
    		APP_ERROR1("NSA_SnepDeregister failed:%d", nsa_status);
    		return -1;
    	}
    }

    return 0;
}

int app_nsa_snep_start_client(void)
{
	tNSA_DM_ENABLEPOLL      	nsa_enable_poll_req;
    tNSA_DM_START_RF_DISCOV     nsa_start_rf_req;
    tNSA_STATUS                 nsa_status;
    tNSA_SNEP_REG_CLIENT        nsa_snep_server;

    /*reset handle for server - it will be set only for not default SNEP server*/
    app_nsa_cb.snep_client_handle = NFA_HANDLE_INVALID;

    NSA_SnepRegisterClientInit(&nsa_snep_server);

   	nsa_status = NSA_SnepRegisterClient(&nsa_snep_server);
   	if (nsa_status != NSA_SUCCESS)
   	{
   		APP_ERROR1("NSA_SnepRegisterClient failed:%d", nsa_status);
   		return -1;
   	}

    /*set polling mask*/
    NSA_DmEnablePollingInit(&nsa_enable_poll_req);
    nsa_enable_poll_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;

    nsa_status = NSA_DmEnablePolling(&nsa_enable_poll_req);
    if (nsa_status != NSA_SUCCESS)
    {
    	APP_ERROR1("NSA_DmEnablePolling failed:%d", nsa_status);
    	return -1;
    }

    /*start RF discovery*/
    NSA_DmStartRfDiscovInit(&nsa_start_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStartRfDiscov(&nsa_start_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStartRfDiscov failed:%d", nsa_status);
        return -1;
    }

    return 0;
}

int app_nsa_snep_config_test(void)
{
	int choice;

	APP_INFO0("Enter the service name to connect to");
	APP_INFO0("enter nothing to use brcm_test:urn:nfc:sn:snep.");

	app_nsa_cb.snep_service_name_lenght = app_get_string(" :  ",
			app_nsa_cb.snep_service_name, LLCP_MAX_SN_LEN);

	if (app_nsa_cb.snep_service_name_lenght == 0)
	{
		app_nsa_cb.snep_service_name_lenght = strlen ((char *) p_snep_default_service_urn);
		memcpy(app_nsa_cb.snep_service_name, p_snep_default_service_urn, app_nsa_cb.snep_service_name_lenght);
	}

	choice = app_get_choice("PUT [0] or GET [1]?: ");

	if (choice)
	{
		APP_INFO0("Choose the NDEF message to pass in the GET request:");
		APP_INFO0(" 0. Text");
		APP_INFO0(" 1. Bluetooth Out of Band");
		choice = app_get_choice(": ");
		if (choice)
		{
			app_nsa_cb.snep_test.tx_ndef_data = bt_oob;
			app_nsa_cb.snep_test.tx_ndef_length = BT_OOB_NDEF_LENGTH;
		}
		else
		{
			app_nsa_cb.snep_test.tx_ndef_data = text_example;
			app_nsa_cb.snep_test.tx_ndef_length = TEXT_NDEF_LENGTH;
		}

		app_nsa_cb.snep_test.req = NSA_SNEP_GET_REQ;
	}
	else
	{
		APP_INFO0("Choose the NDEF message to pass in the PUT request:");
		APP_INFO0(" 0. Text");
		APP_INFO0(" 1. Smart Poster");
		APP_INFO0(" 2. WPS Password for Wifi");
		APP_INFO0(" 3. Sbeam (MAC for Wifi Direct)");
		choice = app_get_choice(": ");
		switch (choice)
		{
		case 0:
			app_nsa_cb.snep_test.tx_ndef_data = text_example;
			app_nsa_cb.snep_test.tx_ndef_length = TEXT_NDEF_LENGTH;
		break;
		case 1:
			app_nsa_cb.snep_test.tx_ndef_data = smart_poster;
			app_nsa_cb.snep_test.tx_ndef_length = SP_NDEF_LENGTH;
		break;
		case 2:
			app_nsa_cb.snep_test.tx_ndef_data = wifi_wps;
			app_nsa_cb.snep_test.tx_ndef_length = wifi_wps_len;
		break;
		case 3:
			app_nsa_cb.snep_test.tx_ndef_data = sbeam_image;
			app_nsa_cb.snep_test.tx_ndef_length = SBEAM_IMAGE_NDEF_LENGTH;
		break;
		default:
			APP_ERROR0("Wrong choice");
			return -1;
		}

		app_nsa_cb.snep_test.req = NSA_SNEP_PUT_REQ;
	}

	return 0;
}

int app_nsa_snep_stop_client(void)
{
    tNSA_DM_STOP_RF_DISCOV          nsa_stop_rf_req;
    tNSA_STATUS                     nsa_status;

    /*first stop RF discovery*/
    NSA_DmStopRfDiscovInit(&nsa_stop_rf_req);
    /*no param to be passed*/
    nsa_status = NSA_DmStopRfDiscov(&nsa_stop_rf_req);
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_DmStopRfDiscov failed:%d", nsa_status);
        return -1;
    }

    /*it will be continued once RF discovery stopped event is received*/
    app_nsa_cb.pending_stop_action = APP_NSA_SNEP_CLIENT;

    return 0;
}

int app_nsa_dm_continue_stop_snep_client(void)
{
    tNSA_STATUS                     nsa_status;
    tNSA_SNEP_DEREG                 nsa_snep_dereg;
    tNSA_DM_DISABLEPOLL nsa_disable_poll_req;

    NSA_DmDisablePollingInit(&nsa_disable_poll_req);
    nsa_status = NSA_DmDisablePolling(&nsa_disable_poll_req);
    if (nsa_status != NSA_SUCCESS)
    {
    	APP_ERROR1("NSA_DmDisablePolling failed:%d", nsa_status);
    	return -1;
    }

   	NSA_SnepDeregisterInit(&nsa_snep_dereg);

   	nsa_snep_dereg.reg_handle = app_nsa_cb.snep_client_handle;

   	nsa_status = NSA_SnepDeregister(&nsa_snep_dereg);
   	if (nsa_status != NSA_SUCCESS)
   	{
   		APP_ERROR1("NSA_SnepDeregister failed:%d", nsa_status);
   		return -1;
   	}

    return 0;
}

/*NSA RW command*/
int app_nsa_rw_write_text(void)
{
	tNSA_RW_WRITE            nsa_rw_write;
	tNSA_STATUS              nsa_status;

	NSA_RwWriteInit(&nsa_rw_write);

	nsa_rw_write.for_ever = app_get_choice("Continue write [1] or just one tag [0]?: ");
	nsa_rw_write.ndef_length = TEXT_NDEF_LENGTH;
	memcpy(nsa_rw_write.ndef_data, text_example, nsa_rw_write.ndef_length);
	nsa_rw_write.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwWrite(&nsa_rw_write);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_RwWrite failed:%d", nsa_status);
		return -1;
	}
	return 0;
}

int app_nsa_rw_write_sp(void)
{
	tNSA_RW_WRITE            nsa_rw_write;
	tNSA_STATUS              nsa_status;

	NSA_RwWriteInit(&nsa_rw_write);

	nsa_rw_write.for_ever = app_get_choice("Continue write [1] or just one tag [0]?: ");
	nsa_rw_write.ndef_length = SP_NDEF_LENGTH;
	memcpy(nsa_rw_write.ndef_data, smart_poster, nsa_rw_write.ndef_length);
	nsa_rw_write.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwWrite(&nsa_rw_write);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_RwWrite failed:%d", nsa_status);
		return -1;
	}
	return 0;
}

int app_nsa_rw_write_bt(void)
{
	tNSA_RW_WRITE            nsa_rw_write;
	tNSA_STATUS              nsa_status;

	NSA_RwWriteInit(&nsa_rw_write);

	nsa_rw_write.for_ever = app_get_choice("Continue write [1] or just one tag [0]?: ");
	nsa_rw_write.ndef_length = BT_OOB_NDEF_LENGTH;
	memcpy(nsa_rw_write.ndef_data, bt_oob, nsa_rw_write.ndef_length);
	nsa_rw_write.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwWrite(&nsa_rw_write);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_RwWrite failed:%d", nsa_status);
		return -1;
	}
	return 0;
}

int app_nsa_rw_write_wps(void)
{
	tNSA_RW_WRITE            nsa_rw_write;
	tNSA_STATUS              nsa_status;

	NSA_RwWriteInit(&nsa_rw_write);

	nsa_rw_write.for_ever = app_get_choice("Continue write [1] or just one tag [0]?: ");
	nsa_rw_write.ndef_length = wifi_wps_len;
	memcpy(nsa_rw_write.ndef_data, wifi_wps, nsa_rw_write.ndef_length);
	nsa_rw_write.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwWrite(&nsa_rw_write);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_RwWrite failed:%d", nsa_status);
		return -1;
	}
	return 0;
}

int app_nsa_rw_read(void)
{
	tNSA_RW_READ	nsa_rw_read;
	tNSA_STATUS     nsa_status;
	tNSA_DM_REG_NDEF_TYPE_HDLR    nsa_ndef_reg_req;
	int				for_ever;

	/*ask before to avoid receive event for default handler after the string is displayed*/
	 for_ever = app_get_choice("Continue Read [1] or just one tag [0]?: ");

	/*first register a default NDEF handler*/
	NSA_DmRegNdefTypeHdlrInit(&nsa_ndef_reg_req);

	nsa_ndef_reg_req.handle_whole_msg = TRUE;
	nsa_ndef_reg_req.tnf = NSA_TNF_DEFAULT;
	nsa_ndef_reg_req.p_ndef_cback = app_nsa_default_ndef_hdlr;
	memcpy(nsa_ndef_reg_req.type_name , "", 0);
	nsa_ndef_reg_req.type_name_len = 0;

	nsa_status = NSA_DmRegNdefTypeHdlr(&nsa_ndef_reg_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmRegNdefTypeHdlr failed:%d", nsa_status);
		return -1;
	}

	NSA_RwReadInit(&nsa_rw_read);

	nsa_rw_read.for_ever = for_ever;
	nsa_rw_read.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwRead(&nsa_rw_read);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_RwRead failed:%d", nsa_status);
		return -1;
	}
	return 0;
}

int app_nsa_rw_format(void)
{
	tNSA_RW_FORMAT	nsa_rw_format;
	tNSA_STATUS     nsa_status;

	NSA_RwFormatInit(&nsa_rw_format);

	nsa_rw_format.for_ever = app_get_choice("Continue format [1] or just one tag [0]?: ");
	nsa_rw_format.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwFormat(&nsa_rw_format);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_RwFormat failed:%d", nsa_status);
		return -1;
	}
	return 0;
}

int app_nsa_rw_set_readOnly(void)
{
	tNSA_RW_SET_RO	nsa_rw_set_readOnly;
	tNSA_STATUS     nsa_status;

	NSA_RwSetReadOnlyInit(&nsa_rw_set_readOnly);

	nsa_rw_set_readOnly.for_ever = app_get_choice("Continue set Read-Only [1] or just one tag [0]?: ");
	nsa_rw_set_readOnly.b_hard_lock = app_get_choice("enable Hard-LOCK? [0,1]: ");
	nsa_rw_set_readOnly.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwSetReadOnly(&nsa_rw_set_readOnly);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_RwSetReadOnly failed:%d", nsa_status);
		return -1;
	}
	return 0;
}

int app_nsa_rw_stop(void)
{
	tNSA_RW_STOP	nsa_rw_stop;
	tNSA_STATUS     nsa_status;

	NSA_RwStopInit(&nsa_rw_stop);

	nsa_status = NSA_RwStop(&nsa_rw_stop);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_RwStop failed:%d", nsa_status);
		return -1;
	}
	return 0;
}

/*NSA CHO command*/
int app_nsa_cho_start(void)
{
	tNSA_CHO_START			nsa_cho_start;
	tNSA_DM_SET_P2P_TECH    nsa_set_p2p_tech_req;
	tNSA_DM_ENABLEPOLL      nsa_enable_poll_req;
	tNSA_DM_START_RF_DISCOV nsa_start_rf_req;
	tNSA_STATUS				nsa_status;

	NSA_ChoStartInit(&nsa_cho_start);

	app_nsa_cb.cho_test.cho_server_started = app_get_choice("Start CHO server to receive Handover request? [0 for NO, 1 for YES]: ");

	nsa_cho_start.reg_server = app_nsa_cb.cho_test.cho_server_started;
	if (nsa_cho_start.reg_server)
	{
		app_nsa_cb.cho_test.bt_availalble = app_get_choice("  Is BT available? [0 for NO, 1 for YES]: ");
		app_nsa_cb.cho_test.wifi_availalble = app_get_choice("  Is WIFI available? [0 for NO, 1 for YES]: ");
		if (app_nsa_cb.cho_test.bt_availalble && app_nsa_cb.cho_test.wifi_availalble )
			app_nsa_cb.cho_test.preference = app_get_choice("  preference for alternative carrier? [0-none, 1-BT, 2-WiFi]: ");
	}


	nsa_cho_start.p_callback = app_nsa_cho_cback;

	nsa_status = NSA_ChoStart(&nsa_cho_start);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_ChoStart failed:%d", nsa_status);
		return -1;
	}

	if (nsa_cho_start.reg_server)
	{
		 /*set P2P tech listen mask for CHO, needed only if server is started to receive CHO req*/
		NSA_DmSetP2pListenTechInit(&nsa_set_p2p_tech_req);

		nsa_set_p2p_tech_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;

		nsa_status = NSA_DmSetP2pListenTech(&nsa_set_p2p_tech_req);
		if (nsa_status != NSA_SUCCESS)
		{
			APP_ERROR1("NSA_DmSetP2pListenTech failed:%d", nsa_status);
			return -1;
		}
	}

	 /*set polling mask*/
	NSA_DmEnablePollingInit(&nsa_enable_poll_req);
	nsa_enable_poll_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;

	nsa_status = NSA_DmEnablePolling(&nsa_enable_poll_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmEnablePolling failed:%d", nsa_status);
		return -1;
	}

	/*start RF discovery*/
	NSA_DmStartRfDiscovInit(&nsa_start_rf_req);
	/*no param to be passed*/
	nsa_status = NSA_DmStartRfDiscov(&nsa_start_rf_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmStartRfDiscov failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_cho_stop(void)
{
	tNSA_DM_STOP_RF_DISCOV          nsa_stop_rf_req;
	tNSA_STATUS                     nsa_status;

	/*first stop RF discovery*/
	NSA_DmStopRfDiscovInit(&nsa_stop_rf_req);
	/*no param to be passed*/
	nsa_status = NSA_DmStopRfDiscov(&nsa_stop_rf_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmStopRfDiscov failed:%d", nsa_status);
		return -1;
	}

	/*it will be continued once RF discovery stopped event is received*/
	app_nsa_cb.pending_stop_action = APP_NSA_CHO;

	return 0;
}

int app_nsa_dm_continue_stop_cho(void)
{
	tNSA_DM_SET_P2P_TECH    nsa_set_p2p_tech_req;
	tNSA_CHO_STOP			nsa_cho_stop;
	tNSA_STATUS				nsa_status;
	tNSA_DM_DISABLEPOLL nsa_disable_poll_req;

	NSA_DmDisablePollingInit(&nsa_disable_poll_req);
	nsa_status = NSA_DmDisablePolling(&nsa_disable_poll_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_DmDisablePolling failed:%d", nsa_status);
		return -1;
	}

	if (app_nsa_cb.cho_test.cho_server_started)
	{
		/*disable P2P tech mask; neede only if started */
		NSA_DmSetP2pListenTechInit(&nsa_set_p2p_tech_req);

		nsa_set_p2p_tech_req.poll_mask = 0x00;
		nsa_status = NSA_DmSetP2pListenTech(&nsa_set_p2p_tech_req);
		if (nsa_status != NSA_SUCCESS)
		{
			APP_ERROR1("NSA_DmSetP2pListenTech failed:%d", nsa_status);
			return -1;
		}
	}

	NSA_ChoStopInit(&nsa_cho_stop);

	nsa_status = NSA_ChoStop(&nsa_cho_stop);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_ChoStop failed:%d", nsa_status);
		return -1;
	}

	app_nsa_cb.cho_test.bt_availalble = FALSE;
	app_nsa_cb.cho_test.wifi_availalble = FALSE;
	app_nsa_cb.cho_test.preference = NSA_CHO_NONE;
	app_nsa_cb.cho_test.cho_server_started = FALSE;

	return 0;
}


int app_nsa_cho_send(void)
{
	tNSA_CHO_SEND_REQ		nsa_cho_send_req;
	tNSA_STATUS				nsa_status;
	int						choice;
	tNDEF_STATUS			ndef_status;
	UINT8            		*p_ndef_header;

	NSA_ChoSendRequestInit(&nsa_cho_send_req);

	NDEF_MsgInit (nsa_cho_send_req.ndef_data, MAX_NDEF_LENGTH, &(nsa_cho_send_req.ndef_len));



	choice = app_get_choice("Request BT as alt carrier? [0 for NO, 1 for YES]: ");
	if (choice)
	{
		/*add hard-coded BT_OOB and ac info value*/
		/* restore NDEF header which might be updated during previous appending record */
		p_ndef_header = (UINT8*)bt_oob_rec;
		*p_ndef_header |= (NDEF_ME_MASK|NDEF_MB_MASK);
		ndef_status = NDEF_MsgAppendRec (nsa_cho_send_req.ndef_data, MAX_NDEF_LENGTH, &(nsa_cho_send_req.ndef_len),
				bt_oob_rec, BT_OOB_REC_LEN);
		nsa_cho_send_req.ac_info[nsa_cho_send_req.num_ac_info].cps          = NFA_CHO_CPS_ACTIVE;
		nsa_cho_send_req.ac_info[nsa_cho_send_req.num_ac_info].num_aux_data = 0;
		nsa_cho_send_req.num_ac_info++;
	}

	choice = app_get_choice("Request WIFI as alt carrier? [0 for NO, 1 for YES]: ");
	if (choice)
	{
		/*add hard-coded WiFi req with Hc as TNF and ac info value*/
		/* restore NDEF header which might be updated during previous appending record */
		p_ndef_header = (UINT8*)wifi_hr_rec;
		*p_ndef_header |= (NDEF_ME_MASK|NDEF_MB_MASK);
		ndef_status = NDEF_MsgAppendRec (nsa_cho_send_req.ndef_data, MAX_NDEF_LENGTH, &(nsa_cho_send_req.ndef_len),
				wifi_hr_rec, WIFI_HR_REC_LEN);
		nsa_cho_send_req.ac_info[nsa_cho_send_req.num_ac_info].cps          = NFA_CHO_CPS_ACTIVE;
		nsa_cho_send_req.ac_info[nsa_cho_send_req.num_ac_info].num_aux_data = 0;
		nsa_cho_send_req.num_ac_info++;
	}
	
	nsa_status = NSA_ChoSendRequest(&nsa_cho_send_req);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_ChoSendRequest failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_cho_cancel(void)
{
	tNSA_CHO_CANCEL_REQ		nsa_cho_cancel;
	tNSA_STATUS				nsa_status;

	NSA_ChoCancelInit(&nsa_cho_cancel);

	nsa_status = NSA_ChoCancel(&nsa_cho_cancel);
	if (nsa_status != NSA_SUCCESS)
	{
		APP_ERROR1("NSA_ChoCancel failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

/* Kenlo + */
void app_nsa_dm_cback_wps(tNSA_DM_EVT event, tNSA_DM_MSG *p_data)
{
    switch(event)
    {
    case NSA_DM_ENABLED_EVT:             /* Result of NFA_Enable             */
        APP_INFO1("NSA_DM_ENABLED_EVT Status:%d", p_data->status);
	wps_nfc_dm_cback(NSA_DM_ENABLED_EVT, p_data->status);
        break;
    case NSA_DM_DISABLED_EVT:            /* Result of NFA_Disable            */
        APP_INFO0("NSA_DM_DISABLED_EVT");
        break;
    case NSA_DM_SET_CONFIG_EVT:          /* Result of NFA_SetConfig          */
        APP_INFO0("NSA_DM_SET_CONFIG_EVT");
        break;
    case NSA_DM_GET_CONFIG_EVT:          /* Result of NFA_GetConfig          */
        APP_INFO0("NSA_DM_GET_CONFIG_EVT");
        break;
    case NSA_DM_PWR_MODE_CHANGE_EVT:     /* Result of NFA_PowerOffSleepMode  */
        APP_INFO0("NSA_DM_PWR_MODE_CHANGE_EVT");
        break;
    case NSA_DM_RF_FIELD_EVT:            /* Status of RF Field               */
        APP_INFO1("NSA_DM_RF_FIELD_EVT Status:%d RfFieldStatus:%d", p_data->rf_field.status,
                p_data->rf_field.rf_field_status);
        break;
    case NSA_DM_NFCC_TIMEOUT_EVT:        /* NFCC is not responding           */
        APP_INFO0("NSA_DM_ENABLED_EVT");
        break;
    case NSA_DM_NFCC_TRANSPORT_ERR_EVT:  /* NCI Tranport error               */
        APP_INFO0("NSA_DM_ENABLED_EVT");
        break;
    default:
        APP_ERROR1("Bad Event", event);
        break;
    }
}

void
app_nsa_conn_cback_wps(tNSA_CONN_EVT event, tNSA_CONN_MSG *p_data)
{
	tNSA_DM_SELECT	nsa_dm_select;
	tNSA_STATUS     nsa_status;

    switch (event)
    {
    case NSA_POLL_ENABLED_EVT:
        APP_INFO1("NSA_POLL_ENABLED_EVT status:%d", p_data->status);
        break;
    case NSA_POLL_DISABLED_EVT:
        APP_INFO1("NSA_POLL_DISABLED_EVT status:%d", p_data->status);
        break;
    case NSA_RF_DISCOVERY_STARTED_EVT:
        APP_INFO0("NSA_RF_DISCOVERY_STARTED_EVT");
        break;
    case NSA_ACTIVATED_EVT:
        APP_INFO0("NSA_ACTIVATED_EVT");
        break;
    case NSA_DEACTIVATED_EVT:
        APP_INFO0("NSA_DEACTIVATED_EVT");
	wps_nfc_conn_cback(event, p_data->status);
	break;
    case NSA_NDEF_DETECT_EVT:
        APP_INFO1("NSA_NDEF_DETECT_EVT status:%d", p_data->ndef_detect.status);
        break;
    case NSA_RF_DISCOVERY_STOPPED_EVT:
        APP_DEBUG0("RF discovery has been stopped.");

        switch (app_nsa_cb.pending_stop_action)
        {
        case APP_NSA_RF_DISCOVERY:
        	app_nsa_dm_continue_stopRFdiscov();
        	break;
        case APP_NSA_SBEAM:
        	app_nsa_dm_continue_stopSbeam();
        	break;
        case APP_NSA_P2P_SERVER:
        	app_nsa_p2p_continue_stopServer();
        	break;
        case APP_NSA_P2P_CLIENT:
        	app_nsa_p2p_continue_stopClient();
        	break;
        case APP_NSA_SNEP_CLIENT:
        	app_nsa_dm_continue_stop_snep_client();
        	break;
        case APP_NSA_SNEP_SERVER:
        	app_nsa_dm_continue_stop_snep_server();
        	break;
        case APP_NSA_CHO:
        	app_nsa_dm_continue_stop_cho();
        	break;

        default:
        	APP_DEBUG1("nothing to be stopped (%d)", app_nsa_cb.pending_stop_action);
        }
        app_nsa_cb.pending_stop_action = APP_NSA_NO_ACTION;
        break;
    case NSA_DATA_EVT:
    	APP_INFO0("NSA_DATA_EVT");
    	break;
    case NSA_LLCP_ACTIVATED_EVT:
    	APP_INFO0("NSA_LLCP_ACTIVATED_EVT");
    	break;
    case NSA_LLCP_DEACTIVATED_EVT:
    	APP_INFO0("NSA_LLCP_DEACTIVATED_EVT");
    	break;
    case NSA_SET_P2P_LISTEN_TECH_EVT:
    	APP_INFO0("NSA_SET_P2P_LISTEN_TECH_EVT");
    	break;
    case NSA_SELECT_RESULT_EVT:
    	APP_INFO0("NSA_SELECT_RESULT_EVT");
    	if (p_data->status == NFA_STATUS_FAILED)
    	{
    		APP_ERROR0("selection failed!!!");;
    	}
    	break;
    case NSA_READ_CPLT_EVT:
    	APP_INFO1("NSA_READ_CPLT_EVT: %d", p_data->status);
	wps_nfc_conn_cback(event, p_data->status);
    	break;
    case NSA_WRITE_CPLT_EVT:
    	APP_INFO1("NSA_WRITE_CPLT_EVT: %d", p_data->status);
	wps_nfc_conn_cback(event, p_data->status);
        break;
    case NSA_FORMAT_CPLT_EVT:
    	APP_INFO1("NSA_FORMAT_CPLT_EVT: %d", p_data->status);
	wps_nfc_conn_cback(event, p_data->status);
        break;
    case NSA_SET_TAG_RO_EVT:
    	APP_INFO1("NSA_SET_TAG_RO_EVT: %d", p_data->status);
        break;
    case NSA_DISC_RESULT_EVT:
    	APP_INFO0("NSA_DISC_RESULT_EVT");
    	APP_INFO1("rf_disc_id = %d", p_data->disc_result.discovery_ntf.rf_disc_id);
    	APP_INFO1("protocol = %d", p_data->disc_result.discovery_ntf.protocol);
    	APP_INFO1("more = %d", p_data->disc_result.discovery_ntf.more);
    	if (p_data->disc_result.discovery_ntf.more == 0)
    	{/*just choose latest disc result in this demo code*/
    		NSA_DmSelectInit(&nsa_dm_select);

    		nsa_dm_select.protocol = p_data->disc_result.discovery_ntf.protocol;
    		nsa_dm_select.rf_disc_id = p_data->disc_result.discovery_ntf.rf_disc_id;
    		if (nsa_dm_select.protocol > NFC_PROTOCOL_NFC_DEP)
    			nsa_dm_select.rf_interface = NCI_INTERFACE_FRAME;
    		else
    			nsa_dm_select.rf_interface = protocol_to_rf_interface[nsa_dm_select.protocol];

    		/*add dummy delay */
    		GKI_delay(100);

    		nsa_status = NSA_DmSelect(&nsa_dm_select);
    		if (nsa_status != NSA_SUCCESS)
    		{
    			APP_ERROR1("NSA_DmSelect failed:%d", nsa_status);
    		}
    	}
    	break;

    default:
        APP_ERROR1("Unknown event:%d", event);
    }
}
int app_nsa_dm_enable_wps(void)
{
	tNSA_DM_ENABLE nsa_req;
	tNSA_STATUS nsa_status;
	int port;

	NSA_DmEnableInit(&nsa_req);

	nsa_req.dm_cback = app_nsa_dm_cback_wps;
	nsa_req.conn_cback = app_nsa_conn_cback_wps;

	/* Select Port: 0 for local, other for BSA IPC */
	port = 0;

	nsa_req.port = (UINT8)port;

	nsa_status = NSA_DmEnable(&nsa_req);
	if (nsa_status != NSA_SUCCESS) {
		APP_ERROR1("NSA_DmEnable failed:%d", nsa_status);
		return -1;
	}

	return 0;
}

int app_nsa_rw_format_wps(void)
{
	tNSA_RW_FORMAT	nsa_rw_format;
	tNSA_STATUS     nsa_status;

	NSA_RwFormatInit(&nsa_rw_format);

	/* 1: Continue format or 0: just one tag */
	nsa_rw_format.for_ever = 0;
	nsa_rw_format.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwFormat(&nsa_rw_format);
	if (nsa_status != NSA_SUCCESS) {
		APP_ERROR1("NSA_RwFormat failed:%d", nsa_status);
		return nsa_status;
	}

	return 0;
}

static void
app_nsa_default_wps_ndef_hdlr(tNSA_NDEF_EVT event, tNSA_NDEF_EVT_DATA *p_data)
{
	switch (event) {
	case NSA_NDEF_DATA_EVT:
		APP_DUMP("NDEF", p_data->ndef_data.data, p_data->ndef_data.len);
		APP_DEBUG0("--->>> Start to parse NDEF content.");
		wps_nfc_ndef_cback(p_data->ndef_data.data, p_data->ndef_data.len);
		break;

	default:
		APP_ERROR1("Unknown event:%d", event);
	}
}

int app_nsa_rw_read_wps(void)
{
	tNSA_RW_READ	nsa_rw_read;
	tNSA_STATUS     nsa_status;
	tNSA_DM_REG_NDEF_TYPE_HDLR    nsa_ndef_reg_req;
	int		for_ever;

	/* 1: Continue Read or 0: just one tag */
	for_ever = 0;

	/* first register a default NDEF handler */
	NSA_DmRegNdefTypeHdlrInit(&nsa_ndef_reg_req);

	nsa_ndef_reg_req.handle_whole_msg = TRUE;
	nsa_ndef_reg_req.tnf = NSA_TNF_DEFAULT;
	nsa_ndef_reg_req.p_ndef_cback = app_nsa_default_wps_ndef_hdlr;
	memcpy(nsa_ndef_reg_req.type_name , "", 0);
	nsa_ndef_reg_req.type_name_len = 0;

	nsa_status = NSA_DmRegNdefTypeHdlr(&nsa_ndef_reg_req);
	if (nsa_status != NSA_SUCCESS) {
		APP_ERROR1("NSA_DmRegNdefTypeHdlr failed:%d", nsa_status);
		return nsa_status;
	}

	NSA_RwReadInit(&nsa_rw_read);

	nsa_rw_read.for_ever = for_ever;
	nsa_rw_read.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwRead(&nsa_rw_read);
	if (nsa_status != NSA_SUCCESS) {
		APP_ERROR1("NSA_RwRead failed:%d", nsa_status);
		return nsa_status;
	}

	return 0;
}

int app_nsa_rw_write_wps_with_payload(UINT8 *payload, UINT32 payload_size)
{
	tNSA_RW_WRITE            nsa_rw_write;
	tNSA_STATUS              nsa_status;
	UINT32			 cp_size;

	NSA_RwWriteInit(&nsa_rw_write);

	nsa_rw_write.for_ever = 0; /* just one tag */

	/* Update new layload */
	cp_size = payload_size;
	if (cp_size > (MAX_NDEF_LENGTH - WIFI_NDEF_REC_WPS_OFFSET)) {
		APP_ERROR1("Read/Write NDEF payload size %d, too big trim off!\n", cp_size);
		cp_size = MAX_NDEF_LENGTH - WIFI_NDEF_REC_WPS_OFFSET;
	}

	memcpy(&wifi_wps[WIFI_NDEF_REC_WPS_OFFSET], payload, cp_size);
	wifi_wps[NDEF_PAYLOAD_LEN_OFFSET] = cp_size;
	wifi_wps_len = WIFI_NDEF_REC_WPS_OFFSET + cp_size;

	nsa_rw_write.ndef_length = wifi_wps_len;
	memcpy(nsa_rw_write.ndef_data, wifi_wps, nsa_rw_write.ndef_length);	
	nsa_rw_write.rw_cback = app_nsa_rw_cback;

	nsa_status = NSA_RwWrite(&nsa_rw_write);
	if (nsa_status != NSA_SUCCESS) {
		APP_ERROR1("NSA_RwWrite failed:%d", nsa_status);
		return nsa_status;
	}
	
	return 0;
}

void app_nsa_cho_cback_wps(tNSA_CHO_EVT event, tNSA_CHO_EVT_DATA *p_data)
{
	BOOLEAN 		req_bt, req_wifi;
	tNSA_CHO_SEND_SEL	app_nsa_cho_send_sel;
	tNSA_CHO_SEND_SEL_ERR 	app_nsa_cho_send_sel_err;
	tNSA_STATUS		nsa_status;
	UINT8            	*p_ndef_header;
	UINT8			*p_rec;
	tNSA_CHO_SELECT		*p_sel;
	int i;

    switch(event)
    {
    case NSA_CHO_STARTED_EVT:
        APP_INFO1("NSA_CHO_STARTED_EVT Status:%d", p_data->status);
        break;
    case NSA_CHO_REQ_EVT:
    	APP_INFO1("NSA_CHO_REQ_EVT Status:%d", p_data->request.status);
    	if (p_data->request.status == NFA_STATUS_OK)
    	{
    		APP_INFO1("\t received %d Alternative Carrier", p_data->request.num_ac_rec);
    	}
    	else
    	{
    		app_nsa_cb.cho_test.cho_server_started = FALSE;
    		break;
    	}

    	nsa_parse_cho_req(&p_data->request, &req_bt, &req_wifi);

    	if (req_bt || req_wifi)
    	{
    		NSA_ChoSendSelectInit(&app_nsa_cho_send_sel);
    		NDEF_MsgInit (app_nsa_cho_send_sel.ndef_data, MAX_NDEF_LENGTH, &(app_nsa_cho_send_sel.ndef_len));
    	}
    	else
    	{
    		NSA_ChoSendSelectErrorInit(&app_nsa_cho_send_sel_err);

    		app_nsa_cho_send_sel_err.error_data = 0;
    		app_nsa_cho_send_sel_err.error_reason = NFA_CHO_ERROR_CARRIER;

    		nsa_status = NSA_ChoSendSelectError(&app_nsa_cho_send_sel_err);
    		if (nsa_status != NSA_SUCCESS)
    		{
    			APP_ERROR1("NSA_ChoSendSelectError failed:%d", nsa_status);
    		}
    	}
    	if ((req_bt) && (app_nsa_cb.cho_test.bt_availalble))
    	{/*if BT is requested by peer and it is avaialable... add it in case...*/
    		if ((!req_wifi) ||   // is the only alternative carrier or...
    			(app_nsa_cb.cho_test.preference == NSA_CHO_BT) ||   // it is the preferred carrier
    			(app_nsa_cb.cho_test.preference == NSA_CHO_NONE))   // no preference
    		{
    			/*add hard-coded BT_OOB and ac info value*/
    			/* restore NDEF header which might be updated during previous appending record */
    			p_ndef_header = (UINT8*)bt_oob_rec;
    			*p_ndef_header |= (NDEF_ME_MASK|NDEF_MB_MASK);
    			NDEF_MsgAppendRec (app_nsa_cho_send_sel.ndef_data, MAX_NDEF_LENGTH, &(app_nsa_cho_send_sel.ndef_len),
    					bt_oob_rec, BT_OOB_REC_LEN);

    			app_nsa_cho_send_sel.ac_info[app_nsa_cho_send_sel.num_ac_info].cps          = NFA_CHO_CPS_ACTIVE;
    			app_nsa_cho_send_sel.ac_info[app_nsa_cho_send_sel.num_ac_info].num_aux_data = 0;
    			app_nsa_cho_send_sel.num_ac_info++;
    		}

    	}
    	if ((req_wifi) && (app_nsa_cb.cho_test.wifi_availalble))
    	{/*if WiFi is requested by peer and it is avaialable... add it in case...*/
    		if ((!req_bt) ||   // is the only alternative carrier or...
    			(app_nsa_cb.cho_test.preference == NSA_CHO_WIFI) ||   // it is the preferred carrier
    			(app_nsa_cb.cho_test.preference == NSA_CHO_NONE))   // no preference
    		{
    			/*add hard-coded WIFI_OOB and ac info value*/
    			/* restore NDEF header which might be updated during previous appending record */
    			p_ndef_header = (UINT8*)wifi_hs_rec;
    			*p_ndef_header |= (NDEF_ME_MASK|NDEF_MB_MASK);

    			NDEF_MsgAppendRec (app_nsa_cho_send_sel.ndef_data, MAX_NDEF_LENGTH, &(app_nsa_cho_send_sel.ndef_len),
    					wifi_hs_rec, wifi_hs_rec_len);

    			app_nsa_cho_send_sel.ac_info[app_nsa_cho_send_sel.num_ac_info].cps          = NFA_CHO_CPS_ACTIVE;
    			app_nsa_cho_send_sel.ac_info[app_nsa_cho_send_sel.num_ac_info].num_aux_data = 0;
    			app_nsa_cho_send_sel.num_ac_info++;
    		}
    	}

    	/*send select*/
    	if (req_bt || req_wifi)
    	{
    		if (NSA_ChoSendSelect(&app_nsa_cho_send_sel) == NSA_SUCCESS)
			wps_nfc_ndef_cback("ChoSendSelectSuccess", sizeof("ChoSendSelectSuccess"));
    	}
    	break;

    case NSA_CHO_SEL_EVT:
    	APP_INFO1("NSA_CHO_SEL_EVT Status:%d", p_data->status);
    	if (p_data->status == NFA_STATUS_OK)
    	{
    		APP_INFO1("\t received %d Alternative Carrier", p_data->request.num_ac_rec);
    	}
    	else
    		break;

    	/* Just for WSC NDEF */
	p_sel = &p_data->select;

	/*parse and display received request*/
	for (i = 0; i < p_sel->num_ac_rec; i++) {
		APP_INFO1("\t\t Parsing AC = %d ", i);
		APP_INFO1("\t\t   cps = % d ", p_sel->ac_rec[i].cps);
		APP_INFO1("\t\t   aux_data_ref_count = % d ", p_sel->ac_rec[i].aux_data_ref_count);
	}

	APP_DUMP("\t\tNDEF CHO sel", p_sel->ref_ndef, p_sel->ref_ndef_len);
	APP_DEBUG0("\t\t Start to parse NDEF content of CHO sel.");

	wps_nfc_ndef_cback(p_sel->ref_ndef, p_sel->ref_ndef_len);
    	break;

    case NSA_CHO_SEL_ERR_EVT:
    	APP_INFO0("NSA_CHO_SEL_ERR_EVT");
    	APP_INFO1("\terror_data = 0x%x", p_data->sel_err.error_data);
    	APP_INFO1("\terror_reason = 0x%x", p_data->sel_err.error_reason);
    	break;

    case NSA_CHO_TX_FAIL_ERR_EVT:
    	APP_INFO1("NSA_CHO_TX_FAIL_ERR_EVT Status:%d", p_data->status);
    	break;

    default:
        APP_ERROR1("Bad CHO Event", event);
        break;
    }
}

/* Both Selector and Requester will invake this function */
int app_nsa_cho_start_wps_with_payload(BOOLEAN cho_as_server, UINT8 *payload, UINT32 payload_size)
{
	tNSA_CHO_START		nsa_cho_start;
	tNSA_DM_SET_P2P_TECH    nsa_set_p2p_tech_req;
	tNSA_DM_ENABLEPOLL      nsa_enable_poll_req;
	tNSA_DM_START_RF_DISCOV nsa_start_rf_req;
	tNSA_STATUS		nsa_status;
	UINT32			cp_size;

	/* sanity check */
	if (cho_as_server && (!payload || !payload_size)) {
		APP_ERROR0("NSA_ChoStart in server mode must have payload argument\n");
		return -1;
	}

	NSA_ChoStartInit(&nsa_cho_start);

	app_nsa_cb.cho_test.cho_server_started = cho_as_server;

	nsa_cho_start.reg_server = app_nsa_cb.cho_test.cho_server_started;
	if (nsa_cho_start.reg_server) {
		app_nsa_cb.cho_test.bt_availalble = 0;
		app_nsa_cb.cho_test.wifi_availalble = 1;
		if (app_nsa_cb.cho_test.bt_availalble && app_nsa_cb.cho_test.wifi_availalble )
			app_nsa_cb.cho_test.preference = 1;

		/* Update new layload */
		cp_size = payload_size;
		if (cp_size > (MAX_NDEF_LENGTH - WIFI_HS_REC_WPS_OFFSET)) {
			APP_ERROR1("Hand over Selector payload size %d, too big trim off!\n", cp_size);
			cp_size = MAX_NDEF_LENGTH - WIFI_HS_REC_WPS_OFFSET;
		}

		memcpy(&wifi_hs_rec[WIFI_HS_REC_WPS_OFFSET], payload, cp_size);
		wifi_hs_rec[NDEF_PAYLOAD_LEN_OFFSET] = cp_size;
		wifi_hs_rec_len = WIFI_HS_REC_WPS_OFFSET + cp_size;
	}

	nsa_cho_start.p_callback = app_nsa_cho_cback_wps;

	nsa_status = NSA_ChoStart(&nsa_cho_start);
	if (nsa_status != NSA_SUCCESS) {
		APP_ERROR1("NSA_ChoStart failed:%d", nsa_status);
		return nsa_status;
	}

	if (nsa_cho_start.reg_server) {
		 /*set P2P tech listen mask for CHO, needed only if server is started to receive CHO req*/
		NSA_DmSetP2pListenTechInit(&nsa_set_p2p_tech_req);

		nsa_set_p2p_tech_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;

		nsa_status = NSA_DmSetP2pListenTech(&nsa_set_p2p_tech_req);
		if (nsa_status != NSA_SUCCESS) {
			APP_ERROR1("NSA_DmSetP2pListenTech failed:%d", nsa_status);
			return nsa_status;
		}
	}

	 /*set polling mask*/
	NSA_DmEnablePollingInit(&nsa_enable_poll_req);
	nsa_enable_poll_req.poll_mask = NFA_TECHNOLOGY_MASK_ALL;

	nsa_status = NSA_DmEnablePolling(&nsa_enable_poll_req);
	if (nsa_status != NSA_SUCCESS) {
		APP_ERROR1("NSA_DmEnablePolling failed:%d", nsa_status);
		return nsa_status;
	}

	/*start RF discovery*/
	NSA_DmStartRfDiscovInit(&nsa_start_rf_req);
	/*no param to be passed*/
	nsa_status = NSA_DmStartRfDiscov(&nsa_start_rf_req);
	if (nsa_status != NSA_SUCCESS) {
		APP_ERROR1("NSA_DmStartRfDiscov failed:%d", nsa_status);
		return nsa_status;
	}

	return 0;
}

/* Requester will invake this function */
int app_nsa_cho_send_wps(void)
{
	tNSA_CHO_SEND_REQ	nsa_cho_send_req;
	tNSA_STATUS		nsa_status;
	int			choice;
	tNDEF_STATUS		ndef_status;
	UINT8            	*p_ndef_header;

	NSA_ChoSendRequestInit(&nsa_cho_send_req);

	NDEF_MsgInit (nsa_cho_send_req.ndef_data, MAX_NDEF_LENGTH, &(nsa_cho_send_req.ndef_len));

	/* add hard-coded WiFi req with Hc as TNF and ac info value */
	/* restore NDEF header which might be updated during previous appending record */
	p_ndef_header = (UINT8*)wifi_hr_rec;
	*p_ndef_header |= (NDEF_ME_MASK|NDEF_MB_MASK);
	ndef_status = NDEF_MsgAppendRec (nsa_cho_send_req.ndef_data, MAX_NDEF_LENGTH, &(nsa_cho_send_req.ndef_len),
			wifi_hr_rec, WIFI_HR_REC_LEN);
	nsa_cho_send_req.ac_info[nsa_cho_send_req.num_ac_info].cps          = NFA_CHO_CPS_ACTIVE;
	nsa_cho_send_req.ac_info[nsa_cho_send_req.num_ac_info].num_aux_data = 0;
	nsa_cho_send_req.num_ac_info++;

	nsa_status = NSA_ChoSendRequest(&nsa_cho_send_req);
	if (nsa_status != NSA_SUCCESS) {
		APP_ERROR1("NSA_ChoSendRequest failed:%d", nsa_status);
		return nsa_status;
	}

	return 0;
}
/* Kenlo - */
